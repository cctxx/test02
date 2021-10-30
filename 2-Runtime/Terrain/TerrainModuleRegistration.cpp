#include "UnityPrefix.h"

#if ENABLE_TERRAIN
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"

static void RegisterTerrainClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (TerrainData)
	REGISTER_CLASS (Tree)
}

#if ENABLE_MONO || UNITY_WINRT
void ExportTerrains ();
void ExportTerrainDataBindings ();
void ExportWindZoneBindings ();

static void RegisterTerrainICallModule ()
{
#if !INTERNAL_CALL_STRIPPING
	ExportTerrains ();
	ExportTerrainDataBindings();
	ExportWindZoneBindings();
#endif
}
#endif

extern "C" EXPORT_MODULE void RegisterModule_Terrain ()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterTerrainClasses;
#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterTerrainICallModule;
#endif
	RegisterModuleInfo (info);
}
#endif