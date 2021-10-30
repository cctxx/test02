using UnityEngine;
using UnityEditor;
using System.Collections;
using UnityEditorInternal;

namespace UnityEditor {

class Toolbar : GUIView {
	static GUIContent[] s_ToolIcons = {
		EditorGUIUtility.IconContent ("MoveTool"),
		EditorGUIUtility.IconContent ("RotateTool"),
		EditorGUIUtility.IconContent ("ScaleTool"),
		EditorGUIUtility.IconContent ("MoveTool On"),
		EditorGUIUtility.IconContent ("RotateTool On"),
		EditorGUIUtility.IconContent ("ScaleTool On") 
	};

	static GUIContent[] s_ViewToolIcons = {
		EditorGUIUtility.IconContent ("ViewToolOrbit"), 
		EditorGUIUtility.IconContent ("ViewToolMove"),
		EditorGUIUtility.IconContent ("ViewToolZoom"),
		EditorGUIUtility.IconContent ("ViewToolOrbit"),
		EditorGUIUtility.IconContent ("ViewToolOrbit On"), 
		EditorGUIUtility.IconContent ("ViewToolMove On"),
		EditorGUIUtility.IconContent ("ViewToolZoom On"),
		EditorGUIUtility.IconContent ("ViewToolOrbit On")
	};

	static GUIContent[] s_PivotIcons = {
		EditorGUIUtility.TextContent ("ToolHandleCenter"), 
		EditorGUIUtility.TextContent ("ToolHandlePivot"),
	};
	static GUIContent[] s_PivotRotation = {
		EditorGUIUtility.TextContent ("ToolHandleLocal"),
		EditorGUIUtility.TextContent ("ToolHandleGlobal")
	};
	
	static GUIContent s_LayerContent = EditorGUIUtility.TextContent ("ToolbarLayers");

	static GUIContent[] s_PlayIcons = {
		EditorGUIUtility.IconContent ("PlayButton"), 
		EditorGUIUtility.IconContent ("PauseButton"), 		
		EditorGUIUtility.IconContent ("StepButton"),
		EditorGUIUtility.IconContent ("PlayButtonProfile"), 
		EditorGUIUtility.IconContent ("PlayButton On"), 
		EditorGUIUtility.IconContent ("PauseButton On"), 		
		EditorGUIUtility.IconContent ("StepButton On"),
		EditorGUIUtility.IconContent ("PlayButtonProfile On"), 
		EditorGUIUtility.IconContent ("PlayButton Anim"), 
		EditorGUIUtility.IconContent ("PauseButton Anim"), 		
		EditorGUIUtility.IconContent ("StepButton Anim"),
		EditorGUIUtility.IconContent ("PlayButtonProfile Anim")
	};
	bool t1,t2,t3;
	
	public void OnEnable () {
		EditorApplication.modifierKeysChanged += Repaint;
		// when undo or redo is done, we need to reset global tools rotation
		Undo.undoRedoPerformed += OnSelectionChange;
		get = this;
	}

	public void OnDisable () {
		EditorApplication.modifierKeysChanged -= Repaint;
		Undo.undoRedoPerformed -= OnSelectionChange;
	}

	// The actual array we display. We build this every frame to make sure it looks correct i.r.t. selection :)
	static GUIContent[] s_ShownToolIcons = { null, null, null, null };
	
	public static Toolbar get = null;

	internal static string lastLoadedLayoutName {
		get
		{
			return string.IsNullOrEmpty (get.m_LastLoadedLayoutName) ? "Layout" : get.m_LastLoadedLayoutName;
		}
		set
		{
			get.m_LastLoadedLayoutName = value;
			get.Repaint ();
		}
	}
	[SerializeField]
	private string m_LastLoadedLayoutName;

	override protected bool OnFocus ()
	{
		return false;
	}

	void OnSelectionChange () {
		Tools.OnSelectionChange();
	}

	void OnGUI () {
		bool isOrWillEnterPlaymode = EditorApplication.isPlayingOrWillChangePlaymode;
		if (isOrWillEnterPlaymode)
			GUI.color = HostView.kPlayModeDarken;

		GUIStyle s = "AppToolbar";
		if (Event.current.type == EventType.Repaint)
			s.Draw (new Rect (0,0,position.width, position.height), false, false, false, false);
		DoToolButtons ();

		float playPauseStopWidth = 100;
		float pos = (position.width - playPauseStopWidth) / 2;
		pos = Mathf.Max (pos, 373);
		
		GUILayout.BeginArea (new Rect (pos, 5, 120, 24));
		GUILayout.BeginHorizontal ();
		
		DoPlayButtons (isOrWillEnterPlaymode);

		GUILayout.EndHorizontal ();
		GUILayout.EndArea ();

		// beta warning
		float warningWidth = 220;
		pos = (position.width - warningWidth);
		pos = Mathf.Max (pos, 440);
		GUILayout.BeginArea (new Rect (pos, 7, position.width - pos- 10, 24));
		GUILayout.BeginHorizontal ();
		
		DoLayersDropDown ();

		GUILayout.Space (6);

		DoLayoutDropDown ();

		GUILayout.EndArea ();
		
		EditorGUI.ShowRepaints ();
		Highlighter.ControlHighlightGUI (this);
	}

