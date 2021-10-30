#include "UnityPrefix.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/Application.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Utilities/ARGV.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/PrefKeys.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Graphics/Image.h"
#include "External/ShaderLab/Library/ShaderLab.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/ExternalEditor.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/Word.h"
#include "Configuration/UnityConfigure.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Input/TimeManager.h"
#include "Editor/Platform/OSX/Utility/CocoaEditorUtility.h"
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#import <AddressBook/AddressBook.h>
#import <AppKit/NSSavePanel.h>
#include <unistd.h>
#include <sys/wait.h>
#define POLL_NO_WARN
#include <poll.h>
#include "Runtime/Utilities/Argv.h"
#include <iostream>
#include <fstream>

#define XCODE4_BUNDLE_ID @"com.apple.dt.Xcode"
#define XCODE3_BUNDLE_ID @"com.apple.Xcode"

using namespace std;

typedef CanAppendBuild (*BuildCanBeAppendedCallback)(std::string);

@interface AppendSavePanel : NSSavePanel
{
	BOOL m_Append;
	BuildCanBeAppendedCallback m_BuildCanBeAppended; 
	NSString *m_CantAppendMessage;
}

- (id)initWithBuildCanBeAppended:(BuildCanBeAppendedCallback)buildCanBeAppended andCantAppendMessage:(NSString *)cantAppendMessage;
- (BOOL)appendSelected;
- (BOOL)_overwriteExistingFileCheck:(NSString *)filename;
@end

@interface DialogDelegate : NSObject
{
@public
	RunComplexOpenPanelDelegate* isValid;
	RunComplexOpenPanelDelegate* shouldShowFilename;
	void* userData;
}

- (BOOL)panel:(id)sender isValidFilename:(NSString *)filename;
- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename;

@end

//----------------------------------------------------------------------
// Append/replace dialog shown when overwritting build folder
//----------------------------------------------------------------------
typedef enum {
	kOverwriteResultAppend,
	kOverwriteResultReplace,
	kOverwriteResultCancel
} OverwriteResult;

@interface AppendReplaceController: NSWindowController {
	NSString* message;
	bool enableAppend;
	NSTextField *textLabel;
	NSButton *enableReplaceButton;
	NSButton *appendButton;
	NSButton *replaceButton;
	OverwriteResult theResult;
}
@property (assign) IBOutlet NSTextField *textLabel;
@property (assign) IBOutlet NSButton *enableReplaceButton;
@property (assign) IBOutlet NSButton *appendButton;
@property (assign) IBOutlet NSButton *replaceButton;
- (IBAction)append:(id)sender;
- (IBAction)replace:(id)sender;
- (IBAction)cancel:(id)sender;
- (IBAction)enableReplaceStatus:(id)sender;
@end

@implementation AppendReplaceController
@synthesize textLabel;
@synthesize enableReplaceButton;
@synthesize appendButton;
@synthesize replaceButton;

// Setup dialog: label text, disable not available buttons, etc
- (void)awakeFromNib
{
	if (!enableAppend)
	{
		NSButton* append = self.appendButton;
		[append setEnabled:NO];
	}
	NSButton* checkbox = self.enableReplaceButton;
	NSButton* replace = self.replaceButton;
	if (!EditorPrefs::GetBool("EnableReplaceBuildButton", false))
	{
		[checkbox setState:NSOffState];
		[replace setEnabled:NO];
	}
	NSTextField* label = self.textLabel;
	[label setStringValue:message];
}

// Show dialog as an app modal sheet. Returns user's choise.
- (OverwriteResult)show:(NSWindow*)parent message:(NSString*)msg enableAppend:(bool)appendEnabled
{
	message = msg;
	enableAppend = appendEnabled;
		
	[NSApp beginSheet:self.window modalForWindow:parent modalDelegate:nil didEndSelector:nil contextInfo:nil];
	[NSApp runModalForWindow: self.window];
	[NSApp endSheet:self.window];
    [self.window orderOut:self];
	
	NSButton* checkbox = self.enableReplaceButton;
	bool replaceEnabled = checkbox.state == NSOnState;
	EditorPrefs::SetBool("EnableReplaceBuildButton", replaceEnabled);
	
	return theResult;
}

- (IBAction)append:(id)sender
{
	theResult = kOverwriteResultAppend;
	[NSApp stopModal];
}

- (IBAction)replace:(id)sender
{
	theResult = kOverwriteResultReplace;
	[NSApp stopModal];
}

- (IBAction)cancel:(id)sender
{
	theResult = kOverwriteResultCancel;
	[NSApp stopModal];
}

- (IBAction)enableReplaceStatus:(id)sender
{
	int state = [self.enableReplaceButton state];
	BOOL enabled = state == NSOnState;
	[replaceButton setEnabled:enabled];
}
@end
//----------------------------------------------------------------------

@implementation DialogDelegate

- (BOOL)panel:(id)sender isValidFilename:(NSString *)nsfilename
{
	string fileName = MakeString (nsfilename);
	if (IsDirectoryCreated (fileName))
		return false;
	
	if (isValid)
		return isValid (userData, fileName);
	else
		return true;
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)nsfilename
{
	string fileName = MakeString (nsfilename);
	if (IsDirectoryCreated (fileName))
		return true;
	
	if (shouldShowFilename)
		return shouldShowFilename (userData, fileName);
	else
		return true;
}

@end

CanAppendBuild iOSBuildCanBeAppended(std::string folder)
{
	folder.append("/Libraries/RegisterMonoModules.cpp");
	std::ifstream fin;
	fin.open(folder.c_str());
	
	CanAppendBuild result = kCanAppendBuildNo;
	std::string s;
	size_t found;
	std::string target = "UnityIPhoneRuntimeVersion = \"";
	
	while (getline(fin, s)) {
		found = s.find(target);
		if (found != std::string::npos)
		{
			found += target.size();
			size_t end = s.find("\"", found);
			
			if (s.substr(found, end - found).compare(UNITY_VERSION) == 0)
			{
				result = kCanAppendBuildYes;
				break;
			}
		}
	}
	
	fin.close();
	return result;
}

std::string GetApplicationSupportPath()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc]init];
	
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSLocalDomainMask, YES);
	NSString *applicationSupportDirectory = [paths objectAtIndex:0];
	std::string path = std::string([applicationSupportDirectory UTF8String]);
	
	[pool release];
	
	return path;
}

std::string GetDefaultEditorPath ()
{
	return AppendPathName( GetApplicationContentsPath(), "../../MonoDevelop.app" );
}

