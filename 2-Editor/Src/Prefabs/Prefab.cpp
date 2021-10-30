#include "UnityPrefix.h"
#include "Prefab.h"
#include "AttachedPrefabAsset.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "PrefabReplacing.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/SceneInspector.h"
#include "PrefabBackwardsCompatibility.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/Utilities/ArrayUtility.h"

///@TODO: Tests: See if we have some code coverage for GetPrefabType
///@TODO: Test save a prefab to disk, modify it, save it again, then replace and see if the prefab instance changes as a result of simply replacing the prefab file
///@TODO: Write test for behaviour when destroying a game object that kills the entire prefab instance? (I removed the old code that handles this!)

///@TODO: WRite test for preventing setting a parent of a prefabbed object.

/// When performing backwards compatilbity, make sure the source prefab is not automatically written even though we delete editorextensionimpl

///@TODO: Test 1. Create prefab 2. Save empty prefab state. 3. make prefab instance etc. 4. Replace file with empty prefab again. Ensure that the  5. Replace with full prefab. ensure we get the prefab game objects back.
///@TODO: Test where you temporarily delete the parent prefab, then get it back. What happens to changes that were done in the mean time. Probably will get reverted, which is ok.

// @TODO: Test for backwards compatibility with multiple root game objects in old prefab format

/////@TODO: 1. Create prefab. 2. modify it on disk (dont refresh assets) 3. Replace prefab -> We should get the result of the call to Replace prefab instead of whatever is stored on disk.

/////@TODO: From OnDisable Destroy a component from a script that is in the prefab. Then Perform a MergePrefab operation. See if we crash. (Deactivate call is surrounded by direct pointers!)

/////@TODO: Test for assigning a Object reference of an incorrect type (For example: Game Object on Camera.m_RenderTexture)


//1. Create prefab with cross reference monobehaviour 
//2. Assign the cross reference to itself
//3. In the instance change it to point to the other prefab instance
//4. Ensure that it shows up as a prefab difference

///****** MAJOR *********
//  * Replacing a prefab  with a completely different instance, does not keep the root transform. This is bad because root position & rotation is not maintained.


IMPLEMENT_CLASS_HAS_INIT(Prefab)
IMPLEMENT_OBJECT_SERIALIZE(Prefab)


static void MergePrefabInternal (Prefab& parentPrefab, Prefab& childPrefab, const vector<ObjectData>& instanceObjects, const vector<Object*>& addedObjects, AwakeFromLoadQueue& awakeFromLoadQueue, std::vector<ObjectData>& output);
static bool IsRootOrDirectChildOfRootGameObject (Object* targetObject);
static bool IsRootGameObjectOrTransformInPrefabInstance (Object* targetObject, const std::vector<ObjectData>& instanceObjects);
static void GetObjectDataArrayFromGameObjectHierarchyWithPrefabParent (GameObject& root, Prefab& prefab, std::vector<ObjectData>& instanceObjects);
static void GetObjectDataArrayFromPrefabRoot (Prefab& prefab, std::vector<ObjectData>& instanceObjects, vector<Object*>& addedObjects);

void Prefab::InitializeClass ()
{
	RegisterAllowNameConversion("Prefab", "m_Father", "m_ParentPrefab");
	RegisterAllowNameConversion("Prefab", "m_IsDataTemplate", "m_IsPrefabParent");
	
	RegisterAddComponentCallback (AddComponentToPrefabParentObjectNotification);
}


Prefab::Prefab (MemLabelId label, ObjectCreationMode mode)
	: Super (label, mode)
{
	m_IsPrefabParent = false;
	m_IsExploded = true;
}

Prefab::~Prefab ()
{
	
}

template<class T>
void Prefab::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	
	transfer.SetVersion(2);
	
	transfer.Transfer(m_Modification, "m_Modification", kHideInEditorMask);
	transfer.Transfer(m_ParentPrefab, "m_ParentPrefab", kHideInEditorMask);
	transfer.Transfer(m_RootGameObject, "m_RootGameObject", kHideInEditorMask);
	transfer.Transfer(m_IsPrefabParent, "m_IsPrefabParent", kHideInEditorMask);
	transfer.Transfer(m_IsExploded, "m_IsExploded", kHideInEditorMask);
	transfer.Align();
	
	if (transfer.IsVersionSmallerOrEqual (1))
	{
		transfer.Transfer(m_DeprecatedObjectsSet, "m_Objects");
	}
}


void Prefab::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	
	PatchPrefabBackwardsCompatibility ();
}

void Prefab::PatchPrefabBackwardsCompatibility ()
{
	// We have loaded an old prefab format, convert data to new format
	if (!m_DeprecatedObjectsSet.empty())
		UpgradeDeprecatedObjectSetAndOverrides(*this);
	
}

void Prefab::GetPropertyModificationsForObject(PPtr<Object> target, PropertyModifications& modifications) const
{
	for (int i = 0; i < m_Modification.m_Modifications.size(); i++)
	{
		if (m_Modification.m_Modifications[i].target == target)
		{
			modifications.push_back(m_Modification.m_Modifications[i]);
		}
	}
}

GameObject* CreatePrefab (const std::string& path, GameObject& gameObjectInstance, ReplacePrefabOptions options)
{
	///@TODO: Currently this will first create an asset (write it)
	//        Then replace it, then modify it again. This means that on disk we will have an empty prefab until we save the prefab and we'll write twice...
	
	Prefab* prefab = CreateEmptyPrefab (path);
	if (prefab == NULL)
		return NULL;
	
	return ReplacePrefab(gameObjectInstance, prefab, options);
}

Prefab* CreateEmptyPrefab (const std::string& path)
{
	if (GetPathNameExtension(path) != "prefab")
	{
		ErrorString("Create prefab path must use .prefab extension");
		return NULL;
	}

	Prefab& prefab = *CreateObjectFromCode<Prefab> ();
	// Make super template
	prefab.m_IsPrefabParent  = true;	
	
	PPtr<Prefab> result = &prefab;
	if (!AssetInterface::Get ().CreateSerializedAsset (prefab, path, AssetInterface::kWriteAndImport | AssetInterface::kDeleteExistingAssets))
	{
		DestroySingleObject(&prefab);
		return NULL;
	}
	
	return result;
}

void SetReplacePrefabHideFlags (Object& object)
{
	if (IsRootOrDirectChildOfRootGameObject (&object))
		object.SetHideFlagsObjectOnly(0);
	else
		object.SetHideFlagsObjectOnly(Object::kHideInHierarchy);
}

