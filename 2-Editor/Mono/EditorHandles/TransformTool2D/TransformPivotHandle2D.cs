using UnityEngine;

namespace UnityEditor
{
	internal class TransformPivotHandle2D
	{
		private static Vector3 s_StartOffset;
		private static bool s_Dragging;

		internal static Vector3 Do(int id, Vector3 position, Rect handleLocalRect, float alpha)
		{
			if (Mathf.Approximately(alpha, 0f) && !s_Dragging)
				return position;

			Event evt = Event.current;

			switch (evt.GetTypeForControl(id))
			{
				case EventType.layout:
					if (handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
						HandleUtility.AddControl(id, 0f);

					break;
				case EventType.mouseDown:
					if (HandleUtility.nearestControl == id && evt.button == 0 || 
						GUIUtility.keyboardControl == id && evt.button == 2 || 
						handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
					{
						GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(1);
						s_StartOffset = TransformTool2DUtility.ScreenToWorld(evt.mousePosition) - position;
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						position = TransformTool2DUtility.ScreenToWorld(evt.mousePosition) - s_StartOffset;
						GUI.changed = true;
						s_Dragging = true;
						evt.Use();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						s_Dragging = false;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(0);
					}
					break;
				case EventType.keyDown:
					if (GUIUtility.hotControl == id)
					{
						if (evt.keyCode == KeyCode.Escape)
						{
							GUIUtility.hotControl = 0;
							GUI.changed = true;
							s_Dragging = false;
							evt.Use();
						}
					}
					break;
				case EventType.Repaint:
					Vector3 screenPos = TransformTool2DUtility.WorldToScreen(position);
					Color oldColor = GUI.color;
					GUI.color = new Color(1f, 1f, 1f, alpha);

					Handles.BeginGUI();
					float w = TransformTool2D.s_Styles.pivotdot.fixedWidth;
					float h = TransformTool2D.s_Styles.pivotdot.fixedHeight;
					Rect r = new Rect(screenPos.x - w / 2f, screenPos.y - h / 2f, w, h);

					if (GUIUtility.hotControl == id)
						TransformTool2D.s_Styles.pivotdotactive.Draw(r, GUIContent.none, id);
					else
						TransformTool2D.s_Styles.pivotdot.Draw(r, GUIContent.none, id);

					Handles.EndGUI();

					GUI.color = oldColor;
					break;
			}
			return position;
		}
	}
}