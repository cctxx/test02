using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{
	[CustomEditor(typeof(Camera))]
	[CanEditMultipleObjects]
	internal class CameraEditor : Editor
	{
		SerializedProperty m_ClearFlags;
		SerializedProperty m_BackgroundColor;
		SerializedProperty m_NormalizedViewPortRect;
		SerializedProperty m_NearClip;
		SerializedProperty m_FarClip;
		SerializedProperty m_FieldOfView;
		SerializedProperty m_Orthographic;
		SerializedProperty m_OrthographicSize;
		SerializedProperty m_Depth;
		SerializedProperty m_CullingMask;
		//SerializedProperty m_MouseEventMask;
		SerializedProperty m_RenderingPath;
        SerializedProperty m_OcclusionCulling;
        SerializedProperty m_TargetTexture;
		SerializedProperty m_HDR;

		readonly AnimValueManager m_Anims = new AnimValueManager();
		readonly AnimBool m_ShowBGColorOptions = new AnimBool();
		readonly AnimBool m_ShowOrthoOptions = new AnimBool();
        readonly AnimBool m_ShowDeferredWarning = new AnimBool();

        private Camera camera { get { return target as Camera; } }
        private bool deferredWarningValue { get { return (!InternalEditorUtility.HasPro() && (camera.renderingPath == RenderingPath.DeferredLighting || (PlayerSettings.renderingPath == RenderingPath.DeferredLighting && camera.renderingPath == RenderingPath.UsePlayerSettings) ) ); } }

		enum ProjectionType { Perspective, Orthographic };
		
		private Camera m_PreviewCamera;
		private Camera previewCamera
		{
			get{
				if(m_PreviewCamera == null)
					m_PreviewCamera = EditorUtility.CreateGameObjectWithHideFlags("Preview Camera", HideFlags.HideAndDontSave, typeof(Camera)).GetComponent<Camera>();
				m_PreviewCamera.enabled = false;
				return m_PreviewCamera;
			}
		}
		
		// should match color in GizmosDrawers.cpp
		private static readonly Color kGizmoCamera = new Color(233f/255f, 233f/255f, 233f/255f, 128f/255f);

		private const float kPreviewWindowOffset = 10f;
		private const float kPreviewNormalizedSize = 0.2f;
		
		private GUIContent m_ClipingPlanesLabel = new GUIContent ("Clipping Planes");
		private GUIContent m_NearClipPlaneLabel = new GUIContent ("Near");
		private GUIContent m_FarClipPlaneLabel = new GUIContent ("Far");
		private GUIContent m_ViewportLabel = new GUIContent ("Viewport Rect");
		
		public void OnEnable()
		{
			m_ClearFlags = serializedObject.FindProperty("m_ClearFlags");
			m_BackgroundColor = serializedObject.FindProperty("m_BackGroundColor");
			m_NormalizedViewPortRect = serializedObject.FindProperty("m_NormalizedViewPortRect");
			m_NearClip = serializedObject.FindProperty("near clip plane");
			m_FarClip = serializedObject.FindProperty("far clip plane");
			m_FieldOfView = serializedObject.FindProperty("field of view");
			m_Orthographic = serializedObject.FindProperty("orthographic");
			m_OrthographicSize = serializedObject.FindProperty("orthographic size");
			m_Depth = serializedObject.FindProperty("m_Depth");
			m_CullingMask = serializedObject.FindProperty("m_CullingMask");
			//m_MouseEventMask = serializedObject.FindProperty("m_MouseEventMask");
 			m_RenderingPath = serializedObject.FindProperty("m_RenderingPath");
            m_OcclusionCulling = serializedObject.FindProperty("m_OcclusionCulling");
			m_TargetTexture = serializedObject.FindProperty("m_TargetTexture");
			m_HDR = serializedObject.FindProperty("m_HDR");

			Camera c = (Camera)target;
			m_ShowBGColorOptions.value = !m_ClearFlags.hasMultipleDifferentValues && (c.clearFlags == CameraClearFlags.Color || c.clearFlags == CameraClearFlags.Skybox);
			m_ShowOrthoOptions.value = c.orthographic;
            m_ShowDeferredWarning.value = deferredWarningValue;

			m_Anims.Add(m_ShowBGColorOptions);
			m_Anims.Add(m_ShowOrthoOptions);
            m_Anims.Add(m_ShowDeferredWarning);
		}
		
		public override void OnInspectorGUI()
		{
			if (m_Anims.callback == null)
				m_Anims.callback = Repaint;
			
			serializedObject.Update();
			Camera c = (Camera)target;
			m_ShowBGColorOptions.target = !m_ClearFlags.hasMultipleDifferentValues && (c.clearFlags == CameraClearFlags.Color || c.clearFlags == CameraClearFlags.Skybox);
			m_ShowOrthoOptions.target = !m_Orthographic.hasMultipleDifferentValues && c.orthographic;
            m_ShowDeferredWarning.target = deferredWarningValue;

			EditorGUILayout.PropertyField(m_ClearFlags);
			if(EditorGUILayout.BeginFadeGroup(m_ShowBGColorOptions.faded))
				EditorGUILayout.PropertyField(m_BackgroundColor, new GUIContent("Background", "Camera clears the screen to this color before rendering."));
			EditorGUILayout.EndFadeGroup();
			EditorGUILayout.PropertyField(m_CullingMask);
			//EditorGUILayout.PropertyField(m_MouseEventMask);
			
			EditorGUILayout.Space();
			
			ProjectionType projectionType = m_Orthographic.boolValue ? ProjectionType.Orthographic : ProjectionType.Perspective;
			EditorGUI.BeginChangeCheck ();
			EditorGUI.showMixedValue = m_Orthographic.hasMultipleDifferentValues;
			projectionType = (ProjectionType)EditorGUILayout.EnumPopup("Projection", projectionType);
			EditorGUI.showMixedValue = false;
			if (EditorGUI.EndChangeCheck ())
				m_Orthographic.boolValue = (projectionType == ProjectionType.Orthographic);
			
			if (!m_Orthographic.hasMultipleDifferentValues)
			{
				if (EditorGUILayout.BeginFadeGroup(m_ShowOrthoOptions.faded))
					EditorGUILayout.PropertyField(m_OrthographicSize, new GUIContent("Size"));
				EditorGUILayout.EndFadeGroup();
				if (EditorGUILayout.BeginFadeGroup(1 - m_ShowOrthoOptions.faded))
					EditorGUILayout.Slider(m_FieldOfView, 1f, 179f, new GUIContent("Field of View"));
				EditorGUILayout.EndFadeGroup();
			}
			
			Rect r = EditorGUILayout.GetControlRect (true, EditorGUI.kSingleLineHeight * 2f);
			r.height = EditorGUI.kSingleLineHeight;
			
			// Main clipping planes label
			GUI.Label (r, m_ClipingPlanesLabel);
			
			// Make labels for near and far control appear righht in front of the number fields.
			r.xMin += EditorGUIUtility.labelWidth - 1; // Shift one pixel to align text with left edge of other controls above and below
			EditorGUIUtility.labelWidth = 32;
			
			// Near and far clip plane controls
			EditorGUI.PropertyField (r, m_NearClip, m_NearClipPlaneLabel);
			r.y += EditorGUI.kSingleLineHeight;
			EditorGUI.PropertyField (r, m_FarClip, m_FarClipPlaneLabel);
			
			// Reset label width
			EditorGUIUtility.labelWidth = 0;
			
			EditorGUILayout.PropertyField (m_NormalizedViewPortRect, m_ViewportLabel);
			
			EditorGUILayout.Space();
			EditorGUILayout.PropertyField(m_Depth);
			EditorGUILayout.PropertyField(m_RenderingPath);
			if (EditorGUILayout.BeginFadeGroup(m_ShowDeferredWarning.faded))
			{
				GUIContent msg = EditorGUIUtility.TextContent("CameraEditor.DeferredProOnly");
				EditorGUILayout.HelpBox(msg.text, MessageType.Warning, false);
			}
			
			EditorGUILayout.EndFadeGroup();
			EditorGUILayout.PropertyField(m_TargetTexture);
			EditorGUILayout.PropertyField(m_OcclusionCulling);
			EditorGUILayout.PropertyField(m_HDR);
			
			serializedObject.ApplyModifiedProperties();
		}

		public void OnOverlayGUI (Object target, SceneView sceneView)
		{
			if (target == null) return;
		
			// cache some deep values
			Camera c = (Camera)target;

			Vector2 previewSize = GameView.GetSizeOfMainGameView ();
			if (previewSize.x < 0f)
			{
				// Fallback to Scene View of not a valid game view size
				previewSize.x = sceneView.position.width;
				previewSize.y = sceneView.position.height;
			}
			// Apply normalizedviewport rect of camera
			Rect normalizedViewPortRect = c.rect;
			previewSize.x *= Mathf.Max(normalizedViewPortRect.width, 0f);
			previewSize.y *= Mathf.Max(normalizedViewPortRect.height, 0f);
			
			// Prevent using invalid previewSize
			if (previewSize.x <= 0f || previewSize.y <= 0f)
				return;

			float aspect = previewSize.x / previewSize.y;

			// Scale down (fit to scene view)
			previewSize.y = kPreviewNormalizedSize * sceneView.position.height;
			previewSize.x = previewSize.y * aspect;
			if (previewSize.y > sceneView.position.height * 0.5f)
			{
				previewSize.y = sceneView.position.height * 0.5f;
				previewSize.x = previewSize.y * aspect;
			}
			if (previewSize.x > sceneView.position.width* 0.5f)
			{
				previewSize.x = sceneView.position.width * 0.5f;
				previewSize.y = previewSize.x / aspect;
			}

			// Get and reserve rect
			Rect cameraRect = GUILayoutUtility.GetRect(previewSize.x, previewSize.y);

			// Render from bottom ??
			cameraRect.y = sceneView.position.height - cameraRect.y - cameraRect.height + 1f;
			
			if (Event.current.type == EventType.Repaint)
			{
				// setup camera and render
				previewCamera.CopyFrom(c);
				previewCamera.targetTexture = null;
				previewCamera.pixelRect = cameraRect;
				previewCamera.Render();
			}
		}
		
		static float GetGameViewAspectRatio (Camera fallbackCamera)
		{
			Vector2 gameViewSize = GameView.GetSizeOfMainGameView ();
			if (gameViewSize.x < 0f)
			{
				// Fallback to Scene View of not a valid game view size
				gameViewSize.x = Screen.width;
				gameViewSize.y = Screen.height;
			}

			return gameViewSize.x / gameViewSize.y;
		}
		
		static float GetFrustumAspectRatio (Camera camera)
		{
			Rect normalizedViewPortRect = camera.rect;
			if (normalizedViewPortRect.width <= 0f || normalizedViewPortRect.height <= 0f)
				return -1f;

			float viewportAspect = normalizedViewPortRect.width / normalizedViewPortRect.height;
			return GetGameViewAspectRatio (camera) * viewportAspect;
		}


		// Returns near- and far-corners in this order: leftBottom, leftTop, rightTop, rightBottom
		// Assumes input arrays are of length 4 (if allocated)
		static bool GetFrustum (Camera camera, Vector3[] near, Vector3[] far, out float frustumAspect)
		{
			frustumAspect = GetFrustumAspectRatio (camera);
			if (frustumAspect < 0)
				return false;

			float yAxis;
			float xAxis;
			float xAxisFar;
			float xAxisNear;
			float yAxisFar;
			float yAxisNear;
			if (!camera.isOrthoGraphic)
			{
				yAxis = Mathf.Tan(camera.fieldOfView * Mathf.Deg2Rad * 0.5f);
				xAxis = yAxis * frustumAspect;

				xAxisFar = xAxis * camera.farClipPlane;
				yAxisFar = yAxis * camera.farClipPlane;
				xAxisNear = xAxis * camera.nearClipPlane;
				yAxisNear = yAxis * camera.nearClipPlane;
			}
			else
			{
				yAxis = camera.orthographicSize;
				xAxis = yAxis * frustumAspect;

				xAxisFar = xAxis;
				yAxisFar = yAxis;
				xAxisNear = xAxis;
				yAxisNear = yAxis;
			}

			// Local to world transform (ignore scale of transform)
			Matrix4x4 localToWorld = Matrix4x4.TRS(camera.transform.position, camera.transform.rotation, Vector3.one);

			if (far != null)
			{
				far[0] = new Vector3(-xAxisFar, -yAxisFar, camera.farClipPlane); // leftBottomFar
				far[1] = new Vector3(-xAxisFar, yAxisFar, camera.farClipPlane);	// leftTopFar
				far[2] = new Vector3(xAxisFar, yAxisFar, camera.farClipPlane);	// rightTopFar
				far[3] = new Vector3(xAxisFar, -yAxisFar, camera.farClipPlane);	// rightBottomFar
				for (int i = 0; i < 4; ++i)
					far[i] = localToWorld.MultiplyPoint(far[i]);
			}

			if (near != null)
			{
				near[0] = new Vector3(-xAxisNear, -yAxisNear, camera.nearClipPlane); // leftBottomNear
				near[1] = new Vector3(-xAxisNear, yAxisNear, camera.nearClipPlane);	// leftTopNear
				near[2] = new Vector3(xAxisNear, yAxisNear, camera.nearClipPlane);	// rightTopNear
				near[3] = new Vector3(xAxisNear, -yAxisNear, camera.nearClipPlane);	// rightBottomNear
				for (int i = 0; i < 4; ++i)
					near[i] = localToWorld.MultiplyPoint(near[i]);
			}
			return true;
		}

		// Called from C++ when we need to render a Camera's gizmo
		static void RenderGizmo (Camera camera)
		{
			Vector3[] near = new Vector3[4];
			Vector3[] far = new Vector3[4];
			float frustumAspect;
			if (GetFrustum (camera, near, far, out frustumAspect))
			{
			Color orgColor = Handles.color;
			Handles.color = kGizmoCamera;
			for (int i = 0; i < 4; ++i)
			{
				Handles.DrawLine(near[i], near[(i+1) % 4]);
				Handles.DrawLine(far[i], far[(i+1) % 4]);
				Handles.DrawLine(near[i], far[i]);
			}
				Handles.color = orgColor;
			}
		}

		static bool IsViewPortRectValidToRender (Rect normalizedViewPortRect)
		{
			if (normalizedViewPortRect.width <= 0f || normalizedViewPortRect.height <= 0f)
				return false;
			if (normalizedViewPortRect.x >= 1f || normalizedViewPortRect.xMax <= 0f)
				return false;
			if (normalizedViewPortRect.y >= 1f || normalizedViewPortRect.yMax <= 0f)
				return false;
			return true;
		}

		public void OnSceneGUI()
		{
			Camera c = (Camera)target;

			if (!IsViewPortRectValidToRender (c.rect))
				return;

			SceneViewOverlay.Window(new GUIContent("Camera Preview"), OnOverlayGUI, (int)SceneViewOverlay.Ordering.Camera, target, SceneViewOverlay.WindowDisplayOption.OneWindowPerTarget);

			Color orgHandlesColor = Handles.color;
			Color slidersColor = kGizmoCamera;
			slidersColor.a *= 2f;
			Handles.color = slidersColor;

			// get the corners of the far clip plane in world space
			Vector3[] far = new Vector3[4];
			float frustumAspect;
			if (!GetFrustum(c, null, far, out frustumAspect))
				return;
			Vector3 leftBottomFar = far[0];
			Vector3 leftTopFar = far[1];
			Vector3 rightTopFar = far[2];
			Vector3 rightBottomFar = far[3];

			// manage our own gui changed state, so we can use it for individual slider changes
			bool guiChanged = GUI.changed;

			// FOV handles
			Vector3 farMid = Vector3.Lerp(leftBottomFar, rightTopFar, 0.5f);

			// Top and bottom handles
			float halfHeight = -1.0f;
			Vector3 changedPosition = MidPointPositionSlider (leftTopFar, rightTopFar, c.transform.up);
			if (!GUI.changed)
				changedPosition = MidPointPositionSlider (leftBottomFar, rightBottomFar, -c.transform.up);
			if (GUI.changed)
				halfHeight = (changedPosition - farMid).magnitude;
			
			// Left and right handles
			GUI.changed = false;
			changedPosition = MidPointPositionSlider(rightBottomFar, rightTopFar, c.transform.right);
			if (!GUI.changed)
				changedPosition = MidPointPositionSlider(leftBottomFar, leftTopFar, -c.transform.right);
			if (GUI.changed)
				halfHeight = (changedPosition - farMid).magnitude / frustumAspect;

			// Update camera settings if changed
			if (halfHeight >= 0.0f)
			{
				Undo.RecordObject (c, "Adjust Camera");
				if (c.orthographic)
				{
					c.orthographicSize = halfHeight;
				}
				else
				{
					Vector3 pos = farMid + c.transform.up * halfHeight;
					c.fieldOfView = Vector3.Angle(c.transform.forward, (pos - c.transform.position)) * 2f;
				}
				guiChanged = true;
			}

			GUI.changed = guiChanged;
			Handles.color = orgHandlesColor;
		}
		
		private static Vector3 MidPointPositionSlider (Vector3 position1, Vector3 position2, Vector3 direction)
		{
			Vector3 midPoint = Vector3.Lerp(position1, position2, 0.5f);
			return Handles.Slider(midPoint, direction, HandleUtility.GetHandleSize(midPoint) * 0.03f, Handles.DotCap, 0f);
		}
	}
}

