#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "resource.h"
#include "Editor/Platform/Interface/ProjectWizard.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/Utilities/Word.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Editor/Src/Utility/BuildPlayerHelpers.h"
#include "PlatformDependent/Win/WinUnicode.h"
#include <afxres.h>

const int kStartMenuItemIDs = 50000;
const int kMaxMenuTitleLength = 200;

HWND GetMainEditorWindow(); // WinEditorMain.cpp


static HMENU s_MainMenuHandle;
static HACCEL s_MainMenuAccelerators;

typedef std::vector<MenuItem*>	MenuIDToItem;
static MenuIDToItem	s_MenuIDToItem;

typedef std::vector<ACCEL>	ShortcutVector;
static ShortcutVector	s_MenuShortcuts;

HMENU GetMainMenuHandle()
{
	return s_MainMenuHandle;
}

HACCEL GetMainMenuAccelerators()
{
	return s_MainMenuAccelerators;
}
void ShutdownMainMenu()
{
	DestroyAcceleratorTable(s_MainMenuAccelerators);
	s_MainMenuAccelerators = NULL;
}

void UpdateMainMenu()
{
	MenuController::UpdateAllMenus();
}

bool ValidateMenuItem( int id )
{
	// TODO
	/*
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
	else if (menuItem == m_FrameSelectedItem)
		return ValidateCommandOnKeyWindow("FrameSelected");
	else if (menuItem == m_SelectAllItem)
		return ValidateCommandOnKeyWindow("SelectAll");
	else
		return true;
	*/
	return true;
}

void ValidateSubmenu( HMENU menu )
{
	MENUITEMINFOW info;
	info.cbSize = sizeof(info);
	info.fMask = MIIM_DATA | MIIM_SUBMENU | MIIM_ID;

	int count = GetMenuItemCount(menu);
	for( int i = 0; i < count; ++i )
	{
		if( !GetMenuItemInfoW( menu, i, TRUE, &info ) )
			continue;

		if( info.hSubMenu != NULL )
			continue;

		if( info.dwItemData == NULL || info.dwItemData == (ULONG_PTR)-1 )
			continue;

		MenuItem* item = reinterpret_cast<MenuItem*>(info.dwItemData);
		if( item->m_Target )
		{
			bool enabled = item->enabled && item->m_Target->Validate(*item);
			EnableMenuItem( menu, i, MF_BYPOSITION | (enabled ? MF_ENABLED : MF_GRAYED) );
		}
	}
}


bool ExecuteStandardMenuItem( int id )
{
	switch ( id )
	{
	case ID_FILE_NEW:
		GetApplication().FileMenuNewScene();
		break;
	case ID_FILE_OPEN:
		GetApplication().FileMenuOpen();
		break;
	case ID_FILE_SAVE:
		GetApplication().FileMenuSave(true);
		break;
	case ID_FILE_SAVE_AS:
		GetApplication().FileMenuSaveAs();
		break;
	case ID_FILE_NEWPROJECT:
		RunProjectWizard(false, true);
		break;
	case ID_FILE_OPENPROJECT:
		RunProjectWizard(false, false);
		break;
	case ID_FILE_SAVEPROJECT:
		AssetInterface::Get ().SaveAssets ();
		break;
	case ID_FILE_BUILDSETTINGS:
		ShowBuildPlayerWindow();
		break;
	case ID_FILE_BUILDRUN:
		BuildPlayerWithLastSettings();
		break;
	case IDC_FILE_CLOSE:
		SendMessage(GetMainEditorWindow(), WM_CLOSE, 0, 0);
		break;

	case ID_EDIT_UNDO:
		GetUndoManager().Undo();
		break;
	case ID_EDIT_REDO:
		GetUndoManager().Redo();
		break;
	case ID_EDIT_CUT:
		ExecuteCommandOnKeyWindow("Cut");
		break;
	case ID_EDIT_COPY:
		ExecuteCommandOnKeyWindow("Copy");
		break;
	case ID_EDIT_PASTE:
		ExecuteCommandOnKeyWindow("Paste");
		break;
	case ID_EDIT_FRAMESELECTED:
		if( !ExecuteCommandInMouseOverWindow("FrameSelected") ) {
			if (!ExecuteCommandOnKeyWindow("FrameSelected")) {
				FrameLastActiveSceneView(false);
			}
		}
		break;
	case ID_EDIT_FRAMESELECTEDWITHLOCK:
		if( !ExecuteCommandInMouseOverWindow("FrameSelectedWithLock") ) {
			if (!ExecuteCommandOnKeyWindow("FrameSelectedWithLock")) {
				FrameLastActiveSceneView(true);
			}
		}
		break;
	case ID_EDIT_SOFTDELETE:
		ExecuteCommandOnKeyWindow("SoftDelete");
		break;
	case ID_EDIT_DELETE:
		ExecuteCommandOnKeyWindow("Delete");
		break;
	case ID_EDIT_DUPLICATE:
		ExecuteCommandOnKeyWindow("Duplicate");
		break;
	case ID_EDIT_SELECT_ALL:
		ExecuteCommandOnKeyWindow("SelectAll");
		break;
	case ID_EDIT_FIND:
		ExecuteCommandOnKeyWindow("Find");
		break;
	case ID_EDIT_UNITYOPTIONS:
		ShowPreferencesDialog();
		break;

	case ID_WINDOW_NEXTWINDOW:
		CycleToContainerWindow(1);
		break;

	case ID_WINDOW_PREVIOUSWINDOW:
		CycleToContainerWindow(-1);
		break;

	case ID_HELP_ABOUT:
		ShowAboutDialog();
		break;
	case ID_HELP_MANAGELICENSE:
		GetApplication().EnterSerialNumber();
		break;

	default:
		return false;
	}

	return true;
}

