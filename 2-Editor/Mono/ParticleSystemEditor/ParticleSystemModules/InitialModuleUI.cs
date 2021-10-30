using UnityEngine;


namespace UnityEditor
{

internal class InitialModuleUI : ModuleUI
{
	public SerializedProperty m_LengthInSec;
	public SerializedProperty m_Looping;
	public SerializedProperty m_Prewarm;
	public SerializedProperty m_StartDelay;
	public SerializedProperty m_PlayOnAwake;
	public SerializedProperty m_SimulationSpace;

	public SerializedMinMaxCurve m_LifeTime;
	public SerializedMinMaxCurve m_Speed;
	public SerializedMinMaxGradient m_Color;
	public SerializedMinMaxCurve m_Size;
	public SerializedMinMaxCurve m_Rotation;
	public SerializedProperty m_GravityModifier;
	public SerializedProperty m_InheritVelocity;
	public SerializedProperty m_MaxNumParticles;
	//public SerializedProperty m_ProceduralMode;

	class Texts
	{
		public GUIContent duration = new GUIContent("Duration", "The length of time the Particle System is emitting partcles, if the system is looping, this indicates the length of one cycle.");
		public GUIContent looping = new GUIContent ("Looping", "If true, the emission cycle will repeat after the duration.");
		public GUIContent prewarm = new GUIContent ("Prewarm", "When played a prewarmed system will be in a state as if it had emitted one loop cycle. Can only be used if the system is looping." );
		public GUIContent startDelay = new GUIContent("Start Delay", "Delay in seconds that this Particle System will wait before emitting particles. Cannot be used together with a prewarmed looping system.");
		public GUIContent maxParticles = new GUIContent("Max Particles", "The number of particles in the system will be limited by this number. Emission will be temporarily halted if this is reached.");
		public GUIContent lifetime = new GUIContent("Start Lifetime", "Start lifetime in seconds, particle will die when its lifetime reaches 0.");
		public GUIContent speed = new GUIContent("Start Speed", "The start speed of particles, applied in the starting direction.");
		public GUIContent color = new GUIContent("Start Color", "The start color of particles.");
		public GUIContent size = new GUIContent("Start Size", "The start size of particles.");
		public GUIContent rotation = new GUIContent("Start Rotation", "The start rotation of particles in degrees.");
		public GUIContent autoplay = new GUIContent("Play On Awake", "If enabled, the system will start playing automatically.");
		public GUIContent gravity = new GUIContent("Gravity Multiplier", "Scales the gravity defined in Physics Manager");
		public GUIContent inheritvelocity = new GUIContent("Inherit Velocity", "Applies the current directional velocity of the Transform to newly emitted particles.");
		public GUIContent simulationSpace = new GUIContent("Simulation Space", "Makes particle positions simulate in worldspace or local space. In local space they stay relative to the Transform.");
		public string[] simulationSpaces = {"World", "Local"};
	}
	private static Texts s_Texts;

	public InitialModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base (owner, o, "InitialModule", displayName, VisibilityState.VisibleAndFoldedOut)
	{
		Init (); // Should always be initialized since it is used by other modules (see ShapeModule)
	}

	public override float GetXAxisScalar ()
	{
		return m_ParticleSystemUI.GetEmitterDuration ();
	}

	protected override void Init()
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		// Already initialized?
		if (m_LengthInSec != null)
			return;
		
		// general emitter state
		m_LengthInSec = GetProperty0("lengthInSec");
		m_Looping = GetProperty0("looping");
		m_Prewarm = GetProperty0("prewarm");
		m_StartDelay = GetProperty0("startDelay");
		m_PlayOnAwake = GetProperty0("playOnAwake");
		m_SimulationSpace = GetProperty0("moveWithTransform");

		// module properties
		m_LifeTime = new SerializedMinMaxCurve(this, s_Texts.lifetime, "startLifetime");
		m_Speed = new SerializedMinMaxCurve(this, s_Texts.speed, "startSpeed", kUseSignedRange);
		m_Color = new SerializedMinMaxGradient(this, "startColor");
		m_Size = new SerializedMinMaxCurve(this, s_Texts.size, "startSize");
		m_Rotation = new SerializedMinMaxCurve(this, s_Texts.rotation, "startRotation", kUseSignedRange);
		m_Rotation.m_RemapValue = Mathf.Rad2Deg;
		m_Rotation.m_DefaultCurveScalar = Mathf.PI;
		m_GravityModifier = GetProperty("gravityModifier");
		m_InheritVelocity = GetProperty("inheritVelocity");
		m_MaxNumParticles = GetProperty("maxNumParticles");
	}
		
		
	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIFloat (s_Texts.duration, m_LengthInSec, "f2");
		m_LengthInSec.floatValue = Mathf.Min(100000.0f, Mathf.Max(0, m_LengthInSec.floatValue));
		
		bool oldLooping = m_Looping.boolValue;
		GUIToggle (s_Texts.looping, m_Looping);
		if (m_Looping.boolValue && !oldLooping && s.time >= m_LengthInSec.floatValue)
			s.time = 0.0f;
		
		EditorGUI.BeginDisabledGroup(!m_Looping.boolValue); 
		GUIToggle(s_Texts.prewarm, m_Prewarm);
		EditorGUI.EndDisabledGroup();

		EditorGUI.BeginDisabledGroup(m_Prewarm.boolValue && m_Looping.boolValue);
		GUIFloat(s_Texts.startDelay, m_StartDelay);
		EditorGUI.EndDisabledGroup();

		GUIMinMaxCurve(s_Texts.lifetime, m_LifeTime);
		GUIMinMaxCurve(s_Texts.speed, m_Speed);
		GUIMinMaxCurve(s_Texts.size, m_Size);
		GUIMinMaxCurve(s_Texts.rotation, m_Rotation);
		GUIMinMaxGradient(s_Texts.color, m_Color);

		GUIFloat(s_Texts.gravity, m_GravityModifier);	
		GUIFloat(s_Texts.inheritvelocity, m_InheritVelocity);	

		GUIBoolAsPopup (s_Texts.simulationSpace, m_SimulationSpace, s_Texts.simulationSpaces);

		bool oldPlayOnAwake = m_PlayOnAwake.boolValue;
		bool newPlayOnAwake = GUIToggle(s_Texts.autoplay, m_PlayOnAwake);
		if (oldPlayOnAwake != newPlayOnAwake)
			m_ParticleSystemUI.m_ParticleEffectUI.PlayOnAwakeChanged(newPlayOnAwake);

		GUIInt(s_Texts.maxParticles, m_MaxNumParticles);
	}

	override public void UpdateCullingSupportedString(ref string text)
	{
		if (!m_SimulationSpace.boolValue)
			text += "\n\tWorld space simulation is used.";

		if ((m_LifeTime.state == MinMaxCurveState.k_TwoCurves || m_LifeTime.state == MinMaxCurveState.k_TwoScalars))
			text += "\n\tStart lifetime is random.";
	}
}
} // namespace UnityEditor
