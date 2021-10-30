#if ENABLE_SPRITES

using UnityEngine;
using UnityEditorInternal;

namespace UnityEditor
{
	internal class SpriteUtilityWindow : EditorWindow
	{
		#region styles
		protected class Styles
		{
			public readonly GUIStyle dragdot = "U2D.dragDot";
			public readonly GUIStyle dragdotDimmed = "U2D.dragDotDimmed";
			public readonly GUIStyle dragdotactive = "U2D.dragDotActive";
			public readonly GUIStyle createRect = "U2D.createRect";
			public readonly GUIStyle preToolbar = "preToolbar";
			public readonly GUIStyle preButton = "preButton";
			public readonly GUIStyle preSlider = "preSlider";
			public readonly GUIStyle preSliderThumb = "preSliderThumb";
			public readonly GUIStyle preBackground = "preBackground";
			public readonly GUIStyle pivotdotactive = "U2D.pivotDotActive";
			public readonly GUIStyle pivotdot = "U2D.pivotDot";

			public readonly GUIStyle toolbar;
			public readonly GUIContent alphaIcon;
			public readonly GUIContent RGBIcon;
			public readonly GUIStyle notice;

			public Styles()
			{
				toolbar = new GUIStyle(EditorStyles.inspectorBig);
				toolbar.margin.top = 0;
				toolbar.margin.bottom = 0;
				alphaIcon = EditorGUIUtility.IconContent("PreTextureAlpha");
				RGBIcon = EditorGUIUtility.IconContent("PreTextureRGB");
				preToolbar.border.top = 0;
				createRect.border = new RectOffset(3, 3, 3, 3);

				notice = new GUIStyle(GUI.skin.label);
				notice.alignment = TextAnchor.MiddleCenter;
				notice.normal.textColor = Color.yellow;
			}
		}

		protected void InitStyles()
		{
			if (s_Styles == null)
				s_Styles = new Styles();
		}

		protected static Styles s_Styles;
		#endregion

		protected const float k_BorderMargin = 10f;
		protected const float k_ScrollbarMargin = 16f;
		protected const float k_InspectorWindowMargin = 8f;
		protected const float k_InspectorWidth = 330f;
		protected const float k_InspectorHeight = 148f;
		protected const float k_MinZoomPercentage = 0.9f;
		protected const float k_MaxZoom = 10f;
		protected const float k_WheelZoomSpeed = 0.03f;
		protected const float k_MouseZoomSpeed = 0.005f;

		protected Texture2D m_Texture;
		protected Rect m_TextureViewRect;
		protected Rect m_TextureRect;

		protected bool m_ShowAlpha = false;
		protected float m_Zoom = -1f;
		protected Vector2 m_ScrollPosition = new Vector2();

		protected float GetMinZoom()
		{
			if (m_Texture == null)
				return 1.0f;
			return Mathf.Min(m_TextureViewRect.width / m_Texture.width, m_TextureViewRect.height / m_Texture.height) * k_MinZoomPercentage;
		}

		protected void HandleZoom()
		{
			if (Event.current.type == EventType.ScrollWheel || Event.current.type == EventType.MouseDrag && Event.current.alt && Event.current.button == 1)
			{
				float zoomMultiplier = 1f;

				zoomMultiplier = 1f - Event.current.delta.y * (Event.current.type == EventType.ScrollWheel ? k_WheelZoomSpeed : -k_MouseZoomSpeed);

				// Clamp zoom
				float wantedZoom = m_Zoom * zoomMultiplier;

				float currentZoom = Mathf.Min(Mathf.Max(wantedZoom, GetMinZoom()), k_MaxZoom);

				if (currentZoom != m_Zoom)
				{
					m_Zoom = currentZoom;

					// We need to fix zoomMultiplier if we clamped wantedZoom != currentZoom
					if (wantedZoom != currentZoom)
						zoomMultiplier /= wantedZoom / currentZoom;

					m_ScrollPosition *= zoomMultiplier;

					Event.current.Use();
				}
			}
		}

		protected void HandlePanning()
		{
			// You can pan by holding ALT and using left button or NOT holding ALT and using right button. ALT + right is reserved for zooming.
			if (Event.current.type == EventType.MouseDrag && (!Event.current.alt && Event.current.button > 0 || Event.current.alt && Event.current.button == 0) && GUIUtility.hotControl == 0)
			{
				m_ScrollPosition -= Event.current.delta;
				Event.current.Use();
			}
		}

		// Bounding values for scrollbars. Changes with zoom, because we want min/max scroll to stop at texture edges.
		protected Rect maxScrollRect
		{
			get
			{
				float halfWidth = m_Texture.width * .5f * m_Zoom;
				float halfHeight = m_Texture.height * .5f * m_Zoom;
				return new Rect(-halfWidth, -halfHeight, m_TextureViewRect.width + halfWidth * 2f, m_TextureViewRect.height + halfHeight * 2f);
			}
		}

