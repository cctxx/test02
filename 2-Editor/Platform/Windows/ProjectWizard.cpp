#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ProjectWizard.h"
#include "Editor/Src/ProjectWizardUtility.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include <shlobj.h>
#include <shlwapi.h>
#include "resource.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "PlatformDependent/Win/TabControl.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Utility/SplashScreen.h"
#include "Utility/BrowseForFolder.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Configuration/UnityConfigureVersion.h"
#include <WindowsX.h>

#include "Runtime/GfxDevice/GfxDeviceSetup.h"

using namespace std;

static HWND s_Dialog;
static bool s_LaunchingApp;
static bool s_InitiallyNewProject;
static TabControl s_TabControl;

HWND GetMainEditorWindow();
void EnableAllContainerWindows( bool enable ); // EditorWindow.cpp

static INT_PTR CALLBACK ProjectWizardDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );


static const wchar_t* kAssetsFolder = L"Assets";

static vector<string> s_Packages;
static vector<bool>   s_EnabledPackages;
static vector<string> s_RecentProjects;

static void DoOpenProject( const std::string& path )
{
	if (!s_LaunchingApp)
	{
		vector<string> commandline;
		commandline.push_back ("-projectpath");
		commandline.push_back (path);

		if(::g_ForcedGfxRenderer == kGfxRendererOpenGL)
		{
			commandline.push_back ("-force-opengl");
		}

		EndDialog( s_Dialog, IDOK );

		RelaunchWithArguments (commandline);
	}
	else
	{
		EndDialog( s_Dialog, IDOK );

		SetProjectPath(path, false);
	}
}

static void DoCreateNewProject(HWND hDlg)
{
	string unityPath;
	wchar_t path[kDefaultPathBufferSize];

	GetWindowTextW(s_TabControl.GetTabDlgItem(1,IDC_EDIT_PATH), path, kDefaultPathBufferSize);
	ConvertWindowsPathName(path, unityPath);
	unityPath = TrimSlash(unityPath);

	CreateDirectoryRecursive(unityPath);
	bool isPathOK = PathIsDirectoryEmptyW(path);
	if( !isPathOK )
	{
		MessageBoxW(hDlg, L"Specified path is not valid (should be a name of non existing or empty directory).", L"Create new project", MB_OK | MB_ICONERROR);
		return;
	}

	HWND packagesListBox = s_TabControl.GetTabDlgItem(1,IDC_LST_PACKAGES);
	HWND templatesComboBox = s_TabControl.GetTabDlgItem(1,IDC_CMB_DEFAULTS);
	for( int i = 0; i < s_EnabledPackages.size(); ++i ) {
		s_EnabledPackages[i] = ListView_GetCheckState(packagesListBox, i) ? true : false;
	}

	ProjectDefaultTemplate templateOption = (ProjectDefaultTemplate)ComboBox_GetCurSel(templatesComboBox);

	if (s_LaunchingApp)
	{
		PathAppendW(path, kAssetsFolder);
		CreateDirectoryW (path, NULL);
		SetProjectPath (unityPath, false);

		vector<string> packages;
		for (int i=0;i<s_Packages.size ();i++)
		{
			if (s_EnabledPackages[i])
				packages.push_back (s_Packages[i]);
		}

		SetIsCreatingProject (packages, templateOption);
		EndDialog( s_Dialog, IDOK );
	}
	else
	{
		vector<string> commandline;
		commandline.push_back ("-createProject");
		commandline.push_back (unityPath);
		for (int i=0;i<s_Packages.size ();i++)
		{
			if (s_EnabledPackages[i])
				commandline.push_back (s_Packages[i]);
		}
		commandline.push_back ("-projectTemplate");
		commandline.push_back (IntToString(templateOption));
		
		EndDialog( s_Dialog, IDOK );

		RelaunchWithArguments (commandline);
	}
}

static bool ValidateNewProjectFolder( const wchar_t* path, std::wstring& outInfoMessage )
{
	if( !path || path[0] == 0 ) {
		outInfoMessage = L"Select an empty folder to create project in.";
		return false;
	}

	// If path/file does not exist - it's ok, we'll create one
	if( !PathFileExistsW(path) ) {
		return true;
	}

	// Otherwise, path does exist. Not ok it's not a directory
	if( !PathIsDirectoryW(path) ) {
		outInfoMessage = L"Select an empty folder to create project in.";
		return false;
	}
	// Also not ok if not an empty directory
	if( !PathIsDirectoryEmptyW(path) ) {
		outInfoMessage = L"Select an empty folder to create project in.";
		return false;
	}

	// Otherwise ok
	return true;
}

static bool ValidateExistingProjectFolder( const wchar_t* path, std::wstring& outInfoMessage )
{
	if( !path || path[0] == 0 ) {
		outInfoMessage = L"Select project folder to open";
		return false;
	}
	if( !PathIsDirectoryW(path) ) {
		outInfoMessage = L"Select project folder to open";
		return false;
	}
	if( PathIsDirectoryEmptyW(path) ) {
		outInfoMessage = L"Selected folder is not a Unity project";
		return false;
	}

	std::string unityPath;
	ConvertWindowsPathName( path, unityPath );
	bool ok = IsProjectFolder(unityPath);
	if( !ok )
		outInfoMessage = L"Selected folder is not a Unity project";
	return ok;
}

