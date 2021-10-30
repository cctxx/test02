#include "PrefabMerging.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "PrefabUtility.h"
#include "GenerateCachedTypeTree.h"

using namespace std;

static void MatchComponents (map<Object*, Object*>& replaceWithMap, set<Object*>& usedDataTemplateObjects);

// Write test that checks that if you have two components of same type, they will be matched by the order in which they are added

static void BuildReplaceMapByGameObjectName (GameObject& rootInstanceGameObject, const std::vector<Object*>& existingPrefabObjects, std::vector<ObjectData>& instanceObjects)
{
	multimap<string, GameObject*> dstGameObjects;
	multimap<string, GameObject*>::iterator found;
	pair<multimap<string, GameObject*>::iterator, multimap<string, GameObject*>::iterator> range;
	set<Object*> usedPrefabObjects;
	map<Object*, Object*> replaceWithMap;
	
	for (int i=0;i<existingPrefabObjects.size();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (existingPrefabObjects[i]);
		if (go)
		{
			string name = go->GetName ();
			if (IsTopLevelGameObject (*go))
				name = "/Root GameObject";

			dstGameObjects.insert (make_pair (name, go));
//			bool inserted = dstGameObjects.insert (make_pair (name, go)).second;
//			if (!inserted)
//				ScriptWarning ("GameObject name " + string(go->GetName ()) + " in prefab is not unique twice in building template replace map!");
		}
	}
	
	for (int i=0;i<instanceObjects.size();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (instanceObjects[i].objectInstance);
		if (go == NULL)
			continue;

		string name = go->GetName ();
			
		if (&rootInstanceGameObject == go)
			name = "/Root GameObject";
		
		// Find matching game object by name
		range = dstGameObjects.equal_range(name);
		for (found=range.first; found != range.second;found++)
		{
			GameObject& prefabGO = *found->second;
			if (usedPrefabObjects.count(&prefabGO) == 0)
			{
				replaceWithMap.insert(make_pair(go, &prefabGO));
				usedPrefabObjects.insert(&prefabGO);
			}
		}

/////@TODO: WRITE SOME TESTS FOR THE WARNINGS WE GET FROM REPLACING WITH SAME NAME
//			bool inserted = srcGameObjects.insert (make_pair (name, go)).second;
//			if (!inserted)
//				ScriptWarning ("GameObject name " + std::string(go->GetName ()) + " is not unique. Matching the prefab by name failed.");
	}
	
	MatchComponents (replaceWithMap, usedPrefabObjects);
	
	for (int i=0;i<instanceObjects.size();i++)
	{
		map<Object*, Object*>::iterator foundPrefabObject = replaceWithMap.find(instanceObjects[i].objectInstance);
		if (foundPrefabObject != replaceWithMap.end())
			instanceObjects[i].prefabParent = foundPrefabObject->second;
		else
			instanceObjects[i].prefabParent = NULL;
	}
	
	// This shouldn't be necessary since all code above should detected gainst duplicated use of a prefab parent.
	// So just in case, because it will crash if there is a incorrect parent mapping.
	if (ClearDuplicatePrefabParentObjects(instanceObjects))
	{
		AssertString("Prefab replace internal failure");
	}
}

inline bool IsComponentSimilar (const Unity::Component& lhs, const Unity::Component& rhs)
{
	if (lhs.GetClassID () != rhs.GetClassID ())
		return false;
	
	const MonoBehaviour* pythonLhs = dynamic_pptr_cast<const MonoBehaviour*> (&lhs);
	if (pythonLhs)
	{
		const MonoBehaviour* pythonRhs = dynamic_pptr_cast<const MonoBehaviour*> (&rhs);
		AssertIf (pythonRhs == NULL);
		if (pythonLhs->GetScript () != pythonRhs->GetScript ())
			return false;
	}
	
	return true;	
}

Unity::Component* FindSimilarComponent (GameObject& go, const Unity::Component& component, const set<Object*>& usedDataTemplateObjects)
{
	for (int i=0;i<go.GetComponentCount ();i++)
	{
		Unity::Component& curCom = go.GetComponentAtIndex (i);
		if (usedDataTemplateObjects.count (&curCom) == 0 && IsComponentSimilar (curCom, component))
			return &curCom;
	}
	return NULL;
}


static void MatchComponents (map<Object*, Object*>& replaceWithMap, set<Object*>& usedDataTemplateObjects)
{
	for (map<Object*, Object*>::iterator i=replaceWithMap.begin ();i != replaceWithMap.end ();i++)
	{
		GameObject* originalGO = dynamic_pptr_cast<GameObject*> (i->first);
		if (originalGO == NULL)
			continue;
			
		GameObject& prefabGO = *dynamic_pptr_cast<GameObject*> (i->second);
		AssertIf (&prefabGO == NULL);
		
		for (int j=0;j<originalGO->GetComponentCount ();j++)
		{
			Unity::Component& originalComponent = originalGO->GetComponentAtIndex (j);

			// Only if we arent already in replaceWithMap
			if (replaceWithMap.find (&originalComponent) == replaceWithMap.end ())
			{
				// Find similar components
				Unity::Component* matchingDataTemplateCom = FindSimilarComponent (prefabGO, originalComponent, usedDataTemplateObjects);
				if (matchingDataTemplateCom)
				{
					replaceWithMap.insert (make_pair (&originalComponent, matchingDataTemplateCom));
					usedDataTemplateObjects.insert (matchingDataTemplateCom);
				}
			}
		}
	}
}

