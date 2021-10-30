#if ENABLE_SPRITES
using System.Collections.Generic;
using UnityEngine;

namespace UnityEditor
{
	internal class TransformTool2D
	{
		private static int s_OldResetHash;
		
		private static Transform[] s_ActiveTransforms;
		private static MouseCursor s_LastCursor;
		private static OldTransform s_OldPivot;
		
		private static RectSpaceCursorRect[] s_RectSpaceCursorRects;
		public static RectSpaceCursorRect[] rectSpaceCursorRects { get { return s_RectSpaceCursorRects; }}

		private static GameObject s_HiddenPivotGO;
		public static Transform pivot { get { return s_HiddenPivotGO.transform; } }

		private static Rect s_Rect;
		public static Rect rect { get { return s_Rect; } }

		public static Styles s_Styles;

		public const float k_ScalingEpsilon = 0.00001f;
		public const float k_MovingEpsilon = 0.00001f;
		public const float k_RotatingEpsilon = 0.001f;
		public const float k_MinScalingDiameter = 55f;
		public const float k_PivotSnappingDistance = 15f;

		public class Styles
		{
			public readonly GUIStyle dragdot = "U2D.dragDot";
			public readonly GUIStyle pivotdot = "U2D.pivotDot";
			public readonly GUIStyle dragdotactive = "U2D.dragDotActive";
			public readonly GUIStyle pivotdotactive = "U2D.pivotDotActive";
		}
		private class OldTransform
		{
			public Vector3 position;
			public Quaternion rotation;
			public Vector3 scale;
		}
		public struct RectSpaceCursorRect
		{
			public Rect rect;
			public CursorRectType type;
			public Vector3 direction;

			public RectSpaceCursorRect (float x1, float y1, float x2, float y2, CursorRectType type, Vector3 direction)
			{
				if (x1 > x2)
					Swap (ref x1, ref x2);
				if (y1 > y2)
					Swap (ref y1, ref y2);
				rect = new Rect (x1, y1, (x2 - x1), (y2 - y1));
				this.type = type;
				this.direction = direction;
			}
			
			static void Swap<T>(ref T x, ref T y)
			{
				T t = y;
				y = x;
				x = t;
			}
		}

		public enum CursorRectType { Rotate = 0, Scale, Move, Pivot };

		// This is the 2D movetool
		// On every OnGUI, it clones a hidden transform hierarchy, modifies it, applies data back to real hierarchy, and then throws it away
		// The reason is, that we need to put all the selected objects under single transform (pivot) anyway. If we do this to real visible hierarchy, it causes BAD THINGS(tm)
		// TODO: Do something less hacky like actual math *ducks*

		public static void OnGUI (SceneView view)
		{
			// Recreate hidden gameobject
			InitHiddenPivotHierarchy ();
			// Check if any external change occured since last OnGUI and reset our data
			CheckForReset ();
			// If nothing is selected, then walk away
			if (s_ActiveTransforms == null)
				return;
			// Populate hidden pivot gameobject with clones of our currently selected transforms
			PopulateHiddenPivotHierarchy ();
			
			InitStyles ();
			
			int movetoolID = GUIUtility.GetControlID ("MoveTool2D".GetHashCode (), FocusType.Keyboard);
			TransformTool2DUtility.RenderRectWithShadow (GUIUtility.hotControl == movetoolID);
			
			EditorGUI.BeginChangeCheck();
			if (!Tools.vertexDragging)
			{
				UpdateCursor ();
				HandlePivot ();
				TransformScale2D.OnGUI ();
				TransformRotate2D.OnGUI ();
			}
			TransformMove2D.OnGUI (movetoolID, view);
			
			if(EditorGUI.EndChangeCheck())
				ApplyTransforms ();

			// Save the transform of our hiddenGO (we recreate it on next OnGUI)
			SaveOldPivot ();
			// Destroy hidden gameobject, because it will leak otherise (recreate it on next OnGUI)
			DestroyHiddenPivotHierarchy ();
			// Get hash of current situation to detect external changes that would cause full reset on next OnGUI
			s_OldResetHash = GetResetHash();
		}
		
		private static void DestroyHiddenPivotHierarchy()
		{
			GameObject.DestroyImmediate (s_HiddenPivotGO);
		}

