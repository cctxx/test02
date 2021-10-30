using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{

// Abstract base class for common dragging behavior
// Usage:
//   - Override StartDrag
//   - Override DoDrag 
// Features:
//   - Expands items with children on hover (after ca 0.7 seconds)
//


internal abstract class TreeViewDragging : ITreeViewDragging
{
	protected TreeView m_TreeView;

	class DropData
	{
		public int[]    expandedArrayBeforeDrag;
		public int      lastControlID;
		public int      dropTargetControlID;
		public int      rowMarkerControlID;
		public double   expandItemBeginTimer;
	}
	DropData m_DropData = new DropData();
	const double k_DropExpandTimeout = 0.7;
	const float k_HalfDropBetweenHeight = 2; // TODO: Should be decided by UI

	public TreeViewDragging (TreeView treeView)
	{
		m_TreeView = treeView;
	}

	public int GetDropTargetControlID ()
	{
		return m_DropData.dropTargetControlID;
	}

	public int GetRowMarkerControlID ()
	{
		return m_DropData.rowMarkerControlID;
	}

	/*
	 * Override e.g: 
	 * 
	 * public override void StartDrag (Item draggedItem, List<int> selectedItems)
	 * {
			DragAndDrop.PrepareStartDrag();
			DragAndDrop.objectReferences = ProjectWindowUtil.GetDragAndDropObjects (draggedItem.id, selectedItems);

			DragAndDrop.paths = ProjectWindowUtil.GetDragAndDropPaths (draggedItem.id, selectedItems);
			if (DragAndDrop.objectReferences.Length > 1)
				DragAndDrop.StartDrag ("<Multiple>");
			else
			{
				string title = ObjectNames.GetDragAndDropTitle (InternalEditorUtility.GetObjectFromInstanceID (draggedItem.id));
				DragAndDrop.StartDrag (title);
			}	 
	 * }
	 */
	public abstract void StartDrag (TreeViewItem draggedItem, List<int> draggedItemIDs);

	/*
	 * Override e.g:
		public override DragAndDropVisualMode DoDrag (Item parentItem, Item targetItem, bool perform)
		{
			HierarchyProperty search = new HierarchyProperty (m_HierarchyType);
			if (parentItem == null || !search.Find (parentItem.id, null))
				search = null;

			if (m_HierarchyType == HierarchyType.Assets)
				return InternalEditorUtility.ProjectWindowDrag(search, perform);
			else
				return InternalEditorUtility.HierarchyWindowDrag(search, perform);
		}	 
	 */
	public abstract DragAndDropVisualMode DoDrag (TreeViewItem parentItem, TreeViewItem targetItem, bool perform);


	// targetItem is null when not hovering over any target Item
	public virtual bool DragElement (TreeViewItem targetItem, Rect targetItemRect)
	{
		if (targetItem == null)
		{
			DragAndDrop.visualMode = DragAndDropVisualMode.None;
			if (m_DropData != null)
			{
				m_DropData.dropTargetControlID = 0;
				m_DropData.rowMarkerControlID = 0;
			}
			return false;
		}

		bool targetCanBeParent = m_TreeView.data.CanBeParent (targetItem);

		// We create a rect that overlaps upwards to detect dropUpon or dropBetween
		Rect dropRect = targetItemRect;
		float betweenHalfHeight = targetCanBeParent ? k_HalfDropBetweenHeight : targetItemRect.height * 0.5f;
		dropRect.yMax += betweenHalfHeight;
		if (targetItem != null && !dropRect.Contains(Event.current.mousePosition))
			return false;

		// Ok we are inside our offset rect
		bool dropOnTopOfNode;

		if (!targetCanBeParent || Event.current.mousePosition.y > targetItemRect.yMax - betweenHalfHeight)
			dropOnTopOfNode = false;
		else
			dropOnTopOfNode = true; 
		

		TreeViewItem parentItem;
		if (m_TreeView.data.IsExpanded (targetItem) && targetItem.children.Length > 0)
			parentItem = targetItem;
		else
			parentItem = targetItem.parent;

		DragAndDropVisualMode mode = DragAndDropVisualMode.None;
		if (Event.current.type == EventType.DragPerform)
		{
			// Try Drop on top of element
			if (dropOnTopOfNode)
				mode = DoDrag (targetItem, targetItem, true);

			// Fall back to dropping on parent  (drop between elements)
			if (mode == DragAndDropVisualMode.None && targetItem != null && parentItem != null)
			{
				mode = DoDrag (parentItem, targetItem, true);
			}

			// Finalize drop
			if (mode != DragAndDropVisualMode.None)
			{
				DragAndDrop.AcceptDrag();
				DragCleanup(false);

				List<Object> objs = new List<Object>(DragAndDrop.objectReferences); // TODO, what about when dragging non objects...

				// Drag selection might be different than current selection
				int [] newSelection = new int [objs.Count];
				for (int i=0; i<objs.Count; ++i)
					newSelection[i] = (objs[i].GetInstanceID ());

				m_TreeView.NotifyListenersThatDragEnded (newSelection);
			}
			else
			{
				DragCleanup(true);
				m_TreeView.NotifyListenersThatDragEnded (null);
			}
		}
		else  // Event.current.type != EventType.DragPerform
		{
			if (m_DropData == null)
				m_DropData = new DropData();
			m_DropData.dropTargetControlID = 0;
			m_DropData.rowMarkerControlID = 0;

			// Handle auto expansion
			int controlID = TreeView.GetItemControlID (targetItem);
			if (controlID != m_DropData.lastControlID)
			{
				m_DropData.lastControlID = controlID;
				m_DropData.expandItemBeginTimer = Time.realtimeSinceStartup;
			}

			bool mayExpand = Time.realtimeSinceStartup - m_DropData.expandItemBeginTimer > k_DropExpandTimeout;

			// Auto open folders we are about to drag into
			if (targetItem != null && mayExpand && targetItem.children.Length > 0 && !m_TreeView.data.IsExpanded(targetItem))
			{
				// Store the expanded array prior to drag so we can revert it with a delay later
				if (m_DropData.expandedArrayBeforeDrag == null)
				{
					List <int> expandedIDs = GetCurrentExpanded ();
					m_DropData.expandedArrayBeforeDrag = expandedIDs.ToArray();
				}

				m_TreeView.data.SetExpanded (targetItem, true);
			}

			// Try drop on top of element
			if (dropOnTopOfNode)
				mode = DoDrag (targetItem, targetItem, false);

			if (mode != DragAndDropVisualMode.None)
			{
				m_DropData.dropTargetControlID = controlID;
				DragAndDrop.visualMode = mode;
			}
			// Fall back to dropping on parent (drop between elements)
			else if (targetItem != null && parentItem != null) 
			{
				mode = DoDrag (parentItem, targetItem, false);

				if (mode != DragAndDropVisualMode.None)
				{
					m_DropData.rowMarkerControlID = controlID;
					m_DropData.dropTargetControlID = TreeView.GetItemControlID (parentItem);
					DragAndDrop.visualMode = mode;
				}
			}
		}

		Event.current.Use();
		return true;
	}

	public virtual void DragCleanup (bool revertExpanded)
	{
		if (m_DropData != null)
		{
			if (m_DropData.expandedArrayBeforeDrag != null && revertExpanded)
			{
				RestoreExpanded (new List<int>(m_DropData.expandedArrayBeforeDrag));
			}
			m_DropData = new DropData ();
		}
	}

	public List<int> GetCurrentExpanded()
	{
		List<TreeViewItem> visibleItems = m_TreeView.data.GetVisibleRows ();
		List<int> expandedIDs = (from item in visibleItems
										 where m_TreeView.data.IsExpanded (item)
										 select item.id).ToList();
		return expandedIDs;
	}

	// We assume that we can only have expanded items during dragging
	public void RestoreExpanded (List<int> ids)
	{
		List<TreeViewItem> visibleItems = m_TreeView.data.GetVisibleRows();
		foreach (TreeViewItem item in visibleItems)
			m_TreeView.data.SetExpanded (item, ids.Contains(item.id));
	}
}

} // namespace UnityEditor
