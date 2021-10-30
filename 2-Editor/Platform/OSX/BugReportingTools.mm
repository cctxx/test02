#include "UnityPrefix.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Utilities/Argv.h"
#import <ExceptionHandling/NSExceptionHandler.h>
#import <Cocoa/Cocoa.h>
#include "dlfcn.h"
#include <stdio.h>
#include <stdlib.h>
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"

static const char* kReportBugApp = "Unity Bug Reporter.app/Contents/MacOS/Unity Bug Reporter";

static void PrintStackTrace ();

int gLaunchedBugReporter = 0;

void LaunchBugReporter (BugReportMode mode)
{
	if (mode == kCrashbug || mode == kFatalError || mode == kCocoaExceptionOrAssertion)
	{
		if (gLaunchedBugReporter != 0)
			return;
		gLaunchedBugReporter = 1;
	}

	if (mode == kCrashbug)
		printf_console("\n*** Launching bug reporter due to crashbug.\n");
	else if (mode == kFatalError)
		printf_console("\n*** Launching bug reporter due to fatal error!\n");
	else if (mode == kCocoaExceptionOrAssertion)
		printf_console("\n*** Launching bug reporter due to cocoa exception or assert.\n");
	
	// do not actually launch bug reporter when in batch mode!
	if (!IsBatchmode())
	{
		string bugReportPath = AppendPathName (GetApplicationFolder (), kReportBugApp);
		if (IsFileCreated (bugReportPath))
		{
			NSTask* task = [[NSTask alloc]init];
			[task setLaunchPath: [NSString stringWithUTF8String: bugReportPath.c_str ()]];
			
			NSMutableArray* nsargs = [[NSMutableArray alloc]initWithCapacity: 0];
			
			#if UNITY_EDITOR
			if (mode != kManualSimple)
			{
			//add project folder to the arguments, if the bug reporter is launched from the editor
			[nsargs addObject: @"-ProjectFolder"];
			NSString* projectPath = [NSString stringWithCString:EditorPrefs::GetString("kProjectBasePath").c_str()];
			[nsargs addObject: projectPath];
			
			// add buildsettings platform here, since it can change during session
			[nsargs addObject: @"-BuildPlatform"];
			NSString* buildPlatform = [NSString stringWithCString:GetBuildTargetName(GetEditorUserBuildSettings().GetActiveBuildTarget()).c_str()];
			[nsargs addObject: buildPlatform];
			}
			#endif
			
			
			if (mode == kCrashbug)
				[nsargs addObject: @"-Crashbug"];
			else if (mode == kFatalError)
				[nsargs addObject: @"-FatalError"];
			else if (mode == kCocoaExceptionOrAssertion)
				[nsargs addObject: @"-CocoaExceptionOrAssertion"];

			[task setArguments: nsargs];
			
			[task launch];
			[task release];
		}
	}
	
	if (mode == kCrashbug ||  mode == kFatalError || mode == kCocoaExceptionOrAssertion)
	{
		PrintStackTrace ();
		abort();
		exit (-1); // Just in case abort returns
	}
}

void ExitDontLaunchBugReporter (int exitValue)
{
	exit (exitValue);
}

// For backwards compatibility with pre 10.5: 
// The backtrace functions are loaded dynamically from libSystem.B.dylib
void PrintStackTrace ()
{
	typedef int (DLBacktraceFunc)(void** array, int size);
	typedef char** (DLBacktraceSymbolsFunc)(void* const* array, int size);

	DLBacktraceFunc* DLBacktrace; 
	DLBacktraceSymbolsFunc* DLBacktraceSymbols; 
	

	DLBacktrace=(DLBacktraceFunc*)dlsym(RTLD_NEXT,"backtrace");
	if(DLBacktrace == NULL)
	{
		printf_console("No stacktrace available on 10.4.\n");
		return;
	}

	DLBacktraceSymbols=(DLBacktraceSymbolsFunc*)dlsym(RTLD_NEXT,"backtrace_symbols");
	if(DLBacktraceSymbols == NULL)
	{
		printf_console("No stacktrace available on 10.4.\n");
		return;
	}
	
	void *array[100];
	size_t size;
	char **strings;
	size_t i;

	size = DLBacktrace (array, 100);
	strings = DLBacktraceSymbols (array, size);

	printf_console ("Obtained %zu stack frames.\n", size);

	for (i = 0; i < size; i++)
	printf_console ("%s\n", strings[i]);

	free (strings);
}


// Register exception handler (quit app and print stacktrace!)
// NSExceptionHandler conflicts with mono exception handling, causing infinite loops upon receiving a signal,
// Disabled by Keli 
// TODO: Add a delegate to the UnhandledException event in the mono root AppDomain instead?

/*
@interface NSAssertionHandlerOverride : NSAssertionHandler {
}

- (void)handleFailureInMethod:(SEL)selector object:(id)object file:(NSString *)fileName lineNumber:(int)line description:(NSString *)format,...;
- (void)handleFailureInFunction:(NSString *)functionName file:(NSString *)fileName lineNumber:(int)line description:(NSString *)format,...;
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldHandleException:(NSException *)exception mask:(unsigned int)aMask;

@end

static int gExceptionHandlerDeepLock = 0;



@implementation NSAssertionHandlerOverride

- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldHandleException:(NSException *)exception mask:(unsigned int)aMask
{
	if (gExceptionHandlerDeepLock != 0)
	{
		printf_console ("Recursive exception handling failure!");
		return YES;
	}
	gExceptionHandlerDeepLock++;
	
	// Ignore it if we only want to extract a stacktrace!
	if ([[exception name]compare: kStackTraceExtract] != NSOrderedSame)
	{
		PrintStacktraceToConsole ();
		LaunchBugReporter (kCocoaExceptionOrAssertion);
	}

	gExceptionHandlerDeepLock--;
	return YES;
}

- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)exception mask:(unsigned int)aMask
{
	return NO;
}

- (void)handleFailureInMethod:(SEL)selector object:(id)object file:(NSString *)fileName lineNumber:(int)line description:(NSString *)format,...
{
	NSString* value = [[NSString alloc]initWithFormat: format arguments: va_list(&format + 1)]; 
	DebugStringToFile (MakeString (value), 0, [fileName UTF8String], line, kError);
	[value release];

	PrintStacktraceToConsole ();
	LaunchBugReporter (kCocoaExceptionOrAssertion);
}

- (void)handleFailureInFunction:(NSString *)functionName file:(NSString *)fileName lineNumber:(int)line description:(NSString *)format,...
{
	NSString* value = [[NSString alloc]initWithFormat: format arguments: va_list(&format + 1)]; 
	DebugStringToFile (MakeString (value), 0, [fileName UTF8String], line, kError);
	[value release];

	PrintStacktraceToConsole ();
	LaunchBugReporter (kCocoaExceptionOrAssertion);
}

@end

void SetupCocoaExceptionHandler ()
{
	// Register Assertion handler
	NSAssertionHandlerOverride* assertOverride = [[NSAssertionHandlerOverride alloc]init];
	NSMutableDictionary* threadDict = [[NSThread currentThread]threadDictionary];
	[threadDict setObject: assertOverride forKey: @"NSAssertionHandler"];
	
		
	
	NSExceptionHandler *handler = [NSExceptionHandler defaultExceptionHandler];
//	[handler setExceptionHandlingMask: NSLogTopLevelExceptionMask | NSHandleTopLevelExceptionMask | NSLogOtherExceptionMask];
//	[handler setDelegate: assertOverride];
	
}

void TestCocoaAssert ()
{
	id stuff = NSApp;
	[stuff ExecuteNonExistingFunction];
}
*/
