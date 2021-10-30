using UnityEngine;
using UnityEditor;

namespace UnityEditorInternal
{
	internal class ProfilerChart
	{
		public ProfilerArea m_Area;
		public Chart.ChartType m_Type;
		public float m_DataScale;
		public Chart m_Chart;
		public ChartData m_Data;
		public ChartSeries[] m_Series;
		public bool m_Active;
		public GUIContent m_Icon;

		public ProfilerChart(ProfilerArea area, Chart.ChartType type, float dataScale, int seriesCount)
		{
			m_Area = area;
			m_Type = type;
			m_DataScale = dataScale;
			m_Chart = new Chart();
			m_Data = new ChartData();
			m_Series = new ChartSeries[seriesCount];
			m_Active = EditorPrefs.GetBool(ProfilerWindow.kPrefCharts + area, true);
			m_Icon = EditorGUIUtility.TextContent("Profiler." + System.Enum.GetName(typeof(ProfilerArea), area));
		}

		public int DoChartGUI(int currentFrame, ProfilerArea currentArea, out Chart.ChartAction action)
		{
			if (Event.current.type == EventType.Repaint)
			{
				string[] labels = new string[m_Series.Length];
				for (int s = 0; s < m_Series.Length; s++)
				{
					string name =
						m_Data.hasOverlay ?
						"Selected" + m_Series[s].identifierName :
						m_Series[s].identifierName;
					int identifier = ProfilerDriver.GetStatisticsIdentifier(name);
					labels[s] = ProfilerDriver.GetFormattedStatisticsValue(currentFrame, identifier);
				}
				m_Data.selectedLabels = labels;
			}


			return m_Chart.DoGUI(m_Type, currentFrame, m_Data, currentArea == m_Area, m_Icon, out action);

		}

		public void LoadAndBindSettings()
		{
			m_Chart.LoadAndBindSettings(ProfilerWindow.kPrefCharts + m_Area, m_Data);
		}
	}
}
