using UnityEngine;


namespace UnityEditor
{

class VelocityModuleUI : ModuleUI
{
	SerializedMinMaxCurve m_X;
	SerializedMinMaxCurve m_Y;
	SerializedMinMaxCurve m_Z;
	SerializedProperty m_InWorldSpace;

	class Texts
	{
		public GUIContent x = new GUIContent("X");
		public GUIContent y = new GUIContent("Y");
		public GUIContent z = new GUIContent("Z");
		public GUIContent space = new GUIContent("Space", "Specifies if the velocity values are in local space (rotated with the transform) or world space.");
		public string[] spaces = {"Local", "World"};
	}
	static Texts s_Texts;

	public VelocityModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "VelocityModule", displayName)
	{
		m_ToolTip = "Controls the velocity of each particle during its lifetime.";
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
		m_InWorldSpace = GetProperty("inWorldSpace");
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUITripleMinMaxCurve(GUIContent.none, s_Texts.x, m_X, s_Texts.y, m_Y, s_Texts.z, m_Z, null);
		GUIBoolAsPopup (s_Texts.space, m_InWorldSpace, s_Texts.spaces);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		Init();

		if (!m_X.SupportsProcedural() || !m_Y.SupportsProcedural() || !m_Z.SupportsProcedural())
			text += "\n\tLifetime velocity curves use too many keys.";
	}
}

} // namespace UnityEditor
