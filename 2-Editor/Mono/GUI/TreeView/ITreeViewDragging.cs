using System.Collections.Generic;
using UnityEngine;


namespace UnityEditor
{


// The TreeView requires implementations from the following three interfaces:
//	ITreeViewDataSource:	Should handle data fetching and data structure
//	ITreeViewGUI:			Should handle visual representation of TreeView and input handling
//	ITreeViewDragging		Should handle dragging, temp expansion of items, allow/disallow dropping
// The TreeView handles:	Navigation, Item selection and initiates dragging


// DragNDrop interface for tree views
internal interface ITreeViewDragging
{
	void StartDrag (TreeViewItem draggedItem, List<int> draggedItemIDs);
	bool DragElement (TreeViewItem targetItem, Rect targetItemRect);				// 'targetItem' is null when not hovering over any target Item.  Returns true if drag was handled.
	void DragCleanup (bool revertExpanded);
	int GetDropTargetControlID ();
	int GetRowMarkerControlID ();
}

}
