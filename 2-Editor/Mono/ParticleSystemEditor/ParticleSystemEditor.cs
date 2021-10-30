using UnityEngine;
using NUnit.Framework;

namespace UnityEditor
{

[CustomEditor(typeof(ParticleSystem))]
//[CanEditMultipleObjects]
internal class ParticleSystemInspector : Editor, ParticleEffectUIOwner
{
	ParticleEffectUI m_ParticleEffectUI;
	GUIContent m_PreviewTitle = new GUIContent ("Particle System Curves");
	GUIContent showWindowText = new GUIContent("Open Editor...");
	GUIContent closeWindowText = new GUIContent("Close Editor");
	GUIContent hideWindowText = new GUIContent("Hide Editor");
	static GUIContent m_PlayBackTitle;


	public static GUIContent playBackTitle
	{
		get
		{
			if (m_PlayBackTitle == null)
				m_PlayBackTitle = new GUIContent("Particle Effect");
			return m_PlayBackTitle;
		}
	}

	public void OnEnable()
	{
		// Get notified when hierarchy- or project window has changes so we can detect if particle systems have been dragged in or out.
		EditorApplication.hierarchyWindowChanged += HierarchyOrProjectWindowWasChanged;
		EditorApplication.projectWindowChanged += HierarchyOrProjectWindowWasChanged;
		SceneView.onSceneGUIDelegate += OnSceneViewGUI;
		Undo.undoRedoPerformed += UndoRedoPerformed;
	}


	public void OnDisable()
	{
		SceneView.onSceneGUIDelegate -= OnSceneViewGUI;
		EditorApplication.projectWindowChanged -= HierarchyOrProjectWindowWasChanged;
		EditorApplication.hierarchyWindowChanged -= HierarchyOrProjectWindowWasChanged;
		Undo.undoRedoPerformed -= UndoRedoPerformed;
	
		if (m_ParticleEffectUI != null)
			m_ParticleEffectUI.Clear ();
	}

	private void HierarchyOrProjectWindowWasChanged()
	{
		if (ShouldShowInspector ())
			Init (true);
	}

	void UndoRedoPerformed()
	{
		if (m_ParticleEffectUI != null)
			m_ParticleEffectUI.UndoRedoPerformed ();
	}

	private void Init (bool forceInit)
	{
		ParticleSystem t = target as ParticleSystem;
		if (t == null)
			return;

		if (m_ParticleEffectUI == null)
		{
			m_ParticleEffectUI = new ParticleEffectUI(this);
			m_ParticleEffectUI.InitializeIfNeeded(t);
		}
		else if (forceInit)
		{
			m_ParticleEffectUI.InitializeIfNeeded(t);
		}
	}

	void ShowEdiorButtonGUI ()
	{
		GUILayout.BeginHorizontal();
		{
			GUILayout.FlexibleSpace();

			GUIContent text = null;
			ParticleSystemWindow window = ParticleSystemWindow.GetInstance();
			if (window && window.IsVisible())
			{
				if (window.GetNumTabs() > 1)
					text = hideWindowText;
				else
					text = closeWindowText;
			}
			else
				text = showWindowText;

			if (GUILayout.Button(text, EditorStyles.miniButton, GUILayout.Width(110)))
			{
				if (window)
				{
					if (window.IsVisible())
					{
						// Hide
						if (!window.ShowNextTabIfPossible())
							window.Close();
					}
					else
					{
						// Show
						window.Focus();
					}
				}
				else
				{
					// Kill inspector gui first to ensure playback time is cached properly
					Clear ();
					 
					ParticleSystemWindow.CreateWindow();
					GUIUtility.ExitGUI();
				}
			}
		}
		GUILayout.EndHorizontal();	
	}
	
	public override bool UseDefaultMargins () { return false; }
	
	public override void OnInspectorGUI ()
	{
		EditorGUILayout.BeginVertical (EditorStyles.inspectorDefaultMargins);
		
		ShowEdiorButtonGUI ();
		
		if (ShouldShowInspector ())
		{
			if (m_ParticleEffectUI == null)
				Init (true);
			
			EditorGUILayout.EndVertical ();
			EditorGUILayout.BeginVertical (EditorStyles.inspectorFullWidthMargins);
			
			m_ParticleEffectUI.OnGUI();
			
			EditorGUILayout.EndVertical ();
			EditorGUILayout.BeginVertical (EditorStyles.inspectorDefaultMargins);
		}
		else
		{
			Clear ();
		}
		
		EditorGUILayout.EndVertical ();
	}

	void Clear()
	{
		if (m_ParticleEffectUI != null)
			m_ParticleEffectUI.Clear();
		m_ParticleEffectUI = null;	
	}

	private bool ShouldShowInspector ()
	{
		// Only show the inspector GUI if we are not showing the ParticleSystemWindow
		ParticleSystemWindow window = ParticleSystemWindow.GetInstance ();
		return !window || !window.IsVisible();
	}


	public void OnSceneGUI ()
	{
		if (ShouldShowInspector())
		{
			if (m_ParticleEffectUI != null)
				m_ParticleEffectUI.OnSceneGUI();
		}
	}


	public void OnSceneViewGUI (SceneView sceneView)
	{
		if (ShouldShowInspector ())
		{
			Init(false); // Here because can be called before inspector GUI so to prevent blinking GUI when selecting ps.
			if (m_ParticleEffectUI != null)
			{
				m_ParticleEffectUI.OnSceneViewGUI ();
			}
		}
	}

	public override bool HasPreviewGUI ()
	{
		return ShouldShowInspector () && Selection.objects.Length == 1; // Do not show multiple curve editors
	}

	public override void OnPreviewGUI(Rect r, GUIStyle background)
	{
		if (m_ParticleEffectUI != null)
			m_ParticleEffectUI.GetParticleSystemCurveEditor().OnGUI (r);
	}

	public override GUIContent GetPreviewTitle ()
	{
		return m_PreviewTitle;
	}

	public override void OnPreviewSettings()
	{
		return;
	}
}

} // namespace UnityEditor
