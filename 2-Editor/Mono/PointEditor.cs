using System.Linq;
using UnityEngine;
using System.Collections.Generic;

namespace UnityEditor
{

/// Semi-Generic point editor.
/// A Point editor basically let you drag around an array of Vector3 points in the scene view.

/// In order to maximize flexibility, all communication goes through the IEditablePoint interface. 
/// The point editor uses this to make callbacks to your editor so no assumptions about your data structure is made.
internal interface IEditablePoint {
	/// Get the world-space position of a specifc point
	Vector3 GetPosition (int idx);
	/// Set the world-space position of a specific point
	void SetPosition (int idx, Vector3 position);
	/// Get the default color for points
	Color GetDefaultColor ();
	/// Get tje selected color for points
	Color GetSelectedColor ();
	/// Scale of the spheres to draw
	float GetPointScale ();
	/// Get the positions of the points
	IEnumerable<Vector3> GetPositions ();

	Vector3[] GetUnselectedPositions();
	Vector3[] GetSelectedPositions();

	int Count { get; }
}

internal class PointEditor {

	// Various variables to keep track of the state of selection rects.
	// Since the user can not be dragging inside more than one thing (they only have one mouse), we can
	// just have these be static.
	//
	// The point on screen that the user started dragging a selection rect from.
	private static Vector2 s_StartMouseDragPosition;
	// The selection at the start of the drag. Using this and the start mousepos, 
	// we can reconstruct the selection live during the drag so the user will always see the drag.
	private static List<int> s_StartDragSelection;

	private static bool s_DidDrag;

	private static Mesh s_Mesh;
	
	private static Mesh GetMesh()
	{
		s_Mesh = s_Mesh ?? Resources.GetBuiltinResource(typeof(Mesh), "New-Sphere.fbx") as Mesh;
		return s_Mesh;
	}

	private static Vector3 s_EditingScale = Vector3.one;
	private static Quaternion s_EditingRotation = Quaternion.identity;

	/// Move the selected points using standard handles. returns true if point moved
	public static bool MovePoints(IEditablePoint points, Transform cloudTransform, List<int> selection)
	{
		if (selection.Count == 0) 
			return false;

		if (Event.current.type == EventType.mouseUp)
		{
			s_EditingScale = Vector3.one;
			s_EditingRotation = Quaternion.identity;
		}

		// Editing for the case where we're in a 3D sceneview
		if (Camera.current)
		{
			var handlePos = Vector3.zero;
			handlePos = Tools.pivotMode == PivotMode.Pivot ?
				points.GetPosition(selection[0]) :
				selection.Aggregate(handlePos, (current, index) => current + points.GetPosition(index)) / selection.Count;

			handlePos = cloudTransform.TransformPoint(handlePos);

			switch (Tools.current)
			{
				case Tool.Move:
				{
					var newPos = Handles.PositionHandle (handlePos,
						Tools.pivotRotation == PivotRotation.Local ? cloudTransform.rotation : Quaternion.identity);
					if (GUI.changed)
					{
						Vector3 delta = cloudTransform.InverseTransformPoint (newPos) - cloudTransform.InverseTransformPoint (handlePos);
						foreach (int i in selection) 
							points.SetPosition (i, points.GetPosition (i) + delta);
						return true;
					}
					break;
				}
				case Tool.Rotate:
				{
					var newRot = Handles.RotationHandle(s_EditingRotation, handlePos);
					if (GUI.changed)
					{
						var handleLocal = cloudTransform.InverseTransformPoint(handlePos);

						foreach (int i in selection)
						{
							var position = points.GetPosition (i) - handleLocal;

							//Transform back to original positons
							position = Quaternion.Inverse (s_EditingRotation)*position;

							//Transfrom into new positions...
							position = newRot*position;
							position += handleLocal;

							points.SetPosition (i, position);
						}
						s_EditingRotation = newRot;
						return true;
					}
					break;
				}
				case Tool.Scale:
				{
					var newScale = Handles.ScaleHandle(s_EditingScale, handlePos, Quaternion.identity, HandleUtility.GetHandleSize(handlePos));
					if (GUI.changed)
					{
						var handleLocal = cloudTransform.InverseTransformPoint(handlePos);

						foreach (var i in selection)
						{
							var position = points.GetPosition (i) - handleLocal;
							
							//Transform points back to original positions using last scale
							position.x /= s_EditingScale.x;
							position.y /= s_EditingScale.y;
							position.z /= s_EditingScale.z;
							
							//Translate to new scale
							position.x *= newScale.x;
							position.y *= newScale.y;
							position.z *= newScale.z;
							position += handleLocal;
							points.SetPosition(i, position);
						}
						s_EditingScale = newScale;
						return true;
					}
					break;
				}
			}
		}
		return false;
	}

	public static int FindNearest(Vector2 point, Transform cloudTransform, IEditablePoint points)
	{
		var r = HandleUtility.GUIPointToWorldRay(point);
		
		var found = new Dictionary<int, float>();
		for (var i = 0; i < points.Count; i++)
		{
			float distance = 0;
			Vector3 collisionPoint = Vector3.zero;

			if (MathUtils.IntersectRaySphere(r, cloudTransform.TransformPoint(points.GetPosition(i)), points.GetPointScale() * 0.5f, ref distance, ref collisionPoint))
			{
				//Only care if we start outside a probe
				if (distance > 0)
				{
					found.Add(i, distance);
				}
			}
		}

		if (found.Count <= 0) return -1;

		var sorted = found.OrderBy(x => x.Value);
		return sorted.First().Key;
	}

