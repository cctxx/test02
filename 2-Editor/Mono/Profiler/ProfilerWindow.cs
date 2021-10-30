//#define PERF_PROFILE

using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;
using System.Collections;

namespace UnityEditor
{
internal class ProfilerWindow : EditorWindow, IProfilerWindowController
{
	#region Locals
	internal class Styles
	{
		public GUIContent addArea = EditorGUIUtility.TextContent ("Profiler.AddArea");
		public GUIContent deepProfile = EditorGUIUtility.TextContent("Profiler.DeepProfile");
		public GUIContent profileEditor = EditorGUIUtility.TextContent("Profiler.ProfileEditor");
		public GUIContent noData = EditorGUIUtility.TextContent("Profiler.NoFrameDataAvailable");
		public GUIContent noLicense = EditorGUIUtility.TextContent("Profiler.NoLicense");

		public GUIContent profilerRecord = EditorGUIUtility.TextContent ("Profiler.Record");
		public GUIContent prevFrame = EditorGUIUtility.IconContent ("Profiler.PrevFrame");
		public GUIContent nextFrame = EditorGUIUtility.IconContent ("Profiler.NextFrame");
		public GUIContent currentFrame = EditorGUIUtility.TextContent ("Profiler.CurrentFrame");
		public GUIContent frame = EditorGUIUtility.TextContent ("Frame: ");
		public GUIContent clearData = EditorGUIUtility.TextContent ("Clear");
		public GUIContent[] reasons = EditorGUIUtility.GetTextContentsForEnum (typeof (MemoryInfoGCReason));

		public GUIStyle background = "OL Box";
		public GUIStyle header = "OL title";
		public GUIStyle label = "OL label";
		public GUIStyle entryEven = "OL EntryBackEven";
		public GUIStyle entryOdd = "OL EntryBackOdd";
		public GUIStyle foldout = "IN foldout";
		public GUIStyle profilerGraphBackground = "ProfilerScrollviewBackground";

		public Styles () 
		{
			profilerGraphBackground.overflow.left = -(int)Chart.kSideWidth;
		}
	}

	private static Styles ms_Styles;

	private bool m_HasProfilerLicense;

	private SplitterState m_VertSplit = new SplitterState(new[] { 50f, 50f }, new[] { 50, 50 }, null);
	private SplitterState m_ViewSplit = new SplitterState(new[] { 70f, 30f }, new[] { 450, 50 }, null);

	// For keeping correct "Recording" state on window maximizing
	[SerializeField]
	private bool m_Recording = true;

	private AttachProfilerUI m_AttachProfilerUI = new AttachProfilerUI();

	private Vector2 m_GraphPos = Vector2.zero;
	private Vector2[] m_PaneScroll = new Vector2[(int)ProfilerArea.AreaCount];

	static List<ProfilerWindow> m_ProfilerWindows = new List<ProfilerWindow> ();

	private ProfilerViewType m_ViewType = ProfilerViewType.Hierarchy;
	private ProfilerArea m_CurrentArea = ProfilerArea.CPU;
	private ProfilerMemoryView m_ShowDetailedMemoryPane = ProfilerMemoryView.Simple;

	private int m_CurrentFrame = -1;
	private int m_LastFrameFromTick = -1;
	private int m_PrevLastFrame = -1;

	// Profiler charts
	private ProfilerChart[] m_Charts;
	private float[] m_ChartOldMax = new[] { -1.0f, -1.0f };
	private float m_ChartMaxClamp = 70000.0f;

	// Profiling GUI constants
	const float kRowHeight = 16;
	const float kIndentPx = 16;
	const float kBaseIndent = 8;
	const float kSmallMargin = 4;

	const float kNameColumnSize = 350;
	const float kColumnSize = 80;

	const float kFoldoutSize = 14;
	const int kFirst = -999999;
	const int kLast = 999999;

	private ProfilerHierarchyGUI m_CPUHierarchyGUI;
	private ProfilerHierarchyGUI m_GPUHierarchyGUI;
	private ProfilerHierarchyGUI m_CPUDetailHierarchyGUI;
	private ProfilerHierarchyGUI m_GPUDetailHierarchyGUI;
	private ProfilerTimelineGUI m_CPUTimelineGUI;

	private MemoryTreeList m_ReferenceListView;
	private MemoryTreeListClickable m_MemoryListView;

