using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{
	[CustomEditor(typeof(Animator))]
	[CanEditMultipleObjects]
	internal class AnimatorInspector : Editor
	{
        SerializedProperty m_Controller;
		SerializedProperty m_Avatar;
		SerializedProperty m_ApplyRootMotion;
		SerializedProperty m_AnimatePhysics;
		SerializedProperty m_CullingMode;		

		void OnEnable ()
		{
            m_Controller = serializedObject.FindProperty("m_Controller");
			m_Avatar = serializedObject.FindProperty("m_Avatar");
			m_ApplyRootMotion = serializedObject.FindProperty("m_ApplyRootMotion");
			m_AnimatePhysics = serializedObject.FindProperty("m_AnimatePhysics");
			m_CullingMode = serializedObject.FindProperty("m_CullingMode");			
		}
							
		public override void OnInspectorGUI()
		{
			bool isEditingMultipleObjects = targets.Length > 1;

			Animator animator = target as Animator;

			serializedObject.UpdateIfDirtyOrScript();

			EditorGUI.BeginChangeCheck();
            EditorGUILayout.PropertyField(m_Controller);
			if (EditorGUI.EndChangeCheck ())
				EditorApplication.RepaintAnimationWindow ();
			
			EditorGUILayout.PropertyField(m_Avatar);
			if (animator.supportsOnAnimatorMove && !isEditingMultipleObjects)
				EditorGUILayout.LabelField("Apply Root Motion", "Handled by Script");
			else
				EditorGUILayout.PropertyField(m_ApplyRootMotion);
			
			EditorGUILayout.PropertyField(m_AnimatePhysics);
			EditorGUILayout.PropertyField(m_CullingMode);

		    serializedObject.ApplyModifiedProperties();
		}
	}
}