bool ExecuteMenuItemWithID( int id )
{
	if ( ExecuteStandardMenuItem( id ) )
		return true;

	int index = id - kStartMenuItemIDs;
	int numIDs = s_MenuIDToItem.size();
	if( index < 0 || index >= numIDs )
		return false;

	MenuItem* item = s_MenuIDToItem[index];

	return MenuController::ExecuteMenuItem(*item);
}


static void CreateStandardShortcuts()
{
	static ACCEL kAcc[] = {
		// File
		{ FVIRTKEY | FCONTROL,			'N',		ID_FILE_NEW },
		{ FVIRTKEY | FCONTROL,			'O',		ID_FILE_OPEN },
		{ FVIRTKEY | FCONTROL,			'S',		ID_FILE_SAVE },
		{ FVIRTKEY | FCONTROL | FSHIFT,	'S',		ID_FILE_SAVE_AS },
		{ FVIRTKEY | FCONTROL | FSHIFT,	'B',		ID_FILE_BUILDSETTINGS },
		{ FVIRTKEY | FCONTROL,			'B',		ID_FILE_BUILDRUN },
		// Edit
		{ FVIRTKEY | FCONTROL,			'Z',		ID_EDIT_UNDO },
		{ FVIRTKEY | FCONTROL,			'Y',		ID_EDIT_REDO },
		{ FVIRTKEY | FCONTROL,			'C',		ID_EDIT_COPY },
		{ FVIRTKEY | FCONTROL,			VK_INSERT,	ID_EDIT_COPY },
		{ FVIRTKEY | FCONTROL,			'V',		ID_EDIT_PASTE },
		{ FVIRTKEY | FSHIFT,			VK_INSERT,	ID_EDIT_PASTE },
		{ FVIRTKEY | FCONTROL,			'X',		ID_EDIT_CUT },
		{ FVIRTKEY | FCONTROL,			'D',		ID_EDIT_DUPLICATE },
		{ FVIRTKEY | FCONTROL,			'F',		ID_EDIT_FIND },
		//{ FVIRTKEY,						VK_DELETE,	ID_EDIT_SOFTDELETE }, // don't add as shortcut - will be impossible to type this into textfields!
		{ FVIRTKEY | FSHIFT,			VK_DELETE,	ID_EDIT_DELETE },
		//{ FVIRTKEY,					'F',		ID_EDIT_FRAMESELECTED }, // don't add as shortcut - will be impossible to type this into textfields!
		//{ FVIRTKEY | FSHIFT,			'F',		ID_EDIT_FRAMESELECTEDWITHLOCK }, // don't add as shortcut - will be impossible to type this into textfields!
		{ FVIRTKEY | FCONTROL,			'A',		ID_EDIT_SELECT_ALL },
		// Window
		{ FVIRTKEY | FCONTROL,			VK_TAB,		ID_WINDOW_NEXTWINDOW },
		{ FVIRTKEY | FCONTROL | FSHIFT,	VK_TAB,		ID_WINDOW_PREVIOUSWINDOW },
	};
	size_t count = ARRAY_SIZE(kAcc);
	s_MenuShortcuts.insert( s_MenuShortcuts.end(), kAcc, kAcc + count );
}


