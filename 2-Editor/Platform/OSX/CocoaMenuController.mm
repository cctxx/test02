// Cocoa-side implementation of MenuController...
#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/BaseClasses/Baseobject.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "ShortCut.h"
#import <Cocoa/Cocoa.h>

CocoaMainMenu *gMainCocoaMenu = NULL;

ShortCut *GBShortCut () {
	static ShortCut *s_ShortCut = NULL;
	if (!s_ShortCut)
		s_ShortCut = [[ShortCut alloc]init];
	return s_ShortCut;
} 

NSMenuItem *GBMakeMenuItem (NSString* title)
{
	// Handle hotkeys
	id menuItem;
	[GBShortCut() setKeyString: title];
	//	menuItem = [[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent: [GBShortCut() keyEquivalent]] autorelease];
	menuItem = [[[NSMenuItem alloc] initWithTitle:[GBShortCut() longStringNoKeys] action:nil keyEquivalent: [GBShortCut() keyEquivalent]] autorelease];
	[menuItem setKeyEquivalentModifierMask:[GBShortCut() modifierKeyMask]];
	return menuItem;
}


@implementation CocoaMainMenu
-(void)awakeFromNib {
	gMainCocoaMenu = self;
}

-(NSMenuItem*)separatorItem {
	NSMenuItem* item = [NSMenuItem separatorItem];
	[item setTarget: self];
	return item;
}

-(void)executeStandard:(id)sender {

	GetUndoManager().IncrementCurrentGroup ();

	MenuItem *item = (MenuItem*)[sender tag];
	if (item == NULL)
		return;
	MenuController::ExecuteMenuItem(*item);
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
	#pragma message ("BAD REMOVE THIS")
	static float lastTime = -5;
	if (CompareApproximately ([[NSApp currentEvent]timestamp], lastTime, .1F))
		return;

	lastTime = [[NSApp currentEvent]timestamp];
	MenuController::UpdateAllMenus ();
}


-(BOOL)validateMenuItem:(id)sender {
	MenuItem *item = (MenuItem*)[sender tag];
	if (item == NULL || item ->enabled == false)
		return NO;

//	NSLog (@"Validating %s",  item->m_Name.c_str());	
	// If we don't have a target, we need to check our responder chain.
	if (item->m_Target == NULL) {
		ErrorString("Validate Standard no implemented yet");
		return NO;
	} else
		// We HAVE a custom target for this object, so we just pass on the validate message.
		return item->m_Target->Validate (*item);
}

// Go over all items from in & add them to out, recursing for submenus.
-(void)addItems:(Menu*)_in toMenu:(NSMenu *)menu
{
	int pos = -1;
	for (Menu::iterator i = _in->begin(); i != _in->end(); i++)
	{
		// If we have a jump in position, it means we have insertion from 2 different places, so put a separator in.
		if (pos != -1 && i->m_Position > pos + 10)
			[menu addItem: [self separatorItem]];
		
		pos = i->m_Position;

		if (i->m_Name != "")
		{
			NSString *title = [NSString stringWithUTF8String: LocalizeString(i->m_Name).c_str()];
			if (i->m_Submenu != NULL)
			{
				NSMenu *submenu = [[NSMenu alloc] init];
				[submenu setAutoenablesItems: [menu autoenablesItems]];

				[self addItems: i->m_Submenu toMenu: submenu];

				NSMenuItem *item = GBMakeMenuItem (title); 	// Piggyback of Guiblocks to get shortcuts 'n all....
				[item setTarget: self];
				[item setState: i->checked];
				[item setEnabled: i->enabled];
				[item setSubmenu: submenu];
				[submenu release];
				[menu addItem:item];
			}
			else
			{
				NSMenuItem *item = GBMakeMenuItem (title);
				[item setState: i->checked];
				[item setEnabled: i->enabled];
				[item setTag: (int)&*i];
				[item setTarget: self];
				[item setAction: @selector (executeStandard:)];
				[menu addItem: item]; 	// Piggyback of Guiblocks to get shortcuts 'n all....
			}
		}
		else
		{
			[menu addItem: [self separatorItem]];
		}
	}
}

void ClearTagRecursive (NSMenuItem* item)
{
	// Attempt to work around random cocoa crash!
	// Happens completely randomly
	// http://www.cocoadev.com/index.pl?CrashWithNSGetMenuItemForCommandKeyEvent10
	[item setTag: 0];
	[item retain];
	[item autorelease];

	NSMenu* menu = [item submenu];
	int count = [menu numberOfItems];
	for (int i=0;i<count;i++)
	{
		NSMenuItem* child = [menu itemAtIndex: i];
		ClearTagRecursive(child);
	}
}

