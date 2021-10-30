#ifndef EDITOR_ONLY_PROJECTSETTINGS_H
#define EDITOR_ONLY_PROJECTSETTINGS_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Math/ColorSpaceConversion.h"

class Texture2D;

struct EditorOnlyPlayerSettingsNotPersistent
{
	UnityStr	AndroidKeystorePass;
	UnityStr	AndroidKeyaliasPass;

	//This keeps the current ColorSpace state, this is different then PlayerSettings::m_ActiveColorSpace:
	//when Awaking the PlayerSettings it is possible that the PlayerSettings::m_ActiveColorSpace has changed but not the actual state yet.
	ColorSpace	CurrentColorSpace;
};

#if DOXYGEN
// We serialize EditorOnlyPlayerSettings directly in PlayerSettings.
// In order to not lose the enum parameters we do this hack
struct PlayerSettings
#else
struct EditorOnlyPlayerSettings
#endif
{
	typedef std::map<UnityStr, UnityStr> WebTemplateTags;
	typedef std::vector<std::string> WebTemplateKeys;

	struct IconWithSize
	{
		PPtr<Texture2D> m_Icon;
		int m_Size;
		DECLARE_SERIALIZE(IconWithSize)
	};

	struct BuildTargetIcons
	{
		UnityStr        m_BuildTarget;
		std::vector<IconWithSize> m_Icons;
		DECLARE_SERIALIZE(BuildTargetIcons)
	};

	struct BuildTargetBatching
	{
		UnityStr		m_BuildTarget;
		bool			m_StaticBatching;
		bool			m_DynamicBatching;
		DECLARE_SERIALIZE(BuildTargetBatching)
	};

	EditorOnlyPlayerSettings ();

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);

	UnityGUID	productGUID;

	UnityStr	AndroidKeystoreName;
	UnityStr	AndroidKeyaliasName;

	int			AndroidSplashScreenScale; ///< enum { Center (only scale down) = 0, Scale to fit (letter-boxed) = 1, Scale to fill (cropped) = 2 }
	int			AndroidTargetDevice; ///< enum { ARMv7 only = 0 }
	bool        forceAndroidInternetPermission;
	bool        forceAndroidSDCardPermission;
	bool        createAndroidWallpaper;
	bool        enableAndroidExpansionFiles;

	int			AndroidBundleVersionCode;
	int         AndroidMinSdkVersion; ///< enum { Android 2.3.1 'Gingerbread' (API level 9) = 9, Android 2.3.3 'Gingerbread' (API level 10) = 10, Android 3.0 'Honeycomb' (API level 11) = 11, Android 3.1 'Honeycomb' (API level 12) = 12, Android 3.2 'Honeycomb' (API level 13) = 13, Android 4.0 'Ice Cream Sandwich' (API level 14) = 14, Android 4.0.3 'Ice Cream Sandwich' (API level 15) = 15, Android 4.1 'Jelly Bean' (API level 16) = 16, Android 4.2 'Jelly Bean' (API level 17) = 17, Android 4.3 'Jelly Bean' (API level 18) = 18 }
	int         AndroidPreferredInstallLocation; ///< enum { Automatic = 0, Prefer External = 1, Force Internal = 2 }
	bool		uIExitOnSuspend;
	bool		uIStatusBarHidden;
	int			uIStatusBarStyle; ///< enum { Default = 0, Black Translucent = 1, Black Opaque = 2 }
	bool		uIPrerenderedIcon;
	bool		uIRequiresPersistentWiFi;

	int         iPhoneStrippingLevel; ///< enum { Disabled = 0, Strip Assemblies = 1, Strip ByteCode = 2, Use micro mscorlib = 3 }
	int         iPhoneScriptCallOptimization; ///< enum { Slow and Safe = 0, Fast but no Exceptions = 1 }
	UnityStr    iPhoneBundleVersion;
	int			iPhoneSdkVersion; ///< enum { Device SDK = 988, Simulator SDK = 989 }
	int			iPhoneTargetOSVersion; ///< enum { 4.0 = 10, 4.1 = 12, 4.2 = 14, 4.3 = 16, 5.0 = 18, 5.1 = 20, 6.0 = 22, Unknown = 999 }

	UnityStr    aotOptions;

	int			apiCompatibilityLevel; ///< enum { .NET 2.0 = 1, .NET 2.0 Subset = 2 }

	bool        stripUnusedMeshComponents;

	PPtr<Texture2D> iPhoneSplashScreen;
	PPtr<Texture2D> iPhoneHighResSplashScreen;
	PPtr<Texture2D> iPhoneTallHighResSplashScreen;
	PPtr<Texture2D> iPadPortraitSplashScreen;
	PPtr<Texture2D> iPadHighResPortraitSplashScreen;
	PPtr<Texture2D> iPadLandscapeSplashScreen;
	PPtr<Texture2D> iPadHighResLandscapeSplashScreen;
	std::vector<BuildTargetIcons> m_BuildTargetIcons;
	std::vector<BuildTargetBatching> m_BuildTargetBatching;

	// Wii
	int wiiRegion; ///< enum { Japan = 1, USA = 2, Europe= 3, China = 4, Korea = 5, Taiwan = 6 }
	UnityStr wiiGameCode;
	UnityStr wiiGameVersion;
	UnityStr wiiCompanyCode;
	bool wiiSupportsNunchuk;
	bool wiiSupportsClassicController;
	bool wiiSupportsBalanceBoard;
	bool wiiSupportsMotionPlus;
	int wiiControllerCount;
	bool wiiFloatingPointExceptions;
	bool wiiScreenCrashDumps;

	// Xbox 360
	UnityStr XboxTitleId;
	UnityStr XboxImageXexPath;
	UnityStr XboxSpaPath;
	int			XboxGenerateSpa;
	int			XboxDeployKinectResources;
	PPtr<Texture2D> XboxSplashScreen;
	bool		xboxEnableSpeech; // Xbox360 Kinect
	int			xboxAdditionalTitleMemorySize;
	bool		xboxDeployKinectHeadOrientation;
	bool		xboxDeployKinectHeadPosition;

	// PS3
	UnityStr ps3TitleConfigPath;
	UnityStr ps3DLCConfigPath;
	UnityStr ps3ThumbnailPath;
	UnityStr ps3BackgroundPath;
	UnityStr ps3SoundPath;
	UnityStr ps3TrophyCommId;
	UnityStr ps3NpCommunicationPassphrase;
	UnityStr ps3TrophyPackagePath;
	int			ps3BootCheckMaxSaveGameSizeKB;
	UnityStr ps3TrophyCommSig;
	bool		ps3TrialMode;
	int			ps3SaveGameSlots;

	// Flash
	int			flashStrippingLevel;	///< enum { Disabled = 0, Strip ByteCode = 2}

	// Metro
	UnityStr   metroPackageName;
	UnityStr   metroPackageLogo;
	UnityStr   metroPackageVersion;
	UnityStr   metroCertificatePath;
	UnityStr   metroCertificatePassword;
	UnityStr   metroCertificateSubject;
	UnityStr   metroCertificateIssuer;
	SInt64		metroCertificateNotAfter;
	UnityStr   metroApplicationDescription;
	UnityStr   metroTileLogo;
	UnityStr   metroTileWideLogo;
	UnityStr   metroTileSmallLogo;
	UnityStr   metroTileShortName;
	UnityStr   metroCommandLineArgsFile;
	int metroTileShowName;	///< enum { (not set) = 0, All logos = 1, No logos = 2, Standard logo only = 3, Wide logo only = 4 }
	int metroTileForegroundText;	///< enum { Light = 1, Dark = 2 }
	ColorRGBAf metroTileBackgroundColor;
	UnityStr   metroSplashScreenImage;
	bool metroSplashScreenUseBackgroundColor;
	int metroCompilationOverrides; ///< enum { None = 0, C# against .NET Core = 1 }
	ColorRGBAf metroSplashScreenBackgroundColor;
	typedef std::map<UnityStr, bool> MetroCapbilitiesMap;
	MetroCapbilitiesMap metroCapabilities;

	// BlackBerry
	int blackberryBuildId;
    UnityStr blackberryDeviceAddress;
	UnityStr blackberryDevicePassword;
	UnityStr blackberryTokenPath;
	UnityStr blackberryTokenExires;
	UnityStr blackberryTokenAuthor;
	UnityStr blackberryTokenAuthorId;
	UnityStr blackberryAuthorId;
	UnityStr blackberryCskPassword;
	UnityStr blackberrySaveLogPath;
	bool blackberryAuthorIdOveride;
	bool blackberrySharedPermissions;
	bool blackberryCameraPermissions;
	bool blackberryGPSPermissions;
	bool blackberryDeviceIDPermissions;
	bool blackberryMicrophonePermissions;
	bool blackberryGamepadSupport;
	
	PPtr<Texture2D> blackberryLandscapeSplashScreen;
	PPtr<Texture2D> blackberryPortraitSplashScreen;
	PPtr<Texture2D> blackberrySquareSplashScreen;

    // Tizen
	UnityStr tizenProductDescription;
	UnityStr tizenProductURL;
	UnityStr tizenCertificatePath;
	UnityStr tizenCertificatePassword;
	UnityStr tizenSaveLogPath;

