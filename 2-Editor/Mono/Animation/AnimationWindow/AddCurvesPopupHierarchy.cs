using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class AddCurvesPopupHierarchy
	{
		private AnimationWindowState state { get; set; }

		private TreeView m_TreeView;
		private TreeViewState m_TreeViewState;
		private AddCurvesPopupHierarchyDataSource m_TreeViewDataSource;

		public AddCurvesPopupHierarchy (AnimationWindowState state)
		{
			this.state = state;
		}

		public void OnGUI (Rect position, EditorWindow owner)
		{
			InitIfNeeded (owner, position);
			m_TreeView.OnEvent ();
			m_TreeView.OnGUI (position, GUIUtility.GetControlID (FocusType.Keyboard));
		}

		public void InitIfNeeded (EditorWindow owner, Rect rect)
		{
			if (m_TreeViewState == null)
				m_TreeViewState = new TreeViewState ();
			else
				return;

			m_TreeView = new TreeView (owner, m_TreeViewState);

			m_TreeView.deselectOnUnhandledMouseDown = true;

			m_TreeViewDataSource = new AddCurvesPopupHierarchyDataSource (m_TreeView, state);
			TreeViewGUI gui = new AddCurvesPopupHierarchyGUI (m_TreeView, state, owner);

			m_TreeView.Init (rect,
							m_TreeViewDataSource,
							gui,
							new AssetOrGameObjectTreeViewDragging(m_TreeView, HierarchyType.GameObjects)
				);

			m_TreeViewDataSource.UpdateData ();
		}

		internal virtual bool IsRenamingNodeAllowed (TreeViewItem node)
		{
			return false;
		}
	}
}