-(void)rebuildMainMenu {
	Menu::iterator i = gMainMenu->begin ();
	i++;  // skip the CONTEXT menu

	NSMenu* mainMenu = [gMainCocoaMenu->m_Windows supermenu];

	// Add any submenus before Window submenu
	int idx = [[m_Windows supermenu] indexOfItemWithSubmenu: m_Windows];
	while (i != gMainMenu->end())
	{
		NSString *title = [NSString stringWithUTF8String: LocalizeString(i->m_Name).c_str()];
		
		NSMenu* submenu = [[mainMenu itemWithTitle: title]submenu];
		
		if (submenu == NULL)
		{
			submenu = [[NSMenu alloc] initWithTitle: title];
			[submenu setDelegate: gMainCocoaMenu];
		
			NSMenuItem *item = GBMakeMenuItem (title); 	// Piggyback of Guiblocks to get shortcuts 'n all....
			[item setState: i->checked];
			[item setEnabled: i->enabled];
			
			[item setSubmenu: submenu];
			[submenu release];
			[mainMenu insertItem:item atIndex: idx];
			idx++;
		}
		else
		{
			int index = 0;
			while (index < [submenu numberOfItems])
			{
				NSMenuItem* item = [submenu itemAtIndex: index];
				if (item && [item target] == gMainCocoaMenu)
				{
					ClearTagRecursive([submenu itemAtIndex: index]);
					[submenu removeItemAtIndex: index];
				}
				else
				{
					index++;
				}
			}
		}
				
		[gMainCocoaMenu addItems: i->m_Submenu toMenu: submenu];

		i++;	
	}
}

+(void)addItemsFromMenu:(Menu*)menu toNSMenu:(NSMenu *)nsmenu {
	[gMainCocoaMenu addItems:menu toMenu:nsmenu];
}

+(NSMenu *)buildMenuNamed:(NSString *)menuName {
	MenuController::UpdateAllMenus ();
	
	MenuItem* root = MenuController::FindItem ([menuName UTF8String]);

	if (!root)
		return NULL;
	
	if (root->m_Submenu == NULL) {
		AssertString (string ("Unable to build menu from a single menu item - must be a level above:")  + [menuName UTF8String]);
		return NULL;
	}
	
	NSMenu *menu = [[[NSMenu alloc] init] autorelease];
	[gMainCocoaMenu addItems: root->m_Submenu toMenu: menu];
	return menu;
}

+(NSMenuItem*)getItemFromName:(NSString *)name
{
	NSArray *com = [name pathComponents];
	NSMenu *menu = [gMainCocoaMenu->m_Windows supermenu];
	int ind = 0;
	while (ind < [com count]) {
		NSMenuItem *item = [menu itemWithTitle: [com objectAtIndex: ind]];
		if (item == nil) {
			AssertString (string ("Error finding menu item ") + MakeString (name));
			return nil;
		}
		if (ind == [com count] - 1)
			return item;
		menu = [item submenu];
		if (menu == nil) {
			AssertString (string ("Error finding menu item ") + MakeString (name));
			return nil;
		}
	}
	return nil;
}

+(NSMenuItem*)getNSMenuItemFromMenuItem:(MenuItem *)item forMenu:(NSMenu *)menu
{
	for (int i=0;i<[menu numberOfItems];i++)
	{
		NSMenuItem* nsitem = [menu itemAtIndex: i];
		if ([nsitem tag] == (int)item)
			return nsitem;
		
		NSMenu* submenu = [nsitem submenu];
		nsitem = [self getNSMenuItemFromMenuItem: item forMenu: submenu];
		if (nsitem != NULL)
			return nsitem;
	}

	return NULL;
}

+(NSMenuItem*)getNSMenuItemFromMenuItem:(MenuItem *)item
{
	if (item == NULL)
		return NULL;
	
	NSMenu *menu = [gMainCocoaMenu->m_Windows supermenu];
	return [self getNSMenuItemFromMenuItem: item forMenu: menu];
}

@end

void MenuController::SetChecked (const string &menuName, bool checked)
{
	MenuItem* item = MenuController::FindItem(menuName);
	if (item == NULL)
	{
		ErrorString ("Menu " + menuName + " can't be checked because it doesn't exist");
		return;
	}
	
	item->checked = checked;
	NSMenuItem* nsitem = [CocoaMainMenu getNSMenuItemFromMenuItem: item];
	if (nsitem)
		[nsitem setState: checked];
}