static short GetVirtualKey (char c)
{
	HKL keybardLayoutHandle = GetKeyboardLayout (0);
	int virtualKey = VkKeyScanExW (ToLower(c), keybardLayoutHandle);
	if (virtualKey == -1)
		printf_console ("GetVirtualKey: Could not map char: %c (%d) to any virtual key\n", c, c);
	return virtualKey;
}


struct SpecialMenuKey
{
	const char* name;
	short vkey;
	const char* display;
};

static SpecialMenuKey s_SpecialKeys[] = {
	{"LEFT", VK_LEFT, "\xE2\x86\x90"}, // UTF-8 sequences for arrows here
	{"RIGHT", VK_RIGHT, "\xE2\x86\x92"},
	{"UP", VK_UP, "\xE2\x86\x91"},
	{"DOWN", VK_DOWN, "\xE2\x86\x93"},
	{"F1", VK_F1, "F1"},
	{"F2", VK_F2, "F2"},
	{"F3", VK_F3, "F3"},
	{"F4", VK_F4, "F4"},
	{"F5", VK_F5, "F5"},
	{"F6", VK_F6, "F6"},
	{"F7", VK_F7, "F7"},
	{"F8", VK_F8, "F8"},
	{"F9", VK_F9, "F9"},
	{"F10", VK_F10, "F10"},
	{"F11", VK_F11, "F11"},
	{"F12", VK_F12, "F12"},
	{"HOME", VK_HOME, "Home"},
	{"END", VK_END, "End"},
	{"PGUP", VK_PRIOR, "PgUp"},
	{"PGDN", VK_NEXT, "PgDown"},
};

static const char* MenuNameToWinMenuName( const std::string& name, int commandID, bool addShortcut )
{
	static std::string res;
	res.clear();
	res.reserve( name.size() );

	bool inShortcut = false;
	ACCEL shortcut;
	shortcut.cmd = commandID;
	shortcut.key = 0;
	shortcut.fVirt = 0;

	if( !addShortcut )
	{
		res = name;
	}
	else
	{
		bool wasSpace = false;
		for( size_t i = 0; i < name.size(); ++i )
		{
			char c = name[i];
			if( wasSpace && !inShortcut && ( c=='%' || c=='#' || c=='&' || c=='_' ) )
			{
				res.push_back( '\t' );
				inShortcut = true;
			}
			wasSpace = IsSpace(c);

			if( inShortcut )
			{
				if( c=='%' ) {
					res += "Ctrl+";
					shortcut.fVirt |= FCONTROL | FVIRTKEY;
				} else if( c=='#' ) {
					res += "Shift+";
					shortcut.fVirt |= FSHIFT | FVIRTKEY;
				} else if( c=='&' ) {
					res += "Alt+";
					shortcut.fVirt |= FALT | FVIRTKEY;
				} else if( c=='_' ) {
					// nothing!
				}
				else
				{
					// check if that's one of the special keys like "LEFT"
					bool specialKey = false;
					for (int j = 0; j < ARRAY_SIZE(s_SpecialKeys); ++j)
					{
						if (!strcmp(name.c_str()+i, s_SpecialKeys[j].name))
						{
							shortcut.key = s_SpecialKeys[j].vkey;
							res += s_SpecialKeys[j].display;
							specialKey = true;
							break;
						}
					}

					// otherwise it's a simple one letter key
					if (!specialKey)
					{
						shortcut.key = GetVirtualKey (c);
						res += ToUpper(c);
					}
					else
					{
						// got a special key shortcut, stop processing further
						break;
					}
				}
			}
			else
			{
				res.push_back( c );
			}
		}
	}

	if( addShortcut && shortcut.key && shortcut.cmd ) {
		s_MenuShortcuts.push_back( shortcut );
	}

	return res.c_str();
}