GameObject* ReplacePrefab (GameObject& originalGameObject, Object* targetObjectInPrefab, ReplacePrefabOptions options)
{
	// Find the prefab into which we are going to upload
 	Prefab* targetPrefab = GetPrefabFromAnyObjectInPrefab (targetObjectInPrefab);

	if (targetPrefab == NULL)
	{
		ErrorString ("The object you are trying to replace does not exist or is not a prefab.");
		return NULL;
	}

	if (!targetPrefab->IsPrefabParent())
	{
		ErrorStringObject ("The prefab you are trying to replace is not a prefab parent but a prefab instance. Please use PrefabUtility.GetPrefabParent().", targetPrefab->GetRootGameObject());
		return NULL;
	}

#if ENABLE_EDITOR_HIERARCHY_ORDERING
	Transform* trans = originalGameObject.QueryComponent(Transform);
	if (trans)
		trans->OrderChildrenRecursively();
#endif

	vector<ObjectData> instanceObjects;
	GetObjectDataArrayFromGameObjectHierarchyWithPrefabParent(originalGameObject, *targetPrefab, instanceObjects);

	vector<ObjectData> additionalAssetObjects;
	GetInstantiatedAssetsToObjectArray (instanceObjects, additionalAssetObjects);
	
	vector<Object*> prefabObjects;
	GetObjectArrayFromPrefabRoot(*targetPrefab, prefabObjects);

	vector<ObjectData> outputObjects;
	ReplacePrefab(originalGameObject, instanceObjects, additionalAssetObjects, prefabObjects, outputObjects, options);
	
	targetPrefab->m_RootGameObject = NULL;
	for (int i=0;i<outputObjects.size();i++)
	{
		EditorExtension* prefabParentObject = dynamic_pptr_cast<EditorExtension*> (outputObjects[i].prefabParent);
		if (prefabParentObject == NULL)
			continue;
		
		AssertIf(prefabParentObject->m_Prefab.IsValid() && prefabParentObject->m_Prefab != PPtr<Prefab> (targetPrefab));
		
		// Add to targetPrefab object list
		prefabParentObject->m_Prefab = targetPrefab;
		
		if (outputObjects[i].objectInstance == &originalGameObject)
			targetPrefab->m_RootGameObject = dynamic_pptr_cast<GameObject*> (outputObjects[i].prefabParent);
		
		// Setup connection from prefab instance to prefab parent
		if ((options & kAutoConnectPrefab) != 0)
		{
			EditorExtension* connectInstanceObject = dynamic_pptr_cast<EditorExtension*> (outputObjects[i].objectInstance);
			if (connectInstanceObject)
				connectInstanceObject->m_PrefabParentObject = prefabParentObject;
		}
		
		// Make the object persistent, if it isn't already persistent
		if ((options & kDontMakePersistentOrReimport) == 0)
			MakePrefabObjectPersistent (*targetPrefab, *prefabParentObject);
		
		SetReplacePrefabHideFlags(*prefabParentObject);
	}

	Assert(targetPrefab->m_RootGameObject.IsValid());
	
	for (int i=0;i<additionalAssetObjects.size();i++)
	{
		EditorExtension* prefabParentObject = dynamic_pptr_cast<EditorExtension*> (outputObjects[i].prefabParent);
		if (prefabParentObject == NULL)
			continue;
		
		AssertIf(prefabParentObject->m_Prefab.IsValid() && prefabParentObject->m_Prefab != PPtr<Prefab> (targetPrefab));
		
		// Setup connection from prefab instance to prefab parent
		if ((options & kAutoConnectPrefab) != 0)
		{
			EditorExtension* connectInstanceObject = dynamic_pptr_cast<EditorExtension*> (outputObjects[i].objectInstance);
			if (connectInstanceObject)
				connectInstanceObject->m_PrefabParentObject = prefabParentObject;
		}
	}
	
	// If the replaced prefab contains anything, hide the prefab (We show only the root game object instead)
	if (outputObjects.empty())
		targetPrefab->SetHideFlagsObjectOnly(0);
	else
		targetPrefab->SetHideFlagsObjectOnly(Object::kHideInHierarchy);
	
	targetPrefab->SetDirty();

	DirtyAndAwakeFromLoadAfterMerge(outputObjects, false);

	string assetPath = GetAssetPathFromObject(targetPrefab); 
	
	// The name of the prefab root object has to be the name in the asset
	// Otherwise ConnectToPrefab will not record the change, since it will get changed in ImportAtPath
	if (targetPrefab->m_RootGameObject.IsValid())
	{
		targetPrefab->m_RootGameObject->SetNameCpp(GetMainAssetNameFromAssetPath(assetPath));
	}
	

	if (options & kAutoConnectPrefab)
	{
		RevertInstantiatedAssetReferencesToParentObject(outputObjects);
		ConnectToPrefab(originalGameObject, targetPrefab);
	}
	
	/////@TODO: We have to prevent that the prefab was also modified on disk!
	////////// otherwise native format importer will reload from disk and then later crash i think.
	
	// Make sure prefab representation in project window is up to date
	if ((options & kDontMakePersistentOrReimport) == 0)
	{
		AssetInterface::Get().ImportAtPath (assetPath);

		// Make sure that we get a repaint because prefab overrides don't automatically cause a repaint
		GetSceneTracker().ForceReloadInspector();
	}	
	
	GameObject* result = dynamic_pptr_cast<GameObject*> (FindPrefabFromInstanceObject(originalGameObject, outputObjects));
	Assert(!result->IsActive());
	return result;
}

void MakePrefabObjectPersistent (Prefab& prefab, Object& object)
{
	enum { kMaxObjectsPerClassID = 100000 };

	if (object.IsPersistent ())
		return;
	
	// Find fileID and Make persistent
	string pathName = GetPersistentManager ().GetPathName (prefab.GetInstanceID ());
	AssertIf (pathName.empty ());

	int classID = object.GetClassID ();
	LocalIdentifierInFileType firstPossibleFileID = kMaxObjectsPerClassID * classID;
	LocalIdentifierInFileType lastPossibleFileID = kMaxObjectsPerClassID * classID + kMaxObjectsPerClassID - 1;
	LocalIdentifierInFileType i;
	for (i=firstPossibleFileID;i < lastPossibleFileID;i += 2)
	{
		int instanceID = GetPersistentManager ().GetInstanceIDFromPathAndFileID (pathName, i);
		if (!PPtr<Object> (instanceID))
			break;
	}
	// Too many objects in there
	ErrorIf (i >= lastPossibleFileID);

	GetPersistentManager ().MakeObjectPersistentAtFileID (object.GetInstanceID (), i, pathName);
}

