using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.Linq;

namespace UnityEditorInternal
{
	internal class DopeSheetEditor : TimeArea
	{
		// How much rendered keyframe left edge is visually offset when compared to the time it represents. 
		// A diamond shape left edge isn't representing the time, the middle part is.
		private const int k_KeyframeOffset = -4;
		// Pptr keyframe preview also needs 1px offset so it sits more tightly in the grid
		private const int k_PptrKeyframeOffset = -1;
		
		struct DrawElement
		{
			public Rect position;
			public Color color;
			public Texture2D texture;

			public DrawElement (Rect position, Color color, Texture2D texture)
			{
				this.position = position;
				this.color = color;
				this.texture = texture;
			}
		}

		#region Fields

		public float contentHeight {
			get
			{
				float height = 0f;
				
				foreach (DopeLine dopeline in state.dopelines)
					height += dopeline.tallMode ? AnimationWindowHierarchyGUI.k_DopeSheetRowHeightTall : AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;

				height += AnimationWindowHierarchyGUI.k_AddCurveButtonNodeHeight;
				return height;
			}
		}

		EditorWindow m_Owner;
		AnimationWindowState state;
		List<AnimationWindowKeyframe> m_KeyframeClipboard;
		DopeSheetSelectionRect m_SelectionRect;
		Texture m_DefaultDopeKeyIcon;

		float m_DragStartTime;
		bool m_MousedownOnKeyframe;
		bool m_IsDragging;
		bool m_IsDraggingPlayheadStarted;
		bool m_IsDraggingPlayhead;

		public Bounds m_Bounds = new Bounds (Vector3.zero, Vector3.zero);
		public override Bounds drawingBounds { get { return m_Bounds; } }

		// Keyframes are first resolved and saved to these buffers
		List<DrawElement> selectedKeysDrawBuffer;
		List<DrawElement> unselectedKeysDrawBuffer;
		List<DrawElement> dragdropKeysDrawBuffer;

		internal int assetPreviewManagerID
		{
			get { return m_Owner.GetInstanceID(); }
		}

		public bool m_SpritePreviewLoading;
		public int m_SpritePreviewCacheSize;

		#endregion

		#region Initialize / Destroy

		public DopeSheetEditor (AnimationWindowState state, EditorWindow owner) : base (false)
		{
			// Initialise fields
			this.state = state;
			m_Owner = owner;
			m_DefaultDopeKeyIcon = EditorGUIUtility.LoadIcon ("blendKey");

			m_KeyframeClipboard = new List<AnimationWindowKeyframe> ();

			// Set TimeArea constrains
			hSlider = true;
			vSlider = false;
			vRangeLocked = true;
			hRangeMin = 0;
			margin = 40;
			scaleWithWindow = true;
			ignoreScrollWheelUntilClicked = false;			
			hTicks.SetTickModulosForFrameRate (state.frameRate);
		}

		internal void OnDestroy ()
		{
			AssetPreview.DeletePreviewTextureManagerByID (assetPreviewManagerID);
		}

		#endregion

		#region GUI

		public void OnGUI (Rect position, Vector2 scrollPosition)
		{
			// drag'n'drops outside any dopelines
			EditorGUI.BeginDisabledGroup (!state.IsEditable);
			HandleDragAndDrop ();
			EditorGUI.EndDisabledGroup();

			EditorGUI.BeginDisabledGroup(state.IsReadOnly);
			GUIClip.Push (position, scrollPosition, Vector2.zero, false);

			Rect localRect = new Rect (0, 0, position.width, position.height);
			Rect dopesheetRect = DopelinesGUI (localRect, scrollPosition);

			if (GUI.enabled)
			{
				HandleKeyboard ();
				HandleDragging ();
				HandleSelectionRect (dopesheetRect);
				HandleDelete();
			}

			GUIClip.Pop ();
			EditorGUI.EndDisabledGroup();
		}

		public void RecalculateBounds()
        {
            if (state.m_ActiveAnimationClip)
                m_Bounds.SetMinMax(new Vector3(state.m_ActiveAnimationClip.startTime, 0, 0), new Vector3(state.m_ActiveAnimationClip.stopTime, 0, 0));
        }

		private Rect DopelinesGUI (Rect position, Vector2 scrollPosition)
		{
			Color oldColor = GUI.color;
			Rect linePosition = position;

			selectedKeysDrawBuffer = new List<DrawElement>();
			unselectedKeysDrawBuffer = new List<DrawElement>();
			dragdropKeysDrawBuffer = new List<DrawElement>();

			if (Event.current.type == EventType.Repaint)
				m_SpritePreviewLoading = false;

			// Workaround for cases when mouseup happens outside the window. Apparently the mouseup event is lost (not true on OSX, though).
			if (Event.current.type == EventType.MouseDown)
				m_IsDragging = false;
	
			// Find out how large preview pool is needed for sprite previews
			UpdateSpritePreviewCacheSize ();

			foreach (DopeLine dopeLine in state.dopelines)
			{
				dopeLine.position = linePosition;
				dopeLine.position.height = (dopeLine.tallMode ? AnimationWindowHierarchyGUI.k_DopeSheetRowHeightTall : AnimationWindowHierarchyGUI.k_DopeSheetRowHeight);

				// Cull out dopelines that are not visible
				if (dopeLine.position.yMin + scrollPosition.y >= position.yMin && dopeLine.position.yMin + scrollPosition.y <= position.yMax ||
					dopeLine.position.yMax + scrollPosition.y >= position.yMin && dopeLine.position.yMax + scrollPosition.y <= position.yMax)
				{ 
					Event evt = Event.current;

					switch (evt.type)
					{
						case EventType.DragUpdated:
						case EventType.DragPerform:
						{
							if (state.IsEditable)
								HandleDragAndDrop (dopeLine);
							break;
						}
						case EventType.MouseDown:
						{
							HandleMouseDown (dopeLine);
							break;
						}
						case EventType.Repaint:
						{
							DopeLineRepaint (dopeLine);
							break;
						}
						case EventType.MouseUp:
						{
							if (evt.button == 1 && !m_IsDraggingPlayhead)
							{
								HandleContextMenu (dopeLine);
							}

							break;						
						}					
					}
				}

				linePosition.y += dopeLine.position.height;
			}

			if (Event.current.type == EventType.MouseUp)
			{
				m_IsDraggingPlayheadStarted = false;
				m_IsDraggingPlayhead = false;
			}

			Rect dopelinesRect = new Rect(position.xMin, position.yMin, position.width, linePosition.yMax - position.yMin);

			DrawGrid(dopelinesRect);

			if (AnimationMode.InAnimationMode())
				DrawPlayhead(dopelinesRect);

			DrawElements (unselectedKeysDrawBuffer);
			DrawElements (selectedKeysDrawBuffer);
			DrawElements (dragdropKeysDrawBuffer);

			GUI.color = oldColor;

			return dopelinesRect;
		}

