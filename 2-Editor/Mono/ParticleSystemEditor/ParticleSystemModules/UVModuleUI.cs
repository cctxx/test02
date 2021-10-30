using UnityEngine;


namespace UnityEditor
{

class UVModuleUI : ModuleUI
{
	// Keep in sync with enum in UVModule.h
	enum AnimationType { WholeSheet = 0, SingleRow = 1 };

	SerializedMinMaxCurve m_FrameOverTime;
	SerializedProperty m_TilesX;
	SerializedProperty m_TilesY;
	SerializedProperty m_AnimationType;
	SerializedProperty m_Cycles;
	SerializedProperty m_RandomRow;
	SerializedProperty m_RowIndex;

	class Texts
	{
		public GUIContent frameOverTime = new GUIContent("Frame over Time", "Controls the uv animation frame of each particle over its lifetime. On the horisontal axis you will find the lifetime. On the vertical axis you will find the sheet index.");
		public GUIContent animation = new GUIContent("Animation", "Specifies the animation type: Whole Sheet or Single Row. Whole Sheet will animate over the whole texture sheet from left to right, top to bottom. Single Row will animate a single row in the sheet from left to right.");
		public GUIContent tiles = new GUIContent ("Tiles", "Defines the tiling of the texture.");
		public GUIContent tilesX = new GUIContent("X");
		public GUIContent tilesY = new GUIContent("Y");
		public GUIContent row = new GUIContent("Row", "The row in the sheet which will be played.");
		public GUIContent frame = new GUIContent("Frame", "The frame in the sheet which will be used.");
		public GUIContent cycles = new GUIContent("Cycles", "Specifies how many times the animation will loop during the lifetime of the particle.");
		public GUIContent randomRow = new GUIContent("Random Row", "If enabled, the animated row will be chosen randomly.");
		public string[] types = new string[] { "Whole Sheet", "Single Row"};
	}
	private static Texts s_Texts;


	public UVModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base(owner, o, "UVModule", displayName)
	{
		m_ToolTip = "Particle UV animation. This allows you to specify a texture sheet (a texture with multiple tiles/sub frames) and animate or randomize over it per particle.";
	}

	protected override void Init()
	{
		// Already initialized?
		if (m_TilesX != null)
			return;
		
		if (s_Texts == null)
			s_Texts = new Texts();

		m_FrameOverTime = new SerializedMinMaxCurve(this, s_Texts.frameOverTime, "frameOverTime");
		m_TilesX = GetProperty("tilesX");
		m_TilesY = GetProperty("tilesY");
		m_AnimationType = GetProperty("animationType");
		m_Cycles = GetProperty("cycles");
		m_RandomRow = GetProperty("randomRow");
		m_RowIndex = GetProperty("rowIndex");
	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		GUIIntDraggableX2(s_Texts.tiles, s_Texts.tilesX, m_TilesX, s_Texts.tilesY, m_TilesY);
				
		int type = GUIPopup (s_Texts.animation, m_AnimationType, s_Texts.types);
		if (type == (int)AnimationType.SingleRow)
		{
			GUIToggle(s_Texts.randomRow, m_RandomRow);
			if (!m_RandomRow.boolValue)
				GUIInt(s_Texts.row, m_RowIndex);
		}

		if (type == (int)AnimationType.SingleRow)
			m_FrameOverTime.m_RemapValue = (float)(m_TilesX.intValue);
		if (type == (int)AnimationType.WholeSheet)
			m_FrameOverTime.m_RemapValue = (float)(m_TilesX.intValue * m_TilesY.intValue);
		GUIMinMaxCurve(s_Texts.frameOverTime, m_FrameOverTime);
		GUIFloat(s_Texts.cycles, m_Cycles);
	}
}


} // namespace UnityEditor
