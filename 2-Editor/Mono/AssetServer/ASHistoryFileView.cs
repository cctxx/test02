using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Collections;
using UnityEditorInternal;

namespace UnityEditor
{
	[System.Serializable]
    class ASHistoryFileView
    {
        int[] m_ExpandedArray = new int[0];
        public Vector2 m_ScrollPosition;

        static float m_RowHeight = 16;
        float m_FoldoutSize = 14;
        float m_Indent = 16;
        float m_BaseIndent = 16;
        float m_SpaceAtTheTop = m_RowHeight + 6; // "Entire project" item with all spaces

        static int ms_FileViewHash = "FileView".GetHashCode();
        int m_FileViewControlID = -1;

        //enum UsingFilter { None, Project, Deleted };
        //UsingFilter m_UsingFilter = UsingFilter.None;

        public enum SelectionType { None, All, Items, DeletedItemsRoot, DeletedItems };

		static bool OSX = Application.platform == RuntimePlatform.OSXEditor;

        SelectionType m_SelType = SelectionType.None;

        public SelectionType SelType
        {
            set
            {
                if (m_SelType == SelectionType.DeletedItems && value != SelectionType.DeletedItems)
                {
                    for (int i = 0; i < m_DelPVstate.selectedItems.Length; i++)
                        m_DelPVstate.selectedItems[i] = false;
                }

                m_SelType = value;
            }
            get
            {
                return m_SelType;
            }
        }

        bool m_DeletedItemsToggle = false;
        bool DeletedItemsToggle
        {
            get
            {
                return m_DeletedItemsToggle;
            }
            set
            {
                m_DeletedItemsToggle = value;
                if (m_DeletedItemsToggle && !m_DeletedItemsInitialized)
                {
                    SetupDeletedItems();
                }
            }
        }

        DeletedAsset[] m_DeletedItems = null;
        bool m_DeletedItemsInitialized = false;

        ParentViewState m_DelPVstate = new ParentViewState();

        Rect m_ScreenRect;

        class Styles
        {
            public GUIStyle foldout = "IN Foldout";
            public GUIStyle insertion = "PR Insertion";
            public GUIStyle label = "PR Label";
            public GUIStyle ping = new GUIStyle("PR Ping");
            public GUIStyle toolbarButton = "ToolbarButton";

			public Styles()
			{
				// Ping style was changed in editor_resources. Resetting the values back to old ones for backward compatibility.
				ping.overflow.left = -2;
				ping.overflow.right = -21;
				ping.padding.left = 48;
				ping.padding.right = 0;
			}
        }

        static Styles ms_Styles = null;

        GUIContent[] dropDownMenuItems = new GUIContent[] { new GUIContent("Recover") }; // change ContextMenuClick function when changing this

        public ASHistoryFileView()
        {
            m_DelPVstate.lv = new ListViewState(0);
            m_DelPVstate.lv.totalRows = 0;
        }

        void SetupDeletedItems()
        {
            m_DeletedItems = AssetServer.GetServerDeletedItems();
            m_DelPVstate.Clear();
            m_DelPVstate.lv = new ListViewState(0);

            m_DelPVstate.AddAssetItems(m_DeletedItems);
            m_DelPVstate.AddAssetItems(AssetServer.GetLocalDeletedItems());
            m_DelPVstate.SetLineCount();
            m_DelPVstate.selectedItems = new bool[m_DelPVstate.lv.totalRows];

            m_DeletedItemsInitialized = true;
        }

        void ContextMenuClick(object userData, string[] options, int selected)
        {
            if (selected >= 0)
            {
                switch (selected)
                {
                    case 0: //"Recover":
                        DoRecover();
                        break;
                }
            }
        }

        public void SelectDeletedItem(string guid)
        {
            SelType = SelectionType.DeletedItems;
            DeletedItemsToggle = true;

            int totalIndex = 0;
            for (int i = 0; i < m_DelPVstate.folders.Length; i++)
            {
                ParentViewFolder folder = m_DelPVstate.folders[i];

                m_DelPVstate.selectedItems[totalIndex] = false;

                if (folder.guid == guid)
                {
                    m_DelPVstate.selectedItems[totalIndex] = true;
                    ScrollToDeletedItem(totalIndex);
                    return;
                }

                for (int k = 0; k < folder.files.Length; k++)
                {
                    totalIndex++;

                    m_DelPVstate.selectedItems[totalIndex] = false;

                    if (folder.files[k].guid == guid)
                    {
                        m_DelPVstate.selectedItems[totalIndex] = true;
                        ScrollToDeletedItem(totalIndex);
                        return;
                    }
                }

                totalIndex++;
            }
        }

