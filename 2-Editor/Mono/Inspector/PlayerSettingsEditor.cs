using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using UnityEditor.Modules;

// ************************************* READ BEFORE EDITING **************************************
//
// DO NOT COPY/PASTE! Please do not have the same setting more than once in the code.
// If a setting for one platform needs to be exposed to more platforms,
// change the if statements so the same lines of code are executed for both platforms,
// instead of copying the lines into multiple code blocks.
// This ensures that if we change labels, or headers, or the order of settings, it will remain
// consistent without having to remember to make the same change multiple places. THANK YOU!

namespace UnityEditor
{
	[CustomEditor(typeof(PlayerSettings))]
	internal class PlayerSettingsEditor : Editor
	{
		class Styles
		{
			public GUIStyle thumbnail = "IN ThumbnailShadow";
			public GUIStyle thumbnailLabel = "IN ThumbnailSelection";
			public GUIStyle categoryBox = new GUIStyle (EditorStyles.helpBox);

			public GUIContent colorSpaceWarning = EditorGUIUtility.TextContent ("PlayerSettings.ActiveColorSpaceWarning");
			public GUIContent cursorHotspot = EditorGUIUtility.TextContent ("PlayerSettings.CursorHotspot");
			public GUIContent defaultCursor = EditorGUIUtility.TextContent ("PlayerSettings.DefaultCursor");
			public GUIContent defaultIcon = EditorGUIUtility.TextContent ("PlayerSettings.DefaultIcon");

			public Styles ()
			{
				categoryBox.padding.left = 14;
			}
		}

		private static Styles s_Styles;

		// Icon layout constants
		const int kSlotSize = 64;
		const int kMaxPreviewSize = 96;
		const int kIconSpacing = 6;

		// Template layout constants
		const string kWebPlayerTemplateDefaultIconResource = "BuildSettings.Web.Small";
		const float kWebPlayerTemplateGridPadding = 15.0f;
		const float kThumbnailSize = 80.0f;
		const float kThumbnailLabelHeight = 20.0f;
		const float kThumbnailPadding = 5.0f;

		// Section and tab selection state

		SavedInt m_SelectedSection = new SavedInt ("PlayerSettings.ShownSection", -1);

		BuildPlayerWindow.BuildPlatform[] validPlatforms;

		// iPhone and iPad
		SerializedProperty m_IPhoneBundleIdentifier;
		SerializedProperty m_IPhoneBundleVersion;
		SerializedProperty m_IPhoneApplicationDisplayName;
		SerializedProperty m_AndroidBundleVersionCode;
		SerializedProperty m_AndroidMinSdkVersion;
		SerializedProperty m_AndroidPreferredInstallLocation;

		SerializedProperty m_ApiCompatibilityLevel;
		SerializedProperty m_IPhoneStrippingLevel;
		SerializedProperty m_IPhoneScriptCallOptimization;
		SerializedProperty m_AotOptions;

		SerializedProperty m_DefaultScreenOrientation;
		SerializedProperty m_AllowedAutoRotateToPortrait;
		SerializedProperty m_AllowedAutoRotateToPortraitUpsideDown;
		SerializedProperty m_AllowedAutoRotateToLandscapeRight;
		SerializedProperty m_AllowedAutoRotateToLandscapeLeft;
		SerializedProperty m_UseOSAutoRotation;
		SerializedProperty m_Use32BitDisplayBuffer;
		SerializedProperty m_Use24BitDepthBuffer;
		SerializedProperty m_iosShowActivityIndicatorOnLoading;
		SerializedProperty m_androidShowActivityIndicatorOnLoading;

		SerializedProperty m_IPhoneSdkVersion;
		SerializedProperty m_IPhoneTargetOSVersion;
		SerializedProperty m_AndroidProfiler;
		SerializedProperty m_ForceInternetPermission;
		SerializedProperty m_ForceSDCardPermission;
		SerializedProperty m_CreateWallpaper;

		SerializedProperty m_UIPrerenderedIcon;
		SerializedProperty m_UIRequiresPersistentWiFi;
		SerializedProperty m_UIStatusBarHidden;
		SerializedProperty m_UIStatusBarStyle;

		SerializedProperty m_UIExitOnSuspend;

		SerializedProperty m_EnableHWStatistics;

		SerializedProperty m_IPhoneSplashScreen;
		SerializedProperty m_IPhoneHighResSplashScreen;
		SerializedProperty m_IPhoneTallHighResSplashScreen;

		SerializedProperty m_IPadPortraitSplashScreen;
		SerializedProperty m_IPadHighResPortraitSplashScreen;
		SerializedProperty m_IPadLandscapeSplashScreen;
		SerializedProperty m_IPadHighResLandscapeSplashScreen;

		SerializedProperty m_AndroidTargetDevice;
		SerializedProperty m_AndroidSplashScreenScale;
		SerializedProperty m_AndroidKeystoreName;
		SerializedProperty m_AndroidKeyaliasName;
		SerializedProperty m_APKExpansionFiles;

		SerializedProperty m_TargetDevice;
        SerializedProperty m_TargetGlesGraphics;
		SerializedProperty m_TargetResolution;
		SerializedProperty m_AccelerometerFrequency;
		SerializedProperty m_OverrideIPodMusic;
		SerializedProperty m_PrepareIOSForRecording;

		// Xbox 360
		SerializedProperty m_XboxTitleId;
		SerializedProperty m_XboxImageXexPath;
		SerializedProperty m_XboxSpaPath;
		SerializedProperty m_XboxGenerateSpa;
		SerializedProperty m_XboxDeployKinectResources;
		SerializedProperty m_XboxPIXTextureCapture;
		SerializedProperty m_XboxEnableAvatar;
		SerializedProperty m_XboxEnableKinect;
		SerializedProperty m_XboxEnableKinectAutoTracking;
        SerializedProperty m_XboxEnableHeadOrientation;
        SerializedProperty m_XboxDeployHeadOrientation;
        SerializedProperty m_XboxDeployKinectHeadPosition;
		SerializedProperty m_XboxSplashScreen;
        SerializedProperty m_XboxEnableSpeech;
	    SerializedProperty m_XboxSpeechDB;
        SerializedProperty m_XboxEnableFitness;
	    SerializedProperty m_XboxAdditionalTitleMemorySize;
		SerializedProperty m_XboxEnableGuest;

		// PS3
		SerializedProperty m_PS3TitleConfigPath;
		SerializedProperty m_PS3DLCConfigPath;
		SerializedProperty m_PS3ThumbnailPath;
		SerializedProperty m_PS3BackgroundPath;
		SerializedProperty m_PS3SoundPath;
		SerializedProperty m_PS3TrophyCommId;
		SerializedProperty m_PS3NpCommunicationPassphrase;
        SerializedProperty m_PS3TrophyPackagePath;
        SerializedProperty m_PS3BootCheckMaxSaveGameSizeKB;
        SerializedProperty m_PS3TrophyCommSig;
        SerializedProperty m_PS3TrialMode;
        SerializedProperty m_PS3SaveGameSlots;

		// Wii
		SerializedProperty m_WiiRegion;
		SerializedProperty m_WiiGameCode;
		SerializedProperty m_WiiGameVersion;
		SerializedProperty m_WiiCompanyCode;

		SerializedProperty m_WiiSupportsNunchuk;
		SerializedProperty m_WiiSupportsClassicController;
		SerializedProperty m_WiiSupportsBalanceBoard;
		SerializedProperty m_WiiSupportsMotionPlus;
		SerializedProperty m_WiiControllerCount;

		SerializedProperty m_WiiFloatingPointExceptions;
		SerializedProperty m_WiiScreenCrashDumps;

		SerializedProperty m_WiiHio2Usage;
		SerializedProperty m_WiiLoadingScreenRectPlacement;
		SerializedProperty m_WiiLoadingScreenPeriod;
		SerializedProperty m_WiiLoadingScreenFileName;
		SerializedProperty m_WiiLoadingScreenRect;
		SerializedProperty m_WiiLoadingScreenBackground;

		// Flash
		SerializedProperty m_FlashStrippingLevel;

        #if INCLUDE_METROSUPPORT

        // Metro
        SerializedProperty m_MetroPackageName;
        SerializedProperty m_MetroPackageLogo;
        SerializedProperty m_MetroPackageVersion;
        SerializedProperty m_MetroApplicationDescription;
        SerializedProperty m_MetroTileLogo;
        SerializedProperty m_MetroTileWideLogo;
        SerializedProperty m_MetroTileSmallLogo;
        SerializedProperty m_MetroTileShortName;
        SerializedProperty m_MetroTileBackgroundColor;
        SerializedProperty m_MetroSplashScreenImage;

        #endif

		// General
		SerializedProperty m_CompanyName;
		SerializedProperty m_ProductName;

		// Cursor
		SerializedProperty m_DefaultCursor;
		SerializedProperty m_CursorHotspot;

		// Screen
		SerializedProperty m_DefaultScreenWidth;
		SerializedProperty m_DefaultScreenHeight;
		SerializedProperty m_DefaultScreenWidthWeb;
		SerializedProperty m_DefaultScreenHeightWeb;

		SerializedProperty m_RenderingPath;
		SerializedProperty m_MobileRenderingPath;
		SerializedProperty m_ActiveColorSpace;
		SerializedProperty m_MTRendering;
		SerializedProperty m_MobileMTRendering;
		SerializedProperty m_StripUnusedMeshComponents;

		SerializedProperty m_DisplayResolutionDialog;
		SerializedProperty m_DefaultIsFullScreen;
		SerializedProperty m_DefaultIsNativeResolution;

		SerializedProperty m_UsePlayerLog;
		SerializedProperty m_ResizableWindow;
        SerializedProperty m_StripPhysics;
		SerializedProperty m_UseMacAppStoreValidation;
		SerializedProperty m_MacFullscreenMode;
		#if ENABLE_SINGLE_INSTANCE_BUILD_SETTING
		SerializedProperty m_ForceSingleInstance;
		#endif

		SerializedProperty m_RunInBackground;
		SerializedProperty m_CaptureSingleScreen;
		SerializedProperty m_ResolutionDialogBanner;

		SerializedProperty m_SupportedAspectRatios;

		SerializedProperty m_SkinOnGPU;

		SerializedProperty m_FirstStreamedLevelWithResources;
		SerializedProperty m_WebPlayerTemplate;

		bool		m_KeystoreCreate = false;
		string		m_KeystoreConfirm = "";
		string[]	m_KeystoreAvailableKeys = null;

		int selectedPlatform = 0;
		int scriptingDefinesControlID = 0;

        // Section animation state
        AnimValueManager m_Anims = new AnimValueManager();
        AnimBool[] m_SectionAnimators = new AnimBool[5];
        readonly AnimBool m_ShowDeferredWarning = new AnimBool();
        readonly AnimBool m_ShowDefaultIsNativeResolution = new AnimBool ();
        readonly AnimBool m_ShowResolution = new AnimBool ();
        private static Texture2D s_WarningIcon;

        private bool IsMobileTarget(BuildTargetGroup targetGroup)
        {
        	return 		targetGroup == BuildTargetGroup.iPhone
        			||  targetGroup == BuildTargetGroup.Android
        			||  targetGroup == BuildTargetGroup.BB10
        			||  targetGroup == BuildTargetGroup.Tizen;
        }

		public SerializedProperty FindPropertyAssert (string name)
		{
			SerializedProperty property = serializedObject.FindProperty(name);
			if (property == null)
				Debug.LogError("Failed to find:" + name);
			return property;
		}

