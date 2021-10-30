using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;

namespace UnityEditor
{
    internal class AssetInspector
    {
		//static GUIContent badgeDelete = EditorGUIUtility.IconContent("AS Badge Delete");
		//static GUIContent badgeMove = EditorGUIUtility.IconContent("AS Badge Move");
		//static GUIContent badgeNew = EditorGUIUtility.IconContent("AS Badge New");

		static AssetInspector s_Instance;

		// When changing these watch ContextMenuClick function
		GUIContent[] m_Menu = new GUIContent[] { EditorGUIUtility.TextContent("Show Diff"), EditorGUIUtility.TextContent("Show History"), EditorGUIUtility.TextContent("Discard") };
		GUIContent[] m_UnmodifiedMenu = new GUIContent[] { EditorGUIUtility.TextContent("Show History")};

		internal static bool IsAssetServerSetUp ()
		{
			return InternalEditorUtility.HasMaint() && ASEditorBackend.SettingsAreValid ();
		}
		
		private AssetInspector(){}

        private bool HasFlag(ChangeFlags flags, ChangeFlags flagToCheck) { return ((int)flagToCheck & (int)flags) != 0; }

		public static AssetInspector Get()
		{
			if (s_Instance == null)
			{
				s_Instance = new AssetInspector();
			}
			return s_Instance;
		}

		string AddChangesetFlag(string str, string strToAdd)
		{
			if (str != string.Empty)
			{
				str += ", ";
				str += strToAdd;
			}
			else
			{
				str = strToAdd;
			}

			return str;
		}

		string GetGUID()
		{
			if (Selection.objects.Length == 0)
				return string.Empty;

			return AssetDatabase.AssetPathToGUID(AssetDatabase.GetAssetPath(Selection.objects[0]));
		}

		void DoShowDiff(string guid)
		{
			List<string> assetsToCompare = new List<string>();
			List<CompareInfo> compareOptions = new List<CompareInfo>();

			int ver1 = AssetServer.GetWorkingItemChangeset(guid);
			assetsToCompare.Add(guid);
			compareOptions.Add(new CompareInfo(ver1, -1, 0, 1));

			//TODO: localize
			Debug.Log("Comparing asset revisions " + ver1.ToString() + " and Local");

			AssetServer.CompareFiles(assetsToCompare.ToArray(), compareOptions.ToArray());
		}

		void ContextMenuClick(object userData, string[] options, int selected)
		{
			if (((bool)userData) == true && selected == 0) // asset is modified
				selected = 1; // only show history here

			switch (selected)
			{
				case 0: //"Show Diff"
					DoShowDiff(GetGUID());
					break;
				case 1: //"Show History":
					ASEditorBackend.DoAS();
					ASEditorBackend.ASWin.ShowHistory();
					break;
				case 2: //"Discard"
					if (!ASEditorBackend.SettingsIfNeeded())
					{
						Debug.Log("Asset Server connection for current project is not set up");
					}

					if (EditorUtility.DisplayDialog("Discard changes", "Are you sure you want to discard local changes of selected asset?", "Discard", "Cancel"))
					{
						AssetServer.DoUpdateWithoutConflictResolutionOnNextTick(new string[] { GetGUID() });
					}
					break;
			}
		}

		ChangeFlags GetChangeFlags()
		{
			string guid = GetGUID();
			if (guid == string.Empty)
				return ChangeFlags.None;

			return AssetServer.GetChangeFlags(guid);
		}

		string GetModificationString(ChangeFlags flags)
		{
			string changeString = string.Empty;

			if (HasFlag(flags, ChangeFlags.Undeleted)) 
				changeString = AddChangesetFlag(changeString, "undeleted");
			if (HasFlag(flags, ChangeFlags.Modified))
				changeString = AddChangesetFlag(changeString, "modified");
			if (HasFlag(flags, ChangeFlags.Renamed))
				changeString = AddChangesetFlag(changeString, "renamed");
			if (HasFlag(flags, ChangeFlags.Moved))
				changeString = AddChangesetFlag(changeString, "moved");
			if (HasFlag(flags, ChangeFlags.Created))
				changeString = AddChangesetFlag(changeString, "created");

			return changeString;
		}

/*		void DrawBadge(ChangeFlags flags)
		{
            GUIContent iconContent = null;

            if (HasFlag(flags, ChangeFlags.Undeleted) || HasFlag(flags, ChangeFlags.Created))
            {
                iconContent = ASMainWindow.badgeNew;
            }
            else
                if (HasFlag(flags, ChangeFlags.Deleted))
                {
                    iconContent = ASMainWindow.badgeDelete;
                }
                else
                    if (HasFlag(flags, ChangeFlags.Renamed) || HasFlag(flags, ChangeFlags.Moved))
                    {
                        iconContent = ASMainWindow.badgeMove;
                    }

            if (iconContent != null)
            {
                Rect iconPos = GUILayoutUtility.GetRect(iconContent.image.width, iconContent.image.height);
                GUIStyle.none.Draw(iconPos, iconContent, false, false, false, false);
            }
		}*/
		
        public void OnAssetStatusGUI(Rect r, int id, Object target, GUIStyle style)
        {
			if (target == null)
				return;

			GUIContent content;

			ChangeFlags flags = GetChangeFlags();
			string mod = GetModificationString(flags);
			if (mod == string.Empty)
			{
				content = EditorGUIUtility.TextContent("Asset is unchanged");
			}
			else
			{
				content = new GUIContent("Locally " + mod);
			}

			if (EditorGUI.DoToggle(r, id, false, content, style))
			{
				GUIUtility.hotControl = 0;
				r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
				EditorUtility.DisplayCustomMenu(r, mod == string.Empty ? m_UnmodifiedMenu : m_Menu, -1, ContextMenuClick, mod == string.Empty);
			}
        }
    }
}