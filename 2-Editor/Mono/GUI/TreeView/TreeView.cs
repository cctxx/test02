using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;


namespace UnityEditor
{

/*
 Description:
 * 
 * 

 The TreeView 

// The TreeView requires implementations from the following three interfaces:
//	ITreeViewDataSource:	Should handle data fetching and data structure
//	ITreeViewGUI:			Should handle visual representation of TreeView and mouse input on row controls
//	ITreeViewDragging:		Should handle dragging, temp expansion of items, allow/disallow dropping
// The TreeView handles:	Navigation, Item selection and initiates dragging

 Important concepts:
 1) The Item Tree: the DataSource should create the tree structure of items with parent and children references
 2) Visible rows: the DataSource should be able to provide the visible items; a simple list that will become the rows we render. 
 3) The root Item might not be visible; its up to the data source to deliver a set of visible items from the tree

 */

/// *undocumented*
[System.Serializable]
internal class TreeViewState
{
	public List<int> selectedIDs { get { return m_SelectedIDs; } set { m_SelectedIDs = value; } }
	public int lastClickedID { get { return m_LastClickedID; } set { m_LastClickedID = value; } }
	public List<int> expandedIDs { get { return m_ExpandedIDs; } set { m_ExpandedIDs = value; } }
	public RenameOverlay renameOverlay { get { return m_RenameOverlay; } set { m_RenameOverlay = value; } }
	public CreateAssetUtility createAssetUtility { get { return m_CreateAssetUtility; } set { m_CreateAssetUtility = value; } }
	public float[] columnWidths { get { return m_ColumnWidths; } set { m_ColumnWidths = value; } }
	public Vector2 scrollPos;

	// Selection state
	[SerializeField] private List<int> m_SelectedIDs = new List<int>();
	[SerializeField] private int m_LastClickedID; // used for navigation
	
	// Expanded state (assumed sorted)
	[SerializeField] private List<int> m_ExpandedIDs = new List<int>();
	
	// Rename and create asset state
	[SerializeField] private RenameOverlay m_RenameOverlay = new RenameOverlay();
	[SerializeField] private CreateAssetUtility m_CreateAssetUtility = new CreateAssetUtility();

	// Misc state
	[SerializeField] private float[] m_ColumnWidths = null;

	public void OnAwake ()
	{
		// Clear state that should not survive closing/starting Unity
		m_RenameOverlay.Clear ();
		m_CreateAssetUtility = new CreateAssetUtility();
	}
}

internal class TreeView
{
	public System.Action<int[]> selectionChangedCallback { get; set; }	// ids
	public System.Action<int> itemDoubleClickedCallback { get; set; }	// id
	public System.Action<int[]> dragEndedCallback { get; set; }			// dragged ids, if null then drag was not allowed
	public System.Action<int> contextClickCallback { get; set; }
	public System.Action keyboardInputCallback { get; set; }
	public System.Action<int, Rect> onGUIRowCallback { get; set; }		// <id, Rect of row>

	// Main state
	EditorWindow m_EditorWindow;										// Containing window for this tree: used for checking if we have focus and for requesting repaints
	public ITreeViewDataSource data { get; set; }						// Data provider for this tree: handles data fetching
	public ITreeViewDragging dragging { get; set; }						// Handle dragging
	public ITreeViewGUI gui { get; set; }								// Handles GUI (input and rendering)
	public TreeViewState state { get; set; }							// State that persists script reloads
	public bool deselectOnUnhandledMouseDown {get; set;}

	List<int> m_DragSelection = new List<int>();						// Temp id state while dragging (not serialized)
	bool m_UseScrollView = true;										// Internal scrollview can be omitted when e.g mulitple tree views in one scrollview is wanted
	bool m_AllowRenameOnMouseUp = true;

	// Cached values during one event (for convenience)
	bool m_GrabKeyboardFocus;
	Rect m_TotalRect;
	bool m_HadFocusLastEvent;								// Cached from last event for keyboard focus changed event
	int m_KeyboardControlID;

	const float kSpaceForScrollBar = 16f;	

	public TreeView (EditorWindow editorWindow, TreeViewState treeViewState)
	{
		m_EditorWindow = editorWindow;
		state = treeViewState;
	}

	public void Init (Rect rect, ITreeViewDataSource data, ITreeViewGUI gui, ITreeViewDragging dragging)
	{
		this.data = data;
		this.gui = gui;
		this.dragging = dragging;
		m_TotalRect = rect;		// We initialize the total rect because it might be needed for framing selection when reloading data the first time.
	}

