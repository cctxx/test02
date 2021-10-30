#include "UnityPrefix.h"
#include "PropertyDiffUndo.h"
#include "UndoManager.h"
#include "PropertyDiffUndoUtilities.h"
#include "Editor/Src/Prefabs/BatchApplyPropertyModification.h"
#include "Editor/Src/Prefabs/Prefab.h"

using namespace std;

PropertyDiffUndo::PropertyDiffUndo(const UndoPropertyModifications& modifications)
: m_UndoPropertyModifications (modifications)

{
}

static bool IsSceneObject (const UndoPropertyModifications& modifications)
{
    for (int i=0;i<modifications.size();i++)
    {
        Object* target = modifications[i].modification.target;
        if (target && target->IsPersistent())
            return false;
    }
    return true;
}

void RegisterPropertyModificationUndo (UndoPropertyModifications& modifications, const UnityStr& actionName)
{
	// Remove any modifications from the input, if the modification is already part of the modifications in the current event group
	if (!GetUndoManager().IsUndoing () && !GetUndoManager().IsRedoing())
	{
		GetUndoManager().RemoveDuplicateModifications(modifications, actionName, GetUndoManager().GetCurrentGroup());
	}

	if (modifications.empty())
		return;

	SET_ALLOC_OWNER(&GetUndoManager());

	PropertyDiffUndo* propertyDiffUndo = UNITY_NEW(PropertyDiffUndo, kMemUndo) (modifications);
	propertyDiffUndo->SetName(actionName);
    propertyDiffUndo->SetIsSceneUndo(IsSceneObject (modifications));
    
	GetUndoManager().RegisterUndo(propertyDiffUndo);
}

bool PropertyDiffUndo::Restore(bool registerRedo)
{
	std::list<RecordedObject> redoRecording;

	if (registerRedo)
	{
		// Record the current state of all valid objects 
		PPtr<Object> currentTarget = NULL;
		for (int i=0; i < m_UndoPropertyModifications.size(); i++)
		{
			if (m_UndoPropertyModifications[i].modification.target != currentTarget)
			{
				currentTarget = m_UndoPropertyModifications[i].modification.target;
				if (currentTarget.IsValid())
					SerializeObjectAndAddToRecording(currentTarget, redoRecording);
			}
		}
	}

	// Apply the previous property state of the target object
	BatchApplyPropertyModification batch;
	for (int i = 0; i < m_UndoPropertyModifications.size(); i++)
		batch.Apply(m_UndoPropertyModifications[i].modification, m_UndoPropertyModifications[i].keepPrefabOverride);
	batch.Complete();

	
	if ( registerRedo )
	{
		// Create the redo property diffs
		PropertyModifications modifications;
		GenerateUndoDiffs (redoRecording, modifications);

		UndoPropertyModifications undoableModifications;
		for (int i = 0; i < modifications.size(); i++)
		{
			UndoPropertyModification undoModification;
			undoModification.modification =  modifications[i];
			undoModification.keepPrefabOverride = true;
			undoableModifications.push_back(undoModification);
		}

		RegisterPropertyModificationUndo (undoableModifications, m_Name);
	}

	return true;
}

void PropertyDiffUndo::RemoveDuplicates( UndoPropertyModifications& modifications )
{
	bool erased = false;
	for (UndoPropertyModifications::iterator pm = modifications.begin(); pm != modifications.end();)
	{
		erased = false;
		for (int i = 0; i < m_UndoPropertyModifications.size(); i++)
		{
			if (PropertyModification::ComparePathAndTarget(pm->modification, m_UndoPropertyModifications[i].modification))
			{
				pm = modifications.erase(pm);
				erased = true;
				break;
			}
		}

		if (!erased)
			++pm;
	}
}