	bool wantsMemoryRefresh { get { return m_MemoryListView.GetRoot() == null; } }

	void BuildColumns ()
	{
		var cpuHierarchyColumns = new[] {
			ProfilerColumn.FunctionName, ProfilerColumn.TotalPercent, ProfilerColumn.SelfPercent,
		    ProfilerColumn.Calls, ProfilerColumn.GCMemory, ProfilerColumn.TotalTime,
		    ProfilerColumn.SelfTime, ProfilerColumn.WarningCount };

		var cpuDetailColumns = new[] { ProfilerColumn.ObjectName, ProfilerColumn.TotalPercent, ProfilerColumn.SelfPercent,
		    ProfilerColumn.Calls, ProfilerColumn.GCMemory, ProfilerColumn.TotalTime, ProfilerColumn.SelfTime };

		m_CPUHierarchyGUI = new ProfilerHierarchyGUI (this, kProfilerColumnSettings, cpuHierarchyColumns, ProfilerColumnNames (cpuHierarchyColumns), false, ProfilerColumn.TotalTime);
		m_CPUTimelineGUI = new ProfilerTimelineGUI (this);

		var objectText = EditorGUIUtility.TextContent("ProfilerColumn.DetailViewObject").text;

		var names = ProfilerColumnNames(cpuDetailColumns);
		names[0] = objectText;
		m_CPUDetailHierarchyGUI = new ProfilerHierarchyGUI (this, kProfilerDetailColumnSettings, cpuDetailColumns, names, true, ProfilerColumn.TotalTime);

		var gpuHierarchyColumns = new[] { ProfilerColumn.FunctionName, ProfilerColumn.TotalGPUPercent, ProfilerColumn.DrawCalls, ProfilerColumn.TotalGPUTime };
		var gpuDetailColumns = new[] { ProfilerColumn.ObjectName, ProfilerColumn.TotalGPUPercent, ProfilerColumn.DrawCalls, ProfilerColumn.TotalGPUTime };

		m_GPUHierarchyGUI = new ProfilerHierarchyGUI (this, kProfilerGPUColumnSettings, gpuHierarchyColumns, ProfilerColumnNames (gpuHierarchyColumns), false, ProfilerColumn.TotalGPUTime);

		names = ProfilerColumnNames(gpuDetailColumns);
		names[0] = objectText;
		m_GPUDetailHierarchyGUI = new ProfilerHierarchyGUI (this, kProfilerGPUDetailColumnSettings, gpuDetailColumns, names, true, ProfilerColumn.TotalGPUTime);
	}

	private static string[] ProfilerColumnNames(ProfilerColumn[] columns)
	{
		var allNames = Enum.GetNames(typeof(ProfilerColumn));
		var names = new string[columns.Length];

		for (var i = 0; i < columns.Length; i++)
			names[i] = "ProfilerColumn." + allNames[(int)columns[i]];

		return names;
	}

	const string kProfilerColumnSettings = "VisibleProfilerColumnsV2";
	const string kProfilerDetailColumnSettings = "VisibleProfilerDetailColumns";
	const string kProfilerGPUColumnSettings = "VisibleProfilerGPUColumns";
	const string kProfilerGPUDetailColumnSettings = "VisibleProfilerGPUDetailColumns";
	const string kProfilerVisibleGraphsSettings = "VisibleProfilerGraphs";
	internal const string kPrefCharts = "ProfilerChart";
	#endregion

	#region IProfilerWindowController
	public void SetSelectedPropertyPath(string path)
	{
		if (ProfilerDriver.selectedPropertyPath != path)
		{
			ProfilerDriver.selectedPropertyPath = path;
			UpdateCharts();
		}
	}

	public void ClearSelectedPropertyPath()
	{
		if (ProfilerDriver.selectedPropertyPath != string.Empty)
		{
			m_CPUHierarchyGUI.selectedIndex = -1;
			ProfilerDriver.selectedPropertyPath = string.Empty;
			UpdateCharts();
		}
	}

	public ProfilerProperty CreateProperty(bool details)
	{
		var property = new ProfilerProperty();
		ProfilerColumn sort =
			m_CurrentArea == ProfilerArea.CPU ?
			(details ? m_CPUDetailHierarchyGUI.sortType : m_CPUHierarchyGUI.sortType) :
			(details ? m_GPUDetailHierarchyGUI.sortType : m_GPUHierarchyGUI.sortType);
		property.SetRoot(GetActiveVisibleFrameIndex(), sort, m_ViewType);
		property.onlyShowGPUSamples = m_CurrentArea == ProfilerArea.GPU;
		return property;
	}
	#endregion

