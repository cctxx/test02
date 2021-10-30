using System.Reflection;
using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using System.Linq;

namespace UnityEditor
{

internal enum WrapModeFixed
{
	Default = (int)WrapMode.Default,
	Once = (int)WrapMode.Once,
	Loop = (int)WrapMode.Loop,
	ClampForever = (int)WrapMode.ClampForever,
	PingPong = (int)WrapMode.PingPong
}

internal class GUIStyleX
{
	public static GUIStyle none = new GUIStyle();
}

internal class AnimationWindow : EditorWindow, CurveUpdater, TimeUpdater
{
	// Alive Animation windows
	private static List<AnimationWindow> s_AnimationWindows = new List<AnimationWindow> ();
	public static List<AnimationWindow> GetAllAnimationWindows ()
	{
		return s_AnimationWindows;
	}

	// We save the state of showing curve editor so if user re-opens animation window, it will be on/off based on previous state
	private static bool s_LastShowCurveEditor;

	// Static variables (do not need to be preserved across script reloads)
	internal static int vPosition;
	internal static bool kEvenRow;
	internal static int s_StartDragFrame = 0;
	
	internal static PrefColor kEulerXColor = new PrefColor ("Testing/EulerX", 1.0f, 0.0f, 1.0f, 1.0f);
	internal static PrefColor kEulerYColor = new PrefColor ("Testing/EulerY", 1.0f, 1.0f, 0.0f, 1.0f);
	internal static PrefColor kEulerZColor = new PrefColor ("Testing/EulerZ", 0.0f, 1.0f, 1.0f, 1.0f);
	internal static PrefColor kAnimatedColor = new PrefColor ("Testing/AnimatedObject", 0.0f, 0.0f, 0.0f, 0.3f);
	internal static PrefKey kAnimationPrevFrame = new PrefKey ("Animation/Previous Frame", ",");
	internal static PrefKey kAnimationNextFrame = new PrefKey ("Animation/Next Frame", ".");
	internal static PrefKey kAnimationPrevKeyframe = new PrefKey ("Animation/Previous Keyframe", "&,");
	internal static PrefKey kAnimationNextKeyframe = new PrefKey ("Animation/Next Keyframe", "&.");
	internal static PrefKey kAnimationRecordKeyframe = new PrefKey ("Animation/Record Keyframe", "k");
	internal static PrefKey kAnimationShowCurvesToggle = new PrefKey ("Animation/Show curves", "c");
	
	// Constants
	internal const int kTickRulerDistMin   =  3; // min distance between ruler tick marks before they disappear completely
	internal const int kTickRulerDistFull  = 80; // distance between ruler tick marks where they gain full strength
	internal const int kTickRulerDistLabel = 40; // min distance between ruler tick mark labels
	internal const float kTickRulerHeightMax    = 0.7f; // height of the ruler tick marks when they are highest
	internal const float kTickRulerFatThreshold = 0.5f; // size of ruler tick marks at which they begin getting fatter
	
	internal const int kIntFieldWidth = 35;
	internal const int kButtonWidth = 30;
	internal const int kAnimationHeight = 15; // 18
	internal const int kTimeRulerHeight = 17;
	internal const int kEventLineHeight = 18; // 16
	internal const int kKeyframeLineHeight = 15; // 13
	internal const int kHierarchyLeftMargin = 2;
	internal const int kHierarchyFieldWidth = 45;
	internal const int kHierarchyIconWidth = 11;
	internal const int kHierarchyIconButtonWidth = 22;
	internal const int kHierarchyIconHeight = 11;
	internal const int kHierarchyGameobjectHeight = 15;
	internal const int kHierarchyComponentHeight = 15;
	internal const int kHierarchyPropertyHeight = 12;
	internal const int kHierarchyAnimationSpacingHeight = 15;
	internal const int kFoldoutArrowWidth = 15;
	internal const int kIconWidth = 10;
	internal const int kSliderThickness = 15;
	internal const int kSamplesLabelWidth = 45;

	internal const float kCurveEditorPlayheadAlpha = 0.6f;

	float indentToContent { get { return kHierarchyLeftMargin + EditorGUI.indent + kFoldoutArrowWidth; } }

	// The foldout control auto indents based on EditorGUI.indent therefore the x is just kHierarchyLeftMargin 
	internal Rect GetFoldoutRect (float width)
	{
		return new Rect(kHierarchyLeftMargin, 
		                vPosition,
						width - kHierarchyLeftMargin - kHierarchyIconButtonWidth,
		                kHierarchyGameobjectHeight);
	}

	internal Rect GetFoldoutTextRect (float width)
	{
		float pixelIndent = indentToContent; 
		return new Rect(pixelIndent,
		                vPosition,
		                width-pixelIndent-kHierarchyIconButtonWidth,
		                kHierarchyGameobjectHeight);
	}

	internal Vector2 GetPropertyPos (float width)
	{
		return new Vector2(indentToContent + kIconWidth, vPosition);
	}

	internal Rect GetPropertyLabelRect (float width)
	{
		return GetPropertyLabelRect(width, indentToContent + kIconWidth);
	}

	internal Rect GetPropertyLabelRect (float width, float pixelIndent)
	{
		return new Rect(pixelIndent,
		                vPosition-3,
		                width-pixelIndent-kHierarchyIconButtonWidth-kHierarchyFieldWidth-15,
		                kHierarchyPropertyHeight+2);
	}

	internal Rect GetPropertySelectionRect (float width)
	{
		return new Rect(0,
		                vPosition,
		                width-kHierarchyIconButtonWidth-kHierarchyFieldWidth-15,
		                kHierarchyPropertyHeight);
	}

	internal Rect GetFieldRect (float width)
	{
		return new Rect(width-kHierarchyIconButtonWidth-kHierarchyFieldWidth-5,
		                vPosition,
		                kHierarchyFieldWidth,
		                kHierarchyPropertyHeight);
	}

	internal Rect GetIconRect (float width)
	{
		return new Rect(width-kHierarchyIconButtonWidth+2,
		                vPosition,
		                kHierarchyIconWidth,
		                kHierarchyPropertyHeight);
	}

	internal Rect GetIconButtonRect (float width)
	{
		return new Rect(width-kHierarchyIconButtonWidth,
		                vPosition,
		                kHierarchyIconButtonWidth,
		                kHierarchyPropertyHeight);
	}

	internal void DrawRowBackground (int width, int height)
	{
		DrawRowBackground(width, height, false);
	}

	internal void DrawRowBackground (int width, int height, bool selected)
	{
		kEvenRow = !kEvenRow;
		GUIStyle s = kEvenRow ? ms_Styles.rowEven : ms_Styles.rowOdd;
		s.Draw (new Rect(0, vPosition, width, height), false, false, selected, false);
	}

	[SerializeField]
	public AnimationWindowState state { get; set; }
	
	[SerializeField] private SerializedStringTable m_ExpandedFoldouts; // Which objects are folded out (states tranfer between different objects)
	[SerializeField] private SerializedStringTable m_ChosenAnimated; // Chosen objects with animation components out of possible choices
	[SerializeField] private SerializedStringTable m_ChosenClip; // Chosen animation clip out of possible choices
	[SerializeField] private bool m_ShowAllProperties = true;
	
	[SerializeField] AnimationEventTimeLine m_AnimationEventTimeLine;
	
	public SerializedStringTable expandedFoldouts { get { return m_ExpandedFoldouts; } set { m_ExpandedFoldouts = value; } }
	public SerializedStringTable chosenAnimated { get { return m_ChosenAnimated; } set { m_ChosenAnimated = value; } }
	public SerializedStringTable chosenClip { get { return m_ChosenClip; } set { m_ChosenClip = value; } }
	public bool showAllProperties { get { return m_ShowAllProperties; } }
	
	// Hierarchy
	AnimationWindowHierarchy m_Hierarchy;

	// Dopesheet editor
	DopeSheetEditor m_DopeSheetEditor;

	// Curve editor
	CurveEditor m_CurveEditor;

	// Selected animated objects
	public static AnimationSelection[] m_Selected;
	[System.NonSerialized]
	private CurveState[] m_ShownProperties = new CurveState[0]; // Which properties are currently shown
	[System.NonSerialized] private bool[] m_SelectedProperties = new bool[0]; // Which properties are currently selected out of those shown
	private int[] m_EditedCurves = new int[0]; // Which properties (curves) are sent to the curve editor
	