		private void DrawGrid (Rect position)
		{
			state.m_AnimationWindow.TimeLineGUI (position, true, false);
		}

		private void DrawPlayhead (Rect position)
		{
			float xPos = state.TimeToPixel (state.GetTimeSeconds ());
			AnimationWindow.DrawPlayHead (new Vector2 (xPos, 0), new Vector2 (xPos, position.height), 1f);
		}

		void UpdateSpritePreviewCacheSize()
		{
			int newPreviewCacheSize = 1;

			// Add all expanded sprite dopelines
			foreach (DopeLine dopeLine in state.dopelines)
			{
				if (dopeLine.tallMode && dopeLine.isPptrDopeline)
				{
					newPreviewCacheSize += dopeLine.keys.Count;
				}
			}

			// Add all drag'n'drop objects
			newPreviewCacheSize += DragAndDrop.objectReferences.Length;

			if (newPreviewCacheSize > m_SpritePreviewCacheSize)
			{
				AssetPreview.SetPreviewTextureCacheSize (newPreviewCacheSize, assetPreviewManagerID);
				m_SpritePreviewCacheSize = newPreviewCacheSize;
			}
		}

		void DrawElements (List<DrawElement> elements)
		{
			Color oldColor = GUI.color;

			Color color = Color.white;
			GUI.color = color;
			Texture icon = m_DefaultDopeKeyIcon;

			foreach (DrawElement element in elements)
			{
				// Change color
				if (element.color != color)
				{
					color = GUI.enabled ? element.color : element.color * 0.8f;
					GUI.color = color;
				}

				if (element.texture != null)
					GUI.DrawTexture (element.position, element.texture);
				else
				{
					Rect rect = new Rect (element.position.center.x - icon.width / 2 - 1.5f,
										  element.position.center.y - icon.height / 2,
										  icon.width,
										  icon.height);

					GUI.DrawTexture (rect, icon, ScaleMode.ScaleToFit, true, 1);
				}
			}

			GUI.color = oldColor;
		}

		private void DopeLineRepaint (DopeLine dopeline)
		{
			Color oldColor = GUI.color;
			
			AnimationWindowHierarchyNode node = (AnimationWindowHierarchyNode)state.m_HierarchyData.FindItem (dopeline.m_HierarchyNodeID);
			bool isChild = node != null && node.depth > 0;
			Color color = isChild ? Color.gray.AlphaMultiplied (0.05f) : Color.gray.AlphaMultiplied (0.16f);

			// Draw background
			if (!dopeline.isMasterDopeline)
				DrawBox (dopeline.position, color);
			else
				AnimationWindow.ms_Styles.eventBackground.Draw (dopeline.position, false, false, false, false);
			
			// Draw keys
			int? previousTimeHash = null;
			int length = dopeline.keys.Count;

			for (int i = 0; i < length; i++)
			{
				AnimationWindowKeyframe keyframe = dopeline.keys[i];
				// Hash optimizations
				if (previousTimeHash == keyframe.m_TimeHash)
					continue;
				
				previousTimeHash = keyframe.m_TimeHash;

				// Default values
				Rect rect = GetKeyframeRect (dopeline, keyframe);
				color = dopeline.isMasterDopeline ? Color.gray.RGBMultiplied (0.85f) : Color.gray.RGBMultiplied (1.2f);
				Texture2D texture = null;

				if (keyframe.isPPtrCurve && dopeline.tallMode)
					texture = keyframe.value == null ? null : AssetPreview.GetAssetPreview (((Object)keyframe.value).GetInstanceID (), assetPreviewManagerID);

				if (texture != null)
				{
					rect = GetPreviewRectFromKeyFrameRect (rect);
					color = Color.white.AlphaMultiplied (0.5f);
				}
				else if (keyframe.value != null && keyframe.isPPtrCurve && dopeline.tallMode)
				{
					m_SpritePreviewLoading = true;
				}

				// TODO: Find out why all keys at 0f time are offset from grid and adding this little magic number helps (on Windows). Some floating point error thingy. Might be per GPU and not per OS.
				if (Application.platform == RuntimePlatform.WindowsEditor)
					rect.xMin -= 0.01f;

				if (AnyKeyIsSelectedAtTime (dopeline, i))
				{
					color = dopeline.tallMode && dopeline.isPptrDopeline ? Color.white : new Color (0.34f, 0.52f, 0.85f, 1f);
					if (dopeline.isMasterDopeline)
						color = color.RGBMultiplied (0.85f);

					selectedKeysDrawBuffer.Add (new DrawElement (rect, color, texture));
				}
				else
				{
					unselectedKeysDrawBuffer.Add (new DrawElement (rect, color, texture));
				}
			}
			
			if (state.IsEditable && DoDragAndDrop (dopeline, dopeline.position, false))
			{
				float time = Mathf.Max (state.PixelToTime (Event.current.mousePosition.x), 0f);
				
				Color keyColor = Color.gray.RGBMultiplied (1.2f);
				Texture2D texture = null;

				foreach (Object obj in GetSortedDragAndDropObjectReferences ())
				{
					Rect rect = GetDragAndDropRect (dopeline, state.TimeToPixel(time));

					if (dopeline.isPptrDopeline && dopeline.tallMode)
						texture = AssetPreview.GetAssetPreview (obj.GetInstanceID (), assetPreviewManagerID);

					if (texture != null)
					{
						rect = GetPreviewRectFromKeyFrameRect (rect);
						keyColor = Color.white.AlphaMultiplied (0.5f);
					}
					
					dragdropKeysDrawBuffer.Add (new DrawElement (rect, keyColor, texture));

					time += 1f / state.frameRate;
				}
			}

			GUI.color = oldColor;
		}

