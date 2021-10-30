using System.IO;
using System.Collections.Generic;
using NUnit.Framework;
using UnityEngine;
using UnityEditorInternal;


namespace UnityEditor
{

// Used for type check
class SearchFilterTreeItem : TreeViewItem
{
	bool m_IsFolder;
	public SearchFilterTreeItem (int id, int depth, TreeViewItem parent, string displayName, bool isFolder)
		: base (id, depth, parent, displayName)
	{
		m_IsFolder = isFolder;
	}
	public bool isFolder {get {return m_IsFolder;}}
}

//------------------------------------------------
// GUI section

internal class ProjectBrowserColumnOneTreeViewGUI : AssetsTreeViewGUI
{
	const float k_DistBetweenRootTypes = 15f;
	Texture2D k_FavoritesIcon = EditorGUIUtility.FindTexture ("Favorite Icon");
	Texture2D k_FavoriteFolderIcon = EditorGUIUtility.FindTexture ("FolderFavorite Icon");
	Texture2D k_FavoriteFilterIcon = EditorGUIUtility.FindTexture ("Search Icon");
	bool m_IsCreatingSavedFilter = false;

	public ProjectBrowserColumnOneTreeViewGUI (TreeView treeView) : base (treeView)
	{
			
	}

	// ------------------
	// Size section


	override public Vector2 GetTotalSize (List<TreeViewItem> rows)
	{
		Vector2 totalSize = base.GetTotalSize (rows);

		totalSize.y += k_DistBetweenRootTypes * 1; // assumes that we have two root

		return totalSize;
	}

	override public float GetTopPixelOfRow (int row, List<TreeViewItem> rows)
	{
		float topPixel = row * k_LineHeight;

		// Assumes Saved filter are second root
		TreeViewItem item = rows[row];
		ProjectBrowser.ItemType type = ProjectBrowser.GetItemType (item.id);
		if (type == ProjectBrowser.ItemType.Asset)
			topPixel += k_DistBetweenRootTypes;
		
		return topPixel;
	}

	override public float GetHeightOfLastRow()
	{
		return k_LineHeight;
	}

	override public int GetNumRowsOnPageUpDown(TreeViewItem fromItem, bool pageUp, float heightOfTreeView)
	{
		return (int)Mathf.Floor(heightOfTreeView / k_LineHeight) - 1; // -1 is fast fix for space between roots
	}

	// Should return the row number of the first and last row thats fits in the pixel rect defined by top and height
	override public void GetFirstAndLastRowVisible (List<TreeViewItem> rows, float topPixel, float heightInPixels, out int firstRowVisible, out int lastRowVisible)
	{
		firstRowVisible = (int)Mathf.Floor(topPixel / k_LineHeight);
		lastRowVisible = firstRowVisible + (int)Mathf.Ceil(heightInPixels / k_LineHeight);

		float rowsPerSpaceBetween = k_DistBetweenRootTypes / k_LineHeight;
		firstRowVisible -= (int)Mathf.Ceil(2 * rowsPerSpaceBetween); // for now we just add extra rows to ensure all rows are visible
		lastRowVisible += (int)Mathf.Ceil(2 * rowsPerSpaceBetween); 

		firstRowVisible = Mathf.Max(firstRowVisible, 0);
		lastRowVisible = Mathf.Min(lastRowVisible, rows.Count - 1);
	}

	// ------------------
	// Row Gui section

	override public Rect OnRowGUI (TreeViewItem item, int row, float rowWidth, bool selected, bool focused)
	{
		float yPos = row * k_LineHeight;
		ProjectBrowser.ItemType type = ProjectBrowser.GetItemType (item.id);
		if (type == ProjectBrowser.ItemType.Asset)
			yPos += k_DistBetweenRootTypes;

		Rect rowRect = new Rect(0, yPos, rowWidth, k_LineHeight);
		bool useBoldFont = IsVisibleRootNode (item);
		DoNodeGUI (rowRect, item, selected, focused, useBoldFont);
		return rowRect;
	}

	bool IsVisibleRootNode (TreeViewItem item)
	{
		return (m_TreeView.data as ProjectBrowserColumnOneTreeViewDataSource).IsVisibleRootNode (item);
	}

	protected override Texture GetIconForNode (TreeViewItem item)
	{
		if (item != null && item.icon != null)
			return item.icon;	

		SearchFilterTreeItem searchFilterItem = item as SearchFilterTreeItem;
		if (searchFilterItem != null)
		{
			if (IsVisibleRootNode (item))
				return k_FavoritesIcon;
			if (searchFilterItem.isFolder)
				return k_FavoriteFolderIcon;
			else
				return k_FavoriteFilterIcon;
		}
		return base.GetIconForNode (item);
	}

