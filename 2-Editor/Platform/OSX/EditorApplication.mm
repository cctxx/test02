#include "UnityPrefix.h"
#include "EditorApplication.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/Application.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Utilities/ARGV.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Selection.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#import <Foundation/NSDebug.h>
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/GfxDevice/opengl/GLAssert.h"
#include "Runtime/Misc/Player.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Editor/Platform/Interface/ProjectWizard.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Platform/Interface/RepaintController.h"
#include "Editor/Src/Utility/BuildPlayerHelpers.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Editor/Src/Utility/CurlRequest.h"
#if UNITY_IPHONE_AUTHORING
#include "Editor/Platform/OSX/iPhone/IPhoneRemoteBonjourServer.h"
#endif
#import "LicenseWebViewWindow.h"

using namespace std;
using std::set;
class TypeTree;

static void RelaunchUnity ();

void DisplayBleedingEdgeWarningIfRequired();

#define	kODBEditorSuite					'R*ch'
#define	kAEModifiedFile					'FMod'
#define	kAEClosedFile					'FCls'

static void RelaunchUnity ()
{
	vector<string> relaunchArgs = GetRelaunchApplicationArguments ();

	// Relaunch unity	
	string executable = MakeString ([[NSBundle mainBundle]executablePath]);
	string shell = "(sleep 0; \"" + executable + "\"";
	for (int i=0;i<relaunchArgs.size ();i++)
		shell += " \"" + relaunchArgs[i] + "\"";
	shell += ") &";
	system (shell.c_str ());
}

@implementation EditorApplication

- (NSApplicationTerminateReply)applicationShouldTerminate:(id)sender {

	if (!GetApplication().Terminate())
		return NSTerminateCancel;
	
	[[NSNotificationCenter defaultCenter]postNotificationName: NSApplicationWillTerminateNotification object:self userInfo:NULL];
	[self RemoveDisplayTimer];

	// Relaunch unity	
	vector<string> relaunchArgs = GetRelaunchApplicationArguments ();
	if (!relaunchArgs.empty ())
	{
		RelaunchUnity();
	}

	// Exit with error code in batch mode. This change came with FusionFall merge; and no one
	// remembers why it was needed. Keeping it here for safety :)
	if (IsBatchmode ())
	{
		exit (1);
	}
	else
	{
        RuntimeCleanup();
		exit (0); //@TODO: not doing exit() here causes crashes in some EditorWindows.mm code later on Mac. To fix later!
	}
	return NSTerminateNow;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification {
	// An application object needs to exist for the URL handler registered in registerAssetStoreURLProtocol is invoked
	new Application();

	// This needs to happen before applicationDidFinishLaunching:, as otherwise Unity will not get the URL that caused it to launch (if launched from an Asset Store link)
	[self registerAssetStoreURLProtocol];
}

- (void)requireOSXVersion
{
	if (systeminfo::GetOperatingSystemNumeric () >= 1060)
		return;
	
	DisplayDialog("Unity requires at least Mac OS X 10.6", "Please upgrade Mac OS X to use the Unity Editor", "Ok");
	
	exit (0);
}

LicenseWebViewWindow* PrepareLicenseActivationWindow();

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	if (!LicenseInfo::Activated())
	{
		if (!IsBatchmode())
		{
			NSWindow* webViewWindow = PrepareLicenseActivationWindow();
			[NSApp runModalForWindow:webViewWindow];
		}

		if (!LicenseInfo::Activated())
		{
			ErrorString("Unity has not been activated with a valid License. You must activate Unity with a valid license before using it. (also in batchmode)");
			exit (1);
		}
	}
	
//	SetupCocoaExceptionHandler ();
	
#if UNITY_IPHONE_AUTHORING
	IphoneRemoteBonjourServerInit();
#endif	

	NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO] forKey:@"NSQuitAlwaysKeepsWindows"];
    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
	
	[NSColor setIgnoresAlpha: NO];

	/// Enable debugging aids
	#if DEBUGMODE
		NSDebugEnabled = YES;
		// See NSDebug.h