		private Rect GetPreviewRectFromKeyFrameRect (Rect keyframeRect)
		{
			keyframeRect.width -= 2;
			keyframeRect.height -= 2;
			keyframeRect.xMin += 2;
			keyframeRect.yMin += 2;

			return keyframeRect;
		}

		private Rect GetDragAndDropRect (DopeLine dopeline, float screenX)
		{
			Rect rect = GetKeyframeRect (dopeline, 0f);
			float offsetX = GetKeyframeOffset (dopeline);
			float time = state.PixelToTime (screenX - rect.width * .5f, true);
			rect.center = new Vector2 (state.TimeToPixel (time) + rect.width * .5f + offsetX, rect.center.y);
			return rect;
		}

		// TODO: This is just temporary until real styles
		private static void DrawBox (Rect position, Color color)
		{
			Color oldColor = GUI.color; 
			GUI.color = color;
			DopeLine.dopekeyStyle.Draw (position, GUIContent.none, 0, false);
			GUI.color = oldColor;
		}

		// TODO: Get these somewhere more generic
		private static void DrawLine (Vector2 p1, Vector2 p2, Color color)
		{
			HandleUtility.handleWireMaterial.SetPass (0);
			GL.Begin (GL.LINES);
			GL.Color (color);
			GL.Vertex (new Vector3 (p1.x, p1.y, 0f));
			GL.Vertex (new Vector3 (p2.x, p2.y, 0f));
			GL.End ();
			GL.Color (Color.white);
		}

		private GenericMenu GenerateMenu (DopeLine dopeline, bool clickedEmpty)
		{		
			GenericMenu menu = new GenericMenu ();

			// Menu items that are only applicaple when in animation mode:
			if (AnimationMode.InAnimationMode ())
			{
				string str = "Add Key";
				if (clickedEmpty)
					menu.AddItem (new GUIContent (str), false, AddKeyToDopeline, dopeline);
				else
					menu.AddDisabledItem (new GUIContent (str));

				str = state.selectedKeys.Count > 1 ? "Delete Keys" : "Delete Key";
				if (state.selectedKeys.Count > 0)
					menu.AddItem (new GUIContent (str), false, DeleteSelectedKeys);
				else
					menu.AddDisabledItem (new GUIContent (str));

				// Float curve tangents
				if (AnimationWindowUtility.ContainsFloatKeyframes(state.selectedKeys))
				{
					menu.AddSeparator (string.Empty);

					List<KeyIdentifier> keyList = new List<KeyIdentifier> ();
					foreach (AnimationWindowKeyframe key in state.selectedKeys)
					{
						if (key.isPPtrCurve)
							continue;

						int index = key.curve.GetKeyframeIndex (AnimationKeyTime.Time (key.time, state.frameRate));
						if (index == -1)
							continue;

						CurveRenderer renderer = CurveRendererCache.GetCurveRenderer (state.m_ActiveAnimationClip, key.curve.binding);
						int id = CurveUtility.GetCurveID (state.m_ActiveAnimationClip, key.curve.binding);
						keyList.Add (new KeyIdentifier (renderer, id, index));
					}

					CurveMenuManager menuManager = new CurveMenuManager (state.m_AnimationWindow);
					menuManager.AddTangentMenuItems (menu, keyList);
				}
			}

			return menu;
		}

		#endregion

		#region Handle Input

		private void HandleDragging ()
		{
			int id = EditorGUIUtility.GetControlID ("dopesheetdrag".GetHashCode (), FocusType.Passive, new Rect ());

			if ((Event.current.type == EventType.MouseDrag || Event.current.type == EventType.MouseUp) && m_MousedownOnKeyframe)
			{
				if (Event.current.type == EventType.MouseDrag && !Event.current.control && !Event.current.shift)
				{
					if (!m_IsDragging && state.selectedKeys.Count > 0)
					{
						m_IsDragging = true;
						m_IsDraggingPlayheadStarted = true;
						GUIUtility.hotControl = id;
						m_DragStartTime = state.PixelToTime (Event.current.mousePosition.x);
						Event.current.Use ();
					}
				}

				// What is the distance from first selected key to zero time. We need this in order to make sure no key goes to negative time while dragging.
				float firstSelectedKeyTime = float.MaxValue;
				foreach (AnimationWindowKeyframe selectedKey in state.selectedKeys)
					firstSelectedKeyTime = Mathf.Min(selectedKey.time, firstSelectedKeyTime);
				
				float currentTime = state.SnapToFrame (state.PixelToTime (Event.current.mousePosition.x));
				float deltaTime = Mathf.Max(currentTime - m_DragStartTime, firstSelectedKeyTime * -1f);
				
				if (m_IsDragging)
				{
					if (!Mathf.Approximately (currentTime, m_DragStartTime))
					{
						m_DragStartTime = currentTime;
						state.MoveSelectedKeys (deltaTime, true, false);
						if (state.activeKeyframe != null)
							state.m_Frame = state.TimeToFrameFloor (state.activeKeyframe.time);
						Event.current.Use ();
					}
				}

				if (Event.current.type == EventType.mouseUp)
				{
					if (m_IsDragging && GUIUtility.hotControl == id)
					{
						state.MoveSelectedKeys (deltaTime, true, true);
						Event.current.Use ();
						m_IsDragging = false;
					}
					m_MousedownOnKeyframe = false;
					GUIUtility.hotControl = 0;
				}

			}

			if (m_IsDraggingPlayheadStarted && Event.current.type == EventType.MouseDrag && Event.current.button == 1)
			{
				m_IsDraggingPlayhead = true;
				
				int frame = state.m_Frame;
				if (!m_IsDragging)
					frame = state.TimeToFrameFloor(state.SnapToFrame (state.PixelToTime (Event.current.mousePosition.x)));

				state.m_AnimationWindow.PreviewFrame (frame);
				Event.current.Use();
			}

		}

