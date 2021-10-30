using UnityEngine;
using System;
using System.Collections.Generic;
using System.Linq;

#if ENABLE_2D_PHYSICS

namespace UnityEditor
{
	internal class PolygonEditorUtility
	{
		const float k_HandlePointSnap = 0.2f;

		private Collider2D m_ActiveCollider;
		private bool m_LoopingCollider = false;
		private int m_MinPathPoints = 3;
		
		private int m_SelectedPath = -1;
		private int m_SelectedVertex = -1;
		private float m_SelectedDistance = 0.0f;
		private int m_SelectedEdgePath = -1;
		private int m_SelectedEdgeVertex0 = -1;
		private int m_SelectedEdgeVertex1 = -1;
		private float m_SelectedEdgeDistance = 0.0f;
		private bool m_LeftIntersect = false;
		private bool m_RightIntersect = false;
		private bool m_DeleteMode = false;

		public void Reset()
		{
			m_SelectedPath = -1;
			m_SelectedVertex = -1;
			m_SelectedEdgePath = -1;
			m_SelectedEdgeVertex0 = -1;
			m_SelectedEdgeVertex1 = -1;
			m_LeftIntersect = false;
			m_RightIntersect = false;
		}

		private void UndoRedoPerformed()
		{
			if (m_ActiveCollider != null)
			{
				Collider2D collider = m_ActiveCollider;
				StopEditing();
				StartEditing(collider);
			}
		}

		public void StartEditing(Collider2D collider)
		{
			Undo.undoRedoPerformed += UndoRedoPerformed;

			Reset();

			PolygonCollider2D polygon = collider as PolygonCollider2D;
			if (polygon)
			{
				m_ActiveCollider = collider;
				m_LoopingCollider = true;
				m_MinPathPoints = 3;
				PolygonEditor.StartEditing (polygon);
				return;
			}

			EdgeCollider2D edge = collider as EdgeCollider2D;
			if (edge)
			{
				m_ActiveCollider = collider;
				m_LoopingCollider = false;
				m_MinPathPoints = 2;
				PolygonEditor.StartEditing (edge);
				return;
			}

			throw new NotImplementedException(string.Format("PolygonEditorUtility does not support {0}", collider));
		}

		public void StopEditing()
		{
			PolygonEditor.StopEditing ();
			m_ActiveCollider = null;

			Undo.undoRedoPerformed -= UndoRedoPerformed;
		}

		private void ApplyEditing(Collider2D collider)
		{
			PolygonCollider2D polygon = collider as PolygonCollider2D;
			if (polygon)
			{
				PolygonEditor.ApplyEditing (polygon);
				return;
			}

			EdgeCollider2D edge = collider as EdgeCollider2D;
			if (edge)
			{
				PolygonEditor.ApplyEditing (edge);
				return;
			}

			throw new NotImplementedException(string.Format("PolygonEditorUtility does not support {0}", collider));
		}

