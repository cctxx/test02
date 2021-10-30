using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Collections;
using System.Reflection;
using UnityEditorInternal;
using UnityEngineInternal;


namespace UnityEditor
{

public interface IHasCustomMenu
{
	void AddItemsToMenu(GenericMenu menu);
}

internal interface ICleanuppable
{
	void Cleanup ();
}


// Interface for drag-dropping windows over each other.
// Must be implemented by anyone who can handle a dragged tab.
internal interface IDropArea
{
	// Fill out a dropinfo class telling what should be done.
	// NULL if no action
	DropInfo DragOver (EditorWindow w, Vector2 screenPos);
	
	// If the client returned a DropInfo from the DragOver, they will get this call when the user releases the mouse
	bool PerformDrop (EditorWindow w, DropInfo dropInfo, Vector2 screenPos);
}

internal class DropInfo
{
	internal enum Type
	{
		// The window will be inserted as a tab into dropArea
		Tab = 0,
		// The window will be a new pane (inside a scrollView)
		Pane = 1,
		// A new window should be created.
		Window
	}
	
	public DropInfo (IDropArea source)
	{
		dropArea = source;
	}
	
	// Who claimed the drop?
	public IDropArea dropArea;
	
	// Extra data for the recipient to communicate between DragOVer and PerformDrop
	public object userData = null;

	// Which type of dropzone are we looking for?
	public Type type = Type.Window;
	// Where should the preview end up on screen.
	public Rect rect;
	// Which style should it be displayed with? Can safely be left as null
//	public GUIStyle style;
}

internal class HostView : GUIView
{
	internal static Color kViewColor = new Color (0.76f, 0.76f, 0.76f, 1);
	internal static PrefColor kPlayModeDarken = new PrefColor ("Playmode tint", .8f, .8f, .8f, 1);
	
	internal GUIStyle background;
	[SerializeField]
	protected EditorWindow m_ActualView;
	
	[System.NonSerialized]
	private Rect m_BackgroundClearRect = new Rect(0,0,0,0);
	
	internal EditorWindow actualView
	{
		get { return m_ActualView; }
		set
		{ 
			if (m_ActualView == value)
				return;
			DeregisterSelectedPane(true);
			m_ActualView = value;
			RegisterSelectedPane ();
		}
	}

	protected override void SetPosition(Rect newPos)
	{
		base.SetPosition(newPos);
		if (m_ActualView != null)
			m_ActualView.OnResized ();
	}

	public void OnEnable ()
	{
		background = null;
		RegisterSelectedPane ();
	}
	
	void OnDisable ()
	{
		DeregisterSelectedPane (false);
	}
	
	void OnGUI ()
	{
		DoWindowDecorationStart();
		if (background == null) 
			background = "hostview";
		GUILayout.BeginVertical (background);
		
		if (actualView)
			actualView.m_Pos = screenPosition;
		EditorGUIUtility.ResetGUIState ();
		Invoke ("OnGUI");
		EditorGUIUtility.ResetGUIState ();

		if (m_ActualView.m_FadeoutTime != 0 && Event.current.type == EventType.Repaint)
			m_ActualView.DrawNotification ();

		GUILayout.EndVertical ();
		DoWindowDecorationEnd();

		EditorGUI.ShowRepaints ();
	}
	
	override protected bool OnFocus ()
	{
		Invoke ("OnFocus");
		
		// Callback could have killed us. If so, die now...
		if (this == null)
			return false;
			
		Repaint ();
		return true;
	}

	void OnLostFocus ()
	{
		Invoke ("OnLostFocus");
		Repaint ();
	}

	public new void OnDestroy ()
	{
		if (m_ActualView)
			Object.DestroyImmediate (m_ActualView, true);
		base.OnDestroy ();
	}

	protected System.Type[] GetPaneTypes ()
	{
		return new System.Type[] { 
				typeof(SceneView), 
				typeof(GameView), 
				typeof(InspectorWindow), 
				typeof(HierarchyWindow), 
				typeof(ProjectBrowser),
				typeof(ProfilerWindow), 
				typeof(AnimationWindow)};
	}


	// Messages sent by Unity to editorwindows today. 
	// This is pretty shitty implemented, but oh well... it gets the message across.
	internal void OnProjectChange ()
	{
		Invoke ("OnProjectChange");
	}     

	internal void OnSelectionChange ()
	{
		Invoke ("OnSelectionChange");
	}     

	internal void OnDidOpenScene ()
	{
		Invoke ("OnDidOpenScene");
	}     