bool OpenWithDefaultApp (const std::string& path)
{
	if (path.find("http://") == 0)
		return [[NSWorkspace sharedWorkspace]openURL: [NSURL URLWithString:MakeNSString (path)]];
	return [[NSWorkspace sharedWorkspace]openFile: MakeNSString (ResolveSymlinks (PathToAbsolutePath (path)))];
}

bool OpenFileWithApplication (const std::string& path, const std::string& app)
{
	NSString* nsApp = MakeNSString (ResolveSymlinks (PathToAbsolutePath (app)));
	NSString* nsPath = MakeNSString (ResolveSymlinks (PathToAbsolutePath (path)));
	return [[NSWorkspace sharedWorkspace]openFile: nsPath withApplication: nsApp];
}

bool LaunchApplication (const std::string& path)
{
	[[NSWorkspace sharedWorkspace]launchApplication: MakeNSString (ResolveSymlinks (PathToAbsolutePath (path)))];
	return true;
}

bool SelectFileWithFinder (const std::string& path)
{
	[[NSWorkspace sharedWorkspace]selectFile: MakeNSString (path) inFileViewerRootedAtPath: NULL];
	return true;
}

static AuthorizationRef AquireRootAuthorization (const string& tool, const string& prompt) 
{ 
	// Cache the authorization for tools!
	static map<string, AuthorizationRef> gAuthorizations;
	if (gAuthorizations.count (tool))
		return gAuthorizations.find (tool)->second;
		AuthorizationRef authorizationRef = NULL;
	AuthorizationItem items[2]; 
	OSStatus err = 0; 

	// The authorization rights structure holds a reference to an array
	// of AuthorizationItem structures that represent the rights for which
	// you are requesting access.
	AuthorizationRights rights;
	AuthorizationFlags flags;
	// We just want the user's current authorization environment,
	// so we aren't asking for any additional rights yet.
	rights.count=0;
	rights.items = NULL;
	flags = kAuthorizationFlagDefaults;
	err = AuthorizationCreate(&rights, kAuthorizationEmptyEnvironment, flags, &authorizationRef);
	if (err != errAuthorizationSuccess) return NULL;
	
	// There should be one item in the AuthorizationItems array for each
	// right you want to acquire.
	// The data in the value and valueLength is dependent on which right you
	// want to acquire.
	// For the right to execute tools as root, kAuthorizationRightExecute,
	// they should hold a pointer to a C string containing the path to
	// the tool you want to execute, and the length of the C string path.
	// There needs to be one item for each tool you want to execute.
	items[0].name = kAuthorizationRightExecute;
	items[0].value = (char *)tool.c_str ();
	items[0].valueLength = strlen((const char*)items[0].value);
	items[0].flags = 0;

	items[1].name = kAuthorizationEnvironmentPrompt;
	items[1].value = (char *)prompt.c_str ();
	items[1].valueLength = strlen((const char*)items[1].value);
	items[1].flags = 0;
	
	rights.count=2;
	rights.items = items;
	flags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;
	
	// Since we've specified kAuthorizationFlagExtendRights and
	// have specified kAuthorizationFlagInteractionAllowed, if the
	// user isn't currently authorized to execute tools as root,
	// they will be asked for a password and err will indicate
	// an authorization failure. 
	err = AuthorizationCopyRights(authorizationRef,&rights, kAuthorizationEmptyEnvironment, flags, NULL);
	if (err != errAuthorizationSuccess)
		authorizationRef = NULL;
	
		
	gAuthorizations.insert (make_pair (tool, authorizationRef));
	return authorizationRef;
}

bool LaunchTaskAuthorizedArray (const string& app, string* output, const string& prompt, const vector<string>& arguments, const string& currentDirectory)
{
    OSStatus err = 0;
	FILE* ioPipe = NULL;
    
	AssertIf (output != NULL);
	vector<char*> carguments;
	carguments.resize (arguments.size () + 1, NULL);
	for (int i=0;i<arguments.size ();i++)
		carguments[i] = (char*)arguments[i].c_str ();
	
	AuthorizationRef authorization = AquireRootAuthorization (app, prompt);
	if (authorization == NULL)
		return false;
		
    err = AuthorizationExecuteWithPrivileges (authorization, app.c_str (), 0, &carguments[0], &ioPipe);
	return err == noErr;
}

bool LaunchTask (const string& app, string* output, ...)
{
	vector<string> arguments;
	va_list ap;
	va_start (ap, output);
	while (true)
	{
		char* cur = va_arg (ap, char*);
		if (cur == NULL) break;
		arguments.push_back (cur);	
	}
	va_end (ap);
	return LaunchTaskArray (app, output, arguments, true);
}

