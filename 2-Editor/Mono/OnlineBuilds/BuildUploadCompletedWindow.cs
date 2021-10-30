using UnityEngine;
using UnityEditor;
using System.Collections;


namespace UnityEditor
{
	
internal class BuildUploadCompletedWindow : EditorWindow
{
	public enum SocialNetwork
	{
		LinkedIn = 0,
		Facebook,
		Twitter,
		GooglePlus,
		UnityDeveloperNetwork
	}
	
	
	class Styles
	{
		public const float
			kWindowX = 100.0f, 
			kWindowY = 100.0f,
			kWindowWidth = 470,
			kWindowHeight = 170.0f, 
			kWindowPadding = 5.0f,
			kBottomButtonWidth = 120.0f,
			kBottomButtonHeight = 20.0f,
			kIconSpacing = 10.0f,
			kIconSpacingRight = 5.0f,
			kLogoAreaWidth = 150.0f,
			kLogoAreaHeight = 150.0f,
			kCopyLabelIndentation = 4.0f,
			kCopyLabelConfirmDelay = 3.0f,
			kTextPaddingBottom = 4.0f;
		public const int
			kMaxCopyLabelLength = 33;
		
		public GUIStyle m_CopyLabelStyle = null;
		
		public Texture2D m_FontColorTexture = null;
		
		
		public Styles ()
		{
			m_CopyLabelStyle = new GUIStyle (EditorStyles.label);
			m_CopyLabelStyle.name = "CopyLabel";
			m_CopyLabelStyle.margin = m_CopyLabelStyle.padding = new RectOffset (0, 0, 0, 0);
			
			m_FontColorTexture = new Texture2D (1, 1);
			m_FontColorTexture.SetPixels (new Color[] {EditorStyles.label.onNormal.textColor, EditorStyles.label.onNormal.textColor});
			m_FontColorTexture.Apply ();
		}
	}
	
	
	class Content
	{
		public const string
			kUDNStatusURL = "http://forum.unity3d.com",
			kUDNPlayerControlPanelURL = "http://forum.unity3d.com";
		
		
		public GUIContent m_UDNLogo = EditorGUIUtility.IconContent ("SocialNetworks.UDNLogo");
		public GUIContent m_LinkedInShare = EditorGUIUtility.IconContent ("SocialNetworks.LinkedInShare");
		public GUIContent m_FacebookShare = EditorGUIUtility.IconContent ("SocialNetworks.FacebookShare");
		public GUIContent m_Tweet = EditorGUIUtility.IconContent ("SocialNetworks.Tweet");
		public GUIContent m_OpenInBrowser = EditorGUIUtility.IconContent ("SocialNetworks.UDNOpen");
		public GUIContent m_PasteboardIcon = EditorGUIUtility.IconContent ("Clipboard");

		public GUIContent m_WindowTitleSuccess = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.WindowTitleSuccess");
		public GUIContent m_TextHeaderSuccess = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.TextHeaderSuccess");
		public GUIContent m_MainTextSuccess = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.MainTextSuccess");
		
		public GUIContent m_WindowTitleFailure = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.WindowTitleFailure");
		public GUIContent m_TextHeaderFailure = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.TextHeaderFailure");
		public GUIContent m_MainTextFailureRecoverable = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.MainTextFailureRecoverable");
		public GUIContent m_MainTextFailureCritical = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.MainTextFailureCritical");
		public GUIContent m_FailurePreLinkAssistText = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.FailurePreLinkAssistText");
		public GUIContent m_UDNStatusLinkText = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.UDNStatusLinkText");
		public GUIContent m_FailurePostLinkAssistText = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.FailurePostLinkAssistText");

		public GUIContent m_ShareMessage = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.ShareMessage");
		public GUIContent m_CopyToClipboardMessage = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.CopyToClipboardMessage");
		public GUIContent m_DidCopyToClipboardMessage = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.DidCopyToClipboardMessage");
		public GUIContent m_CancelButton = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.CancelButton");
		public GUIContent m_FixButton = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.FixButton");
		public GUIContent m_CloseButton = EditorGUIUtility.TextContent ("BuildUploadCompletedWindow.CloseButton");
	}
	
	
	static Styles s_Styles = null;
	static Content s_Content = null;
	
	
	bool m_Success = false;
	bool m_RecoverableError = false;
	string m_URL = "", m_ShortURL = "", m_ErrorMessage = "";
	double m_CopyLabelConfirmStart = 0.0;
	float m_CopyLabelConfirmAlpha = 0.0f;	
	
