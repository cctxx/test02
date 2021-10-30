using System;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using Object = UnityEngine.Object;

namespace UnityEditor {

public class SceneView : SearchableEditorWindow {
	private static SceneView s_LastActiveSceneView;
	private static SceneView s_CurrentDrawingSceneView;
	public static SceneView lastActiveSceneView { get { return s_LastActiveSceneView; } }
	public static SceneView currentDrawingSceneView { get { return s_CurrentDrawingSceneView; } }

	static PrefColor kSceneViewBackground = new PrefColor( "Scene/Background", 0.278431f, 0.278431f, 0.278431f, 0);
	
	static PrefColor kSceneViewWire = new PrefColor ("Scene/Wireframe", 0.0f, 0.0f, 0.0f, 0.5f);
	static PrefColor kSceneViewWireOverlay = new PrefColor ("Scene/Wireframe Overlay", 0.0f, 0.0f, 0.0f, 0.25f);
	static PrefColor kSceneViewWireActive = new PrefColor ("Scene/Wireframe Active", 125.0f/255.0f, 176.0f/255.0f, 250.0f/255.0f, 95.0f/255.0f);
	static PrefColor kSceneViewWireSelected = new PrefColor ("Scene/Wireframe Selected", 94.0f/255.0f, 119.0f/255.0f, 155.0f/255.0f, 0.25f);
	
	internal static Color kSceneViewFrontLight = new Color (0.769f, 0.769f, 0.769f, 1);
	internal static Color kSceneViewUpLight = new Color (0.212f, 0.227f, 0.259f, 1);
	internal static Color kSceneViewMidLight = new Color (0.114f, 0.125f, 0.133f, 1);
	internal static Color kSceneViewDownLight = new Color (0.047f, 0.043f, 0.035f, 1);

	[NonSerialized]
	static readonly Quaternion kDefaultRotation = Quaternion.LookRotation(new Vector3(-1, -.7f, -1));
	[NonSerialized]
	static readonly float kDefaultViewSize = 10f;
	[NonSerialized]
	static readonly Vector3 kDefaultPivot = Vector3.zero;

	const float kOrthoThresholdAngle = 3f;
	const float kOneOverSqrt2 = 0.707106781f;
	
	[System.NonSerialized]
	ActiveEditorTracker m_Tracker;
	
	public bool m_SceneLighting = false;

	public double lastFramingTime = 0;
	private const double k_MaxDoubleKeypressTime = 0.5;

	[Serializable]
	public class SceneViewState
	{
		public bool showFog = true;
		public bool showMaterialUpdate = false;
		public bool showSkybox = true;
		public bool showFlares = true;

		public SceneViewState()
		{
		}

		public SceneViewState(SceneViewState other)
		{
			showFog = other.showFog;
			showMaterialUpdate = other.showMaterialUpdate;
			showSkybox = other.showSkybox;
			showFlares = other.showFlares;
		}

		public bool IsAllOn()
		{
			return showFog && showMaterialUpdate && showSkybox && showFlares;
		}

		public void Toggle(bool value)
		{
			showFog = value;
			showMaterialUpdate = value;
			showSkybox = value;
			showFlares = value;
		}
	}
	
#if ENABLE_SPRITES
	static PrefKey k2DMode = new PrefKey ("Tools/2D Mode", "2");
	private static bool waitingFor2DModeKeyUp;

	[SerializeField]
	private bool m_2DMode = false;
	public bool in2DMode
	{
		get { return m_2DMode; }
		set {
			if (m_2DMode != value)
			{
				m_2DMode = value;
				On2DModeChange ();
			}
		}
	}

	internal Object m_OneClickDragObject;
#endif
	
	[System.NonSerialized]
	public bool m_AudioPlay = false;	
	static SceneView s_AudioSceneView = null;
	
	[SerializeField]
	AnimVector3 m_Position = new AnimVector3 (kDefaultPivot);

	static string[] s_RenderModes = { "Textured", "Wireframe", "Textured Wire", "Render Paths", "Lightmap Resolution" };
	static string[] s_OverlayModes = { "RGB", "Alpha", "Overdraw", "Mipmaps"/*, "Lightmaps"*/ };

	public delegate void OnSceneFunc(SceneView sceneView);
	public static OnSceneFunc onSceneGUIDelegate;

	public DrawCameraMode m_RenderMode = 0;
	public int m_OverlayMode = 0;

	public DrawCameraMode renderMode
	{
		get { return m_RenderMode; }
		set { m_RenderMode = value; }
	}

	[SerializeField] 
	internal SceneViewState m_SceneViewState;

	[SerializeField]
	SceneViewGrid grid;
	[SerializeField]
	internal SceneViewRotation svRot;
	[SerializeField]
	internal AnimQuaternion m_Rotation = new AnimQuaternion (kDefaultRotation);

	/// How large an area the scene view covers (measured diagonally). Modify this for immediate effect, or use LookAt to animate it nicely.
	[SerializeField]
	AnimFloat m_Size = new AnimFloat (kDefaultViewSize);
	[SerializeField]
	internal AnimBool m_Ortho = new AnimBool ();
	
	[System.NonSerialized]
	bool slomo = false;

	[System.NonSerialized]
	Camera m_Camera;

#if ENABLE_SPRITES
	[SerializeField]
	private Quaternion m_LastSceneViewRotation;
	public Quaternion lastSceneViewRotation
	{
		get
		{
			if (m_LastSceneViewRotation == new Quaternion (0f, 0f, 0f, 0f))
				m_LastSceneViewRotation = Quaternion.identity;
			return m_LastSceneViewRotation;
		}
		set { m_LastSceneViewRotation = value; }
	}
	[SerializeField]
	private bool m_LastSceneViewOrtho;
#endif

	// Cursor rect handling
	private struct CursorRect
	{
		public Rect rect;
		public MouseCursor cursor;
		public CursorRect (Rect rect, MouseCursor cursor)
		{
			this.rect = rect;
			this.cursor = cursor;
		}
	}
	private static MouseCursor s_LastCursor = MouseCursor.Arrow;
	private static List<CursorRect> s_MouseRects = new List<CursorRect> ();
	private bool s_DraggingCursorIsCashed = false;
	internal static void AddCursorRect (Rect rect, MouseCursor cursor)
	{
		if (Event.current.type == EventType.Repaint)
			s_MouseRects.Add (new CursorRect (rect, cursor));
	}

	internal float cameraDistance
	{
		get
		{
			float fov = m_Ortho.Fade(kPerspectiveFov, 0);
		
			if (!camera.orthographic)
			{
				return size / Mathf.Tan (fov * 0.5f * Mathf.Deg2Rad);
			}
			else
			{
				return size * 2f;
			}
		}
	}

	[System.NonSerialized]
	Light[] m_Light = new Light[3];
	
	RectSelection m_RectSelection;
	
	const float kPerspectiveFov = 90;
	public const float kToolbarHeight = 17;

	[System.NonSerialized]
	internal AnimValueManager m_AnimValueManager;

	static ArrayList s_SceneViews = new ArrayList();
	public static ArrayList sceneViews { get {return s_SceneViews; }}

	static Material s_AlphaOverlayMaterial;
	static Shader s_ShowOverdrawShader;
	static Shader s_ShowMipsShader;
	static Shader s_ShowLightmapsShader;
	static Shader s_AuraShader;
	static Shader s_GrayScaleShader;
	static Texture2D s_MipColorsTexture;

	// Handle Dragging of stuff over scene view
	//static ArrayList s_DraggedEditors = null;
	//static GameObject[] s_PickedObject = { null };
	static GUIContent s_Fx = new GUIContent("Effects");
	static GUIContent s_Lighting = EditorGUIUtility.IconContent ("SceneviewLighting");
	static GUIContent s_AudioPlay = EditorGUIUtility.IconContent ("SceneviewAudio");
	static GUIContent s_GizmosContent = new GUIContent("Gizmos");
	static GUIContent s_2DMode = new GUIContent("2D");
	
/*	static GUIContent[] s_RenderModes = {
		EditorGUIUtility.TextContent ("SceneTextured"), 
		EditorGUIUtility.TextContent ("SceneWireframe"), 		
		EditorGUIUtility.TextContent ("SceneTexWire"),
	};
*/

