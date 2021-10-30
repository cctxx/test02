using UnityEngine;
using UnityEditor.VersionControl;
using UnityEditor.Modules;
using UnityEditorInternal;
using System.Collections.Generic;
using System;
using System.IO;
using System.Reflection;
using NUnit.Framework;

namespace UnityEditor
{

internal class PreferencesWindow : EditorWindow
{
	internal class Constants
	{
		public GUIStyle sectionScrollView = "PreferencesSectionBox";
		public GUIStyle settingsBoxTitle = "OL Title";
		public GUIStyle settingsBox = "OL Box";
		public GUIStyle errorLabel = "WordWrappedLabel";
		public GUIStyle sectionElement = "PreferencesSection";
		public GUIStyle evenRow = "CN EntryBackEven";
		public GUIStyle oddRow = "CN EntryBackOdd";
		public GUIStyle selected = "ServerUpdateChangesetOn";
		public GUIStyle keysElement = "PreferencesKeysElement";
		public GUIStyle sectionHeader = new GUIStyle (EditorStyles.largeLabel);
		
		public Constants ()
		{
			sectionScrollView = new GUIStyle (sectionScrollView);
			sectionScrollView.overflow.bottom += 1;
			
			sectionHeader.fontStyle = FontStyle.Bold;
			sectionHeader.fontSize = 18;
			sectionHeader.margin.top = 10;
			sectionHeader.margin.left += 1;
			if (!EditorGUIUtility.isProSkin)
				sectionHeader.normal.textColor = new Color (0.4f, 0.4f, 0.4f, 1.0f);
			else
				sectionHeader.normal.textColor = new Color (0.7f, 0.7f, 0.7f, 1.0f);
		}
	}
	
	private delegate void OnGUIDelegate ();
	private class Section
	{
		public GUIContent content;
		public OnGUIDelegate guiFunc;
		
		public Section (string name, OnGUIDelegate guiFunc)
		{
			this.content = new GUIContent (name);
			this.guiFunc = guiFunc;
		}
		public Section (string name, Texture2D icon, OnGUIDelegate guiFunc)
		{
			this.content = new GUIContent (name, icon);
			this.guiFunc = guiFunc;
		}
		public Section (GUIContent content, OnGUIDelegate guiFunc)
		{
			this.content = content;
			this.guiFunc = guiFunc;
		}
	}
	
	private List<Section> m_Sections;
	private int m_SelectedSectionIndex;
	private int selectedSectionIndex 
	{
		get { return m_SelectedSectionIndex; }
		set 
		{ 
			m_SelectedSectionIndex = value;
			if (m_SelectedSectionIndex >= m_Sections.Count)
				m_SelectedSectionIndex = 0;
			else if (m_SelectedSectionIndex < 0)
				m_SelectedSectionIndex = m_Sections.Count - 1;
		}
	}
	private Section selectedSection { get { return m_Sections[m_SelectedSectionIndex]; } }

	static Constants constants = null;

	private List<IPreferenceWindowExtension> prefWinExtensions;
	private bool m_AutoRefresh;
	private bool m_AlwaysShowProjectWizard;
	private bool m_CompressAssetsOnImport;
	private bool m_UseOSColorPicker;
	private bool m_EnableEditorAnalytics;
	private bool m_ShowAssetStoreSearchHits;
	private bool m_VerifySavingAssets;
	private bool m_AllowAttachedDebuggingOfEditor;
	private bool m_AllowAttachedDebuggingOfEditorStateChangedThisSession;
	private RefString m_ScriptEditorPath = new RefString("");
	private string m_ScriptEditorArgs = "";
	private RefString m_ImageAppPath = new RefString("");
	private int m_DiffToolIndex;

	private string m_AndroidSdkPath = string.Empty;

	private string[] m_ScriptApps;
	private string[] m_ImageApps;
	private string[] m_DiffTools;

	private string m_noDiffToolsMessage = string.Empty;