	public bool IsSelected (int id)
	{
		return state.selectedIDs.Contains (id);
	}

	public bool HasSelection ()
	{
		return state.selectedIDs.Count () > 0;
	}

	public int[] GetSelection ()
	{
		return state.selectedIDs.ToArray ();
	}

	bool IsSameAsCurrentSelection (int[] selectedIDs)
	{
		if (selectedIDs.Length != state.selectedIDs.Count)
			return false;

		// By requiring same order we make the test fast for large selection sets
		for (int i=0; i<selectedIDs.Length; ++i)
			if (selectedIDs[i] != state.selectedIDs[i])
				return false;
		return true;
	}

	public int[] GetVisibleRowIDs ()
	{
		return (from item in data.GetVisibleRows() select item.id).ToArray ();
	}

	public void SetSelection (int[] selectedIDs, bool revealSelectionAndFrameLastSelected)
	{
		// Keep for debugging
		//Debug.Log ("SetSelection: new selection: " + DebugUtils.ListToString(new List<int>(selectedIDs)));

		// Init new state
		if (selectedIDs.Length> 0)
		{
			if (revealSelectionAndFrameLastSelected)
			{
				foreach (int id in selectedIDs)
					RevealNode (id);
			}

			state.selectedIDs = new List<int>(selectedIDs); 
			
			// Ensure that our key navigation is setup (Only init last-clicked if new selection set does not have it)
			bool hasLastClicked = state.selectedIDs.IndexOf (state.lastClickedID) >= 0;
			if (!hasLastClicked)
			{
				// Find valid visible id (selected ids might contains scene objects)
				List<TreeViewItem> visibleRows = data.GetVisibleRows ();
				List<int> selectedVisibleIDs = (from item in visibleRows where selectedIDs.Contains(item.id) select item.id).ToList();
				if (selectedVisibleIDs.Count > 0)
					state.lastClickedID = selectedVisibleIDs [selectedVisibleIDs.Count - 1]; 
				else
					state.lastClickedID = 0;
			}

			if (revealSelectionAndFrameLastSelected)
				Frame (state.lastClickedID, true, false);
		}
		else
		{
			state.selectedIDs.Clear ();
			state.lastClickedID = 0;
		}

		// Should not fire callback since this is called from outside
		// NotifyListenersThatSelectionChanged ()
	}

	public TreeViewItem FindNode (int id)
	{
		return data.FindItem (id);
	}

	public void SetUseScrollView (bool useScrollView)
	{
		m_UseScrollView = useScrollView;
	}

	public void Repaint ()
	{
		m_EditorWindow.Repaint ();
	}

	public void ReloadData ()
	{
		// Do not clear rename data here, we could be reloading due to assembly reload
		// and we want to let our rename session survive that

		data.ReloadData();
	}

	public bool HasFocus ()
	{
		return m_EditorWindow.m_Parent.hasFocus && (GUIUtility.keyboardControl == m_KeyboardControlID);
	}

	static internal int GetItemControlID (TreeViewItem item)
	{
		return ((item != null) ? item.id : 0) + 10000000;
	}

