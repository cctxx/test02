using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(MeshCollider))]
	[CanEditMultipleObjects]
	internal class MeshColliderEditor : ColliderEditorBase
	{
		private SerializedProperty m_Mesh;
		private SerializedProperty m_Convex;
		private SerializedProperty m_SmoothSphereCollisions;

		public override void OnEnable ()
		{
			base.OnEnable ();

			m_Mesh = serializedObject.FindProperty ("m_Mesh");
			m_Convex = serializedObject.FindProperty ("m_Convex");
			m_SmoothSphereCollisions = serializedObject.FindProperty ("m_SmoothSphereCollisions");
		}

		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();

			EditorGUILayout.PropertyField (m_IsTrigger);
			EditorGUILayout.PropertyField (m_Material);
			EditorGUILayout.PropertyField (m_Convex);
			// @TODO: Test if this setting actually does anything anymore.
			EditorGUILayout.PropertyField (m_SmoothSphereCollisions);
			EditorGUILayout.PropertyField (m_Mesh);

			serializedObject.ApplyModifiedProperties ();
		}
	}
}
