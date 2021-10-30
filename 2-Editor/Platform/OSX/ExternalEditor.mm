#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ExternalEditor.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorHelper.h"
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"

#define keyFileSender                                   'FSnd'

#pragma options align=mac68k
struct TheSelectionRange
{
	short unused1; // 0 (not used)
	short lineNum; // line to select (<0 to specify range)
	long startRange; // start of selection range (if line < 0)
	long endRange; // end of selection range (if line < 0)
	long unused2; // 0 (not used)
	long theDate; // modification date/time
};
#pragma options align=reset
static OSStatus FindProcess (const FSRef* appRef, ProcessSerialNumber *pPSN);
static BOOL LaunchServicesOpenFileRef (FSRef* fileref, FSRef* appref, AERecord* params, LSLaunchFlags flags);
static bool OpenFileAtLineWithLaunchServices (NSString* path, int line);

static FourCharCode GetCreatorOfThisApp ()
{
	static FourCharCode creator = 0;
	if (creator == 0)
	{
		FourCharCode type;
		CFBundleGetPackageInfo (CFBundleGetMainBundle (), &type, &creator);
	}
	return creator;
}

static BOOL NSStringToFSRef (NSString* path, FSRef* ref)
{
	OSErr err;

	if ((err = FSPathMakeRef (reinterpret_cast<const unsigned char*> ([path UTF8String]), ref, NULL)) != noErr)
		return NO;
	return YES;
}


static OSStatus FindProcess (const FSRef* appRef, ProcessSerialNumber *pPSN)
{
	OSStatus err = noErr;

	pPSN->lowLongOfPSN  = kNoProcess;
	pPSN->highLongOfPSN  = kNoProcess;

	while ((err = GetNextProcess (pPSN)) == noErr)
	{
		FSSpec	processFile;
		memset (&processFile, 0, sizeof(processFile));
		FSRef	processBundle = { 0 };

		//	RMS 20080618 zero the process info record and set its "processInfoLength"
		//	field as advised in the documentation
		ProcessInfoRec processInfo;
		memset (&processInfo, 0, sizeof(processInfo));
		processInfo.processInfoLength = sizeof(processInfo);
		processInfo.processName = NULL;
		processInfo.processAppSpec = &processFile;

		//	RMS 20080618 it's OK if GetProcessInformation() fails, we'll
		//	just keep looking
		if (noErr == GetProcessInformation(pPSN, &processInfo))
		{
			//	sometimes we can see a process that has no associated file on disk.
			//	No idea why this happens, but it has in the past so we guard for it.

			if ((0 == processFile.vRefNum) || (0 == processFile.parID))
			{
				//	keep looking!

				continue;
			}

			//
			//	RMS 20080618 we can't just compare the process's FSSpec against
			//	the application path, because that will fail for any packaged
			//	application which uses a "springboard". An example springboard is
			//	Chris Campbell's "SystemVersionCheck"[1], which many applications
			//	use, including BBEdit and TextWrangler. The basic problem is that
			//	the incoming "appRef" was computed using the bundle's executable
			//	file as reported by [NSBundle executablePath], and if a
			//	springboard is in use, that answer will be wrong and we'll never
			//	find the process.
			//
			//	[1] <http://homepage.mac.com/chris_campbell/blog/SystemVersionCheck.html>
			//
			//	To solve this, just call GetProcessBundleLocation() which gives us the
			//	information we need. Then we can compare FSRefs and be done.
			//

			if ((noErr == GetProcessBundleLocation(pPSN, &processBundle)) &&
				(noErr == FSCompareFSRefs(&processBundle, appRef)))
			{
				//	congratulations, we found it!

				break;
			}
		}
	}

	return err;
}

