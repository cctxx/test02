using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{

// Implements dragging behavior for HierarchyProperty based data: Assets or GameObjects

internal class AssetOrGameObjectTreeViewDragging : TreeViewDragging
{
	readonly HierarchyType m_HierarchyType;

	public AssetOrGameObjectTreeViewDragging (TreeView treeView, HierarchyType hierarchyType)
		: base (treeView)
	{
		m_HierarchyType = hierarchyType;
	}

	public override void StartDrag (TreeViewItem draggedItem, List<int> draggedItemIDs)
	{
		DragAndDrop.PrepareStartDrag();
		DragAndDrop.objectReferences = ProjectWindowUtil.GetDragAndDropObjects (draggedItem.id, draggedItemIDs);

		DragAndDrop.paths = ProjectWindowUtil.GetDragAndDropPaths (draggedItem.id, draggedItemIDs);
		if (DragAndDrop.objectReferences.Length > 1)
			DragAndDrop.StartDrag ("<Multiple>");
		else
		{
			string title = ObjectNames.GetDragAndDropTitle (InternalEditorUtility.GetObjectFromInstanceID (draggedItem.id));
			DragAndDrop.StartDrag (title);
		}
	}

	public override DragAndDropVisualMode DoDrag (TreeViewItem parentItem, TreeViewItem targetItem, bool perform)
	{
		// Need to get a HierarchyProperty instance for handling in C++ land
		HierarchyProperty search = new HierarchyProperty (m_HierarchyType);
		if (parentItem == null || !search.Find (parentItem.id, null))
			search = null;

		if (m_HierarchyType == HierarchyType.Assets)
			return InternalEditorUtility.ProjectWindowDrag(search, perform);
		else // HierarchyType.GameObjects
			return InternalEditorUtility.HierarchyWindowDrag(search, perform);
	}
}

} // namespace UnityEditor
