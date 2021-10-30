using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using System;
using Object = UnityEngine.Object;

namespace UnityEditorInternal
{
	internal class AddCurvesPopup : EditorWindow
	{
		const float k_WindowPadding = 3;

		internal static AnimationWindowState s_State;
		internal static Object animatableObject { get; set; }
		internal static GameObject gameObject { get; set; }
		internal static string path { get { return AnimationUtility.CalculateTransformPath (gameObject.transform, s_State.m_RootGameObject.transform); } }

		private static AddCurvesPopup s_AddCurvesPopup;
		private static long s_LastClosedTime;
		private static AddCurvesPopupHierarchy s_Hierarchy;
		
		private static  Vector2 windowSize = new Vector2(240, 250);

		void Init (Rect buttonRect)
		{
			buttonRect = GUIUtility.GUIToScreenRect (buttonRect);
			ShowAsDropDown (buttonRect, windowSize, new[] { PopupLocationHelper.PopupLocation.Right });
		}

		void OnDisable ()
		{
			s_LastClosedTime = System.DateTime.Now.Ticks / System.TimeSpan.TicksPerMillisecond;
			s_AddCurvesPopup = null;
			s_Hierarchy = null;
		}

		internal static void AddNewCurve (AddCurvesPopupPropertyNode node)
		{
			if (!AnimationWindow.EnsureAllHaveClips())
				return;
		
			AnimationWindowUtility.CreateDefaultCurves (s_State, node.curveBindings);

			TreeViewItem parent = node.parent.displayName == "GameObject" ? node.parent : node.parent.parent;
			s_State.m_hierarchyState.selectedIDs.Clear ();
			s_State.m_hierarchyState.selectedIDs.Add(parent.id);

			s_State.m_HierarchyData.SetExpanded(parent, true);
			s_State.m_HierarchyData.SetExpanded(node.parent.id, true);
			s_State.m_CurveEditorIsDirty = true;
		}

		internal static bool ShowAtPosition (Rect buttonRect, AnimationWindowState state)
		{
			// We could not use realtimeSinceStartUp since it is set to 0 when entering/exitting playmode, we assume an increasing time when comparing time.
			long nowMilliSeconds = System.DateTime.Now.Ticks / System.TimeSpan.TicksPerMillisecond;
			bool justClosed = nowMilliSeconds < s_LastClosedTime + 50;
			if (!justClosed)
			{
				Event.current.Use ();
				if (s_AddCurvesPopup == null)
					s_AddCurvesPopup = ScriptableObject.CreateInstance<AddCurvesPopup> ();

				s_State = state;
				s_AddCurvesPopup.Init (buttonRect);
				return true;
			}
			return false;
		}

		internal void OnGUI ()
		{
			// We do not use the layout event
			if (Event.current.type == EventType.layout)
				return;

			if (s_Hierarchy == null)
				s_Hierarchy = new AddCurvesPopupHierarchy (s_State);

			Rect rect = new Rect (1, 1, windowSize.x - k_WindowPadding, windowSize.y - k_WindowPadding);
			GUI.Box (new Rect (0, 0, windowSize.x, windowSize.y), GUIContent.none, new GUIStyle ("grey_border"));
			s_Hierarchy.OnGUI (rect, this);			
		}
	}
}