		void OnEnable ()
		{
			validPlatforms = BuildPlayerWindow.GetValidPlatforms().ToArray();

			m_PS3TitleConfigPath			  = FindPropertyAssert("ps3TitleConfigPath");
			m_PS3DLCConfigPath				= FindPropertyAssert("ps3DLCConfigPath");
			m_PS3ThumbnailPath				= FindPropertyAssert("ps3ThumbnailPath");
			m_PS3BackgroundPath			   = FindPropertyAssert("ps3BackgroundPath");
			m_PS3SoundPath					= FindPropertyAssert("ps3SoundPath");
			m_PS3TrophyCommId				 = FindPropertyAssert("ps3TrophyCommId");
			m_PS3NpCommunicationPassphrase	= FindPropertyAssert("ps3NpCommunicationPassphrase");
			m_PS3TrophyPackagePath			= FindPropertyAssert("ps3TrophyPackagePath");
            m_PS3BootCheckMaxSaveGameSizeKB = FindPropertyAssert("ps3BootCheckMaxSaveGameSizeKB");
            m_PS3TrophyCommSig              = FindPropertyAssert("ps3TrophyCommSig");
            m_PS3TrialMode                  = FindPropertyAssert("ps3TrialMode");
            m_PS3SaveGameSlots              = FindPropertyAssert("ps3SaveGameSlots");

			m_IPhoneSdkVersion				= FindPropertyAssert("iPhoneSdkVersion");
			m_IPhoneTargetOSVersion		   = FindPropertyAssert("iPhoneTargetOSVersion");
			m_IPhoneStrippingLevel			= FindPropertyAssert("iPhoneStrippingLevel");

			m_IPhoneScriptCallOptimization	= FindPropertyAssert("iPhoneScriptCallOptimization");
			m_AndroidProfiler				= FindPropertyAssert("AndroidProfiler");
			m_ForceInternetPermission		= FindPropertyAssert("ForceInternetPermission");
			m_ForceSDCardPermission			= FindPropertyAssert("ForceSDCardPermission");
			m_CreateWallpaper				= FindPropertyAssert("CreateWallpaper");
			m_CompanyName					= FindPropertyAssert("companyName");
			m_ProductName					= FindPropertyAssert("productName");

			m_DefaultCursor					= FindPropertyAssert("defaultCursor");
			m_CursorHotspot					= FindPropertyAssert("cursorHotspot");

			m_UIPrerenderedIcon				= FindPropertyAssert("uIPrerenderedIcon");
			m_ResolutionDialogBanner		= FindPropertyAssert("resolutionDialogBanner");
			m_IPhoneSplashScreen			= FindPropertyAssert("iPhoneSplashScreen");
			m_IPhoneHighResSplashScreen		= FindPropertyAssert("iPhoneHighResSplashScreen");
			m_IPhoneTallHighResSplashScreen		= FindPropertyAssert("iPhoneTallHighResSplashScreen");
			m_IPadPortraitSplashScreen		= FindPropertyAssert("iPadPortraitSplashScreen");
			m_IPadHighResPortraitSplashScreen		= FindPropertyAssert("iPadHighResPortraitSplashScreen");
			m_IPadLandscapeSplashScreen		= FindPropertyAssert("iPadLandscapeSplashScreen");
			m_IPadHighResLandscapeSplashScreen		= FindPropertyAssert("iPadHighResLandscapeSplashScreen");

			m_AndroidSplashScreenScale		= FindPropertyAssert("AndroidSplashScreenScale");
			m_AndroidKeystoreName			= FindPropertyAssert("AndroidKeystoreName");
			m_AndroidKeyaliasName			= FindPropertyAssert("AndroidKeyaliasName");

			m_UIStatusBarHidden				= FindPropertyAssert("uIStatusBarHidden");
			m_UIStatusBarStyle				= FindPropertyAssert("uIStatusBarStyle");
			m_RenderingPath					= FindPropertyAssert("m_RenderingPath");
			m_MobileRenderingPath			= FindPropertyAssert("m_MobileRenderingPath");
			m_ActiveColorSpace				= FindPropertyAssert("m_ActiveColorSpace");
			m_MTRendering					= FindPropertyAssert("m_MTRendering");
			m_MobileMTRendering				= FindPropertyAssert("m_MobileMTRendering");
			m_StripUnusedMeshComponents		= FindPropertyAssert ("StripUnusedMeshComponents");
			m_FirstStreamedLevelWithResources = FindPropertyAssert("firstStreamedLevelWithResources");
			m_IPhoneBundleIdentifier		= FindPropertyAssert("iPhoneBundleIdentifier");
			m_IPhoneBundleVersion			= FindPropertyAssert("iPhoneBundleVersion");
			m_AndroidBundleVersionCode		= FindPropertyAssert("AndroidBundleVersionCode");
			m_AndroidMinSdkVersion			= FindPropertyAssert("AndroidMinSdkVersion");
			m_AndroidPreferredInstallLocation = FindPropertyAssert("AndroidPreferredInstallLocation");
			m_TargetResolution				= FindPropertyAssert("targetResolution");
			m_AccelerometerFrequency		= FindPropertyAssert("accelerometerFrequency");
			m_OverrideIPodMusic				= FindPropertyAssert("Override IPod Music");
			m_PrepareIOSForRecording		= FindPropertyAssert("Prepare IOS For Recording");
			m_UIRequiresPersistentWiFi		= FindPropertyAssert("uIRequiresPersistentWiFi");
			m_UIExitOnSuspend				= FindPropertyAssert("uIExitOnSuspend");
			m_EnableHWStatistics			= FindPropertyAssert("enableHWStatistics");
			m_AndroidTargetDevice			= FindPropertyAssert("AndroidTargetDevice");
			m_ApiCompatibilityLevel			= FindPropertyAssert("apiCompatibilityLevel");
			m_AotOptions		   			= FindPropertyAssert("aotOptions");
			m_APKExpansionFiles				= FindPropertyAssert("APKExpansionFiles");

			m_DefaultScreenWidth			= FindPropertyAssert("defaultScreenWidth");
			m_DefaultScreenHeight			= FindPropertyAssert("defaultScreenHeight");
			m_DefaultScreenWidthWeb			= FindPropertyAssert("defaultScreenWidthWeb");
			m_DefaultScreenHeightWeb		= FindPropertyAssert("defaultScreenHeightWeb");
			m_RunInBackground				= FindPropertyAssert("runInBackground");

			m_DefaultScreenOrientation				= FindPropertyAssert("defaultScreenOrientation");
			m_AllowedAutoRotateToPortrait			= FindPropertyAssert("allowedAutorotateToPortrait");
			m_AllowedAutoRotateToPortraitUpsideDown	= FindPropertyAssert("allowedAutorotateToPortraitUpsideDown");
			m_AllowedAutoRotateToLandscapeRight		= FindPropertyAssert("allowedAutorotateToLandscapeRight");
			m_AllowedAutoRotateToLandscapeLeft		= FindPropertyAssert("allowedAutorotateToLandscapeLeft");
			m_UseOSAutoRotation						= FindPropertyAssert("useOSAutorotation");
			m_Use32BitDisplayBuffer					= FindPropertyAssert("use32BitDisplayBuffer");
			m_Use24BitDepthBuffer					= FindPropertyAssert("use24BitDepthBuffer");
			m_iosShowActivityIndicatorOnLoading		= FindPropertyAssert("iosShowActivityIndicatorOnLoading");
			m_androidShowActivityIndicatorOnLoading	= FindPropertyAssert("androidShowActivityIndicatorOnLoading");

			m_DefaultIsFullScreen			= FindPropertyAssert("defaultIsFullScreen");
			m_DefaultIsNativeResolution		= FindPropertyAssert("defaultIsNativeResolution");
			m_CaptureSingleScreen			= FindPropertyAssert("captureSingleScreen");
			m_DisplayResolutionDialog		= FindPropertyAssert("displayResolutionDialog");
			m_SupportedAspectRatios			= FindPropertyAssert("m_SupportedAspectRatios");
			m_WebPlayerTemplate				= FindPropertyAssert("webPlayerTemplate");
            m_TargetGlesGraphics            = FindPropertyAssert("targetGlesGraphics");
			m_TargetDevice					= FindPropertyAssert("targetDevice");
			m_UsePlayerLog					= FindPropertyAssert("usePlayerLog");
			m_ResizableWindow				= FindPropertyAssert("resizableWindow");
			m_StripPhysics					= FindPropertyAssert("stripPhysics");
            m_UseMacAppStoreValidation      = FindPropertyAssert("useMacAppStoreValidation");
			m_MacFullscreenMode				= FindPropertyAssert("macFullscreenMode");
			m_SkinOnGPU						= FindPropertyAssert("gpuSkinning");
			#if ENABLE_SINGLE_INSTANCE_BUILD_SETTING
			m_ForceSingleInstance           = FindPropertyAssert("forceSingleInstance");
			#endif

			m_XboxTitleId					= FindPropertyAssert("XboxTitleId");
			m_XboxImageXexPath				= FindPropertyAssert("XboxImageXexPath");
			m_XboxSpaPath					= FindPropertyAssert("XboxSpaPath");
			m_XboxGenerateSpa				= FindPropertyAssert("XboxGenerateSpa");
			m_XboxDeployKinectResources		= FindPropertyAssert("XboxDeployKinectResources");
			m_XboxPIXTextureCapture         = FindPropertyAssert("xboxPIXTextureCapture");
			m_XboxEnableAvatar				= FindPropertyAssert("xboxEnableAvatar");
			m_XboxEnableKinect				= FindPropertyAssert("xboxEnableKinect");
			m_XboxEnableKinectAutoTracking	= FindPropertyAssert("xboxEnableKinectAutoTracking");
			m_XboxSplashScreen				= FindPropertyAssert("XboxSplashScreen");
            m_XboxEnableSpeech              = FindPropertyAssert("xboxEnableSpeech");
            m_XboxSpeechDB                  = FindPropertyAssert("xboxSpeechDB");
            m_XboxEnableFitness             = FindPropertyAssert("xboxEnableFitness");
            m_XboxAdditionalTitleMemorySize = FindPropertyAssert("xboxAdditionalTitleMemorySize");
            m_XboxEnableHeadOrientation     = FindPropertyAssert("xboxEnableHeadOrientation");
            m_XboxDeployHeadOrientation     = FindPropertyAssert("xboxDeployKinectHeadOrientation");
		    m_XboxDeployKinectHeadPosition  = FindPropertyAssert("xboxDeployKinectHeadPosition");
			m_XboxEnableGuest               = FindPropertyAssert("xboxEnableGuest");

			m_WiiSupportsNunchuk			= FindPropertyAssert("wiiSupportsNunchuk");
			m_WiiSupportsClassicController	= FindPropertyAssert("wiiSupportsClassicController");
			m_WiiSupportsBalanceBoard		= FindPropertyAssert("wiiSupportsBalanceBoard");
			m_WiiSupportsMotionPlus			= FindPropertyAssert("wiiSupportsMotionPlus");
			m_WiiControllerCount			= FindPropertyAssert("wiiControllerCount");
			m_WiiFloatingPointExceptions	= FindPropertyAssert("wiiFloatingPointExceptions");
			m_WiiScreenCrashDumps			= FindPropertyAssert("wiiScreenCrashDumps");
			m_WiiHio2Usage					= FindPropertyAssert("wiiHio2Usage");
			m_WiiLoadingScreenRectPlacement	= FindPropertyAssert("wiiLoadingScreenRectPlacement");
			m_WiiLoadingScreenPeriod		= FindPropertyAssert("wiiLoadingScreenPeriod");
			m_WiiLoadingScreenFileName		= FindPropertyAssert("wiiLoadingScreenFileName");
			m_WiiLoadingScreenRect			= FindPropertyAssert("wiiLoadingScreenRect");
			m_WiiLoadingScreenBackground	= FindPropertyAssert("wiiLoadingScreenBackground");

			m_FlashStrippingLevel			= FindPropertyAssert("flashStrippingLevel");

			#if INCLUDE_METROSUPPORT

			m_MetroPackageName = FindPropertyAssert("metroPackageName");
			m_MetroPackageName.stringValue = ValidateMetroPackageName(m_MetroPackageName.stringValue);
			m_MetroPackageLogo = FindPropertyAssert("metroPackageLogo");
			m_MetroPackageVersion = FindPropertyAssert("metroPackageVersion");
			m_MetroPackageVersion.stringValue = ValidateMetroPackageVersion(m_MetroPackageVersion.stringValue);
			m_MetroApplicationDescription = FindPropertyAssert("metroApplicationDescription");
			m_MetroApplicationDescription.stringValue = ValidateMetroApplicationDescription(m_MetroApplicationDescription.stringValue);
			m_MetroTileLogo = FindPropertyAssert("metroTileLogo");
			m_MetroTileWideLogo = FindPropertyAssert("metroTileWideLogo");
			m_MetroTileSmallLogo = FindPropertyAssert("metroTileSmallLogo");
			m_MetroTileShortName = FindPropertyAssert("metroTileShortName");
			m_MetroTileShortName.stringValue = ValidateMetroTileShortName(m_MetroTileShortName.stringValue);
			m_MetroTileBackgroundColor = FindPropertyAssert("metroTileBackgroundColor");
			m_MetroSplashScreenImage = FindPropertyAssert("metroSplashScreenImage");

			#endif

			foreach (BuildPlayerWindow.BuildPlatform nextPlatform in validPlatforms) {
				string module = ModuleManager.GetTargetStringFromBuildTargetGroup( nextPlatform.targetGroup );
				ISettingEditorExtension settingsExtension = ModuleManager.GetEditorSettingsExtension(module);
				if( settingsExtension != null )
					settingsExtension.OnEnable(this);
			}

			for (int i=0; i<m_SectionAnimators.Length; i++)
			{
				AnimBool ab = m_SectionAnimators[i] = new AnimBool();
				ab.value = (m_SelectedSection.value == i);
				m_Anims.Add(ab);
			}
			m_ShowDeferredWarning.value = (!InternalEditorUtility.HasPro() && PlayerSettings.renderingPath == RenderingPath.DeferredLighting);
			m_ShowDefaultIsNativeResolution.value = m_DefaultIsFullScreen.boolValue;
			m_ShowResolution.value = !(m_DefaultIsFullScreen.boolValue && m_DefaultIsNativeResolution.boolValue);
			m_Anims.Add(m_ShowDeferredWarning);
			m_Anims.Add (m_ShowDefaultIsNativeResolution);
			m_Anims.Add (m_ShowResolution);
		}

		void OnDisable ()
		{
			WebPlayerTemplate.ClearTemplates ();
		}

		public override bool UseDefaultMargins ()
		{
			return false;
		}

		public override void OnInspectorGUI()
		{
			if (s_Styles == null)
				s_Styles = new Styles();

			if (m_Anims.callback == null)
				m_Anims.callback = Repaint;

			serializedObject.Update();

			m_ShowDeferredWarning.target = (!InternalEditorUtility.HasPro() && PlayerSettings.renderingPath == RenderingPath.DeferredLighting);

			EditorGUILayout.BeginVertical (EditorStyles.inspectorDefaultMargins);
			{
				CommonSettings();
			}
			EditorGUILayout.EndVertical ();

			EditorGUILayout.Space ();

			EditorGUI.BeginChangeCheck ();
			int oldPlatform = selectedPlatform;
			selectedPlatform = EditorGUILayout.BeginPlatformGrouping(validPlatforms, null);
			if (EditorGUI.EndChangeCheck ())
			{
				// Awesome hackery to get string from delayed textfield when switching platforms
				if (EditorGUI.s_DelayedTextEditor.IsEditingControl (scriptingDefinesControlID))
				{
					EditorGUI.EndEditingActiveTextField ();
					GUIUtility.keyboardControl = 0;
					PlayerSettings.SetScriptingDefineSymbolsForGroup (validPlatforms[oldPlatform].targetGroup, EditorGUI.s_DelayedTextEditor.content.text);
				}
				// Reset focus when changing between platforms.
				// If we don't do this, the resolution width/height value will not update correctly when they have the focus
				GUI.FocusControl("");
			}
			GUILayout.Label("Settings for "+validPlatforms[selectedPlatform].title.text);

			// Compensate so settings inside boxes line up with settings at the top, though keep a minimum of 150.
			EditorGUIUtility.labelWidth = Mathf.Max (150, EditorGUIUtility.labelWidth - 8);

			BuildPlayerWindow.BuildPlatform platform = validPlatforms[selectedPlatform];
			BuildTargetGroup targetGroup = platform.targetGroup;

			string targetName = ModuleManager.GetTargetStringFromBuildTargetGroup(targetGroup);
			ISettingEditorExtension settingsExtension = ModuleManager.GetEditorSettingsExtension(targetName);

			ResolutionSectionGUI(targetGroup, settingsExtension);
			IconSectionGUI(targetGroup);
			SplashSectionGUI(platform, targetGroup, settingsExtension);
			OtherSectionGUI(platform, targetGroup, settingsExtension);
			PublishSectionGUI(targetGroup, settingsExtension);

			EditorGUILayout.EndPlatformGrouping();

			serializedObject.ApplyModifiedProperties ();
		}

		private void CommonSettings ()
		{
			EditorGUILayout.PropertyField(m_CompanyName);
			EditorGUILayout.PropertyField(m_ProductName);
			EditorGUILayout.Space();

			// Get icons and icon sizes for selected platform (or default)
			GUI.changed = false;
			string platformName = "";
			Texture2D[] icons = PlayerSettings.GetIconsForPlatform(platformName);
			int[] sizes = PlayerSettings.GetIconSizesForPlatform(platformName);
			// Ensure the default icon list is always populated correctly
			if (icons.Length != sizes.Length)
			{
				icons = new Texture2D[sizes.Length];
				PlayerSettings.SetIconsForPlatform(platformName, icons);
			}
			icons[0] = (Texture2D)EditorGUILayout.ObjectField(s_Styles.defaultIcon, icons[0], typeof(Texture2D), false);
			// Save changes
			if (GUI.changed)
				PlayerSettings.SetIconsForPlatform(platformName, icons);

			GUILayout.Space (3);

			m_DefaultCursor.objectReferenceValue = EditorGUILayout.ObjectField(s_Styles.defaultCursor, m_DefaultCursor.objectReferenceValue, typeof(Texture2D), false);

			Rect rect = EditorGUILayout.GetControlRect ();
			rect = EditorGUI.PrefixLabel (rect, 0, s_Styles.cursorHotspot);
			EditorGUI.PropertyField (rect, m_CursorHotspot, GUIContent.none);
		}