	public static float GetListAreaGridSize ()
	{
		float previewSize = -1f;
		if (ProjectBrowser.s_LastInteractedProjectBrowser != null)
			previewSize = ProjectBrowser.s_LastInteractedProjectBrowser.listAreaGridSize;
		return previewSize;
	}
	
	
	virtual internal void BeginCreateSavedFilter (SearchFilter filter)
	{
		string savedFilterName = "New Saved Search";

		m_IsCreatingSavedFilter = true;
		int instanceID = SavedSearchFilters.AddSavedFilter (savedFilterName, filter, GetListAreaGridSize ()); 
		m_TreeView.Frame (instanceID, true, false);

		// Start naming the asset
		m_TreeView.state.renameOverlay.BeginRename (savedFilterName, instanceID, 0f);
	}

	override protected void RenameEnded ()
	{
		int instanceID = GetRenameOverlay ().userData;
		ProjectBrowser.ItemType type = ProjectBrowser.GetItemType (instanceID);

		if (m_IsCreatingSavedFilter)
		{
			// Create saved filter
			m_IsCreatingSavedFilter = false;

			if ( GetRenameOverlay ().userAcceptedRename)
			{
				SavedSearchFilters.SetName (instanceID,  GetRenameOverlay ().name);
				m_TreeView.SetSelection (new[] { instanceID }, true);
			}
			else
				SavedSearchFilters.RemoveSavedFilter (instanceID);
		}
		else if (type == ProjectBrowser.ItemType.SavedFilter)
		{
			// Renamed saved filter
			if ( GetRenameOverlay ().userAcceptedRename)
			{
				SavedSearchFilters.SetName (instanceID,  GetRenameOverlay ().name);
			}
		}
		else
		{
			// Let base handle renaming of folders
			base.RenameEnded ();

			// Ensure to sync filter to new folder name (so we still show the contents of the folder)
			if (GetRenameOverlay ().userAcceptedRename)
				m_TreeView.NotifyListenersThatSelectionChanged ();
		}
		
	}
}


//------------------------------------------------
// DataSource section

internal class ProjectBrowserColumnOneTreeViewDataSource : TreeViewDataSource
{
	static string kProjectBrowserString = "ProjectBrowser";

	public ProjectBrowserColumnOneTreeViewDataSource (TreeView treeView) : base(treeView)
	{
		showRootNode = false;
		rootIsCollapsable = false;
		SavedSearchFilters.AddChangeListener (ReloadData); // We reload on change
	}

	public override bool SetExpanded (int id, bool expand)
	{
		if (base.SetExpanded (id, expand))
		{
			// Persist expanded state for ProjectBrowsers
			InternalEditorUtility.expandedProjectWindowItems = expandedIDs.ToArray();

			// Set global expanded state of roots (Assets folder and Favorites root)
			foreach (TreeViewItem item in m_RootItem.children)
				if (item.id == id)
					EditorPrefs.SetBool(kProjectBrowserString + item.displayName, expand);

			return true;
		}
		return false;
	}

	public override bool IsExpandable (TreeViewItem item)
	{
		return item.children.Length > 0 && (item != m_RootItem || rootIsCollapsable);
	}

	public override bool CanBeMultiSelected (TreeViewItem item)
	{
		return ProjectBrowser.GetItemType (item.id) != ProjectBrowser.ItemType.SavedFilter;
	}

	public override bool CanBeParent (TreeViewItem item)
	{
		return !(item is SearchFilterTreeItem) || SavedSearchFilters.AllowsHierarchy ();
	}


	public bool IsVisibleRootNode (TreeViewItem item)
	{
		// The main root Item is invisible the next level is visible root items
		return (item.parent != null && item.parent.parent == null);
	}

	public override bool IsRenamingItemAllowed (TreeViewItem item)
	{
		// The 'Assets' root and 'Filters' roots are not allowed to be renamed
		if (IsVisibleRootNode (item))
			return false;

		return base.IsRenamingItemAllowed (item);
	}

	public static int GetAssetsFolderInstanceID ()
	{
		string rootDir = "Assets";
		string guid = AssetDatabase.AssetPathToGUID (rootDir);
		int instanceID = AssetDatabase.GetInstanceIDFromGUID (guid);
		return instanceID;
	}

	public override void FetchData ()
	{
		m_RootItem = new TreeViewItem (System.Int32.MaxValue, 0, null, "Invisible Root Item");
		SetExpanded (m_RootItem, true); // ensure always visible

		// We want three roots: Favorites, Assets, and Saved Filters
		List<TreeViewItem> visibleRoots = new List<TreeViewItem>();
		
		// Fetch asset folders
		int assetsFolderInstanceID = GetAssetsFolderInstanceID();
		int depth = 0;
		string displayName = "Assets"; //CreateDisplayName (assetsFolderInstanceID);
		TreeViewItem assetRootItem = new TreeViewItem (assetsFolderInstanceID, depth, m_RootItem, displayName);
		ReadAssetDatabase (assetRootItem, depth + 1); 

		// Fetch saved filters
		TreeViewItem savedFiltersRootItem = SavedSearchFilters.ConvertToTreeView ();
		savedFiltersRootItem.parent = m_RootItem;

		// Order 
		visibleRoots.Add (savedFiltersRootItem);
		visibleRoots.Add (assetRootItem);
		m_RootItem.children = visibleRoots.ToArray();

		// Get global expanded state of roots
		foreach (TreeViewItem item in m_RootItem.children)
		{
			bool expanded = EditorPrefs.GetBool (kProjectBrowserString + item.displayName, true);
			SetExpanded (item, expanded);
		}

		m_NeedRefreshVisibleFolders = true;
	}

