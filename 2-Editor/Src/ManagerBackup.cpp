#include "UnityPrefix.h"
#include "ManagerBackup.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Utilities/ArrayUtility.h"

// These managers are saved on play and reloaded when stopping, so that all the changes made at runtime are reset
// Only Reset global managers, per scene managers are reloaded with the scene anyway
const int kManagersToReset[] =
{
	ManagerContext::kInputManager, ManagerContext::kTagManager, 	ManagerContext::kAudioManager, ManagerContext::kGraphicsSettings, ManagerContext::kTimeManager, 
	ManagerContext::kPlayerSettings, ManagerContext::kPhysicsManager, ManagerContext::kQualitySettings, ManagerContext::kNetworkManager, ManagerContext::kNavMeshLayers,
	#if ENABLE_2D_PHYSICS
	ManagerContext::kPhysics2DSettings
	#endif
};

struct ManagerBackup
{
	struct ManagerToReset
	{
		int managerIndex;
		dynamic_array<UInt8> data;
		UInt32 dirtyIndex;
		ManagerToReset () { dirtyIndex = 0; }
	};
	
	// these managers are saved on play and reloaded when stopping, so that all the changes made at runtime are reset
	typedef std::list<ManagerToReset> ManagersToReset;
	ManagersToReset m_ManagersToReset;

	void SaveUserEditableManagers();
	void ResetUserEditableManagers();
};

void ManagerBackup::ResetUserEditableManagers()
{
	for (ManagersToReset::iterator i = m_ManagersToReset.begin(); i != m_ManagersToReset.end(); i++)
	{
		Assert(!i->data.empty());
		
		ManagerToReset& reset = *i;
		
		Object& o = GetManagerFromContext(reset.managerIndex);
		ReadObjectFromVector (&o, reset.data);
		o.SetPersistentDirtyIndex(reset.dirtyIndex);
		o.CheckConsistency();
		o.AwakeFromLoad(kDefaultAwakeFromLoad);
	}
	
	m_ManagersToReset.clear();
}

void ManagerBackup::SaveUserEditableManagers()
{
	Assert(m_ManagersToReset.empty());
	m_ManagersToReset.clear();
	
	for (int i=0;i<ARRAY_SIZE(kManagersToReset);i++)
	{
		m_ManagersToReset.push_back(ManagerToReset());
		ManagerToReset& reset = m_ManagersToReset.back();
		
		reset.managerIndex = kManagersToReset[i];
		Object& o = GetManagerFromContext(reset.managerIndex);
		WriteObjectToVector (o, &reset.data);
		reset.dirtyIndex = o.GetPersistentDirtyIndex();
	}
}


static ManagerBackup* gSingleton = NULL;
ManagerBackup& GetManagerBackup ()
{
	if (gSingleton == NULL)
		gSingleton = new ManagerBackup();
	
	return *gSingleton;
}


void ResetUserEditableManagers()
{
	GetManagerBackup ().ResetUserEditableManagers();
}

void SaveUserEditableManagers()
{
	GetManagerBackup ().SaveUserEditableManagers();
}
