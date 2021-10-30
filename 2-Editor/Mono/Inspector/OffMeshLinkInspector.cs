using UnityEngine;


namespace UnityEditor
{
	[CanEditMultipleObjects]
	[CustomEditor (typeof (OffMeshLink))]
	internal class OffMeshLinkInspector : Editor
	{
		private SerializedProperty m_NavMeshLayer;
		private SerializedProperty m_Start;
		private SerializedProperty m_End;
		private SerializedProperty m_CostOverride;
		private SerializedProperty m_BiDirectional;
		private SerializedProperty m_Activated;
		private SerializedProperty m_AutoUpdatePositions;

		void OnEnable () {
			m_NavMeshLayer = serializedObject.FindProperty ("m_NavMeshLayer");
			m_Start = serializedObject.FindProperty ("m_Start");
			m_End = serializedObject.FindProperty ("m_End");
			m_CostOverride = serializedObject.FindProperty ("m_CostOverride");
			m_BiDirectional = serializedObject.FindProperty ("m_BiDirectional");
			m_Activated = serializedObject.FindProperty ("m_Activated");
			m_AutoUpdatePositions = serializedObject.FindProperty ("m_AutoUpdatePositions");
		}

		public override void OnInspectorGUI ()
		{
			if (!Application.HasProLicense ())
			{
				EditorGUILayout.HelpBox ("This is only available in the Pro version of Unity.", MessageType.Warning);
				GUI.enabled = false;
			}

			serializedObject.Update ();

			EditorGUILayout.PropertyField (m_Start);
			EditorGUILayout.PropertyField (m_End);
			EditorGUILayout.PropertyField (m_CostOverride);
			EditorGUILayout.PropertyField (m_BiDirectional);
			EditorGUILayout.PropertyField (m_Activated);
			EditorGUILayout.PropertyField (m_AutoUpdatePositions);

			SelectNavMeshLayer ();

			serializedObject.ApplyModifiedProperties ();
		}

		private void SelectNavMeshLayer ()
		{
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_NavMeshLayer.hasMultipleDifferentValues;
			var navLayerNames = GameObjectUtility.GetNavMeshLayerNames();
			var currentAbsoluteIndex = m_NavMeshLayer.intValue;
			var navLayerindex = -1;

			//Need to find the index as the list of names will compress out empty layers
			for (var i = 0; i < navLayerNames.Length; i++)
			{
				if (GameObjectUtility.GetNavMeshLayerFromName(navLayerNames[i]) == currentAbsoluteIndex)
				{
					navLayerindex = i;
					break;
				}
			}

			var navMeshLayer = EditorGUILayout.Popup ("Navigation Layer", navLayerindex, navLayerNames);
			EditorGUI.showMixedValue = false;

			if (EditorGUI.EndChangeCheck ())
			{
				var newNavLayerIndex = GameObjectUtility.GetNavMeshLayerFromName(navLayerNames[navMeshLayer]);
				m_NavMeshLayer.intValue = newNavLayerIndex;
			}
		}
	}
}
