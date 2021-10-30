using UnityEngine;


namespace UnityEditor
{
class RotationByVelocityModuleUI : ModuleUI
{
	class Texts
	{
		public GUIContent velocityRange = new GUIContent("Speed Range", "Remaps speed in the defined range to an angular velocity.");
		public GUIContent rotation = new GUIContent("Angular Velocity", "Controls the angular velocity of each particle based on its speed.");
	}
	static Texts s_Texts;
	SerializedMinMaxCurve m_Curve;
	SerializedProperty m_Range;

	public RotationByVelocityModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "RotationBySpeedModule", displayName)
	{
		m_ToolTip = "Controls the angular velocity of each particle based on its speed.";
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
		m_Range = GetProperty("range");
	}	

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts ();
		GUIMinMaxCurve(s_Texts.rotation, m_Curve);
		GUIMinMaxRange(s_Texts.velocityRange, m_Range);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		text += "\n\tRotation by Speed is enabled.";
	}
}
} // namespace UnityEditor