//		NSZombieEnabled = NO;
	#endif
	
	[self requireOSXVersion];
	
	DisplayBleedingEdgeWarningIfRequired();

	// Initial application setup
	GetApplication().InitializeProject();
	
	// Special character menus because cocoa insists on auto adding it. Why doesn't this work???
//	[gMainCocoaMenu->m_Edit removeItemAtIndex: 8];
//	[gMainCocoaMenu->m_Edit removeItemAtIndex: 7];
//	[gMainCocoaMenu->m_Edit removeItemAtIndex: 6];

	GLAssertString ("OpenGL error after Pane initialization: ");

			
	// Register tick timer
	[self RegisterUpdateTimer];

	// Register modified file refresh callback
	// This works with BBEdit-compatible editors (via kODBEditorSuite event class)
	[[NSAppleEventManager sharedAppleEventManager]setEventHandler: self andSelector: @selector (refreshModifiedFile: withReplyEvent: ) forEventClass: kODBEditorSuite andEventID: kAEModifiedFile];
	[[NSAppleEventManager sharedAppleEventManager]setEventHandler: self andSelector: @selector (closeFile: withReplyEvent: ) forEventClass: kODBEditorSuite andEventID: kAEClosedFile];
	
	// Complete loading project
	GetApplication().FinishLoadingProject();
	
	GetApplication().AfterEverythingLoaded();
}


-(void)applicationDidResignActive:(NSNotification *)aNotification
{
	if (Application::IsInitialized())
	{
		GetApplication().ResetReloadAssemblies();

		// if the application is (already) paused by the user - dont start it
		if (GetApplication().IsPaused())
			return;

		// kPlayerPausing will automatically switch to kPlayerPaused in next frame
		SetPlayerPause(!GetPlayerRunInBackground() ? kPlayerPausing : kPlayerRunning);
	}
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
	if (Application::IsInitialized())
	{
		SetPlayerPause(GetApplication().IsPaused() ? kPlayerPaused : kPlayerRunning);
		GetUndoManager().IncrementCurrentGroup ();
	}
}

- (void)autoRefreshIfPossible
{
	if (!GetApplicationPtr())
		return;
	GetApplication().AutoRefresh();
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
	[self autoRefreshIfPossible];
	
	if (Application::IsInitialized())
	{
		GetUndoManager().IncrementCurrentGroup ();
	}
}

// Automatically refresh the assets when a modified file event is sent
- (void)refreshModifiedFile:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	[self performSelector:@selector(autoRefreshIfPossible) withObject:NULL afterDelay: 0.05];
}

- (void)closeFile:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
// We don't really care about this, but TextMate will beep when we don't respond to this AppleEvent
}

- (IBAction)SaveAssets:(id)sender
{
	AssetInterface::Get ().SaveAssets ();
}

- (IBAction)LoadSceneFromDisk: (id)sender
{
	GetApplication().FileMenuOpen ();
}

- (IBAction) saveDocument:(id)sender
{
	GetApplication().FileMenuSave(true);
}

- (IBAction)SaveAsSceneToDisk:(id)sender
{
	GetApplication().FileMenuSaveAs ();
}

-(IBAction)CloseScene:(id)sender
{
	GetApplication().FileMenuNewScene ();
}

- (IBAction)EnterSerialNumber:(id)sender {
	GetApplication().EnterSerialNumber ();
}

- (IBAction)ReturnLicense:(id)sender {
	GetApplication().ReturnLicense ();
}

- (IBAction)CompileScene:(id)sender
{
	ShowBuildPlayerWindow ();
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
	GetUndoManager().IncrementCurrentGroup ();
	if (menuItem == m_CutItem)
		return ValidateCommandOnKeyWindow("Cut");
	else if (menuItem == m_CopyItem)
		return ValidateCommandOnKeyWindow("Copy");
	else if (menuItem == m_PasteItem)
		return ValidateCommandOnKeyWindow("Paste");
	else if (menuItem == m_DuplicateItem)
		return ValidateCommandOnKeyWindow("Duplicate");
	else if (menuItem == m_DeleteItem)
		return ValidateCommandOnKeyWindow("Delete");
	else if (menuItem == m_FindItem)
		return ValidateCommandOnKeyWindow("Find");
	else
		return true;
}