	// Which tool are we currently editing with.
	// This gets updated whenever hotControl == 0, so once the user has started sth, they can't change it mid-drag by e.g. pressing alt
	static Tool s_CurrentTool;
	
	double m_StartSearchFilterTime = -1;
	RenderTexture m_SearchFilterTexture;
	int m_MainViewControlID;
		
	public Camera camera { get { return m_Camera; } }

	[SerializeField]
	private Shader m_ReplacementShader;
	[SerializeField]
	private string m_ReplacementString;

	public void SetSceneViewShaderReplace (Shader shader, string replaceString)
	{
		m_ReplacementShader = shader;
		m_ReplacementString = replaceString;
	}

	internal bool m_ShowSceneViewWindows = false;
	SceneViewOverlay m_SceneViewOverlay;
	EditorCache m_DragEditorCache;

	// While Locking the view to object, we have different behaviour for different scenarios:
	// Smooth camera behaviour: User dragging the handles
	// Instant camera behaviour: Position changed externally (via inspector, physics or scripts etc.)
	internal enum DraggingLockedState
	{
		NotDragging, // Default state. Scene view camera is snapped to selected object instantly
		Dragging, // User is dragging from handles. Scene view camera holds still.
		LookAt // Temporary state after dragging or selection change, where we return scene view camera smoothly to selected object
	}
	DraggingLockedState m_DraggingLockedState;
	internal DraggingLockedState draggingLocked { set { m_DraggingLockedState = value; } }

	[SerializeField]
	private Object m_LastLockedObject;

	[SerializeField]
	bool m_ViewIsLockedToObject;
	internal bool viewIsLockedToObject { 
		get { return m_ViewIsLockedToObject; } 
		set
		{
			if (value)
				m_LastLockedObject = Selection.activeObject;
			else
				m_LastLockedObject = null;

			m_ViewIsLockedToObject = value;
			draggingLocked = DraggingLockedState.LookAt;
		} 
	}

	public static bool FrameLastActiveSceneView()
	{
		if (lastActiveSceneView == null)
			return false;
		return lastActiveSceneView.SendEvent(EditorGUIUtility.CommandEvent("FrameSelected"));
	}

	public static bool FrameLastActiveSceneViewWithLock ()
	{
		if (lastActiveSceneView == null)
			return false;
		return lastActiveSceneView.SendEvent (EditorGUIUtility.CommandEvent ("FrameSelectedWithLock"));
	}

	Editor[] GetActiveEditors ()
	{
		if (m_Tracker == null)
			m_Tracker = ActiveEditorTracker.sharedTracker;
		return m_Tracker.activeEditors;
	}
	
	public static Camera[] GetAllSceneCameras ()
	{
		ArrayList array = new ArrayList();
		for (int i = 0; i < s_SceneViews.Count; ++i)
		{
			Camera cam = ((SceneView)s_SceneViews[i]).m_Camera;
			if (cam != null)
				array.Add(cam);
		}
		return (Camera[])array.ToArray(typeof(Camera));
	}

	public static void RepaintAll ()
	{
		foreach (SceneView sv in s_SceneViews)
		{
			sv.Repaint();
		}
	}

	internal override void SetSearchFilter (string searchFilter, SearchMode searchMode, bool setAll)
	{
		if (m_SearchFilter == "" || searchFilter == "")
			m_StartSearchFilterTime = EditorApplication.timeSinceStartup;
		
		base.SetSearchFilter(searchFilter, searchMode, setAll);
	}
	
	void OnFocus ()
	{
		if (!Application.isPlaying && m_AudioPlay)
			ToggleAudio();
	}
	
	void OnLostFocus ()
	{
		// don't bleed our scene view rendering into game view
		GameView gameView = (GameView)WindowLayout.FindEditorWindowOfType(typeof(GameView));
		if (gameView && gameView.m_Parent != null && m_Parent != null && gameView.m_Parent == m_Parent)
		{
	    	gameView.m_Parent.backgroundValid = false; 
		}
	}

	override public void OnEnable ()
	{
		m_RectSelection = new RectSelection (this);
		m_AnimValueManager = new AnimValueManager ();
		if (grid == null)
			grid = new SceneViewGrid();
		grid.Register(this);
		if (svRot == null)
			svRot = new SceneViewRotation ();
		svRot.Register(this);
				
		autoRepaintOnSceneChange = true;
		m_AnimValueManager.Add (m_Rotation);
		m_AnimValueManager.Add (m_Position);
		m_AnimValueManager.Add (m_Size);
		m_AnimValueManager.Add (m_Ortho);
		m_AnimValueManager.callback = Repaint;
		wantsMouseMove = true;
		dontClearBackground = true;
		s_SceneViews.Add (this);
			
		m_SceneViewOverlay = new SceneViewOverlay(this);

		EditorApplication.modifierKeysChanged += SceneView.RepaintAll; // Because we show handles on shift
		m_DraggingLockedState = DraggingLockedState.NotDragging;
		base.OnEnable();
	}
	
	public SceneView ()
	{
		m_HierarchyType = HierarchyType.GameObjects;
		depthBufferBits = 32;
		antiAlias = -1;
	}
	
	void Awake ()
	{
		if (m_SceneViewState == null)
			m_SceneViewState = new SceneViewState();

		if (!BuildPipeline.isBuildingPlayer)
			m_SceneLighting = InternalEditorUtility.CalculateShouldEnableLights ();

		if (EditorSettings.defaultBehaviorMode == EditorBehaviorMode.Mode2D)
		{
			m_2DMode = true;
			m_LastSceneViewRotation = Quaternion.LookRotation (new Vector3 (-1, -.7f, -1));
			m_LastSceneViewOrtho = false;
			m_Rotation.value = Quaternion.identity;
			m_Ortho.value = true;
		}
	}
	
	static void OnForceEnableLights ()
	{
		foreach (SceneView sv in s_SceneViews)
		{
			sv.m_SceneLighting = true;
		}
	}

	void OnDidOpenScene ()
	{
		if (BuildPipeline.isBuildingPlayer)
			return;

		foreach (SceneView sv in s_SceneViews)
			sv.m_SceneLighting = InternalEditorUtility.CalculateShouldEnableLights();
	}
	
	internal static void PlaceGameObjectInFrontOfSceneView (GameObject go)
	{
		if (s_SceneViews.Count >= 1)
		{
			SceneView view = s_LastActiveSceneView;
			if (!view)
				view = s_SceneViews[0] as SceneView;
			if (view)
			{
				view.MoveToView(go.transform);
			}
		}
	}
	
	override public void OnDisable () 
	{
		EditorApplication.modifierKeysChanged -= SceneView.RepaintAll;

		if (m_Camera)
			DestroyImmediate (m_Camera.gameObject, true);
		if (m_Light[0])
			DestroyImmediate (m_Light[0].gameObject, true);
		if (m_Light[1])
			DestroyImmediate (m_Light[1].gameObject, true);
		if (m_Light[2])
			DestroyImmediate (m_Light[2].gameObject, true);
		if (s_MipColorsTexture)
			DestroyImmediate (s_MipColorsTexture, true);
		s_SceneViews.Remove (this);
		if (s_LastActiveSceneView == this) 
		{
			if (s_SceneViews.Count > 0)
				s_LastActiveSceneView = s_SceneViews[0] as SceneView;
			else
				s_LastActiveSceneView = null;
		}
		
		CleanupEditorDragFunctions();

		base.OnDisable();
	}

	public void OnDestroy() 
	{
		if (m_AudioPlay)
		{
			m_AudioPlay = false;
			ToggleAudio();
		}	
	}

	private static GUIStyle s_DropDownStyle;
	private GUIStyle effectsDropDownStyle
	{
		get
		{
			if (s_DropDownStyle == null)
				s_DropDownStyle = "GV Gizmo DropDown";
			return s_DropDownStyle;
		}
	}