	internal void OnInspectorUpdate ()
	{
		Invoke ("OnInspectorUpdate");
	}     

	internal void OnHierarchyChange ()
	{ 
		Invoke ("OnHierarchyChange");
	}     
	
	System.Reflection.MethodInfo GetPaneMethod(string methodName)
	{
		return GetPaneMethod(methodName, m_ActualView);
	}

	System.Reflection.MethodInfo GetPaneMethod (string methodName, object obj)
	{
		if (obj == null)
			return null;

		System.Type t = obj.GetType();
		
		System.Reflection.MethodInfo method = null;
		while (t != null)
		{
			method = t.GetMethod (methodName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
			if (method != null)
				return method;
				
			t = t.BaseType;
		}
		return null;
	}
	
	///  TODO: Optimize with Delegate.CreateDelegate
	protected void Invoke (string methodName)
	{
		Invoke(methodName, m_ActualView);
	}

	protected void Invoke(string methodName, object obj)
	{
		System.Reflection.MethodInfo mi = GetPaneMethod(methodName, obj);
		if (mi != null)
			mi.Invoke(obj, null);
	}
	
	protected void RegisterSelectedPane ()
	{
		if (!m_ActualView)
			return;
		m_ActualView.m_Parent = this;
	
		if (GetPaneMethod ("Update") != null)
			EditorApplication.update += SendUpdate;
		
		if (GetPaneMethod ("ModifierKeysChanged") != null)
			EditorApplication.modifierKeysChanged += SendModKeysChanged;
			
		m_ActualView.MakeParentsSettingsMatchMe();
		
		if (m_ActualView.m_FadeoutTime != 0) {
			EditorApplication.update += m_ActualView.CheckForWindowRepaint;
		}

		Invoke("OnBecameVisible");
		Invoke("OnFocus");
	}
	
	protected void DeregisterSelectedPane (bool clearActualView)
	{
		if (!m_ActualView)
			return;

		if (GetPaneMethod ("Update") != null)
			EditorApplication.update -= SendUpdate;
		
		if (GetPaneMethod ("ModifierKeysChanged") != null)
			EditorApplication.modifierKeysChanged -= SendModKeysChanged;
		
		if (m_ActualView.m_FadeoutTime != 0)
		{
			EditorApplication.update -= m_ActualView.CheckForWindowRepaint;
		}
		
		if (clearActualView)
		{
			EditorWindow oldActualView = m_ActualView;
			m_ActualView = null;
			Invoke("OnLostFocus", oldActualView);
			Invoke("OnBecameInvisible", oldActualView);
		}
	}
	
	void SendUpdate () { Invoke ("Update"); }
	void SendModKeysChanged () { Invoke ("ModifierKeysChanged"); }
	
	internal RectOffset borderSize { get { return GetBorderSize ();	} }
	
	protected virtual RectOffset GetBorderSize () {	return new RectOffset (); }
	
	protected void ShowGenericMenu ()
	{
		GUIStyle gs = "PaneOptions";
		Rect paneMenu = new Rect (position.width - gs.fixedWidth - 4, Mathf.Floor (background.margin.top + 20 - gs.fixedHeight), gs.fixedWidth, gs.fixedHeight);
		if (EditorGUI.ButtonMouseDown (paneMenu,GUIContent.none, FocusType.Passive, "PaneOptions"))
			PopupGenericMenu (m_ActualView, paneMenu);

		// Give panes an option of showing a small button next to the generic menu (used for inspector lock icon
		System.Reflection.MethodInfo mi = GetPaneMethod("ShowButton", m_ActualView);
		if (mi != null)
		{
			object[] lockButton = { new Rect (position.width - gs.fixedWidth - 20, Mathf.Floor (background.margin.top + 4), 16, 16) };
			
			mi.Invoke(m_ActualView, lockButton);			
		}
	}
	
	public void PopupGenericMenu (EditorWindow view, Rect pos)
	{
		GenericMenu menu = new GenericMenu();
			
		IHasCustomMenu menuProviderFactoryThingy = view as IHasCustomMenu;
		if (menuProviderFactoryThingy != null)
			menuProviderFactoryThingy.AddItemsToMenu (menu);

		AddDefaultItemsToMenu (menu, view);
		menu.DropDown (pos);
		Event.current.Use();
	}
 
	
	protected virtual void AddDefaultItemsToMenu (GenericMenu menu, EditorWindow view) {}

	protected void ClearBackground ()
	{
		if (Event.current.type != EventType.repaint)
			return;
		EditorWindow view = actualView;
		if (view != null && view.dontClearBackground)
		{
			if (backgroundValid && position == m_BackgroundClearRect)
				return;
		}
		Color col = EditorGUIUtility.isProSkin ? EditorGUIUtility.kDarkViewBackground : kViewColor;
		GL.Clear (true, true, EditorApplication.isPlayingOrWillChangePlaymode ? col * kPlayModeDarken : col);
		backgroundValid = true;
		m_BackgroundClearRect = position;
	}
}

internal class DockArea : HostView, IDropArea
{
	internal const float kTabHeight = 17;
	internal const float kDockHeight = 39;
	const float kSideBorders = 2.0f;
	const float kBottomBorders = 2.0f;
	const float kWindowButtonsWidth = 40.0f; // Context & Lock Inspector Buttons