	#region Profiler window functions

	// Constructor
	public ProfilerWindow()
	{
	}

	void OnEnable()
	{
		m_ProfilerWindows.Add(this);
		m_HasProfilerLicense = InternalEditorUtility.HasPro();

		int historySize = ProfilerDriver.maxHistoryLength - 1;

		m_Charts = new ProfilerChart[(int)ProfilerArea.AreaCount];

		Color[] defaultColors = ProfilerColors.colors;

		for (ProfilerArea i = 0; i < ProfilerArea.AreaCount; i++)
		{
			float scale = 1.0f;
			Chart.ChartType chartType = Chart.ChartType.Line;
			string[] statisticsNames = ProfilerDriver.GetGraphStatisticsPropertiesForArea(i);
			int length = statisticsNames.Length;
			if (i == ProfilerArea.GPU || i == ProfilerArea.CPU)
			{
				chartType = Chart.ChartType.StackedFill;
				scale = 1.0f / 1000.0f;
			}

			var chart = new ProfilerChart(i, chartType, scale, length);
			for (int s = 0; s < length; s++)
				chart.m_Series[s] = new ChartSeries(statisticsNames[s], historySize, defaultColors[s]);

			m_Charts[(int)i] = chart;
		}
		
		if (m_ReferenceListView == null)
			m_ReferenceListView = new MemoryTreeList(this, null);
		if (m_MemoryListView == null)
			m_MemoryListView = new MemoryTreeListClickable(this, m_ReferenceListView);
		
		UpdateCharts();
		BuildColumns();
		foreach (var chart in m_Charts)
			chart.LoadAndBindSettings();
	}

	void OnDisable()
	{
		m_ProfilerWindows.Remove(this);		
	}
	
	void Awake ()
	{
		if (! Profiler.supported)
			return;

		// This event gets called every time when some other window is maximized and then unmaximized
		Profiler.enabled = m_Recording;
	}
	
	void OnDestroy ()
	{
		if (Profiler.supported)
			Profiler.enabled = false;
	}

	void OnFocus ()
	{
		// set the real state of profiler. OnDestroy is called not only when window is destroyed, but also when maximized state is changed
		if (Profiler.supported)
			Profiler.enabled = m_Recording;
	}

	void OnLostFocus ()
	{
	}

	static void ShowProfilerWindow()
	{
		EditorWindow.GetWindow<ProfilerWindow>(false);
	}

	static void RepaintAllProfilerWindows()
	{
		foreach (ProfilerWindow window in m_ProfilerWindows)
		{
			if (ProfilerDriver.lastFrameIndex != window.m_LastFrameFromTick)
			{
				window.m_LastFrameFromTick = ProfilerDriver.lastFrameIndex;
				window.RepaintImmediately();
			}
		}
	}

	static void SetMemoryProfilerInfo (ObjectMemoryInfo[] memoryInfo, int[] referencedIndices)
	{
		foreach (var profilerWindow in m_ProfilerWindows)
		{
			if (profilerWindow.wantsMemoryRefresh)
			{
				profilerWindow.m_MemoryListView.SetRoot(MemoryElementDataManager.GetTreeRoot(memoryInfo, referencedIndices));
			}
		}
	}
	#endregion

	#region Functions
	static void SetProfileDeepScripts(bool deep)
	{
		bool currentDeep = ProfilerDriver.deepProfiling;
		if (currentDeep == deep)
			return;
			
		bool doApply = true;

		// When enabling / disabling deep script profiling we need to reload scripts. In play mode this might be intrusive. So ask the user first.
		if (EditorApplication.isPlaying)
		{
			if (deep)
			{
				doApply = EditorUtility.DisplayDialog("Enable deep script profiling", "Enabling deep profiling requires reloading scripts.", 
				"Reload", "Cancel");
			}
			else
			{
				doApply = EditorUtility.DisplayDialog("Disable deep script profiling", "Disabling deep profiling requires reloading all scripts", "Reload", "Cancel");
			}
		}

		
		if (doApply)
		{
			ProfilerDriver.deepProfiling = deep;
			InternalEditorUtility.RequestScriptReload();
		}
	}
	