	private void ReadAssetDatabase (TreeViewItem parent, int baseDepth)
	{
		// Read from Assets directory
		IHierarchyProperty property = new HierarchyProperty (HierarchyType.Assets);
		property.Reset ();

		Texture2D folderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);
		Texture2D emptyFolderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.emptyFolderIconName);

		List<TreeViewItem> allFolders = new List<TreeViewItem>();
		while (property.Next (null))
		{
			if (property.isFolder)
			{
				TreeViewItem folderItem = new TreeViewItem (property.instanceID, baseDepth + property.depth, null, property.name);
				folderItem.icon = property.hasChildren ? folderIcon : emptyFolderIcon;
				allFolders.Add (folderItem);
			}
		}

		// Fix references
		TreeViewUtility.SetChildParentReferences (allFolders, parent);
	}
}

internal class ProjectBrowserColumnOneTreeViewDragging : AssetOrGameObjectTreeViewDragging
{
	public ProjectBrowserColumnOneTreeViewDragging (TreeView treeView) : base (treeView, HierarchyType.Assets)
	{

	}

	public override void StartDrag (TreeViewItem draggedItem, List<int> draggedItemIDs)
	{
		if (SavedSearchFilters.IsSavedFilter (draggedItem.id))
		{
			// Root Filters Item is not allowed to be dragged
			if (draggedItem.id == SavedSearchFilters.GetRootInstanceID ())
				return;
		}

		ProjectWindowUtil.StartDrag (draggedItem.id, draggedItemIDs);
	}

	public override DragAndDropVisualMode DoDrag (TreeViewItem parentItem, TreeViewItem targetItem, bool perform)
	{
		if (targetItem == null)
			return DragAndDropVisualMode.None;

		object savedFilterData = DragAndDrop.GetGenericData (ProjectWindowUtil.k_DraggingFavoriteGenericData);
		
		// Dragging saved filter
		if (savedFilterData != null)
		{
			int instanceID = (int)savedFilterData;
			if (targetItem is SearchFilterTreeItem && parentItem is SearchFilterTreeItem)// && targetItem.id != draggedInstanceID && parentItem.id != draggedInstanceID) 
			{
				bool validMove = SavedSearchFilters.CanMoveSavedFilter (instanceID, parentItem.id, targetItem.id, true);
				if (validMove && perform)
				{
					SavedSearchFilters.MoveSavedFilter (instanceID, parentItem.id, targetItem.id, true);
				}
				return validMove ? DragAndDropVisualMode.Copy : DragAndDropVisualMode.None;
			}
			return DragAndDropVisualMode.None;
		}
		// Dragging of folders into filters
		else
		{
			// Check if we are dragging a single folder
			if (targetItem is SearchFilterTreeItem && parentItem is SearchFilterTreeItem)
			{
				string genericData = DragAndDrop.GetGenericData (ProjectWindowUtil.k_IsFolderGenericData) as string;
				if (genericData == "isFolder")
				{
					if (perform)
					{
						Object[] objs = DragAndDrop.objectReferences;
						if (objs.Length > 0)
						{
							HierarchyProperty hierarchyProperty = new HierarchyProperty (HierarchyType.Assets);
							if (hierarchyProperty.Find (objs[0].GetInstanceID (), null))
							{
								SearchFilter searchFilter = new SearchFilter ();
								string path = AssetDatabase.GetAssetPath (hierarchyProperty.instanceID);
								if (!string.IsNullOrEmpty (path))
								{
									searchFilter.folders = new [] {path};
									bool addAsChild = targetItem == parentItem;

									float previewSize = ProjectBrowserColumnOneTreeViewGUI.GetListAreaGridSize ();
									int instanceID = SavedSearchFilters.AddSavedFilterAfterInstanceID (hierarchyProperty.name, searchFilter, previewSize, targetItem.id, addAsChild);
									Selection.activeInstanceID = instanceID;
								}
								else
								{
									Debug.Log ("Could not get asset path from id " + hierarchyProperty.name);
								}
							}
						}
					}
					return DragAndDropVisualMode.Copy; // Allow dragging folders to filters
					
				}
				return DragAndDropVisualMode.None; // Assets that are not folders are not allowed to be dragged to filters
			}
		}
		//  Assets are handled by base
		return base.DoDrag (parentItem, targetItem, perform);
	}
}


}


