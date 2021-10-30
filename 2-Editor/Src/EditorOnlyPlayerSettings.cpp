#include "UnityPrefix.h"
#include "Editor/Src/EditorOnlyPlayerSettings.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Runtime/Mono/MonoManager.h"


EditorOnlyPlayerSettings::EditorOnlyPlayerSettings ()
{
	productGUID.Init();
	apiCompatibilityLevel = 2; // kNET_Small
	aotOptions = "";
	iPhoneBundleVersion = "1.0";
	AndroidBundleVersionCode = 1;
	AndroidMinSdkVersion = 6; // Android 2.0.1 'Eclair' (API level 6)
	AndroidPreferredInstallLocation = 1; // Prefer External
	m_WebPlayerTemplate = "APPLICATION:Default";
	iPhoneStrippingLevel = 0;
	iPhoneScriptCallOptimization = 0;
	forceAndroidInternetPermission = false;
	forceAndroidSDCardPermission = false;
	createAndroidWallpaper = false;
	enableAndroidExpansionFiles = false;
	uIPrerenderedIcon = false;
	uIRequiresPersistentWiFi = false;
	uIStatusBarHidden = true;
	uIExitOnSuspend = false;
	uIStatusBarStyle = 0;
	AndroidTargetDevice = 0;
	AndroidSplashScreenScale = 0;
	iPhoneSdkVersion = 988; // iOS latest
	iPhoneTargetOSVersion = 16; // 4.3 is new default
	stripUnusedMeshComponents = false;
	XboxGenerateSpa = false;
	XboxDeployKinectResources = false;
	xboxDeployKinectHeadOrientation = false;
	xboxDeployKinectHeadPosition = false;
	xboxEnableSpeech = false;
	xboxAdditionalTitleMemorySize = 0;
	wiiRegion = 1;
	wiiGameCode = "RABA";
	wiiCompanyCode = "ZZ";
	wiiSupportsNunchuk = false;
	wiiSupportsClassicController = false;
	wiiSupportsBalanceBoard = false;
	wiiSupportsMotionPlus = false;
	wiiControllerCount = 1;
	wiiFloatingPointExceptions = false;
	wiiScreenCrashDumps = true;

	ps3TitleConfigPath="";
	ps3DLCConfigPath="";
	ps3ThumbnailPath="";
	ps3BackgroundPath="";
	ps3SoundPath="";
	ps3TrophyCommId="";
	ps3NpCommunicationPassphrase="";
	ps3TrophyCommSig="";
	ps3TrophyPackagePath="";
	ps3BootCheckMaxSaveGameSizeKB = 128;		// 128KB by default
	ps3TrialMode = false;
	ps3SaveGameSlots = 1;

	flashStrippingLevel = kStripByteCode;
	metroCertificateNotAfter = 0;
	metroTileShowName = 1; // AllLogos
	metroTileForegroundText = 1;
	metroTileBackgroundColor.Set(0, 0, 0, 1);
	metroSplashScreenUseBackgroundColor = false;
	metroSplashScreenBackgroundColor.Set(0, 0, 0, 1);
	metroPackageName = GetDefaultProductName();
	metroApplicationDescription = GetDefaultProductName();
	metroCompilationOverrides = 1;

	blackberryDeviceAddress = "";
	blackberryDevicePassword = "";
	blackberryBuildId = 0;
	blackberryAuthorIdOveride = false;
	blackberrySharedPermissions = false;
	blackberryCameraPermissions = false;
	blackberryDeviceIDPermissions = false;
	blackberryGPSPermissions = false;
	blackberryMicrophonePermissions = false;
	blackberryGamepadSupport = false;
}


template<typename T>
struct EqualTarget
{
	string name;
	string groupName;

	EqualTarget (BuildTargetPlatform t)
	{
		name = GetBuildTargetName (t);
		groupName = GetBuildTargetGroupName (t);
	}

	bool operator()(const T& bts) const
	{
		if (bts.m_BuildTarget == name)
			return true;
		if (!groupName.empty() && bts.m_BuildTarget == groupName)
			return true;
		return false;
	}
};

struct EqualTargetByName
{
	string buildTargetName;

	EqualTargetByName(string t)
	{
		buildTargetName = t;
	}

	bool operator()(const EditorOnlyPlayerSettings::BuildTargetIcons& bti)
	{
		return bti.m_BuildTarget == buildTargetName;
	}
};