static bool EqualMenuNames( const char* a, const char* b )
{
	if( *a == '&' )
		++a;
	if( *b == '&' )
		++b;

	while( *a && (*a == *b) )
	{
		++a;
		++b;
		if( *a == '&' )
			++a;
		if( *b == '&' )
			++b;
	}

	return *a == 0 && *b == 0;
}


static HMENU GetSubMenuByName( HMENU menu, const char* name, int* outIndex, int* outID, ULONG_PTR* outItemData )
{
	char buffer[kMaxMenuTitleLength];
	MENUITEMINFOA item;
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STRING | MIIM_ID | MIIM_DATA;
	item.dwTypeData = buffer;
	item.cch = 199;

	int count = GetMenuItemCount(menu);
	for( int i = 0; i < count; ++i )
	{
		item.cch = 199;
		if( !GetMenuItemInfoA( menu, i, TRUE, &item ) )
		{
			AssertString( "Failed to get menu item info" );
			continue;
		}

		if( item.cch == 0 )
			continue;

		if( EqualMenuNames( name, buffer ) )
		{
			*outIndex = i;
			if( outID )
				*outID = item.wID;
			if( outItemData )
				*outItemData = item.dwItemData;
			return GetSubMenu( menu, i );
		}
	}

	*outIndex = -1;
	if( outID )
		*outID = 0;
	if( outItemData )
		*outItemData = NULL;
	return NULL;
}

static void AppendSeparator( HMENU menu )
{
	MENUITEMINFOA item;
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_DATA;
	item.fType = MFT_SEPARATOR;
	item.dwItemData = (ULONG_PTR)-1;
	if( !InsertMenuItemA( menu, GetMenuItemCount(menu), TRUE, &item ) )
	{
		AssertString("Failed to ad separator");
	}
}

static void DoInsertMenu(HMENU menu, MenuItem *menuItem, int index, UINT mask, HMENU submenu)
{
	MENUITEMINFOW item;
	memset( &item, 0, sizeof(item) );
	item.cbSize = sizeof(item);

	item.fMask = mask;
	item.wID = s_MenuIDToItem.size() + kStartMenuItemIDs;
	item.hSubMenu = submenu;

	wchar_t wideStr[kMaxMenuTitleLength];
	UTF8ToWide((char*)MenuNameToWinMenuName(menuItem->m_Name,item.wID,true), wideStr, kMaxMenuTitleLength);
	item.dwTypeData = wideStr;

	item.fState = (menuItem->enabled ? MFS_ENABLED : MFS_DISABLED) | (menuItem->checked ? MFS_CHECKED : 0);
	item.dwItemData = (ULONG_PTR)menuItem;
	if( InsertMenuItemW( menu, index, TRUE, &item ) )
	{
		s_MenuIDToItem.push_back( menuItem );
	}
	else
	{
		AssertString(Format ("Failed to insert item. Name: %s, Command: %s", menuItem->m_Name.c_str(), menuItem->m_Command.c_str()));
	}
}

static void AddSubMenuItems( HMENU menu, Menu* items )
{
	AssertIf( !items );

	int pos = -1;
	for( Menu::iterator i = items->begin(); i != items->end(); ++i )
	{
		// If we have a jump in position, it means we have insertion from 2 different places, so put a separator in
		if( pos != -1 && i->m_Position > pos + 10)
			AppendSeparator( menu );

		pos = i->m_Position;

		if( !i->m_Name.empty() )
		{
			if (i->m_Submenu != NULL)
			{
				HMENU submenu = CreatePopupMenu();
				DoInsertMenu(menu, &*i, GetMenuItemCount(menu), MIIM_STRING | MIIM_SUBMENU | MIIM_ID | MIIM_DATA, submenu);
				AddSubMenuItems( submenu, i->m_Submenu );
			}
			else
				DoInsertMenu(menu, &*i, GetMenuItemCount(menu), MIIM_STRING | MIIM_STATE | MIIM_ID | MIIM_DATA, 0);
		} else 
		{
			AppendSeparator( menu );
		}
	}
}


