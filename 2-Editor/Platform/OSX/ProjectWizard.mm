#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ProjectWizard.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Editor/Src/LicenseInfo.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/ProjectWizardUtility.h"
#include "Runtime/Utilities/ARGV.h"
#include "Configuration/UnityConfigureVersion.h"

static const char* kAssetsFolder = "Assets";

using namespace std;

@implementation ProjectWizard

- (id)initIsLaunching:(bool) launching
{
	[super init];
	
	m_Packages = new std::vector<std::string>() ;
	m_EnabledPackages = new std::vector<bool>() ;
	m_RecentProjects= new std::vector<std::string>() ;
	
	PopulatePackagesList(m_Packages, m_EnabledPackages);
	PopulateRecentProjectsList(*m_RecentProjects);

	// Load nib
	if ([NSBundle loadNibNamed: @"ProjectWizard.nib" owner: self] == false)
	{
		NSLog (@"Couldn't load project wizard nib!");
		return NULL;
	}
	
	[m_2D3DPopopButton insertItemWithTitle: @"3D" atIndex:kProjectTemplate3D];
	[m_2D3DPopopButton insertItemWithTitle: @"2D" atIndex:kProjectTemplate2D];
	[m_2D3DPopopButton selectItemAtIndex:kProjectTemplate3D];
	
	// Setup new project location
	[m_NewProjectLocation setStringValue: MakeNSString(ChooseProjectDirectory())];
	[self NewProjectLocationChanged: self];
	
	// Enable open button
	[m_OpenButton setEnabled: [m_OpenProjectTable selectedRow] != -1];
	[m_OpenProjectTable setDoubleAction: @selector (OpenDoubleClick:)];
	[m_OpenProjectTable setTarget: self];
	[m_OpenProjectTable registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
	
	std::string dlgTitle = Format("Project Wizard (%s)", UNITY_VERSION);
	[[self window]setTitle: MakeNSString(dlgTitle)];
	
	// Run modal when starting up!
	[[self window]makeKeyAndOrderFront:self];
	m_IsLaunching = launching;
	AssertIf ([self window] == NULL);
	if (m_IsLaunching)
		[NSApp runModalForWindow: [self window]];
	
	if (m_CloseWindow)
		[[self window]close];
	
	return self;
}

- (void)dealloc
{
	delete m_Packages ;
	delete m_EnabledPackages ;
	delete m_RecentProjects ;
	[super dealloc];
}

- (IBAction)SetCreateProjectPath:(id)sender
{
	NSSavePanel* panel = [NSSavePanel savePanel];
	[panel setDelegate: self];
	[panel setTitle: @"Create New Project"];
	[panel setDirectory: [m_NewProjectLocation stringValue]];
	
	if ([panel runModal] == NSFileHandlingPanelOKButton)
		[m_NewProjectLocation setStringValue: [panel filename]];
	
	[panel setDelegate: NULL];
	
	[self NewProjectLocationChanged: self];
}

- (IBAction)NewProjectLocationChanged:(id)sender
{
	string path = [[m_NewProjectLocation stringValue]UTF8String];
	string directory = DeleteLastPathNameComponent (path);
	
	[m_NewProjectButton setEnabled: !IsPathCreated (path) && IsDirectoryCreated (directory)];
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
	[self NewProjectLocationChanged: self];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	if (aTableView == m_PackageTable)
		return m_Packages->size ();
	else
		return m_RecentProjects->size ();
}

- (void)tableView:(NSTableView *)aTableView willDisplayCell:(id)aCell forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	if (aTableView == m_PackageTable)
	{
		[aCell setState: (*m_EnabledPackages)[rowIndex]];
		[aCell setTitle: MakeNSString(GetLastPathNameComponent((*m_Packages)[rowIndex]))];
	}
	else
	{
		[aCell setStringValue: MakeNSString ((*m_RecentProjects)[rowIndex])];
	}
}

- (id)tableView:(NSTableView *)aTableView 
	objectValueForTableColumn:(NSTableColumn *)aTableColumn
	row:(int)rowIndex 
{
	return NULL;
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	if (aTableView == m_PackageTable)
		(*m_EnabledPackages)[rowIndex] = [anObject boolValue];
}

- (IBAction)Create:(id)sender
{	
	m_Complete = true;

	string path = MakeString ([m_NewProjectLocation stringValue]);
	
	if (m_IsLaunching)
	{
		CreateDirectory (path);
		CreateDirectory (AppendPathName (path, kAssetsFolder));
		SetProjectPath (path, false);

		vector<string> packages;
		for (int i=0;i<m_Packages->size ();i++)
		{
			if ((*m_EnabledPackages)[i])
				packages.push_back ((*m_Packages)[i]);
		}

		ProjectDefaultTemplate defaultTemplate = (ProjectDefaultTemplate)[m_2D3DPopopButton indexOfSelectedItem];
		SetIsCreatingProject (packages, defaultTemplate);
		[NSApp stopModal];
		m_CloseWindow = true;
	}
	else
	{
		vector<string> commandline;
		commandline.push_back ("-createProject");
		commandline.push_back (path);
		for (int i=0;i<m_Packages->size ();i++)
		{
			if ((*m_EnabledPackages)[i])
				commandline.push_back ((*m_Packages)[i]);
		}
		// Relaunch the application
		RelaunchWithArguments (commandline);
		m_Complete = false;
	}
}

