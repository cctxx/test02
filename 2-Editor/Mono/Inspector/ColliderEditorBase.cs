
namespace UnityEditor
{
	internal class ColliderEditorBase : Editor
	{
		protected SerializedProperty m_Material;
		protected SerializedProperty m_IsTrigger;

		public virtual void OnEnable ()
		{
			m_Material = serializedObject.FindProperty("m_Material");
			m_IsTrigger = serializedObject.FindProperty("m_IsTrigger");
		}

		public override void OnInspectorGUI ()
		{
			serializedObject.Update ();

			EditorGUILayout.PropertyField (m_IsTrigger);
			EditorGUILayout.PropertyField (m_Material);

			serializedObject.ApplyModifiedProperties ();
		}
	}
}