		private static void ApplyTransforms ()
		{
			for (int i = 0; i < s_ActiveTransforms.Length; i++)
			{
				Undo.RecordObject (s_ActiveTransforms[i], "Move");
				
				// TODO: TransformPoint and InverseTransformPoint don't work with infinite accuracy. Maybe find a better way to work around it than these epsilon hacks.
				if ((s_ActiveTransforms[i].position - pivot.GetChild (i).position).magnitude > k_MovingEpsilon)
				{
					s_ActiveTransforms[i].position = pivot.GetChild (i).position;
				}

				if (Quaternion.Angle(s_ActiveTransforms[i].rotation, pivot.GetChild(i).rotation) > k_RotatingEpsilon)
				{
					s_ActiveTransforms[i].rotation = pivot.GetChild(i).rotation;

					// Unless we do this, SHIFT-rotating in 15deg incerements starts to drift like this: 0, 15, 29.999999, ...
					if (Event.current.shift)
						s_ActiveTransforms[i].eulerAngles = TransformTool2DUtility.GetNiceEulers (s_ActiveTransforms[i].eulerAngles);
				}

				if ((s_ActiveTransforms[i].localScale - pivot.GetChild (i).lossyScale).magnitude > k_ScalingEpsilon)
				{
					s_ActiveTransforms[i].localScale = pivot.GetChild (i).lossyScale;
				}
			}
		}

		private static void InitHiddenPivotHierarchy()
		{
			if (s_OldPivot == null)
			{
				s_OldPivot = new OldTransform();
				s_OldPivot.position = Vector3.zero;
				s_OldPivot.rotation = Quaternion.identity;
				s_OldPivot.scale = new Vector3(1f, 1f, 1f);
			}

			SetupPivotTransform(Quaternion.identity);
			pivot.position = s_OldPivot.position;
			pivot.rotation = s_OldPivot.rotation;
			pivot.localScale = s_OldPivot.scale;
		}

		private static void SaveOldPivot()
		{
			s_OldPivot.position = pivot.position;
			s_OldPivot.rotation = pivot.rotation;
			s_OldPivot.scale = pivot.localScale;
		}

		private static void CheckForReset ()
		{
			int newResetHash = GetResetHash ();
			if (newResetHash != s_OldResetHash)
				Reset ();
		}

		// We get hash from all the selected go instanceIDs, transform values and sprite bounds
		// If any of these change between the this and previous OnGUI(), it means we need a full reset
		private static int GetResetHash()
		{
			int result = 0;
			Object[] currentSelections = Selection.objects;
			foreach (Object obj in currentSelections)
			{
				GameObject gameObject = obj as GameObject;
				if (gameObject != null)
				{
					result ^= gameObject.GetInstanceID();
					result ^= gameObject.transform.position.GetHashCode();
					result ^= gameObject.transform.localScale.GetHashCode();
					result ^= gameObject.transform.rotation.GetHashCode();

					SpriteRenderer spriteRenderer = gameObject.GetComponent<SpriteRenderer>();
					if (spriteRenderer != null)
					{
						Sprite sprite = spriteRenderer.sprite;
						if (sprite != null)
						{
							result ^= sprite.bounds.GetHashCode();
						}
					}
				}
			}
			return result;
		}

		private static void Reset ()
		{
			s_ActiveTransforms = Selection.transforms;
			
			if (s_ActiveTransforms.Length == 0)
				return;
			
			Quaternion startupRotation = TransformTool2DUtility.RotatedEqually (s_ActiveTransforms) ? s_ActiveTransforms[0].rotation : Quaternion.identity;
			
			SetupPivotTransform (startupRotation);
			s_Rect = GetStartupRect ();
			SetupRectSpaceCursorRects ();
			
			// Move pivot to the center of the rect
			pivot.position = pivot.TransformPoint(s_Rect.center);
			s_Rect.center = Vector3.zero;

			// If singular active transform, move pivot to it
			if(s_ActiveTransforms.Length == 1)
				MovePivotTo (s_ActiveTransforms[0].position, false);

			s_OldPivot = new OldTransform ();
			SaveOldPivot ();
		}

		private static void HandlePivot()
		{
			float alpha = TransformTool2DUtility.GetSizeBasedAlpha();
			Rect localRect = s_RectSpaceCursorRects[0].rect;
			int id = GUIUtility.GetControlID("PivotHandle".GetHashCode(), FocusType.Keyboard);
			Vector3 oldWorldPosition = pivot.position;

			EditorGUI.BeginDisabledGroup(SingleSelected());
			
			EditorGUI.BeginChangeCheck();
			Vector3 newWorldPosition = TransformPivotHandle2D.Do(id, oldWorldPosition, localRect, alpha);
			if (EditorGUI.EndChangeCheck())
			{
				newWorldPosition = HandlePivotSnapping(newWorldPosition);

				MovePivotTo(newWorldPosition, true);
				GUI.changed = true;
			}

			EditorGUI.EndDisabledGroup();
		}

