#include "UnityPrefix.h"
#include "EditorUserBuildSettings.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"

static EditorUserBuildSettings* gSingleton = NULL; 

EditorUserBuildSettings::EditorUserBuildSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
#if UNITY_WIN
	m_SelectedStandaloneTarget = kBuildStandaloneWinPlayer;
#elif UNITY_OSX
	m_SelectedStandaloneTarget = kBuildStandaloneOSXIntel;
#elif UNITY_LINUX
	m_SelectedStandaloneTarget = kBuildStandaloneLinux64;
#else
#error Unknown platform!
#endif
	m_SelectedBuildTargetGroup = kPlatformStandalone;
	m_ActiveBuildTarget = m_SelectedStandaloneTarget;
	m_Development =  false;
	m_ConnectProfiler = false;
	m_AllowDebugging = false;
	m_InstallInBuildFolder = false;
	m_WebPlayerDeployOnline = false;
	m_WebPlayerStreamed = false;
	m_WebPlayerOfflineDeployment = false;
	m_AppendProject = false;
	m_SymlinkLibraries = false;
	m_ArchitectureFlags = 0;
	m_SelectedPS3Subtarget = 0;
	m_SelectedWiiSubtarget = 0;
	m_SelectedWiiDebugLevel = 0;
	m_SelectedXboxSubtarget = 0;
	m_SelectedXboxRunMethod = 0;
	m_SelectedFlashSubtarget = 0;
	m_SelectedAndroidSubtarget = 0;
	m_SelectedMetroTarget = kBuildMetroPlayerX86;
	m_SelectedMetroBuildType = 4;
	m_SelectedMetroSDK = 0;
	m_SelectedBlackBerrySubtarget = 0;
	m_SelectedBlackBerryBuildType = 0;
	m_ExplicitNullChecks = false;
	m_EnableHeadlessMode = false;
	m_StripPhysics = false;
}

EditorUserBuildSettings::~EditorUserBuildSettings ()
{
	if (gSingleton == this)
		gSingleton = NULL;
}

template<class TransferFunc>
void EditorUserBuildSettings::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);
	
	TRANSFER (m_BuildLocation);
	TRANSFER (m_ActiveBuildTarget);
	TRANSFER (m_SelectedBuildTargetGroup);
	TRANSFER (m_SelectedStandaloneTarget);
	TRANSFER (m_ArchitectureFlags);
	TRANSFER (m_SelectedWiiSubtarget);
	TRANSFER (m_SelectedPS3Subtarget);
	TRANSFER (m_SelectedWiiDebugLevel);
	TRANSFER (m_SelectedXboxSubtarget);
	TRANSFER (m_SelectedXboxRunMethod);
	TRANSFER (m_Development);
	TRANSFER (m_ConnectProfiler);
	TRANSFER (m_AllowDebugging);	
	TRANSFER (m_WebPlayerStreamed);
	TRANSFER (m_WebPlayerOfflineDeployment);
	TRANSFER (m_InstallInBuildFolder);
	TRANSFER (m_SymlinkLibraries);
	TRANSFER (m_ExplicitNullChecks);
	TRANSFER (m_EnableHeadlessMode);
	TRANSFER (m_WebPlayerDeployOnline);
	
	transfer.Align();

	TRANSFER (m_SelectedAndroidSubtarget);
	TRANSFER (m_SelectedMetroTarget);
	TRANSFER (m_SelectedMetroBuildType);
	TRANSFER (m_SelectedMetroSDK);
	TRANSFER (m_SelectedBlackBerrySubtarget);
	TRANSFER (m_SelectedBlackBerryBuildType);

	transfer.Align();
}

BuildTargetPlatform EditorUserBuildSettings::GetActiveBuildTarget () const
{
	return (BuildTargetPlatform)m_ActiveBuildTarget;
}

bool EditorUserBuildSettings::SetActiveBuildTarget (BuildTargetPlatform target)
{
	if (m_ActiveBuildTarget != target)
	{
		m_ActiveBuildTarget = target;
		SetDirty();
		return true;
	}
	return false;
}

