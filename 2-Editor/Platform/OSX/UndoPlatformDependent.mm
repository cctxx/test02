#include "UnityPrefix.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Editor/Platform/Interface/UndoPlatformDependent.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#import <Cocoa/Cocoa.h>


using namespace std;

NSUndoManager* GetGlobalCocoaUndoManager ()
{
	return NULL;
}

@interface UndoMenu : NSObject
{
	@public
	
	IBOutlet id				m_UndoItem;
	IBOutlet id 			m_RedoItem;
}

- (IBAction)undo:(id)sender;
- (IBAction)redo:(id)sender;

@end

UndoMenu* gUndoMenu = NULL;


@implementation UndoMenu
-(void)awakeFromNib {
	gUndoMenu = self;
}

- (IBAction)redo:(id)sender
{
	GetUndoManager().Redo ();
}

- (IBAction)undo:(id)sender
{
	GetUndoManager().Undo ();
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
	if (menuItem == m_UndoItem)
		return GetUndoManager().HasUndo();
	else
		return GetUndoManager().HasRedo();
}

@end

void SetUndoMenuNamePlatformDependent (string undoName, string redoName)
{
	if (gUndoMenu)
	{
		[gUndoMenu->m_UndoItem setTitle:MakeNSString("Undo " + undoName)];
		[gUndoMenu->m_RedoItem setTitle:MakeNSString("Redo " + redoName)];
	}
}