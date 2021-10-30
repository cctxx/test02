
using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class AddCurvesPopupHierarchyGUI : TreeViewGUI
	{
		public EditorWindow owner;
		public AnimationWindowState state { get; set; }
		public bool showPlusButton { get; set; }
		private GUIStyle plusButtonStyle = new GUIStyle ("OL Plus");
		private GUIStyle plusButtonBackgroundStyle = new GUIStyle ("Tag MenuItem");
		private const float plusButtonWidth = 17;

		public AddCurvesPopupHierarchyGUI (TreeView treeView, AnimationWindowState state, EditorWindow owner)
			: base (treeView)
		{
			this.owner = owner;
			this.state = state;
		}

		public override Rect OnRowGUI (TreeViewItem node, int row, float rowWidth, bool selected, bool focused)
		{
			Rect rowRect = base.OnRowGUI (node, row, rowWidth, selected, focused);
			Rect buttonRect = new Rect (rowWidth - plusButtonWidth, rowRect.yMin, plusButtonWidth, plusButtonStyle.fixedHeight);

			AddCurvesPopupPropertyNode hierarchyNode = node as AddCurvesPopupPropertyNode;

			// Is it propertynode. If not, then we don't need plusButton so quit here
			if (hierarchyNode == null || hierarchyNode.curveBindings == null || hierarchyNode.curveBindings.Length == 0)
				return rowRect;

			// TODO Make a style for add curves popup
			// Draw background behind plus button to prevent text overlapping
			GUI.Box(buttonRect, GUIContent.none, plusButtonBackgroundStyle);

			// Check if the curve already exists and remove plus button
			if (GUI.Button (buttonRect, GUIContent.none, plusButtonStyle))
			{
				AddCurvesPopup.AddNewCurve (hierarchyNode);
				owner.Close ();
				m_TreeView.ReloadData ();
			}

			return rowRect;
		}

		override protected void SyncFakeItem()
		{
			//base.SyncFakeItem();
		}

		override protected void RenameEnded()
		{
			//base.RenameEnded();
		}

		override protected Texture GetIconForNode(TreeViewItem item)
		{
			if (item != null)
				return item.icon;
			
			return null;
		}
	}
}