static void AwakePrefabLoadQueue (AwakeFromLoadQueue& queue)
{
	queue.CheckConsistency();
	queue.AwakeFromLoad(kDefaultAwakeFromLoad);
}

void MergeAllPrefabInstances (AwakeFromLoadQueue* awakeFromLoadQueue)
{
	awakeFromLoadQueue->PatchPrefabBackwardsCompatibility();
	
	// When no queue is passed just use a temporary one
	AwakeFromLoadQueue tempQueue (kMemSerialization);
	
	// Collect all prefab parents
	vector<Prefab*> prefabs;
	
	dynamic_array<PPtr<Object> > allObjects;
	awakeFromLoadQueue->ExtractAllObjects(allObjects);
	for (int i=0;i<allObjects.size();i++)
	{
		Prefab* prefab = dynamic_pptr_cast<Prefab*> (allObjects[i]);
		if (prefab)
			prefabs.push_back(prefab);
	}
	
	for (int i=0;i<prefabs.size();i++)
	{
		// Check that we have a prefab instance and that we have a valid prefab parent!
		Prefab& prefabInstance = *prefabs[i];
		if (prefabInstance.IsPrefabParent())
			continue;

		// Prefab parent has gone missing, but it might come back later etc..
		Prefab* prefabParent = prefabInstance.m_ParentPrefab;
		if (prefabParent == NULL || prefabParent->GetRootGameObject().IsNull())
			continue;

		vector<ObjectData> instanceObjects;
		vector<ObjectData> output;
		vector<Object*> addedComponents;
		GetObjectDataArrayFromPrefabRoot(prefabInstance, instanceObjects, addedComponents);
		MergePrefabInternal(*prefabParent, prefabInstance, instanceObjects, addedComponents, tempQueue, output);
	}
	
	// Automatically process Awakefromload queue
	// Otherwise the Queue will be processed from outside
	awakeFromLoadQueue->InsertAwakeFromLoadQueue (tempQueue, kDefaultAwakeFromLoad);
}


void MergeAllPrefabInstances (PPtr<Prefab> prefab, AwakeFromLoadQueue& awakeFromLoadQueue)
{
	// Update last merge identifier
	//superTemplate.m_LastMergeIdentifier.Init ();
	
	vector<Prefab*> prefabInstances;
	CalculateAllLoadedPrefabChildren(prefab, prefabInstances);
	
	for (int i=0;i<prefabInstances.size();i++)
	{
		Prefab* prefabInstance = prefabInstances[i];
		if (prefabInstance)
		{
			MergePrefabInstance(*prefabInstance, awakeFromLoadQueue);
		}
	}
}

void MergeAllPrefabInstances (PPtr<Prefab> prefab)
{
	AwakeFromLoadQueue awakeFromLoadQueue (kMemTempAlloc);
	MergeAllPrefabInstances (prefab, awakeFromLoadQueue);

	AwakePrefabLoadQueue(awakeFromLoadQueue);
}

void SetPropertyModifications (Prefab& prefab, PropertyModifications& modifications)
{
	prefab.m_Modification.m_Modifications = modifications;
	prefab.SetDirty();
	
	MergePrefabInstance(prefab);
}

void MergePrefabInstance (Prefab& prefabInstance, AwakeFromLoadQueue& awakeFromLoadQueue)
{
	Prefab* superPrefab = prefabInstance.m_ParentPrefab;
	if (superPrefab)
	{
		Assert(superPrefab->IsPrefabParent());
		Assert(!prefabInstance.IsPrefabParent());
		
		vector<ObjectData> output;
		MergePrefab(*superPrefab, prefabInstance, awakeFromLoadQueue, output);
	}
}

void MergePrefabInstance (Prefab& prefabInstance)
{
	AwakeFromLoadQueue awakeFromLoadQueue (kMemSerialization);
	MergePrefabInstance(prefabInstance, awakeFromLoadQueue);
	AwakePrefabLoadQueue(awakeFromLoadQueue);
}


void DisconnectPrefabInstance (Object* prefabObject)
{
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(prefabObject);
	if (prefab == NULL)
		return;
	
	if (IsPrefabInstanceOrDisconnectedInstance(prefab))
	{
		DestroyObjectHighLevel (prefab);
		GetSceneTracker().DirtyTransformHierarchy();
		GetSceneTracker().ForceReloadInspector();
	}
	else
	{
		ErrorStringObject("DisconnectPrefabInstance can only be called on prefab instances", prefabObject);
	}
}

void AddComponentToPrefabParentObjectNotification (Unity::Component& component)
{
	GameObject& go = component.GetGameObject();
	
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(&go);
	if (prefab == NULL || !prefab->IsPersistent())
		return;
	
	Assert(!go.TestHideFlag(Object::kNotEditable));
	Assert(!component.IsPersistent());
	
	component.m_Prefab = prefab;
	MakePrefabObjectPersistent(*prefab, component);
	SetReplacePrefabHideFlags(component);
}
 
void PrefabDestroyObjectCallback (Object& targetObject)
{
	if (!IsPrefabInstanceOrDisconnectedInstance(&targetObject))
		return;
	
	Prefab* prefabInstance = GetPrefabFromAnyObjectInPrefab(&targetObject);
	Object* prefabParent = GetPrefabParentObject(&targetObject);

	if (dynamic_pptr_cast<Unity::Component*> (&targetObject) != NULL)
	{
		PrefabModification::RemovedComponents& removedComponents = prefabInstance->m_Modification.m_RemovedComponents;
		if (find(removedComponents.begin(), removedComponents.end(), PPtr<Object>(prefabParent)) == removedComponents.end())
			removedComponents.push_back(PPtr<Object>(prefabParent));
		prefabInstance->SetDirty();
	}
	else
	{
		DisconnectPrefabInstance(&targetObject);
	}	
}


const char* kDefaultTransformOverrides[] = { "m_LocalPosition.x", "m_LocalPosition.y", "m_LocalPosition.z", "m_LocalRotation.x", "m_LocalRotation.y", "m_LocalRotation.z", "m_LocalRotation.w" };
const char* kGameObjectNameProperty = { "m_Name" };