	string PickFrameLabel()
	{
		if (m_CurrentFrame == -1)
			return "Current";

		return (m_CurrentFrame + 1) + " / " + (ProfilerDriver.lastFrameIndex + 1);
	}
	void PrevFrame()
	{
		int previousFrame = ProfilerDriver.GetPreviousFrameIndex(m_CurrentFrame);
		if (previousFrame != -1)
			SetCurrentFrame(previousFrame);
	}

	void NextFrame()
	{
		int nextFrame = ProfilerDriver.GetNextFrameIndex(m_CurrentFrame);
		if (nextFrame != -1)
			SetCurrentFrame(nextFrame);
	}
	#endregion

	#region DrawCPUPane
	private static void DrawEmptyCPUOrRenderingDetailPane()
	{
		GUILayout.Box(string.Empty, ms_Styles.header);

		GUILayout.BeginHorizontal();
		GUILayout.FlexibleSpace();
		GUILayout.BeginVertical();
		GUILayout.FlexibleSpace();
		GUILayout.Label("Select Line for per-object breakdown", EditorStyles.wordWrappedLabel);
		GUILayout.FlexibleSpace();
		GUILayout.EndVertical();
		GUILayout.FlexibleSpace();
		GUILayout.EndHorizontal();
	}

	void DrawCPUOrRenderingToolbar(ProfilerProperty property)
	{
		EditorGUILayout.BeginHorizontal(EditorStyles.toolbar);

		string[] supportedViewTypes;
		int[] supportedEnumValues;
		if (!Unsupported.IsDeveloperBuild())
		{
			supportedViewTypes = new string[] { "Hierarchy", "Raw Hierarchy" };
			supportedEnumValues = new int[] { (int)ProfilerViewType.Hierarchy, (int)ProfilerViewType.RawHierarchy };
		}
		else
		{
			supportedViewTypes = new string[] { "Hierarchy", "Timeline", "Raw Hierarchy" };
			supportedEnumValues = new int[] { (int)ProfilerViewType.Hierarchy, (int)ProfilerViewType.Timeline, (int)ProfilerViewType.RawHierarchy };

		}

		m_ViewType = (ProfilerViewType)EditorGUILayout.IntPopup((int)m_ViewType, supportedViewTypes, supportedEnumValues, EditorStyles.toolbarDropDown, GUILayout.Width(100));

		GUILayout.FlexibleSpace();
		GUILayout.Label(string.Format("CPU:{0}ms   GPU:{1}ms", property.frameTime, property.frameGpuTime), EditorStyles.miniLabel);
		EditorGUILayout.EndHorizontal();
	}

	static bool CheckFrameData(ProfilerProperty property)
	{
		if (!property.frameDataReady)
		{
			GUILayout.Label(ms_Styles.noData, ms_Styles.background);
			return false;
		}
		return true;
	}

	private void DrawCPUOrRenderingPane(ProfilerHierarchyGUI mainPane, ProfilerHierarchyGUI detailPane, ProfilerTimelineGUI timelinePane)
	{
		#if PERF_PROFILE
		double t0 = EditorApplication.timeSinceStartup;
		#endif

		ProfilerProperty property = CreateProperty(false);

		DrawCPUOrRenderingToolbar(property);

		if (!CheckFrameData(property))
		{
			property.Cleanup();
			return;
		}

		if (timelinePane != null && m_ViewType == ProfilerViewType.Timeline)
		{
			float lowerPaneSize = m_VertSplit.realSizes[1];
			lowerPaneSize -= EditorStyles.toolbar.CalcHeight(GUIContent.none, 10.0f) + 2.0f;
			timelinePane.DoGUI(GetActiveVisibleFrameIndex(), position.width, position.height - lowerPaneSize, lowerPaneSize);
			property.Cleanup();
		}
		else
		{
			SplitterGUILayout.BeginHorizontalSplit(m_ViewSplit);

			GUILayout.BeginVertical();
			bool expandAll = false;
#if PERF_PROFILE
			expandAll = true;
#endif
			mainPane.DoGUI(property, expandAll);
			property.Cleanup();
			GUILayout.EndVertical();

			GUILayout.BeginVertical();
			var detailProperty = CreateProperty(true);
			var prop = mainPane.GetDetailedProperty(detailProperty);
			detailProperty.Cleanup();
			if (prop != null)
			{
				detailPane.DoGUI(prop, expandAll);
				prop.Cleanup();
			}
			else
				DrawEmptyCPUOrRenderingDetailPane();

			GUILayout.EndVertical();

			SplitterGUILayout.EndHorizontalSplit();
		}
#if PERF_PROFILE
		double t1 = EditorApplication.timeSinceStartup;
		if (Event.current.type == EventType.Repaint)
			GUI.Label (new Rect(0, position.height-20, 300, 20), ((t1-t0)*1000.0).ToString("f2") + "ms for UI repaint");
		#endif
	}
	#endregion

