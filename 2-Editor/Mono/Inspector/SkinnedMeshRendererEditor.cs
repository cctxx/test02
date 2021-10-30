using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(SkinnedMeshRenderer))]
	[CanEditMultipleObjects]
	internal class SkinnedMeshRendererEditor : Editor
	{
		private static int s_BoxHash = "SkinnedMeshRendererEditor".GetHashCode();

		SerializedProperty m_CastShadows;
		SerializedProperty m_ReceiveShadows;
		SerializedProperty m_Materials;
		SerializedProperty m_UseLightProbes;
		SerializedProperty m_LightProbeAnchor;
		SerializedProperty m_AABB;
		SerializedProperty m_DirtyAABB;
		SerializedProperty m_BlendShapeWeights;

		private BoxEditor m_BoxEditor = new BoxEditor(false, s_BoxHash);

		public void OnEnable()
		{
			m_CastShadows = serializedObject.FindProperty("m_CastShadows");
			m_ReceiveShadows = serializedObject.FindProperty("m_ReceiveShadows");
			m_Materials = serializedObject.FindProperty("m_Materials");
			m_UseLightProbes = serializedObject.FindProperty("m_UseLightProbes");
			m_LightProbeAnchor = serializedObject.FindProperty("m_LightProbeAnchor");
			m_BlendShapeWeights = serializedObject.FindProperty("m_BlendShapeWeights");
			m_AABB = serializedObject.FindProperty("m_AABB");
			m_DirtyAABB = serializedObject.FindProperty("m_DirtyAABB");
			m_BoxEditor.OnEnable();
			m_BoxEditor.SetAlwaysDisplayHandles (true);
		}

		public void OnDisable ()
		{
			m_BoxEditor.OnDisable();
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update ();

			OnBlendShapeUI ();
			
			EditorGUILayout.PropertyField (m_CastShadows);
			EditorGUILayout.PropertyField (m_ReceiveShadows);
			EditorGUILayout.PropertyField (m_Materials, true);
			EditorGUILayout.PropertyField (m_UseLightProbes);
			if (m_UseLightProbes.boolValue)
			{
				EditorGUI.indentLevel++;
				EditorGUILayout.PropertyField (m_LightProbeAnchor, new GUIContent ("Anchor Override", m_LightProbeAnchor.tooltip));
				EditorGUI.indentLevel--;
			}


			DrawPropertiesExcluding (serializedObject,
				"m_CastShadows",
				"m_ReceiveShadows",
				"m_Materials",
				"m_UseLightProbes",
				"m_LightProbeAnchor",
				"m_BlendShapeWeights",
				"m_AABB"
			);

			EditorGUI.BeginChangeCheck ();
			EditorGUILayout.PropertyField(m_AABB, new GUIContent("Bounds"));
			// If we set m_AABB then we need to set m_DirtyAABB to false
			if (EditorGUI.EndChangeCheck ())
				m_DirtyAABB.boolValue = false;

			serializedObject.ApplyModifiedProperties ();
		}

		public void OnBlendShapeUI()
		{
			SkinnedMeshRenderer renderer = (SkinnedMeshRenderer)target;
			int blendShapeCount = renderer.sharedMesh == null ? 0 : renderer.sharedMesh.blendShapeCount;
			if (blendShapeCount == 0)
				return;
			
			GUIContent content = new GUIContent();
			content.text = "BlendShapes";
			
			EditorGUILayout.PropertyField (m_BlendShapeWeights, content, false);
			if (!m_BlendShapeWeights.isExpanded)
				return;

			EditorGUI.indentLevel++;	
			Mesh m = renderer.sharedMesh;
			
			int arraySize = m_BlendShapeWeights.arraySize;
			for (int i=0;i<blendShapeCount;i++)
			{
				content.text = m.GetBlendShapeName(i);

				/// The SkinnedMeshRenderer blendshape weights array size can be out of sync with the size defined in the mesh
				///  (default values in that case are 0)
				/// The desired behaviour is to resize the blendshape array on edit.
				
				// Default path when the blend shape array size is big enough.
				if (i < arraySize)
					EditorGUILayout.PropertyField (m_BlendShapeWeights.GetArrayElementAtIndex(i), content);
				// Fall back to 0 based editing & 
				else
				{
					EditorGUI.BeginChangeCheck ();

					float value = EditorGUILayout.FloatField (content, 0.0F);
					if (EditorGUI.EndChangeCheck ())
					{
						// m_BlendShapeWeights.ResizeArrayWithZero(arraySize);
						m_BlendShapeWeights.arraySize = blendShapeCount;
						arraySize = blendShapeCount;
						m_BlendShapeWeights.GetArrayElementAtIndex(i).floatValue = value;
					}
				}
			}
			
			EditorGUI.indentLevel--;
		}

		public void OnSceneGUI()
		{
			SkinnedMeshRenderer renderer = (SkinnedMeshRenderer)target;
			
			if (renderer.updateWhenOffscreen)
			{
				Bounds bounds = renderer.bounds;
				Vector3 center = bounds.center;
				Vector3 size = bounds.size;
				
				m_BoxEditor.DrawWireframeBox(center, size);
			}
			else
			{
				Bounds bounds = renderer.localBounds;
				Vector3 center = bounds.center;
				Vector3 size = bounds.size;
				
				if (m_BoxEditor.OnSceneGUI(renderer.actualRootBone, Handles.s_BoundingBoxHandleColor, false, ref center, ref size))
				{
					Undo.RecordObject (renderer, "Resize Bounds");
					renderer.localBounds = new Bounds(center, size);
				}
			}
		}
	}
}