	// Which pane window would we drop the currently dragged pane over
	static int s_PlaceholderPos;
	// Which pane is currently being dragged around
	static EditorWindow s_DragPane;
	// Where did it come from
	internal static DockArea s_OriginalDragSource;

	// Mouse coords when we started the drag (used to figure out when we should trigger a drag)
	static Vector2 s_StartDragPosition;
	// Are we dragging yet?
	static int s_DragMode;
	// A view that shouldn't be docked to (to make sure we don't attach to a single view with only the tab that we're dragging)
	static internal View s_IgnoreDockingForView = null;
	
	static DropInfo s_DropInfo = null;

	[SerializeField]
	internal List<EditorWindow> m_Panes = new List<EditorWindow> ();
	[SerializeField]
	internal int m_Selected;
	[SerializeField]
	internal int m_LastSelected;
	
	
	[System.NonSerialized]
	internal GUIStyle tabStyle = null;

	public int selected
	{
		get { return m_Selected; }
		set
		{ 
			if (m_Selected == value) 
				return;
			m_Selected = value;
			if (m_Selected >= 0 && m_Selected < m_Panes.Count)
				actualView = m_Panes[m_Selected];
		}
	}

	public DockArea ()
	{
		if (m_Panes != null && m_Panes.Count !=0)
			Debug.LogError("m_Panes is filled in DockArea constructor.");
	}

	void RemoveNullWindows ()
	{
		List<EditorWindow> result = new List<EditorWindow>();
		foreach(EditorWindow i in m_Panes)
		{
			if (i != null)
				result.Add (i	);
		}
		m_Panes = result;
	}
		
	public new void OnDestroy ()
	{
		if (hasFocus)
			Invoke ("OnLostFocus");		
		
		actualView = null;
		foreach (EditorWindow w in m_Panes) 
			Object.DestroyImmediate (w, true);
		
		base.OnDestroy ();
	}
	
	public new void OnEnable ()
	{
		if (m_Panes != null && m_Panes.Count > m_Selected)
			actualView = m_Panes[m_Selected];
		base.OnEnable ();
	}
	
	public void AddTab (EditorWindow pane)
	{
		AddTab (m_Panes.Count, pane);
	}
	
	public void AddTab (int idx, EditorWindow pane)
	{ 
		DeregisterSelectedPane (true);
		m_Panes.Insert (idx, pane);
		m_ActualView = pane; m_Panes[idx] = pane;
		m_Selected = idx;

		RegisterSelectedPane ();
		Repaint ();
		
	}
	
	public void RemoveTab (EditorWindow pane) { RemoveTab (pane, true); }
	public void RemoveTab (EditorWindow pane, bool killIfEmpty)
	{
		if (m_ActualView == pane)
			DeregisterSelectedPane (true);
		int idx = m_Panes.IndexOf (pane);
		if (idx == -1)
		{
			Debug.LogError ("Unable to remove Pane - it's not IN the window");
			return;	
		}

		m_Panes.Remove (pane);

		if (idx == m_Selected)
		{
			if (m_LastSelected >= m_Panes.Count - 1) 
				m_LastSelected = m_Panes.Count - 1;
			m_Selected = m_LastSelected;
			if (m_Selected > -1)
				m_ActualView = m_Panes[m_Selected];
		} 
		else if (idx < m_Selected)
		{
			m_Selected--;
		}

		Repaint ();
		pane.m_Parent = null;
		if (killIfEmpty) 
			KillIfEmpty();
		RegisterSelectedPane ();
	}
	