	// GUI state
	private Vector2 m_PropertyViewScroll;
	private bool m_CurveEditorToggleChanged;
	
	// Time state
	private bool m_AutoRecord = false;
	private bool m_PlayFromNoMode = false;	
	private float m_PrevRealTime = 0;
	//private float state.framerate = 60; // Temporary State
	//private int m_FinalFrame = 120;
	public float time { get { return state.FrameToTime(state.m_Frame); } }
	public float timeFloor { get { return state.FrameToTimeFloor (state.m_Frame); } }
	public float timeCeiling { get { return state.FrameToTimeCeiling (state.m_Frame); } }
	
	private CurveMenuManager m_MenuManager;
		
	public void SetDirtyCurves ()
	{		
		state.m_CurveEditorIsDirty = true;
	}	
	
	private bool m_PerformFrameSelectedOnCurveEditor = true;
	private bool m_PerformFrameSelectedOnCurveEditorHorizontally = false;

	internal class Styles
	{
		public Texture2D pointIcon = EditorGUIUtility.LoadIcon ("animationkeyframe");
		public GUIContent playContent = EditorGUIUtility.IconContent ("Animation.Play");
		public GUIContent recordContent = EditorGUIUtility.IconContent ("Animation.Record");
		public GUIContent prevKeyContent = EditorGUIUtility.IconContent ("Animation.PrevKey");
		public GUIContent nextKeyContent = EditorGUIUtility.IconContent ("Animation.NextKey");
		public GUIContent addKeyframeContent = EditorGUIUtility.IconContent ("Animation.AddKeyframe");
		public GUIContent addEventContent = EditorGUIUtility.IconContent ("Animation.AddEvent");
		public GUIStyle curveEditorBackground = "AnimationCurveEditorBackground";
		public GUIStyle eventBackground = "AnimationEventBackground";
		public GUIStyle keyframeBackground = "AnimationKeyframeBackground";
		public GUIStyle rowOdd  = "AnimationRowEven";
		public GUIStyle rowEven = "AnimationRowOdd";
		public GUIStyle TimelineTick = "AnimationTimelineTick";
		public GUIStyle miniToolbar = new GUIStyle (EditorStyles.toolbar);
		public GUIStyle miniToolbarButton = new GUIStyle (EditorStyles.toolbarButton);
		public GUIStyle toolbarLabel = new GUIStyle (EditorStyles.toolbarPopup);
		
		public Styles ()
		{
			toolbarLabel.normal.background = null;
			
			miniToolbarButton.padding.top = 0;
			miniToolbarButton.padding.bottom = 3;
		}
	}
	internal static Styles ms_Styles;
	
	SplitterState m_HorizontalSplitter;
	
	/******************************************************
	ANIMATION MODE AND PLAY MODE METHODS
	******************************************************/
	
	void ToggleAnimationMode ()
	{
		if (AnimationMode.InAnimationMode())
			EndAnimationMode();
		else
			BeginAnimationMode(true);
		if (Toolbar.get != null)
			Toolbar.get.Repaint ();
	}
	
	public bool EnsureAnimationMode ()
	{
		if (AnimationMode.InAnimationMode ())
			return true;
		else
			return BeginAnimationMode (true);
	}
	
	public void ReEnterAnimationMode ()
	{
		if (AnimationMode.InAnimationMode())
		{
			int tempFrame = state.m_Frame;
			EndAnimationMode();
			state.m_Frame = tempFrame;
			BeginAnimationMode(false);
		}
	}
	
	public bool AllHaveClips ()
	{
		foreach (AnimationSelection animSel in m_Selected)
		{
			if (animSel.clip == null)
				return false;
		}
		return true;
	}
	
	public static bool EnsureAllHaveClips ()
	{
		foreach (AnimationSelection animSel in m_Selected)
		{
			if (!animSel.EnsureClipPresence())
			{
				return false;
			}
		}
		return true;
	}

	public bool SelectionIsActive ()
	{
		if (m_Selected == null || m_Selected.Length == 0 || m_Selected[0] == null || m_Selected[0].clip == null)
			return false;

		return true;
	}
	
	public bool BeginAnimationMode (bool askUserIfMissingClips)
	{
		if (m_Selected.Length == 0 || !m_Selected[0].GameObjectIsAnimatable)
			return false;
		
		if (!askUserIfMissingClips && !AllHaveClips())
			return false;
		
		if (askUserIfMissingClips && !EnsureAllHaveClips())
			return false;
		
		List<Object> roots = new List<Object>();
		foreach (AnimationSelection sel in m_Selected)
		{
			Object root = sel.animatedObject.transform.root.gameObject;
			if (!roots.Contains(root))
				roots.Add(root);
		}

		AnimationMode.StartAnimationMode();
		
		ResampleAnimation();
		
		// TODO: If we decide to make animation mode = auto-record mode,
		// then make more elegant merge of the modes
		SetAutoRecordMode (true);
		
		SetDirtyCurves();

		// Force repaint all inspectors because the inspectors have some items disabled in animation mode
		Tools.RepaintAllToolViews();
		
		return true;
	}
	
	public void EndAnimationMode ()
	{
		if (!AnimationMode.InAnimationMode())
			return;
		
		// Currently OnSelectionChange is called when adding / removing components.
		// Thus adding a component will exit playmode
		if (state.m_AnimationIsPlaying)
			Stop();
		state.m_Frame = 0;
		
		//float timeB = Time.realtimeSinceStartup;
		//Profiler.SharkBeginRemoteProfiling();
		AnimationMode.StopAnimationMode();
		//Profiler.SharkEndRemoteProfiling();
		//DebugTime("RestoreAnimationModeSnapshot", timeB, Time.realtimeSinceStartup);
		
		SetDirtyCurves();
		
		// Force repaint all inspectors because the inspectors have some items disabled in animation mode
		Tools.RepaintAllToolViews();
	}
	
	void SetPlayMode (bool play)
	{
		if (play != state.m_AnimationIsPlaying)
		{
			if (state.m_AnimationIsPlaying)
				Stop();
			else
				Play();
		}
	}
	
	void Play ()
	{
		bool playFromNoMode = !AnimationMode.InAnimationMode();
		if (!EnsureAnimationMode())
			return;
		
		m_PlayFromNoMode = playFromNoMode;
		state.m_PlayTime = Mathf.Max (new[] { 0, state.minTime, state.GetTimeSeconds () });
		state.m_AnimationIsPlaying = true;
		m_PrevRealTime = Time.realtimeSinceStartup;
	}
	
	void Stop ()
	{
		state.m_Frame = Mathf.RoundToInt (state.m_PlayTime * state.frameRate);
		state.m_AnimationIsPlaying = false;
		if (m_PlayFromNoMode)
			EndAnimationMode();
		else
			// Re-enter animation mode to reset accidental changes user made to properties during play mode
			ReEnterAnimationMode();
		m_PlayFromNoMode = false;
	}
	
	void Update ()
	{
		if (state == null)
			return;

		if (state.m_AnimationIsPlaying)
		{
			if (!SelectionIsActive())
				return;

			float deltaTime = Time.realtimeSinceStartup - m_PrevRealTime;
			float minTime = 0f;
			float maxTime = state.m_ActiveAnimationClip.length;

			state.m_PlayTime += deltaTime;
			
			if (state.m_PlayTime > maxTime)
				state.m_PlayTime = minTime;

			state.m_PlayTime = Mathf.Clamp (state.m_PlayTime, minTime, maxTime);

			m_PrevRealTime = Time.realtimeSinceStartup;
			state.m_Frame = Mathf.RoundToInt (state.m_PlayTime * state.frameRate);
			ResampleAnimation();
			Repaint();
		}

		// If dopesheet editor is in the process of loading previews, we want to fire repaints until everything is loaded
		if (m_DopeSheetEditor != null && m_DopeSheetEditor.m_SpritePreviewLoading)
			Repaint();
	}
	
	/******************************************************
	PREVIEWING METHODS
	******************************************************/
	