static void RevertAllButDefaultPrefabOverrides (Prefab& prefab)
{
	
	const PropertyModifications& oldModifications = prefab.GetPropertyModifications();
	PropertyModifications clearedModifications;
	
	PPtr<Object> rootTransform;
	PPtr<Object> rootGO;
	if (prefab.m_RootGameObject.IsValid())
	{
		rootTransform = GetPrefabParentObject(prefab.m_RootGameObject->QueryComponent(Transform));
		rootGO = GetPrefabParentObject(prefab.m_RootGameObject);
	}	
	
	// Remove all but -> Transform position & rotation on the root and game object name
	if (rootTransform.IsValid() && rootGO.IsValid())
	{
		for (int i=0;i<oldModifications.size();i++)
		{
			if (oldModifications[i].target == rootTransform)
			{
				for (int o=0;o<ARRAY_SIZE(kDefaultTransformOverrides);o++)
				{
					if (oldModifications[i].propertyPath == kDefaultTransformOverrides[o])
						clearedModifications.push_back(oldModifications[i]);
				}
			}
			
			if (oldModifications[i].target == rootGO)
			{
				if (oldModifications[i].propertyPath == kGameObjectNameProperty)
					clearedModifications.push_back(oldModifications[i]);
			}
		}
	}
	
	SetPropertyModifications(prefab, clearedModifications);
}


static void AddDefaultRootTransformModifications (Prefab& prefab)
{
	if (!prefab.m_RootGameObject.IsValid())
		return;
	Transform* rootTransform = prefab.m_RootGameObject->QueryComponent(Transform);
	if (rootTransform == NULL)
		return;
	
	Object* parentTransform = GetPrefabParentObject(rootTransform);
	if (parentTransform == NULL)
		return;
	
	PropertyModifications& modifications = prefab.m_Modification.m_Modifications;
	Assert(modifications.empty());
	
	for (int i=0;i<ARRAY_SIZE(kDefaultTransformOverrides);i++)
		modifications.push_back(CreatePropertyModification(kDefaultTransformOverrides[i], "", parentTransform));
	
	// Grab the actual values from the parent prefab root transform
	RecordPrefabInstancePropertyModifications (*rootTransform);
}

void DisconnectAllPrefabInstances ()
{
	// Get all datatemplates and add their fathers to the toBeMergedSuperTemplates list
	vector<Prefab*> prefabs;
	Object::FindObjectsOfType (&prefabs);
	for (int i=0;i<prefabs.size ();i++)
	{
		Prefab& prefab = *prefabs[i];
		Prefab* superPrefab = prefab.m_ParentPrefab;
		if (superPrefab)
		{
			Assert(!prefab.IsPrefabParent());
			Assert(!prefab.IsPersistent());
			DisconnectPrefabInstance (&prefab);
		}
	}
}

void ConnectToPrefab (GameObject& sourceObject, Object* prefabObject)
{
	// Find super Prefab
	Prefab* superPrefab = GetPrefabFromAnyObjectInPrefab(prefabObject);
	if (superPrefab == NULL || !superPrefab->IsPrefabParent())
	{
		ErrorString("Prefab to connect to is not a prefab.");
		return;
	}
	
	// Create empty prefab
	Prefab& prefabInstance = *NEW_OBJECT(Prefab);
	prefabInstance.Reset();
	prefabInstance.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	prefabInstance.m_ParentPrefab = superPrefab;
	
	std::vector<ObjectData> prefabInstanceObjects;
	GetObjectDataArrayFromGameObjectHierarchyWithPrefabParent(sourceObject, *superPrefab, prefabInstanceObjects);

	// Record any prefab instance modifications. To do this we need to have all m_Prefab pointers hooked up already
	// * First setup m_Prefab on all instances. (This is necessary for RecordPrefabInstancePropertyModifications to work correctly)
	for (int i=0;i<prefabInstanceObjects.size();i++)
	{
		EditorExtension* curInstance = dynamic_pptr_cast<EditorExtension*> (prefabInstanceObjects[i].objectInstance);
		if (curInstance)
		{
			// Disconnect old prefab
			DisconnectPrefabInstance(curInstance);
			
			// Assign new prefab
			Assert(!curInstance->m_Prefab.IsValid());
			curInstance->m_Prefab = &prefabInstance;
		}
	}
	//* root game object also needs to be setup for RecordPrefabInstancePropertyModifications to work correctly
	prefabInstance.m_RootGameObject = &sourceObject;
	
	// Keep default modifications overridden
	AddDefaultRootTransformModifications(prefabInstance);

	// * Compare against parent prefab
	for (int i=0;i<prefabInstanceObjects.size();i++)
	{
		if (prefabInstanceObjects[i].objectInstance != NULL && prefabInstanceObjects[i].prefabParent != NULL)
			RecordPrefabInstancePropertyModifications(*prefabInstanceObjects[i].objectInstance);
	}
	
	///@TODO: Handle added components, handle nested child objects
	
	// Merge prefab
	vector<ObjectData> output;
	vector<Object*> addedObjects;
	AwakeFromLoadQueue awakeFromLoadQueue (kMemSerialization);
	DeactivateGameObjectPrefabInstances(prefabInstanceObjects);
	MergePrefabInternal(*superPrefab, prefabInstance, prefabInstanceObjects, addedObjects, awakeFromLoadQueue, output);
	AwakePrefabLoadQueue(awakeFromLoadQueue);
}

bool IsPrefabEmpty (Prefab& prefab)
{
	return !prefab.m_RootGameObject.IsValid();
}

Prefab* GetPrefabFromAnyObjectInPrefab (Object* sourceObject)
{
	// Find super Prefab
	EditorExtension* prefabEx = dynamic_pptr_cast<EditorExtension*> (sourceObject);
	if (prefabEx)
		return prefabEx->m_Prefab;
	else if (dynamic_pptr_cast<Prefab*> (sourceObject))
		return dynamic_pptr_cast<Prefab*> (sourceObject);
	else
		return NULL;
}


