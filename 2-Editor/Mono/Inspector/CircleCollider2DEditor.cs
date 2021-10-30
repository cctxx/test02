using System;
using UnityEngine;

namespace UnityEditor
{
	[CustomEditor (typeof (CircleCollider2D))]
	[CanEditMultipleObjects]
	internal class CircleCollider2DEditor : Collider2DEditorBase
	{
		private int m_HandleControlID;

		public override void OnEnable ()
		{
			base.OnEnable ();
			m_HandleControlID = -1;
		}

		public void OnSceneGUI ()
		{
			bool dragging = GUIUtility.hotControl == m_HandleControlID;

			CircleCollider2D sphere = (CircleCollider2D)target;

			Color tempColor = Handles.color;
			Handles.color = Handles.s_ColliderHandleColor;
			
			// We only show handles if shift is pressed or we are already 
			bool orgGUIEnabled = GUI.enabled;
			if (!Event.current.shift && !dragging)
			{
				GUI.enabled = false;
				Handles.color = new Color (0, 0, 0, .001f);
			}

			// The scaled radius is similiar to the implemenation in SphereCollider::GetScaledRadius() in SphereCollider.cpp
			// (which is used for rendering the circular arcs)
			Vector3 scale = sphere.transform.lossyScale;
			float maxScale = Mathf.Max (Mathf.Max (Mathf.Abs (scale.x), Mathf.Abs (scale.y)), Mathf.Abs (scale.z));
			float absoluteRadius = maxScale * sphere.radius;
			absoluteRadius = Mathf.Abs (absoluteRadius);
			absoluteRadius = Mathf.Max (absoluteRadius, 0.00001F);

			Vector3 position = sphere.transform.TransformPoint (sphere.center);

			Quaternion rotation = sphere.transform.rotation;

			int prevHotControl = GUIUtility.hotControl;

			float newRadius = Handles.RadiusHandle (rotation, position, absoluteRadius, true);
			if (GUI.changed)
			{
				Undo.RecordObject (sphere, "Adjust Radius");
				sphere.radius = newRadius * 1 / maxScale;
			}

			// Detect if any of the handles got hotcontrol
			if (prevHotControl != GUIUtility.hotControl && GUIUtility.hotControl != 0)
				m_HandleControlID = GUIUtility.hotControl;

			Handles.color = tempColor;

			GUI.enabled = orgGUIEnabled;
		}
	}
}
