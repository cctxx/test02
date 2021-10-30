using System.Collections.Generic;
using UnityEngine;

// The TreeView requires implementations from the following three interfaces:
//	ITreeViewDataSource:	Should handle data fetching, build the tree/data structure and hold expanded state
//	ITreeViewGUI:			Should handle visual representation of TreeView and input handling
//	ITreeViewDragging		Should handle dragging, temp expansion of items, allow/disallow dropping
// The TreeView handles:	Navigation, Item selection and initiates dragging


namespace UnityEditor
{

internal interface ITreeViewDataSource
{
	// Return root of tree
	TreeViewItem root { get; }

	// Reload data
	void ReloadData ();

	// Find Item by id
	TreeViewItem FindItem (int id);

	// Get the flattened tree of visible items. Use ITreeViewGUI.GetFirstAndLastRowVisible to cull invisible items
	List<TreeViewItem> GetVisibleRows();

	// Expand / collapse interface
	// The DataSource has the interface for this because it should be able to rebuild
	// tree when expanding
	void SetExpandedWithChildren (TreeViewItem item, bool expand);
	void SetExpanded (TreeViewItem item, bool expand);
	bool IsExpanded (TreeViewItem item);
	bool IsExpandable (TreeViewItem item);
	int[] GetExpandedIDs ();
	void SetExpandedIDs (int[] ids);
	
	// Selection
	bool CanBeMultiSelected (TreeViewItem item);
	bool CanBeParent (TreeViewItem item);

	// Renaming
	bool IsRenamingItemAllowed (TreeViewItem item);
	void InsertFakeItem (int id, int parentID, string name, Texture2D icon);
	void RemoveFakeItem ();
	bool HasFakeItem ();
}

} // namespace UnityEditor