- (IBAction)Open:(id)sender
{
	if ([m_OpenProjectTable selectedRow] == -1)
		return;
	string projectPath = (*m_RecentProjects)[[m_OpenProjectTable selectedRow]];
	[self CompleteOpenProject: projectPath];
}

- (IBAction)OpenDoubleClick:(id)sender
{
	if ([m_OpenProjectTable clickedRow] == -1)
		return;
	string projectPath = (*m_RecentProjects)[[m_OpenProjectTable clickedRow]];
	[self CompleteOpenProject: projectPath];
}

- (IBAction)OpenOther:(id)sender
{
	id panel = [NSOpenPanel openPanel];
	[panel setTitle: @"Choose Project Directory"];
	[panel setCanChooseFiles: NO];
	[panel setCanChooseDirectories: YES];
	[panel setDelegate: self];
	
	if ([panel runModalForDirectory: NULL file: NULL types: NULL] != NSOKButton)
	{
		[panel setDelegate: NULL];
		return;
	}

	string path = MakeString ([[panel filenames]objectAtIndex: 0]);
	string projectPath;
	[panel setDelegate: NULL];
	
	ExtractProjectAndScenePath(path, &projectPath, NULL);
	
	if (IsProjectFolder (projectPath))
		[self CompleteOpenProject: projectPath];
	else
		NSBeep ();
}

- (IBAction)CompleteOpenProject:(string&)projectPath
{
	m_Complete = true;
	if (m_IsLaunching)
	{
		SetProjectPath (projectPath, false);
		[NSApp stopModal];
		m_CloseWindow = true;
	}
	else
	{
		vector<string> commandline;
		commandline.push_back ("-projectpath");
		commandline.push_back (projectPath);
		RelaunchWithArguments (commandline);
		m_Complete = false;
	}
}

- (void)AddProjectPath:(string&)projectPath
{
	for (std::vector<std::string>::iterator i = m_RecentProjects->begin(); i != m_RecentProjects->end();)
	{
		if (*i == projectPath)
			m_RecentProjects->erase(i);
		else
			i++;
	}

	m_RecentProjects->insert(m_RecentProjects->begin(), 1, projectPath);	
}

- (void)windowWillClose:(NSNotification *)aNotification
{
	[self autorelease];
	if (m_IsLaunching)
	{
		if (!m_Complete)
			ExitDontLaunchBugReporter ();
	}	
}

- (BOOL)IsInsideOpenProjectTab
{
	return [m_TabView indexOfTabViewItem: [m_TabView selectedTabViewItem]] == 0;
}

- (BOOL)panel:(id)sender isValidFilename:(NSString *)filename
{
	if ([self IsInsideOpenProjectTab])
		return ExtractProjectAndScenePath(MakeString (filename), NULL, NULL);
	else
	{
		string path = MakeString (filename);
		return !IsPathCreated (path);
	}
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename
{
	if ([self IsInsideOpenProjectTab])
		return IsDirectoryCreated (MakeString (filename));
	else
	{
		string path = MakeString (filename);
		return IsDirectoryCreated (path);
	}
}

- (NSWindow*)window
{
	return [m_TabView window];
}

- (NSTabView*)tabView
{
	return m_TabView;
}
@end


/// Fix for being able to press Enter and Return to open the project!
@interface OpenProjectWizardTableView : NSTableView
{
	NSDragOperation m_DragResult;
	string m_DragProjectPath;
}
- (void) keyDown:(NSEvent*) event;
- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender;
- (NSDragOperation)draggingUpdated:(id < NSDraggingInfo >)sender;
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender;
- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender;
@end


@implementation OpenProjectWizardTableView 

- (void) keyDown:(NSEvent*)event
{
	unsigned short c = 0;
	c = [[event characters]characterAtIndex: 0];
	if (c == NSEnterCharacter || c == NSCarriageReturnCharacter)
		[((ProjectWizard*)[self delegate]) Open: self];
	else
		[super keyDown: event];
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender
{
	m_DragResult = NSDragOperationNone;
	if ([[sender draggingPasteboard] availableTypeFromArray:[NSArray arrayWithObject:NSFilenamesPboardType]]) {
		NSPasteboard *pb = [sender draggingPasteboard];
		NSString *type = [pb availableTypeFromArray:[NSArray arrayWithObject:NSFilenamesPboardType]];
		NSArray *array = [[pb stringForType:type] propertyList];

		string path = MakeString ([array objectAtIndex: 0]);
				
		if (ExtractProjectAndScenePath(path, &m_DragProjectPath, NULL))
		{
			if (IsProjectFolder (m_DragProjectPath))
				m_DragResult = NSDragOperationLink;\
		}
	}
	return m_DragResult;
}

- (NSDragOperation)draggingUpdated:(id < NSDraggingInfo >)sender
{
	return m_DragResult;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender 
{
	if (m_DragResult == NSDragOperationLink)
	{
		[(ProjectWizard*)[self target] AddProjectPath:m_DragProjectPath];
		[self reloadData];
		[self selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection: NO];
		return YES;
	}
    return NO;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender 
{
    return YES;
}

@end

void RunProjectWizard (bool isLaunching, bool isNewProject)
{
	[[ProjectWizard alloc]initIsLaunching: YES];
}
