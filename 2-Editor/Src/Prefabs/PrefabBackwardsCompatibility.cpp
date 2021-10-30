#include "UnityPrefix.h"
#include "PrefabBackwardsCompatibility.h"
#include <vector>
#include "Runtime/BaseClasses/GameObject.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Editor/Src/Prefabs/PrefabMerging.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"

using namespace std;

// Sets up override so that only variables and their child variables with kForceOverrideMask metaFlag are overridden 
bool InitOverrideVariableRecursive (dynamic_bitset& override, const TypeTree& typeTree)
{
	bool childrenOverridden = false;
	for (TypeTree::const_iterator i=typeTree.m_Children.begin ();i != typeTree.m_Children.end ();++i)
		childrenOverridden = InitOverrideVariableRecursive (override, *i) || childrenOverridden;
	
	override[typeTree.m_Index] = childrenOverridden;
	return override[typeTree.m_Index];
}

inline bool RemapOverrideRecurse (const TypeTree& oldTypeTree, const TypeTree& newTypeTree, dynamic_bitset& oldOverride, dynamic_bitset& newOverride)
{
	if (newTypeTree.IsBasicDataType ())
	{
		AssertIf (newTypeTree.m_Name != oldTypeTree.m_Name);
		newOverride[newTypeTree.m_Index] |= oldOverride[oldTypeTree.m_Index];
		return newOverride[newTypeTree.m_Index];
	}
	
	bool anyOverridden = false;
	for (TypeTree::const_iterator i = newTypeTree.begin ();i != newTypeTree.end ();i++)
	{
		TypeTree::const_iterator found;
		for (found = oldTypeTree.begin ();found != oldTypeTree.end ();found++)
			if (found->m_Name == i->m_Name)
				break;
		
		if (found != oldTypeTree.end () && RemapOverrideRecurse (*found, *i, oldOverride, newOverride))
			anyOverridden = true;
	}
	
	newOverride[newTypeTree.m_Index] = anyOverridden;
	return anyOverridden;
}


void ApplySpecialPrefabBackwardsOverride (const TypeTree& oldTypeTree, const TypeTree& newTypeTree, dynamic_bitset& oldOverride, dynamic_bitset& newOverride)
{
	bool isRemappedPropertyOverridden = false;
	const char* objectType = "GameObject";
	const char* oldPropertyName = "m_IsStatic";
	const char* newPropertyName = "m_StaticEditorFlags";
	
	if (oldTypeTree.m_Type != objectType || newTypeTree.m_Type != objectType)
		return;
	
	for (TypeTree::const_iterator i=oldTypeTree.begin();i != oldTypeTree.end();i++)
	{
		const std::string& name = i->m_Name;
		if (name == oldPropertyName)
		{
			if (oldOverride[i->m_Index])
				isRemappedPropertyOverridden = true;
			break;
		}
	}
	
	if (!isRemappedPropertyOverridden)
		return;

	for (TypeTree::const_iterator i=newTypeTree.begin();i != newTypeTree.end();i++)
	{
		if (i->m_Name == newPropertyName)
		{
			newOverride[i->m_Index] = true;
			return;
		}
	}
}

void RemapOverride (dynamic_bitset& override, const TypeTree& oldTypeTree, const TypeTree& newTypeTree)
{
	if (override.size () == 0)
		return;
	
	dynamic_bitset oldOverride;
	oldOverride.swap (override);
	override.clear ();
	override.resize (CountTypeTreeVariables (newTypeTree), false);
	InitOverrideVariableRecursive (override, newTypeTree);
	
	int oldTypeTreeCount = CountTypeTreeVariables (oldTypeTree);
	if (oldOverride.size () == oldTypeTreeCount)
		RemapOverrideRecurse (oldTypeTree, newTypeTree, oldOverride, override);
	
	ApplySpecialPrefabBackwardsOverride (oldTypeTree, newTypeTree, oldOverride, override);
}

void RemapOverride (EditorExtension& object, const TypeTree& oldTypeTree)
{
	TypeTree newTypeTree;
	GenerateTypeTree (object, &newTypeTree);
	
	EditorExtensionImpl* extension = GetDeprecatedExtensionPtrIfExists (object);
	if (extension)
	{
		RemapOverride (extension->m_OverrideVariable, oldTypeTree, newTypeTree);
	}
}