        public void DoRecover()
        {
            string[] guids = GetSelectedDeletedItemGUIDs();
            Dictionary<string, int> byGUID = new Dictionary<string, int>();
            DeletedAsset[] sortedList;
            int index = 0;

            for (int i = 0; i < guids.Length; i++)
            {
                for (int k = 0; k < m_DeletedItems.Length; k++)
                {
                    if (m_DeletedItems[k].guid == guids[i])
                    {
                        byGUID[m_DeletedItems[k].guid] = k;
                        break;
                    }
                }
            }

            sortedList = new DeletedAsset[byGUID.Count];

            // When restoring multiple entries, sort the list of entries so parent assets will be restored before child assets
            while (byGUID.Count != 0)
            {
                DeletedAsset entry = null;

                foreach (KeyValuePair<string, int> de in byGUID)
                {
                    entry = m_DeletedItems[de.Value];

                    if (!byGUID.ContainsKey(entry.parent))
                    {
                        sortedList[index++] = entry;
                        break;
                    }
                }

                byGUID.Remove(entry.guid);
            }

            AssetServer.SetAfterActionFinishedCallback("ASEditorBackend", "CBReinitASMainWindow");
            AssetServer.DoRecoverOnNextTick(sortedList);
        }

        public string[] GetSelectedDeletedItemGUIDs()
        {
            List<string> guids = new List<string>();

            int totalIndex = 0;
            for (int i = 0; i < m_DelPVstate.folders.Length; i++)
            {
                ParentViewFolder folder = m_DelPVstate.folders[i];

                if (m_DelPVstate.selectedItems[totalIndex])
                    guids.Add(folder.guid);

                for (int k = 0; k < folder.files.Length; k++)
                {
                    totalIndex++;

                    if (m_DelPVstate.selectedItems[totalIndex])
                        guids.Add(folder.files[k].guid);
                }

                totalIndex++;
            }
            
            return guids.ToArray();
        }

        public string[] GetAllDeletedItemGUIDs()
        {
            if (!m_DeletedItemsInitialized)
                SetupDeletedItems();

            string[] guids = new string[m_DeletedItems.Length];

            for (int i = 0; i < guids.Length; i++)
            {
                guids[i] = m_DeletedItems[i].guid;
            }

            return guids;
        }

        public void FilterItems(string filterText)
        {
            //bool filter = filterText.Trim() != string.Empty;

            // clear deleted item selection, have filter buffer.len == total.len
        }

        int ControlIDForProperty(HierarchyProperty property)
        {
            ///@TODO> WHY NOT BY DEFAULT RESERVE 0-1000000 for manually generated ids
            if (property != null)
                return property.row + 10000000;
            else
                return -1;
        }

        void SetExpanded(int instanceID, bool expand)
        {
            Hashtable expandedHash = new Hashtable();
            for (int i = 0; i < m_ExpandedArray.Length; i++)
                expandedHash.Add(m_ExpandedArray[i], null);

            if (expand != expandedHash.Contains(instanceID))
            {
                if (expand)
                    expandedHash.Add(instanceID, null);
                else
                    expandedHash.Remove(instanceID);

                m_ExpandedArray = new int[expandedHash.Count];
                int index = 0;
                foreach (int key in expandedHash.Keys)
                {
                    m_ExpandedArray[index] = key;
                    index++;
                }
            }

            InternalEditorUtility.expandedProjectWindowItems = m_ExpandedArray;
        }

        void SetExpandedRecurse(int instanceID, bool expand)
        {
            HierarchyProperty search = new HierarchyProperty(HierarchyType.Assets);
            if (search.Find(instanceID, m_ExpandedArray))
            {
                SetExpanded(instanceID, expand);

                int depth = search.depth;
                while (search.Next(null) && search.depth > depth)
                {
                    SetExpanded(search.instanceID, expand);
                }
            }
        }