	void Next ()
	{
		List<AnimationWindowCurve> curves = state.m_ShowCurveEditor ? state.activeCurves : state.allCurves;

		state.m_PlayTime = AnimationWindowUtility.GetNextKeyframeTime (curves.ToArray (), state.FrameToTime(state.m_Frame));
		state.m_Frame = state.TimeToFrameFloor (state.m_PlayTime);

		PreviewFrame(state.m_Frame);
	}

	void Prev ()
	{
		List<AnimationWindowCurve> curves = state.m_ShowCurveEditor ? state.activeCurves : state.allCurves;

		state.m_PlayTime = AnimationWindowUtility.GetPreviousKeyframeTime (curves.ToArray (), state.FrameToTime (state.m_Frame));
		state.m_Frame = state.TimeToFrameFloor (state.m_PlayTime);

		PreviewFrame (state.m_Frame);
	}
	
	public void PreviewFrame (int frame)
	{
		if (!EnsureAnimationMode())
			return;
		
		state.m_Frame = frame;
		ResampleAnimation();
	}
	
	/******************************************************
	AUTORECORD METHODS
	******************************************************/
	
	void UndoRedoPerformed ()
	{
		if (AnimationMode.InAnimationMode())
		{
			PreviewFrame(state.m_Frame);
		}
		SetDirtyCurves();
		Repaint();
	}
	
	public bool GetAutoRecordMode ()
	{
		return m_AutoRecord;
	}
	
	private UndoPropertyModification[] PostprocessAnimationRecordingModifications (UndoPropertyModification[] modifications)
	{
		return AnimationRecording.Process (state, modifications);
	}
	
	public void SetAutoRecordMode (bool record)
	{
		if (m_AutoRecord != record)
		{
			// Make sure there are animation clips assigned when entring auto record mode
			if (record)
			{
				record = EnsureAllHaveClips();
				Undo.postprocessModifications += PostprocessAnimationRecordingModifications;
			}
			else
			{
				Undo.postprocessModifications -= PostprocessAnimationRecordingModifications;
			}
			
			m_AutoRecord = record;
			
			if (m_AutoRecord)
				EnsureAnimationMode();
		}
	}
	
	/******************************************************
	PROPERTY MANAGEMENT METHODS
	******************************************************/
	
	private bool IsLinked (CurveState state, bool onlyLinkEuler)
	{
		if (!(state.type == typeof (Transform)))
			return false;
		if (onlyLinkEuler && !state.propertyName.StartsWith ("localEulerAngles."))
			return false;
		return true;
	}
	
	private bool AnyPropertiesSelected()
	{
		foreach (bool b in m_SelectedProperties)
			if (b)
				return true;
		return false;
	}
	
		
	/******************************************************
	REFRESH LOGIC METHODS
	******************************************************/

	void InitSelection ()
	{
		//float timeA = Time.realtimeSinceStartup;
		ClearShownProperties();
		GenerateAnimationSelections();

		EvaluateFramerate ();
		SetDirtyCurves();

		if (state.m_ActiveAnimationClip != null || state.m_ActiveGameObject != null)
			Repaint();
	}
	
	void RefreshAnimationSelections ()
	{
		foreach (AnimationSelection animSel in m_Selected)
			animSel.Refresh();
	}
	
	AnimationSelection GetOrAddAnimationSelectionWithObjects (GameObject[] animatedOptions, List<AnimationSelection> animSelected)
	{
		AnimationSelection thisAnim = null;
		
		string thisOptionsHash = AnimationSelection.GetObjectListHashCode(animatedOptions);
		foreach (AnimationSelection anim in animSelected)
		{
			if (AnimationSelection.GetObjectListHashCode(anim.animatedOptions) == thisOptionsHash)
			{
				thisAnim = anim;
				break;
			}
		}

		if (thisAnim == null)
		{
			// Create new animation selection
			thisAnim = new AnimationSelection(animatedOptions, m_ChosenAnimated, m_ChosenClip, this);
			
			animSelected.Add(thisAnim);
		}
		return thisAnim;
	}
	
	// Get array with game object and its parents
	GameObject[] GetAllParents (Transform tr)
	{
		List<GameObject> gameobjects = new List<GameObject>();
		gameobjects.Add(tr.gameObject);
		while (tr != tr.root)
		{
			tr = tr.parent;
			gameobjects.Add(tr.gameObject);
		}
		gameobjects.Reverse();
		return gameobjects.ToArray();
	}
	
	// Get array with game object and its parents that have animation components
	GameObject[] GetAnimationComponentsInAllParents (Transform tr)
	{
		List<GameObject> gameobjects = new List<GameObject>();
		// Find possible animation components on game object and all parents
		while (true)
		{
			if (tr.animation || tr.GetComponent<Animator> ()) gameobjects.Add(tr.gameObject);
			if (tr == tr.root) break;
			tr = tr.parent;
		}
		gameobjects.Reverse();
		return gameobjects.ToArray();
	}
	
	GameObject[] GetTrackedGameObjects ()
	{
		List<GameObject> animatedGameObjects = new List<GameObject> ();
		
		if (m_Selected.Length > 0)
		{
			FoldoutTree tree = m_Selected[0].trees[0];
			for (int i=0;i<tree.states.Length;i++)
			{
				GameObject go = tree.states[i].obj;
				if (go != null)
					animatedGameObjects.Add(go);
			}
		}
		
		return animatedGameObjects.ToArray();
	}
	
	void GenerateAnimationSelections ()
	{
		//float timeA = Time.realtimeSinceStartup;
		// TODO: changing selection is pretty slow on fully animated objects (deep hierachies)
		// when animation mode is enabled. We need to find out why, and optimize.
		
		// If selecting a (child) object that belongs to the same animated object as one of the
		// currently selected objects, do not go out of animation mode.
		// Technically, animation mode is ended and then started again. This is needed so that
		// objects that are no longer part of the new selection will be properly reverted
		// to their native state.

		bool wasInAnimMode = AnimationMode.InAnimationMode();
		bool sameAnimated = false;
		int tempFrame = state.m_Frame;
		EndAnimationMode();
		
		List<AnimationSelection> animSelected = new List<AnimationSelection>();
		
		GameObject go = Selection.activeGameObject != null ? Selection.activeGameObject : state.m_ActiveGameObject;
		Transform sel = go ? go.transform : null;

		// Maybe the active go just got deleted, but there is still animation in previously active rootGo
		if (sel == null)
		{
			sel = state.m_RootGameObject != null ? state.m_RootGameObject.transform : null;
			state.m_ActiveGameObject = state.m_RootGameObject;
			state.refresh = AnimationWindowState.RefreshType.Everything;
		}

		if (sel != null)
		{
			// Get array with game object and its parents that have animation components:
			GameObject[] animatedOptions = GetAnimationComponentsInAllParents (sel);

			// If none of them have animation components, then just get object and all parents:
			if (animatedOptions.Length == 0)
				animatedOptions = GetAllParents (sel);

			AnimationSelection thisAnim = GetOrAddAnimationSelectionWithObjects (animatedOptions, animSelected);

			if (m_Selected != null)
			{
				foreach (AnimationSelection oldAnim in m_Selected)
				{
					if (oldAnim.animatedObject == thisAnim.animatedObject)
						sameAnimated = true;
				}
			}

			// Create a new foldout tree for the current transform
			FoldoutTree tree = new FoldoutTree (sel, m_ExpandedFoldouts);
			thisAnim.AddTree (tree);
		}

		m_Selected = animSelected.ToArray();

		if (m_Selected != null && m_Selected.Length > 0)
		{
			if (Selection.activeGameObject != null)
				state.m_ActiveGameObject = Selection.activeGameObject;

			state.m_ActiveAnimationClip = m_Selected[0].clip;
            state.m_RootGameObject = m_Selected[0].avatarRootObject;
			state.m_AnimatedGameObject = m_Selected[0].animatedObject;
		}
		else
		{
			state.m_ActiveAnimationClip = null;
			state.m_RootGameObject = null;	
			state.m_AnimatedGameObject = null;
			state.m_ActiveGameObject = null;
			state.refresh = AnimationWindowState.RefreshType.Everything;
		}

		if (wasInAnimMode && sameAnimated)
		{
			state.m_Frame = tempFrame;
			BeginAnimationMode(false);
		}
		if (!sameAnimated)
		{
			AnimationEventPopup.ClosePopup();
			m_AnimationEventTimeLine.DeselectAll ();
		}
		
		//DebugTime("GenerateAnimationSelections", timeA, Time.realtimeSinceStartup);		
	}
	
