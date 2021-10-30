using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	public static class SpriteEditorHandles
	{
		private static Vector2 s_CurrentMousePosition;
		private static Vector2 s_DragStartScreenPosition;
		private static Vector2 s_DragScreenOffset;

		static internal Vector2 ScaleSlider (Vector2 pos, Vector2 direction, MouseCursor cursor, bool renderHandle, Rect cursorRect, GUIStyle dragDot, GUIStyle dragDotActive)
		{
			int id = GUIUtility.GetControlID ("Slider1D".GetHashCode (), FocusType.Keyboard);
			Vector2 screenVal = Handles.matrix.MultiplyPoint (pos);

			Rect handleScreenPos = new Rect (
				screenVal.x - dragDot.fixedWidth * .5f,
				screenVal.y - dragDot.fixedHeight * .5f,
				dragDot.fixedWidth,
				dragDot.fixedHeight
				);

			if (renderHandle)
				cursorRect = handleScreenPos;

			Event evt = Event.current;
			switch (evt.GetTypeForControl (id))
			{
				case EventType.mouseDown:
					// am I closest to the thingy?
					if (evt.button == 0 &&
						cursorRect.Contains (Event.current.mousePosition) &&
						!evt.alt)
					{
						GUIUtility.hotControl = GUIUtility.keyboardControl = id;	// Grab mouse focus
						s_CurrentMousePosition = evt.mousePosition;
						s_DragStartScreenPosition = evt.mousePosition;
						s_DragScreenOffset = s_CurrentMousePosition - handleScreenPos.center;
						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (1);
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition += evt.delta;
						Vector2 oldPos = pos;
						pos = Handles.s_InverseMatrix.MultiplyPoint (s_CurrentMousePosition);
						if (!Mathf.Approximately ((oldPos - pos).magnitude, 0f))
							GUI.changed = true;
						evt.Use ();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (0);
					}
					break;
				case EventType.keyDown:
					if (GUIUtility.hotControl == id)
					{
						if (evt.keyCode == KeyCode.Escape)
						{
							pos = Handles.s_InverseMatrix.MultiplyPoint (s_DragStartScreenPosition - s_DragScreenOffset);
							GUIUtility.hotControl = 0;
							GUI.changed = true;
							evt.Use ();
						}
					}
					break;
				case EventType.repaint:
					EditorGUIUtility.AddCursorRect (cursorRect, cursor, id);

					if (renderHandle)
					{
						if (GUIUtility.hotControl == id)
							dragDotActive.Draw (handleScreenPos, GUIContent.none, id);
						else
							dragDot.Draw (handleScreenPos, GUIContent.none, id);

					}
					break;
			}
			return pos;
		}

		static internal Vector2 PivotSlider (Rect sprite, Vector2 pos, GUIStyle pivotDot, GUIStyle pivotDotActive)
		{
			int id = GUIUtility.GetControlID ("Slider1D".GetHashCode (), FocusType.Keyboard);

			// Convert from normalized space to texture space
			pos = new Vector2 (sprite.xMin + sprite.width * pos.x, sprite.yMin + sprite.height * pos.y);

			Vector2 screenVal = Handles.matrix.MultiplyPoint (pos);

			Rect handleScreenPos = new Rect (
				screenVal.x - pivotDot.fixedWidth * .5f,
				screenVal.y - pivotDot.fixedHeight * .5f,
				pivotDotActive.fixedWidth,
				pivotDotActive.fixedHeight
				);

			Event evt = Event.current;
			switch (evt.GetTypeForControl (id))
			{
				case EventType.mouseDown:
					// am I closest to the thingy?
					if (evt.button == 0 && handleScreenPos.Contains (Event.current.mousePosition) && !evt.alt)
					{
						GUIUtility.hotControl = GUIUtility.keyboardControl = id;	// Grab mouse focus
						s_CurrentMousePosition = evt.mousePosition;
						s_DragStartScreenPosition = evt.mousePosition;
						Vector2 rectScreenCenter = Handles.matrix.MultiplyPoint (pos);
						s_DragScreenOffset = s_CurrentMousePosition - rectScreenCenter;
						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (1);
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition += evt.delta;
						Vector2 oldPos = pos;
						Vector3 scrPos = Handles.s_InverseMatrix.MultiplyPoint (s_CurrentMousePosition - s_DragScreenOffset);
						pos = new Vector2 (scrPos.x, scrPos.y);
						if (!Mathf.Approximately ((oldPos - pos).magnitude, 0f))
							GUI.changed = true;
						evt.Use ();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (0);
					}
					break;
				case EventType.keyDown:
					if (GUIUtility.hotControl == id)
					{
						if (evt.keyCode == KeyCode.Escape)
						{
							pos = Handles.s_InverseMatrix.MultiplyPoint (s_DragStartScreenPosition - s_DragScreenOffset);
							GUIUtility.hotControl = 0;
							GUI.changed = true;
							evt.Use ();
						}
					}
					break;
				case EventType.repaint:
					EditorGUIUtility.AddCursorRect (handleScreenPos, MouseCursor.Arrow, id);

					if (GUIUtility.hotControl == id)
						pivotDotActive.Draw (handleScreenPos, GUIContent.none, id);
					else
						pivotDot.Draw (handleScreenPos, GUIContent.none, id);

					break;
			}

			// Convert from texture space back to normalized space
			pos = new Vector2 ((pos.x - sprite.xMin) / sprite.width, (pos.y - sprite.yMin) / sprite.height);

			return pos;
		}

		static internal Rect SliderRect (Rect pos)
		{
			int id = GUIUtility.GetControlID ("SliderRect".GetHashCode (), FocusType.Keyboard);

			Event evt = Event.current;

			// SpriteEditorWindow is telling us we got selected and so we fake a mousedown on our Repaint event to get "one-click dragging" going on
			if (SpriteEditorWindow.s_OneClickDragStarted && evt.type == EventType.Repaint)
			{
				HandleSliderRectMouseDown (id, evt, pos);
				SpriteEditorWindow.s_OneClickDragStarted = false;
			}

			switch (evt.GetTypeForControl (id))
			{
				case EventType.mouseDown:
					// am I closest to the thingy?
					if (evt.button == 0 && pos.Contains (Handles.s_InverseMatrix.MultiplyPoint (Event.current.mousePosition)) && !evt.alt)
					{
						HandleSliderRectMouseDown (id, evt, pos);
						evt.Use ();
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition += evt.delta;

						Vector2 oldCenter = pos.center;
						pos.center = Handles.s_InverseMatrix.MultiplyPoint (s_CurrentMousePosition - s_DragScreenOffset);
						if (!Mathf.Approximately ((oldCenter - pos.center).magnitude, 0f))
							GUI.changed = true;

						evt.Use ();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						GUIUtility.hotControl = 0;
						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (0);
					}
					break;
				case EventType.keyDown:
					if (GUIUtility.hotControl == id)
					{
						if (evt.keyCode == KeyCode.Escape)
						{
							pos.center = Handles.s_InverseMatrix.MultiplyPoint (s_DragStartScreenPosition - s_DragScreenOffset);
							GUIUtility.hotControl = 0;
							GUI.changed = true;
							evt.Use ();
						}
					}
					break;
				case EventType.repaint:
					Vector2 topleft = Handles.s_InverseMatrix.MultiplyPoint (new Vector2 (pos.xMin, pos.yMin));
					Vector2 bottomright = Handles.s_InverseMatrix.MultiplyPoint (new Vector2 (pos.xMax, pos.yMax));
					EditorGUIUtility.AddCursorRect (new Rect (topleft.x, topleft.y, bottomright.x - topleft.x, bottomright.y - topleft.y), MouseCursor.Arrow, id);
					break;
			}

			return pos;
		}

		static internal void HandleSliderRectMouseDown (int id, Event evt, Rect pos)
		{
			GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
			s_CurrentMousePosition = evt.mousePosition;
			s_DragStartScreenPosition = evt.mousePosition;

			Vector2 rectScreenCenter = Handles.matrix.MultiplyPoint (pos.center);
			s_DragScreenOffset = s_CurrentMousePosition - rectScreenCenter;

			EditorGUIUtility.SetWantsMouseJumping (1);
		}

		
		static int s_RectSelectionID = GUIUtility.GetPermanentControlID ();

		static internal Rect RectCreator (float textureWidth, float textureHeight, GUIStyle rectStyle)
		{
			Event evt = Event.current;
			Vector2 mousePos = evt.mousePosition;
			int id = s_RectSelectionID;
			Rect result = new Rect ();

			switch (evt.GetTypeForControl (id))
			{
				case EventType.mouseDown:
					if (evt.button == 0)
					{
						GUIUtility.hotControl = id;

						// Make sure that the starting position is clamped to inside texture area
						Rect textureArea = new Rect(0, 0, textureWidth, textureHeight);						
						Vector2 point = Handles.s_InverseMatrix.MultiplyPoint(mousePos);
						
						point.x = Mathf.Min(Mathf.Max(point.x, textureArea.xMin), textureArea.xMax);
						point.y = Mathf.Min(Mathf.Max(point.y, textureArea.yMin), textureArea.yMax);						
						
						// Save clamped starting position for later use
						s_DragStartScreenPosition = Handles.s_Matrix.MultiplyPoint(point);

						// Actual position
						s_CurrentMousePosition = mousePos;

						evt.Use ();
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition = new Vector2 (mousePos.x, mousePos.y);
						evt.Use ();
					}
					break;

				case EventType.repaint:
					if (GUIUtility.hotControl == id && ValidRect (s_DragStartScreenPosition, s_CurrentMousePosition))
					{
						// TODO: use rectstyle
						//rectStyle.Draw (GetCurrentRect (true, textureWidth, textureHeight, s_DragStartScreenPosition, s_CurrentMousePosition), GUIContent.none, false, false, false, false);
						SpriteEditorUtility.BeginLines (Color.green * 1.5f);
						SpriteEditorUtility.DrawBox (GetCurrentRect (false, textureWidth, textureHeight, s_DragStartScreenPosition, s_CurrentMousePosition));
						SpriteEditorUtility.EndLines ();
					}
					break;

				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && evt.button == 0)
					{
						if (ValidRect (s_DragStartScreenPosition, s_CurrentMousePosition))
						{
							result = GetCurrentRect (false, textureWidth, textureHeight, s_DragStartScreenPosition, s_CurrentMousePosition);
							GUI.changed = true;
							evt.Use ();
						}

						GUIUtility.hotControl = 0;
					}
					break;
				case EventType.keyDown:
					if (GUIUtility.hotControl == id)
					{
						if (evt.keyCode == KeyCode.Escape)
						{
							GUIUtility.hotControl = 0;
							GUI.changed = true;
							evt.Use ();
						}
					}
					break;
			}
			return result;
		}

		static private bool ValidRect (Vector2 startPoint, Vector2 endPoint)
		{
			return Mathf.Abs ((endPoint - startPoint).x) > 5f && Mathf.Abs ((endPoint - startPoint).y) > 5f;
		}

		static private Rect GetCurrentRect (bool screenSpace, float textureWidth, float textureHeight, Vector2 startPoint, Vector2 endPoint)
		{
			Rect r = EditorGUIExt.FromToRect (Handles.s_InverseMatrix.MultiplyPoint (startPoint), Handles.s_InverseMatrix.MultiplyPoint (endPoint));
			r = SpriteEditorUtility.ClampedRect (SpriteEditorUtility.RoundToInt (r), new Rect (0, 0, textureWidth, textureHeight), false);

			if (screenSpace)
			{
				Vector2 topleft = Handles.matrix.MultiplyPoint (new Vector2 (r.xMin, r.yMin));
				Vector2 bottomright = Handles.matrix.MultiplyPoint (new Vector2 (r.xMax, r.yMax));

				r = new Rect (topleft.x, topleft.y, bottomright.x - topleft.x, bottomright.y - topleft.y);
			}
			return r;
		}
	}
}
