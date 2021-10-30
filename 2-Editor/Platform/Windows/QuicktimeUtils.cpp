#include "UnityPrefix.h"
#include "Editor/Platform/Interface/Quicktime.h"
#include "PlatformDependent/Win/WinUtils.h"

static bool s_QuicktimeInitialized = false;

bool InitializeQuicktime()
{
	if( s_QuicktimeInitialized )
		return true;
	if (InitializeQTML(0L) != 0)
		return false;

	// Dump QT version to log.  Helpful when tracking down issues.
	long version;
	if (Gestalt (gestaltQuickTime, &version) == noErr)
	{
		int major = version >> 24;
		int minor = (version >> 20) & 0xf;
		int revision = (version >> 16) & 0xf;

		printf_console ("Using QuickTime %i.%i.%i\n", major, minor, revision);
	}

	s_QuicktimeInitialized = true;
	return true;
}

void ShutdownQuicktime()
{
	if( s_QuicktimeInitialized )
		TerminateQTML();
	s_QuicktimeInitialized = false;
}

// This one is not implemented in Quicktime windows libraries
long GetPixRowBytes(PixMapHandle pm) {
	return (*pm)->rowBytes & 0x3fff;
}

bool PathToFSSpec( const std::string& path, FSSpec* apSpec )
{
	OSErr err = NativePathNameToFSSpec( const_cast<char*>(path.c_str()), apSpec, kFullNativePath );
	return err == noErr;
}
