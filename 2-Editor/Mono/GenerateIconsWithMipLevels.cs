using System;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.IO;
using System.Collections.Generic;
using Object = UnityEngine.Object;

/* Some notes on icon caches and loading
 * 
 * 
 * Caches:
 *		 Icon name to Texture2D:
 *			gNamedTextures (ObjectImages::Texture2DNamed): string to Texture2D
 *    
 *		InstanceID to Image
 *			gCachedThumbnails (AssetImporter::GetThumbnailForInstanceID) : where image is retrieved from Texture from gNamedTextures
 *			Used for setting icon for hierarchyProperty
 * 
 *		InstanceID to Texture2D
 *			ms_HackedCache (AssetDatabaseProperty::GetCachedAssetDatabaseIcon) : where tetxure2D icon is generated from the image from gCachedThumbnails
 *			Cache max size: 200
 *			Called from C# by AssetDatabase.GetCachedIcon (string assetpath) 
 * 
 * 
 * 
 * 
	Icon loading in Editor Default Resources project: 
	- Texture2DNamed handles reading local files instead of editor resources bundle 
	  
	Icon loading in other projects
	- When reimport of asset (cpp): AssetDatabase::ImportAsset -> MonoImporter::GenerateAssetData -> ImageForMonoScript -> GetImageNames -> ObjectImages::Texture2DNamed
 	- IconContent -> LoadIconRequired -> LoadIcon -> LoadIconForSkin -> Load (EditorResourcesUtility.iconsPath + name) as Texture2D;
	- AssetDatabase.GetCachedIcon (assetpath) -> CPP -> AssetDatabaseProperty::GetCachedAssetDatabaseIcon
	- InternalEditorUtility.GetIconForFile (filename) -> EditorGUIUtility.FindTexture("boo Script Icon") -> CPP -> ObjectImages::Texture2DNamed(cpp)
	 
	*/


class GenerateIconsWithMipLevels
{
	static string k_IconSourceFolder = "Assets/MipLevels For Icons/";
	static string k_IconTargetFolder = "Assets/Editor Default Resources/Icons/Generated";

	class InputData
	{
		public string sourceFolder;
		public string targetFolder;
		public string mipIdentifier;
		public string mipFileExtension;
		public List<string> generatedFileNames = new List<string>(); // for internal use

		public string GetMipFileName (string baseName, int mipResolution)
		{
			return sourceFolder + baseName + mipIdentifier + mipResolution + "." + mipFileExtension;
		}
	}

	// Called from BuildEditorAssetBundles
	public static void GenerateAllIconsWithMipLevels ()
	{
		InputData data = new InputData ();
		data.sourceFolder = k_IconSourceFolder;
		data.targetFolder = k_IconTargetFolder;
		data.mipIdentifier = "@";
		data.mipFileExtension = "png";

		// Clear generated folder to prevent leftovers
		if (AssetDatabase.GetMainAssetInstanceID(data.targetFolder) != 0)
		{
			//Debug.Log ("Delete old assets in: " + data.targetFolder);
			AssetDatabase.DeleteAsset (data.targetFolder);
			AssetDatabase.Refresh ();
		}

		EnsureFolderIsCreated (data.targetFolder);
		
		float startTime = Time.realtimeSinceStartup;
		GenerateIconsWithMips (data);
		Debug.Log (string.Format("Generated {0} icons with mip levels in {1} seconds", data.generatedFileNames.Count, Time.realtimeSinceStartup - startTime));
		
		RemoveUnusedFiles (data.generatedFileNames);
		AssetDatabase.Refresh (); // For some reason we creash if we dont refresh twice...
		InternalEditorUtility.RepaintAllViews ();
	}