	void DoStatusBarGUI () 
	{
		GUILayout.BeginHorizontal ("toolbar"); 
		{
			m_RenderMode = (DrawCameraMode)EditorGUILayout.Popup ((int)m_RenderMode, s_RenderModes, "ToolbarPopup", GUILayout.Width (120));
			EditorGUILayout.Space ();
			bool guiEnabled = GUI.enabled;
			GUI.enabled = string.IsNullOrEmpty(m_SearchFilter);
			m_OverlayMode = EditorGUILayout.Popup (m_OverlayMode, s_OverlayModes, "ToolbarPopup", GUILayout.Width (70));
			GUI.enabled = guiEnabled;
				
			EditorGUILayout.Space ();

			in2DMode = GUILayout.Toggle (in2DMode, s_2DMode, "toolbarbutton");
			
			m_SceneLighting = GUILayout.Toggle (m_SceneLighting, s_Lighting, "toolbarbutton");
			
			GUI.enabled = !Application.isPlaying;
			GUI.changed = false;
			m_AudioPlay = GUILayout.Toggle (m_AudioPlay, s_AudioPlay, EditorStyles.toolbarButton);
			if (GUI.changed)
				ToggleAudio ();
			
			GUI.enabled = true;

			Rect fxRect = GUILayoutUtility.GetRect(s_Fx, effectsDropDownStyle);
			Rect fxRightRect = new Rect(fxRect.xMax - effectsDropDownStyle.border.right, fxRect.y, effectsDropDownStyle.border.right, fxRect.height);
			if (EditorGUI.ButtonMouseDown(fxRightRect, GUIContent.none, FocusType.Passive, GUIStyle.none))
			{
				Rect rect = GUILayoutUtility.topLevel.GetLast();
				if (SceneFXWindow.ShowAtPosition(rect, this))
				{
					GUIUtility.ExitGUI();
				}
			}

			var allOn = GUI.Toggle(fxRect, m_SceneViewState.IsAllOn(), s_Fx, effectsDropDownStyle);
			if (allOn != m_SceneViewState.IsAllOn())
				m_SceneViewState.Toggle (allOn);

			GUILayout.Space (6);

			
			GUILayout.FlexibleSpace();

			if (m_MainViewControlID != GUIUtility.keyboardControl 
				&& Event.current.type == EventType.KeyDown
				&& !string.IsNullOrEmpty(m_SearchFilter) 
			)		
			{
				switch (Event.current.keyCode)
				{
					case KeyCode.UpArrow:
					case KeyCode.DownArrow:
						if (Event.current.keyCode == KeyCode.UpArrow)
							SelectPreviousSearchResult();	
						else
							SelectNextSearchResult();	
						
						FrameSelected (false);
						Event.current.Use();
						GUIUtility.ExitGUI();
						return;
				}
			}
			
			Rect r = GUILayoutUtility.GetRect(s_GizmosContent, EditorStyles.toolbarDropDown);
			if (EditorGUI.ButtonMouseDown(r, s_GizmosContent, FocusType.Passive, EditorStyles.toolbarDropDown)) 
			{
				Rect rect = GUILayoutUtility.topLevel.GetLast();
				if (AnnotationWindow.ShowAtPosition(rect, false))
				{
					GUIUtility.ExitGUI ();
				}
			}
			GUILayout.Space(6);
			
			SearchFieldGUI();
			
			// For Debug purposes only
			//slomo = GUILayout.Toggle(slomo, "Slomo");
			m_AnimValueManager.speed = slomo ? .2f : 2;
			//GUILayout.Label ("hot: " + GUIUtility.hotControl);
			//autoRepaintOnSceneChange = GUILayout.Toggle (autoRepaintOnSceneChange, "AutoRepaint");;

		}
		GUILayout.EndHorizontal ();
	}
	
	void ToggleAudio ()
	{
		if ((s_AudioSceneView != null) && (s_AudioSceneView != this))
		{
			// turn *other* sceneview off
			if (s_AudioSceneView.m_AudioPlay)
			{
				s_AudioSceneView.m_AudioPlay = false;	
				s_AudioSceneView.Repaint();
			}
		}
		
		AudioSource[] sources = (AudioSource[])FindObjectsOfType(typeof(AudioSource));
		foreach(AudioSource source in sources)
		{
			if (source.playOnAwake)
			{
				if (!m_AudioPlay)
				{
					source.Stop();
				}
				else 
				{
					if (!source.isPlaying)
						source.Play();
				}
			}
		}
		AudioUtil.SetListenerTransform(m_AudioPlay?m_Camera.transform:null);
		
		s_AudioSceneView = this;		
	}	

	/// TODO: Don't repaint sceneview unless either old or new selection is a scene object
	public void OnSelectionChange () 
	{
		if (Selection.activeObject != null && m_LastLockedObject != Selection.activeObject)
		{
			viewIsLockedToObject = false;
		}
		Repaint ();
	}


    [MenuItem ("GameObject/Move To View %&f")]
    static void MenuMoveToView ()
    {
        if (ValidateMoveToView())
            s_LastActiveSceneView.MoveToView ();
    }
    [MenuItem ("GameObject/Move To View %f", true)]
    static bool ValidateMoveToView ()
    {
        return s_LastActiveSceneView != null && (Selection.transforms.Length != 0);
    }
    [MenuItem ("GameObject/Align With View %#f")]
    static void MenuAlignWithView ()
    {
        if (ValidateAlignWithView ())
            s_LastActiveSceneView.AlignWithView ();
    }
    [MenuItem ("GameObject/Align With View %#f", true)]
    static bool ValidateAlignWithView ()
    {
        return s_LastActiveSceneView != null && (Selection.activeTransform != null);
    }
    [MenuItem ("GameObject/Align View to Selected")]
    static void MenuAlignViewToSelected ()
    {
        if (ValidateAlignViewToSelected ())
            s_LastActiveSceneView.AlignViewToObject (Selection.activeTransform);
    }
    [MenuItem ("GameObject/Align View to Selected", true)]
    static bool ValidateAlignViewToSelected ()
    {
        return s_LastActiveSceneView != null && (Selection.activeTransform != null);
    }

    static private void CreateMipColorsTexture () {
        if (s_MipColorsTexture)
            return;
        s_MipColorsTexture = new Texture2D (32, 32, TextureFormat.ARGB32, true);
        s_MipColorsTexture.hideFlags = HideFlags.HideAndDontSave;
        Color[] colors = new Color[6];
        colors[0] = new Color (0.0f, 0.0f, 1.0f, 0.8f);
        colors[1] = new Color (0.0f, 0.5f, 1.0f, 0.4f);
        colors[2] = new Color (1.0f, 1.0f, 1.0f, 0.0f); // optimal level
        colors[3] = new Color (1.0f, 0.7f, 0.0f, 0.2f);
        colors[4] = new Color (1.0f, 0.3f, 0.0f, 0.6f);
        colors[5] = new Color (1.0f, 0.0f, 0.0f, 0.8f);
        int mipCount = Mathf.Min (6, s_MipColorsTexture.mipmapCount);
        for (int mip = 0; mip < mipCount; ++mip) {
            int width = Mathf.Max (s_MipColorsTexture.width >> mip, 1);
            int height = Mathf.Max (s_MipColorsTexture.height >> mip, 1);
            Color[] cols = new Color[width * height];
            for (int i = 0; i < cols.Length; ++i)
                cols[i] = colors[mip];
            s_MipColorsTexture.SetPixels (cols, mip);
        }
        s_MipColorsTexture.filterMode = FilterMode.Trilinear;
        s_MipColorsTexture.Apply (false);
        Shader.SetGlobalTexture ("__SceneViewMipcolorsTexture", s_MipColorsTexture);
    }

	private bool m_RequestedSceneViewFiltering;
	private double m_lastRenderedTime;

	public void SetSceneViewFiltering(bool enable)
	{
		m_RequestedSceneViewFiltering = enable;
	}

