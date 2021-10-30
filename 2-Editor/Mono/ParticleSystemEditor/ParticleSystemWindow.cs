using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using System.IO;

namespace UnityEditor
{

internal class  ParticleSystemWindow : EditorWindow, ParticleEffectUIOwner
{
	static ParticleSystemWindow s_Instance;

	ParticleSystem m_Target;
	ParticleEffectUI m_ParticleEffectUI;
	bool m_IsVisible;

	static GUIContent[] s_Icons = {
		EditorGUIUtility.IconContent ("HorizontalSplit"),
		EditorGUIUtility.IconContent ("VerticalSplit")
	};

	class Texts
	{
		public GUIContent lockParticleSystem = new GUIContent("", "Lock the current selected Particle System");
		public GUIContent previewAll = new GUIContent("Simulate All", "Simulate all particle systems that have Play On Awake set");
	}
	static Texts s_Texts;

	static public void CreateWindow()
	{
		s_Instance = EditorWindow.GetWindow<ParticleSystemWindow>();
		s_Instance.title = "Particle Effect";
		s_Instance.minSize = ParticleEffectUI.GetMinSize();
	}

	internal static ParticleSystemWindow GetInstance ()
	{
		return s_Instance;
	}

	internal bool IsVisible()
	{
		return m_IsVisible;
	}

	// Prevent created from outside
	private ParticleSystemWindow()
	{
	}
	
	void OnEnable()
	{
		s_Instance = this;

		m_Target = null; // Ensure we recreate EffectUI after script reloads
		ParticleEffectUI.m_VerticalLayout = EditorPrefs.GetBool("ShurikenVerticalLayout", false);

		// Get notified when hierarchy- or project window has changes so we can detect if particle systems have been dragged in or out.
		EditorApplication.hierarchyWindowChanged += OnHierarchyOrProjectWindowWasChanged;
		EditorApplication.projectWindowChanged += OnHierarchyOrProjectWindowWasChanged;
		SceneView.onSceneGUIDelegate += OnSceneViewGUI;
		EditorApplication.playmodeStateChanged += OnPlayModeStateChanged;
		Undo.undoRedoPerformed += UndoRedoPerformed;
		
		autoRepaintOnSceneChange = false;
	}

	void OnDisable()
	{
		ParticleSystemEditorUtils.editorUpdateAll = false;

		SceneView.onSceneGUIDelegate -= OnSceneViewGUI;
		EditorApplication.projectWindowChanged -= OnHierarchyOrProjectWindowWasChanged;
		EditorApplication.hierarchyWindowChanged -= OnHierarchyOrProjectWindowWasChanged;
		EditorApplication.playmodeStateChanged -= OnPlayModeStateChanged;
		Undo.undoRedoPerformed -= UndoRedoPerformed;
		

		Clear ();

		if (s_Instance == this)
			s_Instance = null;

	}

	void Clear()
	{
		m_Target = null;
		if (m_ParticleEffectUI != null)
		{
			m_ParticleEffectUI.Clear();
			m_ParticleEffectUI = null;
		}	
	}

	void OnPlayModeStateChanged ()
	{
		// Need to refresh because UpdateAll changes state
		Repaint ();
	}

	void UndoRedoPerformed ()
	{
		if (m_ParticleEffectUI != null)
			m_ParticleEffectUI.UndoRedoPerformed();
		Repaint ();
	}

	private void OnHierarchyOrProjectWindowWasChanged()
	{
		InitEffectUI ();
	}

	void OnBecameVisible()
	{
		if (m_IsVisible) 
			return;

		m_IsVisible = true;

		InitEffectUI();
		
		SceneView.RepaintAll();
		InspectorWindow.RepaintAllInspectors ();
	}

	void OnBecameInvisible()
	{
		m_IsVisible = false;

		ParticleSystemEditorUtils.editorUpdateAll = false;

		Clear();

		SceneView.RepaintAll();
		InspectorWindow.RepaintAllInspectors();
	}

	void OnSelectionChange ()
	{
		InitEffectUI ();
		Repaint();
	}

	void InitEffectUI ()
	{
		if (!m_IsVisible)
			return;

		// Use locked particle system if set otherwise check selected gameobject
		ParticleSystem target = ParticleSystemEditorUtils.lockedParticleSystem; 
		if (target == null && Selection.activeGameObject != null)
			target = Selection.activeGameObject.GetComponent<ParticleSystem>();

		m_Target = target;
		if (m_Target != null)
		{
			if (m_ParticleEffectUI == null)
				m_ParticleEffectUI = new ParticleEffectUI (this);

			if (m_ParticleEffectUI.InitializeIfNeeded(m_Target))
				Repaint();
		}
		
		// Cleanup if needed
		if (m_Target == null && m_ParticleEffectUI != null)
		{
			Clear();
			Repaint();
			SceneView.RepaintAll (); 
			GameView.RepaintAll ();
		}
	}
	
	void Awake ()
	{
	}

