#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/vector_map.h"

class BuildSettings : public GlobalGameManager
{
	public:

	REGISTER_DERIVED_CLASS (BuildSettings, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (BuildSettings)

	BuildSettings (MemLabelId label, ObjectCreationMode mode);

	std::vector<UnityStr>    levels;
	std::vector<UnityStr>    remappedLevels;
	typedef vector_map<int, UInt32> ClassHashCont;
	ClassHashCont            runtimeClassHashes;

	bool                     hasRenderTexture;
	bool                     hasPROVersion;
	bool					 hasAdvancedVersion; // PRO version for the active platform.
	bool					 enableDynamicBatching;

	bool                     isNoWatermarkBuild;
	bool                     isPrototypingBuild;
	bool                     isEducationalBuild;
	bool                     isEmbedded;
	bool                     hasPublishingRights;
	bool                     hasShadows;
	bool                     hasSoftShadows;
	bool                     hasLocalLightShadows;
	bool                     isDebugBuild;
	bool					 usesOnMouseEvents;

	UnityStr                 m_AuthToken;

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	std::string GetLevelPathName (int index);
	std::string GetLevelPathName (const std::string& name);
	string GetLevelName (int index);

	int GetLevelIndex (const std::string& name);
	int GetLevelIndexChecked (const string& name);

	int GetIntVersion () { return m_IntVersion; }
	const UnityStr& GetVersion () { return m_Version; }

	UInt32 GetHashOfClass (int classID) const;

	private:

	int                     m_IntVersion;
	UnityStr                m_Version;
};

BuildSettings& GetBuildSettings();
BuildSettings* GetBuildSettingsPtr();

extern int kUnityVersion3_2_a1;
extern int kUnityVersion3_3_a1;
extern int kUnityVersion3_4_a1;
extern int kUnityVersion3_5_a1;
extern int kUnityVersion3_5_3_a1;
extern int kUnityVersion4_0_a1;
extern int kUnityVersion4_1_a1;
extern int kUnityVersion4_1_a3;
extern int kUnityVersion4_1_1_a1;
extern int kUnityVersion4_2_a1;
extern int kUnityVersion4_3_a1;
extern int kUnityVersion_OldWebResourcesAdded;

// The NaCl runtime will always be included with the player files,
// so in practice the version will always match.
#if !WEBPLUG || (UNITY_NACL && !UNITY_NACL_WEBPLAYER)
#define IS_CONTENT_NEWER_OR_SAME(val) true
#else
#define IS_CONTENT_NEWER_OR_SAME(val) (GetBuildSettingsPtr() && GetBuildSettings().GetIntVersion() >= val)
#endif
