using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Specialized;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
	internal class ASCommitWindow
    {
        #region Locals
        class Constants
		{
			public GUIStyle box = "OL Box";
			public GUIStyle entrySelected = "ServerUpdateChangesetOn";
			public GUIStyle serverChangeCount = "ServerChangeCount";
			public GUIStyle title = "OL title";
			public GUIStyle element = "OL elem";
			public GUIStyle header = "OL header";
            public GUIStyle button = "Button";
			public GUIStyle serverUpdateInfo = "ServerUpdateInfo";
            public GUIStyle wwText = "AS TextArea";
            public GUIStyle errorLabel = "ErrorLabel";
            public GUIStyle dropDown = "MiniPullDown";
            public GUIStyle bigButton = "LargeButton";
        }

        const int listLenghts = 20;
        const int widthToHideButtons = 432;
		bool wasHidingButtons = false;
        bool resetKeyboardControl = false;

		static Constants constants = null;

		ParentViewState pv1state = new ParentViewState(), 
						pv2state = new ParentViewState();

		bool pv1hasSelection;
		bool pv2hasSelection;
		bool somethingDiscardableSelected = false;

        bool mySelection = false;

        string[] commitMessageList;
        string[] dropDownMenuItems = new string[] { "", "", "Compare", "Compare Binary", "", "Discard" }; // change ContextMenuClick function when changing this

        string[] guidsToTransferToTheRightSide = null;

        string dragTitle = string.Empty;

        Vector2 iconSize = new Vector2(16, 16);

		SplitterState horSplit = new SplitterState(new float[] {50, 50}, new int[] {50, 50}, null);
		SplitterState vertSplit = new SplitterState(new float[] {60, 30}, new int[] {32, 64}, null);
		
		internal string description = "";
		string totalChanges;

        ASMainWindow parentWin;
        bool initialUpdate;

        Vector2 scrollPos = Vector2.zero;

        internal bool lastTransferMovedDependencies = false;
        internal int lastRevertSelectionChanged = -1;
        internal int showReinitedWarning = -1;

        #endregion

        #region Maintainance
        public ASCommitWindow(ASMainWindow parentWin, string[] guidsToTransfer)
		{
            guidsToTransferToTheRightSide = guidsToTransfer;
			this.parentWin = parentWin;
            initialUpdate = true;
		}

        internal void AssetItemsToParentViews()
        {
            pv1state.Clear();
            pv2state.Clear();

            pv1state.AddAssetItems(parentWin.sharedCommits);
            pv1state.AddAssetItems(parentWin.sharedDeletedItems);

            pv1state.lv = new ListViewState(0);
            pv2state.lv = new ListViewState(0);

            pv1state.SetLineCount();
            pv2state.SetLineCount();

            if (pv1state.lv.totalRows == 0) // nothing to commit...
            {
                parentWin.Reinit();
                return;
            }

            pv1state.selectedItems = new bool[pv1state.lv.totalRows];
            pv2state.selectedItems = new bool[pv1state.lv.totalRows]; // this is NOT meant to be pv2state.lv.totalLines, just creating two max length arrays of bools

            int changedFolders = 0;

            for (int i = 0; i < parentWin.sharedCommits.Length; i++)
            {
                if (parentWin.sharedCommits[i].assetIsDir != 0)
                    changedFolders++;
            }

            for (int i = 0; i < parentWin.sharedDeletedItems.Length; i++)
            {
                if (parentWin.sharedDeletedItems[i].assetIsDir != 0)
                    changedFolders++;
            }

            totalChanges = (pv1state.lv.totalRows - pv1state.GetFoldersCount() + changedFolders).ToString() + " Local Changes";

            GetPersistedData();
        }

		internal void Reinit(bool lastActionsResult)
		{
			parentWin.sharedCommits = GetCommits();
			parentWin.sharedDeletedItems = AssetServer.GetLocalDeletedItems();

			AssetServer.ClearRefreshCommit();
			AssetItemsToParentViews();
		}

        internal void Update()
        {
            SetPersistedData();
            AssetItemsToParentViews();
            GetPersistedData();
        }

        internal void CommitFinished(bool actionResult)
        {
            if (actionResult)
            {
                AssetServer.ClearCommitPersistentData();
                parentWin.Reinit();
            }
            else
                parentWin.Repaint();
        }

        internal void InitiateReinit()
        {
            if (parentWin.CommitNeedsRefresh())
            {
                if (!initialUpdate)
                {
                    SetPersistedData();
                }
                else
                    initialUpdate = false;

                Reinit(true);
            }
            else
                if (initialUpdate)
                {
                    AssetItemsToParentViews();
                    initialUpdate = false;
                }
                else
                {
                    SetPersistedData();
                    AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBReinitCommitWindow");
                    AssetServer.DoRefreshAssetsOnNextTick();
                }
        }


        private void GetPersistedData()
        {
            // Persist commit settings (message and selected files) on editor reloads
            description = AssetServer.GetLastCommitMessage();

            string[] prevGUIDs;

            if (guidsToTransferToTheRightSide != null && guidsToTransferToTheRightSide.Length != 0)
            {
                prevGUIDs = guidsToTransferToTheRightSide;
                guidsToTransferToTheRightSide = null;
            }
            else
                prevGUIDs = AssetServer.GetCommitSelectionGUIDs();

            int i = 0;

            foreach (ParentViewFolder dir in pv1state.folders)
            {
                pv1state.selectedItems[i++] = ((IList)prevGUIDs).Contains(dir.guid) && (AssetServer.IsGUIDValid(dir.guid) != 0);

                foreach (ParentViewFile file in dir.files)
                    pv1state.selectedItems[i++] = ((IList)prevGUIDs).Contains(file.guid) && (AssetServer.IsGUIDValid(file.guid) != 0);
            }

            DoTransferAll(pv1state, pv2state, pv1state.selectedFolder, pv1state.selectedFile);

            // Get commit message history
            commitMessageList = InternalEditorUtility.GetEditorSettingsList("ASCommitMsgs", listLenghts);

            for (i = 0; i < commitMessageList.Length; i++)
            {
                commitMessageList[i] = commitMessageList[i].Replace('/', '?').Replace('%', '?');
            }
        }

        private void SetPersistedData()
        {
            // Persist commit settings (message and selected files) on editor reloads
            AssetServer.SetLastCommitMessage(description);
            AddToCommitMessageHistory(description);

            List<string> selectedItems = new List<string>();

            foreach (ParentViewFolder dir in pv2state.folders)
            {
                if (AssetServer.IsGUIDValid(dir.guid) != 0)
                    selectedItems.Add(dir.guid);

                foreach (ParentViewFile file in dir.files)
                {
                    if (AssetServer.IsGUIDValid(file.guid) != 0)
                        selectedItems.Add(file.guid);
                }
            }

            AssetServer.SetCommitSelectionGUIDs(selectedItems.ToArray());
        }

        internal void OnClose()
        {
            SetPersistedData();
        }

		List<string> GetSelectedItems()
		{
			pv1hasSelection = pv1state.HasTrue();
			pv2hasSelection = pv2state.HasTrue();

			List<string> items = GetParentViewSelectedItems(pv2hasSelection ? pv2state : pv1state, true, false);
			items.Remove(AssetServer.GetRootGUID());

			return items;
		}

		// Only show the first item in the selection as you might be committing a full project 
		// with thousands of files and selecting all of them in the scene inspector will put them all into memory
		void MySelectionToGlobalSelection()
		{
			mySelection = true;
			somethingDiscardableSelected = ASCommitWindow.SomethingDiscardableSelected(pv2hasSelection ? pv2state : pv1state);
			List<string> selection = GetSelectedItems();
			if (selection.Count > 0)
				AssetServer.SetSelectionFromGUID(selection[0]);
		}

        #endregion

        #region General public functions
        internal static bool DoShowDiff(List<string> selectedAssets, bool binary)
        {
            List<string> assetsToCompare = new List<string>();
            List<CompareInfo> compareOptions = new List<CompareInfo>();

            for (int i = 0; i < selectedAssets.Count; i++)
            {
                int ver2 = -1;

                int serverVersion = AssetServer.GetWorkingItemChangeset(selectedAssets[i]);
                serverVersion = AssetServer.GetServerItemChangeset(selectedAssets[i], serverVersion);

                if (AssetServer.IsItemDeleted(selectedAssets[i]))
                {
                    ver2 = -2;
                }

                if (serverVersion == -1)  // item is not on the server, it was created
                {
                    serverVersion = -2;
                }

                assetsToCompare.Add(selectedAssets[i]);
                compareOptions.Add(new CompareInfo(serverVersion, ver2, binary ? 1 : 0, binary ? 0 : 1));
            }

            if (assetsToCompare.Count != 0)
                AssetServer.CompareFiles(assetsToCompare.ToArray(), compareOptions.ToArray());
            else
                return false;

            return true;
        }

		internal static bool IsDiscardableAsset(string guid, ChangeFlags changeFlags)
		{
			return ((AssetServer.IsConstantGUID(guid) == 0) || 
				(!HasFlag(changeFlags, ChangeFlags.Created) && !HasFlag(changeFlags, ChangeFlags.Undeleted)));
		}

        internal static List<string> GetParentViewSelectedItems(ParentViewState state, bool includeFolders, bool excludeUndiscardableOnes)
        {
            List<string> selectedGUIDs = new List<string>();
            int i = 0;

            // This should act equivalent to DoTransferAll selection
            for (int f = 0; f < state.folders.Length; f++)
            {
                ParentViewFolder currFolder = state.folders[f];
                bool noFilesSelected = true;
                bool allFilesSelected = true;

                int folderLine = i++; // folder line
                int insertLine = selectedGUIDs.Count; // no harm in maintaining order

                for (int k = 0; k < currFolder.files.Length; k++)
                {
                    if (state.selectedItems[i])
                    {
						if (!excludeUndiscardableOnes || 
							IsDiscardableAsset(currFolder.files[k].guid, currFolder.files[k].changeFlags))
						{
							selectedGUIDs.Add(currFolder.files[k].guid);
							noFilesSelected = false;
						}
                    }
                    else
                        allFilesSelected = false;

                    i++;
                }

                // do not add folder GUID if only some of its files are selected
                if (includeFolders && state.selectedItems[folderLine] && (noFilesSelected || allFilesSelected))
                {
                    if (AssetServer.IsGUIDValid(currFolder.guid) != 0 && insertLine <= selectedGUIDs.Count)
                    {
                        selectedGUIDs.Insert(insertLine, currFolder.guid);
                    }
                }
            }

            return selectedGUIDs;
        }

		static List<string> s_AssetGuids;
		static string s_Callback;
        internal static void DoRevert(List<string> assetGuids, string callback)
        {
			if (assetGuids.Count == 0)
				return;

			s_AssetGuids = assetGuids;
			s_Callback = callback;

			AssetServer.SetAfterActionFinishedCallback("ASCommitWindow", "DoRevertAfterDialog");
			AssetServer.ShowDialogOnNextTick("Discard changes", "Are you really sure you want to discard selected changes?", "Discard", "Cancel");

        }

		internal static void DoRevertAfterDialog(bool result)
		{
			if (result)
			{
				AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", s_Callback);
				AssetServer.DoUpdateWithoutConflictResolutionOnNextTick(s_AssetGuids.ToArray()); // don't reinitialize to overview window
			}
		}

        // returns if some PV item got selected
        internal static bool MarkSelected(ParentViewState activeState, List<string> guids)
        {
            int i = 0;
            bool somethingSelected = false;
            bool selectThis;

            foreach (ParentViewFolder dir in activeState.folders)
            {
                selectThis = guids.Contains(dir.guid);
                activeState.selectedItems[i++] = selectThis;
                somethingSelected |= selectThis;
                
                foreach (ParentViewFile file in dir.files)
                {
                    selectThis = guids.Contains(file.guid);
                    activeState.selectedItems[i++] = selectThis;
                    somethingSelected |= selectThis;
                }
            }

            return somethingSelected;
        }

        internal static AssetsItem[] GetCommits()
        {
            return AssetServer.BuildAllExportPackageAssetListAssetsItems();
        }

        internal void AddToCommitMessageHistory(string description)
        {
            if (description.Trim() != string.Empty)
            {
                // Save commit message history
                if (ArrayUtility.Contains(commitMessageList, description))
                    ArrayUtility.Remove(ref commitMessageList, description);

                ArrayUtility.Insert(ref commitMessageList, 0, description);

                InternalEditorUtility.SaveEditorSettingsList("ASCommitMsgs", commitMessageList, listLenghts);
            }
        }

        internal static bool ShowDiscardWarning()
        {
            return EditorUtility.DisplayDialog("Discard changes",
                    "More items will be discarded then initially selected. Dependencies of selected items where all marked in commit window. Please review.",
                    "Discard", "Cancel");
        }
        #endregion

        #region Committing
        internal bool CanCommit()
		{
			return (pv2state.folders.Length != 0);
		}

        internal string[] GetItemsToCommit()
        {
            // Build guid list from assets
            List<string> guidsToCommit = new List<string>();

            for (int i = 0; i < pv2state.folders.Length; i++)
            {
                ParentViewFolder currFolder = pv2state.folders[i];

                if (AssetServer.IsGUIDValid(currFolder.guid) != 0)
                    guidsToCommit.Add(currFolder.guid);

                for (int k = 0; k < currFolder.files.Length; k++)
                    if (AssetServer.IsGUIDValid(currFolder.files[k].guid) != 0)
                        guidsToCommit.Add(currFolder.files[k].guid);
            }

            return guidsToCommit.ToArray();
        }

        internal void DoCommit()
		{
            if (AssetServer.GetRefreshCommit())
            {
                SetPersistedData();
                InitiateReinit();
                showReinitedWarning = 2;
                parentWin.Repaint();
                GUIUtility.ExitGUI();
            }

            if (description == string.Empty)
            {
                if (!EditorUtility.DisplayDialog("Commit without description", "Are you sure you want to commit with empty commit description message?", "Commit", "Cancel"))
                {
                    return;
                }
            }

            string[] guidsToCommit = GetItemsToCommit();

            SetPersistedData();

            AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBCommitFinished");
            AssetServer.DoCommitOnNextTick(description, guidsToCommit);
    		GUIUtility.ExitGUI();
        }
        #endregion

        #region Transfering and dependencies

        // returns if transfered more assets then was initially selected (dependent assets)
        private bool TransferDependentParentFolders(ref List<string> guidsOfFoldersToRemove, string guid, bool leftToRight)
        {
            bool changed = false;

            if (leftToRight)
            {
                // If folder and it's sub folder are created (new) and user is adding only sub folder - force adding folder
                while (AssetServer.IsGUIDValid(guid = AssetServer.GetParentGUID(guid, -1)) != 0)
                {
                    if (AllFolderWouldBeMovedAnyway(leftToRight ? pv1state : pv2state, guid))
                    {
                        continue;
                    }

                    int left = IndexOfFolderWithGUID(pv1state.folders, guid);
                    int right = IndexOfFolderWithGUID(pv2state.folders, guid);

                    // even if current folder is not "new" does not mean that it's parent cannot be "new", so keep going up
                    if (left == -1 && right == -1)
                        continue;

                    if (left != -1 && right == -1)
                    {
                        ChangeFlags changeFlags = pv1state.folders[left].changeFlags;

                        if (HasFlag(changeFlags, ChangeFlags.Undeleted) ||
                            HasFlag(changeFlags, ChangeFlags.Created) ||
                            HasFlag(changeFlags, ChangeFlags.Moved))
                        {
                            ArrayUtility.Add(ref pv2state.folders, pv1state.folders[left].CloneWithoutFiles());

                            changed = true;

                            if (pv1state.folders[left].files.Length == 0) // only remove if it had files
                                AddFolderToRemove(ref guidsOfFoldersToRemove, pv1state.folders[left].guid);
                        }
                    }
                }

            }else
            {
                // if removed folder, remove dependent children
                ChangeFlags changeFlags = pv1state.folders[IndexOfFolderWithGUID(pv1state.folders, guid)].changeFlags;

                if (!HasFlag(changeFlags, ChangeFlags.Undeleted) &&
                    !HasFlag(changeFlags, ChangeFlags.Created) &&
                    !HasFlag(changeFlags, ChangeFlags.Moved))
                {
                    return false; // this one was not changed
                }

                // get dependent folders
                for (int i = 0; i < pv2state.folders.Length; i++)
                {
                    string currGUID = pv2state.folders[i].guid;

                    if (AssetServer.GetParentGUID(currGUID, -1) == guid)
                    {
                        // move each one to the left side
                        int left = IndexOfFolderWithGUID(pv1state.folders, currGUID);

                        if (left != -1)
                        {
                            // move remaining files
                            ArrayUtility.AddRange(ref pv1state.folders[left].files, pv2state.folders[i].files);
                        }else
                        {
                            // move whole folder
                            ArrayUtility.Add(ref pv1state.folders, pv2state.folders[i]);
                        }

                        // mark for removal from the right side
                        AddFolderToRemove(ref guidsOfFoldersToRemove, currGUID);

                        // do TransferDependent on all children
                        TransferDependentParentFolders(ref guidsOfFoldersToRemove, currGUID, leftToRight);

                        changed = true;
                    }
                }
            }

            return changed;
        }

        // returns if transfered more assets then was initially selected (dependent assets)
        private bool TransferDeletedDependentParentFolders(ref List<string> guidsOfFoldersToRemove, string guid, bool leftToRight)
        {
            // if moving "deleted" folder, force moving dependent children
            bool changed = false;

            ParentViewState pvFrom = leftToRight ? pv1state : pv2state;
            ParentViewState pvTo = leftToRight ? pv2state : pv1state;

            if (leftToRight)
            {
                // get dependent folders
                for (int i = 0; i < pvFrom.folders.Length; i++)
                {
                    ParentViewFolder currFolder = pvFrom.folders[i];

                    if (AssetServer.GetParentGUID(currFolder.guid, -1) == guid)
                    {
                        if (AllFolderWouldBeMovedAnyway(pvFrom, currFolder.guid))
                        {
                            continue;
                        }

                        // Every deleted folders child is supposed to also be "deleted"
                        // Error if some file/folder down in deleted hierarchy is not "deleted"
                        if (!HasFlag(currFolder.changeFlags, ChangeFlags.Deleted))
                        {
                            Debug.LogError("Folder of nested deleted folders marked as not deleted (" + currFolder.name + ")");
                            return false;
                        }

                        for (int k = 0; k < currFolder.files.Length; k++)
                        {
                            if (!HasFlag(currFolder.files[k].changeFlags, ChangeFlags.Deleted))
                            {
                                Debug.LogError("File of nested deleted folder is marked as not deleted (" + currFolder.files[k].name + ")");
                                return false;
                            }
                        }

                        // do TransferDependent on all children
                        changed |= TransferDeletedDependentParentFolders(ref guidsOfFoldersToRemove, currFolder.guid, leftToRight);

                        // move each one to the right side
                        int right = IndexOfFolderWithGUID(pvTo.folders, currFolder.guid);

                        if (right == -1)
                        {
                            // move whole folder
                            ArrayUtility.Add(ref pvTo.folders, currFolder);
                        }

                        // mark for removal from the left side
                        AddFolderToRemove(ref guidsOfFoldersToRemove, currFolder.guid);

                        changed = true;
                    }
                }
            }
            else
            {
                // Moving from right to left. If moving deleted folder, move out it's parent folders
                while (AssetServer.IsGUIDValid(guid = AssetServer.GetParentGUID(guid, -1)) != 0)
                {
                    int right = IndexOfFolderWithGUID(pv2state.folders, guid);

                    // assume that if this folder is deleted and it's parent is not there will be no deleted folders higher up the hierarchy
                    if (right == -1)
                        break;

                    if (HasFlag(pv2state.folders[right].changeFlags, ChangeFlags.Deleted))
                    {
                        ArrayUtility.Add(ref pv1state.folders, pv2state.folders[right]);

                        changed = true;

                        AddFolderToRemove(ref guidsOfFoldersToRemove, pv2state.folders[right].guid);
                    }
                }
            }

            return changed;
        }

        // returns if transfered more assets then was initially selected (dependent assets)
        private bool DoTransfer(ref ParentViewFolder[] foldersFrom, ref ParentViewFolder[] foldersTo, int folder, int file, ref List<string> guidsOfFoldersToRemove, bool leftToRight)
		{
			ParentViewFolder f = foldersFrom[folder];
			ParentViewFolder fTo = null;
            string lfname = f.name;
            bool changed = false;

			bool sortToFolders = false;
            if (file == -1)
            {
                AddFolderToRemove(ref guidsOfFoldersToRemove, foldersFrom[folder].guid);

                int foundIndex = ParentViewState.IndexOf(foldersTo, lfname);
                if (foundIndex != -1)
                {
                    // move the remaining files
                    fTo = foldersTo[foundIndex];

                    ArrayUtility.AddRange(ref fTo.files, f.files);
                }
                else
                {
                    ArrayUtility.Add(ref foldersTo, f);
                    sortToFolders = true;

                    if (!HasFlag(f.changeFlags, ChangeFlags.Deleted))
                        changed = TransferDependentParentFolders(ref guidsOfFoldersToRemove, f.guid, leftToRight);
                    else
                        changed = TransferDeletedDependentParentFolders(ref guidsOfFoldersToRemove, f.guid, leftToRight);
                }
            }
            else
            {
                int foundIndex = ParentViewState.IndexOf(foldersTo, lfname);
                if (foundIndex == -1)
                {
                    if (HasFlag(f.files[file].changeFlags, ChangeFlags.Deleted) && HasFlag(f.changeFlags, ChangeFlags.Deleted))
                    {
                        ArrayUtility.Add(ref foldersTo, f);

                        AddFolderToRemove(ref guidsOfFoldersToRemove, f.guid);

                        foundIndex = foldersTo.Length - 1;

                        if (!AllFolderWouldBeMovedAnyway(leftToRight ? pv1state : pv2state, f.guid))
                        {
                            changed = true;
                        } 

                        changed |= TransferDeletedDependentParentFolders(ref guidsOfFoldersToRemove, f.guid, leftToRight);
                    }
                    else
                    {
                        ArrayUtility.Add(ref foldersTo, f.CloneWithoutFiles());
                        foundIndex = foldersTo.Length - 1;
                        changed = TransferDependentParentFolders(ref guidsOfFoldersToRemove, f.guid, leftToRight);
                    }

                    sortToFolders = true;
                }

                fTo = foldersTo[foundIndex];

                ArrayUtility.Add(ref fTo.files, f.files[file]);
                ArrayUtility.RemoveAt(ref f.files, file);

                if (f.files.Length == 0)
                    AddFolderToRemove(ref guidsOfFoldersToRemove, foldersFrom[folder].guid);
            }

			if (fTo != null)
				System.Array.Sort(fTo.files, ParentViewState.CompareViewFile); 
			
			if (sortToFolders)
				System.Array.Sort(foldersTo, ParentViewState.CompareViewFolder);

            return changed;
		}

        // returns if transfered more assets then was initially selected (dependent assets)
        private bool MarkDependantFiles(ParentViewState pvState)
        {
            int i, f, k;
            string[] dependentGUIDs = new string[] { };
            bool changed = false;

            // Collect dependencies
            if (pvState == pv1state) // don't drag dependencies when dragging back from the right side 
            {
				dependentGUIDs = AssetServer.CollectAllDependencies(GetParentViewSelectedItems(pv1state, false, false).ToArray());

                // Just mark all dependent files as selected
                if (dependentGUIDs.Length != 0)
                {
                    for (i = 1, f = 0; i < pvState.lv.totalRows; f++, i++) // i marks index in the selectedItems array
                    {
                        for (k = 0; k < pvState.folders[f].files.Length; k++, i++)
                        {
                            if (!pvState.selectedItems[i])
                            {
                                for (int x = 0; x < dependentGUIDs.Length; x++)
                                {
                                    if (dependentGUIDs[x] == pvState.folders[f].files[k].guid)
                                    {
                                        pvState.selectedItems[i] = true;
                                        changed = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return changed;
        }

        // returns if transfered more assets then was initially selected (dependent assets)
		private void DoTransferAll(ParentViewState pvState, ParentViewState anotherPvState, int selFolder, int selFile)
		{
            List<string> guidsOfFoldersToRemove = new List<string>();
            int i, f, k;
            bool changed;

            changed = MarkDependantFiles(pvState);

            // Transfer selected
            for (i = pvState.lv.totalRows - 1, f = pvState.folders.Length - 1; f >= 0; f--)
            {
                ParentViewFolder currFolder = pvState.folders[f];

                bool hasSelectedFiles = false;
                for (k = currFolder.files.Length - 1; k >= -1; k--)
                {
                    if (!guidsOfFoldersToRemove.Contains(currFolder.guid) && // are we removing this folder anyway?
                        pvState.selectedItems[i])
                    {
                        if (!((k == -1) && hasSelectedFiles)) 	// do not move all items of a folder if one of its files is selected
                        {
                            changed |= DoTransfer(ref pvState.folders, ref anotherPvState.folders, f, k, ref guidsOfFoldersToRemove, pvState == pv1state);
                        }

                        hasSelectedFiles = true;
                    }

                    i--;
                }
            }

            for (i = pvState.folders.Length - 1; i >= 0; i--)
            {
                if (guidsOfFoldersToRemove.Contains(pvState.folders[i].guid))
                {
                    guidsOfFoldersToRemove.Remove(pvState.folders[i].guid);
                    ArrayUtility.RemoveAt(ref pvState.folders, i);
                }
            }

			pv1state.SetLineCount();
			pv2state.SetLineCount();

			pv1state.ClearSelection();
			pv2state.ClearSelection();

			pvState.selectedFile = -1;
			pvState.selectedFolder = -1;

            AssetServer.SetSelectionFromGUID(string.Empty);

            lastTransferMovedDependencies = changed;
        }

        private static bool AnyOfTheParentsIsSelected(ref ParentViewState pvState, string guid)
        {
            string parentGUID = guid;

            while (AssetServer.IsGUIDValid(parentGUID = AssetServer.GetParentGUID(parentGUID, -1)) != 0)
            {
                if (AllFolderWouldBeMovedAnyway(pvState, parentGUID))
                    return true;
            }

            return false;
        }

        public static bool MarkAllFolderDependenciesForDiscarding(ParentViewState pvState, ParentViewState anotherPvState)
        {
            bool changed = false;
            bool needsSort = false;
            int index = 0;
            List<string> guidsToMoveToTheOtherSide = new List<string>();

            for (int i = 0; i < pvState.folders.Length; i++)
            {
                ParentViewFolder currFolder = pvState.folders[i];

				if (HasFlag(currFolder.changeFlags, ChangeFlags.Deleted))
				{
					bool anyFileSelected = false;

					for (int k = 1; k <= currFolder.files.Length; k++)
					{
						if (pvState.selectedItems[index + k])
						{
							anyFileSelected = true;
							pvState.selectedItems[index] = true;
							guidsToMoveToTheOtherSide.Add(currFolder.guid);
							break;
						}
					}

					if (pvState.selectedItems[index] || anyFileSelected)
					{
						string guid = currFolder.guid;
						while (AssetServer.IsGUIDValid(guid = AssetServer.GetParentGUID(guid, -1)) != 0)
						{
							int idx = IndexOfFolderWithGUID(pvState.folders, guid);

							if (idx == -1)
								break;

							idx = FolderIndexToTotalIndex(pvState.folders, idx);

							if (!pvState.selectedItems[idx] && HasFlag(pvState.folders[idx].changeFlags, ChangeFlags.Deleted))
							{
								pvState.selectedItems[idx] = true;
								guidsToMoveToTheOtherSide.Add(guid);
								changed = true;
							}
						}
					}
				}
				else
                if (!AllFolderWouldBeMovedAnyway(pvState, currFolder.guid))
                {
                    if (AnyOfTheParentsIsSelected(ref pvState, currFolder.guid))
                    {
                        pvState.selectedItems[index] = true; // select folder

                        guidsToMoveToTheOtherSide.Add(currFolder.guid);

                        // Select all remaining items
                        for (int k = 1; k <= currFolder.files.Length; k++)
                        {
                            pvState.selectedItems[index + k] = true;
                        }

                        changed = true;
                    }
                }
                else // this folder is selected, and none or all of it's children are selected
                {
                    // Select all the files of the folder, just to not confuse the user
                    // note that we don't "change" selection here, it means the same
                    for (int k = 1; k <= currFolder.files.Length; k++)
                    {
                        if (!pvState.selectedItems[index + k])
                        {
                            pvState.selectedItems[index + k] = true;
                        }
                    }

                    guidsToMoveToTheOtherSide.Add(currFolder.guid);
                }

                index += 1 + pvState.folders[i].files.Length;
            }

            if (anotherPvState != null)
            {
                // folders on the other side
                for (int i = 0; i < anotherPvState.folders.Length; i++)
                {
                    ParentViewFolder currFolder = anotherPvState.folders[i];

                    if (AnyOfTheParentsIsSelected(ref pvState, currFolder.guid)) // no mistake, check in pvState
                    {
                        guidsToMoveToTheOtherSide.Add(currFolder.guid);
                    }
                }

                // now fun part, some of the files/folders that will be reverted are on the other side in commit window
                // move those to current side and select
                for (int i = anotherPvState.folders.Length - 1; i >= 0; i--)
                {
                    if (!guidsToMoveToTheOtherSide.Contains(anotherPvState.folders[i].guid))
                        continue; // not moving this one

                    ParentViewFolder otherFolder = anotherPvState.folders[i];

                    int myIndex = FolderSelectionIndexFromGUID(pvState.folders, otherFolder.guid);

                    if (myIndex != -1)
                    {
                        // only move remaining files
                        ParentViewFolder myFolder = pvState.folders[myIndex];

                        // handle selection array
                        int itemCountToCopy = pvState.lv.totalRows - myIndex - 1 - myFolder.files.Length;
                        int copyFrom = myIndex + 1 + myFolder.files.Length;

                        System.Array.Copy(pvState.selectedItems, copyFrom,
                            pvState.selectedItems, copyFrom + otherFolder.files.Length, itemCountToCopy);

                        // move files
                        ArrayUtility.AddRange(ref myFolder.files, otherFolder.files);

                        // select files
                        for (int k = 1; k <= myFolder.files.Length; k++)
                        {
                            pvState.selectedItems[myIndex + k] = true;
                        }

                        // don't care about selection order, all files are selected anyway
                        System.Array.Sort(myFolder.files, ParentViewState.CompareViewFile);
                    }
                    else
                    {
                        // move all folder

                        myIndex = 0;

                        // figure out where the new folder will be inserted.
                        for (int k = 0; k < pvState.folders.Length; k++)
                        {
                            if (ParentViewState.CompareViewFolder(pvState.folders[myIndex], otherFolder) > 0)
                                break;

                            myIndex += 1 + pvState.folders[k].files.Length;
                        }

                        // adjust selection
                        int itemCountToCopy = pvState.lv.totalRows - myIndex;
                        int copyFrom = myIndex;

                        System.Array.Copy(pvState.selectedItems, copyFrom,
                            pvState.selectedItems, copyFrom + 1 + otherFolder.files.Length, itemCountToCopy);

                        // add the folder
                        ArrayUtility.Add(ref pvState.folders, otherFolder);

                        // select folder and files
                        for (int k = 0; k <= otherFolder.files.Length; k++)
                        {
                            pvState.selectedItems[myIndex + k] = true;
                        }

                        needsSort = true;
                    }

                    // remove from other side
                    ArrayUtility.RemoveAt(ref anotherPvState.folders, i);

                    changed = true;
                }

                anotherPvState.SetLineCount();
            }

            pvState.SetLineCount();

            if (needsSort)
            {
                System.Array.Sort(pvState.folders, ParentViewState.CompareViewFolder);
            }

            return changed;  
        }
        #endregion

        #region Misc functions
        private static bool HasFlag(ChangeFlags flags, ChangeFlags flagToCheck) { return ((int)flagToCheck & (int)flags) != 0; }

        private void DoSelectionChange()
        {
            // sync selection. Cache guids.
            HierarchyProperty prop = new HierarchyProperty(HierarchyType.Assets);
            List<string> guids = new List<string>(Selection.objects.Length);

            foreach (Object obj in Selection.objects)
            {
                if (prop.Find(obj.GetInstanceID(), null))
                {
                    guids.Add(prop.guid);
                }
            }

            // Try selecting in "active" ParentView first
            if (pv1hasSelection)
            {
                pv1hasSelection = MarkSelected(pv1state, guids);
            }

            if (!pv1hasSelection)
            {
                if (pv2hasSelection)
                {
                    pv2hasSelection = MarkSelected(pv2state, guids);
                }

                if (!pv2hasSelection)
                {
                    // there is no selection, try left ParentView first
                    pv1hasSelection = MarkSelected(pv1state, guids);

                    // still no selection? try right one
                    if (!pv1hasSelection)
                        pv2hasSelection = MarkSelected(pv2state, guids);
                }
            }
        }

        internal void OnSelectionChange()
        {
            if (!mySelection)
            {
                DoSelectionChange();
                parentWin.Repaint();
            }
            else
                mySelection = false; // don't reselect when this window changes selection

			somethingDiscardableSelected = ASCommitWindow.SomethingDiscardableSelected(pv2hasSelection ? pv2state : pv1state);
        }

		public static bool SomethingDiscardableSelected(ParentViewState st)
		{
			int i = 0;

            foreach (ParentViewFolder dir in st.folders)
            {
				if (st.selectedItems[i++])
					return true;

                foreach (ParentViewFile file in dir.files)
                {
					if (st.selectedItems[i++] && IsDiscardableAsset(file.guid, file.changeFlags))
						return true;
                }
            }

			return false;
		}

        private static bool AllFolderWouldBeMovedAnyway(ParentViewState pvState, string guid)
        {
            int index = 0;

            for (int i = 0; i < pvState.folders.Length; i++)
            {
                if (pvState.folders[i].guid == guid)
                {
                    bool noneSelected = true;
                    bool allSelected = true;

                    bool folderSelected = pvState.selectedItems[index++];

                    for (int k = 0; k < pvState.folders[i].files.Length; k++)
                    {
                        if (pvState.selectedItems[index++])
                            noneSelected = false;
                        else
                            allSelected = false;
                    }

                    return folderSelected && (allSelected || noneSelected);
                }

                index += 1 + pvState.folders[i].files.Length;
            }

            return false;
        }

        bool DoShowMyDiff(bool binary)
        {
            return DoShowDiff(GetParentViewSelectedItems(pv2hasSelection ? pv2state : pv1state, false, false), binary);
        }

        void DoMyRevert(bool afterMarkingDependencies)
        {
            if (!afterMarkingDependencies)
            {
                bool changed;

				List<string> prevSelection = GetSelectedItems();

                if (pv2hasSelection)
					changed = MarkAllFolderDependenciesForDiscarding(pv2state, pv1state);
                else
					changed = MarkAllFolderDependenciesForDiscarding(pv1state, pv2state);

                if (changed)
                    MySelectionToGlobalSelection();

				List<string> currSelection = GetSelectedItems();

				if (prevSelection.Count != currSelection.Count) // it never deselects, so this means it changed
					changed = true;

                lastRevertSelectionChanged = changed ? 1 : -1; // see below...
            }

            if (afterMarkingDependencies || lastRevertSelectionChanged == -1)
            {
                SetPersistedData();
                DoRevert(GetParentViewSelectedItems(pv2hasSelection ? pv2state : pv1state, true, true), "CBReinitCommitWindow");
            }
        }

        void MenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                description = commitMessageList[selected];
                resetKeyboardControl = true;
                parentWin.Repaint();
            }
        }

        void ContextMenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                switch (dropDownMenuItems[selected])
                {
                    case "Compare":
                        DoShowMyDiff(false);
                        break;
                    case "Compare Binary":
                        DoShowMyDiff(true);
                        break;
                    case "Discard":
                        DoMyRevert(false);
                        break;
                    case ">>>":
                        DoTransferAll(pv1state, pv2state, pv1state.selectedFolder, pv1state.selectedFile);
                        break;
                    case "<<<":
                        DoTransferAll(pv2state, pv1state, pv2state.selectedFolder, pv2state.selectedFile);
                        break;
                }
                
            }
        }

        private static int IndexOfFolderWithGUID(ParentViewFolder[] folders, string guid)
        {
            for (int i = 0; i < folders.Length; i++)
                if (folders[i].guid == guid)
                    return i;

            return -1;
        }

		private static int FolderIndexToTotalIndex(ParentViewFolder[] folders, int folderIndex)
		{
			int idx = 0;

			for (int i = 0; i < folderIndex; i++)
			{
				idx += folders[i].files.Length + 1;
			}

			return idx;
		}

        private static int FolderSelectionIndexFromGUID(ParentViewFolder[] folders, string guid)
        {
            int index = 0;

            for (int i = 0; i < folders.Length; i++)
            {
                if (guid == folders[i].guid)
                    return index;

                index += 1 + folders[i].files.Length;
            }

            return -1;
        }

        private void AddFolderToRemove(ref List<string> guidsOfFoldersToRemove, string guid)
        {
            if (!guidsOfFoldersToRemove.Contains(guid))
                guidsOfFoldersToRemove.Add(guid);
        }
        #endregion

        #region GUI
        // returns flag if GUI should continue drawing
		private bool ParentViewGUI(ParentViewState pvState, ParentViewState anotherPvState, ref bool hasSelection)
		{
			bool abnormalQuit = false;
			EditorGUIUtility.SetIconSize (iconSize);
            ChangeFlags flags;
			
			ListViewState pv = pvState.lv;

			bool shiftIsDown = Event.current.shift;
            bool ctrlIsDown = EditorGUI.actionKey;

			ParentViewFolder folder;

			int prevSelectedRow = pv.row;
			int folderI = -1, fileI = -1;
            bool selChanged = false;

			foreach (ListViewElement el in ListViewGUILayout.ListView(pv, ListViewOptions.wantsToAcceptCustomDrag | ListViewOptions.wantsToStartCustomDrag, 
                dragTitle, GUIStyle.none))
			{
				if (folderI == -1)
				{
					if (!pvState.IndexToFolderAndFile(el.row, ref folderI, ref fileI))
					{
						abnormalQuit = true;
						break;
					}
				}

                if (GUIUtility.keyboardControl == pv.ID && Event.current.type == EventType.KeyDown && ctrlIsDown)
                    Event.current.Use();

				folder = pvState.folders[folderI];

                if (pvState.selectedItems[el.row] && Event.current.type == EventType.Repaint)
                {
                    constants.entrySelected.Draw(el.position, false, false, false, false);
                }

				if (ListViewGUILayout.HasMouseUp(el.position))
				{
					if (!shiftIsDown && !ctrlIsDown)
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, pvState.lv.row, ref pvState.initialSelectedItem, ref pvState.selectedItems);
				}
				else
				if (ListViewGUILayout.HasMouseDown(el.position))
				{
                    if (Event.current.clickCount == 2)
                    {
                        DoShowMyDiff(false);
                        GUIUtility.ExitGUI();
                    }
                    else
                    {
                        if (!pvState.selectedItems[el.row] || shiftIsDown || ctrlIsDown)
                        {
							selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref pvState.initialSelectedItem, ref pvState.selectedItems);
                        }

                        pvState.selectedFile = fileI;
                        pvState.selectedFolder = folderI;
                        pv.row = el.row;
                    }
				}else
				if (ListViewGUILayout.HasMouseDown(el.position, 1))
                {
                    if (!pvState.selectedItems[el.row])
                    {
                        selChanged = true;
                        pvState.ClearSelection();
                        pvState.selectedItems[el.row] = true;

                        pvState.selectedFile = fileI;
                        pvState.selectedFolder = folderI;
                        pv.row = el.row;
                    }

                    dropDownMenuItems[0] = (pvState == pv1state) ? ">>>" : "<<<";

                    GUIUtility.hotControl = 0;
                    Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
                    EditorUtility.DisplayCustomMenu(r, dropDownMenuItems, null, ContextMenuClick, null);
                    Event.current.Use();
                }

				if (fileI != -1) // not a folder line
				{
                    Texture2D icon = AssetDatabase.GetCachedIcon(folder.name + "/" + folder.files[fileI].name) as Texture2D;

					if (icon == null)
						icon = InternalEditorUtility.GetIconForFile(folder.files[fileI].name);

                    GUILayout.Label(new GUIContent(folder.files[fileI].name, icon), constants.element);
                    //GUILayout.Label(new GUIContent(folder.files[fileI].guid, icon), constants.element);

                    flags = (ChangeFlags)folder.files[fileI].changeFlags;
				}else
				{
                    GUILayout.Label(folder.name, constants.header);
                    //GUILayout.Label(folder.guid, constants.header);

                    flags = (ChangeFlags)folder.changeFlags;
                }

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
				//----

				pvState.NextFileFolder(ref folderI, ref fileI);
			}

			if (!abnormalQuit)
			{
                // "Select All"
                if (GUIUtility.keyboardControl == pv.ID)
                {
                    if (Event.current.type == EventType.ValidateCommand && Event.current.commandName == "SelectAll")
                    {
                        Event.current.Use();
                    }
                    else
                        if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "SelectAll")
                        {
                            for (int i = 0; i < pvState.selectedItems.Length; i++)
                                pvState.selectedItems[i] = true;

                            selChanged = true;

                            Event.current.Use();
                        }
                }

                if (pv.customDraggedFromID != 0 && pv.customDraggedFromID == anotherPvState.lv.ID)
				{
					DoTransferAll(anotherPvState, pvState, pvState.selectedFolder, pvState.selectedFile);
				}
					
				if (GUIUtility.keyboardControl == pv.ID && !ctrlIsDown)
				{
					if (pv.selectionChanged)
					{
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, pv.row, ref pvState.initialSelectedItem, ref pvState.selectedItems);

						if (!pvState.IndexToFolderAndFile(pv.row, ref pvState.selectedFolder, ref pvState.selectedFile))
							abnormalQuit = true;
					}else
                    if ((pvState.selectedFolder != -1) && Event.current.type == EventType.KeyDown && GUIUtility.keyboardControl == pv.ID)
					{
						if (Event.current.keyCode == KeyCode.Return)
						{
							DoTransferAll(pvState, anotherPvState, pvState.selectedFolder, pvState.selectedFile);
							ListViewGUILayout.MultiSelection(prevSelectedRow, pv.row, ref pvState.initialSelectedItem, ref pvState.selectedItems);
							pvState.IndexToFolderAndFile(pv.row, ref pvState.selectedFolder, ref pvState.selectedFile); // keep selection
							Event.current.Use ();
							abnormalQuit = true;
						}
					}
				}

                if (pv.selectionChanged || selChanged)
                {
                    if (pvState.IndexToFolderAndFile(pv.row, ref folderI, ref fileI))
                    {
                        dragTitle = fileI == -1 ? pvState.folders[folderI].name : pvState.folders[folderI].files[fileI].name;
                    }

                    anotherPvState.ClearSelection();
                    anotherPvState.lv.row = -1;
                    anotherPvState.selectedFile = -1;
                    anotherPvState.selectedFolder = -1;

                    MySelectionToGlobalSelection();
                }
			}

			EditorGUIUtility.SetIconSize (Vector2.zero);
			
			return !abnormalQuit;
		}

        internal bool DoGUI()
		{
			bool wasGUIEnabled = GUI.enabled;

            if (constants == null)
            {
                constants = new Constants();
			}

            if (resetKeyboardControl)
            {
                resetKeyboardControl = false;
                GUIUtility.keyboardControl = 0;
            }

            bool showSmallWindow = parentWin.position.width <= widthToHideButtons;

			if (Event.current.type == EventType.Layout)
			{
				wasHidingButtons = showSmallWindow;
			}else
			{
				if (showSmallWindow != wasHidingButtons)
					GUIUtility.ExitGUI();
			}			
			
			SplitterGUILayout.BeginHorizontalSplit(horSplit);
			
			GUILayout.BeginVertical(constants.box);
            GUILayout.Label(totalChanges, constants.title);
			if (!ParentViewGUI(pv1state, pv2state, ref pv1hasSelection)) return true;
			GUILayout.EndVertical();
			
			SplitterGUILayout.BeginVerticalSplit(vertSplit);
			
			GUILayout.BeginVertical(constants.box);
			GUILayout.Label("Changeset", constants.title);
            if (!ParentViewGUI(pv2state, pv1state, ref pv2hasSelection)) return true;
			GUILayout.EndVertical();
			
			GUILayout.BeginVertical();
			GUILayout.Label("Commit Message", constants.title);

            GUILayout.BeginHorizontal();

            if (commitMessageList.Length > 0)
            {
                GUIContent content = new GUIContent("Recent");

                Rect rect = GUILayoutUtility.GetRect(content, constants.dropDown, null);
                if (GUI.Button(rect, content, constants.dropDown))
                {
                    GUIUtility.hotControl = 0;

                    string[] shortList = new string[commitMessageList.Length];

                    for (int i = 0; i < shortList.Length; i++)
                    {
                        shortList[i] = (commitMessageList[i].Length > 200) ?
                             commitMessageList[i].Substring(0, 200) + " ... " :
                             commitMessageList[i];
                    }

                    EditorUtility.DisplayCustomMenu(rect, shortList, null, MenuClick, null);
                    Event.current.Use();
                }
            }
            GUILayout.FlexibleSpace();

            GUILayout.EndHorizontal();

            GUILayout.BeginHorizontal(constants.box);
            scrollPos = EditorGUILayout.BeginVerticalScrollView(scrollPos);
            description = EditorGUILayout.TextArea(description, constants.wwText);
            EditorGUILayout.EndScrollView();
            GUILayout.EndHorizontal();

			GUILayout.EndVertical();
			
			SplitterGUILayout.EndVerticalSplit();
			SplitterGUILayout.EndHorizontalSplit();

            if (!showSmallWindow)
                GUILayout.Label("Please drag files you want to commit to Changeset and fill in commit description");

            GUILayout.BeginHorizontal();

            if (!pv1hasSelection && !pv2hasSelection)
                GUI.enabled = false;

            if (!showSmallWindow)
            {
                // TODO: only show this if files (not only folders) are selected?
                if (GUILayout.Button("Compare", constants.button))
                {
                    DoShowMyDiff(false);
                    GUIUtility.ExitGUI();
                }
            }

			bool dWasGUIEnabled = GUI.enabled;
			if (!somethingDiscardableSelected)
				GUI.enabled = false;

            if (GUILayout.Button(showSmallWindow ? "Discard" : "Discard Selected Changes", constants.button))
            {
                DoMyRevert(false);
                GUIUtility.ExitGUI();
            }

			GUI.enabled = dWasGUIEnabled;

            GUILayout.FlexibleSpace();

            GUI.enabled = pv1hasSelection && wasGUIEnabled;

            if (GUILayout.Button(showSmallWindow ? ">" : ">>>", constants.button))
                DoTransferAll(pv1state, pv2state, pv1state.selectedFolder, pv1state.selectedFile);

            GUI.enabled = pv2hasSelection && wasGUIEnabled;

            if (GUILayout.Button(showSmallWindow ? "<" : "<<<", constants.button))
                DoTransferAll(pv2state, pv1state, pv2state.selectedFolder, pv2state.selectedFile);

            GUI.enabled = pv1state.lv.totalRows != 0 && wasGUIEnabled;

            if (GUILayout.Button("Add All", constants.button))
            {
                for (int i = 0; i < pv1state.selectedItems.Length; pv1state.selectedItems[i++] = true) ;

                DoTransferAll(pv1state, pv2state, pv1state.selectedFolder, pv1state.selectedFile);
            }

            GUI.enabled = pv2state.lv.totalRows != 0 && wasGUIEnabled;

            if (GUILayout.Button("Remove All", constants.button))
            {
                for (int i = 0; i < pv2state.selectedItems.Length; pv2state.selectedItems[i++] = true) ;

                DoTransferAll(pv2state, pv1state, pv2state.selectedFolder, pv2state.selectedFile);
            }

            GUI.enabled = wasGUIEnabled;

            GUILayout.EndHorizontal();

			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();

			if (!CanCommit())
				GUI.enabled = false;

			if (GUILayout.Button("Commit", constants.bigButton, GUILayout.MinWidth(100)))
			{
                DoCommit();
			}

			GUI.enabled = wasGUIEnabled;

			// FIXME: no need to check for mac here? if windows user has mac-like keyboard
			if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.KeypadEnter 
				&& Application.platform == RuntimePlatform.OSXEditor && CanCommit())
			{
                DoCommit();
			}

			GUILayout.EndHorizontal();

            if (AssetServer.GetAssetServerError() != string.Empty)
            {
                GUILayout.Space(10);
                GUILayout.Label(AssetServer.GetAssetServerError(), constants.errorLabel);
                GUILayout.Space(10);
            }

			GUILayout.Space(10);

            // this clutter is to show user the files that where selected when showing the question dialog. Need to:
            // 1. skip this OnGUI (selection changed, will draw on next OnGUI)
            // 2. skip next OnGUI so stuff gets actually drawn
            // 3. show the warning message
            if (lastRevertSelectionChanged == 0)
            {
                lastRevertSelectionChanged = -1;

				if (ShowDiscardWarning())
				{
					DoMyRevert(true);
				}
            }

            if (lastRevertSelectionChanged > 0)
            {
                if (Event.current.type == EventType.Repaint)
                    lastRevertSelectionChanged--;

                parentWin.Repaint();
            }

            if (showReinitedWarning == 0)
            {
                EditorUtility.DisplayDialog("Commits updated", "Commits had to be updated to reflect latest changes", "OK", "");
                showReinitedWarning = -1;
            }

            if (showReinitedWarning > 0)
            {
                if (Event.current.type == EventType.Repaint)
                    showReinitedWarning--;

                parentWin.Repaint();
            }

            //GUILayout.Label(parentWin.position.width.ToString());

			return true;
		}
        #endregion
	}
}