	public static void LaunchSuccess (string url)
	{
		BuildUploadCompletedWindow window = Launch ();
		
		window.title = s_Content.m_WindowTitleSuccess.text;
		window.m_Success = true;
		window.m_URL = url;
		window.m_ShortURL = ShortenURL (url);
	}
	
	
	public static void LaunchFailureCritical (string message)
	{
		LaunchFailure (message).m_RecoverableError = false;
	}
	
	
	public static void LaunchFailureRecoverable (string message)
	{
		LaunchFailure (message).m_RecoverableError = true;
	}
	
	
	static BuildUploadCompletedWindow LaunchFailure (string message)
	{
		BuildUploadCompletedWindow window = Launch ();
		
		window.title = s_Content.m_WindowTitleFailure.text;
		window.m_Success = false;
		window.m_ErrorMessage = message;
		
		return window;
	}
	
	
	static BuildUploadCompletedWindow Launch ()
	{
		BuildUploadCompletedWindow window = GetWindow<BuildUploadCompletedWindow> (true);
		
		window.position = new Rect (
			Styles.kWindowX,
			Styles.kWindowY,
			Styles.kWindowWidth,
			Styles.kWindowHeight
		);
		window.minSize = new Vector2 (Styles.kWindowWidth, Styles.kWindowHeight);
		window.maxSize = new Vector2 (Styles.kWindowWidth, Styles.kWindowHeight);
		
		return window;
	}
	
	
	public static string ShortenURL (string url)
	{
		return url;
			// TODO: When we have a new domain for the UDN URL shortener, write up a backend and have this use it
	}
	
	
	public static string GetShareURL (string url, string title, SocialNetwork network)
	// Produces the URL needed for sharing a URL with a given title on the given social network
	{
		switch (network)
		{
			case SocialNetwork.LinkedIn:
				return string.Format (
					"http://www.linkedin.com/shareArticle?title={0}&source=%3A%2F%2Funity3d.com&url={1}",
					WWW.EscapeURL (title),
					WWW.EscapeURL (url)
				);
			case SocialNetwork.Facebook:
				return string.Format (
					"http://www.facebook.com/sharer.php?t={0}&u={1}",
					WWW.EscapeURL (title),
					WWW.EscapeURL (url)
				);
					// TODO: Figure out why the hell facebook don't show the title when we're using the URL setup they specified
			case SocialNetwork.Twitter:
				return string.Format (
					"http://twitter.com/share?text={0}+{1}+%23unity3d",
					WWW.EscapeURL (title),
					WWW.EscapeURL (url)
				);
			/*case SocialNetwork.GooglePlus:
				// TODO: Implement this when Google get their stuff together and add the ability to share on G+ in any way
			break;*/
			/*case SocialNetwork.UnityDeveloperNetwork:
				// TODO: Implement this when we offer a "share on UDN" feature
			break;*/
			default:
				throw new System.ArgumentException (string.Format ("Social network {0} not supported", network));
		}
	}
	
	
	bool LinkLabel (GUIContent content)
	// An underlined label or icon with a link cursor on hover - responding to mouse clicks
	{
		Rect rect;
		bool textLabel = !string.IsNullOrEmpty (content.text);
		
		if (textLabel)
		{
			GUILayout.Label (content, GUILayout.ExpandWidth (false));
			rect = GUILayoutUtility.GetLastRect ();
		}
		else
		{
			rect = GUILayoutUtility.GetRect (content.image.width, content.image.height);
		}
		
		EditorGUIUtility.AddCursorRect (rect, MouseCursor.Link);
		
		switch (Event.current.type)
		{
			case EventType.Repaint:
				if (textLabel)
				{
					GUI.DrawTexture (
						new Rect (rect.x, rect.y + rect.height - 3.0f, rect.width, 1.0f),
						s_Styles.m_FontColorTexture
					);
				}
				else
				{
					GUI.DrawTexture (rect, content.image);
				}
			break;
			case EventType.MouseDown:
				if (rect.Contains (Event.current.mousePosition))
				{
					Event.current.Use ();
					return true;
				}
			break;
		}
		
		return false;
	}
	
	
	void CopyLabel (string content, string copyMessage)
	// CopyLabel responds to clicks by copying its contents to the system copy buffer
	{
		// content = "http://u3d.as/OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
			// Use for testing values of Styles.kMaxCopyLabelLength
		
		GUILayout.BeginHorizontal (GUILayout.ExpandWidth (false));
			GUILayout.Space (Styles.kCopyLabelIndentation);
		
			GUILayout.Label (
				new GUIContent (
					(content.Length > Styles.kMaxCopyLabelLength ? content.Substring (0, Styles.kMaxCopyLabelLength) + "..." : content)
				),
				s_Styles.m_CopyLabelStyle,
				GUILayout.ExpandWidth (false)
			);
			
			Rect rect = GUILayoutUtility.GetLastRect ();
			GUILayout.Space (Styles.kIconSpacing);
			
			GUILayout.Label (
				new GUIContent (
					s_Content.m_PasteboardIcon.image
				),
				GUIStyle.none,
				GUILayout.Width (s_Content.m_PasteboardIcon.image.width),
				GUILayout.Height (s_Content.m_PasteboardIcon.image.height)
			);
			
			rect = new Rect (
				rect.x,
				rect.y,
				rect.width + s_Content.m_PasteboardIcon.image.width + Styles.kIconSpacing,
				Mathf.Max (rect.height, s_Content.m_PasteboardIcon.image.height)
			);
			
			switch (Event.current.type)
			{
				case EventType.MouseDown:
					if (rect.Contains (Event.current.mousePosition))
					{
						GUIUtility.systemCopyBuffer = content;
						
						m_CopyLabelConfirmStart = EditorApplication.timeSinceStartup;
						m_CopyLabelConfirmAlpha = 1.0f;
						
						Event.current.Use ();
					}
				break;
				case EventType.Repaint:
					if (rect.Contains (Event.current.mousePosition))
					{
						GUI.tooltip = s_Content.m_CopyToClipboardMessage.text;
						Vector2 position = GUIUtility.GUIToScreenPoint (new Vector2 (rect.x, rect.y));
						GUI.s_ToolTipRect = new Rect (position.x, position.y, rect.width, rect.height);
					}
				
					if (m_CopyLabelConfirmAlpha > 0.0f && Event.current.type == EventType.Repaint)
					{
						rect = new Rect (rect.x, rect.y + rect.height - 1.0f, rect.width, rect.height);

						Color color = GUI.color;
						GUI.color = new Color (1.0f, 1.0f, 1.0f, m_CopyLabelConfirmAlpha);
						EditorStyles.miniLabel.Draw (rect, new GUIContent (copyMessage), false, false, false, false);
						GUI.color = color;
					}
				break;
			}
		GUILayout.EndHorizontal ();
	}
	
	
	void Update ()
	// Handle fading of the CopyLabel "did copy" message
	{
		if (m_CopyLabelConfirmStart != 0.0)
		{
			if (EditorApplication.timeSinceStartup - m_CopyLabelConfirmStart > Styles.kCopyLabelConfirmDelay)
			{
				m_CopyLabelConfirmAlpha = 1.0f - (float)(EditorApplication.timeSinceStartup - m_CopyLabelConfirmStart - Styles.kCopyLabelConfirmDelay);
				
				if (m_CopyLabelConfirmAlpha <= 0.0f)
				{
					m_CopyLabelConfirmAlpha = 0.0f;
					m_CopyLabelConfirmStart = 0.0;
				}
				
				Repaint ();
			}
		}
	}
	
	
	void OnGUI ()
	{
		if (s_Styles == null)
			s_Styles = new Styles ();
		if (s_Content == null)
			s_Content = new Content ();

		GUILayout.BeginArea (new Rect (Styles.kWindowPadding, Styles.kWindowPadding, Screen.width - Styles.kWindowPadding * 2.0f, Screen.height - Styles.kWindowPadding * 2.0f));
			GUILayout.BeginHorizontal ();
				Rect rect = GUILayoutUtility.GetRect (Styles.kLogoAreaWidth, Styles.kLogoAreaHeight);
				if (Event.current.type == EventType.Repaint)
				{
					GUI.DrawTexture (new Rect (rect.x, rect.y, s_Content.m_UDNLogo.image.width, s_Content.m_UDNLogo.image.height), s_Content.m_UDNLogo.image);
				}
				
				GUILayout.BeginVertical ();
					if (m_Success)
					{
						OnSuccessGUI ();
					}
					else
					{
						OnFailureGUI ();
					}
				GUILayout.EndVertical ();
			GUILayout.EndHorizontal ();
		GUILayout.EndArea ();

	}
	
	
	void OnFailureGUI ()
	{
		// Title //
		GUILayout.Label (s_Content.m_TextHeaderFailure.text, EditorStyles.boldLabel);
		
		if (m_RecoverableError)
		// Recoverable error - static text followed by service error message
		{
			GUILayout.Label (s_Content.m_MainTextFailureRecoverable.text + ":", EditorStyles.wordWrappedLabel);
			GUILayout.Label (m_ErrorMessage, EditorStyles.wordWrappedLabel);
		}
		else
		// Critical error
		{
			// Static text and service error message //
			GUILayout.Label (s_Content.m_MainTextFailureCritical.text + ":", EditorStyles.wordWrappedLabel);
			GUILayout.Label (m_ErrorMessage, EditorStyles.wordWrappedLabel);
			
			GUILayout.BeginHorizontal ();
				GUILayout.Label (s_Content.m_FailurePreLinkAssistText.text, GUILayout.ExpandWidth (false));
					// Resolution text before the UDN link
				
				if (LinkLabel (s_Content.m_UDNStatusLinkText))
				// UDN status link
				{
					Application.OpenURL (Content.kUDNStatusURL);
				}
			GUILayout.EndHorizontal ();
			GUILayout.Space (-8.0f);
			GUILayout.Label (s_Content.m_FailurePostLinkAssistText);
				// Resolution text after the UDN link
		}
		
		GUILayout.FlexibleSpace ();
		
		GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			
			if (m_RecoverableError)
			// With recoverable errors do Cancel/Resolve
			{
				if (GUILayout.Button (s_Content.m_CancelButton, GUILayout.Width (Styles.kBottomButtonWidth), GUILayout.Height (Styles.kBottomButtonHeight)))
				{
					Close ();
				}
				
				if (GUILayout.Button (s_Content.m_FixButton, GUILayout.Width (Styles.kBottomButtonWidth), GUILayout.Height (Styles.kBottomButtonHeight)))
				{
					Application.OpenURL (Content.kUDNPlayerControlPanelURL);
					Close ();
				}
			}
			else
			// With critical errors do Close
			{
				if (GUILayout.Button (s_Content.m_CloseButton.text, GUILayout.Width (Styles.kBottomButtonWidth), GUILayout.Height (Styles.kBottomButtonHeight)))
				{
					Close ();
				}
			}
		GUILayout.EndHorizontal ();
	}
	
	
	void OnSuccessGUI ()
	{
		// Title and text //
		GUILayout.Label (s_Content.m_TextHeaderSuccess.text, EditorStyles.boldLabel);
		GUILayout.Label (s_Content.m_MainTextSuccess.text, EditorStyles.wordWrappedLabel);
		GUILayout.Space (Styles.kTextPaddingBottom);
		
		// URL copy area //
		CopyLabel (m_ShortURL, s_Content.m_DidCopyToClipboardMessage.text);
		GUILayout.FlexibleSpace ();
		
		// Open in browser & social network share buttons //
		GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			
			if (LinkLabel (s_Content.m_OpenInBrowser))
			{
				Application.OpenURL (m_URL);
			}
			
			GUILayout.Space (Styles.kIconSpacing);
			
			if (LinkLabel (s_Content.m_Tweet))
			{
				Application.OpenURL (GetShareURL (m_ShortURL, s_Content.m_ShareMessage.text, SocialNetwork.Twitter));
			}
			
			GUILayout.Space (Styles.kIconSpacing);
			
			if (LinkLabel (s_Content.m_LinkedInShare))
			{
				Application.OpenURL (GetShareURL (m_ShortURL, s_Content.m_ShareMessage.text, SocialNetwork.LinkedIn));
			}
			
			GUILayout.Space (Styles.kIconSpacing);
			
			if (LinkLabel (s_Content.m_FacebookShare))
			{
				Application.OpenURL (GetShareURL (m_ShortURL, s_Content.m_ShareMessage.text, SocialNetwork.Facebook));
			}
			
			/*GUILayout.Space (Styles.kIconSpacing);
			if (LinkLabel (s_Content.m_GooglePlusShare))
			// TODO: Implement this when Google get their stuff together and add the ability to share on G+ in any way
			{
				Application.OpenURL (GetShareURL (m_ShortURL, s_Content.m_ShareMessage.text, SocialNetwork.GooglePlus));
			}*/
			/*GUILayout.Space (Styles.kIconSpacing);
			if (LinkLabel (s_Content.m_UDNShare))
			// TODO: Implement this when we offer a "share on UDN" feature
			{
				Application.OpenURL (GetShareURL (m_ShortURL, s_Content.m_ShareMessage.text, SocialNetwork.UnityDeveloperNetwork));
			}*/
			GUILayout.Space (Styles.kIconSpacingRight);
		GUILayout.EndHorizontal ();
		
		// Close button //
		GUILayout.FlexibleSpace ();
		GUILayout.BeginHorizontal ();
			GUILayout.FlexibleSpace ();
			if (GUILayout.Button (s_Content.m_CloseButton.text, GUILayout.Width (Styles.kBottomButtonWidth), GUILayout.Height (Styles.kBottomButtonHeight)))
			{
				Close ();
			}
		GUILayout.EndHorizontal ();
	}
}

}