        void SelectionClick(HierarchyProperty property)
        {
            // Toggle selected property from selection
            if (EditorGUI.actionKey)
            {
                ArrayList newSelection = new ArrayList();
                newSelection.AddRange(Selection.objects);

                if (newSelection.Contains(property.pptrValue))
                    newSelection.Remove(property.pptrValue);
                else
                    newSelection.Add(property.pptrValue);

                Selection.objects = newSelection.ToArray(typeof(Object)) as Object[];
            }
            // Select everything between the first selected object and the selected
            else if (Event.current.shift)
            {
                HierarchyProperty firstSelectedProperty = GetFirstSelected();
                HierarchyProperty lastSelectedProperty = GetLastSelected();

                if (!firstSelectedProperty.isValid)
                {
                    Selection.activeObject = property.pptrValue;
                    return;
                }

                HierarchyProperty from, to;
                if (property.row > lastSelectedProperty.row)
                {
                    from = firstSelectedProperty;
                    to = property;
                }
                else
                {
                    from = property;
                    to = lastSelectedProperty;
                }

                ArrayList newSelection = new ArrayList();

                newSelection.Add(from.pptrValue);
                while (from.Next(m_ExpandedArray))
                {
                    newSelection.Add(from.pptrValue);
                    if (from.instanceID == to.instanceID)
                        break;
                }

                Selection.objects = newSelection.ToArray(typeof(Object)) as Object[];
            }
            // Just set the selection to the clicked object
            else
            {
                Selection.activeObject = property.pptrValue;
            }

            SelType = SelectionType.Items;
            FrameObject(Selection.activeObject);
        }

        HierarchyProperty GetActiveSelected()
        {
            return GetFirstSelected();
        }

        HierarchyProperty GetFirstSelected()
        {
            int firstRow = 1000000000;
            HierarchyProperty found = null;

            foreach (Object obj in Selection.objects)
            {
                HierarchyProperty search = new HierarchyProperty(HierarchyType.Assets);
                if (search.Find(obj.GetInstanceID(), m_ExpandedArray) && search.row < firstRow)
                {
                    firstRow = search.row;
                    found = search;
                }
            }

            return found;
        }

        HierarchyProperty GetLast()
        {
            HierarchyProperty search = new HierarchyProperty(HierarchyType.Assets);
            int offset = search.CountRemaining(m_ExpandedArray);
            search.Reset();
            if (search.Skip(offset, m_ExpandedArray))
                return search;
            else
                return null;
        }

        HierarchyProperty GetFirst()
        {
            HierarchyProperty search = new HierarchyProperty(HierarchyType.Assets);
            if (search.Next(m_ExpandedArray))
                return search;
            else
                return null;
        }

        void OpenAssetSelection()
        {
            foreach (int id in Selection.instanceIDs)
            {
                if (AssetDatabase.Contains(id))
                    AssetDatabase.OpenAsset(id);
            }
        }

        HierarchyProperty GetLastSelected()
        {
            int firstRow = -1;
            HierarchyProperty found = null;

            foreach (Object obj in Selection.objects)
            {
                HierarchyProperty search = new HierarchyProperty(HierarchyType.Assets);
                if (search.Find(obj.GetInstanceID(), m_ExpandedArray) && search.row > firstRow)
                {
                    firstRow = search.row;
                    found = search;
                }
            }

            return found;
        }

        void AllProjectKeyboard()
        {
            switch (Event.current.keyCode)
            {
                case KeyCode.DownArrow:
                    if (GetFirst() != null)
                    {
                        Selection.activeObject = GetFirst().pptrValue;
                        FrameObject(Selection.activeObject);
                        SelType = SelectionType.Items;
						Event.current.Use();
                    }
                    break;
            }
        }