#if 1
bool LaunchTaskArray (const string& app, string* output, const vector<string>& arguments, bool quoteArguments, const string& currentDirectory, UInt32* exitCode)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc]init];
	
	NSTask* task = [[NSTask alloc]init];
	
	[task setLaunchPath: [NSString stringWithUTF8String: app.c_str ()]];
	
	if (!currentDirectory.empty ())
		[task setCurrentDirectoryPath: [NSString stringWithUTF8String: currentDirectory.c_str ()]];
	
	NSFileHandle *readHandle = NULL;
	NSPipe *pipe = NULL;
	if (output != NULL)
	{
		pipe = [[NSPipe alloc] init];
		readHandle = [pipe fileHandleForReading];
		[readHandle retain];
		
		[task setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
		[task setStandardOutput: pipe];
	}
	
	// Set argument list
	NSMutableArray* nsargs = [[NSMutableArray alloc]initWithCapacity: arguments.size ()];
	for (int i=0;i<arguments.size ();i++)
	{
		NSString* str = [[NSString alloc]initWithUTF8String: arguments[i].c_str ()];
		[nsargs addObject: str];
		[str release];
	}
	[task setArguments: nsargs];
	[nsargs release];
	
	NS_DURING
	[task launch];
	NS_HANDLER
	// Exception handling
	{
		if (output)
			*output = "Couldn't start task " + app + " because the executable couldn't opened!";
	
		[pool release];
		[task release];
		[readHandle closeFile];
		[readHandle release];
		[pipe release];
	}
	NS_VALUERETURN (false, bool);
	NS_ENDHANDLER
	
	if (output != NULL)
		output->clear ();
	
	
	while ([task isRunning])
	{
		pollfd p;
		poll (&p, 0, 5);
		
		if (readHandle != NULL)
		{
			// some tools (diff3 on mac) seem to not close until pipe is read 
			NSData* nsoutput = [readHandle readDataOfLength: 1024];
			if ([nsoutput length])
				output->append ((char*)[nsoutput bytes], (char*)[nsoutput bytes] + [nsoutput length]);
		}
	}
	
	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();

	// Wait until exit is needed in order for the task to properly deallocate
	[task waitUntilExit];

	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);
	
	// Read any remaining data
	while (readHandle != NULL)
	{
		// Returns nothing if EOF has been reached.
		NSData* nsoutput = [readHandle availableData];
		if ([nsoutput length])
			output->append ((char*)[nsoutput bytes], (char*)[nsoutput bytes] + [nsoutput length]);
		else
			break;
	}
	
	int status = [task terminationStatus];

	[task release];
	[pipe release];
	[readHandle closeFile];
	[readHandle release];
	[pool release];
	
	if (exitCode)
		*exitCode = status;
	
	return status == 0;
}
#else
bool LaunchTaskArray (const string& app, string* output, const vector<string>& arguments, bool quoteArguments, const string& currentDirectory)
{
	if( output )
		output->clear();

	int filedes[2];
	if ( output != NULL ) 
		pipe(filedes);
	
	pid_t pid = fork(); // start a new process
	if( pid == -1 ) {
		if ( output != NULL )
			*output = "Couldn't start task " + app + ". Couldn't fork a new process!";
		return false;
	}
	
	if (pid == 0) { // This is where we launch the process
		if ( output != NULL ) {
			// set up the output pipe
			close(1);
			close(filedes[0]);
			dup2(filedes[1],1); // This sets up the redirection of stdout
		}
		if(! currentDirectory.empty () ) 
			chdir(currentDirectory.c_str());
		
		// Generate an argument list
		const char* argv[arguments.size()+2];
		int pos=0;
		argv[pos++]= (char*)app.c_str();
		for(int i=0; i < arguments.size(); i++)
			argv[pos++]=(char*)arguments[i].c_str();
		argv[pos++]=NULL;
		
		execvp(argv[0], (char**) argv); // Launch the command
		printf_console("Could not launch %s.", app.c_str());
		_exit(-1);
	}
	
	// This is in the parent process
	// Read pipe output if requested
	bool readError=false;
	if( output != NULL ) {
		char buffer[1024];
		int bytesRead;
		close(filedes[1]); // We don't need the writing side of the pipe
		while( (bytesRead = read(filedes[0], buffer, 1024) ) != 0 ) {
			if ( bytesRead < 0 ) {
				// Handle errors. EINTR should be ignored, other errors will cause the loop to exit.
				if(errno == EINTR) continue;
				*output="Could not read output from " + app;
				readError=true;
				break;
			}
			output->append(buffer, buffer+bytesRead);
		}
		close(filedes[0]);
	}

	// Wait for the process to exit
	int status = 0;
	waitpid(pid, &status, 0);
	return !readError && status == 0;
	
}
#endif

NSString* GetXcode4Path()
{
	return [[NSWorkspace sharedWorkspace]absolutePathForAppBundleWithIdentifier: XCODE4_BUNDLE_ID];
}

bool CheckIPhoneXCode4Installed ()
{
	NSString* curApp = GetXcode4Path();
	if (curApp != NULL && [curApp length] != 0)
		return true;
	
	return false;
}

void LaunchIPhoneXCode4 ()
{
	NSString* curApp = GetXcode4Path();
	[[NSWorkspace sharedWorkspace] launchApplication:curApp];
	
	NSArray *selectedApps =
	[NSRunningApplication runningApplicationsWithBundleIdentifier:XCODE4_BUNDLE_ID];
	
	for (int i = 0; i < [selectedApps count]; i++)
	{
		NSRunningApplication *app = [selectedApps objectAtIndex:i];
		int count = 0;
		NSLog(@"Checking %@\n", app);
		while (![app isFinishedLaunching] && count++ < 300)
			[[NSRunLoop currentRunLoop] runUntilDate: [NSDate dateWithTimeIntervalSinceNow: 1.0f]];
	}
}

void TerminateIPhoneXCode4 ()
{
	NSArray *selectedApps =
	[NSRunningApplication runningApplicationsWithBundleIdentifier:XCODE4_BUNDLE_ID];
	
	[selectedApps makeObjectsPerformSelector:@selector(terminate)];
}

bool CheckIPhoneXCodeInstalled ()
{
	NSString* curApp = [[NSWorkspace sharedWorkspace]absolutePathForAppBundleWithIdentifier: XCODE3_BUNDLE_ID];
	if (curApp != NULL && [curApp length] != 0)
		return true;
    
	return CheckIPhoneXCode4Installed();
}

bool LaunchTaskArrayOptions (const string& app, const vector<string>& arguments, UInt32 options, const string& currentDirectory)
{
	pid_t child = fork(); // start a new process
	if( child == -1 ) 
		return false;
	
	if (child == 0) { // This is where we launch the process
		if(! currentDirectory.empty () ) 
			chdir(currentDirectory.c_str());
		
		// fork again -- this will cause the grandchild to be inherited by init when this fork exits
		
		pid_t grandchild = fork(); // start a new process
		if( grandchild == -1 ) 
			_exit(-1);
		
		if( grandchild == 0) {
			// Generate an argument list
			const char* argv[arguments.size()+2];
			int pos=0;
			argv[pos++]=(char*)app.c_str();
			for(int i=0; i < arguments.size(); i++)
				argv[pos++]=(char*)arguments[i].c_str();
			argv[pos++]=NULL;
			
			execvp(argv[0], (char**) argv); // Launch the command
			printf_console("Could not launch %s.\n", app.c_str());
			_exit(-1);
		}
		
		_exit(0); // Exit the child process... the grandchild is now completely detatched
	}

	// Wait for the process to exit
	int status=0;
	waitpid(child, &status, 0);
	if( status )
		printf_console("Could not launch %s. Exit code from child: %d\n", app.c_str(),status);
	return status == 0;
	
}

@interface ShowCompressionCompressionFinishedCallback : NSObject
{
	NSString* m_DestFile;
	NSString* m_Folder;
	bool m_ShowInFinder;
}
- (void)setDestinationPath:(NSString*)destFile;
- (void)setDeletePath:(NSString*)folder;
- (void)setShowInFinder:(bool)s;
- (void)HandleCompressionFinishedNotification:(NSNotification*)notification;
- (void)CompressionFinished:(NSTask*)task;
@end