- (IBAction)delete:(id)sender
{
	ExecuteCommandOnKeyWindow("Delete");
}
- (IBAction)copy:(id)action
{
	ExecuteCommandOnKeyWindow("Copy");
}
- (IBAction)paste:(id)action
{
	ExecuteCommandOnKeyWindow("Paste");
}
- (IBAction)duplicate:(id)action
{
	ExecuteCommandOnKeyWindow("Duplicate");
}
- (IBAction)cut:(id)action
{
	ExecuteCommandOnKeyWindow("Cut");
}
- (IBAction)selectAll:(id)action
{
	ExecuteCommandOnKeyWindow("SelectAll");
}
- (IBAction)frameSelected:(id)action
{
	if (!ExecuteCommandInMouseOverWindow("FrameSelected"))
		if (!ExecuteCommandOnKeyWindow("FrameSelected"))
			FrameLastActiveSceneView(false);
}
- (IBAction)frameSelectedWithLock:(id)action
{
    if (!ExecuteCommandInMouseOverWindow("FrameSelectedWithLock"))
        if (!ExecuteCommandOnKeyWindow("FrameSelectedWithLock"))
            FrameLastActiveSceneView(true);
}
- (IBAction)find:(id)action
{
	ExecuteCommandOnKeyWindow("Find");	
}

- (IBAction)CompileSceneAutomatic:(id)sender
{
	BuildPlayerWithLastSettings ();
}

- (void)TickTimer
{
	ContainerWindow::HandleSpacesSwitch();
	GetApplication().TickTimer();
}

- (IBAction)NewProject:(id)sender
{
	ProjectWizard* wizard = [[ProjectWizard alloc]initIsLaunching: NO];
	[[wizard tabView]selectLastTabViewItem: self];
}

- (IBAction)OpenProject:(id)sender
{
	ProjectWizard* wizard = [[ProjectWizard alloc]initIsLaunching: NO];
	[[wizard tabView]selectFirstTabViewItem: self];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	return Application::OpenFileGeneric(MakeString(filename));
}

/// main.mm forwards the NSApplication sendEvent for us
- (void) sendEvent: (NSEvent  *)event
{
	if (!Application::IsInitialized())
		return;

	// Send flags changed to Mono
	if ([event type] == NSFlagsChanged)
		CallStaticMonoMethod ("EditorApplication", "Internal_CallKeyboardModifiersChanged");
	
	HandleGameViewInput(event);
	
	[[NSNotificationCenter defaultCenter]postNotificationName:@"SendEventNotification" object:event];
}

- (void)RemoveDisplayTimer
{
	[m_RenderTimer invalidate];
	[m_RenderTimer release];
	m_RenderTimer = NULL;
}

- (void)RegisterUpdateTimer
{
	const float kUpdateMaxFps = 250.0F;
	const float kInvUpdateMaxFps = 1.0F / kUpdateMaxFps;
	[self RemoveDisplayTimer];
	m_RenderTimer = [[NSTimer scheduledTimerWithTimeInterval: kInvUpdateMaxFps
		target: self selector:@selector(TickTimer) userInfo:nil repeats:true] retain];
}

- (IBAction)Step: (id) sender
{ 
	GetApplication().Step();
}

-(IBAction)Pause:(id)sender
{
	GetApplication().SetPaused([sender state]);
}

-(IBAction)Play:(id)sender {
	GetApplication().SetIsPlayingDelayed([sender state]);
}

- (IBAction)ShowAboutDialog:(id)sender
{
	ShowAboutDialog();
}
- (IBAction)ShowPreferences:(id)sender
{
	ShowPreferencesDialog();
}

// Assetstore protocol handler registration
- (void)registerAssetStoreURLProtocol
{
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(getUrl:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

// Assetstore protocol handler 
- (void)getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	std::string url = [[[event paramDescriptorForKeyword:keyDirectObject] stringValue] UTF8String ];
	if ( IsAssetStoreURL(url) )
		GetApplication().OpenAssetStoreURL(url);
}

@end