	public void UpdateFrame (int frame)
	{
		if (state.m_AnimationIsPlaying || !AnimationMode.InAnimationMode())
			return;

		PreviewFrame(frame);
	}
	
	public void UpdateTime (float time)
	{
		if (state.m_AnimationIsPlaying || !AnimationMode.InAnimationMode())
			return;
		state.m_Frame = Mathf.RoundToInt (state.TimeToFrame (time));
		PreviewFrame(state.m_Frame);
	}
	
	public void UpdateCurves (List<int> curveIds, string undoText)
	{
		for (int i=0; i<m_ShownProperties.Length; i++)
		{
			CurveState curveState = m_ShownProperties[i];
			if (!curveIds.Contains(curveState.GetID()))
				continue;
			
			if (!curveState.animated)
				curveState.animationSelection.EnsureClipPresence();
			
			if (curveState.clip == null)
				Debug.LogError("clip is null");

			curveState.SaveCurve(curveState.curve);
		}
		SetDirtyCurves();
	}
	
	void ResampleAnimation()
	{
		if (!EnsureAnimationMode())
			return;
		
		Undo.FlushUndoRecordObjects ();
		
		AnimationMode.BeginSampling ();
		foreach (AnimationSelection animSel in m_Selected)
		{
			AnimationClip clip = animSel.clip;
			if (clip == null)
				continue;
			
			GameObject animated = animSel.animatedObject;
			if (animated == null)
				continue;

			AnimationMode.SampleAnimationClip (animated, clip, state.GetTimeSeconds ());
		}
		AnimationMode.EndSampling();

		SceneView.RepaintAll();
	}
	
	// TODO move code to get keyframes into C++
	
	bool EvaluateFramerate ()
	{
		// We assume in the timeline that the framerate is a whole number
		
		if (m_Selected.Length == 0)
		{
			// keep old values
			state.frameRate = Mathf.Abs(state.frameRate);
			return true;
		}
		
		float oldFrameRate = Mathf.Abs(state.frameRate);
		
		int framerate = 0;
		float maxLength = 0;
		bool sameFramerate = true;
		foreach (AnimationSelection animSel in m_Selected)
		{
			if (animSel.clip != null)
			{
				maxLength = Mathf.Max(maxLength, animSel.clip.length);
				if (framerate == 0) framerate = Mathf.RoundToInt(animSel.clip.frameRate);
				else if (animSel.clip.frameRate != framerate)
				{
					sameFramerate = false;
					break;
				}
			}
		}
		if (framerate == 0)
			framerate = 60;
		
		if (!sameFramerate)
		{
			state.frameRate = -oldFrameRate;
			return false;
		}

		if (state.frameRate != framerate)
			state.frameRate = framerate;
		
		//m_FinalFrame = Mathf.RoundToInt(maxLength * Mathf.Abs(state.frameRate));
		
		float newFrameRateRatio = state.frameRate / oldFrameRate;
		state.m_Frame = Mathf.RoundToInt (newFrameRateRatio * state.m_Frame);
		
		m_CurveEditor.hTicks.SetTickModulosForFrameRate (state.frameRate);
		return true;
	}
	
	private void ClearShownProperties ()
	{
		m_ShownProperties = new CurveState[0];
		m_EditedCurves = new int[0];
		m_SelectedProperties = new bool[0];
		m_CurveEditor.animationCurves = new CurveWrapper[0];
		CurveRendererCache.ClearCurveRendererCache();
	}
	
	public void RefreshShownCurves (bool forceUpdate)
	{
		state.m_CurveEditorIsDirty = false;

		if (!SelectionIsActive ())
		{
			ClearShownProperties();
			return;
		}

		// Needed to bridge new hierarchy and old curve editor
		if (state.m_ShowCurveEditor || forceUpdate)
			SetupEditorCurvesHack (); 
		else
			ClearShownProperties();

		EvaluateFramerate(); // also used to evaluate max length of clips
		m_CurveEditor.invSnap = state.frameRate;
		
		bool anySelected = AnyPropertiesSelected();
		
		// Assign shown curves to curve editor
		CurveWrapper[] curveWrappers = new CurveWrapper[m_EditedCurves.Length];
		for (int i=0; i<curveWrappers.Length; i++)
		{
			CurveState curveState = m_ShownProperties[m_EditedCurves[i]];
			curveWrappers[i] = new CurveWrapper();
			curveWrappers[i].id = curveState.GetID();
			if (IsLinked(curveState, true))
				curveWrappers[i].groupId = curveState.GetGroupID();
			else
				curveWrappers[i].groupId = -1;
			curveWrappers[i].color = curveState.color;
			curveWrappers[i].hidden = (anySelected && !m_SelectedProperties[m_EditedCurves[i]]);
			// TODO: Decide how to visualize read-only curves
			if (curveWrappers[i].readOnly)
				curveWrappers[i].color.a = 0.3f;
				
			curveWrappers[i].renderer = CurveRendererCache.GetCurveRenderer (curveState.clip, curveState.curveBinding);
			curveWrappers[i].renderer.SetWrap (curveState.clip.isLooping ? WrapMode.Loop : WrapMode.Clamp);
			curveWrappers[i].renderer.SetCustomRange(0, curveState.clip.length);
		}
		m_CurveEditor.animationCurves = curveWrappers;
		
		if (AnimationMode.InAnimationMode () && GUI.changed)
			ResampleAnimation();
	}

	public void SetupEditorCurvesHack ()
	{
		if (SelectionIsActive())
		{
			List<CurveState> shownProperties = new List<CurveState> ();

			List<AnimationWindowCurve> curves = state.activeCurves;
			foreach (AnimationWindowCurve curve in curves)
			{
				if (!curve.isPPtrCurve)
				{
					CurveState curveState = new CurveState (curve.binding);
					curveState.animationSelection = m_Selected[0];
					curveState.animated = true;
					curveState.color = CurveUtility.GetPropertyColor (curveState.curveBinding.propertyName);
					shownProperties.Add (curveState);
				}
			}

			m_ShownProperties = shownProperties.ToArray ();
			m_EditedCurves = new int[m_ShownProperties.Length];

			for (int i = 0; i < m_EditedCurves.Length; i++)
				m_EditedCurves[i] = i;
		}
		else
		{
			// Clean out old data
			m_EditedCurves = new int[0];
			m_ShownProperties = new CurveState[0];
		}
	}

	void FrameSelected () { m_PerformFrameSelectedOnCurveEditor = true; }

	/******************************************************
	ONGUI METHODS
	******************************************************/
		
	void DopeSheetGUI (Rect position)
	{
        m_DopeSheetEditor.rect = position;
		position.height -= kSliderThickness;

        m_DopeSheetEditor.RecalculateBounds();
		
		m_DopeSheetEditor.BeginViewGUI ();
		m_DopeSheetEditor.OnGUI(position, state.m_hierarchyState.scrollPos * -1);

		Rect verticalScrollBarPosition = new Rect (position.xMax, position.yMin, 16, position.height);

        float contentHeight = Mathf.Max(m_DopeSheetEditor.contentHeight, position.height);
        state.m_hierarchyState.scrollPos.y = GUI.VerticalScrollbar(verticalScrollBarPosition, state.m_hierarchyState.scrollPos.y, position.height, 0f, contentHeight);

		m_DopeSheetEditor.EndViewGUI();
	}