	// Refresh just one icon (Used in Editor Resources project, find it in Tools/)
	public static void GenerateSelectedIconsWithMips()
	{
		// If no selection do all
		if (Selection.activeInstanceID == 0)
		{
			Debug.Log("Ensure to select a mip texture..." + Selection.activeInstanceID);
			return;
		}

		InputData data = new InputData();
		data.sourceFolder = k_IconSourceFolder;
		data.targetFolder = k_IconTargetFolder;
		data.mipIdentifier = "@";
		data.mipFileExtension = "png";

		int instanceID = Selection.activeInstanceID;
		string assetPath = AssetDatabase.GetAssetPath(instanceID);

		if (assetPath.IndexOf(data.sourceFolder) < 0)
		{
			Debug.Log("Selection is not a valid mip texture, it should be located in: " + data.sourceFolder);
			return;
		}

		if (assetPath.IndexOf(data.mipIdentifier) < 0)
		{
			Debug.Log("Selection does not have a valid mip identifier " + assetPath + "  " + data.mipIdentifier);
			return;
		}

		float startTime = Time.realtimeSinceStartup;

		string baseName = Path.GetFileNameWithoutExtension(assetPath);
		baseName = baseName.Substring(0, baseName.IndexOf(data.mipIdentifier));
		
		List<string> assetPaths = GetIconAssetPaths(data.sourceFolder, data.mipIdentifier, data.mipFileExtension);

		EnsureFolderIsCreated (data.targetFolder);
		GenerateIcon (data, baseName, assetPaths);
		Debug.Log (string.Format("Generated {0} icon with mip levels in {1} seconds", baseName, Time.realtimeSinceStartup - startTime));
		InternalEditorUtility.RepaintAllViews();
	}

	// Private section
	//----------------
	private static void GenerateIconsWithMips (InputData inputData)
	{
		List<string> files = GetIconAssetPaths (inputData.sourceFolder, inputData.mipIdentifier, inputData.mipFileExtension);

		if (files.Count == 0)
		{
			Debug.LogWarning ("No mip files found for generating icons! Searching in: " + inputData.sourceFolder + ", for files with extension: " + inputData.mipFileExtension);
		}

		string[] baseNames = GetBaseNames (inputData, files);
		
		// Base name is assumed to be like: "Assets/Icons/..."
		foreach (string baseName in baseNames)
			GenerateIcon (inputData, baseName, files);
	}

	private static void GenerateIcon (InputData inputData, string baseName, List<string> assetPathsOfAllIcons)
	{
		Texture2D iconWithMips = CreateIconWithMipLevels (inputData, baseName, assetPathsOfAllIcons);
		if (iconWithMips == null)
		{
			Debug.Log ("CreateIconWithMipLevels failed");
			return;
		}

		iconWithMips.name = baseName + " Icon" + ".png"; // asset name must have .png as extension (used when loading the icon, search for LoadIconForSkin)

		// Write texture to disk
		string generatedPath = inputData.targetFolder + "/" + baseName + " Icon" + ".asset";
		AssetDatabase.CreateAsset (iconWithMips, generatedPath);
		inputData.generatedFileNames.Add (generatedPath);
	}

	private static Texture2D CreateIconWithMipLevels (InputData inputData, string baseName, List<string> assetPathsOfAllIcons)
	{
		List<string> allMipPaths = assetPathsOfAllIcons.FindAll (delegate(string o) { return o.IndexOf(baseName + inputData.mipIdentifier) >= 0; });
		List<Texture2D> allMips = new List<Texture2D>();
		foreach (string path in allMipPaths)
		{
			Texture2D mip = GetTexture2D (path);
			if (mip != null)
				allMips.Add (mip);
			else
				Debug.LogError("Mip not found " + path);
		}

		int minResolution = 99999;
		int maxResolution = 0;

		foreach (Texture2D mip in allMips)
		{
			int width = mip.width;
			if (width > maxResolution)
				maxResolution = width;
			if (width < minResolution)
				minResolution = width;
		}

		if (maxResolution == 0)
			return null;

		// Create our icon
		Texture2D iconWithMips = new Texture2D (maxResolution, maxResolution, TextureFormat.ARGB32, true, true);
		
		// Add max mip
		if (BlitMip (iconWithMips, inputData.GetMipFileName (baseName, maxResolution), 0))
			iconWithMips.Apply(true);
		else
			return iconWithMips; // ERROR

		// Keep for debugging
		//Debug.Log ("Processing max mip file: " + inputData.GetMipFileName (baseName, maxResolution) );

		// Now add the rest
		int resolution = maxResolution;
		for (int i = 1; i < iconWithMips.mipmapCount; i++)
		{
			resolution /= 2;
			if (resolution < minResolution)
				break;

			BlitMip (iconWithMips, inputData.GetMipFileName (baseName, resolution), i);
		}
		iconWithMips.Apply(false, true);


		return iconWithMips;
	}

