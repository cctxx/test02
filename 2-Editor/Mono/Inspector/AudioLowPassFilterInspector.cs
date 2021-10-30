using UnityEditor;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor (typeof (AudioLowPassFilter))]
	[CanEditMultipleObjects]
	internal class AudioLowPassFilterInspector : Editor
	{
		SerializedProperty m_LowpassResonanceQ;
		SerializedProperty m_CutoffFrequency;
		SerializedProperty m_LowpassLevelCustomCurve;
		
		void OnEnable () {
			m_LowpassResonanceQ = serializedObject.FindProperty ("m_LowpassResonanceQ");
			m_CutoffFrequency = serializedObject.FindProperty ("m_CutoffFrequency");
			m_LowpassLevelCustomCurve = serializedObject.FindProperty ("lowpassLevelCustomCurve");
		}
		
		public override void OnInspectorGUI ()
		{
			if (!Application.HasAdvancedLicense ())
			{
				GUILayout.BeginHorizontal ();
				GUIContent c = new GUIContent ("This is only available in the Pro version of Unity.");
				GUILayout.Label (c, EditorStyles.helpBox);
				GUILayout.EndHorizontal ();
			}
			
			serializedObject.Update ();
			
			EditorGUI.BeginChangeCheck ();
			AudioSourceInspector.AnimProp (
				new GUIContent ("Cutoff Frequency"),
				m_LowpassLevelCustomCurve,
				AudioSourceInspector.kMaxCutoffFrequency, 0.0f);
			if (EditorGUI.EndChangeCheck ())
			{
				AnimationCurve modifiedCurve = m_LowpassLevelCustomCurve.animationCurveValue;
				if (modifiedCurve.length > 0)					
					m_CutoffFrequency.floatValue = Mathf.Lerp (AudioSourceInspector.kMaxCutoffFrequency, 0, modifiedCurve.keys[0].value);
			}
			
			EditorGUILayout.PropertyField (m_LowpassResonanceQ);
			
			serializedObject.ApplyModifiedProperties ();
		}
	}
}