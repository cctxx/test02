using System.Collections.Generic;
using UnityEngine;

// The TreeView requires implementations from the following three interfaces:
//	ITreeViewDataSource:	Should handle data fetching and data structure
//	ITreeViewGUI:			Should handle visual representation of TreeView and input handling
//	ITreeViewDragging		Should handle dragging, temp expansion of items, allow/disallow dropping
// The TreeView handles:	Navigation, Item selection and initiates dragging


namespace UnityEditor
{

internal interface ITreeViewGUI
{
	// Should return the size of the entire visible content (in pixels)
	Vector2 GetTotalSize (List<TreeViewItem> rows);

	// Should return the row number of the first and last row thats fits between top pixel and the height of the window
	void GetFirstAndLastRowVisible (List<TreeViewItem> rows, float topPixel, float heightInPixels, out int firstRowVisible, out int lastRowVisible);

	// Get top of row. Supports variable heights of rows. (used to ensure visible in scrollview). 
	float GetTopPixelOfRow (int row, List<TreeViewItem> rows);
	float GetHeightOfLastRow ();
	int GetNumRowsOnPageUpDown (TreeViewItem fromItem, bool pageUp, float heightOfTreeView);

	// OnGUI: Implement to handle TreeView OnGUI 
	Rect OnRowGUI(TreeViewItem item, int row, float rowWidth, bool selected, bool focused);	// should return the rect used by the Item
	void BeginRowGUI();																			// use for e.g clearing state before OnRowGUI calls
	void EndRowGUI();																			// use for handling stuff after all rows have had their OnRowGUI 

	// Ping Item interface (implement a rendering of a 'ping' for a Item).
	void BeginPingNode (TreeViewItem item, float topPixelOfRow, float availableWidth);
	void EndPingNode ();

	// Rename interface (BeginRename should return true if rename is handled)
	bool BeginRename (TreeViewItem item, float delay);
	void EndRename ();
	float GetContentIndent (TreeViewItem item);
}

}