Object* InstantiatePrefab (Object* sourceObject)
{
	// Find super Prefab
	Prefab* superPrefab = GetPrefabFromAnyObjectInPrefab(sourceObject);
	if (superPrefab == NULL || !superPrefab->IsPrefabParent())
		return NULL;

	// Only instantiate prefabs with actual content.
	if (superPrefab->m_RootGameObject.IsNull())
		return NULL;

	Assert(superPrefab->IsPrefabParent());

	// Create empty prefab
	Prefab& prefabInstance = *NEW_OBJECT(Prefab);
	prefabInstance.Reset();
	prefabInstance.AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	
	prefabInstance.m_ParentPrefab = superPrefab;

	Assert(!prefabInstance.IsPrefabParent());
	
	vector<ObjectData> output;
	
	// Merge prefab
	AwakeFromLoadQueue awakeQueue (kMemSerialization);
	MergePrefab(*superPrefab, prefabInstance, awakeQueue, output);
	AwakePrefabLoadQueue(awakeQueue);
	
	// position & rotation of the root object is overridden by default.
	AddDefaultRootTransformModifications(prefabInstance);
	
	// We passed in the prefab itself, so return the prefab instance
	if (superPrefab == sourceObject)
		return &prefabInstance;
	
	Object* instantianciatedObject = FindInstanceFromPrefabObject(*sourceObject, output);
	if (instantianciatedObject != NULL)
		return instantianciatedObject;
	else
	{
		ErrorString("Couldn't find matching instance in prefab");
		return NULL;
	}
}

static void CollectRecurseObjectDataArrayFromPrefabRoot (GameObject& go, PPtr<Prefab> prefab, std::vector<ObjectData>& instanceObjects, vector<Object*>& addedObjects)
{
	if (prefab == go.GetPrefab())
	{	
		instanceObjects.push_back(ObjectData(&go, go.GetPrefabParentObject()));
		
		for (int i=0;i<go.GetComponentCount();i++)
		{
			///@TODO: This complexity should not be necessary anymore after the core branch refactor. Remove and check. Test has been added
			Unity::Component* com = go.GetComponentPtrAtIndex(i);
			if (com == NULL)
				continue;
			
			if (com->GetPrefab() == prefab)
				instanceObjects.push_back(ObjectData(com, com->GetPrefabParentObject()));
			else
				addedObjects.push_back(com);
		}
		
		Transform* transform = go.QueryComponent(Transform);
		if (transform)
		{
			for (int i=0;i<transform->GetChildrenCount();i++)
			{
				///@TODO: This complexity should not be necessary anymore after the core branch refactor. Remove and check. Test has been added
				Transform* childTransform = transform->GetChildrenInternal()[i];
				if (childTransform == NULL)
					continue;
				GameObject* child = childTransform->GetGameObjectPtr();
				if (child == NULL)
					continue;
				
				CollectRecurseObjectDataArrayFromPrefabRoot(*child, prefab, instanceObjects, addedObjects);
			}
		}
	}
	else
	{
		addedObjects.push_back(&go);
	}
}


/// Returns all objects contained in the prefab, instanceObjects never contains NULL pointers.
static void GetObjectDataArrayFromPrefabRoot (Prefab& prefab, std::vector<ObjectData>& instanceObjects, vector<Object*>& addedObjects)
{
	Assert(instanceObjects.empty());
	Assert(addedObjects.empty());

	GameObject* root = prefab.GetRootGameObject();
	if (root != NULL)
		CollectRecurseObjectDataArrayFromPrefabRoot(*root, PPtr<Prefab> (&prefab), instanceObjects, addedObjects);
	
	ClearDuplicatePrefabParentObjects (instanceObjects);
	
	/// @TODO: object->TestHideFlag (Object::kDontSave) was in CleanupSuperTemplate, write some tests. Primarily for the case where an object
	/// can't be loaded or deletes itself in awake.
}

static void GetObjectDataArrayFromGameObjectHierarchyWithPrefabParent (GameObject& root, Prefab& prefab, std::vector<ObjectData>& instanceObjects)
{
	instanceObjects.clear();
	
	vector<Object*> prefabObjects;
	CollectPPtrs(root, prefabObjects);
	
	instanceObjects.resize(prefabObjects.size());
	for (int i=0;i<instanceObjects.size();i++)
	{
		EditorExtension* cur = dynamic_pptr_cast<EditorExtension*> (prefabObjects[i]);
		if (cur)
		{
			// Only use parent prefab object if it is attached to the prefab we are looking for.
			Object* parentPrefabObject = NULL;
			if (cur->m_PrefabParentObject.IsValid() && cur->m_PrefabParentObject->m_Prefab == PPtr<Prefab> (&prefab))
				parentPrefabObject = cur->m_PrefabParentObject;
			
			instanceObjects[i] = ObjectData (cur, parentPrefabObject);
		}
	}
	
	ClearDuplicatePrefabParentObjects (instanceObjects);
}


void GetObjectArrayFromPrefabRoot (Prefab& prefab, std::vector<Object*>& instanceObjects)
{
	Assert(instanceObjects.empty());
	
	set<SInt32> collectedPtrs;
	if (prefab.m_RootGameObject.IsValid())
		CollectPPtrs(*prefab.m_RootGameObject, &collectedPtrs);
	
	for (set<SInt32>::const_iterator i = collectedPtrs.begin ();i != collectedPtrs.end ();i++)
	{
		EditorExtension* object = dynamic_instanceID_cast<EditorExtension*> (*i);
		if (object != NULL)
			instanceObjects.push_back(object);
	}
}


void MergePrefab (Prefab& parentPrefab, Prefab& childPrefab, AwakeFromLoadQueue& awakeFromLoadQueue, std::vector<ObjectData>& output)
{
	vector<ObjectData> instanceObjects;
	vector<Object*> addedComponents;
	GetObjectDataArrayFromPrefabRoot(childPrefab, instanceObjects, addedComponents);
	
	DeactivateGameObjectPrefabInstances(instanceObjects);
	MergePrefabInternal(parentPrefab, childPrefab, instanceObjects, addedComponents, awakeFromLoadQueue, output);
}