	void HandleUnusedMouseEventsForNode (Rect rect, TreeViewItem item)
	{
		int itemControlID = GetItemControlID (item);

		Event evt = Event.current;

		switch (evt.type )
		{
			case EventType.MouseDown:
				if (rect.Contains(Event.current.mousePosition))
				{
					// Handle mouse down on entire line
					if (Event.current.button == 0)
					{
						// Grab keyboard
						GUIUtility.keyboardControl = m_KeyboardControlID;
						Repaint(); // Ensure repaint so we can show we have keyboard focus

						// Let client handle double click
						if (Event.current.clickCount == 2)
						{
							if (itemDoubleClickedCallback != null)
								itemDoubleClickedCallback (item.id);
						}
						else
						{
							// Prepare for mouse up selection change or drag&drop
							m_DragSelection = GetNewSelection(item, true, false);
							GUIUtility.hotControl = itemControlID;
							DragAndDropDelay delay = (DragAndDropDelay)GUIUtility.GetStateObject(typeof(DragAndDropDelay), itemControlID);
							delay.mouseDownPosition = Event.current.mousePosition;
						}
						evt.Use();
					}
					else if (Event.current.button == 1)
					{
						// Right mouse down selects;
						bool keepMultiSelection = true;
						SelectionClick (item, keepMultiSelection);
					}
				}
				break;
			
			case EventType.MouseDrag:
				if (GUIUtility.hotControl == itemControlID && dragging != null)
				{
					DragAndDropDelay delay = (DragAndDropDelay)GUIUtility.GetStateObject(typeof(DragAndDropDelay), itemControlID);
					if (delay.CanStartDrag())
					{
						dragging.StartDrag (item, m_DragSelection);
						GUIUtility.hotControl = 0;
					}

					evt.Use();
				}
				break;
			
			case EventType.MouseUp:
				if (GUIUtility.hotControl == itemControlID)
				{
					// On Mouse up change selection
					if (rect.Contains(evt.mousePosition))
					{
						float indent = gui.GetContentIndent (item);
						Rect renameActivationRect = new Rect (rect.x + indent, rect.y, rect.width - indent, rect.height);
						List<int> selected = state.selectedIDs;
						if (m_AllowRenameOnMouseUp && selected != null && selected.Count == 1 && selected[0] == item.id && renameActivationRect.Contains (evt.mousePosition) && !EditorGUIUtility.HasHolddownKeyModifiers(evt) )
						{
							BeginNameEditing (0.5f);
						}
						else
						{
							SelectionClick (item, false);
						}
						GUIUtility.hotControl = 0;
					}

					m_DragSelection.Clear ();
					evt.Use ();
				}
				break;
			
			case EventType.DragUpdated:
			case EventType.DragPerform:
				{
					if (dragging != null && dragging.DragElement (item, rect))
						GUIUtility.hotControl = 0;
				}
				break;
			
			case EventType.ContextClick:
				if (rect.Contains (evt.mousePosition))
				{
					// Do not use the event so the client can react to the context click (here we just handled the treeview selection)
					if (contextClickCallback != null)
						contextClickCallback (item.id);
				}
				break;
		}
	}

	public void GrabKeyboardFocus ()
	{
		m_GrabKeyboardFocus = true;
	}

	public void NotifyListenersThatSelectionChanged ()
	{
		if (selectionChangedCallback != null)
			selectionChangedCallback (state.selectedIDs.ToArray());		
	}

	public void NotifyListenersThatDragEnded (int[] draggedIDs)
	{
		if (dragEndedCallback != null)
			dragEndedCallback (draggedIDs);
	}

	public Vector2 GetContentSize ()
	{
		return gui.GetTotalSize (data.GetVisibleRows ());
	}

	public Rect GetTotalRect ()
	{
		return m_TotalRect;
	}

	public void OnGUI (Rect rect, int keyboardControlID)
	{
		m_TotalRect = rect;

		m_KeyboardControlID = keyboardControlID;

		// Grab keyboard focus if requested or if we have a mousedown in entire rect
		Event evt = Event.current;
		if (m_GrabKeyboardFocus || (evt.type == EventType.MouseDown && m_TotalRect.Contains (evt.mousePosition)))
		{
			m_GrabKeyboardFocus = false;
			GUIUtility.keyboardControl = m_KeyboardControlID;
			m_AllowRenameOnMouseUp = true; 
			Repaint(); // Ensure repaint so we can show we have keyboard focus
		}

		bool hasFocus = HasFocus (); 

		// Detect got focus (ignore layout event which might get fired infront of mousedown)
		if (hasFocus != m_HadFocusLastEvent && evt.type != EventType.Layout)
		{
			m_HadFocusLastEvent = hasFocus;
			
			// We got focus this event
			if (hasFocus)
			{
				if (evt.type == EventType.MouseDown)
					m_AllowRenameOnMouseUp = false; // If we got focus by mouse down then we do not want to begin renaming if clicking on an already selected item
			}
		}

		List <TreeViewItem> rows = data.GetVisibleRows ();
		//if (rows.Count == 0)
		//	return;

		// Calc content size
		Vector2 contentSize = gui.GetTotalSize(rows);
		Rect contentRect = new Rect (0, 0, contentSize.x, contentSize.y); 

		if (m_UseScrollView)
			state.scrollPos = GUI.BeginScrollView (m_TotalRect, state.scrollPos, contentRect);
		
				gui.BeginRowGUI ();
				
				// Do visible items
				int firstRow, lastRow;
				float topPixel = m_UseScrollView ? state.scrollPos.y : 0f;
				gui.GetFirstAndLastRowVisible (rows, topPixel, m_TotalRect.height, out firstRow, out lastRow);
				for (int row = firstRow; row <= lastRow; ++row)
				{
					TreeViewItem item = rows[row];
					bool selected = m_DragSelection.Count > 0 ? m_DragSelection.Contains (item.id) : state.selectedIDs.Contains(item.id);
					Rect rowRect = gui.OnRowGUI (item, row, GUIClip.visibleRect.width, selected, hasFocus);
					
					if (onGUIRowCallback != null)
					{
						float indent = gui.GetContentIndent (item);
						Rect indentedRect = new Rect (rowRect.x + indent, rowRect.y, rowRect.width - indent, rowRect.height);
						onGUIRowCallback (item.id, indentedRect);
					}

					// Lets check unused events for this row
					HandleUnusedMouseEventsForNode (rowRect, rows[row]);
				}

				gui.EndRowGUI ();
		
		if (m_UseScrollView)
			GUI.EndScrollView();

		HandleUnusedEvents ();
		KeyboardGUI ();
	}
	

