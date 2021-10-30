#ifndef UNITYSCENE_SETTINGS_H
#define UNITYSCENE_SETTINGS_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "UmbraTomeData.h"

class Renderer;
class OcclusionPortal;

const float defaultSmallestOccluderValue = 5.0F;;
const float defaultSmallestHoleValue = 0.25F;
const float defaultBackfaceThresholdValue = 100.0F; 

struct OcclusionBakeSettings
{
	float               smallestOccluder;
	float               smallestHole;
	float               backfaceThreshold;
	
	OcclusionBakeSettings()
	{
		smallestOccluder = defaultSmallestOccluderValue;
		smallestHole = defaultSmallestHoleValue;
		backfaceThreshold = defaultBackfaceThresholdValue;
	}
	
	void SetDefaultOcclusionBakeSettings()
	{
		smallestOccluder = defaultSmallestOccluderValue;
		smallestHole = defaultSmallestHoleValue;
		backfaceThreshold = defaultBackfaceThresholdValue;
	}

	friend bool operator == (const OcclusionBakeSettings& lhs, const OcclusionBakeSettings& rhs)
	{
		return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	}
	
	DECLARE_SERIALIZE(OcclusionBakeSettings);
};

class SceneSettings : public LevelGameManager
{
public:

	REGISTER_DERIVED_CLASS (SceneSettings, LevelGameManager)
	DECLARE_OBJECT_SERIALIZE (SceneSettings)
	
	SceneSettings (MemLabelId label, ObjectCreationMode mode);
	// ~SceneSettings (); declared-by-macro

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	static void InitializeClass ();
	static void CleanupClass ();

	
	void SetUmbraTome (const dynamic_array<PPtr<Renderer> >& pvsObjectsArray, const dynamic_array<PPtr<OcclusionPortal> >& portalArray, const UInt8* visibilitybuffer, int size);
	
#if UNITY_EDITOR
	const OcclusionBakeSettings& GetOcclusionBakeSettings () const { return m_OcclusionBakeSettings; }
	void SetDefaultOcclusionBakeSettings () { SetDirty(); m_OcclusionBakeSettings.SetDefaultOcclusionBakeSettings(); }
	OcclusionBakeSettings& GetOcclusionBakeSettingsSetDirty () { SetDirty(); return m_OcclusionBakeSettings; }
#endif
	
	
	const dynamic_array<PPtr<Renderer> >& GetPVSObjectArray () { return m_PVSObjectsArray; }
	const dynamic_array<PPtr<OcclusionPortal> >& GetPortalsArray () { return m_PVSPortalsArray; }

	const UmbraTomeData& GetUmbraTome () { return m_UmbraTome; }

	int		GetUmbraTotalDataSize () const;
	int		GetPortalDataSize () const;

	void InvalidatePVSOnScene ();
	
private:
	
	void Cleanup ();
	
	UmbraTomeData                          m_UmbraTome;
	dynamic_array<PPtr<Renderer> >         m_PVSObjectsArray;
	dynamic_array<PPtr<OcclusionPortal> >  m_PVSPortalsArray;
	
	#if UNITY_EDITOR
	OcclusionBakeSettings                  m_OcclusionBakeSettings;
	#endif
};

SceneSettings& GetSceneSettings ();


#endif
