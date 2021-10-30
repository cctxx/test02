using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using UnityEditor.Scripting;
using System.Collections.Generic;
using System.IO;

namespace UnityEditor
{

	internal class AssetSaveDialog : EditorWindow
	{
		class Styles
		{
			public GUIStyle selected = "ServerUpdateChangesetOn";
			public GUIStyle box = "OL Box";
			public GUIStyle button = "LargeButton";
			public GUIContent saveSelected = EditorGUIUtility.TextContent ("SaveAssetDialog.SaveSelected");
			public GUIContent saveAll = EditorGUIUtility.TextContent ("SaveAssetDialog.SaveAll");
			public GUIContent dontSave = EditorGUIUtility.TextContent ("SaveAssetDialog.DontSave");
			public GUIContent close = EditorGUIUtility.TextContent ("SaveAssetDialog.Close");
			public float buttonWidth;
			public Styles ()
			{
				buttonWidth = Mathf.Max (Mathf.Max (button.CalcSize (saveSelected).x, button.CalcSize (saveAll).x), button.CalcSize (dontSave).x);
			}
		}
	
		static Styles s_Styles = null;
		List<string> m_Assets;
		List<string> m_AssetsToSave;
		ListViewState m_LV = new ListViewState();
		int m_InitialSelectedItem = -1;
		bool []m_SelectedItems;
		List<GUIContent> m_Content;
		
		void SetAssets (string[] assets)
		{
			m_Assets = new List<string> (assets);
			RebuildLists (true);
			m_AssetsToSave = new List<string> ();
		}
	
		public static void ShowWindow(string[] inAssets, out string[] assetsThatShouldBeSaved)
		{
			int numMetaFiles = 0;
			foreach (string path in inAssets)
			{
				if (path.EndsWith("meta"))
					numMetaFiles++;
			}
			int numAssets = inAssets.Length - numMetaFiles;
			
			if (numAssets == 0)
			{
				assetsThatShouldBeSaved = inAssets;
				return;
			}
			
			string[] assets = new string [numAssets];
			string[] metaFiles = new string [numMetaFiles];
			
			numAssets = 0;
			numMetaFiles = 0;
			
			foreach (string path in inAssets)
			{
				if (path.EndsWith("meta"))
					metaFiles[numMetaFiles++] = path;
				else
					assets[numAssets++] = path;
			}
	
			AssetSaveDialog win = EditorWindow.GetWindowDontShow<AssetSaveDialog>();
			win.title = EditorGUIUtility.TextContent ("SaveAssetDialog.Title").text;
			win.SetAssets (assets);
			win.ShowUtility();
			win.ShowModal ();
	
			assetsThatShouldBeSaved = new string [win.m_AssetsToSave.Count + numMetaFiles];
			win.m_AssetsToSave.CopyTo (assetsThatShouldBeSaved, 0);
			metaFiles.CopyTo (assetsThatShouldBeSaved, win.m_AssetsToSave.Count);
		}

		public static GUIContent GetContentForAsset (string path)
		{
			Texture icon = AssetDatabase.GetCachedIcon (path);
			if (path.StartsWith("Library/"))
				path = ObjectNames.NicifyVariableName (AssetDatabase.LoadMainAssetAtPath (path).name);
	
			if (path.StartsWith("Assets/"))
				path = path.Substring (7);
	
			return new GUIContent(path, icon);		
		}
	
		void HandleKeyboard ()
		{
			/* TODO: Implement before we make it fullblown.
			 
			Event evt = Event.current;
			
			if (evt.type == EventType.keyDown)
			{
				Debug.Log (evt);
				if (Application.platform == RuntimePlatform.OSXEditor)
				{
					if ((evt.keyCode == KeyCode.S && evt.modifiers == EventModifiers.Command) || evt.character == '\n')
					{
						SaveSelectedAssets ();	
					} else if (evt.keyCode == KeyCode.D && evt.modifiers == EventModifiers.Command)
					{
						IgnoreSelectedAssets ();	
					} else if (evt.keyCode == KeyCode.Escape && evt.modifiers == (EventModifiers)0)
					{
						Debug.Log ("Cancelling");
						Cancel ();
					}
				} else {
					// TODO: Standard windows keybindings here.	
				}
			}
			*/
		}
		
