using UnityEngine;
using System.Collections.Generic;


namespace UnityEditor
{
abstract partial class ModuleUI : SerializedModule
{
	public ParticleSystemUI m_ParticleSystemUI; // owner
	private string m_DisplayName;
	protected string m_ToolTip = "";
	private SerializedProperty m_Enabled;
	private VisibilityState m_VisibilityState;
	public List<SerializedProperty> m_ModuleCurves = new List<SerializedProperty>();
	private List<SerializedProperty> m_CurvesRemovedWhenFolded = new List<SerializedProperty>();
	
	public enum VisibilityState
	{
		NotVisible = 0,
		VisibleAndFolded = 1,
		VisibleAndFoldedOut = 2
	};

	public bool visibleUI {
		get { return m_VisibilityState != VisibilityState.NotVisible;}
		set { SetVisibilityState (value ? VisibilityState.VisibleAndFolded : VisibilityState.NotVisible);} 
	}
	public bool foldout
	{
		get { return m_VisibilityState == VisibilityState.VisibleAndFoldedOut; }
		set { SetVisibilityState(value ? VisibilityState.VisibleAndFoldedOut : VisibilityState.VisibleAndFolded);}
	}

	public bool enabled {	
		get
		{
			return m_Enabled.boolValue;
		}
		set
		{
			if (m_Enabled.boolValue != value)
			{
				m_Enabled.boolValue = value;
				if (value)
					OnModuleEnable ();
				else
					OnModuleDisable ();
			}
		}
	}

	public string displayName
	{
		get { return m_DisplayName; }
	}

	public string toolTip
	{
		get { return m_ToolTip; }
	}


	public ModuleUI(ParticleSystemUI owner, SerializedObject o, string name, string displayName)
		: base(o, name)
	{
		Setup (owner, o, name, displayName, VisibilityState.NotVisible);
	}
	public ModuleUI(ParticleSystemUI owner, SerializedObject o, string name, string displayName, VisibilityState initialVisibilityState) 
		: base(o, name)
	{
		Setup (owner, o, name, displayName, initialVisibilityState);
	}

	private void Setup (ParticleSystemUI owner, SerializedObject o, string name, string displayName, VisibilityState defaultVisibilityState)
	{
		m_ParticleSystemUI = owner;
		m_DisplayName = displayName;

		if (this is RendererModuleUI)
			m_Enabled = GetProperty0 ("m_Enabled");
		else
			m_Enabled = GetProperty ("enabled");

		m_VisibilityState = (VisibilityState)InspectorState.GetInt(GetUniqueModuleName(), (int)defaultVisibilityState);
		CheckVisibilityState ();

		if (foldout)
			Init();
	}


	protected abstract void Init ();
	public virtual void Validate () {}
	public virtual float GetXAxisScalar () {return 1f;}
	public abstract void OnInspectorGUI(ParticleSystem s);
	public virtual void OnSceneGUI(ParticleSystem s, InitialModuleUI initial) { }
	public virtual void UpdateCullingSupportedString(ref string text) { }

	protected virtual void OnModuleEnable () 
	{
		Init (); // ensure initialized
	}
	
	protected virtual void OnModuleDisable ()
	{
		ParticleSystemCurveEditor psce = m_ParticleSystemUI.m_ParticleEffectUI.GetParticleSystemCurveEditor();
		foreach (SerializedProperty curveProp in m_ModuleCurves)
		{
			if (psce.IsAdded (curveProp))
				psce.RemoveCurve(curveProp);
		}
	}

	internal void CheckVisibilityState ()
	{
		bool isRendererModule = this is RendererModuleUI;

		// Ensure disabled modules are only visible if show all modules is true. Except the renderer module, we want that 
		// to be shown always if the module is there which means that we have a ParticleSystemRenderer
		if (!isRendererModule && !m_Enabled.boolValue && !ParticleEffectUI.GetAllModulesVisible())
				SetVisibilityState(VisibilityState.NotVisible);

		// Ensure enabled modules are visible
		if (m_Enabled.boolValue && !visibleUI)
			SetVisibilityState(VisibilityState.VisibleAndFolded);
	}

	protected virtual void SetVisibilityState (VisibilityState newState)
	{
		if (newState != m_VisibilityState)
		{
			if (newState == VisibilityState.VisibleAndFolded)
			{
				// Remove curves from the curveeditor when closing modules (and put them back when folding out again)
				ParticleSystemCurveEditor psce = m_ParticleSystemUI.m_ParticleEffectUI.GetParticleSystemCurveEditor();
				foreach (SerializedProperty curveProp in m_ModuleCurves)
				{
					if (psce.IsAdded(curveProp))
					{
						m_CurvesRemovedWhenFolded.Add (curveProp);
						psce.SetVisible (curveProp, false);
					}
				}
				psce.Refresh();
			}
			else if (newState == VisibilityState.VisibleAndFoldedOut)
			{
				ParticleSystemCurveEditor psce = m_ParticleSystemUI.m_ParticleEffectUI.GetParticleSystemCurveEditor();
				foreach (SerializedProperty curveProp in m_CurvesRemovedWhenFolded)
				{
					psce.SetVisible (curveProp, true);
				}
				m_CurvesRemovedWhenFolded.Clear();
				psce.Refresh();
			}

			m_VisibilityState = newState;
			InspectorState.SetInt(GetUniqueModuleName(), (int)m_VisibilityState);
			if (newState == VisibilityState.VisibleAndFoldedOut)
				Init();
		}
	}

	protected ParticleSystem GetParticleSystem ()
	{
		return m_Enabled.serializedObject.targetObject as ParticleSystem;
	}
	
	public ParticleSystemCurveEditor GetParticleSystemCurveEditor ()
	{
		return m_ParticleSystemUI.m_ParticleEffectUI.GetParticleSystemCurveEditor ();
	}

	public void AddToModuleCurves (SerializedProperty curveProp)
	{
		m_ModuleCurves.Add (curveProp);
		if (!foldout)
			m_CurvesRemovedWhenFolded.Add(curveProp);
	}
	// See ParticleSystemGUI.cs for more ModuleUI GUI helper functions...
}
} // namespace UnityEditor