		// Offsetting means that when moving the pivot (x,y) amount, you will need to offset transform (-x,-y), so that it will maintain visual world position
		public static void MovePivotTo(Vector3 worldPosition, bool offsetActiveTransforms)
		{
			Vector3 localOffset = pivot.InverseTransformPoint(worldPosition);
			Vector3 worldOffset = worldPosition - pivot.position;

			if (offsetActiveTransforms)
				for (int childIndex = 0; childIndex < pivot.childCount; childIndex++)
					pivot.GetChild(childIndex).position -= worldOffset;

			pivot.position = worldPosition;

			for (int i = 0; i < s_RectSpaceCursorRects.Length; i++)
			{
				if (s_RectSpaceCursorRects[i].type != CursorRectType.Pivot)
					s_RectSpaceCursorRects[i].rect.center -= (Vector2)localOffset;
			}

			s_Rect.center -= (Vector2)localOffset;
		}

		private static Vector3 HandlePivotSnapping(Vector3 position)
		{
			Vector3[] snappingTargets = new Vector3[9];
			Vector3 screenPosition = TransformTool2DUtility.WorldToScreen(position);

			snappingTargets[0] = TransformTool2DUtility.PivotToScreen(s_Rect.center);
			snappingTargets[1] = TransformTool2DUtility.PivotToScreen(TransformTool2DUtility.GetLocalRectPoint(0));
			snappingTargets[2] = TransformTool2DUtility.PivotToScreen(TransformTool2DUtility.GetLocalRectPoint(1));
			snappingTargets[3] = TransformTool2DUtility.PivotToScreen(TransformTool2DUtility.GetLocalRectPoint(2));
			snappingTargets[4] = TransformTool2DUtility.PivotToScreen(TransformTool2DUtility.GetLocalRectPoint(3));
			snappingTargets[5] = TransformTool2DUtility.PivotToScreen(new Vector2(s_Rect.center.x, s_Rect.yMin));
			snappingTargets[6] = TransformTool2DUtility.PivotToScreen(new Vector2(s_Rect.center.x, s_Rect.yMax));
			snappingTargets[7] = TransformTool2DUtility.PivotToScreen(new Vector2(s_Rect.xMin, s_Rect.center.y));
			snappingTargets[8] = TransformTool2DUtility.PivotToScreen(new Vector2(s_Rect.xMax, s_Rect.center.y));

			for (int i = 0; i < snappingTargets.Length; i++)
			{
				if ((screenPosition - snappingTargets[i]).magnitude < k_PivotSnappingDistance)
					return TransformTool2DUtility.ScreenToWorld(snappingTargets[i]);
			}
			return position;
		}

		private static bool SingleSelected()
		{
			return s_ActiveTransforms.Length == 1;
		}

		// Are all selected objects are rotated equally?
		public static bool SingleOrientation()
		{
			return TransformTool2DUtility.RotatedEqually (s_ActiveTransforms);
		}

		private static Vector2 GetActiveSpritePivotOffset()
		{
			if (!SingleSelected())
				return Vector2.zero;

			return GetSpriteRendererPivotOffset(s_ActiveTransforms[0].gameObject.GetComponent<SpriteRenderer> ());
		}

		private static Vector2 GetSpriteRendererPivotOffset(SpriteRenderer spriteRenderer)
		{
			return Vector2.zero;
		}

		private static void SetActiveSpritePivotOffset(Vector2 pivotOffset)
		{
		}

		private static void SetupPivotTransform(Quaternion startupRotation)
		{
			if(s_HiddenPivotGO != null)
				GameObject.DestroyImmediate(s_HiddenPivotGO);

			s_HiddenPivotGO = EditorUtility.CreateGameObjectWithHideFlags("pivot", HideFlags.HideAndDontSave);

			pivot.localScale = new Vector3(1f,1f,1f);
			pivot.position = Vector3.zero;
			pivot.rotation = startupRotation;
		}

		private static void PopulateHiddenPivotHierarchy()
		{
			for (int i = 0; i < s_ActiveTransforms.Length; i++)
			{
				GameObject newChild = EditorUtility.CreateGameObjectWithHideFlags ("pivotchild", HideFlags.HideAndDontSave);

				newChild.transform.position = s_ActiveTransforms[i].transform.position;
				newChild.transform.rotation = s_ActiveTransforms[i].transform.rotation;
				newChild.transform.localScale = s_ActiveTransforms[i].transform.localScale;
				newChild.transform.parent = s_HiddenPivotGO.transform;
			}
		}
		
