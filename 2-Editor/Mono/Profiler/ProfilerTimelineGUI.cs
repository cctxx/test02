using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	[Serializable]
	internal class ProfilerTimelineGUI
	{
		//static int s_TimelineHash = "ProfilerTimeline".GetHashCode();

		const float kSmallWidth = 7.0f;
		const float kTextFadeStartWidth = 50.0f;
		const float kTextFadeOutWidth = 20.0f;
		const float kTextLongWidth = 200.0f;
		const float kLineHeight = 16.0f;

		internal class Styles
		{
			public GUIStyle background = "OL Box";
			public GUIStyle tooltip = "AnimationEventTooltip";
			public GUIStyle tooltipArrow = "AnimationEventTooltipArrow";
			public GUIStyle bar;

			internal Styles()
			{
				bar = new GUIStyle(EditorStyles.miniButton);
				bar.margin = new RectOffset(0,0,0,0);
				bar.padding = new RectOffset(0,0,0,0);
				bar.border = new RectOffset(1,1,1,1);
				bar.normal.background = bar.hover.background = bar.active.background = EditorGUIUtility.whiteTexture;
				bar.normal.textColor = bar.hover.textColor = bar.active.textColor = Color.black;
			}
		}

		private static Styles ms_Styles;
		private static Styles styles
		{
			get { return ms_Styles ?? (ms_Styles = new Styles()); }
		}

		[NonSerialized]
		private ZoomableArea m_TimeArea;
		IProfilerWindowController m_Window;
		int m_SelectedThread = 0;
		int m_SelectedID = -1;
		float m_SelectedTime;
		float m_SelectedDur;

		public ProfilerTimelineGUI(IProfilerWindowController window)
		{
			m_Window = window;
		}
		
		private float GetThreadY(Rect r, int thread, int threadCount)
		{
			// makes first thread take 3x the normal space, effectively
			// distributing space between (count+2) slices
			if (thread > 0)
				thread += 2;
			return r.y + r.height / (threadCount+2) * thread;
		}

		private void DrawGrid(Rect r, int threadCount, float frameTime)
		{
			HandleUtility.handleWireMaterial.SetPass (0);
			
			float x;
			float kDT = 16.66667f;

			GL.Begin (GL.LINES);
			GL.Color (new Color(1,1,1,0.2f));
			// horizontal (threads)
			for (int i = 0; i < threadCount; ++i) {				
				float y = GetThreadY(r, i, threadCount);
				GL.Vertex3 (r.x, y, 0.0f);
				GL.Vertex3 (r.x+r.width, y, 0.0f);
			}
			// vertical 16.6ms apart
			float t = kDT;
			for (; t <= frameTime; t += kDT)
			{
				x = m_TimeArea.TimeToPixel (t, r);
				GL.Vertex3 (x, r.y, 0.0f);
				GL.Vertex3 (x, r.y+r.height, 0.0f);
			}
			// vertical: frame boundaries
			GL.Color (new Color(1,1,1,0.8f));
			x = m_TimeArea.TimeToPixel (0.0f, r);
			GL.Vertex3 (x, r.y, 0.0f);
			GL.Vertex3 (x, r.y+r.height, 0.0f);
			x = m_TimeArea.TimeToPixel (frameTime, r);
			GL.Vertex3 (x, r.y, 0.0f);
			GL.Vertex3 (x, r.y+r.height, 0.0f);
			
			GL.End ();
			
			// time labels
			GUI.color = new Color(1,1,1,0.4f);
			t = 0.0f;
			for (; t <= frameTime; t += kDT)
			{
				x = m_TimeArea.TimeToPixel (t, r);
				GUI.Label (new Rect(x,r.yMax-16,200,16), string.Format("{0:f1}ms ({1:f0}FPS)", t, 1000.0f/t));
			}
			GUI.color = new Color(1,1,1,1.0f);
			x = m_TimeArea.TimeToPixel (frameTime, r);
			GUI.Label (new Rect(x,r.yMax-16,200,16), string.Format("{0:f1}ms ({1:f0}FPS)", frameTime, 1000.0f/frameTime));
		}
		
		
		void DrawSmallGroup (float x1, float x2, float y, int size)
		{
			if (x2-x1 < 1)
				return;
			GUI.color = new Color(0.5f,0.5f,0.5f,0.7f);
			GUI.contentColor = Color.white;
			GUIContent c = GUIContent.none;
			if (x2-x1 > kTextFadeOutWidth)
				c = new GUIContent(size + " items");
			GUI.Label (new Rect(x1,y,x2-x1,kLineHeight-2), c, styles.bar);
		}

		private static float TimeToPixelCached (float time, float rectWidthDivShownWidth, float shownX, float rectX)
		{
			return (time - shownX)*rectWidthDivShownWidth + rectX;
		}


		void DrawProfilingData (ProfilerFrameDataIterator iter, Rect r, int threadIdx, float timeOffset, bool ghost)
		{
			float smallW = ghost ? kSmallWidth * 3.0f : kSmallWidth;
			string selectedPath = ProfilerDriver.selectedPropertyPath;

			bool expanded = true;

			var oldColor = GUI.color;
			var oldContentColor = GUI.contentColor;

			Color[] colors = ProfilerColors.colors;
			
			bool hasSmallGroup = false;
			float smallGroupX1 = -1.0f;
			float smallGroupX2 = -1.0f;
			float smallGroupY = -1.0f;
			int smallGroupSize = 0;
			
			float selectedY = -1.0f;
			string selectedName = null;
			
			r.height -= 1.0f;
			GUI.BeginGroup(r);
			r.x = r.y = 0.0f;
			
			if (!ghost)
				GUI.Label (new Rect (r.x, r.y + r.height * 0.5f - 8, r.width, 16), iter.GetThreadName ());

			Rect cachedShownArea = m_TimeArea.shownArea;
			float rectWidthDivShownWidth = r.width / cachedShownArea.width;
			float rectX = r.x;
			float shownX = cachedShownArea.x;

			while (iter.Next (expanded))
			{
				expanded = true;

				float pt = iter.startTimeMS + timeOffset * 1000.0f;
				float dt = iter.durationMS;
				float dtDisp = Mathf.Max(dt, 0.3e-3f); // some samples end up being exactly zero microseconds, ensure they have at least 0.3us width
				float x1 = TimeToPixelCached(pt, rectWidthDivShownWidth, shownX, rectX);
				float x2 = TimeToPixelCached (pt + dtDisp, rectWidthDivShownWidth, shownX, rectX) - 1.0f;
				float dx = x2 - x1;
				// out of view?
				if (x1 > r.x+r.width || x2 < r.x)
				{
					expanded = false;
					continue;
				}
				float depth = iter.depth - 1;
				float y = r.y + depth * kLineHeight;
				
				// we might need to end a streak of small blocks
				if (hasSmallGroup)
				{
					bool breakSmallGroup = false;
					// we're entering a big block?
					if (dx >= smallW)
						breakSmallGroup = true;
					// different depth?
					if (smallGroupY != y)
						breakSmallGroup = true;
					// had a non-tiny small group and have enough empty space?
					if (x1 - smallGroupX2 > 6)
						breakSmallGroup = true;
					if (breakSmallGroup)
					{
						DrawSmallGroup (smallGroupX1, smallGroupX2, smallGroupY, smallGroupSize);
						hasSmallGroup = false;
					}
				}
				
				// too narrow?
				if (dx < smallW)
				{
					expanded = false;
					if (!hasSmallGroup)
					{
						hasSmallGroup = true;
						smallGroupY = y;
						smallGroupX1 = x1;
						smallGroupSize = 0;
					}
					smallGroupX2 = x2;
					++smallGroupSize;
					continue;
				}

				int myID = iter.id;
				string myPath = iter.path;
				bool selected = (myPath == selectedPath) && !ghost;
				if (m_SelectedID >= 0)
					selected &= (myID == m_SelectedID);
				selected &= (threadIdx == m_SelectedThread);


				var textCol = Color.white;
				var col = colors[iter.group % colors.Length];
				col.a = selected ? 1.0f : 0.75f;

				// Ghosted (next / previous frame) is dimmed
				// to make it very visible what is the current frame and what is not
				if (ghost)
				{
					col.a = 0.4f;
					textCol.a = 0.5F;
				}
				

				string text = iter.name;
				if (selected)
				{
					selectedName = text;
					m_SelectedTime = pt;
					m_SelectedDur = dt;
					selectedY = y+kLineHeight;
				}
				
				if (dx < kTextFadeOutWidth)
					text = string.Empty;
				else
				{
					// fade out text when we're narrow
					if (dx < kTextFadeStartWidth && !selected)
					{
						textCol.a *= (dx-kTextFadeOutWidth) / (kTextFadeStartWidth-kTextFadeOutWidth);
					}
					// add time when we're wide
					if (dx > kTextLongWidth)
					{
						text += string.Format(" ({0:f2}ms)", dt);
					}	
				}

				GUI.color = col;
				GUI.contentColor = textCol;
				if (GUI.Button (new Rect(x1,y,dx,kLineHeight-2), text, styles.bar))
				{
					m_Window.SetSelectedPropertyPath(myPath);
					m_SelectedThread = threadIdx;
					m_SelectedID = myID;
				}
				
				hasSmallGroup = false;
			}

			// we migth have last small group to draw
			if (hasSmallGroup)
				DrawSmallGroup (smallGroupX1, smallGroupX2, smallGroupY, smallGroupSize);

			GUI.color = oldColor;
			GUI.contentColor = oldContentColor;
			
			// draw selected in detail
			if (selectedName != null && threadIdx == m_SelectedThread)
			{
				// Draw body of tooltip
				string text = string.Format (m_SelectedDur >= 1.0 ? "{0}\n{1:f2}ms" : "{0}\n{1:f3}ms", selectedName, m_SelectedDur);
				GUIContent textC = new GUIContent(text);
				GUIStyle style = styles.tooltip;
				Vector2 size = style.CalcSize(textC);
				float x = m_TimeArea.TimeToPixel (m_SelectedTime+m_SelectedDur*0.5f, r);
				if (x < r.x)
					x = r.x + 20;
				if (x > r.xMax)
					x = r.xMax - 20;

				Rect rect;
				if (selectedY + 6 + size.y < r.yMax)
				{
					// Draw arrow of tooltip
					rect = new Rect (x - 32, selectedY, 50, 7);
					GUI.Label (rect, GUIContent.none, styles.tooltipArrow);					
				}


				rect = new Rect (x, selectedY + 6, size.x, size.y);

				// Ensure it doesn't go too far right
				if (rect.xMax > r.xMax + 20)
					rect.x = r.xMax - rect.width + 20;

				// Ensure it doesn't go too far down
				if (rect.yMax > r.yMax)
					rect.y = r.yMax - rect.height;
				if (rect.y < r.y)
					rect.y = r.y;

				GUI.Label (rect, textC, style);				
			}

			// click on empty space deselects
			if (Event.current.type == EventType.MouseDown && r.Contains (Event.current.mousePosition))
			{
				m_Window.ClearSelectedPropertyPath();
				m_SelectedID = -1;
				m_SelectedThread = threadIdx;
				Event.current.Use();
			}
			
			GUI.EndGroup();
		}
		
		private void PerformFrameSelected(float frameMS)
		{
			float t = m_SelectedTime;
			float dt = m_SelectedDur;
			if (m_SelectedID < 0 || dt <= 0.0f) {
				t = 0.0f;
				dt = frameMS;
			}
			m_TimeArea.SetShownHRange (t-dt*0.1f, t+dt*1.1f);
		}
		
		private void HandleFrameSelected(float frameMS)
		{
			Event evt = Event.current;
			if (evt.type == EventType.ValidateCommand || evt.type == EventType.ExecuteCommand)
			{
				if (evt.commandName == "FrameSelected")
				{
					bool execute = evt.type == EventType.ExecuteCommand;
					if (execute)
						PerformFrameSelected(frameMS);
					evt.Use();
				}
			}
		}
		
		void DoProfilerFrame(int frameIndex, Rect fullRect, bool ghost, ref int threadCount, ref double startTime)
		{
			var iter = new ProfilerFrameDataIterator ();
			int myThreadCount = iter.GetThreadCount (frameIndex);
			double myStartTime = iter.GetFrameStartS (frameIndex);
			//Debug.Log("frame " + frameIndex + " start " + myStartTime.ToString("f4"));
			if (ghost && myThreadCount != threadCount)
				return;

			if (!ghost)
			{
				threadCount = myThreadCount;
				startTime = myStartTime;
			}
			for (int t = 0; t < threadCount; ++t)
			{
				Rect r = fullRect;
				r.y = GetThreadY(fullRect, t, threadCount);
				r.height = GetThreadY(fullRect, t+1, threadCount) - r.y;
				iter.SetRoot (frameIndex, t);
				
				if (t == 0 && !ghost)
				{
					DrawGrid (fullRect, threadCount, iter.frameTimeMS);
					HandleFrameSelected (iter.frameTimeMS);
				}
				DrawProfilingData (iter, r, t, (float)(myStartTime - startTime), ghost);
			}
		}

		public void DoGUI(int frameIndex, float width, float ypos, float height)
		{
			Rect fullRect = new Rect (0, ypos, width, height);

			if (m_TimeArea == null)
			{
				m_TimeArea = new ZoomableArea();
				m_TimeArea.hRangeLocked = false;
				m_TimeArea.vRangeLocked = true;
				m_TimeArea.hSlider = true;
				m_TimeArea.vSlider = false;
				m_TimeArea.scaleWithWindow = true;
				m_TimeArea.rect = fullRect;
				m_TimeArea.SetShownHRangeInsideMargins (-2.0f, 33.3f);
				m_TimeArea.OnEnable();
			}
			
			m_TimeArea.rect = fullRect;
			m_TimeArea.BeginViewGUI();
			m_TimeArea.EndViewGUI();
			fullRect = m_TimeArea.drawRect;
			
			int threadCount = 0;
			double startTime = 0.0;
			DoProfilerFrame (frameIndex, fullRect, false, ref threadCount, ref startTime);
			
			bool oldEnabled = GUI.enabled;
			GUI.enabled = false;

			int prevFrame = ProfilerDriver.GetPreviousFrameIndex(frameIndex);
			if (prevFrame != -1)
			{
				DoProfilerFrame (prevFrame, fullRect, true, ref threadCount, ref startTime);
			}			
			int nextFrame = ProfilerDriver.GetNextFrameIndex(frameIndex);
			if (nextFrame != -1)
			{
				DoProfilerFrame (nextFrame, fullRect, true, ref threadCount, ref startTime);
			}
			GUI.enabled = oldEnabled;
		}
	}
}
