using UnityEngine;

namespace UnityEditor
{

internal class TreeViewItem : System.IComparable<TreeViewItem>
{
	int m_ID;							
	TreeViewItem m_Parent;
	TreeViewItem[] m_Children = { };
	int m_Depth;
	string m_DisplayName;
	Texture2D m_Icon;

	// The id should be unique for all items in TreeView because it is used for searching, selection etc.
	public TreeViewItem (int id, int depth, TreeViewItem parent, string displayName)
	{
		m_Depth = depth;
		m_Parent = parent;
		m_ID = id;
		m_DisplayName = displayName;
	}

	virtual public int id { get { return m_ID; } }
	virtual public string displayName { get { return m_DisplayName; } set { m_DisplayName = value; } }
	virtual public int depth { get { return m_Depth; } }
	virtual public bool hasChildren { get { return m_Children != null && m_Children.Length > 0; } }
	virtual public TreeViewItem[] children { get { return m_Children; } set { m_Children = value; } }
	virtual public TreeViewItem parent { get { return m_Parent; } set { m_Parent = value; } }
	virtual public Texture2D icon { get { return m_Icon; } set { m_Icon = value; } }

	public virtual int CompareTo (TreeViewItem other)
	{
		return displayName.CompareTo(other.displayName);
	}

	public override string ToString()
	{
		return string.Format("Item: '{0}' ({1}), has {2} children, depth {3}, parent id {4}", displayName, id, children.Length, depth, (parent!=null)?parent.id:-1);
	}
}

} // UnityEditor
