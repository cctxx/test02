using UnityEngine;
using UnityEditor;
using System.Collections;

// NOTE: 
// This file should only contain internal functions of the EditorGUILayout class

namespace UnityEditor
{
	public sealed partial class EditorGUILayout
	{
		internal static bool IconButton (int id, GUIContent content, GUIStyle style, params GUILayoutOption[] options)
		{ 
			return EditorGUI.IconButton (id, GUILayoutUtility.GetRect(content, style, options), content, style); 
		}

		internal static void GameViewSizePopup ( GameViewSizeGroupType groupType, int selectedIndex, System.Action<int, object> itemClickedCallback, GUIStyle style, params GUILayoutOption[] options)
		{
			Rect buttonRect =  GetControlRect(false, EditorGUI.kSingleLineHeight, style, options);
			EditorGUI.GameViewSizePopup (buttonRect, groupType, selectedIndex, itemClickedCallback, style);
		}
	}
}
