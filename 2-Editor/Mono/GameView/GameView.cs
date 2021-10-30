using UnityEngine;
using System.Collections.Generic;
using UnityEditorInternal;

/*
The main GameView can be in the following states when entering playmode.
These states should be tested when changing the behavior when entering playmode.
 
  
 GameView docked and visible
 GameView docked and visible and maximizeOnPlay
 GameView docked and hidden
 GameView docked and hidden and maximizeOnPlay
 GameView docked and hidden and resized
 Maximized GameView (maximized from start)
 No GameView but a SceneView exists
 No GameView and no SceneView exists
 Floating GameView in separate window
*/

namespace UnityEditor
{
internal class GameView : EditorWindow
{
		const int kToolbarHeight = 17;
	const int kBorderSize = 5;

		[SerializeField] bool m_MaximizeOnPlay;
		[SerializeField] bool m_Gizmos;
		[SerializeField] bool m_Stats;
		[SerializeField] int[] m_SelectedSizes = new int[0]; // We have a selection for each game view size group (e.g standalone, android etc)

		int m_SizeChangeID = int.MinValue;
		GUIContent gizmosContent = new GUIContent ("Gizmos");
	
		static GUIStyle s_GizmoButtonStyle;
		static List<GameView> s_GameViews = new List<GameView> ();
		static Vector2 s_MainGameViewSize = new Vector2 (-1f, -1f);

		public GameView ()
		{
			depthBufferBits = 32;
			antiAlias = -1;
			autoRepaintOnSceneChange = true;
		}

		public bool maximizeOnPlay
		{
			get { return m_MaximizeOnPlay; }
			set { m_MaximizeOnPlay = value; }
		}

		int selectedSizeIndex
		{
			get { return m_SelectedSizes[(int)currentSizeGroupType]; }
			set { m_SelectedSizes[(int)currentSizeGroupType] = value; }
		}

		static GameViewSizeGroupType currentSizeGroupType
		{
			get { return GameViewSizes.instance.currentGroupType; }
		}


		GameViewSize currentGameViewSize
		{
			get { return GameViewSizes.instance.currentGroup.GetGameViewSize(selectedSizeIndex); }
		}

		public void OnValidate ()
		{
			EnsureSelectedSizeAreValid ();
		}

		public void OnEnable ()
		{
			EnsureSelectedSizeAreValid ();
			dontClearBackground = true;
			s_GameViews.Add (this);
		}

		public void OnDisable ()
		{
			s_GameViews.Remove (this);
		}

		internal static GameView GetMainGameView ()
	{
		if (s_GameViews != null)
			if (s_GameViews.Count > 0)
				return s_GameViews[0];
		return null;
	}
		
	public static void RepaintAll ()
	{
		if (s_GameViews == null)
			return;

		foreach (GameView gv in s_GameViews)
			gv.Repaint();
	}		

		internal static Vector2 GetSizeOfMainGameView ()
		{
		GameView gv = GetMainGameView ();
		if (gv != null)
			{
			// When maximizing Scene View all Game Views are thrown out so we cache the size
			s_MainGameViewSize = gv.GetSize ();
			}
		return s_MainGameViewSize;
		}

	static void GameViewAspectWasChanged()
	{
		// When a camera is selected we ensure to update Scene Views to let the preview and gizmos
		// render correctly as they depend on the current Game View aspect ratio.
		if (Selection.activeGameObject != null)
		{
			Camera cam = Selection.activeGameObject.GetComponent<Camera>();
			if (cam != null)
				SceneView.RepaintAll();
		}
	}

		private void OnFocus ()
	{
        InternalEditorUtility.OnGameViewFocus(true);
    }

		private void OnLostFocus ()
	{
		// We unlock the cursor when the game view loses focus to allow the user to regain cursor control.
		// Fix for 389362: Ensure that we do not unlock cursor during init of play mode. Because we could 
		// be maximizing game view, which causes a lostfocus on the original game view, in these situations we do
		// not unlock cursor.
		if (!EditorApplicationLayout.IsInitializingPlaymodeLayout ())
		{
			Unsupported.SetAllowCursorLock(false);
			Unsupported.SetAllowCursorHide(false);
		}

        InternalEditorUtility.OnGameViewFocus(false);
	}

