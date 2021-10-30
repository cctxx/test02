#include "UnityPrefix.h"
#include "GfxDeviceSetup.h"
#include "GfxDevice.h"
#include "threaded/GfxDeviceClient.h"
#include "Configuration/UnityConfigure.h"
#include "../Misc/SystemInfo.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/BuildSettings.h"
#if UNITY_WIN
#include "Runtime/GfxDevice/opengl/GLContext.h"
#if GFX_SUPPORTS_D3D9
#include "Runtime/GfxDevice/d3d/D3D9Context.h"
#endif
#include "PlatformDependent/Win/WinUtils.h"
#include "PlatformDependent/Win/WinUnicode.h"
#endif
#if UNITY_WII
#include "PlatformDependent/wii/WiiDeviceContext.h"
#endif
#if GFX_SUPPORTS_GCM
#include "PS3/GfxDevicePS3.h"
#endif
#if UNITY_EDITOR
#include "Runtime/Utilities/PlayerPrefs.h"
#endif

#if ENABLE_FORCE_GFX_RENDERER
GfxDeviceRenderer g_ForcedGfxRenderer = (GfxDeviceRenderer)-1;
#if GFX_SUPPORTS_D3D9
bool g_ForceD3D9RefDevice;
#endif
#endif

GfxThreadingMode g_ForcedGfxThreadingMode = kGfxThreadingModeAutoDetect;


#if GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30
int gDefaultFBO = -1;
#endif


bool IsThreadableGfxDevice (GfxDeviceRenderer renderer)
{
	switch (renderer) {
	case kGfxRendererD3D9:
	case kGfxRendererD3D11:
#if UNITY_WIN || UNITY_LINUX || UNITY_ANDROID
	// Multithreaded gles30 renderer currently only tested on windows simulator
	case kGfxRendererOpenGLES30:
#endif
#if UNITY_ANDROID
	case kGfxRendererOpenGLES20Mobile:
#endif
#if !UNITY_WIN
	// Multithreaded OpenGL is broken in Windows editor/player
	// Editor used to work when creating real GfxDevice on render thread
	case kGfxRendererOpenGL:
#endif
	case kGfxRendererXenon:
		return true;
	default:
		break;
	}
	return false;
}

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
GfxDevice* CreateRealGfxDevice (GfxDeviceRenderer renderer, bool forceRef)
{
	#if GFX_SUPPORTS_D3D9
	if (renderer == kGfxRendererD3D9)
	{
		GfxThreadableDevice* CreateD3D9GfxDevice(bool forceREF);
		return CreateD3D9GfxDevice(forceRef);
	}
	#endif
	#if GFX_SUPPORTS_OPENGL
	if (renderer == kGfxRendererOpenGL)
	{
		GfxThreadableDevice* CreateGLGfxDevice();
		return CreateGLGfxDevice();
	}
	#endif
	#if GFX_SUPPORTS_XENON
	if (renderer == kGfxRendererXenon)
	{
		GfxThreadableDevice* CreateXenonGfxDevice();
		return CreateXenonGfxDevice();
	}
	#endif

	#if GFX_SUPPORTS_D3D11
	if (renderer == kGfxRendererD3D11)
	{
		GfxDevice* CreateD3D11GfxDevice();
		return CreateD3D11GfxDevice();
	}
	#endif

	#if GFX_SUPPORTS_NULL
	if (renderer == kGfxRendererNull)
	{
		GfxDevice* CreateNullGfxDevice();
		return CreateNullGfxDevice();
	}
	#endif

	#if GFX_SUPPORTS_OPENGLES20
	if (renderer == kGfxRendererOpenGLES20Mobile)
	{
		extern GfxDevice* CreateGLES20GfxDevice();
		return CreateGLES20GfxDevice();
	}
	#endif

	#if GFX_SUPPORTS_OPENGLES30
	if (renderer = kGfxRendererOpenGLES30)
	{
		extern GfxDevice* CreateGLES30GfxDevice();
		return CreateGLES30GfxDevice();
	}
	#endif

	AssertString ("should not happen");
	return NULL;
}
#endif