		private bool BeginSettingsBox (int nr, GUIContent header)
		{
			bool enabled = GUI.enabled;
			GUI.enabled = true; // we don't want to disable the expand behavior
			EditorGUILayout.BeginVertical(s_Styles.categoryBox);
			Rect r = GUILayoutUtility.GetRect(20, 18); r.x += 3; r.width += 6;
			bool expanded = GUI.Toggle(r, m_SelectedSection.value==nr, header, EditorStyles.inspectorTitlebarText);
			if (GUI.changed)
				m_SelectedSection.value = (expanded ? nr : -1);
			m_SectionAnimators[nr].target = expanded;
			GUI.enabled = enabled;
			return EditorGUILayout.BeginFadeGroup (m_SectionAnimators[nr].faded);
		}

		private void EndSettingsBox ()
		{
			EditorGUILayout.EndFadeGroup();
			EditorGUILayout.EndVertical();
		}

		private void ShowNoSettings ()
		{
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.NotApplicableForPlatform"), EditorStyles.miniLabel);
		}

		private void ShowSharedNote ()
		{
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.SharedSettingsFootnote"), EditorStyles.miniLabel);
		}

		private void IconSectionGUI (BuildTargetGroup targetGroup)
		{
			GUI.changed = false;
			if (BeginSettingsBox(1, EditorGUIUtility.TextContent("PlayerSettings.IconHeader")))
			{
				bool selectedDefault = (selectedPlatform < 0);
				{
					// Set default platform variables
					BuildPlayerWindow.BuildPlatform platform = null;
					targetGroup = BuildTargetGroup.Standalone;
					string platformName = "";

					// Override if a platform is selected
					if (!selectedDefault)
					{
						platform = validPlatforms[selectedPlatform];
						targetGroup = platform.targetGroup;
						platformName = platform.name;
					}

					bool enabled = GUI.enabled;

					// Icon for webplayer not supported (yet)
					// Icon for Xbox360 not supported
					if (targetGroup == BuildTargetGroup.XBOX360 || targetGroup == BuildTargetGroup.FlashPlayer || targetGroup == BuildTargetGroup.WebPlayer
						#if INCLUDE_WP8SUPPORT
						#endif
						#if INCLUDE_METROSUPPORT
						|| targetGroup == BuildTargetGroup.Metro
						#endif
					)
					{
						ShowNoSettings();
						EditorGUILayout.Space();
					}
					else
					{
						// Get icons and icon sizes for selected platform (or default)
						Texture2D[] icons = PlayerSettings.GetIconsForPlatform(platformName);
						int[] sizes = PlayerSettings.GetIconSizesForPlatform(platformName);

						bool overrideIcons = true;

						if (selectedDefault)
						{
							// Ensure the default icon list is always populated correctly
							if (icons.Length != sizes.Length)
							{
								icons = new Texture2D[sizes.Length];
								PlayerSettings.SetIconsForPlatform(platformName, icons);
							}
						}
						else
						{
							// If the list of icons for this platform is not empty (and has the correct size),
							// consider the icon overridden for this platform
							GUI.changed = false;
							overrideIcons = (icons.Length == sizes.Length);
							overrideIcons = GUILayout.Toggle (overrideIcons, "Override for " + platform.name);
							GUI.enabled = enabled && overrideIcons;
							if (GUI.changed || (!overrideIcons && icons.Length > 0))
							{
								// Set the list of icons to correct length if overridden, otherwise to an empty list
								if (overrideIcons)
									icons = new Texture2D[sizes.Length];
								else
									icons = new Texture2D[0];
								PlayerSettings.SetIconsForPlatform(platformName, icons);
							}
						}

						// Show the icons for this platform (or default)
						GUI.changed = false;
						for (int i=0; i<sizes.Length; i++) {
							int previewSize = Mathf.Min(kMaxPreviewSize, sizes[i]);
							Rect rect = GUILayoutUtility.GetRect(kSlotSize, Mathf.Max(kSlotSize, previewSize)+kIconSpacing);
							float width = Mathf.Min(rect.width, EditorGUIUtility.labelWidth + 4 + kSlotSize + kIconSpacing + kMaxPreviewSize);

							// Label
							string label = sizes[i]+"x"+sizes[i];
							GUI.Label(new Rect(rect.x, rect.y, width-kMaxPreviewSize-kSlotSize-2*kIconSpacing, 20), label);

							// Texture slot
							if (overrideIcons)
								icons[i] = (Texture2D)EditorGUI.ObjectField(
									new Rect(rect.x+width-kMaxPreviewSize-kSlotSize-kIconSpacing, rect.y, kSlotSize, kSlotSize),
									icons[i],
									typeof(Texture2D),
									false);

							// Preview
							Rect previewRect = new Rect(rect.x+width-kMaxPreviewSize, rect.y, previewSize, previewSize);
							Texture2D closestIcon = PlayerSettings.GetIconForPlatformAtSize(platformName, sizes[i]);
							if (closestIcon != null)
								GUI.DrawTexture(previewRect, closestIcon);
							else
								GUI.Box(previewRect, "");
						}
						// Save changes
						if (GUI.changed)
							PlayerSettings.SetIconsForPlatform(platformName, icons);
						GUI.enabled = enabled;

						if (targetGroup == BuildTargetGroup.iPhone)
						{
							EditorGUILayout.PropertyField(m_UIPrerenderedIcon, EditorGUIUtility.TextContent("PlayerSettings.UIPrerenderedIcon"));
							EditorGUILayout.Space();
						}

					}
				}
			}
			EndSettingsBox();
		}