	#region DrawMemoryPane
	private void DrawMemoryPane(SplitterState splitter)
	{
		DrawMemoryToolbar ();

		if (m_ShowDetailedMemoryPane == ProfilerMemoryView.Simple)
			DrawOverviewText (ProfilerArea.Memory);
		else
			DrawDetailedMemoryPane (splitter);
	}

	private void DrawDetailedMemoryPane(SplitterState splitter)
	{
		SplitterGUILayout.BeginHorizontalSplit(splitter);

		m_MemoryListView.OnGUI();
		m_ReferenceListView.OnGUI();
		
		SplitterGUILayout.EndHorizontalSplit();
	}


	static Rect GenerateRect (ref int row, int indent)
	{
		var rect = new Rect (indent * kIndentPx + kBaseIndent, row * kRowHeight, 0, kRowHeight);
		rect.xMax = kNameColumnSize;

		row++;

		return rect;
	}

	static void DrawBackground (int row, bool selected)
	{
		var currentRect = new Rect (1, kRowHeight * row, GUIClip.visibleRect.width, kRowHeight);

		var background = (row % 2 == 0 ? ms_Styles.entryEven : ms_Styles.entryOdd);
		if (Event.current.type == EventType.Repaint)
			background.Draw (currentRect, GUIContent.none, false, false, selected, false);
	}

	private void DrawMemoryToolbar ()
	{
		EditorGUILayout.BeginHorizontal (EditorStyles.toolbar);

		m_ShowDetailedMemoryPane = (ProfilerMemoryView) EditorGUILayout.EnumPopup (m_ShowDetailedMemoryPane, EditorStyles.toolbarDropDown, GUILayout.Width (70f));

		GUILayout.Space (5f);

		if (m_ShowDetailedMemoryPane == ProfilerMemoryView.Detailed)
		{
			if (GUILayout.Button("Take Sample: " + m_AttachProfilerUI.GetConnectedProfiler(), EditorStyles.toolbarButton))
				RefreshMemoryData ();

			if (m_AttachProfilerUI.IsEditor())
				GUILayout.Label("Memory usage in editor is not as it would be in a player", EditorStyles.toolbarButton);
		}

		GUILayout.FlexibleSpace ();
		EditorGUILayout.EndHorizontal ();
	}

	private void RefreshMemoryData ()
	{
		m_MemoryListView.SetRoot(null);
		ProfilerDriver.RequestObjectMemoryInfo ();
	}

	#endregion

	#region GUI drawing
	private static void UpdateChartGrid (float timeMax, ChartData data)
	{
		if (timeMax < 1500)
		{
			data.SetGrid (
				new float[] { 1000, 250, 100 },
				new[] { "1ms (1000FPS)", "0.25ms (4000FPS)", "0.1ms (10000FPS)" }
			);
		}
		else if (timeMax < 10000)
		{
			data.SetGrid (
				new float[] { 8333, 4000, 1000 },
				new[] { "8ms (120FPS)", "4ms (250FPS)", "1ms (1000FPS)" }
			);
		}
		else if (timeMax < 30000)
		{
			data.SetGrid (
				new float[] { 16667, 10000, 5000 },
				new[] { "16ms (60FPS)", "10ms (100FPS)", "5ms (200FPS)" }
			);
		}
		else if (timeMax < 100000)
		{
			data.SetGrid (
				new float[] { 66667, 33333, 16667 },
				new[] { "66ms (15FPS)", "33ms (30FPS)", "16ms (60FPS)" }
			);
		}
		else
		{
			data.SetGrid (
				new float[] { 500000, 200000, 66667 },
				new[] { "500ms (2FPS)", "200ms (5FPS)", "66ms (15FPS)" }
			);
		}
	}

