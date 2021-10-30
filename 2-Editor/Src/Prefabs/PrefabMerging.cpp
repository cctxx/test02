#include "UnityPrefix.h"
#include "PrefabMerging.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "GenerateCachedTypeTree.h"
#include "PrefabUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Misc/GameObjectUtility.h"

static void ApplyPrefabStateAndPropertyModificationsToObjects (const vector<ObjectData>& instancedObjects, const PrefabModification& parentModification, const PrefabModification& instanceModification);

using namespace std;


////@TODO: wouldn't it be better to do things based on instance id's instead of actually loading object pointers here?

struct RemapFromPrefabToInstancePPtr
{
	const PrefabParentCache& m_Cache;
	RemapFromPrefabToInstancePPtr (const PrefabParentCache& cache) : m_Cache (cache) { }

	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (!IsTypeTreePPtr (typeTree))
			return true;

		int instanceID = ExtractPPtrInstanceID (data, bytePosition);

		Object* prefabObject = dynamic_instanceID_cast<Object*> (instanceID);
		if (prefabObject)
		{
			Object* remappedInstance = FindInstanceFromPrefabObjectCached(*prefabObject, m_Cache);
			if (remappedInstance)
				SetPPtrInstanceID (remappedInstance->GetInstanceID(), data, bytePosition);
		}

		return false;
	}
};

/*
static void InjectExposedProperties (const ExposedProperties& exposedProperties, const ExposedPropertyInstances& exposedInstances, PPtr<Object> target, vector<PropertyModification>& output)
{
	// Extract exposed properties and retarget them to the instance
	for (int e=0;e<exposedProperties.size();e++)
	{
		const ExposedProperty& exposed = exposedProperties[e];
		if (exposed.target == target)
		{
			for (int j=0;j<exposedInstances.size();j++)
			{
				const ExposedPropertyInstance& exposedInstance = exposedInstances[j];
				if (exposedInstance.exposedName == exposed.exposedName)
				{
					output.push_back(PropertyModification());
					output.back().target = target;
					output.back().propertyPath = exposed.propertyPath;
					output.back().value = exposedInstance.value;
					output.back().objectReference = exposedInstance.objectReference;
				}
			}
		}
	}
}
*/

/// - Copy data from parent into all objects
/// - Remap references from prefab island to cloned island
/// - Apply property modifications
static void ApplyPrefabStateAndPropertyModificationsToObjects (const vector<ObjectData>& instancedObjects, const PrefabModification& modificationInstance)
{
	PrefabParentCache cache;
	BuildPrefabParentCache (instancedObjects, cache);

	for (int i=0;i<instancedObjects.size();i++)
	{
		const ObjectData& data = instancedObjects[i];

		// Extract prefab parent data
		dynamic_array<UInt8> buffer(kMemTempAlloc);
		WriteObjectToVector(*data.prefabParent, &buffer, kSerializeForPrefabSystem);
		const TypeTree& typeTree = GenerateCachedTypeTree (*data.prefabParent, kSerializeForPrefabSystem);

		// Remap pptrs from prefab -> prefab instance
		RemapFromPrefabToInstancePPtr remapPPtrFunctor (cache);
		IterateTypeTree (typeTree, buffer, remapPPtrFunctor);

		// Extract all property modifications that need to be applied to this Object
		vector<PropertyModification> modifications;
		for (int i=0;i<modificationInstance.m_Modifications.size();i++)
		{
			const PropertyModification& mod = modificationInstance.m_Modifications[i];
			if (mod.target == PPtr<Object> (data.prefabParent))
				modifications.push_back(mod);
		}

		// Extract exposed properties and retarget them to the instance
		//InjectExposedProperties(modificationParent.m_ExposedProperties, modificationInstance.m_ExposedInstances, PPtr<Object> (data.prefabParent), modifications);

		// Apply Property modifications to data
		ApplyPropertyModification(typeTree, buffer, &modifications[0], modifications.size());

		// Load data into prefab instance
		ReadObjectFromVector (data.objectInstance, buffer, kSerializeForPrefabSystem);
	}
}

static void RemapGameObjectProperties (const ObjectData& data, const PrefabParentCache& cache)
{
	GameObject* instance = dynamic_pptr_cast<GameObject*>(data.objectInstance);
	GameObject* prefab = dynamic_pptr_cast<GameObject*>(data.prefabParent);
	if (instance == NULL || prefab == NULL)
		return;

	// Remap game object array
	GameObject::Container& container = instance->GetComponentContainerInternal();
	container = prefab->GetComponentContainerInternal();
	for (int i=0;i<container.size();i++)
	{
		Unity::Component* componentPtr = container[i].second;
		if (componentPtr)
			componentPtr = dynamic_pptr_cast<Unity::Component*> (FindInstanceFromPrefabObjectCached(*componentPtr, cache));
		container[i].second = componentPtr;
	}
}

