using UnityEngine;
using UnityEditor;

using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace UnityEditor
{
	internal class PackageExport : EditorWindow
	{
		[SerializeField]
		private AssetsItem[] m_assets;
		[SerializeField]
		private bool m_bIncludeDependencies = true;
		[SerializeField]
		private int m_LeastIndent = 999999;
		[SerializeField]
		private ListViewState m_ListView;

		//TODO: move this out of here
		internal class Constants
		{
			public GUIStyle ConsoleEntryBackEven = "CN EntryBackEven";
			public GUIStyle ConsoleEntryBackOdd = "CN EntryBackOdd";
			public GUIStyle title = "OL Title";

			public Color lineColor;

			public Constants()
			{
				lineColor = EditorGUIUtility.isProSkin ? new Color(0.1f, 0.1f, 0.1f) : new Color(0.4f, 0.4f, 0.4f);
			}
		}
		static Constants ms_Constants;

		public PackageExport()
		{
			m_ListView = new ListViewState(0, 18);
			position = new Rect(100, 100, 400, 300);
			minSize = new Vector2(400, 200);
		}

		public void OnGUI()
		{
			if (ms_Constants == null)
				ms_Constants = new Constants();

			if (m_assets == null)
				return;

			if (m_LeastIndent == 999999)
			{
				int least = m_LeastIndent;

				for (int i = 0; i < m_assets.Length; i++)
				{
					int level = CountOccurencesOfChar(m_assets[i].pathName, '/');
					if (least > level)
						least = level;
				}

				m_LeastIndent = least - 1;
			}

			if (m_assets != null)
			{

				SetupListView();

				bool repainting = (Event.current.type == EventType.Repaint);

				GUILayout.BeginVertical();
				GUILayout.Label("Items to Export", ms_Constants.title);
				GUILayout.Space(1);
				EditorGUIUtility.SetIconSize(new Vector2(16, 16));
				foreach (ListViewElement el in ListViewGUI.ListView(m_ListView, GUIStyle.none))
				{
					AssetsItem ai = m_assets[el.row];
					Rect pos = el.position;
					pos = new Rect(pos.x + 1, pos.y, pos.width - 2, pos.height);

					// indent according to level
					int level = CountOccurencesOfChar(ai.pathName, '/') - m_LeastIndent;

					// background
					if (repainting && m_ListView.row == el.row)
                        ms_Constants.ConsoleEntryBackEven.Draw(pos, false, false, true, false);

					float yPos = el.position.y;
					pos.x += 3;

					// enabled
					int check = ai.enabled;
					ai.enabled = GUI.Toggle(new Rect(pos.x, pos.y, 16, 16), ai.enabled != 0, "") ? 1 : 0;
					if (check != ai.enabled)
					{
						m_ListView.row = el.row;
						GUIUtility.keyboardControl = m_ListView.ID;
						CheckChildren(ai);
					}

					// icon
					if (repainting)
					{
						Rect IconPos = new Rect(pos.x + (15 * level), yPos + 1, 16, 16);
						Texture cachedIcon = AssetDatabase.GetCachedIcon(ai.pathName);
						if (cachedIcon != null)
						{
							GUI.DrawTexture(IconPos, cachedIcon);
						}
					}

					// Friendly name	
					pos = new Rect(pos.x + 20 + (15 * level), el.position.y, pos.width - (20 + (15 * level)), pos.height);
					GUI.Label(pos, ai.pathName);
				}
				FrameLastGUIRect();
				GUILayout.EndVertical();

				if (m_ListView.row != -1 && GUIUtility.keyboardControl == m_ListView.ID &&
					Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Space)
				{
					m_assets[m_ListView.row].enabled = m_assets[m_ListView.row].enabled == 0 ? 1 : 0;
					CheckChildren(m_assets[m_ListView.row]);
					Event.current.Use();
				}

				EditorGUIUtility.SetIconSize(Vector2.zero);

				GUILayout.Space(5);
				GUILayout.BeginHorizontal();
				GUILayout.Space(10);

				if (GUILayout.Button(EditorGUIUtility.TextContent("All"), GUILayout.Width(50)))
				{
					for (int i = 0; i < m_assets.Length; i++)
					{
						m_assets[i].enabled = 1;
					}
				}

				if (GUILayout.Button(EditorGUIUtility.TextContent("None"), GUILayout.Width(50)))
				{
					for (int i = 0; i < m_assets.Length; i++)
					{
						m_assets[i].enabled = 0;
					}
				}

				GUILayout.Space(10);

				bool bNew = GUILayout.Toggle(m_bIncludeDependencies, "Include dependencies");
				if (bNew != m_bIncludeDependencies)
				{
					m_bIncludeDependencies = bNew;
					// rebuild list
					BuildAssetList();
				}

				GUILayout.FlexibleSpace();
				if (GUILayout.Button(EditorGUIUtility.TextContent("Export...")))
				{
					Export();
					GUIUtility.ExitGUI();
				}

				GUILayout.Space(10);
				GUILayout.EndHorizontal();
				GUILayout.Space(10);
			}

		}
		
		private void BuildAssetList()
		{
			m_assets = GetAssetItemsForExport (Selection.assetGUIDs, m_bIncludeDependencies).ToArray ();
		}
		
		private void SetupListView()
		{
			if (m_assets != null)
			{
				m_ListView.totalRows = m_assets.Length;
			}
		}

		private void Export()
		{
			string fileName = EditorUtility.SaveFilePanel("Export package ...", "", "", "unitypackage");
			if (fileName != "")
			{
				// build guid list
				List<string> guids = new List<string>();

				foreach (AssetsItem ai in m_assets)
				{
					if (ai.enabled != 0)
						guids.Add(ai.guid);
				}

				AssetServer.ExportPackage(guids.ToArray(), fileName);

				Close();
				GUIUtility.ExitGUI();
			}
		}

		// Entry (called from menu)
		static void ShowExportPackage()
		{
			PackageExport window = EditorWindow.GetWindow<PackageExport>(true, "Exporting package");
			window.BuildAssetList();
			window.Repaint();
		}


		private void CheckChildren(AssetsItem parentAI)
		{
			foreach (AssetsItem ai in m_assets)
			{
				if (ai.parentGuid == parentAI.guid)
				{
					ai.enabled = parentAI.enabled;
					CheckChildren(ai);
				}
			}
		}

		void FrameLastGUIRect()
		{
			var rect = GUILayoutUtility.GetLastRect();
			HandleUtility.handleWireMaterial.SetPass(0);
			GL.Begin(GL.LINES);
			GL.Color(ms_Constants.lineColor);
			GL.Vertex3(rect.xMax + 1, rect.y, 0);
			GL.Vertex3(rect.xMax + 1, rect.yMax, 0);
			GL.Vertex3(rect.xMax + 1, rect.yMax, 0);
			GL.Vertex3(rect.x + 1, rect.yMax, 0);
			GL.Vertex3(rect.x + 1, rect.yMax, 0);
			GL.Vertex3(rect.x + 1, rect.y, 0);
			GL.End();
		}

		private static int CountOccurencesOfChar(string instance, char c)
		{
			int result = 0;
			foreach (char curChar in instance)
			{
				if (c == curChar)
				{
					++result;
				}
			}
			return result;
		}
		
		internal static IEnumerable<AssetsItem> GetAssetItemsForExport (ICollection<string> guids, bool includeDependencies)
		{
			// if nothing is selected, export all
			if (0 == guids.Count)
			{
				string[] temp = new string[0]; // <--- I dont get this API
				guids = new HashSet<string> (AssetServer.CollectAllChildren(AssetServer.GetRootGUID(), temp));
			}
			
			AssetsItem[] assets = AssetServer.BuildExportPackageAssetListAssetsItems(guids.ToArray (), includeDependencies);
			
			// If any scripts are included, add all scripts with dependencies
			if (includeDependencies && assets.Any (asset => 
				UnityEditorInternal.InternalEditorUtility.IsScriptOrAssembly (asset.pathName))
			)
				assets = AssetServer.BuildExportPackageAssetListAssetsItems(guids.Union (
					UnityEditorInternal.InternalEditorUtility.GetAllScriptGUIDs ()).ToArray (), includeDependencies);
			
			return assets;
		}
	}
}
