#if ENABLE_SPRITES
using UnityEngine;

namespace UnityEditor
{
	internal class TransformTool2DUtility
	{
		public static bool RotatedEqually(Transform[] transforms)
		{
			for (int i = 0; i < transforms.Length - 1; i++)
			{
				if (transforms[i].rotation != transforms[i + 1].rotation)
					return false;
			}
			return true;
		}

		public static Vector3 PivotToWorld(Vector3 pos)
		{
			return TransformTool2D.pivot.TransformPoint(pos);
		}

		public static Vector3 WorldToPivot(Vector3 pos)
		{
			return TransformTool2D.pivot.InverseTransformPoint(pos);
		}

		public static Vector2 ScreenToPivot(Vector2 pos)
		{
			return WorldToPivot(ScreenToWorld(pos));
		}

		public static Vector3 PivotToScreen(Vector2 pos)
		{
			return WorldToScreen(PivotToWorld(pos));
		}

		public static Vector2 WorldToPivotDirection(Vector3 direction)
		{
			return TransformTool2D.pivot.transform.InverseTransformDirection(direction);
		}

		public static Vector3 PivotToWorldDirection(Vector2 direction)
		{
			return TransformTool2D.pivot.transform.TransformDirection(direction);
		}

		public static Vector2 WorldToScreenDirection(Vector2 direction)
		{
			return (WorldToScreen(direction) - WorldToScreen(Vector2.zero)).normalized;
		}

		public static Vector2 PivotToScreenDirection(Vector2 direction)
		{
			return WorldToScreenDirection(PivotToWorldDirection(direction));
		}

		public static Vector3 ScreenToWorldDelta(Vector2 screenStart, Vector2 screenDelta)
		{
			return ScreenToWorld(screenStart + screenDelta) - ScreenToWorld(screenStart);
		}

		public static float ScreenToWorldSize(float distance)
		{
			return Mathf.Abs(ScreenToWorld(new Vector2(distance, 0f)).x - ScreenToWorld(new Vector2()).x);
		}

		public static Vector3 ScreenToWorld(Vector2 screen)
		{
			Ray mouseRay = HandleUtility.GUIPointToWorldRay(screen);
			float dist = 0.0f;
			GetPivotPlane().Raycast(mouseRay, out dist);
			return mouseRay.GetPoint(dist);
		}

		public static Vector2 WorldToScreen(Vector3 world)
		{
			return HandleUtility.WorldToGUIPoint(world);
		}

		public static Plane GetPivotPlane()
		{
			return new Plane(TransformTool2D.pivot.forward, TransformTool2D.pivot.position);
		}

		public static float GetRectDiameter()
		{
			return (PivotToScreen(GetLocalRectPoint(0)) - PivotToScreen(GetLocalRectPoint(2))).magnitude;
		}

		public static float GetSizeBasedAlpha()
		{
			return Mathf.Clamp01((GetRectDiameter() - TransformTool2D.k_MinScalingDiameter) / TransformTool2D.k_MinScalingDiameter * 2f);
		}

		public static bool TooSmallForScaling()
		{
			bool tooSmall = GetRectDiameter() < TransformTool2D.k_MinScalingDiameter;
			return tooSmall;
		}

		public static Vector2 GetLocalRectPoint(int index)
		{
			Vector2 p = Vector2.zero;

			switch (index)
			{
				case (0):
					p = new Vector2(TransformTool2D.rect.xMin, TransformTool2D.rect.yMax);
					break;
				case (1):
					p = new Vector2(TransformTool2D.rect.xMax, TransformTool2D.rect.yMax);
					break;
				case (2):
					p = new Vector2(TransformTool2D.rect.xMax, TransformTool2D.rect.yMin);
					break;
				case (3):
					p = new Vector2(TransformTool2D.rect.xMin, TransformTool2D.rect.yMin);
					break;
			}

			return p;
		}

		public static Vector2 GetWorldRectPoint (int index)
		{
			return PivotToWorld (GetLocalRectPoint(index));
		}

		public static Vector3[] GetRectPolyline ()
		{
			Vector3[] points = new Vector3[5];

			for (int i = 0; i < 4; i++)
				points[i] = TransformTool2DUtility.GetWorldRectPoint (i);
			points[4] = points[0];

			return points;
		}

		public static void RenderRectWithShadow(bool active)
		{
			Vector3[] verts = new Vector3[5];

			for (int i = 0; i < 4; i++)
			{
				Vector3 pos = TransformTool2DUtility.GetLocalRectPoint(i);
				verts[i] = TransformTool2DUtility.PivotToWorld(pos);
			}

			verts[4] = verts[0];

			// shadow
			Handles.color = new Color(0f, 0f, 0f, active ? 1f : 0.5f);
			Handles.DrawPolyLine(verts);

			// shift 1,1
			for (int i = 0; i < verts.Length; i++)
			{
				verts[i] = TransformTool2DUtility.WorldToScreen(verts[i]);
				verts[i] -= new Vector3(1f, 1f, 0f);
				verts[i] = TransformTool2DUtility.ScreenToWorld(verts[i]);
			}

			// rect
			Handles.color = new Color(1f, 1f, 1f, active ? 1f : 0.5f);
			Handles.DrawPolyLine(verts);

			Handles.color = Color.white;
		}

		public static Vector3 GetNiceEulers(Vector3 eulerAngles)
		{
			Vector3 result = eulerAngles;

			if (Mathf.Abs(eulerAngles.z - Mathf.Round(eulerAngles.z)) < TransformTool2D.k_RotatingEpsilon)
				result = new Vector3(eulerAngles.x, eulerAngles.y, Mathf.Round(eulerAngles.z));

			return result;
		}
	}
}
#endif
