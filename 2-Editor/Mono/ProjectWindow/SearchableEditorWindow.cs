using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;

namespace UnityEditor {

public class SearchableEditorWindow : EditorWindow
{
	public enum SearchMode { All, Name, Type, Label };
	public enum SearchModeHierarchyWindow { All, Name, Type };
	
	internal static SearchFilter CreateFilter (string searchString, SearchMode searchMode)
	{
		SearchFilter filter = new SearchFilter();
		if (string.IsNullOrEmpty (searchString))
			return filter;
			
		switch (searchMode)
		{
			case SearchMode.All:
				if (!SearchUtility.ParseSearchString (searchString, filter))
				{
					// Use search string for all SearchModes: name, types and labels
					filter.nameFilter = searchString;  
					filter.classNames = new[] { searchString };
					filter.assetLabels = new[] { searchString };
					filter.showAllHits = true;
				}
				break;
			case SearchMode.Name:
				filter.nameFilter = searchString;
				break;
			case SearchMode.Type:
				filter.classNames = new[] { searchString };
				break;
			case SearchMode.Label:
				filter.assetLabels = new[] { searchString };
				break;
		}

		return filter;
	}

	static List<SearchableEditorWindow> searchableWindows = new List<SearchableEditorWindow>();

	internal HierarchyType m_HierarchyType = HierarchyType.Assets;
	internal string m_SearchFilter = "";
	internal SearchMode m_SearchMode = SearchMode.All;
	bool m_FocusSearchField = false;
	bool m_HasSearchFilterFocus = false;

	[MenuItem("Assets/Find References In Scene", false, 25)]
	private static void OnSearchForReferences()
	{
 		string searchFilter;
 
 		int instanceID = Selection.activeInstanceID;
 
 		// only main assets have unique paths (remove "Assets" to make string simpler)
 		string path = AssetDatabase.GetAssetPath(instanceID).Substring(7);
 		if (path.IndexOf(' ') != -1)
 			path = '"' + path + '"';
 
 		if (AssetDatabase.IsMainAsset(instanceID))
 			searchFilter = "ref:" + path;
 		else
 			searchFilter = "ref:" + instanceID + ":" + path;
 		 
 		foreach (SearchableEditorWindow sw in searchableWindows)
 		{
 			if (sw.m_HierarchyType == HierarchyType.GameObjects)
 			{
 				sw.SetSearchFilter (searchFilter, SearchMode.All, false);
 				sw.m_HasSearchFilterFocus = true;
 				sw.Repaint();
 			}
 		}
	}

	[MenuItem("Assets/Find References In Scene", true)]
	private static bool OnSearchForReferencesValidate()
	{
		Object obj = Selection.activeObject;
		if (obj != null)
		{
			if (AssetDatabase.Contains(obj))
			{
				string path = AssetDatabase.GetAssetPath(obj);
				return !System.IO.Directory.Exists(path);
			}
		}

		return false;
	}

	virtual public void OnEnable ()
	{
		searchableWindows.Add (this);
	}

    virtual public void OnDisable()
	{
		searchableWindows.Remove (this);
	}

	internal bool hasSearchFilter
	{
		get { return m_SearchFilter != ""; }
	}
	
	internal bool hasSearchFilterFocus
	{
		get { return m_HasSearchFilterFocus; }
		set { m_HasSearchFilterFocus = value; }
	}
	
	internal SearchMode searchMode
	{
		get { return m_SearchMode; }
		set { m_SearchMode = value; }
	}

	internal void FocusSearchField()
	{
		m_FocusSearchField = true;
	}
	
	internal void ClearSearchFilter ()
	{
		SetSearchFilter("", m_SearchMode, true);
		// Reset current editor. This is needed, so if the user types into a search field, and then 
		// a new object is selected, which is not in the filter, with the editor still having keyboard focus,
		// the search field gets properly cleared.
		
		if (EditorGUI.s_RecycledEditor != null)
			EditorGUI.s_RecycledEditor.controlID = 0;
	}
	
	internal void SelectPreviousSearchResult()
	{
		foreach(SearchableEditorWindow sw in searchableWindows)
		{
			if (sw is BaseProjectWindow && sw.m_HierarchyType == m_HierarchyType)
			{
				((BaseProjectWindow)sw).SelectPrevious();
				return;
			}
		}
	}

	internal void SelectNextSearchResult()
	{
		foreach(SearchableEditorWindow sw in searchableWindows)
		{
			if (sw is BaseProjectWindow && sw.m_HierarchyType == m_HierarchyType)
			{
				((BaseProjectWindow)sw).SelectNext();
				return;
			}
		}
	}
	
	internal virtual void SetSearchFilter (string searchFilter, SearchMode searchMode, bool setAll)
	{
		m_SearchMode = searchMode;
		m_SearchFilter = searchFilter;
		
		if (setAll)
		{
			foreach(SearchableEditorWindow sw in searchableWindows)
			{
				if (sw != this && sw.m_HierarchyType == m_HierarchyType && sw.m_HierarchyType != HierarchyType.Assets)
					sw.SetSearchFilter (m_SearchFilter, m_SearchMode, false);
			}
		}
		m_HasSearchFilterFocus = false;
		Repaint();
		EditorApplication.Internal_CallSearchHasChanged ();
	}

	internal virtual void ClickedSearchField()
	{
	}
	
	internal void SearchFieldGUI()
	{
		Rect rect = GUILayoutUtility.GetRect (0, EditorGUILayout.kLabelFloatMaxW * 1.5f, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.toolbarSearchField);

		if (Event.current.type == EventType.MouseDown && rect.Contains (Event.current.mousePosition))
			ClickedSearchField();	

		GUI.SetNextControlName ("SearchFilter");
		if (m_FocusSearchField)
		{
			EditorGUI.FocusTextInControl ("SearchFilter");
			if (Event.current.type == EventType.Repaint)
				m_FocusSearchField = false;
		}

		int searchMode = (int)m_SearchMode;

		if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Escape && GUI.GetNameOfFocusedControl () == "SearchFilter")
			SetSearchFilter ("", (SearchMode)searchMode, true);

		string oldSearchFilter = "";
		if (m_HasSearchFilterFocus)
			oldSearchFilter = m_SearchFilter;
		
		string[] enumStrings = System.Enum.GetNames(m_HierarchyType == HierarchyType.GameObjects ? typeof(SearchModeHierarchyWindow) : typeof(SearchMode));
		string searchFilter = EditorGUI.ToolbarSearchField(rect, enumStrings, ref searchMode, oldSearchFilter);

		if (searchFilter != oldSearchFilter || searchMode != (int)m_SearchMode)
		{
			SetSearchFilter (searchFilter, (SearchMode)searchMode, true);		
			m_HasSearchFilterFocus = true;
		}
			
		if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Escape && m_SearchFilter !="" && GUIUtility.hotControl == 0)
		{
			m_SearchFilter = "";
			SetSearchFilter (searchFilter, (SearchMode)searchMode, true);	
			Event.current.Use();
		}
	}
}

}