#if ENABLE_SPRITES
	// Sprites
	UnityStr spritePackerPolicy;
#endif

	// Scripting
	std::map<int,UnityStr> scriptingDefineSymbols;

	PPtr<Texture2D> resolutionDialogBanner;
	UnityStr m_WebPlayerTemplate;
	WebTemplateTags m_TemplateCustomTags;

	bool SetPlatformIcons(const std::string& platform, std::vector<PPtr<Texture2D> > icons);
	void ClearPlatformIcons(const std::string& platform);
	Texture2D* GetPlatformIconForSize(const std::string& platform, int size);
	void GetPlatformBatching (BuildTargetPlatform platform, bool* outStaticBatching, bool* outDynamicBatching) const;
	void SetPlatformBatching (BuildTargetPlatform platform, bool staticBatching, bool dynamicBatching);
	std::vector<int> GetPlatformIconSizes (const std::string& platform);
	std::vector<PPtr<Texture2D> > GetPlatformIcons(const std::string& platform);
	std::vector<EditorOnlyPlayerSettings::IconWithSize> GetBestIconWithSizes (const std::string& platform);
	void SetTemplateCustomKeys (std::vector<std::string> keys);
	bool SetTemplateCustomValue (std::string key, std::string value);
	std::string GetTemplateCustomValue (std::string key);
	WebTemplateKeys GetTemplateCustomKeys();
	std::string GetUserScriptingDefineSymbolsForGroup (int targetGroup) const;
	void SetUserScriptingDefineSymbolsForGroup (int targetGroup, const std::string &defines);
	void SetMetroCapability(const std::string& name, bool enabled);
	bool GetMetroCapability(const std::string& name) const;
	void SetApiCompatibility (int value);
};

