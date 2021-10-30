using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using System;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class AddCurvesPopupHierarchyDataSource : TreeViewDataSource
	{
		// Animation window shared state
		private AnimationWindowState state { get; set; }

		public static bool showEntireHierarchy { get; set; }

		public AddCurvesPopupHierarchyDataSource (TreeView treeView, AnimationWindowState animationWindowState)
			: base (treeView)
		{
			showRootNode = false;
			rootIsCollapsable = false;
			state = animationWindowState;
		}

		private void SetupRootNodeSettings ()
		{
			showRootNode = false;
			SetExpanded (root, true);
		}

		public override void FetchData ()
		{
			if (AddCurvesPopup.gameObject == null)
				return;

			AddGameObjectToHierarchy (AddCurvesPopup.gameObject, null);

			SetupRootNodeSettings ();
			m_NeedRefreshVisibleFolders = true;
		}

		private TreeViewItem AddGameObjectToHierarchy (GameObject gameObject, TreeViewItem parent)
		{
			string path = AnimationUtility.CalculateTransformPath (gameObject.transform, state.m_RootGameObject.transform);
			TreeViewItem node = new AddCurvesPopupGameObjectNode (gameObject, parent, gameObject.name);
			List<TreeViewItem> childNodes = new List<TreeViewItem> ();

			if (parent == null)
				m_RootItem = node;

			// Iterate over all animatable objects
			EditorCurveBinding[] allCurveBindings = AnimationUtility.GetAnimatableBindings (gameObject, state.m_RootGameObject);
			List<EditorCurveBinding> singleObjectBindings = new List<EditorCurveBinding> ();
			for (int i = 0; i < allCurveBindings.Length; i++)
			{
				EditorCurveBinding curveBinding = allCurveBindings[i];

				singleObjectBindings.Add (curveBinding);
				
				// Don't create group for GameObject.m_IsActive. It looks messy
				if (curveBinding.propertyName == "m_IsActive")
				{
					// Don't show for the root go
					if (curveBinding.path != "")
					{
						childNodes.Add(AddAnimatablePropertyToHierarchy(singleObjectBindings.ToArray(), node));
						singleObjectBindings.Clear();
					}
					else
					{
						singleObjectBindings.Clear();
					}
				}
				else
				{
					// We expect allCurveBindings to come sorted by type

					bool isLastItemOverall = (i == allCurveBindings.Length - 1);
					bool isLastItemOnThisGroup = false;
					
					if (!isLastItemOverall)
						isLastItemOnThisGroup = (allCurveBindings[i + 1].type != curveBinding.type);
					
					// Let's not add those that already have a existing curve
					if (AnimationWindowUtility.IsCurveCreated (state.m_ActiveAnimationClip, curveBinding))
						singleObjectBindings.Remove (curveBinding);

					if ((isLastItemOverall || isLastItemOnThisGroup) && singleObjectBindings.Count > 0)
					{
						childNodes.Add (AddAnimatableObjectToHierarchy (state.m_RootGameObject, singleObjectBindings.ToArray(), node, path));
						singleObjectBindings.Clear();
					}
				}
			}

			if (showEntireHierarchy)
			{
				// Iterate over all child GOs
				for (int i = 0; i < gameObject.transform.childCount; i++)
				{
					Transform childTransform = gameObject.transform.GetChild (i);
					TreeViewItem childNode = AddGameObjectToHierarchy (childTransform.gameObject, node);
					if (childNode != null)
						childNodes.Add (childNode);
				}
			}

			TreeViewUtility.SetChildParentReferences(childNodes, node);
			return node;
		}

		static string GetClassName (GameObject root, EditorCurveBinding binding)
		{
			Object target = AnimationUtility.GetAnimatedObject (root, binding);
			if (target)
				return ObjectNames.GetInspectorTitle(target);
			else
				return binding.type.Name;
		}


		private TreeViewItem AddAnimatableObjectToHierarchy (GameObject root, EditorCurveBinding[] curveBindings, TreeViewItem parentNode, string path)
		{
			TreeViewItem node = new AddCurvesPopupObjectNode (parentNode, path, GetClassName (root, curveBindings[0]));
			
			node.icon = AssetPreview.GetMiniThumbnail (AnimationUtility.GetAnimatedObject (root, curveBindings[0]));

			List<TreeViewItem> childNodes = new List<TreeViewItem> ();
			List<EditorCurveBinding> singlePropertyBindings = new List<EditorCurveBinding> ();
			
			for (int i = 0; i < curveBindings.Length; i++)
			{
				EditorCurveBinding curveBinding = curveBindings[i];
				
				singlePropertyBindings.Add (curveBinding);

				// We expect curveBindings to come sorted by propertyname
				if (i == curveBindings.Length - 1 || AnimationWindowUtility.GetPropertyGroupName (curveBindings[i + 1].propertyName) != AnimationWindowUtility.GetPropertyGroupName (curveBinding.propertyName))
				{
					childNodes.Add (AddAnimatablePropertyToHierarchy (singlePropertyBindings.ToArray (), node));
					singlePropertyBindings.Clear ();
				}
			}

			childNodes.Sort();

			TreeViewUtility.SetChildParentReferences(childNodes, node);
			return node;
		}

		private TreeViewItem AddAnimatablePropertyToHierarchy (EditorCurveBinding[] curveBindings, TreeViewItem parentNode)
		{
			TreeViewItem node = new AddCurvesPopupPropertyNode (parentNode, curveBindings);
			node.icon = parentNode.icon;
			return node;
		}

		public void UpdateData ()
		{
			m_TreeView.ReloadData ();
		}
	}

	internal class AddCurvesPopupGameObjectNode : TreeViewItem
	{
		public AddCurvesPopupGameObjectNode (GameObject gameObject, TreeViewItem parent, string displayName)
			: base (gameObject.GetInstanceID(), parent != null ? parent.depth + 1 : -1, parent, displayName)
		{
		}
	}

	internal class AddCurvesPopupObjectNode : TreeViewItem
	{
		public AddCurvesPopupObjectNode (TreeViewItem parent, string path, string className)
			: base ((path + className).GetHashCode(), parent.depth + 1, parent, className)
		{
		}
	}

	internal class AddCurvesPopupPropertyNode : TreeViewItem
	{
		public EditorCurveBinding[] curveBindings;

		public AddCurvesPopupPropertyNode (TreeViewItem parent, EditorCurveBinding[] curveBindings)
			: base (curveBindings[0].GetHashCode (), parent.depth + 1, parent, AnimationWindowUtility.NicifyPropertyGroupName (AnimationWindowUtility.GetPropertyGroupName (curveBindings[0].propertyName)))
		{
			this.curveBindings = curveBindings;
		}

		public override int CompareTo(TreeViewItem other)
		{
			if (other is AddCurvesPopupPropertyNode)
			{
				if (displayName.Contains ("Rotation") && other.displayName.Contains ("Position"))
					return 1;
				if (displayName.Contains ("Position") && other.displayName.Contains ("Rotation"))
					return -1;
			}
			return base.CompareTo (other);
		}
	}
}