void EditorUserBuildSettings::SetSelectedBuildTargetGroup (BuildTargetPlatformGroup targetGroup)
{
	if (m_SelectedBuildTargetGroup != targetGroup)
	{
		m_SelectedBuildTargetGroup = targetGroup;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetWebPlayerStreamed (bool streamed)
{
	if (m_WebPlayerStreamed != streamed)
	{
		m_WebPlayerStreamed = streamed;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetWebPlayerDeployOnline (bool deployOnline)
{
	if (m_WebPlayerDeployOnline != deployOnline)
	{
		m_WebPlayerDeployOnline = deployOnline;
		SetDirty();
	}
}


void EditorUserBuildSettings::SetWebPlayerOfflineDeployment (bool offlineDeployment)
{
	if (m_WebPlayerOfflineDeployment != offlineDeployment)
	{
		m_WebPlayerOfflineDeployment = offlineDeployment;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedStandaloneTarget (BuildTargetPlatform target)
{
	if (m_SelectedStandaloneTarget != target)
	{
		m_SelectedStandaloneTarget = target;
		SetDirty();
		ForceRecompileAllScriptsAndDlls ();
	}
}

void EditorUserBuildSettings::SetSelectedFlashBuildSubtarget (FlashBuildSubtarget target)
{
	if(m_SelectedFlashSubtarget != target){
		m_SelectedFlashSubtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedPS3BuildSubtarget (PS3BuildSubtarget target)
{
	if (m_SelectedPS3Subtarget != target)
	{
		m_SelectedPS3Subtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedWiiBuildSubtarget (WiiBuildSubtarget target)
{
	if (m_SelectedWiiSubtarget != target)
	{
		m_SelectedWiiSubtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedMetroTarget (BuildTargetPlatform target)
{
	if (m_SelectedMetroTarget != target)
	{
		m_SelectedMetroTarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedMetroBuildType (MetroBuildType type)
{
	if (m_SelectedMetroBuildType != type)
	{
		m_SelectedMetroBuildType = type;
		SetDirty();
	}
}
void EditorUserBuildSettings::SetSelectedMetroSDK (MetroSDK sdk)
{
	if (m_SelectedMetroSDK != sdk)
	{
		m_SelectedMetroSDK = sdk;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedWiiBuildDebugLevel (WiiBuildDebugLevel level)
{
	if (m_SelectedWiiDebugLevel != level)
	{
		m_SelectedWiiDebugLevel = level;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedXboxBuildSubtarget (XboxBuildSubtarget target)
{
	if (m_SelectedXboxSubtarget != target)
	{
		m_SelectedXboxSubtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedXboxRunMethod (XboxRunMethod method)
{
	if (m_SelectedXboxRunMethod != method)
	{
		m_SelectedXboxRunMethod = method;
		SetDirty();
	}
}


BuildTargetSelection EditorUserBuildSettings::GetActiveBuildTargetSelection ()
{
	BuildTargetPlatform targetPlatform = GetActiveBuildTarget ();
	BuildTargetSelection selection(targetPlatform, 0);
	if (targetPlatform == kBuild_Android)
		selection.subTarget = GetSelectedAndroidBuildSubtarget();
	if(targetPlatform == kBuildFlash)
		selection.subTarget = GetSelectedFlashBuildSubtarget();
	if(targetPlatform == kBuildBB10)
		selection.subTarget = GetSelectedBlackBerryBuildSubtarget();


	return selection;
}


void EditorUserBuildSettings::SetSelectedAndroidBuildSubtarget (AndroidBuildSubtarget target)
{
	if (m_SelectedAndroidSubtarget != target)
	{
		m_SelectedAndroidSubtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedBlackBerryBuildSubtarget (BlackBerryBuildSubtarget target)
{
	if (m_SelectedBlackBerrySubtarget != target)
	{
		m_SelectedBlackBerrySubtarget = target;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSelectedBlackBerryBuildType (BlackBerryBuildType type)
{
	if (m_SelectedBlackBerryBuildType != type)
	{
		m_SelectedBlackBerryBuildType = type;
		SetDirty();
	}
}

std::string EditorUserBuildSettings::GetBuildLocation (BuildTargetPlatform target) const
{
	if (target < m_BuildLocation.size ())
	{
		std::string directory;
		switch (target)
		{
		case kBuildMetroPlayerX86:
		case kBuildMetroPlayerX64:
		case kBuildMetroPlayerARM:
		case kBuildWP8Player:
			directory = m_BuildLocation[target];
			break;
		default:
			directory = DeleteLastPathNameComponent(m_BuildLocation[target]);
		}
		if (IsDirectoryCreated (directory))
			return m_BuildLocation[target];
	}
	
	return string();
}

void EditorUserBuildSettings::SetBuildLocation (BuildTargetPlatform target, std::string path)
{
	if (GetBuildLocation (target) != path)
	{
		if (target >= m_BuildLocation.size())
			m_BuildLocation.resize(target + 1);
		m_BuildLocation[target] = path;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetDevelopment (bool value)
{
	if (m_Development != value)
	{
		m_Development = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetConnectProfiler (bool value)
{
	if (m_ConnectProfiler != value)
	{
		m_ConnectProfiler = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetAllowDebugging (bool value)
{
	if (m_AllowDebugging != value)
	{
		m_AllowDebugging = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetInstallInBuildFolder (bool value)
{
	if (m_InstallInBuildFolder != value)
	{
		m_InstallInBuildFolder = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetAppendProject(bool value)
{
	if (m_AppendProject != value)
	{
		m_AppendProject = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetSymlinkLibraries(bool value)
{
	if (m_SymlinkLibraries != value)
	{
		m_SymlinkLibraries = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetExplicitNullChecks(bool value)
{
	if (m_ExplicitNullChecks != value)
	{
		m_ExplicitNullChecks = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetEnableHeadlessMode(bool value)
{
	if (m_EnableHeadlessMode != value)
	{
		m_EnableHeadlessMode = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::SetStripPhysics(bool value)
{
	if (m_StripPhysics != value)
	{
		m_StripPhysics = value;
		SetDirty();
	}
}

void EditorUserBuildSettings::GetActiveScriptCompilationDefines(std::vector<string>& outDefines)
{
	GetScriptCompilationDefines(this->GetDevelopment(), true, false, this->GetActiveBuildTarget(), outDefines);
}

BuildTargetPlatform FindMostSuitableSupportedTargetPlatform(BuildTargetPlatform p)
{
	if (IsBuildTargetSupported(p)) return p;

	switch (p)
	{
#if UNITY_WIN
		case kBuildStandaloneOSXIntel:
			p = kBuildStandaloneWinPlayer;
			break;
#elif UNITY_OSX
		case kBuildStandaloneWinPlayer:
			p = kBuildStandaloneOSXIntel;
			break;
#elif UNITY_LINUX
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneWinPlayer:
			p = kBuildStandaloneLinux64;
			break;
#endif
	}
	if (IsBuildTargetSupported(p)) return p;
	
	return kBuildWebPlayerLZMA;
}

void EditorUserBuildSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	
	// Verify that we can use the specified build target
	m_ActiveBuildTarget = FindMostSuitableSupportedTargetPlatform((BuildTargetPlatform)m_ActiveBuildTarget);
}

IMPLEMENT_CLASS (EditorUserBuildSettings)
IMPLEMENT_OBJECT_SERIALIZE (EditorUserBuildSettings)

void SetEditorUserBuildSettings(EditorUserBuildSettings* settings)
{
	gSingleton = settings;
}

EditorUserBuildSettings& GetEditorUserBuildSettings()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}
EditorUserBuildSettings* GetEditorUserBuildSettingsPtr()
{
	return gSingleton;
}
