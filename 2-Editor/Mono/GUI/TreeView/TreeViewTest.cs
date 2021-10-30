using System;
using System.Collections.Generic;
using System.Linq;
using UnityEditorInternal;
using UnityEngine;


namespace UnityEditor
{

internal class TreeViewTestWindow : EditorWindow
{
	private TreeViewTest.BackendData m_BackendData;
	private TreeViewTest m_TreeViewTest;
	private TreeViewTest m_TreeViewTest2;

	public TreeViewTestWindow()
	{
		title = "TreeView Test";		
	}

	void OnGUI()
	{
		Rect leftRect = new Rect(0, 0, position.width/2, position.height);
		Rect rightRect = new Rect(position.width / 2, 0, position.width / 2, position.height);
		if (m_TreeViewTest == null)
		{
			m_BackendData = new TreeViewTest.BackendData();
			m_BackendData.GenerateData(1000000);
			
			bool lazy = false;
			m_TreeViewTest = new TreeViewTest(this, lazy);
			m_TreeViewTest.Init(leftRect, m_BackendData);

			lazy = true;
			m_TreeViewTest2 = new TreeViewTest(this, lazy);
			m_TreeViewTest2.Init(rightRect, m_BackendData);
		}

		m_TreeViewTest.OnGUI (leftRect);
		m_TreeViewTest2.OnGUI (rightRect);
		EditorGUI.DrawRect(new Rect( leftRect.xMax-1, 0, 2, position.height), new Color(0.4f, 0.4f, 0.4f, 0.8f));

		//Repaint();
	}
}

internal class TreeViewTest
{
	private BackendData m_BackendData;
	private TreeView m_TreeView;
	private EditorWindow m_EditorWindow;
	private bool m_Lazy;
	private TreeViewColumnHeader m_ColumnHeader;
	private GUIStyle m_HeaderStyle;
	private GUIStyle m_HeaderStyleRightAligned;

	public int GetNumItemsInData()
	{
		return m_BackendData.IDCounter;
	}

	public int GetNumItemsInTree()
	{
		var data = m_TreeView.data as LazyTestDataSource;
		if (data != null)
			return data.itemCounter;

		var data2 = m_TreeView.data as TestDataSource;
		if (data2 != null)
			return data2.itemCounter;

		return -1;
	}
 	
	public TreeViewTest(EditorWindow editorWindow, bool lazy)
	{
		m_EditorWindow = editorWindow;
		m_Lazy = lazy;
	}

	public void Init(Rect rect, BackendData backendData)
	{
		if (m_TreeView != null)
			return;

		m_BackendData = backendData;
		
		TreeViewState state = new TreeViewState();
		state.columnWidths = new float[] {250f, 90f, 93f, 98f, 74f, 78f};
		
		m_TreeView = new TreeView(m_EditorWindow, state);
		ITreeViewGUI gui = new TestGUI (m_TreeView);
		ITreeViewDragging dragging = new TestDragging(m_TreeView, m_BackendData);
		ITreeViewDataSource dataSource;
		if (m_Lazy) dataSource = new LazyTestDataSource(m_TreeView, m_BackendData);
		else		dataSource = new TestDataSource(m_TreeView, m_BackendData);
		m_TreeView.Init(rect, dataSource, gui, dragging);

		m_ColumnHeader = new TreeViewColumnHeader();
		m_ColumnHeader.columnWidths = state.columnWidths;
		m_ColumnHeader.minColumnWidth = 30f;
		m_ColumnHeader.columnRenderer += OnColumnRenderer;
	}

	void OnColumnRenderer(int column, Rect rect)
	{
		if (m_HeaderStyle == null)
		{
			m_HeaderStyle = new GUIStyle(EditorStyles.toolbarButton);
			m_HeaderStyle.padding.left = 4;
			m_HeaderStyle.alignment = TextAnchor.MiddleLeft;

			m_HeaderStyleRightAligned = new GUIStyle(EditorStyles.toolbarButton);
			m_HeaderStyleRightAligned.padding.right = 4;
			m_HeaderStyleRightAligned.alignment = TextAnchor.MiddleRight;

		}

		string[] headers = new[] { "Name", "Date Modified", "Size", "Kind", "Author", "Platform", "Faster", "Slower" };
		GUI.Label(rect, headers[column], (column % 2 == 0) ? m_HeaderStyle : m_HeaderStyleRightAligned);
	}

