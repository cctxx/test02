#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"

static void RegisterNavMeshClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (NavMeshLayers)
	REGISTER_CLASS (NavMesh)
	REGISTER_CLASS (NavMeshAgent)
	REGISTER_CLASS (NavMeshSettings)
	REGISTER_CLASS (OffMeshLink)
	REGISTER_CLASS (NavMeshObstacle)
}

#if ENABLE_MONO || UNITY_WINRT
void ExportNavMeshBindings ();
void ExportNavMeshPathBindings ();
void ExportNavMeshAgentBindings ();
void ExportNavMeshObstacleBindings ();

static void RegisterNavmeshICallModule ()
{
#if !INTERNAL_CALL_STRIPPING
	ExportNavMeshBindings ();
	ExportNavMeshPathBindings ();
	ExportNavMeshAgentBindings ();
	ExportNavMeshObstacleBindings ();
#endif
}
#endif

extern "C" EXPORT_MODULE void RegisterModule_Navigation ()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterNavMeshClasses;
#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterNavmeshICallModule;
#endif
	RegisterModuleInfo (info);
}