		private void HandleKeyboard ()
		{
			if (Event.current.type == EventType.ValidateCommand || Event.current.type == EventType.ExecuteCommand)
			{
				switch (Event.current.commandName)
				{
					case "Copy":
						if(Event.current.type == EventType.ExecuteCommand)
							HandleCopy ();
						Event.current.Use();
						break;
					case "Paste":
						if (Event.current.type == EventType.ExecuteCommand)
							HandlePaste ();
						Event.current.Use ();
						break;
					case "SelectAll":
						if (Event.current.type == EventType.ExecuteCommand)
							HandleSelectAll ();
						Event.current.Use ();
						break;
					case "FrameSelected":
						if (Event.current.type == EventType.ExecuteCommand)
							FrameSelected();
						Event.current.Use ();
						break;
				}
			}
		}

		private void HandleCopy ()
		{
			float smallestTime = float.MaxValue;
			m_KeyframeClipboard.Clear ();
			foreach (AnimationWindowKeyframe keyframe in state.selectedKeys)
			{
				m_KeyframeClipboard.Add (new AnimationWindowKeyframe (keyframe));
				if (keyframe.time < smallestTime)
					smallestTime = keyframe.time;
			}
			foreach (AnimationWindowKeyframe keyframe in m_KeyframeClipboard)
			{
				keyframe.time -= smallestTime;
			}
		}

		private void HandlePaste ()
		{
			state.ClearKeySelections ();
			foreach (AnimationWindowKeyframe keyframe in m_KeyframeClipboard)
			{
				AnimationWindowKeyframe newKeyframe = new AnimationWindowKeyframe (keyframe);
				newKeyframe.time += state.GetTimeSeconds ();
				newKeyframe.curve.m_Keyframes.Add (newKeyframe);
				state.SelectKey (newKeyframe);

				// TODO: Optimize to only save curve once instead once per keyframe
				state.SaveCurve (keyframe.curve);
			}
		}

		private void HandleSelectAll()
		{
			foreach (DopeLine dopeline in state.dopelines)
			{
				foreach (AnimationWindowKeyframe keyframe in dopeline.keys)
				{
					state.SelectKey (keyframe);
				}
				state.SelectHierarchyItem (dopeline, true, false);
			}
		}

		private void HandleDelete ()
		{
			switch (Event.current.type)
			{
				case EventType.ValidateCommand:
				case EventType.ExecuteCommand:
					if ((Event.current.commandName == "SoftDelete" || Event.current.commandName == "Delete"))
					{
						if (Event.current.type == EventType.ExecuteCommand)
							state.DeleteSelectedKeys ();
						Event.current.Use ();
					}
					break;

				case EventType.KeyDown:
					if (Event.current.keyCode == KeyCode.Backspace || Event.current.keyCode == KeyCode.Delete)
					{
						state.DeleteSelectedKeys ();
						Event.current.Use ();
					}
					break;
			}
		}

		private void HandleSelectionRect (Rect rect)
		{
			if (m_SelectionRect == null)
				m_SelectionRect = new DopeSheetSelectionRect (this);

			if(!m_MousedownOnKeyframe)
				m_SelectionRect.OnGUI (rect);
		}

