#pragma once

#include "Runtime/Utilities/CopyPaste.h"

class PasteboardData
{
	public:
	
	virtual ~PasteboardData ();
};

class Pasteboard
{
	public:
	
	static bool IsDeclaredInPasteboard (std::string type);
	
	/// Get/Set Generic pasteboard data. The pasteboard data is only available locally to the application and is stored in a global variable.
	static PasteboardData* GetPasteboardData (std::string type);
	static void SetPasteboardData (std::string type, PasteboardData* data);
	
	/// Copy & Paste of text (system wide)
	static void   SetText (std::string text) { SetCopyBuffer(text); }
	static std::string GetText () { return GetCopyBuffer (); }
};