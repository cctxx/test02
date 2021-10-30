using UnityEngine;


namespace UnityEditor
{

class ColorModuleUI : ModuleUI
{
	class Texts
	{
		public GUIContent color = new GUIContent("Color", "Controls the color of each particle during its lifetime.");
	}
	static Texts s_Texts;
	SerializedMinMaxGradient m_Gradient;
	SerializedProperty m_Scale;

	public ColorModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ColorModule", displayName)
	{
		m_ToolTip = "Controls the color of each particle during its lifetime.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Gradient != null)
			return;
		m_Gradient = new SerializedMinMaxGradient(this);
		m_Gradient.m_AllowColor = false;
		m_Gradient.m_AllowRandomBetweenTwoColors = false;
	}

	public override void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIMinMaxGradient (s_Texts.color, m_Gradient);
	}
}

} // namespace UnityEditor