@implementation ShowCompressionCompressionFinishedCallback

- (void)setDestinationPath:(NSString*)destFile
{
	m_DestFile = destFile;
	[m_DestFile retain];
}

- (void)setDeletePath:(NSString*)folder
{
	m_Folder = folder;
	[m_Folder retain];
}

- (void)setShowInFinder:(bool)s
{
	m_ShowInFinder = s;
}

- (void)dealloc
{
	[m_DestFile release];
	[m_Folder release];
	
	[super dealloc];
}

//// This is a callback called after compression of the package with ExportPackage is finished
//// It selectes the compressed file in the finder 
- (void)HandleCompressionFinishedNotification:(NSNotification*)notification
{
	[self CompressionFinished:(NSTask*) [notification object]];
}

- (void)CompressionFinished:(NSTask*)task
{
	if ([task terminationStatus] != 0)
		NSRunAlertPanel (@"Error while Exporting", @"Error compressing folder", @"Ok", NULL, NULL);

	NSArray* args = [task arguments];
	if ([args count] == 3)
	{
		NSString* nspath = [args objectAtIndex: 1];
		
		MoveReplaceFile([nspath UTF8String], [m_DestFile UTF8String]);
		
		if ( m_ShowInFinder )
		{
			[[NSWorkspace sharedWorkspace]selectFile: m_DestFile inFileViewerRootedAtPath: NULL];
		}
		[task release];
	}

	[[NSFileManager defaultManager]removeFileAtPath: m_Folder handler: NULL];
	
	// clear the modal progress bar
	ClearProgressbar();
	
	[self autorelease];
}

@end

void CompressPackageTarGZAndDeleteFolder (const std::string& folder, const std::string& compressedPath, bool runAsyncAndShowInFinder)
{
	NSString* tempExportPackage = MakeNSString(AppendPathName(PathToAbsolutePath("Temp"), kTempExportPackage));
	
	// clear the place for the temporary and final package
	[[NSFileManager defaultManager]removeFileAtPath: tempExportPackage handler: NULL];
	[[NSFileManager defaultManager]removeFileAtPath: MakeNSString(compressedPath) handler: NULL];
	
	//// Compress the folder and store it to the user selected path
	NSTask* compressionTask = [[NSTask alloc]init];
	// Add finish notification
	NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
	
	ShowCompressionCompressionFinishedCallback* callback = [[ShowCompressionCompressionFinishedCallback alloc]init];
	[callback setDestinationPath: MakeNSString(compressedPath)];
	[callback setDeletePath: MakeNSString(folder)];
	[callback setShowInFinder: runAsyncAndShowInFinder];
	
	if ( runAsyncAndShowInFinder )
		[defaultCenter addObserver:callback selector: @selector(HandleCompressionFinishedNotification:) name: NSTaskDidTerminateNotification object: compressionTask];
	// set params
	[compressionTask setLaunchPath: @"/usr/bin/tar"];
	[compressionTask setCurrentDirectoryPath: MakeNSString(folder)];
	NSArray* args = [NSArray arrayWithObjects: 
					 @"-zcpf", tempExportPackage, @".", nil];
		
	[compressionTask setArguments: args];
	[compressionTask setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
	[compressionTask setStandardOutput: [NSFileHandle fileHandleWithStandardOutput]];
	// launch
	[compressionTask launch];
	if ( !runAsyncAndShowInFinder ) 
	{
		[compressionTask waitUntilExit];
		[callback CompressionFinished: compressionTask];
	}
}

bool DecompressPackageTarGZ (const string& path, const string& destination)
{
	vector<string> args;
	args.push_back ("-zxpf");
	args.push_back (PathToAbsolutePath(path));
	string error;
	bool success = LaunchTaskArray("/usr/bin/tar", &error, args, true, destination);
	if (!error.empty ())
		printf_console ("%s\n", error.c_str ());

	if (!success)
	{
		args[0] = "-xpf";
		success = LaunchTaskArray("/usr/bin/tar", &error, args, true, destination);
		if (!error.empty ())
			printf_console ("%s\n", error.c_str ());
	}
		
	return success;
}


bool SetPermissionsReadWriteInternal (const string& path, bool assetsLibraryAndTempFolder)
{
	AssertIf (path.empty () || path[0] != kPathNameSeparator);
		
	string error;
	vector<string> args;
	if (assetsLibraryAndTempFolder)
		args.push_back("--is-unity-project" );
	args.push_back("--");
	args.push_back(path);

	if (LaunchTaskArray (AppendPathName (GetApplicationPath (), "Contents/Tools/verify_permissions").c_str (), &error, args, true))
		return true;
		
	printf_console ("'%s'\n", error.c_str ());

	const char* kHeading = "Setting permissions of project folder";
	const char* kMessage = "Unity requires that the permissions of the project folder are set to read and write!";
	
	if (!DisplayDialog (kHeading, kMessage, "Set permissions", "Cancel"))
		return false;

	vector<string> args_fix; 
	if (assetsLibraryAndTempFolder)
		args_fix.push_back("--is-unity-project" );
	args_fix.push_back("--fix");
	args_fix.push_back("--uid");
	args_fix.push_back(IntToString(getuid()));
	args_fix.push_back("--");
	args_fix.push_back(path);
	
	if (LaunchTaskArray (AppendPathName (GetApplicationPath (), "Contents/Tools/verify_permissions").c_str (), &error, args_fix, true))
		return true;
	
	// If fixing permissions fails when run as a non-privileged user. Try again as a superuser.
	LaunchTaskAuthorizedArray (AppendPathName (GetApplicationPath (), "Contents/Tools/verify_permissions").c_str (), NULL, kHeading, args_fix, path);
	
	// Wait for the authorized process to exit
	int status;
	int pid = wait(&status);
	if (pid == -1 || ! WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printf_console("\n\n*****Authorized fix permissions failed****\n\n");
		return false;
	}
	
	return true;
}

bool SetPermissionsReadWrite (const string& path)
{
	return SetPermissionsReadWriteInternal (path, false);
}

bool SetPermissionsForProjectFolder (const std::string& projectPath)
{
	return true;
}

bool DisplayDialog (const std::string& title, const std::string& content, const std::string& firstButton, const std::string& secondButton)
{
	if (!Thread::CurrentThreadIsMainThread())
	{
		WarningString(Format("DisplayDialog cancelled because it was run from another thread: %s %s\n", title.c_str(), content.c_str()));
		return false; // cancel
	}
	
	if( IsBatchmode() )
	{
		printf_console("Cancelling DisplayDialog: %s %s\n", title.c_str(), content.c_str());
		return false; // cancel
	}
	
	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();

	int retVal = NSRunInformationalAlertPanel (
		MakeNSString (title),
		@"%@",
		MakeNSString (firstButton),
		secondButton.empty () ? nil : MakeNSString(secondButton),
		nil,
		MakeNSString(content) );

	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);
	return retVal == NSAlertDefaultReturn;
}

