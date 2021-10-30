using UnityEngine;
using UnityEditor;

namespace UnityEditor
{
	// Version for editors not derived from AssetImporterInspector
	internal abstract class TabbedEditor : Editor
	{
		protected System.Type[] m_SubEditorTypes = null;
		protected string[] m_SubEditorNames = null;
		private int m_ActiveEditorIndex = 0;
		private Editor m_ActiveEditor;
		public Editor activeEditor { get { return m_ActiveEditor; } }
		
		internal virtual void OnEnable ()
		{
			m_ActiveEditorIndex = EditorPrefs.GetInt (this.GetType ().Name + "ActiveEditorIndex", 0);
			if (m_ActiveEditor == null)
				m_ActiveEditor = Editor.CreateEditor (targets, m_SubEditorTypes[m_ActiveEditorIndex]);
		}
		
		void OnDestroy ()
		{
			DestroyImmediate (activeEditor);
		}
		
		public override void OnInspectorGUI ()
		{
			GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			EditorGUI.BeginChangeCheck ();
			m_ActiveEditorIndex = GUILayout.Toolbar (m_ActiveEditorIndex, m_SubEditorNames);
			if (EditorGUI.EndChangeCheck ())
			{
				EditorPrefs.SetInt (this.GetType ().Name + "ActiveEditorIndex", m_ActiveEditorIndex);
				DestroyImmediate (activeEditor);
				m_ActiveEditor = Editor.CreateEditor (targets, m_SubEditorTypes[m_ActiveEditorIndex]);
			}
			GUILayout.FlexibleSpace ();
			GUILayout.EndHorizontal ();
			
			activeEditor.OnInspectorGUI ();
		}
		
		public override void OnPreviewSettings ()
		{
			activeEditor.OnPreviewSettings();
		}
	
		public override void OnInteractivePreviewGUI (Rect r, GUIStyle background)
		{
			activeEditor.OnInteractivePreviewGUI (r, background);
		}
		
		public override bool HasPreviewGUI ()
		{
			return activeEditor.HasPreviewGUI ();
		}
	}
	
	// Version for AssetImporterInspector derived editors
	internal abstract class AssetImporterTabbedEditor : AssetImporterInspector
	{
		protected System.Type[] m_SubEditorTypes = null;
		protected string[] m_SubEditorNames = null;
		private int m_ActiveEditorIndex = 0;
		private AssetImporterInspector m_ActiveEditor;
		public AssetImporterInspector activeEditor { get { return m_ActiveEditor; } }
		
		internal virtual void OnEnable ()
		{
			m_ActiveEditorIndex = EditorPrefs.GetInt (this.GetType ().Name + "ActiveEditorIndex", 0);
			if (m_ActiveEditor == null)
				m_ActiveEditor = Editor.CreateEditor (targets, m_SubEditorTypes[m_ActiveEditorIndex]) as AssetImporterInspector;
		}
		
		void OnDestroy ()
		{
			DestroyImmediate (activeEditor);
		}
		
		public override void OnInspectorGUI ()
		{
			if (m_ActiveEditor.m_AssetEditor == null)
				m_ActiveEditor.m_AssetEditor = assetEditor;
			
			GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			EditorGUI.BeginChangeCheck ();
			m_ActiveEditorIndex = GUILayout.Toolbar (m_ActiveEditorIndex, m_SubEditorNames);
			if (EditorGUI.EndChangeCheck ())
			{
				EditorPrefs.SetInt (this.GetType ().Name + "ActiveEditorIndex", m_ActiveEditorIndex);
				DestroyImmediate (activeEditor);
				m_ActiveEditor = Editor.CreateEditor (targets, m_SubEditorTypes[m_ActiveEditorIndex]) as AssetImporterInspector;
				m_ActiveEditor.m_AssetEditor = assetEditor;
			}
			GUILayout.FlexibleSpace ();
			GUILayout.EndHorizontal ();
			
			activeEditor.OnInspectorGUI ();
		}
		
		public override void OnPreviewSettings ()
		{
			activeEditor.OnPreviewSettings();
		}
	
		public override void OnInteractivePreviewGUI (Rect r, GUIStyle background)
		{
			activeEditor.OnInteractivePreviewGUI (r, background);
		}
		
		public override bool HasPreviewGUI ()
		{
			return activeEditor.HasPreviewGUI ();
		}
	}
}
