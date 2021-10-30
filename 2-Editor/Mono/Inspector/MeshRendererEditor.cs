using System.Collections.Generic;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor (typeof (MeshRenderer))]
	[CanEditMultipleObjects]
	internal class MeshRendererEditor : Editor
	{
		SerializedProperty m_UseLightProbes;
		SerializedProperty m_LightProbeAnchor;
		
		public void OnEnable()
		{
			m_UseLightProbes = serializedObject.FindProperty ("m_UseLightProbes");
			m_LightProbeAnchor = serializedObject.FindProperty ("m_LightProbeAnchor");
		}

		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();
			
			DrawPropertiesExcluding (serializedObject, "m_UseLightProbes", "m_LightProbeAnchor");
			
			EditorGUILayout.PropertyField (m_UseLightProbes);
			if (m_UseLightProbes.boolValue)
			{
				EditorGUI.indentLevel++;
				EditorGUILayout.PropertyField (m_LightProbeAnchor, new GUIContent ("Anchor Override", m_LightProbeAnchor.tooltip));
				EditorGUI.indentLevel--;
			}

			serializedObject.ApplyModifiedProperties ();
		}
	}
}
