#pragma once
#include "PropertyDiffUndo.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TypeTree.h"
#include <list>

struct RecordedObject
{
	PPtr<Object>			target;
	dynamic_array<UInt8>	preEditState;
	TypeTree				typeTree;
	PropertyModifications	existingPrefabModifications;

	friend bool operator==(const RecordedObject& left, PPtr<Object> right);
};

void SerializeObjectAndAddToRecording(Object* object, std::list<RecordedObject>& recording);
void GenerateUndoDiffs(const std::list<RecordedObject>& currentRecording, PropertyModifications& output);