int DisplayDialogComplex (const std::string& title, const std::string& content, const std::string& okButton, const std::string& secondary, const std::string& third)
{
	if (!Thread::CurrentThreadIsMainThread())
	{
		WarningString(Format("DisplayDialog cancelled because it was run from another thread: %s %s\n", title.c_str(), content.c_str()));
		return 1; // cancel
	}
	
	if( IsBatchmode()  )
	{
		printf_console("Cancelling DisplayDialog: %s %s\n", title.c_str(), content.c_str());
		return 1; // cancel
	}
	
	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();

	int retVal = NSRunInformationalAlertPanel (
		MakeNSString(title),
		@"%@",
		MakeNSString(okButton),
		MakeNSString(secondary),
		MakeNSString(third),
		MakeNSString(content));

	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);
	
	if (retVal == NSAlertDefaultReturn)	
		return 0;
	else if (retVal == NSAlertAlternateReturn)
		return 1;
	else
		return 2;
}

void RelaunchWithArguments (vector<string>& args)
{
	SetRelaunchApplicationArguments (args);
	[NSApp terminate: NULL];
	SetRelaunchApplicationArguments (vector<string> ());
}

string GetDefaultApplicationForFile (const string& path)
{
	NSString* mayaAppPath;
	NSString* type;
	string absolutePath = PathToAbsolutePath (path);
	if ([[NSWorkspace sharedWorkspace]getInfoForFile: [NSString stringWithUTF8String: absolutePath.c_str ()] application: &mayaAppPath type: &type])
		return MakeString (mayaAppPath);
	else
		return string ();
}

std::string RunOpenPanel (const std::string& title, const std::string& directory, const std::string& extension)
{
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	[panel setTitle: MakeNSString (title)];
	NSString* nsdir = MakeNSString(PathToAbsolutePath (directory));

	if ([panel runModalForDirectory: nsdir file: NULL types:  (extension.empty()?nil:[NSArray arrayWithObject: MakeNSString (extension)])] == NSOKButton)
		return MakeString ([panel filename]);
	else
		return string ();
}

bool BasicSavePanelIsValidFileName (void* userData, const string& fileName)
{
	if (!CheckValidFileName(GetLastPathNameComponent(fileName)))
	{
		DisplayDialog("Correct the file name", "Special characters and reserved names cannot be used for file names.", "Ok");
		return false;
	}
	
	return true;
}

std::string RunSavePanel (const std::string& title, const std::string& directory, const std::string& extension, const std::string& defaultName)
{
	NSSavePanel* panel = [NSSavePanel savePanel];
	 
	[panel setTitle: MakeNSString (title)];
	NSString* nsdir = MakeNSString (PathToAbsolutePath(directory));
	 
	if ( !extension.empty())
		[panel setAllowedFileTypes: [NSArray arrayWithObject: MakeNSString (extension)]];
	else
		[panel setAllowedFileTypes: nil ];
	 
	int res = [panel runModalForDirectory: nsdir file: MakeNSString (defaultName)]; 
	 
	if (res == NSOKButton)
		return MakeString ([panel filename]);
	else
		return string ();
}


std::string RuniPhoneReplaceOrAppendPanel (const std::string& title, const std::string& directory, const std::string& defaultName)
{
	AppendSavePanel* panel = [[AppendSavePanel alloc] initWithBuildCanBeAppended:&iOSBuildCanBeAppended 
								andCantAppendMessage: @"Build folder already exists and was created with other Unity iPhone version or for different target device setting. Would you like to replace it?"];
	
	[panel setTitle: MakeNSString (title)];
	NSString* nsdir = MakeNSString(PathToAbsolutePath(directory));
	
	[panel setAllowedFileTypes: nil ];
	
	if ([panel runModalForDirectory: nsdir file: MakeNSString (defaultName)] == NSOKButton)
	{
		NSFileManager* fm = [NSFileManager defaultManager];
		if ([fm fileExistsAtPath: [panel filename]])
			GetEditorUserBuildSettings().SetAppendProject([panel appendSelected]);
		else
			GetEditorUserBuildSettings().SetAppendProject(false);
		
		return MakeString ([panel filename]);
	}
	else
		return string ();
}

std::string RunOpenFolderPanel (const std::string& title, const std::string& directory, const std::string& defaultName)
{
	// SaveFolderPanel already matches the Windows behavior, so let's reuse it.
	return RunSaveFolderPanel(title, directory, defaultName, false);
}

std::string RunSaveFolderPanel (const std::string& title,
                                const std::string& directory,
                                const std::string& defaultName,
                                bool canCreateFolder)
{
	// Yes NSOpenPanel and not NSSavePanel, as when saving to a folder, you are in fact opening the folder for wrting... get it? eh eh! :-P
	NSOpenPanel* panel = [NSOpenPanel openPanel];
		
	[panel setTitle: MakeNSString (title)];
	NSString* nsdir = MakeNSString(PathToAbsolutePath(directory));
		
	[panel setCanChooseDirectories:YES ];
	[panel setCanChooseFiles:NO ];
	[panel setPrompt: @"Choose" ];

	if (canCreateFolder)
		[panel setCanCreateDirectories:YES ];

	if ([panel runModalForDirectory: nsdir file: MakeNSString (defaultName) types: nil] == NSOKButton)
		return MakeString ([panel filename]);
	else
		return string ();
}

@implementation AppendSavePanel

- (id)initWithBuildCanBeAppended:(BuildCanBeAppendedCallback)buildCanBeAppended andCantAppendMessage:(NSString *)cantAppendMessage
{
	if (self = [super init]) {
		m_Append = YES;
		m_BuildCanBeAppended = buildCanBeAppended;
		if (cantAppendMessage != nil)
			m_CantAppendMessage = cantAppendMessage;
		else
			m_CantAppendMessage = @"";
	}
	
	return self;
}

- (BOOL)appendSelected
{
	return m_Append;
}

