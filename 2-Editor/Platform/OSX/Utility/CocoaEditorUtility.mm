#include "UnityPrefix.h"
#include "CocoaEditorUtility.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Utilities/File.h"
#include <map>

using namespace std;
// Simple way of copying a column's settings
void SetTableColumnCell (NSTableColumn* column, NSCell* cell)
{
	[cell setFont: [[column dataCell]font]];
	[cell setControlSize: NSSmallControlSize];
	[cell setEditable: [[column dataCell]isEditable]];
	// set cell
	[column setDataCell: cell];
}

NSColor *NSColorFromPrefColor (const ColorRGBAf &color) {
	return [NSColor colorWithDeviceRed:color.r green:color.g blue:color.b alpha:color.a];
}

void StoreExpandedRows (NSOutlineView* olv, NSMutableArray* array)
{
	[array removeAllObjects];
	for (int i=0;i<[olv numberOfRows];i++)
		if ([olv isItemExpanded: [olv itemAtRow: i]])
			[array addObject: [NSNumber numberWithInt: i]];
}

void LoadExpandedRows (NSOutlineView* olv, NSMutableArray* array)
{
	for (unsigned int i=0;i<[array count];i++)
	{
		int row = [[array objectAtIndex: i]intValue];
		if (row < [olv numberOfRows])
			[olv expandItem: [olv itemAtRow: row]];
	}
}

bool LoadNibNamed (NSString* nsNibName, id owner, id *globalObjects)
{
//	return [NSBundle loadNibNamed: aNibName owner:owner];
	static std::map <string, NSNib *> s_NibFiles;
	string nibName (MakeString (nsNibName));
	NSNib *nib = s_NibFiles[nibName];
	if (!nib) {
//		NSLog (@"Loading nib %@", aNibName);
		s_NibFiles[nibName] = nib = [[NSNib alloc] initWithNibNamed: nsNibName bundle:nil];
	}
	return [nib instantiateNibWithOwner: owner topLevelObjects: globalObjects];
}

float CalculateNiceMouseDelta (NSEvent* theEvent)
{
	float dx = [theEvent deltaX];
	float dy = [theEvent deltaY];
	
	// Decide which direction the mouse delta goes.
	// Problem is that when the user zooms horizontal and vertical, it can jitter back and forth.
	// So we only update from which axis we pick the sign if x and y 
	// movement is not very close to each other
	static bool useYSign = false;
	if (Abs (Abs (dx) - Abs (dy)) / max (Abs (dx), Abs (dy)) > .1F)
	{
		if (Abs (dx) > Abs (dy))
			useYSign = false;
		else
			useYSign = true;
	}

	float length = sqrt (dx*dx+dy*dy);
	if (useYSign)
		return Sign (dy) * length;
	else
		return Sign (dx) * length;
}


NSString* GetProjectDirectory ()
{
	return MakeNSString(File::GetCurrentDirectory());
}

ColorRGBAf GetSelectedTextColor () {
	NSColor *col = [[NSColor selectedTextColor] colorUsingColorSpaceName: [[NSColor redColor]colorSpaceName]];
	return ColorRGBAf ([col redComponent], [col greenComponent], [col redComponent], [col alphaComponent]);
}
ColorRGBAf GetSelectedBackgroundColor () {
	NSColor *col = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName: [[NSColor redColor]colorSpaceName]];
	return ColorRGBAf ([col redComponent], [col greenComponent], [col redComponent], [col alphaComponent]);
}
