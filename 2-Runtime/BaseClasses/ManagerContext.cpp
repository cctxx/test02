#include "UnityPrefix.h"
#include "ManagerContext.h"
#include "BaseObject.h"
#include "Configuration/UnityConfigure.h"

ManagerContext::ManagerContext ()
{
	for (int i=0;i<kManagerCount;i++)
		m_Managers[i] = NULL;
}

void ManagerContext::InitializeClasses ()
{
	for (int i=0;i<kManagerCount;i++)
	{
		m_ManagerClassIDs[i] = -1;
		#if DEBUGMODE		
		m_ManagerNames[i] = "Not initialized";
		#endif
	}
	
#if DEBUGMODE
	#define INIT_MANAGER_CLASS(x) Assert(m_ManagerClassIDs[k##x] == -1); m_ManagerClassIDs[k##x] = Object::StringToClassID (#x); m_ManagerNames[k##x] = #x;
#else
	#define INIT_MANAGER_CLASS(x) m_ManagerClassIDs[k##x] = Object::StringToClassID (#x); 
#endif	
	
	INIT_MANAGER_CLASS (PlayerSettings)
	INIT_MANAGER_CLASS (InputManager)
	INIT_MANAGER_CLASS (TagManager)
	INIT_MANAGER_CLASS (AudioManager)
	INIT_MANAGER_CLASS (ScriptMapper)
	INIT_MANAGER_CLASS (MonoManager)
	INIT_MANAGER_CLASS (GraphicsSettings)
	INIT_MANAGER_CLASS (TimeManager)
	INIT_MANAGER_CLASS (DelayedCallManager)
	INIT_MANAGER_CLASS (PhysicsManager)
	INIT_MANAGER_CLASS (BuildSettings)
	INIT_MANAGER_CLASS (QualitySettings)
	INIT_MANAGER_CLASS (ResourceManager)
	INIT_MANAGER_CLASS (NetworkManager)
	INIT_MANAGER_CLASS (MasterServerInterface)
	INIT_MANAGER_CLASS (NavMeshLayers)
	#if ENABLE_2D_PHYSICS
	INIT_MANAGER_CLASS (Physics2DSettings)
	#endif

	INIT_MANAGER_CLASS (SceneSettings)
	INIT_MANAGER_CLASS (RenderSettings)
	INIT_MANAGER_CLASS (HaloManager)
	INIT_MANAGER_CLASS (LightmapSettings)
	INIT_MANAGER_CLASS (NavMeshSettings)
	
#if UNITY_EDITOR
	for (int i=0;i<kManagerCount;i++)
	{
		Assert (m_ManagerClassIDs[i] != -1);
	}

	std::vector<SInt32> allDerivedClasses;
	Object::FindAllDerivedClasses(Object::StringToClassID("GameManager"), &allDerivedClasses);
	if (allDerivedClasses.size() != kManagerCount)
	{
		ErrorString("Number of GameManager classes does not match number of game managers registered.");
	}
#endif	
	
}

static ManagerContext gContext;

Object& GetManagerFromContext (int index)
{
#if DEBUGMODE

	if( index >= ManagerContext::kManagerCount )
		FatalErrorString( "GetManagerFromContext: index for managers table is out of bounds" );

	if( gContext.m_Managers[index] == NULL )
	{
		char const* managerName = gContext.m_ManagerNames[ index ];
		FatalErrorString( Format("GetManagerFromContext: pointer to object of manager '%s' is NULL (table index %d)", managerName, index) );
	}
#endif

	return *gContext.m_Managers[index];
}

void ManagerContextInitializeClasses()
{
	gContext.InitializeClasses();
}

Object* GetManagerPtrFromContext (int index)
{
	return gContext.m_Managers[index];
}

void SetManagerPtrInContext(int index, Object* ptr)
{
	gContext.m_Managers[index] = ptr;
}

const ManagerContext& GetManagerContext ()
{
	return gContext;
}