	public void TimeLineGUI (Rect rect, bool onlyDopesheetLines, bool sparseLines)
	{
		// This is to make sure that animation window hierarchy doesn't have focus anymore (case 569381)
		if (Event.current.type == EventType.MouseDown && rect.Contains (Event.current.mousePosition))
			GUIUtility.keyboardControl = 0;

		Color backupCol = GUI.color;
		GUI.BeginGroup(rect);
		rect.x = 0;
		rect.y = 0;
		if (Event.current.type != EventType.Repaint)
		{
			GUI.EndGroup();
			return;
		}
		
		HandleUtility.handleWireMaterial.SetPass (0);
		GL.Begin (GL.LINES);
		
		Color tempBackgroundColor = GUI.backgroundColor;
		
		if (sparseLines)
			m_CurveEditor.hTicks.SetTickStrengths(m_CurveEditor.settings.hTickStyle.distMin, m_CurveEditor.settings.hTickStyle.distFull, true);
		else
			m_CurveEditor.hTicks.SetTickStrengths(kTickRulerDistMin, kTickRulerDistFull, true);
		
		Color baseColor = m_CurveEditor.settings.hTickStyle.color;
		baseColor.a = 0.75f;

		// Precalc for frame->pixel conversion
		float pixelDelta = rect.width / state.frameSpan;
		float minFrame = state.minFrame;

		// Draw tick markers of various sizes
		for (int l=0; l<m_CurveEditor.hTicks.tickLevels; l++)
		{
			float strength = m_CurveEditor.hTicks.GetStrengthOfLevel(l) * .9f;
			float[] ticks = m_CurveEditor.hTicks.GetTicksAtLevel(l, true);

			for (int i=0; i<ticks.Length; i++)
			{
				if (ticks[i] < 0)
					continue;
				int frame = Mathf.RoundToInt(ticks[i]*state.frameRate);
				
				float height = kTimeRulerHeight * Mathf.Min(1,strength) * kTickRulerHeightMax;
				
				float pixel = (frame - minFrame) * pixelDelta;

				if (!onlyDopesheetLines)
				{
					// Draw line
					GL.Color (new Color (1, 1, 1, strength / kTickRulerFatThreshold) * baseColor);
					GL.Vertex (new Vector3 (pixel, kTimeRulerHeight - height + 0.5f, 0));
					GL.Vertex (new Vector3 (pixel, kTimeRulerHeight - 0.5f, 0));
				}

				if (onlyDopesheetLines)
				{
					GL.Color(new Color(1, 1, 1, strength / kTickRulerFatThreshold) * .37f);
					GL.Vertex(new Vector3(pixel, rect.yMin, 0));
					GL.Vertex(new Vector3(pixel, rect.yMax, 0));
				}
				else
				{
					GL.Color(new Color(1, 1, 1, strength / kTickRulerFatThreshold) * .4f);
					GL.Vertex(new Vector3(pixel, rect.yMin + kTimeRulerHeight + kEventLineHeight, 0));
					GL.Vertex (new Vector3 (pixel, rect.yMax, 0));
				}
			}
		}
		
		GL.End ();

		if (!onlyDopesheetLines)
		{
			// Draw tick labels
			int frameDigits = ((int) state.frameRate).ToString ().Length;
			int labelLevel = m_CurveEditor.hTicks.GetLevelWithMinSeparation (kTickRulerDistLabel);
			float[] labelTicks = m_CurveEditor.hTicks.GetTicksAtLevel (labelLevel, false);
			for (int i = 0; i < labelTicks.Length; i++)
			{
				if (labelTicks[i] < 0)
					continue;
				int frame = Mathf.RoundToInt (labelTicks[i] * state.frameRate);
				// Important to take floor of positions of GUI stuff to get pixel correct alignment of
				// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.

				float labelpos = Mathf.Floor (state.FrameToPixel (frame, rect));
				string label = state.FormatFrame (frame, frameDigits);
				GUI.Label (new Rect (labelpos + 3, -3, 40, 20), label, ms_Styles.TimelineTick);
			}
		}

		GUI.EndGroup();
		
		GUI.backgroundColor = tempBackgroundColor;
		GUI.color = backupCol;
	}
	
	void SecondaryTickMarksGUI (Rect rect)
	{
		GUI.BeginGroup(rect);
		if (Event.current.type != EventType.Repaint)
		{
			GUI.EndGroup();
			return;
		}
		
		m_CurveEditor.hTicks.SetTickStrengths(m_CurveEditor.settings.hTickStyle.distMin, m_CurveEditor.settings.hTickStyle.distFull, false);
		
		HandleUtility.handleWireMaterial.SetPass (0);
		GL.Begin (GL.LINES);
		
		// Draw tick markers of various sizes
		for (int l=0; l<m_CurveEditor.hTicks.tickLevels; l++)
		{
			float strength = m_CurveEditor.hTicks.GetStrengthOfLevel(l);
			GL.Color (m_CurveEditor.settings.hTickStyle.color * new Color(1,1,1,strength) * new Color (1, 1, 1, 0.75f));
			float[] ticks = m_CurveEditor.hTicks.GetTicksAtLevel(l, true);
			for (int i=0; i<ticks.Length; i++)
			{
				if (ticks[i] < 0) continue;
				int frame = Mathf.RoundToInt(ticks[i]*state.frameRate);
				
				// Draw line
				GL.Vertex (new Vector2 (state.FrameToPixel (frame, rect), 0));
				GL.Vertex (new Vector2 (state.FrameToPixel (frame, rect), rect.height));
			}
		}
		
		GL.End ();
		
		GUI.EndGroup();
	}

