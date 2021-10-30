using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{
	public sealed partial class EditorGUILayout
	{
		// Gradient versions
		internal static Gradient GradientField(Gradient value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(EditorGUIUtility.fieldWidth, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(r, value);
		}

		internal static Gradient GradientField(string label, Gradient value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(label, r, value);
		}

		internal static Gradient GradientField(GUIContent label, Gradient value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(label, r, value);
		}

		// SerializedProperty versions
		internal static Gradient GradientField(SerializedProperty value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(r, value);
		}

		internal static Gradient GradientField(string label, SerializedProperty value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(label, r, value);
		}

		internal static Gradient GradientField(GUIContent label, SerializedProperty value, params GUILayoutOption[] options)
		{
			Rect r = s_LastRect = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, EditorGUI.kSingleLineHeight, EditorGUI.kSingleLineHeight, EditorStyles.colorField, options);
			return EditorGUI.GradientField(label, r, value);
		}
	}


	public sealed partial class EditorGUI
	{
		static readonly int s_GradientHash = "s_GradientHash".GetHashCode();
		static int s_GradientID;

		// Gradient versions
		internal static Gradient GradientField(Rect position, Gradient gradient)
		{
			int id = EditorGUIUtility.GetControlID(s_GradientHash, EditorGUIUtility.native, position);
			return DoGradientField(position, id, gradient, null);
		}

		internal static Gradient GradientField(string label, Rect position, Gradient gradient)
		{
			return GradientField(EditorGUIUtility.TempContent(label), position, gradient);
		}

		internal static Gradient GradientField(GUIContent label, Rect position, Gradient gradient)
		{
			int id = EditorGUIUtility.GetControlID(s_GradientHash, EditorGUIUtility.native, position);
			return DoGradientField(PrefixLabel(position, id, label), id, gradient, null);
		}

		// SerializedProperty versions
		internal static Gradient GradientField(Rect position, SerializedProperty gradient)
		{
			int id = EditorGUIUtility.GetControlID(s_GradientHash, EditorGUIUtility.native, position);
			return DoGradientField(position, id, null, gradient);
		}

		internal static Gradient GradientField(string label, Rect position, SerializedProperty property)
		{
			return GradientField(EditorGUIUtility.TempContent(label), position, property);
		}

		internal static Gradient GradientField(GUIContent label, Rect position, SerializedProperty property)
		{
			int id = EditorGUIUtility.GetControlID(s_GradientHash, EditorGUIUtility.native, position);
			return DoGradientField(PrefixLabel(position, id, label), id, null, property);
		}

		internal static Gradient DoGradientField (Rect position, int id, Gradient value, SerializedProperty property)
		{
			Event evt = Event.current;

			switch (evt.GetTypeForControl(id))
			{
				case EventType.MouseDown:
					if (position.Contains(evt.mousePosition))
					{
						if (evt.button == 0)
						{
							s_GradientID = id;
							GUIUtility.keyboardControl = id;
							Gradient gradient = property != null ? property.gradientValue : value;
							GradientPicker.Show(gradient);
							GUIUtility.ExitGUI();
						}
						else if (evt.button == 1)
						{
							if (property != null)
								GradientContextMenu.Show (property);
							// TODO: make work for Gradient value
						}
					}
					break;
				case EventType.Repaint:
					{
						Rect r2 = new Rect(position.x +1, position.y + 1, position.width - 2, position.height - 2); // Adjust for box drawn on top
						if (property != null)
							GradientEditor.DrawGradientSwatch(r2, property, Color.white);
						else
							GradientEditor.DrawGradientSwatch(r2, value, Color.white);
						EditorStyles.colorPickerBox.Draw(position, GUIContent.none, id);
						break;
					}
				case EventType.ExecuteCommand:
					if (s_GradientID == id)
					{
						switch (evt.commandName)
						{
							case "GradientPickerChanged":
								GUI.changed = true;
								GradientPreviewCache.ClearCache();
								HandleUtility.Repaint();
								if (property != null)
									property.gradientValue = GradientPicker.gradient;
							
								return GradientPicker.gradient;
						}
					}
					break;
				case EventType.KeyDown:
					if (GUIUtility.keyboardControl == id && (evt.keyCode == KeyCode.Space || evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter))
					{
						Event.current.Use();
						Gradient gradient = property != null ? property.gradientValue : value;
						GradientPicker.Show(gradient);
						GUIUtility.ExitGUI();
					}
					break;
			}
			return value;
		}
	}
}
