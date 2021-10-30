using UnityEngine;


namespace UnityEditor
{
class SizeModuleUI : ModuleUI
{
	SerializedMinMaxCurve m_Curve;
	GUIContent m_SizeText = new GUIContent("Size", "Controls the size of each particle during its lifetime.");

	public SizeModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "SizeModule", displayName)
	{
		m_ToolTip = "Controls the size of each particle during its lifetime.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Curve != null)
			return;

		m_Curve = new SerializedMinMaxCurve(this, m_SizeText);
		m_Curve.m_AllowConstant = false;
	}		

	override public void OnInspectorGUI (ParticleSystem s)
	{
		GUIMinMaxCurve(m_SizeText, m_Curve);
	}
}
} // namespace UnityEditor