static void DoBrowseForNewProject( HWND hDlg )
{
	wchar_t folder[kDefaultPathBufferSize];
	GetWindowTextW(s_TabControl.GetTabDlgItem(1,IDC_EDIT_PATH), folder, kDefaultPathBufferSize);

	std::string path = BrowseForFolderDialog( hDlg, L"Choose location for new project", folder, NULL, false, ValidateNewProjectFolder );
	if( !path.empty() ) {
		ConvertUnityPathName( path.c_str(), folder, kDefaultPathBufferSize );
		SetWindowTextW( s_TabControl.GetTabDlgItem(1,IDC_EDIT_PATH), folder );
	}
}

static void DoBrowseForExistingProject( HWND hDlg )
{
	std::string path = BrowseForFolderDialog( hDlg, L"Open existing project", NULL, NULL, true, ValidateExistingProjectFolder );
	if( !path.empty() )
		DoOpenProject(path);
}

static int GetTextWidthForWindow(HWND wnd, const wchar_t *str)
{
	HDC hdc;
	RECT rc;

	HFONT font = (HFONT)SendMessage(wnd, WM_GETFONT, 0, 0);
	hdc = GetDC(wnd);
	SelectObject(hdc, font);
	DrawTextW(hdc, str, -1, &rc, DT_SINGLELINE + DT_CALCRECT);
	ReleaseDC(wnd, hdc);

	return rc.right - rc.left;
}

static void EnableNewProjectOKButton( HWND hdlg )
{
	wchar_t path[kDefaultPathBufferSize];
	GetWindowTextW(s_TabControl.GetTabDlgItem(1,IDC_EDIT_PATH), path, kDefaultPathBufferSize);
	std::wstring info;
	bool ok = ValidateNewProjectFolder( path, info );
	EnableWindow(s_TabControl.GetTabDlgItem(1,IDOK), ok);
}

static bool HandleNewProjectCommands(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int res;

	switch( LOWORD(wParam) )
	{
	case IDOK:
		DoCreateNewProject(hDlg);
		return TRUE;
	case IDC_BROWSE:
		DoBrowseForNewProject( hDlg );
		return TRUE;
	case IDC_LST_PACKAGES:
		return FALSE;
	case IDC_EDIT_PATH:
		if (HIWORD(wParam) == EN_CHANGE)
			EnableNewProjectOKButton(hDlg);
		return TRUE;
	}

	return FALSE;
}

static bool HandleOpenProjectCommands(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	int selIndex;
	int res;

	switch( LOWORD(wParam) )
	{
	case IDOK:
		if (s_RecentProjects.size() == 0)
		{
			DoBrowseForExistingProject( hDlg );
		}
		else
		{
			selIndex = SendMessage(s_TabControl.GetTabDlgItem(0,IDC_LST_PROJECTS), LB_GETCURSEL, 0, 0);
			DoOpenProject( s_RecentProjects[selIndex] );
		}
		return TRUE;
	case IDC_BROWSE:
		DoBrowseForExistingProject( hDlg );
		return TRUE;
	case IDC_LST_PROJECTS:
		switch (HIWORD(wParam))
		{
		case LBN_SELCHANGE:
			selIndex = SendMessage(s_TabControl.GetTabDlgItem(0,IDC_LST_PROJECTS), LB_GETCURSEL, 0, 0);
			EnableWindow(s_TabControl.GetTabDlgItem(0, IDOK), selIndex >= 0);
			return TRUE;
		case LBN_DBLCLK:
			selIndex = SendMessage(s_TabControl.GetTabDlgItem(0,IDC_LST_PROJECTS), LB_GETCURSEL, 0, 0);
			if (selIndex >= 0)
				DoOpenProject( s_RecentProjects[selIndex] );
			return TRUE;
		}
		return FALSE;
	}

	return FALSE;
}