		private static Rect GetStartupRect ()
		{
			float minX = float.MaxValue - 1f;
			float maxX = float.MinValue + 1f;
			float minY = float.MaxValue - 1f;
			float maxY = float.MinValue + 1f;

			foreach (SpriteRenderer spriteRenderer in GetSelectedSpriteRenderers ())
			{
				if (spriteRenderer.sprite == null)
					continue;

				Vector3[] points = new Vector3[4];
				Vector2 spritePivot = GetSpriteRendererPivotOffset (spriteRenderer);

				Bounds bounds = spriteRenderer.sprite.bounds;
				points[0] = new Vector2 (bounds.min.x, bounds.min.y) + spritePivot;
				points[1] = new Vector2 (bounds.max.x, bounds.min.y) + spritePivot;
				points[2] = new Vector2 (bounds.max.x, bounds.max.y) + spritePivot;
				points[3] = new Vector2 (bounds.min.x, bounds.max.y) + spritePivot;

				foreach (Vector3 point in points)
				{
					Vector3 rectPoint = spriteRenderer.transform.TransformPoint(point);

					rectPoint = pivot.InverseTransformPoint(rectPoint);

					minX = Mathf.Min (rectPoint.x, minX);
					maxX = Mathf.Max (rectPoint.x, maxX);
					minY = Mathf.Min (rectPoint.y, minY);
					maxY = Mathf.Max (rectPoint.y, maxY);
				}
			}

			return new Rect (minX, minY, Mathf.Abs (maxX - minX), Mathf.Abs (maxY - minY));
		}

		private static MouseCursor GetScaleCursor (Vector2 direction)
		{

			float angle = Mathf.Atan2 (direction.x, direction.y) * Mathf.Rad2Deg;

			if (angle < 0f)
				angle = 360f + angle;

			if (angle < 0f + 27.5f)
				return MouseCursor.ResizeVertical;
			if (angle < 45f + 27.5f)
				return MouseCursor.ResizeUpRight;
			if (angle < 90f + 27.5f)
				return MouseCursor.ResizeHorizontal;
			if (angle < 135f + 27.5f)
				return MouseCursor.ResizeUpLeft;
			if (angle < 180f + 27.5f)
				return MouseCursor.ResizeVertical;
			if (angle < 225f + 27.5f)
				return MouseCursor.ResizeUpRight;
			if (angle < 270f + 27.5f)
				return MouseCursor.ResizeHorizontal;
			if (angle < 315f + 27.5f)
				return MouseCursor.ResizeUpLeft;
			else
				return MouseCursor.ResizeVertical;
		}

