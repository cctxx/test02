using System.IO;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{
	internal static class TreeViewUtility
	{
		public static List<TreeViewItem> FindItemsInList (IEnumerable<int> itemIDs, List<TreeViewItem> treeViewItems)
		{
			return (from x in treeViewItems where itemIDs.Contains(x.id) select x).ToList();
		}

		public static TreeViewItem FindItemInList (int id, List<TreeViewItem> treeViewItems)
		{
			return treeViewItems.FirstOrDefault(t => t.id == id);
		}

		public static TreeViewItem FindItem (int id, TreeViewItem searchFromThisItem)
		{
			return FindItemRecursive (id, searchFromThisItem);
		}
		private static TreeViewItem FindItemRecursive (int id, TreeViewItem item)
		{
			if (item == null)
				return null;

			if (item.id == id)
				return item;

			foreach (TreeViewItem child in item.children)
			{
				TreeViewItem result = FindItemRecursive(id, child);
				if (result != null)
					return result;
			}
			return null;
		}

		
		public static void DebugPrintToEditorLogRecursive (TreeViewItem item)
		{
			if (item == null)
				return;
			System.Console.WriteLine(new System.String(' ', item.depth * 3) + item.displayName);
			foreach (TreeViewItem child in item.children)
			{
				DebugPrintToEditorLogRecursive(child);
			}
		}

		public static void SetChildParentReferences(List<TreeViewItem> visibleItems, TreeViewItem root)
		{
			for (int childIndex = 0; childIndex < visibleItems.Count; childIndex++)
			{
				TreeViewItem child = visibleItems[childIndex];
				TreeViewItem parent = FindParent(visibleItems, child.depth, childIndex);
				if (parent == null)
					parent = root;

				SetParent(child, parent);
			}
		}

		private static void SetParent(TreeViewItem child, TreeViewItem parent)
		{
			// set parent
			child.parent = parent;

			// add child to parent
			List<TreeViewItem> list = new List<TreeViewItem>(parent.children);
			if (list.Count > 0 && list[0] == null)
				list.RemoveAt(0); // remove dummy child (see FetchData)
			list.Add(child);
			parent.children = list.ToArray();
		}

		private static TreeViewItem FindParent(List<TreeViewItem> visibleItems, int childDepth, int childIndex)
		{
			if (childDepth == 0)
				return null;

			while (childIndex >= 0)
			{
				TreeViewItem parent = visibleItems[childIndex];
				if (parent.depth == childDepth - 1)
					return parent;
				childIndex--; // search upwards until depth matches
			}

			return null;
		}
	}
}