	private bool m_RefreshCustomPreferences;
	private string[] m_ScriptAppDisplayNames;
	private string[] m_ImageAppDisplayNames;
	Vector2 m_KeyScrollPos;
	Vector2 m_SectionScrollPos;
	PrefKey m_SelectedKey = null;
	private const string kRecentScriptAppsKey = "RecentlyUsedScriptApp";
	private const string kRecentImageAppsKey = "RecentlyUsedImageApp";

	private const string m_ExpressNotSupportedMessage =
		"Unfortunately Visual Studio Express does not allow itself to be controlled by external applications. " +
		"You can still use it by manually opening the Visual Studio project file, but Unity cannot automatically open files for you when you doubleclick them. " +
		"\n(This does work with Visual Studio Pro)";

	private const int kRecentAppsCount = 10;

	SortedDictionary<string, List<KeyValuePair<string, PrefColor>>> s_CachedColors = null;
	static Vector2 s_ColorScrollPos = Vector2.zero;
	int currentPage;

	class RefString
	{
		public RefString(string s) { str = s; }
		public string str;
		public static implicit operator string(RefString s) { return s.str; }
		public override string ToString()
		{
			return str;
		}
	}

	static void ShowPreferencesWindow()
	{
		EditorWindow w = EditorWindow.GetWindowWithRect<PreferencesWindow>(new Rect(100, 100, 500, 400), true, "Unity Preferences");
		w.m_Parent.window.m_DontSaveToLayout = true;
	}

	void OnEnable()
	{
		prefWinExtensions = ModuleManager.GetPreferenceWindowExtensions();

		ReadPreferences();
		
		m_Sections = new List<Section> ();
		
		//@TODO Move these to custom sections
		m_Sections.Add (new Section ("General", ShowGeneral));
		m_Sections.Add (new Section ("External Tools", ShowExternalApplications));
		m_Sections.Add (new Section ("Colors", ShowColors));
		m_Sections.Add (new Section ("Keys", ShowKeys));
		
		// Workaround for EditorAssemblies not loaded yet during mono assembly reload.
		m_RefreshCustomPreferences = true;
	}
	
