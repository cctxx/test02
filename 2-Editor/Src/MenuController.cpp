#include "UnityPrefix.h"
#include "MenuController.h"
#include "string.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Src/LicenseInfo.h"

using namespace std;

std::set<MenuInterface*>* gAllInterfaces = NULL;
Menu* gMainMenu = NULL;
ContextMenuPopupInfo* gDelayedContextMenu = NULL;
static MenuController::DidExecuteMenuItemCallback* gExecuteMenuItemCallback = NULL;

void RecursiveDeleteSubMenu (MenuItem& item);

void RecursiveDeleteSubMenu (MenuItem& item)
{
	delete item.m_Submenu;
	item.m_Submenu = NULL;
}

std::string LocalizeString (const std::string& internalName)
{
	return internalName;
}


/// Struct for a single menu item.
MenuItem::MenuItem (int pos, string sNameBuffer, string cmd,  MenuInterface *target, Menu *submenu)
{
	m_Position = pos;
	m_Name = sNameBuffer;
	m_Command = cmd;
	m_Target = target;
	m_Submenu = submenu;
	contextUserData = 0;
	checked = false;
	enabled = true;
	m_Parent = NULL;
}

MenuItem::~MenuItem ()
{
	RecursiveDeleteSubMenu(*this);
}

/// create a standard menu item.
MenuItem *MenuItem::CreateItem (int pos, string name, string cmd,  MenuInterface *target) {
	return new MenuItem (pos, name, cmd, target, NULL);
}

MenuItem *MenuItem::CreateSubmenu (int pos, string name) {
	return new MenuItem (pos, name, "", NULL, new Menu);
}

MenuItem *MenuItem::CreateSeparator (int pos) {
	return new MenuItem (pos, "", "", NULL, NULL);
}

void MenuItem::CollectInterfaces (std::set<MenuInterface*>& interface)
{
	if (m_Target)
		interface.insert (m_Target);
		
	if (m_Submenu)
	{
		for (Menu::iterator i=m_Submenu->begin ();i != m_Submenu->end ();i++)
			i->CollectInterfaces (interface);
	}
}

void MenuItem::SetContextRecurse (std::vector<Object*> &c, int userData)
{
	context = c;
	contextUserData = userData;
	if (m_Submenu)
	{
		for (Menu::iterator i=m_Submenu->begin ();i != m_Submenu->end ();i++)
			i->SetContextRecurse (context, userData);
	}
}

static MenuItem* InsertItem (MenuItem * parentMenu, MenuItem *item)
{
	Menu* menu = NULL;
	if (parentMenu == NULL)
	{
		menu = gMainMenu;
	}
	else
	{
		Assert(parentMenu->m_Submenu != NULL);
		menu = parentMenu->m_Submenu;
	}
	Menu::iterator i = menu->begin();
	while (true)
	{
		if (i == menu->end() || i->m_Position > item->m_Position)
		{
			MenuItem *ret = &*menu->insert (i, *item);
			item->m_Submenu = NULL;
			delete item;
			
			ret->m_Parent = parentMenu;
			
			return ret;
		}
		i++;
	}
}

static MenuItem *DoFindItem (const char *menuName, Menu *root)
{
	if (root == NULL)
		return NULL;

	string sNameBuffer;
	// Find the separator
	const char *sep = strchr (menuName, '/');
	// If we have a separator & it isn't the last char, we want a submenu
	if (sep != NULL && sep[1] != 0)
	{
		// Build name string from separator
		sNameBuffer.assign(menuName, sep);

		for (Menu::iterator i = root->begin(); i != root->end(); i++)
		{
			MenuItem& item = *i;
			// recurse further down.
			if (CompareMenuItemName(item.m_Name, sNameBuffer)) 	
			{
				MenuItem* res = DoFindItem (sep+1, item.m_Submenu);
				if (res)
					return res;
			}
		}
		// If we get here, we couldn't find it.
		return NULL;
	}
	else
	{
		// No slash at end, or just ends in a slash; we want this item.
		if (sep)
		{ // Ends in a slash, so we won't find it....
			// This is a common case, so we handle it nicely
			sNameBuffer.assign (menuName, sep);
		}
		else
			sNameBuffer.assign (menuName);

		for (Menu::iterator i = root->begin(); i != root->end(); i++)
		{
			MenuItem& item = *i;
			if (CompareMenuItemName(item.m_Name, sNameBuffer)) 
				return &item;
		}
			
		// Damn - didn't find it
		return NULL;
	}
}


