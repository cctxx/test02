using UnityEngine;


namespace UnityEditor
{

class ExternalForcesModuleUI : ModuleUI
{
	SerializedProperty m_Multiplier;
	
	class Texts
	{
		public GUIContent multiplier = new GUIContent("Multiplier", "Used to scale the force applied to this particle system.");
	}
	static Texts s_Texts;

	public ExternalForcesModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ExternalForcesModule", displayName)
	{
		m_ToolTip = "Controls the wind zones that each particle is affected by.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_Multiplier != null)
			return;

		if (s_Texts == null)
			s_Texts = new Texts();

		m_Multiplier = GetProperty("multiplier");
	}
	
	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIFloat(s_Texts.multiplier, m_Multiplier);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		text += "\n\tExternal Forces is enabled.";
	}

} // namespace UnityEditor

}
