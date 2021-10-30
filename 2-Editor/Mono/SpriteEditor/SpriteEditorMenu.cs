using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using System;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class SpriteEditorMenu : EditorWindow
	{
		public readonly string[] spriteAlignmentOptions =
		{
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Center").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopLeft").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Top").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.TopRight").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Left").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Right").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomLeft").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.Bottom").text,
			EditorGUIUtility.TextContent ("SpriteInspector.Pivot.BottomRight").text
		};

		public readonly string[] slicingMethodOptions =
		{
			EditorGUIUtility.TextContent ("SpriteEditor.Slicing.DeleteAll").text,
			EditorGUIUtility.TextContent ("SpriteEditor.Slicing.Smart").text,
			EditorGUIUtility.TextContent ("SpriteEditor.Slicing.Safe").text
		};

		public static SpriteEditorMenu s_SpriteEditorMenu;
		private static Styles s_Styles;
		private static long s_LastClosedTime;
		private static int s_Selected;

		private static Vector2 s_GridSpriteSize = new Vector2 (64, 64);

		private static int s_MinSpriteSize = 4;
		private static int s_AutoSlicingMethod = (int)SpriteEditorWindow.AutoSlicingMethod.DeleteAll;
		private static int s_SpriteAlignment;
		
		private static SlicingType s_SlicingType;
		private enum SlicingType { Automatic = 0, Grid = 1 }

		public static SpriteEditorWindow s_SpriteEditor;

		private class Styles
		{
			public GUIStyle background = "grey_border";
			public GUIStyle notice;

			public Styles()
			{
				notice = new GUIStyle(GUI.skin.label);
				notice.alignment = TextAnchor.MiddleCenter;
				notice.normal.textColor = Color.yellow;
				notice.wordWrap = true;
			}
		}

		private void Init (Rect buttonRect)
		{
			buttonRect = GUIUtility.GUIToScreenRect (buttonRect);
			float windowHeight = 145;
			var windowSize = new Vector2 (300, windowHeight);
			ShowAsDropDown (buttonRect, windowSize);
		}

		private void OnDisable ()
		{
			s_LastClosedTime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
			s_SpriteEditorMenu = null;
		}

		internal static bool ShowAtPosition (Rect buttonRect)
		{
			// We could not use realtimeSinceStartUp since it is set to 0 when entering/exitting playmode, we assume an increasing time when comparing time.
			long nowMilliSeconds = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
			bool justClosed = nowMilliSeconds < s_LastClosedTime + 50;
			if (!justClosed)
			{
				Event.current.Use ();
				if (s_SpriteEditorMenu == null)
					s_SpriteEditorMenu = CreateInstance<SpriteEditorMenu> ();

				s_SpriteEditorMenu.Init (buttonRect);
				return true;
			}
			return false;
		}

		private void OnGUI ()
		{
			if (s_Styles == null)
				s_Styles = new Styles ();

			EditorGUIUtility.labelWidth = 124f;
			EditorGUIUtility.wideMode = true;

			GUI.Label (new Rect (0, 0, position.width, position.height), GUIContent.none, s_Styles.background);

			s_SlicingType = (SlicingType)EditorGUILayout.EnumPopup ("Type", s_SlicingType);

			switch (s_SlicingType)
			{
				case SlicingType.Grid:
					OnGridGUI ();
					break;
				case SlicingType.Automatic:
					OnAutomaticGUI ();
					break;
			}

			GUILayout.BeginHorizontal();
			GUILayout.Space(EditorGUIUtility.labelWidth + 4);
			if (GUILayout.Button("Slice"))
				DoSlicing();
			GUILayout.EndHorizontal();
		}

		private void DoSlicing ()
		{
			DoAnalytics ();
			switch (s_SlicingType)
			{
				case SlicingType.Grid:
					DoGridSlicing ();
					break;
				case SlicingType.Automatic:
					DoAutomaticSlicing ();
					break;
			}
		}

		private void DoAnalytics()
		{
			Analytics.Event ("Sprite Editor", "Slice", "Type", (int)s_SlicingType);

			if (s_SlicingType == SlicingType.Automatic)
			{
				Analytics.Event ("Sprite Editor", "Slice", "Auto Slicing Min Size", (int)s_MinSpriteSize);
				Analytics.Event ("Sprite Editor", "Slice", "Auto Slicing Method", (int)s_AutoSlicingMethod);
			}
			else
			{
				Analytics.Event ("Sprite Editor", "Slice", "Grid Slicing Size X", (int)s_GridSpriteSize.x);
				Analytics.Event ("Sprite Editor", "Slice", "Grid Slicing Size Y", (int)s_GridSpriteSize.y);
			}
		}

		private void OnGridGUI()
		{
			s_GridSpriteSize = EditorGUILayout.Vector2Field("Pixel size", s_GridSpriteSize);
			s_GridSpriteSize.x = Mathf.Clamp((int)s_GridSpriteSize.x, 1, 4096);
			s_GridSpriteSize.y = Mathf.Clamp((int)s_GridSpriteSize.y, 1, 4096);
			
			s_SpriteAlignment = EditorGUILayout.Popup("Pivot", s_SpriteAlignment, spriteAlignmentOptions);
			GUILayout.Space(58f);
		}

		private void OnAutomaticGUI()
		{
			float spacing = 40f;
			if (s_SpriteEditor.originalTexture != null && UnityEditor.TextureUtil.IsCompressedTextureFormat(s_SpriteEditor.originalTexture.format))
			{
				EditorGUILayout.LabelField("Automating slicing of compressed textures gives imperfect results!", s_Styles.notice);
				spacing -= 31f;
			}

			s_MinSpriteSize = EditorGUILayout.IntField("Minimum Size", s_MinSpriteSize);

			s_MinSpriteSize = Mathf.Clamp (s_MinSpriteSize, 1, 4096);

			s_SpriteAlignment = EditorGUILayout.Popup("Pivot", s_SpriteAlignment, spriteAlignmentOptions);
			s_AutoSlicingMethod = EditorGUILayout.Popup("Method", s_AutoSlicingMethod, slicingMethodOptions);
			GUILayout.Space(spacing);
		}

		private void DoAutomaticSlicing ()
		{
			s_SpriteEditor.DoAutomaticSlicing(s_MinSpriteSize, s_SpriteAlignment, (SpriteEditorWindow.AutoSlicingMethod)s_AutoSlicingMethod);
		}

		private void DoGridSlicing()
		{
			s_SpriteEditor.DoGridSlicing((int)s_GridSpriteSize.x, (int)s_GridSpriteSize.y, s_SpriteAlignment);
		}
	}
}
