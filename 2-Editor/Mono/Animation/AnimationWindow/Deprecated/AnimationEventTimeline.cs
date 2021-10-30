using System.Reflection;
using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using UnityEditorInternal;
using System.Linq;

namespace UnityEditor
{

[System.Serializable]
internal class AnimationEventTimeLine
{
	[System.NonSerialized]
	private AnimationEvent[] 	m_EventsAtMouseDown;
	[System.NonSerialized]
	private float[] 			m_EventTimes;
	
	private bool[] 				m_EventsSelected;
	private bool				m_DirtyTooltip = false;
	private int					m_HoverEvent = -1;
	// Rects used for checking mouse-move state changes
	private string				m_InstantTooltipText = null;
	private Vector2				m_InstantTooltipPoint = Vector2.zero;
	private EditorWindow		m_Owner;

	public AnimationEventTimeLine(EditorWindow owner)
	{
		m_Owner = owner;
	}

	public class EventComparer : IComparer
	{
		int IComparer.Compare (System.Object objX, System.Object objY)
		{
			AnimationEvent x = (AnimationEvent)objX;
			AnimationEvent y = (AnimationEvent)objY;
			float timeX = x.time;
			float timeY = y.time;
			if (timeX != timeY)
				return ( (int)Mathf.Sign(timeX - timeY) );
			
			int valueX = x.GetHashCode();
			int valueY = y.GetHashCode();
			return valueX - valueY;
		}
	}
	
	private class EventLineContextMenuObject
	{
		public GameObject		m_Animated;
		public AnimationClip	m_Clip;
		public float			m_Time;
		public int  			m_Index;
		
		public EventLineContextMenuObject (GameObject animated, AnimationClip clip, float time, int index)
		{
			m_Animated = animated;
			m_Clip = clip;
			m_Time = time;
			m_Index = index;
		}
	}
	
		
	public void AddEvent (AnimationWindowState state)
	{
		float time = (float)state.m_Frame / state.frameRate;
		int index = AnimationEventPopup.Create (state.m_RootGameObject, state.m_ActiveAnimationClip, time, m_Owner);
		Select (state.m_ActiveAnimationClip, index);
	}
	
	public void DeselectAll ()
	{
		m_EventsSelected = null;
	}
	
	void Select (AnimationClip clip, int index)
	{
		m_EventsSelected = new bool[AnimationUtility.GetAnimationEvents(clip).Length];
		m_EventsSelected[index] = true;
	}
	