	public void OnGUI ()
	{
		//@TODO: Support for optimized animator
		if (state.AnimatorIsOptimized)
		{
			GUILayout.Label ("Editing optimized game object hierarchy is not supported.\nPlease select a game object that does not have 'Optimize Game Objects' applied.");
			return;
		}

		state.OnGUI();
		
//		bool isPreviousSelectionInvalid = m_Selected == null || (m_Selected.Length != 0 && m_Selected[0].animatedObject == null);
//		if (isPreviousSelectionInvalid)			
//		{
//			OnSelectionChange();
//		}
		InitAllViews();

		if (state.m_ActiveGameObject == null)
		{
			m_Selected = null;
			state.m_ShowCurveEditor = false;
		}

		if (m_Selected == null)
			OnSelectionChange();

		if (m_Selected.Length == 0)
			EndAnimationMode ();

		if (state == null)
			return;

		// Set toggle value on start of OnGUI, will break stuff if changed in the middle
		if (m_CurveEditorToggleChanged)
		{
			m_CurveEditorToggleChanged = false;
			state.m_ShowCurveEditor = !state.m_ShowCurveEditor;
			HandleEmptyCurveEditor ();
		}

		// Select visible timeArea
		state.timeArea = state.m_ShowCurveEditor ? (TimeArea)m_CurveEditor : (TimeArea)m_DopeSheetEditor;

		if (state.m_CurveEditorIsDirty)
		{
			CurveRendererCache.ClearCurveRendererCache ();
			RefreshShownCurves (false);
			// Resampling toggles autorecording on. Resample only when we are already recording
			if (AnimationMode.InAnimationMode ())
				ResampleAnimation();
		}

		// Frame selection
		if (m_PerformFrameSelectedOnCurveEditor)
		{			
			if (state.m_ShowCurveEditor)
				m_CurveEditor.FrameSelected (m_PerformFrameSelectedOnCurveEditorHorizontally, true);

			m_PerformFrameSelectedOnCurveEditor = false;
			m_PerformFrameSelectedOnCurveEditorHorizontally = false;
		}
		
		int hierarchyWidth = m_HorizontalSplitter.realSizes[0];
		
		// Entire right side of animation window
		Rect rightRect = new Rect (hierarchyWidth, 0, position.width - hierarchyWidth, position.height);
		
		// Time ruler on top
		Rect timeRulerRect = new Rect (hierarchyWidth, 0, rightRect.width - kSliderThickness, kTimeRulerHeight);
		
		// Animation events line under the ruler
		Rect eventLineRect = new Rect (hierarchyWidth, timeRulerRect.yMax, rightRect.width - kSliderThickness, kEventLineHeight);
		
		Rect timeRulerAndEventLineRect = new Rect (hierarchyWidth, timeRulerRect.yMin, rightRect.width - kSliderThickness, eventLineRect.yMax);

		// Curve editor has it's own sliders, dopesheet uses the ones from animationwindow
		float contentRectWidth = state.m_ShowCurveEditor ? rightRect.width : rightRect.width - kSliderThickness;
		
		// Main content showing either curveEditor or dopesheet
		Rect mainContentRect = new Rect (hierarchyWidth, eventLineRect.yMax, contentRectWidth, rightRect.height - kTimeRulerHeight - kEventLineHeight);
		Rect dopeSheetRect = state.m_ShowCurveEditor ? new Rect () : mainContentRect;

		Rect curveEditorRect = state.m_ShowCurveEditor ? mainContentRect : new Rect (hierarchyWidth, mainContentRect.yMax, mainContentRect.width, 0);
		Rect curveEditorRectMinusScrollbars = state.m_ShowCurveEditor ? new Rect (curveEditorRect.xMin, curveEditorRect.yMin, curveEditorRect.width, curveEditorRect.height - kSliderThickness) : curveEditorRect;

		// Clickable area for dopesheet. If curve editor is visible (has height), we make sure this rect don't overlap it.
		Rect clickableRightRect = new Rect (hierarchyWidth, 0, rightRect.width - kSliderThickness, rightRect.height - curveEditorRect.height - kSliderThickness);

		m_CurveEditor.rect = curveEditorRect;
			
		if (ms_Styles == null)
			ms_Styles = new Styles();
		
		Handles.lighting = false;
		
		// Reset auto record mode if we are no longer in animation mode
		if (!AnimationMode.InAnimationMode())
		{
			SetAutoRecordMode(false);
			state.m_Frame = 0;
		}
		
		GUI.changed = false;
		vPosition = 0;
		kEvenRow = true;

		SplitterGUILayout.BeginHorizontalSplit (m_HorizontalSplitter);

			EditorGUILayout.BeginVertical(GUILayout.MaxWidth(hierarchyWidth));
				// Time buttons and fields
				GUI.changed = false;
				EditorGUILayout.BeginHorizontal(EditorStyles.toolbar, GUILayout.Width(hierarchyWidth));

					/******************************************************
					LEFT SIDE: TOOLBAR:  TOP
					******************************************************/

					// Animation mode button
					EditorGUI.BeginDisabledGroup (!state.m_ActiveGameObject || state.IsPrefab);
					if (m_PlayFromNoMode)
					{
						bool animMode = GUILayout.Toggle (false, ms_Styles.recordContent, EditorStyles.toolbarButton);
						if (animMode == true)
						{
							Stop();
							ToggleAnimationMode();
						}
					}
					else
					{
						Color backupCol = GUI.color;
						if (AnimationMode.InAnimationMode()) 
							GUI.color = backupCol * AnimationMode.animatedPropertyColor;
						bool animMode = GUILayout.Toggle (AnimationMode.InAnimationMode(), ms_Styles.recordContent, EditorStyles.toolbarButton);
						GUI.color = backupCol;
						if (animMode != AnimationMode.InAnimationMode())
						{
							ToggleAnimationMode();
						}
						GUI.color = backupCol;
					}

					EditorGUI.EndDisabledGroup ();

					// Animation mode button
					EditorGUI.BeginDisabledGroup (state.IsPrefab);
					SetPlayMode (GUILayout.Toggle (state.m_AnimationIsPlaying, ms_Styles.playContent, EditorStyles.toolbarButton));
					GUILayout.FlexibleSpace ();
					EditorGUI.EndDisabledGroup();

					// Time line buttons
					EditorGUI.BeginDisabledGroup (!state.m_ActiveGameObject || !state.m_ActiveAnimationClip || state.IsPrefab);

					if (GUILayout.Button(ms_Styles.prevKeyContent, EditorStyles.toolbarButton))
						Prev();
					if (GUILayout.Button(ms_Styles.nextKeyContent, EditorStyles.toolbarButton))
						Next();
										
					// Time value input field
					{
						int newFrame = EditorGUILayout.IntField(state.m_Frame, EditorStyles.toolbarTextField, GUILayout.Width(kIntFieldWidth));
						newFrame = Mathf.Max(0, newFrame);
						if (newFrame != state.m_Frame)
							PreviewFrame(newFrame);
					}

					EditorGUI.EndDisabledGroup();

					// Add keyframe button
					EditorGUI.BeginDisabledGroup (!state.IsEditable);

					if (GUILayout.Button (ms_Styles.addKeyframeContent, EditorStyles.toolbarButton) || kAnimationRecordKeyframe.activated)
						if (EnsureAnimationMode())
							AnimationWindowUtility.AddSelectedKeyframes (state, state.time);
				
					// Add event button		
					if (GUILayout.Button (ms_Styles.addEventContent, EditorStyles.toolbarButton))
					{
						if (m_Selected.Length > 0)
						{
							AnimationSelection selection = m_Selected[0];
							if (selection.EnsureClipPresence())
							{
								m_CurveEditor.SelectNone ();
								
								m_AnimationEventTimeLine.AddEvent(state);
								
								SetDirtyCurves();
							}
						}
					}

					EditorGUI.EndDisabledGroup();

				EditorGUILayout.EndHorizontal();

				/******************************************************
				LEFT SIDE: TOOLBAR:  SECOND FROM TOP
				******************************************************/

				EditorGUILayout.BeginHorizontal (EditorStyles.toolbar, GUILayout.Width (hierarchyWidth+1));

				// List of properties
				GUI.changed = false;

				AnimationSelection animationSelection = m_Selected.Length > 0 ? m_Selected[0] : null;
				AnimationSelection.OnGUISelection (animationSelection);

				EditorGUI.BeginDisabledGroup(state.IsReadOnly);
				
				Rect samplesLabelRect = GUILayoutUtility.GetRect (0, 0, ms_Styles.toolbarLabel, GUILayout.Width (kSamplesLabelWidth));
				samplesLabelRect.width += 10;
				GUI.Label (samplesLabelRect, "Samples", ms_Styles.toolbarLabel);

				EditorGUI.BeginChangeCheck ();
				int framerate = EditorGUILayout.IntField ((int)state.frameRate, EditorStyles.toolbarTextField, GUILayout.Width (kIntFieldWidth));
				if (EditorGUI.EndChangeCheck ())
					state.frameRate = framerate;

				EditorGUI.EndDisabledGroup();

				EditorGUILayout.EndHorizontal ();				

				/******************************************************
				LEFT SIDE: HIERARCHY
				******************************************************/

				GUILayoutUtility.GetRect (hierarchyWidth, position.height - kTimeRulerHeight - kEventLineHeight - kSliderThickness);

				if (m_Selected.Length > 0)
					HierarchyGUI (hierarchyWidth, (int)mainContentRect.height - kSliderThickness);
				
				// TODO: Use styles instead
				Handles.color = EditorGUIUtility.isProSkin ? new Color (0.15f, 0.15f, 0.15f) : new Color(0.6f, 0.6f, 0.6f);
				Handles.DrawLine (new Vector2 (hierarchyWidth, 35), new Vector2 (hierarchyWidth, (int)mainContentRect.height - kSliderThickness + 35));
				
				// It's a hack to use GUI.color here to make this toolbar slightly darker. We should create proper styles, but no time for 4.3...
				Color tempColor = GUI.color;
				GUI.color = new Color (0.95f,0.95f,0.95f,1);
				EditorGUILayout.BeginHorizontal (ms_Styles.miniToolbar, GUILayout.Width (hierarchyWidth+1), GUILayout.MinHeight (kSliderThickness));
				// Dummy layout with nice style because we don't want to leave ugly empty space in the bottom of the hierarchy
				GUILayout.FlexibleSpace();
				
				EditorGUI.BeginDisabledGroup (state.m_ActiveAnimationClip == null);
				EditorGUI.BeginChangeCheck();
				GUILayout.Toggle (!state.m_ShowCurveEditor, "Dope Sheet", ms_Styles.miniToolbarButton, GUILayout.Width (80));
				GUILayout.Toggle (state.m_ShowCurveEditor, "Curves", ms_Styles.miniToolbarButton, GUILayout.Width (80));
				if (EditorGUI.EndChangeCheck())
					m_CurveEditorToggleChanged = true;
				EditorGUI.EndDisabledGroup();
				GUI.color = tempColor;
				
				EditorGUILayout.EndHorizontal ();

			EditorGUILayout.EndVertical ();
			// Make sure control ids are found consistently after this point
			// despite controls above only being created sometimes.
			GUIUtility.GetControlID(3487653, FocusType.Passive);

			/******************************************************
			RIGHT SIDE:
			******************************************************/
			GUI.changed = false;
		
			EditorGUILayout.BeginVertical(GUILayout.ExpandWidth(true));
				
				EditorGUILayout.BeginHorizontal("Toolbar");
					GUILayout.FlexibleSpace();
				EditorGUILayout.EndHorizontal();
				
				// Drawing of background colors etc.
				GUI.Label (eventLineRect, GUIContent.none, ms_Styles.eventBackground);
				TimeLineGUI (eventLineRect, true, true);
				if (state.m_ShowCurveEditor)
					GUI.Label (m_CurveEditor.drawRect, GUIContent.none, ms_Styles.curveEditorBackground);

				// Setup curve editor
				m_CurveEditor.rect = curveEditorRect;
				m_CurveEditor.hRangeLocked = Event.current.shift;
				m_CurveEditor.vRangeLocked = EditorGUI.actionKey;
				
				GUI.changed = false;

				// TODO proper handling of invalid range					
				if (state.maxFrame < state.minFrame + 5)
					m_CurveEditor.SetShownHRange (state.minTime, state.minTime + 5.0f / state.frameRate);

				// Curve editor gui
				m_CurveEditor.vSlider = state.m_ShowCurveEditor;
				m_CurveEditor.hSlider = state.m_ShowCurveEditor;

				m_CurveEditor.BeginViewGUI();
				m_CurveEditor.GridGUI();

				if (state.m_ShowCurveEditor)
				{
					if (state.m_ActiveAnimationClip != null)
						DrawEndOfClip (curveEditorRectMinusScrollbars, state.TimeToPixel (state.m_ActiveAnimationClip.length) + curveEditorRectMinusScrollbars.xMin);

					HandlePlayhead (hierarchyWidth, curveEditorRectMinusScrollbars, kCurveEditorPlayheadAlpha);
					EditorGUI.BeginDisabledGroup (state.IsReadOnly);
					m_CurveEditor.CurveGUI ();
					EditorGUI.EndDisabledGroup();
				}
		
				m_CurveEditor.EndViewGUI();
		
				// Actual GUI for time line, events, keyframes
				TimeLineGUI (timeRulerRect, false, false);

				if (state.m_ActiveAnimationClip != null)
					DrawEndOfClip (timeRulerRect, state.TimeToPixel (state.m_ActiveAnimationClip.length) + timeRulerRect.xMin);

				HandlePlayhead (hierarchyWidth, timeRulerRect);
				
				// Event line
				if (m_Selected.Length > 0)
				{
					EditorGUI.BeginDisabledGroup (!state.IsEditable);
					HandlePlayhead (hierarchyWidth, eventLineRect);
					m_AnimationEventTimeLine.EventLineGUI (eventLineRect, m_Selected[0], state, m_CurveEditor);
					EditorGUI.EndDisabledGroup();
				}
		
				// Render DopeSheet
				if (!state.m_ShowCurveEditor)
				{
					DopeSheetGUI (dopeSheetRect);
					
					// TODO bad design, state should handle where time values are read from
					m_CurveEditor.SetShownHRange (state.minTime, state.maxTime);
				}

				// Time slider
				if (m_Selected.Length > 0 && Event.current.button == 0)
				{
					EditorGUI.BeginChangeCheck();
					int newFrame = Mathf.RoundToInt (
						GUI.HorizontalSlider (clickableRightRect, state.m_Frame, state.minFrame, state.maxFrame, GUIStyle.none, GUIStyle.none)
					);

					// Sometimes mousedown is eaten and HorizontalSlider misses it, but we want to still move the playhead with mouseUp.
					if (clickableRightRect.Contains (Event.current.mousePosition) && Event.current.type == EventType.MouseUp)
					{
						newFrame = Mathf.RoundToInt (state.TimeToFrame (state.PixelToTime (Event.current.mousePosition.x - hierarchyWidth, false)));
					}
					newFrame = Mathf.Max (0, newFrame);
					if (EditorGUI.EndChangeCheck())
						PreviewFrame (newFrame);
				}

				// Update curves
				if (m_ShownProperties == null)
					Debug.LogError("m_ShownProperties is null");
				else if (m_CurveEditor.animationCurves.Length != m_EditedCurves.Length)
					Debug.LogError("animationCurves and m_EditedCurves not same length! ("+m_CurveEditor.animationCurves.Length+" vs "+m_EditedCurves.Length+")");
				else if (m_Selected.Length > 0 && m_Selected[0] != null && m_ShownProperties.Length > 0)
				{
					int changedCurves = 0;
					for (int i=0; i<m_EditedCurves.Length; i++)
					{
						if (m_CurveEditor.animationCurves[i] == null)
							Debug.LogError("Curve "+i+" is null");
						
						CurveState curveState = m_ShownProperties[m_EditedCurves[i]];
						
						if (!m_CurveEditor.animationCurves[i].changed)
							continue;
						
						m_CurveEditor.animationCurves[i].changed = false;
						changedCurves++;
						
						if (EnsureAnimationMode())
							curveState.SaveCurve(m_CurveEditor.animationCurves[i].curve);
					}
					if (changedCurves > 0)
					{
						SetDirtyCurves();
					}
				}

		EditorGUILayout.EndVertical();

		SplitterGUILayout.EndHorizontalSplit();

		HandleZoomingOutsideTimearea (timeRulerAndEventLineRect);
		
		AnimationControls();

		// Remove various focus when clicking any random place
		Rect win = position;
		win.x = 0; win.y = 0;
		if (Event.current.type == EventType.MouseDown
			&& win.Contains(Event.current.mousePosition)
			&& Event.current.button == 0
			&& !Event.current.shift && !Event.current.control)
		{
			// Remove keyboard focus when clicking anywhere inside animation window
			GUIUtility.keyboardControl = 0;
			
			if (Event.current.mousePosition.x < hierarchyWidth
				&& Event.current.mousePosition.y > kTimeRulerHeight+kAnimationHeight)
			{
				for (int i=0; i<m_SelectedProperties.Length; i++)
					m_SelectedProperties[i] = false;
					
				SetDirtyCurves();
			}
			
			Event.current.Use();
		}
		
		m_AnimationEventTimeLine.DrawInstantTooltip(position);
	}

