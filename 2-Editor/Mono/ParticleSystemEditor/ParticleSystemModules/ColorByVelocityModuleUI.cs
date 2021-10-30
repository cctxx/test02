using UnityEngine;


namespace UnityEditor
{
class ColorByVelocityModuleUI : ModuleUI
{
	class Texts
	{
		public GUIContent color = new GUIContent("Color", "Controls the color of each particle based on its speed.");
		public GUIContent velocityRange = new GUIContent("Speed Range", "Remaps speed in the defined range to a color.");
	}
	static Texts s_Texts;
	SerializedMinMaxGradient m_Gradient;
	SerializedProperty m_Range;
	SerializedProperty m_Scale;

	public ColorByVelocityModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ColorBySpeedModule", displayName)
	{
		m_ToolTip = "Controls the color of each particle based on its speed.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Gradient != null)
			return;

		m_Gradient = new SerializedMinMaxGradient(this);
		m_Gradient.m_AllowColor = false;
		m_Gradient.m_AllowRandomBetweenTwoColors = false;
		
		m_Range = GetProperty("range");
	}	

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIMinMaxGradient (s_Texts.color, m_Gradient);	
		GUIMinMaxRange (s_Texts.velocityRange, m_Range);
	}
}
} // namespace UnityEditor
