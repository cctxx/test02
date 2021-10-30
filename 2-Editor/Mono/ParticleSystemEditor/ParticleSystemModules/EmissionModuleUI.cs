using UnityEngine;


namespace UnityEditor
{

class EmissionModuleUI : ModuleUI
{
	SerializedProperty m_Type;
	public SerializedMinMaxCurve m_Rate;
	

	// Keep in sync with EmissionModule.h
	const int k_MaxNumBursts = 4;
	SerializedProperty[] m_BurstTime = new SerializedProperty[k_MaxNumBursts];
	SerializedProperty[] m_BurstParticleCount = new SerializedProperty[k_MaxNumBursts];
	SerializedProperty m_BurstCount;

	private string[] m_GuiNames = new string[] { "Time", "Distance" };

	// Keep in sync with EmissionModule.h
	enum EmissionTypes { Time, Distance };

	class Texts
	{
		public GUIContent rate = new GUIContent("Rate", "The number of particles emitted per second (Time) or per distance unit (Distance)");
		public GUIContent burst = new GUIContent("Bursts", "Emission of extra particles at specific times during the duration of the system.");
	}
	private static Texts s_Texts;


	public EmissionModuleUI(ParticleSystemUI owner, SerializedObject o, string displayName)
		: base (owner, o, "EmissionModule", displayName)
	{
		m_ToolTip = "Emission of the emitter. This controls the rate at which particles are emitted as well as burst emissions.";
	}

	protected override void Init()
	{
		if (s_Texts == null)
			s_Texts = new Texts();

		// Already initialized?
		if (m_BurstCount != null)
			return;

		m_Type = GetProperty("m_Type");

		m_Rate = new SerializedMinMaxCurve(this, s_Texts.rate, "rate");
		m_Rate.m_AllowRandom = false;
	
		// Keep in sync with EmissionModule.h
		m_BurstTime[0] = GetProperty("time0");
		m_BurstTime[1] = GetProperty("time1");
		m_BurstTime[2] = GetProperty("time2");
		m_BurstTime[3] = GetProperty("time3");

		m_BurstParticleCount[0] = GetProperty("cnt0");
		m_BurstParticleCount[1] = GetProperty("cnt1");
		m_BurstParticleCount[2] = GetProperty("cnt2");
		m_BurstParticleCount[3] = GetProperty("cnt3");

		m_BurstCount = GetProperty("m_BurstCount");


	}

	override public void OnInspectorGUI (ParticleSystem s)
	{
		GUIMinMaxCurve(s_Texts.rate, m_Rate);
		GUIPopup(GUIContent.none, m_Type, m_GuiNames);
		
		if (m_Type.intValue != (int)EmissionTypes.Distance)
			DoBurstGUI (s);
	}

	private void DoBurstGUI (ParticleSystem s)
	{
		EditorGUILayout.Space ();
		
		Rect rect = GetControlRect(kSingleLineHeight);
	
		GUI.Label(rect, s_Texts.burst, ParticleSystemStyles.Get().label);
		
		float dragWidth = kDragSpace;
		float entryWidth = 40;
		float lineWidth = (entryWidth + dragWidth) * 2 + dragWidth-1;
		float rightAlign = rect.width - lineWidth;
		rightAlign = Mathf.Min(rightAlign, EditorGUIUtility.labelWidth);

		int count = m_BurstCount.intValue;

		Rect lineRect = new Rect(rect.x + rightAlign, rect.y, lineWidth, 3);

		GUI.Label(lineRect, GUIContent.none, ParticleSystemStyles.Get().line);

		Rect r = new Rect(rect.x + dragWidth + rightAlign, rect.y, entryWidth + dragWidth, rect.height);
		GUI.Label(r, "Time", ParticleSystemStyles.Get().label);
		r.x += dragWidth + entryWidth;
		GUI.Label(r, "Particles", ParticleSystemStyles.Get().label);

		lineRect.y += kSingleLineHeight-1;
		GUI.Label(lineRect, GUIContent.none, ParticleSystemStyles.Get().line);


		float lengthInSec = s.duration;


		int oldCount = count;
		for (int q = 0; q < count; ++q)
		{
			SerializedProperty pt = m_BurstTime[q];
			SerializedProperty pc = m_BurstParticleCount[q];

			// Reserve space
			rect = GetControlRect(kSingleLineHeight);
			r = new Rect(rect.x + rightAlign, rect.y, dragWidth+entryWidth, rect.height);

			// Time
			float time = FloatDraggable(r, pt, 1f, dragWidth, "n2");
			if (time < 0.0f)
				pt.floatValue = 0.0f;
			if (time > lengthInSec)
				pt.floatValue = lengthInSec;

			// Num particles
			int numParticles = pc.intValue;
			r.x += r.width;

			pc.intValue = IntDraggable(r, null, numParticles, dragWidth);

			if (q == count - 1) // only last one will have minus button for now
			{
				r.x = lineRect.xMax - kPlusAddRemoveButtonWidth;
				if (MinusButton(r))
				{
					count--;
					//bursts.RemoveAt (q);
					continue;
				}
			}
		}

		if (count < k_MaxNumBursts)
		{
			r = GetControlRect(kSingleLineHeight);
			r.xMin = r.xMax - kPlusAddRemoveButtonWidth;
			if (PlusButton(r))
				count++;
		}

		if (count != oldCount)
		{
			m_BurstCount.intValue = count;
		}
	}
	
	override public void UpdateCullingSupportedString(ref string text)
	{
		Init();

		if (m_Type.intValue == (int)EmissionTypes.Distance)
			text += "\n\tEmission is distance based.";
	}

	}


} // namespace UnityEditor
