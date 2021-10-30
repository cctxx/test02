using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEditorInternal;


namespace UnityEditor
{

internal struct AttachProfilerUI
{
	private GUIContent m_CurrentProfiler;
	private int[] m_ConnectionGuids;
	private Rect m_ButtonScreenRect;
	private static string kEnterIPText = "<Enter IP>";
	private static GUIContent ms_NotificationMessage;
	
	void SelectProfilerClick(object userData, string[] options, int selected)
	{
		if (selected < m_ConnectionGuids.Length)
		{
			int guid = m_ConnectionGuids[selected];
			ProfilerDriver.connectedProfiler = guid;
		}
		else if (options[selected] == kEnterIPText)
		{
			// launch Enter IP dialog
			ProfilerIPWindow.Show(m_ButtonScreenRect);
		}
		else
		{
			// last ip
			DirectIPConnect(options[selected]);
		}
	}

	public bool IsEditor()
	{
		return ProfilerDriver.IsConnectionEditor();
	}
	
	public string GetConnectedProfiler()
	{
		return ProfilerDriver.GetConnectionIdentifier(ProfilerDriver.connectedProfiler);
	}

	public static void DirectIPConnect(string ip)
	{
		// Profiler.DirectIPConnect is a blocking call, so a notification message and the console are used to show progress
		ConsoleWindow.ShowConsoleWindow(true);
		ms_NotificationMessage = new GUIContent("Connecting to IP...(this can take a while)");
		ProfilerDriver.DirectIPConnect(ip);
		ms_NotificationMessage = null;
	}

    public void OnGUILayout(EditorWindow window)
	{
		if (m_CurrentProfiler == null)
			m_CurrentProfiler = EditorGUIUtility.TextContent("Profiler.CurrentProfiler");
			
		Rect connectRect = GUILayoutUtility.GetRect(m_CurrentProfiler, EditorStyles.toolbarDropDown, GUILayout.Width(100));
		OnGUI (connectRect, m_CurrentProfiler);

		if (ms_NotificationMessage != null)
			window.ShowNotification(ms_NotificationMessage);
		else
			window.RemoveNotification();
	}

	public void OnGUI (Rect connectRect, GUIContent profilerLabel)
	{
		if (!EditorGUI.ButtonMouseDown(connectRect, profilerLabel, FocusType.Native, EditorStyles.toolbarDropDown))
			return;

		int currentProfiler = ProfilerDriver.connectedProfiler;
		m_ConnectionGuids = ProfilerDriver.GetAvailableProfilers();
		int length = m_ConnectionGuids.Length;
		int[] selected = { 0 };
		var enabled = new List<bool>(); 
		var names = new List<string>(); 
		for (int index = 0; index < length; index++)
		{
			int guid = m_ConnectionGuids[index];
			string name = ProfilerDriver.GetConnectionIdentifier(guid);
			bool enable = ProfilerDriver.IsIdentifierConnectable(guid);
			bool isProhibited = ProfilerDriver.IsIdentifierOnLocalhost(guid) && name.Contains("MetroPlayerX");
			if (isProhibited)
				enable = false;
			enabled.Add(enable);
			if (!enable)
			{
				if (isProhibited)
					name += " (Localhost prohibited)";
				else
					name += " (Version mismatch)";
			}
			names.Add (name);
			
			if (guid == currentProfiler)
				selected[0] = index;
		}
		string lastIP = ProfilerIPWindow.GetLastIPString();
		if (!string.IsNullOrEmpty(lastIP))
		{
			// keep this constant in sync with PLAYER_DIRECT_IP_CONNECT_GUID in GeneralConnection.h
			if (currentProfiler == 0xFEED)
				selected[0] = length;
			names.Add(lastIP);
			enabled.Add(true);
		}

		names.Add(kEnterIPText);
		enabled.Add(true);

		m_ButtonScreenRect = GUIUtility.GUIToScreenRect(connectRect);
		
		EditorUtility.DisplayCustomMenu(connectRect, names.ToArray(), enabled.ToArray(), selected, SelectProfilerClick, null);
	}
}

internal class ProfilerIPWindow : EditorWindow
{
	private const string kTextFieldId = "IPWindow";
	private const string kLastIP = "ProfilerLastIP";
	internal string m_IPString = GetLastIPString();
	internal bool didFocus = false;

	public static void Show(Rect buttonScreenRect)
	{
		Rect rect = new Rect(buttonScreenRect.x, buttonScreenRect.yMax, 300, 60);
		ProfilerIPWindow w = EditorWindow.GetWindowWithRect<ProfilerIPWindow>(rect, true, "Enter Player IP");
		w.position = rect;
		w.m_Parent.window.m_DontSaveToLayout = true;
	}
	
	public static string GetLastIPString()
	{
		return EditorPrefs.GetString(kLastIP, "");
	}

	void OnGUI()
	{
		Event evt = Event.current;
		bool hitEnter = evt.type == EventType.KeyDown && (evt.keyCode == KeyCode.Return || evt.keyCode == KeyCode.KeypadEnter);
		GUI.SetNextControlName(kTextFieldId);

		/*Rect contentRect = */EditorGUILayout.BeginVertical();
		{
			m_IPString = EditorGUILayout.TextField(m_IPString);


			if (!didFocus)
			{
				didFocus = true;
				EditorGUI.FocusTextInControl(kTextFieldId);
			}

			GUI.enabled = m_IPString.Length != 0;
			if (GUILayout.Button("Connect") || hitEnter)
			{
				Close();
				// Save ip
				EditorPrefs.SetString(kLastIP, m_IPString);
				AttachProfilerUI.DirectIPConnect(m_IPString);
				GUIUtility.ExitGUI();
			}
		}
		EditorGUILayout.EndVertical();

		//position.height = contentRect.height;
	}
}


}
