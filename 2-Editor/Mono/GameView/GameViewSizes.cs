using UnityEngine;
using UnityEditorInternal;
using System;

namespace UnityEditor
{

public enum GameViewSizeGroupType
{
	Standalone,
	WebPlayer,
	iOS,
	Android,
	Wii,
	PS3,
	Xbox360,
	BB10,
	Tizen,
	#if INCLUDE_WP8SUPPORT
	WP8,
	#endif
}

[FilePathAttribute ( "GameViewSizes.asset", FilePathAttribute.Location.PreferencesFolder)]
internal class GameViewSizes : ScriptableSingleton<GameViewSizes>
{
	// Written out to make it easy to find in text file (instead of an array)
	[SerializeField] GameViewSizeGroup m_Standalone = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_WebPlayer = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_iOS = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_Android = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_XBox360 = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_PS3 = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_Wii = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_BB10 = new GameViewSizeGroup ();
	[SerializeField] GameViewSizeGroup m_Tizen = new GameViewSizeGroup ();
	#if INCLUDE_WP8SUPPORT
	[SerializeField] GameViewSizeGroup m_WP8 = new GameViewSizeGroup ();
	#endif

	[NonSerialized] GameViewSize m_Remote = null;
	[NonSerialized] Vector2 m_LastStandaloneScreenSize = new Vector2 (-1, -1);
	[NonSerialized] Vector2 m_LastWebPlayerScreenSize = new Vector2 (-1, -1);
	[NonSerialized] Vector2 m_LastRemoteScreenSize = new Vector2 (-1, -1);
	[NonSerialized] int m_ChangeID = 0;
	[NonSerialized] static GameViewSizeGroupType s_GameViewSizeGroupType;

	public GameViewSizeGroupType currentGroupType
	{
		get { return s_GameViewSizeGroupType; }
	}

	public GameViewSizeGroup currentGroup 
	{
		get {return GetGroup (s_GameViewSizeGroupType);}
	}

	static GameViewSizes ()
	{
		RefreshGameViewSizeGroupType ();
		EditorUserBuildSettings.activeBuildTargetChanged += delegate () { RefreshGameViewSizeGroupType (); };
	}

	public GameViewSizeGroup GetGroup (GameViewSizeGroupType gameViewSizeGroupType)
	{
		InitBuiltinGroups ();
		switch (gameViewSizeGroupType)
		{
			case GameViewSizeGroupType.Standalone:
				return m_Standalone;
			case GameViewSizeGroupType.WebPlayer:
				return m_WebPlayer;
			case GameViewSizeGroupType.iOS:
				return m_iOS;
			case GameViewSizeGroupType.Android:
				return m_Android;
			case GameViewSizeGroupType.PS3:
				return m_PS3;
			case GameViewSizeGroupType.Xbox360:
				return m_XBox360;
			case GameViewSizeGroupType.Wii:
				return m_Wii;
			case GameViewSizeGroupType.BB10:
				return m_BB10;
			case GameViewSizeGroupType.Tizen:
				return m_Tizen;
			#if INCLUDE_WP8SUPPORT
			case GameViewSizeGroupType.WP8:
				return m_WP8;
			#endif
			default:
				Debug.LogError ("Unhandled group enum! " + gameViewSizeGroupType);
				break;
		}
		return m_Standalone;
	}

	public void SaveToHDD ()
	{
		bool saveAsText = true;
		Save (saveAsText);
	}

	public bool IsDefaultStandaloneScreenSize (GameViewSizeGroupType gameViewSizeGroupType, int index)
	{
		return gameViewSizeGroupType == GameViewSizeGroupType.Standalone && GetDefaultStandaloneIndex () == index;
	}

	public bool IsDefaultWebPlayerScreenSize (GameViewSizeGroupType gameViewSizeGroupType, int index)
	{
		return gameViewSizeGroupType == GameViewSizeGroupType.WebPlayer && GetDefaultWebPlayerIndex () == index;
	}

	public bool IsRemoteScreenSize (GameViewSizeGroupType gameViewSizeGroupType, int index)
	{
		return GetGroup(gameViewSizeGroupType).IndexOf(m_Remote) == index;
	}