		private static void SetupRectSpaceCursorRects ()
		{
			float edgeRectSize = 12f;
			float rotateRectSize = 15f;

			Vector3 right = pivot.InverseTransformDirection (Vector3.right);
			Vector3 up = pivot.InverseTransformDirection (Vector3.up);

			float guiPixelWidth = (TransformTool2DUtility.ScreenToPivot(right) - TransformTool2DUtility.ScreenToPivot(Vector3.zero)).magnitude;
			float guiPixelHeight = (TransformTool2DUtility.ScreenToPivot(up) - TransformTool2DUtility.ScreenToPivot(Vector3.zero)).magnitude;

			Vector2 topleft = new Vector2 (s_Rect.xMin, s_Rect.yMax);
			Vector2 bottomright = new Vector2 (s_Rect.xMax, s_Rect.yMin);

			float x0 = topleft.x - guiPixelWidth * rotateRectSize;
			float x1 = topleft.x - guiPixelWidth * edgeRectSize * .5f;
			float x2 = topleft.x + guiPixelWidth * edgeRectSize * .5f;
			float x3 = bottomright.x - guiPixelWidth * edgeRectSize * .5f;
			float x4 = bottomright.x + guiPixelWidth * edgeRectSize * .5f;
			float x5 = bottomright.x + guiPixelWidth * rotateRectSize;

			float y0 = topleft.y + guiPixelHeight * rotateRectSize;
			float y1 = topleft.y + guiPixelHeight * edgeRectSize * .5f;
			float y2 = topleft.y - guiPixelHeight * edgeRectSize * .5f;
			float y3 = bottomright.y + guiPixelHeight * edgeRectSize * .5f;
			float y4 = bottomright.y - guiPixelHeight * edgeRectSize * .5f;
			float y5 = bottomright.y - guiPixelHeight * rotateRectSize;

			s_RectSpaceCursorRects = new RectSpaceCursorRect[14];

			int index = 0;
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (guiPixelWidth * edgeRectSize * -.5f, guiPixelWidth * edgeRectSize * -.5f, guiPixelWidth * edgeRectSize * .5f, guiPixelWidth * edgeRectSize * .5f, CursorRectType.Pivot, Vector3.zero);

			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x1, y2, x2, y3, CursorRectType.Scale, Vector3.left);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x3, y2, x4, y3, CursorRectType.Scale, Vector3.right);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x2, y1, x3, y2, CursorRectType.Scale, Vector3.up);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x2, y3, x3, y4, CursorRectType.Scale, Vector3.down);

			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x1, y1, x2, y2, CursorRectType.Scale, (Vector3.left + Vector3.up).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x3, y1, x4, y2, CursorRectType.Scale, (Vector3.right + Vector3.up).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x3, y3, x4, y4, CursorRectType.Scale, (Vector3.right + Vector3.down).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x1, y3, x2, y4, CursorRectType.Scale, (Vector3.left + Vector3.down).normalized);

			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (topleft.x, topleft.y, bottomright.x, bottomright.y, CursorRectType.Move, Vector3.zero);

			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x0, y0, x2, y2, CursorRectType.Rotate, (Vector3.left + Vector3.up).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x3, y0, x5, y2, CursorRectType.Rotate, (Vector3.right + Vector3.up).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x3, y3, x5, y5, CursorRectType.Rotate, (Vector3.right + Vector3.down).normalized);
			s_RectSpaceCursorRects[index++] = new RectSpaceCursorRect (x0, y3, x2, y5, CursorRectType.Rotate, (Vector3.left + Vector3.down).normalized);
		}

		private static void UpdateCursor ()
		{
			if (Event.current.type != EventType.MouseMove && Event.current.type != EventType.Repaint)
				return;

			SetupRectSpaceCursorRects ();

			Vector3 mousePos = Event.current.mousePosition;
			Vector3 mouseRectPos = TransformTool2DUtility.ScreenToPivot(mousePos);

			// 200px x 200px rect around mousepos to switch cursor via fake cursorRect. Oh so dirty.
			Rect mouseScreenRect = new Rect (mousePos.x - 100f, mousePos.y - 100f, 200f, 200f);

			if (GUIUtility.hotControl == 0)
			{
				foreach (RectSpaceCursorRect r in s_RectSpaceCursorRects)
				{
					if (TransformTool2DUtility.TooSmallForScaling() && r.type == CursorRectType.Scale)
						continue;

					if (r.rect.Contains (mouseRectPos))
					{
						Vector2 dir = TransformTool2DUtility.PivotToScreenDirection(r.direction);
						dir.y *= -1f;
						dir = Vector2.Scale(dir, (Vector2)pivot.localScale).normalized;
						MouseCursor newCursor = MouseCursor.Arrow;

						switch (r.type)
						{
							case(CursorRectType.Rotate):
								newCursor = MouseCursor.RotateArrow;
							
								break;
							case(CursorRectType.Scale):
								newCursor = GetScaleCursor (dir);
								break;
						}

						if (Event.current.type == EventType.Repaint)
						{
							EditorGUIUtility.AddCursorRect (mouseScreenRect, newCursor);
							s_LastCursor = newCursor;
						}
						else // On MouseMove, ask for repaint if cursor needs changing
						{
							if (newCursor != s_LastCursor)
								Event.current.Use ();
						}

						return;
					}
				}

				// If it was outside all the rects, revert to Arrow
				if (s_LastCursor != MouseCursor.Arrow && Event.current.type == EventType.MouseMove)
				{
					s_LastCursor = MouseCursor.Arrow;
					Event.current.Use ();
				}
			}
			else // On hotControl != 0, make a fullscreen cursorRect
			{
				if (Event.current.type == EventType.Repaint)
					EditorGUIUtility.AddCursorRect (mouseScreenRect, s_LastCursor);
			}
		}

		private static SpriteRenderer[] GetSelectedSpriteRenderers ()
		{
			List<SpriteRenderer> spriteRenderers = new List<SpriteRenderer> ();
			foreach (GameObject go in Selection.gameObjects)
			{
				if (go.transform.gameObject.GetComponent<SpriteRenderer> ())
				{
					spriteRenderers.Add (go.transform.gameObject.GetComponent<SpriteRenderer> ());
				}
			}
			return spriteRenderers.ToArray ();
		}
		
		private static void InitStyles()
		{
			if (s_Styles == null)
				s_Styles = new Styles();
		}
	}
} // namespace
#endif
