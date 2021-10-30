#include "Runtime/Modules/LoadDylib.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "BuildPipeline/BuildTargetPlatformSpecific.h"
#include "EditorModules.h"
#include "PlatformSupport/IPlatformSupportModule.h"

typedef IPlatformSupportModule* (*InitializePlatformSupportModule)();

static EditorModuleEntry gEditorModules[] = {
	{ "UnityEditor.iOS.Extensions.dll",  kBuild_iPhone },
	{ "UnityEditor.Xbox360.Extensions.dll", kBuildXBOX360 },
	{ "UnityEditor.WP8.Extensions.dll",  kBuildWP8Player },
	{ "UnityEditor.Metro.Extensions.dll",  kBuildMetroPlayerX86 },
	{ "UnityEditor.BB10.Extensions.dll", kBuildBB10 },
	{ "UnityEditor.Android.Extensions.dll", kBuild_Android },
	{ "UnityEditor.Tizen.Extensions.dll", kBuildTizen },
	{ "UnityEditor.PS3.Extensions.dll", kBuildPS3 }
};

static void* gLoadedPlatformSupportModule = NULL;
static IPlatformSupportModule* gPlatformSupportModule = NULL; 


int GetEditorModuleCount()
{
	return sizeof(gEditorModules) / sizeof(EditorModuleEntry);
}

const EditorModuleEntry& GetEditorModule(int index)
{
	Assert(index >= 0);
	Assert(index < GetEditorModuleCount());

	return gEditorModules[index];
}

const EditorModuleEntry* GetEditorModuleByDllName(const std::string& dllName)
{
	int count = GetEditorModuleCount();
	for (int i = 0; i < count; ++i)
	{
		if (gEditorModules[i].dllName == dllName)
		{
			return &gEditorModules[i];
		}
	}

	return NULL;
}

void InitializeManagedModuleManager ()
{
	ScriptingInvocation invocation("UnityEditor.Modules", "ModuleManager", "Initialize");
	invocation.Invoke();
}

void RegisterPlatformSupportModulesInManaged ()
{
	ScriptingInvocation invocation("UnityEditor.Modules", "ModuleManager", "InitializePlatformSupportModules");
	invocation.Invoke();
}

void ShutdownManagedModuleManager ()
{
	ScriptingInvocation invocation("UnityEditor.Modules", "ModuleManager", "Shutdown");
	// it might not actually be loaded yet (should never happen because the call to this method is guarded)
	if (invocation.method == NULL)
	{
		printf_console("Trying to shutdown module manager when nothing has been loaded yet.\n");
		return;
	}
	invocation.Invoke();
}

std::string GetDirectoryForTargetExtensionModules(BuildTargetPlatform target)
{
	const std::string buildTools = GetPlaybackEngineDirectory(target, 0, false);
	std::string path = AppendPathName(buildTools, "EditorExtensions");
	return path;
}

static std::string GetFutureCorrectBuildTargetName(BuildTargetPlatform target)
{
	switch (target)
	{
		case kBuildWP8Player: return "WP8";
		case kBuildMetroPlayerX86: return "Metro";
		case kBuild_iPhone: return "iOS";
		case kBuildXBOX360: return "Xbox360";
		case kBuildBB10: return "BB10";
		case kBuildTizen: return "Tizen";
		case kBuildPS3: return "PS3";
		default: break;
	}

	return GetBuildTargetName(target);
}

void UnloadCurrentPlatformSupportModule()
{
	if (gLoadedPlatformSupportModule) 
	{
		printf_console("Unload native extension module for build target\n");
		gPlatformSupportModule->OnUnload();
		UnloadDynamicLibrary(gLoadedPlatformSupportModule);
		gLoadedPlatformSupportModule = NULL;
		gPlatformSupportModule = NULL;
	}
}

void LoadPlatformSupportModule(BuildTargetPlatform target)
{
	if (gLoadedPlatformSupportModule) return;
	std::string extensionsDir = GetDirectoryForTargetExtensionModules(target);
	std::string path = AppendPathName(extensionsDir, "UnityEditor." + GetFutureCorrectBuildTargetName(target) + ".Extensions.Native");
	path = GetPathWithPlatformSpecificDllExtension(path);

	UnloadCurrentPlatformSupportModule();

	if (!IsFileCreated(path))
	{
		printf_console("Native extension for build target not found\n");
		return;
	}

	printf_console("Load native extension module: %s\n", path.c_str());

	gLoadedPlatformSupportModule = LoadDynamicLibrary(path);
	if (!gLoadedPlatformSupportModule)
	{
		printf_console("Failed to load: %s\n", path.c_str());
		return;
	}

	InitializePlatformSupportModule initModule = (InitializePlatformSupportModule)LookupSymbol(gLoadedPlatformSupportModule, "InitializeModule");
	if (!initModule)
	{
		printf_console("Couldn't find RegisterModule symbol in: %s\n", path.c_str());
		return;
	}

	gPlatformSupportModule = initModule();
	if (!gPlatformSupportModule)
	{
		return;
	}

	gPlatformSupportModule->OnLoad();
}

void* GetSymbolInNativeBuildTargetExtensionModule(const std::string& symbol)
{
	if (gLoadedPlatformSupportModule)
	{
		return LookupSymbol(gLoadedPlatformSupportModule, symbol);
	}
	else 
	{
		return NULL;
	}
}

IPlatformSupportModule* GetPlatformSupportModule()
{
	return gPlatformSupportModule;
}
