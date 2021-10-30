using UnityEngine;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor
{
internal class PackageImport : EditorWindow
{
	[SerializeField]
	private AssetsItem[] m_assets;
	[SerializeField]
	private ListViewState m_ListView;
	[SerializeField]
	private int m_LeastIndent = 999999;
	[SerializeField]
	private bool m_PackageContainsPreviews;
	[SerializeField]
	private string m_PackageIconPath;


	private Texture2D m_Preview;
	private string m_LastPreviewPath;

	//TODO: move this out of here
	internal class Constants
	{
		public GUIStyle ConsoleEntryBackEven = "CN EntryBackEven";
		public GUIStyle ConsoleEntryBackOdd = "CN EntryBackOdd";
		public GUIStyle title = "OL Title";
		public Color lineColor;

		public Constants()
		{
			lineColor = EditorGUIUtility.isProSkin ? new Color(0.1f, 0.1f, 0.1f) : new Color(0.4f, 0.4f, 0.4f);
		}
	}
	static Constants ms_Constants; 
	
	public PackageImport ()
	{
		m_ListView = new ListViewState(0, 18);
        minSize = new Vector2 (460, 350);
        m_PackageContainsPreviews = false;
    }
	
	// invoked from menu
	public static void ShowImportPackage(AssetsItem[] items, string packageIconPath)
	{
		PackageImport window = EditorWindow.GetWindow<PackageImport>(true, "Importing package");
		window.m_assets = items;
		window.m_PackageIconPath=packageIconPath;
		if (packageIconPath != ""){
				window.m_PackageContainsPreviews=true;
		}
		else {
			
			foreach ( AssetsItem i in items ) 
			{
				if ( i.previewPath != "" )
				{
					window.m_PackageContainsPreviews=true;
					break;
				}
			}
		}
		window.Repaint ();
	}
	
	private void ShowPreview(string previewPath)
	{
		if ( previewPath != m_LastPreviewPath ) 
		{
			if ( ! m_Preview )
				m_Preview = new Texture2D(128,128);
			byte[] fileContents = null;
			
			try 
			{
				fileContents = System.IO.File.ReadAllBytes(previewPath);
			}
			catch 
			{
				// ignore
			}
			
			if (previewPath == "" || fileContents == null || ! m_Preview.LoadImage( fileContents )) 
			{
				Color[] pixels = m_Preview.GetPixels();
				for( int i = 0; i < pixels.Length ; ++i )
					pixels[i] = new Color(1.0f,1.0f,1.0f,0f);
				m_Preview.SetPixels(pixels);
				m_Preview.Apply();
			}
			m_LastPreviewPath = previewPath;
		}

		// TODO: Fallback to correct icons for type
		Texture2D tex = m_Preview ? m_Preview : EditorGUIUtility.FindTexture("DefaultAsset Icon");
		if (tex)
		{
			float width = tex.width;
			float height = tex.height;
			if (width >= height && width > 128)
			{
				height = height * 128 / width;
				width = 128;
			}
			else if (height > width && height > 128)
			{
				width = width * 128 / height;
				height = 128;
			}

			Rect r = GUILayoutUtility.GetRect(width, height, GUILayout.ExpandWidth(false));
			GUI.DrawTexture(r, tex, ScaleMode.ScaleToFit, true);
 		}

		FrameLastGUIRect();
	}

	public void OnGUI()
	{
		if (ms_Constants == null)
			ms_Constants = new Constants();
		if (m_assets == null)	
			return;
			
		if (m_LeastIndent == 999999)
		{
			int least = m_LeastIndent;

			for (int i = 0; i < m_assets.Length; i++)
			{
				int level = CountOccurencesOfChar(m_assets[i].pathName, '/');
				if (least > level)
					least = level;
			}

			m_LeastIndent = least - 1;
		}

        if (m_assets.Length > 0) {
            m_ListView.totalRows = m_assets.Length;

			if (m_PackageContainsPreviews)
			{
				GUILayout.BeginHorizontal();
				GUILayout.Space(2);
			}

			GUILayout.BeginVertical();
			GUILayout.Label("Items to Import", ms_Constants.title);
			GUILayout.Space(1);

            bool repainting = (Event.current.type == EventType.Repaint);
			
            foreach (ListViewElement el in ListViewGUI.ListView (m_ListView, GUIStyle.none)) 
			{
                AssetsItem ai = m_assets[el.row];
                Rect pos = el.position;
                pos = new Rect (pos.x + 1, pos.y, pos.width - 2, pos.height);

                // indent according to level
                int level = CountOccurencesOfChar(ai.pathName, '/') - m_LeastIndent;

                // background
				if (repainting && m_ListView.row == el.row)
                    ms_Constants.ConsoleEntryBackOdd.Draw(pos, false, false, true, false);

                int oldAiEnabled = ai.enabled;
				ai.enabled = GUI.Toggle(new Rect (pos.x + 3, pos.y, 16, 16), ai.enabled != 0, "")  ? 1 : 0;

                // enabled
                pos = new Rect (pos.x + 3, pos.y, pos.width - 3, pos.height);
				bool selected = GUI.Toggle(pos, false, "", GUIStyle.none) ;

                if (selected || oldAiEnabled != ai.enabled)
				{
					m_ListView.row = el.row;
					GUIUtility.keyboardControl = m_ListView.ID;
					CheckChildren(ai);
				}

                // icon
				Rect iconPos = new Rect(pos.x + (15 * level), pos.y + 1, 16, 16);
				Texture2D icon = GetIcon(ai) ?? EditorGUIUtility.FindTexture("DefaultAsset Icon");
				if (icon != null)
					GUI.DrawTexture(iconPos, icon);

                // Friendly name	
				pos = new Rect(pos.x + 20 + (15 * level), pos.y, pos.width - (20 + (15 * level)), pos.height);
                GUI.Label (pos, ai.message);
                
                if ( ai.exists == 0 ) 
                {
                	Texture badge = ASMainWindow.badgeNew.image;
                	GUI.DrawTexture(new Rect(el.position.width-badge.width-6, el.position.y + ( el.position.height - badge.height ) / 2  , badge.width, badge.height), badge);
                }
				
            }

			FrameLastGUIRect();
			GUILayout.EndVertical();
			
			if (m_PackageContainsPreviews)
			{
				GUILayout.Space(3);
				
				string previewPath=m_PackageIconPath;
				if ( m_ListView.row >=0 && m_ListView.row < m_assets.Length)
				{
					if ( m_assets[m_ListView.row].previewPath != "" ) 
						previewPath = m_assets[m_ListView.row].previewPath;
				}
				GUILayout.BeginVertical(GUILayout.Width(128));
				
				GUILayout.Label("Preview", ms_Constants.title);
				GUILayout.Space(1);
				ShowPreview(previewPath);
				
				GUILayout.EndVertical();
				GUILayout.Space(3);
				
				GUILayout.EndHorizontal();
			}

			if (m_ListView.row != -1 && GUIUtility.keyboardControl == m_ListView.ID && 
				Event.current.type == EventType.KeyDown && Event.current.keyCode == KeyCode.Space)
			{
				m_assets[m_ListView.row].enabled = m_assets[m_ListView.row].enabled == 0 ? 1 : 0;
				CheckChildren(m_assets[m_ListView.row]);
				Event.current.Use();
			}

			GUILayout.Space(5);
			GUILayout.BeginHorizontal();
			GUILayout.Space(10);
			if (GUILayout.Button(EditorGUIUtility.TextContent("All"), GUILayout.Width(50)))
			{
				for (int i = 0; i < m_assets.Length; i++)
				{
					m_assets[i].enabled = 1;
				}
			}

			if (GUILayout.Button(EditorGUIUtility.TextContent("None"), GUILayout.Width(50)))
			{
				for (int i = 0; i < m_assets.Length; i++)
				{
					m_assets[i].enabled = 0;
				}
			}

            GUILayout.FlexibleSpace ();
            if (GUILayout.Button (EditorGUIUtility.TextContent("Cancel"))) {
                Close ();
                GUIUtility.ExitGUI ();
            }
            if (GUILayout.Button (EditorGUIUtility.TextContent("Import"))) {
                if (m_assets != null)
                    AssetServer.ImportPackageStep2 (m_assets);
                Close ();
                GUIUtility.ExitGUI ();
            }
			GUILayout.Space(10);
            GUILayout.EndHorizontal ();
			GUILayout.Space(10);

        } else {
			GUILayout.Label ("Nothing to import!", EditorStyles.boldLabel);
            GUILayout.Label ("All assets from this package are already in your project.", "WordWrappedLabel");
            GUILayout.FlexibleSpace ();
            GUILayout.BeginHorizontal ();
            GUILayout.FlexibleSpace ();
            if (GUILayout.Button ("OK")) {
                Close ();
                GUIUtility.ExitGUI ();
            }
            GUILayout.EndHorizontal ();
        }
		
	}	

	private void OnDisable() 
	{
		if ( m_Preview != null )
			DestroyImmediate( m_Preview );
	}

	private Texture2D GetIcon(AssetsItem ai)
	{
		if (ai.assetIsDir != 0)
		{
			return EditorGUIUtility.FindTexture(EditorResourcesUtility.folderIconName);
		}


		return InternalEditorUtility
			.GetIconForFile(ai.pathName);
	}
	
	private void CheckChildren(AssetsItem parentAI)
	{
		foreach (AssetsItem ai in m_assets)
		{
			if (ai.parentGuid == parentAI.guid)	
			{
				ai.enabled = parentAI.enabled;
				CheckChildren(ai);	
			}
		}		
	}
	
	void FrameLastGUIRect()
	{
		var rect = GUILayoutUtility.GetLastRect();
		HandleUtility.handleWireMaterial.SetPass(0);
		GL.Begin(GL.LINES);
		GL.Color(ms_Constants.lineColor);
		GL.Vertex3(rect.xMax + 1, rect.y, 0);
		GL.Vertex3(rect.xMax + 1, rect.yMax, 0);
		GL.Vertex3(rect.xMax + 1, rect.yMax, 0);
		GL.Vertex3(rect.x + 1, rect.yMax, 0);
		GL.Vertex3(rect.x + 1, rect.yMax, 0);
		GL.Vertex3(rect.x + 1, rect.y, 0);
		GL.End();
	}
	
	private static int CountOccurencesOfChar(string instance, char c) {
		int result = 0;
		foreach (char curChar in instance) {
			if (c == curChar) {
				++result;
			}
		}
		return result;
	}
}
}
