using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Collections;
using UnityEditorInternal;

namespace UnityEditor
{
	internal class ASMainWindow : EditorWindow, IHasCustomMenu
	{
		#region Locals
		internal class Constants
		{
			public GUIStyle background = "OL Box";
			public GUIStyle groupBox;
			public GUIStyle groupBoxNoMargin;
			public GUIStyle contentBox = "GroupBox";
			public GUIStyle entrySelected = "ServerUpdateChangesetOn";
			public GUIStyle entryNormal = "ServerUpdateChangeset";
			public GUIStyle element = "OL elem";
			public GUIStyle header = "OL header";
			public GUIStyle title = "OL Title";
			public GUIStyle columnHeader = "OL Title";
			public GUIStyle serverUpdateLog = "ServerUpdateLog";
			public GUIStyle serverUpdateInfo = "ServerUpdateInfo";
			public GUIStyle smallButton = "Button";
			public GUIStyle errorLabel = "ErrorLabel";
			
			public GUIStyle miniButton = "MiniButton";
			public GUIStyle button = "Button";
			public GUIStyle largeButton = "ButtonMid";
			public GUIStyle bigButton = "LargeButton";
			
			public GUIStyle entryEven = "CN EntryBackEven";
			public GUIStyle entryOdd = "CN EntryBackOdd";
			public GUIStyle dropDown = "MiniPullDown";
			public GUIStyle toggle = "Toggle";
			
			public Vector2 toggleSize;
			
			public Constants ()
			{
				// @TODO: Make new styles for these things.
				
				groupBoxNoMargin = new GUIStyle ();
				
				groupBox = new GUIStyle ();
				groupBox.margin = new RectOffset (10, 10, 10, 10);
				
				contentBox = new GUIStyle (contentBox);
				contentBox.margin = new RectOffset (0, 0, 0, 0);
				contentBox.overflow = new RectOffset (0, 1, 0, 1);
				contentBox.padding = new RectOffset (8, 8, 7, 7);
				
				title = new GUIStyle (title);
				title.padding.left = title.padding.right = contentBox.padding.left+2;
				
				background = new GUIStyle (background);
				background.padding.top = 1;
			}
		}

		static Constants constants = null;

		public static GUIContent badgeDelete = EditorGUIUtility.IconContent("AS Badge Delete");
		public static GUIContent badgeMove = EditorGUIUtility.IconContent("AS Badge Move");
		public static GUIContent badgeNew = EditorGUIUtility.IconContent("AS Badge New");

		public AssetsItem[] sharedCommits = null;
		public AssetsItem[] sharedDeletedItems = null;
		public Changeset[] sharedChangesets = null;
		GUIContent[] changesetContents = null;

		public enum ShowSearchField { None, ProjectView, HistoryList }
		public ShowSearchField m_ShowSearch = ShowSearchField.None; // this is used in history list (this file), and in ASHistoryFileView.cs
		public ShowSearchField m_SearchToShow = ShowSearchField.HistoryList; // which side last had selection, so we know where to put search field when "Find" event comes

		public SearchField m_SearchField = new SearchField();

		internal enum Page { NotInitialized = -1, Overview, Update, Commit, // main pages
			History, ServerConfig, Admin }

		const Page lastMainPage = Page.Commit;

		string[] pageTitles = new string[] { "Overview", "Update", "Commit", "" };

		string[] dropDownMenuItems = new string[] { "Connection", "", "Show History", "Discard Changes", "", "Server Administration" };
		string[] unconfiguredDropDownMenuItems = new string[] { "Connection", "", "Server Administration" };

		string[] commitDropDownMenuItems = new string[] { "Commit", "", "Compare", "Compare Binary", "", "Discard" }; // change CommitContextMenuClick function when changing this

		bool needsSetup = true;
		public bool NeedsSetup
		{
			get { return needsSetup; }
			set { needsSetup = value; }
		}

		string connectionString = "";

		int maxNickLength = 1;
		bool showSmallWindow = false;
		int widthToHideButtons = 591;
		bool wasHidingButtons = false;

		Page selectedPage = Page.NotInitialized;

		ListViewState lv = new ListViewState(0);
		ParentViewState pv = new ParentViewState();

		internal ASHistoryWindow asHistoryWin;
		internal ASUpdateWindow asUpdateWin;
		internal ASCommitWindow asCommitWin;
		internal ASServerAdminWindow asAdminWin;
		internal ASConfigWindow asConfigWin;

		bool error = false;
		bool isInitialUpdate = false;

		Vector2 iconSize = new Vector2(16, 16);

		SplitterState splitter = new SplitterState(new float[] { 50,50 }, new int[] { 80, 80 }, null);

		public bool Error { get { return error; } }

		// Quick commit
		bool committing = false;
		bool selectionChangedWhileCommitting = false;
		string commitMessage = string.Empty;
		bool pvHasSelection = false;
		bool somethingDiscardableSelected = false;
		bool mySelection = false;
		bool focusCommitMessage = false;
		int lastRevertSelectionChanged = -1;

		// check if user has asset server license
		bool m_CheckedMaint = false;

		[System.Serializable]
		public class SearchField
		{
			string m_FilterText = string.Empty;
			bool m_Show = false;

			public string FilterText { get { return m_FilterText; } }
			public bool Show
			{
				get
				{
					return m_Show;
				}
				set
				{
					m_Show = value;
				}
			}

			// returns if filter has changed
			public bool DoGUI()
			{
				GUI.SetNextControlName("SearchFilter");
				string searchFilter = EditorGUILayout.ToolbarSearchField(m_FilterText);

				if (m_FilterText != searchFilter)
				{
					m_FilterText = searchFilter;
					return true;	
				}

				return false;
			}
		}
		#endregion