static bool CreateTargetProcessDesc (NSString* application, AEDesc* targetProcess)
{
  OSStatus       err;
	FSRef appRef;
	if (!NSStringToFSRef (application, &appRef))
		return false;

  ProcessSerialNumber psn;
	err = FindProcess (&appRef, &psn);
	if (err != noErr)
		return false;
	err = AECreateDesc(typeProcessSerialNumber, &psn, sizeof(psn), targetProcess);
	if (err != noErr)
		return false;
	return true;
}

static BOOL OpenFileAtLineWithAppleEvent (NSString* path, int line, NSString* appPath)
{
	OSErr err;

//	RMS 20080618 this is unnecessary; the incoming "appPath" points to the bundle
//	on disk, and we're going to use that as-is to find the running application.
//	(Actually, it's beyond unnecessary, since the bundle executable may not be
//	the executable file that's actually running. See the comment in FindProcess()
//	for more information.)
//
// 	NSBundle* bundle = [NSBundle bundleWithPath: appPath];
// 	if (bundle)
// 		appPath = [bundle executablePath];

	AEDesc target;
	if (!CreateTargetProcessDesc (appPath, &target))
		return NO;

	// create apple event to be sent to target process
	AppleEvent theAE;
	err = AECreateAppleEvent(kCoreEventClass, kAEOpenDocuments, &target,
				  kAutoGenerateReturnID, kAnyTransactionID,&theAE);
	if (err != noErr)
		return false;

	// Add open file to event
	FSRef fileFSRef;
	if (!NSStringToFSRef (path, &fileFSRef))
		return NO;
	err = AEPutParamPtr(&theAE, keyDirectObject, typeFSRef, &fileFSRef, sizeof( fileFSRef ));
	if (err != noErr)
		return false;

	// Add selection range to event
	TheSelectionRange range;
	range.unused1 = 0;
	range.lineNum = line - 1;
	range.startRange = -1;
	range.endRange = -1;
	range.unused2 = 0;
	range.theDate = -1;

	if (( err = AEPutParamPtr( &theAE, keyAEPosition, typeChar, &range, sizeof (TheSelectionRange))) != 0 )
	{
		NSLog (@"AEPutParamPtr");
		return NO;
	}

	AEKeyword keyServerID = GetCreatorOfThisApp ();

    if (( err = AEPutParamPtr( &theAE, keyFileSender, typeType, (Ptr)&keyServerID, sizeof (AEKeyword))) != 0 ) {
		NSLog (@"AEPutParamPtr");
        return NO;
    }

	AppleEvent reply = { typeNull, nil };
	if (( err = AESend (&theAE, &reply, kAENoReply+kAENeverInteract, kAENormalPriority, kAEDefaultTimeout, nil, nil)) != noErr) {
		//	RMS 20080618 log the error
		NSLog (@"AESend");
        return NO;
	};

	return YES;
}

static BOOL LaunchServicesOpenApp (NSString* appPath)
{
    LSLaunchURLSpec     lspec = { NULL, NULL, NULL, 0, NULL };
    BOOL                success = YES;
    OSStatus            status;
    CFURLRef            appurl = NULL;

	FSRef appRef;
	if (!NSStringToFSRef (appPath, &appRef))
		return false;

    if (( appurl = CFURLCreateFromFSRef( kCFAllocatorDefault, &appRef )) == NULL ) {
        NSLog( @"CFURLCreateFromFSRef failed." );
        return( NO );
    }

    lspec.appURL = appurl;
    lspec.itemURLs = NULL;
    lspec.passThruParams = NULL;
    lspec.launchFlags = kLSLaunchNoParams;
    lspec.asyncRefCon = NULL;

    status = LSOpenFromURLSpec( &lspec, NULL );

    if ( status != noErr ) {
        NSLog( @"LSOpenFromRefSpec failed: error %d", status );
        success = NO;
    }

    if ( appurl != NULL ) {
        CFRelease( appurl );
    }

    return( success );
}