std::vector<int> EditorOnlyPlayerSettings::GetPlatformIconSizes (const std::string& platform)
{
	std::vector<int> sizes;
	if (platform == "")
	{
		sizes.push_back(128);
	}
	if (platform == "Web")
	{
		sizes.push_back(128);
		sizes.push_back(48);
		sizes.push_back(16);
	}
	if (platform == "Standalone")
	{
		sizes.push_back(1024);
		sizes.push_back(512);
		sizes.push_back(256);
		sizes.push_back(128);
		sizes.push_back(48);
		sizes.push_back(32);
		sizes.push_back(16);
	}
	if (platform == "iPhone")
	{
		sizes.push_back(152);
		sizes.push_back(144);
		sizes.push_back(120);
		sizes.push_back(114);
		sizes.push_back(76);
		sizes.push_back(72);
		sizes.push_back(57);
	}
	if (platform == "Android")
	{
		sizes.push_back(144);	// xxhdpi
		sizes.push_back(96);	// xhdpi
		sizes.push_back(72);	// hdpi
		sizes.push_back(48);	// mdpi
		sizes.push_back(36);	// ldpi
	}
	if (platform == "PS3")
	{
		sizes.push_back(176);
	}
	if (platform == "BlackBerry")
	{
		sizes.push_back(114);
	}
	if (platform == "Tizen")
	{
		sizes.push_back(117);
	}
	return sizes;
}

std::vector<PPtr<Texture2D> > EditorOnlyPlayerSettings::GetPlatformIcons(const std::string& platform)
{
	// any specific platform settings?
	std::vector<BuildTargetIcons>::const_iterator it =
	find_if(m_BuildTargetIcons, EqualTargetByName(platform));

	if (it != m_BuildTargetIcons.end())
	{
		it->m_Icons;
		std::vector<PPtr<Texture2D> > icons;
		for (int i=0;i<it->m_Icons.size();i++)
			icons.push_back(it->m_Icons[i].m_Icon);
		return icons;
	}
	else
	{
		return std::vector<PPtr<Texture2D> >();
	}
}

bool EditorOnlyPlayerSettings::SetPlatformIcons(const std::string& platform, std::vector<PPtr<Texture2D> > icons)
{
	if (icons.size() <= 0)
	{
		ClearPlatformIcons(platform);
		return true;
	}

	// Get dictated icon sizes for this platform
	std::vector<int> iconSizes = GetPlatformIconSizes(platform);
	// Ignore if amount of icons is incorrect
	if (icons.size() != iconSizes.size())
		return false;

	std::vector<IconWithSize> iconWithSizeVector;
	for (int i=0; i<icons.size(); i++)
	{
		IconWithSize iconWithSize;
		iconWithSize.m_Icon = icons[i];
		iconWithSize.m_Size = iconSizes[i];
		iconWithSizeVector.push_back(iconWithSize);
	}

	// see if we already have an entry for this platform
	std::vector<BuildTargetIcons>::iterator it =
	find_if(m_BuildTargetIcons, EqualTargetByName(platform));

	if (it != m_BuildTargetIcons.end()) // replace existing
	{
		BuildTargetIcons& targetIcons = *it;
		targetIcons.m_Icons = iconWithSizeVector;
	}
	else // add new
	{
		BuildTargetIcons targetIcons;
		targetIcons.m_BuildTarget = platform;
		targetIcons.m_Icons = iconWithSizeVector;
		m_BuildTargetIcons.push_back(targetIcons);
	}
	return true;
}

std::vector<EditorOnlyPlayerSettings::IconWithSize> EditorOnlyPlayerSettings::GetBestIconWithSizes (const std::string& platform)
{
	// See if icons for platform exist
	std::vector<BuildTargetIcons>::const_iterator it =
	find_if(m_BuildTargetIcons, EqualTargetByName(platform));

	// Use them if they exist
	if (it != m_BuildTargetIcons.end())
	{
		return it->m_Icons;
	}

	// Else, use the default icons
	it = find_if(m_BuildTargetIcons, EqualTargetByName(""));
	if (it != m_BuildTargetIcons.end())
	{
		return it->m_Icons;
	}
	// Else, return an empty vector
	else
	{
		return std::vector<IconWithSize>();
	}
}

void EditorOnlyPlayerSettings::ClearPlatformIcons(const std::string& platform)
{
	erase_if(m_BuildTargetIcons, EqualTargetByName(platform));
}

Texture2D * EditorOnlyPlayerSettings::GetPlatformIconForSize(const std::string& platform, int size)
{
	std::vector<IconWithSize> iconWithSizes = GetBestIconWithSizes(platform);

	Texture2D *bestMatch = NULL;
	int minDist = 0x7fffffff;

	for (int i=0; i<iconWithSizes.size(); i++)
	{
		int dist = Abs(size - iconWithSizes[i].m_Size);

		// scaling down looks better then scaling up. so first look for bigger icons or exact fits.
		if (iconWithSizes[i].m_Icon.IsValid() && (dist < minDist || bestMatch == NULL) && (size <= iconWithSizes[i].m_Size))
		{
			bestMatch = iconWithSizes[i].m_Icon;
			minDist = dist;
		}
	}

	if (bestMatch == NULL)
	{
		for (int i=0; i<iconWithSizes.size(); i++)
		{
			int dist = Abs(size - iconWithSizes[i].m_Size);
			if (iconWithSizes[i].m_Icon.IsValid() && (dist < minDist || bestMatch == NULL))
			{
				bestMatch = iconWithSizes[i].m_Icon;
				minDist = dist;
			}
		}
	}
	return bestMatch;
}