- (BOOL)_overwriteExistingFileCheck:(NSString *)filename
{
	NSFileManager* fm = [NSFileManager defaultManager];
	if ([fm fileExistsAtPath: filename])
	{	
		CanAppendBuild canBeAppended = (*m_BuildCanBeAppended)(MakeString(filename));

		NSString *msg = canBeAppended == kCanAppendBuildYes ? @"Build folder already exists. Would you like to append or replace it?" : m_CantAppendMessage;

		AppendReplaceController *ctrl = [[AppendReplaceController alloc] initWithWindowNibName:@"AppendReplace"];
		OverwriteResult res = [ctrl show:self message:msg enableAppend:(canBeAppended == kCanAppendBuildYes)];
		
		if (res == kOverwriteResultCancel)
			return NO;
		
		if (res == kOverwriteResultAppend)
			m_Append = YES;
		else
			m_Append = NO;
		
		return YES;
	}
	else if (MakeString(filename).find("/Assets/") != std::string::npos)
	{
		int res = NSRunAlertPanel(@"Warning", @"It is not recommended to store your XCode project inside your Unity Assets folder,"
								  " since this could lead to a damaged Unity Project.  Are you sure you want to continue?"
								  " Choose Cancel if you are unsure.", @"Cancel", @"Continue", nil);
		
		if (NSAlertDefaultReturn == res)
			return NO;
	}
	
	return YES;
}
@end

std::string RunComplexOpenPanel (const std::string& title, const std::string message, const string& openButton, const std::string& directory, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData)
{
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	DialogDelegate* delegate = [[DialogDelegate alloc]init];
		
	delegate->isValid = isValidFilename;
	delegate->shouldShowFilename = shouldShowFileName;
	delegate->userData = userData;
		
	[panel setTitle: MakeNSString (title)];
	[panel setDelegate: (id)delegate];
	if (!message.empty ())
		[panel setMessage: MakeNSString (message)];
	if (!openButton.empty ())
		[panel setPrompt: MakeNSString (openButton)];
	
	// trying to fix crash in Import new asset ... (case 1045)
	[delegate retain];
	[[panel title]retain];
	[[panel message]retain];
	[[panel prompt]retain];
			
	NSString* nsdir = NULL;
	if (directory.empty ())
		nsdir = GetProjectDirectory();
	else
		nsdir = MakeNSString (directory);
	
	int res = [panel runModalForDirectory: nsdir file: NULL types: NULL];
	[panel setDelegate: NULL];
	delegate->userData = NULL;
	[delegate release];

	// trying to fix crash in Import new asset ... (case 1045)
	[delegate release];
	[[panel title]release];
	[[panel message]release];
	[[panel prompt]release];

	if (res == NSOKButton)
		return MakeString ([panel filename]);
	else
		return string ();
}

std::string RunComplexSavePanel (const std::string& title, const std::string message, const std::string& saveButton, const std::string& directory, const std::string& defaultName, const std::string& extension, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData)
{
	NSSavePanel* panel = [NSSavePanel savePanel];
	DialogDelegate* delegate = [[DialogDelegate alloc]init];
		
	delegate->isValid = isValidFilename;
	delegate->shouldShowFilename = shouldShowFileName;
	delegate->userData = userData;
	[panel setTitle: MakeNSString (title)];
	[panel setDelegate: (id)delegate];
	[panel setNameFieldLabel: MakeNSString (saveButton)];
	
	if (!message.empty ())
		[panel setMessage: MakeNSString (message)];
	
	NSString* nsdir = NULL;
	if (!directory.empty ())
		nsdir = MakeNSString (directory);

	[panel setRequiredFileType: MakeNSString(extension)];
	
	// trying to fix crash in Import new asset ... (case 1045)
	[delegate retain];
	[[panel title]retain];
	[[panel message]retain];
	[[panel prompt]retain];

	int res = [panel runModalForDirectory: nsdir file: MakeNSString (defaultName)];
	[panel setDelegate: NULL];
	delegate->userData = NULL;
	[delegate release];

	// trying to fix crash in Import new asset ... (case 1045)
	[delegate release];
	[[panel title]release];
	[[panel message]release];
	[[panel prompt]release];

	if (res == NSOKButton)
		return MakeString ([panel filename]);
	else
		return string ();
}


void OpenURLInWebbrowser (const string& url)
{
	if (!url.empty())
		[[NSWorkspace sharedWorkspace]openURL: [NSURL URLWithString:MakeNSString (url)]];
}

void OpenPathInWebbrowser (const string& path)
{
	if (!path.empty())
		[[NSWorkspace sharedWorkspace]openURL: [NSURL fileURLWithPath:MakeNSString (path)]];
}

void RevealInFinder (const string& path)
{
	[[NSWorkspace sharedWorkspace]selectFile: MakeNSString (PathToAbsolutePath (path)) inFileViewerRootedAtPath: NULL];
}


void UnityBeep()
{
	NSBeep();
}

#if defined(__GNUC__) && !UNITY_RELEASE
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

bool AmIBeingDebugged()
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.
    
    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.
    
    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    AssertIf(junk != 0);

    // We're being debugged if the P_TRACED flag is set.
    
    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}
#else
bool AmIBeingDebugged() { return false; }
#endif

std::string GetBundleVersionForApplication (const std::string& path)
{
	string absolutePath = PathToAbsolutePath (path);
	NSBundle* bundle = [NSBundle bundleWithPath: [NSString stringWithUTF8String: absolutePath.c_str ()]];
	if (bundle == NULL)
		return string();
	
	NSString* version = [[bundle infoDictionary]valueForKey: @"CFBundleVersion"];
	return MakeString(version);
}



@interface DockIconProgress : NSObject

+ (void) setProgress:(float)value;

@end

@implementation DockIconProgress