static void MergePrefabInternal (Prefab& parentPrefab, Prefab& childPrefab, const vector<ObjectData>& instanceObjects, const vector<Object*>& addedObjects, AwakeFromLoadQueue& awakeFromLoadQueue, std::vector<ObjectData>& output)
{
	Assert(parentPrefab.IsPrefabParent());
	Assert(!childPrefab.IsPrefabParent());
	Assert(output.empty());

	vector<Object*> prefabObjects;
	vector<Object*> outputAddedObjects;
	GetObjectArrayFromPrefabRoot(parentPrefab, prefabObjects);
	
	MergePrefabChanges (prefabObjects, instanceObjects, output, addedObjects, outputAddedObjects, childPrefab.m_Modification);
	
	GameObject* root = NULL;
	for (int i=0;i<output.size();i++)
	{
		EditorExtension* instanceObject = dynamic_pptr_cast<EditorExtension*> (output[i].objectInstance);
		EditorExtension* instanceParentObject = dynamic_pptr_cast<EditorExtension*> (output[i].prefabParent);
		
		if (instanceObject == NULL)
			continue;
		
		// @TODO: If this case happens we should call UncouplePrefab before. Write test
		// (Assigning prefab instance game objects to a different prefab)
		AssertIf(instanceObject->m_Prefab.IsValid() && instanceObject->m_Prefab != PPtr<Prefab> (&childPrefab));

		instanceObject->m_Prefab = &childPrefab;
		instanceObject->m_PrefabParentObject = instanceParentObject;
		
		// In exploded mode we show all game objects and components
		// In unexploded mode we only show the root game object and transform
		bool showObject = childPrefab.m_IsExploded;
		instanceObject->SetHideFlagsObjectOnly(showObject ? 0 : Object::kHideInHierarchy | Object::kHideInspector);
		
		if (parentPrefab.m_RootGameObject == instanceParentObject)
			root = dynamic_pptr_cast<GameObject*> (instanceObject);
	}
	
	// Apply prefab parenting
	if (root && root->QueryComponent(Transform))
		root->GetComponent(Transform).SetParent(childPrefab.m_Modification.m_TransformParent, Transform::kDisableTransformMessage);

	childPrefab.m_RootGameObject = root;
	
	// Setup root game object and transform as visible in exploded mode
	if (!childPrefab.m_IsExploded)
	{
		GameObject* root = childPrefab.m_RootGameObject;
		if (root)
			root->SetHideFlagsObjectOnly(0);
		if (root && root->QueryComponent(Transform))
			root->GetComponent(Transform).SetHideFlagsObjectOnly(0);
	}
	
	Assert(childPrefab.m_RootGameObject.IsValid());
	
	// Add all objects to awake queues
	for (int i=0;i<output.size();i++)
	{
		output[i].objectInstance->SetDirty ();

		if (IsGameObjectOrComponentActive(output[i].objectInstance))
			awakeFromLoadQueue.Add(*output[i].objectInstance, NULL, false, kActivateAwakeFromLoad);
		else
			awakeFromLoadQueue.Add(*output[i].objectInstance);
	}	

	for (int i=0;i<outputAddedObjects.size();i++)
	{
		if (IsGameObjectOrComponentActive(outputAddedObjects[i]))
			awakeFromLoadQueue.Add(*outputAddedObjects[i], NULL, false, kActivateAwakeFromLoad);
		else
			awakeFromLoadQueue.Add(*outputAddedObjects[i]);
	}	
	
}


/// On return prefabChildren conains a list of all prefabs that inherit from prefab.
void CalculateAllLoadedPrefabChildren (PPtr<Prefab> prefab, std::vector<Prefab*>& prefabChildren)
{
	Assert(prefabChildren.empty());

	vector<Prefab*> prefabs;
	Object::FindObjectsOfType (&prefabs);

	for (int i=0;i<prefabs.size();i++)
	{
		Prefab& curPrefab = *prefabs[i];
		PPtr<Prefab> curPrefabParent = curPrefab.m_ParentPrefab;
		if (curPrefabParent == prefab)
			prefabChildren.push_back(&curPrefab);
	}
}

static void RemoveAllPrefabModificationsForObject (EditorExtension& original)
{
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(&original);
	Object* parentObject = GetPrefabParentObject(&original);
	if (prefab == NULL || parentObject == NULL)
		return;
	
	// Remove any property modifications with the targetObject of the parent prefab object
	PropertyModifications& modifications = prefab->m_Modification.m_Modifications;
	for (int i=0;i<modifications.size();)
	{
		if (modifications[i].target == PPtr<Object> (parentObject))
		{
			modifications.erase(modifications.begin() + i);
			prefab->SetDirty();
		}
		else
			i++;
	}
}

bool SmartResetToInstantiatedPrefabState (EditorExtension& original)
{
	if (!IsPrefabInstanceWithValidParent(&original))
		return false;

	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(&original);
	
	RemoveAllPrefabModificationsForObject(original);
	MergePrefabInstance(*prefab);
	
	return true;
}

bool RevertPrefabInstance (Object& original)
{
	///@TODO: VALIDATE if valid prefab instance
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(&original);
	if (!IsPrefabInstanceOrDisconnectedInstance(prefab))
		return false;

	// Destroy any child objects / added components /attached assets
	vector<ObjectData> instanceObjects;
	vector<Object*> addedComponents;
	GetObjectDataArrayFromPrefabRoot(*prefab, instanceObjects, addedComponents);

	RevertInstantiatedAssetReferencesToParentObject(instanceObjects);

	vector<PPtr<Object> > addedComponentsPPtr (addedComponents.begin(), addedComponents.end());
	for (int i=0;i<addedComponentsPPtr.size();i++)
		DestroyObjectHighLevel(addedComponentsPPtr[i]);
	
	prefab->m_Modification.m_RemovedComponents.clear();
	
	RevertAllButDefaultPrefabOverrides(*prefab);

	return true;
}

bool ReconnectToLastPrefab (GameObject& go)
{
	// We only connect if we are a disconnected prefab
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(&go);
	if (prefab != NULL)
		return false;
	
	Prefab* parentPrefab = GetParentPrefabOrDisconnectedPrefab (go);
	if (parentPrefab == NULL)
		return false;

	GameObject& root = FindRootGameObjectWithSameParentPrefab(go);

	ConnectToPrefab(root, parentPrefab);
	return true;
}

void SetIsPrefabExploded (Prefab& prefab, bool exploded)
{
	Assert (!prefab.m_IsPrefabParent);
	if (prefab.m_IsExploded == exploded)
		return;
	
	prefab.m_IsExploded = exploded;
	prefab.SetDirty();
	
	MergePrefabInstance(prefab);
}

bool IsComponentAddedToPrefabInstance (Object* obj)
{
	Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (obj);
	if (component == NULL)
		return false;
	
	GameObject* go = component->GetGameObjectPtr();
	if (go == NULL)
		return false;
	
	return !IsPrefabInstanceWithValidParent(component) && IsPrefabInstanceWithValidParent(go);
}

Object* GetPrefabParentObject (Object* sourceObject)
{
	EditorExtension* targetTmp = dynamic_pptr_cast<EditorExtension*> (sourceObject);
	if (targetTmp == NULL)
		return NULL;
	return targetTmp->m_PrefabParentObject;
}