void ParseGfxDeviceArgs ()
{
	// multithreading modes
	if (HasARGV("force-gfx-direct"))
	{
		::g_ForcedGfxThreadingMode = kGfxThreadingModeDirect;
	}
	else if (HasARGV("force-gfx-st"))
	{
		::g_ForcedGfxThreadingMode = kGfxThreadingModeNonThreaded;
	}
	else if (HasARGV("force-gfx-mt"))
	{
		::g_ForcedGfxThreadingMode = kGfxThreadingModeThreaded;
	}

	// forced gfx device types
	#if ENABLE_FORCE_GFX_RENDERER

	#if GFX_SUPPORTS_D3D9
	if (HasARGV ("force-d3d9"))
		::g_ForcedGfxRenderer = kGfxRendererD3D9;
	#endif
	#if GFX_SUPPORTS_OPENGL
	if (HasARGV ("force-opengl"))
		::g_ForcedGfxRenderer = kGfxRendererOpenGL;
	#endif
	#if GFX_SUPPORTS_D3D11
	if (HasARGV ("force-d3d11"))
		::g_ForcedGfxRenderer = kGfxRendererD3D11;
	#endif
	#if GFX_SUPPORTS_OPENGLES20
	if (HasARGV ("force-gles20"))
		::g_ForcedGfxRenderer = kGfxRendererOpenGLES20Mobile;
	#endif
	#if GFX_SUPPORTS_OPENGLES30
	if (HasARGV ("force-gles30"))
		::g_ForcedGfxRenderer = kGfxRendererOpenGLES30;
	#endif
	#if GFX_SUPPORTS_D3D9
	if (HasARGV("force-d3d9-ref"))
		::g_ForceD3D9RefDevice = true;
	#endif

	#endif // ENABLE_FORCE_GFX_RENDERER
}

#if ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER
bool InitializeGfxDeviceWorkerProcess(size_t size, void *buffer)
{
	Assert (!IsGfxDevice());
	GfxDevice* device = NULL;
	device = CreateClientGfxDevice (kGfxRendererOpenGL, kClientDeviceThreaded | kClientDeviceWorkerProcess, size, buffer);
	SetGfxDevice(device);

	// hack to make sure FastPropertyNames are initialized on the Worker process.
	ShaderLab::FastPropertyName name;
	name.SetName("");

	return device != NULL;
}
#endif