		#region Misc
		public ASMainWindow()
		{
			position = new Rect(50, 50, 800, 600);
		}

		public void LogError(string errorStr)
		{
			Debug.LogError(errorStr);
			AssetServer.SetAssetServerError(errorStr, false);
			error = true;
		}
		
		void Awake ()
		{
			pv.lv = new ListViewState(0);
			//SwitchSelectedPage(Page.Configure);
			isInitialUpdate = true;
		}

		void NotifyClosingCommit()
		{
			if (asCommitWin != null)
				asCommitWin.OnClose();
		}

		void OnDestroy()
		{
			sharedCommits = null;
			sharedDeletedItems = null;
			sharedChangesets = null;
			changesetContents = null;
			
			if (selectedPage == Page.Commit)
			{
				NotifyClosingCommit();
			}
		}

		private void DoSelectionChange()
		{
			if (committing)
			{
				selectionChangedWhileCommitting = true;
				return;
			}

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

			pvHasSelection = ASCommitWindow.MarkSelected(pv, guids);
		}

		void OnSelectionChange()
		{
			switch (selectedPage)
			{
				case Page.Overview:
					if (!mySelection)
					{
						DoSelectionChange();
						Repaint();
					}
					else
						mySelection = false; // don't reselect when this window changes selection

					somethingDiscardableSelected = ASCommitWindow.SomethingDiscardableSelected(pv);
					break;
				case Page.Update:
					asUpdateWin.OnSelectionChange();
					break;
				case Page.Commit:
					asCommitWin.OnSelectionChange();
					break;
				case Page.History:
					asHistoryWin.OnSelectionChange();
					break;
			}
		}



		internal void Reinit()
		{
			SwitchSelectedPage(Page.Overview);
			Repaint();
		}

		public void DoDiscardChanges(bool lastActionsResult)
		{
			List<string> assetGuids = new List<string>();

			bool useSelection = false;

			if (useSelection)
			{
				assetGuids.AddRange(AssetServer.CollectDeepSelection());
			}
			else
			{
				assetGuids.AddRange(AssetServer.GetAllRootGUIDs());
				assetGuids.AddRange(AssetServer.CollectAllChildren(AssetServer.GetRootGUID(), AssetServer.GetAllRootGUIDs()));
			}

			if (assetGuids.Count == 0)
				assetGuids.AddRange(AssetServer.GetAllRootGUIDs());

			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBReinitOnSuccess");
			AssetServer.DoUpdateWithoutConflictResolutionOnNextTick(assetGuids.ToArray());
		}

		bool WordWrappedLabelButton(string label, string buttonText)
		{
			GUILayout.BeginHorizontal();

			GUILayout.Label(label, EditorStyles.wordWrappedLabel);

			bool retval = GUILayout.Button(buttonText, GUILayout.Width(110));

			GUILayout.EndHorizontal();
			return retval;
		}

		bool ToolbarToggle(bool pressed, string title, GUIStyle style)
		{
			bool changed = GUI.changed;
			GUI.changed = false;

			GUILayout.Toggle(pressed, title, style);
			if (GUI.changed)
				return true;

			GUI.changed |= changed;

			return false;
		}

		bool RightButton(string title) { return RightButton(title, constants.smallButton); }
		bool RightButton(string title, GUIStyle style)
		{
			GUILayout.BeginHorizontal();
			GUILayout.FlexibleSpace();
			bool retval = GUILayout.Button(title, style);
			GUILayout.EndHorizontal();
			return retval;
		}

		public void ShowConflictResolutions(string[] conflicting)
		{
			if (asUpdateWin == null)
			{
				LogError("Found unexpected conflicts. Please use Bug Reporter to report a bug.");
				return;
			}

			asUpdateWin.ShowConflictResolutions(conflicting);
		}

		public virtual void AddItemsToMenu(GenericMenu menu)
		{
			if (!needsSetup)
			{
				menu.AddItem(new GUIContent("Refresh"), false, ActionRefresh);
				menu.AddSeparator("");
			}

			menu.AddItem(new GUIContent("Connection"), false, ActionSwitchPage, Page.ServerConfig);
			menu.AddSeparator("");

			if (!needsSetup)
			{
				menu.AddItem(new GUIContent("Show History"), false, ActionSwitchPage, Page.History);
				menu.AddItem(new GUIContent("Discard Changes"), false, ActionDiscardChanges);
				menu.AddSeparator("");
			}

			menu.AddItem(new GUIContent("Server Administration"), false, ActionSwitchPage, Page.Admin);
		}

		public bool UpdateNeedsRefresh()
		{
			return sharedChangesets == null || AssetServer.GetRefreshUpdate();
		}

		public bool CommitNeedsRefresh()
		{
			return sharedCommits == null || sharedDeletedItems == null || AssetServer.GetRefreshCommit();
		}

		void ContextMenuClick(object userData, string[] options, int selected)
		{
			if (selected >= 0)
			{
				switch (dropDownMenuItems[selected])
				{
					case "Connection": ActionSwitchPage(Page.ServerConfig); break;
					case "Show History": ActionSwitchPage(Page.History); break;
					case "Discard Changes": ActionDiscardChanges(); break;
					case "Server Administration": ActionSwitchPage(Page.Admin); break;
				}
			}
		}

		void CommitContextMenuClick(object userData, string[] options, int selected)
		{
			if (selected >= 0)
			{
				switch (commitDropDownMenuItems[selected])
				{
					case "Commit": StartCommitting(); break;
					case "Compare": ASCommitWindow.DoShowDiff(ASCommitWindow.GetParentViewSelectedItems(pv, false, false), false); break;
					case "Compare Binary": ASCommitWindow.DoShowDiff(ASCommitWindow.GetParentViewSelectedItems(pv, false, false), true); break;
					case "Discard": DoMyRevert(false); break;
				}
			}
		}