	public int GetDefaultStandaloneIndex ()
	{
		return m_Standalone.GetBuiltinCount () - 1;
	}

	public int GetDefaultWebPlayerIndex ()
	{
		return m_WebPlayer.GetBuiltinCount () - 1;
	}

	// returns true if screen size was changed
	public void RefreshStandaloneAndWebplayerDefaultSizes ()
	{
		if (InternalEditorUtility.defaultScreenWidth != m_LastStandaloneScreenSize.x ||
			InternalEditorUtility.defaultScreenHeight != m_LastStandaloneScreenSize.y)
		{
			m_LastStandaloneScreenSize = new Vector2 (InternalEditorUtility.defaultScreenWidth,
				                                        InternalEditorUtility.defaultScreenHeight);
			RefreshStandaloneDefaultScreenSize ((int)m_LastStandaloneScreenSize.x, (int)m_LastStandaloneScreenSize.y);
		}

		if (InternalEditorUtility.defaultWebScreenWidth != m_LastWebPlayerScreenSize.x ||
			InternalEditorUtility.defaultWebScreenHeight != m_LastWebPlayerScreenSize.y)
		{
			m_LastWebPlayerScreenSize = new Vector2 (InternalEditorUtility.defaultWebScreenWidth,
				                                        InternalEditorUtility.defaultWebScreenHeight);
			RefreshWebPlayerDefaultScreenSize ((int)m_LastWebPlayerScreenSize.x, (int)m_LastWebPlayerScreenSize.y);
		}

		if (InternalEditorUtility.remoteScreenWidth != m_LastRemoteScreenSize.x ||
			InternalEditorUtility.remoteScreenHeight != m_LastRemoteScreenSize.y)
		{
			m_LastRemoteScreenSize = new Vector2 (InternalEditorUtility.remoteScreenWidth,
													InternalEditorUtility.remoteScreenHeight);
			RefreshRemoteScreenSize ((int)m_LastRemoteScreenSize.x, (int)m_LastRemoteScreenSize.y);
		}
	}

	public void RefreshStandaloneDefaultScreenSize (int width, int height)
	{
		GameViewSize gvs = m_Standalone.GetGameViewSize (GetDefaultStandaloneIndex ());
		gvs.height = height;
		gvs.width = width;
		Changed ();
	}

	public void RefreshWebPlayerDefaultScreenSize (int width, int height)
	{
		GameViewSize gvs = m_WebPlayer.GetGameViewSize (GetDefaultWebPlayerIndex ());
		gvs.height = height;
		gvs.width = width;
		Changed ();
	}

	public void RefreshRemoteScreenSize (int width, int height)
	{
		m_Remote.width = width;
		m_Remote.height = height;
		if (width > 0 && height > 0)
			m_Remote.baseText = "Remote";
		else
			m_Remote.baseText = "Remote (Not Connected)";
		Changed ();
	}

	public void Changed()
	{
		m_ChangeID++;
	}

	public int GetChangeID()
	{
		return m_ChangeID;
	}