	private bool UseSceneFiltering()
	{
		return !string.IsNullOrEmpty(m_SearchFilter) || m_RequestedSceneViewFiltering;
	}

	void OnGUI ()
	{
		if (Event.current.type == EventType.Repaint)
			s_MouseRects.Clear ();
		
		Color origColor = GUI.color;
		
		if (Event.current.type == EventType.MouseDown || Event.current.type == EventType.MouseDrag)
			s_LastActiveSceneView = this;
		else if (s_LastActiveSceneView == null)
			s_LastActiveSceneView = this;

		if (Event.current.type == EventType.MouseDrag)
			draggingLocked = DraggingLockedState.Dragging;
		else if (Event.current.type == EventType.MouseUp)
			draggingLocked = DraggingLockedState.LookAt;

		if (Event.current.type == EventType.mouseDown)
		{
			Tools.s_ButtonDown = Event.current.button;
			if (Event.current.button == 1 && Application.platform == RuntimePlatform.OSXEditor)
				Focus ();
		}
	
		if(Event.current.type == EventType.Layout)
			m_ShowSceneViewWindows = (SceneView.lastActiveSceneView == this);
		
		m_SceneViewOverlay.Begin();	
			
		bool oldFog = RenderSettings.fog;
		float oldShadowDistance = QualitySettings.shadowDistance;
		if (Event.current.type == EventType.Repaint)
		{		
			if (!m_SceneViewState.showFog)
				Unsupported.SetRenderSettingsUseFogNoDirty (false);
			if (m_Camera.isOrthoGraphic)
				Unsupported.SetQualitySettingsShadowDistanceTemporarily (QualitySettings.shadowDistance + 0.5f*cameraDistance);
		}

		DoStatusBarGUI ();
		GUI.skin = EditorGUIUtility.GetBuiltinSkin (EditorSkin.Scene);
		EditorGUIUtility.labelWidth = 100;
		
		SetupCamera ();
		RenderingPath oldRenderingPath = m_Camera.renderingPath;
		
		if (!m_SceneLighting)
		{
			m_Light[0].transform.rotation = m_Camera.transform.rotation;
			if (Event.current.type == EventType.Repaint)
				InternalEditorUtility.SetCustomLighting (m_Light, kSceneViewMidLight);
		}
		
		GUI.BeginGroup (new Rect (0, kToolbarHeight, position.width, position.height - kToolbarHeight));
		Rect cameraRect = new Rect (0, 0, position.width, position.height - kToolbarHeight);
		
		if (Tools.viewToolActive && Event.current.type == EventType.Repaint)
		{
			MouseCursor cursor = MouseCursor.Arrow;
				switch (Tools.viewTool)
				{
				case (ViewTool.Pan): cursor = MouseCursor.Pan; break;
				case (ViewTool.Orbit): cursor = MouseCursor.Orbit; break;
				case (ViewTool.FPS): cursor = MouseCursor.FPS; break;
				case (ViewTool.Zoom): cursor = MouseCursor.Zoom; break;
			}
			if (cursor != MouseCursor.Arrow)
				AddCursorRect (new Rect (0, kToolbarHeight, position.width, position.height - kToolbarHeight), cursor);
		}
		
		// When search is enabled, render camera into a render texture, so we can Apply grayscale effect.
		if (UseSceneFiltering ())
		{
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (true);
			if (m_SearchFilterTexture == null)
			{
				m_SearchFilterTexture = new RenderTexture (0, 0, 24);
				m_SearchFilterTexture.hideFlags = HideFlags.HideAndDontSave;
			}

			Rect actualCameraRect = Handles.GetCameraRect (cameraRect);

			if (m_SearchFilterTexture.width != (int)actualCameraRect.width || m_SearchFilterTexture.height != (int)actualCameraRect.height)
			{
				m_SearchFilterTexture.Release ();
				m_SearchFilterTexture.width = (int)actualCameraRect.width;
				m_SearchFilterTexture.height = (int)actualCameraRect.height;
			}
			m_Camera.targetTexture = m_SearchFilterTexture;
			if (m_Camera.actualRenderingPath == RenderingPath.DeferredLighting)
				m_Camera.renderingPath = RenderingPath.Forward;
		}
		else
			m_Camera.targetTexture = null;
		// Clear (color/skybox)
		// We do funky FOV interpolation when switching between ortho and perspective. However,
		// for the skybox we always want to use the same FOV.
		float skyboxFOV = GetVerticalFOV (kPerspectiveFov);
		float realFOV = m_Camera.fieldOfView;
		m_Camera.fieldOfView = skyboxFOV;
		Handles.ClearCamera (cameraRect, m_Camera);
		m_Camera.fieldOfView = realFOV;

		m_Camera.cullingMask = Tools.visibleLayers;
		
		// Give editors a chance to kick in. Disable in search mode, editors calling Handles.BeginGUI () will 
		// break the camera setup.
		if (!UseSceneFiltering())
		{
			Handles.SetCamera (cameraRect, m_Camera);
			CallOnPreSceneGUI ();
		}
		
		if (Event.current.type == EventType.Repaint)
		{
			// Set scene view colors
			Handles.SetSceneViewColors (kSceneViewWire, kSceneViewWireOverlay, kSceneViewWireActive, kSceneViewWireSelected);

			// Setup shader replacement if needed by overlay mode
			if (m_OverlayMode == 2)
			{
				// show overdraw
				if (!s_ShowOverdrawShader)
					s_ShowOverdrawShader = EditorGUIUtility.LoadRequired ("SceneView/SceneViewShowOverdraw.shader") as Shader;
				m_Camera.SetReplacementShader (s_ShowOverdrawShader, "RenderType");
			}
			else if (m_OverlayMode == 3)
			{
				// show mip levels
				if (!s_ShowMipsShader)
					s_ShowMipsShader = EditorGUIUtility.LoadRequired ("SceneView/SceneViewShowMips.shader") as Shader;
				if (s_ShowMipsShader.isSupported)
				{
					CreateMipColorsTexture ();
					m_Camera.SetReplacementShader (s_ShowMipsShader, "RenderType");
				}
				else
				{
					m_Camera.SetReplacementShader (m_ReplacementShader, m_ReplacementString);
				}
			}
			else if (m_OverlayMode == 4)
			{
				// show mip levels
				if (!s_ShowLightmapsShader)
					s_ShowLightmapsShader = EditorGUIUtility.LoadRequired ("SceneView/SceneViewShowLightmap.shader") as Shader;
				if (s_ShowLightmapsShader.isSupported)
				{
					m_Camera.SetReplacementShader (s_ShowLightmapsShader, "RenderType");
				}
				else
				{
					m_Camera.SetReplacementShader (m_ReplacementShader, m_ReplacementString);
				}
			}
			else
			{
				m_Camera.SetReplacementShader (m_ReplacementShader, m_ReplacementString);
			}
		}

		// Unfocus search field on mouse clicks into content, so that key presses work to navigate.
		m_MainViewControlID = EditorGUIUtility.GetControlID (FocusType.Keyboard);
		if (Event.current.GetTypeForControl (m_MainViewControlID) == EventType.MouseDown)
			GUIUtility.keyboardControl = m_MainViewControlID;

		// Draw camera
		if (m_Camera.gameObject.activeInHierarchy)
		{
			DrawGridParameters gridParam = grid.PrepareGridRender(camera, pivot, m_Rotation.target, m_Size, m_Ortho.target, AnnotationUtility.showGrid);

			if (UseSceneFiltering())
			{
				if (Event.current.type == EventType.Repaint)
				{
					// First pass: Draw objects which do not meet the search filter with grayscale image effect.
					Handles.EnableCameraFx (m_Camera, true);
					
					Handles.SetCameraFilterMode (m_Camera, Handles.FilterMode.ShowRest);
					
					float fade = Mathf.Clamp01 ((float)(EditorApplication.timeSinceStartup - m_StartSearchFilterTime));
					Handles.DrawCamera (cameraRect, m_Camera, m_RenderMode);
					Handles.DrawCameraFade (m_Camera, fade);
					RenderTexture.active = null;
					
					// Second pass: Draw aura for objects which do meet search filter, but are occluded.
					Handles.EnableCameraFx (m_Camera, false);
					Handles.SetCameraFilterMode (m_Camera, Handles.FilterMode.ShowFiltered);
					if (!s_AuraShader)
						s_AuraShader = EditorGUIUtility.LoadRequired ("SceneView/SceneViewAura.shader") as Shader;
					m_Camera.SetReplacementShader (s_AuraShader, "");
					Handles.DrawCamera (cameraRect, m_Camera, m_RenderMode);
					
					// Third pass: Draw objects which do meet filter normally
					m_Camera.SetReplacementShader(m_ReplacementShader, m_ReplacementString);
					Handles.DrawCamera(cameraRect, m_Camera, m_RenderMode, gridParam);

					if (fade < 1)
						Repaint ();
				}
				Rect r = cameraRect;
				GUI.EndGroup ();
				GUI.BeginGroup (new Rect (0, kToolbarHeight, position.width, position.height - kToolbarHeight));
				GUI.DrawTexture (r, m_SearchFilterTexture, ScaleMode.StretchToFill, false, 0);
				Handles.SetCamera (cameraRect, m_Camera);

				HandleSelectionAndOnSceneGUI();
			}
			else
			{
				// If the camera is rendering into a Render Texture we need to reset the offsets of the GUIClip stack
				// otherwise all GUI drawing after here will get offset incorrectly.
				if (HandleUtility.CameraNeedsToRenderIntoRT (camera))
					GUIClip.Push (new Rect (0f, 0f, position.width, position.height), Vector2.zero, Vector2.zero, true);

				Handles.DrawCameraStep1(cameraRect, m_Camera, m_RenderMode, gridParam);
				DrawAlphaOverlay ();
			}
		}

		if (!m_SceneLighting)
		{
			if (Event.current.type == EventType.Repaint)
				InternalEditorUtility.RemoveCustomLighting ();
		}

		// Give editors a chance to kick in. Disable in search mode, editors rendering to the scene
		// view won't be able to properly render to the rendertexture as needed.
		if (!UseSceneFiltering())
		{
			HandleSelectionAndOnSceneGUI();
		}
		else
		{
			EditorUtility.SetTemporarilyAllowIndieRenderTexture (false);
		}
		
		// Handle commands
		if (Event.current.type == EventType.ExecuteCommand || Event.current.type == EventType.ValidateCommand)
			CommandsGUI ();
		
		if (Event.current.type == EventType.Repaint)
		{
			Unsupported.SetRenderSettingsUseFogNoDirty (oldFog);
			Unsupported.SetQualitySettingsShadowDistanceTemporarily (oldShadowDistance);
		}
			
		m_Camera.renderingPath = oldRenderingPath;

		// blit drawing to screen in deferred mode
		if (!UseSceneFiltering())
			Handles.DrawCameraStep2(m_Camera, m_RenderMode);

		if (UseSceneFiltering())
			Handles.SetCameraFilterMode(Camera.current, Handles.FilterMode.ShowFiltered);
		else
			Handles.SetCameraFilterMode(Camera.current, Handles.FilterMode.Off);

		// Let scene default do it
		DefaultHandles();

		if (!UseSceneFiltering())
			CallOnSceneGUIAfterGizmos ();

		// If we reset the offsets pop that clip off now.
		if (HandleUtility.CameraNeedsToRenderIntoRT (camera) && !UseSceneFiltering ())
			GUIClip.Pop ();

		Handles.SetCameraFilterMode (Camera.current, Handles.FilterMode.Off);
		Handles.SetCameraFilterMode (m_Camera, Handles.FilterMode.Off);

		// Handle Dragging of stuff over scene view
		HandleDragging();

		// SceneViewMotion.DoViewTool will eat any clicks, so give svRot a chance to catch them
		// (Right clicks and middle clicks always, left clicks when hand tool is selected.)
		svRot.HandleContextClick(this);
		svRot.OnGUI(this);

		// Handle scene view motion
		SceneViewMotion.ArrowKeys(this);
		SceneViewMotion.DoViewTool(camera.transform, this);


		if (k2DMode.activated && !waitingFor2DModeKeyUp)
		{
			waitingFor2DModeKeyUp = true;
			in2DMode = !in2DMode;
			Event.current.Use ();
		}
		else
		{
			if (Event.current.type == EventType.KeyUp && Event.current.keyCode == k2DMode.KeyboardEvent.keyCode)
				waitingFor2DModeKeyUp = false;
		}

		GUI.EndGroup ();
		GUI.color = origColor;
			
		m_SceneViewOverlay.End();
		
		if (EditorGUIUtility.hotControl == 0)
			s_DraggingCursorIsCashed = false;
		Rect cursorRect = new Rect (0, 0, position.width, position.height);
		if (!s_DraggingCursorIsCashed)
		{
			// Determine if mouse is inside a new cursor rect
			MouseCursor cursor = MouseCursor.Arrow;
			if (Event.current.type == EventType.MouseMove || Event.current.type == EventType.Repaint)
			{
				foreach (CursorRect r in s_MouseRects)
				{
					if (r.rect.Contains (Event.current.mousePosition))
					{
						cursor = r.cursor;
						cursorRect = r.rect;
					}
				}
				if (EditorGUIUtility.hotControl != 0)
					s_DraggingCursorIsCashed = true;
				if (cursor != s_LastCursor)
				{
					s_LastCursor = cursor;
					InternalEditorUtility.ResetCursor ();
					Repaint ();
				}
			}
		}
		// Apply the one relevant cursor rect
		if (Event.current.type == EventType.Repaint && s_LastCursor != MouseCursor.Arrow)
		{
			EditorGUIUtility.AddCursorRect (cursorRect, s_LastCursor);
			// GUI.color = Color.magenta; GUI.Box (rect, ""); EditorGUI.DropShadowLabel (rect, ""+s_LastCursor); GUI.color = Color.white;
		}
	}