static void RebuildMainMenu()
{
	if( !s_MainMenuHandle ) {
		s_MainMenuHandle = ::LoadMenu( winutils::GetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINMENU) );
	}
	AssertIf( !s_MainMenuHandle );

	s_MenuIDToItem.clear();
	s_MenuShortcuts.clear();
	CreateStandardShortcuts();

	Menu::iterator i = gMainMenu->begin();
	i++;  // skip the CONTEXT menu

	// Add any submenus before Window submenu
	int idx = 2;
	GetSubMenuByName( s_MainMenuHandle, "Window", &idx, NULL, NULL );

	while( i != gMainMenu->end() )
	{
		MenuItem& menuItem = *i;

		int submenuIndex;
		int submenuID;
		ULONG_PTR submenuData;
		HMENU submenu = GetSubMenuByName( s_MainMenuHandle, menuItem.m_Name.c_str(), &submenuIndex, &submenuID, &submenuData );
		if( submenu != NULL )
		{
			// delete items in submenu that are created by us earlier
			MENUITEMINFOA item;
			item.cbSize = sizeof(item);
			item.fMask = MIIM_DATA;
			for( int j = 0; j < GetMenuItemCount(submenu); /**/ )
			{
				if( GetMenuItemInfoA( submenu, j, TRUE, &item ) )
				{
					if( item.dwItemData != NULL )
					{
						DeleteMenu( submenu, j, MF_BYPOSITION );
						continue;
					}
				}

				++j;
			}
		}

		if( submenu == NULL )
		{
			submenu = CreatePopupMenu();
			DoInsertMenu(s_MainMenuHandle, &menuItem, idx, MIIM_STRING | MIIM_SUBMENU | MIIM_ID | MIIM_DATA, submenu);
			idx++;
		}

		AddSubMenuItems( submenu, menuItem.m_Submenu );

		i++;	
	}

	// rebuild accelerator table
	if( s_MainMenuAccelerators )
		DestroyAcceleratorTable( s_MainMenuAccelerators );
	s_MainMenuAccelerators = CreateAcceleratorTable( &s_MenuShortcuts[0], s_MenuShortcuts.size() );

	HWND mainWindow = GetMainEditorWindow();
	if( mainWindow ) {
		SetMenu( mainWindow, s_MainMenuHandle );
		DrawMenuBar( mainWindow );
	}
}



static bool FindMenuItemRecurse( const ULONG_PTR item, HMENU startMenu, HMENU& outMenu, int& outIndex )
{
	MENUITEMINFOA info;
	info.cbSize = sizeof(info);
	info.fMask = MIIM_DATA | MIIM_SUBMENU;

	int count = GetMenuItemCount(startMenu);
	for( int i = 0; i < count; ++i )
	{
		if( GetMenuItemInfoA( startMenu, i, TRUE, &info ) )
		{
			if( info.hSubMenu == NULL )
			{
				// check if this item is the one
				if( info.dwItemData == item )
				{
					outMenu = startMenu;
					outIndex = i;
					return true;
				}
			}
			else
			{
				// recurse into submenu
				bool found = FindMenuItemRecurse( item, info.hSubMenu, outMenu, outIndex );
				if( found )
					return true;
			}
		}
	}

	// didn't find it here
	return false;
}

static bool FindMenuItemInMainMenu( const MenuItem* item, HMENU& outMenu, int& outIndex)
{
	return FindMenuItemRecurse( reinterpret_cast<ULONG_PTR>(item), s_MainMenuHandle, outMenu, outIndex );
}

static bool FindMenuItemByIDRecurse( DWORD id, HMENU startMenu, HMENU& outMenu, int& outIndex )
{
	MENUITEMINFOA info;
	info.cbSize = sizeof(info);
	info.fMask = MIIM_SUBMENU | MIIM_ID;

	int count = GetMenuItemCount(startMenu);
	for( int i = 0; i < count; ++i )
	{
		if( GetMenuItemInfoA( startMenu, i, TRUE, &info ) )
		{
			if( info.hSubMenu == NULL )
			{
				// check if this item is the one
				if( info.wID == id )
				{
					outMenu = startMenu;
					outIndex = i;
					return true;
				}
			}
			else
			{
				// recurse into submenu
				bool found = FindMenuItemByIDRecurse( id, info.hSubMenu, outMenu, outIndex );
				if( found )
					return true;
			}
		}
	}

	// didn't find it here
	return false;
}