PrefabType GetPrefabType (Object& obj)
{
	EditorExtension* target = dynamic_pptr_cast<EditorExtension*> (&obj);
	if (target == NULL)
		return kNoPrefabType;
	
	Prefab* prefab = target->m_Prefab;
	if (prefab)
	{
		if (prefab->IsPrefabParent())
		{
			if (prefab->TestHideFlag(Object::kNotEditable))
				return kModelPrefabType;
			else
				return kPrefabType;
		}
		else
		{
			Prefab* parentPrefab = prefab->m_ParentPrefab;
			if (parentPrefab == NULL || parentPrefab->GetRootGameObject().IsNull())
				return kMissingPrefabInstanceType;

			if (parentPrefab->TestHideFlag(Object::kNotEditable))
				return kModelPrefabInstanceType;
			else
				return kPrefabInstanceType;
		}
	}
	else if (target->m_PrefabParentObject)
	{
		Prefab* lastConnectedPrefabParent = target->m_PrefabParentObject->m_Prefab;
		// Object was at some point connected to a prefab, but now it is not attached to one anymore and the prefab no longer exists
		if (lastConnectedPrefabParent == NULL)
			return kNoPrefabType;
		
		if (lastConnectedPrefabParent->TestHideFlag(Object::kNotEditable))
			return kDisconnectedModelPrefabInstanceType;
		else
			return kDisconnectedPrefabInstanceType;
	}
	else
		return kNoPrefabType;
}

void AddParentWithSamePrefab (TempSelectionSet& selection)
{
	if (selection.empty ())
		return;
	
	Prefab* currentFatherTemplate = GetParentPrefabOrDisconnectedPrefab (**selection.begin ());
	if (currentFatherTemplate)
	{
		Transform* transform = GetTransformFromComponentOrGameObject(*selection.begin ());
		while (transform)
		{
			if (GetParentPrefabOrDisconnectedPrefab (*transform) == currentFatherTemplate)
				selection.insert(transform->GetGameObjectPtr());
			
			transform = transform->GetParent();
		}
		return;
	}
}

GameObject* FindValidUploadPrefabRoot (GameObject& gameObject)
{
	TempSelectionSet selection;
	selection.insert(&gameObject);
	AddParentWithSamePrefab(selection);
	
	Prefab* prefab;
	GameObject* outGameObject;
	if (FindValidUploadPrefab (selection, &prefab, &outGameObject))
		return outGameObject;
	else
		return NULL;
}

bool FindValidUploadPrefab (const TempSelectionSet& selection, Prefab** outPrefab, GameObject** outRootGameObject)
{
	*outRootGameObject = NULL;
	*outPrefab = NULL;
	
	if (selection.empty ())
		return false;
	
	// Make sure we have a parent prefab
	Object& firstObject = **selection.begin ();
	Prefab* parentPrefab = GetParentPrefabOrDisconnectedPrefab(firstObject);
	if (parentPrefab == NULL)
		return false;
	
	// An uploadable prefab must be marked editable and must be in a prefab file thus persistet on disk
	if (!parentPrefab->IsPersistent() || parentPrefab->TestHideFlag(Object::kNotEditable))
		return false;
	Assert(parentPrefab->IsPrefabParent());
	
	GameObject* rootGameObject = NULL;
	GameObject* prefabRootGameObject = parentPrefab->m_RootGameObject;
	for (TempSelectionSet::const_iterator i=selection.begin ();i != selection.end ();i++)
	{
		Object& cur = **i;
		if (GetParentPrefabOrDisconnectedPrefab (cur) != parentPrefab)
			return false;
		
		Object* parentObject = GetPrefabParentObject(&cur);
		if (parentObject == prefabRootGameObject)
			rootGameObject = dynamic_pptr_cast<GameObject*> (&cur);
	}
	
	if (rootGameObject)
	{
		*outPrefab = parentPrefab;
		*outRootGameObject = rootGameObject;
		return true;
	}
	else
		return false;	
}

Prefab* GetParentPrefabOrDisconnectedPrefab (Object& target)
{
	EditorExtension* ext = dynamic_pptr_cast<EditorExtension*> (&target);
	if (ext == NULL)
		return NULL;
	
	EditorExtension* parent = ext->m_PrefabParentObject;
	if (parent)
		return parent->m_Prefab;
	else
		return NULL;	
}

struct LookupPrefabParentRemap : public RemapPPtrCallback
{
	SInt32 prefab;
	
	LookupPrefabParentRemap (Prefab& inPrefab)
	{
		prefab = inPrefab.GetInstanceID();
	}
	
	virtual SInt32 Remap (SInt32 clonedObject)
	{
		EditorExtension* target = dynamic_instanceID_cast<EditorExtension*>(clonedObject);
		
		if (target != NULL && target->m_Prefab.GetInstanceID() == prefab)
			return target->m_PrefabParentObject.GetInstanceID();
		else
			return clonedObject;
	}
};

void RecordPrefabInstancePropertyModifications (Object& object)
{
	if (!IsPrefabInstanceWithValidParent(&object))
		return;

	Prefab* prefabInstance = GetPrefabFromAnyObjectInPrefab (&object);
	Object* parentObject = GetPrefabParentObject(&object);
	
	if (GenerateRootTransformPropertyModification(object, prefabInstance->m_RootGameObject, prefabInstance->m_Modification))
		prefabInstance->SetDirty();

	LookupPrefabParentRemap remap (*prefabInstance);
	if (GeneratePrefabPropertyDiff(*parentObject, object, &remap, prefabInstance->m_Modification.m_Modifications) == kAddedOrUpdatedProperties)
		prefabInstance->SetDirty();
}

static bool IsMonoBehaviourScriptAllowed (Object& object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&object);
	MonoBehaviour* parentBehaviour = dynamic_pptr_cast<MonoBehaviour*> (GetPrefabParentObject(&object));
	if (behaviour != NULL && parentBehaviour != NULL)
	{
		if (behaviour->GetScript() != parentBehaviour->GetScript())
			return false;
	}
	return true;
}


bool IsPrefabGameObjectDeleteAllowed (GameObject& go)
{
	if (!IsPrefabInstanceWithValidParent(&go))
		return true;
	
	Prefab* prefabInstance = GetPrefabFromAnyObjectInPrefab (&go);
	
	// The root game object can do whatever it likes
	if (prefabInstance->m_RootGameObject == PPtr<GameObject> (&go))
		return true;
	else
		return false;
}


