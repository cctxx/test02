using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor 
{

internal class GradientPicker : EditorWindow
{
	private static GradientPicker s_GradientPicker;
	public static string presetsEditorPrefID { get { return "Gradient"; } }

	private GradientEditor m_GradientEditor;
	private PresetLibraryEditor<GradientPresetLibrary> m_GradientLibraryEditor;
	[SerializeField]
	private PresetLibraryEditorState m_GradientLibraryEditorState;
	private Gradient m_Gradient;
	private const int k_DefaultNumSteps = 0;
	private GUIView m_DelegateView;
	private bool gradientChanged {get; set;}

	// Static methods
	public static void Show(Gradient newGradient)
	{
		GUIView currentView = GUIView.current;
		if (s_GradientPicker == null)
		{
			s_GradientPicker = (GradientPicker)GetWindow(typeof(GradientPicker), true, "Gradient Editor", false);
			Vector2 minSize = new Vector2(360, 224);
			Vector2 maxSize = new Vector2(1900, 3000);
			s_GradientPicker.minSize = minSize;
			s_GradientPicker.maxSize = maxSize;
			s_GradientPicker.wantsMouseMove = true;
			s_GradientPicker.ShowAuxWindow(); // Use this if auto close on lost focus is wanted.
		}
		else
		{
			s_GradientPicker.Repaint (); // Ensure we get a OnGUI so we refresh if new gradient
		}
		s_GradientPicker.m_DelegateView = currentView;
		s_GradientPicker.Init (newGradient);
	}

	public static GradientPicker instance
	{
		get
		{
			if (!s_GradientPicker)
				Debug.LogError("Gradient Picker not initalized, did you call Show first?");
			return s_GradientPicker;
		}
	}

	GradientPicker ()
	{
		hideFlags = HideFlags.DontSave;
	}
	
	public string currentPresetLibrary
	{
		get
		{
			InitIfNeeded();
			return m_GradientLibraryEditor.currentLibraryWithoutExtension;
		}
		set
		{
			InitIfNeeded();
			m_GradientLibraryEditor.currentLibraryWithoutExtension = value;
		}
	}

	private void Init (Gradient newGradient)
	{
		m_Gradient = newGradient;
		if (m_GradientEditor != null)
			m_GradientEditor.Init (newGradient, k_DefaultNumSteps);
		Repaint ();
	}

	private void SetGradientData (Gradient gradient)
	{
		m_Gradient.colorKeys = gradient.colorKeys;
		m_Gradient.alphaKeys = gradient.alphaKeys;
		Init (m_Gradient);
	}

	public static bool visible
	{
		get { return s_GradientPicker != null; }
	}
	
	public static Gradient gradient
	{
		get
		{
			if (s_GradientPicker != null)
				return s_GradientPicker.m_Gradient;
			return null;
		}
	}

	public void OnEnable()
	{
		// Use these if window is not an aux window for auto closing on play/stop
		//EditorApplication.playmodeStateChanged += OnPlayModeStateChanged;
	}

	public void OnDisable()
	{
		//EditorApplication.playmodeStateChanged -= OnPlayModeStateChanged;
		if (m_GradientLibraryEditorState != null)
			m_GradientLibraryEditorState.TransferEditorPrefsState (false);

		if (m_GradientEditor != null)
			m_GradientEditor.Clear ();
		s_GradientPicker = null;
	}

	public void OnDestroy ()
	{
		m_GradientLibraryEditor.UnloadUsedLibraries();
	}

	void OnPlayModeStateChanged()
	{
		Close ();
	}

	void InitIfNeeded ()
	{
		// Init editor when needed
		if (m_GradientEditor == null)
		{
			m_GradientEditor = new GradientEditor();
			m_GradientEditor.Init (m_Gradient, k_DefaultNumSteps);
		}

		if (m_GradientLibraryEditorState ==  null)
		{
			m_GradientLibraryEditorState = new PresetLibraryEditorState(presetsEditorPrefID);
			m_GradientLibraryEditorState.TransferEditorPrefsState (true);
		}

		if (m_GradientLibraryEditor == null)
		{	
			var saveLoadHelper = new ScriptableObjectSaveLoadHelper<GradientPresetLibrary> ("gradients", SaveType.Text);
			m_GradientLibraryEditor = new PresetLibraryEditor<GradientPresetLibrary> (saveLoadHelper, m_GradientLibraryEditorState, PresetClickedCallback);
			m_GradientLibraryEditor.showHeader = true;
			m_GradientLibraryEditor.minMaxPreviewHeight = new Vector2(14f, 14f);
		}
	}


	void PresetClickedCallback (int clickCount, object presetObject)
	{
		Gradient gradient = presetObject as Gradient;
		if (gradient == null)
			Debug.LogError ("Incorrect object passed " + presetObject);

		SetCurrentGradient (gradient);
		gradientChanged = true;
	}
	
	public void OnGUI()
	{
		// When we start play (using shortcut keys) we get two OnGui calls and m_Gradient is null: so early out.
		if (m_Gradient == null)
			return;

		InitIfNeeded ();
	
		float gradientEditorHeight = Mathf.Min(position.height, 120);
		float distBetween = 10f;
		float presetLibraryHeight = position.height - gradientEditorHeight - distBetween;

		Rect gradientEditorRect = new Rect (10, 10, position.width - 20, gradientEditorHeight - 20);
		Rect gradientLibraryRect = new Rect (0, gradientEditorHeight + distBetween, position.width, presetLibraryHeight);

		// Separator
		EditorGUI.DrawRect(new Rect(gradientLibraryRect.x, gradientLibraryRect.y - 1, gradientLibraryRect.width, 1), new Color(0, 0, 0, 0.3f));
		EditorGUI.DrawRect(new Rect(gradientLibraryRect.x, gradientLibraryRect.y, gradientLibraryRect.width, 1), new Color(1, 1, 1, 0.1f));		

		// The meat
		EditorGUI.BeginChangeCheck ();
		m_GradientEditor.OnGUI(gradientEditorRect);
		if (EditorGUI.EndChangeCheck ())
			gradientChanged	= true;
		m_GradientLibraryEditor.OnGUI (gradientLibraryRect, m_Gradient);
		if (gradientChanged)
		{
			gradientChanged = false;
			SendEvent (true);
		}
	}

	void SendEvent (bool exitGUI)
	{
		if (m_DelegateView)
		{
			Event e = EditorGUIUtility.CommandEvent("GradientPickerChanged");
			Repaint();
			m_DelegateView.SendEvent(e);
			if (exitGUI)
				GUIUtility.ExitGUI();
		}
	}




	public static void SetCurrentGradient (Gradient gradient)
	{
		if (s_GradientPicker == null)
			return;

		s_GradientPicker.SetGradientData (gradient);
		GUI.changed = true;
	}

	public static void CloseWindow ()
	{
		if (s_GradientPicker == null)
			return;

		s_GradientPicker.Close ();
		GUIUtility.ExitGUI ();
	}

	public static void RepaintWindow ()
	{
		if (s_GradientPicker == null)
			return;
		s_GradientPicker.Repaint ();
	}
}

    
} // namespace