	private void InitBuiltinGroups ()
	{
		bool isInitialized = m_Standalone.GetBuiltinCount () > 0;
		if (isInitialized)
			return;

		m_Remote = new GameViewSize (GameViewSizeType.FixedResolution, 0, 0, "Remote (Not Connected)");

		// Shared
		GameViewSize kFree = new GameViewSize (GameViewSizeType.AspectRatio, 0, 0, "Free Aspect");
		GameViewSize k5_4 = new GameViewSize (GameViewSizeType.AspectRatio, 5, 4, "");
		GameViewSize k4_3 = new GameViewSize (GameViewSizeType.AspectRatio, 4, 3, "");
		GameViewSize k3_2 = new GameViewSize (GameViewSizeType.AspectRatio, 3, 2, "");
		GameViewSize k16_10 = new GameViewSize (GameViewSizeType.AspectRatio, 16, 10, "");
		GameViewSize k16_9 = new GameViewSize (GameViewSizeType.AspectRatio, 16, 9, "");
		GameViewSize kStandalone = new GameViewSize (GameViewSizeType.FixedResolution, 0, 0, "Standalone");
		GameViewSize kWeb = new GameViewSize (GameViewSizeType.FixedResolution, 0, 0, "Web");

		// iOS
		GameViewSize k_iPhoneTall = new GameViewSize (GameViewSizeType.FixedResolution, 320, 480, "iPhone Tall");
		GameViewSize k_iPhoneWide = new GameViewSize (GameViewSizeType.FixedResolution, 480, 320, "iPhone Wide");
		GameViewSize k_iPhone4GTall = new GameViewSize (GameViewSizeType.FixedResolution, 640, 960, "iPhone 4 Tall");
		GameViewSize k_iPhone4GWide = new GameViewSize (GameViewSizeType.FixedResolution, 960, 640, "iPhone 4 Wide");
		GameViewSize k_iPadTall = new GameViewSize (GameViewSizeType.FixedResolution, 768, 1024, "iPad Tall");
		GameViewSize k_iPadWide = new GameViewSize (GameViewSizeType.FixedResolution, 1024, 768, "iPad Wide");
		GameViewSize k_iPhone5Tall = new GameViewSize (GameViewSizeType.AspectRatio, 9, 16, "iPhone 5 Tall");
		GameViewSize k_iPhone5Wide = new GameViewSize (GameViewSizeType.AspectRatio, 16, 9, "iPhone 5 Wide");
		GameViewSize k_iPhoneTall2_3 = new GameViewSize (GameViewSizeType.AspectRatio, 2, 3, "iPhone Tall");
		GameViewSize k_iPhoneWide3_2 = new GameViewSize (GameViewSizeType.AspectRatio, 3, 2, "iPhone Wide");
		GameViewSize k_iPadTall3_4 = new GameViewSize (GameViewSizeType.AspectRatio, 3, 4, "iPad Tall");
		GameViewSize k_iPadWide4_3 = new GameViewSize (GameViewSizeType.AspectRatio, 4, 3, "iPad Wide");

		// Android
		GameViewSize k_HVGA_Portrait = new GameViewSize (GameViewSizeType.FixedResolution, 320, 480, "HVGA Portrait");
		GameViewSize k_HVGA_Landscape = new GameViewSize (GameViewSizeType.FixedResolution, 480, 320, "HVGA Landscape");
		GameViewSize k_WVGA_Portrait = new GameViewSize (GameViewSizeType.FixedResolution, 480, 800, "WVGA Portrait");
		GameViewSize k_WVGA_Landscape = new GameViewSize (GameViewSizeType.FixedResolution, 800, 480, "WVGA Landscape");
		GameViewSize k_FWVGA_Portrait = new GameViewSize (GameViewSizeType.FixedResolution, 480, 854, "FWVGA Portrait");
		GameViewSize k_FWVGA_Landscape = new GameViewSize (GameViewSizeType.FixedResolution, 854, 480, "FWVGA Landscape");
		GameViewSize k_WSVGA_Portrait = new GameViewSize (GameViewSizeType.FixedResolution, 600, 1024, "WSVGA Portrait");
		GameViewSize k_WSVGA_Landscape = new GameViewSize (GameViewSizeType.FixedResolution, 1024, 600, "WSVGA Landscape");
		GameViewSize k_WXGA_Portrait = new GameViewSize (GameViewSizeType.FixedResolution, 800, 1280, "WXGA Portrait");
		GameViewSize k_WXGA_Landscape = new GameViewSize (GameViewSizeType.FixedResolution, 1280, 800, "WXGA Landscape");
		GameViewSize k_3_2_Portrait = new GameViewSize (GameViewSizeType.AspectRatio, 2, 3, "3:2 Portrait");
		GameViewSize k_3_2_Landscape = new GameViewSize (GameViewSizeType.AspectRatio, 3, 2, "3:2 Landscape");
		GameViewSize k_16_10_Portrait = new GameViewSize (GameViewSizeType.AspectRatio, 10, 16, "16:10 Portrait");
		GameViewSize k_16_10_Landscape = new GameViewSize (GameViewSizeType.AspectRatio, 16, 10, "16:10 Landscape");

		// XBox360
		GameViewSize kXbox360_720p_169 = new GameViewSize (GameViewSizeType.FixedResolution, 1280, 720, "720p (16:9)");

		// PS3
		GameViewSize kPS3_1080p_169 = new GameViewSize (GameViewSizeType.FixedResolution, 1920, 1080, "1080p (16:9)");
		GameViewSize kPS3_720p_169 = new GameViewSize (GameViewSizeType.FixedResolution, 1280, 720, "720p (16:9)");
		GameViewSize kPS3_576p_43 = new GameViewSize (GameViewSizeType.FixedResolution, 720, 576, "576p (4:3)");
		GameViewSize kPS3_576p_169 = new GameViewSize (GameViewSizeType.FixedResolution, 1024, 576, "576p (16:9)");
		GameViewSize kPS3_480p_43 = new GameViewSize (GameViewSizeType.FixedResolution, 640, 480, "480p (4:3)");
		GameViewSize kPS3_480p_169 = new GameViewSize (GameViewSizeType.FixedResolution, 853, 480, "480p (16:9)");

		// Wii
		GameViewSize kWii4_3 = new GameViewSize(GameViewSizeType.FixedResolution, 608, 456, "Wii 4:3");
		GameViewSize kWii16_9 = new GameViewSize(GameViewSizeType.FixedResolution, 832, 456, "Wii 16:9");
			
		// BB10 
		GameViewSize k_BBTouchPortrait = new GameViewSize (GameViewSizeType.FixedResolution, 720, 1280, "Touch Phone Portrait");
		GameViewSize k_BBTouchLandscape = new GameViewSize (GameViewSizeType.FixedResolution, 1280, 720, "Touch Phone Landscape");
		GameViewSize k_BBKeyboard = new GameViewSize (GameViewSizeType.FixedResolution, 720, 720, "Keyboard Phone");
		GameViewSize k_BBTabletPortrait = new GameViewSize (GameViewSizeType.FixedResolution, 600, 1024, "Playbook Portrait");
		GameViewSize k_BBTabletLandscape = new GameViewSize (GameViewSizeType.FixedResolution, 1024, 600, "Playbook Landscape");
		GameViewSize k_9_16_Portrait = new GameViewSize (GameViewSizeType.AspectRatio, 9, 16, "9:16 Portrait");
		GameViewSize k_16_9_Landscape = new GameViewSize (GameViewSizeType.AspectRatio, 16, 9, "16:9 Landscape");
		GameViewSize k_1_1 = new GameViewSize (GameViewSizeType.AspectRatio, 1, 1, "1:1");

		// Tizen
		GameViewSize k_Tizen_720p_16_9 = new GameViewSize (GameViewSizeType.FixedResolution, 1280, 720, "16:9 Landscape");
		GameViewSize k_Tizen_720p_9_16 = new GameViewSize (GameViewSizeType.FixedResolution, 720, 1280, "9:16 Portrait");

		// WP8
		#if INCLUDE_WP8SUPPORT
		GameViewSize kWP8_WVGA_Portrait = new GameViewSize(GameViewSizeType.FixedResolution, 480, 800, "WVGA Portrait");
		GameViewSize kWP8_WVGA_Landscape = new GameViewSize(GameViewSizeType.FixedResolution, 800, 480, "WVGA Landscape");
		GameViewSize kWP8_WXGA_Portrait = new GameViewSize(GameViewSizeType.FixedResolution, 768, 1280, "WXGA Portrait");
		GameViewSize kWP8_WXGA_Landscape = new GameViewSize(GameViewSizeType.FixedResolution, 1280, 768, "WXGA Landscape");
		GameViewSize kWP8_720p_Portrait = new GameViewSize(GameViewSizeType.FixedResolution, 720, 1280, "720p Portrait");
		GameViewSize kWP8_720p_Landscape = new GameViewSize(GameViewSizeType.FixedResolution, 1280, 720, "720p Landscape");
		GameViewSize kWP8_15_9_Portrait = new GameViewSize(GameViewSizeType.AspectRatio, 9, 15, "WVGA Portrait");
		GameViewSize kWP8_15_9_Landscape = new GameViewSize(GameViewSizeType.AspectRatio, 15, 9, "WVGA Landscape");
		GameViewSize kWP8_5_3_Portrait = new GameViewSize(GameViewSizeType.AspectRatio, 9, 15, "WXGA Portrait");
		GameViewSize kWP8_5_3_Landscape = new GameViewSize(GameViewSizeType.AspectRatio, 15, 9, "WXGA Landscape");
		GameViewSize kWP8_16_9_Portrait = new GameViewSize(GameViewSizeType.AspectRatio, 9, 16, "720p Portrait");
		GameViewSize kWP8_16_9_Landscape = new GameViewSize(GameViewSizeType.AspectRatio, 16, 9, "720p Landscape");

		m_WP8.AddBuiltinSizes(kFree,
			kWP8_WVGA_Portrait, kWP8_15_9_Portrait,
			kWP8_WVGA_Landscape, kWP8_15_9_Landscape,
			kWP8_WXGA_Portrait, kWP8_5_3_Portrait,
			kWP8_WXGA_Landscape, kWP8_5_3_Landscape,
			kWP8_720p_Portrait, kWP8_16_9_Portrait,
			kWP8_720p_Landscape, kWP8_16_9_Landscape);
		#endif

		m_Standalone.AddBuiltinSizes (kFree, k5_4, k4_3, k3_2, k16_10, k16_9, kStandalone);
		m_WebPlayer.AddBuiltinSizes (kFree, k5_4, k4_3, k3_2, k16_10, k16_9, kWeb);
		m_Wii.AddBuiltinSizes (kFree, k4_3, k16_9, kWii4_3, kWii16_9);
		m_PS3.AddBuiltinSizes (kFree, k4_3, k16_9, k16_10, kPS3_1080p_169, kPS3_720p_169, kPS3_576p_43, kPS3_576p_169,
			                    kPS3_480p_43, kPS3_480p_169);
		m_XBox360.AddBuiltinSizes (kFree, k4_3, k16_9, k16_10, kXbox360_720p_169);
		m_iOS.AddBuiltinSizes (kFree,
			                    k_iPhoneTall, k_iPhoneWide,
			                    k_iPhone4GTall, k_iPhone4GWide,
			                    k_iPadTall, k_iPadWide,
			                    k_iPhone5Tall, k_iPhone5Wide,
			                    k_iPhoneTall2_3, k_iPhoneWide3_2,
			                    k_iPadTall3_4, k_iPadWide4_3);
		m_Android.AddBuiltinSizes (kFree, m_Remote, 
			                        k_HVGA_Portrait, k_HVGA_Landscape,
			                        k_WVGA_Portrait, k_WVGA_Landscape,
			                        k_FWVGA_Portrait, k_FWVGA_Landscape,
			                        k_WSVGA_Portrait, k_WSVGA_Landscape,
			                        k_WXGA_Portrait, k_WXGA_Landscape,
			                        k_3_2_Portrait, k_3_2_Landscape,
			                        k_16_10_Portrait, k_16_10_Landscape);
		m_BB10.AddBuiltinSizes(kFree,
				k_BBTouchPortrait, k_BBTouchLandscape,
				k_BBKeyboard,
				k_BBTabletPortrait, k_BBTabletLandscape,
				k_9_16_Portrait, k_16_9_Landscape,
				k_1_1);
		m_Tizen.AddBuiltinSizes(kFree,
				k_Tizen_720p_16_9,
				k_Tizen_720p_9_16);
	}

