#include "UnityPrefix.h"
#include "Editor/Src/Undo/UndoManager.h"
#include <afxres.h>

// MenuControllerWin.cpp
void UpdateMenuItemNameByID( DWORD id, const std::string& name, bool enabled );


void SetUndoMenuNamePlatformDependent (std::string undoName, std::string redoName)
{
	UndoManager& undo = GetUndoManager();
	UpdateMenuItemNameByID( ID_EDIT_UNDO, "Undo " + undoName + "\tCtrl+Z", undo.HasUndo() );
	UpdateMenuItemNameByID( ID_EDIT_REDO, "Redo " + redoName + "\tCtrl+Y", undo.HasRedo() );
}