	private void UpdateCharts ()
	{
		int historyLength = ProfilerDriver.maxHistoryLength - 1;
		int firstEmptyFrame = ProfilerDriver.lastFrameIndex - historyLength;
		int firstFrame = Mathf.Max(ProfilerDriver.firstFrameIndex, firstEmptyFrame);
		
		// Collect chart values
		foreach (var chart in m_Charts)
		{
			var scales = new float[chart.m_Series.Length];
			for (int i = 0; i < chart.m_Series.Length; ++i)
			{
				int identifier = ProfilerDriver.GetStatisticsIdentifier (chart.m_Series[i].identifierName);
				float maxValue;
				ProfilerDriver.GetStatisticsValues (identifier, firstEmptyFrame, chart.m_DataScale, chart.m_Series[i].data, out maxValue);
				float scale;
				if (chart.m_Type == Chart.ChartType.Line)
				{
					// Scale line charts so they never hit the top. Scale them slightly differently for each line
					// so that in "no stuff changing" case they will not end up being exactly the same.
					scale = 1.0f / (maxValue * (1.05f + i * 0.05f));
				}
				else
				{
					scale = 1.0f / maxValue;
				}
				scales[i] = scale;
			}
			if (chart.m_Type == Chart.ChartType.Line)
			{
				chart.m_Data.AssignScale(scales);
			}

			chart.m_Data.Assign (chart.m_Series, firstEmptyFrame, firstFrame);
		}

		// CPU chart overlay values
		string selectedName = ProfilerDriver.selectedPropertyPath;
		bool hasCPUOverlay = (selectedName != string.Empty) && m_CurrentArea == ProfilerArea.CPU;
		ProfilerChart cpuChart = m_Charts[(int)ProfilerArea.CPU];
		if (hasCPUOverlay)
		{
			cpuChart.m_Data.hasOverlay = true;
			foreach (var t in cpuChart.m_Series)
			{
				int identifier = ProfilerDriver.GetStatisticsIdentifier ("Selected" + t.identifierName);
				float maxValue;
				t.CreateOverlayData ();
				ProfilerDriver.GetStatisticsValues (identifier, firstEmptyFrame, cpuChart.m_DataScale, t.overlayData, out maxValue);
			}
		}
		else
		{
			cpuChart.m_Data.hasOverlay = false;
		}

		// CPU & GPU chart scale value
		for (ProfilerArea i = ProfilerArea.CPU; i <= ProfilerArea.GPU; i++)
		{
			ProfilerChart chart = m_Charts[(int)i];
			float timeMax = 0.0f;
			float timeMaxExcludeFirst = 0.0f;
			for (int k = 0; k < historyLength; k++)
			{
				float timeNow = 0.0F;
				for (int j = 0; j < chart.m_Series.Length; j++)
				{
					if (chart.m_Series[j].enabled)
						timeNow += chart.m_Series[j].data[k];
				}
				if (timeNow > timeMax)
					timeMax = timeNow;
				if (timeNow > timeMaxExcludeFirst && k + firstEmptyFrame >= firstFrame + 1)
					timeMaxExcludeFirst = timeNow;
			}
			if (timeMaxExcludeFirst != 0.0f)
				timeMax = timeMaxExcludeFirst;

			timeMax = Math.Min(timeMax, m_ChartMaxClamp);
			// Do not apply the new scale immediately, but gradually go towards it
			if (m_ChartOldMax[(int)i] > 0.0f)
				timeMax = Mathf.Lerp(m_ChartOldMax[(int)i], timeMax, 0.4f);
			m_ChartOldMax[(int)i] = timeMax;

			chart.m_Data.AssignScale(new[] { 1.0f / timeMax });
			UpdateChartGrid(timeMax, chart.m_Data);
		}
		
		// Is GPU Profiling supported warning
		string warning = null;
		if (ProfilerDriver.isGPUProfilerBuggyOnDriver)
		{
			warning = "Graphics card driver returned invalid timing information. Please update to a newer version if available.";
		}
		else if (!ProfilerDriver.isGPUProfilerSupported)
		{
			warning = "GPU profiling is not supported by the graphics card driver. Please update to a newer version if available.";
			
			if (Application.platform == RuntimePlatform.OSXEditor)
			{
				if (!ProfilerDriver.isGPUProfilerSupportedByOS)
					warning = "GPU profiling requires Mac OS X 10.7 (Lion) and a capable video card. GPU profiling is currently not supported on mobile.";
				else
					warning = "GPU profiling is not supported by the graphics card driver (or it was disabled because of driver bugs).";
			}
		}
		m_Charts[(int)ProfilerArea.GPU].m_Chart.m_NotSupportedWarning = warning;
	}