bool IsPrefabTransformParentChangeAllowed (Transform& transform, Transform* newParent)
{
	if (!IsPrefabInstanceWithValidParent(&transform))
		return true;
	
	Prefab* prefabInstance = GetPrefabFromAnyObjectInPrefab (&transform);
	
	// The root game object can do whatever it likes
	if (prefabInstance->m_RootGameObject == PPtr<GameObject> (transform.GetGameObjectPtr()))
		return true;
	
	// prefab parent object should always be there.
	Transform* prefabParentObject = dynamic_pptr_cast<Transform*> (GetPrefabParentObject(&transform));
	if (prefabParentObject == NULL)
		return false;
	
	// Verify that the Transform parent points to the same object in the parent prefab
	LookupPrefabParentRemap remap (*prefabInstance);
	
	SInt32 transformParentRemappedInstanceID = remap.Remap(newParent ? newParent->GetInstanceID() : 0);
	
	Transform* prefabParentParentTransform = prefabParentObject->GetParent();
	SInt32 prefabParentParentTransformInstanceID = prefabParentParentTransform ? prefabParentParentTransform->GetInstanceID() : 0;
	
	return prefabParentParentTransformInstanceID == transformParentRemappedInstanceID;
}

void RecordPrefabInstancePropertyModificationsAndValidate (Object& object)
{
	if (!IsPrefabInstanceWithValidParent(&object))
		return;
	
	RecordPrefabInstancePropertyModifications(object);

	// Detect any changes that are not allowed on prefab instances (Parenting of transforms or MonoBehaviour script assignment)
	bool breakConnection = false;
	Transform* transform = dynamic_pptr_cast<Transform*> (&object);
	if (transform && !IsPrefabTransformParentChangeAllowed(*transform, transform->GetParent()))
		breakConnection = true;
	
	if (!IsMonoBehaviourScriptAllowed(object))
		breakConnection = true;

	if (breakConnection)
	{	
		DisconnectPrefabInstance(&object);
		return;
	}
}

static bool IsRootOrDirectChildOfRootGameObject (Object* targetObject)
{
	// Grab transform from game object
	GameObject* go = dynamic_pptr_cast<GameObject*> (targetObject);
	if (go != NULL)
	{
		Transform* transform = go->QueryComponent(Transform);
		if (transform == NULL)
			return true;
		
		Transform* parentTransform = transform->GetParent();
		if (parentTransform == NULL)
			return true;

		if (parentTransform->GetParent() == NULL)
			return true;
	}
	return false;
}

GameObject& FindRootGameObjectWithSameParentPrefab (GameObject& gameObject)
{
	Prefab* searchForPrefab = GetParentPrefabOrDisconnectedPrefab(gameObject);
	GameObject* bestParent = &gameObject;
	
	Transform* transform = gameObject.QueryComponent(Transform);
	while (transform)
	{
		///@TODO: If we add support for nesting this should not contiue searching
		/// if the prefab instance is in a different prefab instance
		
		if (GetParentPrefabOrDisconnectedPrefab(*transform) == searchForPrefab)
			bestParent = &transform->GetGameObject();
		
		transform = transform->GetParent();
	}
	
	return *bestParent;
}

/// Helper function to find the prefab root of an object (used for picking niceness)
/// Returns the root prefab if there is one or the passed game object if there isn't
GameObject* FindPrefabRoot (GameObject* gameObject)
{
	Prefab* prefab = NULL;
	if (gameObject != NULL)
		prefab = gameObject->m_Prefab;
	
	if (prefab && prefab->m_RootGameObject.IsValid())
		return prefab->m_RootGameObject;
	else
		return gameObject;
}

bool IsPrefabInstanceOrDisconnectedInstance (Object* p)
{
	EditorExtension* object = dynamic_pptr_cast<EditorExtension*> (p);
	Prefab* prefab = dynamic_pptr_cast<Prefab*> (p);
	Assert(object == NULL || prefab == NULL);
	if (object != NULL)
		prefab = object->m_Prefab;
	
	if (prefab == NULL)
		return false;
	
	return !prefab->IsPrefabParent ();
}

bool IsPrefabInstanceWithValidParent (Object* p)
{
	EditorExtension* object = dynamic_pptr_cast<EditorExtension*> (p);
	if (object == NULL)
		return false;

	Prefab* prefab = object->m_Prefab;
	if (prefab == NULL || prefab->m_IsPrefabParent)
		return false;
	
	EditorExtension* parent = object->m_PrefabParentObject;
	if (parent == NULL)
		return false;
	
	if (parent->m_Prefab == PPtr<Prefab> (prefab))
		return false;

	return true;
}

GameObject* CalculateSingleRootGameObject (const set<GameObject*>& gos)
{
	GameObject* root = NULL;
	for (set<GameObject*>::const_iterator i=gos.begin();i!=gos.end();i++)
	{
		GameObject* cur = *i;
		if (cur == NULL)
			continue;
		
		if (root == NULL)
			root = *i;
		else
		{
			if (root->QueryComponent(Transform) && root->QueryComponent(Transform))
			{
				// Cur is a child of root
				if (IsChildOrSameTransform(*cur->QueryComponent(Transform), *root->QueryComponent(Transform)))
					root = cur;
				// root is already root of child
				else if (IsChildOrSameTransform(*root->QueryComponent(Transform), *cur->QueryComponent(Transform)))
					;
				// Comes from a completely different transform hierarchy
				else
					return NULL;
			}
		}
	}
	
	return root;
}

bool IsProjectPrefab (Object* object)
{
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(object);
	return prefab != NULL && prefab->IsPrefabParent();
}

bool RemovePropertyModification (PropertyModifications& modifications, Object* target, const std::string& propertyPath)
{
	PPtr<Object> prefabParentObject = GetPrefabParentObject(target);
	bool didRemove = false;
	for (int i=0;i<modifications.size();i++)
	{
		if (modifications[i].target == prefabParentObject && IsPropertyPathOverridden(modifications[i].propertyPath.c_str(), propertyPath.c_str()))
		{
			modifications.erase(modifications.begin() + i);
			didRemove = true;
			i--;
		}
	}
	return didRemove;
}

bool HasPrefabOverride (Object* targetObject, const std::string& propertyPath)
{
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(targetObject);
	if (prefab)
		return HasPrefabOverride (prefab->GetPropertyModifications(), targetObject, propertyPath);
	else
		return false;
}


bool HasPrefabOverride (const PropertyModifications& modifications, Object* target, const std::string& propertyPath)
{
	PPtr<Object> prefabParentObject = GetPrefabParentObject(target);
	for (int i=0;i<modifications.size();i++)
	{
		if (modifications[i].target == prefabParentObject && IsPropertyPathOverridden(modifications[i].propertyPath.c_str(), propertyPath.c_str()))
			return true;
	}
	return false;
}