void MenuController::SetEnabled (const string &menuName, bool enabled)
{
	MenuItem* item = MenuController::FindItem(menuName);
	if (item == NULL)
	{
		ErrorString ("Menu " + menuName + " can't be enabled because it doesn't exist");
		return;
	}
	
	item->enabled = enabled;
	NSMenuItem* nsitem = [CocoaMainMenu getNSMenuItemFromMenuItem: item];
	if (nsitem)
		[nsitem setEnabled: enabled];
}

void RebuildMenu ()
{
	[gMainCocoaMenu rebuildMainMenu];
}

bool HasDelayedContextMenu ()
{
	return gDelayedContextMenu != NULL;
}

void ShowDelayedContextMenu ()
{
	if (gDelayedContextMenu)
	{
		ContextMenuPopupInfo* info = gDelayedContextMenu;

		NSEvent* currentEvent = [NSApp currentEvent];
		float screenHeight = [[[NSScreen screens] objectAtIndex:0] frame].size.height;
		// location is top-left relative to desktop
		// Now we need to convert that to be screen-relative.
		float windowBottom = NSMinY ([[currentEvent window] frame]);
		NSRect location;
		location.origin.y = screenHeight - info->location.y - info->location.height;
		location.origin.x = info->location.x;
		location.size.width = info->location.width;
		location.size.height = info->location.height;
		
		//Check which screen mouse is on
		NSArray *screens = [NSScreen screens];
		NSScreen *cursorScreen = [screens objectAtIndex:0];
		NSPoint screenPos = [currentEvent locationInWindow];
		screenPos.x += [[currentEvent window] frame].origin.x;
		screenPos.y += [[currentEvent window] frame].origin.y;
		for(int i=1;i<[screens count];i++)
		{
			if(NSPointInRect(screenPos, [[screens objectAtIndex:i] frame]))
				cursorScreen = [screens objectAtIndex:i];
		}
		
		//Now make sure there is enough space for menu at bottom of screen. Otherwise offset position. Why must they make it so hard?
		const int kMinScreenOffset = 100;
		if(location.origin.y < [cursorScreen visibleFrame].origin.y + kMinScreenOffset)
			location.origin.y = [cursorScreen visibleFrame].origin.y + kMinScreenOffset; 
		
		//Now we have a save rect. Transform to window coordinates.
		location.origin.y -= windowBottom - 2;
		location.origin.x -= [[currentEvent window] frame].origin.x + 4;

		// Build cocoa menu and show
		NSMenu* theMenu = ExtractDelayedContextMenu();

		if(theMenu != NULL)
		{
			[theMenu insertItemWithTitle:@"Dummy" action:nil keyEquivalent:@"" atIndex: 0];
			
			NSPopUpButtonCell *popup = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES];
			[popup setUsesItemFromMenu:NO];
			[popup setMenu: theMenu];
			[popup setFont: [NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
			
			[popup performClickWithFrame:  location/*(NSRect)frame*/ inView:[[currentEvent window] contentView] /* (NSView *)controlView*/];
		}
// Old version, useful for function clicks.
/*		NSEvent* currentEvent = [NSApp currentEvent];
		
		int windowNb = [currentEvent windowNumber];
		NSPoint location = NSMakePoint(info->location.x, info->location.y);
		
		float screenHeight = [[[currentEvent window] screen] frame].size.height;
// location is top-left relative to desktop
// Now we need to convert that to be window-relative.
		float windowBottom = NSMinY ([[currentEvent window] frame]);
		location.y = screenHeight - location.y - windowBottom;
		location.x -= [[currentEvent window] frame].origin.x;
		
		NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown location:location modifierFlags:0 timestamp:0 windowNumber:windowNb context:NULL eventNumber:0 clickCount:1 pressure:1];
		

		[NSMenu popUpContextMenu:theMenu withEvent:event forView:NULL withFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
 */
	}
}

NSMenu* ExtractDelayedContextMenu ()
{
	if (gDelayedContextMenu)
	{
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
			return NULL;
		}
		
		// Build cocoa menu and show
		NSMenu* theMenu = [[[NSMenu alloc] init] autorelease];
		
		[CocoaMainMenu addItemsFromMenu: item->m_Submenu toNSMenu: theMenu];

		delete gDelayedContextMenu;
		gDelayedContextMenu = NULL;
		
		return theMenu;
	}
	return NULL;
}