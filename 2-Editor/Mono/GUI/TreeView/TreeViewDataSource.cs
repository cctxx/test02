using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace UnityEditor
{
// TreeViewDataSource is a base abstract class for a data source for a TreeView.
// Usage:
//   Override FetchData () and build the entire tree with m_RootItem as root. 
//   Configure showRootNode and rootIsCollapsable as wanted
//
// Note: if dealing with very large trees use LazyTreeViewDataSource instead: it assumes that tree only contains visible items.

internal abstract class TreeViewDataSource : ITreeViewDataSource
{
	protected readonly TreeView m_TreeView;					// TreeView using this data source
	protected TreeViewItem m_RootItem;
	protected List<TreeViewItem> m_VisibleRows;			
	protected bool m_NeedRefreshVisibleFolders = true;
	protected TreeViewItem m_FakeItem;

	public bool showRootNode { get; set; }
	public bool rootIsCollapsable { get; set; }
	public TreeViewItem root { get { return m_RootItem; } }

	protected List<int> expandedIDs 
	{
		get {return m_TreeView.state.expandedIDs;}
		set { m_TreeView.state.expandedIDs = value;}
	}

	public TreeViewDataSource (TreeView treeView)
	{
		m_TreeView = treeView;
		showRootNode = true;
		rootIsCollapsable = false;
	}

	// Override this function and build entire tree with m_RootItem as root
	public abstract void FetchData ();



	public void ReloadData ()
	{
		m_FakeItem = null;
		FetchData ();
	}

	virtual public TreeViewItem FindItem (int id)
	{
		return TreeViewUtility.FindItem (id, m_RootItem);
	}

	//----------------------------
	// Visible Item section

	protected void GetVisibleItemsRecursive (TreeViewItem item, List<TreeViewItem> items)
	{
		if (item != m_RootItem || showRootNode)
			items.Add(item);

		if (IsExpanded(item))
			foreach (TreeViewItem child in item.children)
				GetVisibleItemsRecursive(child, items); 
	}

	// Get the flattend tree of visible items.
	virtual public List<TreeViewItem> GetVisibleRows ()
	{
		// Cached for large trees...
		if (m_VisibleRows == null || m_NeedRefreshVisibleFolders)
		{
			m_VisibleRows = new List<TreeViewItem>();
			GetVisibleItemsRecursive(m_RootItem, m_VisibleRows);
			m_NeedRefreshVisibleFolders = false;
			// Expanded state has changed ensure that we repaint
			m_TreeView.Repaint();
		}
		return m_VisibleRows;
	}

	//----------------------------
	// Expanded/collapsed section

	virtual public int[] GetExpandedIDs ()
	{
		return expandedIDs.ToArray ();
	}

	virtual public void SetExpandedIDs (int[] ids)
	{
		expandedIDs = new List<int> (ids);
		expandedIDs.Sort ();
		m_NeedRefreshVisibleFolders = true;
		OnExpandedStateChanged();
	}

	virtual public bool IsExpanded (int id)
	{
		return expandedIDs.BinarySearch(id) >= 0;
	}

	virtual public bool SetExpanded (int id, bool expand)
	{
		bool expanded = IsExpanded (id);
		if (expand != expanded)
		{
			if (expand)
			{
				NUnit.Framework.Assert.That (!expandedIDs.Contains (id));
				expandedIDs.Add(id);
				expandedIDs.Sort();
			}
			else
			{
				expandedIDs.Remove(id);
			}
			m_NeedRefreshVisibleFolders = true;
			OnExpandedStateChanged();
			return true;
		}
		return false;
	}

	virtual public void SetExpandedWithChildren(TreeViewItem fromItem, bool expand)
	{
		Stack<TreeViewItem> stack = new Stack<TreeViewItem>();
		stack.Push(fromItem);

		HashSet<int> parents = new HashSet<int>();
		while (stack.Count > 0)
		{
			TreeViewItem current = stack.Pop();
			if (current.hasChildren)
			{
				parents.Add(current.id);
				foreach (var foo in current.children)
				{
					stack.Push(foo);
				}
			}
		}

		// Get existing expanded in hashset
		HashSet<int> oldExpandedSet = new HashSet<int>(expandedIDs);

		if (expand)
			oldExpandedSet.UnionWith(parents);
		else
			oldExpandedSet.ExceptWith(parents);

		// Bulk set expanded ids (is sorted in SetExpandedIDs)
		SetExpandedIDs (oldExpandedSet.ToArray());
	}

	virtual public void SetExpanded (TreeViewItem item, bool expand)
	{
		SetExpanded(item.id, expand);
	}

	virtual public bool IsExpanded (TreeViewItem item)
	{
		return IsExpanded (item.id);
	}

	virtual public bool IsExpandable (TreeViewItem item)
	{
		return item.children.Length > 0;
	}

	virtual public bool CanBeMultiSelected (TreeViewItem item)
	{
		return true;
	}

	virtual public bool CanBeParent (TreeViewItem item)
	{
		return true;
	}

	virtual public void OnExpandedStateChanged()
	{
	}

	//----------------------------
	// Renaming section

	virtual public bool IsRenamingItemAllowed (TreeViewItem item)
	{
		return true;
	}


	//----------------------------
	// Insert tempoary Item section

	// Fake Item should be inserted into the m_VisibleRows (not the tree itself).
	virtual public void InsertFakeItem (int id, int parentID, string name, Texture2D icon)
	{
		Debug.LogError("InsertFakeItem missing implementation");
	}

	virtual public bool HasFakeItem ()
	{
		return m_FakeItem != null;
	}

	virtual public void RemoveFakeItem ()
	{
		if (!HasFakeItem())
			return;

		List<TreeViewItem> visibleRows = GetVisibleRows ();
		int index = TreeView.GetIndexOfID (visibleRows, m_FakeItem.id);
		if (index != -1)
		{
			visibleRows.RemoveAt (index);
		}
		m_FakeItem = null;
	}
}

}