        void AssetViewKeyboard()
        {
            switch (Event.current.keyCode)
            {
                // Fold in
                case KeyCode.LeftArrow:
                {
                    HierarchyProperty activeSelected = GetActiveSelected();
                    if (activeSelected != null)
                        SetExpanded(activeSelected.instanceID, false);
                    break;
                }
                // Fold out
                case KeyCode.RightArrow:
                {
                    HierarchyProperty activeSelected = GetActiveSelected();
                    if (activeSelected != null)
                        SetExpanded(activeSelected.instanceID, true);
                    break;
                }
                case KeyCode.UpArrow:
                {
                    Event.current.Use();

                    HierarchyProperty firstSelected = GetFirstSelected();
                    // Select previous
                    if (firstSelected != null)
                    {
                        if (firstSelected.instanceID == GetFirst().instanceID)
                        {
                            SelType = SelectionType.All;
                            Selection.objects = new Object[0];
                            ScrollTo(0);
                        }
                        else
                        if (firstSelected.Previous(m_ExpandedArray))
                        {
                            Object newSelectedObject = firstSelected.pptrValue;
                            SelectionClick(firstSelected);
                            FrameObject(newSelectedObject);
                        }
                    }
                    break;
                }
                // Select next or first
                case KeyCode.DownArrow:
                {
                    Event.current.Use();

                    // Select next
                    HierarchyProperty lastSelected = GetLastSelected();
                    // cmd-down -> open selection
                    if (Application.platform == RuntimePlatform.OSXEditor && Event.current.command)
                    {
                        OpenAssetSelection();
                        GUIUtility.ExitGUI();
                    }
                    else
                        if (lastSelected != null)
                        {
                            if (lastSelected.instanceID == GetLast().instanceID)
                            {
                                SelType = SelectionType.DeletedItemsRoot;
                                Selection.objects = new Object[0];
                                ScrollToDeletedItem(-1);
                            }
                            else
                            if (lastSelected.Next(m_ExpandedArray))
                            {
                                Object newSelectedObject = lastSelected.pptrValue;
                                SelectionClick(lastSelected);
                                FrameObject(newSelectedObject);
                            }
                        }
                    break;
                }
                // Select next or first
                case KeyCode.KeypadEnter:
                case KeyCode.Return:
                {
                    if (Application.platform == RuntimePlatform.WindowsEditor)
                    {
                        OpenAssetSelection();
                        GUIUtility.ExitGUI();
                    }
                    break;
                }
                case KeyCode.Home:
                {
                    if (GetFirst() != null)
                    {
                        Selection.activeObject = GetFirst().pptrValue;
                        FrameObject(Selection.activeObject);
                    }
                    break;
                }
                case KeyCode.End:
                {
                    if (GetLast() != null)
                    {
                        Selection.activeObject = GetLast().pptrValue;
                        FrameObject(Selection.activeObject);
                    }
                    break;
                }
                case KeyCode.PageUp:
                {
                    Event.current.Use();

                    if (OSX)
                    {
                        m_ScrollPosition.y -= m_ScreenRect.height;

                        if (m_ScrollPosition.y < 0)
                            m_ScrollPosition.y = 0;
                    }
                    else
                    {
                        HierarchyProperty firstSelected = GetFirstSelected();

                        if (firstSelected != null)
                        {
                            for (int i = 0; i < m_ScreenRect.height / m_RowHeight; i++)
                            {
                                if (!firstSelected.Previous(m_ExpandedArray))
                                {
                                    firstSelected = GetFirst();
                                    break;
                                }
                            }

                            Object newSelectedObject = firstSelected.pptrValue;
                            SelectionClick(firstSelected);
                            FrameObject(newSelectedObject);
                        }
                        // No previous selection -> Select last
                        else if (GetFirst() != null)
                        {
                            Selection.activeObject = GetFirst().pptrValue;
                            FrameObject(Selection.activeObject);
                        }
                    }
                    break;
                }
                case KeyCode.PageDown:
                {
                    Event.current.Use();

                    if (OSX)
                    {
                        m_ScrollPosition.y += m_ScreenRect.height;
                    }
                    else
                    {
                        HierarchyProperty lastSelected = GetLastSelected();

                        if (lastSelected != null)
                        {
                            for (int i = 0; i < m_ScreenRect.height / m_RowHeight; i++)
                            {
                                if (!lastSelected.Next(m_ExpandedArray))
                                {
                                    lastSelected = GetLast();
                                    break;
                                }
                            }

                            Object newSelectedObject = lastSelected.pptrValue;
                            SelectionClick(lastSelected);
                            FrameObject(newSelectedObject);
                        }
                        // No next selection -> Select last
                        else if (GetLast() != null)
                        {
                            Selection.activeObject = GetLast().pptrValue;
                            FrameObject(Selection.activeObject);
                        }
                    }
                    break;
                }
				default: return;
            }

			Event.current.Use();
        }

        void DeletedItemsRootKeyboard(ASHistoryWindow parentWin)
        {
            switch (Event.current.keyCode)
            {
                case KeyCode.LeftArrow:
                    DeletedItemsToggle = false;
                    break;
                case KeyCode.RightArrow:
                    DeletedItemsToggle = true;
                    break;
                case KeyCode.UpArrow:
                    SelType = SelectionType.Items;
                    if (GetLast() != null)
                    {
                        Selection.activeObject = GetLast().pptrValue;
                        FrameObject(Selection.activeObject);
                    }
                    break;
                case KeyCode.DownArrow:
                    if (m_DelPVstate.selectedItems.Length > 0 && DeletedItemsToggle)
                    {
                        SelType = SelectionType.DeletedItems;
                        m_DelPVstate.selectedItems[0] = true;
                        m_DelPVstate.lv.row = 0;
                        ScrollToDeletedItem(0);
                    }
                    break;
                default:
                    return;
            }

			if (SelType != SelectionType.Items)
				parentWin.DoLocalSelectionChange();
			Event.current.Use();
        }