		// Handles drag and drop into empty area outside dopelines
		private void HandleDragAndDrop ()
		{
			Event evt = Event.current;

			if (evt.type != EventType.DragPerform && evt.type != EventType.DragUpdated)
				return;

			if (!ValidateDragAndDropObjects () || state.m_ActiveGameObject == null)
				return;

			// TODO: handle multidropping of other types than sprites/textures
			if (DragAndDrop.objectReferences[0].GetType () == typeof (Sprite) || DragAndDrop.objectReferences[0].GetType () == typeof (Texture2D))
			{
				// TODO: Remove state having a reference to animation window once the whole deprecated animation window is rewritten and we hopefully have better way of ensuring we are ready to add curves
				if (evt.type == EventType.DragPerform)
				{
					if (!state.m_AnimationWindow.EnsureAnimationMode ())
					{
						DragAndDrop.visualMode = DragAndDropVisualMode.Rejected;
						return;
					}
				}

				EditorCurveBinding? spriteBinding = null;
				
				string targetPath = AnimationUtility.CalculateTransformPath (state.m_ActiveGameObject.transform, state.m_RootGameObject.transform);
				
				// Let's see if such dopeline already exists and if it does then just quit
				foreach (DopeLine dopeline in state.dopelines)
				{
					if (dopeline.valueType == typeof (Sprite) && dopeline.objectType == typeof (SpriteRenderer))
					{
						AnimationWindowHierarchyNode node = (AnimationWindowHierarchyNode)state.m_HierarchyData.FindItem (dopeline.m_HierarchyNodeID);
						if (node != null && node.path.Equals(targetPath))
						{
							DragAndDrop.visualMode = DragAndDropVisualMode.Rejected;
							return;
						}
					}
				}
				
				// Let's make sure there is spriterenderer to animate
				if (!state.m_ActiveGameObject.GetComponent<SpriteRenderer> ())
					state.m_ActiveGameObject.AddComponent<SpriteRenderer> ();

				// Now we should always find an animatable binding for it
				EditorCurveBinding[] curveBindings = AnimationUtility.GetAnimatableBindings (state.m_ActiveGameObject, state.m_RootGameObject);
				foreach (EditorCurveBinding curveBinding in curveBindings)
					if (curveBinding.isPPtrCurve && curveBinding.type == typeof (SpriteRenderer))
						spriteBinding = curveBinding;

				if (spriteBinding != null)
				{
					if (evt.type == EventType.DragPerform)
					{
						if(DragAndDrop.objectReferences.Length == 1)
							Analytics.Event ("Sprite Drag and Drop", "Drop single sprite into empty dopesheet", "null", 1);
						else
							Analytics.Event ("Sprite Drag and Drop", "Drop multiple sprites into empty dopesheet", "null", 1);
						
						// Create the new curve for our sprites
						AnimationWindowCurve newCurve = new AnimationWindowCurve (state.m_ActiveAnimationClip, (EditorCurveBinding)spriteBinding, typeof(Sprite));
						state.SaveCurve (newCurve);

						// And finally perform the drop onto the curve
						PeformDragAndDrop (newCurve, 0f);
					}
					DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
					evt.Use ();
					return;
				}	
			}
			DragAndDrop.visualMode = DragAndDropVisualMode.Rejected;
		}

		private void HandleDragAndDrop (DopeLine dopeline)
		{
			Event evt = Event.current;
			
			if (evt.type != EventType.DragPerform && evt.type != EventType.DragUpdated)
				return;

			if (DoDragAndDrop (dopeline, dopeline.position, evt.type == EventType.DragPerform))
			{
				DragAndDrop.visualMode = DragAndDropVisualMode.Copy;
				evt.Use ();
			}
			else
			{
				DragAndDrop.visualMode = DragAndDropVisualMode.Rejected;
			}
		}

		private void HandleMouseDown (DopeLine dopeline)
		{
			Event evt = Event.current;

			state.m_AnimationWindow.EnsureAnimationMode ();

			// If single keyframe is clicked without shift or control, then clear all other selections
			if (!evt.control && !evt.shift)
			{
				foreach (AnimationWindowKeyframe keyframe in dopeline.keys)
				{
					Rect r = GetKeyframeRect (dopeline, keyframe);
					if (r.Contains (evt.mousePosition) && !state.KeyIsSelected (keyframe))
					{
						state.ClearKeySelections ();
						break;
					}
				}
			}

			float startTime = state.PixelToTime (Event.current.mousePosition.x);
			float endTime = startTime;

			// For shift selecting we need to have time range we choose between
			if (Event.current.shift)
			{
				foreach (AnimationWindowKeyframe key in dopeline.keys)
				{
					if (state.KeyIsSelected (key))
					{
						if (key.time < startTime)
							startTime = key.time;
						if (key.time > endTime)
							endTime = key.time;
					}
				}
			}

			bool clickedOnKeyframe = false;
			foreach (AnimationWindowKeyframe keyframe in dopeline.keys)
			{
				Rect r = GetKeyframeRect (dopeline, keyframe);
				if (r.Contains (evt.mousePosition))
				{
					clickedOnKeyframe = true;

					if (!state.KeyIsSelected (keyframe))
					{
						if (Event.current.shift)
						{
							foreach (AnimationWindowKeyframe key in dopeline.keys)
								if (key == keyframe || key.time > startTime && key.time < endTime)
									state.SelectKey (key);
						}
						else
						{
							state.SelectKey (keyframe);
						}

						state.SelectHierarchyItem (dopeline, evt.control || evt.shift);
					}
					else
					{
						if (evt.control)
							state.UnselectKey(keyframe);
					}
					state.activeKeyframe = keyframe;
					m_MousedownOnKeyframe = true;
					state.m_Frame = state.TimeToFrameFloor (state.activeKeyframe.time);
					evt.Use ();
				}
			}

			if (dopeline.position.Contains (Event.current.mousePosition))
			{
				if (evt.clickCount == 2 && evt.button == 0 && !Event.current.shift && !Event.current.control)
					HandleDopelineDoubleclick (dopeline);

				// Move playhead when clicked with right mouse button
				if (evt.button == 1)
				{
					float timeAtMousePosition = state.PixelToTime (Event.current.mousePosition.x, true);
					AnimationKeyTime mouseKeyTime = AnimationKeyTime.Time (timeAtMousePosition, state.frameRate);
					state.m_Frame = mouseKeyTime.frame;

					// Clear keyframe selection if right clicked empty space
					if (!clickedOnKeyframe)
					{
						state.ClearKeySelections();
						m_IsDraggingPlayheadStarted = true;
						HandleUtility.Repaint();
						evt.Use ();
					}
				}
			}
		}

		private void HandleDopelineDoubleclick (DopeLine dopeline)
		{
			state.ClearKeySelections();
			float timeAtMousePosition = state.PixelToTime (Event.current.mousePosition.x, true);
			AnimationKeyTime mouseKeyTime = AnimationKeyTime.Time (timeAtMousePosition, state.frameRate);
			foreach (AnimationWindowCurve curve in dopeline.m_Curves)
			{
				AnimationWindowKeyframe keyframe = AnimationWindowUtility.AddKeyframeToCurve (state, curve, mouseKeyTime);
				state.SelectKey (keyframe);
			}
			state.m_Frame = mouseKeyTime.frame;

			Event.current.Use();
		}

