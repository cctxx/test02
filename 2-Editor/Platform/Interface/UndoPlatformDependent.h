#pragma once

class UndoBase;

void SetUndoMenuNamePlatformDependent (std::string undoName, std::string redoName);

#ifdef __OBJC__
@class NSUndoManager;
NSUndoManager* GetGlobalCocoaUndoManager ();
#endif
