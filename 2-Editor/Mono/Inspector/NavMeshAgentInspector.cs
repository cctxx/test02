using UnityEngine;


namespace UnityEditor
{
	[CanEditMultipleObjects]
	[CustomEditor(typeof(NavMeshAgent))]
	internal class NavMeshAgentInspector : Editor
	{
		private SerializedProperty m_WalkableMask;
		private SerializedProperty m_Radius;
		private SerializedProperty m_Speed;
		private SerializedProperty m_Acceleration;
		private SerializedProperty m_AngularSpeed;
		private SerializedProperty m_StoppingDistance;
		private SerializedProperty m_AutoTraverseOffMeshLink;
		private SerializedProperty m_AutoBraking;
		private SerializedProperty m_AutoRepath;
		private SerializedProperty m_Height;
		private SerializedProperty m_BaseOffset;
		private SerializedProperty m_ObstacleAvoidanceType;
		private SerializedProperty m_AvoidancePriority;

		void OnEnable () {
			m_WalkableMask = serializedObject.FindProperty("m_WalkableMask");
			m_Radius = serializedObject.FindProperty("m_Radius");
			m_Speed = serializedObject.FindProperty("m_Speed");
			m_Acceleration = serializedObject.FindProperty("m_Acceleration");
			m_AngularSpeed = serializedObject.FindProperty("m_AngularSpeed");
			m_StoppingDistance = serializedObject.FindProperty("m_StoppingDistance");
			m_AutoTraverseOffMeshLink = serializedObject.FindProperty("m_AutoTraverseOffMeshLink");
			m_AutoBraking = serializedObject.FindProperty("m_AutoBraking");
			m_AutoRepath = serializedObject.FindProperty("m_AutoRepath");
			m_Height = serializedObject.FindProperty("m_Height");
			m_BaseOffset = serializedObject.FindProperty("m_BaseOffset");
			m_ObstacleAvoidanceType = serializedObject.FindProperty("m_ObstacleAvoidanceType");
			m_AvoidancePriority = serializedObject.FindProperty("avoidancePriority");
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update();

			EditorGUILayout.PropertyField(m_Radius);
			EditorGUILayout.PropertyField(m_Speed);
			EditorGUILayout.PropertyField(m_Acceleration);
			EditorGUILayout.PropertyField(m_AngularSpeed);
			EditorGUILayout.PropertyField(m_StoppingDistance);
			EditorGUILayout.PropertyField(m_AutoTraverseOffMeshLink);
			EditorGUILayout.PropertyField(m_AutoBraking);
			EditorGUILayout.PropertyField(m_AutoRepath);
			EditorGUILayout.PropertyField(m_Height);
			EditorGUILayout.PropertyField(m_BaseOffset);
			EditorGUILayout.PropertyField(m_ObstacleAvoidanceType);
			EditorGUILayout.PropertyField(m_AvoidancePriority);

			//Initially needed data
			var navLayerNames = GameObjectUtility.GetNavMeshLayerNames();
			var currentMask = m_WalkableMask.intValue;
			var compressedMask = 0;
			
			//Need to find the index as the list of names will compress out empty layers
			for (var i = 0; i < navLayerNames.Length; i++)
			{
				var layer = GameObjectUtility.GetNavMeshLayerFromName(navLayerNames[i]);
				if (((1 << layer) & currentMask) > 0)
					compressedMask = compressedMask | (1 << i);
			}

			//TODO: Refactor this to use the mask field that takes a label.
			const float kH = EditorGUI.kSingleLineHeight;
			var position =GUILayoutUtility.GetRect(EditorGUILayout.kLabelFloatMinW, EditorGUILayout.kLabelFloatMaxW, kH, kH, EditorStyles.layerMaskField);

			EditorGUI.BeginChangeCheck();
			EditorGUI.showMixedValue = m_WalkableMask.hasMultipleDifferentValues;
			var walkableLayers = EditorGUI.MaskField(position, "NavMesh Walkable", compressedMask, navLayerNames, EditorStyles.layerMaskField); 
			EditorGUI.showMixedValue = false;

			if (EditorGUI.EndChangeCheck())
			{
				if (walkableLayers == ~0)
				{
					m_WalkableMask.intValue = ~0;
				}
				else
				{
					var newMask = 0;
					for (var i = 0; i < navLayerNames.Length; i++)
					{
						//If the bit has been set in the compacted mask
						if (((walkableLayers >> i) & 1) > 0)
						{
							//Find out the 'real' layer from the name, then set it in the new mask
							newMask = newMask | (1 << GameObjectUtility.GetNavMeshLayerFromName(navLayerNames[i]));
						}
					}
					m_WalkableMask.intValue = newMask;
				}
			}
			serializedObject.ApplyModifiedProperties();
		}
	}
}