	void DrawAlphaOverlay ()
	{
		if (m_OverlayMode != 1)
			return;

		if (!s_AlphaOverlayMaterial) 
			s_AlphaOverlayMaterial = EditorGUIUtility.LoadRequired ("SceneView/SceneViewAlphaMaterial.mat") as Material;
			
		Handles.BeginGUI ();
		if (Event.current.type == EventType.Repaint)
			Graphics.DrawTexture (new Rect (0,0,position.width,position.height), EditorGUIUtility.whiteTexture, s_AlphaOverlayMaterial);
		Handles.EndGUI ();
	}

	private void HandleSelectionAndOnSceneGUI()
	{
		m_RectSelection.OnGUI();
		CallOnSceneGUI();
	}

	/// Center point of the scene view. Modify it to move the sceneview immediately, or use LookAt to animate it nicely.
	public Vector3 pivot { get { return m_Position; } set { m_Position.value = value;  } }

	/// The direction of the scene view.
	public Quaternion rotation { get { return m_Rotation; } set { m_Rotation.value = value; } }	

	public float size { 
		get { return m_Size; } 
		set { 
			if (value > 40000f) 
				value = 40000;
			m_Size.value = value; 
		} 
	}

	
	/// Is the scene view orthographic. 
	public bool orthographic { get { return m_Ortho.value; } set {m_Ortho.value = value; } }
	
	public void FixNegativeSize () {
		float fov = kPerspectiveFov;
		if (size < 0) {
			float distance = size / Mathf.Tan (fov * 0.5f * Mathf.Deg2Rad);
			Vector3 p = m_Position + rotation * new Vector3 (0,0,-distance);
			size = -size;
			distance = size / Mathf.Tan (fov * 0.5f * Mathf.Deg2Rad);
			m_Position.value = p + rotation * new Vector3 (0,0,distance);
		}
	}
	
