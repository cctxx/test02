#include "UnityPrefix.h"
#include "PropertyDiffUndoUtilities.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Serialize/TransferUtility.h"

using namespace std;

void SerializeObjectAndAddToRecording(Object* object, std::list<RecordedObject>& recording)
{
	// Don't add objects twice
	if (std::find(recording.begin(), recording.end(), PPtr<Object>(object)) != recording.end())
		return;
	
	recording.push_back(RecordedObject ());
	RecordedObject& recordedObject = recording.back();
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(object);

	if (prefab != NULL)
		prefab->GetPropertyModificationsForObject(GetPrefabParentObject(object), recordedObject.existingPrefabModifications);

	WriteObjectToVector(*object, &recordedObject.preEditState, kSerializeForPrefabSystem);
	recordedObject.target = object;

	if (object->GetNeedsPerObjectTypeTree())
		recordedObject.typeTree = GenerateCachedTypeTree(*object, kSerializeForPrefabSystem);
}

void GenerateUndoDiffs(const std::list<RecordedObject>& currentlyRecording, PropertyModifications& output)
{
	std::list<RecordedObject>::const_iterator i = currentlyRecording.begin();
	for (; i != currentlyRecording.end(); ++i)
	{
		const RecordedObject& recordItem = *i;
		if (recordItem.target.IsNull())
			continue;
		
		// Get the type tree from the current state of the object
		// Serialize the current state to memory using WriteObjectToVector
		// Do a property diff between the two serialized data states
		const TypeTree& typeTree = GenerateCachedTypeTree(*recordItem.target, kSerializeForPrefabSystem);

		if (recordItem.target->GetNeedsPerObjectTypeTree())
		{
			// Verify that the typetree in the current state matches the typetree when recording was started.
			// This should always be the case otherwise we missed the registration of an undo able action
			// which should be fixed
			bool isStreamCompatible = IsStreamedBinaryCompatbile(typeTree, recordItem.typeTree);

			if ( !isStreamCompatible)
			{
				// this should really not happen
				ErrorStringObject("Generating diff  of this object for undo because the type tree changed.\n"
					"This happens if you have used Undo.RecordObject when changing the script property.\n"
					"Please use Undo.RegisterCompleteObjectUndo", recordItem.target); 
				continue;
			}
		}

		dynamic_array<UInt8> currentState(kMemTempAlloc);
		WriteObjectToVector(*recordItem.target, &currentState, kSerializeForPrefabSystem);

		// And generate property diff, for any properties that are different from the prefab parent
		std::vector<PropertyModification> newProperties;
		GeneratePropertyDiff(typeTree, currentState, recordItem.preEditState, NULL, newProperties);
		// We have to set the target on each property modification or we will not be able to apply it later
		for (int i = 0; i < newProperties.size(); i++)
		{
			newProperties[i].target = recordItem.target;
		}

		if ( newProperties.size() != 0)
		{
			output.insert(output.end(), newProperties.begin(), newProperties.end());
		}
	}
}

bool operator==(const RecordedObject& left, PPtr<Object> right)
{
	return left.target == right;
}
