#ifndef MENUCONTROLLER_H
#define MENUCONTROLLER_H

#include <string>
#include "Runtime/Math/Rect.h"
#include "Runtime/BaseClasses/GameObject.h"
#include <list>

using std::string;
class Object;

struct MenuItem;

/** Abstract class that all objects wanting to execute menu commands must implement.
 *   If you want to insert stuff into the Apps main menu, you must implement this interface.
 */
struct MenuInterface
{
	virtual ~MenuInterface(){}
	/** return whether or not a menu item is enabled
	 *   @param menuItem the menu item that needs validation.
	 *   @return true if the menu item is valid, false if not.
	 */
	virtual bool Validate (const MenuItem &menuItem) = 0;
	/** Called when a menu item has been selected by the user.
	 */
	virtual void Execute (const MenuItem &menuItem) = 0;

	/** Override this version to support calling the menu function on a custom set of GameObjects.
	 *  Not called by actual menus; only from API. Validation is not called first for this version.
	 */
	virtual void ExecuteOnGameObjects (const MenuItem &menuItem, std::vector<GameObject*>& /*selection*/)
	{
		Execute(menuItem);
	}

	/** Called before the menu will be displayed.
	    You can add and remove menu items in here and then call RebuildMenu.
	 */
	virtual void Update () { };
	/** Called before a context menu will be displayed.
	*/
	virtual void UpdateContextMenu (std::vector<Object*>& /*context*/, int /*userData*/) { }
};

/** This is the top-level menu controller. It works a bit like Cocoa's responder chain.
 *   The basic principle is that commands have string names. when the user selects a menu,
 *   we ask a chain of MenuInterface objects to execute this command. If any return true, we stop the
 *   chain. This allows different panes to share the same menu items (e.g. Create Package being
 *   supported by BOTH asset & server panes).
 *
 *   Our responder chain is like this:
 *    * The selected Pane (e.g. 'Frame Selected')
 *    * The active SubEditor (e.g. 'Place On Surface')
 *
 *    If a plugin, an editor, a pane or whatever whats to create a main menu item, it does so by calling
 *    AddMenu with a command name. It then has to be able to execute that command.
 *
 *    Subimplementations would be:
 *	- an objC implementation that transparently converts the command names to obj-C selectors.
 *	- A C# implementation that transparently checks if the selected has the correct method names.
 *    In both cases, the command name is moot - this is only to have a base-level C++ interface as a
 *    common building block. We can then add a backend for Win32 that implements the same basics.
 */
class MenuController
	{
  public:
	/** Add a main menu item.
	 *   @param menuName a slash-separated menu item position (e.g. Scene/Place on Surface)
	 *   @param command the string used to actually call the command.
	 *   @param obj the object that handles the call. if NULL, we use the responder chain.
	 *   @param position the position of the menu item. Items with lower indices gets placed to the top of their relative menu. Items with the same index gets placed in insertion order.
	 */
	static void AddMenuItem (const string &menuName, const string &command, MenuInterface *obj = NULL, int position = 100);

	/** Add a context-dependant item.
	 *   @param menuName a slash-separated menu item position (e.g. Transform/Reset Scale)
	 *   @param command the string used to actually call the command.
	 *   @param obj the object that handles the call. if NULL, we use the responder chain.
	 *   @param position the position of the menu item. Items with lower indices gets placed to the top of their relative menu. Items with the same index gets placed in insertion order.
	 */
	static void AddContextItem (const string &menuName, const string &command, MenuInterface *obj = NULL, int position = 100);

	/** Add a separator.
	 *   @param menuName a slash-separated menu position (e.g. 'Scene')
	 *   @param position the position of the menu item.
	 */
	static void AddSeparator (const string &menuName, int position = 100);

	/// Set the checked status of a menu	item.
	/// @param menuName a slash-separated menu item position (e.g. Scene/Place on Surface)
	/// @param checked Should the menu item be checked or not.
	static void SetChecked (const string &menuName, bool checked);

	/// Set the checked status of a menu	item.
	/// @param menuName a slash-separated menu item position (e.g. Scene/Place on Surface)
	/// @return the checked status of the menu item...
	static bool GetChecked (const string &menuName);

	/// Set the enabled status of a menu	item.
	/// @param menuName a slash-separated menu item position (e.g. Scene/Place on Surface)
	/// @param enabled Should the menu item be enabled or not.
	static void SetEnabled (const string &menuName, bool enabled);

	/// Set the enabled status of a menu	item.
	/// @param menuName a slash-separated menu item position (e.g. Scene/Place on Surface)
	/// @return the enabled status of the menu item...
	static bool GetEnabled (const string &menuName);

	/// Get a submenu tree or a single item from the main menu.
	/// @param menuName a slash-separated menu item position (e.g. Assets/)
	/// @return a pointer to the correct item, or NULL if it can't be found.
	static MenuItem *FindItem (const string &menuName);

	/// Whenever the menu is about to be displayed the MenuInterfaces Update function is called.
	/// The update function may then remove and add menu items
	//static void SetUpdateMenuCallback (const string &menuName, MenuInterface* obj);

	/// Removes a menu item and all its children
	static bool RemoveMenuItem (const string &menuName);

	/// Removes all parent menus that are completely empty and do not have a menu command attached.
	static void RemoveEmptyParentMenus ();

	/// Execute a menu command
	/// This will first Update the menu controller, then validate the command and do nothing if the command isn't valid. While a bit
	/// awkward, this makes writing new commands much simpler as a menu command can assume it has
	/// a validated state. This function does not work with context menus.
	/// @param menuName a slash-separated menu item position (e.g. Assets/Import Settings...)
	/// @return validation status: true if the command has been executed, false if not.
	static bool ExecuteMainMenuItem (const string &menuName);

	/// Like ExecuteMainMenuItem, but executes the menu command on a specific set of GameObjects, if supported by the menu command.
	/// This version is called from API only and does not perform validation or callback.
	static bool ExecuteMainMenuItemOnGameObjects (const string &menuName, std::vector<GameObject*> &selection);

	static bool ExecuteMenuItem(MenuItem& menuItem);
	static bool ExecuteMenuItemOnGameObjects(MenuItem& menuItem, std::vector<GameObject*> &selection);

	/// Extracts the path "GameObject/Create Other/Particle System" from the menu item
	static std::string ExtractMenuItemPath(MenuItem& menuItem);

	/// Extracts a list of all submenus' commands
	static std::vector<std::string> ExtractSubmenusCommands(const std::string& menu);

	/// Extracts a list of all submenus that can be executed
	static std::vector<std::string> ExtractSubmenus(const std::string& menu);

	typedef void DidExecuteMenuItemCallback (MenuItem& item);
	static void RegisterDidExecuteMenuItem(DidExecuteMenuItemCallback* callback);

	/// Displays a context menu at position
	/// (Internally this will only queue the context menu and display it later from ShowDelayedContextMenu)
	static void DisplayPopupMenu (const Rectf &pos, const string &menuName, std::vector<Object*> &context, int userData);

	static void UpdateContextMenu (std::vector<Object*> &context, int userData);
	static void UpdateAllMenus ();

	// Copies all children of srcItem into dstItem, if an item has the same name already in the dstItem, then the old item will get replaced
	static void DeepCopyReplaceSame (MenuItem& srcItem, MenuItem& dstItem);

	static void CleanupClass();
};

