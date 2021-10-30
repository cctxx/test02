using UnityEngine;

namespace UnityEditor
{
	[CustomEditor(typeof(CapsuleCollider))]
	[CanEditMultipleObjects]
	internal class CapsuleColliderEditor : ColliderEditorBase
	{
		SerializedProperty m_Center;
		SerializedProperty m_Radius;
		SerializedProperty m_Height;
		SerializedProperty m_Direction;
		private int m_HandleControlID;

		public override void OnEnable()
		{
			base.OnEnable ();

			m_Center = serializedObject.FindProperty("m_Center");
			m_Radius = serializedObject.FindProperty("m_Radius");
			m_Height = serializedObject.FindProperty("m_Height");
			m_Direction = serializedObject.FindProperty("m_Direction");
			m_HandleControlID = -1;
		}

		public void OnDisable()
		{
		}

		public override void OnInspectorGUI()
		{
			serializedObject.Update();

			EditorGUILayout.PropertyField(m_IsTrigger);
			EditorGUILayout.PropertyField(m_Material);
			EditorGUILayout.PropertyField(m_Center);
			EditorGUILayout.PropertyField(m_Radius);
			EditorGUILayout.PropertyField(m_Height);
			EditorGUILayout.PropertyField(m_Direction);

			serializedObject.ApplyModifiedProperties();
		}

		public void OnSceneGUI()
		{
			bool dragging = GUIUtility.hotControl == m_HandleControlID;

			CapsuleCollider capsule = (CapsuleCollider)target;
			
			// Use our own color for handles
			Color tempColor = Handles.color;
			if (capsule.enabled)
				Handles.color = Handles.s_ColliderHandleColor;
			else
				Handles.color = Handles.s_ColliderHandleColorDisabled;

			bool orgGuiEnabled = GUI.enabled;
			if (!Event.current.shift && !dragging)
			{
				GUI.enabled = false;
				Handles.color = new Color (1,0,0,.001f);
			}
			
			Vector3 extents = ColliderUtil.GetCapsuleExtents(capsule);
			float height = extents.y + 2.0f * extents.x;
			float radius = extents.x;

			Matrix4x4 matrix = ColliderUtil.CalculateCapsuleTransform(capsule);

			int prevHotControl = GUIUtility.hotControl;

			// Height (two handles)
			Vector3 halfHeight = Vector3.up * height * 0.5f;
			float adjusted = SizeHandle(halfHeight, Vector3.up, matrix, true);
			if (!GUI.changed)
				adjusted = SizeHandle(-halfHeight, Vector3.down, matrix, true);
			if (GUI.changed)
			{
				float heightScale = height / capsule.height; 
				capsule.height += adjusted / heightScale;
			}

			// Radius (four handles)
			adjusted = SizeHandle(Vector3.left * radius, Vector3.left, matrix, true);
			if (!GUI.changed)
				adjusted = SizeHandle(-Vector3.left * radius, -Vector3.left, matrix, true);
			if (!GUI.changed)
				adjusted = SizeHandle(Vector3.forward * radius, Vector3.forward, matrix, true);
			if (!GUI.changed)
				adjusted = SizeHandle(-Vector3.forward * radius, -Vector3.forward, matrix, true);
			if (GUI.changed)
			{
				float radiusScale = Mathf.Max(extents.z / capsule.radius, extents.x / capsule.radius); 
				capsule.radius += adjusted / radiusScale;
			}

			// Detect if any of our handles got hotcontrol
			if (prevHotControl != GUIUtility.hotControl && GUIUtility.hotControl != 0)
				m_HandleControlID = GUIUtility.hotControl;

			if (GUI.changed)
			{
				Undo.RecordObject (capsule, "Modified Box Collider");
				const float minValue = 0.00001f;
				capsule.radius = Mathf.Max (capsule.radius, minValue);
				capsule.height = Mathf.Max (capsule.height, minValue);
			}

			// Reset original color
			Handles.color = tempColor;
			GUI.enabled = orgGuiEnabled;
		}

		private static float SizeHandle(Vector3 localPos, Vector3 localPullDir, Matrix4x4 matrix, bool isEdgeHandle)
		{
			Vector3 worldDir = matrix.MultiplyVector(localPullDir);
			Vector3 worldPos = matrix.MultiplyPoint(localPos);

			float handleSize = HandleUtility.GetHandleSize(worldPos);
			bool orgGUIchanged = GUI.changed;
			GUI.changed = false;
			Color tempColor = Handles.color;
			
			// Adjust color of handle if in background
			float displayThreshold = 0.0f;
			if (isEdgeHandle)
				displayThreshold = Mathf.Cos(Mathf.PI * 0.25f);
			float cosV;
			if (Camera.current.isOrthoGraphic)
				cosV = Vector3.Dot( -Camera.current.transform.forward, worldDir);
			else
				cosV = Vector3.Dot((Camera.current.transform.position - worldPos).normalized, worldDir);
			if (cosV < -displayThreshold)
				Handles.color = new Color(Handles.color.r, Handles.color.g, Handles.color.b, Handles.color.a * Handles.backfaceAlphaMultiplier);
			
			// Now do handle
			Vector3 newWorldPos = Handles.Slider(worldPos, worldDir, handleSize * 0.03f, Handles.DotCap, 0f);
			float adjust = 0.0f;
			if (GUI.changed)
			{
				// Project newWorldPos to worldDir (the sign of the return value indicates if we growing or shrinking)
				adjust = HandleUtility.PointOnLineParameter(newWorldPos, worldPos, worldDir);
			}

			// Reset states
			GUI.changed |= orgGUIchanged;
			Handles.color = tempColor;

			return adjust;
		}
	}
}
