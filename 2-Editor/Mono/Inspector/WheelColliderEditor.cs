using UnityEngine;

namespace UnityEditor
{
	[CustomEditor (typeof (WheelCollider))]
	[CanEditMultipleObjects]
	internal class WheelColliderEditor : ColliderEditorBase
	{
		SerializedProperty m_Center;
		SerializedProperty m_Radius;
		SerializedProperty m_SuspensionDistance;
		SerializedProperty m_SuspensionSpring;
		SerializedProperty m_Mass;
		SerializedProperty m_ForwardFriction;
		SerializedProperty m_SidewaysFriction;

		public override void OnEnable ()
		{
			// Wheel Collider does not serialize Collider properties, so we don't use base OnEnable like other collider types
			m_Center = serializedObject.FindProperty ("m_Center");
			m_Radius = serializedObject.FindProperty ("m_Radius");
			m_SuspensionDistance = serializedObject.FindProperty ("m_SuspensionDistance");
			m_SuspensionSpring = serializedObject.FindProperty ("m_SuspensionSpring");
			m_Mass = serializedObject.FindProperty ("m_Mass");
			m_ForwardFriction = serializedObject.FindProperty ("m_ForwardFriction");
			m_SidewaysFriction = serializedObject.FindProperty ("m_SidewaysFriction");
		}

		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();

			EditorGUILayout.PropertyField (m_Mass);
			EditorGUILayout.PropertyField (m_Radius);
			EditorGUILayout.PropertyField (m_SuspensionDistance);
			EditorGUILayout.Space ();
			EditorGUILayout.PropertyField (m_Center);
			EditorGUILayout.Space ();
			StructPropertyGUILayout.JointSpring (m_SuspensionSpring);
			StructPropertyGUILayout.WheelFrictionCurve (m_ForwardFriction);
			StructPropertyGUILayout.WheelFrictionCurve (m_SidewaysFriction);

			serializedObject.ApplyModifiedProperties ();
		}
	}
}