	public void EventLineGUI (Rect rect, AnimationSelection selection, AnimationWindowState state, CurveEditor curveEditor)
	{
		AnimationClip clip = state.m_ActiveAnimationClip;
		GameObject animated = state.m_RootGameObject;
		
		GUI.BeginGroup(rect);
		Color backupCol = GUI.color;		
		
		Rect eventLineRect = new Rect (0, 0, rect.width, rect.height);
		
		float mousePosTime = Mathf.RoundToInt (state.PixelToTime (Event.current.mousePosition.x, rect) * state.frameRate) / state.frameRate;
		
		// Draw events
		if (clip != null)
		{			
			AnimationEvent[] events = AnimationUtility.GetAnimationEvents(clip);
			Texture eventMarker = EditorGUIUtility.IconContent("Animation.EventMarker").image;
			
			// Calculate rects
			Rect[] hitRects = new Rect[events.Length];
			Rect[] drawRects = new Rect[events.Length];
			int shared = 1;
			int sharedLeft = 0;
			for (int i=0; i<events.Length; i++)
			{
				AnimationEvent evt = events[i];
				
				if (sharedLeft == 0)
				{
					shared = 1;
					while (i+shared < events.Length && events[i+shared].time == evt.time)
						shared++;
					sharedLeft = shared;
				}
				sharedLeft--;
				
				// Important to take floor of positions of GUI stuff to get pixel correct alignment of
				// stuff drawn with both GUI and Handles/GL. Otherwise things are off by one pixel half the time.
				float keypos = Mathf.Floor (state.FrameToPixel (evt.time * clip.frameRate, rect));
				int sharedOffset = 0;
				if (shared > 1)
				{
					float spread = Mathf.Min ((shared - 1) * (eventMarker.width - 1), (int)(state.FrameDeltaToPixel (rect) - eventMarker.width * 2));
					sharedOffset = Mathf.FloorToInt(Mathf.Max(0, spread - (eventMarker.width-1) * (sharedLeft)));
				}
				
				Rect r = new Rect (
					keypos + sharedOffset - eventMarker.width/2,
					(rect.height-10) * (float)(sharedLeft-shared+1) / Mathf.Max(1, shared-1),
					eventMarker.width,
					eventMarker.height);
				
				hitRects[i] = r;
				drawRects[i] = r;
			}
			
			// Store tooptip info
			if (m_DirtyTooltip)
			{
				if (m_HoverEvent >= 0 && m_HoverEvent < hitRects.Length)
				{
					m_InstantTooltipText = AnimationEventPopup.FormatEvent(animated, events[m_HoverEvent]);
					m_InstantTooltipPoint = new Vector2(hitRects[m_HoverEvent].xMin + (int)(hitRects[m_HoverEvent].width/2) + rect.x - 30, rect.yMax);
				}
				m_DirtyTooltip = false;
			}
			
			// Selection control
			if (m_EventsSelected == null || m_EventsSelected.Length != events.Length)
			{
				m_EventsSelected = new bool[events.Length];
				AnimationEventPopup.ClosePopup();
			}
			Vector2 offset = Vector2.zero;
			int clickedIndex;
			float startSelection, endSelection;
			
			// TODO: GUIStyle.none has hopping margins that need to be fixed
			HighLevelEvent hEvent = EditorGUIExt.MultiSelection (
				rect,
				drawRects,
				new GUIContent(eventMarker),
				hitRects,
				ref m_EventsSelected,
				null,
				out clickedIndex,
				out offset,
				out startSelection,
				out endSelection,
				GUIStyleX.none
				);
			
			if (hEvent != HighLevelEvent.None)
			{
				switch (hEvent)
				{
				case HighLevelEvent.BeginDrag:
					m_EventsAtMouseDown = events;
					m_EventTimes = new float[events.Length];
					for (int i=0; i<events.Length; i++)
						m_EventTimes[i] = events[i].time;
					break;
				case HighLevelEvent.SelectionChanged:
					curveEditor.SelectNone ();
					if (clickedIndex != -1)
						AnimationEventPopup.UpdateSelection(animated, selection.clip, clickedIndex, m_Owner);
					break;
				case HighLevelEvent.Delete:
					DeleteEvents(clip, m_EventsSelected);
					break;
					
				case HighLevelEvent.DoubleClick:
					if (clickedIndex != -1)
						AnimationEventPopup.Edit(animated, selection.clip, clickedIndex, m_Owner);
					else
						EventLineContextMenuAdd (new EventLineContextMenuObject(animated, clip, mousePosTime, -1));
					break;	
				case HighLevelEvent.Drag:
					for (int i=events.Length-1; i>=0; i--)
					{
						if (m_EventsSelected[i])
						{
							AnimationEvent evt = m_EventsAtMouseDown[i];
							evt.time = m_EventTimes[i] + offset.x * state.PixelDeltaToTime (rect);
							evt.time = Mathf.Max(0.0F, evt.time);
							evt.time = Mathf.RoundToInt(evt.time * clip.frameRate) / clip.frameRate;
						}
					}
					int[] order = new int[m_EventsSelected.Length];
					for (int i=0; i<order.Length; i++)
					{
						order[i] = i;
					}
					System.Array.Sort(m_EventsAtMouseDown, order, new EventComparer());
					bool[] selectedOld = (bool[])m_EventsSelected.Clone();
					float[] timesOld = (float[])m_EventTimes.Clone();
					for (int i=0; i<order.Length; i++)
					{
						m_EventsSelected[i] = selectedOld[order[i]];
						m_EventTimes[i] = timesOld[order[i]];
					}
					
					Undo.RegisterCompleteObjectUndo (clip, "Move Event");
					AnimationUtility.SetAnimationEvents(clip, m_EventsAtMouseDown);
					m_DirtyTooltip = true;
					break;
				case HighLevelEvent.ContextClick:
					GenericMenu menu = new GenericMenu();
					var contextData = new EventLineContextMenuObject(animated, clip, events[clickedIndex].time, clickedIndex);
					menu.AddItem(
						new GUIContent("Edit Animation Event"),
						false,
						EventLineContextMenuEdit,
						contextData);
					menu.AddItem(
						new GUIContent("Add Animation Event"),
						false,
						EventLineContextMenuAdd,
						contextData);
					menu.AddItem(
						new GUIContent("Delete Animation Event"),
						false,
						EventLineContextMenuDelete,
						contextData);
					menu.ShowAsContext();
						
						// Mouse may move while context menu is open - make sure instant tooltip is handled
					m_InstantTooltipText = null;
					m_DirtyTooltip = true;
					state.Repaint();
					break;
				}
			}

			CheckRectsOnMouseMove (rect, events, hitRects);
		}

		// Create context menu on context click
		if (Event.current.type == EventType.ContextClick && eventLineRect.Contains(Event.current.mousePosition))
		{
			if (selection.EnsureClipPresence())
			{
				Event.current.Use();
				// Create menu
				GenericMenu menu = new GenericMenu();
				menu.AddItem(
					new GUIContent("Add Animation Event"),
					false,
					EventLineContextMenuAdd,
					new EventLineContextMenuObject(animated, clip, mousePosTime, -1));
				menu.ShowAsContext();
			}
		}
		
		GUI.color = backupCol;	
		GUI.EndGroup();
	}
	
