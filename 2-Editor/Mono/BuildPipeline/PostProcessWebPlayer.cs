using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditorInternal;
using UnityEngine;

internal class PostProcessWebPlayer
{
	static Dictionary<BuildOptions,string> optionNames = new Dictionary<BuildOptions,string> () {
		{ BuildOptions.AllowDebugging, "enableDebugging" },
	};
	
	public static void PostProcess(BuildOptions options, string installPath, string downloadWebplayerUrl, int width, int height)
	{
		string installName = FileUtil.UnityGetFileName(installPath);
		string installDir = installPath;
			
		string templateBuildDirectory = "Temp/BuildingWebplayerTemplate";
		FileUtil.DeleteFileOrDirectory(templateBuildDirectory);			

		// Validate template
		if (PlayerSettings.webPlayerTemplate == null || !PlayerSettings.webPlayerTemplate.Contains(":"))
		{
			Debug.LogError("Invalid WebPlayer template selection! Select a template in player settings.");
			return;
		}

		// Resolve template path
		string[] templateElements = PlayerSettings.webPlayerTemplate.Split(':');
		string templatePath;
		if (templateElements[0].Equals("PROJECT"))
			templatePath = Application.dataPath;
		else	
			templatePath = Path.Combine(EditorApplication.applicationContentsPath, "Resources");
		templatePath = Path.Combine(Path.Combine(templatePath, "WebPlayerTemplates"), templateElements[1]);

		// Validate template path
		if (!Directory.Exists(templatePath))
		{
			Debug.LogError("Invalid WebPlayer template path! Select a template in player settings.");
			return;
		}
		string[] indexFiles = Directory.GetFiles(templatePath, "index.*");
		if (indexFiles.Length < 1)
		{
			Debug.LogError("Invalid WebPlayer template selection! Select a template in player settings.");
			return;
		}

		// Copy the template to the install directory
		FileUtil.CopyDirectoryRecursive (templatePath, templateBuildDirectory);
		
		// Rename the index file
		indexFiles = Directory.GetFiles(templateBuildDirectory, "index.*");
		string tempIndexFilePath = indexFiles[0];
		string extension = Path.GetExtension(tempIndexFilePath);
		string indexFilePath = Path.Combine(templateBuildDirectory, installName + extension);
		FileUtil.MoveFileOrDirectory(tempIndexFilePath, indexFilePath);

		// Remove thumbnail if found
		string[] thumbnails = Directory.GetFiles(templateBuildDirectory, "thumbnail.*");
		if (thumbnails.Length > 0)
		{
			FileUtil.DeleteFileOrDirectory(thumbnails[0]);
		}

		// Local UnityObject is only needed when online version is not available.
		bool local = ((options & BuildOptions.WebPlayerOfflineDeployment) != 0);

		//TODO: replace with final URL
		string unityObject2Url = (local ? "UnityObject2.js" : (downloadWebplayerUrl + "/3.0/uo/UnityObject2.js"));
		string unityObjectDependencies = string.Format("<script type='text/javascript' src='{0}'></script>", local ? "jquery.min.js" : "https://ssl-webplayer.unity3d.com/download_webplayer-3.x/3.0/uo/jquery.min.js");

		// Construct built-in replacements list
		List<string> replacements = new List<string>();
		replacements.Add("%UNITY_UNITYOBJECT_DEPENDENCIES%"); replacements.Add(unityObjectDependencies);
		replacements.Add("%UNITY_UNITYOBJECT_URL%"); replacements.Add(unityObject2Url);
		replacements.Add("%UNITY_WIDTH%"); replacements.Add(width.ToString());
		replacements.Add("%UNITY_HEIGHT%"); replacements.Add(height.ToString());
		replacements.Add("%UNITY_PLAYER_PARAMS%"); replacements.Add(GeneratePlayerParamsString (options));
		replacements.Add("%UNITY_WEB_NAME%"); replacements.Add(PlayerSettings.productName);
		replacements.Add("%UNITY_WEB_PATH%"); replacements.Add(installName + ".unity3d");

		if (InternalEditorUtility.IsUnityBeta())
		{
			replacements.Add("%UNITY_BETA_WARNING%"); replacements.Add("\r\n\t\t<p style=\"color: #c00; font-size: small; font-style: italic;\">Built with beta version of Unity. Will only work on your computer!</p>");
			replacements.Add("%UNITY_SET_BASE_DOWNLOAD_URL%"); replacements.Add(",baseDownloadUrl: \"" + downloadWebplayerUrl + "/\"");
		}
		else
		{
			replacements.Add("%UNITY_BETA_WARNING%"); replacements.Add(string.Empty);
			replacements.Add("%UNITY_SET_BASE_DOWNLOAD_URL%"); replacements.Add(string.Empty);
		}

		// Add custom replacements
		foreach (string key in PlayerSettings.templateCustomKeys)
		{
			replacements.Add("%UNITY_CUSTOM_" + key.ToUpper() + "%");
			replacements.Add(PlayerSettings.GetTemplateCustomValue(key));
		}

		// Exchange template markers for data
		FileUtil.ReplaceText(indexFilePath, replacements.ToArray());

		if (local)
		{
			string scriptFilePath = Path.Combine(templateBuildDirectory, "UnityObject2.js");
			FileUtil.DeleteFileOrDirectory(scriptFilePath);
			FileUtil.UnityFileCopy(EditorApplication.applicationContentsPath + "/Resources/UnityObject2.js", scriptFilePath);
			scriptFilePath = Path.Combine(templateBuildDirectory, "jquery.min.js");
			FileUtil.DeleteFileOrDirectory(scriptFilePath);
			FileUtil.UnityFileCopy(EditorApplication.applicationContentsPath + "/Resources/jquery.min.js", scriptFilePath);
		}
		
		FileUtil.CopyDirectoryRecursive (templateBuildDirectory, installPath, true);

		// Move data file
		string targetUnity3DFile = Path.Combine(installDir, installName + ".unity3d");
		FileUtil.DeleteFileOrDirectory(targetUnity3DFile);			
		FileUtil.MoveFileOrDirectory("Temp/unitystream.unity3d", targetUnity3DFile);
		
		if (Directory.Exists(PostprocessBuildPlayer.StreamingAssets))
			FileUtil.CopyDirectoryRecursiveForPostprocess(PostprocessBuildPlayer.StreamingAssets, Path.Combine(installDir, "StreamingAssets"), true);
	}
	
	static string GeneratePlayerParamsString (BuildOptions options)
	{
		return string.Format ("{{ {0} }}", string.Join (",", optionNames.Select (
			pair => string.Format ("{0}:\"{1}\"", pair.Value,
				((pair.Key == (options & pair.Key))? 1: 0)
			)
		).ToArray ()));
	}
}