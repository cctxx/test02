using System.Linq;
using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

namespace UnityEditorInternal
{
	[System.Serializable]
	internal class AnimationWindowHierarchyState : TreeViewState
	{
		public List<int> m_TallInstanceIDs = new List<int> ();

		public bool getTallMode (AnimationWindowHierarchyNode node)
		{
			return m_TallInstanceIDs.Contains (node.id);
		}

		public void setTallMode (AnimationWindowHierarchyNode node, bool tallMode)
		{
			if (tallMode)
				m_TallInstanceIDs.Add(node.id);
			else
				m_TallInstanceIDs.Remove(node.id);
		}

	}

	internal class AnimationWindowHierarchy
	{
		// Animation window shared state
		public AnimationWindowState state { get; set; }

		TreeView m_TreeView;

		public AnimationWindowHierarchy (AnimationWindowState state, EditorWindow owner, Rect position)
		{
			this.state = state;
			Init (owner, position);
		}

		public void OnGUI (Rect position, EditorWindow owner)
		{
			m_TreeView.OnEvent ();
			m_TreeView.OnGUI (position, GUIUtility.GetControlID (FocusType.Keyboard));
		}
		
		public void Init (EditorWindow owner, Rect rect)
		{
			if (state.m_hierarchyState == null)
				state.m_hierarchyState = new AnimationWindowHierarchyState ();

			m_TreeView = new TreeView (owner, state.m_hierarchyState);
			state.m_HierarchyData = new AnimationWindowHierarchyDataSource (m_TreeView, state);
			m_TreeView.Init (rect,
							state.m_HierarchyData,
							new AnimationWindowHierarchyGUI (m_TreeView, state),
							new AnimationWindowHierarchyDragging ()
							);

			m_TreeView.deselectOnUnhandledMouseDown = true;
			m_TreeView.selectionChangedCallback += state.OnHierarchySelectionChanged;
			
			m_TreeView.ReloadData ();
		}

		virtual internal bool IsRenamingNodeAllowed (TreeViewItem node) { return true; }

		public bool IsIDVisible (int id)
		{
			if (m_TreeView == null)
				return false;

			return m_TreeView.IsVisible (id);
		}
	}

	// We want to disable dragging in animation window hierarchy
	internal class AnimationWindowHierarchyDragging : ITreeViewDragging
	{
		public void StartDrag (TreeViewItem draggedItem, List<int> draggedItemIDs)
		{
			// noop
		}

		public bool DragElement (TreeViewItem targetItem, Rect targetItemRect)
		{
			return false;
		}

		public void DragCleanup (bool revertExpanded)
		{
			// noop
		}

		public int GetDropTargetControlID ()
		{
			return 0;
		}

		public int GetRowMarkerControlID ()
		{
			return 0;
		}
	}
} // namespace
