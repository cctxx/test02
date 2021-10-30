#include "UnityPrefix.h"
#include "Editor/Platform/Interface/DragAndDrop.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#import <Cocoa/Cocoa.h>

using namespace std;

static void WriteArrayToDrag (NSArray* array, NSPasteboard* pboard, NSString* pboardType);
static NSArray* ReadArrayFromDrag (id info, NSString* pboardType);

static NSString* kPPtrArrayPboardType = @"PPtrArrayPboardType";


/*
MonoDragAndDropData::MonoDragAndDropData (MonoObject* obj)
{
	m_MonoReference = mono_gchandle_new (obj, 0);
}

MonoObject* MonoDragAndDropData::GetMonoObject()
{
	MonoObject* instance = mono_gchandle_get_target(m_MonoReference);
	return instance;
}

MonoDragAndDropData::~MonoDragAndDropData ()
{
	mono_gchandle_free (m_MonoReference);
}

DragAndDropData::~DragAndDropData () {}


void ClearWriteItemsToDrag (NSArray* items, NSPasteboard* pboard)
{
	ClearPasteboard (pboard);
	NSMutableArray* array = [NSMutableArray arrayWithCapacity: [items count]];
	for (int i=0;i<[items count];i++)
		[array addObject: [NSNumber numberWithInt: [[items objectAtIndex: i] instanceID]]];
	WriteArrayToDrag (array, pboard, kPPtrArrayPboardType);
}
*/

static DragAndDrop* gSingleton = NULL;
DragAndDrop& GetDragAndDrop ()
{
	if (gSingleton == NULL)
		gSingleton = new DragAndDrop();
	return *gSingleton;
}

DragAndDrop::DragAndDrop ()
{
	m_PBoard = NULL; m_DelayedStartDrag = NULL;
}

void DragAndDrop::Setup (id sender)
{
	m_VisualMode = kDragOperationNone;
	m_AcceptedDrag = false;
	SetActiveControlID(0);
	m_PBoard = [sender draggingPasteboard];
	[m_PBoard retain];
	
}

void DragAndDrop::Cleanup ()
{
	[m_PBoard release];
	m_PBoard = NULL;
}

NSArray* DragAndDrop::GetAllSupportedTypes()
{
	return [NSArray arrayWithObjects: kPPtrArrayPboardType, NSFilenamesPboardType, nil];
}

static void WriteArrayToDrag (NSArray* array, NSPasteboard* pboard, NSString* pboardType)
{
	// Copy old type
	NSArray* types = [pboard types];

	// Copy old data
	NSMutableArray* datas = [NSMutableArray array];
	for (int i=0;i <[types count];i++)
		[datas addObject: [pboard propertyListForType: [types objectAtIndex: i]]];
	
	types = [[pboard types]arrayByAddingObject: pboardType];
	[datas addObject: array];
	
	// Declare and add data
	[pboard declareTypes: types owner: NULL];
	for (int i=0;i<[datas count];i++)
	{
		[pboard setPropertyList: [datas objectAtIndex: i] forType: [types objectAtIndex: i]];
	}
}

NSArray* ReadArrayFromDrag (NSPasteboard* pboard, NSString* pboardType)
{
	if ([pboard availableTypeFromArray: [NSArray arrayWithObject: pboardType]] == NULL)
		return NULL;
	if ([pboard dataForType: pboardType] == NULL)
		return NULL;
	
	NSArray* array = [pboard propertyListForType: pboardType];
	return array;
}


// Get PPtrs from the pboard
std::vector<PPtr<Object> > DragAndDrop::GetPPtrs()
{
	NSArray* array = ReadArrayFromDrag (m_PBoard, kPPtrArrayPboardType);
	if (array == NULL)
		return std::vector<PPtr<Object> > ();

	std::vector<PPtr<Object> > objects;
	for (int i=0;i<[array count];i++)
		objects.push_back(PPtr<Object> ([[array objectAtIndex: i]intValue]));

	return objects;
}

// Add PPtrs to the pboard
void DragAndDrop::SetPPtrs(std::vector<PPtr<Object> >& objects)
{
	NSMutableArray* array = [NSMutableArray arrayWithCapacity: objects.size()];
	for (int i=0;i<objects.size();i++)
		[array addObject: [NSNumber numberWithInt: objects[i].GetInstanceID()]];

	WriteArrayToDrag (array, m_PBoard, kPPtrArrayPboardType);
}

std::vector<std::string> DragAndDrop::GetPaths()
{
	NSArray* nodes = ReadArrayFromDrag (m_PBoard, NSFilenamesPboardType);
	vector<string> paths;
	for (int i=0;i<[nodes count];i++)
	{
		string absolutePath = MakeString([nodes objectAtIndex: i]);
		string relative = GetProjectRelativePath(absolutePath);
		if (!relative.empty())
			paths.push_back(relative);
		else
			paths.push_back(absolutePath);
	}
	return paths;
}

void DragAndDrop::SetPaths(const std::vector<std::string>& paths)
{
	NSMutableArray* array = [NSMutableArray arrayWithCapacity: paths.size()];
	
	for (int i=0;i<paths.size();i++)
	{
		string path = paths[i];
		if (path.empty())
			continue;
		path = PathToAbsolutePath(GetActualPathSlow (path));
		[array addObject: MakeNSString (path)];
	}
	
	WriteArrayToDrag (array, m_PBoard, NSFilenamesPboardType);
}


