//#define PERF_PROFILE
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEditorInternal;
using UnityEditorInternal.VersionControl;
using Object = UnityEngine.Object;
using UnityEditor.VersionControl;

namespace UnityEditor
{
	internal class InspectorWindow : EditorWindow, IHasCustomMenu
	{
		public Vector2 m_ScrollPosition;
		public InspectorMode   m_InspectorMode = InspectorMode.Normal;
		
		static readonly List<InspectorWindow> m_AllInspectors = new List<InspectorWindow> ();

		const float kBottomToolbarHeight = 17f;
		internal const int kInspectorPaddingLeft = 4 + 10;
		internal const int kInspectorPaddingRight = 4;
		
		bool m_ResetKeyboardControl;
		protected ActiveEditorTracker m_Tracker;
		Editor m_LastInteractedEditor;
		bool m_IsOpenForEdit = false;
		
		private static Styles s_Styles;
		internal static Styles styles { get { return s_Styles ?? ( s_Styles = new Styles()); } }
		
		[SerializeField]
		PreviewResizer m_PreviewResizer = new PreviewResizer ();
		[SerializeField]
		PreviewWindow m_PreviewWindow;
		
		TypeSelectionList m_TypeSelectionList = null;

		private double m_lastRenderedTime;
		
		internal class Styles
		{
			public readonly GUIStyle preToolbar = "preToolbar";
			public readonly GUIStyle preToolbar2 = "preToolbar2";
			public readonly GUIStyle lockButton = "IN LockButton";
			public readonly GUIContent preTitle = EditorGUIUtility.TextContent("InspectorPreviewTitle");
			public readonly GUIContent labelTitle = EditorGUIUtility.TextContent ("InspectorLabelTitle");
			public GUIStyle preBackground = "preBackground";
			public GUIStyle addComponentArea = EditorStyles.inspectorTitlebar;
			public GUIStyle addComponentButtonStyle = "LargeButton";
			public GUIStyle previewMiniLabel = new GUIStyle (EditorStyles.whiteMiniLabel);
			public GUIStyle typeSelection = new GUIStyle ("PR Label");
			public GUIStyle lockedHeaderButton = "preButton";
			public GUIStyle stickyNote = new GUIStyle("VCS_StickyNote");
			public GUIStyle stickyNoteArrow = new GUIStyle("VCS_StickyNoteArrow");
			public GUIStyle stickyNotePerforce = new GUIStyle("VCS_StickyNoteP4");
			public GUIStyle stickyNoteLabel = new GUIStyle("VCS_StickyNoteLabel");
			
			public Styles ()
			{
				typeSelection.padding.left = 12;
			}
		}

		void Awake()
		{
			if (!m_AllInspectors.Contains(this))
				m_AllInspectors.Add(this);
		}
		
		void OnDestroy ()
		{
			if (m_PreviewWindow != null)
				m_PreviewWindow.Close ();
			if (m_Tracker != null && !m_Tracker.Equals (ActiveEditorTracker.sharedTracker))
				m_Tracker.Destroy();
		}

		protected virtual void OnEnable ()
		{
			title = m_InspectorMode == InspectorMode.Normal ? "UnityEditor.InspectorWindow" : "UnityEditor.DebugInspectorWindow";
			minSize = new Vector2(275, 50);
			
			if (!m_AllInspectors.Contains(this))
				m_AllInspectors.Add(this);
			
			m_PreviewResizer.Init ("InspectorPreview");
		}

		protected virtual void OnDisable ()
		{
			m_AllInspectors.Remove(this);
			LabelInspector.OnDisable();
		}

		void OnLostFocus()
		{
			EditorGUI.EndEditingActiveTextField();
			LabelInspector.OnLostFocus();
		}

		static internal void RepaintAllInspectors()
		{
			foreach (var win in m_AllInspectors)
				win.Repaint();
		}
		
		internal static List<InspectorWindow> GetInspectors ()
		{
			return m_AllInspectors;
		}

		void OnSelectionChange()
		{
			m_TypeSelectionList = null;
			m_Parent.ClearKeyboardControl();
			PropertyDrawer.s_PropertyDrawers.Clear ();
			Repaint ();
		}


        static public InspectorWindow[] GetAllInspectorWindows()
        {
            return m_AllInspectors.ToArray();
        }
		void OnInspectorUpdate()
		{
			if (m_Tracker != null)
			{
				// Check if scripts have changed without calling set dirty
				m_Tracker.VerifyModifiedMonoBehaviours();

				if (!m_Tracker.isDirty)
					return;
			}

			Repaint();
		}