		public void CommitItemsChanged()
		{
			InitCommits();
			DisplayedItemsChanged();

			if (selectedPage == Page.Commit)
			{
				asCommitWin.Update();
			}

			Repaint();
		}

		public void RevertProject(int toRevision, Changeset[] changesets)
		{
			AssetServer.SetStickyChangeset(toRevision);
			asUpdateWin = new ASUpdateWindow(this, changesets);
			asUpdateWin.SetSelectedRevisionLine(0);
			asUpdateWin.DoUpdate(false);
			selectedPage = Page.Update;
		}

		public void ShowHistory()
		{
			SwitchSelectedPage(Page.Overview); // load window
			isInitialUpdate = false;
			SwitchSelectedPage(Page.History);
		}

		#endregion

		#region Action functions
		void ActionRefresh()
		{
			switch (selectedPage)
			{
				case Page.Commit:
					asCommitWin.InitiateReinit();
					break;
				case Page.Update:
					AssetServer.CheckForServerUpdates();
					if (UpdateNeedsRefresh()) // if only commit needs refresh there is no point in doing status update
						InitiateUpdateStatusWithCallback("CBInitUpdatePage");
					break;
				case Page.History:
					//SwitchSelectedPage(selectedPage);
					AssetServer.CheckForServerUpdates();
					if (UpdateNeedsRefresh()) // if only commit needs refresh there is no point in doing status update
						InitiateUpdateStatusWithCallback("CBInitHistoryPage");
					break;
				case Page.Overview:
					AssetServer.CheckForServerUpdates();

					//if (UpdateNeedsRefresh()) // if only commit needs refresh there is no point in doing status update
					//	InitiateUpdateStatusWithCallback("CBInitUpdatePage");

					InitiateRefreshAssetsAndUpdateStatusWithCallback("CBInitOverviewPage");
					break;
				default:
					Reinit();
					break;
			}
		}

		void ActionSwitchPage(object page)
		{
			SwitchSelectedPage((Page)page);
		}

		void ActionDiscardChanges()
		{
			if (EditorUtility.DisplayDialog("Discard all changes", "Are you sure you want to discard all local changes made in the project?", "Discard", "Cancel"))
			{
				AssetServer.RemoveMaintErrorsFromConsole();

				if (!ASEditorBackend.SettingsIfNeeded())
				{
					Debug.Log("Asset Server connection for current project is not set up");
					error = true;
					return;
				}

				error = false;
				AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBDoDiscardChanges");
				AssetServer.DoUpdateStatusOnNextTick(); // must update status before discarding.
			}
		}
		#endregion

		#region Selected page changed
		void SwitchSelectedPage(Page page)
		{
			Page prevPage = selectedPage;
			selectedPage = page;
			SelectedPageChanged();

			if (error)
			{
				selectedPage = prevPage;
				error = false;
			}
		}

		void InitiateUpdateStatusWithCallback(string callbackName)
		{
			if (!ASEditorBackend.SettingsIfNeeded())
			{
				Debug.Log("Asset Server connection for current project is not set up");
				error = true;
				return;
			}

			error = false;
			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", callbackName);
			AssetServer.DoUpdateStatusOnNextTick();
		}

		void InitiateRefreshAssetsWithCallback(string callbackName)
		{
			if (!ASEditorBackend.SettingsIfNeeded())
			{
				Debug.Log("Asset Server connection for current project is not set up");
				error = true;
				return;
			}

			error = false;
			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", callbackName);
			AssetServer.DoRefreshAssetsOnNextTick();
		}

		// Now that's a descriptive name :)
		void InitiateRefreshAssetsAndUpdateStatusWithCallback(string callbackName)
		{
			if (!ASEditorBackend.SettingsIfNeeded())
			{
				Debug.Log("Asset Server connection for current project is not set up");
				error = true;
				return;
			}

			error = false;
			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", callbackName);
			AssetServer.DoRefreshAssetsAndUpdateStatusOnNextTick();
		}

		void SelectedPageChanged()
		{
			AssetServer.ClearAssetServerError();

			if (committing)
				CancelCommit();

			switch (selectedPage)
			{
				case Page.Update:
					//AssetServer.CheckForServerUpdates();

					//if (UpdateNeedsRefresh()) // if only commit needs refresh there is no point in doing status update
					//	InitiateUpdateStatusWithCallback("CBInitUpdatePage");
					//else
						InitUpdatePage(true);
					break;
				case Page.Commit:
					asCommitWin = new ASCommitWindow(this, pvHasSelection ? ASCommitWindow.GetParentViewSelectedItems(pv, true, false).ToArray() : null);
					asCommitWin.InitiateReinit();
					break;
				case Page.History:
					pageTitles[3] = "History";

					//AssetServer.CheckForServerUpdates();

					//if (UpdateNeedsRefresh())
					//	InitiateUpdateStatusWithCallback("CBInitHistoryPage");
					//else
						InitHistoryPage(true);
					break;
				case Page.Overview:
					if (ASEditorBackend.SettingsAreValid())
					{
						AssetServer.CheckForServerUpdates();

						if (UpdateNeedsRefresh()) // if only commit needs refresh there is no point in doing status update
							InitiateUpdateStatusWithCallback("CBInitOverviewPage");
						else
							//InitiateRefreshAssetsWithCallback("CBInitOverviewPage");
							InitOverviewPage(true);
					}
					else
					{
						connectionString = "Asset Server connection for current project is not set up";
						sharedChangesets = new Changeset[0];
						changesetContents = new GUIContent[0];
						needsSetup = true;
					}
					break;
				case Page.ServerConfig:
					pageTitles[3] = "Connection";
					asConfigWin = new ASConfigWindow(this);
					break;
				case Page.Admin:
					pageTitles[3] = "Administration";

					asAdminWin = new ASServerAdminWindow(this);

					if (error)
						return;
					break;
			}
		}