void EditorOnlyPlayerSettings::GetPlatformBatching (BuildTargetPlatform platform, bool* outStaticBatching, bool* outDynamicBatching) const
{
	std::vector<BuildTargetBatching>::const_iterator it = find_if (m_BuildTargetBatching.begin(), m_BuildTargetBatching.end(), EqualTarget<BuildTargetBatching>(platform));
	if (it != m_BuildTargetBatching.end())
	{
		*outStaticBatching = it->m_StaticBatching;
		*outDynamicBatching = it->m_DynamicBatching;
	}
	else
	{
		GetTargetPlatformBatchingDefaults (platform, outStaticBatching, outDynamicBatching);
	}
}

void EditorOnlyPlayerSettings::SetPlatformBatching (BuildTargetPlatform platform, bool staticBatching, bool dynamicBatching)
{
	std::vector<BuildTargetBatching>::iterator it = find_if (m_BuildTargetBatching, EqualTarget<BuildTargetBatching>(platform));
	if (it != m_BuildTargetBatching.end())
	{
		it->m_StaticBatching = staticBatching;
		it->m_DynamicBatching = dynamicBatching;
	}
	else
	{
		BuildTargetBatching batching;
		batching.m_BuildTarget = GetBuildTargetGroupName (platform);
		batching.m_StaticBatching = staticBatching;
		batching.m_DynamicBatching = dynamicBatching;
		m_BuildTargetBatching.push_back (batching);
	}
}

void EditorOnlyPlayerSettings::SetTemplateCustomKeys (std::vector<std::string> keys)
{
	m_TemplateCustomTags.clear();
	for(int i = 0; i < keys.size(); ++i)
		m_TemplateCustomTags[keys[i]] = "";
}

bool EditorOnlyPlayerSettings::SetTemplateCustomValue (std::string key, std::string value)
{
	WebTemplateTags::iterator it = m_TemplateCustomTags.find(key);
	if (it != m_TemplateCustomTags.end())
	{
		if (it->second == value)
			return false;
		m_TemplateCustomTags[key] = value;
		return true;
	}
	return false;
}

std::string EditorOnlyPlayerSettings::GetTemplateCustomValue (std::string key)
{
	WebTemplateTags::iterator it = m_TemplateCustomTags.find(key);
	if (it != m_TemplateCustomTags.end())
		return it->second;
	else
		return "";
}

std::vector<std::string> EditorOnlyPlayerSettings::GetTemplateCustomKeys ()
{
	WebTemplateKeys keys;
	for(WebTemplateTags::iterator it = m_TemplateCustomTags.begin(); it != m_TemplateCustomTags.end(); ++it)
		keys.push_back(it->first);
	return keys;
}

std::string EditorOnlyPlayerSettings::GetUserScriptingDefineSymbolsForGroup (int targetGroup) const
{
	std::map<int,UnityStr>::const_iterator match = scriptingDefineSymbols.find (targetGroup);
	if (scriptingDefineSymbols.end () == match)
		return "";
	return match->second;
}

static void RecompileScripts(BuildTargetPlatform activeTarget, bool refreshProject)
{
	bool buildingForEditor = true;
	DirtyAllScriptCompilers ();
	RecompileScripts (kCompileFlagsNone, buildingForEditor, activeTarget);
	if (refreshProject)
		CallStaticMonoMethod ("SyncVS", "SyncVisualStudioProjectIfItAlreadyExists");
}

void EditorOnlyPlayerSettings::SetUserScriptingDefineSymbolsForGroup (int targetGroup, const std::string &defines)
{
	std::map<int,UnityStr>::const_iterator match = scriptingDefineSymbols.find (targetGroup);
	if (scriptingDefineSymbols.end () == match || match->second != defines)
	{
		BuildTargetPlatform activeTarget = GetEditorUserBuildSettings ().GetActiveBuildTarget ();
		scriptingDefineSymbols[targetGroup] = defines;
		// Rebuild and update projects if changing defined symbols for the active target
		if (static_cast<int> (GetBuildTargetGroup (activeTarget)) == targetGroup)
		{
			RecompileScripts(activeTarget, true);
		}
	}
}
void EditorOnlyPlayerSettings::SetMetroCapability(const std::string& name, bool enabled)
{
	metroCapabilities[name] = enabled;
}
bool EditorOnlyPlayerSettings::GetMetroCapability(const std::string& name) const
{
	MetroCapbilitiesMap::const_iterator item = metroCapabilities.find(name);
	if (item == metroCapabilities.end())
		return false;
	else
		return item->second;
}

void EditorOnlyPlayerSettings::SetApiCompatibility (int value)
{
	RecompileScripts(GetEditorUserBuildSettings ().GetActiveBuildTarget (), false);
}

string GetDefaultCompanyName ()
{
	return "DefaultCompany";
}

string GetDefaultProductName ()
{
	return GetLastPathNameComponent (File::GetCurrentDirectory ());
}