        void DeletedItemsKeyboard(ASHistoryWindow parentWin)
        {
            int prevRow = m_DelPVstate.lv.row;
            int newRow = prevRow;

            if (!DeletedItemsToggle)
                return;

            switch (Event.current.keyCode)
            {
                case KeyCode.UpArrow:
                {
                    if (newRow > 0)
                    {
                        newRow--;
                    }
                    else
                    {
                        SelType = SelectionType.DeletedItemsRoot;
                        ScrollToDeletedItem(-1);
                        parentWin.DoLocalSelectionChange();
                    }
                    break;
                }
                case KeyCode.DownArrow:
                {
                    if (newRow < m_DelPVstate.lv.totalRows - 1)
                        newRow++;
                    break;
                }
                case KeyCode.Home:
                {
                    newRow = 0;
                    break;
                }
                case KeyCode.End:
                {
                    newRow = m_DelPVstate.lv.totalRows - 1;
                    break;
                }
                case KeyCode.PageUp:
                {
                    if (OSX)
                    {
                        m_ScrollPosition.y -= m_ScreenRect.height;

                        if (m_ScrollPosition.y < 0)
                            m_ScrollPosition.y = 0;
                    }
                    else
                    {
                        newRow -= (int)(m_ScreenRect.height / m_RowHeight);
                        if (newRow < 0)
                            newRow = 0;
                    }
                    break;
                }
                case KeyCode.PageDown:
                {
                    if (OSX)
                    {
                        m_ScrollPosition.y += m_ScreenRect.height;
                    }
                    else
                    {
                        newRow += (int)(m_ScreenRect.height / m_RowHeight);

                        if (newRow > m_DelPVstate.lv.totalRows - 1)
                            newRow = m_DelPVstate.lv.totalRows - 1;
                    }
                    break;
                }
				default: return;
            }

            Event.current.Use();

            if (newRow != prevRow)
            {
                m_DelPVstate.lv.row = newRow;
                ListViewShared.MultiSelection(null, prevRow, newRow, ref m_DelPVstate.initialSelectedItem, ref m_DelPVstate.selectedItems);
                ScrollToDeletedItem(newRow);
                parentWin.DoLocalSelectionChange();
            }
        }

        void ScrollToDeletedItem(int index) // -1 for root
        {
            HierarchyProperty property = new HierarchyProperty(HierarchyType.Assets);

            float offset = m_SpaceAtTheTop + property.CountRemaining(m_ExpandedArray) * m_RowHeight + 6;

            if (index == -1)
            {
                ScrollTo(offset);
            }
            else
            {
                ScrollTo(offset + (index + 1) * m_RowHeight);
            }
        }

        void KeyboardGUI(ASHistoryWindow parentWin)
        {
            if (Event.current.GetTypeForControl(m_FileViewControlID) != EventType.KeyDown ||
                m_FileViewControlID != GUIUtility.keyboardControl) // FIXME: this should not be needed... but it is
                return;

            switch (SelType)
            {
                case SelectionType.All:
                    AllProjectKeyboard();
					break;
                case SelectionType.Items:
                    AssetViewKeyboard();
                    break;
                case SelectionType.DeletedItemsRoot:
                    DeletedItemsRootKeyboard(parentWin);
                    break;
                case SelectionType.DeletedItems:
                    DeletedItemsKeyboard(parentWin);
                    break;
            }
        }

        bool FrameObject(Object target)
        {
            if (target == null)
                return false;

            HierarchyProperty property = new HierarchyProperty(HierarchyType.Assets);
            if (property.Find(target.GetInstanceID(), null))
            {
                while (property.Parent())
                {
                    SetExpanded(property.instanceID, true);
                }
            }

            property.Reset();
            if (property.Find(target.GetInstanceID(), m_ExpandedArray))
            {
				ScrollTo(m_RowHeight * property.row + m_SpaceAtTheTop);
                return true;
            }
            else
            {
                return false;
            }
        }

        void ScrollTo(float scrollTop)
        {
            float scrollBottom = scrollTop - m_ScreenRect.height + m_RowHeight;
            m_ScrollPosition.y = Mathf.Clamp(m_ScrollPosition.y, scrollBottom, scrollTop);
        }

        public void DoDeletedItemsGUI(ASHistoryWindow parentWin, Rect theRect, GUIStyle s, float offset, float endOffset, bool focused)
        {
            Event evt = Event.current;
            GUIContent content;
            int indent;
            Texture2D folderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);

            offset += 3;

			Rect selectionRect = new Rect(m_Indent, offset, theRect.width - m_Indent, m_RowHeight);

