using UnityEditor.ProjectWindowCallback;
using UnityEngine;
using UnityEditorInternal;
using System.IO;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace UnityEditor
{

// Callbacks to be used when creating assets via the project window
// You can extend the EndNameEditAction and write your own callback
// It is done this way instead of via a delegate because the action 
// needs to survive an assembly reload.
namespace ProjectWindowCallback
{
	public abstract class EndNameEditAction : ScriptableObject
	{
		public virtual void OnEnable()
		{
			hideFlags = HideFlags.HideAndDontSave;
		}

		public abstract void Action(int instanceId, string pathName, string resourceFile);

		public virtual void CleanUp()
		{
			DestroyImmediate(this);
		}
	}

	internal class DoCreateNewAsset : EndNameEditAction
	{
		public override void Action(int instanceId, string pathName, string resourceFile)
		{
			AssetDatabase.CreateAsset(EditorUtility.InstanceIDToObject(instanceId),
				                        AssetDatabase.GenerateUniqueAssetPath(pathName));
			ProjectWindowUtil.FrameObjectInProjectWindow(instanceId);
		}
	}

	internal class DoCreateFolder : EndNameEditAction
	{
		public override void Action(int instanceId, string pathName, string resourceFile)
		{
			string guid = AssetDatabase.CreateFolder(Path.GetDirectoryName(pathName), Path.GetFileName(pathName));
			Object o = AssetDatabase.LoadAssetAtPath(AssetDatabase.GUIDToAssetPath(guid), typeof (Object));
			ProjectWindowUtil.ShowCreatedAsset(o);
		}
	}

	internal class DoCreatePrefab : EndNameEditAction
	{
		public override void Action(int instanceId, string pathName, string resourceFile)
		{
			Object o = PrefabUtility.CreateEmptyPrefab(pathName);
			ProjectWindowUtil.ShowCreatedAsset(o);
		}
	}

	internal class DoCreateScriptAsset : EndNameEditAction
	{
		public override void Action(int instanceId, string pathName, string resourceFile)
		{
			Object o = ProjectWindowUtil.CreateScriptAssetFromTemplate(pathName, resourceFile);
			ProjectWindowUtil.ShowCreatedAsset(o);
		}
	}

	internal class DoCreateAnimatorController : EndNameEditAction
	{
		public override void Action(int instanceId, string pathName, string resourceFile)
		{
            AnimatorController controller = AnimatorController.CreateAnimatorControllerAtPath(pathName);
			ProjectWindowUtil.ShowCreatedAsset(controller);
		}
	}
}

public class ProjectWindowUtil
{
	[MenuItem("Assets/Create/GUI Skin", false, 601)]
	public static void CreateNewGUISkin()
	{
		GUISkin skin = ScriptableObject.CreateInstance<GUISkin>();
		GUISkin original = Resources.GetBuiltinResource(typeof(GUISkin), "GameSkin/GameSkin.guiskin") as GUISkin;
		if (original)
			EditorUtility.CopySerialized(original, skin);
		else
			Debug.LogError("Internal error: unable to load builtin GUIskin");

		CreateAsset(skin, "New GUISkin.guiskin");
	}

	// Returns the path of currently selected folder. If multiple are selected, returns the first one.
	internal static string GetActiveFolderPath ()
	{
		ProjectBrowser projectBrowser = GetProjectBrowserIfExists ();
		
		if(projectBrowser == null)
			return "Assets";

		return projectBrowser.GetActiveFolderPath();
	}

	internal static void EndNameEditAction(EndNameEditAction action, int instanceId, string pathName, string resourceFile)
	{
		pathName = AssetDatabase.GenerateUniqueAssetPath(pathName);
		if (action != null)
		{
			action.Action(instanceId, pathName, resourceFile);
			action.CleanUp();
		}
	}

	// Create a standard Object-derived asset.
	public static void CreateAsset (Object asset, string pathName)
	{
		StartNameEditingIfProjectWindowExists(asset.GetInstanceID(), ScriptableObject.CreateInstance<DoCreateNewAsset>(), pathName, AssetPreview.GetMiniThumbnail(asset), null);
	}

	// Create a folder
	public static void CreateFolder ()
	{
		StartNameEditingIfProjectWindowExists (0, ScriptableObject.CreateInstance<DoCreateFolder>(), "New Folder", EditorGUIUtility.IconContent (EditorResourcesUtility.emptyFolderIconName).image as Texture2D, null);
	}

	// Create a prefab
	public static void CreatePrefab ()
	{
		StartNameEditingIfProjectWindowExists (0, ScriptableObject.CreateInstance<DoCreatePrefab>(), "New Prefab.prefab", EditorGUIUtility.IconContent ("Prefab Icon").image as Texture2D, null);
	}

	static void CreateScriptAsset (string templatePath, string destName)
	{
		Texture2D icon = null;
		switch (Path.GetExtension (destName))
		{
		case ".js":
			icon = EditorGUIUtility.IconContent ("js Script Icon").image as Texture2D;
			break;
		case ".cs":
			icon = EditorGUIUtility.IconContent ("cs Script Icon").image as Texture2D;
			break;
		case ".boo":
			icon = EditorGUIUtility.IconContent ("boo Script Icon").image as Texture2D;
			break;
		case ".shader":
			icon = EditorGUIUtility.IconContent ("Shader Icon").image as Texture2D;
			break;
		default:
			icon = EditorGUIUtility.IconContent ("TextAsset Icon").image as Texture2D;
			break;
		}
		StartNameEditingIfProjectWindowExists (0, ScriptableObject.CreateInstance<DoCreateScriptAsset>(), destName, icon, templatePath);
	}


	public static void ShowCreatedAsset (Object o)
	{
		// Show it
		Selection.activeObject = o;
		if (o)
			FrameObjectInProjectWindow (o.GetInstanceID());
	}

	static private void CreateAnimatorController ()
	{
		var icon = EditorGUIUtility.IconContent ("AnimatorController Icon").image as Texture2D;
		StartNameEditingIfProjectWindowExists (0, ScriptableObject.CreateInstance<DoCreateAnimatorController>(), "New Animator Controller.controller", icon, null);
	}

	internal static Object CreateScriptAssetFromTemplate (string pathName, string resourceFile)
	{
		string fullPath = Path.GetFullPath (pathName);
			
		// Search/Replace of #NAME# with non-spaced name
		StreamReader reader = new StreamReader (resourceFile);
		string content = reader.ReadToEnd();
		reader.Close();
		
		string baseFile = Path.GetFileNameWithoutExtension (pathName);
		content = Regex.Replace (content, "#NAME#", baseFile);
		string baseFileNoSpaces = Regex.Replace (baseFile, " ", "");
		content = Regex.Replace (content, "#SCRIPTNAME#", baseFileNoSpaces);
		// if the script name begins with an uppercase character we support a lowercase substitution variant
		if(char.IsUpper(baseFileNoSpaces, 0))
		{
			baseFileNoSpaces = char.ToLower(baseFileNoSpaces[0]) + baseFileNoSpaces.Substring(1);
			content = Regex.Replace (content, "#SCRIPTNAME_LOWER#", baseFileNoSpaces);
		}
		else
		{
			// still allow the variant, but change the first character to upper and prefix with "my"
			baseFileNoSpaces = "my" + char.ToUpper(baseFileNoSpaces[0]) + baseFileNoSpaces.Substring(1);
			content = Regex.Replace (content, "#SCRIPTNAME_LOWER#", baseFileNoSpaces);
		}
		
		bool useBOM = true;
		bool throwOnInvalidBytes = false; // just insert '?'
		var enc = new System.Text.UTF8Encoding(useBOM, throwOnInvalidBytes);
		bool append = false;
		StreamWriter writer = new StreamWriter (fullPath, append, enc);

		writer.Write (content);
		writer.Close();
			
		// Import the asset
		AssetDatabase.ImportAsset (pathName);
		
		return AssetDatabase.LoadAssetAtPath (pathName, typeof (Object));
	}
		
	public static void StartNameEditingIfProjectWindowExists (int instanceID, EndNameEditAction endAction, string pathName, Texture2D icon, string resourceFile)
	{
		ProjectBrowser pb = GetProjectBrowserIfExists();
		if (pb)
		{
			pb.Focus();
			pb.BeginPreimportedNameEditing (instanceID, endAction, pathName, icon, resourceFile);
			pb.Repaint();
		}
		else
		{
			if (!pathName.StartsWith ("assets/", System.StringComparison.CurrentCultureIgnoreCase))
				pathName = "Assets/" + pathName;
			EndNameEditAction(endAction, instanceID, pathName, resourceFile);
			Selection.activeObject = EditorUtility.InstanceIDToObject (instanceID);
		}
	}

	static ProjectBrowser GetProjectBrowserIfExists ()
	{
		return ProjectBrowser.s_LastInteractedProjectBrowser;
	}
		
	internal static void FrameObjectInProjectWindow (int instanceID)
	{
		ProjectBrowser pb = GetProjectBrowserIfExists();
		if (pb)
		{
			pb.FrameObject (instanceID, false);
		}
	}
	

	// InstanceIDs larger than this is considered a favorite by the projectwindows
	internal static int k_FavoritesStartInstanceID = 1000000000;
	internal static string k_DraggingFavoriteGenericData = "DraggingFavorite";
	internal static string k_IsFolderGenericData = "IsFolder";

	internal static bool IsFavoritesItem (int instanceID)
	{
		return instanceID >= k_FavoritesStartInstanceID;
	}

	internal static void StartDrag (int draggedInstanceID, List<int> selectedInstanceIDs)
	{
		DragAndDrop.PrepareStartDrag();

		string title = "";
		if (IsFavoritesItem (draggedInstanceID))
		{
			DragAndDrop.SetGenericData (k_DraggingFavoriteGenericData, draggedInstanceID);
			DragAndDrop.objectReferences = new UnityEngine.Object[] { }; // this IS required for dragging to work
		}
		else
		{
			bool isFolder = false;
			HierarchyProperty hierarchyProperty = new HierarchyProperty (HierarchyType.Assets);
			if (hierarchyProperty.Find (draggedInstanceID, null))
				isFolder = hierarchyProperty.isFolder;

			// Normal assets dragging
			DragAndDrop.objectReferences = GetDragAndDropObjects (draggedInstanceID, selectedInstanceIDs);
			DragAndDrop.SetGenericData (k_IsFolderGenericData, isFolder ? "isFolder" : "");
			string[] paths = GetDragAndDropPaths (draggedInstanceID, selectedInstanceIDs);
			if (paths.Length > 0)
				DragAndDrop.paths = paths;

			if (DragAndDrop.objectReferences.Length > 1)
				title = "<Multiple>";
			else
				title = ObjectNames.GetDragAndDropTitle (InternalEditorUtility.GetObjectFromInstanceID (draggedInstanceID));
		}

		DragAndDrop.StartDrag (title);
	}


	internal static Object[] GetDragAndDropObjects (int draggedInstanceID, List<int> selectedInstanceIDs)
	{
		if (selectedInstanceIDs.Contains (draggedInstanceID))
		{
			Object[] objs = new Object[selectedInstanceIDs.Count];
			for (int i=0; i<selectedInstanceIDs.Count; ++i)
				objs[i] = InternalEditorUtility.GetObjectFromInstanceID (selectedInstanceIDs[i]);
			return objs;
		}
		else
		{
			Object[] objs = { InternalEditorUtility.GetObjectFromInstanceID (draggedInstanceID) };
			return objs;		
		}
	}

	internal static string[] GetDragAndDropPaths (int draggedInstanceID, List<int> selectedInstanceIDs)
	{
		// Assets
		List<string> paths = new List<string>();
		foreach (int instanceID in selectedInstanceIDs)
		{
			if (AssetDatabase.IsMainAsset (instanceID))
			{
				string path = AssetDatabase.GetAssetPath (instanceID);
				paths.Add (path);
			}
		}

		string dragPath = AssetDatabase.GetAssetPath (draggedInstanceID);
		if (!string.IsNullOrEmpty (dragPath))
		{
			if (paths.Contains(dragPath))
			{
				return paths.ToArray();
			}
			else
			{
				return new [] { dragPath };
			}
		}
		return new string[0];
	}


	// Input the following list:
	//	assets/flesh/big
	//	assets/icons/duke
	//	assets/icons/duke/snake
	//	assets/icons/duke/zoo
	//
	// ... And the returned list becomes:
	//	assets/flesh
	//	assets/icons/duke
	public static string[] GetBaseFolders (string [] folders)
	{
		if (folders.Length < 2)
			return folders;
	
		List<string> result = new List<string> ();
		List<string> sortedFolders = new List<string> (folders);
		sortedFolders.Sort();
	
		// We assume sortedFolders is sorted with less first. E.g: {assets/, assets/icons}
		string curPath = sortedFolders[0];
		result.Add (curPath);
		for (int i=1; i<sortedFolders.Count; ++i)
		{
			if (sortedFolders[i].IndexOf (curPath) < 0)
			{
				// Add tested path if not part of current path and use tested path as new base
				result.Add (sortedFolders[i]);
				curPath = sortedFolders[i];
			}
		}
		return result.ToArray ();
	}


}

}
