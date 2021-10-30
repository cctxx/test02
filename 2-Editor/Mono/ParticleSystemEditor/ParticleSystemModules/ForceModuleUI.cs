using UnityEngine;


namespace UnityEditor
{

class ForceModuleUI : ModuleUI
{
	SerializedMinMaxCurve m_X;
	SerializedMinMaxCurve m_Y;
	SerializedMinMaxCurve m_Z;
	SerializedProperty m_RandomizePerFrame;
	SerializedProperty m_InWorldSpace;

	
	class Texts
	{
		public GUIContent x = new GUIContent("X");
		public GUIContent y = new GUIContent("Y");
		public GUIContent z = new GUIContent("Z");
		public GUIContent randomizePerFrame = new GUIContent("Randomize", "Randomize force every frame. Only available when using random between two constants or random between two curves.");
		public GUIContent space = new GUIContent("Space", "Specifies if the force values are in local space (rotated with the transform) or world space.");
		public string[] spaces = {"Local", "World"};
	}
	static Texts s_Texts;

	public ForceModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ForceModule", displayName)
	{
		m_ToolTip = "Controls the force of each particle during its lifetime.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_X != null)
			return;

		if (s_Texts == null)
			s_Texts = new Texts();

		m_X = new SerializedMinMaxCurve(this, s_Texts.x, "x", kUseSignedRange);
		m_Y = new SerializedMinMaxCurve(this, s_Texts.y, "y", kUseSignedRange);
		m_Z = new SerializedMinMaxCurve(this, s_Texts.z, "z", kUseSignedRange);
		m_RandomizePerFrame = GetProperty("randomizePerFrame");
		m_InWorldSpace = GetProperty("inWorldSpace");
	}
	
	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		MinMaxCurveState state = m_X.state;
		GUITripleMinMaxCurve(GUIContent.none, s_Texts.x, m_X, s_Texts.y, m_Y, s_Texts.z, m_Z, m_RandomizePerFrame);
		
		GUIBoolAsPopup (s_Texts.space, m_InWorldSpace, s_Texts.spaces);

		EditorGUI.BeginDisabledGroup((state != MinMaxCurveState.k_TwoScalars) && (state != MinMaxCurveState.k_TwoCurves)); 
		GUIToggle(s_Texts.randomizePerFrame, m_RandomizePerFrame);
		EditorGUI.EndDisabledGroup();
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		Init();

		if (!m_X.SupportsProcedural() || !m_Y.SupportsProcedural() || !m_Z.SupportsProcedural())
			text += "\n\tLifetime force curves use too many keys.";

		if(m_RandomizePerFrame.boolValue)
			text += "\n\tLifetime force curves use random per frame.";
	}

} // namespace UnityEditor

}
