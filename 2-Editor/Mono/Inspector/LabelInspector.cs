
using System;
using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Linq;
using Object = UnityEngine.Object;

namespace UnityEditor
{

internal class LabelInspector 
{
		
	static Object	currentAsset = null;
	static bool		currentChanged = false;
	
	public static void OnDisable()
	{
		SaveLabels();
	}
	
	public static void OnLostFocus()
	{
		SaveLabels();
	}
	
	public static void SaveLabels() 
	{
		if (currentChanged && m_AssetLabels != null && currentAsset != null)
		{
			AssetDatabase.SetLabels(currentAsset, (from i in m_AssetLabels.m_ListElements where i.selected select i.text).ToArray() );
			currentChanged = false;
		}
	}

	public static void AssetLabelListCallback (PopupList.ListElement element)
	{
		element.selected = !element.selected;
		currentChanged = true;
		SaveLabels();
		InspectorWindow.RepaintAllInspectors();
	}

	static PopupList.InputData m_AssetLabels;

	public static void InitLabelCache(Object asset)
	{
		// Init only if new asset
		if ((asset != currentAsset) || m_AssetLabels == null)
		{
			string[] currentLabels = AssetDatabase.GetLabels(asset);


			Dictionary<string,float> tags = AssetDatabase.GetAllLabels();
			m_AssetLabels = new PopupList.InputData
			                {
			                	m_CloseOnSelection = false,
			                	m_AllowCustom = true,
			                	m_OnSelectCallback = AssetLabelListCallback,
			                	m_MaxCount = 15,
			                	m_SortAlphabetically = true
			                };

			foreach (var pair in tags)
			{
				PopupList.ListElement element = m_AssetLabels.NewOrMatchingElement(pair.Key);
				if ( element.filterScore < pair.Value )
					element.filterScore = pair.Value;
						
				element.selected = currentLabels.Any( label => string.Equals(label, pair.Key, StringComparison.OrdinalIgnoreCase) );
				
			}

		}
	
		currentAsset = asset;
		currentChanged = false;

	}
		
	private static int m_MaxShownLabels = 10;
		
	public static void OnLabelGUI(Object asset) 
	{
		InitLabelCache(asset);
					
		GUIStyle tmp = EditorStyles.tagTextField;
		
		float height = EditorStyles.tagTextFieldButton.CalcHeight(GUIContent.none, 20);
		float dropDownWidth = 20;
			
		Rect labelsRect = GUILayoutUtility.GetRect (0, 10240, 0, height);			
		labelsRect.width -= dropDownWidth; // reserve some width for the dropdown
		
		EditorGUILayout.BeginHorizontal();
		
		foreach(  GUIContent content in (from i in m_AssetLabels.m_ListElements where i.selected orderby i.text.ToLower() select i.m_Content).Take(m_MaxShownLabels) ) 
		{
			Rect rt = GUILayoutUtility.GetRect(content, EditorStyles.tagTextField);		
			if ( Event.current.type == EventType.Repaint && rt.xMax >= labelsRect.xMax)
				break;
			GUI.Label (rt, content, tmp);
		}

		GUILayout.FlexibleSpace();

		Rect r = GUILayoutUtility.GetRect(20, 20, 20, 20);
		r.x = labelsRect.xMax;
		if (EditorGUI.ButtonMouseDown(r, GUIContent.none, FocusType.Passive, EditorStyles.tagTextFieldButton))
		{
			if (PopupList.isOpen)
				PopupList.CloseList ();
		
			if (PopupList.ShowAtPosition(new Vector2(r.x, r.y), m_AssetLabels, PopupList.Gravity.Bottom))
			{
					GUIUtility.ExitGUI();
			}
			HandleUtility.Repaint();
		}

		EditorGUILayout.EndHorizontal();
							
	}
	
}

}