		public virtual void AddItemsToMenu(GenericMenu menu)
		{
			menu.AddItem (new GUIContent ("Normal"), m_InspectorMode == InspectorMode.Normal, SetNormal);
			menu.AddItem (new GUIContent ("Debug"), m_InspectorMode == InspectorMode.Debug, SetDebug);
			if (Unsupported.IsDeveloperBuild())
				menu.AddItem (new GUIContent ("Debug-Internal"), m_InspectorMode == InspectorMode.DebugInternal, SetDebugInternal);

			menu.AddSeparator(string.Empty);
            menu.AddItem(new GUIContent("Lock"), m_Tracker != null && isLocked, FlipLocked);
		}

		void SetMode (InspectorMode mode)
		{
			if (mode == InspectorMode.Normal)
				title = "UnityEditor.InspectorWindow";
			else
				title = "UnityEditor.DebugInspectorWindow";
			m_InspectorMode = mode;
			CreateTracker ();
			m_Tracker.inspectorMode = mode;
			m_ResetKeyboardControl = true;
		}
		
		void SetDebug()
		{
			SetMode(InspectorMode.Debug);
		}
		
		void SetNormal()
		{
			SetMode(InspectorMode.Normal);
		}
		
		void SetDebugInternal()
		{
			SetMode(InspectorMode.DebugInternal);
		}
		void FlipLocked()
		{
            isLocked = !isLocked;
		}

        public bool isLocked
        {
          get
          {
              CreateTracker();
              return m_Tracker.isLocked;
          }
          set 
          {
              CreateTracker();
              m_Tracker.isLocked = value;
          }
        }

		static void DoInspectorDragAndDrop(Rect rect, Object[] targets)
		{
			if (!Dragging (rect))
				return;

			DragAndDrop.visualMode = InternalEditorUtility.InspectorWindowDrag (targets, Event.current.type == EventType.DragPerform);

			if (Event.current.type == EventType.DragPerform)
				DragAndDrop.AcceptDrag ();
		}

		private static bool Dragging (Rect rect)
		{
			return (Event.current.type == EventType.DragUpdated || Event.current.type == EventType.DragPerform) && rect.Contains (Event.current.mousePosition);
		}

		public ActiveEditorTracker GetTracker ()
		{
			CreateTracker ();
			return m_Tracker;
		}
		
		protected virtual void CreateTracker ()
		{
			if (m_Tracker != null)
				return;

			var sharedTracker = ActiveEditorTracker.sharedTracker;
			bool sharedTrackerInUse = m_AllInspectors.Any (i => i.m_Tracker != null && i.m_Tracker.Equals (sharedTracker));
			m_Tracker = sharedTrackerInUse ? new ActiveEditorTracker() : ActiveEditorTracker.sharedTracker;
			m_Tracker.inspectorMode = m_InspectorMode;
			m_Tracker.RebuildIfNecessary();
		}

		protected virtual void ShowButton (Rect r)
		{
			bool willLock = GUI.Toggle(r, isLocked, GUIContent.none, styles.lockButton);
            if (willLock != isLocked)
			{
                isLocked = willLock;
				m_Tracker.RebuildIfNecessary();
			}
		}

#if PERF_PROFILE
		float time;
#endif

		static public InspectorWindow s_CurrentInspectorWindow;
		

		protected virtual void OnGUI ()
		{
			Profiler.BeginSample("InspectorWindow.OnGUI");
			
			CreateTracker();

#if PERF_PROFILE
			if (Event.current.type == EventType.Layout) 
				time = Time.realtimeSinceStartup;
#endif

			ResetKeyboardControl ();
			m_ScrollPosition = EditorGUILayout.BeginVerticalScrollView (m_ScrollPosition);
			{
				bool isRepaintEvent = Event.current.type == EventType.repaint;
				
				if (isRepaintEvent)
					m_Tracker.ClearDirty();
				
				bool eyeDropperDirty = Event.current.type == EventType.ExecuteCommand && Event.current.commandName == "EyeDropperUpdate";
				s_CurrentInspectorWindow = this;
				Profiler.BeginSample("InspectorWindow.DrawEditors()");
				DrawEditors (isRepaintEvent, m_Tracker.activeEditors, eyeDropperDirty);
				Profiler.EndSample();
				s_CurrentInspectorWindow = null;
				EditorGUI.indentLevel = 0;
				
				AddComponentButton (m_Tracker.activeEditors);
				
				GUI.enabled = true;
				CheckDragAndDrop (m_Tracker.activeEditors);
				MoveFocusOnKeyPress();
			}
			GUILayout.EndScrollView ();
			
			if (m_Tracker.activeEditors.Length > 0)
			{
				Profiler.BeginSample("InspectorWindow.DrawPreviewAndLabels");
				DrawPreviewAndLabels ();
				Profiler.EndSample();
				DrawVCSShortInfo ();
			}
			
			

#if PERF_PROFILE
			if (Event.current.type == EventType.Repaint)
			{
				time = Time.realtimeSinceStartup - time;
				GUI.Box (new Rect (position.width - 200, position.height - 20, 200, 20), "" + Mathf.Round (time * 1000000.0F));
			}
#endif
			Profiler.EndSample();
			
		}