	float CalcCameraDist () {
		float fov = m_Ortho.Fade (kPerspectiveFov, 0);
		if (fov > kOrthoThresholdAngle) {
			m_Camera.orthographic = false;
			return size / Mathf.Tan (fov * 0.5f * Mathf.Deg2Rad);
		}
		return 0;
	}
	
	void ResetIfNaN ()
	{
		// If you zoom out enough, m_Position would get corrupted with no way to reset it,
		// even after restarting Unity. Crude hack to at least get the scene view working again!
		if (System.Single.IsInfinity (m_Position.value.x) || System.Single.IsNaN (m_Position.value.x))
			m_Position.value = Vector3.zero;
		if (System.Single.IsInfinity (m_Rotation.value.x) || System.Single.IsNaN (m_Rotation.value.x))
			m_Rotation.value = Quaternion.identity;
	}
	
	static internal RenderingPath GetSceneViewRenderingPath ()
	{
		// take path from main camera
		Camera mainCamera = Camera.main;
		if (mainCamera != null)
			return mainCamera.renderingPath;

		// take path from a camera if we have just one
		Camera[] allCameras = Camera.allCameras;
		if (allCameras != null && allCameras.Length == 1)
			return allCameras[0].renderingPath;

		// otherwise, player settings
		return RenderingPath.UsePlayerSettings;
	}
	
	void SetupCamera () {
		if (!m_Camera)
			Setup ();

		if (m_OverlayMode == 2) {
			// overdraw
			m_Camera.backgroundColor = Color.black;
		} else {
			m_Camera.backgroundColor = kSceneViewBackground;
		}

		EditorUtility.SetCameraAnimateMaterials(m_Camera, m_SceneViewState.showMaterialUpdate);

		ResetIfNaN ();
		
		m_Camera.transform.rotation = m_Rotation;

		float fov = m_Ortho.Fade(kPerspectiveFov, 0);
		if (fov > kOrthoThresholdAngle)
		{
			m_Camera.orthographic = false;
			
			// Old calculations were strange and were more zoomed in for tall aspect ratios than for wide ones.
			//m_Camera.fieldOfView = Mathf.Sqrt((fov * fov) / (1 + aspect));
			// 1:1: Sqrt((90*90) / (1+1))   = 63.63 degrees = atan(0.6204)  -  means we have 0.6204 x 0.6204 in tangents
			// 2:1: Sqrt((90*90) / (1+2))   = 51.96 degrees = atan(0.4873)  -  means we have 0.9746 x 0.4873 in tangents
			// 1:2: Sqrt((90*90) / (1+0.5)) = 73.48 degrees = atan(0.7465)  -  means we have 0.3732 x 0.7465 in tangents - 25% more zoomed in!
			
			m_Camera.fieldOfView = GetVerticalFOV (fov);
			
		}
		else
		{
			m_Camera.orthographic = true;
			
			//m_Camera.orthographicSize = Mathf.Sqrt((size * size) / (1 + aspect));
			m_Camera.orthographicSize = GetVerticalOrthoSize ();
		}
		m_Camera.transform.position = m_Position + m_Camera.transform.rotation * new Vector3(0, 0, -cameraDistance);

		if (size < 1)
		{
			m_Camera.nearClipPlane = .005f;
			m_Camera.farClipPlane = 1000;
		}
		else if (size < 100)
		{
			m_Camera.nearClipPlane = .03f;
			m_Camera.farClipPlane = 3000f;
		}
		else if (size < 1000)
		{
			m_Camera.nearClipPlane = 0.5f;
			m_Camera.farClipPlane = 20000f;
		}
		else
		{
			m_Camera.nearClipPlane = 1;
			m_Camera.farClipPlane = 1000000f;
		}

		m_Camera.renderingPath = GetSceneViewRenderingPath();

		Handles.EnableCameraFlares (m_Camera, m_SceneViewState.showFlares);
		Handles.EnableCameraSkybox (m_Camera, m_SceneViewState.showSkybox);
		
		// Update the light
		m_Light[0].transform.position = m_Camera.transform.position;
		m_Light[0].transform.rotation = m_Camera.transform.rotation;

		// Update audio engine
		if (m_AudioPlay)
		{
			AudioUtil.SetListenerTransform(m_Camera.transform);
			AudioUtil.UpdateAudio();
		}

		if (m_ViewIsLockedToObject && Selection.gameObjects.Length > 0)
		{
			switch (m_DraggingLockedState)
			{
				case (DraggingLockedState.Dragging):
					// While dragging via handles, we don't want to move the camera
					break;
				case (DraggingLockedState.LookAt):
					if (!m_Position.value.Equals(Selection.activeGameObject.transform.position))
						// In playmode things might be moving fast so we can't lerp because it will lag behind
						if (!EditorApplication.isPlaying)
							m_Position.target = Selection.activeGameObject.transform.position;
						else
							m_Position.value = Selection.activeGameObject.transform.position;
					else
						m_DraggingLockedState = DraggingLockedState.NotDragging;
					break;
				case (DraggingLockedState.NotDragging):
					m_Position.value = Selection.activeGameObject.transform.position;
					break;
			}
		}
	}
	

	public void Update()
	{
		if (m_SceneViewState.showMaterialUpdate && m_lastRenderedTime + 0.033f < EditorApplication.timeSinceStartup)
		{
			m_lastRenderedTime = EditorApplication.timeSinceStartup;
			Repaint();
		}
		
	}

	internal Quaternion cameraTargetRotation { get { return m_Rotation.target; } }
	
	internal Vector3 cameraTargetPosition { get { return m_Position.target + m_Rotation.target * new Vector3 (0, 0, cameraDistance); } }
	
	internal float GetVerticalFOV (float aspectNeutralFOV)
	{
		float verticalHalfFovTangent = Mathf.Tan (aspectNeutralFOV * 0.5f * Mathf.Deg2Rad) * kOneOverSqrt2 / Mathf.Sqrt (m_Camera.aspect);
		return Mathf.Atan (verticalHalfFovTangent) * 2 * Mathf.Rad2Deg;
	}
	
	internal float GetVerticalOrthoSize ()
	{
		return size * kOneOverSqrt2 / Mathf.Sqrt (m_Camera.aspect);
	}

	// Look at a specific point.
	public void LookAt (Vector3 position) {
		FixNegativeSize ();
		m_Position.target = position;
	}

	// Look at a specific point from a given direction.
	public void LookAt (Vector3 position, Quaternion rotation) {
		FixNegativeSize ();
		m_Position.target = position;
		m_Rotation.target = rotation;
		// Update name in the top-right handle
		svRot.UpdateGizmoLabel (this, rotation * Vector3.forward, m_Ortho.target);
	}
	
	// Look directly at a specific point from a given direction.
	public void LookAtDirect (Vector3 position, Quaternion rotation) 
	{
		FixNegativeSize ();
		m_Position.value = position;
		m_Rotation.value = rotation;
		// Update name in the top-right handle
		svRot.UpdateGizmoLabel (this, rotation * Vector3.forward, m_Ortho.target);
	}

	// Look at a specific point from a given direction with a given zoom level.
	public void LookAt(Vector3 position, Quaternion rotation, float size)
	{
		FixNegativeSize();
		m_Position.target = position;
		m_Rotation.target = rotation;
		m_Size.target = Mathf.Abs(size);
		// Update name in the top-right handle
		svRot.UpdateGizmoLabel (this, rotation * Vector3.forward, m_Ortho.target);
	}

	// Look directally at a specific point from a given direction with a given zoom level.
	public void LookAtDirect(Vector3 position, Quaternion rotation, float size)
	{
		FixNegativeSize();
		m_Position.value = position;
		m_Rotation.value = rotation;
		m_Size.value = Mathf.Abs(size);
		// Update name in the top-right handle
		svRot.UpdateGizmoLabel (this, rotation * Vector3.forward, m_Ortho.target);
	}

	// Look at a specific point from a given direction with a given zoom level, enabling and disabling perspective
	public void LookAt (Vector3 position, Quaternion rotation, float size, bool orthographic)
	{
		LookAt (position, rotation, size, orthographic, false);
	}