	private void AddCustomSections ()
	{
		foreach (var assembly in EditorAssemblies.loadedAssemblies)
		{
			Type[] types = AssemblyHelper.GetTypesFromAssembly(assembly);			
			foreach (Type type in types)
			{
				foreach (MethodInfo methodInfo in type.GetMethods (BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
				{
					PreferenceItem attr = Attribute.GetCustomAttribute(methodInfo, typeof(PreferenceItem)) as PreferenceItem;
					if (attr == null)
						continue;
					
					OnGUIDelegate callback = Delegate.CreateDelegate (typeof (OnGUIDelegate), methodInfo) as OnGUIDelegate;
					if (callback != null)
						m_Sections.Add (new Section (attr.name, callback));
				}
			}
		}
	}

	private void OnGUI()
	{
		// Workaround for EditorAssemblies not loaded yet during mono assembly reload.
		if (m_RefreshCustomPreferences)
		{
			AddCustomSections ();
			m_RefreshCustomPreferences = false;
		}
		
		GUILayout.BeginArea (new Rect (0f, 0f, Screen.width, Screen.height)); // Workaround for Utility window adding 10 pixels to the top
	
		EditorGUIUtility.labelWidth = 180f;
		
		if (constants == null)
		{
			constants = new Constants();
		}
		
		HandleKeys ();
		
		GUILayout.BeginHorizontal (); {
			m_SectionScrollPos = GUILayout.BeginScrollView (m_SectionScrollPos, constants.sectionScrollView, GUILayout.Width (120f)); {
				GUILayout.Space (40f);
				for (int i = 0; i < m_Sections.Count; i++)
				{
					var section = m_Sections[i];
				
					Rect elementRect = GUILayoutUtility.GetRect (section.content, constants.sectionElement, GUILayout.ExpandWidth (true));
					
					if (section == selectedSection && Event.current.type == EventType.Repaint)
						constants.selected.Draw (elementRect, false, false, false, false);
					
					EditorGUI.BeginChangeCheck ();
					if(GUI.Toggle (elementRect, m_SelectedSectionIndex == i, section.content, constants.sectionElement))
						m_SelectedSectionIndex = i;
					if (EditorGUI.EndChangeCheck ())
						GUIUtility.keyboardControl = 0;
				}
			} GUILayout.EndScrollView ();
			
			GUILayout.Space (10.0f);
			
			GUILayout.BeginVertical (); {
				GUILayout.Label (selectedSection.content, constants.sectionHeader);
				selectedSection.guiFunc ();
				GUILayout.Space(5.0f);
			} GUILayout.EndVertical ();

			GUILayout.Space(10.0f);
		} GUILayout.EndHorizontal ();
		
		GUILayout.EndArea ();
	}
	
	private void HandleKeys ()
	{
		if (Event.current.type != EventType.KeyDown || GUIUtility.keyboardControl != 0)
			return;
		
		switch (Event.current.keyCode)
		{
		case KeyCode.UpArrow:
			selectedSectionIndex--;
			Event.current.Use ();
			break;
		case KeyCode.DownArrow:
			selectedSectionIndex++;
			Event.current.Use ();
			break;
		}
	}
	
	private void ShowExternalApplications ()
	{
		GUILayout.Space (10f);
		
		// Applications
		FilePopup("External Script Editor", m_ScriptEditorPath, ref m_ScriptAppDisplayNames, ref m_ScriptApps, m_ScriptEditorPath, OnScriptEditorChanged);
		if (!IsSelectedScriptEditorSpecial() && (Application.platform != RuntimePlatform.OSXEditor))
		{
			string oldEditorArgs = m_ScriptEditorArgs;
			m_ScriptEditorArgs = EditorGUILayout.TextField("External Script Editor Args", m_ScriptEditorArgs);
			if (oldEditorArgs != m_ScriptEditorArgs)
				OnScriptEditorArgsChanged();
		}

		bool oldValue = m_AllowAttachedDebuggingOfEditor;
		m_AllowAttachedDebuggingOfEditor = EditorGUILayout.Toggle("Editor Attaching", m_AllowAttachedDebuggingOfEditor);
		
		if (oldValue != m_AllowAttachedDebuggingOfEditor)
			m_AllowAttachedDebuggingOfEditorStateChangedThisSession = true;

		if (m_AllowAttachedDebuggingOfEditorStateChangedThisSession)
			GUILayout.Label("Changing this setting requires a restart to take effect.", EditorStyles.helpBox);

		if (m_ScriptEditorPath.str.Contains("VCSExpress"))
		{
			GUILayout.BeginHorizontal(EditorStyles.helpBox);
			GUILayout.Label("", "CN EntryWarn");
			GUILayout.Label(m_ExpressNotSupportedMessage, constants.errorLabel);
			GUILayout.EndHorizontal();
		}
		
		GUILayout.Space (10f);

		FilePopup("Image application", m_ImageAppPath, ref m_ImageAppDisplayNames, ref m_ImageApps, m_ImageAppPath, null);
		
		GUILayout.Space (10f);

		EditorGUI.BeginDisabledGroup (!InternalEditorUtility.HasMaint()); {
			m_DiffToolIndex = EditorGUILayout.Popup("Revision Control Diff/Merge", m_DiffToolIndex, m_DiffTools);
		} EditorGUI.EndDisabledGroup ();
		
		if (m_noDiffToolsMessage != string.Empty)
		{
			GUILayout.BeginHorizontal(EditorStyles.helpBox);
			GUILayout.Label("", "CN EntryWarn");
			GUILayout.Label(m_noDiffToolsMessage, constants.errorLabel);
			GUILayout.EndHorizontal();
		}
		
		GUILayout.Space (10f);

		AndroidSdkLocation ();

		foreach (IPreferenceWindowExtension extension in prefWinExtensions)
		{
			if (extension.HasExternalApplications())
			{
				GUILayout.Space (10f);
				extension.ShowExternalApplications();
			}
		}

		ApplyChangesToPrefs ();
	}
	
	private bool IsSelectedScriptEditorSpecial()
	{
		string path = m_ScriptEditorPath.str.ToLower();
		return
			path == string.Empty ||
			path.EndsWith("monodevelop.exe") ||
			path.EndsWith("devenv.exe") ||
			path.EndsWith("vcsexpress.exe");
	}

	private void OnScriptEditorChanged()
	{
		if (IsSelectedScriptEditorSpecial())
			m_ScriptEditorArgs = "";
		else
			m_ScriptEditorArgs = EditorPrefs.GetString("kScriptEditorArgs" + m_ScriptEditorPath.str, "\"$(File)\"");
		EditorPrefs.SetString("kScriptEditorArgs", m_ScriptEditorArgs);
	}

	private void OnScriptEditorArgsChanged()
	{
		EditorPrefs.SetString("kScriptEditorArgs" + m_ScriptEditorPath.str, m_ScriptEditorArgs);
		EditorPrefs.SetString("kScriptEditorArgs", m_ScriptEditorArgs);
	}

	private void ShowGeneral()
	{
		GUILayout.Space (10f);
		
		// Options
		m_AutoRefresh = EditorGUILayout.Toggle("Auto Refresh", m_AutoRefresh);
		m_AlwaysShowProjectWizard = EditorGUILayout.Toggle("Always Show Project Wizard", m_AlwaysShowProjectWizard);

		bool oldCompressOnImport = m_CompressAssetsOnImport;
		m_CompressAssetsOnImport = EditorGUILayout.Toggle("Compress Assets on Import", oldCompressOnImport);
		
		if (GUI.changed && m_CompressAssetsOnImport != oldCompressOnImport)
			Unsupported.SetApplicationSettingCompressAssetsOnImport(m_CompressAssetsOnImport);

		if (Application.platform == RuntimePlatform.OSXEditor)
			m_UseOSColorPicker = EditorGUILayout.Toggle("OS X Color Picker", m_UseOSColorPicker);

		m_EnableEditorAnalytics = EditorGUILayout.Toggle("Editor Analytics", m_EnableEditorAnalytics);

		bool assetStoreSearchChanged = false;
		EditorGUI.BeginChangeCheck ();
		m_ShowAssetStoreSearchHits = EditorGUILayout.Toggle("Show Asset Store search hits", m_ShowAssetStoreSearchHits);
		if (EditorGUI.EndChangeCheck())
			assetStoreSearchChanged = true;

		m_VerifySavingAssets = EditorGUILayout.Toggle("Verify Saving Assets", m_VerifySavingAssets);

		EditorGUI.BeginDisabledGroup(!InternalEditorUtility.HasPro()); {
			int newSkin = EditorGUILayout.Popup ("Skin (Pro Only)", !EditorGUIUtility.isProSkin ? 0 : 1, new string[] {"Light", "Dark"});
			if ((!EditorGUIUtility.isProSkin ? 0 : 1) != newSkin)
				InternalEditorUtility.SwitchSkinAndRepaintAllViews ();
		} EditorGUI.EndDisabledGroup ();

		ApplyChangesToPrefs ();

		if (assetStoreSearchChanged)
		{

			ProjectBrowser.ShowAssetStoreHitsWhileSearchingLocalAssetsChanged ();

		}
	}
	
	private void ApplyChangesToPrefs ()
	{
		if (GUI.changed)
		{
			WritePreferences();
			ReadPreferences();
			Repaint();
		}
	}

	private void RevertKeys()
	{
		foreach (KeyValuePair<string, PrefKey> kvp in Settings.Prefs<PrefKey>())
		{
			kvp.Value.ResetToDefault();
			EditorPrefs.SetString(kvp.Value.Name, kvp.Value.ToUniqueString());
		}
	}

	private SortedDictionary<string, List<KeyValuePair<string, T>>> OrderPrefs<T>(IEnumerable<KeyValuePair<string, T>> input)
			where T : IPrefType
	{
		SortedDictionary<string, List<KeyValuePair<string, T>>> retval = new SortedDictionary<string, List<KeyValuePair<string, T>>>();

		foreach (KeyValuePair<string, T> kvp in input)
		{
			int idx = kvp.Key.IndexOf('/');
			string first, second;
			if (idx == -1)
			{
				first = "General";
				second = kvp.Key;
			}
			else
			{
				first = kvp.Key.Substring(0, idx);
				second = kvp.Key.Substring(idx + 1);
			}
			if (!retval.ContainsKey(first))
			{
				List<KeyValuePair<string, T>> inner = new List<KeyValuePair<string, T>>();
				inner.Add(new KeyValuePair<string, T>(second, kvp.Value));
				retval.Add(first, new List<KeyValuePair<string, T>>(inner));
			}
			else
			{
				retval[first].Add(new KeyValuePair<string, T>(second, kvp.Value));
			}
		}

		return retval;
	}
		
	static int s_KeysControlHash = "KeysControlHash".GetHashCode ();
	private void ShowKeys()
	{
		int id = GUIUtility.GetControlID(s_KeysControlHash, FocusType.Keyboard);
		
		GUILayout.Space (10f);
		GUILayout.BeginHorizontal();
		GUILayout.BeginVertical(GUILayout.Width(185f));
		GUILayout.Label("Actions", constants.settingsBoxTitle, GUILayout.ExpandWidth(true));
		m_KeyScrollPos = GUILayout.BeginScrollView(m_KeyScrollPos, constants.settingsBox);

		PrefKey prevKey = null;
		PrefKey nextKey = null;
		bool foundSelectedKey = false;

		foreach (KeyValuePair<string, PrefKey> kvp in Settings.Prefs<PrefKey>())
		{
			if (!foundSelectedKey)
			{
				if (kvp.Value == m_SelectedKey)
				{
					foundSelectedKey = true;
				}
				else
				{
					prevKey = kvp.Value;
				}
			}
			else
			{
				if (nextKey == null) nextKey = kvp.Value;
			}
				
			EditorGUI.BeginChangeCheck ();
				if (GUILayout.Toggle(kvp.Value == m_SelectedKey, kvp.Key, constants.keysElement))
					m_SelectedKey = kvp.Value;
			if (EditorGUI.EndChangeCheck ())
				GUIUtility.keyboardControl = id;
				
		}
		GUILayout.EndScrollView();
		GUILayout.EndVertical();

		GUILayout.Space(10.0f);

		GUILayout.BeginVertical();

		if (m_SelectedKey != null)
		{
			Event e = m_SelectedKey.KeyboardEvent;
			GUI.changed = false;
			var splitKey = m_SelectedKey.Name.Split ('/');
			Assert.AreEqual (splitKey.Length, 2, "Unexpected Split: " + m_SelectedKey.Name);
			GUILayout.Label (splitKey[0], "boldLabel");
			GUILayout.Label (splitKey[1], "boldLabel");

			GUILayout.BeginHorizontal();
			GUILayout.Label("Key:");
			e = EditorGUILayout.KeyEventField(e);
			GUILayout.EndHorizontal();

			GUILayout.BeginHorizontal();
			GUILayout.Label("Modifiers:");
			GUILayout.BeginVertical();
			if (Application.platform == RuntimePlatform.OSXEditor)
				e.command = GUILayout.Toggle(e.command, "Command");
			e.control = GUILayout.Toggle(e.control, "Control");
			e.shift = GUILayout.Toggle(e.shift, "Shift");
			e.alt = GUILayout.Toggle(e.alt, "Alt");
			GUILayout.EndVertical();
			GUILayout.EndHorizontal();

			if (GUI.changed)
			{
				m_SelectedKey.KeyboardEvent = e;
				Settings.Set(m_SelectedKey.Name, m_SelectedKey);
			}
			else
			{
				if (GUIUtility.keyboardControl == id && Event.current.type == EventType.KeyDown)
				{
					switch (Event.current.keyCode)
					{
						case KeyCode.UpArrow:
							if (prevKey != null) m_SelectedKey = prevKey;
							Event.current.Use();
							break;
						case KeyCode.DownArrow:
							if (nextKey != null) m_SelectedKey = nextKey;
							Event.current.Use();
							break;
					}
				}
			}
		}

		GUILayout.EndVertical();
		GUILayout.Space (10f);

		GUILayout.EndHorizontal();
		GUILayout.Space (5f);
		
		if (GUILayout.Button("Use Defaults", GUILayout.Width(120)))
			RevertKeys();
	}

	private void RevertColors()
	{
		foreach (KeyValuePair<string, PrefColor> kvp in Settings.Prefs<PrefColor>())
		{
			kvp.Value.ResetToDefault();
			EditorPrefs.SetString(kvp.Value.Name, kvp.Value.ToUniqueString());
		}
	}

	private void ShowColors()
	{
		if (s_CachedColors == null)
		{
			s_CachedColors = OrderPrefs<PrefColor>(Settings.Prefs<PrefColor>());
		}

		var changedColor = false;
		s_ColorScrollPos = EditorGUILayout.BeginScrollView(s_ColorScrollPos);
		GUILayout.Space (10f);
		PrefColor ccolor = null;
		foreach (KeyValuePair<string, List<KeyValuePair<string, PrefColor>>> category in s_CachedColors)
		{
			GUILayout.Label(category.Key, EditorStyles.boldLabel);
			foreach (KeyValuePair<string, PrefColor> kvp in category.Value)
			{
				EditorGUI.BeginChangeCheck ();
				Color c = EditorGUILayout.ColorField(kvp.Key, kvp.Value.Color);
				if (EditorGUI.EndChangeCheck ())
				{
					ccolor = kvp.Value;
					ccolor.Color = c;
					changedColor = true;
				}
			}
			if (ccolor != null)
				Settings.Set(ccolor.Name, ccolor);
		}
		GUILayout.EndScrollView();
		GUILayout.Space(5f);

		if (GUILayout.Button("Use Defaults", GUILayout.Width(120)))
		{
			RevertColors ();
			changedColor = true;
		}

		if (changedColor)
			EditorApplication.RequestRepaintAllViews ();
	}

	private void WriteRecentAppsList(string[] paths, string path, string prefsKey)
	{
		int appIndex = 0;
		// first write the selected app (if it's not a built-in one)
		if (path.Length != 0)
		{
			EditorPrefs.SetString(prefsKey + appIndex, path);
			++appIndex;
		}
		// write the other apps
		for (int i = 0; i < paths.Length; ++i)
		{
			if (appIndex >= kRecentAppsCount)
				break; // stop when we wrote up to the limit
			if (paths[i].Length == 0)
				continue; // do not write built-in app into recently used list
			if (paths[i] == path)
				continue; // this is a selected app, do not write it twice
			EditorPrefs.SetString(prefsKey + appIndex, paths[i]);
			++appIndex;
		}
	}

	private void WritePreferences()
	{
		EditorPrefs.SetString("kScriptsDefaultApp", m_ScriptEditorPath);
		EditorPrefs.SetString("kScriptEditorArgs", m_ScriptEditorArgs);
		EditorPrefs.SetString("kImagesDefaultApp", m_ImageAppPath);
		EditorPrefs.SetString("kDiffsDefaultApp", m_DiffTools.Length == 0 ? "" : m_DiffTools[m_DiffToolIndex]);
		EditorPrefs.SetString("AndroidSdkRoot", m_AndroidSdkPath);

		WriteRecentAppsList(m_ScriptApps, m_ScriptEditorPath, kRecentScriptAppsKey);
		WriteRecentAppsList(m_ImageApps, m_ImageAppPath, kRecentImageAppsKey);
			
		EditorPrefs.SetBool("kAutoRefresh", m_AutoRefresh);
		EditorPrefs.SetBool("AlwaysShowProjectWizard", m_AlwaysShowProjectWizard);
		EditorPrefs.SetBool("UseOSColorPicker", m_UseOSColorPicker);
		EditorPrefs.SetBool("EnableEditorAnalytics", m_EnableEditorAnalytics);
		EditorPrefs.SetBool("ShowAssetStoreSearchHits", m_ShowAssetStoreSearchHits);
		EditorPrefs.SetBool("VerifySavingAssets", m_VerifySavingAssets);
		EditorPrefs.SetBool("AllowAttachedDebuggingOfEditor", m_AllowAttachedDebuggingOfEditor);

		foreach (IPreferenceWindowExtension extension in prefWinExtensions)
		{
			extension.WritePreferences();
		}
	}

	static private void SetupDefaultPreferences()
	{
	}

	static private string GetProgramFilesFolder()
	{
		string result = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
		if (result != null) return result;
		return Environment.GetEnvironmentVariable("ProgramFiles");
	}

	private void ReadPreferences()
	{
		m_ScriptEditorPath.str = EditorPrefs.GetString("kScriptsDefaultApp");
		m_ScriptEditorArgs = EditorPrefs.GetString("kScriptEditorArgs", "\"$(File)\"");
		m_ImageAppPath.str = EditorPrefs.GetString("kImagesDefaultApp");
		m_AndroidSdkPath = EditorPrefs.GetString("AndroidSdkRoot");

		m_ScriptApps = BuildAppPathList(m_ScriptEditorPath, kRecentScriptAppsKey);

		if (Application.platform == RuntimePlatform.WindowsEditor)
		{
			foreach (string vs in SyncVS.InstalledVisualStudios.Values)
				if (Array.IndexOf(m_ScriptApps, vs) == -1)
				{
					if (m_ScriptApps.Length < kRecentAppsCount)
						ArrayUtility.Add(ref m_ScriptApps, vs);
					else
						// Replace the second entry
						m_ScriptApps[1] = vs;
				}
		}

		m_ImageApps = BuildAppPathList(m_ImageAppPath, kRecentImageAppsKey);

		m_ScriptAppDisplayNames = BuildFriendlyAppNameList(m_ScriptApps,
			"MonoDevelop (built-in)");
		m_ImageAppDisplayNames = BuildFriendlyAppNameList(m_ImageApps,
			"Open by file extension");

		m_DiffTools = InternalEditorUtility.GetAvailableDiffTools();

		// only show warning if has asset server license
		if ((m_DiffTools == null || m_DiffTools.Length == 0) && InternalEditorUtility.HasMaint())
		{
			m_noDiffToolsMessage = InternalEditorUtility.GetNoDiffToolsDetectedMessage();
		}

		string diffTool = EditorPrefs.GetString("kDiffsDefaultApp");
		m_DiffToolIndex = ArrayUtility.IndexOf(m_DiffTools, diffTool);
		if (m_DiffToolIndex == -1)
			m_DiffToolIndex = 0;
			
		m_AutoRefresh = EditorPrefs.GetBool("kAutoRefresh");
		m_AlwaysShowProjectWizard = EditorPrefs.GetBool("AlwaysShowProjectWizard");
		m_UseOSColorPicker = EditorPrefs.GetBool("UseOSColorPicker");
		m_EnableEditorAnalytics = EditorPrefs.GetBool("EnableEditorAnalytics", true);
		m_ShowAssetStoreSearchHits = EditorPrefs.GetBool("ShowAssetStoreSearchHits", true);
		m_VerifySavingAssets = EditorPrefs.GetBool("VerifySavingAssets", false);

		m_AllowAttachedDebuggingOfEditor = EditorPrefs.GetBool("AllowAttachedDebuggingOfEditor", true);

		m_CompressAssetsOnImport = Unsupported.GetApplicationSettingCompressAssetsOnImport();

		foreach (IPreferenceWindowExtension extension in prefWinExtensions)
		{
			extension.ReadPreferences();
		}
	}


	class AppsListUserData
	{
		public AppsListUserData(string[] paths, RefString str, Action onChanged)
		{
			this.paths = paths;
			this.str = str;
			this.onChanged = onChanged;
		}
		public string[] paths;
		public RefString str;
		public Action onChanged;
	}

	void AppsListClick(object userData, string[] options, int selected)
	{
		AppsListUserData ud = (AppsListUserData)userData;
		if (options[selected] == "Browse...")
		{
			string path = EditorUtility.OpenFilePanel("Browse for application", "", Application.platform == RuntimePlatform.OSXEditor ? "app" : "exe");
			if (path.Length != 0)
			{
				// browsed to new application
				ud.str.str = path;
				if (ud.onChanged != null)
					ud.onChanged();
			}
		}
		else
		{
			// value comes from the list
			ud.str.str = ud.paths[selected];
			if (ud.onChanged != null)
				ud.onChanged();
		}

		WritePreferences();
		ReadPreferences();

		return;
	}


	private void FilePopup(string label, string selectedString, ref string[] names, ref string[] paths, RefString outString, Action onChanged)
	{
		GUIStyle style = "MiniPopup";
		GUILayout.BeginHorizontal();
		EditorGUILayout.PrefixLabel(label, style);

		int[] selected = { Array.IndexOf(paths, selectedString) };
		GUIContent text = new GUIContent(names[selected[0]]);
		Rect r = GUILayoutUtility.GetRect(GUIContent.none, style);
		AppsListUserData ud = new AppsListUserData(paths, outString, onChanged);
		if (EditorGUI.ButtonMouseDown(r, text, FocusType.Native, style))
		{
			ArrayUtility.Add(ref names, "Browse...");
			EditorUtility.DisplayCustomMenu(r, names, selected, AppsListClick, ud);
		}
		GUILayout.EndHorizontal();
	}

	// TODO relocate to dll extension once Android enables it
	private void AndroidSdkLocation ()
	{
		GUIStyle style = "MiniPopup";
		GUILayout.BeginHorizontal ();
		EditorGUILayout.PrefixLabel ("Android SDK Location", style);
		var text = string.IsNullOrEmpty (m_AndroidSdkPath) ? "Browse..." : m_AndroidSdkPath;

		GUIContent guiText = new GUIContent (text);
		Rect r = GUILayoutUtility.GetRect (GUIContent.none, style);

		if (EditorGUI.ButtonMouseDown (r, guiText, FocusType.Native, style))
		{
			var path = AndroidSdkRoot.Browse (m_AndroidSdkPath);

			if (!string.IsNullOrEmpty (path))
			{
				m_AndroidSdkPath = path;
				WritePreferences ();
				ReadPreferences ();
			}
		}

		GUILayout.EndHorizontal ();
	}

	private string[] BuildAppPathList(string userAppPath, string recentAppsKey)
	{
		// built-in (empty path) is always the first
		string[] apps = new string[1];
		apps[0] = string.Empty;

		// current user setting
		if (userAppPath != null && userAppPath.Length != 0 && Array.IndexOf(apps, userAppPath) == -1)
			ArrayUtility.Add(ref apps, userAppPath);

		// add any recently used apps
		for (int i = 0; i < kRecentAppsCount; ++i)
		{
			string path = EditorPrefs.GetString(recentAppsKey + i);
			if (!File.Exists(path))
			{
				path = "";
				EditorPrefs.SetString(recentAppsKey + i, path);
			}

			if (path.Length != 0 && Array.IndexOf(apps, path) == -1)
				ArrayUtility.Add(ref apps, path);
		}

		return apps;
	}


	private string[] BuildFriendlyAppNameList(string[] appPathList, string defaultBuiltIn)
	{
		var list = new List<string>();
		foreach (var appPath in appPathList)
		{
			if (appPath == "") // use built-in
				list.Add(defaultBuiltIn);
			else
				list.Add(OSUtil.GetAppFriendlyName(appPath));
		}

		return list.ToArray();
	}
}
}
