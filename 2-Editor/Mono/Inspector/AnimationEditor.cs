using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(Animation))]
	[CanEditMultipleObjects]
	internal class AnimationEditor : Editor
	{
		private static int s_BoxHash = "AnimationBoundsEditorHash".GetHashCode();
		private BoxEditor m_BoxEditor = new BoxEditor(false, s_BoxHash);
		private int m_PrePreviewAnimationArraySize = -1;

		SerializedProperty m_UserAABB;

		public void OnEnable()
		{
			m_UserAABB = serializedObject.FindProperty("m_UserAABB");
			m_PrePreviewAnimationArraySize = -1;
			m_BoxEditor.OnEnable();
		}

		public void OnDisable ()
		{
			m_BoxEditor.OnDisable();
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update();

			SerializedProperty clipProperty = serializedObject.FindProperty("m_Animation");
			
			EditorGUILayout.PropertyField(clipProperty, true);
			int newAnimID = clipProperty.objectReferenceInstanceIDValue;

			SerializedProperty arrProperty = serializedObject.FindProperty("m_Animations");
			int arrSize = arrProperty.arraySize;
			
			// Remember the array size when ObjectSelector becomes visible
			if (ObjectSelector.isVisible && m_PrePreviewAnimationArraySize == -1)
				m_PrePreviewAnimationArraySize = arrSize;

			// Make sure the array is the original array size + 1 at max (+1 for the ObjectSelector preview slot)
			if (m_PrePreviewAnimationArraySize != -1)
			{
				// Always resize if the last anim element is not the current animation
				int lastAnimID = arrSize > 0 ? arrProperty.GetArrayElementAtIndex(arrSize-1).objectReferenceInstanceIDValue : -1;
				if (lastAnimID != newAnimID)
					arrProperty.arraySize = m_PrePreviewAnimationArraySize;
				if (!ObjectSelector.isVisible)
					m_PrePreviewAnimationArraySize = -1;
			}

			DrawPropertiesExcluding(serializedObject, "m_Animation", "m_UserAABB");

			Animation animation = (Animation)target;
			
			if (animation && animation.cullingType == AnimationCullingType.BasedOnUserBounds)
			{
				EditorGUILayout.PropertyField(m_UserAABB, new GUIContent("Bounds"));

				if (GUI.changed)
					EditorUtility.SetDirty(target);
			}

			serializedObject.ApplyModifiedProperties();
		}
		
		// A minimal list of settings to be shown in the Asset Store preview inspector
		internal override void OnAssetStoreInspectorGUI ()
		{
			OnInspectorGUI();
		}
		
		
		public void OnSceneGUI()
		{
			Animation animation = (Animation)target;
			if (animation && 
				(animation.cullingType == AnimationCullingType.BasedOnClipBounds ||
				 animation.cullingType == AnimationCullingType.BasedOnUserBounds))
			{
				m_BoxEditor.SetAlwaysDisplayHandles (animation.cullingType == AnimationCullingType.BasedOnUserBounds);

				Bounds bounds = animation.localBounds;
				Vector3 center = bounds.center;
				Vector3 size = bounds.size;

				if (m_BoxEditor.OnSceneGUI(animation.transform, Handles.s_BoundingBoxHandleColor, ref center, ref size))
				{
					Undo.RecordObject (animation, "Modified Animation bounds");
					animation.localBounds = new Bounds(center, size);
				}

			}
		}
	}
}