	internal override void OnResized ()
	{
		GameViewAspectWasChanged ();
	}

		// Call when number of available aspects can have changed (after deserialization or gui change)
		private void EnsureSelectedSizeAreValid ()
		{
			// Ensure deserialized array is resized if needed
			int numGameViewSizeGroups = System.Enum.GetNames (typeof (GameViewSizeGroupType)).Length;
			if (m_SelectedSizes.Length != numGameViewSizeGroups)
				System.Array.Resize (ref m_SelectedSizes, numGameViewSizeGroups);

			// Ensure deserialized selection index for each group is within valid range
			foreach (GameViewSizeGroupType groupType in System.Enum.GetValues (typeof (GameViewSizeGroupType)))
	{
				GameViewSizeGroup gvsg = GameViewSizes.instance.GetGroup (groupType);
				int index = (int)groupType;
				m_SelectedSizes[index] = Mathf.Clamp (m_SelectedSizes[index], 0, gvsg.GetTotalCount () - 1);
			}
	}

		public bool IsShowingGizmos ()
	{
			return m_Gizmos;
	}

		private void OnSelectionChange ()
	{
			if (m_Gizmos)
				Repaint ();
		}

		Rect gameViewRenderRect
		{
			get { return new Rect (0, kToolbarHeight, position.width, position.height - kToolbarHeight);}
		}
		
		static internal Rect GetConstainedGameViewRenderRect (Rect renderRect, int sizeIndex)
		{
			return GameViewSizes.GetConstrainedRect (renderRect, currentSizeGroupType, sizeIndex);
	}

		internal Vector2 GetSize ()
	{
			m_Pos = m_Parent.borderSize.Remove (m_Parent.position);
			Rect constrainedRect = GetConstainedGameViewRenderRect (gameViewRenderRect, selectedSizeIndex);
			return new Vector2 (constrainedRect.width, constrainedRect.height);
	}

		public void InitSize ()
	{
			if (m_Parent != null)
		{
				m_Pos = m_Parent.borderSize.Remove (m_Parent.position);

				EnsureSelectedSizeAreValid ();
				Rect constrainedRect = GetConstainedGameViewRenderRect (gameViewRenderRect, selectedSizeIndex);
				SetInternalGameViewRect (constrainedRect);
			}
		}

		void SelectionCallback (int indexClicked, object objectSelected) 
		{
			if (indexClicked != selectedSizeIndex)
			{
				selectedSizeIndex = indexClicked;
				dontClearBackground = true; // will cause re-clear
				GameViewAspectWasChanged ();
			}
		}
		
		private void DoToolbarGUI ()
		{
			GameViewSizes.instance.RefreshStandaloneAndWebplayerDefaultSizes ();
			if (GameViewSizes.instance.GetChangeID () != m_SizeChangeID)
			{
				EnsureSelectedSizeAreValid ();
				m_SizeChangeID = GameViewSizes.instance.GetChangeID ();
			}

			GUILayout.BeginHorizontal (EditorStyles.toolbar);
			{
				EditorGUILayout.GameViewSizePopup (currentSizeGroupType, selectedSizeIndex, SelectionCallback, EditorStyles.toolbarDropDown, GUILayout.Width (160f));

			GUILayout.FlexibleSpace();

			m_MaximizeOnPlay = GUILayout.Toggle(m_MaximizeOnPlay, "Maximize on Play", EditorStyles.toolbarButton);
			m_Stats = GUILayout.Toggle(m_Stats, "Stats", EditorStyles.toolbarButton);

			Rect r = GUILayoutUtility.GetRect(gizmosContent, s_GizmoButtonStyle);
			Rect rightRect = new Rect (r.xMax - s_GizmoButtonStyle.border.right, r.y, s_GizmoButtonStyle.border.right, r.height);
			if (EditorGUI.ButtonMouseDown(rightRect, GUIContent.none, FocusType.Passive, GUIStyle.none))
			{
				Rect rect = GUILayoutUtility.topLevel.GetLast();
				if (AnnotationWindow.ShowAtPosition(rect, true))
				{
					GUIUtility.ExitGUI();
				}
			}
			m_Gizmos = GUI.Toggle (r, m_Gizmos, gizmosContent, s_GizmoButtonStyle);
		}
		GUILayout.EndHorizontal ();
	}
	
