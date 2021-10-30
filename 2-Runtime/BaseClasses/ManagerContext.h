#ifndef MANAGERCONTEXT_H
#define MANAGERCONTEXT_H

class Object;
struct ManagerContext;

EXPORT_COREMODULE Object& GetManagerFromContext (int index);
EXPORT_COREMODULE Object* GetManagerPtrFromContext (int index);
void SetManagerPtrInContext(int index, Object* ptr);
void ManagerContextInitializeClasses();

#define GET_MANAGER(x) x& Get##x () { return reinterpret_cast<x&> (GetManagerFromContext (ManagerContext::k##x)); }
#define GET_MANAGER_PTR(x) x* Get##x##Ptr () { return reinterpret_cast<x*> (GetManagerPtrFromContext (ManagerContext::k##x)); }

const ManagerContext& GetManagerContext ();

struct ManagerContext
{
	enum Managers
	{
		// Global managers
		kPlayerSettings = 0,
		kInputManager,
		kTagManager,
		kAudioManager,
		kScriptMapper,
		kMonoManager,
		kGraphicsSettings,
		kTimeManager,
		kDelayedCallManager,
		kPhysicsManager,
		kBuildSettings,
		kQualitySettings,
		kResourceManager,
		kNetworkManager,
		kMasterServerInterface,
		kNavMeshLayers,
		#if ENABLE_2D_PHYSICS
		kPhysics2DSettings,
		#endif
		kGlobalManagerCount,
		
		// Level managers
		kSceneSettings = kGlobalManagerCount,
		kRenderSettings,
		kHaloManager,
		kLightmapSettings,
		kNavMeshSettings,
		kManagerCount
	};
	
	ManagerContext ();
	Object*	m_Managers[kManagerCount];
	int		m_ManagerClassIDs[kManagerCount];
	#if DEBUGMODE
	const char* m_ManagerNames[kManagerCount];
	#endif
	void InitializeClasses ();
};

#endif