	void HandleUnusedEvents ()
	{
		switch (Event.current.type)
		{
			case EventType.DragUpdated:
				if (dragging != null)
				{
					dragging.DragElement(null, new Rect());
					Repaint();
				}
				break;

			case EventType.DragPerform:
				if (dragging != null)
				{
					m_DragSelection.Clear();
					dragging.DragElement(null, new Rect());
					Repaint();
				}
				break;
	
			case EventType.DragExited:
				if (dragging != null)
				{
					m_DragSelection.Clear();
					dragging.DragCleanup(true);
					Repaint();
				}
				break;

			case EventType.MouseDown:
				if (deselectOnUnhandledMouseDown && Event.current.button == 0 && m_TotalRect.Contains(Event.current.mousePosition) &&  state.selectedIDs.Count > 0)
				{
					SetSelection (new int[0], false);
					NotifyListenersThatSelectionChanged ();
				}
				break;
			case EventType.ContextClick:
				if (m_TotalRect.Contains(Event.current.mousePosition) )
				{
					if (contextClickCallback != null)
						contextClickCallback (0);
				}
				break;
		}
	}

	public void OnEvent ()
	{
		state.renameOverlay.OnEvent ();
	}
	
	bool BeginNameEditing (float delay)
	{
 		// Only allow rename by keydown if we have 1 Item selected
 		if (state.selectedIDs.Count == 1)
 		{
			TreeViewItem item = data.FindItem (state.selectedIDs[0]);
			if (item != null)
			{
				if (data.IsRenamingItemAllowed (item)) 
					return gui.BeginRename (item, delay);
			}
			else
			{
				Debug.LogError ("Item not found for renaming with id = " + state.selectedIDs[0]);
			}
		}
 		return false;
	}

	// Let client end renaming from outside
	public void EndNameEditing (bool acceptChanges)
	{
		if (state.renameOverlay.IsRenaming ())
		{
			state.renameOverlay.EndRename (acceptChanges);
			gui.EndRename ();
		}
	}