		private void HandleContextMenu (DopeLine dopeline)
		{
			if (!dopeline.position.Contains (Event.current.mousePosition))
				return;

			// Find out if clicked on top of keyframe or in empty space
			bool clickedEmpty = true;
			foreach (var key in dopeline.keys)
			{
				Rect rect = GetKeyframeRect (dopeline, key);

				if (rect.Contains (Event.current.mousePosition))					
				{
					clickedEmpty = false;
					break;					
				}
			}

			// TODO: Remove this hack to keep curve editor updated to have working context menu on dopesheet
			state.m_AnimationWindow.RefreshShownCurves (true);

			// Actual context menu
			GenerateMenu (dopeline, clickedEmpty).ShowAsContext ();
		}

		private Rect GetKeyframeRect (DopeLine dopeline, AnimationWindowKeyframe keyframe)
		{
			return GetKeyframeRect (dopeline, state.SnapToFrame (keyframe.time));
		}

		private Rect GetKeyframeRect (DopeLine dopeline, float time)
		{
			if (dopeline.isPptrDopeline && dopeline.tallMode)
				return new Rect (state.TimeToPixel (time) + GetKeyframeOffset (dopeline), dopeline.position.yMin, dopeline.position.height, dopeline.position.height);
			else
				return new Rect (state.TimeToPixel (time) + GetKeyframeOffset (dopeline), dopeline.position.yMin, 10, dopeline.position.height);
		}

		// This means "how much is the rendered keyframe offset in pixels for x-axis".
		// Say you are rendering keyframe to some time t. The time t relates to some pixel x, but you then need to offset because keyframe diamond center represents the time, not the left edge
		// However for pptr keyframes, the time is represented by left edge
		private float GetKeyframeOffset (DopeLine dopeline)
		{
			if (dopeline.isPptrDopeline && dopeline.tallMode)
				return k_PptrKeyframeOffset;
			else
				return k_KeyframeOffset;
		}
		
		// Frame the selected keyframes or selected dopelines
		public void FrameClip()
		{
			if (!state.m_ActiveAnimationClip)
				return;

			float maxTime = Mathf.Max (state.m_ActiveAnimationClip.length, 1);
			SetShownHRangeInsideMargins (0, maxTime);
		}

		public void FrameSelected ()
		{
			float minTime = float.MaxValue;
			float maxTime = float.MinValue;			

			bool keyframesSelected = state.selectedKeys.Count > 0;

			if (keyframesSelected)
			{
				foreach (AnimationWindowKeyframe key in state.selectedKeys)
				{
					minTime = Mathf.Min (key.time, minTime);
					maxTime = Mathf.Max (key.time, maxTime);
				}
			}

			// No keyframes selected. Frame to selected dopelines
			bool frameToClip = !keyframesSelected;
			if (!keyframesSelected)
			{
				bool dopelinesSelected = state.m_hierarchyState.selectedIDs.Count > 0;
				if (dopelinesSelected)
				{
					foreach (AnimationWindowCurve curve in state.activeCurves)
					{
						int keyCount = curve.m_Keyframes.Count;

						if (keyCount > 1)
						{
							minTime = Mathf.Min (curve.m_Keyframes[0].time, minTime);
							maxTime = Mathf.Max (curve.m_Keyframes[keyCount - 1].time, maxTime);
							frameToClip = false;
						}
					}
				}
			}

			if (frameToClip)
				FrameClip();
			else
			{
				// Let's make sure we don't zoom too close.
				float padding = state.FrameToTime (Mathf.Min(4, state.frameRate));
				padding = Mathf.Min (padding, Mathf.Max (state.m_ActiveAnimationClip.length, state.FrameToTime(4)));

				maxTime = Mathf.Max (maxTime, minTime + padding);
				SetShownHRangeInsideMargins (minTime, maxTime);
			}
		}

		#endregion

		#region Utils

		private bool DoDragAndDrop (DopeLine dopeLine, Rect position, bool perform)
		{
			return DoDragAndDrop (dopeLine, position, false, perform);
		}
		private bool DoDragAndDrop (DopeLine dopeLine, bool perform)
		{
			return DoDragAndDrop (dopeLine, new Rect (), true, perform);
		}
		private bool DoDragAndDrop (DopeLine dopeLine, Rect position, bool ignoreMousePosition, bool perform)
		{
			if (!ignoreMousePosition && position.Contains (Event.current.mousePosition) == false)
				return false;

			if (!ValidateDragAndDropObjects ()) 
				return false;

			System.Type targetType = DragAndDrop.objectReferences[0].GetType();

			AnimationWindowCurve curve = null;
			if (dopeLine.valueType == targetType)
			{
				curve = dopeLine.m_Curves[0];
			} 
			else 
			{
				// dopeline ValueType wasn't exact match. We can still look for a curve that accepts our drop object type
				foreach (AnimationWindowCurve dopelineCurve in dopeLine.m_Curves)
				{
					if (dopelineCurve.isPPtrCurve)
					{
						if (dopelineCurve.m_ValueType == targetType)
							curve = dopelineCurve;
						if (dopelineCurve.m_ValueType == typeof (Sprite) && SpriteUtility.GetSpriteFromDraggedPathsOrObjects () != null)
						{
							curve = dopelineCurve;
							targetType = typeof (Sprite);
						}
					}
				}
			}

			bool success = true;
			if(curve != null)
			{
				if(perform)
				{
					if (DragAndDrop.objectReferences.Length == 1)
						Analytics.Event ("Sprite Drag and Drop", "Drop single sprite into existing dopeline", "null", 1);
					else
						Analytics.Event ("Sprite Drag and Drop", "Drop multiple sprites into existing dopeline", "null", 1);

					Rect rect = GetDragAndDropRect (dopeLine, Event.current.mousePosition.x);
					float time = state.PixelToTime (rect.xMin, true);
					AnimationWindowCurve targetCurve = GetCurveOfType (dopeLine, targetType);
					PeformDragAndDrop (targetCurve, time);
				}
			}
			else
			{
				success = false;
			}

			return success;
		}

