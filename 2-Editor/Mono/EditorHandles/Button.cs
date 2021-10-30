using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class Button
	{
		public static bool Do (int id, Vector3 position, Quaternion direction, float size, float pickSize, Handles.DrawCapFunction capFunc)
		{
			Event evt = Event.current;

			switch (evt.GetTypeForControl(id))
			{
				case EventType.layout:
					if (GUI.enabled)
						HandleUtility.AddControl(id, HandleUtility.DistanceToCircle(position, pickSize));
					break;
				case EventType.mouseMove:
					if ((HandleUtility.nearestControl == id && evt.button == 0) || (GUIUtility.keyboardControl == id && evt.button == 2))
						HandleUtility.Repaint();
					break;
				case EventType.mouseDown:
					// am I closest to the thingy?
					if (HandleUtility.nearestControl == id)
					{
						GUIUtility.hotControl = id;	// Grab mouse focus
						evt.Use();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						evt.Use();
						
						if (HandleUtility.nearestControl == id)
							return true;
					}
					break;
				case EventType.repaint:
					Color origColor = Handles.color;
					if (HandleUtility.nearestControl == id && GUI.enabled)
						Handles.color = Handles.selectedColor;
						
					capFunc(id, position, direction, size);
					
					Handles.color = origColor;
					break;
			}
			return false;
		}
	}
}