	private static void RefreshDerivedGameViewSize (GameViewSizeGroupType groupType, int gameViewSizeIndex, GameViewSize gameViewSize)
	{
		if (GameViewSizes.instance.IsDefaultStandaloneScreenSize (groupType, gameViewSizeIndex))
		{
			gameViewSize.width = (int)InternalEditorUtility.defaultScreenWidth;
			gameViewSize.height = (int)InternalEditorUtility.defaultScreenHeight;
		}
		else if (GameViewSizes.instance.IsDefaultWebPlayerScreenSize (groupType, gameViewSizeIndex))
		{
			gameViewSize.width = (int)InternalEditorUtility.defaultWebScreenWidth;
			gameViewSize.height = (int)InternalEditorUtility.defaultWebScreenHeight;
		}
		else if (GameViewSizes.instance.IsRemoteScreenSize (groupType, gameViewSizeIndex))
		{
			if (InternalEditorUtility.remoteScreenWidth <= 0 || InternalEditorUtility.remoteScreenHeight <= 0)
			{
				gameViewSize.sizeType = GameViewSizeType.AspectRatio;
				gameViewSize.width = gameViewSize.height = 0; // Free aspect if invalid remote width or height
			}
			else
			{
				gameViewSize.sizeType = GameViewSizeType.FixedResolution;
				gameViewSize.width = (int)InternalEditorUtility.remoteScreenWidth;
				gameViewSize.height =(int)InternalEditorUtility.remoteScreenHeight;			
			}
		}
	}