static void ExtractSubmenusCommandsRecurse(Menu& menu, vector<std::string>& outSubmenus)
{
	for (Menu::iterator i=menu.begin();i != menu.end();++i)
	{
		if (i->m_Target)
			outSubmenus.push_back(i->m_Command);
		
		if (i->m_Submenu != NULL)
			ExtractSubmenusCommandsRecurse(*i->m_Submenu, outSubmenus);
	}
}

vector<std::string> MenuController::ExtractSubmenusCommands(const std::string& menu)
{
	// Make sure all menu item target are up to date
	UpdateAllMenus();
	
	vector<string> menus;
	
	MenuItem* item = MenuController::FindItem (menu);
	if (item != NULL && item->m_Submenu != NULL)
		ExtractSubmenusCommandsRecurse (*item->m_Submenu, menus);
	
	return menus;
}



static void ExtractSubmenusRecurse(Menu& menu, vector<std::string>& outSubmenus)
{
	for (Menu::iterator i=menu.begin();i != menu.end();++i)
	{
		if (i->m_Target)
			outSubmenus.push_back(MenuController::ExtractMenuItemPath(*i));
		
		if (i->m_Submenu != NULL)
			ExtractSubmenusRecurse(*i->m_Submenu, outSubmenus);
	}
}

vector<std::string> MenuController::ExtractSubmenus(const std::string& menu)
{
	// Make sure all menu item target are up to date
	UpdateAllMenus();
	
	vector<string> menus;
	
	MenuItem* item = MenuController::FindItem (menu);
	if (item != NULL && item->m_Submenu != NULL)
		ExtractSubmenusRecurse (*item->m_Submenu, menus);
	
	return menus;
}

MenuItem *MenuController::FindItem (const string& menuName)
{
	MenuItem *it = DoFindItem (menuName.c_str(), gMainMenu);
//	if (!it)
//		AssertString ("Unable to find menu item for'" + menuName +"'");
	return it;
}

bool MenuController::ExecuteMainMenuItem (const string& menuName)
{
	// Make sure all menu item target are up to date
	UpdateAllMenus();
	
	MenuItem *item = MenuController::FindItem (menuName);
	if (!item)
	{
		ErrorString("ExecuteMenuItem failed because there is no menu named '" + menuName + "'");
		return false;
	}
	
	return ExecuteMenuItem(*item);
}

/// Like ExecuteMainMenuItem, but executes the menu command on a specific set of GameObjects, if supported by the menu command.
/// This version is called from API only and does not perform validation or callback.
bool MenuController::ExecuteMainMenuItemOnGameObjects (const string& menuName, std::vector<GameObject*> &selection)
{
	// Make sure all menu item target are up to date
	UpdateAllMenus();
	
	MenuItem *item = MenuController::FindItem (menuName);
	if (!item)
	{
		ErrorString("ExecuteMenuItem failed because there is no menu named '" + menuName + "'");
		return false;
	}
	
	return ExecuteMenuItemOnGameObjects(*item, selection);
}

bool MenuController::ExecuteMenuItem (MenuItem& item)
{
	if (item.m_Target == NULL)
	{
		ErrorString(Format("ExecuteMenuItem target for %s does not exist", item.m_Name.c_str()));		
		return false;
	}
	
	if (!item.m_Target->Validate (item))
		return false;
	
	if (gExecuteMenuItemCallback)
		gExecuteMenuItemCallback(item);

	item.m_Target->Execute (item);
		
	return true;
}