		public void InitUpdatePage(bool lastActionsResult)
		{
			if (!lastActionsResult)
			{
				Reinit();
				return;
			}

			if (UpdateNeedsRefresh())
			{
				GetUpdates();
			}

			if (sharedChangesets == null)
			{
				Reinit();
				return;
			}

			asUpdateWin = new ASUpdateWindow(this, sharedChangesets);
			asUpdateWin.SetSelectedRevisionLine(0);
		}

		private void InitCommits()
		{
			if (CommitNeedsRefresh())
			{
				if (AssetServer.GetAssetServerError() == string.Empty)
				{
					sharedCommits = ASCommitWindow.GetCommits();
					sharedDeletedItems = AssetServer.GetLocalDeletedItems();
				}
				else
				{
					sharedCommits = new AssetsItem[0];
					sharedDeletedItems = new AssetsItem[0];
				}
			}

			pv.Clear();
			pv.AddAssetItems(sharedCommits);
			pv.AddAssetItems(sharedDeletedItems);
			pv.SetLineCount();

			pv.selectedItems = new bool[pv.lv.totalRows];
			pv.initialSelectedItem = -1;
			AssetServer.ClearRefreshCommit();
		}

		private void GetUpdates()
		{
			AssetServer.ClearAssetServerError();

			sharedChangesets = AssetServer.GetNewItems();
			System.Array.Reverse(sharedChangesets);
			changesetContents = null;
			maxNickLength = 1;

			AssetServer.ClearRefreshUpdate();

			if (AssetServer.GetAssetServerError() != string.Empty)
			{
				sharedChangesets = null;
			}
		}

		public void DisplayedItemsChanged()
		{
			float[] split = new float[2];
			bool updateHasItems = sharedChangesets != null && sharedChangesets.Length != 0;
			bool commitHasItems = pv.lv.totalRows != 0;

			if ((updateHasItems && commitHasItems) || (!updateHasItems && !commitHasItems))
			{
				split[0] = split[1] = 0.5f;
			}
			else
			{
				split[0] = updateHasItems ? 1 : 0;
				split[1] = commitHasItems ? 1 : 0;
			}

			splitter = new SplitterState(split, new int[] { 80, 80 }, null);

			DoSelectionChange();
		}

		public void InitOverviewPage(bool lastActionsResult)
		{
			if (!lastActionsResult)
			{
				needsSetup = true;
				sharedChangesets = null;
				sharedCommits = null;
				sharedDeletedItems = null;
				return;
			}
			PListConfig plc = new PListConfig(ASEditorBackend.kServerSettingsFile);
			connectionString = plc[ASEditorBackend.kUserName] + " @ " + plc[ASEditorBackend.kServer] + " : " + plc[ASEditorBackend.kProjectName];

			if (UpdateNeedsRefresh())
			{
				GetUpdates();
			}

			needsSetup = sharedChangesets == null || AssetServer.HasConnectionError();

			InitCommits();

			DisplayedItemsChanged();
		}

		public void InitHistoryPage(bool lastActionsResult)
		{
			if (!lastActionsResult)
			{
				Reinit();
				return;
			}

			asHistoryWin = new ASHistoryWindow(this);

			if (asHistoryWin == null)
			{
				Reinit();
				return;
			}
		}
		#endregion

		#region Page GUIs
		void OverviewPageGUI()
		{
			bool wasGUIEnabled = GUI.enabled;
			showSmallWindow = position.width <= widthToHideButtons;
			
			if (Event.current.type == EventType.Layout)
			{
				wasHidingButtons = showSmallWindow;
			}else
			{
				if (showSmallWindow != wasHidingButtons)
					GUIUtility.ExitGUI();
			}
		
			GUILayout.BeginHorizontal ();

			if (!showSmallWindow)
			{
				GUILayout.BeginVertical();
				ShortServerInfo();

				if (needsSetup)
					GUI.enabled = false;

				OtherServerCommands();

				GUI.enabled = wasGUIEnabled;
				ServerAdministration();

				GUI.enabled = !needsSetup && wasGUIEnabled;

				GUILayout.EndVertical();

				GUILayout.BeginHorizontal(GUILayout.Width((position.width - 30) / 2));
			}
			else
			{
				GUILayout.BeginHorizontal();
			}

			GUI.enabled = !needsSetup && wasGUIEnabled;

			SplitterGUILayout.BeginVerticalSplit(splitter);
			ShortUpdateList ();
			ShortCommitList ();
			SplitterGUILayout.EndVerticalSplit();

			GUILayout.EndHorizontal();

			GUILayout.EndHorizontal ();

			GUI.enabled = wasGUIEnabled;
		}

		void OtherServerCommands ()
		{
			GUILayout.BeginVertical (constants.groupBox);
			GUILayout.Label("Asset Server Actions", constants.title);
			GUILayout.BeginVertical (constants.contentBox);

			if (WordWrappedLabelButton ("Browse the complete history of the project", "Show History"))
			{
				SwitchSelectedPage(Page.History);
				GUIUtility.ExitGUI();
			}

			GUILayout.Space(5);

			if (WordWrappedLabelButton("Discard all local changes you made to the project", "Discard Changes"))
			{
				ActionDiscardChanges();
			}

			GUILayout.EndVertical ();
			GUILayout.EndVertical ();
		}