	public static Rect GetConstrainedRect (Rect startRect, GameViewSizeGroupType groupType, int gameViewSizeIndex)
	{
		Rect constrainedRect = startRect;
		GameViewSize gameViewSize = GameViewSizes.instance.GetGroup (groupType).GetGameViewSize (gameViewSizeIndex);
		RefreshDerivedGameViewSize (groupType, gameViewSizeIndex, gameViewSize);

		if (gameViewSize.isFreeAspectRatio)
		{
			return startRect;
		}

		float newRatio=0;
		bool useRatio;
		switch (gameViewSize.sizeType)
		{
			case GameViewSizeType.AspectRatio:
				{
					newRatio = gameViewSize.aspectRatio;
					useRatio = true;
				}
				break;
			case GameViewSizeType.FixedResolution:
				{
					if (gameViewSize.height > startRect.height || gameViewSize.width > startRect.width)
					{
						newRatio = gameViewSize.aspectRatio;
						useRatio = true;
					}
					else
					{
						constrainedRect.height = gameViewSize.height;
						constrainedRect.width = gameViewSize.width;
						useRatio = false;
					}				
				}
				break;
			default:
				Debug.LogError("Unhandled enum");
				return startRect;
		}

		if (useRatio)
		{
			constrainedRect.height = (constrainedRect.width / newRatio) > startRect.height
				                   ? (startRect.height)
				                   : (constrainedRect.width / newRatio);
			constrainedRect.width = (constrainedRect.height * newRatio);
		}

		// clamp
		constrainedRect.height = Mathf.Clamp (constrainedRect.height, 0f, startRect.height);
		constrainedRect.width = Mathf.Clamp (constrainedRect.width, 0f, startRect.width);

		// center
		constrainedRect.y = (startRect.height * 0.5f - constrainedRect.height * 0.5f) + startRect.y;
		constrainedRect.x = (startRect.width * 0.5f - constrainedRect.width * 0.5f) + startRect.x;
		return constrainedRect;
	}