	void KillIfEmpty ()
	{
		// if we're empty, remove ourselves
		if (m_Panes.Count != 0)
			return;
		
		if (parent == null)
		{
			window.InternalCloseWindow ();
			return;
		}

		SplitView sw = parent as SplitView;
		ICleanuppable p = parent as ICleanuppable;
		sw.RemoveChildNice (this); 
		
		DestroyImmediate (this, true);

		if (p != null) 
			p.Cleanup ();
	}
	
	Rect tabRect { get { return new Rect (0,0, position.width, kTabHeight); } }
	
	public DropInfo DragOver (EditorWindow window, Vector2 mouseScreenPosition)
	{
		Rect r = screenPosition;
		r.height = kDockHeight;
		if (r.Contains (mouseScreenPosition))
		{
			if (background == null) 
				background = "hostview";
			Rect scr = background.margin.Remove (screenPosition);
			Vector2 pos = mouseScreenPosition - new Vector2 (scr.x, scr.y);

			Rect tr = tabRect;
			int mPos = GetTabAtMousePos (pos, tr);
			float w = GetTabWidth (tr.width);

			if (s_PlaceholderPos != mPos)
			{
				Repaint ();
				s_PlaceholderPos = mPos;
			}

//			if (s_DropInfo != null && s_DropInfo.dropArea != this)
//				s_DropInfo.dropArea.Repaint ();
//			Repaint ();

			DropInfo di = new DropInfo (this);
			di.type = DropInfo.Type.Tab;
			di.rect = new Rect (pos.x - w * .25f + scr.x, tr.y + scr.y, w, tr.height);

			return di;
		}
		return null;
	}

	public bool PerformDrop (EditorWindow w, DropInfo info, Vector2 screenPos)
	{
		s_OriginalDragSource.RemoveTab (w, s_OriginalDragSource != this);
		int pos2 = s_PlaceholderPos > m_Panes.Count ? m_Panes.Count : s_PlaceholderPos;
		AddTab (pos2, w);
		selected = pos2;
		return true;
	}
	
	public void OnGUI ()
	{
		ClearBackground ();

		// Add CursorRects
		SplitView sp = parent as SplitView;

		if (Event.current.type == EventType.Repaint && sp)
		{
			View view = this;
			while (sp)
			{
				int id = sp.controlID;

				if (id == GUIUtility.hotControl || GUIUtility.hotControl == 0)
				{
					int idx = sp.IndexOfChild(view);
					if (sp.vertical)
					{
						if (idx != 0)
							EditorGUIUtility.AddCursorRect(new Rect(0, 0, position.width, SplitView.kGrabDist), MouseCursor.SplitResizeUpDown, id);
						if (idx != sp.children.Length - 1)
							EditorGUIUtility.AddCursorRect(
								new Rect(0, position.height - SplitView.kGrabDist, position.width, SplitView.kGrabDist),
								MouseCursor.SplitResizeUpDown, id);
					}
					else // horizontal
					{
						if (idx != 0)
							EditorGUIUtility.AddCursorRect(new Rect(0, 0, SplitView.kGrabDist, position.height), MouseCursor.SplitResizeLeftRight,
							                               id);
						if (idx != sp.children.Length - 1)
							EditorGUIUtility.AddCursorRect(
								new Rect(position.width - SplitView.kGrabDist, 0, SplitView.kGrabDist, position.height),
								MouseCursor.SplitResizeLeftRight, id);
					}
				}

				view = sp;
				sp = sp.parent as SplitView;
			}
			
			// reset
			sp = parent as SplitView;
		}
		bool customBorder = false;
		if (window.mainView.GetType() != typeof (MainWindow)) 
		{
			customBorder = true;
			if (windowPosition.y == 0) 
				background = "dockareaStandalone";
			else 
				background = "dockarea";		
			
		}
		else 
			background = "dockarea";		
	
		if (EditorApplication.isPlayingOrWillChangePlaymode)
			GUI.color = kPlayModeDarken;
		if (sp)
		{
			Event e = new Event (Event.current);
			e.mousePosition += new Vector2 (position.x, position.y);
			sp.SplitGUI (e);
			if (e.type == EventType.Used)
				Event.current.Use();			
		}
		GUIStyle overlay = "dockareaoverlay";
		Rect r = background.margin.Remove (new Rect (0,0,position.width, position.height));
		r.x = background.margin.left;
		r.y = background.margin.top;
		Rect wPos = windowPosition;
		float sideBorder = kSideBorders;
		if (wPos.x == 0)
		{
			r.x -= sideBorder;
			r.width += sideBorder;
		}
		if (wPos.xMax == window.position.width)
		{
			r.width += sideBorder;
		}
		
		if (wPos.yMax == window.position.height)
		{
			r.height += customBorder ? 2f : kBottomBorders;
		}
		GUI.Box (r,GUIContent.none, background);
		if (tabStyle == null)
			tabStyle = "dragtab";

		DragTab (new Rect (r.x + 1, r.y, r.width - kWindowButtonsWidth, kTabHeight), tabStyle);

		// TODO: Make this nice - right now this is meant to be overridden by Panes in Layout if they want something else. Inspector does this
		tabStyle = "dragtab";

		ShowGenericMenu ();
		if (m_Panes.Count > 0)
		{
			DoWindowDecorationStart();

			if (m_Panes[selected] is GameView) // GameView exits GUI, so draw overlay border earlier
				GUI.Box (r,GUIContent.none, overlay);

			// Contents:
			// scroll it by -1, -1 so we top & left 1px gets culled (they are drawn already by the us, so we want to 
			// not have them here (thing that the default skin relies on)
			BeginOffsetArea (new Rect (r.x + 2,r.y + kTabHeight,r.width - 4, r.height - kTabHeight - 2), GUIContent.none, "TabWindowBackground");
			
			// Set up the pane's position, so its GUI can use this 
			Vector2 basePos = GUIUtility.GUIToScreenPoint (Vector2.zero);
			Rect p = borderSize.Remove (position);
			p.x = basePos.x;
			p.y = basePos.y;
			m_Panes[selected].m_Pos = p;
			
			// Draw it
			EditorGUIUtility.ResetGUIState ();
			try
			{
				Invoke ("OnGUI");
			}
			catch (TargetInvocationException e)
			{
				// hack: translate it to InnerException
				throw e.InnerException;
			}
			EditorGUIUtility.ResetGUIState ();

			if (actualView != null && actualView.m_FadeoutTime != 0 && Event.current != null && Event.current.type == EventType.Repaint)
				actualView.DrawNotification ();
			
			EndOffsetArea ();
			DoWindowDecorationEnd();
		} 
		
		GUI.Box (r,GUIContent.none, overlay);
//		if (GUI.Button (new Rect (position.width-250, 0 , 250,20), "Hmm"))
//			EditorUtility.DisplayObjectContextMenu(Vector2.zero, this, 0);

//		GUI.Label (new Rect (position.width-250, 0 , 250,20), "min:"+minSize + "  max:" + maxSize,"Label");
//		if (GUI.Button (new Rect (position.width-100, 0 , 100,15), "Debug"))		Debug.Log (window.DebugHierarchy ());
		
		EditorGUI.ShowRepaints ();
		Highlighter.ControlHighlightGUI (this);
	}