/// Like ExecuteMenuItem, but executes the menu command on a specific set of GameObjects, if supported by the menu command.
/// This version is called from API only and does not perform validation or callback.
bool MenuController::ExecuteMenuItemOnGameObjects (MenuItem& item, std::vector<GameObject*> &selection)
{
	if (item.m_Target == NULL)
	{
		ErrorString(Format("ExecuteMenuItem target for %s does not exist", item.m_Name.c_str()));		
		return false;
	}
	
	item.m_Target->ExecuteOnGameObjects (item, selection);
	
	return true;
}

void MenuController::RegisterDidExecuteMenuItem(DidExecuteMenuItemCallback* callback)
{
	Assert(gExecuteMenuItemCallback == NULL);
	
	gExecuteMenuItemCallback = callback;
}
	

/// The meat of the MenuController.
/// this recurses down the menus, adding new submenus as we go along.
static bool RemoveItemFromMenu (Menu *menu, const char *menuName)
{
	// find separator.
	const char *sep = strchr (menuName, '/');
	// If we have one, we want a submenu
	if (sep != NULL)
	{
		string sNameBuffer (menuName, sep);

		// Find the menu we want to remove
		Menu::iterator i = menu->begin();
		while (i != menu->end())
		{
			if (i->m_Name == sNameBuffer)
			{
				break;
			}
			i++;
		}
		MenuItem *item;
		if (i == menu->end())
		{
			return false;
		}
		else
		{
			item = &*i;
			if (!item->m_Submenu)
				return false;
		}
		
		// recurse further down.
		return RemoveItemFromMenu (item->m_Submenu, sep + 1);
	}
	else
	{
		// See if we already have it. If so, we exit.
		Menu::iterator i = menu->begin();
		while (i != menu->end())
		{
			if (i->m_Name == menuName)
				break;
			i++;	
		}
		if (i == menu->end ())
			return false;
		
		menu->erase (i);
		return true;
	}
}


/// The meat of the MenuController.
/// this recurses down the menus, adding new submenus as we go along.
static void AddItemToMenu (MenuItem *parentMenu, const char *menuName, const string &command, MenuInterface *target, int position)
{
	Menu* menu = NULL;
	gAllInterfaces->insert(target);
	if (parentMenu == NULL)
	{
		menu = gMainMenu;
	}
	else
	{
		Assert(parentMenu->m_Submenu != NULL);
		menu = parentMenu->m_Submenu;
	}
	
	// find separator.
	const char *sep = strchr (menuName, '/');
	// If we have one, we want a submenu
	if (sep != NULL)
	{
		string sNameBuffer (menuName, sep);
		// If we don't have this item, create & insert it.
		Menu::iterator i = menu->begin();
		while (i != menu->end())
		{
			if (i->m_Name == sNameBuffer && i->m_Submenu != NULL)
			{
				break;
			}
			i++;
		}
		
		MenuItem *item;
		if (i == menu->end())
		{
			item = InsertItem (parentMenu, MenuItem::CreateSubmenu (position, sNameBuffer));
		}
		else
		{
			item = &*i;
			AssertIf (!item->m_Submenu);
		}
		
		// recurse further down.
		AddItemToMenu (item, sep + 1, command, target, position);
	}
	else
	{
		// If we have no separator, we want to insert a menu item.
		if (strlen (menuName) != 0)
		{	
			// See if we already have it. If so, we exit.
			Menu::iterator i = menu->begin();
			while (i != menu->end()) {
				if (i->m_Name == menuName)
					return;
				i++;	
			}
			// Create the new item & insert into menu
			InsertItem (parentMenu, MenuItem::CreateItem (position, menuName, command, target));
		}
		else
		{
			// If it IS a separator, create & insert.
			InsertItem (parentMenu, MenuItem::CreateSeparator (position));
		}
	}
}

string MenuController::ExtractMenuItemPath (MenuItem& srcItem)
{
	string path;
	MenuItem* cur = &srcItem;
	while (cur)
	{
		string name = GetMenuNameWithoutHotkey(cur->m_Name);
		if (!path.empty())
			path = name + "/" + path;
		else
			path = name;
		
		cur = cur->m_Parent;
	}
	return path;
}