// taken from http://svn.oofn.net/CTProgressBadge/trunk/CTProgressBadge.m
+ (NSImage *)progressBadgeOfSize:(float)size withProgress:(float)progress
{
	float scaleFactor = size/16;
	float stroke = 2*scaleFactor;	//native size is 16 with a stroke of 2
	float shadowBlurRadius = 1*scaleFactor;
	float shadowOffset = 1*scaleFactor;
	
	float shadowOpacity = .4;
	
	NSRect pieRect = NSMakeRect(shadowBlurRadius,shadowBlurRadius+shadowOffset,size,size);
	
	NSImage *progressBadge = [[NSImage alloc] initWithSize:NSMakeSize(size + 2*shadowBlurRadius, size + 2*shadowBlurRadius+1)];
	
	[progressBadge lockFocus];
	[NSGraphicsContext saveGraphicsState];
	NSShadow *theShadow = [[NSShadow alloc] init];
	[theShadow setShadowOffset: NSMakeSize(0,-shadowOffset)];
	[theShadow setShadowBlurRadius:shadowBlurRadius];
	[theShadow setShadowColor:[[NSColor blackColor] colorWithAlphaComponent:shadowOpacity]];
	[theShadow set];
	[theShadow release];
	[[NSColor colorWithDeviceRed:91./255 green:133./255 blue:182./255 alpha:1] set];
	[[NSBezierPath bezierPathWithOvalInRect:pieRect] fill];
	[NSGraphicsContext restoreGraphicsState];
	
	[[NSColor whiteColor] set];
	if(progress <= 0)
	{
		[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(pieRect)+stroke,NSMinY(pieRect)+stroke,
														   NSWidth(pieRect)-2*stroke,NSHeight(pieRect)-2*stroke)] fill];
	}
	else if(progress < 1)
	{
		NSBezierPath *slice = [NSBezierPath bezierPath];
		[slice moveToPoint:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect))];
		[slice appendBezierPathWithArcWithCenter:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect)) radius:NSHeight(pieRect)/2-stroke startAngle:90 endAngle:90-progress*360 clockwise:NO];
		[slice moveToPoint:NSMakePoint(NSMidX(pieRect),NSMidY(pieRect))];
		[slice fill];
	}
	[progressBadge unlockFocus];
	
	return progressBadge;
}

+ (void) setProgress:(float)value
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	if (value >= 0.0f && value < 1.0f)
	{
		NSImage *overlayImage = [[NSImage alloc] initWithSize:NSMakeSize(128,128)];
		NSImage *badgeImage = [DockIconProgress progressBadgeOfSize:42 withProgress:value];
		NSImage *appIcon = [NSImage imageNamed:@"NSApplicationIcon"];
		
		[overlayImage lockFocus];
		[appIcon drawAtPoint:NSMakePoint(0,0) fromRect:NSZeroRect operation: NSCompositeCopy fraction:1.0];
		[badgeImage compositeToPoint:NSMakePoint(0,80) fromRect:NSZeroRect operation: NSCompositeSourceOver fraction:1.0];
		[overlayImage unlockFocus];
		
		[NSApp setApplicationIconImage: overlayImage];
		
		[badgeImage release];
		[overlayImage release];	
	}
	else
	{
		NSImage *appIcon = [NSImage imageNamed:@"NSApplicationIcon"];
		[NSApp setApplicationIconImage: appIcon];
	}
	[pool drain];
}

@end


@interface GlobalProgressbarController : NSObject
{
	@public
	id m_Progress;
	id m_Text;
	bool m_WantsToCancel;
	bool m_CanCancel;
	bool m_CancelableNibLoaded;
	bool m_NibLoaded;
	Vector2f m_BuildInterval;
	NSModalSession m_ModalSession;
	NSDate* m_StartTime;
}

@end

@implementation GlobalProgressbarController

- (id)init
{
	m_ModalSession = 0;
	m_StartTime = [[NSDate alloc]init];
	m_WantsToCancel = false;
	m_BuildInterval = Vector2f(-1.0f, -1.0f);
	m_CancelableNibLoaded = false;
	m_NibLoaded = false;
	return self;
}

- (void)dealloc
{
	[DockIconProgress setProgress:-1.0f];
	if (m_ModalSession != NULL)
		[NSApp endModalSession:m_ModalSession];
	[[m_Progress window]close];
	[m_StartTime release];

	[super dealloc];
}

- (void)cancel: (id)sender
{
	m_WantsToCancel = true;
}

- (void)setBuildInterval:(Vector2f)value
{
	m_BuildInterval = value;
}

- (bool)isBuildProgress
{
	return (m_BuildInterval.x >= 0.0f && m_BuildInterval.x < 1.0f);
}

- (void) setProgressbar:(float)value andText:(const string&) text andTitle:(const string&) title canCancel:(bool)canCancel
{
	if (m_Progress == NULL || m_CanCancel != canCancel)
	{
		if (m_Progress != NULL)
		{
			[NSApp endModalSession:m_ModalSession];
			[[m_Progress window]close];
		}

		if (!m_CancelableNibLoaded && canCancel)
		{
			if (LoadNibNamed (@"ScriptProgressWithCancel.nib", self) == false)
				NSLog (@"nib not loadable");
			else
			{
				m_CancelableNibLoaded = true;
			}
		}
		else if (!m_NibLoaded && !canCancel)
		{
			if (LoadNibNamed (@"ScriptProgress.nib", self) == false)
				NSLog (@"nib not loadable");
			else
			{
				m_NibLoaded = true;
			}
		}
		
		[[m_Progress window] setTitle: MakeNSString(title)];
		[[m_Progress window]center];
		[[m_Progress window]orderFront: NULL];
		[m_Progress setMaxValue: 1.0F];
		m_ModalSession = [NSApp beginModalSessionForWindow:[m_Progress window]];
		m_CanCancel = canCancel;
		
		// Fix for continuing bouncing dock icon (case 383402):
		// From the Max OSX Reference Library: "If the inactive application presents a modal panel, requestUserAttention will be invoked with NSCriticalRequest automatically."
		// To prevent the bouncing dock icon when using modal progress bars we here manually cancel that userAttentionRequest. (We still get one bounce though)
		[NSApp cancelUserAttentionRequest:NSCriticalRequest];		
	}

	if (m_Progress)
	{
		float progress = value;
		if ([self isBuildProgress] )
		{
			// This lets child processes have their progress displayed as part of the overall build progress.
			progress = (m_BuildInterval.y - m_BuildInterval.x)*value + m_BuildInterval.x;
		}
		
		// Update progress if it has changed (We only count full percentage point to avoid to many redraws)
		int oldPercent = RoundfToInt([m_Progress doubleValue] * 100);
		int newPercent = RoundfToInt(progress * 100);
		string oldTitle = MakeString([[m_Progress window]title]);
		if (oldPercent != newPercent || text != MakeString([m_Text stringValue]) || title != oldTitle)
		{	
			[[m_Progress window]setTitle: MakeNSString(title)];
			[m_Progress setDoubleValue: progress];
		
			// Update progress if it has changed (We only count full percentage point to avoid to many redraws)
			NSString* newString = MakeNSString(text);
			if ([[m_Text stringValue]compare: newString] != NSOrderedSame)
				[m_Text setStringValue: MakeNSString(text)];
			
			[m_Progress displayIfNeeded];
			[m_Text displayIfNeeded];
			
			[NSApp runModalSession:m_ModalSession];

			[DockIconProgress setProgress:progress];
		}
	}
}