	void KeyboardGUI ()
	{
		if (m_KeyboardControlID != GUIUtility.keyboardControl || !GUI.enabled)
			return;

		// Let client handle keyboard first
		if (keyboardInputCallback != null)
			keyboardInputCallback ();

		if (Event.current.type == EventType.keyDown)
		{
			switch (Event.current.keyCode)
			{
				// Fold in
				case KeyCode.LeftArrow:
					foreach (int id in state.selectedIDs)
					{
						TreeViewItem item = data.FindItem (id);
						if (item != null)
						{
							if (data.IsExpandable (item) && data.IsExpanded (item))
							{
								if (Event.current.alt)
									data.SetExpandedWithChildren(item, false);
								else
									data.SetExpanded(item, false);
							}
							else
							{
								// Jump to parent 
								if (item.parent != null && state.selectedIDs.Count == 1)
								{	
									// Only allow jump to visible items (e.g. the root might be invisible)
									if (data.GetVisibleRows ().Contains (item.parent))
										SelectionClick (item.parent, false); 
								}
							}
						}
					}
					Event.current.Use();
					break;

				// Fold out
				case KeyCode.RightArrow:
		
					foreach (int id in state.selectedIDs)
					{
						TreeViewItem item = data.FindItem (id);
						if (item != null)
						{
							if (data.IsExpandable (item) && !data.IsExpanded(item))
							{
								if (Event.current.alt)
									data.SetExpandedWithChildren(item, true);
								else
									data.SetExpanded (item, true);

								UserExpandedNode (item);
							}
							else
							{
								// Select first child (for fast expansion of child items)
								if (item.children.Length > 0 && state.selectedIDs.Count == 1)
									SelectionClick (item.children[0], false); 
							}
						}
					}

					Event.current.Use();
					break;

				case KeyCode.UpArrow:
					Event.current.Use();
					OffsetSelection (-1);
					break;

				// Select next or first
				case KeyCode.DownArrow:
					Event.current.Use();
					OffsetSelection(1);
					break;
					
				case KeyCode.Home:
					Event.current.Use();
					OffsetSelection(-1000000);
					break;

				case KeyCode.End:
					Event.current.Use();
					OffsetSelection(1000000);
					break;

				case KeyCode.PageUp:
					{
						Event.current.Use();
						TreeViewItem lastClickedItem = data.FindItem (state.lastClickedID);
						if (lastClickedItem != null)
						{
							int numRowsPageUp = gui.GetNumRowsOnPageUpDown (lastClickedItem, true, m_TotalRect.height);
							OffsetSelection (-numRowsPageUp);
						}
					}
					break;

				case KeyCode.PageDown:
					{
						Event.current.Use();
						TreeViewItem lastClickedItem = data.FindItem (state.lastClickedID);
						if (lastClickedItem != null)
						{
							int numRowsPageDown = gui.GetNumRowsOnPageUpDown (lastClickedItem, true, m_TotalRect.height);
							OffsetSelection (numRowsPageDown);
						}
					}
					break;

				case KeyCode.Return:
				case KeyCode.KeypadEnter:
					if (Application.platform == RuntimePlatform.OSXEditor)
						if (BeginNameEditing (0f))
							Event.current.Use();
					break;

				case KeyCode.F2:
					if (Application.platform == RuntimePlatform.WindowsEditor)
						if (BeginNameEditing (0f))
							Event.current.Use();
					break;

				default:
					if (Event.current.keyCode > KeyCode.A && Event.current.keyCode < KeyCode.Z)
					{
						// TODO: jump to folder with char?
					}
					break;
			}
		}
	}

	static internal int GetIndexOfID (List<TreeViewItem> items, int id)
	{
		for (int i = 0; i < items.Count; ++i)
			if (items[i].id == id)
				return i;
		return -1;
	}

	void OffsetSelection (int offset)
	{
		List<TreeViewItem> visibleRows = data.GetVisibleRows ();

		if (visibleRows.Count <= 1)
			return;

		int index = GetIndexOfID (visibleRows, state.lastClickedID);
		int newIndex = Mathf.Clamp((index != -1 ? index : 0) + offset, 0, visibleRows.Count - 1);
		SelectionByKey (visibleRows[newIndex]);
		EnsureRowIsVisible (newIndex, visibleRows);

		Event.current.Use();
	}

	bool GetFirstAndLastSelected (List<TreeViewItem> items, out int firstIndex, out int lastIndex)
	{
		firstIndex = -1;
		lastIndex = -1;
		for (int i = 0; i < items.Count; ++i)
		{
			if (state.selectedIDs.Contains (items[i].id))
			{
				if (firstIndex == -1)
					firstIndex = i;
				lastIndex = i; // just overwrite and we will have the last in the end...
			}
		}
		return firstIndex != -1 && lastIndex != -1;
	}

	// Returns list of selected ids
	List<int> GetNewSelection (TreeViewItem clickedItem, bool keepMultiSelection, bool useShiftAsActionKey)
	{
		// Get ids from items
		List<TreeViewItem> visibleRows = data.GetVisibleRows(); 
		List<int> allIDs = new List<int> (visibleRows.Count);
		for (int i=0; i<visibleRows.Count; ++i)
			allIDs.Add (visibleRows[i].id);

		List<int> selectedIDs = state.selectedIDs;
		int lastClickedID = state.lastClickedID;
		bool allowMultiselection = data.CanBeMultiSelected (clickedItem);

		return InternalEditorUtility.GetNewSelection (clickedItem.id, allIDs, selectedIDs, lastClickedID, keepMultiSelection, useShiftAsActionKey, allowMultiselection);
	}