            if (evt.type == EventType.MouseDown && selectionRect.Contains(evt.mousePosition))
            {
                GUIUtility.keyboardControl = m_FileViewControlID;
                //parentWin.m_SearchToShow = ASHistoryWindow.ShowSearchField.ProjectView;
                SelType = SelectionType.DeletedItemsRoot;
                ScrollToDeletedItem(-1);
                parentWin.DoLocalSelectionChange();
            }

			selectionRect.width -= selectionRect.x;
			selectionRect.x = 0;

            content = new GUIContent("Deleted Assets");
            content.image = folderIcon;
            indent = (int)(m_BaseIndent);
            s.padding.left = indent;
			
			if (evt.type == EventType.Repaint)
			{
            		s.Draw(selectionRect, content,
                    false, false,
                    SelType == SelectionType.DeletedItemsRoot,
                    focused);
			}
			
            Rect foldoutRect = new Rect(m_BaseIndent - m_FoldoutSize, offset, m_FoldoutSize, m_RowHeight);

            if (!m_DeletedItemsInitialized || m_DelPVstate.lv.totalRows != 0)
                DeletedItemsToggle = GUI.Toggle(foldoutRect, DeletedItemsToggle, GUIContent.none, ms_Styles.foldout);

            offset += m_RowHeight;

            if (!DeletedItemsToggle)
                return;

            int prevSelectedRow = m_DelPVstate.lv.row;
            int currRow = 0;

            ParentViewFolder folder;
            int folderI = -1, fileI = -1, row = 0;

            while (offset <= endOffset && row < m_DelPVstate.lv.totalRows)
            {
                if (offset + m_RowHeight >= 0)
                {
                    if (folderI == -1)
                        m_DelPVstate.IndexToFolderAndFile(row, ref folderI, ref fileI);

                        selectionRect = new Rect(0, offset, Screen.width, m_RowHeight);

                        folder = m_DelPVstate.folders[folderI];

                        if (evt.type == EventType.MouseDown && selectionRect.Contains(evt.mousePosition))
                        {
                            bool rightClickedOnSelected = evt.button == 1 && SelType == SelectionType.DeletedItems && m_DelPVstate.selectedItems[currRow];

                            if (!rightClickedOnSelected)
                            {
                                GUIUtility.keyboardControl = m_FileViewControlID;
                                //parentWin.m_SearchToShow = ASHistoryWindow.ShowSearchField.ProjectView;

                                SelType = SelectionType.DeletedItems;

                                m_DelPVstate.lv.row = currRow;
                                ListViewShared.MultiSelection(null, prevSelectedRow, m_DelPVstate.lv.row, ref m_DelPVstate.initialSelectedItem, ref m_DelPVstate.selectedItems);
                                ScrollToDeletedItem(currRow);
                                parentWin.DoLocalSelectionChange();
                            }

                            if (evt.button == 1 && SelType == SelectionType.DeletedItems)
                            {
                                GUIUtility.hotControl = 0;
                                Rect r = new Rect(evt.mousePosition.x, evt.mousePosition.y, 1, 1);
                                EditorUtility.DisplayCustomMenu(r, dropDownMenuItems, -1, ContextMenuClick, null);
                            }

                            Event.current.Use();
                        }

                        if (fileI != -1)
                        {
                            content.text = folder.files[fileI].name;
                            content.image = InternalEditorUtility.GetIconForFile(folder.files[fileI].name);
                            indent = (int)(m_BaseIndent + m_Indent * 2);
                        }
                        else
                        {
                            content.text = folder.name;
                            content.image = folderIcon;
                            indent = (int)(m_BaseIndent + m_Indent);
                        }

                        s.padding.left = indent;

                        if (Event.current.type == EventType.Repaint)
                        {
                            s.Draw(selectionRect, content,
                                    false, false,
                                    m_DelPVstate.selectedItems[currRow],
                                    focused);
                        }

                    m_DelPVstate.NextFileFolder(ref folderI, ref fileI);
                    currRow++;
                }

                row++;
                offset += m_RowHeight;
            }
        }

