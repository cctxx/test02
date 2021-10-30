using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
	internal class ASUpdateConflictResolveWindow
	{
        private class Constants
        {
            public GUIStyle ButtonLeft = "ButtonLeft";
            public GUIStyle ButtonMiddle = "ButtonMid";
            public GUIStyle ButtonRight = "ButtonRight";
            public GUIStyle EntrySelected = "ServerUpdateChangesetOn";
            public GUIStyle EntryNormal = "ServerUpdateInfo";
            public GUIStyle lvHeader = "OL title";
            public GUIStyle selected = "ServerUpdateChangesetOn";
            public GUIStyle background = "OL Box";
            public GUIStyle button = "Button";
            public GUIStyle bigButton = "LargeButton";
        }

		ListViewState lv1 = new ListViewState();
		ListViewState lv2 = new ListViewState();

		bool[] selectedLV1Items, selectedLV2Items;
        bool[] deletionConflict;
		int initialSelectedLV1Item = -1, initialSelectedLV2Item = -1;
        bool lv1HasSelection = false;
        bool lv2HasSelection = false;

		SplitterState lvHeaderSplit1 = new SplitterState(new float[] {20, 80}, new int[] {100, 100}, null);
		SplitterState lvHeaderSplit2 = new SplitterState(new float[] {20, 80}, new int[] {100, 100}, null);
		
		static string[] conflictButtonTexts = {"Skip Asset", "Discard My Changes", "Ignore Server Changes", "Merge", "Unresolved"};
		static string[] nameConflictButtonTexts = {"Rename Local Asset", "Rename Server Asset"};

        string[] dropDownMenuItems = new string[] { "Compare", "Compare Binary" }; // change ContextMenuClick function when changing this

		string[] downloadConflicts = new string[0];
		string[] nameConflicts = new string[0];

		string[] dConflictPaths = new string[0];
		string[] dNamingPaths = new string[0];
		
		DownloadResolution[] downloadResolutions = new DownloadResolution[0];
		NameConflictResolution[] namingResolutions = new NameConflictResolution[0];
		
		int downloadConflictsToResolve = 0;
		
		bool showDownloadConflicts;
		bool showNamingConflicts;

        bool mySelection = false;
		
		bool enableContinueButton = false;
		bool enableMergeButton = true;
        bool splittersOk = false;

        Vector2 iconSize = new Vector2(16, 16);

		public string[] GetDownloadConflicts() { return downloadConflicts; }
		public string[] GetNameConflicts() { return nameConflicts; }

		Constants constants = null;
			
		string[] downloadResolutionString = new string[]
		{
			"Unresolved",
			"Skip Asset",
			"Discard My Changes",
			"Ignore Server Changes",
			"Merge"
		};
		
		string[] namingResolutionString = new string[]
		{
			"Unresolved",
			"Rename Local Asset",
			"Rename Server Asset"
		};


		public bool CanContinue()
		{
			return enableContinueButton;
		}

        public ASUpdateConflictResolveWindow(string[] conflicting)
		{
			downloadConflictsToResolve = 0;
			
			ArrayList newDownloadConflicts = new ArrayList();
			ArrayList newDownloadResolutions = new ArrayList();
			ArrayList newNamingResolutions = new ArrayList();
			ArrayList newNameConflicts = new ArrayList();
			
			for (int i = 0; i < conflicting.Length; i++)
			{
				AssetStatus status = AssetServer.GetStatusGUID(conflicting[i]);
			
				if (status == AssetStatus.Conflict)
				{
					newDownloadConflicts.Add(conflicting[i]);
					
					DownloadResolution res = AssetServer.GetDownloadResolution(conflicting[i]);
					newDownloadResolutions.Add(res);
					
					if (res == DownloadResolution.Unresolved) 
						downloadConflictsToResolve++;
				}
				
				if (AssetServer.GetPathNameConflict(conflicting[i]) != null && status != AssetStatus.ServerOnly)
				{
					newNameConflicts.Add(conflicting[i]);
					
					NameConflictResolution res = AssetServer.GetNameConflictResolution(conflicting[i]);
					newNamingResolutions.Add(res);
					
					if (res == NameConflictResolution.Unresolved) 
						downloadConflictsToResolve++;
				}
			}
			
			downloadConflicts = newDownloadConflicts.ToArray(typeof(string)) as string[];
			downloadResolutions = newDownloadResolutions.ToArray(typeof(DownloadResolution)) as DownloadResolution[];
			namingResolutions = newNamingResolutions.ToArray(typeof(NameConflictResolution)) as NameConflictResolution[];
			nameConflicts = newNameConflicts.ToArray(typeof(string)) as string[];
			
			enableContinueButton = downloadConflictsToResolve == 0;
				
			dConflictPaths = new string[downloadConflicts.Length];
            deletionConflict = new bool[downloadConflicts.Length];

            for (int i = 0; i < downloadConflicts.Length; i++)
            {
                if (AssetServer.HasDeletionConflict(downloadConflicts[i]))
                {
                    dConflictPaths[i] = ParentViewFolder.MakeNiceName(AssetServer.GetDeletedItemPathAndName(downloadConflicts[i]));
                    deletionConflict[i] = true;
                }
                else
                {
                    dConflictPaths[i] = ParentViewFolder.MakeNiceName(AssetServer.GetAssetPathName(downloadConflicts[i]));
                    deletionConflict[i] = false;
                }
            }

			dNamingPaths = new string[nameConflicts.Length];
			
			for (int i = 0; i < nameConflicts.Length; i++)
				dNamingPaths[i] = ParentViewFolder.MakeNiceName(AssetServer.GetAssetPathName(nameConflicts[i]));

            //dNamingPaths = new string[10];
            //nameConflicts = new string[10];
            //namingResolutions = new NameConflictResolution[10];
	
			showDownloadConflicts = downloadConflicts.Length > 0;
			showNamingConflicts = nameConflicts.Length > 0;
			
			lv1.totalRows = downloadConflicts.Length;
			lv2.totalRows = nameConflicts.Length;
			
			selectedLV1Items = new bool[downloadConflicts.Length];
			selectedLV2Items = new bool[nameConflicts.Length];

            DoSelectionChange();
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

		void ResolveSelectedDownloadConflicts(DownloadResolution res)
		{
            int selectedItem = -1; // check if theres only one item selected (and which one)
			bool hadUnmergable = false;

			for (int i = 0; i < downloadConflicts.Length; i++) 
			{
				if (selectedLV1Items[i])
				{
					string currentGUID = downloadConflicts[i];

					if (res == DownloadResolution.Merge && (AssetServer.AssetIsBinaryByGUID(currentGUID) || AssetServer.IsItemDeleted(currentGUID)))
					{
						hadUnmergable = true;
						continue; // Make sure we don't try to merge binary (or deleted) assets
					}

                    if (res != DownloadResolution.Unresolved)
                    {
                        if (AssetServer.GetDownloadResolution(currentGUID) == DownloadResolution.Unresolved)
                            downloadConflictsToResolve--;
                    }else
                        downloadConflictsToResolve++;
				
					downloadResolutions[i] = res;
					AssetServer.SetDownloadResolution(currentGUID, res);

                    selectedItem = selectedItem == -1 ? i : -2;
                }
			}

			enableContinueButton = downloadConflictsToResolve == 0;

            if (selectedItem >= 0)
            {
                selectedLV1Items[selectedItem] = false;

                if (selectedItem < selectedLV1Items.Length - 1)
                {
                    selectedLV1Items[selectedItem + 1] = true;
                }
            }

			enableMergeButton = AtLeastOneSelectedAssetCanBeMerged();

			if (hadUnmergable)
				EditorUtility.DisplayDialog("Some conflicting changes cannot be merged", 
					"Notice that not all selected changes where selected for merging. " +
					"This happened because not all of them can be merged (e.g. assets are binary or deleted).", "OK");
		}
		
		void ResolveSelectedNamingConflicts(NameConflictResolution res)
		{
			if (res != NameConflictResolution.Unresolved)
			{
				for (int i = 0; i < nameConflicts.Length; i++)
				{
					if (selectedLV2Items[i])
					{
						string currentGUID = nameConflicts[i];
					
						if(AssetServer.GetNameConflictResolution(currentGUID) == NameConflictResolution.Unresolved)
							downloadConflictsToResolve--;
				
						namingResolutions[i] = res;
						AssetServer.SetNameConflictResolution(currentGUID, res);
					}
				}

				enableContinueButton = downloadConflictsToResolve == 0;
			}
		}

        bool DoShowDiff(bool binary)
        {
            List<string> assetsToCompare = new List<string>();
            List<CompareInfo> compareOptions = new List<CompareInfo>();

            for (int i = 0; i < selectedLV1Items.Length; i++)
            {
                if (selectedLV1Items[i])
                {
                    int ver1 = AssetServer.GetServerItemChangeset(downloadConflicts[i], -1); // version to be updated
                    int ver2 = AssetServer.HasDeletionConflict(downloadConflicts[i]) ? -2 : - 1; // local version

                    assetsToCompare.Add(downloadConflicts[i]);
                    compareOptions.Add(new CompareInfo(ver1, ver2, binary ? 1 : 0, binary ? 0 : 1));
                }
            }

            if (assetsToCompare.Count != 0)
                AssetServer.CompareFiles(assetsToCompare.ToArray(), compareOptions.ToArray());
            else
                return false;

            return true;
        }

        string[] GetSelectedGUIDs()
        {
            List<string> guids = new List<string>();

            for (int i = 0; i < downloadConflicts.Length; i++)
            {
                if (selectedLV1Items[i])
                    guids.Add(downloadConflicts[i]);
            }

            return guids.ToArray();
        }

        string[] GetSelectedNamingGUIDs()
        {
            List<string> guids = new List<string>();

            for (int i = 0; i < nameConflicts.Length; i++)
            {
                if (selectedLV2Items[i])
                    guids.Add(nameConflicts[i]);
            }

            return guids.ToArray();
        }

		bool HasTrue(ref bool[] array)
		{
			for (int i = 0; i < array.Length; i++)
			{
				if (array[i])
					return true;
			}

			return false;
		}

        private void DoSelectionChange()
        {
            HierarchyProperty prop = new HierarchyProperty(HierarchyType.Assets);
            List<string> guids = new List<string>(Selection.objects.Length);

            foreach (Object obj in Selection.objects)
            {
                if (prop.Find(obj.GetInstanceID(), null))
                {
                    guids.Add(prop.guid);
                }
            }

            for (int i = 0; i < downloadConflicts.Length; i++)
            {
                selectedLV1Items[i] = guids.Contains(downloadConflicts[i]);
            }

            for (int i = 0; i < nameConflicts.Length; i++)
            {
                selectedLV2Items[i] = guids.Contains(nameConflicts[i]);
            }

            lv1HasSelection = HasTrue(ref selectedLV1Items);
            lv2HasSelection = HasTrue(ref selectedLV2Items);

			enableMergeButton = AtLeastOneSelectedAssetCanBeMerged();
        }

        public void OnSelectionChange(ASUpdateWindow parentWin)
        {
            if (!mySelection)
            {
                DoSelectionChange();
                parentWin.Repaint();
            }
            else
                mySelection = false;
        }

		bool AtLeastOneSelectedAssetCanBeMerged()
		{
			for (int i = 0; i < downloadConflicts.Length; i++)
			{
				if (selectedLV1Items[i])
				{
					if (!AssetServer.AssetIsBinaryByGUID(downloadConflicts[i]) && !AssetServer.IsItemDeleted(downloadConflicts[i]))
						return true;
				}
			}

			return false;
		}

        void DoDownloadConflictsGUI()
        {
			bool wasGUIEnabled = GUI.enabled;
            bool shiftIsDown = Event.current.shift;
            bool ctrlIsDown = EditorGUI.actionKey;
            int prevSelectedRow;

            GUILayout.BeginVertical();

            GUILayout.Label("The following assets have been changed both on the server and in the local project.\nPlease select a conflict resolution for each before continuing the update.");

            GUILayout.Space(10);

            GUILayout.BeginHorizontal();
            GUILayout.FlexibleSpace();

            GUI.enabled = lv1HasSelection && wasGUIEnabled;
            if (GUILayout.Button(conflictButtonTexts[0], constants.ButtonLeft))
                ResolveSelectedDownloadConflicts(DownloadResolution.SkipAsset);

            if (GUILayout.Button(conflictButtonTexts[1], constants.ButtonMiddle))
                ResolveSelectedDownloadConflicts(DownloadResolution.TrashMyChanges);

            if (GUILayout.Button(conflictButtonTexts[2], constants.ButtonMiddle))
                ResolveSelectedDownloadConflicts(DownloadResolution.TrashServerChanges);

			if (!enableMergeButton)
				GUI.enabled = false;

            if (GUILayout.Button(conflictButtonTexts[3], constants.ButtonRight))
                ResolveSelectedDownloadConflicts(DownloadResolution.Merge);

            //if (GUILayout.Button(conflictButtonTexts[4], constants.ButtonRight))
            //    ResolveSelectedDownloadConflicts(DownloadResolution.Unresolved);
            GUI.enabled = wasGUIEnabled;

            GUILayout.EndHorizontal();

            GUILayout.Space(5);

            SplitterGUILayout.BeginHorizontalSplit(lvHeaderSplit1);
            GUILayout.Box("Action", constants.lvHeader);
            GUILayout.Box("Asset", constants.lvHeader);
            SplitterGUILayout.EndHorizontalSplit();

            prevSelectedRow = lv1.row;
            bool selChanged = false;

			foreach (ListViewElement el in ListViewGUILayout.ListView(lv1, constants.background))
            {
                if (GUIUtility.keyboardControl == lv1.ID && Event.current.type == EventType.KeyDown && ctrlIsDown)
                    Event.current.Use();

                if (selectedLV1Items[el.row] && Event.current.type == EventType.Repaint)
                    constants.selected.Draw(el.position, false, false, false, false);

				if (ListViewGUILayout.HasMouseUp(el.position))
                {
                    if (!shiftIsDown && !ctrlIsDown)
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, lv1.row, ref initialSelectedLV1Item, ref selectedLV1Items);
                }
				else if (ListViewGUILayout.HasMouseDown(el.position))
                {
                    if (Event.current.clickCount == 2 && !AssetServer.AssetIsDir(downloadConflicts[el.row]))
                    {
                        DoShowDiff(false);
                        GUIUtility.ExitGUI();
                    }
                    else
                    {
                        if (!selectedLV1Items[el.row] || shiftIsDown || ctrlIsDown)
							selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref initialSelectedLV1Item, ref selectedLV1Items);

                        lv1.row = el.row;
                    }
                }
				else if (ListViewGUILayout.HasMouseDown(el.position, 1))
                {
                    if (!selectedLV1Items[el.row])
                    {
                        selChanged = true;

                        for (int i = 0; i < selectedLV1Items.Length; i++)
                            selectedLV1Items[i] = false;

                        lv1.selectionChanged = true;
                        selectedLV1Items[el.row] = true;
                        lv1.row = el.row;
                    }

                    GUIUtility.hotControl = 0;
                    Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
                    EditorUtility.DisplayCustomMenu(r, dropDownMenuItems, null, ContextMenuClick, null);
                    Event.current.Use();
                }


                GUILayout.Label(downloadResolutionString[(int)downloadResolutions[el.row]], GUILayout.Width(lvHeaderSplit1.realSizes[0]), GUILayout.Height(18));

                if (deletionConflict[el.row] && Event.current.type == EventType.Repaint)
                {
                    GUIContent icon = ASMainWindow.badgeDelete;

                    Rect iconPos = new Rect(
                        el.position.x + lvHeaderSplit1.realSizes[0] - icon.image.width - 5,
                        el.position.y + el.position.height / 2 - icon.image.height / 2,
                        icon.image.width,
                        icon.image.height);

                    EditorGUIUtility.SetIconSize(Vector2.zero);
                    GUIStyle.none.Draw(iconPos, icon, false, false, false, false);
                    EditorGUIUtility.SetIconSize(iconSize);
                }

                GUILayout.Label(new GUIContent(dConflictPaths[el.row],
                    AssetServer.AssetIsDir(downloadConflicts[el.row]) ? EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName) :
                    InternalEditorUtility.GetIconForFile(dConflictPaths[el.row])), GUILayout.Width(lvHeaderSplit1.realSizes[1]), GUILayout.Height(18));
            }

            GUILayout.EndVertical();

            if (GUIUtility.keyboardControl == lv1.ID)
            {
                // "Select All"
                if (Event.current.type == EventType.ValidateCommand && Event.current.commandName == "SelectAll")
                {
                    Event.current.Use();
                }
                else
                    if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "SelectAll")
                    {
                        for (int i = 0; i < selectedLV1Items.Length; i++)
                            selectedLV1Items[i] = true;

                        selChanged = true;

                        Event.current.Use();
                    }

                if (lv1.selectionChanged && !ctrlIsDown)
                {
					selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, lv1.row, ref initialSelectedLV1Item, ref selectedLV1Items);
                }
                else
                if (GUIUtility.keyboardControl == lv1.ID && Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Return && !AssetServer.AssetIsDir(downloadConflicts[lv1.row]))
                {
                    DoShowDiff(false);
                    GUIUtility.ExitGUI();
                }
            }

            if (lv1.selectionChanged || selChanged)
            {
                mySelection = true;
                AssetServer.SetSelectionFromGUIDs(GetSelectedGUIDs());

                lv1HasSelection = HasTrue(ref selectedLV1Items);

				enableMergeButton = AtLeastOneSelectedAssetCanBeMerged();
            }
        }

        void DoNamingConflictsGUI()
        {
			bool wasGUIEnabdled = GUI.enabled;
            bool shiftIsDown = Event.current.shift;
            bool ctrlIsDown = EditorGUI.actionKey;
            int prevSelectedRow;

            GUILayout.BeginVertical();
            GUILayout.Space(10);

            GUILayout.Label("The following assets have the same name as an existing asset on the server.\nPlease select which one to rename before continuing the update.");

            GUILayout.Space(10);

            GUILayout.BeginHorizontal();
            GUILayout.FlexibleSpace();

            GUI.enabled = lv2HasSelection && wasGUIEnabdled;
            if (GUILayout.Button(nameConflictButtonTexts[0], constants.ButtonLeft))
                ResolveSelectedNamingConflicts(NameConflictResolution.RenameLocal);

            if (GUILayout.Button(nameConflictButtonTexts[1], constants.ButtonRight))
                ResolveSelectedNamingConflicts(NameConflictResolution.RenameRemote);
            GUI.enabled = wasGUIEnabdled;

            GUILayout.EndHorizontal();

            GUILayout.Space(5);

            SplitterGUILayout.BeginHorizontalSplit(lvHeaderSplit2);
            GUILayout.Box("Action", constants.lvHeader);
            GUILayout.Box("Asset", constants.lvHeader);
            SplitterGUILayout.EndHorizontalSplit();

            prevSelectedRow = lv2.row;
            bool selChanged = false;

			foreach (ListViewElement el in ListViewGUILayout.ListView(lv2, constants.background))
            {
                if (GUIUtility.keyboardControl == lv2.ID && Event.current.type == EventType.KeyDown && ctrlIsDown)
                    Event.current.Use();

                if (selectedLV2Items[el.row] && Event.current.type == EventType.Repaint)
                    constants.selected.Draw(el.position, false, false, false, false);

				if (ListViewGUILayout.HasMouseUp(el.position))
                {
                    if (!shiftIsDown && !ctrlIsDown)
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, lv2.row, ref initialSelectedLV2Item, ref selectedLV2Items);
                }
                else
					if (ListViewGUILayout.HasMouseDown(el.position))
                {
                    if (!selectedLV2Items[el.row] || shiftIsDown || ctrlIsDown)
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref initialSelectedLV2Item, ref selectedLV2Items);

                    lv2.row = el.row;
                }

                GUILayout.Label(namingResolutionString[(int)namingResolutions[el.row]], GUILayout.Width(lvHeaderSplit2.realSizes[0]), GUILayout.Height(18));

                GUILayout.Label(new GUIContent(dNamingPaths[el.row],
                    AssetServer.AssetIsDir(nameConflicts[el.row]) ? EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName) :
                    InternalEditorUtility.GetIconForFile(dNamingPaths[el.row])), GUILayout.Width(lvHeaderSplit2.realSizes[1]), GUILayout.Height(18));
            }

            GUILayout.EndVertical();

            if (GUIUtility.keyboardControl == lv2.ID)
            {
                // "Select All"
                if (Event.current.type == EventType.ValidateCommand && Event.current.commandName == "SelectAll")
                {
                    Event.current.Use();
                }
                else
                    if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "SelectAll")
                    {
                        for (int i = 0; i < selectedLV2Items.Length; i++)
                            selectedLV2Items[i] = true;

                        selChanged = true;

                        Event.current.Use();
                    }

                if (lv2.selectionChanged && !ctrlIsDown)
                {
					selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, lv2.row, ref initialSelectedLV2Item, ref selectedLV2Items);
                }
            }

            if (lv2.selectionChanged || selChanged)
            {
                mySelection = true;
                AssetServer.SetSelectionFromGUIDs(GetSelectedNamingGUIDs());

                lv2HasSelection = HasTrue(ref selectedLV2Items);
            }
        }

        public bool DoGUI(ASUpdateWindow parentWin) 
		{
			if (constants == null)
				constants = new Constants();

			bool wasGUIEnabled = GUI.enabled;

			EditorGUIUtility.SetIconSize (iconSize);
			
			GUILayout.BeginVertical();
			
			if (showDownloadConflicts)
			{
                DoDownloadConflictsGUI();
			}

			if (showNamingConflicts)
			{
                DoNamingConflictsGUI();
			}

			GUILayout.EndVertical();

			EditorGUIUtility.SetIconSize (Vector2.zero);


            GUILayout.BeginHorizontal();
            GUI.enabled = lv1HasSelection && wasGUIEnabled;

            if (GUILayout.Button("Compare", constants.button))
            {
                if (!DoShowDiff(false))
                    Debug.Log("No differences found");

                GUIUtility.ExitGUI();
            }

            GUI.enabled = wasGUIEnabled;

            GUILayout.FlexibleSpace();

            GUI.enabled = parentWin.CanContinue && wasGUIEnabled;
            if (GUILayout.Button("Continue", constants.bigButton, GUILayout.MinWidth(100)))
            {
                parentWin.DoUpdate(true);
                return false;
            }

            GUI.enabled = wasGUIEnabled;
            if (GUILayout.Button("Cancel", constants.bigButton, GUILayout.MinWidth(100)))
            {
                //showingConflicts = false;
                return false;
            }

            GUILayout.EndHorizontal();

            if (!splittersOk && Event.current.type == EventType.Repaint)
            {
                splittersOk = true;
                parentWin.Repaint();
            }

            return true;
		}
	}
}