	public void OnGUI (Rect rect)
	{
		int keyboardControl = GUIUtility.GetControlID(FocusType.Keyboard, rect);

		const float kHeaderHeight = 17f;
		const float kBottomHeight = 20f;
		Rect headerRect = new Rect(rect.x, rect.y, rect.width, kHeaderHeight);
		Rect bottomRect = new Rect(rect.x, rect.yMax - kBottomHeight, rect.width, kBottomHeight);
		
		// Header
		GUI.Label(headerRect, "", EditorStyles.toolbar);
		m_ColumnHeader.OnGUI (headerRect);

		Profiler.BeginSample("TREEVIEW");

		// TreeView
		rect.y += headerRect.height;
		rect.height -= headerRect.height + bottomRect.height;
		m_TreeView.OnEvent();
		m_TreeView.OnGUI(rect, keyboardControl);

		Profiler.EndSample();

		// BottomBar
		GUILayout.BeginArea(bottomRect, GetHeader(), EditorStyles.helpBox);
		GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();
			m_BackendData.m_RecursiveFindParentsBelow = GUILayout.Toggle(m_BackendData.m_RecursiveFindParentsBelow, GUIContent.Temp("Recursive"));
			if (GUILayout.Button("Ping", EditorStyles.miniButton))
			{
				int id = GetNumItemsInData() / 2;
				m_TreeView.Frame(id, true, true);
				m_TreeView.SetSelection(new [] {id}, false);

			}
			if (GUILayout.Button("Frame", EditorStyles.miniButton))
			{
				int id = GetNumItemsInData() / 10;
				m_TreeView.Frame(id, true, false);
				m_TreeView.SetSelection(new[] { id }, false);
			}
		GUILayout.EndHorizontal();
		GUILayout.EndArea();

	}

	private string GetHeader ()
	{
		return (m_Lazy ? "LAZY: " : "FULL: ") + "GUI items: " + GetNumItemsInTree() + "  (data items: " + GetNumItemsInData() + ")";
	}


	public class FooTreeViewItem : TreeViewItem
	{
		public BackendData.Foo foo { get; private set; }
		public FooTreeViewItem(int id, int depth, TreeViewItem parent, string displayName, BackendData.Foo foo)
			: base(id, depth, parent, displayName)
		{
			this.foo = foo;
		}
	}

	class TestDataSource : TreeViewDataSource
	{
		private BackendData m_Backend;
		public int itemCounter { get; private set; }

		public TestDataSource(TreeView treeView, BackendData data) : base(treeView)
		{
			m_Backend = data;
			FetchData();
		}

		public override void FetchData()
		{
			itemCounter = 1;
			m_RootItem = new FooTreeViewItem(m_Backend.root.id, 0, null, m_Backend.root.name, m_Backend.root);
			AddChildrenRecursive(m_Backend.root, m_RootItem);
			m_NeedRefreshVisibleFolders = true;
		}

		void AddChildrenRecursive (BackendData.Foo source, TreeViewItem dest)
		{
			if (source.children != null && source.children.Count > 0)
			{
				dest.children = new FooTreeViewItem [source.children.Count];
				for (int i =0; i<source.children.Count; ++i)
				{
					BackendData.Foo s = source.children[i];
					dest.children[i] = new FooTreeViewItem(s.id, dest.depth+1, dest, s.name, s);
					itemCounter++;
					AddChildrenRecursive(s, dest.children[i]);
				}
			}
		}

		public override bool CanBeParent(TreeViewItem item)
		{
			return item.hasChildren;
		}
	}

	class LazyTestDataSource : LazyTreeViewDataSource
	{
		private BackendData m_Backend;
		public int itemCounter { get; private set; }

		public LazyTestDataSource(TreeView treeView, BackendData data)
			: base(treeView)
		{
			m_Backend = data;
			FetchData();
		}
		
		public override void FetchData()
		{
			// For LazyTreeViewDataSources we just generate the 'm_VisibleRows' items:
			itemCounter = 1;
			m_RootItem = new FooTreeViewItem(m_Backend.root.id, 0, null, m_Backend.root.name, m_Backend.root);
			AddVisibleChildrenRecursive (m_Backend.root, m_RootItem);

			m_VisibleRows = new List<TreeViewItem>();
			GetVisibleItemsRecursive(m_RootItem, m_VisibleRows);
			m_NeedRefreshVisibleFolders = false;
		}