@interface DelayedDrag : NSObject
{
@public
	NSString* title;
	NSPasteboard* pboard;
	NSPoint location;
	NSWindow* window;
	NSEvent* event;
}

@end

#include "Editor/Src/Application.h"

@implementation DelayedDrag

- (void) DoStart
{
    NSPoint dragPosition = location;
    dragPosition.x -= 16;
    dragPosition.y -= 16;

//  NSImage *dragImage = [[NSWorkspace sharedWorkspace] iconForFile:@"/Users/peter/Archive.zip"];

	// Generate drag image from text
	NSImage* dragImage = [[NSImage alloc]init];
	
	NSRect bounds;
	bounds.origin.x = bounds.origin.y = 0;
	bounds.size.width = 256;
	bounds.size.height = 32;
	[dragImage setSize: bounds.size];
	[dragImage lockFocus];
	[dragImage compositeToPoint:NSMakePoint (NSMinX(bounds) + 18.0f, NSMaxY (bounds) - 15.0) operation:NSCompositeSourceOver fraction:1.0f];
	NSDictionary* attribs = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]], NSFontAttributeName, nil];
	[title drawAtPoint: NSMakePoint(NSMinX(bounds) + 16.0, NSMinY(bounds) + 16.0) withAttributes: attribs];
	[dragImage unlockFocus];
	
	GetApplication().ResetReloadAssemblies();
	
	[window dragImage:dragImage 
            at:dragPosition
            offset:NSZeroSize
            event:event
            pasteboard:pboard
            source:window
            slideBack:YES];

	[dragImage release];
	[pboard release];
	[window release];
	[title release];
	[event release];
}
@end

void DragAndDrop::ApplyQueuedStartDrag ()
{
	if (m_DelayedStartDrag)
	{
		// Make sure we dont start the same drag multiple times
		DelayedDrag* drag = m_DelayedStartDrag;
		m_DelayedStartDrag = NULL;
		// Start the actual drag through cocoa
		[drag DoStart];
		[drag release];
	}
}


void DragAndDrop::StartDrag (string title)
{
	if (m_PBoard == NULL)
	{
		ErrorString("No Drag&Drop has been setup. Please");
		return;
	}
	
	if (m_DelayedStartDrag != NULL)
	{
		ErrorString("Starting multiple Drags. You can only start on drag at a time!");
		return;
	}
	
	NSEvent* theEvent = [NSApp currentEvent];

	DelayedDrag* drag = [[DelayedDrag alloc]init];
	drag->title = [MakeNSString(title) retain];
	drag->pboard = [m_PBoard retain];
	drag->location = [theEvent locationInWindow];
	drag->window = [[theEvent window]retain];
	drag->event = [theEvent retain];
	m_DelayedStartDrag = drag;
	
	[m_PBoard release];
	m_PBoard = NULL;
}

void DragAndDrop::PrepareStartDrag ()
{
	m_PBoard = [NSPasteboard pasteboardWithUniqueName];
	[m_PBoard retain];
}


/*
DragAndDropData* DragAndDrop::GetData (std::string type)
{
	if (m_Supported.count(type))
		return m_Supported[type];
	else
		return NULL;
}

void DragAndDrop::SetData (std::string type, DragAndDropData* data)
{
	delete m_Supported[type];
	m_Supported[type] = data;
}	

vector<PPtr<Object> > DragAndDrop::GetPPtrs()
{
	if (GetData (kPPtrArrayType))
	{
		PPtrDragAndDropData* ret = dynamic_cast<PPtrDragAndDropData*> (GetData (kPPtrArrayType));
		if (ret)
			return ret->m_PPtrs;
	}
	return vector<PPtr<Object> > ();
}

// Get the All PPtrs
void DragAndDrop::SetPPtrs(vector<PPtr<Object> >& objects)
{
	if (objects.empty())
		SetData(kPPtrArrayType, NULL);
	else
	{
		PPtrDragAndDropData* data = new PPtrDragAndDropData();
		data->m_PPtrs = objects;
		SetData(kPPtrArrayType, data);
	}
}

void DragAndDrop::SetPPtrs(vector<Object*>& objects)
{
	if (objects.empty())
		SetData(kPPtrArrayType, NULL);
	else
	{
		PPtrDragAndDropData* data = new PPtrDragAndDropData();
		data->m_PPtrs = objects;
		SetData(kPPtrArrayType, data);
	}
}

vector<string> DragAndDrop::GetPathNames()
{
	if (GetData (kPathNameType))
	{
		PathNameDragAndDropData* ret = dynamic_cast<PathNameDragAndDropData*> (GetData (kPathNameType));
		if (ret)
			return ret->m_PathNames;
	}
	return vector<string> ();
}

void DragAndDrop::SetPathNames(const vector<string>& paths)
{
	if (paths.empty())
		SetData(kPathNameType, NULL);
	else
	{
		PathNameDragAndDropData* data = new PathNameDragAndDropData();
		data->m_PathNames = paths;
		SetData(kPathNameType, data);
	}
}

MonoObject* DragAndDrop::GetMonoData (string type)
{
	MonoDragAndDropData* ret = dynamic_cast<MonoDragAndDropData*> (GetData (type));
	if (ret)
		return ret->GetMonoObject();
	else
		return NULL;
}

void DragAndDrop::SetMonoData (string type, MonoObject* obj)
{
	if (obj == NULL)
		SetData (type, NULL);
	else
	{
		MonoDragAndDropData* ret = new MonoDragAndDropData (obj);
		SetData (type, ret);
	}
}*/