	public static void DrawEndOfClip (Rect rect, float endOfClipPixel)
	{
		Rect dimmedRect = new Rect (Mathf.Max (endOfClipPixel, rect.xMin), rect.yMin, rect.width, rect.height);
		Vector3[] dimmed = new Vector3[4];
		dimmed[0] = new Vector3 (dimmedRect.xMin, dimmedRect.yMin);
		dimmed[1] = new Vector3 (dimmedRect.xMax, dimmedRect.yMin);
		dimmed[2] = new Vector3 (dimmedRect.xMax, dimmedRect.yMax);
		dimmed[3] = new Vector3 (dimmedRect.xMin, dimmedRect.yMax);

		Color backgroundColor = EditorGUIUtility.isProSkin ? Color.black.AlphaMultiplied (0.32f) : Color.gray.AlphaMultiplied (0.42f);
		Color lineColor = EditorGUIUtility.isProSkin ? Color.white.RGBMultiplied (0.32f) : Color.white.RGBMultiplied (0.4f);

		DrawRect (dimmed, backgroundColor);
		DrawLine (dimmed[0], dimmed[3] + new Vector3(0f, -1f, 0f), lineColor);
	}

	private void HandlePlayhead (float hierarchyWidth, Rect r)
	{
		HandlePlayhead (hierarchyWidth, r, 1f);
	}

	private void HandlePlayhead (float hierarchyWidth, Rect r, float alpha)
	{
		// Draw playhead (red line)
		if (AnimationMode.InAnimationMode () && state.m_Frame >= state.minFrame - 1 && state.m_Frame < state.maxFrame)
		{
			float timePixel = state.TimeToPixel (state.GetTimeSeconds (), false) + hierarchyWidth;

			// Clip it from going over hierarchy
			if (r.xMin < timePixel)
				DrawPlayHead (new Vector2 (timePixel, r.yMin), new Vector2 (timePixel, r.yMax), alpha);
		}
	}

	public static void DrawPlayHead(Vector2 start, Vector2 end, float alpha)
	{
		DrawLine (start, end, Color.red.AlphaMultiplied (alpha));
	}

	private static void DrawLine (Vector2 p1, Vector2 p2, Color color)
	{
		HandleUtility.handleWireMaterial.SetPass (0);
		GL.PushMatrix ();
		GL.MultMatrix (Handles.matrix); 
		GL.Begin (GL.LINES);
		GL.Color (color);
		GL.Vertex (p1);
		GL.Vertex (p2);
		GL.End ();
		GL.PopMatrix ();
	}