		public virtual Editor GetLastInteractedEditor()
		{
			return m_LastInteractedEditor;
		}

		public Editor GetEditorThatControlsPreview (Editor[] editors)
		{
			if (editors.Length == 0)
				return null;

			// Find last interacted editor, if not found check if we had an editor of similar type,
			// if not found use first editor that can show a preview otherwise return null.

			bool isPreviewWindow = this is PreviewWindow;
			Editor lastInteractedEditor = GetLastInteractedEditor ();
			System.Type lastType = lastInteractedEditor ? lastInteractedEditor.GetType () : null;

			Editor firstEditorThatHasPreview = null;
			Editor similarEditorAsLast = null;
			foreach (Editor e in editors)
			{
				if (e.target == null)
					continue;
				
				// Don't make the importer have the preview
//				if (e is AssetImporterInspector)
//					continue;
				
				// If target is an asset, but not the same asset as the asset
				// of the first editor, then ignore it. This will prevent showing
				// preview of materials attached to a GameObject but should cover
				// future use cases as well.
				if (EditorUtility.IsPersistent (e.target) &&
					AssetDatabase.GetAssetPath (e.target) != AssetDatabase.GetAssetPath (editors[0].target))
					continue;
				
				bool canPreview = isPreviewWindow ? true : CanPreview (e.target);

				if (e.HasPreviewGUI() && canPreview)
				{
					if (e == lastInteractedEditor)
						return e;
					
					if (similarEditorAsLast == null && e.GetType () == lastType)
						similarEditorAsLast = e;

					if (firstEditorThatHasPreview == null)
						firstEditorThatHasPreview = e;
				}
			}

			if (similarEditorAsLast != null)
				return similarEditorAsLast;

			if (firstEditorThatHasPreview != null)
				return firstEditorThatHasPreview;

			// Found no valid editor
			return null;
		}
		
		public Object GetInspectedObject ()
		{
			if (m_Tracker == null)
				return null;
			Editor editor = GetFirstNonImportInspectorEditor (m_Tracker.activeEditors);
			if (editor == null)
				return null;
			return editor.target;
		}
		
		Editor GetFirstNonImportInspectorEditor (Editor[] editors)
		{
			foreach (Editor e in editors)
			{
				// Check for target rather than the editor type itself,
				// because some importers use default inspector
				if (e.target is AssetImporter)
					continue;
				return e;
			}
			return null;
		}

		private void MoveFocusOnKeyPress ()
		{
			var key = Event.current.keyCode;

			if (Event.current.type != EventType.keyDown || (key != KeyCode.DownArrow && key != KeyCode.UpArrow && key != KeyCode.Tab))
				return;

			if (key != KeyCode.Tab)
				EditorGUIUtility.MoveFocusAndScroll(key == KeyCode.DownArrow);
			else
				EditorGUIUtility.ScrollForTabbing(!Event.current.shift);
			
			Event.current.Use();
		}

		private void ResetKeyboardControl ()
		{
			if (m_ResetKeyboardControl)
			{
				GUIUtility.keyboardControl = 0;
				m_ResetKeyboardControl = false;
			}
		}

		private void CheckDragAndDrop (Editor[] editors)
		{
			Rect remainingRect = GUILayoutUtility.GetRect(GUIContent.none, GUIStyle.none, GUILayout.ExpandHeight(true));

			if (remainingRect.Contains(Event.current.mousePosition))
			{
				Editor editor = GetFirstNonImportInspectorEditor (editors);
				if (editor != null)
					DoInspectorDragAndDrop(remainingRect, editor.targets);

				if (Event.current.type == EventType.MouseDown)
				{
					GUIUtility.keyboardControl = 0;
					Event.current.Use();
				}
			}
		}