template<class TransferFunction>
void EditorOnlyPlayerSettings::Transfer (TransferFunction& transfer)
{
	TRANSFER_EDITOR_ONLY(productGUID);

	transfer.Transfer (iPhoneBundleVersion, "iPhoneBundleVersion");
	transfer.Transfer (AndroidBundleVersionCode, "AndroidBundleVersionCode");
	transfer.Transfer (AndroidMinSdkVersion, "AndroidMinSdkVersion");
	if (AndroidMinSdkVersion < 9)
		AndroidMinSdkVersion = 9; // Android 2.3.1 'Gingerbread' (API level 9)
	transfer.Transfer (AndroidPreferredInstallLocation, "AndroidPreferredInstallLocation");
	transfer.Transfer (aotOptions, "aotOptions");

	transfer.Transfer (apiCompatibilityLevel, "apiCompatibilityLevel");
	// we don't support kNET_1_1 anymore. if found, set to kNET_Small
	if (apiCompatibilityLevel == 0)
		apiCompatibilityLevel = 2;// kNET_Small

	transfer.Transfer (iPhoneStrippingLevel, "iPhoneStrippingLevel");
	transfer.Transfer (iPhoneScriptCallOptimization, "iPhoneScriptCallOptimization");
	transfer.Transfer (forceAndroidInternetPermission, "ForceInternetPermission");
	transfer.Transfer (forceAndroidSDCardPermission, "ForceSDCardPermission");
	transfer.Transfer (createAndroidWallpaper, "CreateWallpaper");
	transfer.Transfer (enableAndroidExpansionFiles, "APKExpansionFiles");
	transfer.Transfer (stripUnusedMeshComponents, "StripUnusedMeshComponents");
	transfer.Align();

	transfer.Transfer (iPhoneSdkVersion, "iPhoneSdkVersion");
	// we moved from setting sdk version in here to set device/simulator and selecting real sdk in xcode
	// where latest is used by default
	if (iPhoneSdkVersion < 988)
		iPhoneSdkVersion = 988;

	transfer.Transfer (iPhoneTargetOSVersion, "iPhoneTargetOSVersion");
	if (iPhoneTargetOSVersion < 10)
		iPhoneTargetOSVersion = 10;

	TRANSFER (uIPrerenderedIcon);
	TRANSFER (uIRequiresPersistentWiFi);
	TRANSFER (uIStatusBarHidden);
	TRANSFER (uIExitOnSuspend);

	transfer.Transfer (uIStatusBarStyle, "uIStatusBarStyle");

	transfer.Transfer (iPhoneSplashScreen, "iPhoneSplashScreen");
	transfer.Transfer (iPhoneHighResSplashScreen, "iPhoneHighResSplashScreen");
	transfer.Transfer (iPhoneTallHighResSplashScreen, "iPhoneTallHighResSplashScreen");
	transfer.Transfer (iPadPortraitSplashScreen, "iPadPortraitSplashScreen");
	transfer.Transfer (iPadHighResPortraitSplashScreen, "iPadHighResPortraitSplashScreen");
	transfer.Transfer (iPadLandscapeSplashScreen, "iPadLandscapeSplashScreen");
	transfer.Transfer (iPadHighResLandscapeSplashScreen, "iPadHighResLandscapeSplashScreen");

	transfer.Transfer (AndroidTargetDevice, "AndroidTargetDevice");
	transfer.Transfer (AndroidSplashScreenScale, "AndroidSplashScreenScale");
	if (AndroidTargetDevice == 1/*ARMv6 with VFP*/ || AndroidTargetDevice == 2/*Emulator*/)  //deprecated
		AndroidTargetDevice = 0;/*ARMv7 only*/

	transfer.Transfer (AndroidKeystoreName, "AndroidKeystoreName");
	transfer.Transfer (AndroidKeyaliasName, "AndroidKeyaliasName");

	transfer.Transfer(resolutionDialogBanner, "resolutionDialogBanner");
	TRANSFER (m_BuildTargetIcons);
	TRANSFER (m_BuildTargetBatching);
	transfer.Transfer (m_WebPlayerTemplate, "webPlayerTemplate");
	TRANSFER (m_TemplateCustomTags);

	// Wii
	TRANSFER_EDITOR_ONLY(wiiRegion);
	TRANSFER_EDITOR_ONLY(wiiGameCode);
	TRANSFER_EDITOR_ONLY(wiiGameVersion);
	TRANSFER_EDITOR_ONLY(wiiCompanyCode);
	TRANSFER_EDITOR_ONLY(wiiSupportsNunchuk);
	TRANSFER_EDITOR_ONLY(wiiSupportsClassicController);
	TRANSFER_EDITOR_ONLY(wiiSupportsBalanceBoard);
	TRANSFER_EDITOR_ONLY(wiiSupportsMotionPlus);
	TRANSFER_EDITOR_ONLY(wiiControllerCount);
	TRANSFER_EDITOR_ONLY(wiiFloatingPointExceptions);
	TRANSFER_EDITOR_ONLY(wiiScreenCrashDumps);
	transfer.Align();

	// Xbox 360
	TRANSFER(XboxTitleId);
	TRANSFER(XboxImageXexPath);
	TRANSFER(XboxSpaPath);
	TRANSFER(XboxGenerateSpa);
	TRANSFER(XboxDeployKinectResources);
	TRANSFER(XboxSplashScreen);
	TRANSFER(xboxEnableSpeech);
	transfer.Align();
	TRANSFER_EDITOR_ONLY(xboxAdditionalTitleMemorySize);
	TRANSFER_EDITOR_ONLY(xboxDeployKinectHeadOrientation);
	TRANSFER_EDITOR_ONLY(xboxDeployKinectHeadPosition);
	transfer.Align();

	// PS3
	TRANSFER_EDITOR_ONLY(ps3TitleConfigPath);
	TRANSFER_EDITOR_ONLY(ps3DLCConfigPath);
	TRANSFER_EDITOR_ONLY(ps3ThumbnailPath);
	TRANSFER_EDITOR_ONLY(ps3BackgroundPath);
	TRANSFER_EDITOR_ONLY(ps3SoundPath);
	TRANSFER_EDITOR_ONLY(ps3TrophyCommId);
	TRANSFER_EDITOR_ONLY(ps3NpCommunicationPassphrase);
	TRANSFER_EDITOR_ONLY(ps3TrophyPackagePath);
	TRANSFER_EDITOR_ONLY(ps3BootCheckMaxSaveGameSizeKB);
	TRANSFER_EDITOR_ONLY(ps3TrophyCommSig);
	TRANSFER_EDITOR_ONLY(ps3SaveGameSlots);
	TRANSFER_EDITOR_ONLY(ps3TrialMode);
	transfer.Align();

	// Flash
	TRANSFER(flashStrippingLevel);

#if ENABLE_SPRITES
	// Sprites
	TRANSFER_EDITOR_ONLY(spritePackerPolicy);
#endif

	// Scripting
	TRANSFER_EDITOR_ONLY(scriptingDefineSymbols);

	// Metro
	TRANSFER_EDITOR_ONLY(metroPackageName);
	TRANSFER_EDITOR_ONLY(metroPackageLogo);
	TRANSFER_EDITOR_ONLY(metroPackageVersion);
	TRANSFER_EDITOR_ONLY(metroCertificatePath);
	TRANSFER_EDITOR_ONLY(metroCertificatePassword);
	TRANSFER_EDITOR_ONLY(metroCertificateSubject);
	TRANSFER_EDITOR_ONLY(metroCertificateIssuer);
	TRANSFER_EDITOR_ONLY(metroCertificateNotAfter);
	TRANSFER_EDITOR_ONLY(metroApplicationDescription);
	TRANSFER_EDITOR_ONLY(metroTileLogo);
	TRANSFER_EDITOR_ONLY(metroTileWideLogo);
	TRANSFER_EDITOR_ONLY(metroTileSmallLogo);
	TRANSFER_EDITOR_ONLY(metroTileShortName);
	TRANSFER_EDITOR_ONLY(metroCommandLineArgsFile);
	TRANSFER_EDITOR_ONLY(metroTileShowName);
	TRANSFER_EDITOR_ONLY(metroTileForegroundText);
	TRANSFER_EDITOR_ONLY(metroTileBackgroundColor);
	TRANSFER_EDITOR_ONLY(metroSplashScreenImage);
	TRANSFER_EDITOR_ONLY(metroSplashScreenBackgroundColor);
	TRANSFER_EDITOR_ONLY(metroSplashScreenUseBackgroundColor);
	transfer.Align();
	TRANSFER_EDITOR_ONLY(metroCapabilities);
	transfer.Align();
	TRANSFER_EDITOR_ONLY(metroCompilationOverrides);
	transfer.Align();

	// BlackBerry
	TRANSFER_EDITOR_ONLY(blackberryDeviceAddress);
	TRANSFER_EDITOR_ONLY(blackberryDevicePassword);
	TRANSFER_EDITOR_ONLY(blackberryTokenPath);
	TRANSFER_EDITOR_ONLY(blackberryTokenExires);
	TRANSFER_EDITOR_ONLY(blackberryTokenAuthor);
	TRANSFER_EDITOR_ONLY(blackberryTokenAuthorId);
	TRANSFER_EDITOR_ONLY(blackberryAuthorId);
	TRANSFER_EDITOR_ONLY(blackberryCskPassword);
	TRANSFER_EDITOR_ONLY(blackberrySaveLogPath);
	TRANSFER_EDITOR_ONLY(blackberryAuthorIdOveride);
	TRANSFER_EDITOR_ONLY(blackberrySharedPermissions);
	TRANSFER_EDITOR_ONLY(blackberryCameraPermissions);
	TRANSFER_EDITOR_ONLY(blackberryGPSPermissions);
	TRANSFER_EDITOR_ONLY(blackberryDeviceIDPermissions);
	TRANSFER_EDITOR_ONLY(blackberryMicrophonePermissions);
	TRANSFER_EDITOR_ONLY(blackberryGamepadSupport);
	transfer.Align();
	TRANSFER_EDITOR_ONLY(blackberryBuildId);
	TRANSFER_EDITOR_ONLY(blackberryLandscapeSplashScreen);
	TRANSFER_EDITOR_ONLY(blackberryPortraitSplashScreen);
	TRANSFER_EDITOR_ONLY(blackberrySquareSplashScreen);

    // Tizen
	TRANSFER_EDITOR_ONLY(tizenProductDescription);
	TRANSFER_EDITOR_ONLY(tizenProductURL);
	TRANSFER_EDITOR_ONLY(tizenCertificatePath);
	TRANSFER_EDITOR_ONLY(tizenCertificatePassword);
	TRANSFER_EDITOR_ONLY(tizenSaveLogPath);
}

std::string GetDefaultCompanyName ();
std::string GetDefaultProductName ();

template<class TransferFunction>
inline void EditorOnlyPlayerSettings::IconWithSize::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_Icon);
	TRANSFER (m_Size);
}

template<class TransferFunction>
inline void EditorOnlyPlayerSettings::BuildTargetIcons::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_BuildTarget);
	TRANSFER (m_Icons);
}

template<class TransferFunction>
inline void EditorOnlyPlayerSettings::BuildTargetBatching::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_BuildTarget);
	TRANSFER (m_StaticBatching);
	TRANSFER (m_DynamicBatching);
	transfer.Align ();
}

inline std::string InsecureScramblePassword(std::string const& password, std::string const& path)
{
	std::string::value_type secret = 0;

	for (std::string::size_type i = 0; i < path.length(); ++i)
	{
		secret += path[i];
	}

	std::string scrambled(password);

	for (std::string::size_type i = 0; i < scrambled.length(); ++i)
	{
		scrambled[i] ^= secret;
	}

	return scrambled;
}

#endif