bool OpenFileAtLine (const std::string& inPath, int line, const std::string& inAppPath_, const std::string& inAppArgs, OpenFileAtLineMode openMode)
{
	NSString* path = MakeNSString(inPath);
	NSString* appPath = NULL;

	const std::string inAppPath = inAppPath_.empty() ? GetDefaultEditorPath() : inAppPath_;

	appPath = MakeNSString(inAppPath);

	// Get the default application if appPath is null
	if (appPath == NULL)
	{
		NSString* type;
		if (![[NSWorkspace sharedWorkspace]getInfoForFile:path application:&appPath type:&type])
			return NO;
	}

	if (!LaunchServicesOpenApp (appPath))
		return NO;

	if (std::string::npos != inAppPath.find ("MonoDevelop"))
	{
		// If launching with MonoDevelop, sync and open solutions first
		CallStaticMonoMethod("SyncVS", "CreateIfDoesntExist");

		string solutionPath;
		string filePath;
		GetSolutionFileToOpenForSourceFile (inPath, true, &solutionPath, &filePath);
		path = MakeNSString(filePath);

		if (solutionPath != PathToAbsolutePath(filePath))
			OpenFileAtLineWithAppleEvent (MakeNSString (solutionPath), 1, appPath);
	}

	return OpenFileAtLineWithAppleEvent (path, line, appPath);
}

/*
static bool OpenFileWithLaunchServices (NSString* path, NSString* appPath)
{
	FSRef pathRef, appRef;
	if (!NSStringToFSRef (path, &pathRef))
		return false;

	if (!NSStringToFSRef (appPath, &appRef))
		return false;

	AERecord rec = { typeNull, NULL };
	OSErr err;

	if (( err = AECreateList( NULL, 0, TRUE, &rec )) != 0 ) {
		NSLog (@"AEPutParamPtr");
        return false;
    }

	// Send Our application creator type in the keyFileSender in order to get the modified file event back!
	AEKeyword keyServerID = GetCreatorOfThisApp ();
    if (( err = AEPutParamPtr( &rec, keyFileSender, typeType, (Ptr)&keyServerID, sizeof (AEKeyword))) != 0 ) {
		NSLog (@"AEPutParamPtr");
        return NO;
    }

	return LaunchServicesOpenFileRef (&pathRef, &appRef, &rec, kLSLaunchDefaults);
}


static BOOL LaunchServicesOpenFileRef (FSRef* fileref, FSRef* appref, AERecord* params, LSLaunchFlags flags)
{
    LSLaunchURLSpec     lspec = { NULL, NULL, NULL, 0, NULL };
    BOOL                success = YES;
    OSStatus            status;
    CFURLRef            fileurl = NULL, appurl = NULL;
    CFArrayRef          arrayref = NULL;

    if (( fileurl = CFURLCreateFromFSRef( kCFAllocatorDefault, fileref )) == NULL ) {
        NSLog( @"CFURLCreateFromFSRef failed." );
        return( NO );
    }
    if (( arrayref = CFArrayCreate( kCFAllocatorDefault,
                ( const void ** )&fileurl, 1, NULL )) == NULL ) {
        NSLog( @"CFArrayCreate failed." );
        return( NO );
    }

    if (( appurl = CFURLCreateFromFSRef( kCFAllocatorDefault, appref )) == NULL ) {
        NSLog( @"CFURLCreateFromFSRef failed." );
        return( NO );
    }

    lspec.appURL = appurl;
    lspec.itemURLs = arrayref;
    lspec.passThruParams = params;
    lspec.launchFlags = flags;
    lspec.asyncRefCon = NULL;

    status = LSOpenFromURLSpec( &lspec, NULL );

    if ( status != noErr ) {
        NSLog( @"LSOpenFromRefSpec failed: error %d", status );
        success = NO;
    }

    if ( appurl != NULL ) {
        CFRelease( appurl );
    }
    if ( fileurl != NULL ) {
        CFRelease( fileurl );
    }
    if ( arrayref != NULL ) {
        CFRelease( arrayref );
    }

    return( success );
}
*/