	private static void DrawRect (Vector3[] corners, Color color)
	{
		if (corners.Length != 4)
			return;

		HandleUtility.handleWireMaterial.SetPass (0);
		GL.PushMatrix ();
		GL.MultMatrix (Handles.matrix);
		GL.Begin (GL.QUADS);
		GL.Color (color);
		GL.Vertex (corners[0]);
		GL.Vertex (corners[1]);
		GL.Vertex (corners[2]);
		GL.Vertex (corners[3]);
		GL.End ();
		GL.PopMatrix ();
	}

	private void HandleZoomingOutsideTimearea (Rect area)
	{
		if (state.m_ShowCurveEditor)
			m_CurveEditor.HandleZoomAndPanEvents (area);
		else
			m_DopeSheetEditor.HandleZoomAndPanEvents (area);
	}

	// If no dopelines are active, make sure there is something to show (new users get confused otherwise).
	private void HandleEmptyCurveEditor ()
	{
		if (state.m_ShowCurveEditor && state.activeCurves.Count == 0 && state.dopelines.Count > 0)
		{
			state.SelectHierarchyItem (state.dopelines[0], false, false);
		}
	}

	void InitAllViews()
	{
		if (m_Hierarchy == null)
			m_Hierarchy = new AnimationWindowHierarchy(state, this, new Rect(0,0,1,1));

		if (m_DopeSheetEditor == null)
		{
			m_DopeSheetEditor = new DopeSheetEditor (state, this);
		}
		
		if (m_AnimationEventTimeLine == null)
			m_AnimationEventTimeLine = new AnimationEventTimeLine(this);
	}

	void HierarchyGUI (int hierarchyWidth, int hierarchyHeight)
	{
		// Animation selection has changed
		if (GUI.changed)
		{
			RefreshAnimationSelections();
		}

		Rect r = new Rect (0, 35, hierarchyWidth, hierarchyHeight);
		m_Hierarchy.OnGUI (r, this);		

		EditorGUIUtility.SetIconSize (Vector2.zero);
	}

	/******************************************************
	INTERNAL EVENTS
	******************************************************/

	private void DebugTime (string str, float timeA, float timeB)
	{
		Debug.Log (str + " took " + (timeB - timeA));
	}

	public void OnSelectionChange ()
	{
		// Zoom view both horizontally and vertically when selection is actually changed
		if (state != null && (Selection.activeGameObject && Selection.activeGameObject != state.m_ActiveGameObject))
		{			
			m_PerformFrameSelectedOnCurveEditorHorizontally = true;
			FrameSelected ();
		}

		InitSelection ();
	}

	private void SetGridColors ()
	{
		CurveEditorSettings settings = new CurveEditorSettings ();
		settings.hTickStyle.distMin = 30; // min distance between vertical lines before they disappear completely
		settings.hTickStyle.distFull = 80; // distance between vertical lines where they gain full strength
		settings.hTickStyle.distLabel = 0; // min distance between vertical lines labels
		if (EditorGUIUtility.isProSkin)
		{
			settings.vTickStyle.color = new Color (1, 1, 1, settings.vTickStyle.color.a); // color and opacity of horizontal lines
			settings.vTickStyle.labelColor = new Color (1, 1, 1, settings.vTickStyle.labelColor.a); // color and opacity of horizontal line labels
		}
		settings.vTickStyle.distMin = 15; // min distance between horizontal lines before they disappear completely
		settings.vTickStyle.distFull = 40; // distance between horizontal lines where they gain full strength
		settings.vTickStyle.distLabel = 30; // min distance between horizontal lines labels
		settings.vTickStyle.stubs = true;
		settings.hRangeMin = 0;
		settings.hRangeLocked = false;
		settings.vRangeLocked = false;

		m_CurveEditor.settings = settings;
	}

	void AnimationControls ()
	{
		Event evt = Event.current;

		switch (evt.type)
		{
			case EventType.keyDown:
				if (kAnimationPrevFrame.activated)
				{
					PreviewFrame (Mathf.Max (0, state.m_Frame - 1));
					evt.Use ();
				}
				if (kAnimationNextFrame.activated)
				{
					PreviewFrame (state.m_Frame + 1);
					evt.Use ();
				}
				if (kAnimationPrevKeyframe.activated)
				{
					Prev ();
					evt.Use ();
				}
				if (kAnimationNextKeyframe.activated)
				{
					Next ();
					evt.Use ();
				}
				
				if (kAnimationRecordKeyframe.activated)
				{
					AnimationWindowUtility.AddSelectedKeyframes (state, state.time);
					evt.Use ();
				}
				
				if (kAnimationShowCurvesToggle.activated)
				{
					m_CurveEditorToggleChanged = true;
					evt.Use ();
				}
				break;
			case EventType.keyUp:

				break;
		}
	}

	/******************************************************
	EXTERNAL EVENTS
	******************************************************/
				
	public AnimationWindow ()
	{
	}

	public void Awake ()
	{
		if (state == null)
		{
			state = new AnimationWindowState();

			// Show dopesheet as default
			state.m_ShowCurveEditor = false;
			state.m_AnimationWindow = this;
		}
		// Force dopesheet or curveEditor to update their zoom state
		state.timeArea = null;

		minSize = new Vector2 (400, 200);
		m_HorizontalSplitter = new SplitterState (new float[] { 250, 10000 }, new int[] { 250, 150 }, null);
		m_HorizontalSplitter.realSizes[0] = 300;
		
		wantsMouseMove = true;

		m_Selected = new AnimationSelection[0];
		if (m_ExpandedFoldouts == null)
			m_ExpandedFoldouts = new SerializedStringTable ();
		if (m_ChosenAnimated == null)
			m_ChosenAnimated = new SerializedStringTable ();
		if (m_ChosenClip == null)
			m_ChosenClip = new SerializedStringTable ();

		m_CurveEditor = new CurveEditor (
			new Rect (position.x, position.y, 500, 200),
			new CurveWrapper[] { },
			false
		);

		// Curve editor style settings
		SetGridColors ();

		m_CurveEditor.m_TimeUpdater = this;
		m_CurveEditor.m_DefaultBounds = new Bounds (new Vector3 (1, 1, 0), new Vector3 (2, 1000, 0));
		m_CurveEditor.SetShownHRangeInsideMargins (0, 2);

		m_CurveEditor.hTicks.SetTickModulosForFrameRate (state.frameRate);
		
		InitAllViews();
		InitSelection ();
	}

	public void OnEnable ()
	{
		s_AnimationWindows.Add (this);

		//Editor.onSceneGUIDelegate += DrawClipsInSceneView;
		Undo.undoRedoPerformed += UndoRedoPerformed;
	
		if (state != null)
			state.OnEnable (this);

		state.m_OnHierarchySelectionChanged += FrameSelected;

		// Only run code below on script Reloads - not the first time the script is enabled.
		if (m_Selected == null)
			return;
		
		if (m_CurveEditor != null)
		{
			m_CurveEditor.m_TimeUpdater = this;
			SetGridColors();
			m_CurveEditor.OnEnable();
		}
		
		if (m_AutoRecord)
			Undo.postprocessModifications += PostprocessAnimationRecordingModifications;

		state.m_ShowCurveEditor = s_LastShowCurveEditor;

		SetDirtyCurves();
	}
	
	public void OnDisable () 
	{
		s_AnimationWindows.Remove (this);

		if (state != null)
			state.OnDisable ();

		s_LastShowCurveEditor = state.m_ShowCurveEditor;

		//Editor.onSceneGUIDelegate -= DrawClipsInSceneView;
		Undo.undoRedoPerformed -= UndoRedoPerformed;
		Undo.postprocessModifications -= PostprocessAnimationRecordingModifications;
		state.m_OnHierarchySelectionChanged -= FrameSelected;
		m_CurveEditor.OnDisable();	
	}

	public void OnDestroy ()
	{
		EndAnimationMode();
		AnimationEventPopup.ClosePopup();
		CurveRendererCache.ClearCurveRendererCache();

		if (m_DopeSheetEditor != null)
			m_DopeSheetEditor.OnDestroy();
	}

	public void DrawClipsInSceneView ()
	{
		//Handles.DrawLine(Vector3.zero, Vector3.one);
	}
}

} // namespace
