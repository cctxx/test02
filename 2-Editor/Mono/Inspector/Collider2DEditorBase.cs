#if ENABLE_2D_PHYSICS

namespace UnityEditor
{
	internal class Collider2DEditorBase : Editor
	{
		protected SerializedProperty m_Material;
		protected SerializedProperty m_IsTrigger;

		public virtual void OnEnable ()
		{
			m_Material = serializedObject.FindProperty ("m_Material");
			m_IsTrigger = serializedObject.FindProperty ("m_IsTrigger");
		}

		protected void BeginColliderInspector ()
		{
			serializedObject.Update ();
			EditorGUILayout.PropertyField (m_IsTrigger);
			EditorGUILayout.PropertyField (m_Material);
		}

		protected void EndColliderInspector ()
		{
			serializedObject.ApplyModifiedProperties ();
		}
	}
}

#endif
