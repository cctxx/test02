using UnityEditor;
using UnityEngine;
using System.Collections.Generic;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class AnimationWindowHierarchyDataSource : TreeViewDataSource
	{
		// Animation window shared state
		private AnimationWindowState state { get; set; }
		public bool showAll { get; set; }

		public AnimationWindowHierarchyDataSource (TreeView treeView, AnimationWindowState animationWindowState)
			: base (treeView)
		{
			state = animationWindowState;
		}

		private void SetupRootNodeSettings ()
		{
			showRootNode = false;
			rootIsCollapsable = false;
			SetExpanded (m_RootItem, true);
		}

		private AnimationWindowHierarchyNode GetEmptyRootNode ()
		{
			return new AnimationWindowHierarchyNode (0, -1, null, null, "", "", "root");
		}

		public override void FetchData ()
		{
			m_RootItem = GetEmptyRootNode ();
			SetupRootNodeSettings ();

			if (state.m_ActiveGameObject == null || state.m_RootGameObject == null)
				return;

			List<AnimationWindowHierarchyNode> childNodes = new List<AnimationWindowHierarchyNode> ();

			if (state.allCurves.Count > 0)
				childNodes.Add (new AnimationWindowHierarchyMasterNode());
			
			childNodes.AddRange (CreateTreeFromCurves ());
			childNodes.Add (new AnimationWindowHierarchyAddButtonNode());
			
			TreeViewUtility.SetChildParentReferences (new List<TreeViewItem> (childNodes.ToArray()), root);
			m_NeedRefreshVisibleFolders = true;
		}

		public override bool IsRenamingItemAllowed (TreeViewItem item)
		{
			return false;
		}

		public List<AnimationWindowHierarchyNode> CreateTreeFromCurves ()
		{
			List<AnimationWindowHierarchyNode> nodes = new List<AnimationWindowHierarchyNode>();
			List<AnimationWindowCurve> singlePropertyCurves = new List<AnimationWindowCurve> ();

			AnimationWindowCurve[] curves = state.allCurves.ToArray();
			for (int i = 0; i < curves.Length; i++)
			{
				AnimationWindowCurve curve = curves[i];
				AnimationWindowCurve nextCurve = i < curves.Length - 1 ? curves[i + 1] : null;

				singlePropertyCurves.Add (curve);

				bool areSameGroup = nextCurve != null && AnimationWindowUtility.GetPropertyGroupName (nextCurve.propertyName) == AnimationWindowUtility.GetPropertyGroupName (curve.propertyName);
				bool areSamePathAndType = nextCurve != null && curve.path.Equals (nextCurve.path) && curve.type == nextCurve.type;

				// We expect curveBindings to come sorted by propertyname
				// So we compare curve vs nextCurve. If its different path or different group (think "scale.xyz" as group), then we know this is the last element of such group.
				if (i == curves.Length - 1 || !areSameGroup || !areSamePathAndType)
				{
					if (singlePropertyCurves.Count > 1)
						nodes.Add (AddPropertyGroupToHierarchy (singlePropertyCurves.ToArray (), (AnimationWindowHierarchyNode)m_RootItem));
					else
						nodes.Add (AddPropertyToHierarchy (singlePropertyCurves[0], (AnimationWindowHierarchyNode)m_RootItem));
					singlePropertyCurves.Clear ();
				}	
			}

			return nodes;
		}
		
		private AnimationWindowHierarchyPropertyGroupNode AddPropertyGroupToHierarchy (AnimationWindowCurve[] curves, AnimationWindowHierarchyNode parentNode)
		{
			List<AnimationWindowHierarchyNode> childNodes = new List<AnimationWindowHierarchyNode> ();
			
			System.Type animatableObjectType = curves[0].type;
			AnimationWindowHierarchyPropertyGroupNode node = new AnimationWindowHierarchyPropertyGroupNode (animatableObjectType, AnimationWindowUtility.GetPropertyGroupName (curves[0].propertyName), curves[0].path, parentNode);
			node.icon = AssetPreview.GetMiniTypeThumbnail (animatableObjectType);
			node.indent = curves[0].depth;
			node.curves = curves;

			foreach (AnimationWindowCurve curve in curves)
			{
				AnimationWindowHierarchyPropertyNode childNode = AddPropertyToHierarchy (curve, node);
				// For child nodes we do not want to display the type in front (It is already shown by the group node)
				childNode.displayName = AnimationWindowUtility.GetPropertyDisplayName (childNode.propertyName);
				childNodes.Add (childNode);
			}

			TreeViewUtility.SetChildParentReferences (new List<TreeViewItem> (childNodes.ToArray ()), node);
			return node;
		}

		private AnimationWindowHierarchyPropertyNode AddPropertyToHierarchy (AnimationWindowCurve curve, AnimationWindowHierarchyNode parentNode)
		{
			AnimationWindowHierarchyPropertyNode node = new AnimationWindowHierarchyPropertyNode (curve.type, curve.propertyName, curve.path, parentNode, curve.binding, curve.isPPtrCurve);
			
			if (parentNode.icon != null)
				node.icon = parentNode.icon;
			else
				node.icon = AssetPreview.GetMiniTypeThumbnail (curve.type);

			node.indent = curve.depth;
			node.curves = new[] { curve };
			return node;
		}
		
		public void UpdateData ()
		{
			m_TreeView.ReloadData ();
		}
	}
}