	void DoToolButtons ()
	{
		// Handle temporary override with ALT
		GUI.changed = false;

		int displayTool = Tools.viewToolActive ? 0 : (int)Tools.current;

		// Change the icon to match the correct view tool.);
		for (int i = 1; i < 4; i++) {
			s_ShownToolIcons[i] = s_ToolIcons[i == displayTool ? i + 2 : i -1];
			s_ShownToolIcons[i].tooltip = s_ToolIcons[i - 1].tooltip;
		}
		s_ShownToolIcons[0] = s_ViewToolIcons[(int)Tools.viewTool + (displayTool == 0 ? 4 : 0)];

		displayTool = GUI.Toolbar (new Rect (10,5,130,24), displayTool, s_ShownToolIcons, "Command");
		if (GUI.changed)
			Tools.current = (Tool)displayTool;
		
		Tools.pivotMode = (PivotMode)EditorGUI.CycleButton (new Rect (160, 8, 64, 18), (int)Tools.pivotMode, s_PivotIcons, "ButtonLeft");
		if (Tools.current == Tool.Scale)
			GUI.enabled = false;
		PivotRotation tempPivot = (PivotRotation)EditorGUI.CycleButton (new Rect (224, 8, 64, 18), (int)Tools.pivotRotation, s_PivotRotation, "ButtonRight");
		if (Tools.pivotRotation != tempPivot)
		{
			Tools.pivotRotation = tempPivot;
			if (tempPivot == PivotRotation.Global)
				Tools.ResetGlobalHandleRotation ();
		}

		if (Tools.current == Tool.Scale)
			GUI.enabled = true;

		if (GUI.changed)
			Tools.RepaintAllToolViews ();	
	}

	void DoPlayButtons (bool isOrWillEnterPlaymode)
	{
		// Enter / Exit Playmode
		bool isPlaying = EditorApplication.isPlaying;
		GUI.changed = false;
		
		int buttonOffset = isPlaying ? 4 : 0;
		if (AnimationMode.InAnimationMode ())
			buttonOffset = 8;
			
		Color c = GUI.color + new Color (.01f, .01f, .01f, .01f);
		GUI.contentColor = new Color (1.0f / c.r, 1.0f / c.g, 1.0f / c.g, 1.0f / c.a);
		GUILayout.Toggle (isOrWillEnterPlaymode, s_PlayIcons[buttonOffset], "CommandLeft");
		GUI.backgroundColor = Color.white;
		if (GUI.changed) {
			TogglePlaying ();
			GUIUtility.ExitGUI();
		}
		
		// Pause game			
		GUI.changed = false;
		
		bool isPaused = GUILayout.Toggle (EditorApplication.isPaused, s_PlayIcons[buttonOffset + 1], "CommandMid");
		if (GUI.changed) 
		{
			EditorApplication.isPaused = isPaused;
			GUIUtility.ExitGUI();
		}
		
		// Step playmode
		if (GUILayout.Button (s_PlayIcons[buttonOffset + 2], "CommandRight"))
		{
			EditorApplication.Step();
			GUIUtility.ExitGUI();
		}
	}

	void DoLayersDropDown ()
	{
		GUIStyle dropStyle = "DropDown";
		Rect fxRect = GUILayoutUtility.GetRect (s_LayerContent, dropStyle);
		if (EditorGUI.ButtonMouseDown (fxRect, s_LayerContent, FocusType.Passive, dropStyle))
		{
			Rect rect = GUILayoutUtility.topLevel.GetLast ();
			if (LayerVisibilityWindow.ShowAtPosition (rect))
			{
				GUIUtility.ExitGUI ();
			}
		}
	}
	
	void DoLayoutDropDown ()
	{
		// Layout DropDown
		Rect r = GUILayoutUtility.GetRect (s_LayerContent, "DropDown");
		if (EditorGUI.ButtonMouseDown (r, GUIContent.Temp(lastLoadedLayoutName), FocusType.Passive, "DropDown")) {
			Vector2 temp = GUIUtility.GUIToScreenPoint (new Vector2 (r.x, r.y));
			r.x = temp.x;
			r.y = temp.y;
			EditorUtility.Internal_DisplayPopupMenu(r, "Window/Layouts", this, 0);
		}
		GUILayout.EndHorizontal ();
	}

	// Repaints all views, called from C++ when playmode entering is aborted
	// and when the user clicks on the playmode button.
	static void InternalWillTogglePlaymode()
	{
		InternalEditorUtility.RepaintAllViews ();
	}

  	static void TogglePlaying ()
  	{
		bool willPlay = !EditorApplication.isPlaying;
		EditorApplication.isPlaying = willPlay;

        InternalWillTogglePlaymode();
	}

	static internal void RepaintToolbar ()
	{
		if (get != null)
			get.Repaint();
	}
	
	public float CalcHeight () 
	{
		return 30;
	}
}


} // namespace
