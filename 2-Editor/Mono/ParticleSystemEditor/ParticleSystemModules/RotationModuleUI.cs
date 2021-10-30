using UnityEngine;


namespace UnityEditor
{

class RotationModuleUI : ModuleUI
{
	SerializedMinMaxCurve m_Curve;

	class Texts
	{
		public GUIContent rotation = new GUIContent("Angular Velocity", "Controls the angular velocity of each particle during its lifetime.");
	}
	static Texts s_Texts;


	public RotationModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "RotationModule", displayName)
	{
		m_ToolTip = "Controls the angular velocity of each particle during its lifetime.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Curve != null)
			return;

		if (s_Texts == null)
			s_Texts = new Texts();

		m_Curve = new SerializedMinMaxCurve(this, s_Texts.rotation, kUseSignedRange);
		m_Curve.m_RemapValue = Mathf.Rad2Deg;
	}	

	public override void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIMinMaxCurve(s_Texts.rotation, m_Curve);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		Init();

		if(!m_Curve.SupportsProcedural())
			text += "\n\tLifetime rotation curve uses too many keys.";
	}
}


} // namespace UnityEditor
