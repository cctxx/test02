#include "UnityPrefix.h"
#include "SelectionUndo.h"
#include "UndoManager.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/BaseClasses/IsPlaying.h"

bool SelectionUndo::Restore (bool registerRedo)
{
	SetActiveObject(m_Active);
	SetObjectSelection(m_Selection);
	return true;
}

void RegisterSelectionUndo ()
{
	std::set<PPtr<Object> > curSelection = GetObjectSelectionPPtr();
	PPtr<Object> curActive = GetActiveObject();
    Object* curveActivePtr = curActive;
    
    bool sceneUndo = curveActivePtr == NULL || !curveActivePtr->IsPersistent();
    
	// Create Undo
	SET_ALLOC_OWNER(&GetUndoManager());
	SelectionUndo* undo = UNITY_NEW(SelectionUndo, kMemUndo) ();
    
    undo->SetIsSceneUndo(sceneUndo);
	undo->m_Selection = curSelection;
	undo->m_Active = curActive;
	undo->SetName("Selection Change");
	undo->SetNamePriority(-1);
	GetUndoManager().RegisterUndo(undo);
}