		void ShortServerInfo () 
		{
			GUILayout.BeginVertical (constants.groupBox);
			GUILayout.Label("Current Project", constants.title);
			GUILayout.BeginVertical (constants.contentBox);

			if (WordWrappedLabelButton(connectionString, "Connection"))
			{
				SwitchSelectedPage(Page.ServerConfig);
			}

			if (AssetServer.GetAssetServerError() != string.Empty)
			{
				GUILayout.Space(10);
				GUILayout.Label(AssetServer.GetAssetServerError(), constants.errorLabel);
			}

			GUILayout.EndVertical ();
			GUILayout.EndVertical ();
		}

		void ServerAdministration()
		{
			GUILayout.BeginVertical(constants.groupBox);
			GUILayout.Label("Asset Server Administration", constants.title);
			GUILayout.BeginVertical(constants.contentBox);

			if (WordWrappedLabelButton("Create and administer Asset Server projects", "Administration"))
			{
				SwitchSelectedPage(Page.Admin);
				GUIUtility.ExitGUI();
			}

			GUILayout.EndVertical();
			GUILayout.EndVertical();
		}

		private bool HasFlag(ChangeFlags flags, ChangeFlags flagToCheck) { return ((int)flagToCheck & (int)flags) != 0; }

		void MySelectionToGlobalSelection()
		{
			mySelection = true;

			List<string> items = ASCommitWindow.GetParentViewSelectedItems(pv, true, false);
			items.Remove(AssetServer.GetRootGUID());

			if (items.Count > 0)
				AssetServer.SetSelectionFromGUID(items[0]);

			pvHasSelection = pv.HasTrue();
			somethingDiscardableSelected = ASCommitWindow.SomethingDiscardableSelected(pv);
		}

		void DoCommitParentView()
		{
			//TODO: refactor this and commit windows one?
			ChangeFlags flags;

			bool shiftIsDown = Event.current.shift;
			bool ctrlIsDown = EditorGUI.actionKey;

			ParentViewFolder folder;

			int prevSelectedRow = pv.lv.row;
			int folderI = -1, fileI = -1;
			bool selChanged = false;

			foreach (ListViewElement el in ListViewGUILayout.ListView(pv.lv, constants.background))
			{
				if ((GUIUtility.keyboardControl == pv.lv.ID) && Event.current.type == EventType.KeyDown && ctrlIsDown)
					Event.current.Use();

				if (folderI == -1)
					if (!pv.IndexToFolderAndFile(el.row, ref folderI, ref fileI))
						break;

				folder = pv.folders[folderI];

				if (pv.selectedItems[el.row] && Event.current.type == EventType.Repaint)
				{
					constants.entrySelected.Draw(el.position, false, false, false, false);
				}

				if (!committing)
				{
					if (ListViewGUILayout.HasMouseUp(el.position))
					{
						if (!shiftIsDown && !ctrlIsDown)
							selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, pv.lv.row, ref pv.initialSelectedItem, ref pv.selectedItems);
					} else
						if (ListViewGUILayout.HasMouseDown(el.position))
					{
						if (Event.current.clickCount == 2)
						{
							ASCommitWindow.DoShowDiff(ASCommitWindow.GetParentViewSelectedItems(pv, false, false), false);
							GUIUtility.ExitGUI();
						}
						else
						{
							if (!pv.selectedItems[el.row] || shiftIsDown || ctrlIsDown)
								selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref pv.initialSelectedItem, ref pv.selectedItems);

							pv.selectedFile = fileI;
							pv.selectedFolder = folderI;
							pv.lv.row = el.row;
						}
					} else
					if (ListViewGUILayout.HasMouseDown(el.position, 1))
					{
						if (!pv.selectedItems[el.row])
						{
							selChanged = true;
							pv.ClearSelection();
							pv.selectedItems[el.row] = true;

							pv.selectedFile = fileI;
							pv.selectedFolder = folderI;
							pv.lv.row = el.row;
						}

						GUIUtility.hotControl = 0;
						Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
						EditorUtility.DisplayCustomMenu(r, commitDropDownMenuItems, null, CommitContextMenuClick, null);
						Event.current.Use();
					}
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

				pv.NextFileFolder(ref folderI, ref fileI);
			}

			if (!committing)
			{
				// "Select All"
				if (GUIUtility.keyboardControl == pv.lv.ID)
				{
					if (Event.current.type == EventType.ValidateCommand && Event.current.commandName == "SelectAll")
					{
						Event.current.Use();
					}
					else
						if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "SelectAll")
						{
							for (int i = 0; i < pv.selectedItems.Length; i++)
								pv.selectedItems[i] = true;

							selChanged = true;

							Event.current.Use();
						}
				}

				if (GUIUtility.keyboardControl == pv.lv.ID && !ctrlIsDown)
				{
					if (pv.lv.selectionChanged)
					{
						selChanged |= ListViewGUILayout.MultiSelection(prevSelectedRow, pv.lv.row, ref pv.initialSelectedItem, ref pv.selectedItems);

						pv.IndexToFolderAndFile(pv.lv.row, ref pv.selectedFolder, ref pv.selectedFile);
					}
				}

