#if ENABLE_SPRITES
using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{
	internal class TransformMove2D
	{
		private static Vector3 axisLock;
		private static bool axisLocked;
		private static Vector3 dragMouseStart;
		private static Vector3 dragPivotStart;

		public static void OnGUI (int id, SceneView sceneview)
		{
			if (Selection.gameObjects.Length == 0)
				return;

			Event evt = Event.current;

			// We check if RectSelection is telling us that we got selected.
			// In this case, we fake mousedown on repaint. This allows us to do one-click dragging right away.
			if (sceneview.m_OneClickDragObject == Selection.activeGameObject && evt.type == EventType.Repaint)
			{
				HandleMouseDown(id, Selection.activeTransform);
				sceneview.m_OneClickDragObject = null;
			}

			switch (evt.GetTypeForControl(id))
			{
				case EventType.layout:
					bool contained = TransformTool2D.rect.Contains (TransformTool2DUtility.ScreenToPivot (evt.mousePosition));
					
					HandleUtility.AddControl (id, contained ? HandleUtility.kPickDistance : HandleUtility.DistanceToPolyLine (TransformTool2DUtility.GetRectPolyline ()));
					break;

				case EventType.mouseDown:
					bool acceptClick = false;
					
					if (Tools.vertexDragging)
					{
						Handles.SetupIgnoreRaySnapObjects ();
						
						// We could check the distance to handle here, but when user is already explicitly holding down V, I don't think he ever wants to interact with any other handle
						acceptClick = true;
					}
					else
					{
						acceptClick = TransformTool2D.rect.Contains (TransformTool2DUtility.ScreenToPivot (evt.mousePosition));
					}

					if (acceptClick)
					{
						HandleMouseDown (id, Selection.activeTransform);
						evt.Use ();
					}

					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id && Tools.vertexDragging)
					{
						Vector3 near;
						if (HandleUtility.FindNearestVertex (evt.mousePosition, null, out near))
							TransformTool2D.pivot.transform.position = near - Tools.handleOffset;
						
						GUI.changed = true;
						evt.Use ();
					}
					else
					{
						if (evt.shift && axisLocked == false)
						{
							axisLocked = true;
							axisLock = Mathf.Abs(evt.delta.x) > Mathf.Abs(evt.delta.y) ? new Vector3(1f, 0f, 1f) : new Vector3(0f, 1f, 1f);
						}
					
						if (GUIUtility.hotControl == id)
						{
							Vector3 delta = TransformTool2DUtility.ScreenToWorld(Event.current.mousePosition) - TransformTool2DUtility.ScreenToWorld(dragMouseStart);

							delta.x = Handles.SnapValue (delta.x, SnapSettings.move.x);
							delta.y = Handles.SnapValue (delta.y, SnapSettings.move.y);
							delta.z = Handles.SnapValue (delta.z, SnapSettings.move.z);
							
							if (axisLocked)
								delta.Scale (axisLock);

							TransformTool2D.pivot.transform.position = dragPivotStart + delta;

							GUI.changed = true;
							evt.Use();
						}
					}
					break;
				case EventType.mouseUp:
					axisLocked = false;
					
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						// only reset the handle offset if we are no longer vertex dragging
						if (!Tools.vertexDragging)
							Tools.handleOffset = Vector3.zero;

						HandleUtility.ignoreRaySnapObjects = null;
						GUIUtility.hotControl = 0;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(0);
					}
					break;
				case EventType.MouseMove:
					{
						Vector3 near;
						if (Tools.vertexDragging && HandleUtility.FindNearestVertex (evt.mousePosition, Selection.GetTransforms (SelectionMode.Deep | SelectionMode.ExcludePrefab | SelectionMode.Editable), out near))
						{
							Tools.handleOffset = near - TransformTool2DUtility.PivotToWorld(Vector3.zero);
							evt.Use ();
						}
						break;
					}
				case EventType.keyDown:
					// Vertex selection
					if (evt.keyCode == KeyCode.V)
					{
						// We are searching for a vertex in our selection
						if (!Tools.vertexDragging && !evt.shift)
						{
							HandleUtility.ignoreRaySnapObjects = null;
							Tools.vertexDragging = true;
							Vector3 near;
							
							if (HandleUtility.FindNearestVertex (evt.mousePosition, Selection.GetTransforms (SelectionMode.Deep | SelectionMode.ExcludePrefab | SelectionMode.Editable), out near))
								Tools.handleOffset = near - TransformTool2DUtility.PivotToWorld (Vector3.zero);
						}
						evt.Use ();
					}
					break;

				case EventType.KeyUp:
					// Vertex selection
					if (evt.keyCode == KeyCode.V)
					{
						if (evt.shift)
						{
							// toggle vertex selection
							Tools.vertexDragging = !Tools.vertexDragging;
							if (!Tools.vertexDragging)
								Tools.handleOffset = Vector3.zero;
						}
						else if (Tools.vertexDragging)
						{
							// stop vertex dragging and reset things
							Tools.vertexDragging = false;
							Tools.handleOffset = Vector3.zero;
							evt.Use ();
						}
						evt.Use ();
					}
					break;
				case EventType.repaint:
					// can't render rect here, because we want rect rendered before all the other handles

					// Instead we are rendering a gizmo if user is vertex snapping
					if (Tools.vertexDragging)
					{
						Vector3 screenPos = TransformTool2DUtility.WorldToScreen (TransformTool2DUtility.PivotToWorld (Vector3.zero) + Tools.handleOffset);

						Handles.BeginGUI ();
						float w = TransformTool2D.s_Styles.dragdot.fixedWidth;
						float h = TransformTool2D.s_Styles.dragdot.fixedHeight;
						Rect r = new Rect (screenPos.x - w / 2f, screenPos.y - h / 2f, w, h);
						if (GUIUtility.hotControl == id)
							TransformTool2D.s_Styles.dragdotactive.Draw (r, GUIContent.none, id);
						else
							TransformTool2D.s_Styles.dragdot.Draw (r, GUIContent.none, id);

						Handles.EndGUI ();
					}
					break;
			}

		}
		static private void HandleMouseDown(int id, Transform t)
		{
			GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
			dragMouseStart = Event.current.mousePosition;
			dragPivotStart = TransformTool2DUtility.PivotToWorld (Vector3.zero);
			EditorGUIUtility.SetWantsMouseJumping(1);
		}
	}
}
#endif
