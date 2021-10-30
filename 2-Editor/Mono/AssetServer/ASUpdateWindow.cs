using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
	internal class ASUpdateWindow //: EditorWindow
	{	
		internal class Constants
		{
			public GUIStyle box = "OL Box";
			public GUIStyle entrySelected = "ServerUpdateChangesetOn";
			public GUIStyle entryNormal = "ServerUpdateChangeset";
			public GUIStyle serverUpdateLog = "ServerUpdateLog";
			public GUIStyle serverChangeCount = "ServerChangeCount";
			public GUIStyle title = "OL title";
			public GUIStyle element = "OL elem";
			public GUIStyle header = "OL header";
			public GUIStyle serverUpdateInfo = "ServerUpdateInfo";
            public GUIStyle button = "Button";
            public GUIStyle errorLabel = "ErrorLabel";
            public GUIStyle bigButton = "LargeButton";
            public GUIStyle wwText = "AS TextArea";
            public GUIStyle entryEven = "CN EntryBackEven";
            public GUIStyle entryOdd = "CN EntryBackOdd";
        }

		Constants constants = null;
		
		ASUpdateConflictResolveWindow asResolveWin = null;
		ASMainWindow parentWin;

        string[] dropDownMenuItems = new string[] { "Compare", "Compare Binary" }; // change ContextMenuClick function when changing this

		Changeset[] changesets;

        Vector2 iconSize = new Vector2(16, 16);

		string[] messageFirstLines;

		int maxNickLength;
        string selectedGUID = string.Empty;
        bool isDirSelected = false;

		ListViewState lv;
		ParentViewState pv = new ParentViewState();

		SplitterState horSplit = new SplitterState(new float[] {50, 50}, new int[] {50, 50}, null);
		SplitterState vertSplit = new SplitterState(new float[] {60, 30}, new int[] {32, 32}, null);
		
		string totalUpdates;

		bool showingConflicts = false;

		public bool ShowingConflicts { get { return showingConflicts; } }
		public bool CanContinue { get { return asResolveWin.CanContinue(); } }

        public ASUpdateWindow(ASMainWindow parentWin, Changeset[] changesets)
		{
			this.changesets = changesets;
			this.parentWin = parentWin;

			lv = new ListViewState(changesets.Length, 5);
			pv.lv = new ListViewState(0, 5);

			messageFirstLines = new string[changesets.Length];

			for (int i = 0; i < changesets.Length; i++)
				messageFirstLines[i] = changesets[i].message.Split('\n')[0]; // need to only show first lines of messages

			totalUpdates = changesets.Length.ToString() + (changesets.Length == 1 ? " Update" : " Updates");
		}

        void ContextMenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                switch (dropDownMenuItems[selected])
                {
                    case "Compare":
                        DoShowDiff(false);
                        break;
                    case "Compare Binary":
                        DoShowDiff(true);
                        break;
                }

            }
        }

        private void DoSelectionChange()
        {
            // sync selection
            if (lv.row != -1)
            {
                string guid = GetFirstSelected();
                if (guid != string.Empty)
                    selectedGUID = guid;

                if (AssetServer.IsGUIDValid(selectedGUID) != 0)
                {
                    int i = 0;
                    pv.lv.row = -1;

                    foreach (ParentViewFolder dir in pv.folders)
                    {
                        if (dir.guid == selectedGUID)
                        {
                            pv.lv.row = i;
                            return;
                        }

                        i++;

                        foreach (ParentViewFile file in dir.files)
                        {
                            if (file.guid == selectedGUID)
                            {
                                pv.lv.row = i;
                                return;
                            }

                            i++;
                        }
                    }
                }
                else
                    pv.lv.row = -1;
            }
        }

        // gets guid of first selected item
        string GetFirstSelected()
        {
            // now that's a bullshit, but I guess it's ok for one asset
            Object[] objects = Selection.GetFiltered(typeof(Object), SelectionMode.Assets);
            return objects.Length != 0 ? AssetDatabase.AssetPathToGUID(AssetDatabase.GetAssetPath(objects[0])) : string.Empty;
        }

        public void OnSelectionChange()
        {
            if (showingConflicts)
            {
                asResolveWin.OnSelectionChange(this);
            }else
            {
                DoSelectionChange();
                parentWin.Repaint();
            }
        }

		public int GetSelectedRevisionNumber()
		{
			return ((pv.lv.row > lv.totalRows - 1) || (lv.row < 0)) ? -1 : changesets[lv.row].changeset;
		}

		public void SetSelectedRevisionLine(int selIndex)
		{
			if (selIndex >= lv.totalRows)
			{
				pv.Clear();
				lv.row = -1;
			} 
			else
			{
				lv.row = selIndex;
				pv.Clear();
				pv.AddAssetItems(changesets[selIndex]);
				pv.SetLineCount();
			}

			pv.lv.scrollPos = Vector2.zero;
			pv.lv.row = -1; 
			pv.selectedFolder = -1;
			pv.selectedFile = -1;

            DoSelectionChange();
		}

		public string[] GetGUIDs()
		{
			List<string> guids = new List<string>();

			if (lv.row < 0)
				return null;

			for (int i = lv.row; i < lv.totalRows; i++)
			{
				for (int k = 0; k < changesets[i].items.Length; k++)
					if (!guids.Contains(changesets[i].items[k].guid))
                        guids.Add(changesets[i].items[k].guid);
			}

			return guids.ToArray();
		}

        public bool DoUpdate(bool afterResolvingConflicts)
        {
            AssetServer.RemoveMaintErrorsFromConsole();

            // Read settings
            if (!ASEditorBackend.SettingsIfNeeded())
                return true;

            showingConflicts = false;

            // if there are any conflicts, even if they are resolved - first time show the conflict resolution window
            // next time just try to update, if there's some remaining conflicts cpp side will call conflict resolution window again
            AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBReinitOnSuccess");
			AssetServer.DoUpdateOnNextTick(!afterResolvingConflicts, "ShowASConflictResolutionsWindow");

            return true;
        }

        public void ShowConflictResolutions(string[] conflicting)
        {
            asResolveWin = new ASUpdateConflictResolveWindow(conflicting);
            showingConflicts = true;
        }

        private bool HasFlag(ChangeFlags flags, ChangeFlags flagToCheck) { return ((int)flagToCheck & (int)flags) != 0; }

        private void DoSelect(int folderI, int fileI, int row)
        {
            pv.selectedFile = fileI;
            pv.selectedFolder = folderI;
            pv.lv.row = row;
            pv.lv.selectionChanged = true;

            if (fileI == -1)
            {
                if (folderI != -1)
                {
                    selectedGUID = pv.folders[folderI].guid;
                    isDirSelected = true;
                }
                else
                {
                    selectedGUID = string.Empty;
                    isDirSelected = false;
                }
            }
            else
            {
                selectedGUID = pv.folders[folderI].files[fileI].guid;
                isDirSelected = false;
            }
        }

		public void UpdateGUI()
		{
            ChangeFlags flags;

			SplitterGUILayout.BeginHorizontalSplit(horSplit);
			
			GUILayout.BeginVertical(constants.box);
			GUILayout.Label(totalUpdates, constants.title /* constants.serverChangeCount */ );

            Rect rect;

			// list view 1
			foreach (ListViewElement el in ListViewGUILayout.ListView(lv, GUIStyle.none))
			{
                rect = el.position;
                rect.x += 1;
                rect.y += 1;

                if (Event.current.type == EventType.Repaint)
                {
                    if (el.row % 2 == 0)
                    {
                        constants.entryEven.Draw(rect, false, false, false, false);
                    }
                    else
                        constants.entryOdd.Draw(rect, false, false, false, false);
                }

				GUILayout.BeginVertical (el.row == lv.row ? constants.entrySelected : constants.entryNormal);
				GUILayout.Label (messageFirstLines[el.row], constants.serverUpdateLog, GUILayout.MinWidth (50));
				GUILayout.BeginHorizontal ();
				GUILayout.Label (changesets[el.row].changeset.ToString() + " " + changesets[el.row].date, constants.serverUpdateInfo, GUILayout.MinWidth(100));
				GUILayout.Label (changesets[el.row].owner, constants.serverUpdateInfo, GUILayout.Width(maxNickLength));
				GUILayout.EndHorizontal ();
				GUILayout.EndVertical ();
			}

            if (lv.selectionChanged)
            {
                SetSelectedRevisionLine(lv.row);
            }
			
			GUILayout.EndVertical();
			
			SplitterGUILayout.BeginVerticalSplit(vertSplit);
			
			GUILayout.BeginVertical(constants.box);
			GUILayout.Label("Changeset", constants.title);
			
			// --- files and folders
			ParentViewFolder folder;

			int folderI = -1, fileI = -1;

			foreach (ListViewElement el in ListViewGUILayout.ListView(pv.lv, GUIStyle.none))
			{
				if (folderI == -1)
					if (!pv.IndexToFolderAndFile(el.row, ref folderI, ref fileI))
						return;

                folder = pv.folders[folderI];

				if (ListViewGUILayout.HasMouseDown(el.position))
                {
                    if (Event.current.clickCount == 2)
                    {
                        if (!isDirSelected && selectedGUID != string.Empty)
                        {
                            DoShowDiff(false);
                            GUIUtility.ExitGUI();
                        }
                    }
                    else
                    {
						pv.lv.scrollPos = ListViewShared.ListViewScrollToRow(pv.lv.ilvState, el.row); // this is about clicking on a row that is partially visible
                        DoSelect(folderI, fileI, el.row);
                    }
                }else
				if (ListViewGUILayout.HasMouseDown(el.position, 1))
                {
                    if (lv.row != el.row)
                        DoSelect(folderI, fileI, el.row);

                    if (!isDirSelected && selectedGUID != string.Empty) // because there's nothing but compare actions in context menu
                    {
                        GUIUtility.hotControl = 0;
                        Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
                        EditorUtility.DisplayCustomMenu(r, dropDownMenuItems, null, ContextMenuClick, null);
                        Event.current.Use();
                    }
                }

                if (el.row == pv.lv.row && Event.current.type == EventType.Repaint)
                {
                    constants.entrySelected.Draw(el.position, false, false, false, false);
                }

                if (fileI != -1) // not a folder line
                {
                    Texture2D icon = AssetDatabase.GetCachedIcon(folder.name + "/" + folder.files[fileI].name) as Texture2D;

                    if (icon == null)
                        icon = InternalEditorUtility.GetIconForFile(folder.files[fileI].name);

                    GUILayout.Label(new GUIContent(folder.files[fileI].name, icon), constants.element);

                    flags = (ChangeFlags)folder.files[fileI].changeFlags;
                }
                else
                {
                    GUILayout.Label(folder.name, constants.header);

                    flags = (ChangeFlags)folder.changeFlags;
                }

                // ---
                // Draw the modification type badge
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

                if (iconContent != null && Event.current.type == EventType.Repaint)
                {
                    Rect iconPos = new Rect(
                        el.position.x + el.position.width - iconContent.image.width - 5,
                        el.position.y + el.position.height / 2 - iconContent.image.height / 2,
                        iconContent.image.width,
                        iconContent.image.height);

                    EditorGUIUtility.SetIconSize(Vector2.zero);
                    GUIStyle.none.Draw(iconPos, iconContent, false, false, false, false);
                    EditorGUIUtility.SetIconSize(iconSize);
                }
                //---

				pv.NextFileFolder(ref folderI, ref fileI);
			}

            if (pv.lv.selectionChanged && selectedGUID != string.Empty)
            {
                if (selectedGUID != AssetServer.GetRootGUID())
                    AssetServer.SetSelectionFromGUID(selectedGUID);
                else
                    AssetServer.SetSelectionFromGUID(string.Empty);
            }


            if (GUIUtility.keyboardControl == pv.lv.ID)
            {
                if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Return && !isDirSelected && selectedGUID != string.Empty)
                {
                    DoShowDiff(false);
                    GUIUtility.ExitGUI();
                }
            }
			
			GUILayout.EndVertical();
			
			GUILayout.BeginVertical(constants.box);
			GUILayout.Label("Update Message", constants.title);
			GUILayout.TextArea(lv.row >= 0 ? changesets[lv.row].message : "", constants.wwText);
			GUILayout.EndVertical();
			
			SplitterGUILayout.EndVerticalSplit();
			
			SplitterGUILayout.EndHorizontalSplit();
		}

        bool DoShowDiff(bool binary)
        {
            List<string> assetsToCompare = new List<string>();
            List<CompareInfo> compareOptions = new List<CompareInfo>();

            int ver1 = -1, ver2 = -1;

            if (AssetServer.IsItemDeleted(selectedGUID))
            {
                ver1 = -2;
            }
            else
            {
                ver1 = AssetServer.GetWorkingItemChangeset(selectedGUID);
                ver1 = AssetServer.GetServerItemChangeset(selectedGUID, ver1);
            }

            int tmpVer2 = AssetServer.GetServerItemChangeset(selectedGUID, -1);

            // This means file was created.
            ver2 = tmpVer2 == -1 ? -2 : tmpVer2;

            assetsToCompare.Add(selectedGUID);
            compareOptions.Add(new CompareInfo(ver1, ver2, binary ? 1 : 0, binary ? 0 : 1));

            if (assetsToCompare.Count != 0)
                AssetServer.CompareFiles(assetsToCompare.ToArray(), compareOptions.ToArray());
            else
                return false;

            return true;
        }

        public void Repaint()
        {
            parentWin.Repaint();
        }

		public bool DoGUI ()
		{
			bool wasGUIEnabled = GUI.enabled;
			if (constants == null)
			{
				constants = new Constants();

				// Find the longest username string so that we can use it with layouted LV
				maxNickLength = 1;

				for (int i = 0; i < changesets.Length; i++)
				{
					int thisLen = (int)constants.serverUpdateInfo.CalcSize(new GUIContent(changesets[i].owner)).x;
					if (thisLen > maxNickLength)
						maxNickLength = thisLen;
				}
			}

			EditorGUIUtility.SetIconSize (iconSize);

            if (showingConflicts)
            {
                if (!asResolveWin.DoGUI(this))
                    showingConflicts = false;
            } else
                UpdateGUI();
			
			EditorGUIUtility.SetIconSize (Vector2.zero);

			if (!showingConflicts)
			{
				GUILayout.BeginHorizontal();

                GUI.enabled = !isDirSelected && selectedGUID != string.Empty && wasGUIEnabled;

                if (GUILayout.Button("Compare", constants.button))
                {
                    DoShowDiff(false);
                    GUIUtility.ExitGUI();
                }

                GUI.enabled = wasGUIEnabled;

				GUILayout.FlexibleSpace();

				if (changesets.Length == 0)
					GUI.enabled = false;

				if (GUILayout.Button("Update", constants.bigButton, GUILayout.MinWidth(100)))
				{
                    if (changesets.Length == 0)
                        Debug.Log("Nothing to update.");
                    else
                    {
                        DoUpdate(false);
                    }

					parentWin.Repaint();
					GUIUtility.ExitGUI();
				}

				if (changesets.Length == 0)
					GUI.enabled = wasGUIEnabled;

				GUILayout.EndHorizontal();

                if (AssetServer.GetAssetServerError() != string.Empty)
                {
                    GUILayout.Space(10);
                    GUILayout.Label(AssetServer.GetAssetServerError(), constants.errorLabel);
                    GUILayout.Space(10);
                }
			}

			GUILayout.Space(10);

			return true;
		}
	}
}