		private void SplashSectionGUI (BuildPlayerWindow.BuildPlatform platform, BuildTargetGroup targetGroup, ISettingEditorExtension settingsExtension)
		{
			GUI.changed = false;
			if (BeginSettingsBox(2, EditorGUIUtility.TextContent("PlayerSettings.SplashHeader")))
			{
				// PLEASE DO NOT COPY SETTINGS TO APPEAR MULTIPLE PLACES IN THE CODE! See top of file for more info.

				{
					if (targetGroup == BuildTargetGroup.Standalone)
					{
						m_ResolutionDialogBanner.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.ResolutionDialogBanner"), (Texture2D)m_ResolutionDialogBanner.objectReferenceValue, typeof(Texture2D), false);
						EditorGUILayout.Space();
					}

					if (targetGroup == BuildTargetGroup.WebPlayer || targetGroup == BuildTargetGroup.FlashPlayer
						#if INCLUDE_WP8SUPPORT
						#endif
						#if INCLUDE_METROSUPPORT
						|| targetGroup == BuildTargetGroup.Metro
						#endif
					)
					{
						// @TODO: Implement
						//EditorGUILayout.ObjectField("Splash Image", null, typeof(Texture2D));
						ShowNoSettings();
						EditorGUILayout.Space();
					}

					if (targetGroup == BuildTargetGroup.XBOX360)
					{
						m_XboxSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.XboxSplashScreen"), (Texture2D)m_XboxSplashScreen.objectReferenceValue, typeof(Texture2D), false);
						EditorGUILayout.Space();
					}

					// iPhone, Android and BB10 splash screens (advanced only)
					EditorGUI.BeginDisabledGroup (!InternalEditorUtility.HasAdvancedLicenseOnBuildTarget(platform.DefaultTarget));
					if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android )
					{
						m_IPhoneSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPhoneSplashScreen"), (Texture2D)m_IPhoneSplashScreen.objectReferenceValue, typeof(Texture2D), false);
						EditorGUILayout.Space();
					}

					if (targetGroup == BuildTargetGroup.iPhone)
					{
						m_IPhoneHighResSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPhoneHighResSplashScreen"), (Texture2D)m_IPhoneHighResSplashScreen.objectReferenceValue, typeof(Texture2D), false);

						EditorGUILayout.Space();

						m_IPhoneTallHighResSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPhoneTallHighResSplashScreen"), (Texture2D)m_IPhoneTallHighResSplashScreen.objectReferenceValue, typeof(Texture2D), false);

						EditorGUILayout.Space();

						m_IPadPortraitSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPadPortraitSplashScreen"), (Texture2D)m_IPadPortraitSplashScreen.objectReferenceValue, typeof(Texture2D), false);

						EditorGUILayout.Space();

						m_IPadHighResPortraitSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPadHighResPortraitSplashScreen"), (Texture2D)m_IPadHighResPortraitSplashScreen.objectReferenceValue, typeof(Texture2D), false);

						EditorGUILayout.Space();

						m_IPadLandscapeSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPadLandscapeSplashScreen"), (Texture2D)m_IPadLandscapeSplashScreen.objectReferenceValue, typeof(Texture2D), false);

						EditorGUILayout.Space();

						m_IPadHighResLandscapeSplashScreen.objectReferenceValue = EditorGUILayout.ObjectField(EditorGUIUtility.TextContent("PlayerSettings.IPadHighResLandscapeSplashScreen"), (Texture2D)m_IPadHighResLandscapeSplashScreen.objectReferenceValue, typeof(Texture2D), false);
						EditorGUILayout.Space();
					}

					if (targetGroup == BuildTargetGroup.Android)
					{
						EditorGUI.BeginDisabledGroup (m_IPhoneSplashScreen.objectReferenceValue == null);
						EditorGUILayout.PropertyField(m_AndroidSplashScreenScale, EditorGUIUtility.TextContent("PlayerSettings.AndroidSplashScreenScale"));
						EditorGUI.EndDisabledGroup ();
					}

					if( settingsExtension != null )
						settingsExtension.SplashSectionGUI();

					EditorGUI.EndDisabledGroup ();

					if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android )
						ShowSharedNote();
				}
			}
			EndSettingsBox();
		}

		public void ResolutionSectionGUI (BuildTargetGroup targetGroup, ISettingEditorExtension settingsExtension)
		{
			GUI.changed = false;
			if (BeginSettingsBox(0, EditorGUIUtility.TextContent("PlayerSettings.ResolutionHeader")))
			{
				// PLEASE DO NOT COPY SETTINGS TO APPEAR MULTIPLE PLACES IN THE CODE! See top of file for more info.

				{
					// Resolution

					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.ResolutionSubHeader"), EditorStyles.boldLabel);
					// Resolution itself
					if (targetGroup == BuildTargetGroup.Standalone)
					{
						EditorGUILayout.PropertyField(m_DefaultIsFullScreen);
						m_ShowDefaultIsNativeResolution.target = m_DefaultIsFullScreen.boolValue;
						if (EditorGUILayout.BeginFadeGroup (m_ShowDefaultIsNativeResolution.faded))
							EditorGUILayout.PropertyField(m_DefaultIsNativeResolution);
						EditorGUILayout.EndFadeGroup ();

						m_ShowResolution.target = !(m_DefaultIsFullScreen.boolValue && m_DefaultIsNativeResolution.boolValue);
						if (EditorGUILayout.BeginFadeGroup (m_ShowResolution.faded)) {
							EditorGUILayout.PropertyField(m_DefaultScreenWidth, EditorGUIUtility.TextContent("PlayerSettings.DefaultScreenWidth"));
							EditorGUILayout.PropertyField(m_DefaultScreenHeight, EditorGUIUtility.TextContent("PlayerSettings.DefaultScreenHeight"));
						}
						EditorGUILayout.EndFadeGroup ();
					}
					if (targetGroup == BuildTargetGroup.WebPlayer || targetGroup == BuildTargetGroup.FlashPlayer || targetGroup == BuildTargetGroup.NaCl)
					{
						EditorGUILayout.PropertyField(m_DefaultScreenWidthWeb, EditorGUIUtility.TextContent("PlayerSettings.DefaultScreenWidthWeb"));
						EditorGUILayout.PropertyField(m_DefaultScreenHeightWeb, EditorGUIUtility.TextContent("PlayerSettings.DefaultScreenHeightWeb"));
					}
					if (targetGroup == BuildTargetGroup.XBOX360)
					{
						// TODO: implement if we allow any settings here (like MSAA control or auto letter-boxing)
						ShowNoSettings();
						EditorGUILayout.Space();
					}
					if (targetGroup == BuildTargetGroup.Standalone || targetGroup == BuildTargetGroup.WebPlayer)
					{
						EditorGUILayout.PropertyField(m_RunInBackground, EditorGUIUtility.TextContent("PlayerSettings.RunInBackground"));
					}

					if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android || (settingsExtension != null && settingsExtension.SupportsOrientation())
						#if INCLUDE_WP8SUPPORT
						#endif
						)
					{
						EditorGUILayout.PropertyField(m_DefaultScreenOrientation, EditorGUIUtility.TextContent("PlayerSettings.DefaultScreenOrientation"));

						if( m_DefaultScreenOrientation.enumValueIndex == 4 )
						{
							if (targetGroup == BuildTargetGroup.iPhone )
								EditorGUILayout.PropertyField(m_UseOSAutoRotation, EditorGUIUtility.TextContent("PlayerSettings.UseOSAutorotation"));

							EditorGUI.indentLevel++;

							GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.AutoRotationAllowedOrientation"), EditorStyles.boldLabel);

							bool somethingAllowed =		m_AllowedAutoRotateToPortrait.boolValue
													||	m_AllowedAutoRotateToPortraitUpsideDown.boolValue
													||	m_AllowedAutoRotateToLandscapeRight.boolValue
													||	m_AllowedAutoRotateToLandscapeLeft.boolValue;

							if( !somethingAllowed )
							{
								m_AllowedAutoRotateToPortrait.boolValue = true;
								Debug.LogError("All orientations are disabled. Allowing portrait");
							}

							EditorGUILayout.PropertyField(m_AllowedAutoRotateToPortrait,			EditorGUIUtility.TextContent("PlayerSettings.PortraitOrientation"));
							EditorGUILayout.PropertyField(m_AllowedAutoRotateToPortraitUpsideDown,	EditorGUIUtility.TextContent("PlayerSettings.PortraitUpsideDownOrientation"));
							EditorGUILayout.PropertyField(m_AllowedAutoRotateToLandscapeRight, 		EditorGUIUtility.TextContent("PlayerSettings.LandscapeRightOrientation"));
							EditorGUILayout.PropertyField(m_AllowedAutoRotateToLandscapeLeft,		EditorGUIUtility.TextContent("PlayerSettings.LandscapeLeftOrientation"));

							EditorGUI.indentLevel--;
						}
					}

					if(targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android ) {

						#if INCLUDE_WP8SUPPORT
						if (targetGroup != BuildTargetGroup.WP8)
						#endif
						{
							GUILayout.Label (EditorGUIUtility.TextContent ("PlayerSettings.StatusBarSubHeader"), EditorStyles.boldLabel);
							EditorGUILayout.PropertyField (m_UIStatusBarHidden, EditorGUIUtility.TextContent ("PlayerSettings.UIStatusBarHidden"));
						}

						// iPhone-specific Status Bar settings
						if (targetGroup == BuildTargetGroup.iPhone)
						{
							EditorGUILayout.PropertyField(m_UIStatusBarStyle, EditorGUIUtility.TextContent("PlayerSettings.UIStatusBarStyle"));
							EditorGUILayout.Space();
						}
					}

					EditorGUILayout.Space();

					// Standalone Player
					if (targetGroup == BuildTargetGroup.Standalone)
					{
						GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.StandalonePlayerSubHeader"), EditorStyles.boldLabel);
						EditorGUILayout.PropertyField(m_CaptureSingleScreen);
						EditorGUILayout.PropertyField(m_DisplayResolutionDialog);
						EditorGUILayout.PropertyField(m_UsePlayerLog);
						EditorGUILayout.PropertyField(m_ResizableWindow);
						EditorGUILayout.PropertyField(m_UseMacAppStoreValidation, EditorGUIUtility.TempContent ("Mac App Store Validation"));
						EditorGUILayout.PropertyField(m_MacFullscreenMode);
						#if ENABLE_SINGLE_INSTANCE_BUILD_SETTING
						EditorGUILayout.PropertyField(m_ForceSingleInstance);
						#endif
						EditorGUILayout.PropertyField(m_SupportedAspectRatios, true);

						EditorGUILayout.Space();
					}

					// Webplayer Template
					if (targetGroup == BuildTargetGroup.WebPlayer)
					{
						GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.WebPlayerTemplateSubHeader"), EditorStyles.boldLabel);

						if (WebPlayerTemplate.TemplateGUIThumbnails.Length < 1)
						{
							GUILayout.Label (EditorGUIUtility.TextContent("PlayerSettings.NoTemplatesFound"));
						}
						else
						{
							int numCols = Mathf.Min ((int)Mathf.Max ((Screen.width - kWebPlayerTemplateGridPadding * 2.0f) / kThumbnailSize, 1.0f), WebPlayerTemplate.TemplateGUIThumbnails.Length);
							int numRows = Mathf.Max ((int)Mathf.Ceil ((float)WebPlayerTemplate.TemplateGUIThumbnails.Length / (float)numCols), 1);


							bool wasChanged = GUI.changed;

							m_WebPlayerTemplate.stringValue = WebPlayerTemplate.Templates[
								ThumbnailList (
									GUILayoutUtility.GetRect (numCols * kThumbnailSize, numRows * (kThumbnailSize + kThumbnailLabelHeight), GUILayout.ExpandWidth (false)),
									WebPlayerTemplate.GetTemplateIndex (m_WebPlayerTemplate.stringValue),
									WebPlayerTemplate.TemplateGUIThumbnails,
									numCols
								)].ToString ();

							bool templateChanged = !wasChanged && GUI.changed;

							bool orgChanged = GUI.changed;
							GUI.changed = false;
							foreach (string key in PlayerSettings.templateCustomKeys)
							{
								string value = PlayerSettings.GetTemplateCustomValue (key);
								value = EditorGUILayout.TextField (PrettyTemplateKeyName (key), value);
								PlayerSettings.SetTemplateCustomValue (key, value);
							}
							if (GUI.changed)
								serializedObject.Update();
							GUI.changed |= orgChanged;

							if (templateChanged)
							{
								GUIUtility.hotControl = 0;
								GUIUtility.keyboardControl = 0;
								serializedObject.ApplyModifiedProperties ();
								PlayerSettings.templateCustomKeys = WebPlayerTemplate.Templates [WebPlayerTemplate.GetTemplateIndex (m_WebPlayerTemplate.stringValue)].CustomKeys;
								serializedObject.Update ();
							}
						}
						EditorGUILayout.Space();
					}

					// mobiles color/depth bits setup
					if (IsMobileTarget(targetGroup))
					{
						EditorGUILayout.PropertyField(m_Use32BitDisplayBuffer, EditorGUIUtility.TextContent("PlayerSettings.Use32BitDisplayBuffer"));
						EditorGUILayout.PropertyField(m_Use24BitDepthBuffer, EditorGUIUtility.TextContent("PlayerSettings.Use24BitDepthBuffer"));
					}
					// activity indicator on loading
					if (targetGroup == BuildTargetGroup.iPhone)
					{
						EditorGUILayout.PropertyField(m_iosShowActivityIndicatorOnLoading, EditorGUIUtility.TextContent("PlayerSettings.iosShowActivityIndicatorOnLoading"));
					}
					if (targetGroup == BuildTargetGroup.Android)
					{
						EditorGUILayout.PropertyField(m_androidShowActivityIndicatorOnLoading, EditorGUIUtility.TextContent("PlayerSettings.androidShowActivityIndicatorOnLoading"));
					}
					if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android)
					{
						EditorGUILayout.Space();
					}

					ShowSharedNote();
				}
			}
			EndSettingsBox();
		}


		private enum FakeEnum
		{
			WebplayerSubset,
			FlashPlayerSubset
		}

		void ShowDisabledFakeEnumPopup (FakeEnum enumValue)
		{
			GUILayout.BeginHorizontal();
			EditorGUILayout.PrefixLabel(m_ApiCompatibilityLevel.displayName);
			EditorGUI.BeginDisabledGroup(true);
			EditorGUILayout.EnumPopup(enumValue);
			EditorGUI.EndDisabledGroup();
			GUILayout.EndHorizontal();
		}

		void ShowAdvancedBatchingWarning (BuildTarget target)
		{
			if (!InternalEditorUtility.HasAdvancedLicenseOnBuildTarget(target))
			{
				GUIContent c = new GUIContent("Static Batching requires " + BuildPipeline.GetBuildTargetAdvancedLicenseName(target));
				EditorGUILayout.HelpBox (c.text, MessageType.Warning);
			}
		}

		void DX11SettingGUI (BuildTargetGroup targetGroup)
		{
			if (targetGroup != BuildTargetGroup.Standalone && targetGroup != BuildTargetGroup.WebPlayer)
				return;

			bool isWindows = Application.platform == RuntimePlatform.WindowsEditor;

			// Don't use SerializedProperty here, since at the time it changes it's not propagated
			// to the actual object yet.
			bool oldUse11 = PlayerSettings.useDirect3D11;
			EditorGUI.BeginDisabledGroup (EditorApplication.isPlaying); // can't switch DX11 while in play mode
			bool newUse11 = EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("PlayerSettings.UseDX11"), oldUse11);
			EditorGUI.EndDisabledGroup ();

			if (newUse11 != oldUse11)
			{
				if (isWindows)
				{
					if (EditorUtility.DisplayDialog("Changing graphics device",
					                                "Changing DX11 option requires reloading all graphics objects, it might take a while",
					                                "Apply", "Cancel"))
					{
						if (EditorApplication.SaveCurrentSceneIfUserWantsTo())
						{
							PlayerSettings.useDirect3D11 = newUse11;
							ShaderUtil.RecreateGfxDevice();
							GUIUtility.ExitGUI();
						}
					}
				}
				else
				{
					PlayerSettings.useDirect3D11 = newUse11;
				}
			}

			if (newUse11)
			{
				if (!isWindows)
					EditorGUILayout.HelpBox (EditorGUIUtility.TextContent ("PlayerSettings.DX11Warning").text, MessageType.Warning);
				else if (!SystemInfo.graphicsDeviceVersion.StartsWith ("Direct3D 11"))
					EditorGUILayout.HelpBox (EditorGUIUtility.TextContent ("PlayerSettings.DX11NotSupported").text, MessageType.Warning);
			}
		}

		public void OtherSectionGUI (BuildPlayerWindow.BuildPlatform platform, BuildTargetGroup targetGroup, ISettingEditorExtension settingsExtension)
		{
			GUI.changed = false;
			if (BeginSettingsBox(3, EditorGUIUtility.TextContent("PlayerSettings.OtherHeader")))
			{
				// PLEASE DO NOT COPY SETTINGS TO APPEAR MULTIPLE PLACES IN THE CODE! See top of file for more info.

				{
					// Rendering

					GUILayout.Label (EditorGUIUtility.TextContent ("PlayerSettings.RenderingSubHeader"), EditorStyles.boldLabel);

					if (targetGroup == BuildTargetGroup.Standalone || targetGroup == BuildTargetGroup.WebPlayer || IsMobileTarget(targetGroup))
					{
						bool isMobile = IsMobileTarget(targetGroup);
						EditorGUILayout.PropertyField(isMobile ? m_MobileRenderingPath : m_RenderingPath, EditorGUIUtility.TextContent("PlayerSettings.RenderingPath"));
					}
					if (targetGroup == BuildTargetGroup.Standalone || targetGroup == BuildTargetGroup.WebPlayer)
					{
                        if (EditorGUILayout.BeginFadeGroup(m_ShowDeferredWarning.faded))
                        {
                            GUIContent msg = EditorGUIUtility.TextContent("CameraEditor.DeferredProOnly");
                            EditorGUILayout.HelpBox(msg.text, MessageType.Warning, false);
                        }
                        EditorGUILayout.EndFadeGroup();
					}

					if (targetGroup == BuildTargetGroup.Standalone
						|| targetGroup == BuildTargetGroup.WebPlayer
						|| targetGroup == BuildTargetGroup.PS3
						|| targetGroup == BuildTargetGroup.XBOX360 )
					{
						EditorGUILayout.PropertyField(m_ActiveColorSpace, EditorGUIUtility.TextContent("PlayerSettings.ActiveColorSpace"));

						if (QualitySettings.activeColorSpace != QualitySettings.desiredColorSpace)
							EditorGUILayout.HelpBox (s_Styles.colorSpaceWarning.text, MessageType.Warning);
					}

					if (targetGroup == BuildTargetGroup.XBOX360 || targetGroup == BuildTargetGroup.Android) //TODO:enable on desktops when decision is made
					{
						if(IsMobileTarget(targetGroup))
							m_MobileMTRendering.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.MultithreadedRendering"), m_MobileMTRendering.boolValue);
						else
							m_MTRendering.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.MultithreadedRendering"), m_MTRendering.boolValue);
					}

					DX11SettingGUI (targetGroup);

					// Batching section
                    // Xbox 360 only supports static batching
					{
                        int staticBatching, dynamicBatching;
                        bool staticBatchingSupported = (targetGroup != BuildTargetGroup.Wii) && (targetGroup != BuildTargetGroup.PS3);
                        bool dynamicBatchingSupported = (targetGroup != BuildTargetGroup.Wii) && (targetGroup != BuildTargetGroup.PS3) && (targetGroup != BuildTargetGroup.XBOX360);
						PlayerSettings.GetBatchingForPlatform (platform.DefaultTarget, out staticBatching, out dynamicBatching);

                        bool reset = false;
                        if (staticBatchingSupported == false && staticBatching == 1)
                        {
                            staticBatching = 0;
                            reset = true;
                        }

                        if (dynamicBatchingSupported == false && dynamicBatching == 1)
                        {
                            dynamicBatching = 0;
                            reset = true;
                        }

                        if (reset)
						{
                            PlayerSettings.SetBatchingForPlatform(platform.DefaultTarget, staticBatching, dynamicBatching);
						}

						EditorGUI.BeginDisabledGroup (!(InternalEditorUtility.HasAdvancedLicenseOnBuildTarget(platform.DefaultTarget) && staticBatchingSupported));
						if (GUI.enabled)
							staticBatching = EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("PlayerSettings.StaticBatching"), staticBatching != 0) ? 1 : 0;
						else
							EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("PlayerSettings.StaticBatching"), false);
						EditorGUI.EndDisabledGroup ();

						EditorGUI.BeginDisabledGroup (!dynamicBatchingSupported);
						dynamicBatching = EditorGUILayout.Toggle (EditorGUIUtility.TextContent ("PlayerSettings.DynamicBatching"), dynamicBatching != 0) ? 1 : 0;
						EditorGUI.EndDisabledGroup ();

						ShowAdvancedBatchingWarning(platform.DefaultTarget);

						if (GUI.changed)
						{
							Undo.RecordObject (target, "Changed Batching Settings");
							PlayerSettings.SetBatchingForPlatform (platform.DefaultTarget, staticBatching, dynamicBatching);
						}
  					}

					// GPU Skinning toggle (only show on relevant platforms)
					if (targetGroup == BuildTargetGroup.XBOX360 ||
						targetGroup == BuildTargetGroup.Standalone ||
						targetGroup == BuildTargetGroup.WebPlayer ||
						targetGroup == BuildTargetGroup.Android)
					{
						EditorGUI.BeginChangeCheck();
						EditorGUILayout.PropertyField (m_SkinOnGPU, EditorGUIUtility.TextContent ("PlayerSettings.GPUSkinning"));
						if (EditorGUI.EndChangeCheck())
						{
							ShaderUtil.RecreateSkinnedMeshResources ();
						}
					}

					if (targetGroup == BuildTargetGroup.XBOX360 || targetGroup == BuildTargetGroup.Standalone)
					{
						m_XboxPIXTextureCapture.boolValue = EditorGUILayout.Toggle("Enable PIX texture capture", m_XboxPIXTextureCapture.boolValue);
					}

					EditorGUILayout.Space ();

					// Streaming

					if (targetGroup == BuildTargetGroup.WebPlayer)
					{
						GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.StreamingSubHeader"), EditorStyles.boldLabel);
						EditorGUILayout.PropertyField(m_FirstStreamedLevelWithResources, EditorGUIUtility.TextContent("PlayerSettings.FirstStreamedLevelWithResources"));
						EditorGUILayout.Space();
					}

					// Identification

					if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android || (settingsExtension != null && settingsExtension.HasIdentificationGUI()))
					{
						GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.IdentificationSubHeader"), EditorStyles.boldLabel);

						EditorGUILayout.PropertyField(m_IPhoneBundleIdentifier, EditorGUIUtility.TextContent("PlayerSettings.IPhoneBundleIdentifier"));

						EditorGUILayout.PropertyField(m_IPhoneBundleVersion, EditorGUIUtility.TextContent("PlayerSettings.IPhoneBundleVersion"));

						if (targetGroup == BuildTargetGroup.Android)
						{
							EditorGUILayout.PropertyField(m_AndroidBundleVersionCode, EditorGUIUtility.TextContent("PlayerSettings.AndroidBundleVersionCode"));
							EditorGUILayout.PropertyField(m_AndroidMinSdkVersion, EditorGUIUtility.TextContent("PlayerSettings.AndroidMinSdkVersion"));
						}

						if( settingsExtension != null )
							settingsExtension.IdentificationSectionGUI();

						EditorGUILayout.Space();
					}

					// Configuration

                    if (targetGroup != BuildTargetGroup.FlashPlayer)
					{
						GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.ConfigurationSubHeader"), EditorStyles.boldLabel);

						// mobile-only settings
						if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Android || targetGroup == BuildTargetGroup.Metro)
						{
							if (targetGroup == BuildTargetGroup.iPhone)
							{
								EditorGUILayout.PropertyField(m_TargetDevice);

								if (m_TargetDevice.intValue == 1 ||
									m_TargetDevice.intValue == 2)
								{
									if (m_IPhoneTargetOSVersion.intValue <= 6)
									{
										m_IPhoneTargetOSVersion.intValue = 7;
									}
								}

								EditorGUILayout.PropertyField(m_TargetResolution, EditorGUIUtility.TextContent("PlayerSettings.TargetResolution"));
							}

							if (targetGroup == BuildTargetGroup.Android)
							{
								EditorGUILayout.PropertyField(m_AndroidTargetDevice, EditorGUIUtility.TextContent("PlayerSettings.AndroidTargetDevice"));

								if (!m_CreateWallpaper.boolValue)
								{
									GUIContent label = EditorGUIUtility.TextContent ("PlayerSettings.AndroidPreferredInstallLocation");
									EditorGUILayout.PropertyField (m_AndroidPreferredInstallLocation, label);
								}
							}

	                        if (targetGroup == BuildTargetGroup.Android)
	                        {
	                            EditorGUILayout.PropertyField(m_TargetGlesGraphics, EditorGUIUtility.TextContent("PlayerSettings.TargetGlesGraphics"));
	                        }

							// Eventually it should be common setting for Android & iOS. Remove the if guard when implemented on Android
							if (targetGroup == BuildTargetGroup.iPhone || targetGroup == BuildTargetGroup.Metro)
								EditorGUILayout.PropertyField(m_AccelerometerFrequency, EditorGUIUtility.TextContent("PlayerSettings.AccelerometerFrequency"));

							if (targetGroup == BuildTargetGroup.iPhone)
							{
								EditorGUILayout.PropertyField(m_OverrideIPodMusic);
								EditorGUILayout.PropertyField(m_PrepareIOSForRecording);
								EditorGUILayout.PropertyField(m_UIRequiresPersistentWiFi, EditorGUIUtility.TextContent("PlayerSettings.UIRequiresPersistentWiFi"));
								EditorGUILayout.PropertyField(m_UIExitOnSuspend, EditorGUIUtility.TextContent("PlayerSettings.UIExitOnSuspend"));
								EditorGUILayout.PropertyField(m_EnableHWStatistics, EditorGUIUtility.TextContent("PlayerSettings.enableHWStatistics"));
							}

							if (targetGroup == BuildTargetGroup.Android)
							{
							EditorGUILayout.PropertyField(m_EnableHWStatistics, EditorGUIUtility.TextContent("PlayerSettings.enableHWStatistics"));

								EditorGUI.BeginChangeCheck ();
								bool internet = m_ForceInternetPermission.boolValue;
								internet = EditorGUILayout.Popup (EditorGUIUtility.TextContent("PlayerSettings.ForceInternetPermission").text, internet ? 1 : 0, new string[] { "Auto", "Require" }) == 1 ? true : false;
								if (EditorGUI.EndChangeCheck ())
									m_ForceInternetPermission.boolValue = internet;

								EditorGUI.BeginChangeCheck ();
								bool writeExternal = m_ForceSDCardPermission.boolValue;
								writeExternal = EditorGUILayout.Popup (EditorGUIUtility.TextContent("PlayerSettings.ForceSDCardPermission").text, writeExternal ? 1 : 0, new string[] { "Internal Only", "External (SDCard)" }) == 1 ? true : false;
								if (EditorGUI.EndChangeCheck ())
									m_ForceSDCardPermission.boolValue = writeExternal;

								if (Unsupported.IsDeveloperBuild())
									EditorGUILayout.PropertyField(m_CreateWallpaper, EditorGUIUtility.TextContent("PlayerSettings.CreateWallpaper"));
							}
						}

						if( settingsExtension != null )
							settingsExtension.ConfigurationSectionGUI();


						// User script defines
						{
							EditorGUILayout.LabelField (EditorGUIUtility.TextContent ("PlayerSettings.scriptingDefineSymbols"));
							EditorGUI.BeginChangeCheck ();
							string scriptDefines = EditorGUILayout.DelayedTextField(PlayerSettings.GetScriptingDefineSymbolsForGroup (targetGroup), null, EditorStyles.textField);
							scriptingDefinesControlID = EditorGUIUtility.s_LastControlID;
							if (EditorGUI.EndChangeCheck ())
								PlayerSettings.SetScriptingDefineSymbolsForGroup (targetGroup, scriptDefines);
						}

						EditorGUILayout.Space();
					}

					if (targetGroup == BuildTargetGroup.Wii)
					{
						OtherSectionGUIWii();
					}

					// Optimization

					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.OptimizationSubHeader"), EditorStyles.boldLabel);

					if (targetGroup == BuildTargetGroup.WebPlayer)
					{
						ShowDisabledFakeEnumPopup (FakeEnum.WebplayerSubset);
					}
					else if (targetGroup == BuildTargetGroup.FlashPlayer)
					{
						ShowDisabledFakeEnumPopup(FakeEnum.FlashPlayerSubset);
						EditorGUILayout.PropertyField(m_FlashStrippingLevel, EditorGUIUtility.TextContent("PlayerSettings.flashStrippingLevel"));
					}
					else
					{
						EditorGUI.BeginChangeCheck ();
						EditorGUILayout.PropertyField(m_ApiCompatibilityLevel);
						if (EditorGUI.EndChangeCheck ())
						{
							PlayerSettings.SetApiCompatibilityInternal(m_ApiCompatibilityLevel.intValue);
						}
					}

					if (targetGroup == BuildTargetGroup.NaCl || targetGroup == BuildTargetGroup.FlashPlayer)
					{
						EditorGUILayout.PropertyField(m_StripPhysics, EditorGUIUtility.TextContent("PlayerSettings.StripPhysics"));
					}

					if (targetGroup == BuildTargetGroup.iPhone ||
						targetGroup == BuildTargetGroup.XBOX360 ||
						targetGroup == BuildTargetGroup.PS3)
						EditorGUILayout.PropertyField(m_AotOptions, EditorGUIUtility.TextContent("PlayerSettings.aotOptions"));

					if (targetGroup == BuildTargetGroup.iPhone ||
						targetGroup == BuildTargetGroup.Android ||
						targetGroup == BuildTargetGroup.PS3 ||
						targetGroup == BuildTargetGroup.BB10 ||
						targetGroup == BuildTargetGroup.XBOX360)
					{
						if (targetGroup == BuildTargetGroup.iPhone)
						{
							EditorGUILayout.PropertyField(m_IPhoneSdkVersion, EditorGUIUtility.TextContent("PlayerSettings.IPhoneSdkVersion"));
							EditorGUILayout.PropertyField(m_IPhoneTargetOSVersion, EditorGUIUtility.TextContent("PlayerSettings.IPhoneTargetOSVersion"));
						}

						if (InternalEditorUtility.HasAdvancedLicenseOnBuildTarget(platform.DefaultTarget))
						{
							EditorGUILayout.PropertyField(m_IPhoneStrippingLevel, EditorGUIUtility.TextContent("PlayerSettings.IPhoneStrippingLevel"));
						}
						else
						{
							EditorGUI.BeginDisabledGroup (true);
							int[] values = { 0 };
							GUIContent[] displayNames = { new GUIContent("Disabled") };
							EditorGUILayout.IntPopup(EditorGUIUtility.TextContent("PlayerSettings.IPhoneStrippingLevel"), 0, displayNames, values);
							EditorGUI.EndDisabledGroup ();
						}

						if (targetGroup == BuildTargetGroup.iPhone)
						{
							EditorGUILayout.PropertyField(m_IPhoneScriptCallOptimization, EditorGUIUtility.TextContent("PlayerSettings.IPhoneScriptCallOptimization"));
						}
						if (targetGroup == BuildTargetGroup.Android)
						{
							EditorGUILayout.PropertyField(m_AndroidProfiler, EditorGUIUtility.TextContent("PlayerSettings.AndroidProfiler"));
						}


						EditorGUILayout.Space();
					}

					EditorGUILayout.PropertyField (m_StripUnusedMeshComponents, EditorGUIUtility.TextContent ("PlayerSettings.StripUnusedMeshComponents"));
					EditorGUILayout.Space ();

					ShowSharedNote();
				}
			}
			EndSettingsBox();
		}

		private void AutoAssignProperty(SerializedProperty property, string packageDir, string fileName)
		{
			if (property.stringValue.Length == 0 || !File.Exists(Path.Combine(packageDir, property.stringValue)))
			{
				string filePath = Path.Combine(packageDir, fileName);
				if (File.Exists(filePath))
					property.stringValue = fileName;
			}
		}

		private void ShowBrowseableProperty(SerializedProperty property, string textContent, string extension, string dir, float kLabelFloatMinW, float kLabelFloatMaxW, float h )
		{
			bool showTitle = textContent.Length != 0;

			if (showTitle)
				GUILayout.Label(EditorGUIUtility.TextContent(textContent), EditorStyles.boldLabel);

			Rect r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
			float labelWidth = EditorGUIUtility.labelWidth;
			Rect buttonRect = new Rect(r.x + EditorGUI.indent, r.y, labelWidth - EditorGUI.indent, r.height);
			Rect fieldRect = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);

			string displayTitleConfigPath = (property.stringValue.Length == 0) ? "Not selected." : property.stringValue;
			EditorGUI.TextArea(fieldRect, displayTitleConfigPath, EditorStyles.label);

			if (GUI.Button(buttonRect, EditorGUIUtility.TextContent("PlayerSettings.BrowseGeneric")))
			{
				property.stringValue = FileUtil.GetLastPathNameComponent(EditorUtility.OpenFilePanel(
                    EditorGUIUtility.TextContent("PlayerSettings.BrowseGeneric").text, dir, extension));
				serializedObject.ApplyModifiedProperties();
				GUIUtility.ExitGUI();
			}
			EditorGUILayout.Space();
		}

		public void OtherSectionGUIWii()
		{
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.wiiControllers"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField(m_WiiSupportsNunchuk, EditorGUIUtility.TextContent("PlayerSettings.wiiSupportsNunchuk"));
			EditorGUILayout.PropertyField(m_WiiSupportsClassicController, EditorGUIUtility.TextContent("PlayerSettings.wiiSupportsClassicController"));
			EditorGUILayout.PropertyField(m_WiiSupportsBalanceBoard, EditorGUIUtility.TextContent("PlayerSettings.wiiSupportsBalanceBoard"));
			EditorGUILayout.PropertyField(m_WiiSupportsMotionPlus, EditorGUIUtility.TextContent("PlayerSettings.wiiSupportsMotionPlus"));
			int newValue = EditorGUILayout.IntField(EditorGUIUtility.TextContent("PlayerSettings.wiiControllerCount"), m_WiiControllerCount.intValue);
			m_WiiControllerCount.intValue = Mathf.Clamp(newValue, 1, 4);

			EditorGUILayout.Space();

			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreen"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField(m_WiiLoadingScreenFileName, EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreenFileName"));
			EditorGUILayout.PropertyField(m_WiiLoadingScreenPeriod, EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreenPeriod"));
			EditorGUILayout.PropertyField(m_WiiLoadingScreenRectPlacement, EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreenRectPlacement"));
			EditorGUILayout.PropertyField(m_WiiLoadingScreenRect, EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreenRect"));
			EditorGUILayout.PropertyField(m_WiiLoadingScreenBackground, EditorGUIUtility.TextContent("PlayerSettings.wiiLoadingScreenBackground"));
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.wiiMisc"), EditorStyles.boldLabel);
			EditorGUILayout.PropertyField(m_WiiFloatingPointExceptions, EditorGUIUtility.TextContent("PlayerSettings.wiiFloatingPointExceptions"));
			EditorGUILayout.PropertyField(m_WiiScreenCrashDumps, EditorGUIUtility.TextContent("PlayerSettings.wiiScreenCrashDumps"));

			EditorGUILayout.PropertyField(m_WiiHio2Usage, EditorGUIUtility.TextContent("PlayerSettings.wiiHio2Usage"));


			EditorGUILayout.Space();
		}
		public void PublishSectionGUI (BuildTargetGroup targetGroup, ISettingEditorExtension settingsExtension)
		{
            if (targetGroup != BuildTargetGroup.Wii &&
                #if INCLUDE_METROSUPPORT
                targetGroup != BuildTargetGroup.Metro &&
                #endif
				targetGroup != BuildTargetGroup.Android &&
				targetGroup != BuildTargetGroup.XBOX360 &&
				targetGroup != BuildTargetGroup.PS3 &&
			    !(settingsExtension != null && settingsExtension.HasPublishSection()) )
				return;

			GUI.changed = false;
            if (BeginSettingsBox(4, EditorGUIUtility.TextContent("PlayerSettings.PublishingHeader")))
			{
				string assetsDirectory = "Assets";
				string projectDirectory = FileUtil.DeleteLastPathNameComponent(Application.dataPath);

				float h = EditorGUI.kSingleLineHeight;
				float kLabelFloatMinW = EditorGUI.kLabelW + EditorGUIUtility.fieldWidth + EditorGUI.kSpacing;
				float kLabelFloatMaxW = EditorGUI.kLabelW + EditorGUIUtility.fieldWidth + EditorGUI.kSpacing;
				Rect r;

				if (settingsExtension != null )
				{
					settingsExtension.PublishSectionGUI (h, kLabelFloatMinW, kLabelFloatMaxW);
				}
                #if INCLUDE_METROSUPPORT
                if (targetGroup == BuildTargetGroup.Metro)
                {
                    PublishSectionGUIMetro(kLabelFloatMinW, kLabelFloatMaxW, h, h);
                }
                #endif
				if (targetGroup == BuildTargetGroup.Wii)
				{
					PublishSectionGUIWii(kLabelFloatMinW, kLabelFloatMaxW, h, h);
				}

				if (targetGroup == BuildTargetGroup.PS3)
				{
					// try and auto-assign stuff
					string ps3SubmissionPackageDir = Path.Combine(Application.dataPath, "PS3 Submission Package");
					if (Directory.Exists(ps3SubmissionPackageDir))
					{
						AutoAssignProperty(m_PS3TitleConfigPath, ps3SubmissionPackageDir, "TITLECONFIG.XML");
						AutoAssignProperty(m_PS3DLCConfigPath, ps3SubmissionPackageDir, "DLCconfig.txt");
						AutoAssignProperty(m_PS3ThumbnailPath, ps3SubmissionPackageDir, "ICON0.PNG");
						AutoAssignProperty(m_PS3BackgroundPath, ps3SubmissionPackageDir, "BACKGROUND0.PNG");
						AutoAssignProperty(m_PS3TrophyPackagePath, ps3SubmissionPackageDir, "TROPHY.TRP");
						AutoAssignProperty(m_PS3SoundPath, ps3SubmissionPackageDir, "SDN0.AT3");
					}
					else
						ps3SubmissionPackageDir = assetsDirectory;

					ShowBrowseableProperty(m_PS3TitleConfigPath, "PlayerSettings.ps3TitleConfigPath", "xml", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);
					ShowBrowseableProperty(m_PS3DLCConfigPath, "PlayerSettings.ps3DLCConfigPath", "txt", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);
					ShowBrowseableProperty(m_PS3ThumbnailPath, "PlayerSettings.ps3ThumbnailPath", "png", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);
					ShowBrowseableProperty(m_PS3BackgroundPath, "PlayerSettings.ps3BackgroundPath", "png", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);
					ShowBrowseableProperty(m_PS3SoundPath, "PlayerSettings.ps3SoundPath", "at3", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);

					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.ps3TrophyPackagePath"), EditorStyles.boldLabel);
                    ShowBrowseableProperty(m_PS3TrophyPackagePath, "", "trp", ps3SubmissionPackageDir, kLabelFloatMinW, kLabelFloatMaxW, h);
					EditorGUILayout.PropertyField(m_PS3TrophyCommId, EditorGUIUtility.TextContent("PlayerSettings.ps3TrophyCommId"));
                    m_PS3NpCommunicationPassphrase.stringValue = EditorGUILayout.TextField(EditorGUIUtility.TextContent("PlayerSettings.ps3NpCommunicationPassphrase"), m_PS3NpCommunicationPassphrase.stringValue, GUILayout.Height(280));
                    m_PS3TrophyCommSig.stringValue = EditorGUILayout.TextField(EditorGUIUtility.TextContent("PlayerSettings.ps3TrophyCommSig"), m_PS3TrophyCommSig.stringValue, GUILayout.Height(280));

                    GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.ps3TitleSettings"), EditorStyles.boldLabel);
                    EditorGUILayout.PropertyField(m_PS3TrialMode, EditorGUIUtility.TextContent("PlayerSettings.ps3TrialMode"));
                    m_PS3BootCheckMaxSaveGameSizeKB.intValue = EditorGUILayout.IntField(EditorGUIUtility.TextContent("PlayerSettings.ps3BootCheckMaxSaveGameSizeKB"), m_PS3BootCheckMaxSaveGameSizeKB.intValue);
                    m_PS3SaveGameSlots.intValue = EditorGUILayout.IntField(EditorGUIUtility.TextContent("PlayerSettings.ps3SaveGameSlots"), m_PS3SaveGameSlots.intValue);
				}

				if (targetGroup == BuildTargetGroup.XBOX360)
				{
				    m_XboxAdditionalTitleMemorySize = serializedObject.FindProperty("xboxAdditionalTitleMemorySize");
                    m_XboxAdditionalTitleMemorySize.intValue = (int)EditorGUILayout.Slider(EditorGUIUtility.TextContent("PlayerSettings.XboxAdditionalTitleMemorySize"), (float)m_XboxAdditionalTitleMemorySize.intValue, 0f, 448f - 32f);
                    if (m_XboxAdditionalTitleMemorySize.intValue > 0)
                        ShowWarning(EditorGUIUtility.TextContent("PlayerSettings.XboxAdditionalTitleMemoryWarning"));

					// Submission
					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.SubmissionSubHeader"), EditorStyles.boldLabel);
					{
						// Title id
						EditorGUILayout.PropertyField(m_XboxTitleId, EditorGUIUtility.TextContent("PlayerSettings.XboxTitleId"));
					}

					EditorGUILayout.Space();

					// Image conversion
					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxImageConversion"), EditorStyles.boldLabel);
					{
						// ImageXex configuration override file
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						float labelWidth = EditorGUIUtility.labelWidth;
						Rect buttonRect = new Rect(r.x + EditorGUI.indent, r.y, labelWidth - EditorGUI.indent, r.height);
						Rect fieldRect = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);

						string displayImageXexPath = (m_XboxImageXexPath.stringValue.Length == 0) ? "Not selected." : m_XboxImageXexPath.stringValue;
						EditorGUI.TextArea(fieldRect, displayImageXexPath, EditorStyles.label);

						if (GUI.Button(buttonRect, EditorGUIUtility.TextContent("PlayerSettings.XboxImageXEXFile")))
						{
							string filePath = EditorUtility.OpenFilePanel(
							EditorGUIUtility.TextContent("PlayerSettings.XboxImageXEXFile").text, projectDirectory, "cfg");

							m_XboxImageXexPath.stringValue = filePath;
							filePath = FileUtil.GetProjectRelativePath(filePath);
							if (filePath != string.Empty)
								m_XboxImageXexPath.stringValue = filePath;

							serializedObject.ApplyModifiedProperties();
							GUIUtility.ExitGUI();
						}
					}

					EditorGUILayout.Space();

					// Xbox Live!
					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxLive"), EditorStyles.boldLabel);
					{
						// SPA file
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						float labelWidth = EditorGUIUtility.labelWidth;
						Rect buttonRect = new Rect(r.x + EditorGUI.indent, r.y, labelWidth - EditorGUI.indent, r.height);
						Rect fieldRect = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);

						string displaySpaPath = (m_XboxSpaPath.stringValue.Length == 0) ? "Not selected." : m_XboxSpaPath.stringValue;
						EditorGUI.TextArea(fieldRect, displaySpaPath, EditorStyles.label);

						if (GUI.Button(buttonRect, EditorGUIUtility.TextContent("PlayerSettings.XboxSpaFile")))
						{
							string filePath = EditorUtility.OpenFilePanel(
								EditorGUIUtility.TextContent("PlayerSettings.XboxSpaFile").text, projectDirectory, "spa");

							m_XboxSpaPath.stringValue = filePath;
							filePath = FileUtil.GetProjectRelativePath(filePath);
							if (filePath != string.Empty)
								m_XboxSpaPath.stringValue = filePath;

							// Check if title id is present
							if (m_XboxTitleId.stringValue.Length == 0)
								Debug.LogWarning("Title id must be present when using a SPA file.");

							serializedObject.ApplyModifiedProperties();
							GUIUtility.ExitGUI();
						}

						if (m_XboxSpaPath.stringValue.Length > 0)
						{
							bool prevVal = m_XboxGenerateSpa.boolValue;
							m_XboxGenerateSpa.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.XboxGenerateSPAConfig"), prevVal);
							if (!prevVal && m_XboxGenerateSpa.boolValue)
							{
								InternalEditorUtility.Xbox360GenerateSPAConfig(m_XboxSpaPath.stringValue);
							}
						}

						m_XboxEnableGuest.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.XboxEnableGuest"), m_XboxEnableGuest.boolValue);
					}

					EditorGUILayout.Space();

					// Services
					GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxServices"), EditorStyles.boldLabel);
					{
						m_XboxEnableAvatar.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.XboxAvatarEnable"), m_XboxEnableAvatar.boolValue);
					}

					KinectGUI();
				}
				if (targetGroup == BuildTargetGroup.Android)
				{

				string keystorePassword = PlayerSettings.keystorePass;
				string keystoreKeyPass = PlayerSettings.keyaliasPass;

				bool keystorePasswordOk = false;

				GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.KeystoreSubHeader"), EditorStyles.boldLabel);
				{
					{
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h + EditorGUI.kSpacing, h + EditorGUI.kSpacing, EditorStyles.layerMaskField, null);
						GUIContent[] content = { EditorGUIUtility.TextContent("PlayerSettings.UseExistingKeystore"),
												 EditorGUIUtility.TextContent("PlayerSettings.CreateNewKeystore") };
						bool newMode = GUI.SelectionGrid(r, m_KeystoreCreate ? 1 : 0, content, 2, "toggle") == 1 ? true : false;
						if (newMode != m_KeystoreCreate)
						{
							m_KeystoreCreate = newMode;
							m_AndroidKeystoreName.stringValue = "";
							m_AndroidKeyaliasName.stringValue = "";
							m_KeystoreAvailableKeys = null;
						}
					}

					{
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						GUIContent gc = null;
						if (m_AndroidKeystoreName.stringValue.Length == 0)
						{
							gc = EditorGUIUtility.TextContent("PlayerSettings.BrowseToSelectKeystoreName");
							EditorGUI.BeginDisabledGroup (true);
						}
						else
						{
							gc = EditorGUIUtility.TempContent(m_AndroidKeystoreName.stringValue);
							EditorGUI.BeginDisabledGroup (false);
						}

						Rect buttonRect;
						Rect fieldRect;

						float labelWidth = EditorGUIUtility.labelWidth;
						buttonRect = new Rect(r.x + EditorGUI.indent, r.y, labelWidth - EditorGUI.indent, r.height);
						fieldRect = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);

						EditorGUI.TextArea(fieldRect, gc.text, EditorStyles.label);

						EditorGUI.EndDisabledGroup ();

						if (GUI.Button(buttonRect, EditorGUIUtility.TextContent("PlayerSettings.BrowseKeystore")))
						{
							m_KeystoreAvailableKeys = null;

							string projectFolder = Directory.GetCurrentDirectory();
							if (m_KeystoreCreate)
								m_AndroidKeystoreName.stringValue = EditorUtility.SaveFilePanel(
											EditorGUIUtility.TextContent("PlayerSettings.CreateKeystoreDialog").text,
											projectFolder, "user.keystore", "keystore");
							else
								m_AndroidKeystoreName.stringValue = EditorUtility.OpenFilePanel(
											EditorGUIUtility.TextContent("PlayerSettings.OpenKeystoreDialog").text,
											projectFolder, "keystore");

							if (m_KeystoreCreate && File.Exists(m_AndroidKeystoreName.stringValue))
								FileUtil.DeleteFileOrDirectory(m_AndroidKeystoreName.stringValue);

							projectFolder += Path.DirectorySeparatorChar;
							if (m_AndroidKeystoreName.stringValue.StartsWith(projectFolder))
								m_AndroidKeystoreName.stringValue = m_AndroidKeystoreName.stringValue.Substring(projectFolder.Length);

							serializedObject.ApplyModifiedProperties();
							GUIUtility.ExitGUI();
						}
					}
					EditorGUI.BeginChangeCheck();
					{
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						keystorePassword = EditorGUI.PasswordField(r, EditorGUIUtility.TextContent("PlayerSettings.StorePass"), keystorePassword);
					}
					if (EditorGUI.EndChangeCheck())
					{
						Android.AndroidKeystoreWindow.GetAvailableKeys("", "");		// flush keystore
						m_KeystoreAvailableKeys = null;
					}
					{
						EditorGUI.BeginDisabledGroup (!m_KeystoreCreate);
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						m_KeystoreConfirm = EditorGUI.PasswordField(r, EditorGUIUtility.TextContent("PlayerSettings.StoreConfirm"), m_KeystoreConfirm);
						EditorGUI.EndDisabledGroup ();
					}
					{
						GUIContent gc = null;
						keystorePasswordOk = false;
						if (keystorePassword.Length == 0)
							gc = EditorGUIUtility.TextContent("PlayerSettings.EnterPassword");
						else if (keystorePassword.Length < 6)
							gc = EditorGUIUtility.TextContent("PlayerSettings.PasswordTooShort");
						else if (m_KeystoreCreate && m_KeystoreConfirm.Length == 0)
							gc = EditorGUIUtility.TextContent("PlayerSettings.ConfirmPassword");
						else if (m_KeystoreCreate && !keystorePassword.Equals(m_KeystoreConfirm))
							gc = EditorGUIUtility.TextContent("PlayerSettings.PasswordsDoNotMatch");
						else
						{
							gc = EditorGUIUtility.TempContent(" ");
							keystorePasswordOk = true;
						}
						GUILayout.Label(gc, EditorStyles.miniLabel);
					}

				}

				EditorGUILayout.Space();


				GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.KeyaliasSubHeader"), EditorStyles.boldLabel);
				{
					string[] strs = { EditorGUIUtility.TextContent("PlayerSettings.KeyaliasUnsigned").text };

					r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);

					if (keystorePasswordOk && !m_KeystoreCreate)
					{
						float labelWidth = EditorGUIUtility.labelWidth;
						Rect position = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);
						if (Event.current.type == EventType.MouseDown && Event.current.button == 0 && position.Contains(Event.current.mousePosition))
						{
							string keystore = Path.IsPathRooted(m_AndroidKeystoreName.stringValue) ? m_AndroidKeystoreName.stringValue
					   								: Path.Combine(Directory.GetCurrentDirectory(), m_AndroidKeystoreName.stringValue);
							m_KeystoreAvailableKeys = Android.AndroidKeystoreWindow.GetAvailableKeys(keystore, keystorePassword);
						}
					}
					else
					{
						Android.AndroidKeystoreWindow.GetAvailableKeys("", "");		// flush keystore
						m_KeystoreAvailableKeys = null;
					}

					int selectedKey = 0;
					if (m_KeystoreAvailableKeys != null && m_KeystoreAvailableKeys.Length != 0)
					{
						ArrayUtility.AddRange(ref strs, m_KeystoreAvailableKeys);
					}
					else if (m_AndroidKeyaliasName.stringValue.Length != 0)
					{
						ArrayUtility.Add(ref strs, m_AndroidKeyaliasName.stringValue);
					}

					for (int i = 0; i < strs.Length; ++i)
					{
						if (strs[i].Equals(m_AndroidKeyaliasName.stringValue))
							selectedKey = i;
					}

					bool addCreateKey = keystorePasswordOk && (m_KeystoreCreate && m_AndroidKeystoreName.stringValue.Length != 0 || m_KeystoreAvailableKeys != null);
					if (addCreateKey)
					{
						ArrayUtility.Add(ref strs, "");
						ArrayUtility.Add(ref strs, EditorGUIUtility.TextContent("PlayerSettings.CreateNewKey").text);
					}

					int newSelection = EditorGUI.Popup(r, EditorGUIUtility.TextContent("PlayerSettings.Keyalias"), selectedKey, EditorGUIUtility.TempContent(strs), EditorStyles.popup);

					if (addCreateKey && newSelection == strs.Length - 1)
					{
						newSelection = selectedKey;
						m_KeystoreCreate = false;
						string keystore = Path.IsPathRooted(m_AndroidKeystoreName.stringValue) ? m_AndroidKeystoreName.stringValue
				   								: Path.Combine(Directory.GetCurrentDirectory(), m_AndroidKeystoreName.stringValue);
						Android.AndroidKeystoreWindow.ShowAndroidKeystoreWindow(m_CompanyName.stringValue,
																				keystore,
																				PlayerSettings.keystorePass);
						GUIUtility.ExitGUI();
					}

					if (newSelection != selectedKey)
					{
						selectedKey = newSelection;
						m_AndroidKeyaliasName.stringValue = (selectedKey == 0) ? "" : strs[selectedKey];
					}

					{
						EditorGUI.BeginDisabledGroup (selectedKey == 0);
						r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
						keystoreKeyPass = EditorGUI.PasswordField(r, EditorGUIUtility.TextContent("PlayerSettings.KeyPass"), keystoreKeyPass);
						EditorGUI.EndDisabledGroup ();
					}

					{
						GUIContent gc = null;
						if (selectedKey == 0)
							gc = EditorGUIUtility.TempContent(" ");
						else if (keystoreKeyPass.Length == 0)
							gc = EditorGUIUtility.TextContent("PlayerSettings.EnterPassword");
						else if (keystoreKeyPass.Length < 6)
							gc = EditorGUIUtility.TextContent("PlayerSettings.PasswordTooShort");
						else
							gc = EditorGUIUtility.TempContent(" ");
						GUILayout.Label(gc, EditorStyles.miniLabel);
					}
				}
				PlayerSettings.keystorePass = keystorePassword;
				PlayerSettings.keyaliasPass = keystoreKeyPass;

				EditorGUILayout.Space();

				EditorGUILayout.PropertyField(m_APKExpansionFiles, EditorGUIUtility.TextContent("PlayerSettings.APKExpansionFiles"));
				EditorGUILayout.Space();
			}
			}
			EndSettingsBox();
		}

        private void KinectGUI()
        {
            // Kinect
            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxKinect"), EditorStyles.boldLabel);

            m_XboxEnableKinect.boolValue = EditorGUILayout.Toggle(EditorGUIUtility.TextContent("PlayerSettings.XboxEnableKinect"), m_XboxEnableKinect.boolValue);
            if (m_XboxEnableKinect.boolValue)
            {
                GUILayout.BeginHorizontal();
                GUILayout.Space(10);
                m_XboxEnableHeadOrientation.boolValue = GUILayout.Toggle(m_XboxEnableHeadOrientation.boolValue, new GUIContent("Head Orientation", "Head orientation support"));
                m_XboxEnableKinectAutoTracking.boolValue = GUILayout.Toggle(m_XboxEnableKinectAutoTracking.boolValue, new GUIContent("Auto Tracking", "Automatic player tracking"));
                m_XboxEnableFitness.boolValue = GUILayout.Toggle(m_XboxEnableFitness.boolValue, new GUIContent("Fitness", "Fitness support"));
                GUILayout.EndHorizontal();
                GUILayout.BeginHorizontal();
                GUILayout.Space(10);
                m_XboxEnableSpeech.boolValue = GUILayout.Toggle(m_XboxEnableSpeech.boolValue, new GUIContent("Speech", "Speech Recognition Support"));
                GUILayout.EndHorizontal();
                m_XboxDeployKinectResources.boolValue = true;
                if (m_XboxEnableHeadOrientation.boolValue)
                    m_XboxDeployHeadOrientation.boolValue = true;
            }

            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxKinectDeployResources"), EditorStyles.boldLabel);
            GUILayout.BeginHorizontal();
            GUILayout.Space(10);
            GUI.enabled = !m_XboxEnableKinect.boolValue;
            m_XboxDeployKinectResources.boolValue = GUILayout.Toggle(m_XboxDeployKinectResources.boolValue, new GUIContent("Base", "Identity and Skeleton Database files"));
            GUI.enabled = !(m_XboxEnableHeadOrientation.boolValue && m_XboxEnableKinect.boolValue);
            m_XboxDeployHeadOrientation.boolValue = GUILayout.Toggle(m_XboxDeployHeadOrientation.boolValue, new GUIContent("Head Orientation", "Head orientation database"));
            GUI.enabled = true;
            m_XboxDeployKinectHeadPosition.boolValue = GUILayout.Toggle(m_XboxDeployKinectHeadPosition.boolValue, new GUIContent("Head Position", "Head position database"));
            GUILayout.EndHorizontal();

            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxKinectSpeech"));
            GUILayout.BeginHorizontal();
            GUILayout.Space(10);
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 0)) != 0, new GUIContent("en-US", "Speech database: English - US, Canada")) != ((m_XboxSpeechDB.intValue & (1 << 0)) != 0) ? 1 << 0 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 1)) != 0, new GUIContent("fr-CA", "Speech database: French - Canada")) != ((m_XboxSpeechDB.intValue & (1 << 1)) != 0) ? 1 << 1 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 2)) != 0, new GUIContent("en-GB", "Speech database: English - United Kingdom, Ireland")) != ((m_XboxSpeechDB.intValue & (1 << 2)) != 0) ? 1 << 2 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 3)) != 0, new GUIContent("es-MX", "Speech database: Spanish - Mexico")) != ((m_XboxSpeechDB.intValue & (1 << 3)) != 0) ? 1 << 3 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 4)) != 0, new GUIContent("ja-JP", "Speech database: Japanese - Japan")) != ((m_XboxSpeechDB.intValue & (1 << 4)) != 0) ? 1 << 4 : 0;
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUILayout.Space(10);
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 5)) != 0, new GUIContent("fr-FR", "Speech database: French - France, Switzerland")) != ((m_XboxSpeechDB.intValue & (1 << 5)) != 0) ? 1 << 5 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 6)) != 0, new GUIContent("es-ES", "Speech database: Spanish - Spain")) != ((m_XboxSpeechDB.intValue & (1 << 6)) != 0) ? 1 << 6 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 7)) != 0, new GUIContent("de-DE", "Speech database: German - Germany, Austria, Switzerland")) != ((m_XboxSpeechDB.intValue & (1 << 7)) != 0) ? 1 << 7 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 8)) != 0, new GUIContent("it-IT", "Speech database: Italian - Italy")) != ((m_XboxSpeechDB.intValue & (1 << 8)) != 0) ? 1 << 8 : 0;
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 9)) != 0, new GUIContent("en-AU", "Speech database: English - Australia, New Zealand")) != ((m_XboxSpeechDB.intValue & (1 << 9)) != 0) ? 1 << 9 : 0;
            GUILayout.EndHorizontal();
            GUILayout.BeginHorizontal();
            GUILayout.Space(10);
            m_XboxSpeechDB.intValue ^= GUILayout.Toggle((m_XboxSpeechDB.intValue & (1 << 10)) != 0, new GUIContent("pt-BR", "Speech database: Portuguese - Brazil")) != ((m_XboxSpeechDB.intValue & (1 << 10)) != 0) ? 1 << 10 : 0;
            GUILayout.EndHorizontal();
        }

        private static void ShowWarning(GUIContent warningMessage)
        {
            if (s_WarningIcon == null)
                s_WarningIcon = EditorGUIUtility.LoadIcon("console.warnicon");

//            var c = new GUIContent(error) { image = s_WarningIcon };
            warningMessage.image = s_WarningIcon;

            GUILayout.Space(5);
            GUILayout.BeginVertical(EditorStyles.helpBox);
            GUILayout.Label(warningMessage, EditorStyles.wordWrappedMiniLabel);
            GUILayout.EndVertical();
        }

