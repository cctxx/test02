#if ENABLE_SPRITES
using UnityEngine;

namespace UnityEditor
{
	internal class TransformScale2D
	{
		private static Transform pivot { get { return TransformTool2D.pivot; } }
		private static bool s_Scaling = false;

		public static void OnGUI()
		{
			Vector3 newScale = pivot.localScale;
			Vector3 oldPivot = pivot.position;

			float alpha = TransformTool2DUtility.GetSizeBasedAlpha();
			TransformTool2D.RectSpaceCursorRect[] cursorRects = TransformTool2D.rectSpaceCursorRects;
			
			Color oldColor = GUI.color;
			GUI.color = new Color(1f, 1f, 1f, alpha);

			bool scaleFromPivot = Event.current.alt;
			bool squashing = Event.current.command || Event.current.control;
			bool uniformScaling = (!TransformTool2D.SingleOrientation() || !Event.current.shift) && !squashing;

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[2].rect.center), true);

			EditorGUI.BeginChangeCheck();
			newScale = ScaleHandle1D(pivot.localScale, cursorRects[1].rect, uniformScaling, alpha > 0f);
			if (EditorGUI.EndChangeCheck())
			{
				if (squashing)
					newScale.y *= pivot.localScale.x / newScale.x;
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);
			}

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[1].rect.center), true);

			EditorGUI.BeginChangeCheck();
			newScale = ScaleHandle1D(pivot.localScale, cursorRects[2].rect, uniformScaling, alpha > 0f);
			if (EditorGUI.EndChangeCheck())
			{
				if (squashing)
					newScale.y *= pivot.localScale.x / newScale.x;
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);
			}

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[4].rect.center), true);

			EditorGUI.BeginChangeCheck();
			newScale = ScaleHandle1D(pivot.localScale, cursorRects[3].rect, uniformScaling, alpha > 0f);
			if (EditorGUI.EndChangeCheck())
			{
				if (squashing)
					newScale.x *= pivot.localScale.y / newScale.y;
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);
			}

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[3].rect.center), true);

			EditorGUI.BeginChangeCheck();
			newScale = ScaleHandle1D(pivot.localScale, cursorRects[4].rect, uniformScaling, alpha > 0f);
			if (EditorGUI.EndChangeCheck())
			{
				if (squashing)
					newScale.x *= pivot.localScale.y / newScale.y;
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);
			}

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[7].rect.center), true);

			newScale = ScaleHandle2D(pivot.localScale, cursorRects[5].rect, uniformScaling, alpha);

			if (GUI.changed)
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[8].rect.center), true);

			newScale = ScaleHandle2D(pivot.localScale, cursorRects[6].rect, uniformScaling, alpha);

			if (GUI.changed)
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[5].rect.center), true);

			newScale = ScaleHandle2D(pivot.localScale, cursorRects[7].rect, uniformScaling, alpha);

			if (GUI.changed)
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(pivot.TransformPoint(cursorRects[6].rect.center), true);

			newScale = ScaleHandle2D(pivot.localScale, cursorRects[8].rect, uniformScaling, alpha);

			if (GUI.changed)
				oldPivot = RefreshPivotAfterScale(oldPivot, newScale, scaleFromPivot);

			if (!scaleFromPivot)
				TransformTool2D.MovePivotTo(oldPivot, true);

			GUI.color = oldColor;
		}

		private static Vector3 RefreshPivotAfterScale(Vector3 oldPivot, Vector3 newScale, bool scalingFromPivot)
		{
			Vector3 result = pivot.InverseTransformPoint(oldPivot);
			pivot.localScale = newScale;
			result = pivot.TransformPoint(result);
			return scalingFromPivot ? oldPivot : result;
		}

		private static Vector3 ScaleHandle1D(Vector3 value, Rect localPosition, bool uniformScaling, bool enabled)
		{
			int id = GUIUtility.GetControlID("ScaleHandle1D".GetHashCode(), FocusType.Keyboard);

			if (enabled || s_Scaling)
			{
				Vector3 valueBefore = value;

				Vector2 localPositionBefore = localPosition.center;
				Vector3 worldPositionBefore = TransformTool2DUtility.PivotToWorld(localPositionBefore);
				Vector3 worldPositionAfter = ScaleSlider.Do(id, worldPositionBefore, pivot.forward, pivot.up, pivot.right, localPosition, null, Vector2.zero);
				Vector2 localPositionAfter = TransformTool2DUtility.WorldToPivot(worldPositionAfter);

				if (GUIUtility.hotControl == id)
				{
					Vector3 localValueBefore = Vector3.Project(localPositionBefore, localPosition.center.normalized);
					Vector3 localValueAfter = Vector3.Project(localPositionAfter, localPosition.center.normalized);
					
					value.x *= (localValueBefore.x != 0.0f ? localValueAfter.x / localValueBefore.x : 1f);
					value.y *= (localValueBefore.y != 0.0f ? localValueAfter.y / localValueBefore.y : 1f);

					bool xAxis = Mathf.Abs(localPosition.center.x) > Mathf.Abs(localPosition.center.y);
					if (uniformScaling)
					{
						if (xAxis)
							value.y = valueBefore.x != 0.0f ? valueBefore.y * (value.x / valueBefore.x) : 1f;
						else
							value.x = valueBefore.y != 0.0f ? valueBefore.x * (value.y / valueBefore.y) : 1f;
					}
					else
					{
						if (xAxis)
							value.y = valueBefore.y;
						else
							value.x = valueBefore.x;
					}

					if (Mathf.Abs(valueBefore.x - value.x) < TransformTool2D.k_ScalingEpsilon)
						value.x = valueBefore.x;
					if (Mathf.Abs(valueBefore.y - value.y) < TransformTool2D.k_ScalingEpsilon)
						value.y = valueBefore.y;
				}
			}
			
			return value;
		}

		private static Vector3 ScaleHandle2D(Vector3 value, Rect localPosition, bool uniformScaling, float alpha)
		{
			int id = GUIUtility.GetControlID("ScaleHandle2D".GetHashCode(), FocusType.Keyboard);

			alpha = s_Scaling ? 1f : alpha;

			if (alpha > 0f)
			{
				Vector3 valueBefore = value;

				Vector2 localPositionBefore = localPosition.center;
				Vector3 worldPositionBefore = TransformTool2DUtility.PivotToWorld(localPositionBefore);
				Vector3 worldPositionAfter = ScaleSlider.Do(id, worldPositionBefore, pivot.forward, pivot.up, pivot.right, localPosition, ScalingDot, Vector2.zero);
				Vector2 localPositionAfter = TransformTool2DUtility.WorldToPivot(worldPositionAfter);

				if (GUIUtility.hotControl == id)
				{
					if (uniformScaling)
					{
						Vector3 localValueBefore = Vector3.Project(localPositionBefore, localPosition.center.normalized);
						Vector3 localValueAfter = Vector3.Project(localPositionAfter, localPosition.center.normalized);

						if (Mathf.Abs(localValueAfter.x - localValueBefore.x) > TransformTool2D.k_ScalingEpsilon)
							value.y *= (localValueBefore.x != 0.0f ? localValueAfter.x / localValueBefore.x : 1f);
						if (Mathf.Abs(localValueAfter.y - localValueBefore.y) > TransformTool2D.k_ScalingEpsilon)
							value.x *= (localValueBefore.y != 0.0f ? localValueAfter.y / localValueBefore.y : 1f);
					}
					else
					{
						if (Mathf.Abs(localPositionAfter.x - localPositionBefore.x) > TransformTool2D.k_ScalingEpsilon)
							value.x *= (localPositionBefore.x != 0.0f ? localPositionAfter.x / localPositionBefore.x : 1f);
						if (Mathf.Abs(localPositionAfter.y - localPositionBefore.y) > TransformTool2D.k_ScalingEpsilon)
							value.y *= (localPositionBefore.y != 0.0f ? localPositionAfter.y / localPositionBefore.y : 1f);
					}

					if (Mathf.Abs(valueBefore.x - value.x) < TransformTool2D.k_ScalingEpsilon)
						value.x = valueBefore.x;
					if (Mathf.Abs(valueBefore.y - value.y) < TransformTool2D.k_ScalingEpsilon)
						value.y = valueBefore.y;
				}
			}
			return value;
		}

		public static void ScalingDot(int controlID, Vector3 position, Quaternion rotation, float size)
		{
			Vector3 screenPos = TransformTool2DUtility.WorldToScreen(position);

			Handles.BeginGUI();
			float w = TransformTool2D.s_Styles.dragdot.fixedWidth;
			float h = TransformTool2D.s_Styles.dragdot.fixedHeight;
			Rect r = new Rect(screenPos.x - w / 2f, screenPos.y - h / 2f, w, h);
			if (GUIUtility.hotControl == controlID)
				TransformTool2D.s_Styles.dragdotactive.Draw(r, GUIContent.none, controlID);
			else
				TransformTool2D.s_Styles.dragdot.Draw(r, GUIContent.none, controlID);

			Handles.EndGUI();
		}

		internal static class ScaleSlider
		{
			private static Vector2 s_CurrentMousePosition;
			private static Vector3 s_StartPosition;
			private static Vector2 s_StartPlaneOffset;

			// Returns the new handlePos 
			public static Vector3 Do(int id, Vector3 handlePos, Vector3 handleDir, Vector3 slideDir1, Vector3 slideDir2, Rect handleLocalRect, Handles.DrawCapFunction drawFunc, Vector2 snap)
			{
				bool orgGuiChanged = GUI.changed;
				GUI.changed = false;
				
				Vector2 delta = CalcDeltaAlongDirections(id, handlePos, handleDir, slideDir1, slideDir2, handleLocalRect, drawFunc, snap);
				if (GUI.changed)
					handlePos = s_StartPosition + slideDir1 * delta.x + slideDir2 * delta.y;

				GUI.changed |= orgGuiChanged;
				return handlePos;
			}

			// Returns the distance the new position has moved along slideDir1 and slideDir2
			private static Vector3 CalcDeltaAlongDirections(int id, Vector3 handlePos, Vector3 handleDir, Vector3 slideDir1, Vector3 slideDir2, Rect handleLocalRect, Handles.DrawCapFunction drawFunc, Vector2 snap)
			{
				Vector2 deltaDistanceAlongDirections = new Vector2(0, 0);

				Event evt = Event.current;
				switch (evt.GetTypeForControl(id))
				{
					case EventType.layout:
						if (handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
							HandleUtility.AddControl(id, 3f); // Setting distance to 3, so that other handles on top still have room to get hot with distances of 0-3
						break;
					case EventType.mouseDown:
						if (handleLocalRect.Contains(TransformTool2DUtility.ScreenToPivot(evt.mousePosition)))
						{
							Plane plane = new Plane(Handles.matrix.MultiplyVector(handleDir), Handles.matrix.MultiplyPoint(handlePos));
							Ray mouseRay = HandleUtility.GUIPointToWorldRay(evt.mousePosition);
							float dist = 0.0f;
							plane.Raycast(mouseRay, out dist);

							GUIUtility.hotControl = GUIUtility.keyboardControl = id; // Grab mouse focus
							s_CurrentMousePosition = evt.mousePosition;
							s_StartPosition = handlePos;

							Vector3 localMousePoint = Handles.s_InverseMatrix.MultiplyPoint(mouseRay.GetPoint(dist));
							Vector3 clickOffset = localMousePoint - handlePos;
							s_StartPlaneOffset.x = Vector3.Dot(clickOffset, slideDir1);
							s_StartPlaneOffset.y = Vector3.Dot(clickOffset, slideDir2);

							evt.Use();
							EditorGUIUtility.SetWantsMouseJumping(1);
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
							Ray mouseRay = HandleUtility.GUIPointToWorldRay(s_CurrentMousePosition);
							Plane plane = new Plane(worldPosition, worldPosition + worldSlideDir1, worldPosition + worldSlideDir2);
							float dist = 0.0f;
							if (plane.Raycast(mouseRay, out dist))
							{
								Vector3 hitpos = Handles.s_InverseMatrix.MultiplyPoint(mouseRay.GetPoint(dist));

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
								s_Scaling = true;
							}
							evt.Use();
						}
						break;
					case EventType.mouseUp:
						if (GUIUtility.hotControl == id && (evt.button == 0 || evt.button == 2))
						{
							GUIUtility.hotControl = 0;
							evt.Use();
							s_Scaling = false;
							EditorGUIUtility.SetWantsMouseJumping(0);
						}
						break;
					case EventType.repaint:
						{
							if (drawFunc == null)
								break;

							Vector3 position = handlePos;
							Quaternion rotation = Quaternion.LookRotation(handleDir, slideDir1);

							Color temp = Color.white;
							if (id == GUIUtility.keyboardControl)
							{
								temp = Handles.color;
								Handles.color = Handles.selectedColor;
							}

							drawFunc(id, position, rotation, 1f);

							if (id == GUIUtility.keyboardControl)
								Handles.color = temp;
						}

						break;
				}

				return deltaDistanceAlongDirections;
			}
		}
	}
}
#endif