		private void DrawPreviewAndLabels ()
		{
			if (m_PreviewWindow && Event.current.type == EventType.Repaint)
				m_PreviewWindow.Repaint ();
			
			Editor previewEditor = GetEditorThatControlsPreview (m_Tracker.activeEditors);
			Editor assetEditor = GetFirstNonImportInspectorEditor (m_Tracker.activeEditors);
			
			// Do we have a preview?
			bool hasPreview = previewEditor != null &&
				CanPreview (previewEditor.target) &&
				previewEditor.HasPreviewGUI () &&
				(m_PreviewWindow == null);
			
			// Do we have labels?
			bool hasLabels = false;
			// @TODO: Implement multi-object editing of labels?
			if (assetEditor != null && assetEditor.targets.Length == 1)
			{
				string assetPathForLabels = AssetDatabase.GetAssetPath (assetEditor.target);
				hasLabels = assetPathForLabels.ToLower ().StartsWith ("assets") && !Directory.Exists (assetPathForLabels);
			}
			
			if (!hasPreview && !hasLabels)
				return;
			
			Event evt = Event.current;

			// Preview / Asset Labels toolbar
			Rect rect = EditorGUILayout.BeginHorizontal (GUIContent.none, styles.preToolbar, GUILayout.Height (kBottomToolbarHeight));
			Rect labelRect;
			{
				GUILayout.FlexibleSpace ();
				labelRect = GUILayoutUtility.GetLastRect ();
				// The label rect is also needed to know which area should be draggable.
				GUIContent title;
				if (hasPreview)
				{
					GUIContent userDefinedTitle = previewEditor.GetPreviewTitle();
					title = userDefinedTitle ?? styles.preTitle;
				}
				else
				{
					title = styles.labelTitle;
				}
				GUI.Label(labelRect, title, styles.preToolbar2);

				if (hasPreview && m_PreviewResizer.GetExpandedBeforeDragging ())
					previewEditor.OnPreviewSettings();
			} EditorGUILayout.EndHorizontal ();
			


			// Detach preview on right click in preview title bar
			if (evt.type == EventType.MouseUp && evt.button == 1 && rect.Contains(evt.mousePosition) && m_PreviewWindow == null)
				DetachPreview();
			
			// Logic for resizing and collapsing
			float previewSize;
			if (hasPreview) 
			{
				// If we have a preview we'll use the ResizerControl which handles both resizing and collapsing
				
				// We have to subtract space used by asset server info from the window size we pass to the PreviewResizer
				Rect windowRect = position;
				if (EditorSettings.externalVersionControl != ExternalVersionControl.Disabled && 
					EditorSettings.externalVersionControl != ExternalVersionControl.AutoDetect &&
					EditorSettings.externalVersionControl != ExternalVersionControl.Generic
					)
				{
					windowRect.height -= kBottomToolbarHeight;
				}
				previewSize = m_PreviewResizer.ResizeHandle (windowRect, 100, 100, kBottomToolbarHeight, labelRect);
				
			}
			else
			{
				// If we don't have a preview, just toggle the collapseble state with a button
				if (GUI.Button (rect, GUIContent.none, GUIStyle.none))
					m_PreviewResizer.ToggleExpanded();
				
				previewSize = 0;
			}
			
			// If collapsed, early out
			if (!m_PreviewResizer.GetExpanded ())
				return;
			
			// The preview / label area (not including the toolbar)
			GUILayout.BeginVertical (styles.preBackground, GUILayout.Height (previewSize));
			{
				// Draw preview
				if (hasPreview)
					previewEditor.DrawPreview (GUILayoutUtility.GetRect (0, 10240, 64, 10240));
				
				if (hasLabels)
					LabelInspector.OnLabelGUI (assetEditor.target);
			} GUILayout.EndVertical ();
		}
		
		private void DetachPreview ()
		{
			Event.current.Use ();
			m_PreviewWindow = ScriptableObject.CreateInstance (typeof (PreviewWindow)) as PreviewWindow;
			m_PreviewWindow.SetParentInspector (this);
			m_PreviewWindow.Show ();
			Repaint ();
			GUIUtility.ExitGUI ();
		}

		private bool CanPreview (Object obj)
		{
			if (obj == null)
				return false;
			
			// Only let GameObject have preview if it's an asset
			if (obj is GameObject)
				return EditorUtility.IsPersistent (obj);
			
			// For other objects (including components), always allow preview
			return true;
		}