	void AddAreaClick (object userData, string[] options, int selected)
	{
		m_Charts[selected].m_Active = true;
		EditorPrefs.SetBool (kPrefCharts + (ProfilerArea)selected, true);
	}

	private void DrawMainToolbar ()
	{
		GUILayout.BeginHorizontal (EditorStyles.toolbar);

		Rect popupRect = GUILayoutUtility.GetRect (ms_Styles.addArea, EditorStyles.toolbarDropDown, GUILayout.Width (120));
		if (EditorGUI.ButtonMouseDown (popupRect, ms_Styles.addArea, FocusType.Native, EditorStyles.toolbarDropDown))
		{
			int length = m_Charts.Length;
			var names = new string[length];
			var enabled = new bool[length];
			for (int c = 0; c < length; ++c)
			{
				names[c] = ((ProfilerArea)c).ToString ();
				enabled[c] = !m_Charts[c].m_Active;
			}
			EditorUtility.DisplayCustomMenu (popupRect, names, enabled, null, AddAreaClick, null);
		}
		GUILayout.FlexibleSpace ();

		m_Recording = GUILayout.Toggle (m_Recording, ms_Styles.profilerRecord, EditorStyles.toolbarButton);
		Profiler.enabled = m_Recording;

		SetProfileDeepScripts(GUILayout.Toggle(ProfilerDriver.deepProfiling, ms_Styles.deepProfile, EditorStyles.toolbarButton));
		ProfilerDriver.profileEditor = GUILayout.Toggle(ProfilerDriver.profileEditor, ms_Styles.profileEditor, EditorStyles.toolbarButton);

		m_AttachProfilerUI.OnGUILayout(this);
		
		if (GUILayout.Button (ms_Styles.clearData, EditorStyles.toolbarButton))
		{
			ProfilerDriver.ClearAllFrames();
		}
		
		GUILayout.Space (5);

		GUILayout.FlexibleSpace ();

		FrameNavigationControls();

		GUILayout.EndHorizontal ();
	}

	private void FrameNavigationControls ()
	{
		if (m_CurrentFrame > ProfilerDriver.lastFrameIndex)
		{
			SetCurrentFrameDontPause (ProfilerDriver.lastFrameIndex);
		}

		// Frame number
		GUILayout.Label (ms_Styles.frame, EditorStyles.miniLabel);
		GUILayout.Label ("   " +PickFrameLabel (), EditorStyles.miniLabel, GUILayout.Width(100));
		
		// Previous/next/current buttons

		GUI.enabled = ProfilerDriver.GetPreviousFrameIndex(m_CurrentFrame) != -1;
		if (GUILayout.Button (ms_Styles.prevFrame, EditorStyles.toolbarButton))
			PrevFrame();

		GUI.enabled = ProfilerDriver.GetNextFrameIndex(m_CurrentFrame) != -1;
		if (GUILayout.Button (ms_Styles.nextFrame, EditorStyles.toolbarButton))
			NextFrame();

		GUI.enabled = true;
		GUILayout.Space (10);
		if (GUILayout.Button (ms_Styles.currentFrame, EditorStyles.toolbarButton))
		{
			SetCurrentFrame(-1);
			m_LastFrameFromTick = ProfilerDriver.lastFrameIndex;
		}
	}
	
	int GetActiveVisibleFrameIndex ()
	{
		// Update the current frame only at fixed intervals,
		// otherwise it looks weird when it is rapidly jumping around when we have a lot of repaints
		return m_CurrentFrame == -1 ? m_LastFrameFromTick : m_CurrentFrame;
	}

	static void DrawOtherToolbar()
	{
		EditorGUILayout.BeginHorizontal(EditorStyles.toolbar);
		GUILayout.FlexibleSpace();
		EditorGUILayout.EndHorizontal();
	}

