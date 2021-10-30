using System;
using UnityEngine;

namespace UnityEditor
{

	internal class SceneFXWindow : EditorWindow
	{
		private class Styles
		{
			public GUIStyle background = "grey_border";
			public GUIStyle menuItem = "MenuItem";
		}

		private static SceneFXWindow s_SceneFXWindow;
		private static long s_LastClosedTime;
		private static Styles s_Styles;
		private SceneView m_SceneView;

		const float kFrameWidth = 1f;

		private float GetHeight()
		{
			return EditorGUI.kSingleLineHeight * 4;
		}

		private SceneFXWindow()
		{
			hideFlags = HideFlags.DontSave;
			wantsMouseMove = true;
		}

		private void OnDisable()
		{
			s_LastClosedTime = System.DateTime.Now.Ticks / System.TimeSpan.TicksPerMillisecond;
			s_SceneFXWindow = null;
		}

		internal static bool ShowAtPosition(Rect buttonRect, SceneView view)
		{
			// We could not use realtimeSinceStartUp since it is set to 0 when entering/exitting playmode, we assume an increasing time when comparing time.
			long nowMilliSeconds = System.DateTime.Now.Ticks / System.TimeSpan.TicksPerMillisecond;
			bool justClosed = nowMilliSeconds < s_LastClosedTime + 50;
			if (!justClosed)
			{
				Event.current.Use();
				if (s_SceneFXWindow == null)
					s_SceneFXWindow = CreateInstance<SceneFXWindow>();
				s_SceneFXWindow.Init (buttonRect, view);
				return true;
			}
			return false;
		}

		private void Init(Rect buttonRect, SceneView view)
		{
			// Has to be done before calling Show / ShowWithMode
			buttonRect = GUIUtility.GUIToScreenRect (buttonRect);

			m_SceneView = view;

			var windowHeight = 2f * kFrameWidth + GetHeight();
			windowHeight = Mathf.Min (windowHeight, 900);
			var windowSize = new Vector2 (150, windowHeight);

			ShowAsDropDown (buttonRect, windowSize);
		}

		internal void OnGUI()
		{
			// We do not use the layout event
			if (Event.current.type == EventType.layout)
				return;
			
			if (Event.current.type == EventType.MouseMove)
				Event.current.Use ();
			
			if (s_Styles == null)
				s_Styles = new Styles();

			// Content
			Draw(GetHeight());

			// Background with 1 pixel border
			GUI.Label (new Rect (0, 0, position.width, position.height), GUIContent.none, s_Styles.background);
		}

		private void Draw(float height)
		{
			if (m_SceneView == null || m_SceneView.m_SceneViewState == null)
				return;
		
			var drawPos = new Rect(kFrameWidth, kFrameWidth, position.width - 2 * kFrameWidth, EditorGUI.kSingleLineHeight);
		
			var state = m_SceneView.m_SceneViewState;

			DrawListElement(drawPos, "Skybox", state.showSkybox, value => state.showSkybox = value);
			drawPos.y += EditorGUI.kSingleLineHeight;

			DrawListElement(drawPos, "Fog", state.showFog, value => state.showFog = value);
			drawPos.y += EditorGUI.kSingleLineHeight;

			DrawListElement(drawPos, "Flares", state.showFlares, value => state.showFlares = value);
			drawPos.y += EditorGUI.kSingleLineHeight;

			DrawListElement (drawPos, "Animated Materials", state.showMaterialUpdate, value => state.showMaterialUpdate = value);
			drawPos.y += EditorGUI.kSingleLineHeight;
		}

		void DrawListElement(Rect rect, string toggleName, bool value, Action<bool> setValue)
		{
			EditorGUI.BeginChangeCheck ();
			bool result = GUI.Toggle (rect, value, EditorGUIUtility.TempContent (toggleName), s_Styles.menuItem);
			if (EditorGUI.EndChangeCheck ())
			{
				setValue (result);
				m_SceneView.Repaint ();
			}
		}
	}
}
