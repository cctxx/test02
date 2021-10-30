#include "UnityPrefix.h"
#include "Plugins.h"

#if UNITY_PLUGINS_AVAILABLE

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif
#if UNITY_OSX || UNITY_LINUX
#include <dlfcn.h>
#endif
#if UNITY_XENON
#include "Runtime/Mono/MonoIncludes.h"
#endif


const char* FindPluginExecutable (const char* pluginName); // implemented in platform dependent



// --------------------------------------------------------------------------

static void* LoadPluginExecutable (const char* pluginPath)
{
	#if UNITY_WINRT
	std::wstring widePath;
	ConvertUnityPathName(pluginPath, widePath);
	return (void*)LoadPackagedLibrary (widePath.c_str(), 0);
	#elif (UNITY_WIN && !UNITY_WINRT)
	std::wstring widePath;
	ConvertUnityPathName(pluginPath, widePath);
	return (void*)LoadLibraryW (widePath.c_str());
	
	#elif UNITY_OSX || UNITY_LINUX
	return dlopen (pluginPath, RTLD_NOW);
	
	#elif UNITY_XENON	
	return cached_module_load(pluginPath, MONO_DL_LAZY, 0);

	#else
	return NULL;
	
	#endif
}

static void UnloadPluginExecutable (void* pluginHandle)
{
#if (UNITY_WIN || UNITY_WINRT)
	FreeLibrary ((HMODULE) pluginHandle);

#elif UNITY_OSX || UNITY_LINUX
	dlclose (pluginHandle);

#else
	// We don't unload on other platforms.  Only really relevant to editor
	// anyway.
#endif
}

static void* LoadPluginFunction (void* pluginHandle, const char* name)
{
	#if (UNITY_WIN || UNITY_WINRT)
	return GetProcAddress ((HMODULE)pluginHandle, name);
	
	#elif UNITY_OSX || UNITY_LINUX
	return dlsym (pluginHandle, name);
	
	#elif UNITY_XENON
	void* symbol = 0;
	char* err = mono_dl_symbol((MonoDl*)pluginHandle, name, &symbol);
	g_free(err);
	return symbol;

	#else
	return NULL;
	
	#endif
}


// --------------------------------------------------------------------------


typedef void PluginSetGraphicsDeviceFunc (void* device, int deviceType, int eventType);
typedef void PluginRenderMarkerFunc (int marker);

struct UnityPlugin {
	void* pluginHandle;
	PluginSetGraphicsDeviceFunc* setGraphicsDeviceFunc;
	PluginRenderMarkerFunc* renderMarkerFunc;
};

typedef dynamic_array<UnityPlugin> UnityPluginArray;
static UnityPluginArray	g_Plugins;



static void InitializePlugin (void* pluginHandle)
{
	// do nothing if we've already loaded this plugin
	for (size_t i = 0; i < g_Plugins.size(); ++i)
	{
		if (g_Plugins[i].pluginHandle == pluginHandle)
			return;
	}

	UnityPlugin plugin;
	plugin.pluginHandle = pluginHandle;
	plugin.setGraphicsDeviceFunc = (PluginSetGraphicsDeviceFunc*)LoadPluginFunction (pluginHandle, "UnitySetGraphicsDevice");
	plugin.renderMarkerFunc = (PluginRenderMarkerFunc*)LoadPluginFunction (pluginHandle, "UnityRenderEvent");
	g_Plugins.push_back (plugin);

	if (IsGfxDevice() && plugin.setGraphicsDeviceFunc)
	{
		GfxDevice& device = GetGfxDevice();
		plugin.setGraphicsDeviceFunc (device.GetNativeGfxDevice(), device.GetRenderer(), kGfxDeviceEventInitialize);
	}
}


void PluginsSetGraphicsDevice (void* device, int deviceType, GfxDeviceEventType eventType)
{
	for (size_t i = 0; i < g_Plugins.size(); ++i)
	{
		if (g_Plugins[i].setGraphicsDeviceFunc)
		{
			g_Plugins[i].setGraphicsDeviceFunc (device, deviceType, eventType);
		}
	}
}


void PluginsRenderMarker (int marker)
{
	if (!IsGfxDevice())
		return;
	GfxDevice& device = GetRealGfxDevice();

	for (size_t i = 0; i < g_Plugins.size(); ++i)
	{
		if (g_Plugins[i].renderMarkerFunc)
		{
			device.InvalidateState();
			g_Plugins[i].renderMarkerFunc (marker);
			device.InvalidateState();
		}
	}
}

#if UNITY_EDITOR
static bool gAllowPlugins = true;
void SetAllowPlugins (bool allow)
{
	gAllowPlugins = allow;
}

#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Editor/Src/Keys/PublicKeys.h"

static bool TryVerifySignature (const char *pluginPath, const unsigned char publicKey[])
{
	ScriptingInvocation verifySignature ("UnityEngine", "Security", "VerifySignature");
	verifySignature.AddString (pluginPath);
	verifySignature.AddArray (CreateScriptingArray (publicKey, PUBLIC_KEY_SIZE, GetScriptingTypeRegistry ().GetType ("System", "Byte")));
	if (!MonoObjectToBool (verifySignature.Invoke ()))
		return false;

	// Additionally enable render to texture
	GetBuildSettings ().hasRenderTexture = true;
	return true;
}

// Explicitly allow plugins signed by approved parties
static bool VerifySignature (const char *pluginPath)
{
	// Qualcomm
	if (TryVerifySignature (pluginPath, kqualcomm))
		return true;

	return false;
}
#endif

const char* FindAndLoadUnityPlugin (const char* pluginName)
{
	const char* pluginPath = FindPluginExecutable (pluginName);

	if (pluginPath == NULL
#if !UNITY_WINRT
		|| !strcmp(pluginPath, pluginName)
#endif
		)
		return pluginPath; // not found

#if UNITY_EDITOR
	if (!gAllowPlugins)
	{
		if (!VerifySignature (pluginPath))
		{
			ErrorString ("License error. This plugin is only supported in Unity Pro!\n");
			return pluginName;
		}
	}
#endif

	// plugin found, try loading & initializing it
	void* pluginHandle = LoadPluginExecutable (pluginPath);
	if (pluginHandle)
		InitializePlugin (pluginHandle);

	return pluginPath;
}


void UnloadAllPlugins ()
{
	for (size_t i = 0; i < g_Plugins.size(); ++i)
		UnloadPluginExecutable (g_Plugins[i].pluginHandle);

	g_Plugins.clear ();
}


#endif // UNITY_PLUGINS_AVAILABLE