		public void OnSceneGUI()
		{
			if (m_ActiveCollider == null)
				return;

			Event evt = Event.current;
			m_DeleteMode = evt.command || evt.control;
			Transform transform = m_ActiveCollider.transform;

			// Handles.Slider2D will render active point as yellow if there is keyboardControl set. We don't want that happening.
			GUIUtility.keyboardControl = 0;

			// Find mouse positions in local and world space
			Plane plane = new Plane (-transform.forward, transform.position);
			Ray mouseRay = HandleUtility.GUIPointToWorldRay (evt.mousePosition);
			float dist;
			plane.Raycast (mouseRay, out dist);

			Vector3 mouseWorldPos = mouseRay.GetPoint (dist);
			Vector2 mouseLocalPos = transform.InverseTransformPoint (mouseWorldPos);

			// Select the active vertex and edge
			if (evt.type == EventType.MouseMove)
			{
				int pathIndex;
				int pointIndex0, pointIndex1;
				float distance;
				if (PolygonEditor.GetNearestPoint (mouseLocalPos, out pathIndex, out pointIndex0, out distance))
				{
					m_SelectedPath = pathIndex;
					m_SelectedVertex = pointIndex0;
					m_SelectedDistance = distance;
				}
				else
				{
					m_SelectedPath = -1;
				}

				if (PolygonEditor.GetNearestEdge (mouseLocalPos, out pathIndex, out pointIndex0, out pointIndex1, out distance, m_LoopingCollider))
				{
					m_SelectedEdgePath = pathIndex;
					m_SelectedEdgeVertex0 = pointIndex0;
					m_SelectedEdgeVertex1 = pointIndex1;
					m_SelectedEdgeDistance = distance;
				}
				else
				{
					m_SelectedEdgePath = -1;
				}

				evt.Use();
			}
			else if (evt.type == EventType.MouseUp)
			{
				m_LeftIntersect = false;
				m_RightIntersect = false;
			}

			// Do we handle point or line?
			// TODO: there probably isn't a case when selectedPath is valid and selectedEdge is invalid. This needs a refactor.
			bool handlePoint = false;
			bool handleEdge = false;

			if (m_SelectedPath != -1 && m_SelectedEdgePath != -1)
			{
				// Calculate snapping distance
				Vector2 point;
				PolygonEditor.GetPoint (m_SelectedPath, m_SelectedVertex, out point);
				Vector3 worldPos = transform.TransformPoint (point);
				float snapDistance = HandleUtility.GetHandleSize (worldPos) * k_HandlePointSnap;

				handleEdge = (m_SelectedEdgeDistance < m_SelectedDistance - snapDistance); // Note: could we improve this somehow?
				handlePoint = !handleEdge;
			}
			else if (m_SelectedPath != -1)
				handlePoint = true;
			else if (m_SelectedEdgePath != -1)
				handleEdge = true;

			if (m_DeleteMode && handleEdge)
			{
				handleEdge = false;
				handlePoint = true;
			}

			bool applyToCollider = false;

			// Edge handle
			if (handleEdge && !m_DeleteMode)
			{
				Vector2 p0, p1;
				PolygonEditor.GetPoint (m_SelectedEdgePath, m_SelectedEdgeVertex0, out p0);
				PolygonEditor.GetPoint (m_SelectedEdgePath, m_SelectedEdgeVertex1, out p1);
				Vector3 worldPosV0 = transform.TransformPoint(p0);
				Vector3 worldPosV1 = transform.TransformPoint(p1);

				Handles.color = Color.green;
				Handles.DrawAAPolyLine(4.0f, new Vector3[] { worldPosV0, worldPosV1 });
				Handles.color = Color.white;

			 	Vector2 newPoint = GetNearestPointOnEdge (transform.TransformPoint (mouseLocalPos), worldPosV0, worldPosV1);

				EditorGUI.BeginChangeCheck();
				float guiSize = HandleUtility.GetHandleSize (newPoint) * 0.04f;
				Handles.color = Color.green;
				
				newPoint = Handles.Slider2D(
					newPoint,
					new Vector3(0, 0, 1),
					new Vector3(1, 0, 0),
					new Vector3(0, 1, 0),
					guiSize,
					Handles.DotCap,
					Vector3.zero);
				Handles.color = Color.white;
				if (EditorGUI.EndChangeCheck())
				{
					PolygonEditor.InsertPoint (m_SelectedEdgePath, m_SelectedEdgeVertex1, (p0 + p1) / 2);
					m_SelectedPath = m_SelectedEdgePath;
					m_SelectedVertex = m_SelectedEdgeVertex1;
					m_SelectedDistance = 0.0f;
					handlePoint = true;
					applyToCollider = true;
				}
			}

			// Point handle
			if (handlePoint)
			{
				Vector2 point;
				PolygonEditor.GetPoint (m_SelectedPath, m_SelectedVertex, out point);
				Vector3 worldPos = transform.TransformPoint(point);
				float guiSize = HandleUtility.GetHandleSize (worldPos) * 0.04f;

				if (m_DeleteMode && evt.type == EventType.MouseDown && Vector2.Distance (worldPos, mouseWorldPos) < guiSize * 2)
				{
					int pathPointCount = PolygonEditor.GetPointCount(m_SelectedPath);
					if (pathPointCount > m_MinPathPoints)
					{
						PolygonEditor.RemovePoint (m_SelectedPath, m_SelectedVertex);
						Reset();
						applyToCollider = true;
					}
					evt.Use ();
				}

				EditorGUI.BeginChangeCheck();
				Handles.color = m_DeleteMode ? Color.red : Color.green;
				Vector3 newWorldPos = Handles.Slider2D(
					worldPos,
					new Vector3(0, 0, 1),
					new Vector3(1, 0, 0),
					new Vector3(0, 1, 0),
					guiSize,
					Handles.DotCap,
					Vector3.zero);
				Handles.color = Color.white;
				if (EditorGUI.EndChangeCheck() && !m_DeleteMode)
				{
					point = transform.InverseTransformPoint(newWorldPos);
					PolygonEditor.TestPointMove (m_SelectedPath, m_SelectedVertex, point, out m_LeftIntersect, out m_RightIntersect, m_LoopingCollider);
					PolygonEditor.SetPoint (m_SelectedPath, m_SelectedVertex, point);
					applyToCollider = true;
				}

				if (!applyToCollider)
					DrawEdgesForSelectedPoint(newWorldPos, transform, m_LeftIntersect, m_RightIntersect, m_LoopingCollider);
			}

			// Apply changes
			if (applyToCollider)
			{
				Undo.RecordObject(m_ActiveCollider, "Edit Collider");
				PolygonEditor.ApplyEditing (m_ActiveCollider);
			}
		}