void MenuController::DeepCopyReplaceSame (MenuItem& srcItem, MenuItem& dstItem)
{
	Menu& src = *srcItem.m_Submenu;
	Menu& dst = *dstItem.m_Submenu;
	
	for (Menu::iterator i=src.begin();i!=src.end();i++)
	{
		for (Menu::iterator j=dst.begin();j!=dst.end();)	
		{
			if (i->m_Name == j->m_Name)
				j = dst.erase(j);
			else
				++j;
		}
		
		dst.push_back (*i);
		dst.back().m_Submenu = NULL;
		dst.back().m_Parent = &dstItem;
		
		if (i->m_Submenu != NULL && !i->m_Submenu->empty())
		{
			dst.back().m_Submenu = new Menu();
			DeepCopyReplaceSame (dst.back(), *i);
		}
	}
}

static void InitMenus ()
{
	if (!gMainMenu)
	{	
		gMainMenu = UNITY_NEW(Menu, kMemEditorGui);
		gAllInterfaces = UNITY_NEW(std::set<MenuInterface*>, kMemEditorGui);
		// When building our main menu we assume that 'CONTEXT' is always our
		// first menu element we therefore give it lowest posible priority so 
		// users can't insert menus with lower priority (bug fix case 377134) 
		int priority = std::numeric_limits<int>::min();
		
		// Create default items.
		gMainMenu->push_back (*MenuItem::CreateSubmenu (priority, "CONTEXT"));	// Special-cased context menu (must always be first)
		gMainMenu->push_back (*MenuItem::CreateSubmenu (priority, "Edit"));
		gMainMenu->push_back (*MenuItem::CreateSubmenu (priority, "Assets"));
		gMainMenu->push_back (*MenuItem::CreateSubmenu (priority, "GameObject"));
		gMainMenu->push_back (*MenuItem::CreateSubmenu (priority, "Component"));
	}
}

void MenuController::AddMenuItem (const string &menuName, const string &command, MenuInterface *target, int position) {
	InitMenus ();	
	
	if (menuName.find('/') == string::npos)
	{
		ErrorString("Ignoring menu item " + menuName + " because it is in no submenu!");
		return;
	}
//	if (BeginsWith(menuName, "Window"))
//	{
//		ErrorString("Ignoring menu item " + menuName + ". You can not add menu items to the Window Menu!");
//		return;
//	}
	
	/// Enter recursion into the root menu.
	AddItemToMenu (NULL, menuName.c_str(), command, target, position);
}

bool MenuController::RemoveMenuItem (const string &menuName) {
	InitMenus ();	
	/// Enter recursion into the root menu.
	return RemoveItemFromMenu (gMainMenu, menuName.c_str());
}


void MenuController::AddContextItem (const string &menuName, const string &command, MenuInterface *target, int position) {
	InitMenus ();
	/// Enter recursion into the root menu.
	AddItemToMenu (NULL, (string ("CONTEXT/") +menuName).c_str(), command, target, position);
}

static void RemoveEmptyParentMenus (Menu& menu)
{
	Menu::iterator next;
	for (Menu::iterator i=menu.begin();i!=menu.end();i=next)
	{
		next = i;
		next++;
		if (i->m_Submenu == NULL || i->m_Submenu->empty())
		{
			if (i->m_Target == NULL && i->m_Command.empty() && !i->m_Name.empty())
				menu.erase (i);
		}
		else
		{
			RemoveEmptyParentMenus(*i->m_Submenu);
		}
	}
}

void MenuController::RemoveEmptyParentMenus ()
{
	InitMenus ();
	
	Menu& mainMenu = *gMainMenu;
	for (Menu::iterator i=mainMenu.begin();i!=mainMenu.end();i++)
	{
		if (i->m_Submenu != NULL)
			::RemoveEmptyParentMenus(*i->m_Submenu);
	}
}

/** Add a separator.
 *   @param menuName a slash-separated menu position (e.g. 'Scene')
 *   @param position the position of the menu item.
 */
void MenuController::AddSeparator (const string &menuName, int position)
{
	InitMenus ();
	AddItemToMenu (NULL, menuName.c_str(), "", NULL, position);
}