#if INCLUDE_METROSUPPORT
	    private Vector2 capScrollViewPosition = Vector2.zero;
        public void PublishSectionGUIMetro(float kLabelMinWidth, float kLabelMaxWidth, float kLabelMinHeight, float kLabelMaxHeight)
        {
            // packaging

            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.MetroPackaging"), EditorStyles.boldLabel);

            EditorGUILayout.PropertyField(m_MetroPackageName, EditorGUIUtility.TextContent("PlayerSettings.MetroPackageName"));
            m_MetroPackageName.stringValue = ValidateMetroPackageName(m_MetroPackageName.stringValue);

            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroPackageDisplayName"), new GUIContent(m_ProductName.stringValue));

            ImageField(m_MetroPackageLogo, EditorGUIUtility.TextContent("PlayerSettings.MetroPackageLogo"), kLabelMaxWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, 50, 50);

            EditorGUILayout.PropertyField(m_MetroPackageVersion, EditorGUIUtility.TextContent("PlayerSettings.MetroPackageVersion"));
            m_MetroPackageVersion.stringValue = ValidateMetroPackageVersion(m_MetroPackageVersion.stringValue);

            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroPackagePublisherDisplayName"), new GUIContent(m_CompanyName.stringValue));

            // certificate

            EditorGUILayout.Space();
            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.MetroCertificate"), EditorStyles.boldLabel);

            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroCertificatePublisher"), new GUIContent(PlayerSettings.Metro.certificateSubject));
            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroCertificateIssuer"), new GUIContent(PlayerSettings.Metro.certificateIssuer));
            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroCertificateNotAfter"), new GUIContent(PlayerSettings.Metro.certificateNotAfter.HasValue ? PlayerSettings.Metro.certificateNotAfter.Value.ToShortDateString() : null));

            {
                var bounds = GUILayoutUtility.GetRect(kLabelMinWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, EditorStyles.layerMaskField);
                var buttonBounds = new Rect((bounds.x + EditorGUIUtility.labelWidth), bounds.y, (bounds.width - EditorGUIUtility.labelWidth), bounds.height);

                GUIContent button;

                var path = PlayerSettings.Metro.certificatePath;

                if (string.IsNullOrEmpty(path))
                {
                    button = EditorGUIUtility.TextContent("PlayerSettings.MetroCertificateSelect");
                }
                else
                {
                    button = new GUIContent(FileUtil.GetLastPathNameComponent(path), path);
                }

                if (GUI.Button(buttonBounds, button))
                {
                    path = EditorUtility.OpenFilePanel(null, Application.dataPath, "pfx").Replace('\\', '/');
                    var relativePath = FileUtil.GetProjectRelativePath  (path);

                    if (string.IsNullOrEmpty(relativePath) && !string.IsNullOrEmpty(path))
                    {
                        Debug.LogError("Certificate path '" + Path.GetFullPath(path) +"' has to be relative to " + Path.GetFullPath(Application.dataPath + "\\.."));
                    }
                    else
                    {
                        try
                        {
                            if (!PlayerSettings.Metro.SetCertificate(path, null))
                            {
                                MetroCertificatePasswordWindow.Show(path);
                            }
                        }
                        catch (UnityException ex)
                        {
                            Debug.LogError(ex.Message);
                        }
                    }
                }
            }

            {
                var bounds = GUILayoutUtility.GetRect(kLabelMinWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, EditorStyles.layerMaskField);
                var buttonBounds = new Rect((bounds.x + EditorGUIUtility.labelWidth), bounds.y, (bounds.width - EditorGUIUtility.labelWidth), bounds.height);

                if (GUI.Button(buttonBounds, EditorGUIUtility.TextContent("PlayerSettings.MetroCertificateCreate")))
                {
                    MetroCreateTestCertificateWindow.Show(m_CompanyName.stringValue);
                }
            }

            // application ui

            EditorGUILayout.Space();
            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.MetroApplication"), EditorStyles.boldLabel);

            EditorGUILayout.LabelField(EditorGUIUtility.TextContent("PlayerSettings.MetroApplicationDisplayName"), new GUIContent(m_ProductName.stringValue));

            EditorGUILayout.PropertyField(m_MetroApplicationDescription, EditorGUIUtility.TextContent("PlayerSettings.MetroApplicationDescription"));
            m_MetroApplicationDescription.stringValue = ValidateMetroApplicationDescription(m_MetroApplicationDescription.stringValue);

            // tile

            EditorGUILayout.Space();
            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.MetroTile"), EditorStyles.boldLabel);

            ImageField(m_MetroTileLogo, EditorGUIUtility.TextContent("PlayerSettings.MetroTileLogo"), kLabelMaxWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, 150, 150);
            ImageField(m_MetroTileWideLogo, EditorGUIUtility.TextContent("PlayerSettings.MetroTileWideLogo"), kLabelMaxWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, 310, 150);
            ImageField(m_MetroTileSmallLogo, EditorGUIUtility.TextContent("PlayerSettings.MetroTileSmallLogo"), kLabelMaxWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, 30, 30);

            EditorGUILayout.PropertyField(m_MetroTileShortName, EditorGUIUtility.TextContent("PlayerSettings.MetroTileShortName")); // todo: limit to 13 characters
            m_MetroTileShortName.stringValue = ValidateMetroTileShortName(m_MetroTileShortName.stringValue);

            PlayerSettings.Metro.tileShowName = (PlayerSettings.MetroApplicationShowName)EditorGUILayout.EnumPopup(EditorGUIUtility.TextContent("PlayerSettings.MetroTileShowName"), PlayerSettings.Metro.tileShowName);

            PlayerSettings.Metro.tileForegroundText = (PlayerSettings.MetroApplicationForegroundText)EditorGUILayout.EnumPopup(EditorGUIUtility.TextContent("PlayerSettings.MetroTileForegroundText"), PlayerSettings.Metro.tileForegroundText);
            EditorGUILayout.PropertyField(m_MetroTileBackgroundColor, EditorGUIUtility.TextContent("PlayerSettings.MetroTileBackgroundColor"));

            // splash screen

            EditorGUILayout.Space();
            GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.MetroSplashScreen"), EditorStyles.boldLabel);

            ImageField(m_MetroSplashScreenImage, EditorGUIUtility.TextContent("PlayerSettings.MetroSplashScreenImage"), kLabelMaxWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, 620, 300);

            {
                var hasValue = PlayerSettings.Metro.splashScreenBackgroundColor.HasValue;
                var newHasValue = EditorGUILayout.BeginToggleGroup(EditorGUIUtility.TextContent("PlayerSettings.MetroSplashScreenOverwriteBackgroundColor"), hasValue);

                var bounds = GUILayoutUtility.GetRect(kLabelMinWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, EditorStyles.layerMaskField);

                var labelBounds = new Rect((bounds.x - EditorGUI.indent), bounds.y, (EditorGUIUtility.labelWidth - EditorGUI.indent), bounds.height);
                var colorBounds = new Rect((bounds.x + EditorGUIUtility.labelWidth), bounds.y, (bounds.width - EditorGUIUtility.labelWidth), bounds.height);

                EditorGUI.LabelField(labelBounds, EditorGUIUtility.TextContent("PlayerSettings.MetroSplashScreenBackgroundColor"));

                if (newHasValue != hasValue)
                {
                    if (newHasValue)
                    {
                        PlayerSettings.Metro.splashScreenBackgroundColor = PlayerSettings.Metro.tileBackgroundColor;
                    }
                    else
                    {
                        PlayerSettings.Metro.splashScreenBackgroundColor = null;
                    }
                }

                if (newHasValue)
                {
                    PlayerSettings.Metro.splashScreenBackgroundColor = EditorGUI.ColorField(colorBounds, PlayerSettings.Metro.splashScreenBackgroundColor.Value);
                }
                else
                {
                    EditorGUI.ColorField(colorBounds, PlayerSettings.Metro.tileBackgroundColor);
                }

                EditorGUILayout.EndToggleGroup();
            }
            EditorGUILayout.Space();
            GUILayout.Label("Compilation", EditorStyles.boldLabel);
            PlayerSettings.Metro.compilationOverrides = (PlayerSettings.MetroCompilationOverrides)EditorGUILayout.EnumPopup(EditorGUIUtility.TextContent("PlayerSettings.MetroCompilationOverrides"), PlayerSettings.Metro.compilationOverrides);

            GUILayout.Label("Capabilities", EditorStyles.boldLabel);
            capScrollViewPosition = GUILayout.BeginScrollView(capScrollViewPosition, EditorStyles.helpBox, GUILayout.MinHeight(200));
            foreach (PlayerSettings.MetroCapability cap in Enum.GetValues(typeof(PlayerSettings.MetroCapability)))
            {
                GUILayout.BeginHorizontal();
                bool capEnabled = GUILayout.Toggle (PlayerSettings.Metro.GetCapability (cap), cap.ToString (), GUILayout.MinWidth (150));
                PlayerSettings.Metro.SetCapability (cap, capEnabled);
                GUILayout.EndHorizontal();
            }
            GUILayout.EndScrollView();
            EditorGUILayout.Space();
        }

        private static void ImageField(SerializedProperty property, GUIContent label, float kLabelMinWidth, float kLabelMaxWidth, float kLabelMinHeight, float kLabelMaxHeight,
                                       int imageWidth, int imageHeight)
        {
            var bounds = GUILayoutUtility.GetRect(kLabelMinWidth, kLabelMaxWidth, kLabelMinHeight, kLabelMaxHeight, EditorStyles.layerMaskField);

            var labelBounds = new Rect((bounds.x - EditorGUI.indent), bounds.y, (EditorGUIUtility.labelWidth - EditorGUI.indent), bounds.height);
            var buttonBounds = new Rect((bounds.x + EditorGUIUtility.labelWidth), bounds.y, (bounds.width - EditorGUIUtility.labelWidth), bounds.height);

            EditorGUI.LabelField(labelBounds, label);

            GUIContent button;

            if (string.IsNullOrEmpty(property.stringValue))
            {
                button = EditorGUIUtility.TextContent("PlayerSettings.MetroImageSelect");
            }
            else
            {
                button = new GUIContent(FileUtil.GetLastPathNameComponent(property.stringValue), property.stringValue);
            }

            if (GUI.Button(buttonBounds, button))
            {
                var path = EditorUtility.OpenFilePanel(null, Application.dataPath, "png").Replace('\\', '/');
                if (string.IsNullOrEmpty(path))
                    property.stringValue = path;
                else if (ValidateImage(path, imageWidth, imageHeight))
                    property.stringValue = path;
                else
                {
                    property.stringValue = "";
                    return;
                }
                path = FileUtil.GetProjectRelativePath(path);

                if (!string.IsNullOrEmpty(path))
                {
                    property.stringValue = path;
                }
            }
        }

        private static bool ValidateImage(string imageFile, int width, int height)
        {
            Texture2D image = new Texture2D(1, 1);
            image.LoadImage(File.ReadAllBytes(imageFile));
            int imageWidth = image.width;
            int imageHeight = image.height;
            Destroy(image);
            if(imageWidth != width || imageHeight != height)
            {
                Debug.LogError(string.Format("Invalid image size ({0}x{1}), should be {2}x{3}",
                                             imageWidth, imageHeight, width, height));
                return false;
            }
            return true;
        }

        private string ValidateMetroPackageName(string value)
        {
            if (IsValidMetroPackageName(value))
            {
                return value;
            }
            else
            {
                return GetDefaultMetroPackageName();
            }
        }

        private static readonly Regex metroPackageNameRegex = new Regex(@"^[A-Za-z0-9\.\-]{2,49}[A-Za-z0-9\-]$", (RegexOptions.Compiled | RegexOptions.CultureInvariant));

        private static bool IsValidMetroPackageName(string value)
        {
            if (!metroPackageNameRegex.IsMatch(value))
            {
                return false;
            }

            switch (value.ToUpper())
            {
                case "CON":
                case "PRN":
                case "AUX":
                case "NUL":
                case "COM1":
                case "COM2":
                case "COM3":
                case "COM4":
                case "COM5":
                case "COM6":
                case "COM7":
                case "COM8":
                case "COM9":
                case "LPT1":
                case "LPT2":
                case "LPT3":
                case "LPT4":
                case "LPT5":
                case "LPT6":
                case "LPT7":
                case "LPT8":
                case "LPT9":
                    return false;
            }

            return true;
        }

        private string GetDefaultMetroPackageName()
        {
            var value = m_ProductName.stringValue;

            if (value != null)
            {
                var sb = new StringBuilder(value.Length);

                for (var i = 0; i < value.Length; ++i)
                {
                    var c = value[i];

                    if (char.IsLetterOrDigit(c) || (c == '-'))
                    {
                        sb.Append(c);
                    }
                    else if ((c == '.') && (i != (value.Length - 1)))
                    {
                        sb.Append(c);
                    }
                }

                value = sb.ToString();
            }

            if (!IsValidMetroPackageName(value))
            {
                value = "DefaultPackageName";
            }

            return value;
        }

        private static readonly Regex metroPackageVersionRegex = new Regex(@"^(\d+)\.(\d+)\.(\d+)\.(\d+)$", (RegexOptions.Compiled | RegexOptions.CultureInvariant));

        internal static string ValidateMetroPackageVersion(string value)
        {
            if (metroPackageVersionRegex.IsMatch(value))
            {
                return value;
            }
            else
            {
                return "1.0.0.0";
            }
        }

        private string ValidateMetroApplicationDescription(string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                return m_ProductName.stringValue;
            }
            else
            {
                return value;
            }
        }

        private string ValidateMetroTileShortName(string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                value = m_ProductName.stringValue;
            }

            if ((value != null) && (value.Length > 13))
            {
                return value.Substring(0, 13).TrimEnd(' ');
            }
            else
            {
                return value;
            }
        }