		private void PeformDragAndDrop (AnimationWindowCurve targetCurve, float time)
		{
			if (DragAndDrop.objectReferences.Length == 0 || targetCurve == null)
				return;

			state.ClearKeySelections ();
			Object[] objectReferences = GetSortedDragAndDropObjectReferences ();			

			foreach (var obj in objectReferences)
			{
				Object value = obj;

				if (value is Texture2D)
					value = SpriteUtility.TextureToSprite (obj as Texture2D);

				CreateNewPPtrKeyframe (time, value, targetCurve);				
				time += 1f / state.m_ActiveAnimationClip.frameRate;
			}

			state.SaveCurve (targetCurve);
			DragAndDrop.AcceptDrag ();
		}

		private Object[] GetSortedDragAndDropObjectReferences ()
		{
			Object[] objectReferences = DragAndDrop.objectReferences;

			// Use same name compare as when we sort in the backend: See AssetDatabase.cpp: SortChildren
			System.Array.Sort(objectReferences, (a, b) => EditorUtility.SemiNumericCompare(a.name, b.name));

			return objectReferences;
		}

		private void CreateNewPPtrKeyframe (float time, Object value, AnimationWindowCurve targetCurve)
		{
			ObjectReferenceKeyframe referenceKeyframe = new ObjectReferenceKeyframe();

			referenceKeyframe.time = time;
			referenceKeyframe.value = value;

			AnimationWindowKeyframe keyframe = new AnimationWindowKeyframe(targetCurve, referenceKeyframe);
			AnimationKeyTime newTime = AnimationKeyTime.Time(keyframe.time, state.frameRate);
			targetCurve.AddKeyframe(keyframe, newTime);
			state.SelectKey(keyframe);
		}

		// if targetType == null, it means that all types are fine (as long as they are all of the same type)
		private static bool ValidateDragAndDropObjects ()
		{
			if (DragAndDrop.objectReferences.Length == 0)
				return false;

			// Let's be safe and early out if any of the objects are null or if they aren't all of the same type (exception beign sprite vs. texture2D, which are considered equal here)
			for (int i = 0; i < DragAndDrop.objectReferences.Length; i++)
			{
				Object obj = DragAndDrop.objectReferences[i];
				if (obj == null)
				{
					return false;
				}

				if (i < DragAndDrop.objectReferences.Length - 1)
				{
					Object nextObj = DragAndDrop.objectReferences[i + 1];
					bool bothAreSpritesOrTextures = (obj is Texture2D || obj is Sprite) && (nextObj is Texture2D || nextObj is Sprite);

					if (obj.GetType () != nextObj.GetType () && !bothAreSpritesOrTextures)
					{
						return false;
					}
				}
			}
			return true;
		}

		private AnimationWindowCurve GetCurveOfType (DopeLine dopeLine, System.Type type)
		{
			foreach (AnimationWindowCurve curve in dopeLine.m_Curves)
			{
				if (curve.m_ValueType == type)
					return curve;
			}
			return null;
		}
		
		// For optimizing. Starting from keyIndex, we check through any key with same time and see if any are selected
		private bool AnyKeyIsSelectedAtTime (DopeLine dopeLine, int keyIndex)
		{
			AnimationWindowKeyframe keyframe;
			int timeHash = dopeLine.keys[keyIndex].m_TimeHash;

			int length = dopeLine.keys.Count;
			for (int i = keyIndex; i < length; i++)
			{
				keyframe = dopeLine.keys[i];

				if (keyframe.m_TimeHash != timeHash)
					return false;

				if (state.KeyIsSelected (keyframe))
					return true;
			}

			return false;
		}

		private void AddKeyToDopeline (object obj) { AddKeyToDopeline ((DopeLine)obj); }
		private void AddKeyToDopeline (DopeLine dopeLine)
		{
			state.ClearKeySelections ();
			
			foreach (AnimationWindowCurve curve in dopeLine.m_Curves)
			{
				AnimationWindowKeyframe addedKeyframe = AnimationWindowUtility.AddKeyframeToCurve (state, curve, state.time);
				state.SelectKey (addedKeyframe);
			}
		}

		private void DeleteSelectedKeys ()
		{
			state.DeleteSelectedKeys ();
		}

		#endregion

		#region Utility classes

		internal class DopeSheetSelectionRect
		{
			Vector2 m_SelectStartPoint;
			Vector2 m_SelectMousePoint;
			bool m_ValidRect;
			private DopeSheetEditor owner;

			enum SelectionType { Normal, Additive, Subtractive }
			public readonly GUIStyle createRect = "U2D.createRect";

			static int s_RectSelectionID = GUIUtility.GetPermanentControlID ();

			public DopeSheetSelectionRect (DopeSheetEditor owner)
			{
				this.owner = owner;
			}