void ReadOldRootGameObjectOverrides (GameObject* rootGameObject, Prefab& prefab)
{
	if (prefab.IsPrefabParent())
		return;
	if (rootGameObject == NULL || rootGameObject->QueryComponent(Transform) == NULL)
		return;
		
	prefab.m_Modification.m_TransformParent = rootGameObject->GetComponent(Transform).GetParent();
}

///@TODO: After merge need to go over GarbageCollectUnused function and remove all the editorextension impl crap
///        -> Unloading should probably ignore any prefab dependencies!

void ReadOldPrefabFormat (Prefab& prefab, EditorExtension& targetObject, EditorExtension& prefabParentObject, EditorExtensionImpl& impl)
{
	if (impl.m_OverrideVariable.size() == 0)
		return;
	
	dynamic_bitset overrides = impl.m_OverrideVariable;
	dynamic_array<UInt8> state(kMemTempAlloc);
	TypeTree typeTree;
	
	WriteObjectToVector (targetObject, &state, kSerializeForPrefabSystem);
	GenerateTypeTree (targetObject, &typeTree, kSerializeForPrefabSystem);
	
	if (targetObject.GetNeedsPerObjectTypeTree())
	{
		RemapOverride (overrides, impl.m_LastMergedTypeTree, typeTree);
	}
	else
	{
		TypeTree fullTypeTree;
		GenerateTypeTree (targetObject, &fullTypeTree);
		RemapOverride (overrides, fullTypeTree, typeTree);
	}
	
	PropertyModifications newProperties;
	GeneratePropertyDiffBackwardsCompatible(typeTree, overrides, state, newProperties);
	
	// Merge it into the existing properties array
	for (int j=0;j<newProperties.size();j++)
	{
		PropertyModification& modification = newProperties[j];
		// Active property is not stored in overrides by default
		if (targetObject.GetClassID() == ClassID (GameObject) && modification.propertyPath == "m_IsActive" && modification.value == "1")
		{	
			continue;
		}
		
		// Inject property modification
		modification.target = &prefabParentObject;
		InsertPropertyModification(modification, prefab.m_Modification.m_Modifications);
	}
}

void RemapOldPrefabOverrideFromLoading (Object& object, const TypeTree& oldTypeTree)
{
	EditorExtension* extended = dynamic_pptr_cast<EditorExtension*> (&object);
	if (extended != NULL)
	{
		AssertIf (object.GetNeedsPerObjectTypeTree ());
		RemapOverride (*extended, oldTypeTree);
	}
}

void UpgradeDeprecatedObjectSetAndOverrides (Prefab& prefab)
{
	if (prefab.m_DeprecatedObjectsSet.empty())
		return;
	
	Assert(prefab.m_RootGameObject.IsNull());
	
	vector<GameObject*> roots = OldPrefabObjectContainerToRootGameObjects (prefab.m_DeprecatedObjectsSet);
	prefab.m_DeprecatedObjectsSet.clear();
	
	if (roots.size () == 1)
	{	
		prefab.m_RootGameObject = roots[0];
	}
	else if (roots.size () > 1)
	{
		prefab.m_RootGameObject = roots[0];
		Transform* rootTransform = roots[0]->QueryComponent(Transform);
		
		for(int i=1;i<roots.size();i++)
		{
			Transform* transform = roots[i]->QueryComponent(Transform);
			transform->SetParent(rootTransform, Transform::kAllowParentingFromPrefab);
		}
	}
	
	ReadOldRootGameObjectOverrides (prefab.m_RootGameObject, prefab);
}

std::vector<GameObject*> OldPrefabObjectContainerToRootGameObjects (const Prefab::DeprecatedObjectContainer& objects)
{
	vector<GameObject*> roots;
	Prefab::DeprecatedObjectContainer::const_iterator i;
	for (i=objects.begin();i != objects.end();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go == NULL)
			continue;
		Transform* transform = go->QueryComponent(Transform);
		if (transform == NULL || transform->GetParent() == NULL || objects.count(transform->GetParent()) == 0)
			roots.push_back(go);
	}

	return roots;
}
