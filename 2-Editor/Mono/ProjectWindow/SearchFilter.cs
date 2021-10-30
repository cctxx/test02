using UnityEngine;
using System.Collections.Generic;
using System.Text.RegularExpressions;


namespace UnityEditor
{
	[System.Serializable]
	public class SearchFilter
	{
		public enum SearchArea
		{
			AllAssets,
			SelectedFolders, 
			AssetStore
		}

		public enum State
		{
			EmptySearchFilter,
			FolderBrowsing,
			SearchingInAllAssets,
			SearchingInFolders,
			SearchingInAssetStore
		}

		// Searching
		[SerializeField]
		private string m_NameFilter = "";		
		[SerializeField]
		private string[] m_ClassNames = new string[0];
		[SerializeField]
		private string[] m_AssetLabels = new string[0];
		[SerializeField]
		private int[] m_ReferencingInstanceIDs = new int[0];
		[SerializeField]
		private bool m_ShowAllHits = false;			// If true then just one filter must match to show an object, if false then all filters must match to show an object
		[SerializeField]
		SearchArea m_SearchArea = SearchArea.AllAssets;

		// Folder browsing
		[SerializeField]
		private string[] m_Folders = new string[0];
	
		// Interface
		public string nameFilter {get {return m_NameFilter;} set { m_NameFilter = value;} }
		public string[] classNames { get {return m_ClassNames;} set {m_ClassNames = value;}}
		public string[] assetLabels {get {return m_AssetLabels;} set {m_AssetLabels = value;}}
		public int[] referencingInstanceIDs { get { return m_ReferencingInstanceIDs; } set { m_ReferencingInstanceIDs = value; } }
		public bool showAllHits { get { return m_ShowAllHits; } set { m_ShowAllHits = value; }}
		public string[] folders { get {return m_Folders;} set {m_Folders = value;}}
		public SearchArea searchArea {  get { return m_SearchArea;} set { m_SearchArea = value;}}

		public void ClearSearch ()
		{
			m_NameFilter = "";
			m_ClassNames = new string[0];
			m_AssetLabels = new string[0];
			m_ReferencingInstanceIDs = new int[0];
			m_ShowAllHits = false;
		}

		bool IsNullOrEmtpy <T> (T [] list)
		{
			return (list == null || list.Length == 0);
		}

		public State GetState ()
		{
			bool searchActive = !string.IsNullOrEmpty(m_NameFilter) ||
								!IsNullOrEmtpy (m_AssetLabels) || 
								!IsNullOrEmtpy (m_ClassNames) || 
								!IsNullOrEmtpy (m_ReferencingInstanceIDs);

			bool foldersActive = !IsNullOrEmtpy (m_Folders);

			if (searchActive)
			{
				if (m_SearchArea == SearchArea.AssetStore)
					return State.SearchingInAssetStore;
				
				if (foldersActive && m_SearchArea == SearchArea.SelectedFolders)
					return State.SearchingInFolders;

				return State.SearchingInAllAssets;
			}
			else if (foldersActive)
			{
				return State.FolderBrowsing;
			}

			return State.EmptySearchFilter;
		}

		public bool IsSearching ()
		{
			State state = GetState ();
			return (state == State.SearchingInAllAssets || state == State.SearchingInFolders || state == State.SearchingInAssetStore);
		}

		public bool SetNewFilter (SearchFilter newFilter)
		{
			bool changed = false;

			if (newFilter.m_NameFilter != m_NameFilter)
			{
				m_NameFilter = newFilter.m_NameFilter;
				changed = true;
			}

			if (newFilter.m_ClassNames != m_ClassNames)
			{
				m_ClassNames = newFilter.m_ClassNames;
				changed = true;
			}			

			if (newFilter.m_Folders != m_Folders)
			{
				m_Folders = newFilter.m_Folders;
				changed = true;
			}

			if (newFilter.m_AssetLabels != m_AssetLabels)
			{
				m_AssetLabels = newFilter.m_AssetLabels;
				changed = true;
			}

			if (newFilter.m_ReferencingInstanceIDs != m_ReferencingInstanceIDs)
			{
				m_ReferencingInstanceIDs = newFilter.m_ReferencingInstanceIDs;
				changed = true;
			}

			if (newFilter.m_SearchArea != m_SearchArea)
			{
				m_SearchArea = newFilter.m_SearchArea;
				changed = true;
			}

			m_ShowAllHits = newFilter.m_ShowAllHits;
			

			return changed;
		}

		// Debug
		public override string ToString ()
		{
			string result = "SearchFilter: ";

			result += string.Format("[Area: {0}, State: {1}]", m_SearchArea, GetState());

			if (!string.IsNullOrEmpty(m_NameFilter))
				result += "[Name: " + m_NameFilter + "]";

			if (m_AssetLabels != null && m_AssetLabels.Length > 0)
				result += "[Labels: " + m_AssetLabels[0] + "]";

			if (m_ClassNames != null && m_ClassNames.Length > 0)
				result += "[Types: " +m_ClassNames[0] + " (" + m_ClassNames.Length + ")]";

			if (m_ReferencingInstanceIDs != null && m_ReferencingInstanceIDs.Length > 0)
				result += "[RefIDs: " + m_ReferencingInstanceIDs[0] + "]";

			if (m_Folders != null && m_Folders.Length > 0)
				result += "[Folders: " + m_Folders[0] + "]";

			result += "[ShowAllHits: " + showAllHits + "]";
			return result;
		}

		internal string FilterToSearchFieldString ()
		{
			string result = "";
			if (!string.IsNullOrEmpty(m_NameFilter))
				result += m_NameFilter;

			// See SearchUtility.cs for search tokens
			AddToString ("t:", m_ClassNames, ref result);
			AddToString ("l:", m_AssetLabels, ref result);
			return result;
		}

		void AddToString <T> (string prefix, T[] list, ref string result)
		{
			if (list == null)
				return;
			if (result == null)
				result = "";

			foreach (T item in list)
			{
				if (!string.IsNullOrEmpty (result))
					result += " ";
				result += prefix + item;
			}
		}

		// Keeps current SearchArea
		internal void SearchFieldStringToFilter (string searchString)
		{
			ClearSearch ();

			if (string.IsNullOrEmpty (searchString))
				return;

			SearchUtility.ParseSearchString (searchString, this);
		}

		internal static SearchFilter CreateSearchFilterFromString (string searchText)
		{
			SearchFilter searchFilter = new SearchFilter ();
			SearchUtility.ParseSearchString (searchText, searchFilter);
			return searchFilter;
		}

		// Split name filter into words by whitespace and handle quotes
		// E.g 'one man' becomes:	'one', 'man'
		// E.g '"one man' becomes:	'one', 'man'
		// E.g '"one man"' becomes:	'one man'
		public string[] SplitNameFilter ()
		{
			List<string> words = new List <string>();
			foreach (Match m in Regex.Matches(m_NameFilter, @"(?<match>\w+)|\""(?<match>[\w\s]*)"""))
			{
				words.Add (m.Value.Replace ("\"", ""));
			}
			return words.ToArray ();
		}
	} // end of class SearchFilter
}	

