using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(BoxCollider))]
	[CanEditMultipleObjects]
	internal class BoxColliderEditor : ColliderEditorBase
	{
		private static readonly int s_BoxHash = "BoxColliderEditor".GetHashCode();
		SerializedProperty m_Center;
		SerializedProperty m_Size;

		private readonly BoxEditor m_BoxEditor = new BoxEditor(true, s_BoxHash);

		public override void OnEnable()
		{
			base.OnEnable();

			m_Center = serializedObject.FindProperty("m_Center");
			m_Size = serializedObject.FindProperty("m_Size");

			m_BoxEditor.OnEnable();
		}

		public void OnDisable ()
		{
			m_BoxEditor.OnDisable();
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update();

			EditorGUILayout.PropertyField(m_IsTrigger);
			EditorGUILayout.PropertyField(m_Material);
			EditorGUILayout.PropertyField(m_Center);
			EditorGUILayout.PropertyField(m_Size);

			serializedObject.ApplyModifiedProperties();
		}

		public void OnSceneGUI()
		{
			BoxCollider boxCollider = (BoxCollider)target;

			Vector3 center = boxCollider.center;
			Vector3 size = boxCollider.size;

			Color color = Handles.s_ColliderHandleColor;
			if (!boxCollider.enabled)
				color = Handles.s_ColliderHandleColorDisabled;
			
			if (m_BoxEditor.OnSceneGUI(boxCollider.transform, color, ref center, ref size))
			{
				Undo.RecordObject(boxCollider, "Modified Box Collider");
				boxCollider.center = center;
				boxCollider.size = size;
			}
		}
	}
}