static void RemapComponentProperties (const ObjectData& data, const PrefabParentCache& cache)
{
	Unity::Component* instance = dynamic_pptr_cast<Unity::Component*>(data.objectInstance);
	Unity::Component* prefab = dynamic_pptr_cast<Unity::Component*>(data.prefabParent);
	if (instance == NULL || prefab == NULL)
		return;

	GameObject* remap = prefab->GetGameObjectPtr();
	if (remap)
		remap = dynamic_pptr_cast<GameObject*> (FindInstanceFromPrefabObjectCached(*remap, cache));

	instance->SetGameObjectInternal(remap);
}

///@TODO: Disallow changing m_Script PPtr from propertymodification!

static void RemapTransformProperties (const ObjectData& data, const PrefabParentCache& cache)
{
	Transform* instance = dynamic_pptr_cast<Transform*>(data.objectInstance);
	Transform* prefab = dynamic_pptr_cast<Transform*>(data.prefabParent);
	if (instance == NULL || prefab == NULL)
		return;

	Transform::TransformComList& container = instance->GetChildrenInternal();
	container = prefab->GetChildrenInternal();
	for (int i=0;i<container.size();i++)
	{
		Transform* childPtr = container[i];
		if (childPtr)
			childPtr = dynamic_pptr_cast<Transform*> (FindInstanceFromPrefabObjectCached(*childPtr, cache));
		container[i] = childPtr;
		Assert(instance != childPtr);
	}

	Transform* remap = prefab->GetParentPtrInternal();
	if (remap)
		remap = dynamic_pptr_cast<Transform*> (FindInstanceFromPrefabObjectCached(*remap, cache));
	instance->GetParentPtrInternal() = remap;
	Assert(instance != remap);
}

static void SetupMonoBehaviour (const ObjectData& data)
{
	MonoBehaviour* instance = dynamic_pptr_cast<MonoBehaviour*>(data.objectInstance);
	MonoBehaviour* prefab = dynamic_pptr_cast<MonoBehaviour*>(data.prefabParent);
	if (instance == NULL || prefab == NULL)
		return;

	instance->SetScript(prefab->GetScript());
}

// Setup game object hierarchy and transform hierarchy explicitly but don't modify any other properties
void InheritGameObjectAndTransformProperties(const vector<ObjectData>& instancedObjects)
{
	PrefabParentCache cache;
	BuildPrefabParentCache (instancedObjects, cache);

	// Setup game object, component, transform properties
	for (int i=0;i<instancedObjects.size();i++)
	{
		const ObjectData& data = instancedObjects[i];

		RemapGameObjectProperties(data, cache);
		RemapComponentProperties(data, cache);
		RemapTransformProperties(data, cache);
		SetupMonoBehaviour(data);
	}
}

///@TODO Optimization: Most of those removefrom parent calls are unneeded because they are internal
// in the prefab which is getting reset anyway...
static void ClearTransformParenting (const vector<ObjectData>& instancedObjects)
{
	/// Since instantiated prefabs can be children of other TransformComponents we need to remove all transforms from its parents.
	/// Children dont matter since a prefab can't have transform children outside the prefab
	for (int i=0;i<instancedObjects.size();i++)
	{
		Object* object = instancedObjects[i].objectInstance;
		if (dynamic_pptr_cast<Transform*> (object))
			static_cast<Transform*> (object)->RemoveFromParent ();
	}
}



/////@TODO: Review if we should chnage size of outputInstancedObjects or just clear pointers to instances that are dead!
void DestroyRemovedComponents (const PrefabModification::RemovedComponents& removed, vector<ObjectData>& outputInstancedObjects)
{
	vector<ObjectData> temp;
	temp.reserve(outputInstancedObjects.size());

	for (int i=0;i<outputInstancedObjects.size();i++)
	{
		Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (outputInstancedObjects[i].objectInstance);

		Object* prefabParentObject = outputInstancedObjects[i].prefabParent;
		if (count (removed.begin(), removed.end(), PPtr<Object> (prefabParentObject)) != 0 && component != NULL)
		{
			GameObject::RemoveComponentFromGameObjectInternal(*component);

			component->SetPersistentDirtyIndex(0);

			component->HackSetAwakeWasCalled();

			DestroySingleObject(component);

			continue;
		}
		else
		{
			temp.push_back(outputInstancedObjects[i]);
		}
	}

	outputInstancedObjects = temp;
}

