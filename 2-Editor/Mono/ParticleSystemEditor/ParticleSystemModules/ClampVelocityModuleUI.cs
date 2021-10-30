using UnityEngine;


namespace UnityEditor
{

class ClampVelocityModuleUI : ModuleUI
{
	SerializedMinMaxCurve m_X;
	SerializedMinMaxCurve m_Y;
	SerializedMinMaxCurve m_Z;
	SerializedMinMaxCurve m_Magnitude;
	SerializedProperty m_SeparateAxis;
	SerializedProperty m_InWorldSpace;
	SerializedProperty m_Dampen;

	class Texts
	{
		public GUIContent x = new GUIContent("X");
		public GUIContent y = new GUIContent("Y");
		public GUIContent z = new GUIContent("Z");
		public GUIContent dampen = new GUIContent("Dampen", "Controls how much the velocity that exceeds the velocity limit should be dampened. A value of 0.5 will dampen the exceeding velocity by 50%.");
		public GUIContent magnitude = new GUIContent("  Speed", "The speed limit of particles over the particle lifetime.");
		public GUIContent separateAxis = new GUIContent("Separate Axis", "If enabled, you can control the velocity limit separately for each axis.");
		public GUIContent space = new GUIContent("  Space", "Specifies if the velocity values are in local space (rotated with the transform) or world space.");
		public string[] spaces = { "Local", "World" };
	}
	static Texts s_Texts;


	public ClampVelocityModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "ClampVelocityModule", displayName)
	{
		m_ToolTip = "Controls the velocity limit and damping of each particle during its lifetime.";
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
		m_Magnitude = new SerializedMinMaxCurve(this, s_Texts.magnitude, "magnitude");
		m_SeparateAxis = GetProperty("separateAxis");
		m_InWorldSpace = GetProperty("inWorldSpace");
		m_Dampen = GetProperty("dampen");
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		EditorGUI.BeginChangeCheck();
		bool separateAxis = GUIToggle (s_Texts.separateAxis, m_SeparateAxis);
		if (EditorGUI.EndChangeCheck())
		{
			// Remove old curves from curve editor
			if (separateAxis)
			{
				m_Magnitude.RemoveCurveFromEditor(); 
			}
			else
			{
				m_X.RemoveCurveFromEditor();
				m_Y.RemoveCurveFromEditor();
				m_Z.RemoveCurveFromEditor();
			}
		}

		if (separateAxis)
		{
			GUITripleMinMaxCurve(GUIContent.none, s_Texts.x, m_X, s_Texts.y, m_Y, s_Texts.z, m_Z, null);
			GUIBoolAsPopup(s_Texts.space, m_InWorldSpace, s_Texts.spaces);
		}
		else
		{
			GUIMinMaxCurve(s_Texts.magnitude, m_Magnitude);
		}

		GUIFloat (s_Texts.dampen, m_Dampen);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		text += "\n\tLimit velocity is enabled.";
	}
}

} // namespace UnityEditor