static INT_PTR CALLBACK ProjectWizardDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg ) 
	{
	case WM_INITDIALOG:
		{
			s_Dialog = hDlg;
			if( s_LaunchingApp )
				SetWindowLong( hDlg, GWL_EXSTYLE, GetWindowLong(hDlg,GWL_EXSTYLE) | WS_EX_APPWINDOW );

			// initialize tab control
			static char* kTabNames[] = { "Open Project", "Create New Project", 0 };
			static const char* kDlgNames[] = { MAKEINTRESOURCE(IDD_TAB_OPENPROJECT), MAKEINTRESOURCE(IDD_TAB_NEWPROJECT), 0 };
			CreateTabControl( s_TabControl, GetDlgItem(hDlg, IDC_TAB_PROJECT ),
				kTabNames, kDlgNames, s_InitiallyNewProject ? 1 : 0, &ProjectWizardDlgProc );

			std::string dlgTitle = Format("Unity - Project Wizard (%s)", UNITY_VERSION);
			SetWindowTextA (hDlg, dlgTitle.c_str());
			
			winutils::CenterWindowOnParent(hDlg);

			// populate recent projects list
			HWND recentListBox = s_TabControl.GetTabDlgItem(0,IDC_LST_PROJECTS);
			PopulateRecentProjectsList(s_RecentProjects);
			int maxTextWidth = 0;
			for( size_t i = 0; i < s_RecentProjects.size(); ++i )
			{
				const std::string& str = s_RecentProjects[i];
				wchar_t path[kDefaultPathBufferSize];
				ConvertUnityPathName(str.c_str(), path, kDefaultPathBufferSize);
				SendMessageW(recentListBox, LB_ADDSTRING, 0, (LPARAM)path);
				int currStrWidth = GetTextWidthForWindow(recentListBox, path);
				if (currStrWidth > maxTextWidth)
					maxTextWidth = currStrWidth;
			}
			SendMessage(recentListBox, LB_SETHORIZONTALEXTENT, maxTextWidth + 5, 0); // God Bless WinAPI
			if( !s_RecentProjects.empty() ) {
				ListBox_SetCurSel( s_TabControl.GetTabDlgItem(0,IDC_LST_PROJECTS), 0 );
				EnableWindow(s_TabControl.GetTabDlgItem(0,IDOK), TRUE);
			}

			// initial project path value
			std::string projectPath = ChooseProjectDirectory();
			wchar_t wideProjectPath[kDefaultPathBufferSize];
			ConvertUnityPathName( projectPath.c_str(), wideProjectPath, kDefaultPathBufferSize );
			SetWindowTextW( s_TabControl.GetTabDlgItem(1,IDC_EDIT_PATH), wideProjectPath );

			// populate packages list
			HWND packagesListBox = s_TabControl.GetTabDlgItem(1,IDC_LST_PACKAGES);
			PopulatePackagesList(&s_Packages, &s_EnabledPackages);
			ListView_SetExtendedListViewStyle(packagesListBox, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

			LVCOLUMNA lvCol;
			ZeroMemory(&lvCol, sizeof(lvCol));
			LVITEMA lvItem;
			ZeroMemory(&lvItem, sizeof(lvItem));

			RECT listRect;
			GetWindowRect(packagesListBox, &listRect);
			lvCol.mask = LVCF_TEXT | LVCF_WIDTH;
			lvCol.cx = listRect.right - listRect.left - 5;

			SendMessage(packagesListBox, LVM_INSERTCOLUMN, 0, (LPARAM)&lvCol);

			lvItem.mask = LVIF_TEXT;
			lvItem.cchTextMax = kDefaultPathBufferSize;

			for (size_t i = 0; i < s_Packages.size(); ++i)
			{
				std::string name = GetLastPathNameComponent(s_Packages[i]);
				lvItem.iItem = i;
				lvItem.pszText = (LPSTR)name.c_str();
				ListView_InsertItem(packagesListBox, &lvItem);
				ListView_SetCheckState(packagesListBox, i, s_EnabledPackages[i]);
			}

			// populate project template list
			HWND templateComboBox = s_TabControl.GetTabDlgItem(1,IDC_CMB_DEFAULTS);
			for (int i = 0; i < kProjectTemplateCount; ++i)
				ComboBox_AddString(templateComboBox, kProjectTemplateNames[i]);
			ComboBox_SetCurSel(templateComboBox, 0);

			return TRUE;
		}

	case WM_NOTIFY:
		if( wParam == IDC_TAB_PROJECT ) {
			NMHDR* notify = (NMHDR*)lParam;
			BOOL res = s_TabControl.Notify( notify );
			if( res ) {
				int curTab = s_TabControl.GetCurrentTab();
				if( curTab == 1 )
					EnableNewProjectOKButton(hDlg);
			}
			return res;
		}
		break;

	case WM_COMMAND:
		switch( LOWORD(wParam) ) {
		case IDCANCEL:
			EndDialog( s_Dialog, IDCANCEL );
			if (s_LaunchingApp)
				ExitProcess(0);//FIXME: this terminates without cleanup, but otherwise editor will try to save project (witch is not set in this case)
			//SendMessage(GetMainEditorWindow(), WM_CLOSE, NULL, NULL);
			return TRUE;
		default:
			if( s_TabControl.GetCurrentTab() == 0 )
				return HandleOpenProjectCommands(hDlg, wParam, lParam);
			else
				return HandleNewProjectCommands(hDlg, wParam, lParam);
		}
	}

	return FALSE;
}

void RunProjectWizard (bool isLaunching, bool isNewProject)
{
	HideSplashScreen();

	s_LaunchingApp = isLaunching;
	s_InitiallyNewProject = isNewProject;

	s_Packages.clear();
	s_EnabledPackages.clear();
	s_RecentProjects.clear();

	EnableAllContainerWindows(false);
	int res = DialogBoxA( winutils::GetInstanceHandle(), MAKEINTRESOURCEA( IDD_PROJECTWIZARD ), GetMainEditorWindow(), (DLGPROC)ProjectWizardDlgProc );
	EnableAllContainerWindows(true);

	if( res == -1 ) 
		AssertString( "Error displaying dialog: " + winutils::ErrorCodeToMsg(GetLastError()) );
}
