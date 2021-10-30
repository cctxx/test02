using UnityEngine;

namespace UnityEditor
{
	internal class StructPropertyGUILayout
	{
		internal static void JointSpring (SerializedProperty property, params GUILayoutOption[] options)
		{
			GenericStruct (property, options);
		}

		internal static void WheelFrictionCurve (SerializedProperty property, params GUILayoutOption[] options)
		{
			GenericStruct (property, options);
		}

		internal static void GenericStruct (SerializedProperty property, params GUILayoutOption[] options)
		{
			float height = EditorGUI.kStructHeaderLineHeight + EditorGUI.kSingleLineHeight * GetChildrenCount (property);
			Rect rect = GUILayoutUtility.GetRect (EditorGUILayout.kLabelFloatMinW, EditorGUILayout.kLabelFloatMaxW,
												  height, height, EditorStyles.layerMaskField, options);

			StructPropertyGUI.GenericStruct (rect, property);
		}

		internal static int GetChildrenCount (SerializedProperty property)
		{
			int propertyDepth = property.depth;

			SerializedProperty iterator = property.Copy ();
			iterator.NextVisible (true);

			int count = 0;
			while (iterator.depth == propertyDepth + 1)
			{
				count++;
				iterator.NextVisible (false);
			}

			return count;
		}
	}

	internal class StructPropertyGUI
	{
		internal static void JointSpring (Rect position, SerializedProperty property)
		{
			GenericStruct (position, property);
		}

		internal static void WheelFrictionCurve (Rect position, SerializedProperty property)
		{
			GenericStruct (position, property);
		}

		internal static void GenericStruct (Rect position, SerializedProperty property)
		{
			GUI.Label (EditorGUI.IndentedRect (position), property.displayName, EditorStyles.label);
			position.y += EditorGUI.kStructHeaderLineHeight;

			DoChildren (position, property);
		}

		private static void DoChildren (Rect position, SerializedProperty property)
		{
			float propertyDepth = property.depth;

			position.height = EditorGUI.kSingleLineHeight;

			EditorGUI.indentLevel++;

			SerializedProperty iterator = property.Copy ();
			iterator.NextVisible (true);
			while (iterator.depth == propertyDepth + 1)
			{
				EditorGUI.PropertyField (position, iterator);
				position.y += EditorGUI.kSingleLineHeight;
				iterator.NextVisible (false);
			}

			EditorGUI.indentLevel--;

			EditorGUILayout.Space ();
		}
	}
}