void ReplaceGameObjectAndTransformProperties(const vector<ObjectData>& objectData)
{
	// Assign all game object data 
	vector<ObjectData> inverted = objectData;
	for (int i=0;i<inverted.size();i++)
		swap (inverted[i].prefabParent, inverted[i].objectInstance);
	InheritGameObjectAndTransformProperties (inverted);
}

class RemapFunctor : public GenerateIDFunctor
{
public:
	typedef std::map<SInt32, SInt32> IDRemap;
	const IDRemap& remap;
	
	RemapFunctor (const IDRemap& inRemap) : remap (inRemap) { }
	
	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlags = kNoTransferFlags)
	{
		IDRemap::const_iterator found = remap.find (oldInstanceID);
		// No Remap found -> set zero or dont touch instanceID
		if (found == remap.end ())
		{
			AssertIf (metaFlags & kStrongPPtrMask);
			
			if (GetPersistentManager ().GetPathName (oldInstanceID).empty ())
				return 0;
			
			return oldInstanceID;
		}
		// Remap
		else
			return found->second;
	}
};

void RemapPPtrsAndClearSceneReferences (Object& o, const std::map<SInt32, SInt32>& remap, int flags)
{
	RemapFunctor functor (remap);
	RemapPPtrTransfer transferFunction (flags, true);
	transferFunction.SetGenerateIDFunctor (&functor);
	o.VirtualRedirectTransfer (transferFunction);
}

void ReplacePrefab (GameObject& rootInstanceGameObject, const std::vector<ObjectData>& instanceObjects, const std::vector<ObjectData>& additionalInstantiatedAssets, const std::vector<Object*>& existingPrefabObjects, std::vector<ObjectData>& outputPrefabObjects, ReplacePrefabOptions mode)
{
	Assert(outputPrefabObjects.empty());
	
	// Find all ptrs to objects we can reuse instead of deleting old and creating new ones (Which would kill the overrides of the derived templates)

	outputPrefabObjects = instanceObjects;
	
	
	if (mode & kReplacePrefabNameBased)
		BuildReplaceMapByGameObjectName (rootInstanceGameObject, existingPrefabObjects, outputPrefabObjects);

	set<PPtr<Object> > toBeDestroyedObjects (existingPrefabObjects.begin(), existingPrefabObjects.end());


	// Produce, if possible reuse, all objects that will be in the prefab
	map<SInt32, SInt32> ptrs;
	for (int i=0;i<outputPrefabObjects.size();i++)
	{
		// Produce the objects which we don't already have in the prefab
		ObjectData& cur = outputPrefabObjects[i];
		if (cur.prefabParent == NULL)
		{
			cur.prefabParent = Object::Produce (cur.objectInstance->GetClassID ());
			cur.prefabParent->Reset();
		}
		
		// Keep list of objects we need to remove in sync
		toBeDestroyedObjects.erase (cur.prefabParent);

		ptrs.insert(make_pair(cur.objectInstance->GetInstanceID(), cur.prefabParent->GetInstanceID()));
	}
	
	// Inject all instantiated assets into the remap ptrs,
	// so that they will get remapped to the uploaded material instead
	for (int i=0;i<additionalInstantiatedAssets.size();i++)
	{
		const ObjectData& cur = additionalInstantiatedAssets[i];
		Assert (cur.prefabParent != NULL && cur.objectInstance != NULL);
		ptrs.insert(make_pair(cur.objectInstance->GetInstanceID(), cur.prefabParent->GetInstanceID()));
		toBeDestroyedObjects.erase (cur.prefabParent);
	}
	
	// Clean up objects that should no longer be in the prefab
	DestroyPrefabObjects (toBeDestroyedObjects);

	ReplaceGameObjectAndTransformProperties(outputPrefabObjects);

	// Apply instance data to prefab parent
	vector<ObjectData> prefabObjectsAndAdditionalAssets = outputPrefabObjects;
	prefabObjectsAndAdditionalAssets.insert(prefabObjectsAndAdditionalAssets.end(), additionalInstantiatedAssets.begin(), additionalInstantiatedAssets.end());

	dynamic_array<UInt8> instanceData(kMemTempAlloc);
	for (int i=0;i<prefabObjectsAndAdditionalAssets.size();i++)
	{
		ObjectData& cur = prefabObjectsAndAdditionalAssets[i];
		
		string originalName = cur.prefabParent->GetName();
		
		// Apply data from instance to prefab
		WriteObjectToVector (*cur.objectInstance, &instanceData, kSerializeForPrefabSystem);
		ReadObjectFromVector (cur.prefabParent, instanceData, kSerializeForPrefabSystem);

		// Remap references to be relative to the prefab group of objects
		// Objects in a prefab are not allowed to have references to scene objects, so null them
		Object& prefabObject = *cur.prefabParent;
		RemapPPtrsAndClearSceneReferences (prefabObject, ptrs, kSerializeForPrefabSystem);
		
		// Keep name from prefab instead of from instance.
		// Since the uploaded object is an asset and thus the name will revert when importing.
		bool isAdditionalInstantiatedAsset = i >= outputPrefabObjects.size();
		if (isAdditionalInstantiatedAsset)
			cur.prefabParent->SetName(originalName.c_str());
	}
}
