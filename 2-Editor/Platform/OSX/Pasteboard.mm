#include "UnityPrefix.h"
#include "Editor/Platform/Interface/Pasteboard.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#import <Cocoa/Cocoa.h>

using namespace std;

static PasteboardData* ms_LastPasteBoardData = NULL;
	
bool Pasteboard::IsDeclaredInPasteboard (string type)
{
	NSPasteboard* pboard = [NSPasteboard generalPasteboard];
	return [pboard availableTypeFromArray: [NSArray arrayWithObject: MakeNSString(type)]] != NULL;
}

PasteboardData* Pasteboard::GetPasteboardData (string type)
{
	NSPasteboard* pboard = [NSPasteboard generalPasteboard];
	if ([pboard availableTypeFromArray: [NSArray arrayWithObject: MakeNSString(type)]] != NULL)
	{
		NSData* data = [pboard dataForType: MakeNSString(type)];
		void* dataPtr = NULL;
		[data getBytes:&dataPtr length:sizeof(void*)];
		if (dataPtr == ms_LastPasteBoardData)
			return ms_LastPasteBoardData;
	}
	
	return NULL;
}

void Pasteboard::SetPasteboardData (string type, PasteboardData* data)
{
	delete ms_LastPasteBoardData;
	ms_LastPasteBoardData = data;

	// save to pboard
	NSPasteboard* pboard = [NSPasteboard generalPasteboard];
	[pboard declareTypes: [NSArray arrayWithObject: MakeNSString(type)] owner: NULL];
	[pboard setData: [NSData dataWithBytes: &ms_LastPasteBoardData length: sizeof(void*)] forType: MakeNSString(type)];
}

PasteboardData::~PasteboardData () {}