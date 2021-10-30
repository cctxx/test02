using UnityEngine;


namespace UnityEditor
{

class SizeByVelocityModuleUI : ModuleUI
{
	class Texts
	{
		public GUIContent velocityRange = new GUIContent("Speed Range", "Remaps speed in the defined range to a size.");
		public GUIContent size = new GUIContent("Size", "Controls the size of each particle based on its speed.");
	}
	static Texts s_Texts;

	SerializedMinMaxCurve m_Curve;
	SerializedProperty m_Range;


	public SizeByVelocityModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "SizeBySpeedModule", displayName)
	{
		m_ToolTip = "Controls the size of each particle based on its speed.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Curve != null)
			return;

		if (s_Texts == null)
			s_Texts = new Texts();

		m_Curve = new SerializedMinMaxCurve (this, s_Texts.size);
		m_Curve.m_AllowConstant = false;
		
		m_Range = GetProperty ("range");
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts ();

		GUIMinMaxCurve(s_Texts.size, m_Curve);
		GUIMinMaxRange(s_Texts.velocityRange, m_Range);
	}
}

} // namespace UnityEditor