	void Maximize (object userData)
	{
		EditorWindow ew = ((EditorWindow)userData);
		WindowLayout.Maximize(ew);
	}
	
	void Close (object userData) {
		((EditorWindow)userData).Close ();
	}

	protected override void AddDefaultItemsToMenu (GenericMenu menu, EditorWindow view)
	{
		if (menu.GetItemCount () != 0)
			menu.AddSeparator ("");

		if(parent.window.showMode == ShowMode.MainWindow)
			menu.AddItem (EditorGUIUtility.TextContent ("DockAreaMaximize"), !(parent is SplitView), Maximize, view);
		else
			menu.AddDisabledItem(EditorGUIUtility.TextContent ("DockAreaMaximize"));
			
		menu.AddItem (EditorGUIUtility.TextContent ("DockAreaCloseTab"), false, Close, view);
		menu.AddSeparator ("");

		System.Type[] types = GetPaneTypes ();
		GUIContent baseContent = EditorGUIUtility.TextContent ("DockAreaAddTab");
		foreach (System.Type t in types)
		{
			if (t == null) 
				continue;
			GUIContent entry = new GUIContent (EditorGUIUtility.TextContent (t.ToString()));
			entry.text = baseContent.text + "/" + entry.text; 
			menu.AddItem (entry, false, AddTabToHere, t);
		}
	}

	void AddTabToHere (object userData) 
    {
       /* if ((System.Type)userData == typeof(ASMainWindow))
        {
            ASEditorBackend.asMainWin = EditorWindow.GetWindowDontShow<ASMainWindow>();
            ASEditorBackend.asMainWin.RemoveFromDockArea();
            AddTab(ASEditorBackend.asMainWin);
        }
        else if ((System.Type)userData == typeof(AssetStoreWindow))
       	{
       		EditorWindow tmp = EditorWindow.GetWindowDontShow<AssetStoreWindow>();
       		tmp.RemoveFromDockArea();
       		AddTab(tmp);
       	}
        else
        {*/
            EditorWindow win = (EditorWindow)CreateInstance((System.Type)userData);
            AddTab(win);
        //}
	}