	static void RefreshGameViewSizeGroupType()
	{
		BuildTargetGroup buildTargetGroup = BuildPipeline.GetBuildTargetGroup(EditorUserBuildSettings.activeBuildTarget);
		s_GameViewSizeGroupType = BuildTargetGroupToGameViewSizeGroup(buildTargetGroup);
	}

	public static GameViewSizeGroupType BuildTargetGroupToGameViewSizeGroup (BuildTargetGroup buildTargetGroup)
	{
		switch (buildTargetGroup)
		{
			case BuildTargetGroup.Standalone:
				return GameViewSizeGroupType.Standalone;

			case BuildTargetGroup.WebPlayer:
			case BuildTargetGroup.FlashPlayer:
				return GameViewSizeGroupType.WebPlayer;

			case BuildTargetGroup.Wii:
				return GameViewSizeGroupType.Wii;

			case BuildTargetGroup.PS3:
				return GameViewSizeGroupType.PS3;

			case BuildTargetGroup.XBOX360:
				return GameViewSizeGroupType.Xbox360;

			case BuildTargetGroup.iPhone:
				return GameViewSizeGroupType.iOS;

			case BuildTargetGroup.Android:
				return GameViewSizeGroupType.Android;
				
			case BuildTargetGroup.BB10:
				return GameViewSizeGroupType.BB10;

			case BuildTargetGroup.Tizen:
				return GameViewSizeGroupType.Tizen;

			#if INCLUDE_WP8SUPPORT
			case BuildTargetGroup.WP8:
				return GameViewSizeGroupType.WP8;
			#endif

			default:
				return GameViewSizeGroupType.Standalone;
		}
	}
}
}

// namespace