		// Max rect in texture space that can ever be visible
		protected Rect maxRect
		{
			get
			{
				float marginW = m_TextureViewRect.width * .5f / GetMinZoom();
				float marginH = m_TextureViewRect.height * .5f / GetMinZoom();
				float left = -marginW;
				float top = -marginH;
				float width = m_Texture.width + marginW * 2f;
				float height = m_Texture.height + marginH * 2f;
				return new Rect(left, top, width, height);
			}
		}

		protected void DrawTexturespaceBackground()
		{
			float size = Mathf.Max(maxRect.width, maxRect.height);
			Vector2 offset = new Vector2(maxRect.xMin, maxRect.yMin);

			float halfSize = size * .5f;
			float alpha = EditorGUIUtility.isProSkin ? 0.15f : 0.08f;
			float gridSize = 8f;

			SpriteEditorUtility.BeginLines(new Color(0f, 0f, 0f, alpha));
			for (float v = 0; v <= size; v += gridSize)
				SpriteEditorUtility.DrawLine(new Vector2(-halfSize + v, halfSize + v) + offset, new Vector2(halfSize + v, -halfSize + v) + offset);
			SpriteEditorUtility.EndLines();
		}

		protected void DrawTexture()
		{
			if (m_ShowAlpha)
				EditorGUI.DrawTextureAlpha(m_TextureRect, m_Texture);
			else
				EditorGUI.DrawTextureTransparent(m_TextureRect, m_Texture);
		}

		protected void DrawScreenspaceBackground()
		{
			if (Event.current.type == EventType.Repaint)
				s_Styles.preBackground.Draw(m_TextureViewRect, false, false, false, false);
		}

		protected void HandleScrollbars()
		{
			Rect horizontalScrollBarPosition = new Rect(m_TextureViewRect.xMin, m_TextureViewRect.yMax, m_TextureViewRect.width, k_ScrollbarMargin);
			m_ScrollPosition.x = GUI.HorizontalScrollbar(horizontalScrollBarPosition, m_ScrollPosition.x, m_TextureViewRect.width, maxScrollRect.xMin, maxScrollRect.xMax);

			Rect verticalScrollBarPosition = new Rect(m_TextureViewRect.xMax, m_TextureViewRect.yMin, k_ScrollbarMargin, m_TextureViewRect.height);
			m_ScrollPosition.y = GUI.VerticalScrollbar(verticalScrollBarPosition, m_ScrollPosition.y, m_TextureViewRect.height, maxScrollRect.yMin, maxScrollRect.yMax);
		}

		protected void SetupHandlesMatrix()
		{
			// Offset from top left to center in view space
			Vector3 handlesPos = new Vector3(m_TextureRect.x, m_TextureRect.yMax, 0f);
			// We flip Y-scale because Unity texture space is bottom-up
			Vector3 handlesScale = new Vector3(m_Zoom, -m_Zoom, 1f);

			// Handle matrix is for converting between view and texture space coordinates, without taking account the scroll position.
			// Scroll position is added separately so we can use it with GUIClip.
			Handles.matrix = Matrix4x4.TRS(handlesPos, Quaternion.identity, handlesScale);
		}

		protected void DoAlphaZoomToolbarGUI()
		{
			m_ShowAlpha = GUILayout.Toggle(m_ShowAlpha, m_ShowAlpha ? s_Styles.alphaIcon : s_Styles.RGBIcon, "toolbarButton");
			m_Zoom = GUILayout.HorizontalSlider(m_Zoom, GetMinZoom(), k_MaxZoom, s_Styles.preSlider, s_Styles.preSliderThumb, GUILayout.MaxWidth(64));
		}

		protected void DoTextureGUI()
		{
			if (m_Texture == null)
				return;

			// zoom startup init
			if (m_Zoom < 0f)
				m_Zoom = GetMinZoom();

			// Texture rect in view space
			m_TextureRect = new Rect(
				m_TextureViewRect.width / 2f - (m_Texture.width * m_Zoom / 2f),
				m_TextureViewRect.height / 2f - (m_Texture.height * m_Zoom / 2f),
				(m_Texture.width * m_Zoom),
				(m_Texture.height * m_Zoom)
				);

			HandleScrollbars();
			SetupHandlesMatrix();
			HandleZoom();
			HandlePanning();
			DrawScreenspaceBackground();

			GUIClip.Push(m_TextureViewRect, -m_ScrollPosition, Vector2.zero, false);

			if (Event.current.type == EventType.Repaint)
			{
				DrawTexturespaceBackground();
				DrawTexture();
				DrawGizmos();
			}

			DoTextureGUIExtras();

			GUIClip.Pop();
		}

		protected virtual void DoTextureGUIExtras()
		{
		}

		protected virtual void DrawGizmos()
		{
		}

		protected void SetNewTexture(Texture2D texture)
		{
			if (texture != m_Texture)
			{
				m_Texture = texture;
				m_Zoom = -1;
			}
		}



		internal override void OnResized()
		{
			if (m_Texture != null && Event.current != null)
				HandleZoom();
		}

	} // class
}

#endif
