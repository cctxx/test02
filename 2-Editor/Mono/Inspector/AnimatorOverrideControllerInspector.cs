using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{
    [CustomEditor(typeof(AnimatorOverrideController))]
	[CanEditMultipleObjects]
    internal class AnimatorOverrideControllerInspector : Editor
	{
		SerializedProperty m_Controller;

    	private AnimationClipPair[] m_Clips;

		ReorderableList m_ClipList;

		void OnEnable ()
		{
			AnimatorOverrideController animatorOverrideController = target as AnimatorOverrideController;

			m_Controller = serializedObject.FindProperty("m_Controller");
			if (m_ClipList == null)
			{
				m_ClipList = new ReorderableList(animatorOverrideController.clips, typeof(AnimationClipPair), false, true, false, false);
				m_ClipList.drawElementCallback = DrawClipElement;
				m_ClipList.drawHeaderCallback = DrawClipHeader;
				m_ClipList.elementHeight = 16;
			}
			animatorOverrideController.OnOverrideControllerDirty += Repaint;
		}

		void OnDisable ()
		{
			AnimatorOverrideController animatorOverrideController = target as AnimatorOverrideController;
			animatorOverrideController.OnOverrideControllerDirty -= Repaint;
		}
							
		public override void OnInspectorGUI()
		{
			bool isEditingMultipleObjects = targets.Length > 1;
			bool changeCheck = false;

			serializedObject.UpdateIfDirtyOrScript();

			AnimatorOverrideController animatorOverrideController = target as AnimatorOverrideController;
			RuntimeAnimatorController  runtimeAnimatorController  = m_Controller.hasMultipleDifferentValues ? null : animatorOverrideController.runtimeAnimatorController;

            EditorGUI.BeginChangeCheck();
			runtimeAnimatorController = EditorGUILayout.ObjectField("Controller", runtimeAnimatorController, typeof(AnimatorController), false) as RuntimeAnimatorController;
			if(EditorGUI.EndChangeCheck())
			{
				for (int i = 0; i < targets.Length;i++ )
				{
					AnimatorOverrideController controller = targets[i] as AnimatorOverrideController;
					controller.runtimeAnimatorController = runtimeAnimatorController;
				}
				
				changeCheck = true;
			}

			EditorGUI.BeginDisabledGroup(m_Controller == null || (isEditingMultipleObjects && m_Controller.hasMultipleDifferentValues) || runtimeAnimatorController == null);
			{
				EditorGUI.BeginChangeCheck();
				m_Clips = animatorOverrideController.clips;
				m_ClipList.list = m_Clips; 
				m_ClipList.DoList();
				if (EditorGUI.EndChangeCheck())
				{
					for(int i=0;i<targets.Length;i++)
					{
						AnimatorOverrideController controller = targets[i] as AnimatorOverrideController;
						controller.clips = m_Clips;
					}
					changeCheck = true;
				}
			}
			EditorGUI.EndDisabledGroup();

			if (changeCheck)
				animatorOverrideController.PerformOverrideClipListCleanup();
		}
		
		private void DrawClipElement(Rect rect, int index, bool selected, bool focused)
		{
			AnimationClip originalClip = m_Clips[index].originalClip;
			AnimationClip overrideClip = m_Clips[index].overrideClip;

		    rect.xMax = rect.xMax / 2.0f;
			GUI.Label(rect, originalClip.name, EditorStyles.label);
		    rect.xMin = rect.xMax;
		    rect.xMax *= 2.0f;

			EditorGUI.BeginChangeCheck();
			overrideClip = EditorGUI.ObjectField(rect, "", overrideClip, typeof(AnimationClip), false) as AnimationClip;
			if (EditorGUI.EndChangeCheck())
				m_Clips[index].overrideClip = overrideClip;      	
		}

		private void DrawClipHeader(Rect rect)
		{
			rect.xMax = rect.xMax / 2.0f;
			GUI.Label(rect, "Original", EditorStyles.label);
			rect.xMin = rect.xMax;
			rect.xMax *= 2.0f;
			GUI.Label(rect, "Override", EditorStyles.label);
		}
	}
}
