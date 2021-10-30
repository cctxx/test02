using UnityEngine;
using System.Collections;
using UnityEditor;

namespace UnityEditor {

internal class WelcomeScreen : EditorWindow 
{
	static void DoShowWelcomeScreen (string how) 
	{
		s_ShowCount ++;
		EditorPrefs.SetInt ("WelcomeScreenShowCount", s_ShowCount);
		
		Analytics.Track(string.Format("/WelcomeScreen/Show/{0}/{1}", how, s_ShowCount) );
		EditorWindow.GetWindowWithRect<WelcomeScreen>(new Rect(0, 0, 570, 440), true, "Welcome To Unity");
	}
	
	static void ShowWelcomeScreen () 
	{
		DoShowWelcomeScreen("Manual");
	}

	static void ShowWelcomeScreenAtStartup () 
	{
		LoadLogos ();
		if (s_ShowAtStartup) 
		{
			DoShowWelcomeScreen("Startup");
		}	
	}
	class Styles 
	{
		public GUIContent unityLogo = EditorGUIUtility.IconContent ("UnityLogo");
		public GUIContent mainHeader = EditorGUIUtility.IconContent ("WelcomeScreen.MainHeader");
		public GUIContent mainText = EditorGUIUtility.TextContent ("WelcomeScreen.MainText");

		public GUIContent videoTutLogo = EditorGUIUtility.IconContent ("WelcomeScreen.VideoTutLogo");
		public GUIContent videoTutHeader = EditorGUIUtility.TextContent ("WelcomeScreen.VideoTutHeader");
		public GUIContent videoTutText = EditorGUIUtility.TextContent ("WelcomeScreen.VideoTutText");

		public GUIContent unityBasicsLogo = EditorGUIUtility.IconContent ("WelcomeScreen.UnityBasicsLogo");
		public GUIContent unityBasicsHeader = EditorGUIUtility.TextContent ("WelcomeScreen.UnityBasicsHeader");
		public GUIContent unityBasicsText = EditorGUIUtility.TextContent ("WelcomeScreen.UnityBasicsText");
		
		public GUIContent unityForumLogo = EditorGUIUtility.IconContent ("WelcomeScreen.UnityForumLogo");
		public GUIContent unityForumHeader = EditorGUIUtility.TextContent ("WelcomeScreen.UnityForumHeader");
		public GUIContent unityForumText = EditorGUIUtility.TextContent ("WelcomeScreen.UnityForumText");
		
		public GUIContent unityAnswersLogo = EditorGUIUtility.IconContent ("WelcomeScreen.UnityAnswersLogo");
		public GUIContent unityAnswersHeader = EditorGUIUtility.TextContent ("WelcomeScreen.UnityAnswersHeader");
		public GUIContent unityAnswersText = EditorGUIUtility.TextContent ("WelcomeScreen.UnityAnswersText");
		
		public GUIContent assetStoreLogo = EditorGUIUtility.IconContent ("WelcomeScreen.AssetStoreLogo");
		public GUIContent assetStoreHeader = EditorGUIUtility.TextContent ("WelcomeScreen.AssetStoreHeader");
		public GUIContent assetStoreText = EditorGUIUtility.TextContent ("WelcomeScreen.AssetStoreText");

		public GUIContent showAtStartupText = EditorGUIUtility.TextContent ("WelcomeScreen.ShowAtStartup");
	};
	
	static Styles styles;
	private static bool s_ShowAtStartup;
	private static int  s_ShowCount;
	
	const string kVideoTutURL = "http://unity3d.com/learn/tutorials/modules/";
	const string kUnityBasicsHelp = "file:///unity/Manual/UnityBasics.html";
	const string kUnityAnswersURL = "http://answers.unity3d.com/";
	const string kUnityForumURL = "http://forum.unity3d.com/";
	const string kAssetStoreURL = "home/?ref=http%3a%2f%2fUnityEditor.unity3d.com%2fWelcomeScreen";
	
	const float kItemHeight = 55;
	
	static void LoadLogos () 
	{
		if (styles == null)
		{
			s_ShowAtStartup = EditorPrefs.GetInt ("ShowWelcomeAtStartup4", 1) != 0;
			s_ShowCount =  EditorPrefs.GetInt ("WelcomeScreenShowCount", 0);
			styles = new Styles ();
		}	
	}
	
