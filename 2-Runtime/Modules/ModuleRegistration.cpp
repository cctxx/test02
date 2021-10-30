#include "UnityPrefix.h"
#include "ModuleRegistration.h"
#include "LoadDylib.h"
#include "RegisterStaticallyLinkedModules.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"

#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
#endif

enum { kMaxModuleCount = 10 };

static int gAvailableModuleCount = 0;
static ModuleRegistrationInfo gModuleRegistrationInfos[kMaxModuleCount];

void RegisterAllAvailableModuleClasses (ClassRegistrationContext& context)
{
	RegisterAvailableModules ();

	for(int i=0;i<gAvailableModuleCount;i++)
	{
		if (gModuleRegistrationInfos[i].registerClassesCallback != NULL)
			gModuleRegistrationInfos[i].registerClassesCallback (context);
	}
}

void RegisterAllAvailableModuleICalls ()
{
	RegisterAvailableModules ();

	for(int i=0;i<gAvailableModuleCount;i++)
	{
		if (gModuleRegistrationInfos[i].registerIcallsCallback != NULL)
			gModuleRegistrationInfos[i].registerIcallsCallback ();
	}
}

void RegisterModuleInfo(ModuleRegistrationInfo& info)
{
	gModuleRegistrationInfos[gAvailableModuleCount] = info;
	gAvailableModuleCount++;

	// Bump up kMaxModuleCount if we have too many modules. 
	Assert(gAvailableModuleCount < kMaxModuleCount);
}

#if !UNITY_WP8

void RegisterModuleInDynamicLibrary (const char* dllName, const char* modulename)
{
	std::string dllFullName = dllName;
#if WEBPLUG
	ErrorString ("Dynamic module loading not implemented for web plug");
#elif UNITY_OSX
	dllFullName = AppendPathName (GetApplicationPath (), "Contents/Frameworks/MacStandalonePlayer_"+dllFullName+".dylib");
#elif UNITY_WIN
	dllFullName = "StandalonePlayer_"+dllFullName+".dll";
#elif UNITY_LINUX
	dllFullName += "LinuxStandalonePlayer_"+dllFullName+".so";
#endif

	std::string funcName("RegisterModule_");
	funcName.append(modulename);
	void (*registerModuleFunction)() = (void(*)())LoadAndLookupSymbol(dllFullName, funcName);
	if (registerModuleFunction == NULL)
		return;
	
	registerModuleFunction();
}

#endif

static void RegisterDynamicallyLinkedModules()
{
	//Work in progress:
//	RegisterModuleInDynamicLibrary("GfxDeviceModule_Dynamic","GfxDevice");
//	RegisterModuleInDynamicLibrary("TerrainModule_Dynamic","Terrain");
//	RegisterModuleInDynamicLibrary("NavMeshModule_Dynamic","Navigation");
//	RegisterModuleInDynamicLibrary("AnimationModule_Dynamic","Animation");
//	RegisterModuleInDynamicLibrary("Dynamics2DModule_Dynamic","Physics2D");
//	RegisterModuleInDynamicLibrary("PhysicsModule_Dynamic","Physics");
//	RegisterModuleInDynamicLibrary("PhysicsEditorModule_Dynamic","PhysicsEditor");
}

void RegisterAvailableModules ()
{
	if (gAvailableModuleCount != 0)
		return;

	RegisterStaticallyLinkedModules();
	RegisterDynamicallyLinkedModules();
}
