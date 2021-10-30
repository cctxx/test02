using UnityEditor;
using UnityEngine;

namespace UnityEditorInternal
{
	internal class FreeMove
	{
		private static Vector2 s_StartMousePosition, s_CurrentMousePosition;
		private static Vector3 s_StartPosition;


		public static Vector3 Do(int id, Vector3 position, Quaternion rotation, float size, Vector3 snap, Handles.DrawCapFunction capFunc)
		{
			Vector3 worldPosition = Handles.matrix.MultiplyPoint(position);
			Matrix4x4 origMatrix = Handles.matrix;

			Event evt = Event.current;
			switch (evt.GetTypeForControl(id))
			{
				case EventType.layout:
					// We only want the position to be affected by the Handles.matrix.
					Handles.matrix = Matrix4x4.identity;
					HandleUtility.AddControl(id, HandleUtility.DistanceToCircle(worldPosition, size * 1.2f));
					Handles.matrix = origMatrix;
					break;
				case EventType.mouseDown:
					// am I closest to the thingy?
					if ((HandleUtility.nearestControl == id && evt.button == 0) || (GUIUtility.keyboardControl == id && evt.button == 2))
					{
						GUIUtility.hotControl = GUIUtility.keyboardControl = id;	 // Grab mouse focus
						s_CurrentMousePosition = s_StartMousePosition = evt.mousePosition;
						s_StartPosition = position;
						HandleUtility.ignoreRaySnapObjects = null;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(1);
					}
					break;
				case EventType.mouseDrag:
					if (GUIUtility.hotControl == id)
					{
						bool rayDrag = EditorGUI.actionKey && evt.shift;

						if (rayDrag)
						{
							if (HandleUtility.ignoreRaySnapObjects == null)
								Handles.SetupIgnoreRaySnapObjects();

							object hit = HandleUtility.RaySnap(HandleUtility.GUIPointToWorldRay(evt.mousePosition));
							if (hit != null)
							{
								RaycastHit rh = (RaycastHit)hit;
								float offset = 0;
								if (Tools.pivotMode == PivotMode.Center)
								{
									float geomOffset = HandleUtility.CalcRayPlaceOffset(HandleUtility.ignoreRaySnapObjects, rh.normal);
									if (geomOffset != Mathf.Infinity)
									{
										offset = Vector3.Dot(position, rh.normal) - geomOffset;
									}
								}
								position = Handles.s_InverseMatrix.MultiplyPoint(rh.point + (rh.normal * offset));
							}
							else
							{
								rayDrag = false;
							}
						}

						if (!rayDrag)
						{
							// normal drag
							s_CurrentMousePosition += new Vector2(evt.delta.x, -evt.delta.y);
							Vector3 screenPos = Camera.current.WorldToScreenPoint(Handles.s_Matrix.MultiplyPoint(s_StartPosition));
							screenPos += (Vector3)(s_CurrentMousePosition - s_StartMousePosition);
							position = Handles.s_InverseMatrix.MultiplyPoint(Camera.current.ScreenToWorldPoint(screenPos));

							// Due to floating point inaccuracies, the back-and-forth transformations used may sometimes introduce 
							// tiny unintended movement in wrong directions. People notice when using a straight top/left/right ortho camera.
							// In that case, just restrain the movement to the plane.
							if (Camera.current.transform.forward == Vector3.forward || Camera.current.transform.forward == -Vector3.forward)
								position.z = s_StartPosition.z;
							if (Camera.current.transform.forward == Vector3.up || Camera.current.transform.forward == -Vector3.up)
								position.y = s_StartPosition.y;
							if (Camera.current.transform.forward == Vector3.right || Camera.current.transform.forward == -Vector3.right)
								position.x = s_StartPosition.x;

							if (Tools.vertexDragging)
							{
								if (HandleUtility.ignoreRaySnapObjects == null)
									Handles.SetupIgnoreRaySnapObjects();
								Vector3 near;
								if (HandleUtility.FindNearestVertex(evt.mousePosition, null, out near))
								{
									position = Handles.s_InverseMatrix.MultiplyPoint(near);
								}
							}

							if (EditorGUI.actionKey && !evt.shift)
							{
								Vector3 delta = position - s_StartPosition;
								delta.x = Handles.SnapValue(delta.x, snap.x);
								delta.y = Handles.SnapValue(delta.y, snap.y);
								delta.z = Handles.SnapValue(delta.z, snap.z);
								position = s_StartPosition + delta;
							}
						}
						GUI.changed = true;
						evt.Use();
					}
					break;
				case EventType.MouseMove:
					{
						Vector3 near;
						if (Tools.vertexDragging && HandleUtility.FindNearestVertex(evt.mousePosition, Selection.GetTransforms (SelectionMode.Deep | SelectionMode.ExcludePrefab | SelectionMode.Editable), out near))
						{
							Tools.handleOffset = Vector3.zero;
							Tools.handleOffset = near - Tools.handlePosition;
							evt.Use();
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
							Tools.vertexDragging = true;
							Vector3 near;
							if (HandleUtility.FindNearestVertex(evt.mousePosition, Selection.GetTransforms (SelectionMode.Deep | SelectionMode.ExcludePrefab | SelectionMode.Editable), out near))
							{
								Tools.handleOffset = Vector3.zero;
								Tools.handleOffset = near - Tools.handlePosition;
							}
						}
						evt.Use();
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
							evt.Use();
						}
						evt.Use();
					}
					break;
				case EventType.mouseUp:
					if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
					{
						// only reset the handle offset if we are no longer vertex dragging
						if (!Tools.vertexDragging)
							Tools.handleOffset = Vector3.zero;
						GUIUtility.hotControl = 0;
						HandleUtility.ignoreRaySnapObjects = null;
						evt.Use();
						EditorGUIUtility.SetWantsMouseJumping(0);
					}
					break;
				case EventType.repaint:
					Color temp = Color.white;
					if (id == GUIUtility.keyboardControl)
					{
						temp = Handles.color;
						Handles.color = Handles.selectedColor;
					}
					
					// We only want the position to be affected by the Handles.matrix.
					Handles.matrix = Matrix4x4.identity;
					capFunc(id, worldPosition, Camera.current.transform.rotation, size);
					Handles.matrix = origMatrix;

					if (id == GUIUtility.keyboardControl)
						Handles.color = temp;
					break;
			}
			return position;
		}

	}
}
