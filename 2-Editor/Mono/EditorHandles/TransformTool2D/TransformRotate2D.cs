#if ENABLE_SPRITES
using UnityEngine;

namespace UnityEditor
{
	internal class TransformRotate2D
	{
		public const float kHandleSizeMultiplier = 0.25f;
		public const float guiSize = 30f;

		public static void OnGUI()
		{
			Vector3 rotationPivot = TransformTool2D.pivot.transform.position;

			float before = TransformTool2D.pivot.transform.eulerAngles.z;

			for (int i = 0; i < 4; i++)
			{
				EditorGUI.BeginChangeCheck();
				float after = RotateSlider2D.Do(before, rotationPivot, TransformTool2D.rectSpaceCursorRects[i + 10].rect);
				if (EditorGUI.EndChangeCheck())
				{
					TransformTool2D.pivot.rotation *= Quaternion.AngleAxis(before - after, Vector3.back);
				}
			}
		}
		
		private class RotateSlider2D
		{
			private static Vector2 s_StartMousePosition, s_CurrentMousePosition;
			private static float s_StartRotation;
			private static float s_RotationDist;

			public static float Do(float rotation, Vector3 pivotWorldPos, Rect handleLocalRect)
			{
				int id = GUIUtility.GetControlID("RotateSlider2D".GetHashCode(), FocusType.Keyboard);

				Event evt = Event.current;
				switch (evt.GetTypeForControl(id))
				{
					case EventType.layout:
						if (handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
							HandleUtility.AddControl(id, 0f);
						break;
					case EventType.mouseDown:
						if (handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
						{
							GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
							Tools.LockHandlePosition();

							s_RotationDist = 0;
							s_StartRotation = rotation;
							s_CurrentMousePosition = s_StartMousePosition = Event.current.mousePosition;
							evt.Use();
							EditorGUIUtility.SetWantsMouseJumping(1);
						}
						break;
					case EventType.mouseDrag:
						if (GUIUtility.hotControl == id)
						{
							s_CurrentMousePosition += evt.delta;
							Vector3 oldTangent = (TransformTool2DUtility.ScreenToWorld(s_StartMousePosition) - pivotWorldPos);
							Vector3 newTangent = (TransformTool2DUtility.ScreenToWorld(s_CurrentMousePosition) - pivotWorldPos);

							s_RotationDist = Mathf.Rad2Deg * (Mathf.Atan2(newTangent.y, newTangent.x) - Mathf.Atan2(oldTangent.y, oldTangent.x));

							if (evt.shift)
								s_RotationDist = Mathf.Round(s_RotationDist / 15f) * 15f;

							rotation = s_StartRotation + s_RotationDist;
							
							GUI.changed = true;
							evt.Use();
						}
						break;
					case EventType.mouseUp:
						if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
						{
							Tools.UnlockHandlePosition();
							GUIUtility.hotControl = 0;
							evt.Use();
							EditorGUIUtility.SetWantsMouseJumping(0);
						}
						break;
				}
				return rotation;
			}
		}
	}
}
#endif