	static public void EndOffsetArea ()
	{
		if (Event.current.type == EventType.Used)
			return;
		GUILayoutUtility.EndLayoutGroup ();
		GUI.EndGroup ();
	}

	static public void BeginOffsetArea (Rect screenRect, GUIContent content, GUIStyle style)
	{
		GUILayoutGroup g = EditorGUILayoutUtilityInternal.BeginLayoutArea (style, typeof (GUILayoutGroup));
		switch (Event.current.type) {
		case EventType.Layout:
			g.resetCoords = false;
			g.minWidth = g.maxWidth = screenRect.width + 1;
			g.minHeight = g.maxHeight = screenRect.height + 2;
			g.rect = Rect.MinMaxRect(-1, -1, g.rect.xMax, g.rect.yMax-10);
			break;
		}
		GUI.BeginGroup (screenRect, content, style);
	}
	
	float GetTabWidth (float width)
	{
		int count = m_Panes.Count;
		if (s_DropInfo != null && System.Object.ReferenceEquals (s_DropInfo.dropArea, this))
			count++;
		if (m_Panes.IndexOf (s_DragPane) != -1) 
			count--;
		
		return Mathf.Min (width / count, 100);
	}
	
	int GetTabAtMousePos (Vector2 mousePos, Rect position)
	{
		int sel = (int)Mathf.Min ((mousePos.x - position.xMin) / GetTabWidth (position.width), 100);
		return sel;
	}	
	

	// Hack to get around Unity crashing when we have circular references in saved stuff
	internal override void Initialize (ContainerWindow win)
	{
		base.Initialize (win);
		RemoveNullWindows ();
		foreach (EditorWindow i in m_Panes)
			i.m_Parent = this;
	}
		
	static void CheckDragWindowExists ()
	{
		if (s_DragMode == 1 && !PaneDragTab.get.m_Window)
		{
			s_OriginalDragSource.RemoveTab (s_DragPane);
			DestroyImmediate (s_DragPane);
			PaneDragTab.get.Close ();
			GUIUtility.hotControl = 0;
			ResetDragVars ();
		}
	}

