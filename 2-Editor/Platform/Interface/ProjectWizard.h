/* QuickProjectSetup */

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

#ifdef MAC_OS_X_VERSION_10_6
@interface ProjectWizard : NSObject <NSOpenSavePanelDelegate>
#else
@interface ProjectWizard : NSObject
#endif
{
	std::vector<std::string>* m_Packages;
	std::vector<bool>*        m_EnabledPackages;
	std::vector<std::string>* m_RecentProjects;
	
	IBOutlet id    m_NewProjectLocation;
	IBOutlet id    m_NewProjectButton;
	IBOutlet id    m_PackageTable;
	IBOutlet id    m_OpenProjectTable;
	IBOutlet id    m_OpenButton;
	IBOutlet id    m_TabView;
	IBOutlet id    m_2D3DPopopButton;

	bool           m_IsLaunching;
	bool           m_Complete;
	bool           m_CloseWindow;
}

- (id)initIsLaunching:(bool) launching;
- (IBAction)NewProjectLocationChanged:(id)sender;
- (IBAction)SetCreateProjectPath:(id)sender;
- (IBAction)Create:(id)sender;
- (IBAction)OpenOther:(id)sender;
- (IBAction)Open:(id)sender;
- (IBAction)OpenDoubleClick:(id)sender;
- (IBAction)CompleteOpenProject:(std::string&)path;
- (NSTabView*)tabView;
- (NSWindow*)window;
- (BOOL)IsInsideOpenProjectTab;
- (void)AddProjectPath:(std::string&)path;
@end
#endif

void RunProjectWizard (bool isLaunching, bool isNewProject);