int GetInstanceObjectIndex (Object* targetObject, const vector<ObjectData>& instancedObjects)
{
	for (int i=0;i<instancedObjects.size();i++)
	{
		if (targetObject == instancedObjects[i].objectInstance)
			return i;
	}
	return -1;
}


bool IsValidAddComponent (Unity::Component* component, const vector<ObjectData>& instancedObjects)
{
	if (component == NULL)
		return false;

	GameObject* go = component->GetGameObjectPtr();
	if (go == NULL)
		return false;

	if (dynamic_pptr_cast<Transform*> (component))
		return false;

	return GetInstanceObjectIndex (go, instancedObjects) != -1;
}

Transform* GetRootTransformFromObjectDataArray (const vector<ObjectData>& outputInstancedObjects)
{
	Assert(outputInstancedObjects.size());
	GameObject* root = dynamic_pptr_cast<GameObject*> (outputInstancedObjects[0].objectInstance);
	if (root)
		return root->QueryComponent(Transform);
	else
		return NULL;
}



void ValidateAddedObjects (const vector<Object*>& addedObjects, vector<Object*>& outputAddedObjects, vector<ObjectData>& outputInstancedObjects)
{
	// Add components that have been added
	for (int i=0;i<addedObjects.size();i++)
	{
		Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (addedObjects[i]);
		// Add component to the same place it was previously added to.
		if (component != NULL)
		{
			if (IsValidAddComponent(component, outputInstancedObjects))
			{
				GameObject* gameObject = component->GetGameObjectPtr();
				component->SetGameObjectInternal(NULL);

//				Assert(!gameObject->IsActive());

				GameObject::AddComponentInternal (*gameObject, *component);
				outputAddedObjects.push_back(component);
			}
			else
			{
				DestroySingleObject(component);
				continue;
			}
		}

		// Add game object to the same parent it was previously added to.
		GameObject* go = dynamic_pptr_cast<GameObject*> (addedObjects[i]);
		if (go != NULL)
		{
			Transform* transform = go->QueryComponent(Transform);
			// Inject transform back into it's old transform parent
			if (transform && GetInstanceObjectIndex(transform->GetParent(), outputInstancedObjects) != -1)
				transform->GetParent()->GetChildrenInternal().push_back(transform);
			// Old transform parent has been destroyed. Inject into root game object instead.
			else
			{
				transform->GetParentPtrInternal() = NULL;

				Transform* rootTransform = GetRootTransformFromObjectDataArray(outputInstancedObjects);
				Assert(rootTransform != NULL);
				transform->SetParent(rootTransform, Transform::kLocalPositionStays);
			}
		}

		Assert((go == NULL) != (component == NULL));
	}
}


void DeactivateGameObjectPrefabInstances (const vector<ObjectData>& instancedObjects)
{
	for (int i=0;i<instancedObjects.size();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (instancedObjects[i].objectInstance);
		if (go != NULL)
		{
			go->Deactivate (kNormalDeactivate);
		}
	}
}

void MergePrefabChanges (const std::vector<Object*>& prefabObjects, const std::vector<ObjectData>& instancedObjects, std::vector<ObjectData>& outputInstancedObjects, const vector<Object*>& addedObjects, vector<Object*>& outputAddedObjects, const PrefabModification& modificationInstance)
{
	Assert(outputInstancedObjects.empty());

	ClearTransformParenting(instancedObjects);

	// Fill toBeDestroyedObjects with all instanceObjects, with each merge operation we remove from the list
	// so that at the end of the process, we know what we have to kill off
	set<PPtr<Object> > toBeDestroyedObjects;
	for (int i=0;i<instancedObjects.size();i++)
		toBeDestroyedObjects.insert(instancedObjects[i].objectInstance);

	// Produce instances for all prefab objects
	// And remove those instances from toBeDestroyedObjects
	// Thus at the end of this loop we have all the instanceObjects (Reusing the ones the existing instances where possible)
	// And toBeDestroyedObjects is a list of all objects that could not be mapped to the prefab, thus shall be destroyed.
	PrefabParentCache attachedCache;
	BuildPrefabParentCache (instancedObjects, attachedCache);
	for (int i=0;i<prefabObjects.size();i++)
	{
		Object& prefabObject = *prefabObjects[i];

		// Find instance for the prefab object
		Object* clone = FindInstanceFromPrefabObjectCached (prefabObject, attachedCache);
		if (clone == NULL)
		{
			clone = Object::Produce (prefabObject.GetClassID ());
			clone->Reset();
		}

		outputInstancedObjects.push_back(ObjectData(clone, &prefabObject));
		toBeDestroyedObjects.erase(clone);
	}

	// Setup basic inherited transform / game object hierarchy properties
	InheritGameObjectAndTransformProperties(outputInstancedObjects);

	// Destroy any components that have been removed in the instance
	DestroyRemovedComponents(modificationInstance.m_RemovedComponents, outputInstancedObjects);

	// Reinject any added component or game objects
	ValidateAddedObjects(addedObjects, outputAddedObjects, outputInstancedObjects);

	// Setup objects and property modifications
	ApplyPrefabStateAndPropertyModificationsToObjects(outputInstancedObjects, modificationInstance);

	// Destroy Objects that are no longer in the prefab
	DestroyPrefabObjects (toBeDestroyedObjects);
}

