#include "UnityPrefix.h"
#import <Cocoa/Cocoa.h>
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "External/UnitTest++/src/UnitTest++.h"
#include "Editor/Src/Application.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Misc/Player.h"

#if ENABLE_UNIT_TESTS && !DEPLOY_OPTIMIZED
#include "Runtime/Testing/Testing.h"
#endif

@interface ForwardSendEvent : NSApplication
{

}
- (void) sendEvent: (NSEvent  *)event;
@end

@implementation ForwardSendEvent

- (void) sendEvent: (NSEvent  *)event
{
	profiler_start_mode(kProfilerEditor);
		
	if ([[self delegate] respondsToSelector:@selector(sendEvent:)])
		[[self delegate] performSelector: @selector(sendEvent:) withObject: event];
	[super sendEvent: event];

	profiler_end_mode(kProfilerEditor);
}

@end


int EditorMain(int argc, const char *argv[])
{
	AutoInitializeAndCleanupRuntime autoInit;
	
	SetupArgv (argc, argv);
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; // ResolveSymlinks requires an NSAutoreleasePool to be present
	[ForwardSendEvent poseAsClass: [NSApplication class] ];
	
	if (HasARGV ("logfile"))
		LogOutputToSpecificFile(GetFirstValueForARGV ("logfile").c_str());
	if (HasARGV ("cleanedLogFile"))
	{
		string logFile = GetFirstValueForARGV ("cleanedLogFile");
		InitializeCleanedLogFile(fopen(logFile.c_str(), "w"));
	}

	RunUnitTestsIfRequiredAndExit ();
	
	ParseGfxDeviceArgs();
	SetupDefaultPreferences();
	SetupFileLimits ();
		
	string dataFolder = ResolveSymlinks (AppendPathName (GetApplicationPath (), "Contents/Frameworks"));
	
	std::vector<string> monoPaths;
#if MONO_2_10 || MONO_2_12
	string profiledir = AppendPathName(dataFolder,"MonoBleedingEdge/lib/mono/");
#else
	string profiledir = AppendPathName(dataFolder,"Mono/lib/mono/");
#endif

	profiledir += kMonoClasslibsProfile;
	monoPaths.push_back(AppendPathName(dataFolder,"Managed"));
	monoPaths.push_back(profiledir); //mono/2.0
	InitializeMonoFromMain(monoPaths, AppendPathName(dataFolder,"Mono/etc"), argc, (const char**)(argv));
	
	[pool release];
	
	NSApplicationMain (argc, argv);
	
	return 0;
}

int main(int argc, const char *argv[])
{
	return EditorMain(argc, argv);
}