	void DragTab (Rect pos, GUIStyle tabStyle)
	{
		int id = GUIUtility.GetControlID (FocusType.Passive);
		float elemWidth = GetTabWidth(pos.width);
		
		Event evt = Event.current;

		// Detect if hotcontrol was cleared while dragging (happens when pressing Esc).
		// We do not listen for the Escape keydown event because it is sent to the dragged window (not this dockarea)
		if (s_DragMode != 0 && GUIUtility.hotControl == 0)
		{
			PaneDragTab.get.Close();
			ResetDragVars();
		}

		switch (evt.GetTypeForControl (id)) {
		case EventType.MouseDown:
			if (pos.Contains (evt.mousePosition) && GUIUtility.hotControl == 0) {
				int sel = GetTabAtMousePos (evt.mousePosition, pos);
				if (sel < m_Panes.Count) {
					switch (evt.button) {
					case 0:
						if (sel != selected)
							selected = sel;
						
						GUIUtility.hotControl = id;
						s_StartDragPosition = evt.mousePosition;
						s_DragMode = 0;
						evt.Use ();
						break;
					case 2:
						m_Panes[sel].Close ();
						evt.Use ();
						break;
					}
				}
			}
			break;
		case EventType.ContextClick:
			if (pos.Contains (evt.mousePosition) && GUIUtility.hotControl == 0)
			{
				int sel = GetTabAtMousePos (evt.mousePosition, pos);
				if (sel < m_Panes.Count)
					PopupGenericMenu (m_Panes[sel], new Rect (evt.mousePosition.x, evt.mousePosition.y, 0, 0));
			}
			
			break;
		case EventType.MouseDrag:
			if (GUIUtility.hotControl == id) {
				Vector2 delta = evt.mousePosition - s_StartDragPosition;
				evt.Use ();
				Rect screenRect = screenPosition;
			
				// if we're not tabdragging yet, check to see if we should start
				if (s_DragMode == 0 && delta.sqrMagnitude > 99) {
					s_DragMode = 1;
					s_PlaceholderPos = selected;
					s_DragPane = m_Panes[selected];
					
					// If we're moving the only editorwindow in this dockarea, we'll be destroyed - so it looks silly if we can attach as children of ourselves
					if (m_Panes.Count == 1)
						s_IgnoreDockingForView = this;
					else
						s_IgnoreDockingForView = null;

					s_OriginalDragSource = this;
//					GenerateAllDropZones ();
//					EditorApplication.windowsReordered += GenerateAllDropZones;
					PaneDragTab.get.content = s_DragPane.cachedTitleContent;

					// make sure that our window's contents are sitting in the backbuffer, so that
					// we are reading the proper pixels in GrabThumbnail()
					Internal_SetAsActiveWindow();

					PaneDragTab.get.GrabThumbnail();
					PaneDragTab.get.Show (new Rect(pos.x + screenRect.x + elemWidth * selected, pos.y + screenRect.y, elemWidth, pos.height), GUIUtility.GUIToScreenPoint (evt.mousePosition));
					EditorApplication.update += CheckDragWindowExists;
					// We just showed a window. Exit the GUI because the window might be
					// repainting already (esp. on Windows)
					GUIUtility.ExitGUI ();
				}
				if (s_DragMode == 1) {
					// Go over all container windows, ask them to dock the window.
					DropInfo di = null;
					ContainerWindow[] windows = ContainerWindow.windows;
					Vector2 screenMousePos = GUIUtility.GUIToScreenPoint  (evt.mousePosition);
					ContainerWindow win = null;
					foreach (ContainerWindow w in windows)
					{
						foreach (View view in w.mainView.allChildren)
						{
							IDropArea ida = view as IDropArea;
							if (ida != null)
								di = ida.DragOver (s_DragPane, screenMousePos);
							
							if (di != null)
								break;
						}
						if (di != null)
						{
							win = w;
							break;
						}						
					}				
					// Ok, we couldn't find anything, let's create a simplified DropIn
					if (di == null)
					{
						di = new DropInfo (null);
					}
					
					if (di.type != DropInfo.Type.Tab)
						s_PlaceholderPos = -1;
					
					s_DropInfo = di;
						
					// Handle the window getting closed mid-drag
					if (PaneDragTab.get.m_Window)
						PaneDragTab.get.SetDropInfo (di, screenMousePos, win);
				}
			}
			break;
		case EventType.MouseUp:
			if (GUIUtility.hotControl == id) {
				Vector2 screenMousePos = GUIUtility.GUIToScreenPoint (evt.mousePosition);
				if (s_DragMode != 0) {
					// This is where we want to insert it.
					s_DragMode = 0;	
					PaneDragTab.get.Close ();
					EditorApplication.update -= CheckDragWindowExists;

					// Try to tell the current DPZ 
					if (s_DropInfo != null && s_DropInfo.dropArea != null) {
						s_DropInfo.dropArea.PerformDrop (s_DragPane, s_DropInfo, screenMousePos);
					} else {
						EditorWindow w = s_DragPane;

						ResetDragVars ();
						
						RemoveTab (w);
						Rect wPos = w.position;
						wPos.x = screenMousePos.x - wPos.width *.5f;
						wPos.y = screenMousePos.y - wPos.height *.5f;

						// don't put windows top outside of the screen, on mac OS handles this
						if (Application.platform == RuntimePlatform.WindowsEditor)
							wPos.y = Mathf.Max(InternalEditorUtility.GetBoundsOfDesktopAtPoint(screenMousePos).y, wPos.y);							

						EditorWindow.CreateNewWindowForEditorWindow (w, false, false);

						w.position = w.m_Parent.window.FitWindowRectToScreen(wPos, true, true);

						GUIUtility.hotControl = 0;
						GUIUtility.ExitGUI ();
					}
					ResetDragVars ();
				} 
				GUIUtility.hotControl = 0;
				evt.Use ();
			}
			
			break;
				
		case EventType.Repaint:
			float xPos = pos.xMin;
			int drawNum = 0;
			for (int i = 0; i < m_Panes.Count; i++) {
				// if we're dragging the tab we're about to draw, don't do that (handled by some window)
				if (s_DragPane == m_Panes[i]) 
					continue;
				
				// If we need space for inserting a tab here, skip some horizontal
				if (s_DropInfo != null && System.Object.ReferenceEquals (s_DropInfo.dropArea, this) && s_PlaceholderPos == drawNum)
					xPos += elemWidth;
				
				Rect r = new Rect (xPos, pos.yMin, elemWidth, pos.height);
				float roundR = Mathf.Round (r.x);
				Rect r2 = new Rect (roundR, r.y, Mathf.Round (r.x + r.width) - roundR, r.height);
				tabStyle.Draw (r2, m_Panes[i].cachedTitleContent, false, false, i == selected, hasFocus);
				xPos += elemWidth;
				drawNum++;
			}
			break;
		}
		selected = Mathf.Clamp (selected, 0, m_Panes.Count - 1);
	}
	
