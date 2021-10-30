#include "UnityPrefix.h"
#include "Editor/Platform/Interface/Pasteboard.h"

using namespace std;

static PasteboardData* ms_LastPasteBoardData = NULL;
static string pbType = string();

bool Pasteboard::IsDeclaredInPasteboard (string type)
{
	return pbType == type;
}

PasteboardData* Pasteboard::GetPasteboardData (string type)
{
	PasteboardData* retData = NULL;
	if (pbType == type)
		retData = ms_LastPasteBoardData;
	return retData;
}

void Pasteboard::SetPasteboardData (string type, PasteboardData* data)
{
	if (ms_LastPasteBoardData != NULL)
		delete ms_LastPasteBoardData;

	ms_LastPasteBoardData = data;
	pbType = type;
}

PasteboardData::~PasteboardData () {}