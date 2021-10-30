using Microsoft.Win32;
using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using System;
using System.IO;

namespace UnityEditor
{
internal class CacheServerPreferences
{
	private static bool s_PrefsLoaded;
	enum ConnectionState {Unknown, Success, Failure};
	private static ConnectionState s_ConnectionState;

	private static bool s_CacheServerEnabled;
	private static string s_CacheServerIPAddress;

	public static void ReadPreferences()
	{
		s_CacheServerIPAddress = EditorPrefs.GetString("CacheServerIPAddress", s_CacheServerIPAddress);
		s_CacheServerEnabled = EditorPrefs.GetBool("CacheServerEnabled");
	}

	public static void WritePreferences()
	{
		EditorPrefs.SetString("CacheServerIPAddress", s_CacheServerIPAddress);
		EditorPrefs.SetBool("CacheServerEnabled", s_CacheServerEnabled);
	}

	[PreferenceItem("Cache Server")]
	public static void OnGUI()
	{
		GUILayout.Space (10f);
		
		if (!InternalEditorUtility.HasMaint())
			GUILayout.Label (EditorGUIUtility.TempContent ("You need to have an Asset Server license to use the cache server.", EditorGUIUtility.GetHelpIcon (MessageType.Warning)), EditorStyles.helpBox);

		EditorGUI.BeginDisabledGroup (!InternalEditorUtility.HasMaint());		
		
		if (!s_PrefsLoaded)
		{
			ReadPreferences ();
			s_PrefsLoaded = true;
		}
		
		EditorGUI.BeginChangeCheck ();
		
		s_CacheServerEnabled = EditorGUILayout.Toggle("Use Cache Server", s_CacheServerEnabled);
		
		EditorGUI.BeginDisabledGroup (!s_CacheServerEnabled); {	
			
			s_CacheServerIPAddress = EditorGUILayout.TextField("IP Address", s_CacheServerIPAddress);
			if (GUI.changed)
				s_ConnectionState = ConnectionState.Unknown;
			
			GUILayout.Space(5);
			if (GUILayout.Button ("Check Connection", GUILayout.Width(150)))
			{
				if (InternalEditorUtility.CanConnectToCacheServer())
					s_ConnectionState = ConnectionState.Success;
				else
					s_ConnectionState = ConnectionState.Failure;
			}
			GUILayout.Space(-25);
			switch (s_ConnectionState)
			{
				case ConnectionState.Success:
					EditorGUILayout.HelpBox ("Connection successful.", MessageType.Info, false);
					break;

				case ConnectionState.Failure:
					EditorGUILayout.HelpBox ("Connection failed.", MessageType.Warning, false);
					break;
			}
		} EditorGUI.EndDisabledGroup ();
		
		if (EditorGUI.EndChangeCheck ())
		{
			WritePreferences ();
			ReadPreferences ();
		}
		EditorGUI.EndDisabledGroup ();
	}
}
}