	// This function implements selection of points. Returns true is selection changes
	public static bool SelectPoints(IEditablePoint points, Transform cloudTransform, ref List<int> selection, bool firstSelect)
	{
		int id = GUIUtility.GetControlID (FocusType.Passive);
		
		if (Event.current.alt && Event.current.type != EventType.Repaint)
			return false;
		
		bool selectionChanged = false;
		Event evt = Event.current;
		switch (evt.GetTypeForControl (id)) 
		{
		case EventType.Layout:
			// Tell the handles system that we're the default tool (the one that should get focus when user clicks on nothing else.)
			HandleUtility.AddDefaultControl (id);
			break;

		case EventType.MouseDown:
			// If we got a left-mouse down (HandleUtility.nearestControl== id is only true when the user clicked outside any handles),
			// we should begin selecting.
			if ((HandleUtility.nearestControl == id || firstSelect) && evt.button == 0)
			{
				// If neither shift nor control is held down, we'll clear the selection as the fist thing.
				if (!evt.shift && !EditorGUI.actionKey)
				{
					selection.Clear ();
					selectionChanged = true;
				}

				// Grab focus so that we can do a rect selection.
				GUIUtility.hotControl = id;
				// And remember where the drag was from.
				s_StartMouseDragPosition = evt.mousePosition;
				
				// Also remember the selection at the start so additive rect selection will work correctly
				s_StartDragSelection = new List<int> (selection);
				
				// Use the mouse down event so no other controls get them
				evt.Use ();
			}
			break;

		case EventType.MouseDrag:
			// The user dragged the mouse (and we have the focus from MouseDown). We have a rect selection here
			if (GUIUtility.hotControl == id && evt.button == 0)
			{
				s_DidDrag = true;
				// Start by resetting the selection to what it was when the drag began.
				selection.Clear ();
				selection.AddRange (s_StartDragSelection);
				
				// now, we'll go over every point and see if it's inside the Rect defined by the mouse position and 
				// the start drag position
				Rect r = FromToRect (s_StartMouseDragPosition, evt.mousePosition);

				var oldMatrix = Handles.matrix;
				Handles.matrix = cloudTransform.localToWorldMatrix;
				// Go over all the points and add them if they are inside the rect
				for (int i = 0; i < points.Count; i++)
				{
					var point = HandleUtility.WorldToGUIPoint (points.GetPosition (i));
					if (r.Contains(point)) selection.Add (i);
				}
				Handles.matrix = oldMatrix;
				
				// We'll assume the selection has changed and set GUI.changed to true.
				// Worst case, somebody will validate a bit too much, but oh well.
				GUI.changed = true;
				evt.Use ();
			}
			break;
			
		case EventType.MouseUp:
			// If we got the mousedown event, the mouseup is ours as well - this is where we clean up.
			if (GUIUtility.hotControl == id && evt.button == 0)
			{
				//Dragging vs clicking);
				if (!s_DidDrag)
				{
					// Find out if it was on top of a point.
					int selectedPoint = FindNearest (s_StartMouseDragPosition, cloudTransform, points);

					// We found a point. We either need to make it selected or add it to an existing selection.
					if (selectedPoint != -1)
					{
						// If neither shift nor action is held down, simply set selection to the picked point.
						if (!evt.shift && !EditorGUI.actionKey)
						{
							selection.Add (selectedPoint);
						}
						else
						{
							// Shift was held down. This means we need to add/remove the point
							int alreadyInSelection = selection.IndexOf (selectedPoint);
							if (alreadyInSelection != -1) selection.RemoveAt (alreadyInSelection);
							else selection.Add (selectedPoint);
						}
					}

					// Selection has changed. set GUI.changed to true so caller can react (e.g. repaint inspector).
					GUI.changed = true;
					selectionChanged = true;
				}


				// Clean up various stuff.
				s_StartDragSelection = null;
				s_StartMouseDragPosition = Vector2.zero;
				s_DidDrag = false;
				
				// Release the mouse focus
				GUIUtility.hotControl = 0;
				
				// use the event
				evt.Use ();
			}
			break;
		case EventType.Repaint:
			// If we have focus and the mouse has been moved, we'll the draw selection rect.
			if (GUIUtility.hotControl == id && evt.mousePosition != s_StartMouseDragPosition) 
			{
				GUIStyle gs = "SelectionRect";
				Handles.BeginGUI();
				gs.Draw (FromToRect (s_StartMouseDragPosition, evt.mousePosition), false, false, false, false);
				Handles.EndGUI();
			}
			break;
		}
		selection = selection.Distinct ().ToList ();
		return selectionChanged;
	}
	
	/// Draw the points in points array. All selected points will be drawn blue, for unselected, 
	/// points.GetColor will be called to get the color of each point
	public static void Draw(IEditablePoint points, Transform cloudTransform, List<int> selection, bool twoPassDrawing)
	{
		LightmapVisualization.DrawPointCloud(
			points.GetUnselectedPositions(),
			points.GetSelectedPositions (),
			points.GetDefaultColor (),
			points.GetSelectedColor (),
			points.GetPointScale (),
			cloudTransform);
	}
	
	// Small helper: Build a rect and make sure that rect has positive size so Rect.Contains works correctly.
	static Rect FromToRect (Vector2 from, Vector2 to) 
	{
		var r = new Rect (from.x, from.y, to.x - from.x, to.y - from.y);
		
		if (r.width < 0) 
		{
			r.x += r.width;
			r.width = -r.width;
		}
		if (r.height < 0) 
		{
			r.y += r.height;
			r.height = -r.height;
		}
		return r;	
	}

}

}