GeneratePrefabPropertyDiffResult InsertPropertyModification (const PropertyModification& modification, std::vector<PropertyModification>& properties)
{
	// Check if the property already exists, in that case replace it
	for (int i=0;i<properties.size();i++)
	{
		if (modification.propertyPath == properties[i].propertyPath && properties[i].target == modification.target)
		{
			if (!PropertyModification::CompareValues(properties[i], modification))
			{
				properties[i] = modification;
				return kAddedOrUpdatedProperties;
			}
			else
			{
				return kNoChangesDiffResult;
			}
		}
	}

	// Add Property
	properties.push_back(modification);

	if (EndsWith(modification.propertyPath, "Array.size"))
		EnsureSizePropertiesSorting (properties);

	return kAddedOrUpdatedProperties;
}

GeneratePrefabPropertyDiffResult GeneratePrefabPropertyDiff (Object& prefab, Object& clonedObject, RemapPPtrCallback* clonedObjectToParentObjectRemap, std::vector<PropertyModification>& properties)
{
	const TypeTree& typeTree = GenerateCachedTypeTree(prefab, kSerializeForPrefabSystem);

	if (prefab.GetNeedsPerObjectTypeTree())
	{
		// Verify that the typetree matches, otherwise when the script pointer changes comparing the data will read out of bounds.
		TypeTree clonedTypeTree;
		GenerateTypeTree(clonedObject, &clonedTypeTree, kSerializeForPrefabSystem);
		if (!IsStreamedBinaryCompatbile(clonedTypeTree, typeTree))
			return kTypeTreeMismatch;
	}

	// Write binary data for comparison
	dynamic_array<UInt8> prefabData(kMemTempAlloc);
	WriteObjectToVector(prefab, &prefabData, kSerializeForPrefabSystem);

	dynamic_array<UInt8> clonedObjectData(kMemTempAlloc);
	WriteObjectToVector(clonedObject, &clonedObjectData, kSerializeForPrefabSystem);

	// Extract the current state of all existing property modfications
	// (We need this in addition to GeneratePropertyDiff, because properties can be overriden but still have the same value as the prefab parent)
	bool anyModificationsAgainstExistingProperties = ExtractCurrentValueOfAllModifications(&prefab, typeTree, clonedObjectData, &properties[0], properties.size());
	GeneratePrefabPropertyDiffResult result = anyModificationsAgainstExistingProperties ? kAddedOrUpdatedProperties : kNoChangesDiffResult;

	// And generate property diff, for any properties that are different from the prefab parent
	std::vector<PropertyModification> newProperties;

	GeneratePropertyDiff(typeTree, prefabData, clonedObjectData, clonedObjectToParentObjectRemap, newProperties);

	// Merge it into the existing properties array
	for (int j=0;j<newProperties.size();j++)
	{
		// Inject property modification
		newProperties[j].target = &prefab;
		GeneratePrefabPropertyDiffResult tempResult = InsertPropertyModification(newProperties[j], properties);

		if (tempResult == kAddedOrUpdatedProperties)
			result = kAddedOrUpdatedProperties;
	}
	return result;
}

bool GenerateRootTransformPropertyModification (Object& object, PPtr<GameObject> root, PrefabModification& modification)
{
	Transform* rootTransform = dynamic_pptr_cast<Transform*> (&object);
	if (rootTransform == NULL)
		return false;
	if (rootTransform->GetGameObjectPtr() != root)
		return false;

	PPtr<Transform> newParent = rootTransform->GetParent();

	if (modification.m_TransformParent == newParent)
		return false;

	modification.m_TransformParent = newParent;
	return true;
}
