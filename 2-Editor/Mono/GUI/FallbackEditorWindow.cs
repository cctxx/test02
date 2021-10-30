using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Reflection;

namespace UnityEditor
{

internal class FallbackEditorWindow : EditorWindow
{
	FallbackEditorWindow ()
	{
		title = "Failed to load";
	}
	
	void OnEnable ()
	{
		title = "Failed to load";
	}

	void OnGUI ()
	{
		GUILayout.BeginVertical();
		GUILayout.FlexibleSpace ();
	
			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace ();
				GUILayout.Label ("EditorWindow could not be loaded because the script is not found in the project", "WordWrapLabel");
			GUILayout.FlexibleSpace ();
			GUILayout.EndHorizontal();
	
		GUILayout.FlexibleSpace ();
		GUILayout.EndVertical();
	}
}

} // namespace