	// Look at a specific point from a given direction with a given zoom level, enabling and disabling perspective
	public void LookAt (Vector3 position, Quaternion rotation, float size, bool orthographic, bool instant)
	{
		FixNegativeSize ();
		if (instant)
		{
			m_Position.value = position;
			m_Rotation.value = rotation;
			m_Size.value = Mathf.Abs (size);
			m_Ortho.value = orthographic;
		}
		else
		{
			m_Position.target = position;
			m_Rotation.target = rotation;
			m_Size.target = Mathf.Abs (size);
			m_Ortho.target = orthographic;
		}
		// Update name in the top-right handle
		svRot.UpdateGizmoLabel (this, rotation * Vector3.forward, m_Ortho.target);
	}
	
	void DefaultHandles () {		
		// Only switch tools when we don't have a hot control.
		if (GUIUtility.hotControl == 0)
			s_CurrentTool = Tools.viewToolActive ? 0 : Tools.current;

		Tool tool = (Event.current.type == EventType.repaint ? Tools.current : s_CurrentTool);
		
		switch (tool) {
		case Tool.None:	
		case Tool.View:	
			break;
		case Tool.Move:
			MoveTool.OnGUI (this);
			break;
		case Tool.Rotate:
			RotateTool.OnGUI ();
			break;
		case Tool.Scale:
			ScaleTool.OnGUI ();
			break;
		}
	}
	
	void CleanupEditorDragFunctions()
	{
		if (m_DragEditorCache != null)
			m_DragEditorCache.Dispose ();
		m_DragEditorCache = null;
	}

	void CallEditorDragFunctions ()
	{
        if (DragAndDrop.objectReferences.Length == 0)
            return;

        if (m_DragEditorCache == null)
            m_DragEditorCache = new EditorCache(EditorFeatures.OnSceneDrag);
		
		foreach (Object o in DragAndDrop.objectReferences)
		{
			if (o == null)
				continue;

			EditorWrapper w = m_DragEditorCache[o];
			if (w != null)
				w.OnSceneDrag (this);

			if (Event.current.type == EventType.Used)
				return;
		}
	}
	
	void HandleDragging () {
		Event evt = Event.current;
		
		switch (evt.type) {
		case EventType.DragPerform:
		case EventType.DragUpdated:
			CallEditorDragFunctions();
			
#if ENABLE_SPRITES
			bool isPerform = evt.type == EventType.DragPerform;

			if(m_2DMode)
				SpriteUtility.OnSceneDrag(this);
#endif

			if (evt.type == EventType.Used)
				break;

#if ENABLE_SPRITES
		// call old-style C++ dragging handlers

			if (DragAndDrop.visualMode != DragAndDropVisualMode.Copy)
			{
				GameObject go = HandleUtility.PickGameObject (Event.current.mousePosition, true);
				DragAndDrop.visualMode = InternalEditorUtility.SceneViewDrag (go, pivot, Event.current.mousePosition, isPerform);
			}

			if (isPerform && DragAndDrop.visualMode != DragAndDropVisualMode.None) {
				DragAndDrop.AcceptDrag ();
				evt.Use ();
				// Bail out as state can be fucked by now.
				GUIUtility.ExitGUI ();
			}

#else
			// call old-style C++ dragging handlers
			GameObject go = HandleUtility.PickGameObject (Event.current.mousePosition, true);
			
			DragAndDrop.visualMode = InternalEditorUtility.SceneViewDrag (go, pivot, Event.current.mousePosition, evt.type == EventType.DragPerform);
			
			if (evt.type == EventType.DragPerform && DragAndDrop.visualMode != DragAndDropVisualMode.None) {
				DragAndDrop.AcceptDrag ();
				evt.Use ();
				// Bail out as state can be fucked by now.
				GUIUtility.ExitGUI ();
			}
#endif		
			
			evt.Use ();
			break;
		case EventType.DragExited:
			CallEditorDragFunctions ();
			CleanupEditorDragFunctions ();
			break;
		case EventType.Repaint:
			CallEditorDragFunctions ();
			break;
		}
	}


	void CommandsGUI ()
	{
		/// @TODO: Validation should be more accurate based on what the view supports

		bool execute = Event.current.type == EventType.ExecuteCommand;
		switch (Event.current.commandName) {
		case "Find":
			if (execute)
				FocusSearchField();
			Event.current.Use();
			break;
		case "FrameSelected":
			if (execute)
			{
				bool useLocking = EditorApplication.timeSinceStartup - lastFramingTime < k_MaxDoubleKeypressTime;

				FrameSelected(useLocking);

				lastFramingTime = EditorApplication.timeSinceStartup;
			}
			Event.current.Use();
			break;
		case "FrameSelectedWithLock":
			if (execute)
				FrameSelected (true);
			Event.current.Use ();
			break;
		case "SoftDelete":
		case "Delete":
			if (execute)
				Unsupported.DeleteGameObjectSelection();
			Event.current.Use();
			break;
		case "Duplicate":
			if (execute)
				Unsupported.DuplicateGameObjectsUsingPasteboard ();
			Event.current.Use();
			break;
		case "Copy":
			if (execute)
				Unsupported.CopyGameObjectsToPasteboard();
			Event.current.Use();
			break;
		case "Paste":
			if (execute)
				Unsupported.PasteGameObjectsFromPasteboard();
			Event.current.Use();
			break;
		case "SelectAll":
			if (execute)
				Selection.objects = FindObjectsOfType(typeof(GameObject));      		        	
			Event.current.Use();
			break;

		default:
			break;
		}
	}
	
	public void AlignViewToObject (Transform t)
	{
		FixNegativeSize ();
		size = 10;
		LookAt (t.position + t.forward * CalcCameraDist (), t.rotation);
	}
	
	public void AlignWithView ()
	{
		FixNegativeSize ();
		Vector3 center = camera.transform.position;
		Vector3 dif = center - Tools.handlePosition;
		Quaternion delta = Quaternion.Inverse (Selection.activeTransform.rotation) * camera.transform.rotation;
		float angle;
		Vector3 axis;
		delta.ToAngleAxis (out angle, out axis);
		axis = Selection.activeTransform.TransformDirection (axis);

		Undo.RecordObjects (Selection.transforms, "Align with view");

		foreach (Transform t in Selection.transforms)
		{
			t.position += dif;
			t.RotateAround (center, axis, angle);
		}
	}	

	public void MoveToView ()
	{
		FixNegativeSize ();
		Vector3 dif = pivot - Tools.handlePosition;

		Undo.RecordObjects (Selection.transforms, "Move to view");

		foreach (Transform t in Selection.transforms)
		{
			t.position += dif;
		}
			
	}

	public void MoveToView (Transform target)
	{
		target.position = pivot;
	}

	public bool FrameSelected ()
	{
		return FrameSelected (false);
	}

	public bool FrameSelected (bool lockView)
	{
		viewIsLockedToObject = lockView;
		FixNegativeSize ();

		Bounds bounds = InternalEditorUtility.CalculateSelectionBounds (false);

		// Check active editor for OnGetFrameBounds
		foreach (Editor editor in GetActiveEditors ())
		{
			MethodInfo hasBoundsMethod = editor.GetType ().GetMethod ("HasFrameBounds", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.FlattenHierarchy);

			if (hasBoundsMethod != null)
			{
				object hasBounds = hasBoundsMethod.Invoke (editor, null);
				if (hasBounds != null && hasBounds is bool && (bool)hasBounds)
				{
					MethodInfo getBoundsMethod = editor.GetType ().GetMethod ("OnGetFrameBounds", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.FlattenHierarchy);

					if (getBoundsMethod != null)
					{
						object obj = getBoundsMethod.Invoke (editor, null);
						if (obj != null && obj is Bounds)
							bounds = (Bounds)obj;
					}
				}
			}
		}

		float size = bounds.extents.magnitude * 1.5f;
		if (size == Mathf.Infinity)
			return false;
		if (size == 0)
			size = 10;

		// We snap instantly into target on playmode, because things might be moving fast and lerping lags behind
		LookAt (bounds.center, m_Rotation.target, size * 2.2f, m_Ortho.value, EditorApplication.isPlaying);
		
		return true;
	}