bool InitializeGfxDevice()
{
	Assert (!IsGfxDevice());
	GfxDevice* device = NULL;
	PlayerSettings* playerSettings = GetPlayerSettingsPtr();

	#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT

	device = CreateClientGfxDevice (kGfxRendererOpenGL, kClientDeviceThreaded | kClientDeviceClientProcess);

	#else //ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT

	#if ENABLE_MULTITHREADED_CODE && !UNITY_WP8

	// Device threading mode and flags
	#if !UNITY_WIN && !UNITY_OSX && !UNITY_XENON && !UNITY_LINUX && !UNITY_ANDROID
	g_ForcedGfxThreadingMode = kGfxThreadingModeDirect; // direct device everywhere except where it isn't
	#endif

	bool threaded = systeminfo::GetProcessorCount() > 1; // default to MT rendering on multicore
	#if WEBPLUG
	threaded = false; // for now, don't use MT rendering in web player; needs way more testing
	#endif
	#if !WEBPLUG
	if (!IsHumanControllingUs())
		threaded = false; // in automated/batchmode, default to non threaded
	#endif

	#if UNITY_XENON || UNITY_ANDROID
	if (playerSettings && playerSettings->GetMTRenderingRuntime() == false)
		threaded = false; // disabled by the user
	#endif

	#if UNITY_WIN && UNITY_EDITOR
	if (systeminfo::GetOperatingSystemNumeric() < 600 && g_ForcedGfxThreadingMode == kGfxThreadingModeAutoDetect)
	{
		printf_console("Disabled multithreaded rendering due to old version of Windows.\n"\
					   "Changing input language can cause lockups with versions before Vista.\n"\
					   "Use -force-gfx-mt option on the command line to override.\n");
		threaded = false;
	}
	#endif

	if (g_ForcedGfxThreadingMode == kGfxThreadingModeThreaded)
		threaded = true;
	else if (g_ForcedGfxThreadingMode == kGfxThreadingModeNonThreaded)
		threaded = false;

	UInt32 deviceFlags = 0;

	if (threaded) deviceFlags |= kClientDeviceThreaded;
	if (g_ForcedGfxThreadingMode == kGfxThreadingModeDirect) deviceFlags |= kClientDeviceUseRealDevice;
	#if GFX_SUPPORTS_D3D9 && ENABLE_FORCE_GFX_RENDERER
	if (g_ForceD3D9RefDevice) deviceFlags |= kClientDeviceForceRef;
	#endif

	#endif


#if ENABLE_FORCE_GFX_RENDERER

if (g_ForcedGfxRenderer >= 0)
{
	printf_console ("Forcing GfxDevice: %d\n", g_ForcedGfxRenderer);
	if (!IsThreadableGfxDevice(g_ForcedGfxRenderer))
		deviceFlags |= kClientDeviceUseRealDevice;
	device = CreateClientGfxDevice (g_ForcedGfxRenderer, deviceFlags);
}
#endif

	#if UNITY_WP8

	device = CreateRealGfxDevice (kGfxRendererD3D11, false);

	#elif UNITY_WIN
	// -------- Windows -----------------------------------------------------

	// Try D3D11 if project was built with support for that
	#if GFX_SUPPORTS_D3D11
	if (!device && (UNITY_WINRT || (playerSettings && playerSettings->GetUseDX11())))
	{
		if (!IsThreadableGfxDevice(kGfxRendererD3D11))
			deviceFlags |= kClientDeviceUseRealDevice;
		device = CreateClientGfxDevice (kGfxRendererD3D11, deviceFlags);
	}
	#endif

	// Try D3D9
	#if GFX_SUPPORTS_D3D9
	if (!device)
	{
		device = CreateClientGfxDevice (kGfxRendererD3D9, deviceFlags);
		if (!device & (deviceFlags & kClientDeviceForceRef))
		{
			winutils::AddErrorMessage( "Failed to initialize Direct3D 9 REF device.\r\nMake sure you have DX9 SDK installed." );
			return false;
		}
	}
	#endif

	if (!device)
	{
		// Try OpenGL. Editor is dx only unless you force GL
		#if GFX_SUPPORTS_OPENGL && !UNITY_EDITOR
		printf_console( "D3D9 initialization failed, trying OpenGL\n" );
		device = CreateClientGfxDevice (kGfxRendererOpenGL, deviceFlags);
		#endif
		if (!device)
			winutils::AddErrorMessage( "Failed to initialize Direct3D 9.\r\nMake sure you have DirectX 9.0c installed, have drivers for your\r\ngraphics card and have not disabled 3D acceleration\r\nin display settings." );
	}


	#elif UNITY_OSX
	// -------- Mac OS X -----------------------------------------------------

	device = CreateClientGfxDevice (kGfxRendererOpenGL, deviceFlags);

	#elif UNITY_WII

	GfxDevice* CreateWiiGfxDevice();
	device = CreateWiiGfxDevice();

	#elif UNITY_XENON

	GfxThreadableDevice* CreateXenonGfxDevice();
	device = CreateClientGfxDevice(kGfxRendererXenon, deviceFlags);

	#elif UNITY_PS3

	GfxDevice* CreateGCMGfxDevice();
	device = CreateGCMGfxDevice();
	SetGfxDevice(device);
	((GfxDevicePS3*)device)->Init();

	#elif UNITY_IPHONE

	extern GfxDevice* CreateUniversalGLESGfxDevice();
	device = CreateUniversalGLESGfxDevice();

	#elif UNITY_ANDROID

	if (!device)
	{
		GfxDeviceRenderer renderer  =
			playerSettings->GetTargetGlesGraphics() + 1 == 2 ? kGfxRendererOpenGLES20Mobile :
			playerSettings->GetTargetGlesGraphics() + 1 == 3 ? kGfxRendererOpenGLES30 :
			kGfxRendererNull;

		if (!IsThreadableGfxDevice(renderer))
			deviceFlags |= kClientDeviceUseRealDevice;
		device = CreateClientGfxDevice(renderer, deviceFlags);
	}

	#elif UNITY_BB10

	GfxDevice* CreateGLES20GfxDevice();
	device = CreateGLES20GfxDevice();

	#elif UNITY_TIZEN

	GfxDevice* CreateGLES20GfxDevice();
	device = CreateGLES20GfxDevice();

	#elif UNITY_PEPPER

	GfxDevice* CreateGLES20GfxDevice();
	device = CreateGLES20GfxDevice();
	#elif UNITY_WEBGL

	GfxDevice* CreateGLES20GfxDevice();
	device = CreateGLES20GfxDevice();

	#elif UNITY_LINUX && GFX_SUPPORTS_OPENGL

	if (NULL == device) {
		GfxThreadableDevice* CreateGLGfxDevice();
		device = CreateGLGfxDevice();
	}

	#elif UNITY_LINUX && GFX_SUPPORTS_OPENGLES20

	GfxDevice* CreateGLES20GfxDevice();
	device = CreateGLES20GfxDevice();

	#elif UNITY_LINUX && !SUPPORT_X11

	GfxDevice* CreateNullGfxDevice();
	device = CreateNullGfxDevice();

	#elif UNITY_FLASH
	GfxDevice* CreateMolehillGfxDevice();
	device = CreateMolehillGfxDevice();

	#else
	#error "Unknown platform"
	#endif
	#endif //ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT

	SetGfxDevice(device);

	gGraphicsCaps.SharedCapsPostInitialize ();

	return device != NULL;
}
