#include "UnityPrefix.h"
#include "PropertyDiffUndoRecorder.h"
#include "PropertyDiffUndoUtilities.h"
#include "UndoManager.h"
#include "PropertyDiffUndo.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

using namespace std;

/*
* How the Undo works:
* You add an object to the recording stack
* When recording stops we calculate a property modification diff between the objects state when recording started and the current state
*		The diff will the contain the property modifications needed to revert to the old state
* The diff is stored on the undo stack
* Redo is simply the reverse process
*/

static PropertyDiffUndoRecorder* gInstance = NULL;

static RegisterRuntimeInitializeAndCleanup s_PropertyDiffUndoRecorderCallbacks(PropertyDiffUndoRecorder::StaticInitialize, PropertyDiffUndoRecorder::StaticDestroy);

void PropertyDiffUndoRecorder::StaticInitialize()
{
	gInstance = UNITY_NEW_AS_ROOT(PropertyDiffUndoRecorder, kMemUndo, "Undo", "PropertyDiffUndoRecorder");
}

void PropertyDiffUndoRecorder::StaticDestroy()
{
	UNITY_DELETE(gInstance, kMemUndo);
	gInstance = NULL;
}

PropertyDiffUndoRecorder::PropertyDiffUndoRecorder()
{
	m_PostprocessCallback = NULL;
}

void PropertyDiffUndoRecorder::RecordObject(Object* target, const UnityStr& actionName)
{
	RecordObjects (&target, 1, actionName);
}

void PropertyDiffUndoRecorder::RecordObjects(Object** object, int object_count, const UnityStr& actionName)
{
	// Record the current state of each object
	for (int i = 0; i < object_count; i++)
	{
		SerializeObjectAndAddToRecording(object[i], m_CurrentlyRecording);
	}
	
	if (!actionName.empty())
		m_CurrentRecordingName = actionName;
}


void PropertyDiffUndoRecorder::Flush()
{
	// Nothing to flush
	if (m_CurrentlyRecording.size() == 0)
		return;

	// Generate modifications from currently recording
	PropertyModifications modifications;
	GenerateUndoDiffs(m_CurrentlyRecording, modifications);

	// Convert to UndoPropertyModifications (keepPrefabOverride support)
	UndoPropertyModifications undoableModifications;
	undoableModifications.resize(modifications.size());
	for (int i = 0; i < modifications.size(); i++)
	{
		UndoPropertyModification& undoModification = undoableModifications[i];
		undoModification.modification =  modifications[i];
		undoModification.keepPrefabOverride = true;

		list<RecordedObject>::const_iterator recordItem = std::find(m_CurrentlyRecording.begin(), m_CurrentlyRecording.end(), modifications[i].target);
		// If the prefab modification did not exist before we have to tell the Undo system to remove the property modification when undoing
		if (!HasPrefabOverride(recordItem->existingPrefabModifications, recordItem->target, modifications[i].propertyPath))
			undoModification.keepPrefabOverride = false;
	}

	// Postprocess
	if (m_PostprocessCallback != NULL && !undoableModifications.empty())
		m_PostprocessCallback (undoableModifications);

	RegisterPropertyModificationUndo (undoableModifications, m_CurrentRecordingName);

	// Clean up the state
	m_CurrentRecordingName.clear();
	m_CurrentlyRecording.clear();
}

PropertyDiffUndoRecorder& GetPropertyDiffUndoRecorder()
{
	return *gInstance;
}


void RecordUndoDiff (Object** o, int size, const std::string& actionName)
{
	GetPropertyDiffUndoRecorder ().RecordObjects (o, size, actionName);
}

void RecordUndoDiff (Object* o, const std::string& actionName)
{
	GetPropertyDiffUndoRecorder ().RecordObject (o, actionName);
}