	// setup scene view camera & lights
	void Setup () {
		GameObject cameraGO = EditorUtility.CreateGameObjectWithHideFlags ("SceneCamera", HideFlags.HideAndDontSave, typeof (Camera));
		cameraGO.AddComponent ("FlareLayer");
		cameraGO.AddComponent ("HaloLayer");
		m_Camera = cameraGO.camera;
		m_Camera.enabled = false;
		
		for (int i = 0; i < 3; i++) {
			GameObject lightGO = EditorUtility.CreateGameObjectWithHideFlags("SceneLight", HideFlags.HideAndDontSave, typeof(Light));
			m_Light[i] = lightGO.light;
			m_Light[i].type = LightType.Directional;
			m_Light[i].intensity = .5f;
			m_Light[i].enabled = false;
		}
		m_Light[0].color = kSceneViewFrontLight;

		m_Light[1].color = (Color)kSceneViewUpLight - (Color)kSceneViewMidLight;
		m_Light[1].transform.LookAt (Vector3.down);
		m_Light[1].renderMode = LightRenderMode.ForceVertex;

		m_Light[2].color = (Color)kSceneViewDownLight - (Color)kSceneViewMidLight;
		m_Light[2].transform.LookAt (Vector3.up);
		m_Light[2].renderMode = LightRenderMode.ForceVertex;
		
		HandleUtility.handleMaterial.SetColor ("_SkyColor", ((Color)kSceneViewUpLight) * 1.5f);
		HandleUtility.handleMaterial.SetColor ("_GroundColor", ((Color)kSceneViewDownLight) * 1.5f);
		HandleUtility.handleMaterial.SetColor ("_Color", ((Color)kSceneViewFrontLight) * 1.5f);
	}
	
	void CallOnSceneGUI ()
	{
		s_CurrentDrawingSceneView = this;
		
		foreach (Editor editor in GetActiveEditors())
		{
			// reset the handles matrix, OnSceneGUI calls may change it.
			Handles.matrix = Matrix4x4.identity;
		
			if (!EditorGUIUtility.IsGizmosAllowedForObject(editor.target))
				continue;
			/*
			// Don't call function for editors whose target's GameObject is not active.
			Component comp = editor.target as Component;
			if (comp && !comp.gameObject.activeInHierarchy)
				continue;

			// No gizmo if component state is disabled
			if (!InternalEditorUtility.GetIsInspectorExpanded(comp))
				continue;
			 * */

			MethodInfo method = editor.GetType().GetMethod("OnSceneGUI", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.FlattenHierarchy);
			
			if (method != null)
			{
				for (int n = 0; n < editor.targets.Length; n++)
				{
					editor.referenceTargetIndex = n;
					
					EditorGUI.BeginChangeCheck ();
					// Ironically, only allow multi object access inside OnSceneGUI if editor does NOT support multi-object editing.
					// since there's no harm in going through the serializedObject there if there's always only one target.
					Editor.m_AllowMultiObjectAccess = !editor.canEditMultipleObjects;
					method.Invoke(editor, null);
					Editor.m_AllowMultiObjectAccess = true;
					if (EditorGUI.EndChangeCheck ())
						editor.serializedObject.SetIsDifferentCacheDirty ();
				}
			}
		}
		
		// reset the handles matrix, OnSceneGUI calls may change it.
		Handles.matrix = Matrix4x4.identity;
	}

	void CallOnSceneGUIAfterGizmos ()
	{
		if (onSceneGUIDelegate != null)
			onSceneGUIDelegate (this);
		
		// reset the handles matrix, OnSceneGUI calls may change it.
		Handles.matrix = Matrix4x4.identity;
	}

	void CallOnPreSceneGUI ()
	{
		foreach (Editor editor in GetActiveEditors())
		{
			// reset the handles matrix, OnSceneGUI calls may change it.
			Handles.matrix = Matrix4x4.identity;
			
			// Don't call function for editors whose target's GameObject is not active.
			Component comp = editor.target as Component;
			if (comp && !comp.gameObject.activeInHierarchy)
				continue;
			
			MethodInfo method = editor.GetType().GetMethod("OnPreSceneGUI", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.FlattenHierarchy);
			
			if (method != null)
			{
				for (int n = 0; n < editor.targets.Length; n++)
				{
					editor.referenceTargetIndex = n;
					// Ironically, only allow multi object access inside OnPreSceneGUI if editor does NOT support multi-object editing.
					// since there's no harm in going through the serializedObject there if there's always only one target.
					Editor.m_AllowMultiObjectAccess = !editor.canEditMultipleObjects;
					method.Invoke(editor, null);
					Editor.m_AllowMultiObjectAccess = true;
				}
			}
		}
		
		// reset the handles matrix, OnSceneGUI calls may change it.
		Handles.matrix = Matrix4x4.identity;
	}
		
	internal static void ShowNotification (string notificationText)
	{
		Object[] sceneViews = Resources.FindObjectsOfTypeAll (typeof(SceneView));
		List<EditorWindow> notificationViews = new List<EditorWindow>();
		foreach (SceneView sceneView in sceneViews)
		{
			if(sceneView.m_Parent is DockArea)
			{
				DockArea dock = (DockArea)sceneView.m_Parent;
				if(dock)
				{
					if(dock.actualView == sceneView)
					{
						notificationViews.Add(sceneView);
					}
				}
			}
		}
		
		if(notificationViews.Count > 0)
		{
			foreach (EditorWindow notificationView in notificationViews)
				notificationView.ShowNotification(GUIContent.Temp(notificationText));
		}
		else
		{
			Debug.LogError(notificationText);
		}
	}
		
	public static void ShowCompileErrorNotification ()
	{
		ShowNotification ("All compiler errors have to be fixed before you can enter playmode!");
	}
	
	internal static void ShowSceneViewPlayModeSaveWarning ()
	{
		// In this case, we wan't to explicitely try the GameView before passing it on to whatever notificationView we have
		GameView gameView = (GameView)WindowLayout.FindEditorWindowOfType(typeof(GameView));
		if (gameView != null)
			gameView.ShowNotification (new GUIContent("You must exit play mode to save the scene!"));
		else
			ShowNotification ("You must exit play mode to save the scene!");
	}


#if ENABLE_SPRITES

	void ResetToDefaults(EditorBehaviorMode behaviorMode)
	{
		switch (behaviorMode)
		{
			case EditorBehaviorMode.Mode2D:
				m_2DMode = true;
				m_Rotation.value = Quaternion.identity;
				m_Position.value = kDefaultPivot;
				m_Size.value = kDefaultViewSize;
				m_Ortho.value = true;

				m_LastSceneViewRotation = kDefaultRotation;
				m_LastSceneViewOrtho = false;
				break;

			default: // Default to 3D mode (BUGFIX:569204)
			case EditorBehaviorMode.Mode3D:
				m_2DMode = false;
				m_Rotation.value = kDefaultRotation;
				m_Position.value = kDefaultPivot;
				m_Size.value = kDefaultViewSize;
				m_Ortho.value = false;
				break;
		}
	}

	internal void OnNewProjectLayoutWasCreated ()
	{
		ResetToDefaults (EditorSettings.defaultBehaviorMode);
	}

	private void On2DModeChange ()
	{
		if (m_2DMode)
		{
			if (!m_AnimValueManager.isUpdating)
				lastSceneViewRotation = rotation;
			m_LastSceneViewOrtho = orthographic;
			LookAt (pivot, Quaternion.identity, size, true);
		}
		else
		{
			LookAt (pivot, lastSceneViewRotation, size, m_LastSceneViewOrtho);
		}

		// Let's not persist the vertex snapping mode on 2D/3D mode change
		HandleUtility.ignoreRaySnapObjects = null;
		Tools.vertexDragging = false;
		Tools.handleOffset = Vector3.zero;
	}
#endif
}

} // namespace