#endif

        public void PublishSectionGUIWii(float minW, float maxW, float minH, float maxH)
		{
			m_WiiRegion = serializedObject.FindProperty("wiiRegion");
			m_WiiGameCode = serializedObject.FindProperty("wiiGameCode");
			m_WiiGameVersion = serializedObject.FindProperty("wiiGameVersion");
			m_WiiCompanyCode = serializedObject.FindProperty("wiiCompanyCode");

			EditorGUILayout.PropertyField(m_WiiRegion, EditorGUIUtility.TextContent("PlayerSettings.wiiRegion"));
			EditorGUILayout.Space();

			EditorGUILayout.PropertyField(m_WiiGameCode, EditorGUIUtility.TextContent("PlayerSettings.wiiGameCode"));
			EditorGUILayout.Space();

			EditorGUILayout.PropertyField(m_WiiGameVersion, EditorGUIUtility.TextContent("PlayerSettings.wiiGameVersion"));
			EditorGUILayout.Space();

			EditorGUILayout.PropertyField(m_WiiCompanyCode, EditorGUIUtility.TextContent("PlayerSettings.wiiCompanyCode"));
			EditorGUILayout.Space();

			/*

			// Submission
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.SubmissionSubHeader"), EditorStyles.boldLabel);
			{
				// Title id
				EditorGUILayout.PropertyField(m_XboxTitleId, EditorGUIUtility.TextContent("PlayerSettings.XboxTitleId"));
			}

			EditorGUILayout.Space();

			// Image conversion
			GUILayout.Label(EditorGUIUtility.TextContent("PlayerSettings.XboxImageConversion"), EditorStyles.boldLabel);
			{
				// ImageXex configuration override file
				r = GUILayoutUtility.GetRect(kLabelFloatMinW, kLabelFloatMaxW, h, h, EditorStyles.layerMaskField, null);
				float labelWidth = EditorGUIUtility.labelWidth;
				Rect buttonRect = new Rect(r.x + EditorGUI.indent, r.y, labelWidth - EditorGUI.indent, r.height);
				Rect fieldRect = new Rect(r.x + labelWidth, r.y, r.width - labelWidth, r.height);

				string displayImageXexPath = (m_XboxImageXexPath.stringValue.Length == 0) ? "Not selected." : m_XboxImageXexPath.stringValue;
				EditorGUI.TextArea(fieldRect, displayImageXexPath, EditorStyles.label);

				if (GUI.Button(buttonRect, EditorGUIUtility.TextContent("PlayerSettings.XboxImageXEXFile")))
				{
					m_XboxImageXexPath.stringValue = EditorUtility.OpenFilePanel(
						EditorGUIUtility.TextContent("PlayerSettings.XboxImageXEXFile").text, projectDirectory, "cfg");

					serializedObject.ApplyModifiedProperties();
					GUIUtility.ExitGUI();
				}
			}*/
		}

		static string PrettyTemplateKeyName (string name)
		{
			string[] elements = name.Split ('_');

			elements[0] = UppercaseFirst (elements[0].ToLower ());
			for (int i = 1; i < elements.Length; i++)
			{
				elements[i] = elements[i].ToLower ();
			}

			return string.Join (" ", elements);
		}

		static string UppercaseFirst (string target)
		{
			if (string.IsNullOrEmpty (target))
			{
				return string.Empty;
			}
			return char.ToUpper (target[0]) + target.Substring (1);
		}

		static int ThumbnailList (Rect rect, int selection, GUIContent[] thumbnails, int maxRowItems)
		{
			for (int y = 0, i = 0; i < thumbnails.Length; y++)
			{
				for (int x = 0; x < maxRowItems && i < thumbnails.Length; x++, i++)
				{
					if (ThumbnailListItem (
						new Rect (rect.x + x * kThumbnailSize, rect.y + y * (kThumbnailSize + kThumbnailLabelHeight), kThumbnailSize, (kThumbnailSize + kThumbnailLabelHeight)),
						i == selection, thumbnails[i]))
					{
						selection = i;
					}
				}
			}
			return selection;
		}

		static bool ThumbnailListItem (Rect rect, bool selected, GUIContent content)
		{
			switch (Event.current.type)
			{
				case EventType.MouseDown:
					if (rect.Contains (Event.current.mousePosition))
					{
						if (!selected)
						{
							GUI.changed = true;
						}
						selected = true;
						Event.current.Use ();
					}
				break;
				case EventType.Repaint:
					Rect thumbRect = new Rect (rect.x + kThumbnailPadding, rect.y + kThumbnailPadding, rect.width - kThumbnailPadding * 2.0f, rect.height - kThumbnailLabelHeight - kThumbnailPadding * 2.0f);
					s_Styles.thumbnail.Draw (thumbRect, content.image, false, false, selected, selected);
					s_Styles.thumbnailLabel.Draw (new Rect (rect.x, rect.y + rect.height - kThumbnailLabelHeight, rect.width, kThumbnailLabelHeight), content.text, false, false, selected, selected);
				break;
			}
			return selected;
		}

		internal class WebPlayerTemplate : System.Object
		{
			private static WebPlayerTemplate[] s_Templates = null;
			private static GUIContent[] s_TemplateGUIThumbnails = null;

			private string m_Path, m_Name;
			private Texture2D m_Thumbnail;
			private string[] m_CustomKeys;

			public string[] CustomKeys
			{
				get
				{
					return m_CustomKeys;
				}
			}

			public static WebPlayerTemplate[] Templates
			{
				get
				{
					if (s_Templates == null || s_TemplateGUIThumbnails == null)
					{
						BuildTemplateList ();
					}

					return s_Templates;
				}
			}

			public static GUIContent[] TemplateGUIThumbnails
			{
				get
				{
					if (s_Templates == null || s_TemplateGUIThumbnails == null)
					{
						BuildTemplateList ();
					}

					return s_TemplateGUIThumbnails;
				}
			}

			public static int GetTemplateIndex (string path)
			{
				for (int i = 0; i < Templates.Length; i++)
				{
					if (path.Equals (Templates [i].ToString ()))
					{
						return i;
					}
				}
				return 0;
			}

			public static void ClearTemplates ()
			{
				s_Templates = null;
				s_TemplateGUIThumbnails = null;
			}

			private static void BuildTemplateList ()
			{
				List<WebPlayerTemplate> templates = new List<WebPlayerTemplate> ();

				string customTemplates = Path.Combine (
					Application.dataPath.Replace ('/', Path.DirectorySeparatorChar), "WebPlayerTemplates");
				if (Directory.Exists (customTemplates))
				{
					templates.AddRange (ListTemplates (customTemplates));
				}

				string builtInTemplates = Path.Combine(Path.Combine(
					EditorApplication.applicationContentsPath.Replace('/', Path.DirectorySeparatorChar), "Resources"), "WebPlayerTemplates");
				if (Directory.Exists (builtInTemplates))
				{
					templates.AddRange (ListTemplates (builtInTemplates));
				}
				else
				{
					Debug.LogError ("Did not find built-in templates.");
				}

				s_Templates = templates.ToArray ();

				s_TemplateGUIThumbnails = new GUIContent [s_Templates.Length];
				for (int i = 0; i < s_TemplateGUIThumbnails.Length; i++)
				{
					s_TemplateGUIThumbnails[i] = s_Templates[i].ToGUIContent ();
				}
			}

			private static WebPlayerTemplate Load (string path)
			{
				if (!Directory.Exists (path) || Directory.GetFiles (path, "index.*").Length < 1)
				{
					return null;
				}

				string[] splitPath = path.Split (Path.DirectorySeparatorChar);

				WebPlayerTemplate template = new WebPlayerTemplate ();

				template.m_Name = splitPath[splitPath.Length - 1];
				if (splitPath.Length > 3 && splitPath[splitPath.Length - 3].Equals ("Assets"))
				{
					template.m_Path = "PROJECT:" + template.m_Name;
				}
				else
				{
					template.m_Path = "APPLICATION:" + template.m_Name;
				}

				string[] thumbFiles = Directory.GetFiles (path, "thumbnail.*");
				if (thumbFiles.Length > 0)
				{
					template.m_Thumbnail = new Texture2D (2, 2);
					template.m_Thumbnail.LoadImage (File.ReadAllBytes (thumbFiles[0]));
				}

				List<string> keys = new List<string> ();
				Regex customKeyFinder = new Regex ("\\%UNITY_CUSTOM_([A-Z_]+)\\%");
				MatchCollection matches = customKeyFinder.Matches (File.ReadAllText (Directory.GetFiles (path, "index.*")[0]));
				foreach (Match match in matches)
				{
					string name = match.Value.Substring ("%UNITY_CUSTOM_".Length);
					name = name.Substring (0, name.Length -1);
					if (!keys.Contains (name))
					{
						keys.Add (name);
					}
				}
				template.m_CustomKeys = keys.ToArray ();

				return template;
			}

			private static List<WebPlayerTemplate> ListTemplates (string path)
			{
				List<WebPlayerTemplate> templates = new List<WebPlayerTemplate> ();
				string[] directories = Directory.GetDirectories (path);
				foreach (string directory in directories)
				{
					WebPlayerTemplate template = Load (directory);
					if (template != null)
					{
						templates.Add (template);
					}
				}
				return templates;
			}

			public override bool Equals (System.Object other)
			{
				return other is WebPlayerTemplate && other.ToString ().Equals (ToString ());
			}

			public override int GetHashCode()
			{
				return base.GetHashCode () ^ m_Path.GetHashCode ();
			}

			public override string ToString ()
			{
				return m_Path;
			}

			public GUIContent ToGUIContent ()
			{
				return new GUIContent (m_Name, m_Thumbnail == null ? (Texture2D)EditorGUIUtility.IconContent (kWebPlayerTemplateDefaultIconResource).image : m_Thumbnail);
			}
		}
	}
}