		protected virtual void DrawVCSSticky(float offset)
		{
			string message = "";
			Editor assetEditor = GetFirstNonImportInspectorEditor (m_Tracker.activeEditors);
			bool hasRemovedSticky = EditorPrefs.GetBool("vcssticky");
			if (!hasRemovedSticky && !AssetDatabase.IsOpenForEdit(assetEditor.target, out message))
			{
				var rect = new Rect (10, position.height - 94, position.width - 20, 80);
				rect.y -= offset;
				if (Event.current.type == EventType.repaint)
				{
					styles.stickyNote.Draw(rect, false, false, false, false);
					
					Rect iconRect = new Rect(rect.x, rect.y + rect.height/2 - 32, 64, 64);
					if (EditorSettings.externalVersionControl == "Perforce") // TODO: remove hardcoding of perforce
					{
						styles.stickyNotePerforce.Draw (iconRect, false, false, false, false);
					}
					
					Rect textRect = new Rect(rect.x + iconRect.width, rect.y, rect.width - iconRect.width, rect.height);
					GUI.Label (textRect, new GUIContent("<b>Under Version Control</b>\nCheck out this asset in order to make changes."), styles.stickyNoteLabel);
					
					Rect arrowRect = new Rect(rect.x + rect.width/2, rect.y + 80, 19, 14);
					styles.stickyNoteArrow.Draw(arrowRect, false, false, false, false);
				}
			}
		}
		
		private void DrawVCSShortInfo ()
		{
			if (EditorSettings.externalVersionControl == ExternalVersionControl.AssetServer)
			{
				EditorGUILayout.BeginHorizontal(GUIContent.none, styles.preToolbar, GUILayout.Height(kBottomToolbarHeight));
				Object target = GetFirstNonImportInspectorEditor (m_Tracker.activeEditors).target;
				int id = GUIUtility.GetControlID(FocusType.Passive);
				GUILayout.FlexibleSpace();
				Rect labelRect = GUILayoutUtility.GetLastRect ();
				EditorGUILayout.EndHorizontal();
				AssetInspector.Get().OnAssetStatusGUI(labelRect, id, target, styles.preToolbar2);
			}

			if (Provider.isActive &&
				EditorSettings.externalVersionControl != ExternalVersionControl.Disabled &&
				EditorSettings.externalVersionControl != ExternalVersionControl.AutoDetect &&
			    EditorSettings.externalVersionControl != ExternalVersionControl.Generic)
			{
				Editor assetEditor = GetFirstNonImportInspectorEditor (m_Tracker.activeEditors);
				string assetPath = AssetDatabase.GetAssetPath (assetEditor.target);
				Asset asset = Provider.GetAssetByPath(assetPath);
				if (asset == null || !(asset.path.StartsWith("Assets") || asset.path.StartsWith("ProjectSettings")))
					return;
				
				GUILayout.Label(GUIContent.none, styles.preToolbar, GUILayout.Height(kBottomToolbarHeight));

				var rect = GUILayoutUtility.GetLastRect();

				string currentState = asset.StateToString();

				if (currentState != "" && 
					( Event.current.type == EventType.layout || Event.current.type == EventType.repaint))
				{
					
					Texture2D icon = AssetDatabase.GetCachedIcon(assetPath) as Texture2D;
					Rect overlayRect = new Rect(rect.x, rect.y, 28, 16);
					Rect iconRect = overlayRect;
					iconRect.x += 6;
					iconRect.width = 16;
					if (icon != null)
						GUI.DrawTexture(iconRect, icon);
					Overlay.DrawOverlay(asset, overlayRect);

					Rect textRect = new Rect (rect.x + 26, rect.y, rect.width - 31, rect.height);
					GUIContent content = GUIContent.Temp(currentState);
					EditorGUI.LabelField (textRect, content, styles.preToolbar2);
				}

				string message = "";
				bool openForEdit = AssetDatabase.IsOpenForEdit(assetEditor.target, out message);
				if (!openForEdit)
				{
					float buttonWidth = 66;
					Rect buttonRect = new Rect (rect.x + rect.width - buttonWidth, rect.y, buttonWidth, rect.height);
					if (GUI.Button (buttonRect, "Checkout", styles.lockedHeaderButton))
					{
						EditorPrefs.SetBool ("vcssticky", true);
						Task task = Provider.Checkout (assetEditor.targets, CheckoutMode.Both);
						task.SetCompletionAction(CompletionAction.UpdatePendingWindow);
						task.Wait();
						Repaint ();
					}
					DrawVCSSticky(rect.height / 2);
				}
				
			}
		}

