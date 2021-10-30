#include "UnityPrefix.h"
#include "SharkProfiler.h"
#include "Configuration/UnityConfigure.h"

#define ENABLE_SHARK_PROFILER ENABLE_PROFILER && UNITY_OSX

#if ENABLE_SHARK_PROFILE
#include <mach-o/dyld.h>

typedef int (*fpChudAcquireRemoteAccess) ( void );
typedef int (*fpChudReleaseRemoteAccess) ( void );
typedef int (*fpChudStartRemotePerfMonitor) ( const char *label );
typedef int (*fpChudStopRemotePerfMonitor) ( void );
typedef int (*fpChudInitialize) ( void );
typedef int (*fpChudCleanup) ( void );
typedef int (*fpChudIsInitialized) ( void );

// declare storage for each API's function pointers
static int gChudLoaded = -1;
static fpChudAcquireRemoteAccess gChudAcquireRemoteAccess = NULL;
static fpChudReleaseRemoteAccess gChudReleaseRemoteAccess = NULL;
static fpChudStartRemotePerfMonitor gChudStartRemotePerfMonitor = NULL;
static fpChudStopRemotePerfMonitor gChudStopRemotePerfMonitor = NULL;
static fpChudInitialize gChudInitialize = NULL;
static fpChudCleanup gChudCleanup = NULL;
static fpChudIsInitialized gChudIsInitialized = NULL;

static bool LoadChudFunctionPointers ()
{
	if (gChudLoaded == 1)
		return true;
	if (gChudLoaded == 0)
		return false;
	
	CFStringRef bundlePath = CFSTR("/System/Library/PrivateFrameworks/CHUD.framework/Versions/A/Frameworks/CHUDCore.framework");
	CFURLRef bundleURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, bundlePath, kCFURLPOSIXPathStyle, true);
	CFBundleRef bundle = CFBundleCreate (kCFAllocatorDefault, bundleURL);
	
	if (bundle == NULL)
	{
		ErrorString("Failed loading Shark system library");
		gChudLoaded = 0;
		return false;
	}
	
	gChudAcquireRemoteAccess=(fpChudAcquireRemoteAccess)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudAcquireRemoteAccess"));
	gChudReleaseRemoteAccess=(fpChudReleaseRemoteAccess)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudReleaseRemoteAccess"));
	gChudStartRemotePerfMonitor=(fpChudStartRemotePerfMonitor)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudStartRemotePerfMonitor"));
	gChudStopRemotePerfMonitor=(fpChudStopRemotePerfMonitor)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudStopRemotePerfMonitor"));
	gChudInitialize=(fpChudInitialize)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudInitialize"));
	gChudCleanup=(fpChudCleanup)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudCleanup"));
	gChudIsInitialized=(fpChudIsInitialized)CFBundleGetFunctionPointerForName (bundle, CFSTR("chudIsInitialized"));
	
	if (gChudAcquireRemoteAccess == NULL || gChudReleaseRemoteAccess == NULL || gChudStartRemotePerfMonitor == NULL
		|| gChudStartRemotePerfMonitor == NULL || gChudStopRemotePerfMonitor == NULL || gChudInitialize == NULL || gChudCleanup == NULL || gChudIsInitialized == NULL)
	{
		ErrorString("Failed loading Shark system library function");
		gChudLoaded = 0;
		return false;
	}
	else
	{
		gChudLoaded = 1;
		return true;
	}
}
#endif

void SharkBeginRemoteProfiling ()
{
#if ENABLE_SHARK_PROFILER
	if (!LoadChudFunctionPointers ())
		return;
	
	if (gChudInitialize() != 0)
		ErrorString("Launching Shark in SharkBeginProfile failed");
	if (gChudAcquireRemoteAccess() != 0)
		ErrorString("Launching Shark in SharkBeginProfile failed");
	if (gChudStartRemotePerfMonitor("Unity") != 0)
		ErrorString("Launching Shark in SharkBeginProfile failed");
#endif
}

void SharkEndRemoteProfiling ()
{
#if ENABLE_SHARK_PROFILER
	if (!LoadChudFunctionPointers ())
		return;
	
	gChudStopRemotePerfMonitor();
	gChudReleaseRemoteAccess();
#endif
}