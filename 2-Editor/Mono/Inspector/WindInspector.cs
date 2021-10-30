using UnityEngine;
using UnityEditor;


namespace UnityEditor
{
	[CustomEditor(typeof(WindZone))]
	[CanEditMultipleObjects]
	internal class WindInspector : Editor
	{
		public override void OnInspectorGUI()
		{
			serializedObject.Update ();

			GUI.enabled = true;
			WindZone t = target as WindZone;

			if (t == null)
			{
				GUILayout.Label("Error: Not a WindZode");
				return;
			}

			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_Mode"));
			GUI.enabled = t.mode == WindZoneMode.Spherical;
			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_Radius"));
			GUI.enabled = true;
			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_WindMain"));
			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_WindTurbulence"));
			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_WindPulseMagnitude"));
			EditorGUILayout.PropertyField (serializedObject.FindProperty ("m_WindPulseFrequency"));

			serializedObject.ApplyModifiedProperties ();
		}
	}
}