	public void DrawInstantTooltip (Rect position)
	{
		if (m_InstantTooltipText != null && m_InstantTooltipText != "")
		{
			// Draw body of tooltip
			GUIStyle style = (GUIStyle)"AnimationEventTooltip";
			Vector2 size = style.CalcSize(new GUIContent(m_InstantTooltipText));
			Rect rect = new Rect(m_InstantTooltipPoint.x - 10, m_InstantTooltipPoint.y + 24, size.x, size.y);
			
			// Right align tooltip rect if it would otherwise exceed the bounds of the window
			if (rect.xMax > position.width)
				rect.x = position.width - rect.width;
			
			GUI.Label(rect, m_InstantTooltipText, style);
			
			// Draw arrow of tooltip
			rect = new Rect(m_InstantTooltipPoint.x-3, m_InstantTooltipPoint.y, 7, 25);
			GUI.Label(rect, "", "AnimationEventTooltipArrow");
		}
	}	
	
		
	void DeleteEvents (AnimationClip clip, bool[] deleteIndices)
	{
		bool deletedAny = false;
		
		List<AnimationEvent> eventList = new List<AnimationEvent>(AnimationUtility.GetAnimationEvents(clip));
		for (int i=eventList.Count-1; i>=0; i--)
		{
			if (deleteIndices[i])
			{
				eventList.RemoveAt(i);
				deletedAny = true;
			}
		}
		
		if (deletedAny)
		{
			AnimationEventPopup.ClosePopup();
			
			Undo.RegisterCompleteObjectUndo (clip, "Delete Event");
			
			AnimationUtility.SetAnimationEvents(clip, eventList.ToArray());
			m_EventsSelected = new bool[eventList.Count];
			
			m_DirtyTooltip = true;
		}
	}
	
	public void EventLineContextMenuAdd (object obj)
	{
		EventLineContextMenuObject eventObj = (EventLineContextMenuObject)obj;
		int index = AnimationEventPopup.Create(eventObj.m_Animated, eventObj.m_Clip, eventObj.m_Time, m_Owner);
		Select (eventObj.m_Clip, index);
		m_EventsSelected = new bool[AnimationUtility.GetAnimationEvents(eventObj.m_Clip).Length];
		m_EventsSelected[index] = true;
	}
	
	public void EventLineContextMenuEdit (object obj)
	{
		EventLineContextMenuObject eventObj = (EventLineContextMenuObject)obj;
		AnimationEventPopup.Edit(eventObj.m_Animated, eventObj.m_Clip, eventObj.m_Index, m_Owner);
		Select (eventObj.m_Clip, eventObj.m_Index);
	}
	
	public void EventLineContextMenuDelete (object obj)
	{
		EventLineContextMenuObject eventObj = (EventLineContextMenuObject)obj;
		AnimationClip clip = eventObj.m_Clip;
		if (clip == null)
			return;
		
		int clickedIndex = eventObj.m_Index;
		
		// If context menu click is on a selected animation event, delete all selected animation events
		if (m_EventsSelected[clickedIndex])
		{
			DeleteEvents (clip, m_EventsSelected);
		}
		// Else, only delete the clicked animation event
		else
		{
			bool[] deleteIndices = new bool[m_EventsSelected.Length];
			deleteIndices[clickedIndex] = true;
			DeleteEvents (clip, deleteIndices);
		}
	}

	private void CheckRectsOnMouseMove (Rect eventLineRect, AnimationEvent[] events, Rect[] hitRects)
	{
		Vector2 mouse = Event.current.mousePosition;
		bool hasFound = false;

		if (events.Length == hitRects.Length)
		{
			for (int i = hitRects.Length - 1; i >= 0; i--)
			{
				if (hitRects[i].Contains (mouse))
				{
					hasFound = true;
					if (m_HoverEvent != i)
					{
						m_HoverEvent = i;
						m_InstantTooltipText = events[m_HoverEvent].functionName;
						m_InstantTooltipPoint = new Vector2 (hitRects[m_HoverEvent].xMin + (int)(hitRects[m_HoverEvent].width / 2) + eventLineRect.x, eventLineRect.yMax);
						m_DirtyTooltip = true;
					}
				}
			}
		}
		if (!hasFound)
		{
			m_HoverEvent = -1;
			m_InstantTooltipText = "";
		}
	}
}

} // namespace