bool FindMenuItemByID( DWORD id, HMENU& outMenu, int& outIndex )
{
	return FindMenuItemByIDRecurse( id, s_MainMenuHandle, outMenu, outIndex );
}


void MenuController::SetChecked (const string &menuName, bool checked)
{
	MenuItem* item = MenuController::FindItem(menuName);
	if( item == NULL )
	{
		ErrorString ("Menu " + menuName + " can't be checked because it doesn't exist");
		return;
	}

	item->checked = checked;

	HMENU menu;
	int itemIndex;
	if( FindMenuItemInMainMenu( item, menu, itemIndex ) )
	{
		CheckMenuItem( menu, itemIndex, MF_BYPOSITION | (checked ? MF_CHECKED : MF_UNCHECKED) );
	}
}

void MenuController::SetEnabled (const string &menuName, bool enabled) {
	MenuItem* item = MenuController::FindItem(menuName);
	if( item == NULL )
	{
		ErrorString ("Menu " + menuName + " can't be enabled because it doesn't exist");
		return;
	}

	item->enabled = enabled;

	HMENU menu;
	int itemIndex;
	if( FindMenuItemInMainMenu( item, menu, itemIndex ) )
	{
		EnableMenuItem( menu, itemIndex, MF_BYPOSITION | (enabled ? MF_ENABLED : MF_GRAYED) );
	}
}

void UpdateMenuItemNameByID( DWORD id, const std::string& name, bool enabled )
{
	HMENU menu;
	int index;
	if( FindMenuItemByID(id, menu, index) )
	{
		MENUITEMINFOA info;
		info.cbSize = sizeof(info);
		info.fMask = MIIM_STRING | MIIM_STATE;
		info.dwTypeData = (char*)MenuNameToWinMenuName( name, 0, false );
		info.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
		SetMenuItemInfoA( menu, index, TRUE, &info );
	}
}


void RebuildMenu()
{
	RebuildMainMenu();
}

bool HasDelayedContextMenu()
{
	return gDelayedContextMenu != NULL;
}


int GetCurrentStartingMenuItemID() { return s_MenuIDToItem.size() + kStartMenuItemIDs; }


void ShowDelayedContextMenu()
{
	if( !gDelayedContextMenu )
		return;

	UpdateMainMenu();

	ContextMenuPopupInfo* info = gDelayedContextMenu;
	std::vector<Object*> context;
	for (std::vector<int>::iterator i = info->contextInstanceIDs.begin(); i!=info->contextInstanceIDs.end(); i++)
		context.push_back(PPtr<Object>(*i));
	MenuController::UpdateContextMenu (context, info->userData);

	// Find the context menu
	MenuItem *item = MenuController::FindItem (info->menuName);
	if (item == NULL)
	{
		ErrorString(Format("Menu %s couldn't be found", info->menuName.c_str()));
		delete gDelayedContextMenu;
		gDelayedContextMenu = NULL;
		return;
	}

	// Build Windows popup menu
	HMENU submenu = CreatePopupMenu();
	size_t oldMenuIDSize = s_MenuIDToItem.size();
	AddSubMenuItems( submenu, item->m_Submenu );

	// Show the menu
	POINT pt;
	pt.x = info->location.x;
	pt.y = info->location.y + info->location.height;

	// TODO
	//if( fromWindow )
	//	ClientToScreen( fromWindow->GetWindowHandle(), &pt );

	delete gDelayedContextMenu;
	gDelayedContextMenu = NULL;

	DWORD result = TrackPopupMenuEx( submenu, /*TPM_LEFTALIGN |*/ TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, GetMainEditorWindow(), NULL );
	if( result != 0 )
		ExecuteMenuItemWithID( result );

	s_MenuIDToItem.resize( oldMenuIDSize );

	DestroyMenu( submenu );
}