        public void DoGUI(ASHistoryWindow parentWin, Rect theRect, bool focused)
        {
            if (ms_Styles == null)
            {
                ms_Styles = new Styles();
            }

            m_ScreenRect = theRect;

            Hashtable selection = new Hashtable();
            foreach (Object obj in Selection.objects)
            {
                selection.Add(obj.GetInstanceID(), null);
            }

            m_FileViewControlID = GUIUtility.GetControlID(ms_FileViewHash, FocusType.Native);
            KeyboardGUI(parentWin);

            focused &= GUIUtility.keyboardControl == m_FileViewControlID;

            HierarchyProperty property = new HierarchyProperty(HierarchyType.Assets);

            int elements = property.CountRemaining(m_ExpandedArray);

            int delCount = DeletedItemsToggle ? m_DelPVstate.lv.totalRows : 0;
            Rect contentRect = new Rect(0, 0, 1, (elements + 2 + delCount) * m_RowHeight + 16);

            m_ScrollPosition = GUI.BeginScrollView(m_ScreenRect, m_ScrollPosition, contentRect);

            theRect.width = contentRect.height > m_ScreenRect.height ? theRect.width - 18 : theRect.width; // take scrollBar into account

            int invisibleRows = Mathf.RoundToInt(m_ScrollPosition.y - 6 - m_RowHeight) / Mathf.RoundToInt(m_RowHeight);
            if (invisibleRows > elements)
            {
                invisibleRows = elements;
            }
            else
            if (invisibleRows < 0)
            {
                invisibleRows = 0;
                m_ScrollPosition.y = 0;
            }

            float offset = 0;

            GUIContent content = new GUIContent();
            Event evt = Event.current;
            Rect selectionRect;
            Rect foldoutRect;
            int indent;

            GUIStyle s = new GUIStyle(ms_Styles.label);

            Texture2D folderIcon = EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);

            offset = invisibleRows * m_RowHeight + 3;

            float endOffset = m_ScreenRect.height + m_ScrollPosition.y;

            selectionRect = new Rect(0, offset, theRect.width, m_RowHeight);

            if (evt.type == EventType.MouseDown && selectionRect.Contains(evt.mousePosition))
            {
                SelType = SelectionType.All;
                GUIUtility.keyboardControl = m_FileViewControlID;
                //parentWin.m_SearchToShow = ASHistoryWindow.ShowSearchField.ProjectView;
                ScrollTo(0);
                parentWin.DoLocalSelectionChange();
                evt.Use();
            }

            content = new GUIContent("Entire Project");
            content.image = folderIcon;
            indent = (int)(m_BaseIndent);
            s.padding.left = 3;
			
			if (Event.current.type == EventType.repaint)
			{
	            s.Draw(selectionRect, content,
                    false, false,
                    SelType == SelectionType.All,
                    focused);
			}

            offset += m_RowHeight + 3;

            property.Reset();
            property.Skip(invisibleRows, m_ExpandedArray);

            //bool firstRow = true;

            while (property.Next(m_ExpandedArray) && offset <= endOffset)
            {
                int instanceID = property.instanceID;

                /*if (firstRow)
                {

                    selectionRect = new Rect(theRect.width - 27, offset - 2, 27, m_RowHeight);
                    GUI.Toggle(selectionRect, false, ms_SearchIcon, ms_Styles.toolbarButton);
                    selectionRect = new Rect(0, offset, theRect.width - 27, m_RowHeight);
                    firstRow = false;
                }
                else*/
                    selectionRect = new Rect(0, offset, theRect.width, m_RowHeight);

                //int rowSelectionControlID = ControlIDForProperty(property);

                // Draw icon & name
                if (Event.current.type == EventType.Repaint)
                {
                    content.text = property.name;
                    content.image = property.icon;
                    indent = (int)(m_BaseIndent + m_Indent * property.depth);
                    s.padding.left = indent;

                    bool selected = selection.Contains(instanceID); //   || GUIUtility.hotControl == rowSelectionControlID 

                    s.Draw(selectionRect, content,
                            false, false,
                            selected,
                            focused);
                }

                // Draw foldout
                if (property.hasChildren)
                {
                    bool expanded = property.IsExpanded(m_ExpandedArray);

                    GUI.changed = false;
                    foldoutRect = new Rect(m_BaseIndent + m_Indent * property.depth - m_FoldoutSize, offset, m_FoldoutSize, m_RowHeight);

                    expanded = GUI.Toggle(foldoutRect, expanded, GUIContent.none, ms_Styles.foldout);

                    // When clicking toggle, add instance id to expanded list
                    if (GUI.changed)
                    {
                        // Recursive set expanded
                        if (Event.current.alt)
                        {
                            SetExpandedRecurse(instanceID, expanded);
                        }
                        // Expand one element only
                        else
                        {
                            SetExpanded(instanceID, expanded);
                        }
                    }
                }

                // Handle mouse down based selection change
                if (evt.type == EventType.MouseDown && Event.current.button == 0 && selectionRect.Contains(Event.current.mousePosition))
                {
                    GUIUtility.keyboardControl = m_FileViewControlID;
                    //parentWin.m_SearchToShow = ASHistoryWindow.ShowSearchField.ProjectView;

                    // Open Asset
                    if (Event.current.clickCount == 2)
                    {
                        AssetDatabase.OpenAsset(instanceID);
                        GUIUtility.ExitGUI();
                    }
                    else
                    {
                        if (selectionRect.Contains(evt.mousePosition))
                            SelectionClick(property);
                    }
                    evt.Use();
                }

                offset += m_RowHeight;
            }