@end

static GlobalProgressbarController* gGlobalProgressbar = NULL;

ProgressBarState DisplayBuildProgressbar (const std::string& title, const std::string& info, Vector2f buildInterval, bool canCancel)
{
	if (gGlobalProgressbar == NULL)
		gGlobalProgressbar = [[GlobalProgressbarController alloc]init];

	[gGlobalProgressbar setBuildInterval:buildInterval];
	if ([gGlobalProgressbar isBuildProgress])
	{
		return DisplayProgressbar (title, info, 0.0f, canCancel);
	}
	return kPBSNone;
}



ProgressBarState DisplayProgressbar (const std::string& title, const std::string& info, float progress, bool canCancel)
{
	if( IsBatchmode() )
	{
		static std::string prevTitle;
		if (title != prevTitle) {
			printf_console("DisplayProgressbar: %s\n", title.c_str());
			prevTitle = title;
		}
		return kPBSNone;
	}

	NSAutoreleasePool *subPool = [[NSAutoreleasePool alloc] init];
	
	if (gGlobalProgressbar == NULL)
		gGlobalProgressbar = [[GlobalProgressbarController alloc]init];
		
	[gGlobalProgressbar setProgressbar: progress andText: info andTitle: title canCancel:canCancel];

	[subPool drain];
	
	return gGlobalProgressbar->m_WantsToCancel ? kPBSWantsToCancel : kPBSNone;
}

void ClearProgressbar ()
{
	if( IsBatchmode() )
		return;

	// During build we do not allow to clear the progress bar, this
	// prevents spawning OSX user attention requests (bouncing dock icon) during the build process.
	// The build process clears the progress bar on its cleanup.
	// On the other hand if we canceled progress bar - clear anyway
	if (gGlobalProgressbar && gGlobalProgressbar->m_WantsToCancel || ![gGlobalProgressbar isBuildProgress])
	{
		[gGlobalProgressbar release];
		gGlobalProgressbar = NULL;
	}
}




short IsKeyPressed(unsigned short k )
{
	unsigned char km[16];
#ifdef MAC_OS_X_VERSION_10_6
	GetKeys( reinterpret_cast<BigEndianUInt32*> (km));
#else
	GetKeys( reinterpret_cast<BigEndianLong*> (km));
#endif
	return ( ( km[k>>3] >> (k & 7) ) & 1);
}


void SetCocoaCurrentDirectory(std::string path)
{
	[[NSFileManager defaultManager]changeCurrentDirectoryPath: MakeNSString (path)];
}

string GetCocoaCurrentDirectory()
{
	return MakeString ([[NSFileManager defaultManager] currentDirectoryPath]);
}

bool IsApplicationActiveOSImpl ()
{
	return [NSApp isActive];
}

void ActivateApplication ()
{
	[NSApp activateIgnoringOtherApps: YES];
}

bool IsOptionKeyDown()
{
	return GetCurrentKeyModifiers() & (optionKey | rightOptionKey);
}

bool CopyFileOrDirectoryFollowSymlinks (const string& from, const string& to)
{
	if (!IsPathCreated(from))
		return false;
	if (IsPathCreated(to))
		return false;
	
	return LaunchTask ("/bin/cp", NULL, "-RL", PathToAbsolutePath(from).c_str(), PathToAbsolutePath(to).c_str(), NULL);
}

// Busy "beachball" cursor is handled by the OS on the Mac, nothing to do here
void BeginBusyCursor() { }
void EndBusyCursor() { }
void EnsureBusyCursor() { }


ExternalTask::ExternalTask() : task(0) {}

std::auto_ptr<ExternalTask> ExternalTask::LauchTask(const std::string& taskPath, const std::vector<std::string>& arguments)
{
	NSMutableArray* args = [[NSMutableArray alloc] initWithCapacity:arguments.size()];
	for (unsigned int i = 0; i < arguments.size(); ++i)
		[args addObject: MakeNSString(arguments[i].c_str())];

	NSTask* task = [[NSTask alloc]init];
	[task setLaunchPath: MakeNSString (ResolveSymlinks(taskPath))];
	[task setArguments: args];
	[task setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
	[task setStandardOutput: [NSFileHandle fileHandleWithStandardOutput]];
	
	NS_DURING
	[task launch];
	NS_HANDLER
	NS_VALUERETURN (std::auto_ptr<ExternalTask>(), std::auto_ptr<ExternalTask>);
	NS_ENDHANDLER
	
	std::auto_ptr<ExternalTask> externalTask(new ExternalTask());
	externalTask->task = task;
	
	return externalTask;
}

ExternalTask::~ExternalTask()
{
	if (task)
	{
		Terminate();
		[task release];
	}
}

void ExternalTask::Terminate()
{
	Assert(task);
	[task terminate];
	task = NULL;
}

bool ExternalTask::IsRunning() const
{
	Assert(task);
	return [task isRunning];
}

std::auto_ptr<ExternalTask> ExternalTask::AttachWatchDog() const
{
	Assert(task);
	
	std::auto_ptr<ExternalTask> externalTask(new ExternalTask());
	externalTask->task = ::AttachWatchDog([task processIdentifier]);
	
	return externalTask;
}

void ExternalTask::Sleep(int miliseconds)
{
	pollfd p;
	poll (&p, 0, miliseconds);
}


NSTask* AttachWatchDog(int pid)
{
 	// launch the watchdog task
	NSTask* launchWatchdogTask = [[NSTask alloc]init];
	[launchWatchdogTask setLaunchPath: MakeNSString (AppendPathName (GetApplicationPath (), "Contents/Tools/auto_quitter"))];
	NSArray* args = [NSArray arrayWithObjects: [NSString stringWithFormat: @"%d", pid], nil];
	[launchWatchdogTask setArguments: args];
	[launchWatchdogTask setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
	[launchWatchdogTask setStandardOutput: [NSFileHandle fileHandleWithStandardOutput]];
	// launch synchrously
	NS_DURING
	[launchWatchdogTask launch];
	NS_HANDLER
	NS_VALUERETURN (NULL, NSTask*);
	NS_ENDHANDLER
	
	return launchWatchdogTask;
}