		private void DrawEditors(bool isRepaintEvent, Editor[] editors, bool eyeDropperDirty)
		{
			// We need to force optimized GUI to dirty when object becomes open for edit 
			// e.g. after checkout in version control. If this is not done the optimized
			// GUI will need an extra repaint before it gets ungrayed out.

			Object inspectedObject = GetInspectedObject();
			string msg = String.Empty;
			bool forceDirty = false;
			if (isRepaintEvent && inspectedObject != null && m_IsOpenForEdit != AssetDatabase.IsOpenForEdit(inspectedObject, out msg))
			{
				m_IsOpenForEdit = !m_IsOpenForEdit;
				forceDirty = true;
			}

			Editor.m_AllowMultiObjectAccess = true;
			bool showImportedObjectBarNext = false;
			Rect importedObjectBarRect = new Rect ();
			bool firstVisible = true;
			for (int editorIndex = 0; editorIndex < editors.Length; editorIndex++)
			{
				Editor editor = editors[editorIndex];

				if (editor.hideInspector)
					continue;

				Object target = editor.target;
				GUIUtility.GetControlID (target.GetInstanceID (), FocusType.Passive);
				EditorGUIUtility.ResetGUIState ();

				// cache the layout group we expect to have at the end of drawing this editor
				GUILayoutGroup expectedGroup = GUILayoutUtility.current.topLevel;

				// Display title bar and early out if folded
				int wasVisibleState = m_Tracker.GetVisible (editorIndex);
				bool wasVisible;

				if (wasVisibleState == -1)
				{
					wasVisible = InternalEditorUtility.GetIsInspectorExpanded (target);
					m_Tracker.SetVisible (editorIndex, wasVisible ? 1 : 0);
				}
				else
					wasVisible = wasVisibleState == 1;

				// Reset dirtyness when repainting
				bool wasDirty = forceDirty || editor.isInspectorDirty || eyeDropperDirty;

				if (isRepaintEvent)
					editor.isInspectorDirty = false;
				
				if (ShouldCullEditor (editors, editorIndex))
					continue;
				
				// Assign asset editor to importer editor
				if (editor is AssetImporterInspector && editors.Length > 1)
					(editor as AssetImporterInspector).m_AssetEditor = editors[1];
				
				bool largeHeader = AssetDatabase.IsMainAsset (target) || AssetDatabase.IsSubAsset (target) || editorIndex == 0 || target is Material;
				
				// First element controls color of tabs of dockarea
				if (firstVisible)
				{
					DockArea dockArea = m_Parent as DockArea;
					if (dockArea != null)
						dockArea.tabStyle = "dragtabbright";
					GUILayout.Space (0);
					firstVisible = false;
				}
				
				// Draw large headers before we do the culling of unsupported editors below,
				// so the large header is always shown even when the editor can't be.
				if (largeHeader)
				{
					String message = String.Empty;
					bool IsOpenForEdit = editor.IsOpenForEdit (out message);

					if (showImportedObjectBarNext)
					{
						showImportedObjectBarNext = false;
						GUILayout.Space (15);
						importedObjectBarRect = GUILayoutUtility.GetRect(16, 16);
						importedObjectBarRect.height = 17;
					}
					
					wasVisible = true;
					
					// Header
					EditorGUI.BeginDisabledGroup (!IsOpenForEdit); // Only disable the entire header if the asset is locked by VCS
					editor.DrawHeader ();
					EditorGUI.EndDisabledGroup ();
				}
				
				if (editor.target is AssetImporter)
					showImportedObjectBarNext = true;
				
				// Culling of editors that can't be properly shown.
				// If the current editor is a GenericInspector even though a custom editor for it exists,
				// then it's either a fallback for a custom editor that doesn't support multi-object editing,
				// or we're in debug mode.
				bool multiEditingNotSupported = false;
				if (editor is GenericInspector && CustomEditorAttributes.FindCustomEditorType (target, false) != null)
				{
					if (m_InspectorMode == InspectorMode.DebugInternal)
					{
						// Do nothing
					}
					else if (m_InspectorMode == InspectorMode.Normal)
					{
						// If we're not in debug mode and it thus must be a fallback,
						// hide the editor and show a notification.
						multiEditingNotSupported = true;
					}
					else if (target is AssetImporter)
					{
						// If we are in debug mode and it's an importer type,
						// hide the editor and show a notification.
						multiEditingNotSupported = true;
					}
					// If we are in debug mode and it's an NOT importer type,
					// just show the debug inspector as usual.
				}
				
				// Draw small headers (the header above each component) after the culling above
				// so we don't draw a component header for all the components that can't be shown.
				if (!largeHeader)
				{
					EditorGUI.BeginDisabledGroup (!editor.IsEnabled ());
					
					bool isVisible = UnityEditor.EditorGUILayout.InspectorTitlebar (wasVisible, editor.targets);
					if (wasVisible != isVisible)
					{
						m_Tracker.SetVisible (editorIndex, isVisible ? 1 : 0);
						InternalEditorUtility.SetIsInspectorExpanded (target, isVisible);
						if (isVisible)
							m_LastInteractedEditor = editor;
						else if (m_LastInteractedEditor == editor)
							m_LastInteractedEditor = null;
							
					}
					EditorGUI.EndDisabledGroup ();
				}
				
				if (multiEditingNotSupported && wasVisible)
				{
					GUILayout.Label ("Multi-object editing not supported.", EditorStyles.helpBox);
					continue;
				}
				
				// We need to reset again after drawing the header.
				EditorGUIUtility.ResetGUIState ();
				
				EditorGUI.BeginDisabledGroup (!editor.IsEnabled ());
				
				var genericEditor = editor as GenericInspector;
				if (genericEditor)
					genericEditor.m_InspectorMode = m_InspectorMode;

				// Optimize block code path
				float height;
				OptimizedGUIBlock optimizedBlock;
				
				EditorGUIUtility.hierarchyMode = true;
				EditorGUIUtility.wideMode = position.width > 330;
				
				if (editor.GetOptimizedGUIBlock (wasDirty, wasVisible, out optimizedBlock, out height))
				{
					Rect contentRect = GUILayoutUtility.GetRect (0, wasVisible ? height : 0);
					HandleLastInteractedEditor (contentRect, editor);

					// Layout events are ignored in the optimized code path
					if (Event.current.type == EventType.Layout)
						continue;

					// Try reusing optimized block
					if (optimizedBlock.Begin (wasDirty, contentRect))
					{
						// Draw content
						if (wasVisible)
						{
							GUI.changed = false;
							editor.OnOptimizedInspectorGUI (contentRect);
						}
					}

					optimizedBlock.End ();
				}
				else
				{
					// Render contents if folded out
					if (wasVisible)
					{
						GUIStyle editorWrapper = (editor.UseDefaultMargins () ? EditorStyles.inspectorDefaultMargins : GUIStyle.none);
						Rect contentRect = EditorGUILayout.BeginVertical (editorWrapper);
						{
							HandleLastInteractedEditor (contentRect, editor);

							GUI.changed = false;

							try
							{
								editor.OnInspectorGUI ();
							}
							catch (Exception e)
							{
								// Don't want to catch ExitGUI
								if (e is ExitGUIException)
								{
									throw;
								}

								Debug.LogException(e);
							}
						}
						EditorGUILayout.EndVertical ();
					}

					// early out if an event has been used
					if (Event.current.type == EventType.Used)
						return;
				}

				EditorGUI.EndDisabledGroup ();

				// Check and try to cleanup layout groups.
				if (GUILayoutUtility.current.topLevel != expectedGroup)
				{
					if (!GUILayoutUtility.current.layoutGroups.Contains (expectedGroup))
					{
						// We can't recover from this, so we error.
						Debug.LogError ("Expected top level layout group missing! Too many GUILayout.EndScrollView/EndVertical/EndHorizontal?");
						GUIUtility.ExitGUI();
					}
					else
					{
						// We can recover from this, so we warning.
						Debug.LogWarning ("Unexpected top level layout group! Missing GUILayout.EndScrollView/EndVertical/EndHorizontal?");

						while (GUILayoutUtility.current.topLevel != expectedGroup)
							GUILayoutUtility.EndLayoutGroup ();
					}
				}
			}
			
			EditorGUIUtility.ResetGUIState ();
			
			// Draw the bar to show that the imported object is below
			if (importedObjectBarRect.height > 0)
			{
				// Clip the label to avoid a black border at the bottom
				GUI.BeginGroup (importedObjectBarRect);
				GUI.Label (new Rect(0, 0, importedObjectBarRect.width, importedObjectBarRect.height), "Imported Object", "OL Title");
				GUI.EndGroup ();
			}

			if (m_Tracker.hasComponentsWhichCannotBeMultiEdited)
			{
				if (editors.Length == 0 && !m_Tracker.isLocked && Selection.objects.Length > 0)
				{
					DrawSelectionPickerList ();
				}
				else
				{
					// Visually separates the Add Component button from the existing components
					Rect lineRect = GUILayoutUtility.GetRect (10, 4, EditorStyles.inspectorTitlebar);
					if (Event.current.type == EventType.Repaint)
						DrawSplitLine (lineRect.y);
					
					GUILayout.Label (
						"Components that are only on some of the selected objects cannot be multi-edited.",
						EditorStyles.helpBox);
					
					GUILayout.Space (4);
				}
				}
			}