		void OnGUI()
		{
			if (s_Styles == null)
			{
				s_Styles = new Styles();
				minSize = new Vector2(500, 300);
				position = new Rect(position.x, position.y, minSize.x, minSize.y);
			}
	
			HandleKeyboard ();
			GUILayout.Space(10);
			GUILayout.BeginHorizontal ();
			GUILayout.Space(10);
			GUILayout.Label("Unity is about to save the following modified files. Unsaved changes will be lost!");
			GUILayout.Space(10);
			GUILayout.EndHorizontal ();
			GUILayout.Space(10);
			
			GUILayout.BeginHorizontal();
			GUILayout.Space(10);
	
			int prevSelectedRow = m_LV.row;
			int numSelected = 0;
			foreach (ListViewElement el in ListViewGUILayout.ListView(m_LV, s_Styles.box))
			{
				if (m_SelectedItems[el.row] && Event.current.type == EventType.Repaint)
				{
					Rect box = el.position;
					box.x += 1;
					box.y += 1;
					box.width -= 1;
					box.height -= 1;
					s_Styles.selected.Draw (box, false, false, false, false);
				}
	
				GUILayout.Label(m_Content[el.row]);
	
				if (ListViewGUILayout.HasMouseUp(el.position))
				{
					Event.current.command = true;
					Event.current.control = true;
					ListViewGUILayout.MultiSelection(prevSelectedRow, el.row, ref m_InitialSelectedItem, ref m_SelectedItems);
				}
				if (m_SelectedItems[el.row])
					numSelected++;
			}
		
			GUILayout.Space(10);
			GUILayout.EndHorizontal();
			GUILayout.Space(10);
	
			GUILayout.BeginHorizontal();
			GUILayout.Space(10);
			
			if (GUILayout.Button (s_Styles.close, s_Styles.button, GUILayout.Width (s_Styles.buttonWidth)))
			{
				CloseWindow ();
			}
			
			GUILayout.FlexibleSpace();

			GUI.enabled = numSelected > 0;			
			
			bool allSelected = numSelected == m_Assets.Count;
						
			if (GUILayout.Button(s_Styles.dontSave, s_Styles.button, GUILayout.Width (s_Styles.buttonWidth)))
			{
				IgnoreSelectedAssets ();
			}

			if (GUILayout.Button (allSelected ? s_Styles.saveAll : s_Styles.saveSelected, 
								  s_Styles.button, GUILayout.Width (s_Styles.buttonWidth)))
			{
				SaveSelectedAssets ();
			}
			
			if (m_Assets.Count == 0)
				CloseWindow ();

			
			GUI.enabled = true;
			
			GUILayout.Space(10);
			GUILayout.EndHorizontal();
	
			GUILayout.Space(10);
		}	

		void Cancel ()
		{
			Close();
			GUIUtility.ExitGUI();
		}		
		
		void CloseWindow ()
		{
			Close();
			GUIUtility.ExitGUI();
		}
		
		void SaveSelectedAssets ()
		{
			List<string> newAssets = new List<string>();
			for (int i = 0; i < m_SelectedItems.Length; i++)
			{
				if (m_SelectedItems[i])
					m_AssetsToSave.Add (m_Assets[i]);	
				else
					newAssets.Add (m_Assets[i]);
			}
			
			m_Assets = newAssets;
			RebuildLists (false);
		}
		
		void IgnoreSelectedAssets ()
		{
			List<string> newAssets = new List<string>();
			for (int i = 0; i < m_SelectedItems.Length; i++)
			{
				if (!m_SelectedItems[i])
					newAssets.Add (m_Assets[i]);
			}
			
			m_Assets = newAssets;
			RebuildLists (false);
			
			if (m_Assets.Count == 0)
				CloseWindow ();
		}
		
		void RebuildLists (bool selected)
		{
			m_LV.totalRows = m_Assets.Count;
			m_SelectedItems = new bool [m_Assets.Count]; 
			m_Content = new List<GUIContent> (m_Assets.Count);
			for (int i=0; i<m_Assets.Count; i++)
			{
				m_SelectedItems[i] = selected;
				m_Content.Add (GetContentForAsset (m_Assets[i]));
			}
		}		
	}
}
