using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class ProfilerHierarchyGUI
	{
		#region State and const
		static int hierarchyViewHash = "HierarchyView".GetHashCode();

		// Profiling GUI constants
		const float kRowHeight = 16;
		const float kFoldoutSize = 14;
		const float kIndent = 16;
		const float kSmallMargin = 4;
		const float kBaseIndent = 4;
		const int kFirst = -999999;
		const int kLast = 999999;
		const float kScrollbarWidth = 16;

		internal class Styles
		{
			public GUIStyle background = "OL Box";
			public GUIStyle header = "OL title";
			public GUIStyle rightHeader = "OL title TextRight";
			public GUIStyle entryEven = "OL EntryBackEven";
			public GUIStyle entryOdd = "OL EntryBackOdd";
			public GUIStyle numberLabel = "OL Label";
			public GUIStyle foldout = "IN foldout";
		}

		private static Styles ms_Styles;
		private static Styles styles
		{
			get { return ms_Styles ?? (ms_Styles = new Styles()); }
		}

		private IProfilerWindowController m_Window;
		private SplitterState m_Splitter = null;
		private ProfilerColumn[] m_ColumnsToShow;
		private string[] m_ColumnNames;
		private bool[] m_VisibleColumns;
		
		private float[] m_SplitterRelativeSizes;
		private int[] m_SplitterMinWidths;
		private string m_ColumnSettingsName;
		private Vector2 m_TextScroll = Vector2.zero;

		private GUIContent[] m_HeaderContent;
		// Hash table to keep track of expanded elements
		private SerializedStringTable m_ExpandedHash = new SerializedStringTable();
		private bool m_ExpandAll;

		private int m_ScrollViewHeight;
		private int m_DoScroll;
		private int m_SelectedIndex = -1;
		private bool m_DetailPane;

		public int selectedIndex
		{
			get { return m_SelectedIndex; }
			set { m_SelectedIndex = value; }
		}

		private ProfilerColumn m_SortType = ProfilerColumn.TotalTime;

		public ProfilerColumn sortType
		{
			get { return m_SortType; }
			private set { m_SortType = value; }
		}

		private string m_DetailViewSelectedProperty = string.Empty;
		#endregion

		public ProfilerHierarchyGUI(IProfilerWindowController window, string columnSettingsName, ProfilerColumn[] columnsToShow, string[] columnNames, bool detailPane, ProfilerColumn sort)
		{
			m_Window = window;
			m_ColumnNames = columnNames;
			m_ColumnSettingsName = columnSettingsName;
			m_ColumnsToShow = columnsToShow;
			m_DetailPane = detailPane;
			m_SortType = sort;
			m_HeaderContent = new GUIContent[columnNames.Length];
			m_Splitter = null;
			for (int i = 0; i < m_HeaderContent.Length; i++)
				m_HeaderContent[i] = EditorGUIUtility.TextContent(m_ColumnNames[i]);
			
			if (columnsToShow.Length != columnNames.Length)
				throw new ArgumentException("Number of columns to show does not match number of column names.");

			
			m_VisibleColumns = new bool[columnNames.Length];

			for (int i = 0; i < m_VisibleColumns.Length; i++)
				m_VisibleColumns[i] = true;
		}

		private void SetupSplitter()
		{
			if (m_Splitter != null && m_SplitterMinWidths != null)
				return;

			// Initialize Splitter sizes
			m_SplitterRelativeSizes = new float[m_ColumnNames.Length];
			m_SplitterMinWidths = new int[m_ColumnNames.Length];
			for (int i = 0; i < m_SplitterRelativeSizes.Length; i++)
			{
				//				styles.header.CalcSize(m_HeaderContent[i]).x
				//@TODO: Extract text string size to automatically come up with minimum size values
				m_SplitterMinWidths[i] = (int)styles.header.CalcSize(m_HeaderContent[i]).x;
				m_SplitterRelativeSizes[i] = 70;
				if (m_HeaderContent[i].image != null)
				{
					m_SplitterRelativeSizes[i] = 1;
				}
			}

			if (m_ColumnsToShow[0] == ProfilerColumn.FunctionName)
			{
				m_SplitterRelativeSizes[(int)ProfilerColumn.FunctionName] = 400;
				m_SplitterMinWidths[(int)ProfilerColumn.FunctionName] = 100;
			}

			m_Splitter = new SplitterState(m_SplitterRelativeSizes, m_SplitterMinWidths, null);

			// Disable Colums by parsing the enabled colums string
			string str = EditorPrefs.GetString(m_ColumnSettingsName);
			for (int i = 0; i < m_ColumnNames.Length; i++)
				if (i < str.Length && str[i] == '0')
					SetColumnVisible(i, false);
		}
		
		public ProfilerProperty GetDetailedProperty(ProfilerProperty property)
		{
			var expanded = true;

			string selectedPropertyPath = ProfilerDriver.selectedPropertyPath;
			while (property.Next(expanded))
			{
				string propertyPath = property.propertyPath;
				if (propertyPath == selectedPropertyPath)
				{
					var detailProperty = new ProfilerProperty();
					detailProperty.InitializeDetailProperty(property);
					return detailProperty;
				}

				if (property.HasChildren)
					expanded = IsExpanded(propertyPath);
			}

			return null;
		}

		void DoScroll()
		{
			m_DoScroll = 2;
		}

		void MoveSelection(int steps)
		{
			int needRow = m_SelectedIndex + steps;

			if (needRow < 0)
				needRow = 0;

			var property  = m_Window.CreateProperty(m_DetailPane);

			if (m_DetailPane)
			{
				var detailproperty = GetDetailedProperty(property);
				property.Cleanup();
				property = detailproperty;
			}

			if (property == null)
				return;

			bool expanded = true;
			int currentRow = 0;
			int lastInstanceID = -1;

			while (property.Next(expanded))
			{
				if (m_DetailPane && property.instanceIDs != null && property.instanceIDs.Length > 0 && property.instanceIDs[0] != 0)
					lastInstanceID = property.instanceIDs[0];

				if (currentRow == needRow)
					break;

				if (property.HasChildren)
					expanded = !m_DetailPane && IsExpanded(property.propertyPath);

				currentRow++;
			}

			if (m_DetailPane)
				m_DetailViewSelectedProperty = DetailViewSelectedPropertyPath(property, lastInstanceID);
			else
				m_Window.SetSelectedPropertyPath(property.propertyPath);

			property.Cleanup();
		}

		void SetExpanded(string expandedName, bool expanded)
		{
			if (expanded != IsExpanded(expandedName))
			{
				if (expanded)
				{
					m_ExpandedHash.Set(expandedName);
				}
				else
				{
					m_ExpandedHash.Remove(expandedName);
				}
			}
		}

		void HandleKeyboard(int id)
		{
			Event evt = Event.current;
			if (evt.GetTypeForControl(id) != EventType.KeyDown ||
				id != GUIUtility.keyboardControl)
				return;

			switch (evt.keyCode)
			{
				case KeyCode.UpArrow:
					MoveSelection(-1);
					break;
				case KeyCode.DownArrow:
					MoveSelection(1);
					break;
				case KeyCode.Home:
					MoveSelection(kFirst);
					break;
				case KeyCode.End:
					MoveSelection(kLast);
					break;
				case KeyCode.LeftArrow:
					SetExpanded(ProfilerDriver.selectedPropertyPath, false);
					break;
				case KeyCode.RightArrow:
					SetExpanded(ProfilerDriver.selectedPropertyPath, true);
					break;

				case KeyCode.PageUp:
					if (Application.platform == RuntimePlatform.OSXEditor)
					{
						m_TextScroll.y -= m_ScrollViewHeight;

						if (m_TextScroll.y < 0)
							m_TextScroll.y = 0;

						evt.Use();
						return;
					}

					MoveSelection(-Mathf.RoundToInt(m_ScrollViewHeight / kRowHeight));
					break;
				case KeyCode.PageDown:
					if (Application.platform == RuntimePlatform.OSXEditor)
					{
						m_TextScroll.y += m_ScrollViewHeight;
						evt.Use();
						return;
					}

					MoveSelection(Mathf.RoundToInt(m_ScrollViewHeight / kRowHeight));
					break;
				default:
					return;
			}

			DoScroll();
			evt.Use();
		}

		void DrawColumnsHeader()
		{
			var eventIsRightMouseDown = false;

			GUILayout.BeginHorizontal();

			// right clicks would get eaten by toggles, do this right before drawing header
			if (Event.current.type == EventType.MouseDown && Event.current.button == 1)
			{
				eventIsRightMouseDown = true;
				Event.current.type = EventType.Used;
			}

			SplitterGUILayout.BeginHorizontalSplit(m_Splitter, EditorStyles.toolbar);
			for (int i = 0; i < m_ColumnNames.Length; i++)
				DrawTitle(m_HeaderContent[i], i);
			SplitterGUILayout.EndHorizontalSplit();

			GUILayout.Box(string.Empty, ms_Styles.header, GUILayout.Width(kScrollbarWidth));

			GUILayout.EndHorizontal();

			if (eventIsRightMouseDown)
			{
				Event.current.type = EventType.MouseDown;
				HandleHeaderMouse(GUILayoutUtility.GetLastRect());
			}
		}

		bool IsExpanded(string expanded)
		{
			if (m_ExpandAll)
				return true;
			return m_ExpandedHash.Contains(expanded);
		}

		void SetExpanded(ProfilerProperty property, bool expanded)
		{
			SetExpanded(property.propertyPath, expanded);
		}

		int DrawProfilingData(ProfilerProperty property, int id)
		{
			m_SelectedIndex = -1;

			bool expanded = true;
			int rowCount = 0;
			string selectedPropertyPath = ProfilerDriver.selectedPropertyPath;

			while (property.Next(expanded))
			{
				string propertyPath = property.propertyPath;
				bool selected = m_DetailPane ?
					m_DetailViewSelectedProperty != string.Empty && m_DetailViewSelectedProperty == DetailViewSelectedPropertyPath(property) :
					propertyPath == selectedPropertyPath;

				if (selected)
					m_SelectedIndex = rowCount;

				// Layouting doesn't need to draw, also we can cull invisible lines
				bool needDrawing = Event.current.type != EventType.Layout;

				// m_ScrollViewHeight is not initialized on first pass
				needDrawing &= m_ScrollViewHeight == 0 ||
				               (rowCount*kRowHeight <= m_ScrollViewHeight + m_TextScroll.y) &&
				               ((rowCount + 1)*kRowHeight > m_TextScroll.y);

				if (needDrawing)
					expanded = DrawProfileDataItem(property, rowCount, selected, id);
				else
					expanded = property.HasChildren && IsExpanded(propertyPath);

				++rowCount;
			}
			return rowCount;
		}

		void UnselectIfClickedOnEmptyArea(int rowCount)
		{
			var r = GUILayoutUtility.GetRect(GUIClip.visibleRect.width, kRowHeight * rowCount, GUILayout.MinHeight(kRowHeight * rowCount));

			if (Event.current.type != EventType.MouseDown ||
				Event.current.mousePosition.y <= r.y ||
				Event.current.mousePosition.y >= Screen.height)
				return;

			if (!m_DetailPane)
				m_Window.ClearSelectedPropertyPath();
			else
				m_DetailViewSelectedProperty = string.Empty;
			Event.current.Use();
		}

		void HandleHeaderMouse(Rect columnHeaderRect)
		{
			var evt = Event.current;

			// right clicks would get eaten by toggles, do this right before drawing header
			if (evt.type != EventType.MouseDown || evt.button != 1 || !columnHeaderRect.Contains(evt.mousePosition))
				return;

			GUIUtility.hotControl = 0;
			var rect = new Rect(Event.current.mousePosition.x, Event.current.mousePosition.y, 1, 1);

			EditorUtility.DisplayCustomMenu(rect, m_ColumnNames, GetVisibleDropDownIndexList(), ColumnContextClick, null);
			evt.Use();
		}

		void SetColumnVisible(int index, bool enabled)
		{
			SetupSplitter();

			// Name column is always enabled
			if (index == 0)
				return;

			if (m_VisibleColumns[index] == enabled)
				return;

			m_VisibleColumns[index] = enabled;

			int splitColIndex = 0;
			for (int i = 0; i < index; i++)
			{
				if (ColIsVisible(i))
					splitColIndex++;
			}

			if (enabled)
			{
				ArrayUtility.Insert(ref m_Splitter.relativeSizes, splitColIndex, m_SplitterRelativeSizes[index]);
				ArrayUtility.Insert(ref m_Splitter.minSizes, splitColIndex, m_SplitterMinWidths[index]);
			}
			else
			{
				ArrayUtility.RemoveAt(ref m_Splitter.relativeSizes, splitColIndex);
				ArrayUtility.RemoveAt(ref m_Splitter.minSizes, splitColIndex);
			}

			m_Splitter = new SplitterState(m_Splitter.relativeSizes, m_Splitter.minSizes, null);

			SaveColumns();
		}

		int[] GetVisibleDropDownIndexList()
		{
			var selected = new List<int>();
			for (int i = 0; i < m_ColumnNames.Length; i++)
			{
				if (m_VisibleColumns[i])
					selected.Add(i);
			}

			return selected.ToArray();
		}

		void SaveColumns()
		{
			string str = string.Empty;

			for (int i = 0; i < m_VisibleColumns.Length; i++)
				str += ColIsVisible(i) ? '1' : '0';

			EditorPrefs.SetString(m_ColumnSettingsName, str);
		}

		bool ColIsVisible(int index)
		{
			if (index < 0 || index > m_VisibleColumns.Length)
				return false;

			return m_VisibleColumns[index];
		}

		void ColumnContextClick(object userData, string[] options, int selected)
		{
			SetColumnVisible(selected, !ColIsVisible(selected));
		}

		void DrawTextColumn(ref Rect currentRect, string text, int index, float margin, bool selected)
		{
			if (index != 0)
				currentRect.x += m_Splitter.realSizes[index - 1];

			currentRect.x += margin;
			currentRect.width = m_Splitter.realSizes[index] - margin;
			styles.numberLabel.Draw(currentRect, text, false, false, false, selected);

			currentRect.x -= margin;
		}

		static string DetailViewSelectedPropertyPath(ProfilerProperty property)
		{
			if (property == null || property.instanceIDs == null || property.instanceIDs.Length == 0 || property.instanceIDs[0] == 0)
				return string.Empty;
			return DetailViewSelectedPropertyPath(property, property.instanceIDs[0]);
		}

		static string DetailViewSelectedPropertyPath(ProfilerProperty property, int instanceId)
		{
			return property.propertyPath + "/" + instanceId;
		}

		bool DrawProfileDataItem(ProfilerProperty property, int rowCount, bool selected, int id)
		{
			bool expanded = false;

			Event evt = Event.current;

			var currentRect = new Rect(1, kRowHeight * rowCount, GUIClip.visibleRect.width, kRowHeight);
			var r = currentRect;

			var background = (rowCount % 2 == 0 ? styles.entryEven : styles.entryOdd);

			if (evt.type == EventType.Repaint)
				background.Draw(r, GUIContent.none, false, false, selected, false);

			float firstIndent = property.depth * kIndent + kBaseIndent;

			if (property.HasChildren)
			{
				expanded = IsExpanded(property.propertyPath);

				GUI.changed = false;

				firstIndent -= kFoldoutSize;

				var foldoutRect = new Rect(firstIndent, r.y, kFoldoutSize, kRowHeight);
				expanded = GUI.Toggle(foldoutRect, expanded, GUIContent.none, styles.foldout);

				if (GUI.changed)
					SetExpanded(property, expanded);

				firstIndent += kIndent;
			}

			if (evt.type == EventType.Repaint)
			{
				DrawTextColumn(ref r, property.GetColumn(m_ColumnsToShow[0]), 0,
					m_ColumnsToShow[0] == ProfilerColumn.FunctionName ? firstIndent : kSmallMargin, selected);

				styles.numberLabel.alignment = TextAnchor.MiddleRight;
				int sizerIndex = 1;

				for (var i = 1; i < m_VisibleColumns.Length; i++)
				{
					if (!ColIsVisible(i))
						continue;

					r.x += m_Splitter.realSizes[sizerIndex - 1];
					r.width = m_Splitter.realSizes[sizerIndex] - kSmallMargin;
					sizerIndex++;

					styles.numberLabel.Draw(r, property.GetColumn(m_ColumnsToShow[i]), false, false, false, selected);
				}

				styles.numberLabel.alignment = TextAnchor.MiddleLeft;
			}

			// Handle mouse events
			if (evt.type == EventType.MouseDown)
			{
				if (currentRect.Contains(evt.mousePosition))
				{
					GUIUtility.hotControl = 0;

					if (!EditorGUI.actionKey)
					{
						if (m_DetailPane)
						{
							if (evt.clickCount == 1 && property.instanceIDs.Length > 0)
							{
								var newSelection = DetailViewSelectedPropertyPath(property);
								if (m_DetailViewSelectedProperty != newSelection)
								{
									m_DetailViewSelectedProperty = newSelection;

									var obj = EditorUtility.InstanceIDToObject(property.instanceIDs[0]);
									if (obj is Component)
										obj = ((Component) obj).gameObject;

									EditorGUIUtility.PingObject(obj.GetInstanceID());
								}
								else
									m_DetailViewSelectedProperty = string.Empty;
							}
							else
							if (evt.clickCount == 2)
							{
								SelectObjectsInHierarchyView(property);
								m_DetailViewSelectedProperty = DetailViewSelectedPropertyPath(property);
							}
						}
						else
						{
							if (property.propertyPath == ProfilerDriver.selectedPropertyPath)
							{
								m_Window.ClearSelectedPropertyPath();
							}
							else
							{
								m_Window.SetSelectedPropertyPath(property.propertyPath);
							}
						}

						DoScroll();
					}
					else
					{
						if (!m_DetailPane)
							m_Window.ClearSelectedPropertyPath();
						else
							m_DetailViewSelectedProperty = string.Empty;
					}

					GUIUtility.keyboardControl = id;

					evt.Use();
				}
			}

			if (selected && GUIUtility.keyboardControl == id && evt.type == EventType.KeyDown && (evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter))
				SelectObjectsInHierarchyView(property);

			return expanded;
		}

		static void SelectObjectsInHierarchyView(ProfilerProperty property)
		{
			var instanceIDs = property.instanceIDs;
			var selection = new List<Object>();

			foreach (int t in instanceIDs)
			{
				var obj = EditorUtility.InstanceIDToObject(t);
				var com = obj as Component;
				if (com != null)
					selection.Add(com.gameObject);
				else
					if (obj != null)
						selection.Add(obj);
			}

			if (selection.Count != 0)
				Selection.objects = selection.ToArray();
		}

		void DrawTitle(GUIContent name, int index)
		{
			if (!ColIsVisible(index)) 
				return;

			ProfilerColumn type = m_ColumnsToShow[index];

			bool isColumnSelected = sortType == type;

			// Display header (Function name is on the left, other columns are right aligned)
			bool didSelectColum = (index == 0) ? 
				GUILayout.Toggle(isColumnSelected, name, styles.header) : 
				GUILayout.Toggle(isColumnSelected, name, styles.rightHeader, GUILayout.Width(m_SplitterMinWidths[index]));

			// Select the column if it's not already selected
			if (didSelectColum)
				sortType = type;
		}

		void DoScrolling()
		{
			if (m_DoScroll > 0)
			{
				m_DoScroll--;

				if (m_DoScroll == 0)
				{
					float scrollTop = kRowHeight * m_SelectedIndex;
					float scrollBottom = scrollTop - m_ScrollViewHeight + kRowHeight;

					m_TextScroll.y = Mathf.Clamp(m_TextScroll.y, scrollBottom, scrollTop);
				}
				else
					m_Window.Repaint();
			}
		}

		public void DoGUI(ProfilerProperty property, bool expandAll)
		{
			m_ExpandAll = expandAll;

			SetupSplitter();

			DoScrolling();

			int id = GUIUtility.GetControlID(hierarchyViewHash, FocusType.Passive);

			DrawColumnsHeader();

			m_TextScroll = EditorGUILayout.BeginScrollView(m_TextScroll, ms_Styles.background);

			int rowCount = DrawProfilingData (property, id);

			property.Cleanup();

			UnselectIfClickedOnEmptyArea(rowCount);

			if (Event.current.type == EventType.Repaint)
				m_ScrollViewHeight = (int)GUIClip.visibleRect.height;

			GUILayout.EndScrollView();

			HandleKeyboard(id);
		}
	}
}