	private static bool BlitMip (Texture2D iconWithMips, string mipFile, int mipLevel)
	{
		Texture2D tex = GetTexture2D (mipFile);
		if (tex)
		{
			Blit (tex, iconWithMips, mipLevel);
			return true;
		}
		else
		{
			Debug.Log("Mip file NOT found: " + mipFile);
		}	
		return false;
	}

	private static Texture2D GetTexture2D(string path)
	{
		return AssetDatabase.LoadAssetAtPath(path, typeof(Texture2D)) as Texture2D;
	}


	static List<string> GetIconAssetPaths (string folderPath, string mustHaveIdentifier, string extension)
	{
		string curDirectory = Directory.GetCurrentDirectory();
		string absolute = Path.Combine(curDirectory, folderPath);
		List<string> files =  new List<string> (Directory.GetFiles (absolute, "*." + extension));
		files.RemoveAll (delegate (string o) {return o.IndexOf (mustHaveIdentifier) < 0;} ); // // Remove all files that do not have the 'mustHaveIdentifier'
		for (int i=0; i<files.Count; ++i)
			files[i] = folderPath + Path.GetFileName (files[i]);
		return files;
	}

	static void Blit (Texture2D source, Texture2D dest, int mipLevel)
	{
		Color32[] pixels = source.GetPixels32 ();
		for (int i = 0; i < pixels.Length; i++)
		{
			Color32 p = pixels[i];
			if (p.a >= 3)
				p.a -= 3;
			pixels[i] = p;
		}
		dest.SetPixels32 (pixels, mipLevel);
	}

	private static void EnsureFolderIsCreated(string targetFolder)
	{
		if (AssetDatabase.GetMainAssetInstanceID(targetFolder) == 0)
		{
			Debug.Log("Created target folder " + targetFolder);
			AssetDatabase.CreateFolder(Path.GetDirectoryName(targetFolder), Path.GetFileName(targetFolder));
		}
	}

	static void DeleteFile (string file)
	{
		if (AssetDatabase.GetMainAssetInstanceID (file) != 0)
		{
			Debug.Log("Deleted unused file: " + file);
			AssetDatabase.DeleteAsset (file);
		}
	}

	// Get rid of old icons in the Icons folder (with same filename as a generated icon)
	static void RemoveUnusedFiles(List<string> generatedFiles)
	{
		for (int i = 0; i < generatedFiles.Count; ++i)
		{
			string deleteFile = generatedFiles[i].Replace("Icons/Generated", "Icons");
			deleteFile = deleteFile.Replace(".asset", ".png");
			DeleteFile (deleteFile);

			// Remove the d_ version as well
			string fileName = Path.GetFileNameWithoutExtension(deleteFile);
			if (!fileName.StartsWith("d_"))
			{
				deleteFile = deleteFile.Replace(fileName, ("d_" + fileName));
				DeleteFile (deleteFile);
			}
		}
		AssetDatabase.Refresh ();
	}

	private static string[] GetBaseNames(InputData inputData, List<string> files)
	{
		string[] baseNames = new string[files.Count];
		int startPos = inputData.sourceFolder.Length;
		for (int i = 0; i < files.Count; ++i)
		{
			baseNames[i] = files[i].Substring(startPos, files[i].IndexOf(inputData.mipIdentifier) - startPos);
		}
		HashSet<string> hashset = new HashSet<string>(baseNames);
		baseNames = new string[hashset.Count];
		hashset.CopyTo(baseNames);

		return baseNames;
	}

}


