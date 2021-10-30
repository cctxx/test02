#ifndef PREFAB_MERGING_H
#define PREFAB_MERGING_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "PropertyModification.h"
#include "PrefabUtility.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include <vector>

/*
struct ExposedProperty
{
	std::string exposedName;

	PPtr<Object> target; // The object the property should be applied to
	std::string propertyPath;
	
	DECLARE_SERIALIZE(ExposedProperty)

};
typedef std::vector<ExposedProperty> ExposedProperties;

struct ExposedPropertyInstance
{
	std::string exposedName;

	string value; // The encoded value for all builtin types
	PPtr<Object> objectReference; // the serialized object reference
	
	DECLARE_SERIALIZE(ExposedPropertyInstance)
};
typedef std::vector<ExposedPropertyInstance> ExposedPropertyInstances;
template<class TransferFunction>
void ExposedProperty::Transfer (TransferFunction& transfer)
{
	TRANSFER(exposedName);
	TRANSFER(target);
	TRANSFER(propertyPath);
}

template<class TransferFunction>
void ExposedPropertyInstance::Transfer (TransferFunction& transfer)
{
	TRANSFER(exposedName);
	TRANSFER(value);
	TRANSFER(objectReference);
}

 */


struct PrefabModification
{
	typedef std::vector<PPtr<Object> > RemovedComponents;

	PPtr<Transform>              m_TransformParent;
	RemovedComponents            m_RemovedComponents;
	PropertyModifications        m_Modifications;
	
	DECLARE_SERIALIZE(PrefabModification)
};

template<class TransferFunction>
void PrefabModification::Transfer (TransferFunction& transfer)
{
	TRANSFER(m_TransformParent);
	TRANSFER(m_Modifications);
	TRANSFER(m_RemovedComponents);
}

void MergePrefabChanges (const std::vector<Object*>& prefabObjects, const std::vector<ObjectData>& instancedObjects, std::vector<ObjectData>& outputInstancedObjects, const std::vector<Object*>& addedObjects, std::vector<Object*>& outputAddedObjects, const PrefabModification& modificationInstance);

/// GeneratePrefabPropertyDiff by comparing all properties of prefab against clonedObject.
/// Returns if modified properties have been added or updated to a different value.
enum GeneratePrefabPropertyDiffResult { kNoChangesDiffResult = 0, kAddedOrUpdatedProperties = 1, kTypeTreeMismatch = 2 };
GeneratePrefabPropertyDiffResult GeneratePrefabPropertyDiff (Object& prefab, Object& clonedObject, RemapPPtrCallback* clonedObjectToParentObjectRemap, PropertyModifications& properties);

bool GenerateRootTransformPropertyModification (Object& object, PPtr<GameObject> root, PrefabModification& modification);

/// Inserts a single property modification into the properties modification array. Replacing any previous modifications if the propertyPath and target match.
GeneratePrefabPropertyDiffResult InsertPropertyModification (const PropertyModification& modification, std::vector<PropertyModification>& properties);

void InheritGameObjectAndTransformProperties(const std::vector<ObjectData>& instancedObjects);

/// 
void RecordPrefabInstancePropertyModifications (Object& prefabInstance, Object& prefabParent);

#endif