	void SelectionByKey (TreeViewItem itemSelected)
	{
		state.selectedIDs = GetNewSelection (itemSelected, false, true);
		state.lastClickedID = itemSelected.id; // Must be set after GetNewSelection (its used there to detect direction when key navigating)

		NotifyListenersThatSelectionChanged ();
	}

	public void RemoveSelection ()
	{
		if (state.selectedIDs.Count > 0)
		{
			state.selectedIDs.Clear ();
			NotifyListenersThatSelectionChanged ();
		}
	}

	void SelectionClick (TreeViewItem itemClicked, bool keepMultiSelection)
	{
		state.selectedIDs = GetNewSelection (itemClicked, keepMultiSelection, false);
		state.lastClickedID = itemClicked != null ? itemClicked.id : 0; // Must be set after GetNewSelection (its used there to detect direction when key navigating)

		NotifyListenersThatSelectionChanged ();
	}

	float EnsureRowIsVisible (int row, List<TreeViewItem> rows)
	{
		float topPixelOfRow = -1f;
		if (row >= 0)
		{
			topPixelOfRow = gui.GetTopPixelOfRow (row, rows);
			float scrollBottom = topPixelOfRow - m_TotalRect.height + gui.GetHeightOfLastRow();
			state.scrollPos.y = Mathf.Clamp(state.scrollPos.y, scrollBottom, topPixelOfRow);
			return topPixelOfRow;
		}
		return topPixelOfRow;
	}

	public void Frame (int id, bool frame, bool ping)
	{
		float topPixelOfRow = -1f;
		TreeViewItem item = null;

		if (frame)
		{
			RevealNode (id);

			List<TreeViewItem> visibleRows = data.GetVisibleRows ();
			int row = GetIndexOfID (visibleRows, id);
			if (row >= 0)
			{
				item = visibleRows [row];
				topPixelOfRow = gui.GetTopPixelOfRow (row, visibleRows);
				EnsureRowIsVisible (row, visibleRows);
			}
		}

		if (ping)
		{
			if (topPixelOfRow == -1f)
			{
				// Was not framed first so we need to 
				List<TreeViewItem> visibleRows = data.GetVisibleRows ();
				int row = GetIndexOfID (visibleRows, id);
				if (row >= 0)
				{
					item = visibleRows[row];
					topPixelOfRow = gui.GetTopPixelOfRow (row, visibleRows);
				}
			}

			if (topPixelOfRow >= 0f && item != null)
			{
				float scrollBarOffset = GetContentSize().y > m_TotalRect.height ? -kSpaceForScrollBar : 0f;
				gui.BeginPingNode (item, topPixelOfRow, m_TotalRect.width + scrollBarOffset);
			}
		}
	}

	public void EndPing ()
	{
		gui.EndPingNode ();
	}

	public bool IsVisible (int id)
	{
		List<TreeViewItem> visibleRows = data.GetVisibleRows ();
		return TreeView.GetIndexOfID(visibleRows, id) >= 0;
	}

	void RevealNode (int id)
	{
		// For very large databases this is a huge optimization
		if (IsVisible (id))
			return;

		// Reveal (expand parents up to root)
		TreeViewItem item = FindNode (id);
		if (item != null)
		{	
			TreeViewItem parent = item.parent;
			while (parent != null)
			{
				data.SetExpanded (parent, true); 
				parent = parent.parent;
			}	
		}
	}

	public void UserExpandedNode (TreeViewItem item)
	{
		// This function ensures we do no lose sight of our main Item but try to show all expanded rows
		// It changes scrollposition if needed.

		// Outcommented because we do not want this behavior right now.

		// 	Item lastNodeVisible = Item;
		// 
		// 	while (lastNodeVisible != null)
		// 	{
		// 		if (lastNodeVisible.children.Length > 0 && data.IsExpanded (lastNodeVisible))
		// 		{
		// 			lastNodeVisible = lastNodeVisible.children[lastNodeVisible.children.Length -1];
		// 		}
		// 		else
		// 		{
		// 			break;
		// 		}
		// 	}
		// 	if (lastNodeVisible != null)
		// 		Frame (lastNodeVisible.id);
		// 	Frame (Item.id);
	}

	// Item holding most basic data for TreeView event handling
	// Extend this class to hold your specific tree data and make your DataSource
	// create items of your type. During e.g OnGUI you can the cast Item to your own type when needed.

}
} // end namespace UnityEditor