		private void DrawEdgesForSelectedPoint(Vector3 worldPos, Transform transform, bool leftIntersect, bool rightIntersect, bool loop)
		{
			bool drawLeft = true;
			bool drawRight = true;

			int pathPointCount = UnityEditor.PolygonEditor.GetPointCount(m_SelectedPath);
			int v0 = m_SelectedVertex - 1;
			if (v0 == -1)
			{
				v0 = pathPointCount - 1;
				drawLeft = loop;
			}
			int v1 = m_SelectedVertex + 1;
			if (v1 == pathPointCount)
			{
				v1 = 0;
				drawRight = loop;
			}

			Vector2 p0, p1;
			UnityEditor.PolygonEditor.GetPoint(m_SelectedPath, v0, out p0);
			UnityEditor.PolygonEditor.GetPoint(m_SelectedPath, v1, out p1);
			Vector3 worldPosV0 = transform.TransformPoint(p0);
			Vector3 worldPosV1 = transform.TransformPoint(p1);

			float lineWidth = 4.0f;
			if (drawLeft)
			{
				Handles.color = leftIntersect || m_DeleteMode ? Color.red : Color.green;
				Handles.DrawAAPolyLine(lineWidth, new Vector3[] { worldPos, worldPosV0 });
			}
			if (drawRight)
			{
				Handles.color = rightIntersect || m_DeleteMode ? Color.red : Color.green;
				Handles.DrawAAPolyLine(lineWidth, new Vector3[] { worldPos, worldPosV1 });
			}
			Handles.color = Color.white;
		}

		Vector2 GetNearestPointOnEdge (Vector2 point, Vector2 start, Vector2 end)
		{
			Vector2 startToPoint = point - start;
			Vector2 startToEnd = (end - start).normalized;
			float dot = Vector2.Dot (startToEnd, startToPoint);

			if (dot <= 0)
				return start;

			if (dot >= Vector2.Distance (start, end))
				return end;

			Vector2 offsetToPoint = startToEnd * dot;
			return start + offsetToPoint;
		}
	}
}

#endif // #if ENABLE_2D_PHYSICS