			public void OnGUI (Rect position)
			{
				Event evt = Event.current;
				Vector2 mousePos = evt.mousePosition;
				int id = s_RectSelectionID;
				switch (evt.GetTypeForControl (id))
				{
					case EventType.mouseDown:
						if (evt.button == 0 && position.Contains (mousePos))
						{
							GUIUtility.hotControl = id;
							m_SelectStartPoint = mousePos;
							m_ValidRect = false;
							evt.Use ();
						}
						break;
					case EventType.mouseDrag:
						if (GUIUtility.hotControl == id)
						{
							m_ValidRect = Mathf.Abs ((mousePos - m_SelectStartPoint).x) > 1f;

							if (m_ValidRect)
								m_SelectMousePoint = new Vector2 (mousePos.x, mousePos.y);

							evt.Use ();
						}
						break;

					case EventType.repaint:
						if (GUIUtility.hotControl == id && m_ValidRect)
							EditorStyles.selectionRect.Draw (GetCurrentPixelRect (), GUIContent.none, false, false, false, false);
						break;

					case EventType.mouseUp:
						if (GUIUtility.hotControl == id && evt.button == 0)
						{
							if (m_ValidRect)
							{
								if (!Event.current.control)
									owner.state.ClearKeySelections ();

								float frameRate = owner.state.frameRate;

								Rect timeRect = GetCurrentTimeRect ();
								AnimationKeyTime startTime = AnimationKeyTime.Time (timeRect.xMin, frameRate);
								AnimationKeyTime endTime = AnimationKeyTime.Time (timeRect.xMax, frameRate);

								GUI.changed = true;
								
								owner.state.ClearHierarchySelection();

								List<AnimationWindowKeyframe> toBeUnselected = new List<AnimationWindowKeyframe> ();
								List<AnimationWindowKeyframe> toBeSelected = new List<AnimationWindowKeyframe> ();

								foreach (DopeLine dopeline in owner.state.dopelines)
								{
									if (dopeline.position.yMin >= timeRect.yMin && dopeline.position.yMax <= timeRect.yMax)
									{
										foreach (AnimationWindowKeyframe keyframe in dopeline.keys)
										{
											AnimationKeyTime keyTime = AnimationKeyTime.Time (keyframe.time, frameRate);
											// for dopeline tallmode, we don't want to select the sprite at the end. It just feels wrong.
											if (!dopeline.tallMode && keyTime.frame >= startTime.frame && keyTime.frame <= endTime.frame ||
												 dopeline.tallMode && keyTime.frame >= startTime.frame && keyTime.frame < endTime.frame)
											{
												if (!toBeSelected.Contains (keyframe) && !toBeUnselected.Contains (keyframe))
												{
													if (!owner.state.KeyIsSelected (keyframe))
														toBeSelected.Add (keyframe);
													else if (owner.state.KeyIsSelected (keyframe))
														toBeUnselected.Add (keyframe);
												}
											}
										}
									}
								}

								// Only if all the keys inside rect are selected, we want to unselect them.
								if (toBeSelected.Count == 0)
									foreach (AnimationWindowKeyframe keyframe in toBeUnselected)
										owner.state.UnselectKey (keyframe);

								foreach (AnimationWindowKeyframe keyframe in toBeSelected)
									owner.state.SelectKey(keyframe);

								// Update hierarchy selection based on newly selected keys
								foreach (DopeLine dopeline in owner.state.dopelines)
									if (owner.state.AnyKeyIsSelected(dopeline))
										owner.state.SelectHierarchyItem(dopeline, true, false);
							}
							else
							{
								owner.state.ClearKeySelections ();
							}
							evt.Use ();
							GUIUtility.hotControl = 0;
						}
						break;
				}
			}

			public Rect GetCurrentPixelRect ()
			{
				float height = AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;
				Rect r = EditorGUIExt.FromToRect (m_SelectStartPoint, m_SelectMousePoint);
				r.xMin = owner.state.TimeToPixel (owner.state.PixelToTime (r.xMin, true), true);
				r.xMax = owner.state.TimeToPixel (owner.state.PixelToTime (r.xMax, true), true);
				r.yMin = Mathf.Floor (r.yMin / height) * height;
				r.yMax = (Mathf.Floor (r.yMax / height) + 1) * height;
				return r;
			}

			public Rect GetCurrentTimeRect ()
			{
				float height = AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;
				Rect r = EditorGUIExt.FromToRect (m_SelectStartPoint, m_SelectMousePoint);
				r.xMin = owner.state.PixelToTime (r.xMin, true);
				r.xMax = owner.state.PixelToTime (r.xMax, true);
				r.yMin = Mathf.Floor (r.yMin / height) * height;
				r.yMax = (Mathf.Floor (r.yMax / height) + 1) * height;
				return r;
			}
		}

		internal class DopeSheetPopup
		{
			Rect position;

			static int s_width = 96;
			static int s_height = (int)AnimationWindowHierarchyGUI.k_DopeSheetRowHeight + 96;

			Rect backgroundRect;

			public DopeSheetPopup (Rect position)
			{
				this.position = position;
			}

			public void OnGUI (AnimationWindowState state, AnimationWindowKeyframe keyframe)
			{
				if (keyframe.isPPtrCurve)
					return;

				backgroundRect = position;
				backgroundRect.x = state.TimeToPixel (keyframe.time) + position.x - s_width / 2;
				backgroundRect.y += AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;
				backgroundRect.width = s_width;
				backgroundRect.height = s_height;

				Rect objectRect = backgroundRect;
				objectRect.height = AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;

				Rect previewRect = backgroundRect;
				previewRect.y += AnimationWindowHierarchyGUI.k_DopeSheetRowHeight;
				previewRect.height = s_width;

				GUI.Box (backgroundRect, "");
				GUI.Box (previewRect, AssetPreview.GetAssetPreview ((Object)keyframe.value));

				EditorGUI.BeginChangeCheck ();
				Object value = EditorGUI.ObjectField (objectRect, (Object)keyframe.value, keyframe.curve.m_ValueType, false);

				if (EditorGUI.EndChangeCheck ())
				{
					keyframe.value = value;
					state.SaveCurve (keyframe.curve);
				}
			}
		}
	
		#endregion
	}
}