            offset += 3;

            DoDeletedItemsGUI(parentWin, theRect, s, offset, endOffset, focused);

            GUI.EndScrollView();
            
            switch (evt.type)
            {
                case EventType.MouseDown:
                    if (evt.button == 0 && m_ScreenRect.Contains(evt.mousePosition))
                    {
                        GUIUtility.hotControl = m_FileViewControlID;
                        evt.Use();
                    }
                    break;
                case EventType.MouseUp:
                    // Handle mouse up on empty area, select nothing
                    if (GUIUtility.hotControl == m_FileViewControlID)
                    {
                        if (m_ScreenRect.Contains(evt.mousePosition))
                            Selection.activeObject = null;

                        GUIUtility.hotControl = 0;
                        evt.Use();
                    }
                    break;
            }

			HandleFraming();
        }

		private void HandleFraming()
		{
			if ((Event.current.type == EventType.ExecuteCommand || Event.current.type == EventType.ValidateCommand) && Event.current.commandName == "FrameSelected")
			{
				if (Event.current.type == EventType.ExecuteCommand)
					DoFramingMindSelectionType();

				HandleUtility.Repaint();
				Event.current.Use();
			}
		}

		private void DoFramingMindSelectionType()
		{
			switch (m_SelType)
			{
				case SelectionType.All:
					ScrollTo(0);
					break;
				case SelectionType.Items:
					FrameObject(Selection.activeObject);
					break;
				case SelectionType.DeletedItemsRoot:
					ScrollToDeletedItem(-1);
					break;
				case SelectionType.DeletedItems:
					ScrollToDeletedItem(m_DelPVstate.lv.row);
					break;
			}
		}

		private List<int> GetOneFolderImplicitSelection(HierarchyProperty property, Hashtable selection, bool rootSelected, ref bool retHasSelectionInside, out bool eof)
		{
			// for each folder. If folder is selected and none of it's assets are selected - all assets in folder are selected
			// if at least one asset selected - only selected assets go into selection + folder asset itself
			int depth = property.depth;
			bool hasSelectionInside = false;
			bool prevWasSelected = false;
			eof = false;

			List<int> allItems = new List<int>();
			List<int> selectedItems = new List<int>();
			List<int> deeperFolderItems = new List<int>();

			do
			{
				if (property.depth > depth)
				{
					deeperFolderItems.AddRange(GetOneFolderImplicitSelection(property, selection, rootSelected || prevWasSelected, ref hasSelectionInside, out eof));
				}

				if (depth != property.depth || eof)
					break;

				if (rootSelected && !hasSelectionInside) // otherwise won't use this list anyway
					allItems.Add(property.instanceID);

				if (selection.Contains(property.instanceID))
				{
					selectedItems.Add(property.instanceID);
					hasSelectionInside = true;
					prevWasSelected = true;
				}
				else
					prevWasSelected = false;

				eof = !property.Next(null);
			} 
			while (!eof);

			retHasSelectionInside |= hasSelectionInside;

			List<int> items = !rootSelected || hasSelectionInside ? selectedItems : allItems;
			items.AddRange(deeperFolderItems);

			return items;
		}

		public string[] GetImplicitProjectViewSelection()
		{
			HierarchyProperty property = new HierarchyProperty(HierarchyType.Assets);
			bool hasSelectionInside = false;
			bool eof;

			if (!property.Next(null))
				return new string[0];

			Hashtable selection = new Hashtable();
			foreach (Object obj in Selection.objects)
			{
				selection.Add(obj.GetInstanceID(), null);
			}

			List<int> items = GetOneFolderImplicitSelection(property, selection, false, ref hasSelectionInside, out eof);

			string[] selectionGUIDs = new string[items.Count];

			for (int i = 0; i < selectionGUIDs.Length; i++)
			{
				selectionGUIDs[i] = AssetDatabase.AssetPathToGUID(AssetDatabase.GetAssetPath (items[i])); // FIXME: that's how it was done in old AS, still doesn't look right...
			}

			return selectionGUIDs;
		}
    }
}
