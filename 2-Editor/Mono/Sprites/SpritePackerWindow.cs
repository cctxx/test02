#if ENABLE_SPRITES

using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor.Sprites
{
	internal class PackerWindow : SpriteUtilityWindow
	{	
		struct Edge
		{
			public UInt16 v0;
			public UInt16 v1;
			public Edge(UInt16 a, UInt16 b)
			{
				v0 = a;
				v1 = b;
			}

			public override bool Equals(object obj)
			{
				Edge item = (Edge)obj;
				return (v0 == item.v0 && v1 == item.v1) || (v0 == item.v1 && v1 == item.v0);
			}

			public override int GetHashCode()
			{
			    return (v0 << 16 | v1) ^ (v1 << 16 | v0).GetHashCode();
			}
		};

		private static string[] s_AtlasNamesEmpty = new string[1] { "Sprite atlas cache is empty" };
		private string[] m_AtlasNames = s_AtlasNamesEmpty;
		private int m_SelectedAtlas = 0;

		private static string[] s_PageNamesEmpty = new string[0];
		private string[] m_PageNames = s_PageNamesEmpty;
		private int m_SelectedPage = 0;

		private Sprite m_SelectedSprite = null;


		void OnEnable()
		{
			minSize = new Vector2(400f, 256f);
			title = EditorGUIUtility.TextContent("SpritePackerWindow.WindowTitle").text;
			
			Reset();
		}

		private void Reset()
		{
			RefreshAtlasNameList();
			RefreshAtlasPageList();

			m_SelectedAtlas = 0;
			m_SelectedPage = 0;
			m_SelectedSprite = null;
		}

		private void RefreshAtlasNameList()
		{
			m_AtlasNames = Packer.atlasNames;

			// Validate
			if (m_SelectedAtlas >= m_AtlasNames.Length)
				m_SelectedAtlas = 0;
		}

		private void RefreshAtlasPageList()
		{
			if (m_AtlasNames.Length > 0)
			{
				string atlas = m_AtlasNames[m_SelectedAtlas];
				Texture2D[] textures = Packer.GetTexturesForAtlas(atlas);
				m_PageNames = new string[textures.Length];
				for (int i = 0; i < textures.Length; ++i)
					m_PageNames[i] = string.Format("Page {0}", i+1);
			}
			else
			{
				m_PageNames = s_PageNamesEmpty;
			}

			// Validate
			if (m_SelectedPage >= m_PageNames.Length)
				m_SelectedPage = 0;
		}

		private void OnAtlasNameListChanged()
		{
			if (m_AtlasNames.Length > 0)
			{
				string[] atlasNames = Packer.atlasNames;
				string curAtlasName = m_AtlasNames[m_SelectedAtlas];
				string newAtlasName = (atlasNames.Length <= m_SelectedAtlas) ? null : atlasNames[m_SelectedAtlas];
				if (curAtlasName.Equals(newAtlasName))
				{
					RefreshAtlasNameList();
					RefreshAtlasPageList();
					m_SelectedSprite = null;
					return;
				}
			}

			Reset();
		}

		private bool ValidateProGUI()
		{
			if (!Application.HasAdvancedLicense())
			{
				EditorGUILayout.BeginHorizontal();
				GUILayout.Label("Sprite packing is only supported in Unity Pro.");
				EditorGUILayout.EndHorizontal();
				return false;
			}

			return true;
		}

		private bool ValidateIsPackingEnabled()
		{
			if (EditorSettings.spritePackerMode == SpritePackerMode.Disabled)
			{
				EditorGUILayout.BeginHorizontal();
				GUILayout.Label("Sprite packing is disabled. Enable it in Project EditorSettings.");
				EditorGUILayout.EndHorizontal();
				return false;
			}

			return true;
		}

		private void DoToolbarGUI()
		{
			EditorGUILayout.BeginHorizontal();
			EditorGUI.BeginDisabledGroup(Application.isPlaying);

			if (GUILayout.Button("Pack", EditorStyles.toolbarButton))
			{
				Packer.RebuildAtlasCacheIfNeeded(EditorUserBuildSettings.activeBuildTarget, true);
				m_SelectedSprite = null;
				RefreshState();
			}
			else
			{
				EditorGUI.BeginDisabledGroup(Packer.SelectedPolicy == Packer.kDefaultPolicy);
				if (GUILayout.Button("Repack", EditorStyles.toolbarButton))
				{
					Packer.RebuildAtlasCacheIfNeeded(EditorUserBuildSettings.activeBuildTarget, true, Packer.Execution.ForceRegroup);
					m_SelectedSprite = null;
					RefreshState();
				}
				EditorGUI.EndDisabledGroup();
			}

			EditorGUI.EndDisabledGroup();

			EditorGUI.BeginDisabledGroup(m_AtlasNames.Length == 0);
			GUILayout.Space(16);
			GUILayout.Label("View atlas:");

			EditorGUI.BeginChangeCheck();
			m_SelectedAtlas = EditorGUILayout.Popup(m_SelectedAtlas, m_AtlasNames, EditorStyles.toolbarPopup);
			if (EditorGUI.EndChangeCheck())
			{
				RefreshAtlasPageList();
				m_SelectedSprite = null;
			}

			EditorGUI.BeginChangeCheck();
			m_SelectedPage = EditorGUILayout.Popup(m_SelectedPage, m_PageNames, EditorStyles.toolbarPopup, GUILayout.Width(70));
			if (EditorGUI.EndChangeCheck())
			{
				m_SelectedSprite = null;
			}
			EditorGUI.EndDisabledGroup();

			EditorGUI.BeginChangeCheck();
			string[] policies = Packer.Policies;
			int selectedPolicy = Array.IndexOf(policies, Packer.SelectedPolicy);
			selectedPolicy = EditorGUILayout.Popup(selectedPolicy, policies, EditorStyles.toolbarPopup);
			if (EditorGUI.EndChangeCheck())
			{
				Packer.SelectedPolicy = policies[selectedPolicy];
			}

			EditorGUILayout.EndHorizontal();
		}

		void OnSelectionChange()
		{
			if (Selection.activeObject == null)
				return;

			Sprite selectedSprite = Selection.activeObject as Sprite;
			if (selectedSprite == null)
				return;

			if (selectedSprite != m_SelectedSprite)
			{
				string selAtlasName;
				Texture2D selAtlasTexture;
				Packer.GetAtlasDataForSprite(selectedSprite, out selAtlasName, out selAtlasTexture);
				
				int selAtlasIndex = m_AtlasNames.ToList().FindIndex(delegate(string s) { return selAtlasName == s; });
				if (selAtlasIndex == -1)
					return;
				int selAtlasPage = Packer.GetTexturesForAtlas(selAtlasName).ToList().FindIndex(delegate(Texture2D t) { return selAtlasTexture == t; });
				if (selAtlasPage == -1)
					return;

				m_SelectedAtlas = selAtlasIndex;
				m_SelectedPage = selAtlasPage;
				RefreshAtlasPageList();

				m_SelectedSprite = selectedSprite;

				Repaint();
			}
		}

		private void RefreshState()
		{
			// Check if atlas name list changed
			string[] atlasNames = Packer.atlasNames;
			if (!atlasNames.SequenceEqual(m_AtlasNames))
			{
				if (atlasNames.Length == 0)
				{
					Reset();
					return;
				}
				else
				{
					OnAtlasNameListChanged();
				}
			}

			if (m_AtlasNames.Length == 0)
			{
				SetNewTexture(null);
				return;
			}

			// Validate selections
			if (m_SelectedAtlas >= m_AtlasNames.Length)
				m_SelectedAtlas = 0;
			string curAtlasName = m_AtlasNames[m_SelectedAtlas];

			Texture2D[] textures = Packer.GetTexturesForAtlas(curAtlasName);
			if (m_SelectedPage >= textures.Length)
				m_SelectedPage = 0;

			SetNewTexture(textures[m_SelectedPage]);
		}

		public void OnGUI()
		{
			if (!ValidateProGUI())
				return;

			if (!ValidateIsPackingEnabled())
				return;

			Matrix4x4 oldHandlesMatrix = Handles.matrix;
			InitStyles();

			RefreshState();
			
			// Top menu bar
			Rect toolbarRect = EditorGUILayout.BeginVertical();

			EditorGUILayout.BeginHorizontal(GUIContent.none, EditorStyles.toolbar);
			DoToolbarGUI();
			GUILayout.FlexibleSpace();
			bool wasEnabled = GUI.enabled;
			GUI.enabled = m_AtlasNames.Length > 0;
			DoAlphaZoomToolbarGUI();
			GUI.enabled = wasEnabled;
			EditorGUILayout.EndHorizontal();

			GUILayout.Label("Sprite packing is a developer preview feature!", s_Styles.notice);
			EditorGUILayout.EndVertical();
			
			if (m_Texture == null)
				return;

			// Texture view
			EditorGUILayout.BeginHorizontal();
			m_TextureViewRect = new Rect(0f, toolbarRect.yMax, position.width - k_ScrollbarMargin, position.height - k_ScrollbarMargin - toolbarRect.height);
			GUILayout.FlexibleSpace();
			DoTextureGUI();
			string info = string.Format("{1}x{2}, {0}", TextureUtil.GetTextureFormatString(m_Texture.format), m_Texture.width, m_Texture.height);
			EditorGUI.DropShadowLabel(new Rect(m_TextureViewRect.x, m_TextureViewRect.y + 10, m_TextureViewRect.width, 20), info);
			EditorGUILayout.EndHorizontal();
			
			Handles.matrix = oldHandlesMatrix;
		}

		private void DrawLineUtility(Vector2 from, Vector2 to)
		{
			SpriteEditorUtility.DrawLine(new Vector3(from.x * m_Texture.width + 1f / m_Zoom, from.y * m_Texture.height + 1f / m_Zoom, 0.0f), new Vector3(to.x * m_Texture.width + 1f / m_Zoom, to.y * m_Texture.height + 1f / m_Zoom, 0.0f));
		}

		private Edge[] FindUniqueEdges(UInt16[] indices)
		{
			Edge[] allEdges = new Edge[indices.Length];
			int tris = indices.Length / 3;
			for (int i = 0; i < tris; ++i)
			{
				allEdges[i * 3] = new Edge(indices[i * 3], indices[i * 3 + 1]);
				allEdges[i * 3 + 1] = new Edge(indices[i * 3 + 1], indices[i * 3 + 2]);
				allEdges[i * 3 + 2] = new Edge(indices[i * 3 + 2], indices[i * 3]);
			}

			Edge[] uniqueEdges = allEdges.GroupBy(x => x).Where(x => x.Count() == 1).Select(x => x.First()).ToArray();
			return uniqueEdges;
		}

		protected override void DrawGizmos()
		{
			if (m_SelectedSprite != null && m_Texture != null)
			{
				Vector2[] uvs = DataUtility.GetSpriteUVs(m_SelectedSprite, true);
				UInt16[] indices = DataUtility.GetSpriteIndices(m_SelectedSprite, true);
				Edge[] uniqueEdges = FindUniqueEdges(indices); // Assumes that our mesh has no duplicate vertices

				SpriteEditorUtility.BeginLines(new Color(0.3921f, 0.5843f, 0.9294f, 0.75f)); // Cornflower blue :)
				foreach (Edge e in uniqueEdges)
					DrawLineUtility(uvs[e.v0], uvs[e.v1]);
				SpriteEditorUtility.EndLines();
			}
		}


	} // class
}

#endif
