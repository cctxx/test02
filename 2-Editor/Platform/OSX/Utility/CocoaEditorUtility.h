#pragma once

#import <Cocoa/Cocoa.h>
#include "Runtime/Math/Color.h"
#include <string>

struct ColorRGBAf;

/// Returns a nice mouse delta for use in eg. zooming
float CalculateNiceMouseDelta (NSEvent* theEvent);

/// Store/Load all expanded rows in a outlineview
void StoreExpandedRows (NSOutlineView* olv, NSMutableArray* array);
void LoadExpandedRows (NSOutlineView* olv, NSMutableArray* array);

void SetTableColumnCell (NSTableColumn* column, NSCell* cell);

NSColor *NSColorFromPrefColor (const ColorRGBAf &color);

/// Loads a nib and caches it. 
/// equivalent [NSBundle loadNibNamed: aNibName owner:owner];)
bool LoadNibNamed (NSString* nsNibName, id owner, id *globalObjects = nil);


NSString* GetProjectDirectory ();

ColorRGBAf GetSelectedTextColor ();
ColorRGBAf GetSelectedBackgroundColor ();

