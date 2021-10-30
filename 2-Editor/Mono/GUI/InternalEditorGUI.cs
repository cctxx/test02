using UnityEngine;
using System.Collections.Generic;

// NOTE: 
// This file should only contain internal functions of the EditorGUI class
// 

namespace UnityEditor
{
	public sealed partial class EditorGUI
	{
		static int s_ButtonMouseDownHash = "ButtonMouseDown".GetHashCode();
		static int s_MouseDeltaReaderHash = "MouseDeltaReader".GetHashCode();

		// A button that returns true on mouse down - like a popup button
		internal static bool ButtonMouseDown(Rect position, GUIContent content, FocusType fType, GUIStyle style)
		{
			int id = EditorGUIUtility.GetControlID(s_ButtonMouseDownHash, fType, position);
			Event evt = Event.current;
			switch (evt.type)
			{
				case EventType.Repaint:
					if (showMixedValue)
					{
						BeginHandleMixedValueContentColor ();
						style.Draw(position, s_MixedValueContent, id, false);
						EndHandleMixedValueContentColor ();
					}
					else
						style.Draw(position, content, id, false);
					break;
				case EventType.MouseDown:
					if (position.Contains(evt.mousePosition) && evt.button == 0)
					{
						Event.current.Use();
						return true;
					}
					break;
				case EventType.KeyDown:
					if (GUIUtility.keyboardControl == id && evt.character == ' ')
					{
						Event.current.Use();
						return true;
					}
					break;
			}
			return false;
		}

		// Button used for the icon selector where an icon can be selected by pressing and dragging the
		// mouse cursor around to select different icons
		internal static bool IconButton (int id, Rect position, GUIContent content, GUIStyle style) 
		{
			GUIUtility.CheckOnGUI ();
			switch (Event.current.GetTypeForControl (id)) 
			{
			case EventType.MouseDown:
				// If the mouse is inside the button, we say that we're the hot control
				if (position.Contains (Event.current.mousePosition)) 
				{
					GUIUtility.hotControl = id;
					Event.current.Use ();
					return true;
				}
				return false;
			case EventType.MouseUp:
				if (GUIUtility.hotControl == id) 
				{
					GUIUtility.hotControl = 0;

					// If we got the mousedown, the mouseup is ours as well
					// (no matter if the click was in the button or not)
					Event.current.Use ();

					// But we only return true if the button was actually clicked
					return position.Contains (Event.current.mousePosition);
				}
				return false;
			case EventType.MouseDrag:
				if (position.Contains(Event.current.mousePosition))
				{
					GUIUtility.hotControl = id;
					Event.current.Use ();
					return true;
				}
				break;
			case EventType.Repaint:
				style.Draw (position, content, id);
				break;
			}
			return false;
		}

		// Get mouse delta values in different situations when click-dragging 
		static Vector2 s_MouseDeltaReaderLastPos;
		internal static Vector2 MouseDeltaReader(Rect position, bool activated)
		{
			int id = EditorGUIUtility.GetControlID(s_MouseDeltaReaderHash, FocusType.Passive, position);
			Event evt = Event.current;
			switch (evt.GetTypeForControl(id))
			{
				case EventType.MouseDown:
					if (activated && GUIUtility.hotControl == 0 && position.Contains(evt.mousePosition) && evt.button == 0)
					{
						GUIUtility.hotControl = id;
						GUIUtility.keyboardControl = 0;
						s_MouseDeltaReaderLastPos = GUIClip.Unclip(evt.mousePosition); // We unclip to screenspace to prevent being affected by scrollviews
						evt.Use();
					}
					break;
				case EventType.MouseDrag:
					if (GUIUtility.hotControl == id)
					{
						Vector2 screenPos = GUIClip.Unclip(evt.mousePosition);	// We unclip to screenspace to prevent being affected by scrollviews
						Vector2 delta = (screenPos - s_MouseDeltaReaderLastPos);
						s_MouseDeltaReaderLastPos = screenPos;
						evt.Use();
						return delta;
					}
					break;
				case EventType.MouseUp:
					if (GUIUtility.hotControl == id && evt.button == 0)
					{
						GUIUtility.hotControl = 0;
						evt.Use();
					}
					break;
			}
			return Vector2.zero;
		}

		internal static void GameViewSizePopup (Rect buttonRect, GameViewSizeGroupType groupType, int selectedIndex, System.Action<int, object> itemClickedCallback, GUIStyle guiStyle)
		{
			GameViewSizeGroup group = GameViewSizes.instance.GetGroup (groupType);
			string text = null;
			if (selectedIndex >= 0 && selectedIndex < group.GetTotalCount())
				text = group.GetGameViewSize (selectedIndex).displayText;
			else
				text = "";

			if (EditorGUI.ButtonMouseDown(buttonRect, GUIContent.Temp(text), FocusType.Passive, guiStyle))
			{
				IFlexibleMenuItemProvider menuData = new GameViewSizesMenuItemProvider (groupType);
				FlexibleMenu flexibleMenu = new FlexibleMenu(menuData, selectedIndex, new GameViewSizesMenuModifyItemUI(), itemClickedCallback);
				PopupWindow.Show (buttonRect, flexibleMenu);
			}
		}
		public static void DrawRect(Rect rect, Color color)
		{
			if (Event.current.type != EventType.repaint)
				return;

			Color orgColor = GUI.color;
			GUI.color = GUI.color * color;//((EditorGUIUtility.isProSkin) ? new Color(0.12f, 0.12f, 0.12f, 1.333f) : new Color(0.6f, 0.6f, 0.6f, 1.333f)); // dark : light
			GUI.DrawTexture(rect, EditorGUIUtility.whiteTexture);
			GUI.color = orgColor;
		}
	}
	
	internal struct PropertyGUIData
	{
		public SerializedProperty property;
		public Rect totalPosition;
		public bool wasBoldDefaultFont;
		public bool wasEnabled;
		public Color color;
		public PropertyGUIData (SerializedProperty property, Rect totalPosition, bool wasBoldDefaultFont, bool wasEnabled, Color color)
		{
			this.property = property;
			this.totalPosition = totalPosition;
			this.wasBoldDefaultFont = wasBoldDefaultFont;
			this.wasEnabled = wasEnabled;
			this.color = color;
		}
	}

	internal class DebugUtils
	{
		internal static string ListToString<T> (IEnumerable<T>  list)
		{
			if (list == null)
				return "[null list]";

			string r = "[";
			int count = 0;
			foreach (T item in list)
			{
				if (count != 0)
					r += ", ";
				if (item != null)
					r += item.ToString();
				else
					r += "'null'";
				count++;
			}
			r += "]";
			
			if (count == 0)
				return "[empty list]";
			
			return "(" + count + ") " + r;
		}

	}
}