typedef std::list<MenuItem> Menu;
/// Struct for a single menu item.
/// This is passed back to the caller for validation...
struct MenuItem
{
	int m_Position;				///< Integer position (this is what we sort by)
	string m_Name;			///< Name of the command (without prefix)
	string m_Command;		///< Command name. If empty, item is a separator
	MenuInterface *m_Target;	///< the target for the event.
	Menu* m_Submenu;		///< Pointer to a submenu.
	MenuItem* m_Parent;
	std::vector<Object*> context;
	int     contextUserData;
	bool	checked;
	bool	enabled;

	MenuItem (int pos, string name, string cmd,  MenuInterface *target, Menu *submenu);
	~MenuItem ();

	/// create a standard menu item.
	static MenuItem *CreateItem (int pos, string name, string cmd, MenuInterface *target);
	static MenuItem *CreateSubmenu (int pos, string name);
	static MenuItem *CreateSeparator (int pos);
	void SetContextRecurse (std::vector<Object*> &c, int userData);

	void CollectInterfaces (std::set<MenuInterface*>& interfaces);

	private:
	void operator = (MenuItem& item);
};

std::string GetMenuNameWithoutHotkey (const std::string& key);
std::string GetMenuItemHotkey (const std::string& key);
int FindHotkeyStartIndex (const char *src);
std::string LocalizeString (const std::string& internalName);
std::string GetLocalizedMenuNameWithoutHotkey (const std::string& key);

bool CompareMenuItemName(const std::string& menuItemName, const std::string& searchFor);

struct ContextMenuPopupInfo
{
	Rectf     location;
	std::string  menuName;
	std::vector<int> contextInstanceIDs;
	int          userData;
};

/// THIS IS A HACK AND SHOULD BE DONE AUTOMATIC!
/// I call this after adding / removing new menu items!
void RebuildMenu ();

// If gDelayedContextMenu is popup the context menu.
// We must delay this because OnGUI is not reentrant and can not be interrupted.
// This is called by EditorWindow after OnGUI
void ShowDelayedContextMenu();
bool HasDelayedContextMenu ();

extern Menu *gMainMenu;
extern ContextMenuPopupInfo *gDelayedContextMenu;

#ifdef __OBJC__
#include <AppKit/NSMenu.h>
#include <AppKit/NSNibDeclarations.h>
#ifdef MAC_OS_X_VERSION_10_6
@interface CocoaMainMenu : NSObject <NSMenuDelegate>
#else
@interface CocoaMainMenu : NSObject
#endif
{
@public
	IBOutlet NSMenu *m_Edit, *m_Help, *m_Windows;
}

// Get an NSMenu back from a main menu submenu.
// The NSMenu is autoreleased.
+(NSMenu *)buildMenuNamed:(NSString *)menuName;
// Add all items from menu to an NSMenu
+(void)addItemsFromMenu:(Menu*)menu toNSMenu:(NSMenu *)nsmenu;
@end
extern CocoaMainMenu* gMainCocoaMenu;

NSMenu* ExtractDelayedContextMenu ();


#endif

#endif
