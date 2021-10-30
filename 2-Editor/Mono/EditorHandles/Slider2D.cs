using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class Slider2D
	{
		private static Vector2 s_CurrentMousePosition;
		private static Vector3 s_StartPosition;
		private static Vector2 s_StartPlaneOffset;

		// Returns the new handlePos 
		public static Vector3 Do (
			int id,
			Vector3 handlePos,
			Vector3 handleDir,
			Vector3 slideDir1,
			Vector3 slideDir2,
			float handleSize,
			Handles.DrawCapFunction drawFunc,
			float snap,
			bool drawHelper)
		{
			return Do(id, handlePos, new Vector3 (0, 0, 0), handleDir, slideDir1, slideDir2, handleSize, drawFunc, new Vector2(snap,snap), drawHelper);
		}

		// Returns the new handlePos 
		public static Vector3 Do (
			int id,
			Vector3 handlePos,
			Vector3 offset,
			Vector3 handleDir,
			Vector3 slideDir1,
			Vector3 slideDir2,
			float handleSize,
			Handles.DrawCapFunction drawFunc,
			float snap,
			bool drawHelper)
		{
			return Do(id, handlePos, offset, handleDir, slideDir1, slideDir2, handleSize, drawFunc, new Vector2(snap,snap), drawHelper);
		}

		// Returns the new handlePos 
		public static Vector3 Do (
			int id,
			Vector3 handlePos,
			Vector3 offset,
			Vector3 handleDir,
			Vector3 slideDir1,
			Vector3 slideDir2,
			float handleSize,
			Handles.DrawCapFunction drawFunc,
			Vector2 snap,
			bool drawHelper)
		{
			bool orgGuiChanged = GUI.changed;
			GUI.changed = false;

			Vector2 delta = CalcDeltaAlongDirections(id, handlePos, offset, handleDir, slideDir1, slideDir2, handleSize, drawFunc, snap, drawHelper);
			if (GUI.changed)
				handlePos = s_StartPosition + slideDir1 * delta.x + slideDir2 * delta.y;

			GUI.changed |= orgGuiChanged;
			return handlePos;
		}

		// Returns the distance the new position has moved along slideDir1 and slideDir2
		private static Vector2 CalcDeltaAlongDirections(
			int id,
			Vector3 handlePos,
			Vector3 offset,
			Vector3 handleDir,
			Vector3 slideDir1,
			Vector3 slideDir2,
			float handleSize,
			Handles.DrawCapFunction drawFunc,
			Vector2 snap,
			bool drawHelper)
		{
			
			Vector2 deltaDistanceAlongDirections = new Vector2 (0, 0);

			Event evt = Event.current;
			switch (evt.GetTypeForControl (id))
			{
				case EventType.layout:
					// This is an ugly hack. It would be better if the drawFunc can handle it's own layout.
					if (drawFunc == Handles.ArrowCap)
					{
						HandleUtility.AddControl (id, HandleUtility.DistanceToLine (handlePos + offset, handlePos + handleDir * handleSize));
						HandleUtility.AddControl (id, HandleUtility.DistanceToCircle ((handlePos + offset) + handleDir * handleSize, handleSize * .2f));
					}
					else if (drawFunc == Handles.RectangleCap)
					{
						HandleUtility.AddControl (id, HandleUtility.DistanceToRectangle (handlePos + offset, Quaternion.LookRotation (handleDir, slideDir1), handleSize));
					}
					else
					{
#if ENABLE_SPRITES
						HandleUtility.AddControl (id, HandleUtility.DistanceToCircle (handlePos + offset, handleSize * .5f));
#else
						HandleUtility.AddControl (id, HandleUtility.DistanceToCircle (handlePos + offset, handleSize * .2f));
#endif
					}
					break;

				case EventType.mouseDown:
					// am I closest to the thingy?
					if (((HandleUtility.nearestControl == id && evt.button == 0) ||
						(GUIUtility.keyboardControl == id && evt.button == 2)) && GUIUtility.hotControl == 0)
					{
						Plane plane = new Plane(Handles.matrix.MultiplyVector (handleDir), Handles.matrix.MultiplyPoint (handlePos));
						Ray mouseRay = HandleUtility.GUIPointToWorldRay(evt.mousePosition);
						float dist = 0.0f;
						plane.Raycast(mouseRay, out dist);

						GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
						s_CurrentMousePosition = evt.mousePosition;
						s_StartPosition = handlePos;
					
						Vector3 localMousePoint = Handles.s_InverseMatrix.MultiplyPoint (mouseRay.GetPoint (dist));
						Vector3 clickOffset = localMousePoint - handlePos;
						s_StartPlaneOffset.x = Vector3.Dot(clickOffset, slideDir1);
						s_StartPlaneOffset.y = Vector3.Dot(clickOffset, slideDir2);

						evt.Use ();
						EditorGUIUtility.SetWantsMouseJumping (1);
					}
					break;

				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						s_CurrentMousePosition += evt.delta;
						Vector3 worldPosition = Handles.matrix.MultiplyPoint(handlePos);
						Vector3 worldSlideDir1 = Handles.matrix.MultiplyVector(slideDir1).normalized;
						Vector3 worldSlideDir2 = Handles.matrix.MultiplyVector(slideDir2).normalized;

						// Detect hit with plane (ray from campos to cursor)
						Ray mouseRay = HandleUtility.GUIPointToWorldRay (s_CurrentMousePosition);
						Plane plane = new Plane(worldPosition, worldPosition + worldSlideDir1, worldPosition + worldSlideDir2);
						float dist = 0.0f;
						if (plane.Raycast (mouseRay, out dist))
						{
							Vector3 hitpos = Handles.s_InverseMatrix.MultiplyPoint (mouseRay.GetPoint (dist));

							// Determine hitpos projection onto slideDirs
							deltaDistanceAlongDirections.x = HandleUtility.PointOnLineParameter(hitpos, s_StartPosition, slideDir1);
							deltaDistanceAlongDirections.y = HandleUtility.PointOnLineParameter(hitpos, s_StartPosition, slideDir2);
							deltaDistanceAlongDirections -= s_StartPlaneOffset;
							if (snap.x > 0 || snap.y > 0)
							{
								deltaDistanceAlongDirections.x = Handles.SnapValue(deltaDistanceAlongDirections.x, snap.x);
								deltaDistanceAlongDirections.y = Handles.SnapValue(deltaDistanceAlongDirections.y, snap.y);
							}
							
							GUI.changed = true;
						}
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
				case EventType.repaint:
					{
#if ENABLE_SPRITES
						if (drawFunc == null)
							break;
#endif

						Vector3 position = handlePos + offset;
						Quaternion rotation = Quaternion.LookRotation (handleDir, slideDir1);

						Color temp = Color.white;
						if (id == GUIUtility.keyboardControl)
						{
							temp = Handles.color;
							Handles.color = Handles.selectedColor;
						}

						drawFunc (id, position, rotation, handleSize);

						if (id == GUIUtility.keyboardControl)
							Handles.color = temp;

						// Draw a helper rectangle to show what plane we are dragging in
						if (drawHelper && GUIUtility.hotControl == id)
						{
							Vector3[] verts = new Vector3[4];
							float helperSize = handleSize * 10.0f;
							verts[0] = position + (slideDir1 * helperSize + slideDir2 * helperSize);
							verts[1] = verts[0] - slideDir1 * helperSize * 2.0f;
							verts[2] = verts[1] - slideDir2 * helperSize * 2.0f;
							verts[3] = verts[2] + slideDir1 * helperSize * 2.0f;
							Color prevColor = Handles.color;
							Handles.color = Color.white;
							float outline = 0.6f;
							Handles.DrawSolidRectangleWithOutline(verts, new Color(1, 1, 1, 0.05f), new Color(outline, outline, outline, 0.4f));
							Handles.color = prevColor;
						}
					}

					break;
			}

			return deltaDistanceAlongDirections;
		}
	}
}
