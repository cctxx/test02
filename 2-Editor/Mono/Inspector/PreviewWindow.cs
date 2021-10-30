using UnityEngine;

namespace UnityEditor
{
	
	internal class PreviewWindow : InspectorWindow
	{
		[SerializeField]
		private InspectorWindow m_ParentInspectorWindow;
		
		public void SetParentInspector (InspectorWindow inspector)
		{
			m_ParentInspectorWindow = inspector;
		}
		
		protected override void OnEnable ()
		{
			base.OnEnable ();
			title = "Preview";
			minSize = new Vector2(260, 220);
		}
		
		protected override void OnDisable ()
		{
			base.OnDisable ();
			m_ParentInspectorWindow.Repaint ();
		}
		
		protected override void CreateTracker ()
		{
			m_Tracker = m_ParentInspectorWindow.GetTracker ();
		}
		
		public override Editor GetLastInteractedEditor ()
		{
			return m_ParentInspectorWindow.GetLastInteractedEditor ();
		}

		protected override void OnGUI ()
		{
			if (!m_ParentInspectorWindow)
			{
				Close ();
				EditorGUIUtility.ExitGUI ();
			}
			Editor.m_AllowMultiObjectAccess = true;
			
			CreateTracker();
			
			// Do we have an editor that supports previews? Null if not.
			Editor editor = GetEditorThatControlsPreview (m_Tracker.activeEditors);
			
			bool hasPreview = editor && editor.HasPreviewGUI();

			// Toolbar
			Rect labelRect;
			Rect toolbarRect = EditorGUILayout.BeginHorizontal (GUIContent.none, styles.preToolbar, GUILayout.Height (17));
			{
				GUILayout.FlexibleSpace ();
				labelRect = GUILayoutUtility.GetLastRect ();
				// Label
				string label = string.Empty;
				if (editor)
				{
					if (editor.target is ParticleSystem)
						label = editor.GetPreviewTitle().text;
					else
						label = editor.targetTitle;
				}
				GUI.Label (labelRect, label, styles.preToolbar2);
				if (hasPreview)
					editor.OnPreviewSettings();
			} EditorGUILayout.EndHorizontal ();
			


			Event evt = Event.current;
			if (evt.type == EventType.MouseUp && evt.button == 1 && toolbarRect.Contains(evt.mousePosition))
			{
				Close ();
				evt.Use ();
				// Don't draw preview if we just closed this window
				return;
			}
			
			// Preview
			Rect previewPosition = GUILayoutUtility.GetRect(0, 10240, 64, 10240);
			
			// Draw background
			if (Event.current.type == EventType.Repaint)
				styles.preBackground.Draw(previewPosition, false, false, false, false);
			
			// Draw preview
			if (editor && editor.HasPreviewGUI ())
				editor.DrawPreview (previewPosition);
		}
		
		public override void AddItemsToMenu (GenericMenu menu) { }
		
		protected override void ShowButton (Rect r) { }
	}
}