	private void DrawOverviewText (ProfilerArea area)
	{
		m_PaneScroll[(int)area] = GUILayout.BeginScrollView (m_PaneScroll[(int)area], ms_Styles.background);
		GUILayout.Label (ProfilerDriver.GetOverviewText (area, GetActiveVisibleFrameIndex ()), EditorStyles.wordWrappedLabel);
		GUILayout.EndScrollView ();
	}

	private void DrawPane (ProfilerArea area)
	{
		DrawOtherToolbar ();
		DrawOverviewText (area);
	}

	void SetCurrentFrameDontPause(int frame)
	{
		m_CurrentFrame = frame;
	}

	void SetCurrentFrame(int frame)
	{
		if (frame != -1 && Profiler.enabled && !ProfilerDriver.profileEditor && m_CurrentFrame != frame && EditorApplication.isPlayingOrWillChangePlaymode)
			EditorApplication.isPaused = true;

		SetCurrentFrameDontPause(frame);
	}

	void OnGUI()
	{
		/*// need to do this in non layout event to not have exceptions
		if (Event.current.type != EventType.Layout !Profiler.supported  )
		{
			Close();
			GUIUtility.ExitGUI();
		}*/

		// Initialization

		if (ms_Styles == null)
		{
			ms_Styles = new Styles();
		}

		if (!m_HasProfilerLicense)
		{
			GUILayout.Label(ms_Styles.noLicense, EditorStyles.largeLabel);
			return;
		}

		DrawMainToolbar ();

		SplitterGUILayout.BeginVerticalSplit(m_VertSplit);

		m_GraphPos = EditorGUILayout.BeginScrollView (m_GraphPos, ms_Styles.profilerGraphBackground);
		//GUILayout.Box (GUIContent.none, ms_Styles.paneLeftBackground, GUILayout.Width (Chart.kSideWidth));

		if (m_PrevLastFrame != ProfilerDriver.lastFrameIndex)
		{
			UpdateCharts();
			m_PrevLastFrame = ProfilerDriver.lastFrameIndex;
		}

		int newCurrentFrame = m_CurrentFrame;
		var actions = new Chart.ChartAction[m_Charts.Length];
		for (int c = 0; c < m_Charts.Length; ++c)
		{
			ProfilerChart chart = m_Charts[c];
			if (!chart.m_Active)
				continue;
				
			newCurrentFrame = chart.DoChartGUI (newCurrentFrame, m_CurrentArea, out actions[c]);
		}

		bool needsExit = false;
		if (newCurrentFrame != m_CurrentFrame)
		{
			SetCurrentFrame (newCurrentFrame);
			needsExit = true;
		}
		for (int c = 0; c < m_Charts.Length; ++c)
		{
			ProfilerChart chart = m_Charts[c];
			if (!chart.m_Active)
				continue;
			if (actions[c] == Chart.ChartAction.Closed)
			{
				if (m_CurrentArea == (ProfilerArea)c)
					m_CurrentArea = ProfilerArea.CPU;
				chart.m_Active = false;
				EditorPrefs.SetBool(kPrefCharts + (ProfilerArea)c, false);
			}
			else if (actions[c] == Chart.ChartAction.Activated)
			{
				m_CurrentArea = (ProfilerArea)c;
				// if switched out of CPU area, reset selected property
				if (m_CurrentArea != ProfilerArea.CPU && m_CPUHierarchyGUI.selectedIndex != -1)
				{
					ClearSelectedPropertyPath ();
				}
				needsExit = true;
			}
		}
		if (needsExit)
		{
			Repaint ();
			GUIUtility.ExitGUI ();
		}

		GUILayout.EndScrollView ();

		GUILayout.BeginVertical();

		switch (m_CurrentArea)
		{
			case ProfilerArea.CPU:
				DrawCPUOrRenderingPane (m_CPUHierarchyGUI, m_CPUDetailHierarchyGUI, m_CPUTimelineGUI);
				break;
			case ProfilerArea.GPU:
				DrawCPUOrRenderingPane (m_GPUHierarchyGUI, m_GPUDetailHierarchyGUI, null);
				break;
			case ProfilerArea.Memory:
				DrawMemoryPane (m_ViewSplit);
				break;
			default:
				DrawPane (m_CurrentArea);
				break;
		}

		GUILayout.EndVertical();

		SplitterGUILayout.EndVerticalSplit();
	}
	#endregion
}

}