		void AddVisibleChildrenRecursive(BackendData.Foo source, TreeViewItem dest)
		{
			if (IsExpanded(source.id))
			{
				if (source.children != null && source.children.Count > 0)
				{
					dest.children = new TreeViewItem[source.children.Count];
					for (int i = 0; i < source.children.Count; ++i)
					{
						BackendData.Foo s = source.children[i];
						dest.children[i] = new FooTreeViewItem(s.id, dest.depth + 1, dest, s.name, s);
						++itemCounter;
						AddVisibleChildrenRecursive(s, dest.children[i]);
					}
				}
			}
			else
			{
				if (source.hasChildren)
					dest.children = new[] { new TreeViewItem(-1, -1, null, "") }; // add a dummy child in children list to ensure we show the collapse arrow (because we do not fetch data for collapsed items)
			}
		}

		public override bool CanBeParent(TreeViewItem item)
		{
			return item.hasChildren;
		}

		protected override HashSet<int> GetParentsAbove(int id)
		{
			HashSet<int> parentsAbove = new HashSet<int>();
			BackendData.Foo target = BackendData.FindNodeRecursive(m_Backend.root, id);

			while (target != null)
			{
				if (target.parent != null)
					parentsAbove.Add(target.parent.id);
				target = target.parent;
			}
			return parentsAbove;
		}

		protected override HashSet<int> GetParentsBelow (int id)
		{
			HashSet<int> parents = m_Backend.GetParentsBelow(id);
			return parents;			
		}

	}

	class TestGUI : TreeViewGUI
	{
		private Texture2D m_FolderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);
		private Texture2D m_Icon = EditorGUIUtility.FindTexture("boo Script Icon");
		private GUIStyle m_LabelStyle;
		private GUIStyle m_LabelStyleRightAlign;

		public TestGUI(TreeView treeView)
			: base(treeView)
		{
		}

		protected override Texture GetIconForNode(TreeViewItem item)
		{
			return (item.children != null && item.children.Length > 0) ? m_FolderIcon : m_Icon;
		}

		protected override void RenameEnded()
		{
		}

		protected override void SyncFakeItem()
		{
		}

		float[] columnWidths { get { return m_TreeView.state.columnWidths; } }

		protected override void DrawIconAndLabel (Rect rect, TreeViewItem item, string label, bool selected, bool focused, bool useBoldFont, bool isPinging)
		{
			if (m_LabelStyle == null)
			{
				m_LabelStyle = new GUIStyle(s_Styles.lineStyle);
				m_LabelStyle.padding.left = m_LabelStyle.padding.right = 6;

				m_LabelStyleRightAlign = new GUIStyle(s_Styles.lineStyle);
				m_LabelStyleRightAlign.padding.right = m_LabelStyleRightAlign.padding.left = 6;
				m_LabelStyleRightAlign.alignment = TextAnchor.MiddleRight;
			}

			// If pinging just render main label and icon (not columns)
			if (isPinging || columnWidths == null || columnWidths.Length == 0)
			{
				base.DrawIconAndLabel(rect, item, label, selected, focused, useBoldFont, isPinging);
				return;
			}

			Rect columnRect = rect;
			for (int i = 0; i < columnWidths.Length; ++i)
			{
				columnRect.width = columnWidths[i];
				if (i == 0)
					base.DrawIconAndLabel(columnRect, item, label, selected, focused, useBoldFont, isPinging);
				else
					GUI.Label(columnRect, "Zksdf SDFS DFASDF ", (i%2==0)?m_LabelStyle:m_LabelStyleRightAlign);
				columnRect.x += columnRect.width;
			}
		}
	}

	public class TestDragging : TreeViewDragging
	{
		private const string k_GenericDragID = "FooDragging";
		private BackendData m_BackendData;

		class FooDragData
		{
			public FooDragData (List<TreeViewItem> draggedItems)
			{
				m_DraggedItems = draggedItems;
			}
			public List<TreeViewItem> m_DraggedItems;
		}