				if (pv.lv.selectionChanged || selChanged)
				{
					MySelectionToGlobalSelection();
				}
			}
		}

		void DoCommit()
		{
			if (commitMessage == string.Empty)
			{
				if (!EditorUtility.DisplayDialog("Commit without description", "Are you sure you want to commit with empty commit description message?", "Commit", "Cancel"))
				{
					GUIUtility.ExitGUI();
				}
			}

			bool refreshCommit = AssetServer.GetRefreshCommit();

			// Build guid list from assets
			ASCommitWindow win = new ASCommitWindow(this, ASCommitWindow.GetParentViewSelectedItems(pv, true, false).ToArray());
			win.InitiateReinit();

			if (refreshCommit || win.lastTransferMovedDependencies)
			{
				if ((!refreshCommit && !EditorUtility.DisplayDialog("Committing with dependencies",
					"Assets selected for committing have dependencies that will also be committed. Press Details to view full changeset",
					"Commit", "Details")) || refreshCommit)
				{
					// in this case switch to big commit window and forget it
					committing = false;
					selectedPage = Page.Commit;
					win.description = commitMessage;

					if (refreshCommit)
						win.showReinitedWarning = 1;

					asCommitWin = win;
					Repaint();
					GUIUtility.ExitGUI();
					return;
				}
			}

			// No dependencies or user does not care. Will do commit from here.
			string[] guidsToCommit = win.GetItemsToCommit();

			AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBOverviewsCommitFinished");
			AssetServer.DoCommitOnNextTick(commitMessage, guidsToCommit);

			AssetServer.SetLastCommitMessage(commitMessage);
			win.AddToCommitMessageHistory(commitMessage);

			committing = false;

			GUIUtility.ExitGUI();
		}

		void StartCommitting()
		{
			committing = true;
			commitMessage = string.Empty; // just to not make quick committing too easy
			selectionChangedWhileCommitting = false;
			focusCommitMessage = true;
		}

		internal void CommitFinished(bool actionResult)
		{
			if (actionResult)
			{
				AssetServer.ClearCommitPersistentData();
				InitOverviewPage(true);
			}
			else
				Repaint();
		}

		void CancelCommit()
		{
			committing = false;

			if (selectionChangedWhileCommitting)
				DoSelectionChange();
		}

		void DoMyRevert(bool afterMarkingDependencies)
		{
			if (!afterMarkingDependencies)
			{
				List<string> prevSelection = ASCommitWindow.GetParentViewSelectedItems(pv, true, false);

				if (ASCommitWindow.MarkAllFolderDependenciesForDiscarding(pv, null))
				{
					lastRevertSelectionChanged = 2; // see below...
					MySelectionToGlobalSelection();
				}
				else
					lastRevertSelectionChanged = -1;

				List<string> currSelection = ASCommitWindow.GetParentViewSelectedItems(pv, true, false);

				if (prevSelection.Count != currSelection.Count) // it never deselects, so this means it changed
					lastRevertSelectionChanged = 2;
			}

			if (afterMarkingDependencies || lastRevertSelectionChanged == -1)
			{
				ASCommitWindow.DoRevert(ASCommitWindow.GetParentViewSelectedItems(pv, true, true), "CBInitOverviewPage");
			}
		}

		void ShortCommitList()
		{
			bool wasGUIEnabled = GUI.enabled;
			GUILayout.BeginVertical (showSmallWindow ? constants.groupBoxNoMargin : constants.groupBox);
			GUILayout.Label ("Local Changes", constants.title);

			if (pv.lv.totalRows == 0)
			{
				GUILayout.BeginVertical (constants.contentBox, GUILayout.ExpandHeight (true));
				GUILayout.Label("Nothing to commit");
				GUILayout.EndVertical ();
			}
			else
			{
				DoCommitParentView();

				GUILayout.BeginHorizontal(constants.contentBox);

				Event evt = Event.current;

				if (!committing)
				{
					GUI.enabled = pvHasSelection && wasGUIEnabled;
					if (GUILayout.Button("Compare", constants.smallButton))
					{
						ASCommitWindow.DoShowDiff(ASCommitWindow.GetParentViewSelectedItems(pv, false, false), false);
						GUIUtility.ExitGUI();
					}

					bool dWasGUIEnabled = GUI.enabled;
					if (!somethingDiscardableSelected)
						GUI.enabled = false;

					if (GUILayout.Button("Discard", constants.smallButton))
					{
						DoMyRevert(false);
						GUIUtility.ExitGUI();
					}

					GUI.enabled = dWasGUIEnabled;

					GUILayout.FlexibleSpace();

					if (GUILayout.Button("Commit...", constants.smallButton) ||
						(pvHasSelection && evt.type == EventType.KeyDown && evt.keyCode == KeyCode.Return))
					{
						StartCommitting();
						evt.Use();
					}

					if (evt.type == EventType.KeyDown && (evt.character == '\n' || (int)evt.character == 3))
					{
						evt.Use();
					}

					GUI.enabled = wasGUIEnabled;

					if (GUILayout.Button("Details", constants.smallButton))
					{
						SwitchSelectedPage(Page.Commit);
						this.Repaint();
						GUIUtility.ExitGUI();
					}
				}else
				{
					if (evt.type == EventType.KeyDown)
					{
						switch (evt.keyCode)
						{
							case KeyCode.Escape:
								CancelCommit();
								evt.Use();
								break;
							case KeyCode.Return:
								DoCommit();
								evt.Use();
								break;
							default:
								if (evt.character == '\n' || (int)evt.character == 3)
									evt.Use();
								break;
						}
					}

					GUI.SetNextControlName("commitMessage");
					commitMessage = EditorGUILayout.TextField(commitMessage);
					if (GUILayout.Button("Commit", constants.smallButton, GUILayout.Width(60)))
					{
						DoCommit();
					}
					if (GUILayout.Button("Cancel", constants.smallButton, GUILayout.Width(60)))
					{
						CancelCommit();
					}

					if (focusCommitMessage)
					{
						EditorGUI.FocusTextInControl("commitMessage");
						focusCommitMessage = false;
						Repaint();
					}
				}

				GUILayout.EndHorizontal();
			}

			GUILayout.EndVertical ();

			// this clutter is to show user the files that where selected when showing the question dialog. See ASCommitWindow.cs
			if (lastRevertSelectionChanged == 0)
			{
				lastRevertSelectionChanged = -1;

				if (ASCommitWindow.ShowDiscardWarning())
				{
					DoMyRevert(true);
				}
			}

			if (lastRevertSelectionChanged > 0)
			{
				lastRevertSelectionChanged--;
				Repaint();
			}
		}
	
		void ShortUpdateList ()
		{
			GUILayout.BeginVertical (showSmallWindow ? constants.groupBoxNoMargin : constants.groupBox);
			GUILayout.Label ("Updates on Server", constants.title);

			if (sharedChangesets == null)
			{
				GUILayout.BeginVertical (constants.contentBox, GUILayout.ExpandHeight (true));
				GUILayout.Label("Could not retrieve changes");
				GUILayout.EndVertical ();
			}
			else
			if (sharedChangesets.Length == 0)
			{
				GUILayout.BeginVertical (constants.contentBox, GUILayout.ExpandHeight (true));
				GUILayout.Label("You are up to date");
				GUILayout.EndVertical ();
			}
			else
			{
				lv.totalRows = sharedChangesets.Length;
				Rect r, rect;
				int height = (int)constants.entryNormal.CalcHeight(new GUIContent("X"), 100);

				constants.serverUpdateLog.alignment = TextAnchor.MiddleLeft;
				constants.serverUpdateInfo.alignment = TextAnchor.MiddleLeft;

				foreach (ListViewElement el in ListViewGUILayout.ListView(lv, constants.background))
				{
					// never-ending joys of layouting
					rect = GUILayoutUtility.GetRect(GUIClip.visibleRect.width, height, GUILayout.MinHeight(height));

					r = rect;
					r.x += 1;
					r.y += 1;

					if (el.row % 2 == 0)
					{
						if (Event.current.type == EventType.Repaint)
							constants.entryEven.Draw(r, false, false, false, false);
						r.y += rect.height;
						if (Event.current.type == EventType.Repaint)
							constants.entryOdd.Draw(r, false, false, false, false);
					}

					r = rect;
					r.width -= maxNickLength + 25;
					r.x += 10;
					GUI.Button(r, changesetContents[el.row], constants.serverUpdateLog);

					r = rect;
					r.x += r.width - maxNickLength - 5;
					GUI.Label(r, sharedChangesets[el.row].owner, constants.serverUpdateInfo);
				}

				constants.serverUpdateLog.alignment = TextAnchor.UpperLeft;
				constants.serverUpdateInfo.alignment = TextAnchor.UpperLeft;
				
				GUILayout.BeginHorizontal (constants.contentBox);

				GUILayout.FlexibleSpace ();

				if (GUILayout.Button("Update", constants.smallButton))
				{
					selectedPage = Page.Update;
					InitUpdatePage(true);
					asUpdateWin.DoUpdate(false);
				}

				if (GUILayout.Button("Details", constants.smallButton))
				{
					SwitchSelectedPage(Page.Update);
					this.Repaint();
					GUIUtility.ExitGUI();
				}

				GUILayout.EndHorizontal ();

			}

			GUILayout.EndVertical ();
		}

		void DoSelectedPageGUI()
		{
			switch (selectedPage)
			{
				case Page.Update:
					if (asUpdateWin != null && asUpdateWin != null)
						asUpdateWin.DoGUI();
					break;
				case Page.Commit:
					if (asCommitWin != null && asCommitWin != null)
						asCommitWin.DoGUI();
					break;
				case Page.Overview:
					OverviewPageGUI();
					break;
				case Page.History:
					if (asHistoryWin != null && !asHistoryWin.DoGUI(m_Parent.hasFocus))
					{
						SwitchSelectedPage(Page.Overview);
						GUIUtility.ExitGUI();
					}
					break;
				case Page.ServerConfig:
					if (asConfigWin != null && !asConfigWin.DoGUI())
					{
						SwitchSelectedPage(Page.Overview);
						GUIUtility.ExitGUI();
					}
					break;
				case Page.Admin:
					if (asAdminWin != null && !asAdminWin.DoGUI())
					{
						SwitchSelectedPage(Page.Overview);
						GUIUtility.ExitGUI();
					}
					break;
			}
		}

		void SetShownSearchField(ShowSearchField newShow)
		{
			EditorGUI.FocusTextInControl("SearchFilter");
			m_SearchField.Show = false;
			m_ShowSearch = newShow;
			m_SearchField.Show = true;
			asHistoryWin.FilterItems(false);
		}

		void DoSearchToggle(ShowSearchField field)
		{
			if (selectedPage == Page.History)
			{
				if (m_SearchField.DoGUI())
				{
					asHistoryWin.FilterItems(false);
				}
				GUILayout.Space(10);
			}
		}
		#endregion

		#region Main OnGUI

		bool IsLastOne(int f, int fl, ParentViewState st)
		{
			return st.folders.Length - 1 == f && st.folders[f].files.Length - 1 == fl;
		}

		void OnGUI()
		{
			if (EditorSettings.externalVersionControl != ExternalVersionControl.Disabled && EditorSettings.externalVersionControl != ExternalVersionControl.AssetServer)
			{
				GUILayout.FlexibleSpace ();
				GUILayout.BeginHorizontal ();
				GUILayout.FlexibleSpace ();
				GUILayout.Label ("Asset Server is disabled when external version control is used. Go to 'Edit -> Project Settings -> Editor' to re-enable it.");
				GUILayout.FlexibleSpace ();
				GUILayout.EndHorizontal ();
				GUILayout.FlexibleSpace ();
				return;
			}
			if (constants == null)
			{
				constants = new Constants();
			}

			// need to do this in non layout event to not have exceptions
			if (!m_CheckedMaint && Event.current.type != EventType.Layout)
			{
				if (!InternalEditorUtility.HasMaint())
				{
					Close();
					GUIUtility.ExitGUI();
				}

				m_CheckedMaint = true;
			}

			// Find the longest username string so that we can use it with layouted LV
			if (maxNickLength == 1 && sharedChangesets != null)
			{
				for (int i = 0; i < sharedChangesets.Length; i++)
				{
					int thisLen = (int)constants.serverUpdateInfo.CalcSize(new GUIContent(sharedChangesets[i].owner)).x;
					if (thisLen > maxNickLength)
						maxNickLength = thisLen;
				}

				changesetContents = new GUIContent[sharedChangesets.Length];

				ParentViewState pvDummy = new ParentViewState();

				for (int i = 0; i < changesetContents.Length; i++)
				{
					int maxTooltipLines = 15;
					Changeset cs = sharedChangesets[i];
					string msg = cs.message.Split('\n')[0];
					msg = msg.Length < 45 ? msg : msg.Substring(0, 42) + "...";
					string tooltip = string.Format("[{0} {1}] {2}", cs.date, cs.owner, msg);
					maxTooltipLines--;

					pvDummy.Clear();
					pvDummy.AddAssetItems(cs);

					for (int f = 0; f < pvDummy.folders.Length; f++)
					{
						if (--maxTooltipLines == 0 && !IsLastOne(f, 0, pvDummy))
						{
							tooltip += "\n(and more...)";
							break;
						}

						tooltip += "\n" + pvDummy.folders[f].name;

						for (int fl = 0; fl < pvDummy.folders[f].files.Length; fl++)
						{
							if (--maxTooltipLines == 0 && !IsLastOne(f, fl, pvDummy))
							{
								tooltip += "\n(and more...)";
								break;
							}
							tooltip += "\n\t" + pvDummy.folders[f].files[fl].name;
						}

						if (maxTooltipLines == 0)
							break;
					}

					changesetContents[i] = new GUIContent(sharedChangesets[i].message.Split('\n')[0], tooltip);
				}

				if (maxNickLength == 1)
					maxNickLength = 0; // just to know it's been setup
			}

			if (AssetServer.IsControllerBusy() != 0) // controller is busy. Don't try to draw anything.
			{
				Repaint();
				return;
			}

			if (isInitialUpdate)
			{
				isInitialUpdate = false;
				SwitchSelectedPage(Page.Overview); // this can take a long time, so don't do it until Server page is actually visible
			}

			if (Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "Find")
			{
				SetShownSearchField(m_SearchToShow);
				Event.current.Use();
			}

			GUILayout.BeginHorizontal(EditorStyles.toolbar);

			int newConfigPage = -1;

			bool wasGUIEnabled = GUI.enabled;

			if (ToolbarToggle(selectedPage == Page.Overview, pageTitles[0], EditorStyles.toolbarButton))
				newConfigPage = 0;

			GUI.enabled = !needsSetup && sharedChangesets != null && sharedChangesets.Length != 0 && wasGUIEnabled;

			if (ToolbarToggle(selectedPage == Page.Update, pageTitles[1], EditorStyles.toolbarButton))
				newConfigPage = 1;

			GUI.enabled = !needsSetup && pv.lv.totalRows != 0 && wasGUIEnabled;

			if (selectedPage > lastMainPage)
			{
				if (ToolbarToggle(selectedPage == Page.Commit, pageTitles[2], EditorStyles.toolbarButton))
					newConfigPage = 2;

				GUI.enabled = wasGUIEnabled;

				if (ToolbarToggle(selectedPage > lastMainPage, pageTitles[3], EditorStyles.toolbarButton))
					newConfigPage = 3;
			}else
			{
				if (ToolbarToggle(selectedPage == Page.Commit, pageTitles[2], EditorStyles.toolbarButton))
					newConfigPage = 2;

				GUI.enabled = wasGUIEnabled;
			}

			// We switch the tab, Exit and Repaint
			if (newConfigPage != -1 && newConfigPage != (int)selectedPage)
			{
				if (selectedPage == Page.Commit)
				{
					NotifyClosingCommit();
				}

				if (newConfigPage <= (int)lastMainPage)
				{
					SwitchSelectedPage((Page)newConfigPage);
					GUIUtility.ExitGUI();
				}
			}

			GUILayout.FlexibleSpace();

			if (selectedPage == Page.History)
			{
				DoSearchToggle(ShowSearchField.HistoryList);
			}

			if (!needsSetup)
			{
				switch (selectedPage)
				{
					case Page.Overview:
					case Page.Update:
					case Page.History:
						if (GUILayout.Button("Refresh", EditorStyles.toolbarButton))
						{
							ActionRefresh();
							GUIUtility.ExitGUI();
						}
						break;
				}
			}

			GUILayout.EndHorizontal ();

			EditorGUIUtility.SetIconSize(iconSize);
			DoSelectedPageGUI();
			EditorGUIUtility.SetIconSize(Vector2.zero);

			if (Event.current.type == EventType.ContextClick)
			{
				GUIUtility.hotControl = 0;
				Rect r = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);
				EditorUtility.DisplayCustomMenu(r, needsSetup ? unconfiguredDropDownMenuItems : dropDownMenuItems, null, ContextMenuClick, null);
				Event.current.Use();
			}

			//GUILayout.Label(position.width.ToString());

			// Show selected items GUID
			/*HierarchyProperty prop = new HierarchyProperty(HierarchyType.Assets);

			Object[] objects = Selection.objects;

			if (objects != null && objects.Length != 0)
			{
				if (prop.Find(objects[0].GetInstanceID(), null))
				{
					GUILayout.Label(prop.guid);
				}
			}*/
		}
		#endregion
	}
}
