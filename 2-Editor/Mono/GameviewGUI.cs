using UnityEngine;
using UnityEditor;
using System.Collections;
using System;
using System.Text;

namespace UnityEditor
{
	internal class GameViewGUI  {
		private static string FormatNumber (int num) {
			if (num < 1000)
				return num.ToString();
			if (num < 1000000)
				return (num*0.001).ToString("f1") + "k";
			return (num*0.000001).ToString("f1") + "M";
		}

		private static int m_FrameCounter = 0;
		private static float m_ClientTimeAccumulator = 0.0f;
		private static float m_RenderTimeAccumulator = 0.0f;
		private static float m_MaxTimeAccumulator = 0.0f;
		private static float m_ClientFrameTime = 0.0f;
		private static float m_RenderFrameTime = 0.0f;
		private static float m_MaxFrameTime = 0.0f;

		// Prevent using implicit creation of GUIStyle with strings. We therefore cache the dark
		// skin for BoldLabels. Using EditorStyles.boldLabel does not work for light skin because
		// it is the light skin style cached.
		private static GUIStyle s_SectionHeaderStyle;
		private static GUIStyle sectionHeaderStyle
		{
			get
			{
				if (s_SectionHeaderStyle == null)
					s_SectionHeaderStyle = EditorGUIUtility.GetBuiltinSkin (EditorSkin.Scene).GetStyle ("BoldLabel");
				return s_SectionHeaderStyle;
			}
		}            

		// Time per frame varies wildly, so we average a bit and display that.
		private static void UpdateFrameTime ()
		{
			if (Event.current.type != EventType.Repaint)
				return;

			float frameTime = UnityStats.frameTime;
			float renderTime = UnityStats.renderTime;
			m_ClientTimeAccumulator += frameTime;
			m_RenderTimeAccumulator += renderTime;
			m_MaxTimeAccumulator += Mathf.Max(frameTime, renderTime);
			++m_FrameCounter;

			bool needsTime = m_ClientFrameTime == 0.0f && m_RenderFrameTime == 0.0f;
			bool resetTime = m_FrameCounter > 30 || m_ClientTimeAccumulator > 0.3f || m_RenderTimeAccumulator > 0.3f;

			if (needsTime || resetTime)
			{
				m_ClientFrameTime = m_ClientTimeAccumulator / m_FrameCounter;
				m_RenderFrameTime = m_RenderTimeAccumulator / m_FrameCounter;
				m_MaxFrameTime = m_MaxTimeAccumulator / m_FrameCounter;
			}
			if (resetTime)
			{
				m_ClientTimeAccumulator = 0.0f;
				m_RenderTimeAccumulator = 0.0f;
				m_MaxTimeAccumulator = 0.0f;
				m_FrameCounter = 0;
			}
		}
	
		public static void GameViewStatsGUI ()
		{
			GUI.skin = EditorGUIUtility.GetBuiltinSkin (EditorSkin.Scene);

			GUI.color = new Color (1,1,1,.75f);			
			float w = 300, h = 200;
			
			// Increase windows size to make room for network data
			int connectionCount = Network.connections.Length;
			if (connectionCount != 0)
				h += 220;
			
			GUILayout.BeginArea (new Rect (Screen.width - w - 10, 27, w, h), "Statistics", GUI.skin.window);
			
			
			// Graphics stats
			GUILayout.Label ("Graphics:", sectionHeaderStyle);
			
			// Time stats
			UpdateFrameTime ();

			string timeStats = System.String.Format("{0:F1} FPS ({1:F1}ms)",
				1.0f / Mathf.Max(m_MaxFrameTime, 1.0e-5f), m_MaxFrameTime * 1000.0f);
				GUI.Label (new Rect(170,19,120,20), timeStats);
			
			int textureBytes = UnityStats.usedTextureMemorySize;
			int renderTextureBytes = UnityStats.renderTextureBytes;
			int screenBytes = UnityStats.screenBytes;
			int vboBytes = UnityStats.vboTotalBytes;
			int vramUsageMin = screenBytes + renderTextureBytes;
			int vramUsageMax = screenBytes + Mathf.Max(textureBytes,renderTextureBytes) + vboBytes;
			StringBuilder gfxStats = new StringBuilder(400);
			gfxStats.Append (String.Format("  Main Thread: {0:F1}ms  Renderer: {1:F1}ms\n", m_ClientFrameTime * 1000.0f, m_RenderFrameTime * 1000.0f));
			gfxStats.Append (String.Format("  Draw Calls: {0} \tSaved by batching: {1} \n", UnityStats.drawCalls, UnityStats.batchedDrawCalls - UnityStats.batches));
			gfxStats.Append (String.Format("  Tris: {0} \tVerts: {1} \n", FormatNumber(UnityStats.triangles), FormatNumber(UnityStats.vertices)));
			gfxStats.Append (String.Format("  Used Textures: {0} - {1}\n", UnityStats.usedTextureCount, EditorUtility.FormatBytes(textureBytes)));
			gfxStats.Append (String.Format("  Render Textures: {0} - {1} \tswitches: {2}\n", UnityStats.renderTextureCount, EditorUtility.FormatBytes(renderTextureBytes), UnityStats.renderTextureChanges));
			gfxStats.Append (String.Format("  Screen: {0} - {1}\n", UnityStats.screenRes, EditorUtility.FormatBytes(screenBytes)));
			gfxStats.Append (String.Format("  VRAM usage: {0} to {1} (of {2})\n", EditorUtility.FormatBytes(vramUsageMin), EditorUtility.FormatBytes(vramUsageMax), EditorUtility.FormatBytes(SystemInfo.graphicsMemorySize * 1024 * 1024)));
			gfxStats.Append (String.Format("  VBO Total: {0} - {1}\n", UnityStats.vboTotal, EditorUtility.FormatBytes(vboBytes)));
			gfxStats.Append (String.Format("  Shadow Casters: {0} \n", UnityStats.shadowCasters));
			gfxStats.Append (String.Format("  Visible Skinned Meshes: {0} \t Animations: {1}", UnityStats.visibleSkinnedMeshes, UnityStats.visibleAnimations));
			GUILayout.Label (gfxStats.ToString());
			
			/// Networking stats
			if (connectionCount != 0)
			{
				GUILayout.Label ("Network:", sectionHeaderStyle);
				GUILayout.BeginHorizontal(); 
				for (int i=0;i<connectionCount;i++)
				{
					GUILayout.Label (UnityStats.GetNetworkStats(i));
				}
				GUILayout.EndHorizontal(); 
			}
			else
			{
				GUILayout.Label ("Network: (no players connected)", sectionHeaderStyle);
			}
			GUILayout.EndArea ();
		}
	}
}