		private void OnGUI ()
	{
		if (s_GizmoButtonStyle == null)
			s_GizmoButtonStyle = "GV Gizmo DropDown";
			
			DoToolbarGUI ();

		// Setup game view rect, so that player loop can access it
			Rect entireGameViewRenderRect = gameViewRenderRect;
			Rect constrainedRect = GetConstainedGameViewRenderRect (entireGameViewRenderRect, selectedSizeIndex);
			Rect screenRect = GUIClip.Unclip (constrainedRect);
			SetInternalGameViewRect (screenRect);

			EditorGUIUtility.AddCursorRect (constrainedRect, MouseCursor.CustomCursor);

		EventType type = Event.current.type;

		// Gain mouse lock when clicking on game view content
			if (type == EventType.MouseDown && entireGameViewRenderRect.Contains (Event.current.mousePosition))
		{
			Unsupported.SetAllowCursorLock(true);
			Unsupported.SetAllowCursorHide(true);
		}
		// Lose mouse lock when pressing escape
		else if (type == EventType.KeyDown && Event.current.keyCode == KeyCode.Escape)
			Unsupported.SetAllowCursorLock(false);

		if (type == EventType.Repaint)
		{
			// Draw background texture if we use custom letterboxing or have any camera that clears whole screen
				if (!currentGameViewSize.isFreeAspectRatio || !InternalEditorUtility.HasFullscreenCamera ())
			{
					GUI.Box (entireGameViewRenderRect, GUIContent.none, "GameViewBackground");
			}
			
			Vector2 oldOffset = GUIUtility.s_EditorScreenPointOffset;
			GUIUtility.s_EditorScreenPointOffset = Vector2.zero;
			SavedGUIState oldState = SavedGUIState.Create ();
	
				EditorGUIUtility.RenderGameViewCameras (screenRect, m_Gizmos, true);
			
			oldState.ApplyAndForget ();
			GUIUtility.s_EditorScreenPointOffset = oldOffset;
		}
		else if (type != EventType.Layout && type != EventType.Used)
		{
			if (WindowLayout.s_MaximizeKey.activated)
			{
				if (!EditorApplication.isPlaying || EditorApplication.isPaused)
					return;
			}

				bool mousePosInGameViewRect = constrainedRect.Contains (Event.current.mousePosition);
			
			// MouseDown events outside game view rect are not send to scripts but MouseUp events are (see below)
			if (Event.current.rawType == EventType.MouseDown && !mousePosInGameViewRect)
				return;

			// Transform events into local space, so the mouse position is correct
			// Then queue it up for playback during playerloop
				Event.current.mousePosition = new Vector2 (	Event.current.mousePosition.x - constrainedRect.x,
															Event.current.mousePosition.y - constrainedRect.y);
			EditorGUIUtility.QueueGameViewInputEvent (Event.current);
			
			bool useEvent = true;

			// Do not use mouse UP event if mousepos is outside game view rect (fix for case 380995: Gameview tab's context menu is not appearing on right click)
			// Placed after event queueing above to ensure scripts can react on mouse up events.
			if (Event.current.rawType == EventType.MouseUp && !mousePosInGameViewRect)
				useEvent = false;

			// Don't use command events, or they won't be sent to other views.
			if (type == EventType.ExecuteCommand || type == EventType.ValidateCommand)
				useEvent = false;
			
			if (useEvent)
				Event.current.Use();
			else
					Event.current.mousePosition = new Vector2 (	Event.current.mousePosition.x + constrainedRect.x,
																Event.current.mousePosition.y + constrainedRect.y);
		} 
		if (m_Stats)
			GameViewGUI.GameViewStatsGUI ();		
	}
}
}
