using System.Collections.Generic;
using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{

internal abstract class TreeViewGUI : ITreeViewGUI
{
	protected TreeView m_TreeView;
	PingData m_Ping = new PingData();
	protected Rect m_DraggingInsertionMarkerRect;

	// Icon overlay
	public float iconLeftPadding { get; set; }
	public float iconRightPadding { get; set; }
	public float iconTotalPadding { get { return iconLeftPadding + iconRightPadding; } }
	public System.Action<TreeViewItem, Rect> iconOverlayGUI { get; set; } // Rect includes iconLeftPadding and iconRightPadding

	// Layout
	protected float k_LineHeight = 16f;
	protected float k_BaseIndent = 2f;
	protected float k_IndentWidth = 14f;
	protected float k_FoldoutWidth = 12f;
	protected float k_IconWidth = 16f;
	protected float k_SpaceBetweenIconAndText = 2f;
	protected float k_DropBetweenHeight = 3f; // Pixel height in which the drop between two items is used instead of drop onto a Item
	protected float indentWidth { get { return k_IndentWidth + iconTotalPadding; } }

	// Styles
	internal class Styles
	{
		public GUIStyle foldout = "IN Foldout";
		public GUIStyle insertion = "PR Insertion";
		public GUIStyle ping = new GUIStyle("PR Ping");
		public GUIStyle toolbarButton = "ToolbarButton";
		public GUIStyle lineStyle = new GUIStyle("PR Label");
		public GUIStyle lineBoldStyle = new GUIStyle("PR Label");
		public GUIContent content = new GUIContent (EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName));

		public Styles ()
		{
			lineStyle.alignment = TextAnchor.MiddleLeft;
			lineBoldStyle.alignment = TextAnchor.MiddleLeft;
			lineBoldStyle.font = EditorStyles.boldLabel.font;
			lineBoldStyle.fontStyle = EditorStyles.boldLabel.fontStyle;
			ping.padding.left = 16;
			ping.padding.right = 16;
			ping.fixedHeight = 16; // needed because otherwise height becomes the largest mip map size in the icon
		}
	}
	protected static Styles s_Styles;
	
	public TreeViewGUI (TreeView treeView)
	{
		m_TreeView = treeView;
	}

	void InitStyles()
	{
		if (s_Styles == null)
			s_Styles = new Styles ();
	}

	protected abstract Texture GetIconForNode(TreeViewItem item);

	// ------------------
	// Size section

	// Calc correct width if horizontal scrollbar is wanted return new Vector2(1, height)
	virtual public Vector2 GetTotalSize (List<TreeViewItem> rows)
	{
		InitStyles (); 

		// Width is 1 to prevent showing horizontal scrollbar
		return new Vector2 (1, rows.Count * k_LineHeight); 
	}

	virtual public float GetTopPixelOfRow (int row, List<TreeViewItem> rows)
	{
		return row * k_LineHeight;
	}

	virtual public float GetHeightOfLastRow ()
	{
		return k_LineHeight;
	}

	virtual public int GetNumRowsOnPageUpDown (TreeViewItem fromItem, bool pageUp, float heightOfTreeView)
	{
		return (int)Mathf.Floor(heightOfTreeView / k_LineHeight);
	}

	// Should return the row number of the first and last row thats fits in the pixel rect defined by top and height
	virtual public void GetFirstAndLastRowVisible(List<TreeViewItem> rows, float topPixel, float heightInPixels, out int firstRowVisible, out int lastRowVisible)
	{
		firstRowVisible = (int)Mathf.Floor(topPixel / k_LineHeight);
		lastRowVisible = firstRowVisible + (int)Mathf.Ceil(heightInPixels / k_LineHeight);

		firstRowVisible = Mathf.Max(firstRowVisible, 0);
		lastRowVisible = Mathf.Min(lastRowVisible, rows.Count - 1);
	}

	// ---------------------
	// OnGUI section

	virtual public void BeginRowGUI ()
	{
		InitStyles();

		// Reset
		m_DraggingInsertionMarkerRect.x = -1;
		
		SyncFakeItem (); // After domain reload we ensure to reconstruct new Item state

		// Input for rename overlay (repainted in EndRowGUI to ensure rendered on top)
		if (Event.current.type != EventType.Repaint)
			DoRenameOverlay ();
	}

	virtual public void EndRowGUI()
	{
		// Draw row marker when dragging
		if (m_DraggingInsertionMarkerRect.x >= 0 && Event.current.type == EventType.repaint)
			s_Styles.insertion.Draw(m_DraggingInsertionMarkerRect, false, false, false, false);

		// Render rename overlay last (input is handled in BeginRowGUI)
		if (Event.current.type == EventType.Repaint)
			DoRenameOverlay();

		// Ping a Item
		HandlePing();
	}

	virtual public Rect OnRowGUI (TreeViewItem item, int row, float rowWidth, bool selected, bool focused)
	{
		Rect rowRect = new Rect(0, row * k_LineHeight, rowWidth, k_LineHeight);
		DoNodeGUI (rowRect, item, selected, focused, false);
		return rowRect;
	}

	virtual protected void DoNodeGUI (Rect rect, TreeViewItem item, bool selected, bool focused, bool useBoldFont)
	{
		EditorGUIUtility.SetIconSize (new Vector2(16,16)); // If not set we see icons scaling down if text is being cropped

		float indent = GetFoldoutIndent (item);
		Rect tmpRect = rect;

		int itemControlID = TreeView.GetItemControlID (item);

		bool isDropTarget = false;
		if (m_TreeView.dragging != null)
			isDropTarget = m_TreeView.dragging.GetDropTargetControlID() == itemControlID && m_TreeView.data.CanBeParent(item);
		bool isRenamingThisNode = IsRenaming (item.id);
		bool showFoldout = m_TreeView.data.IsExpandable (item);
		
		// Adjust edit field if needed (on layout rect.width is invalid when using GUILayout)
		if (isRenamingThisNode && rect.width > 1)
		{
			float offset = indent + k_FoldoutWidth + k_IconWidth + iconTotalPadding - 1; // -1 to match text of rename overlay perfect with Item text
			GetRenameOverlay ().editFieldRect = new Rect(rect.x + offset, rect.y, rect.width - offset, rect.height);
		}

		// Draw Item icon and name
		if (Event.current.type == EventType.Repaint)
		{
			string label = item.displayName;

			if (isRenamingThisNode)
			{
				selected = false;
				label = "";
			}

			if (selected)
				s_Styles.lineStyle.Draw(rect, false, false, true, focused);

			if (isDropTarget)
				s_Styles.lineStyle.Draw(rect, GUIContent.none, true, true, false, false);

			DrawIconAndLabel(rect, item, label, selected, focused, useBoldFont, false);
		
			// Show marker below this Item
			if (m_TreeView.dragging != null && m_TreeView.dragging.GetRowMarkerControlID() == itemControlID)
				m_DraggingInsertionMarkerRect = new Rect(rect.x + indent + k_FoldoutWidth, rect.y, rect.width - indent, rect.height);
		}

		// Draw foldout (after text content above to ensure drop down icon is rendered above selection highlight)
		if (showFoldout)
		{
			tmpRect.x = indent;
			tmpRect.width = k_FoldoutWidth;
			EditorGUI.BeginChangeCheck();
			bool newExpandedValue = GUI.Toggle(tmpRect, m_TreeView.data.IsExpanded (item), GUIContent.none, s_Styles.foldout);
			if (EditorGUI.EndChangeCheck())
			{
				if (Event.current.alt)	
					m_TreeView.data.SetExpandedWithChildren(item, newExpandedValue);
				else					
					m_TreeView.data.SetExpanded(item, newExpandedValue);

				if (newExpandedValue)
					m_TreeView.UserExpandedNode (item);
			}
		}

		EditorGUIUtility.SetIconSize (Vector2.zero);
	}

	protected virtual void DrawIconAndLabel (Rect rect, TreeViewItem item, string label, bool selected, bool focused, bool useBoldFont, bool isPinging)
	{
		if (!isPinging)
		{
			// The rect is assumed indented and sized after the content when pinging
			float indent = GetContentIndent(item);
			rect.x += indent;
			rect.width -= indent;
		}

		GUIStyle lineStyle = useBoldFont ? s_Styles.lineBoldStyle : s_Styles.lineStyle;
	
		// Draw text
		lineStyle.padding.left = (int)(k_IconWidth + iconTotalPadding + k_SpaceBetweenIconAndText);
		lineStyle.Draw(rect, label, false, false, selected, focused);

		// Draw icon
		Rect iconRect = rect;
		iconRect.width = k_IconWidth;
		iconRect.height = k_IconWidth;
		iconRect.x += iconLeftPadding;
		Texture icon = GetIconForNode(item);
		if (icon != null)
			GUI.DrawTexture(iconRect, icon);

		if (iconOverlayGUI != null)
		{
			Rect iconOverlayRect = rect;
			iconOverlayRect.width = k_IconWidth + iconTotalPadding;
			iconOverlayGUI (item, iconOverlayRect);
		}
	}


	// Ping Item
	// -------------

	virtual public void BeginPingNode (TreeViewItem item, float topPixelOfRow, float availableWidth)
	{
		if (item == null)
			return;

		// Setup ping
		if (topPixelOfRow >= 0f)
		{
			m_Ping.m_TimeStart = Time.realtimeSinceStartup;
			m_Ping.m_PingStyle = s_Styles.ping;
			
			GUIContent cont = GUIContent.Temp(item.displayName);
			Vector2 contentSize = m_Ping.m_PingStyle.CalcSize(cont);
			
			m_Ping.m_ContentRect = new Rect(GetContentIndent(item), 
											topPixelOfRow,
											k_IconWidth + k_SpaceBetweenIconAndText + contentSize.x + iconTotalPadding, 
											contentSize.y);
			m_Ping.m_AvailableWidth = availableWidth;

			bool useBoldFont = item.displayName.Equals("Assets");
			m_Ping.m_ContentDraw = (Rect r) =>
			{
				// get Item parameters from closure
				DrawIconAndLabel(r, item, item.displayName, false, false, useBoldFont, true);
			};	

			m_TreeView.Repaint();
		}
	}

	virtual public void EndPingNode ()
	{
		m_Ping.m_TimeStart = -1f;
	}

	void HandlePing()
	{
		m_Ping.HandlePing();

		if (m_Ping.isPinging)
			m_TreeView.Repaint();
	}

	//-------------------
	// Rename section

	protected RenameOverlay GetRenameOverlay ()
	{
		return m_TreeView.state.renameOverlay;
	}

	virtual protected bool IsRenaming (int id)
	{
		return GetRenameOverlay().IsRenaming() && GetRenameOverlay ().userData == id && !GetRenameOverlay ().isWaitingForDelay;
	}

	virtual public bool BeginRename (TreeViewItem item, float delay)
	{
		return GetRenameOverlay ().BeginRename (item.displayName, item.id, delay); 
	}

	virtual public void EndRename ()
	{
		// We give keyboard focus back to our tree view because the rename utility stole it (now we give it back)
		if (GetRenameOverlay ().HasKeyboardFocus ())
			m_TreeView.GrabKeyboardFocus ();
	
		RenameEnded ();
		ClearRenameAndNewNodeState (); // Ensure clearing if RenameEnden is overrided
	}

	abstract protected void RenameEnded ();

	virtual public void DoRenameOverlay ()
	{
		if (GetRenameOverlay ().IsRenaming ())
			if (!GetRenameOverlay ().OnGUI ())
				EndRename ();
	}

	abstract protected void SyncFakeItem();


	virtual protected void ClearRenameAndNewNodeState ()
	{
		m_TreeView.data.RemoveFakeItem ();
		GetRenameOverlay ().Clear ();
	}

	virtual public float GetFoldoutIndent (TreeViewItem item)
	{
		return k_BaseIndent + item.depth * indentWidth;
	}

	virtual public float GetContentIndent (TreeViewItem item)
	{
		return k_BaseIndent + item.depth * indentWidth + k_FoldoutWidth;
	}
}


}