	public void OnGUI() 
	{
		LoadLogos ();
		GUILayout.BeginVertical ();
		GUI.Box (new Rect (13,8, styles.unityLogo.image.width,styles.unityLogo.image.height), styles.unityLogo, GUIStyle.none);
		GUILayout.Space (5);
		GUILayout.BeginHorizontal ();
			GUILayout.Space (120);
			GUILayout.BeginVertical ();
				GUILayout.Label (styles.mainHeader); 
				GUILayout.Label (styles.mainText, "WordWrappedLabel", GUILayout.Width (300));		
			GUILayout.EndVertical ();
		GUILayout.EndHorizontal ();

		GUILayout.Space (8);

		ShowEntry (styles.videoTutLogo, kVideoTutURL, styles.videoTutHeader, styles.videoTutText, "VideoTutorials");
		ShowEntry (styles.unityBasicsLogo, kUnityBasicsHelp, styles.unityBasicsHeader, styles.unityBasicsText, "UnityBasics");
		ShowEntry (styles.unityAnswersLogo, kUnityAnswersURL, styles.unityAnswersHeader, styles.unityAnswersText, "UnityAnswers");
		ShowEntry (styles.unityForumLogo, kUnityForumURL, styles.unityForumHeader, styles.unityForumText, "UnityForum");
		ShowEntry (styles.assetStoreLogo, kAssetStoreURL, styles.assetStoreHeader, styles.assetStoreText, "AssetStore");

		GUILayout.FlexibleSpace ();
		GUILayout.BeginHorizontal (GUILayout.Height (20));
			GUILayout.FlexibleSpace ();
			GUI.changed = false;
			s_ShowAtStartup = GUILayout.Toggle (s_ShowAtStartup, styles.showAtStartupText);
			if (GUI.changed) 
			{
				EditorPrefs.SetInt ("ShowWelcomeAtStartup4", s_ShowAtStartup ? 1: 0);
				
				if (s_ShowAtStartup)
					Analytics.Track(string.Format("/WelcomeScreen/EnableAtStartup/{0}", s_ShowCount) );
				else
					Analytics.Track(string.Format("/WelcomeScreen/DisableAtStartup/{0}", s_ShowCount) );
				
				// We are interested in counting how many times the WelcomeScreen is shown before toggling
				// the show at startup flag, so we reset the counter every time it happens.
				s_ShowCount = 0;
				EditorPrefs.SetInt ("WelcomeScreenShowCount", 0);
			}

			GUILayout.Space (10);
		GUILayout.EndHorizontal ();	
		GUILayout.EndVertical ();
	}
	
	void ShowEntry (GUIContent logo, string url, GUIContent header, GUIContent text, string analyticsAction)
	{
		GUILayout.BeginHorizontal (GUILayout.Height (kItemHeight));
			GUILayout.BeginHorizontal (GUILayout.Width(120));
				GUILayout.FlexibleSpace ();
				if (GUILayout.Button (logo, GUIStyle.none))
					ShowHelpPageOrBrowseURL(url, analyticsAction);
				EditorGUIUtility.AddCursorRect (GUILayoutUtility.GetLastRect (), MouseCursor.Link);
				GUILayout.Space (10);
			GUILayout.EndHorizontal ();
			GUILayout.BeginVertical ();
				if (GUILayout.Button (header, "HeaderLabel"))
					ShowHelpPageOrBrowseURL(url, analyticsAction);
				EditorGUIUtility.AddCursorRect (GUILayoutUtility.GetLastRect (), MouseCursor.Link);
				GUILayout.Label (text, "WordWrappedLabel", GUILayout.Width (400));
			GUILayout.EndVertical ();
		GUILayout.EndHorizontal ();
	}
	
	void ShowHelpPageOrBrowseURL (string url, string analyticsAction)
	{
		Analytics.Track(string.Format("/WelcomeScreen/OpenURL/{0}/{1}", analyticsAction, s_ShowCount) );
		
		if (url.StartsWith("file"))
		{
			Help.ShowHelpPage (url);
		}
		else if (url.StartsWith("home/"))
		{
			UnityEditorInternal.AssetStore.Open(url);
			EditorGUIUtility.ExitGUI();
		}
		else
		{
			Help.BrowseURL (url);
		}
	}
}

} // namespace
