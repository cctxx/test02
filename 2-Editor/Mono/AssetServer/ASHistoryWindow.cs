using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
    internal class ASHistoryWindow
    {
        #region Locals
        internal class Constants
        {
            public GUIStyle selected = "ServerUpdateChangesetOn";
            public GUIStyle lvHeader = "OL title";
            public GUIStyle button = "Button";
            public GUIStyle label = "PR Label";
			public GUIStyle descriptionLabel = "Label";
            public GUIStyle entryEven = "CN EntryBackEven";
            public GUIStyle entryOdd = "CN EntryBackOdd";
            public GUIStyle boldLabel = "BoldLabel";
			public GUIStyle foldout = "IN Foldout";
			public GUIStyle ping = new GUIStyle("PR Ping");

			public Constants()
			{
				// Ping style was changed in editor_resources. Resetting the values back to old ones for backward compatibility.
				ping.overflow.left = -2;
				ping.overflow.right = -21;
				ping.padding.left = 48;
				ping.padding.right = 0;
			}
        }

        static Constants ms_Style = null;
        static int ms_HistoryControlHash = "HistoryControl".GetHashCode();

        const int kFirst = -999999;
        const int kLast = 999999;
		const int kUncollapsedItemsCount = 5;

        SplitterState m_HorSplit = new SplitterState(new float[] { 30, 70 }, new int[] { 60, 100 }, null);

        static Vector2 ms_IconSize = new Vector2(16, 16);
		bool m_NextSelectionMine = false;

        [System.Serializable]
        class GUIHistoryListItem
        {
            public GUIContent colAuthor;
			public GUIContent colRevision;
			public GUIContent colDate;
			public GUIContent colDescription;
            public ParentViewState assets;
			public int totalLineCount;
            public bool[] boldAssets;
            public int height;
            public bool inFilter;
			public int collapsedItemCount;
			public int startShowingFrom;
        }

        GUIHistoryListItem[] m_GUIItems;
        int m_TotalHeight = 0;
        Vector2 m_ScrollPos = Vector2.zero;

        bool m_SplittersOk = false;

        int m_RowHeight = 16;
        int m_HistoryControlID = -1;

        int m_ChangesetSelectionIndex = -1;
        int m_AssetSelectionIndex = -1;

		int m_ChangeLogSelectionRev = -1;
		int ChangeLogSelectionRev 
        {
            get 
            {
                return m_ChangeLogSelectionRev;
            }
            set
            {
				m_ChangeLogSelectionRev = value;
				if (m_InRevisionSelectMode)
                {
                    FinishShowCustomDiff();
                }
			}
        }

        bool m_BinaryDiff = false;
        int m_Rev1ForCustomDiff = -1;

        int m_ScrollViewHeight = 0;

        string m_ChangeLogSelectionGUID = string.Empty;
        string m_ChangeLogSelectionAssetName = string.Empty;

        string m_SelectedPath = string.Empty;
        string m_SelectedGUID = string.Empty;
        bool m_FolderSelected = false;

        bool m_InRevisionSelectMode = false;
		static GUIContent emptyGUIContent = new GUIContent();

        GUIContent[] m_DropDownMenuItems = new GUIContent[] { 
            EditorGUIUtility.TextContent("Show History"), emptyGUIContent, EditorGUIUtility.TextContent("Compare to Local"), 
			EditorGUIUtility.TextContent("Compare Binary to Local"), emptyGUIContent, 
            EditorGUIUtility.TextContent("Compare to Another Revision"), EditorGUIUtility.TextContent("Compare Binary to Another Revision"), emptyGUIContent, 
            EditorGUIUtility.TextContent("Download This File") }; // change ContextMenuClick function when changing this
        GUIContent[] m_DropDownChangesetMenuItems = new GUIContent[] { EditorGUIUtility.TextContent("Revert Entire Project to This Changeset") }; // change ChangesetContextMenuClick function when changing this

        EditorWindow m_ParentWindow = null; // for repaint in OnSelectionChanged, cannot use HandleUtility.Repaint, because that event is called outside GUI

        Changeset[] m_Changesets;
        ASHistoryFileView m_FileViewWin = new ASHistoryFileView();
        #endregion

        public ASHistoryWindow(EditorWindow parent)
        {
            m_ParentWindow = parent;

            ASEditorBackend.SettingsIfNeeded();

			if (Selection.objects.Length != 0)
				m_FileViewWin.SelType = ASHistoryFileView.SelectionType.Items;
        }

        void ContextMenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                switch (selected)
                {
                    case 0: //"Show History":
                        ShowAssetsHistory();
                        break;
                    case 2: //"Compare to Local":
                        DoShowDiff(false, ChangeLogSelectionRev, -1);
                        break;
                    case 3: //"Compare Binary to Local":
                        DoShowDiff(true, ChangeLogSelectionRev, -1);
                        break;
                    case 5: //"Compare to Another Revision":
                        DoShowCustomDiff(false);
                        break;
                    case 6: //"Compare Binary to Another Revision":
                        DoShowCustomDiff(true);
                        break;
                    case 8: //"Download This File":
                        DownloadFile();
                        break;
                }
            }
        }

        void DownloadFile()
        {
            if (ChangeLogSelectionRev < 0 || m_ChangeLogSelectionGUID == string.Empty)
                return;

			//TODO: not sure how to localize this
            if (EditorUtility.DisplayDialog("Download file",
                "Are you sure you want to download '" + m_ChangeLogSelectionAssetName + "' from revision " +
                ChangeLogSelectionRev.ToString() + " and lose all changes?",
                "Download", "Cancel"))
            {
                AssetServer.DoRevertOnNextTick(ChangeLogSelectionRev, m_ChangeLogSelectionGUID);
            }
        }

        void ShowAssetsHistory()
        {
            if (AssetServer.IsAssetAvailable(m_ChangeLogSelectionGUID) != 0)
            {
                string[] guids = new string[1];
                guids[0] = m_ChangeLogSelectionGUID;
                m_FileViewWin.SelType = ASHistoryFileView.SelectionType.Items;
                AssetServer.SetSelectionFromGUIDs(guids);
            }else
            {
                m_FileViewWin.SelectDeletedItem(m_ChangeLogSelectionGUID);
                DoLocalSelectionChange();
            }
        }

        void ChangesetContextMenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                switch (selected)
                {
                    case 0: //"Revert All Project to This Changeset":
                        DoRevertProject();
                        break;
                }
            }
        }

        void DoRevertProject()
        {
            if (ChangeLogSelectionRev > 0)
            {
                ASEditorBackend.ASWin.RevertProject(ChangeLogSelectionRev, m_Changesets);
            }
        }

		int MarkBoldItemsBySelection(GUIHistoryListItem item)
		{
			List<string> selectedGUIDs = new List<string>();
			ParentViewState pv = item.assets;
			int firstIndex = -1;
			int totalIndex = 0;

			if (Selection.instanceIDs.Length == 0)
				return 0;

			foreach (int id in Selection.instanceIDs)
			{
				selectedGUIDs.Add(AssetDatabase.AssetPathToGUID(AssetDatabase.GetAssetPath(id)));
			}

			for (int i = 0; i < pv.folders.Length; i++)
			{
				ParentViewFolder folder = pv.folders[i];

				if (selectedGUIDs.Contains(folder.guid))
				{
					item.boldAssets[totalIndex] = true;
					if (firstIndex == -1)
						firstIndex = totalIndex;
				}

				totalIndex++;

				for (int k = 0; k < folder.files.Length; k++)
				{
					if (selectedGUIDs.Contains(folder.files[k].guid))
					{
						item.boldAssets[totalIndex] = true;
						if (firstIndex == -1)
							firstIndex = totalIndex;
					}
					totalIndex++;
				}
			}

			return firstIndex;
		}

        int CheckParentViewInFilterAndMarkBoldItems(GUIHistoryListItem item, string text)
        {
            ParentViewState pv = item.assets;
			int firstIndex = -1;
            int totalIndex = 0;

            for (int i = 0; i < pv.folders.Length; i++)
            {
                ParentViewFolder folder = pv.folders[i];

                if (folder.name.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) != -1)
                {
                    item.boldAssets[totalIndex] = true;
					if (firstIndex == -1)
						firstIndex = totalIndex;
                }

                totalIndex++;

                for (int k = 0; k < folder.files.Length; k++)
                {
                    if (folder.files[k].name.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) != -1)
                    {
                        item.boldAssets[totalIndex] = true;
						if (firstIndex == -1)
							firstIndex = totalIndex;
					}
                    totalIndex++;
                }
            }

            return firstIndex;
        }

        void MarkBoldItemsByGUID(string guid)
        {
            for (int itm = 0; itm < m_GUIItems.Length; itm++)
            {
                GUIHistoryListItem item = m_GUIItems[itm];
                ParentViewState pv = item.assets;
                int totalIndex = 0;

                item.boldAssets = new bool[pv.GetLineCount()];

                for (int i = 0; i < pv.folders.Length; i++)
                {
                    ParentViewFolder folder = pv.folders[i];

                    if (folder.guid == guid)
                    {
                        item.boldAssets[totalIndex] = true;
                    }

                    totalIndex++;

                    for (int k = 0; k < folder.files.Length; k++)
                    {
                        if (folder.files[k].guid == guid)
                        {
                            item.boldAssets[totalIndex] = true;
                        }
                        totalIndex++;
                    }
                }
            }
        }


        public void FilterItems(bool recreateGUIItems)
        {
            m_TotalHeight = 0;

            if (m_Changesets == null || m_Changesets.Length == 0)
            {
                m_GUIItems = null;
                return;
            }

            if (recreateGUIItems)
                m_GUIItems = new GUIHistoryListItem[m_Changesets.Length];

            string text = ((ASMainWindow)m_ParentWindow).m_SearchField.FilterText;
            bool noFilter = text.Trim() == string.Empty;

            for (int i = 0; i < m_Changesets.Length; i++)
            {
                if (recreateGUIItems)
                {
                    m_GUIItems[i] = new GUIHistoryListItem();

					m_GUIItems[i].colAuthor = new GUIContent(m_Changesets[i].owner);
					m_GUIItems[i].colRevision = new GUIContent(m_Changesets[i].changeset.ToString());
					m_GUIItems[i].colDate = new GUIContent(m_Changesets[i].date);
					m_GUIItems[i].colDescription = new GUIContent(m_Changesets[i].message);
                    m_GUIItems[i].assets = new ParentViewState();
                    m_GUIItems[i].assets.AddAssetItems(m_Changesets[i]);
					m_GUIItems[i].totalLineCount = m_GUIItems[i].assets.GetLineCount();
					m_GUIItems[i].height = m_RowHeight * (1 + m_GUIItems[i].totalLineCount) + 20 + (int)ms_Style.descriptionLabel.CalcHeight(m_GUIItems[i].colDescription, float.MaxValue);
                }

                m_GUIItems[i].boldAssets = new bool[m_GUIItems[i].assets.GetLineCount()];

				int firstBoldItemIndex = noFilter ? MarkBoldItemsBySelection(m_GUIItems[i]) : CheckParentViewInFilterAndMarkBoldItems(m_GUIItems[i], text);

                m_GUIItems[i].inFilter = noFilter ||
					firstBoldItemIndex != -1 ||
					(m_GUIItems[i].colDescription.text.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) >= 0) ||
					(m_GUIItems[i].colRevision.text.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) >= 0) ||
					(m_GUIItems[i].colAuthor.text.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) >= 0) ||
					(m_GUIItems[i].colDate.text.IndexOf(text, System.StringComparison.InvariantCultureIgnoreCase) >= 0);

				if (recreateGUIItems && (m_GUIItems[i].totalLineCount > kUncollapsedItemsCount))
				{
					m_GUIItems[i].collapsedItemCount = m_GUIItems[i].totalLineCount - kUncollapsedItemsCount + 1;
					m_GUIItems[i].height = m_RowHeight * (1 + kUncollapsedItemsCount) + 20 + (int)ms_Style.descriptionLabel.CalcHeight(m_GUIItems[i].colDescription, float.MaxValue);
				}

				m_GUIItems[i].startShowingFrom = 0;

				if (m_GUIItems[i].collapsedItemCount != 0 && m_GUIItems[i].totalLineCount > kUncollapsedItemsCount && firstBoldItemIndex >= kUncollapsedItemsCount - 1)
				{
					if (firstBoldItemIndex + kUncollapsedItemsCount - 1 > m_GUIItems[i].totalLineCount)
						m_GUIItems[i].startShowingFrom = m_GUIItems[i].totalLineCount - kUncollapsedItemsCount + 1;
					else
						m_GUIItems[i].startShowingFrom = firstBoldItemIndex;
				}

				if (m_GUIItems[i].inFilter)
					m_TotalHeight += m_GUIItems[i].height;
            }
        }

		private void UncollapseListItem(ref GUIHistoryListItem item)
		{
			int heightToAdd = (item.collapsedItemCount - 1) * m_RowHeight;
			item.collapsedItemCount = 0;
			item.startShowingFrom = 0;
			item.height += heightToAdd;
			m_TotalHeight += heightToAdd;
		}

        private void ClearLV()
        {
            m_Changesets = new Changeset[0];
            m_TotalHeight = 5;
        }

        public void DoLocalSelectionChange()
        {
			if (m_NextSelectionMine)
			{
				m_NextSelectionMine = false;
				return;
			}
            Object[] objects = Selection.GetFiltered(typeof(Object), SelectionMode.Assets);
            string[] selectionGUIDs = new string[0];

			switch (m_FileViewWin.SelType)
            {
                case ASHistoryFileView.SelectionType.DeletedItemsRoot:
					if (Selection.objects.Length != 0)
					{
						Selection.objects = new Object[0];
						m_NextSelectionMine = true;
					}

                    selectionGUIDs = m_FileViewWin.GetAllDeletedItemGUIDs();

                    if (selectionGUIDs.Length == 0) // don't go default path in this case, would retrieve history of all project.
                    {
                        ClearLV();
                        return;
                    }
                    break;
                case ASHistoryFileView.SelectionType.DeletedItems:
					if (Selection.objects.Length != 0)
					{
						Selection.objects = new Object[0];
						m_NextSelectionMine = true;
					}

                    selectionGUIDs = m_FileViewWin.GetSelectedDeletedItemGUIDs();
                    break;
                case ASHistoryFileView.SelectionType.Items:
                    if (objects.Length < 1)
                    {
                        m_SelectedPath = string.Empty;
                        m_SelectedGUID = string.Empty;
                        ClearLV();
                        return;
                    }
                    else
                    {
                        m_SelectedPath = AssetDatabase.GetAssetPath(objects[0]);
                        m_SelectedGUID = AssetDatabase.AssetPathToGUID(m_SelectedPath);
						selectionGUIDs = m_FileViewWin.GetImplicitProjectViewSelection();
                    }
                    break;
                case ASHistoryFileView.SelectionType.All:
					if (Selection.objects.Length != 0)
					{
						Selection.objects = new Object[0];
						m_NextSelectionMine = true;
					}

                    m_SelectedPath = "";
                    m_SelectedGUID = "";
                    ClearLV();
                    break;
            }

            m_Changesets = AssetServer.GetHistorySelected(selectionGUIDs);

            if (m_Changesets != null)
            {
                FilterItems(true);
            }
            else
                ClearLV();

            if (selectionGUIDs != null && m_GUIItems != null && selectionGUIDs.Length == 1)
                MarkBoldItemsByGUID(m_SelectedGUID);

            m_ParentWindow.Repaint();
        }

        public void OnSelectionChange()
        {
            if (Selection.objects.Length != 0)
                m_FileViewWin.SelType = ASHistoryFileView.SelectionType.Items;
            DoLocalSelectionChange();
        }

        void DoShowDiff(bool binary, int ver1, int ver2)
        {
            List<string> assetsToCompare = new List<string>();
            List<CompareInfo> compareOptions = new List<CompareInfo>();

			if (ver2 == -1 && AssetDatabase.GUIDToAssetPath(m_ChangeLogSelectionGUID) == string.Empty)
			{
				Debug.Log("Cannot compare asset " + m_ChangeLogSelectionAssetName + " to local version because it does not exists.");
				return;
			}

            assetsToCompare.Add(m_ChangeLogSelectionGUID);
            compareOptions.Add(new CompareInfo(ver1, ver2, binary ? 1 : 0, binary ? 0 : 1));

			//TODO: localize
            Debug.Log("Comparing asset " + m_ChangeLogSelectionAssetName + " revisions " + ver1.ToString() + " and " + (ver2 == -1 ? "Local" : ver2.ToString()));

            AssetServer.CompareFiles(assetsToCompare.ToArray(), compareOptions.ToArray());
        }

        void DoShowCustomDiff(bool binary)
        {
            ShowAssetsHistory();
            m_InRevisionSelectMode = true;
            m_BinaryDiff = binary;
            m_Rev1ForCustomDiff = ChangeLogSelectionRev;
        }

        void FinishShowCustomDiff()
        {
			if (m_Rev1ForCustomDiff != ChangeLogSelectionRev)
			{
				DoShowDiff(m_BinaryDiff, m_Rev1ForCustomDiff, ChangeLogSelectionRev);
			}
			else
				Debug.Log("You chose to compare to the same revision.");

			m_InRevisionSelectMode = false;
        }

        void CancelShowCustomDiff()
        {
            m_InRevisionSelectMode = false;
        }

        bool IsComparableAssetSelected()
        {
            return !m_FolderSelected && m_ChangeLogSelectionGUID != string.Empty;
        }

        void DrawBadge(Rect offset, ChangeFlags flags, GUIStyle style, GUIContent content, float textColWidth)
        {
            if (Event.current.type != EventType.Repaint)
                return;

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

            if (iconContent != null)
            {
				float width = style.CalcSize(content).x;
				float x;

				if (width > textColWidth - iconContent.image.width)
					x = offset.xMax - iconContent.image.width - 5;
				else
					x = textColWidth - iconContent.image.width;
				
                Rect iconPos = new Rect(
                    x,
                    offset.y + offset.height / 2 - iconContent.image.height / 2,
                    iconContent.image.width,
                    iconContent.image.height);

                EditorGUIUtility.SetIconSize(Vector2.zero);
                GUIStyle.none.Draw(iconPos, iconContent, false, false, false, false);
                EditorGUIUtility.SetIconSize(ms_IconSize);
            }
        }

        bool HasFlag(ChangeFlags flags, ChangeFlags flagToCheck) { return ((int)flagToCheck & (int)flags) != 0; }

		void ClearItemSelection()
		{
			m_ChangeLogSelectionGUID = string.Empty;
			m_ChangeLogSelectionAssetName = string.Empty;
			m_FolderSelected = false;
			m_AssetSelectionIndex = -1;
		}

        void DrawParentView(Rect r, ref GUIHistoryListItem item, int changesetIndex, bool hasFocus)
        {
            ParentViewState pv = item.assets;
            GUIContent content = new GUIContent();
            Texture2D folderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);
            Event evt = Event.current;

            hasFocus &= m_HistoryControlID == GUIUtility.keyboardControl;

            r.height = m_RowHeight;
            r.y += 3;

            GUIStyle style;
            int totalIndex = -1;
			int upTo = item.collapsedItemCount != 0 ? 4 : item.totalLineCount;
			upTo += item.startShowingFrom;

            for (int i = 0; i < pv.folders.Length; i++)
            {
                ParentViewFolder folder = pv.folders[i];
                content.text = folder.name;
                content.image = folderIcon;

                totalIndex++;

				if (totalIndex == upTo)
					break;

				if (totalIndex >= item.startShowingFrom)
				{
					style = ms_Style.label;

					if (evt.type == EventType.MouseDown && r.Contains(evt.mousePosition))
					{
						if (ChangeLogSelectionRev == m_Changesets[changesetIndex].changeset && m_ChangeLogSelectionGUID == folder.guid && EditorGUI.actionKey)
						{
							ClearItemSelection();
						}
						else
						{
							ChangeLogSelectionRev = m_Changesets[changesetIndex].changeset;
							m_ChangeLogSelectionGUID = folder.guid;
							m_ChangeLogSelectionAssetName = folder.name;
							m_FolderSelected = true;
							m_AssetSelectionIndex = totalIndex;
						}

						m_ChangesetSelectionIndex = changesetIndex;

						GUIUtility.keyboardControl = m_HistoryControlID;

						((ASMainWindow)m_ParentWindow).m_SearchToShow = ASMainWindow.ShowSearchField.HistoryList;

						if (evt.clickCount == 2)
						{
							ShowAssetsHistory();
							GUIUtility.ExitGUI();
						}
						else
							if (evt.button == 1)
							{
								GUIUtility.hotControl = 0;
								r = new Rect(evt.mousePosition.x, evt.mousePosition.y, 1, 1);
								EditorUtility.DisplayCustomMenu(r, m_DropDownMenuItems, -1, ContextMenuClick, null);
							}

						DoScroll();
						evt.Use();
					}

					bool selected = ChangeLogSelectionRev == m_Changesets[changesetIndex].changeset && m_ChangeLogSelectionGUID == folder.guid;

					if (item.boldAssets[totalIndex] && !selected)
						GUI.Label(r, string.Empty, ms_Style.ping);

                    if (Event.current.type == EventType.Repaint)
                    {
                        style.Draw(r, content, false, false, selected, hasFocus);
                        DrawBadge(r, folder.changeFlags, style, content, GUIClip.visibleRect.width - 150);
                    }

					r.y += m_RowHeight;
				}

                ms_Style.label.padding.left += 16;
                ms_Style.boldLabel.padding.left += 16;

                try
                {
                    for (int k = 0; k < folder.files.Length; k++)
                    {
                        totalIndex++;
						if (totalIndex == upTo)
							break;

						if (totalIndex >= item.startShowingFrom)
						{
							style = ms_Style.label;

							if (evt.type == EventType.MouseDown && r.Contains(evt.mousePosition))
							{
								if (ChangeLogSelectionRev == m_Changesets[changesetIndex].changeset && m_ChangeLogSelectionGUID == folder.files[k].guid && EditorGUI.actionKey)
								{
									ClearItemSelection();
								}
								else
								{
									ChangeLogSelectionRev = m_Changesets[changesetIndex].changeset;
									m_ChangeLogSelectionGUID = folder.files[k].guid;
									m_ChangeLogSelectionAssetName = folder.files[k].name;
									m_FolderSelected = false;
									m_AssetSelectionIndex = totalIndex;
								}

								m_ChangesetSelectionIndex = changesetIndex;

								GUIUtility.keyboardControl = m_HistoryControlID;
								((ASMainWindow)m_ParentWindow).m_SearchToShow = ASMainWindow.ShowSearchField.HistoryList;

								if (evt.clickCount == 2)
								{
									if (IsComparableAssetSelected() && m_SelectedGUID == m_ChangeLogSelectionGUID)
									{
										DoShowDiff(false, ChangeLogSelectionRev, -1);
									}
									else
									{
										ShowAssetsHistory();
										GUIUtility.ExitGUI();
									}
								}
								else
									if (evt.button == 1)
									{
										GUIUtility.hotControl = 0;
										r = new Rect(evt.mousePosition.x, evt.mousePosition.y, 1, 1);
										EditorUtility.DisplayCustomMenu(r, m_DropDownMenuItems, -1, ContextMenuClick, null);
									}

								DoScroll();
								evt.Use();
							}

							content.text = folder.files[k].name;
							content.image = InternalEditorUtility.GetIconForFile(folder.files[k].name);

							bool selected = ChangeLogSelectionRev == m_Changesets[changesetIndex].changeset && m_ChangeLogSelectionGUID == folder.files[k].guid;

							if (item.boldAssets[totalIndex] && !selected)
								GUI.Label(r, string.Empty, ms_Style.ping);

                            if (Event.current.type == EventType.Repaint)
                            {
                                style.Draw(r, content, false, false, selected, hasFocus);
                                DrawBadge(r, folder.files[k].changeFlags, style, content, GUIClip.visibleRect.width - 150);
                            }
							r.y += m_RowHeight;
						}
                    }

					if (totalIndex == upTo)
						break;
                }
                finally
                {
                    ms_Style.label.padding.left -= 16;
                    ms_Style.boldLabel.padding.left -= 16;
                }
            }

			if ((totalIndex == upTo || upTo >= item.totalLineCount) && item.collapsedItemCount != 0)
			{
				r.x += 16 + 3;
				if (GUI.Button(r, item.collapsedItemCount.ToString() + " more...", ms_Style.foldout))
				{
					GUIUtility.keyboardControl = m_HistoryControlID;
					UncollapseListItem(ref item);
				}
			}
        }

        int FindFirstUnfilteredItem(int fromIndex, int direction)
        {
            int index = fromIndex;

            while (index >= 0 && index < m_GUIItems.Length)
            {
                if (m_GUIItems[index].inFilter)
                {
                    return index;
                }

                index += direction;
            }

            return -1;
        }

        void MoveSelection(int steps)
        {
            if (m_ChangeLogSelectionGUID == string.Empty)
            {
                int direction = (int)Mathf.Sign(steps);
                steps = (int)Mathf.Abs(steps);
                int index = 0;

                for (int i = 0; i < steps; i++)
                {
                    index = FindFirstUnfilteredItem(m_ChangesetSelectionIndex + direction, direction);
                    if (index == -1)
                        break;

                    m_ChangesetSelectionIndex = index;
                }

                ChangeLogSelectionRev = m_Changesets[m_ChangesetSelectionIndex].changeset;
            }
            else
            {
                m_AssetSelectionIndex += steps;

				if (m_AssetSelectionIndex < m_GUIItems[m_ChangesetSelectionIndex].startShowingFrom)
					m_AssetSelectionIndex = m_GUIItems[m_ChangesetSelectionIndex].startShowingFrom;
                else
                {
                    int count = m_GUIItems[m_ChangesetSelectionIndex].assets.GetLineCount();

					if (m_AssetSelectionIndex >= (kUncollapsedItemsCount - 1) + m_GUIItems[m_ChangesetSelectionIndex].startShowingFrom && 
						m_GUIItems[m_ChangesetSelectionIndex].collapsedItemCount != 0)
					{
						UncollapseListItem(ref m_GUIItems[m_ChangesetSelectionIndex]);
					}

					if (m_AssetSelectionIndex >= count)
						m_AssetSelectionIndex = count - 1;
                }

                int folderI = 0, fileI = 0;
                if (m_GUIItems[m_ChangesetSelectionIndex].assets.IndexToFolderAndFile(m_AssetSelectionIndex, ref folderI, ref fileI))
                {
                    if (fileI == -1)
                        m_ChangeLogSelectionGUID = m_GUIItems[m_ChangesetSelectionIndex].assets.folders[folderI].guid;
                    else
                        m_ChangeLogSelectionGUID = m_GUIItems[m_ChangesetSelectionIndex].assets.folders[folderI].files[fileI].guid;
                }
            }
        }

        void HandleWebLikeKeyboard()
        {
            Event evt = Event.current;

            if (evt.GetTypeForControl(m_HistoryControlID) == EventType.KeyDown && m_GUIItems.Length != 0)
            {
                switch (evt.keyCode)
                {
                    case KeyCode.KeypadEnter:
                    case KeyCode.Return:
                        if (IsComparableAssetSelected())
                            DoShowDiff(false, ChangeLogSelectionRev, -1);
                        break;
                    case KeyCode.UpArrow:
                        MoveSelection(-1);
                        break;
                    case KeyCode.DownArrow:
                        MoveSelection(1);
                        break;
                    case KeyCode.LeftArrow:
                        m_ChangeLogSelectionGUID = string.Empty;
                        break;
                    case KeyCode.RightArrow:
                        if (m_ChangeLogSelectionGUID == string.Empty && m_GUIItems.Length > 0)
                        {
                            m_ChangeLogSelectionGUID = m_GUIItems[m_ChangesetSelectionIndex].assets.folders[0].guid;
                            m_ChangeLogSelectionAssetName = m_GUIItems[m_ChangesetSelectionIndex].assets.folders[0].name;
                            m_FolderSelected = true;
                            m_AssetSelectionIndex = 0;
                        }
                        break;
                    case KeyCode.Home:
                        if (m_ChangeLogSelectionGUID == string.Empty)
                        {
                            int index = FindFirstUnfilteredItem(0, 1);
                            if (index != -1)
                                m_ChangesetSelectionIndex = index;
                            ChangeLogSelectionRev = m_Changesets[m_ChangesetSelectionIndex].changeset;
                        }
                        else
                        {
                            MoveSelection(kFirst);
                        }
                        break;
                    case KeyCode.End:
                        if (m_ChangeLogSelectionGUID == string.Empty)
                        {
                            int index = FindFirstUnfilteredItem(m_GUIItems.Length - 1, -1);
                            if (index != -1)
                                m_ChangesetSelectionIndex = index;
                            ChangeLogSelectionRev = m_Changesets[m_ChangesetSelectionIndex].changeset;
                        }
                        else
                        {
                            MoveSelection(kLast);
                        }
                        break;
                    case KeyCode.PageUp:
						if (Application.platform == RuntimePlatform.OSXEditor)
                        {
                            m_ScrollPos.y -= m_ScrollViewHeight;

                            if (m_ScrollPos.y < 0)
                                m_ScrollPos.y = 0;
                        }
                        else
                        {
                            MoveSelection(-Mathf.RoundToInt(m_ScrollViewHeight / m_RowHeight));
                        }
                        break;
                    case KeyCode.PageDown:
						if (Application.platform == RuntimePlatform.OSXEditor)
                        {
                            m_ScrollPos.y += m_ScrollViewHeight;
                        }
                        else
                        {
                            MoveSelection(Mathf.RoundToInt(m_ScrollViewHeight / m_RowHeight));
                        }
                        break;
                    default:
                        return;
                }

                DoScroll();
                evt.Use();
            }

        }

        void WebLikeHistory(bool hasFocus)
        {
            if (m_Changesets == null)
            {
                m_Changesets = new Changeset[0];
            }

			if (m_GUIItems == null)
				return;

            m_HistoryControlID = GUIUtility.GetControlID(ms_HistoryControlHash, FocusType.Native);

            HandleWebLikeKeyboard();

            Event evt = Event.current;
            EventType t = evt.GetTypeForControl(m_HistoryControlID);

            if (t == EventType.ValidateCommand)
            {
                evt.Use();
                return;
            }

			GUILayout.Space(1);

            m_ScrollPos = GUILayout.BeginScrollView(m_ScrollPos);

            int currentHeight = 0;

			GUILayoutUtility.GetRect(1, m_TotalHeight - 1);

			if ((evt.type == EventType.Repaint || evt.type == EventType.MouseDown || evt.type == EventType.MouseUp) && m_GUIItems != null)
            {
                Rect r;

                for (int i = 0; i < m_Changesets.Length; i++)
                {
                    if (m_GUIItems[i].inFilter)
                    {
						if ((currentHeight + m_GUIItems[i].height > GUIClip.visibleRect.y) && currentHeight < GUIClip.visibleRect.yMax)
                        {
							float descriptionHeight = ms_Style.descriptionLabel.CalcHeight(m_GUIItems[i].colDescription, float.MaxValue);

							if (evt.type == EventType.Repaint)
							{
                                if (ChangeLogSelectionRev == m_Changesets[i].changeset && Event.current.type == EventType.Repaint)
								{
									r = new Rect(0, currentHeight, GUIClip.visibleRect.width, m_GUIItems[i].height - 10);
									ms_Style.selected.Draw(r, false, false, false, false);
								}

								r = new Rect(0, currentHeight + 3, GUIClip.visibleRect.width, m_GUIItems[i].height);
								GUI.Label(r, m_GUIItems[i].colAuthor, ms_Style.boldLabel);

								r = new Rect(GUIClip.visibleRect.width - 160, currentHeight + 3, 60, m_GUIItems[i].height);
								GUI.Label(r, m_GUIItems[i].colRevision, ms_Style.boldLabel);

								r.x += 60;
								r.width = 100;
								GUI.Label(r, m_GUIItems[i].colDate, ms_Style.boldLabel);

								r.x = ms_Style.boldLabel.margin.left;
								r.y += m_RowHeight;
								r.width = GUIClip.visibleRect.width;
								r.height = descriptionHeight;
								GUI.Label(r, m_GUIItems[i].colDescription, ms_Style.descriptionLabel);
								r.y += descriptionHeight;
							}

							r = new Rect(0, currentHeight + descriptionHeight + m_RowHeight, GUIClip.visibleRect.width, m_GUIItems[i].height - descriptionHeight - m_RowHeight);

                            DrawParentView(r, ref m_GUIItems[i], i, hasFocus);

                            if (evt.type == EventType.MouseDown)
                            {
								r = new Rect(0, currentHeight, GUIClip.visibleRect.width, m_GUIItems[i].height - 10);
                                if (r.Contains(evt.mousePosition))
                                {
                                    ChangeLogSelectionRev = m_Changesets[i].changeset;
                                    m_ChangesetSelectionIndex = i;

                                    GUIUtility.keyboardControl = m_HistoryControlID;
									((ASMainWindow)m_ParentWindow).m_SearchToShow = ASMainWindow.ShowSearchField.HistoryList;

                                    if (evt.button == 1)
                                    {
                                        GUIUtility.hotControl = 0;
                                        r = new Rect(evt.mousePosition.x, evt.mousePosition.y, 1, 1);
                                        EditorUtility.DisplayCustomMenu(r, m_DropDownChangesetMenuItems, -1, ChangesetContextMenuClick, null);
                                        Event.current.Use();
                                    }

                                    DoScroll(); 
                                    evt.Use();
                                }
                            }
                        }

                        currentHeight += m_GUIItems[i].height;
                    }
                }
            }
            else
            if (m_GUIItems == null)
            {
                GUILayout.Label(EditorGUIUtility.TextContent("This item is not yet committed to the Asset Server"));
            }

            if (Event.current.type == EventType.Repaint)
                m_ScrollViewHeight = (int)GUIClip.visibleRect.height;

            GUILayout.EndScrollView();
        }

        void DoScroll()
        {
            int height = 0;
            int i;
            for (i = 0; i < m_ChangesetSelectionIndex; i++)
            {
                if (m_GUIItems[i].inFilter)
                    height += m_GUIItems[i].height;
            }

            float scrollTop;
            float scrollBottom;

            if (m_ChangeLogSelectionGUID != string.Empty)
            {
                scrollTop = height + (2 + m_AssetSelectionIndex) * m_RowHeight + 5;
                scrollBottom = scrollTop - m_ScrollViewHeight + m_RowHeight;
            }
            else
            {
                scrollTop = height;
                scrollBottom = scrollTop - m_ScrollViewHeight + m_GUIItems[i].height - 10;
            }

            m_ScrollPos.y = Mathf.Clamp(m_ScrollPos.y, scrollBottom, scrollTop);
        }

        public bool DoGUI(bool hasFocus)
        {
			bool wasGUIEnabled = GUI.enabled;

            if (ms_Style == null)
            {
                ms_Style = new Constants();

				ms_Style.entryEven = new GUIStyle(ms_Style.entryEven);
				ms_Style.entryEven.padding.left = 3;
				ms_Style.entryOdd = new GUIStyle(ms_Style.entryOdd);
				ms_Style.entryOdd.padding.left = 3;

				ms_Style.label = new GUIStyle(ms_Style.label);
				ms_Style.boldLabel = new GUIStyle(ms_Style.boldLabel);

				ms_Style.label.padding.left = 3;
				ms_Style.boldLabel.padding.left = 3;
				ms_Style.boldLabel.padding.top = 0;
				ms_Style.boldLabel.padding.bottom = 0;

				DoLocalSelectionChange();
			}

            EditorGUIUtility.SetIconSize(ms_IconSize);

            if (Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Escape)
            {
                CancelShowCustomDiff();
                Event.current.Use();
            }

            SplitterGUILayout.BeginHorizontalSplit(m_HorSplit);

            GUILayout.BeginVertical();

            Rect rect = GUILayoutUtility.GetRect(0, 0, GUILayout.ExpandWidth(true), GUILayout.ExpandHeight(true));
            m_FileViewWin.DoGUI(this, rect, hasFocus);

            GUILayout.EndVertical();

            GUILayout.BeginVertical();

            WebLikeHistory(hasFocus);

            GUILayout.EndVertical();

            SplitterGUILayout.EndHorizontalSplit();

			if (Event.current.type == EventType.Repaint)
			{
				Handles.color = Color.black;
				Handles.DrawLine(new Vector3(m_HorSplit.realSizes[0] - 1, rect.y, 0), new Vector3(m_HorSplit.realSizes[0] - 1, rect.yMax, 0));
				Handles.DrawLine(new Vector3(0, rect.yMax, 0), new Vector3(Screen.width, rect.yMax, 0));
			}

            GUILayout.BeginHorizontal();

            GUI.enabled = m_FileViewWin.SelType == ASHistoryFileView.SelectionType.DeletedItems && wasGUIEnabled;

            if (GUILayout.Button(EditorGUIUtility.TextContent("Recover"), ms_Style.button))
            {
                m_FileViewWin.DoRecover();
            }

            GUILayout.FlexibleSpace();

            if (m_InRevisionSelectMode)
            {
                GUI.enabled = wasGUIEnabled;
                GUILayout.Label(EditorGUIUtility.TextContent("Select revision to compare to"), ms_Style.boldLabel);
            }

            GUILayout.Space(10);

			#region For asset server testing
			/*
			GUI.enabled = true;

			if (GUILayout.Button("Export"))
			{
				System.IO.StreamWriter sw = new System.IO.StreamWriter("c:/temp/history.txt");

				for (int i = m_GUIItems.Length - 1; i >= 0; i--)
				{
					sw.WriteLine(m_GUIItems[i].colAuthor.text + " " + m_GUIItems[i].colRevision.text + " " + m_GUIItems[i].colDescription.text);

					for (int l = 0; l < m_GUIItems[i].assets.folders.Length; l++)
					{
						sw.WriteLine(m_GUIItems[i].assets.folders[l].name + " " + m_GUIItems[i].assets.folders[l].guid);
						for (int k = 0; k < m_GUIItems[i].assets.folders[l].files.Length; k++)
						{
							sw.WriteLine("\t" + m_GUIItems[i].assets.folders[l].files[k].name + " " + m_GUIItems[i].assets.folders[l].files[k].guid);
						}
					}
				}

				sw.Close();
			}
			*/
			#endregion

			GUI.enabled = IsComparableAssetSelected() && wasGUIEnabled;

			if (GUILayout.Button(EditorGUIUtility.TextContent("Compare to Local Version"), ms_Style.button))
            {
                DoShowDiff(false, ChangeLogSelectionRev, -1);
                GUIUtility.ExitGUI();
            }

            GUI.enabled = ChangeLogSelectionRev > 0 && m_ChangeLogSelectionGUID != string.Empty && wasGUIEnabled;

			if (GUILayout.Button(EditorGUIUtility.TextContent("Download Selected File"), ms_Style.button))
            {
                DownloadFile();
            }

            GUILayout.Space(10);

            GUI.enabled = ChangeLogSelectionRev > 0 && wasGUIEnabled;

			//TODO: localize this
			if (GUILayout.Button(ChangeLogSelectionRev > 0 ? "Revert Entire Project to " + ChangeLogSelectionRev : "Revert Entire Project", ms_Style.button))
            {
                DoRevertProject();
            }

            GUI.enabled = wasGUIEnabled;

            GUILayout.EndHorizontal();

            GUILayout.Space(10);

            if (!m_SplittersOk && Event.current.type == EventType.Repaint)
            {
                m_SplittersOk = true;
                HandleUtility.Repaint();
            }

            EditorGUIUtility.SetIconSize(Vector2.zero);

            return true;
        }
    }
}