		private bool ShouldCullEditor (Editor[] editors, int editorIndex)
		{
			Object currentTarget = editors[editorIndex].target;

			// Objects that should always be hidden
			if (currentTarget is SubstanceImporter || currentTarget is ParticleSystemRenderer)
				return true;

			// Hide regular AssetImporters (but not inherited types)
			if (currentTarget.GetType () == typeof (AssetImporter))
				return true;

			// Let asset importers decide if the imported object should be shown or not
			if (m_InspectorMode == InspectorMode.Normal && editorIndex != 0)
			{
				AssetImporterInspector importerEditor = editors[0] as AssetImporterInspector;
				if (importerEditor != null && !importerEditor.showImportedObject)
					return true;
			}

			return false;
		}
		
		void DrawSelectionPickerList ()
		{
			if (m_TypeSelectionList == null)
				m_TypeSelectionList = new TypeSelectionList (Selection.objects);
			
			DockArea dockArea = m_Parent as DockArea;
			if (dockArea != null)
				dockArea.tabStyle = "dragtabbright";
			GUILayout.Space (0);
			
			Editor.DrawHeaderGUI (null, Selection.objects.Length + " Objects");
			
			GUILayout.Label ("Narrow the Selection:", EditorStyles.label);
			GUILayout.Space (4);
			
			Vector2 oldSize = EditorGUIUtility.GetIconSize ();
			EditorGUIUtility.SetIconSize (new Vector2 (16, 16));
			foreach (TypeSelection ts in m_TypeSelectionList.typeSelections)
			{
				Rect r = GUILayoutUtility.GetRect (16, 16, GUILayout.ExpandWidth (true));
				if (GUI.Button (r, ts.label, styles.typeSelection))
				{
					Selection.objects = ts.objects;
					Event.current.Use ();
				}
				if (GUIUtility.hotControl == 0)
					EditorGUIUtility.AddCursorRect (r, MouseCursor.Link);
				GUILayout.Space (4);
			}
			EditorGUIUtility.SetIconSize (oldSize);
		}
		