	protected override RectOffset GetBorderSize () {
		if (!window) {
			return new RectOffset ();
		}
		RectOffset borders = new RectOffset ();
		Rect r = windowPosition;
		if (r.xMin != 0)
			borders.left += (int)kSideBorders;
		if (r.xMax != window.position.width) {
			borders.right += (int)kSideBorders;
		}

		borders.top = (int)kTabHeight;

		// Aras: I don't really know why, but this makes GUI be actually correct.
		bool touchesTop = windowPosition.y == 0;
		bool touchesBottom = r.yMax == window.position.height;
		borders.bottom = 4;
		if (touchesBottom)
			borders.bottom -= 2;
		if (touchesTop)
			borders.bottom += 3;

		return borders;
	}
	
	static void ResetDragVars () {
		s_DragPane = null;
		s_DropInfo = null;
		s_PlaceholderPos = -1;
		s_DragMode = 0;
		s_OriginalDragSource = null;
	}
}

internal class MaximizedHostView : HostView 
{
	public void OnGUI () 
	{
		ClearBackground ();

		Rect r = new Rect (-2,0,position.width+4, position.height);
		background = "dockarea";
		GUIStyle overlay = "dockareaoverlay";
		r = background.margin.Remove (r);
		DoWindowDecorationStart ();
		Rect backRect = new Rect (r.x + 1, r.y, r.width - 2, DockArea.kTabHeight);
		if (Event.current.type == EventType.Repaint) {
			background.Draw (r, GUIContent.none, false, false, false, false);
			GUIStyle s = "dragTab";
			s.Draw (backRect, actualView.cachedTitleContent, false, false, true, hasFocus);
		}

		if (Event.current.type == EventType.ContextClick && backRect.Contains (Event.current.mousePosition)) {
			PopupGenericMenu (actualView, new Rect (Event.current.mousePosition.x, Event.current.mousePosition.y, 0, 0));
		}
		ShowGenericMenu ();
		if (actualView)
		{
			actualView.m_Pos = borderSize.Remove (screenPosition);
			if (actualView is GameView) // GameView exits GUI, so draw overlay border earlier
				GUI.Box (r, GUIContent.none, overlay);
		}
		DockArea.BeginOffsetArea (new Rect (r.x + 2, r.y + DockArea.kTabHeight, r.width - 4, r.height - DockArea.kTabHeight - 2), GUIContent.none, "TabWindowBackground");
		
		EditorGUIUtility.ResetGUIState ();
		try
		{
			Invoke ("OnGUI");
		}
		catch (TargetInvocationException e)
		{
			// hack: translate it to InnerException
			throw e.InnerException;
		}
		EditorGUIUtility.ResetGUIState ();
					
		DockArea.EndOffsetArea ();
		DoWindowDecorationEnd();

		GUI.Box (r,GUIContent.none, overlay);
	}

	protected override RectOffset GetBorderSize ()
	{
		RectOffset borders = new RectOffset ();
		borders.top = (int)DockArea.kTabHeight;

		// Aras: I don't really know why, but this makes GUI be actually correct.
		borders.bottom = 4;

		return borders;
	}

	void Unmaximize (object userData)
	{
		EditorWindow ew = ((EditorWindow)userData);
		WindowLayout.Unmaximize(ew);
	}
	
	protected override void AddDefaultItemsToMenu (GenericMenu menu, EditorWindow view) {
		if (menu.GetItemCount () != 0)
			menu.AddSeparator ("");

		menu.AddItem (EditorGUIUtility.TextContent ("DockAreaMaximize"), !(parent is SplitView), Unmaximize, view);
		menu.AddDisabledItem (EditorGUIUtility.TextContent ("DockAreaCloseTab"));
		menu.AddSeparator ("");
		System.Type[] types = GetPaneTypes ();
			
		GUIContent baseContent = EditorGUIUtility.TextContent ("DockAreaAddTab");
		foreach (System.Type t in types) {
			if (t == null) 
				continue;
			GUIContent entry = new GUIContent (EditorGUIUtility.TextContent (t.ToString()));
			entry.text = baseContent.text + "/" + entry.text; 
			menu.AddDisabledItem (entry);
		}
	}
	
}

} // namespace
 
