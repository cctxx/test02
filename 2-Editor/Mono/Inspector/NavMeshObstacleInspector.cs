using UnityEngine;

namespace UnityEditor
{
	[CanEditMultipleObjects]
	[CustomEditor (typeof (NavMeshObstacle))]
	internal class NavMeshObstacleInspector : Editor
	{

		private SerializedProperty m_Radius;
		private SerializedProperty m_Height;
#if ENABLE_NAVMESH_CARVING
		private SerializedProperty m_MoveThreshold;
		private SerializedProperty m_Carve;
#endif

		void OnEnable ()
		{
			m_Radius = serializedObject.FindProperty ("m_Radius");
			m_Height = serializedObject.FindProperty ("m_Height");
#if ENABLE_NAVMESH_CARVING
			m_MoveThreshold = serializedObject.FindProperty ("m_MoveThreshold");
			m_Carve = serializedObject.FindProperty ("m_Carve");
#endif
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update ();

			EditorGUILayout.PropertyField (m_Radius);
			EditorGUILayout.PropertyField (m_Height);

#if ENABLE_NAVMESH_CARVING
			if (!Application.HasProLicense ())
			{
				EditorGUILayout.HelpBox ("This is only available in the Pro version of Unity.", MessageType.Warning);
				GUI.enabled = false;
			}

			EditorGUILayout.PropertyField (m_MoveThreshold);
			EditorGUILayout.PropertyField (m_Carve);
#endif
			serializedObject.ApplyModifiedProperties ();
		}
	}
}
