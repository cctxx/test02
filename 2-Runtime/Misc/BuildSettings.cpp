#include "UnityPrefix.h"
#include "Configuration/UnityConfigureVersion.h"
#include "BuildSettings.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/ErrorExit.h"

int kUnityVersion3_0_0a1 = GetNumericVersion("3.0.0.a1");
int kUnityVersion3_2_a1 = GetNumericVersion("3.2.0a1");
int kUnityVersion3_3_a1 = GetNumericVersion("3.3.0a1");
int kUnityVersion3_4_a1 = GetNumericVersion("3.4.0a1");
int kUnityVersion3_5_a1 = GetNumericVersion("3.5.0a1");
int kUnityVersion3_5_3_a1 = GetNumericVersion("3.5.3a1");
int kUnityVersion4_0_a1 = GetNumericVersion("4.0.0a1");
int kUnityVersion4_1_a1 = GetNumericVersion("4.1.0a1");
int kUnityVersion4_1_1_a1 = GetNumericVersion("4.1.1a1");
int kUnityVersion4_1_a3 = GetNumericVersion("4.1.0a3");
int kUnityVersion4_2_a1 = GetNumericVersion("4.2.0a1");
int kUnityVersion4_3_a1 = GetNumericVersion("4.3.0a1");
int kUnityVersion_OldWebResourcesAdded = GetNumericVersion("4.2.0a1");

BuildSettings::BuildSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	hasRenderTexture = true;
	hasPROVersion = true;
	hasAdvancedVersion = true;
	hasShadows = true;
	hasSoftShadows = true;
	hasLocalLightShadows = true;
	isNoWatermarkBuild = false;
	isPrototypingBuild = false;
	isEducationalBuild = false;
	isEmbedded = false;
	hasPublishingRights = true;
	isDebugBuild = true;
	usesOnMouseEvents = true;
	enableDynamicBatching = true;
	#if UNITY_EDITOR
	m_Version = UNITY_VERSION;
	#else
	m_Version = "1.6.0";
	#endif
	m_IntVersion = GetNumericVersion(m_Version);
}

BuildSettings::~BuildSettings ()
{
}

string BuildSettings::GetLevelPathName (int index)
{
	if (index < remappedLevels.size () && index >= 0)
		return remappedLevels[index];
	else
		return string ();
}

string BuildSettings::GetLevelPathName (const string& name)
{
	return GetLevelPathName (GetLevelIndex (name));
}

string BuildSettings::GetLevelName (int index)
{
	if (index < levels.size () && index >= 0)
		return DeletePathNameExtension (GetLastPathNameComponent(levels[index]));
	else
		return string ();
}

int BuildSettings::GetLevelIndex (const string& name)
{
	for (int i=0;i<levels.size ();i++)
	{
		string curName = levels[i];
		curName = DeletePathNameExtension (GetLastPathNameComponent(curName));
		if (StrICmp (name, curName) == 0)
			return i;
	}
	return -1;
}

int BuildSettings::GetLevelIndexChecked (const string& name)
{
	int index = GetLevelIndex (name);
	if (index == -1)
	{
		ErrorString (Format ("Level %s couldn't be found because it has not been added to the build settings.\nTo add a level to the build settings use the menu File->Build Settings...", name.c_str()));
	}
	return index;
}

UInt32 BuildSettings::GetHashOfClass (int classID) const
{
	ClassHashCont::const_iterator it = runtimeClassHashes.find (classID);
	if (it == runtimeClassHashes.end ())
		return 0;

	return it->second;
}

template<class TransferFunction>
void BuildSettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (2);
	TRANSFER (levels);
	TRANSFER (hasRenderTexture);
	TRANSFER (hasPROVersion);
	TRANSFER (isNoWatermarkBuild);
	TRANSFER (isPrototypingBuild);
	TRANSFER (isEducationalBuild);
	TRANSFER (isEmbedded);
	TRANSFER (hasPublishingRights);
	TRANSFER (hasShadows);
	TRANSFER (hasSoftShadows);
	TRANSFER (hasLocalLightShadows);
	TRANSFER (hasAdvancedVersion);
	TRANSFER (enableDynamicBatching);
	TRANSFER (isDebugBuild);
	TRANSFER (usesOnMouseEvents);
	transfer.Align();
	
	if (transfer.IsOldVersion(1))
	{
		hasPROVersion = hasRenderTexture;
	}
	
	// Only in game build mode we transfer the version,
	// in the editor it is always the latest one
	if (transfer.IsSerializingForGameRelease())
	{
		TRANSFER (m_Version);
		TRANSFER (m_AuthToken);
		
		if(transfer.IsReading()) {
			if(GetNumericVersion(m_Version) < kUnityVersion3_0_0a1) {
				// We're attempting to load a version of unity that we don't have
				// backwards compatibility for.
				ExitWithErrorCode(kErrorIncompatibleRuntimeVersion);
			}
		}
	}

	// We need hashed type trees only for the player 
	if (transfer.IsSerializingForGameRelease())
	{
		TRANSFER (runtimeClassHashes);
	}
}

void BuildSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	remappedLevels = levels;
	#if GAMERELEASE
	if (remappedLevels.empty ())
	{
		remappedLevels.push_back ("mainData");
		levels.push_back ("");
	}
	else
	{
		remappedLevels[0] = "mainData";
		for (int i=1;i<remappedLevels.size ();i++)
			remappedLevels[i] = Format ("level%d", i - 1);
	}
	#endif
	
#if UNITY_WINRT
	// On Metro you switch between debug/release in VS solution,
	// so we need isDebugBuild to dependant on the player, not on the settings
#	if UNITY_DEVELOPER_BUILD
		isDebugBuild = true;
#	else
		isDebugBuild = false;
#	endif
#endif
	m_IntVersion = GetNumericVersion(m_Version);
}

GET_MANAGER(BuildSettings)
GET_MANAGER_PTR(BuildSettings)
IMPLEMENT_CLASS (BuildSettings)
IMPLEMENT_OBJECT_SERIALIZE (BuildSettings)