		public TestDragging (TreeView treeView, BackendData data)
			: base(treeView)
		{
			m_BackendData = data;
		}

		public override void StartDrag(TreeViewItem draggedNode, List<int> draggedItemIDs)
		{
			DragAndDrop.PrepareStartDrag();
			DragAndDrop.SetGenericData(k_GenericDragID, new FooDragData (GetItemsFromIDs (draggedItemIDs)));
			DragAndDrop.objectReferences = new UnityEngine.Object[] { }; // this IS required for dragging to work
			string title = draggedItemIDs.Count + " Foo" + (draggedItemIDs.Count > 1 ? "s" : ""); // title is only shown on OSX (at the cursor)
			DragAndDrop.StartDrag(title);
		}

		public override DragAndDropVisualMode DoDrag (TreeViewItem parentItem, TreeViewItem targetItem, bool perform)
		{
			var dragData = DragAndDrop.GetGenericData(k_GenericDragID) as FooDragData;
			var insertAfterItem = targetItem as FooTreeViewItem;
			var fooParent = parentItem as FooTreeViewItem;
			if (fooParent != null && dragData != null)
			{
				bool validDrag = ValidDrag(parentItem, dragData.m_DraggedItems);
				if (perform && validDrag)
				{
					// Do reparenting here
					List<BackendData.Foo> draggedFoos = (from x in dragData.m_DraggedItems where x is FooTreeViewItem select ((FooTreeViewItem)x).foo).ToList();
					m_BackendData.ReparentSelection(fooParent.foo, insertAfterItem.foo, draggedFoos);
					m_TreeView.ReloadData();
				}
				return validDrag ? DragAndDropVisualMode.Move : DragAndDropVisualMode.None;
			}
			return DragAndDropVisualMode.None;
		}

		bool ValidDrag (TreeViewItem parent, List<TreeViewItem> draggedItems)
		{
			if (!parent.hasChildren)
				return false;

			TreeViewItem currentParent = parent;
			while (currentParent != null)
			{
				if (draggedItems.Contains(currentParent))
					return false;
				currentParent = currentParent.parent;
			}
			return true;
		}

		private List<TreeViewItem> GetItemsFromIDs (IEnumerable<int> draggedItemIDs)
		{
			// Note we only drag visible items here...
			return TreeViewUtility.FindItemsInList (draggedItemIDs, m_TreeView.data.GetVisibleRows());
		}
	}



	public class BackendData
	{
		public class Foo
		{
			public Foo(string name, int depth, int id) { this.name = name;
				this.depth = depth;  this.id = id; }
			public string name { get; set; }
			public int id { get; set; }
			public int depth { get; set; }
			public Foo parent { get; set; }
			public List<Foo> children { get; set; }
			public bool hasChildren { get { return children != null && children.Count > 0; } }
		}

		public Foo root { get { return m_Root; } }

		private Foo m_Root;
		public bool m_RecursiveFindParentsBelow = true;
		public int IDCounter { get; private set; }
		private int m_MaxItems = 10000;
		private const int k_MinChildren = 3;
		private const int k_MaxChildren = 15;
		private const float k_ProbOfLastDescendent = 0.5f;
		private const int k_MaxDepth = 12;

		public void GenerateData (int maxNumItems)
		{
			m_MaxItems = maxNumItems;
			IDCounter = 1;
			m_Root = new Foo("Root", 0, 0);
			while (IDCounter < m_MaxItems)
				AddChildrenRecursive(m_Root, UnityEngine.Random.Range(k_MinChildren, k_MaxChildren), true);
		}

		public HashSet<int> GetParentsBelow(int id)
		{
			Foo searchFromThis = FindNodeRecursive(root, id);
			if (searchFromThis != null)
			{
				if (m_RecursiveFindParentsBelow)
					return GetParentsBelowRecursive(searchFromThis);
				
				return GetParentsBelowStackBased(searchFromThis);
			}
			return new HashSet<int>();
		}

		private HashSet<int> GetParentsBelowStackBased (Foo searchFromThis)
		{
			Stack<Foo> stack = new Stack<Foo>();
			stack.Push(searchFromThis);

			HashSet<int> parentsBelow = new HashSet<int>();
			while (stack.Count > 0)
			{
				Foo current = stack.Pop();
				if (current.hasChildren)
				{
					parentsBelow.Add(current.id);
					foreach (var foo in current.children)
					{
						stack.Push(foo);
					}
				}
			}

			return parentsBelow;
		}

