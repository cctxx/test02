using UnityEngine;
using UnityEditor;

namespace UnityEditor
{
	[CustomEditor(typeof(Transform))]
	[CanEditMultipleObjects]
	internal class TransformInspector : Editor
	{
		private Vector3 m_EulerAngles;
		// Some random rotation that will never be the same as the current one
		private Quaternion m_OldQuaternion = new Quaternion (1234,1000,4321,-1000);
		
		SerializedProperty m_Position;
		SerializedProperty m_Scale;
		SerializedProperty m_Rotation;
		
		static int s_LabelHash = "TransformLabel".GetHashCode ();
		
		class Contents
		{
			public GUIContent positionContent = new GUIContent ("Position");
			public GUIContent rotationContent = new GUIContent ("Rotation");
			public GUIContent scaleContent = new GUIContent ("Scale");
			public GUIContent[] subLabels = new GUIContent[] { new GUIContent ("X"), new GUIContent ("Y") };
			public string floatingPointWarning = "Due to floating-point precision limitations, it is recommended to bring the world coordinates of the GameObject within a smaller range.";
		}
		Contents s_Contents;

		public void OnEnable()
		{
			m_Position = serializedObject.FindProperty("m_LocalPosition");
			m_Scale = serializedObject.FindProperty("m_LocalScale");
			m_Rotation = serializedObject.FindProperty("m_LocalRotation");
		}
		
		public override void OnInspectorGUI ()
		{
			if (s_Contents == null)
				s_Contents = new Contents ();
			
			if (!EditorGUIUtility.wideMode)
			{
				EditorGUIUtility.wideMode = true;
				EditorGUIUtility.labelWidth = GUIView.current.position.width - 212;
			}
		
			serializedObject.Update();
			
			Inspector3D();
			// Warning if global position is too large for floating point errors.
			// SanitizeBounds function doesn't even support values beyond 100000
			Transform t = target as Transform;
			Vector3 pos = t.position;
			if (Mathf.Abs (pos.x) > 100000 || Mathf.Abs (pos.y) > 100000 || Mathf.Abs (pos.z) > 100000)
				EditorGUILayout.HelpBox (s_Contents.floatingPointWarning, MessageType.Warning);
			
			serializedObject.ApplyModifiedProperties();
		}

		private void Inspector3D()
		{
			EditorGUILayout.PropertyField (m_Position, s_Contents.positionContent);
			RotationField (false);
			EditorGUILayout.PropertyField (m_Scale, s_Contents.scaleContent);
		}

		private void RotationField (bool onlyShowZ)
		{
			Transform t = target as Transform;
			
			Quaternion quaternion = t.localRotation;
			if (
				// Have to do component-wise comparison because the
				// equality operator assumes normalized quaternions
				m_OldQuaternion.x != quaternion.x ||
				m_OldQuaternion.y != quaternion.y ||
				m_OldQuaternion.z != quaternion.z ||
				m_OldQuaternion.w != quaternion.w
			)
			{
				m_EulerAngles = t.localEulerAngles;
				m_OldQuaternion = quaternion;
			}
			bool differentRotation = false;
			foreach (Transform tr in targets)
				differentRotation |= (tr.localEulerAngles != m_EulerAngles);
			
			Rect r = EditorGUILayout.GetControlRect (true, EditorGUIUtility.singleLineHeight * (EditorGUIUtility.wideMode ? 1 : 2));
			GUIContent label = EditorGUI.BeginProperty (r, s_Contents.rotationContent, m_Rotation);

			EditorGUI.showMixedValue = differentRotation;
			
			EditorGUI.BeginChangeCheck();
			
			if (onlyShowZ)
			{
				int id = EditorGUIUtility.GetControlID (s_LabelHash, FocusType.Keyboard);
				r = EditorGUI.MultiFieldPrefixLabel (r, id, label, 2);
				r.xMin += 1;
				m_EulerAngles = new Vector3 (m_EulerAngles.x, m_EulerAngles.y, EditorGUI.FloatField (r, m_EulerAngles.z));
			}
			else
				m_EulerAngles = EditorGUI.Vector3Field (r, label, m_EulerAngles);

			if (EditorGUI.EndChangeCheck())
			{
				Undo.RecordObjects(targets, "Inspector"); // Generic undo title to be consistent with Position and Scale changes.
				foreach (Transform tr in targets)
				{
					tr.localEulerAngles = m_EulerAngles;
					if (tr.parent != null)
						tr.SendTransformChangedScale(); // force scale update, needed if tr has non-uniformly scaled parent.
				}
				serializedObject.SetIsDifferentCacheDirty();
				m_OldQuaternion = quaternion;
			}
			
			EditorGUI.showMixedValue = false;
			
			EditorGUI.EndProperty ();
		}
	}
}