	void DoToolbarGUI()
	{
		GUILayout.BeginHorizontal("Toolbar");

		EditorGUI.BeginDisabledGroup(m_ParticleEffectUI == null);
		EditorGUI.BeginDisabledGroup(ParticleSystemEditorUtils.editorUpdateAll);

		if (!EditorApplication.isPlaying)
		{
			bool isPlaying = false;

			if (m_ParticleEffectUI != null)
			{
				isPlaying = m_ParticleEffectUI.IsPlaying();
			}

			if (GUILayout.Button(isPlaying ? ParticleEffectUI.texts.pause : ParticleEffectUI.texts.play, "ToolbarButton", GUILayout.Width(65)))
			{
				if (m_ParticleEffectUI != null)
				{
					if (isPlaying)
						m_ParticleEffectUI.Pause();
					else
						m_ParticleEffectUI.Play();
				}
				Repaint(); // we switch texts
			}

			if (GUILayout.Button(ParticleEffectUI.texts.stop, "ToolbarButton"))
				if (m_ParticleEffectUI != null)
					m_ParticleEffectUI.Stop();
		}
		else
		{
			// In play mode we have pulse play behavior
			if (GUILayout.Button(ParticleEffectUI.texts.play, "ToolbarButton", GUILayout.Width(65)))
			{
				if (m_ParticleEffectUI != null)
				{
					m_ParticleEffectUI.Stop();
					m_ParticleEffectUI.Play();
				}
			}
			if (GUILayout.Button(ParticleEffectUI.texts.stop, "ToolbarButton"))
			{
				if (m_ParticleEffectUI != null)
					m_ParticleEffectUI.Stop();
			}
		}
		
		GUILayout.FlexibleSpace();

		bool isShowOnlySelected = m_ParticleEffectUI != null ? m_ParticleEffectUI.IsShowOnlySelectedMode() : false;
		bool newState = GUILayout.Toggle(isShowOnlySelected, isShowOnlySelected ? "Show: Selected" : "Show: All", ParticleSystemStyles.Get().toolbarButtonLeftAlignText, GUILayout.Width(100));
			if (newState != isShowOnlySelected && m_ParticleEffectUI != null)
				m_ParticleEffectUI.SetShowOnlySelectedMode (newState);	

		// Resimulation toggle
		ParticleSystemEditorUtils.editorResimulation = GUILayout.Toggle(ParticleSystemEditorUtils.editorResimulation, ParticleEffectUI.texts.resimulation, "ToolbarButton");

		// Wireframe toggle
		ParticleEffectUI.m_ShowWireframe = GUILayout.Toggle(ParticleEffectUI.m_ShowWireframe, ParticleEffectUI.texts.wireframe, "ToolbarButton");

		// Editor layout
		if (GUILayout.Button(ParticleEffectUI.m_VerticalLayout ? s_Icons[0] : s_Icons[1], "ToolbarButton"))
		{
			ParticleEffectUI.m_VerticalLayout = !ParticleEffectUI.m_VerticalLayout;
			EditorPrefs.SetBool("ShurikenVerticalLayout", ParticleEffectUI.m_VerticalLayout);
		}

		// Lock toggle
		GUILayout.BeginVertical();
			GUILayout.Space(3);
			ParticleSystem lockedParticleSystem = ParticleSystemEditorUtils.lockedParticleSystem;
			bool isLocked = lockedParticleSystem != null;
			bool newLocked = GUILayout.Toggle(isLocked, s_Texts.lockParticleSystem, "IN LockButton");
			if (isLocked != newLocked)
			{
				if (m_ParticleEffectUI != null && m_Target != null)
				{
					if (newLocked)
						ParticleSystemEditorUtils.lockedParticleSystem = m_Target;
					else
						ParticleSystemEditorUtils.lockedParticleSystem = null;
				}
			}
		GUILayout.EndVertical();

		EditorGUI.EndDisabledGroup(); // ParticleSystemEditorUtils.editorUpdateAll
		EditorGUI.EndDisabledGroup(); // ParticleEffectUI == null

		GUILayout.EndHorizontal();
	}

	void OnGUI ()
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		if (m_Target == null && (Selection.activeGameObject != null || ParticleSystemEditorUtils.lockedParticleSystem != null))
		{
			InitEffectUI ();
		}
		
		DoToolbarGUI ();

		EditorGUI.BeginDisabledGroup(ParticleSystemEditorUtils.editorUpdateAll);
			if (m_ParticleEffectUI != null)
				m_ParticleEffectUI.OnGUI();
		EditorGUI.EndDisabledGroup();
	}



	public void OnSceneViewGUI (SceneView sceneView)
	{
		if (!m_IsVisible)
			return;

		// We do not render SceneView gui when showing all particle systems
		if (ParticleSystemEditorUtils.editorUpdateAll)
			return;

		if (m_ParticleEffectUI != null)
		{
			m_ParticleEffectUI.OnSceneGUI();
			m_ParticleEffectUI.OnSceneViewGUI();
		}
	}


	void OnDidOpenScene ()
	{
		Repaint();
	}

} // ParticleSystemWindow

} // UnityEditor