void MenuController::UpdateContextMenu (std::vector<Object*> &context, int userData)
{
	InitMenus ();
	
	set<MenuInterface*> interfaces;
	for (Menu::iterator i=gMainMenu->begin ();i != gMainMenu->end ();i++)
		i->CollectInterfaces (interfaces);
	
	for (set<MenuInterface*>::iterator i=interfaces.begin();i != interfaces.end();i++)
		(**i).UpdateContextMenu (context, userData);

	MenuItem* contextMenu = FindItem("CONTEXT");
	if (contextMenu)
		contextMenu->SetContextRecurse (context, userData);
}


void MenuController::UpdateAllMenus ()
{
	// This might happen during startup.
	if (gMainMenu == NULL)
		return;
		
	set<MenuInterface*> interfaces;
	for (Menu::iterator i=gMainMenu->begin ();i != gMainMenu->end ();i++)
		i->CollectInterfaces (interfaces);
	
	for (set<MenuInterface*>::iterator i=interfaces.begin ();i != interfaces.end ();i++)	
		(**i).Update ();
}

bool MenuController::GetChecked (const string &menuName) {
	MenuItem* item = MenuController::FindItem(menuName);
	if (item == NULL)
	{
		ErrorString ("Menu " + menuName + " can't be checked because doesn't exist");
		return false;
	}
	
	return item->checked;
}

bool MenuController::GetEnabled (const string &menuName) {
	MenuItem* item = MenuController::FindItem(menuName);
	if (item == NULL)
	{
		ErrorString ("Menu " + menuName + " can't be enabled because doesn't exist");
		return false;
	}
	
	return item->enabled;
}

void MenuController::DisplayPopupMenu (const Rectf &pos, const string &menuName, std::vector<Object*> &context, int userData)
{
	delete gDelayedContextMenu;
	gDelayedContextMenu = NULL;
	gDelayedContextMenu = new ContextMenuPopupInfo();
	gDelayedContextMenu->location = pos;
	gDelayedContextMenu->menuName = menuName;
	for (std::vector<Object*>::iterator i = context.begin(); i!=context.end(); i++)
		gDelayedContextMenu->contextInstanceIDs.push_back ((**i).GetInstanceID());
	gDelayedContextMenu->userData = userData;
}

void MenuController::CleanupClass()
{
	UNITY_DELETE(gMainMenu, kMemEditorGui);
	for(std::set<MenuInterface*>::iterator it = gAllInterfaces->begin(); it != gAllInterfaces->end(); ++it)
		delete (*it);
	UNITY_DELETE(gAllInterfaces, kMemEditorGui);
}



bool CompareMenuItemName(const std::string& menuItemName, const std::string& searchFor)
{
	std::string menuItemDisplayName = GetMenuNameWithoutHotkey(menuItemName);
	std::string searchForDisplayName = GetMenuNameWithoutHotkey(searchFor);
	
	if (menuItemDisplayName != searchForDisplayName)
		return false;
	
	std::string searchForHotkey = GetMenuItemHotkey(searchFor);
	if (searchForHotkey.empty())
		return true;
	
	// Check the complete name
	return GetMenuItemHotkey(menuItemName) == searchForHotkey;
}

string GetMenuNameWithoutHotkey (const string& key)
{
	int offset = FindHotkeyStartIndex(key.c_str());
	while (offset > 0 && key[offset - 1] == ' ')
		offset--;
	return string (key.begin(), key.begin() + offset);
}

string GetMenuItemHotkey (const string& key)
{
	int offset = FindHotkeyStartIndex(key.c_str());
	return string (key.begin() + offset, key.end());
}

int FindHotkeyStartIndex (const char *src)
{
	int len = 0;
	const char *last = src;
	// scan the string until we reach the first hotkey char
	while (*src && len < 127 && !((*src == '&' || *src == '%' || *src == '^' || *src == '#' || *src == '_') && *last == ' ')) {
		last = src;
		src++;
		len++;
	}
	return len;
}