		void HandleLastInteractedEditor (Rect componentRect, Editor editor)
		{
			if (editor != m_LastInteractedEditor &&
				Event.current.type == EventType.MouseDown && componentRect.Contains (Event.current.mousePosition))
			{
				// Don't use the event because the editor might want to use it.
				// But don't move the check down below the editor either,
				// because we want to set the last interacted editor simultaneously.
				m_LastInteractedEditor = editor;
				Repaint ();
			}
		}
		
		private void AddComponentButton (Editor[] editors)
		{
			Editor editor = GetFirstNonImportInspectorEditor (editors);
			if (editor != null && editor.target != null && editor.target is GameObject && editor.IsEnabled ())
			{
				EditorGUILayout.BeginHorizontal();
				const int width = 230;
				var content = new GUIContent("Add Component");
				Rect rect = GUILayoutUtility.GetRect(content, styles.addComponentButtonStyle, null);
				rect.y += 10;
				rect.x += (rect.width - width) / 2;
				rect.width = width;
				
				// Visually separates the Add Component button from the existing components
				if (Event.current.type == EventType.Repaint)
					DrawSplitLine (rect.y - 11);

				Event evt = Event.current;
				bool openWindow = false;
				switch (evt.type)
				{
					case EventType.ExecuteCommand:
						string commandName = evt.commandName;
						if (commandName == "OpenAddComponentDropdown")
						{
							openWindow = true;
							evt.Use();
						}
						break;
				}

				if (EditorGUI.ButtonMouseDown (rect, content, FocusType.Passive, styles.addComponentButtonStyle) || openWindow)
				{
					if (AddComponentWindow.Show (rect, editor.targets.Select (o => (GameObject)o).ToArray ()))
						GUIUtility.ExitGUI ();
				}
				EditorGUILayout.EndHorizontal();
			}
		}

		private void DrawSplitLine(float y)
		{
			Rect position = new Rect(0, y, this.m_Pos.width + 1, 1);
			Rect uv = new Rect(0, 1f, 1, 1f - 1f/EditorStyles.inspectorTitlebar.normal.background.height);
			GUI.DrawTextureWithTexCoords(position, EditorStyles.inspectorTitlebar.normal.background, uv);
		}
		// Invoked from C++
		internal static void ShowWindow()
		{
			GetWindow(typeof(InspectorWindow));
 		}

		private void Update()
		{
			if (m_Tracker == null || m_Tracker.activeEditors == null)
				return;

			bool wantsRepaint = false;
			foreach (var myEditor in m_Tracker.activeEditors)
			{
				if (myEditor.RequiresConstantRepaint() && !myEditor.hideInspector)
					wantsRepaint = true;
			}

			if (wantsRepaint && m_lastRenderedTime + 0.033f < EditorApplication.timeSinceStartup)
			{
				m_lastRenderedTime = EditorApplication.timeSinceStartup;
				Repaint();
			}
		}
	}
}