		private HashSet<int> GetParentsBelowRecursive(Foo searchFromThis)
		{
			HashSet<int> result = new HashSet<int>();
			GetParentsBelowRecursive(searchFromThis, result);
			return result;
		}

		private static void GetParentsBelowRecursive(Foo item, HashSet<int> parentIDs)
		{
			if (!item.hasChildren)
				return;
			parentIDs.Add(item.id);
			foreach (var child in item.children)
				GetParentsBelowRecursive(child, parentIDs);
		}

		public void ReparentSelection (Foo parentItem, Foo insertAfterItem, List<Foo> draggedItems)
		{
			// Remove draggedItems from their parents
			foreach (var draggedItem in draggedItems)
			{
				draggedItem.parent.children.Remove(draggedItem);	// remove from old parent
				draggedItem.parent = parentItem;					// set new parent
			}

			if (!parentItem.hasChildren)
				parentItem.children = new List<Foo>();
			var newChildren = new List<Foo> (parentItem.children);

			// Add to parent children after insertAfterItem 
			int insertIndex = 0;
			if (parentItem == insertAfterItem)
			{
				// insert as first child
				insertIndex = 0;
			}
			else
			{
				int index = parentItem.children.IndexOf(insertAfterItem);
				if (index >= 0)
					insertIndex = index + 1;
				else
					Debug.LogError("Did not find insertAfterItem, should be a child of parentItem!!");
			}

			// Insert dragged items under new parent
			newChildren.InsertRange(insertIndex, draggedItems);
			parentItem.children = newChildren;
		}

		void AddChildrenRecursive(Foo foo, int numChildren, bool force)
		{
			if (IDCounter > m_MaxItems)
				return;

			if (foo.depth >= k_MaxDepth)
				return;

			if (!force && UnityEngine.Random.value < k_ProbOfLastDescendent)
				return;

			if (foo.children == null)
				foo.children = new List<Foo>(numChildren);
			for (int i = 0; i < numChildren; ++i)
			{
				Foo child = new Foo("Tud" + IDCounter, foo.depth+1, ++IDCounter);
				child.parent = foo;
				foo.children.Add(child);
			}

			if (IDCounter > m_MaxItems)
				return;
			
			foreach (var child in foo.children)
			{
				AddChildrenRecursive(child, UnityEngine.Random.Range(k_MinChildren, k_MaxChildren), false);
			}

		}

		public static Foo FindNodeRecursive(Foo item, int id)
		{
			if (item == null)
				return null;

			if (item.id == id)
				return item;

			if (item.children == null)
				return null;

			foreach (Foo child in item.children)
			{
				Foo result = FindNodeRecursive(child, id);
				if (result != null)
					return result;
			}
			return null;
		}

	}

	internal class TreeViewColumnHeader
	{
		public float[] columnWidths { get; set; }
		public float minColumnWidth { get; set; }
		public float dragWidth { get; set; }
		public Action<int, Rect> columnRenderer { get; set; }

		public TreeViewColumnHeader()
		{
			minColumnWidth = 10;
			dragWidth = 6f;
		}

		public void OnGUI(Rect rect)
		{
			const float dragAreaWidth = 3f;
			float columnPos = rect.x;
			for (int i = 0; i < columnWidths.Length; ++i)
			{
				Rect columnRect = new Rect(columnPos, rect.y, columnWidths[i], rect.height);
				columnPos += columnWidths[i];
				Rect dragRect = new Rect(columnPos - dragWidth / 2, rect.y, dragAreaWidth, rect.height);
				float deltaX = EditorGUI.MouseDeltaReader(dragRect, true).x;
				if (deltaX != 0f)
				{
					columnWidths[i] += deltaX;
					columnWidths[i] = Mathf.Max(columnWidths[i], minColumnWidth);
				}

				if (columnRenderer != null)
					columnRenderer(i, columnRect);

				if (Event.current.type == EventType.Repaint)
					EditorGUIUtility.AddCursorRect(dragRect, MouseCursor.SplitResizeLeftRight);
			}
		}
	}

}
} // UnityEditor


