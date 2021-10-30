using System.Collections.Generic;
using System.Linq;
using UnityEditorInternal;


namespace UnityEditor
{

// LazyTreeViewDataSource assumes that the Item tree only contains visible items, optimal for large data sets.
// Usage:
//    - Override FetchData () and build the tree with visible items with m_RootItem as root  (and and populate the m_VisibleRows List)
//	  - FetchData () is called every time the expanded state changes. 
//    - Configure showRootNode and rootIsCollapsable as wanted
//
// Note: if dealing with small trees consider using TreeViewDataSource instead: it assumes that the tree contains all items.

internal abstract class LazyTreeViewDataSource : TreeViewDataSource
{
	public LazyTreeViewDataSource(TreeView treeView)
		: base (treeView)
	{
	}

	// Return all ancestor items of the Item with 'id'
	protected abstract HashSet<int> GetParentsAbove (int id);

	// Return all descendant items that have children from the Item with 'id'
	protected abstract HashSet<int> GetParentsBelow(int id);

	override public TreeViewItem FindItem (int id)
	{
		// Get existing expanded in hashset
		HashSet<int> expandedSet = new HashSet<int>(expandedIDs);
		int orgSize = expandedSet.Count;

		// Get all parents above id
		HashSet<int> candidates = GetParentsAbove(id);

		// Add parent ids
		expandedSet.UnionWith(candidates);

		if (orgSize != expandedSet.Count)
		{
			// Bulk set expanded ids (is sorted in SetExpandedIDs)
			SetExpandedIDs(expandedSet.ToArray());

			// Refresh immediately if any Item was expanded
			if (m_NeedRefreshVisibleFolders)
				FetchData();
		}

		// Now find the item after we have expanded and created parent items
		return base.FindItem (id);			
	}


	// Override for special handling of recursion
	// We cannot recurse normally to tree Item children because we have not loaded children of collapsed items
	// therefore let client implement GetParentsBelow to fetch ids instead
	override public void SetExpandedWithChildren (TreeViewItem item, bool expand)
	{
		// Get existing expanded in hashset
		HashSet<int> oldExpandedSet = new HashSet<int>(expandedIDs);

		// Add all children expanded ids to hashset
		HashSet<int> candidates = GetParentsBelow (item.id);

		if (expand)		oldExpandedSet.UnionWith (candidates);
		else			oldExpandedSet.ExceptWith (candidates);

		// Bulk set expanded ids (is sorted in SetExpandedIDs)
		SetExpandedIDs (oldExpandedSet.ToArray ());

		// Keep for debugging
		// Debug.Log ("New expanded state (bulk): " + DebugUtils.ListToString(new List<int>(expandedIDs)));
	}

	public override bool SetExpanded (int id, bool expand)
	{
		if (base.SetExpanded (id, expand))
		{
			// Persist expanded state for ProjectBrowsers
			InternalEditorUtility.expandedProjectWindowItems = expandedIDs.ToArray ();
			return true;
		}
		return false;
	}
	
	// Get the flattened tree of visible items. Use GetFirstAndLastRowVisible to cull invisible items
	override public List<TreeViewItem> GetVisibleRows ()
	{
		// Cached for large trees...
		if (m_VisibleRows == null || m_NeedRefreshVisibleFolders)
		{
			FetchData(); // Only need to fetch visible data..

			m_NeedRefreshVisibleFolders = false;

			m_TreeView.Repaint();
		}
		return m_VisibleRows;
	}
}


}
