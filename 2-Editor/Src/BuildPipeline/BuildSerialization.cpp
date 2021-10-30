#include "UnityPrefix.h"
#include "BuildSerialization.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/BuildPipeline/AssetBundleBuilder.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/ManagerContextLoading.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Network/NetworkManager.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Editor/Src/Utility/BuildPlayerUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/HierarchyState.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystemRenderer.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Camera/GraphicsSettings.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Graphics/SubstanceArchive.h"
#include "Runtime/Graphics/ProceduralTexture.h"
#include "Runtime/Graphics/ProceduralMaterial.h"
#include "Editor/Src/AssetPipeline/AssetMetaData.h"
#include "Editor/Src/Utility/RuntimeClassHashing.h"

using namespace std;

typedef dynamic_array<int> InstanceIDArray;
typedef set<SInt32> InstanceIDSet;

static void CalculateAllLevelManagersAndUsedSceneObjects (WriteDataArray& output, TransferInstructionFlags instructions);
static void LinearizeLocalIdentifierInFile (WriteDataArray& output);

static const char* kDefaultResourcePathTemplate = "library/unity default resources";
static const char* kOldWebResourcePathTemplate = "Library/unity_web_old";
static const char* kDefaultExtraResourcesPathTemplate = "Resources/unity_builtin_extra";

map<UnityGUID, PPtr<GameObject> >  BuildGUIDToPrefabList (InstanceIDToBuildAsset& assets);


#define NESTED_PREFAB_SUPPORT 0

bool ResolveInstanceIDMapping (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, const InstanceIDBuildRemap& assets)
{
	// Referenced object is available in another asset file, this is cool!
	InstanceIDBuildRemap::const_iterator foundAsset = assets.find(id);
	if (foundAsset == assets.end ())
		return false;

	const SerializedObjectIdentifier& identifier = foundAsset->second;

	Assert(identifier.localIdentifierInFile != 0);

	localIdentifier = GetPersistentManager().GlobalToLocalSerializedFileIndexInternal(identifier);
	return true;
}


void BuildingPlayerOrAssetBundleInstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context)
{
	if (ResolveInstanceIDMapping(id, localIdentifier, *reinterpret_cast<InstanceIDBuildRemap*>(context)))
		return;

	// Anything in default resources can be referenced too.  Also, always included shaders and their dependencies.
	if (IsDefaultResourcesObject (id) ||
		IsAlwaysIncludedShaderOrDependency (id))
	{
		GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (id, localIdentifier);
		return;
	}

	// This is a problem, most likely the referenced object is missing, or the user explicitly didn't want it to be included
	// So just mark it as a null reference so that when loading it, we don't actually try to load it from disk.
	//Object* object = Object::IDToPointer (id);
	//printf_console ("    OBJECT [player] %d '%s' (from '%s') is OUT!\n", id, object->GetName (), GetPersistentManager ().GetPathName(id).c_str ());
	localIdentifier.localSerializedFileIndex = 0;
	localIdentifier.localIdentifierInFile = 0;
}

static void SaveSceneInstanceIDResolveCallback (SInt32 id, LocalSerializedObjectIdentifier& localIdentifier, void* context)
{
	if (ResolveInstanceIDMapping(id, localIdentifier, *reinterpret_cast<InstanceIDBuildRemap*>(context)))
		return;

	GetPersistentManager ().InstanceIDToLocalSerializedObjectIdentifierInternal (id, localIdentifier);
}


static void ConvertAssetsToInstanceIDBuildRemap (const InstanceIDToBuildAsset& src, InstanceIDBuildRemap& output)
{
	output.reserve(output.size() + src.size());
	for (InstanceIDToBuildAsset::const_iterator i=src.begin();i != src.end();i++)
	{
		output.push_unsorted(i->first, i->second.temporaryObjectIdentifier);
	}
	output.sort();
}

static void ConvertSceneObjectsToInstanceIDBuildRemap (const string& path, const WriteDataArray& sceneObjects, InstanceIDBuildRemap& output)
{
	int pathIndex = GetPersistentManager().GetSerializedFileIndexFromPath(path);

	output.reserve(output.size() + sceneObjects.size());
	for (WriteDataArray::const_iterator i=sceneObjects.begin();i != sceneObjects.end();i++)
	{
		output.push_unsorted(i->instanceID, SerializedObjectIdentifier(pathIndex, i->localIdentifierInFile));
	}
	output.sort();
}


static int GetClassIDFromLoadedObjectOnly (int instanceID)
{
	Object* object = Object::IDToPointer(instanceID);
	if (object)
		return object->GetClassID();
	else
		return -1;
}

int GetClassIDWithoutLoadingObject (int instanceID)
{
	Object* targetObject = Object::IDToPointer(instanceID);

	if (targetObject != NULL)
		return targetObject->GetClassID();
	else
		return GetPersistentManager().GetSerializedClassID(instanceID);
}


class CollectSaveSceneDependencies : public GenerateIDFunctor
{
	set<int>     m_AllProcessed;
	dynamic_array<int>  m_NeedsProcessing;

public:
	virtual ~CollectSaveSceneDependencies () {}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
	{
		if (oldInstanceID == 0)
			return oldInstanceID;

		if (m_AllProcessed.insert (oldInstanceID).second)
			m_NeedsProcessing.push_back (oldInstanceID);

		return oldInstanceID;
	}

	void CollectWithRoots (const InstanceIDArray& roots, InstanceIDArray& collectedObjects, TransferInstructionFlags options)
	{
		collectedObjects.clear();
		RemapPPtrTransfer transferFunction (options, false);
		transferFunction.SetGenerateIDFunctor (this);
		m_NeedsProcessing.reserve (roots.size() + 10000);

		m_NeedsProcessing = roots;
		for (dynamic_array<int>::iterator i=m_NeedsProcessing.begin ();i != m_NeedsProcessing.end ();i++)
			m_AllProcessed.insert(*i);

		while (!m_NeedsProcessing.empty ())
		{
			Object* referenced = PPtr<Object> (m_NeedsProcessing.back ());
			m_NeedsProcessing.pop_back();

			if (referenced == NULL || referenced->IsPersistent() || referenced->TestHideFlag(Object::kDontSave))
				continue;

			/// In case one of the components has a reference to a prefab - This would clearly be an invalid prefab setup (case 402747)
			EditorExtension* editorExtension = dynamic_pptr_cast<EditorExtension*> (referenced);
			if (editorExtension != NULL && editorExtension->IsPrefabParent())
				continue;

			referenced->VirtualRedirectTransfer (transferFunction);
			collectedObjects.push_back(referenced->GetInstanceID());
		}
	}
};


class CollectDirectReferences : public GenerateIDFunctor
{
	set<int>& references;

	public:

	CollectDirectReferences (set<int>& r) : references (r) {}
	virtual ~CollectDirectReferences () {}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
	{
		references.insert(oldInstanceID);
		return oldInstanceID;
	}
};

static void StripPrefabObjectsWhichAreNotReferenced (InstanceIDArray& usedSceneObjectsOutput)
{
// enable this on retained gui branch
#if 0
	set<int> allReferencedObjects;
	set<int> allPrefabObjects;

	CollectDirectReferences collector (allReferencedObjects);

	RemapPPtrTransfer transfer (kSerializeEditorMinimalScene, false);
	transfer.SetGenerateIDFunctor (&collector);

	for (InstanceIDArray::iterator i=usedSceneObjectsOutput.begin(); i != usedSceneObjectsOutput.end();i++)
	{
		Object* obj = dynamic_instanceID_cast<Object*>(*i);

		// Collect all references to objects from the Prefab class itself
		Prefab* prefab = dynamic_pptr_cast<Prefab*> (obj);
		if (prefab == NULL)
		{
			// Collect all references to objects from any non-prefabbed objects
			if (!IsPrefabInstanceOrDisconnectedInstance(obj))
			{
				allReferencedObjects.insert(obj->GetInstanceID());
				obj->VirtualRedirectTransfer (transfer);
			}
		}
			else
		{
			if (!prefab->IsNestedInstance ())
				transfer.Transfer(prefab->m_Modification, "Base");
		}
	}

	InstanceIDArray objectsToWrite;
	for (InstanceIDArray::iterator i=usedSceneObjectsOutput.begin(); i != usedSceneObjectsOutput.end();i++)
	{
		int instanceID = *i;
		Object* obj = dynamic_instanceID_cast<Object*>(instanceID);

		// We only serialize objects that are necessary to reconstruct the scene.
		// * We save non-nested prefabs
		// * components / game objects that are not part of a prefab
		// * components / game objects that are referenced by other objects in the scene
		bool isNonNestedPrefab = dynamic_pptr_cast<Prefab*> (obj) != NULL && !static_cast<Prefab*> (obj)->IsNestedInstance ();
		bool isNotRelatedToAnyPrefab = !IsPrefabInstanceOrDisconnectedInstance (obj);
		bool isReferencedByAnySceneObject = allReferencedObjects.count (instanceID);
		if (isNonNestedPrefab || isNotRelatedToAnyPrefab || isReferencedByAnySceneObject)
			objectsToWrite.push_back(instanceID);
	}

	usedSceneObjectsOutput = objectsToWrite;

#endif
}

void CollectUsedSceneObjects (InstanceIDArray& usedSceneObjectsOutput, TransferInstructionFlags options)
{
	// Fill usedObjectsRoots from level managers & scene game objects
	InstanceIDArray usedObjectsRoots;
	CollectLevelGameManagers(usedObjectsRoots);
	vector_set<int> levelGameManagerSet (usedObjectsRoots.begin(), usedObjectsRoots.end());
	CollectSceneGameObjects(usedObjectsRoots);

	// Calculate all depdendencies from scene game objects and scene managers
	InstanceIDArray allDependencies;

	CollectSaveSceneDependencies dependencyCollector;
	dependencyCollector.CollectWithRoots(usedObjectsRoots, allDependencies, options & (~kSerializeEditorMinimalScene));

	usedSceneObjectsOutput.clear();
	for (InstanceIDArray::iterator i=allDependencies.begin(); i != allDependencies.end();i++)
	{
		Object& object = *PPtr<Object> (*i);
		AssertIf (object.IsPersistent ());

		// Ignore level game managers
		if (levelGameManagerSet.count(*i))
			continue;

		usedSceneObjectsOutput.push_back(*i);
	}

	if (options & kSerializeEditorMinimalScene)
		StripPrefabObjectsWhichAreNotReferenced (usedSceneObjectsOutput);

	if (options & kWarnAboutLeakedObjects)
	{
		//// Track which objects are being leaked and warn aboutit
		map<string, int> remove;
		vector_set<int> usedSceneObjectsOutputSet (usedSceneObjectsOutput.begin(), usedSceneObjectsOutput.end());

		vector<SInt32> objects;
		Object::FindAllDerivedObjects (ClassID (EditorExtension), &objects);
		Object *leaked = NULL;
		for (vector<SInt32>::iterator i= objects.begin ();i != objects.end ();++i)
		{
			EditorExtension& object = *PPtr<EditorExtension> (*i);
			if (object.IsPersistent ())
				continue;
			if (object.TestHideFlag (Object::kDontSave))
				continue;
			if (object.IsDerivedFrom (ClassID (GameManager)))
				continue;
			if (object.IsPrefabParent ())
				continue;
			if (IsPrefabInstanceOrDisconnectedInstance(&object))
				continue;

			if (usedSceneObjectsOutputSet.count (object.GetInstanceID()) ==  0)
			{
				MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&object);

				string name;
				if( behaviour && behaviour->GetScript().IsValid() )
					name = behaviour->GetScript ()->GetScriptClassName ();
				else
				{
					name = object.GetClassName() + " "+ object.GetName();
				}

				remove[name]++;
				if (!leaked)
					leaked = &object;
			}
		}

		if (!remove.empty())
		{
			string cleaned = "Cleaning up leaked objects in scene since no game object, component or manager is referencing them";
			for (map<string, int>::iterator i=remove.begin(); i != remove.end();i++)
			{
				cleaned += Format("\n %s has been leaked %d times.", i->first.c_str(), i->second);
			}
			LogStringObject(cleaned, leaked);
		}
	}
}

bool IsValidFileID (LocalIdentifierInFileType fileID)
{
	return fileID > 0;
}

void SetupFileIDsForObjects (dynamic_array<int>& serializeInstanceIDs, WriteDataArray& allSceneObjects, TransferInstructionFlags options)
{
	Rand r (time (NULL));

	set<LocalIdentifierInFileType> usedIds;
	allSceneObjects.resize(serializeInstanceIDs.size());
	dynamic_array<Object*> loadedObjects (kMemTempAlloc);
	loadedObjects.resize_initialized(serializeInstanceIDs.size());

#define AssignID(newid) {	allSceneObjects[i].localIdentifierInFile = newid; usedIds.insert (allSceneObjects[i].localIdentifierInFile); obj->SetFileIDHint(allSceneObjects[i].localIdentifierInFile); }

#define AssignNextFreeRandomID() { \
LocalIdentifierInFileType tempFreeID = 0; \
while (!IsValidFileID(tempFreeID) || usedIds.count(tempFreeID)) \
tempFreeID = r.Get() & 0x7fffffff; \
AssignID (tempFreeID); }

#define AssignNextIncreasingID(newid) { \
LocalIdentifierInFileType tempFreeID = newid; \
while (!IsValidFileID(tempFreeID) || usedIds.count(tempFreeID)) \
tempFreeID++; \
AssignID (tempFreeID); }


	// - First assign file ids from file id hints stored in objects to retain previous object order.
	// - Setup strippedPrefabObject build usage
	for (int i=0; i<serializeInstanceIDs.size(); i++)
	{
		Object* obj = Object::IDToPointer(serializeInstanceIDs[i]);
		Assert(obj != NULL);
		loadedObjects[i] = obj;

		LocalIdentifierInFileType hint = obj->GetFileIDHint();
		if (hint != 0 && usedIds.count(hint) == 0)
			AssignID (hint)

		allSceneObjects[i].instanceID = serializeInstanceIDs[i];

		if ((options & kSerializeEditorMinimalScene))
			allSceneObjects[i].buildUsage.strippedPrefabObject = IsPrefabInstanceOrDisconnectedInstance(obj) && dynamic_pptr_cast<Prefab*> (obj) == NULL;
	}

	// For those fileIDs which did not have hints or which had conflicts, assign new values.
	// First assign sequential values starting from 1 for GameManagers
	for (int i=0; i<allSceneObjects.size(); i++)
	{
		if (allSceneObjects[i].localIdentifierInFile != 0)
			continue;

		Object* obj = loadedObjects[i];
		if (obj->IsDerivedFrom (ClassID (GameManager)))
		{
			AssignNextIncreasingID(1);
		}
	}

	// Assign random IDs to prefabs
	for (int i=0; i<allSceneObjects.size(); i++)
	{
		if (allSceneObjects[i].localIdentifierInFile != 0)
			continue;

		Object* obj = loadedObjects[i];
		if (loadedObjects[i]->IsDerivedFrom (ClassID (Prefab)))
		{
			AssignNextFreeRandomID();
		}
	}

	// Assign the next free ID after prefabs to stripped components / game objects
	for (int i=0; i<allSceneObjects.size(); i++)
	{
		if (allSceneObjects[i].localIdentifierInFile != 0)
			continue;

		if (allSceneObjects[i].buildUsage.strippedPrefabObject)
		{
			Object* obj = loadedObjects[i];
			Prefab* prefab = GetPrefabFromAnyObjectInPrefab(obj);
			if (prefab != NULL)
			{
				AssignNextIncreasingID (prefab->GetFileIDHint() + 1);
			}
		}
	}

	// Assign random IDs to game objects & assets
	for (int i=0; i<allSceneObjects.size(); i++)
	{
		if (allSceneObjects[i].localIdentifierInFile != 0)
			continue;

		Object* obj = loadedObjects[i];
		if (!obj->IsDerivedFrom (ClassID (Component)))
		{
			AssignNextFreeRandomID();
		}
	}

	// Assign the next free ID after GameObject ID to components
	for (int i=0; i<allSceneObjects.size(); i++)
	{
		if (allSceneObjects[i].localIdentifierInFile != 0)
			continue;

		Object* obj = loadedObjects[i];

		Assert(obj->IsDerivedFrom (ClassID (Component)));
		Unity::Component* component = static_cast<Unity::Component*> (obj);
		GameObject* go = component->GetGameObjectPtr();

		LocalIdentifierInFileType freeID;
		if (go != NULL)
			freeID = go->GetFileIDHint() + 1;
		else
			freeID = r.Get() & 0x7fffffff;

		AssignNextIncreasingID(freeID);
	}

	sort(allSceneObjects.begin(), allSceneObjects.end());
}

bool SaveScene (const string& pathName, map<LocalIdentifierInFileType, SInt32>* backupFileIDToHeapID, TransferInstructionFlags options)
{
	Assert (!pathName.empty ());

	PersistentManager& pm = GetPersistentManager ();

	if (!ResetSerializedFileAtPath (pathName))
		return false;

	WriteDataArray allSceneObjects;
	CalculateAllLevelManagersAndUsedSceneObjects(allSceneObjects, options);

	if (backupFileIDToHeapID)
	{
		for (int i=0;i<allSceneObjects.size();i++)
			(*backupFileIDToHeapID)[allSceneObjects[i].localIdentifierInFile] = allSceneObjects[i].instanceID;
	}

	InstanceIDBuildRemap writeRemap;

	ConvertSceneObjectsToInstanceIDBuildRemap (pathName, allSceneObjects, writeRemap);

	pm.Lock();
	SetInstanceIDResolveCallback (SaveSceneInstanceIDResolveCallback, &writeRemap);
	int error = pm.WriteFileInternal (pathName, pm.GetSerializedFileIndexFromPath(pathName), &allSceneObjects[0], allSceneObjects.size(), NULL, BuildTargetSelection::NoTarget(), options);
	SetInstanceIDResolveCallback (NULL);
	pm.Unlock();

	VerifyNothingIsPersistentInLoadedScene (pathName);

	return error == 0;
}

static UInt64 CalculateSortIndex (int lhsID, int lhsClassID)
{
	if (lhsClassID == -1)
		return 30000;

	if (Object::IsDerivedFromClassID(lhsClassID, ClassID (PreloadData)))
		return 1;
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (AssetBundle)))
		return 2;

	// ProceduralMaterial objects
	if (lhsClassID==ClassID (SubstanceArchive))
		return 3;
	else if (lhsClassID==ClassID (ProceduralTexture))
		return 4;
	else if (lhsClassID==ClassID (ProceduralMaterial))
		return 5;

	// Editor extension and prefabs come last
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (EditorExtensionImpl)))
		return numeric_limits<UInt64>::max();
	// Editor extension and prefabs come last
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (Prefab)))
		return numeric_limits<UInt64>::max() - 1;
	// Monobehaviours come last but before any prefabs
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (MonoBehaviour)))
	{
		///@TODO: Remove this shit once we support loading monobehaviours without recursion!

		MonoBehaviour* com = dynamic_instanceID_cast<MonoBehaviour*> (lhsID);
		// Scriptable Assets...
		if (com && com->GetGameObjectPtr() == NULL)
			return 1000000;
		// Before actual MonoBehaviour Components
		else
			return 1000001;
	}
	// Terrain data references game objects directly, thus it needs to come after prefabs.
	// We already make sure all assets are loaded before the scene starts loading so it's unlikely
	// there are cross references from a prefab game object to a TerrainData
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (TerrainData)))
		return numeric_limits<UInt64>::max() - 2;
	else if (Object::IsDerivedFromClassID(lhsClassID, ClassID (NamedObject)))
		return lhsClassID + 10000;
	// Components and other objects are sorted by classID and come after asset types
	else
	{
		return lhsClassID + 20000;
	}
}

static int CompareAssetLoadOrderRaw (int lhsID, int lhsClassID, UInt64 lhsPreloadIndex, int rhsID, int rhsClassID, UInt64 rhsPreloadIndex)
{
	if (lhsPreloadIndex == 0)
		lhsPreloadIndex = CalculateSortIndex(lhsID, lhsClassID);

	if (rhsPreloadIndex == 0)
		rhsPreloadIndex = CalculateSortIndex(rhsID, rhsClassID);

	if (lhsPreloadIndex < rhsPreloadIndex)
		return -1;
	else if (lhsPreloadIndex > rhsPreloadIndex)
		return 1;
	else
		return 0;
}

bool CompareAssetLoadOrderForBuildingScene (const WriteData& lhsData, const WriteData& rhsData)
{
	int lhs = lhsData.instanceID;
	int rhs = rhsData.instanceID;

	int lhsClassID = GetClassIDFromLoadedObjectOnly(lhs);
	int rhsClassID = GetClassIDFromLoadedObjectOnly(rhs);
	Assert(lhsClassID != -1);
	Assert(rhsClassID != -1);

	// MonoBehaviours in a scene need to be sorted by script execution order
	if (lhsClassID == ClassID(MonoBehaviour) && rhsClassID == ClassID(MonoBehaviour))
		return AwakeFromLoadQueue::SortBehaviourByExecutionOrderAndInstanceID(lhs, rhs);

	// Sort other assets
	int compare = CompareAssetLoadOrderRaw (lhs, lhsClassID, 0, rhs, rhsClassID, 0);
	if (compare != 0)
		return compare < 0;
	else
		return false;
}


class DependencyCollector : public GenerateIDFunctor
{
	InstanceIDSet* m_IDs;

public:

	DependencyCollector (set<SInt32>* ptrs)
	{
		m_IDs = ptrs;
	}

	virtual ~DependencyCollector () {}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag = kNoTransferFlags)
	{
		Object* targetObject = dynamic_instanceID_cast<Object*> (oldInstanceID);
		if (targetObject == NULL || targetObject->GetClassID() >= ClassID(SmallestEditorClassID))
			return oldInstanceID;

		// Persistent pointers get all their dependencies resolved
		// Selecting something in the scene, we only do it for strong pointers.
		// Eg. game object -> component or transform children but not transform parent.
		if (!targetObject->IsPersistent() && (metaFlag & kStrongPPtrMask) == 0)
			return oldInstanceID;

		// Already Inserted?
		if (!m_IDs->insert (oldInstanceID).second)
			return oldInstanceID;

#define DEBUG_DEPENDENCIES 0
#if DEBUG_DEPENDENCIES
		static int gLevel = 0;

		for (int i=0;i<gLevel;i++)
			printf_console ("\t");
		gLevel++;
		printf_console ("Adding %s at path %s\n", extendable->GetName().c_str(), GetPersistentManager().GetPathName(extendable->GetInstanceID()).c_str());
#endif

		RemapPPtrTransfer transferFunction (kBuildPlayerOnlySerializeBuildProperties, false);
		transferFunction.SetGenerateIDFunctor (this);
		targetObject->VirtualRedirectTransfer (transferFunction);

#if DEBUG_DEPENDENCIES
		gLevel--;
#endif

		return oldInstanceID;
	}
};

bool HasClassGameReleaseDependencies (int classID)
{
	if (classID == ClassID(Texture2D))
		return false;
	if (classID == ClassID(AudioClip))
		return false;
	if (classID == ClassID(Mesh))
		return false;

//	if (classID == ClassID(Shader))
//		return false;

	return true;
}

bool IsClassIDSupportedInBuild (int classID)
{
	return classID >= 0 && classID <= ClassID(SmallestEditorClassID);
}

#define DEBUG_DEPENDENCIES 0

SInt32 GameReleaseCollector::GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
{
	int classID = GetClassIDWithoutLoadingObject(oldInstanceID);

	if (!IsClassIDSupportedInBuild (classID))
		return oldInstanceID;

	// Already Inserted? No need to process further.
	if (!m_IDs->insert (oldInstanceID).second)
		return oldInstanceID;

	// Since there can be no dependencies after this object there is no point in loading it.
	// Thus reduces build times and memory usage while building
	if (!HasClassGameReleaseDependencies(classID))
		return oldInstanceID;

#if DEBUG_DEPENDENCIES
	static int gLevel = 0;

	for (int i=0;i<gLevel;i++)
		printf_console ("\t");
	gLevel++;
	printf_console ("Adding %s at path %s\n", extendable->GetName().c_str(), GetPersistentManager().GetPathName(extendable->GetInstanceID()).c_str());
#endif

	Object* targetObject = dynamic_instanceID_cast<Object*> (oldInstanceID);

	if (targetObject == NULL)
	{
		AssertString("Target object has dissappeared during dependency calculation");
		return oldInstanceID;
	}

	RemapPPtrTransfer transferFunction (kBuildPlayerOnlySerializeBuildProperties | kSerializeGameRelease | m_ExtraTransferInstructionFlags, false);
	transferFunction.SetGenerateIDFunctor (this);
	targetObject->VirtualRedirectTransfer (transferFunction);

#if DEBUG_DEPENDENCIES
	gLevel--;
#endif

	return oldInstanceID;
}

SInt32 GameReleaseDependenciesSameAsset::GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
{
	Object* targetObject = dynamic_instanceID_cast<Object*> (oldInstanceID);
	if (targetObject == NULL || targetObject->GetClassID() >= ClassID(SmallestEditorClassID))
		return oldInstanceID;

	if (targetObject->TestHideFlag(Object::kDontSave) || !targetObject->IsPersistent())
		return oldInstanceID;

	string path = GetPersistentManager().GetPathName(oldInstanceID);
	if (GetGUIDPersistentManager().GUIDFromAnySerializedPath(path) != m_Asset)
		return oldInstanceID;

	if (!m_SameAssetIDs.insert(oldInstanceID).second)
		return oldInstanceID;

	m_IDs->insert (oldInstanceID);

	RemapPPtrTransfer transferFunction (kBuildPlayerOnlySerializeBuildProperties | kSerializeGameRelease, false);
	transferFunction.SetGenerateIDFunctor (this);
	targetObject->VirtualRedirectTransfer (transferFunction);

	return oldInstanceID;
}


static void CollectSceneRootDependencies (string& pathName, set<SInt32>& ids);
static void CollectSceneRootDependencies (string& pathName, set<SInt32>& ids)
{
	SerializedFile* file = UNITY_NEW(SerializedFile,kMemTempAlloc);
	ResourceImageGroup group;
	if (file->InitializeRead(pathName, group, 512, 4, 0))
	{
		const dynamic_block_vector<FileIdentifier>& files = file->GetExternalRefs();
		for (int i=0;i<files.size();i++)
		{
			UnityGUID guid = files[i].guid;
			if (!IsUnitySceneFile(GetAssetPathFromGUID (guid)))
			{
				GetPersistentManager().GetInstanceIDsAtPath(GetAssetPathFromGUID (guid), &ids);
				GetPersistentManager().GetInstanceIDsAtPath(GetMetaDataPathFromGUID (guid), &ids);
			}
		}
	}
	UNITY_DELETE(file,kMemTempAlloc);
}

static void CollectGUIDDependencies (const string& pathName, set<UnityGUID>& ids, set<UnityGUID>& newGUIDs)
{
	if (!IsFileCreated(pathName))
		return;

	SerializedFile* file = UNITY_NEW(SerializedFile, kMemTempAlloc);
	ResourceImageGroup group;
	if (file->InitializeRead(pathName, group, 512, 4, 0))
	{
		const dynamic_block_vector<FileIdentifier>& files = file->GetExternalRefs();
		for (int i=0;i<files.size();i++)
		{
			UnityGUID guid = files[i].guid;

			if (guid == UnityGUID() || ids.count (guid))
				continue;

			if (GetGUIDPersistentManager().IsConstantGUID (guid) || !AssetDatabase::Get().IsAssetAvailable(guid))
				continue;

			ids.insert(guid);
			newGUIDs.insert(guid);
		}
	}
	UNITY_DELETE(file,kMemTempAlloc);
}

set<UnityGUID> CollectAllDependencies (const set<UnityGUID>& selection)
{
	set<UnityGUID> guids;
	set<UnityGUID> newGUIDs = selection;

	while (!newGUIDs.empty())
	{
		set<UnityGUID> newGUIDsFrozen = newGUIDs;
		newGUIDs.clear();

		for (set<UnityGUID>::iterator i=newGUIDsFrozen.begin();i!=newGUIDsFrozen.end();i++)
		{
			UnityGUID guid = *i;
			if (GetGUIDPersistentManager().IsConstantGUID (guid) || !AssetDatabase::Get().IsAssetAvailable(guid))
				continue;

			guids.insert(guid);
			CollectGUIDDependencies(GetAssetPathFromGUID(guid), guids, newGUIDs);
			CollectGUIDDependencies(GetMetaDataPathFromGUID(guid), guids, newGUIDs);
		}
	}

	return guids;
}

void CollectAllDependencies (const TempSelectionSet& selection, TempSelectionSet& output )
{
	output.clear();
	set<SInt32> ids;
	DependencyCollector collector (&ids);

	for (TempSelectionSet::const_iterator i=selection.begin();i != selection.end ();i++)
	{
		if (*i == NULL)
			continue;

		int instanceID = (**i).GetInstanceID();
		collector.GenerateInstanceID(instanceID, kStrongPPtrMask);

		string pathName = GetPersistentManager().GetPathName(instanceID);
		pathName = GetGUIDPersistentManager().AssetPathNameFromAnySerializedPath(pathName);
		if (IsUnitySceneFile (pathName))
		{
			set<SInt32> fileDepencies;
			CollectSceneRootDependencies(pathName, fileDepencies);

			for (set<SInt32>::iterator j=fileDepencies.begin();j != fileDepencies.end();j++)
			{
				collector.GenerateInstanceID(*j, kStrongPPtrMask);
			}
		}
	}
	
	for (set<SInt32>::iterator i=ids.begin();i != ids.end ();i++)
	{
		Object* object = PPtr<Object> (*i);
		if (object)
			output.insert(object);
	}
}


void VerifyAllAssetsHaveAssignedTemporaryLocalIdentifierInFile (const InstanceIDToBuildAsset& assets)
{
	for (InstanceIDToBuildAsset::const_iterator j=assets.begin ();j != assets.end ();j++)
	{
		Assert (j->second.temporaryObjectIdentifier.localIdentifierInFile != 0);
	}
}

bool CompareAssetLoadOrderInstanceIDToBuildAsset (const BuildAsset& lhs, const BuildAsset& rhs)
{
	if (lhs.temporaryPathName == rhs.temporaryPathName)
	{
		int order = CompareAssetLoadOrderRaw (lhs.instanceID, lhs.classID, lhs.preloadSortIndex, rhs.instanceID, rhs.classID, rhs.preloadSortIndex);
		// Sort based on optimal asset load order
		if (order != 0)//
			return order < 0;
		else
		{
			// Sort based file id in the asset
			if (lhs.originalLocalIdentifierInFile != rhs.originalLocalIdentifierInFile)
				return lhs.originalLocalIdentifierInFile < rhs.originalLocalIdentifierInFile;
			else
			{
				string lhsOriginalpath = GetPersistentManager().GetPathName(lhs.instanceID);
				string rhsOriginalpath = GetPersistentManager().GetPathName(rhs.instanceID);

				// sort based on source file name
				if (lhsOriginalpath != rhsOriginalpath)
					return lhsOriginalpath < rhsOriginalpath;
				// Lastly try instance id, i dont think we can even end up here
				else
					return lhs.instanceID < rhs.instanceID;
			}
		}
	}
	// Sort based on the path name in which it is serialized
	else
		return lhs.temporaryPathName < rhs.temporaryPathName;
}


void AssignTemporaryLocalIdentifierInFileForAssets (const string& path, const InstanceIDToBuildAsset& assets)
{
	vector<BuildAsset> assetsSorted;
	assetsSorted.reserve(assets.size());

	// Sort all assets at this path for optimal loading order.
	for (InstanceIDToBuildAsset::const_iterator j=assets.begin ();j != assets.end ();j++)
	{
		if (j->second.temporaryPathName == path)
			assetsSorted.push_back(j->second);
	}
	sort(assetsSorted.begin(), assetsSorted.end(), CompareAssetLoadOrderInstanceIDToBuildAsset);

	// Loop through sorted assets array and assign temporaryObjectIdentifier in increasing order
	int serializedFileIndex = GetPersistentManager().GetSerializedFileIndexFromPath(path);
	LocalIdentifierInFileType localIdentifierInFile = 1;
	for (vector<BuildAsset>::iterator i=assetsSorted.begin ();i != assetsSorted.end ();i++)
	{
		const BuildAsset& srcFileInfo = assets.find(i->instanceID)->second;
		DebugAssert(srcFileInfo.temporaryPathName == path);
		if (srcFileInfo.temporaryObjectIdentifier.localIdentifierInFile != 0)
		{
			Assert(srcFileInfo.temporaryObjectIdentifier.localIdentifierInFile == localIdentifierInFile);
			Assert(srcFileInfo.temporaryObjectIdentifier.serializedFileIndex == serializedFileIndex);
		}

		srcFileInfo.temporaryObjectIdentifier.localIdentifierInFile = localIdentifierInFile;
		srcFileInfo.temporaryObjectIdentifier.serializedFileIndex = serializedFileIndex;

		localIdentifierInFile++;
	}
}

void AssignMissingTemporaryLocalIdentifierInFileForAssets (const std::string& path, InstanceIDToBuildAsset& assets)
{
	int serializedFileIndex = GetPersistentManager().GetSerializedFileIndexFromPath(path);
	LocalIdentifierInFileType startWith = 0;
	for (InstanceIDToBuildAsset::iterator j = assets.begin(); j != assets.end(); j++)
	{
		BuildAsset& asset = j->second;
		if (asset.temporaryPathName != path)
			continue;
		startWith = std::max(startWith, asset.temporaryObjectIdentifier.localIdentifierInFile);
	}

	for (InstanceIDToBuildAsset::iterator j = assets.begin(); j != assets.end(); j++)
	{
		BuildAsset& asset = j->second;
		if (asset.temporaryObjectIdentifier.localIdentifierInFile == 0)
		{
			asset.temporaryObjectIdentifier.localIdentifierInFile = (++startWith);
			asset.temporaryObjectIdentifier.serializedFileIndex = serializedFileIndex;
		}
	}
}


/* @TODO: Reintegrate this code
if (!loadable)
{
	ErrorString("Building player error, persistent asset has disappeared, asset path: " + AssetPathNameFromAnySerializedPath(i->originalPathName));
}
*/

BuildAsset& AddBuildAssetInfo (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, InstanceIDToBuildAsset& assets)
{
	Object* loadedObject = Object::IDToPointer(instanceID);

	// Save old pathName and fileID
	BuildAsset& info = assets[instanceID];
	info.instanceID = instanceID;
	info.classID = GetClassIDWithoutLoadingObject(instanceID);
	info.originalLocalIdentifierInFile = 0;
	info.buildPackageSortIndex = buildPackageSortIndex;
	info.preloadSortIndex = CalculateSortIndex(instanceID, info.classID);
	info.originalDirtyIndex = loadedObject != NULL ? loadedObject->GetPersistentDirtyIndex() : 0;

	info.temporaryPathName = temporaryPath;

	return info;
}

BuildAsset& AddBuildAssetInfoWithLocalIdentifier (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, LocalIdentifierInFileType fileID, InstanceIDToBuildAsset& assets)
{
	PersistentManager& pm = GetPersistentManager ();

	BuildAsset& info = AddBuildAssetInfo (instanceID, temporaryPath, buildPackageSortIndex, assets);
	info.originalLocalIdentifierInFile = pm.GetLocalFileID (instanceID);

	if (fileID != 0)
	{
		info.temporaryObjectIdentifier.serializedFileIndex = pm.GetSerializedFileIndexFromPath (temporaryPath);
		info.temporaryObjectIdentifier.localIdentifierInFile = fileID;
	}

#if LOCAL_IDENTIFIER_IN_FILE_SIZE != 32
	-- fix this
		AssertIf(info.temporaryFileID >= 1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE);
#endif

	return info;
}

void AddBuildAssetInfoChecked (SInt32 instanceID, const string& temporaryPath, int buildPackageSortIndex, InstanceIDToBuildAsset& assets, LocalIdentifierInFileType fileID, bool allowBuiltinsResources)
{
	// Skip if already in set.
	if (assets.find (instanceID) != assets.end ())
		return;

	// Grab asset path.
	PersistentManager& pm = GetPersistentManager ();
	string pathName = pm.GetPathName (instanceID);
	if (pathName.empty ())
	{
		Assert (!Object::IDToPointer (instanceID)->IsPersistent ());
		return;
	}

	// Skip asset if it is a built-in resource and we don't want them included.
	if (StrICmp (pathName, kResourcePath) == 0 && !allowBuiltinsResources)
		return;

	// Check for extra resources.
	if (StrICmp (pathName, kDefaultExtraResourcesPath) == 0
		&& IsAlwaysIncludedShaderOrDependency (instanceID))
	{
		// It is a resource in the extra-resources bundle.  We normally want to suck
		// these into bundles except if they're shaders that are on the "Always Include Shader"
		// list of the graphics settings (because then they're available as part of every
		// player build and putting them into bundles would just lead to duplication of
		// shaders).
		return;
	}

	// Add to set.
	AddBuildAssetInfoWithLocalIdentifier (instanceID, temporaryPath, buildPackageSortIndex, fileID, assets);
}

static void ClearGlobalManagersFromList (set<SInt32>& allObjects)
{
	// Ensure that no global managers are included in scenes
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
		allObjects.erase(GetManagerFromContext(i).GetInstanceID());
}

typedef map<PPtr<Object>, ResourceManager::Dependency> ObjectDependenciesCont;

void ClearGlobalManagersFromList (ObjectDependenciesCont& allObjects)
{
	// Ensure that no global managers are included in scenes
	set<SInt32> managers;
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
		managers.insert (GetManagerFromContext(i).GetInstanceID());

	// Clear from dependency containers
	for (ObjectDependenciesCont::iterator it = allObjects.begin (); it != allObjects.end (); )
	{
		set<SInt32>::iterator mgrIt = managers.find (it->first.GetInstanceID ());
		if (mgrIt != managers.end ())
		{
			allObjects.erase (it++);
		}
		else
		{
			// Check children
			ResourceManager::Dependency::ChildCont& depCont = it->second.dependencies;
			for (ResourceManager::Dependency::ChildCont::iterator cit = depCont.begin (); cit != depCont.end (); )
			{
				set<SInt32>::iterator mgrIt = managers.find (cit->GetInstanceID ());
				if (mgrIt != managers.end ())
					cit = depCont.erase (cit, cit + 1);
				else
					++cit;
			}

			++it;
		}
	}
}

static void StoreGlobalManagerDirtyIndex (InstanceIDToBuildAsset& assets)
{
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
		AddBuildAssetInfo(GetManagerFromContext(i).GetInstanceID(), "", 0, assets);
}

static void RestoreObjectDirtyIndex (const InstanceIDToBuildAsset& assets)
{
	for (InstanceIDToBuildAsset::const_iterator i=assets.begin ();i != assets.end ();i++)
	{
		Object* target = Object::IDToPointer(i->first);
		if (target == NULL)
			continue;

		target->SetPersistentDirtyIndex (i->second.originalDirtyIndex);
	}
}

bool CompileGameSceneDependencies (const string& pathName, const string& assetPathName, int buildPackageSortIndex, InstanceIDToBuildAsset& assets, set<int>& usedClassIDs, PreloadData* preload, BuildTargetPlatform platform, int options)
{
	bool success = true;
	ResetSerializedFileAtPath (pathName);

	set<SInt32> allObjects;

	WriteDataArray allSceneObjects;
	CalculateAllLevelManagersAndUsedSceneObjects (allSceneObjects, kSerializeGameRelease);

	// Use GameReleaseCollector to collect all required assets by recursively
	// checking every transferred pptr
	GameReleaseCollector collector (&allObjects);

	// Collect dependencies from an explicit set of global managers.
	// Ideally we'd like to simply iterate all game managers however this results in dependencies being found
	// that we don't want deployed (example: 'ScriptMapper' referring to shaders).

	if (options & kSaveGlobalManagers)
	{
		collector.GenerateInstanceID (GetManagerFromContext (ManagerContext::kMonoManager).GetInstanceID ());
		collector.GenerateInstanceID (GetManagerFromContext (ManagerContext::kPhysicsManager).GetInstanceID ());
		collector.GenerateInstanceID (GetManagerFromContext(ManagerContext::kGraphicsSettings).GetInstanceID());
		collector.GenerateInstanceID (GetManagerFromContext (ManagerContext::kPlayerSettings).GetInstanceID ());

		#if ENABLE_2D_PHYSICS
		collector.GenerateInstanceID (GetManagerFromContext (ManagerContext::kPhysics2DSettings).GetInstanceID ());
		#endif
	}

	// Collect core and on Demand objects and everything that they reference
	for (WriteDataArray::iterator i=allSceneObjects.begin ();i != allSceneObjects.end ();++i)
		collector.GenerateInstanceID (i->instanceID);

	// Ensure that no global managers are included in scenes
	ClearGlobalManagersFromList(allObjects);

	// Add all objects as dependent assets
	for (set<SInt32>::iterator i = allObjects.begin ();i != allObjects.end ();i++)
	{
		int instanceID = *i;

		AddBuildAssetInfoChecked (instanceID, assetPathName, buildPackageSortIndex, assets, 0, false);

		// Setup preload data. Add any assets that are used and already included in previous asset files.
		// Sorting of preload assets is done as a seperate steps once file id's have been determined
		if (assets.find(instanceID) != assets.end())
		{
			BuildAsset& info = assets.find(instanceID)->second;
			if (info.temporaryPathName != assetPathName)
			{
				AssertIf(find(preload->m_Assets.begin(), preload->m_Assets.end(), PPtr<Object> (instanceID)) != preload->m_Assets.end());
				preload->m_Assets.push_back(PPtr<Object> (instanceID));
			}
		}
		else
		{
			// Also store references to default resources so that they get preloaded
			string path = GetPersistentManager().GetPathName(instanceID);
			if (StrICmp(path, kDefaultResourcePathTemplate) == 0)
			{
				AssertIf(find(preload->m_Assets.begin(), preload->m_Assets.end(), PPtr<Object> (instanceID)) != preload->m_Assets.end());
				preload->m_Assets.push_back(PPtr<Object> (instanceID));
			}
		}
	}

	ComputeObjectUsage(allObjects, usedClassIDs, assets);

	return success;
}

struct SortPreloadAssetByFileIDFunc : binary_function<PPtr<Object>, PPtr<Object>, size_t>
{
	InstanceIDToBuildAsset* m_Assets;

	SortPreloadAssetByFileIDFunc (InstanceIDToBuildAsset& assets) : m_Assets (&assets) {}

	bool operator () (PPtr<Object> lhs, PPtr<Object> rhs) const
	{
		if (lhs.GetInstanceID() == rhs.GetInstanceID())
			return lhs.GetInstanceID() < rhs.GetInstanceID();

		LocalIdentifierInFileType lhsFileID = 0;
		LocalIdentifierInFileType rhsFileID = 0;

		int lhsPackageIndex;
		int rhsPackageIndex;

		int lhsPreloadSortIndex;
		int rhsPreloadSortIndex;

		const char* lhsTargetFileName;
		const char* rhsTargetFileName;

		// Find the left hand side asset path and file id
		BuildAsset* lhsInfo;
		if (m_Assets->find(lhs.GetInstanceID()) != m_Assets->end())
		{
			lhsInfo = &m_Assets->find(lhs.GetInstanceID())->second;
			lhsFileID = lhsInfo->temporaryObjectIdentifier.localIdentifierInFile;
			lhsPackageIndex = lhsInfo->buildPackageSortIndex;
			lhsPreloadSortIndex = lhsInfo->preloadSortIndex;
			lhsTargetFileName = lhsInfo->temporaryPathName.c_str();

			AssertIf(lhsPackageIndex == -1); // Linking to a project manager???
		}
		else
		{
			lhsFileID = GetPersistentManager().GetLocalFileID(lhs.GetInstanceID());
			lhsPackageIndex = -1; // default resources first
			lhsPreloadSortIndex = -1;

			string path = GetPersistentManager().GetPathName(lhs.GetInstanceID());
			if (StrICmp(path, kDefaultResourcePathTemplate) == 0)
				lhsTargetFileName = kDefaultResourcePathTemplate;
			else if (StrICmp (path, kDefaultExtraResourcesPath) == 0)
				lhsTargetFileName = kDefaultExtraResourcesPath;
			else
			{
				ErrorString(Format("Inconsistent asset when sorting preload assets: '%s' fileID: %d", path.c_str(), (int)lhsFileID));
				return lhs.GetInstanceID() < rhs.GetInstanceID();
			}
		}

		// Find the right hand side asset path and file id
		BuildAsset* rhsInfo;
		if (m_Assets->find(rhs.GetInstanceID()) != m_Assets->end())
		{
			rhsInfo = &m_Assets->find(rhs.GetInstanceID())->second;
			rhsFileID = rhsInfo->temporaryObjectIdentifier.localIdentifierInFile;
			rhsPackageIndex = rhsInfo->buildPackageSortIndex;
			rhsPreloadSortIndex = rhsInfo->preloadSortIndex;
			rhsTargetFileName = rhsInfo->temporaryPathName.c_str();

			AssertIf(rhsPackageIndex == -1); // Linking to a project manager???
		}
		else
		{
			rhsFileID = GetPersistentManager().GetLocalFileID(rhs.GetInstanceID());
			rhsPackageIndex = -1; // default resources first
			rhsPreloadSortIndex = -1;
			string path = GetPersistentManager().GetPathName(rhs.GetInstanceID());
			if (StrICmp(path, kDefaultResourcePathTemplate) == 0)
				rhsTargetFileName = kDefaultResourcePathTemplate;
			else if (StrICmp (path, kDefaultExtraResourcesPath) == 0)
				rhsTargetFileName = kDefaultExtraResourcesPath;
			else
			{
				ErrorString(Format("Inconsistent asset when sorting preload assets: '%s' fileID: %d", path.c_str(), (int)rhsFileID));
				return lhs.GetInstanceID() < rhs.GetInstanceID();
			}
		}

		AssertIf (rhsFileID == 0 || lhsFileID == 0);

		// First sort by package (so we load in the order we generate files)
		if (lhsPackageIndex != rhsPackageIndex)
			return lhsPackageIndex < rhsPackageIndex;

		// Sort by target file name
		int fileCompareResult = strcmp(lhsTargetFileName, rhsTargetFileName);
		if (fileCompareResult != 0)
			return fileCompareResult < 0;

		// Then sort by file ID
		if (lhsFileID != rhsFileID)
			return lhsFileID < rhsFileID;

		ErrorString("Inconsistent asset when sorting preload assets (Same path & file ID) ");

		return lhs.GetInstanceID() < rhs.GetInstanceID();
	}
};

void SortPreloadAssetsByFileID (vector<PPtr<Object> >& preload, InstanceIDToBuildAsset& assets)
{
	SortPreloadAssetsByFileID(preload, 0, preload.size(), assets);
}

void SortPreloadAssetsByFileID (vector<PPtr<Object> >& preload, int start, int size, InstanceIDToBuildAsset& assets)
{
	SortPreloadAssetByFileIDFunc sorter (assets);

	sort(preload.begin() + start, preload.begin() + start + size, sorter);

	int previous = 0;
	for (int i=start;i<start + size;i++)
	{
		AssertIf(preload[i].GetInstanceID() == previous);
		previous = preload[i].GetInstanceID();
		//		printf_console("Preload %s %s\n", preload[i]->GetName().c_str(), preload[i]->GetClassName().c_str());
	}
}

#define DEBUG_BUILDING_DEPENDENCY_TREE 0

class DependencyTreeCollector : public GenerateIDFunctor
{
public:
	ResourceManager::Dependency m_EmptyRoot;
	std::vector<ResourceManager::Dependency*> m_VisitedNodesStack;
	ObjectDependenciesCont& m_PPtrs;

	DependencyTreeCollector (ObjectDependenciesCont& pptrs)
	:	m_PPtrs (pptrs)
	{
		m_VisitedNodesStack.push_back (&m_EmptyRoot);
	}

	virtual ~DependencyTreeCollector () {}

	// Returns a pointer to the newly created Dependency object (with instanceID as id)
	ResourceManager::Dependency* InsertDependency (int instanceID, ResourceManager::Dependency& intoThis)
	{
		ResourceManager::Dependency info(instanceID);
		ResourceManager::Dependency* dep = &m_PPtrs.insert (std::make_pair (instanceID, info)).first->second;
		
		ResourceManager::Dependency::ChildCont::const_iterator found = find(intoThis.dependencies.begin(), intoThis.dependencies.end(), PPtr<Object> (instanceID));
		if (found == intoThis.dependencies.end())
		intoThis.dependencies.push_back (PPtr<Object> (instanceID));
		
		return dep;
	}

	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag = kNoTransferFlags)
	{
		int classID = GetClassIDWithoutLoadingObject (oldInstanceID);
		if (classID != -1 && !HasClassGameReleaseDependencies(classID))
		{
			ResourceManager::Dependency* parentDep = m_VisitedNodesStack.back ();

#if DEBUG_BUILDING_DEPENDENCY_TREE
			int indent = m_VisitedNodesStack.size () - 1;
			if (Object* obj = Object::IDToPointer (oldInstanceID))
				printf_console ("%*sAdding %s (%s) at path %s\n", indent * 3, "", obj->GetName(), obj->GetClassName().c_str(), GetPersistentManager().GetPathName(oldInstanceID).c_str());
			else
				printf_console ("%*sAdding <without-loading> (%s) at path %s\n", indent * 3, "", Object::ClassIDToString(classID).c_str(), GetPersistentManager().GetPathName(oldInstanceID).c_str());
#endif
			InsertDependency (oldInstanceID, *parentDep);
			return oldInstanceID;
		}

		Object* targetObject = dynamic_instanceID_cast<Object*> (oldInstanceID);
		if (targetObject == NULL || targetObject->GetClassID() >= ClassID(SmallestEditorClassID))
			return oldInstanceID;

		// Persistent pointers get all their dependencies resolved
		// Selecting something in the scene, we only do it for strong pointers.
		// Eg. game object -> component or transform children but not transform parent.
		if (!targetObject->IsPersistent())
			return oldInstanceID;

		bool didProcessAlready = m_PPtrs.count (PPtr<Object> (oldInstanceID));

		// insert this node
		ResourceManager::Dependency* parentDep = m_VisitedNodesStack.back ();
		ResourceManager::Dependency* dep = InsertDependency (oldInstanceID, *parentDep);

#if DEBUG_BUILDING_DEPENDENCY_TREE
		int indent = m_VisitedNodesStack.size ();
		printf_console ("%*sAdding %s (%s) at path %s\n", indent * 3, "", targetObject->GetName(), targetObject->GetClassName().c_str(), GetPersistentManager().GetPathName(targetObject->GetInstanceID()).c_str());
#endif

		// If we've processed this id already, skip it!
		if (didProcessAlready)
			return oldInstanceID;
		
		m_VisitedNodesStack.push_back (dep);

		RemapPPtrTransfer transferFunction (kBuildPlayerOnlySerializeBuildProperties | kSerializeGameRelease , false);
		transferFunction.SetGenerateIDFunctor (this);
		targetObject->VirtualRedirectTransfer (transferFunction);

		m_VisitedNodesStack.pop_back ();

		return oldInstanceID;
	}
};

void BuildDependencies(ObjectDependenciesCont& instances, ResourceManager::DependencyContainer& out)
{
	for (ObjectDependenciesCont::const_iterator  i=instances.begin();i != instances.end();++i)
	{
		Assert(i->second.object.GetInstanceID() != 0);

		if (i->second.dependencies.empty())
			continue;
		out.push_back(i->second);
	}
}

bool CompileGameResourceManagerDependencies (const string& assetPathName, int buildPackageSortIndex, BuildTargetPlatform platform, InstanceIDToBuildAsset& assets, set<int>& usedClassIds, set<string>& outFilenames, bool splitResources)
{
	// Use GameReleaseCollector to collect all required assets by recursively
	// checking every transferred pptr
	ObjectDependenciesCont allObjects;
	DependencyTreeCollector treeCollector (allObjects);

	for (ResourceManager::range r = GetResourceManager ().GetAll (); r.first != r.second; ++r.first)
	{
		SInt32 iid = r.first->second.GetInstanceID ();
		if (treeCollector.m_PPtrs.find (PPtr<Object>(iid)) == treeCollector.m_PPtrs.end ())
			treeCollector.GenerateInstanceID (r.first->second.GetInstanceID ());
	}

	// Ensure that no global managers are included in scenes
	ClearGlobalManagersFromList(treeCollector.m_PPtrs);

	// We'll need a std::set later
	std::set<SInt32> allObjectInstanceIDs;
	for (ObjectDependenciesCont::iterator i = allObjects.begin ();i != allObjects.end (); i++)
	{
		Assert (i->first.GetInstanceID () != 0);
		allObjectInstanceIDs.insert (i->first.GetInstanceID ());

		string outputName;
		if (splitResources)
			outputName = GUIDToString(ObjectToGUID(i->first));
		else
			outputName = assetPathName;

		AddBuildAssetInfoChecked (i->first.GetInstanceID (), outputName, buildPackageSortIndex, assets, 0, false);
		outFilenames.insert(outputName);
	}

	// Build resources files for all assets in the "Assets/Resources" folder
	for (set<string>::const_iterator it = outFilenames.begin(); it != outFilenames.end(); ++it)
		AssignTemporaryLocalIdentifierInFileForAssets (*it, assets);

	// Refresh dependencies of objects in resource manager
	ResourceManager& rm = GetResourceManager ();
	rm.ClearDependencyInfo ();
	rm.m_DependentAssets.reserve (treeCollector.m_PPtrs.size ());
	BuildDependencies (treeCollector.m_PPtrs, rm.m_DependentAssets);
	std::sort (rm.m_DependentAssets.begin (), rm.m_DependentAssets.end (), ResourceManager::Dependency::Sorter ());

	// Compute mesh usage - (Compute which meshes use a physx mesh collider)
	// Assets have already been processed, this computes it for assets stored in the scene
	ComputeObjectUsage(allObjectInstanceIDs, usedClassIds, assets);

	return true;
}


static void CalculateAllLevelManagersAndUsedSceneObjects (WriteDataArray& output, TransferInstructionFlags instructions)
{
	// Get all objects in the scene
	InstanceIDArray managers;
	InstanceIDArray sceneObjects;

	CollectLevelGameManagers (managers);
	CollectUsedSceneObjects (sceneObjects, instructions);

	InstanceIDArray serializeInstanceIDs;
	serializeInstanceIDs.insert(serializeInstanceIDs.end(), managers.begin(), managers.end());
	serializeInstanceIDs.insert(serializeInstanceIDs.end(), sceneObjects.begin(), sceneObjects.end());

	SetupFileIDsForObjects (serializeInstanceIDs, output, instructions);
}

static void LinearizeLocalIdentifierInFile (WriteDataArray& output)
{
	// Generate tightly packed id range
	LocalIdentifierInFileType index = 1;
	for (WriteDataArray::iterator i=output.begin(); i != output.end();i++)
	{
		i->localIdentifierInFile = index;
		index++;
	}
}

void PrependGlobalManagersToAllSceneObjects (WriteDataArray& allObjects)
{
	// Make space for the global managers in the fileID's
	for (int i=0;i<allObjects.size();i++)
		allObjects[i].localIdentifierInFile += ManagerContext::kGlobalManagerCount;

	WriteData managerValues[ManagerContext::kGlobalManagerCount];

	// Save eg. ProjectSettings, InputManager, AudioManager, TagManager.
	// They are all global managers and stored in the mainData file.
	for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
	{
		managerValues[i].localIdentifierInFile = i + 1;
		managerValues[i].instanceID = GetManagerFromContext (i).GetInstanceID ();
	}
	allObjects.insert(allObjects.begin(), managerValues, managerValues + ManagerContext::kGlobalManagerCount);
}



bool CompileGameScene (const string& targetPath, InstanceIDToBuildAsset& assets, set<int>& usedClassIds, BuildTargetPlatform targetPlatform, int options, const vector<UnityStr>& playerAssemblyNames)
{
	AssertIf (targetPath.empty ());

	PersistentManager& pm = GetPersistentManager ();

	ResetSerializedFileAtPath (targetPath);

	WriteDataArray allSceneObjects;
	CalculateAllLevelManagersAndUsedSceneObjects(allSceneObjects, kSerializeGameRelease);

	// Sort based on optimal scene order
	stable_sort(allSceneObjects.begin(), allSceneObjects.end(), CompareAssetLoadOrderForBuildingScene);

	LinearizeLocalIdentifierInFile(allSceneObjects);

	// Ensure that everything we are about to save to the scene and all their dependencies are loaded.
	// Otherwise some references might get lost when writing due to kWriteNULLWhenNotLoaded when we write the scene file
	set<SInt32> allObjects;
	GameReleaseCollector collector (&allObjects);
	for (WriteDataArray::iterator i=allSceneObjects.begin ();i != allSceneObjects.end ();++i)
		collector.GenerateInstanceID(i->instanceID);

	ClearGlobalManagersFromList(allObjects);

	vector<UnityStr> oldAssemblyNames = GetMonoManager().GetRawAssemblyNames();
	if (!playerAssemblyNames.empty())
		GetMonoManager().GetRawAssemblyNames() = playerAssemblyNames;

	if (options & kSaveGlobalManagers)
	{
		for (int i=0;i<ManagerContext::kGlobalManagerCount;i++)
		{
			int instanceID = GetManagerFromContext (i).GetInstanceID ();
			collector.GenerateInstanceID(instanceID);
		}
	}

	// Compute mesh usage - (Compute which meshes use a physx mesh collider)
	// Assets have already been processed, this computes it for assets stored in the scene
	ComputeObjectUsage(allObjects, usedClassIds, assets);

	ScriptMapper::Shaders originalScriptMapperShaderState;
	InstanceIDToBuildAsset restoreGlobalManagerDirtyIndex;

	// Persist global managers
	if (options & kSaveGlobalManagers)
	{
		PrependGlobalManagersToAllSceneObjects(allSceneObjects);
		StoreGlobalManagerDirtyIndex(restoreGlobalManagerDirtyIndex);

		// Network manager needs some special processing
		GetNetworkManager().SetAssetToPrefab(BuildGUIDToPrefabList(assets));

		// We need to strip all shaders that are not included from the build from the script mapper
		originalScriptMapperShaderState = GetScriptMapper().GetShaders();

		ScriptMapper::Shaders newShaders = originalScriptMapperShaderState;
		vector<PPtr<Shader> > removes;
		vector<PPtr<Shader> > all = newShaders.GetAllObjects();

		for (int i=0;i<all.size();i++)
		{
			// Assets that are included need to be registered with script mapper
			if (assets.count(all[i].GetInstanceID()) != 0)
				continue;
			// Builtin shaders all need to be registered with script mapper
			if (IsAnyDefaultResourcesObject(all[i].GetInstanceID()))
				continue;

			removes.push_back(all[i]);
		}

		for (int i=0;i<removes.size();i++)
		{
			newShaders.Remove(removes[i]);
		}

		GetScriptMapper().SetShaders(newShaders);
	}

	Assert(options & kSerializeGameRelease);
	Assert(options & kBuildPlayerOnlySerializeBuildProperties);


	InstanceIDBuildRemap writeRemap;
	ConvertAssetsToInstanceIDBuildRemap (assets, writeRemap);
	ConvertSceneObjectsToInstanceIDBuildRemap (targetPath, allSceneObjects, writeRemap);

	pm.Lock();
	SetInstanceIDResolveCallback (BuildingPlayerOrAssetBundleInstanceIDResolveCallback, &writeRemap);
	BuildTargetSelection targetSelection(targetPlatform,0); //@TODO: take currently set subtarget?
	int error = pm.WriteFileInternal (targetPath, pm.GetSerializedFileIndexFromPath(targetPath), &allSceneObjects[0], allSceneObjects.size(), &VerifyDeployment, targetSelection, options);
	SetInstanceIDResolveCallback (NULL);
	pm.Unlock();

	// Cleanup network manager special prefab map.
	if (options & kSaveGlobalManagers)
	{
		GetNetworkManager().SetAssetToPrefab(NetworkManager::AssetToPrefab ());
		GetScriptMapper().SetShaders(originalScriptMapperShaderState);
	}

	GetMonoManager().GetRawAssemblyNames() = oldAssemblyNames;

	RestoreObjectDirtyIndex (assets);
	RestoreObjectDirtyIndex (restoreGlobalManagerDirtyIndex);

	return error == kNoError;

	//     for (set<SInt32>::iterator i = allObjects.begin ();i != allObjects.end ();i++)
	//             AssertIf (!PPtr<Object> (*i));
}

/// From the assets included in the build. Creates a list of guid to pptr.
/// Only include root GameObject prefabs
/// This is used by the network manager for network instantiate
NetworkManager::AssetToPrefab BuildGUIDToPrefabList (InstanceIDToBuildAsset& assets)
{
	NetworkManager::AssetToPrefab guidToPrefab;
	for (InstanceIDToBuildAsset::iterator i=assets.begin();i!=assets.end();i++)
	{
		if (i->second.classID != ClassID(GameObject))
			continue;

		UnityGUID assetGUID = ObjectToGUID(PPtr<Object> (i->first));

		// Make sure that we are referencing an asset and it is the main game object
		const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(assetGUID);
		if (asset == NULL)
			continue;
		if (asset->mainRepresentation.object.GetInstanceID() != i->first)
			continue;
		AssertIf (asset->mainRepresentation.classID != ClassID(GameObject));

		guidToPrefab[assetGUID] = PPtr<GameObject> (i->first);
	}
	return guidToPrefab;
}

bool ResetSerializedFileAtPath (const string& assetPath)
{
	if (!GetPersistentManager ().DeleteFile (assetPath, PersistentManager::kDontDeleteLoadedObjects))
		return false;
	GetPersistentManager ().ResetHighestFileIDAtPath (assetPath);
	return true;
}

#if NESTED_PREFAB_SUPPORT
// Instead of writing the prefabs themselves we instantiate the prefabs and write the instantiated prefab instead.
// This way we have a fully nested prefab with bubbled up properties, that the Instantiate function in the player can instantiate.

void PatchNestedPrefabInstances (const vector<Prefab*>& prefabsToBePatched, int serializedFileIndex, const InstanceIDToBuildAsset& assets, WriteDataArray& output, InstanceIDBuildRemap& outputWriteRemap)
{
	LocalIdentifierInFileType highestLocalIdentifier = 0;
	for (int i=0;i<output.size();i++)
		highestLocalIdentifier = max(highestLocalIdentifier, output[i].localIdentifierInFile);

	vector_map<int, int> prefabRemap;

	for (int i=0;i<prefabsToBePatched.size();i++)
	{
		Prefab& nestedPrefab = *prefabsToBePatched[i];

		Prefab* prefabInstance = dynamic_pptr_cast<Prefab*> (InstantiatePrefab(&nestedPrefab, false));
		if (prefabInstance == NULL || !prefabInstance->GetRootGameObject().IsValid())
			continue;

		GameObject& root = *prefabInstance->GetRootGameObject();
		dynamic_array<Object*> instances;
		CollectObjectHierarchy(root, instances);

		for (int i=0;i<instances.size();i++)
		{
			Object* instanceObject = instances[i];
			Object* prefabObject = GetPrefabParentObject(instances[i]);
			if (prefabObject == NULL)
				continue;

			// Any gameobjects & components in the root prefab will be remapped
			if (GetPrefabFromAnyObjectInPrefab(instanceObject) == prefabInstance)
			{
				prefabRemap.push_unsorted(prefabObject->GetInstanceID(), instanceObject->GetInstanceID());

				InstanceIDToBuildAsset::const_iterator buildAsset = assets.find(prefabObject->GetInstanceID());
				Assert(buildAsset != assets.end());

				outputWriteRemap.push_unsorted(instanceObject->GetInstanceID(), buildAsset->second.temporaryObjectIdentifier);
			}
			// Nested prefab gameobject & components will be added as new objects to the output
			else
			{
				///@TODO: How does this deal with dependencies of the nested prefabs getting included or not included... ???
				///       Write test where nested prefab is pulling in a mesh or something.

				highestLocalIdentifier++;
				WriteData writeData;
				writeData.localIdentifierInFile = highestLocalIdentifier;
				writeData.instanceID = instanceObject->GetInstanceID();

				output.push_back(writeData);

				outputWriteRemap.push_unsorted (instanceObject->GetInstanceID(), SerializedObjectIdentifier(serializedFileIndex, highestLocalIdentifier));
			}
		}
	}

	prefabRemap.sort();

	// Write the nested prefab instances instead of the actual prefab
	for (int i=0;i<output.size();i++)
	{
		vector_map<int, int>::iterator found = prefabRemap.find(output[i].instanceID);

		if (found == prefabRemap.end())
			continue;

		output[i].instanceID = found->second;
	}
}
#endif

static bool ExtractAssetsToWrite (const string& targetPath, const InstanceIDToBuildAsset& assets, WriteDataArray& output, InstanceIDBuildRemap& outputWriteRemap)
{
	vector_set<Prefab*> allNestedPrefabs;

	for (InstanceIDToBuildAsset::const_iterator i=assets.begin();i != assets.end();i++)
	{
		if (i->second.temporaryPathName != targetPath)
			continue;

		int instanceID = i->first;
		LocalIdentifierInFileType localIdentifier = i->second.temporaryObjectIdentifier.localIdentifierInFile;
		Assert(localIdentifier != 0);

		// Load all assets that might have other dependencies.
		// This is so that only assets which have no dependencies are loaded while writing.
		// This way we can safely delete them immediately after loading them.
		if (HasClassGameReleaseDependencies(i->second.classID))
		{
			Object* targetObject = dynamic_instanceID_cast<Object*> (instanceID);
			if (targetObject == NULL)
			{
				ErrorString("Asset has disappeared while building player: " + GetAssetPathFromInstanceID(instanceID));
				return false;
			}

			#if NESTED_PREFAB_SUPPORT
			Prefab* prefab = GetPrefabFromAnyObjectInPrefab(targetObject);
			///@TODO: Try running this without the nested prefab check to get more broad test coverage in real games.
			if (prefab != NULL && DoesPrefabHasNesting(*prefab))
				allNestedPrefabs.push_unsorted(prefab);
			#endif
		}

		output.push_back(WriteData (localIdentifier, instanceID, i->second.buildUsage));
	}

	// Build basic asset remap
	ConvertAssetsToInstanceIDBuildRemap (assets, outputWriteRemap);

#if NESTED_PREFAB_SUPPORT

	allNestedPrefabs.sort_clear_duplicates();

	// Patch basic asset remap
	int serializedFileIndex = GetPersistentManager().GetSerializedFileIndexFromPath(targetPath);
	PatchNestedPrefabInstances(allNestedPrefabs.get_vector(), serializedFileIndex, assets, output, outputWriteRemap);
#endif

	sort(output.begin(), output.end());

	return true;
}

string BuildCustomAssetBundle (PPtr<Object> mainAsset, const std::vector<PPtr<Object> >& objects, const std::vector<std::string>* overridePaths, const std::string& targetPath, int buildPackageSortIndex, InstanceIDToBuildAsset& assets, BuildTargetPlatform platform, TransferInstructionFlags flags, BuildAssetBundleOptions assetBundleOptions)
{
	AssetBundleBuilder builder (assets);
	return builder.BuildCustomAssetBundle (mainAsset, objects, overridePaths, targetPath, buildPackageSortIndex, platform, flags, assetBundleOptions);
}

//////@TODO: Review enum passing for all flags passed to WriteFileInternal

bool WriteSharedAssetFile (const string& targetPath, const InstanceIDToBuildAsset& assets, BuildTargetSelection target, InstanceIDResolveCallback* resolveCallback, int flags)
{
	PersistentManager& pm = GetPersistentManager ();
	ResetSerializedFileAtPath (targetPath);
	WriteDataArray assetsWriteData;

	InstanceIDBuildRemap writeRemap;

	if (!ExtractAssetsToWrite(targetPath, assets, assetsWriteData, writeRemap))
		return false;

	pm.Lock();
	SetInstanceIDResolveCallback (resolveCallback, &writeRemap);
	int error = pm.WriteFileInternal (targetPath, pm.GetSerializedFileIndexFromPath(targetPath), &assetsWriteData[0], assetsWriteData.size(), &VerifyDeployment, target, flags);
	SetInstanceIDResolveCallback (NULL);
	pm.Unlock();

	if (error != kNoError)
		ErrorString("Building - Failed to write file: " + targetPath);

	RestoreObjectDirtyIndex (assets);

	return error == kNoError;
}

///@TODO: Handle return value
bool CompileSharedAssetsFile (const string& assetPath, const InstanceIDToBuildAsset& assets, BuildTargetPlatform platform, int options)
{
	BuildTargetSelection targetSelection(platform,0); //@TODO: take currently set/passed subtarget?
	return WriteSharedAssetFile(assetPath, assets, targetSelection, &BuildingPlayerOrAssetBundleInstanceIDResolveCallback, options | kSerializeGameRelease | kBuildPlayerOnlySerializeBuildProperties | kLoadAndUnloadAssetsDuringBuild);
}

UInt32 CalculateRequiredVertexComponents (Material& material, bool lightmapped)
{
	Shader* shader = material.GetShader();
	if (shader == NULL)
		return 0;

	return shader->CalculateUsedVertexComponents(lightmapped);
}

BuildUsageTag* GetBuildUsageTagFromAssets (int instanceID, InstanceIDToBuildAsset& assets)
{
	InstanceIDToBuildAsset::iterator found = assets.find(instanceID);
	if (found != assets.end())
		return &found->second.buildUsage;
	else
		return NULL;
}


BuildUsageTag* GetBuildUsageTagFromAssets (Object& obj, InstanceIDToBuildAsset& assets)
{
	InstanceIDToBuildAsset::iterator found = assets.find(obj.GetInstanceID());
	if (found != assets.end())
		return &found->second.buildUsage;
	else
		return NULL;
}

PPtr<Mesh> GetMeshFromRenderer (Renderer& renderer)
{
	MeshRenderer* meshRenderer = dynamic_pptr_cast<MeshRenderer*>(&renderer);
	if (meshRenderer)
		return meshRenderer->GetSharedMesh();

	SkinnedMeshRenderer* skinnedMeshRenderer = dynamic_pptr_cast<SkinnedMeshRenderer*>(&renderer);
	if (skinnedMeshRenderer)
		return skinnedMeshRenderer->GetMesh();

	return PPtr<Mesh> ();
}

void ComputeRendererSupportedMeshChannelsAndReadable (Renderer* renderer, InstanceIDToBuildAsset& assets)
{
	if (renderer == NULL)
		return;

	PPtr<Mesh> mesh = GetMeshFromRenderer (*renderer);
	if (!mesh)
		return;

	if (GetPlayerSettings().GetEditorOnly().stripUnusedMeshComponents)
	{

		UInt32 requiredVertexComponents = 0;
		const Renderer::MaterialArray& materials = renderer->GetMaterialArray();
		const bool lightmapped = renderer->IsLightmappedForRendering();
		for (int m=0;m<materials.size();m++)
		{
			Material* material = materials[m];
			if (material)
				requiredVertexComponents |= CalculateRequiredVertexComponents(*material, lightmapped);
		}

		BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(mesh.GetInstanceID(), assets);
		if (usageTag)
			usageTag->meshSupportedChannels |= requiredVertexComponents;
	}

	if (IsNonUniformScaleTransform(renderer->GetTransformInfo().transformType))
	{
		// When we encounter a non-uniform scaled mesh, it must be readable.
		BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(mesh.GetInstanceID(), assets);
		if (dynamic_pptr_cast<MeshRenderer*> (renderer) != NULL && usageTag)
		{
			usageTag->meshUsageFlags |= kMeshMustKeepVertexAndIndexData;
		}
	}
}

void ComputeMeshColliderUsageFlags (MeshCollider* meshCollider, InstanceIDToBuildAsset& assets)
{
	if (meshCollider == NULL)
		return;
	Mesh* mesh = meshCollider->GetSharedMesh();
	if (mesh == NULL)
		return;
	BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(*mesh, assets);
	if (usageTag == NULL)
		return;

	Matrix4x4f temp;
	TransformType scaleType = meshCollider->GetComponent(Transform).CalculateTransformMatrix(temp);
	if (IsNoScaleTransform(scaleType))
	{
		if (meshCollider->GetConvex())
			usageTag->meshUsageFlags |= kRequiresSharedConvexCollisionMesh;
		else
			usageTag->meshUsageFlags |= kRequiresSharedTriangleCollisionMesh;
	}
	else
	{
		usageTag->meshUsageFlags |= kRequiresScaledCollisionMesh;
		usageTag->meshUsageFlags |= kMeshMustKeepVertexAndIndexData;
	}
}

void ComputeParticleSystemUsageFlags (ParticleSystem* particleSystem, InstanceIDToBuildAsset& assets)
{
	if (particleSystem == NULL)
		return;

	for (int i=0;i<ParticleSystemRendererData::kMaxNumParticleMeshes;i++)
	{
		int meshInstanceID = particleSystem->GetShapeModule().GetMeshEmitterShape().GetInstanceID();
		BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(meshInstanceID, assets);
		if (usageTag)
			usageTag->meshUsageFlags |= kMeshMustKeepVertexAndIndexData;
	}
}

void ComputeParticleSystemRendererUsageFlags (ParticleSystemRenderer* particleSystem, InstanceIDToBuildAsset& assets)
{
	if (particleSystem == NULL)
		return;

	for (int i=0;i<ParticleSystemRendererData::kMaxNumParticleMeshes;i++)
	{
		int meshInstanceID = particleSystem->GetMeshes ()[i].GetInstanceID();
		BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(meshInstanceID, assets);
		if (usageTag)
			usageTag->meshUsageFlags |= kMeshMustKeepVertexAndIndexData;
	}
}



// Mark textures used by terrain to always be readable
void ComputeTerrainTexturesReadableFlags (TerrainData* data, InstanceIDToBuildAsset& assets)
{
	if (data == NULL)
		return;

	const vector<SplatPrototype>& splats = data->GetSplatDatabase().GetSplatPrototypes();
	TempSelectionSet sources;
	for (int i=0;i<splats.size();i++)
	{
		sources.insert(splats[i].texture);
		// note: no need to mark normal maps as readable; they aren't used for basemap computation
	}

	const vector<DetailPrototype>& detailPrototypes = data->GetDetailDatabase().GetDetailPrototypes();
	for (int i=0;i<detailPrototypes.size();i++)
	{
		if (detailPrototypes[i].usePrototypeMesh)
			sources.insert(detailPrototypes[i].prototype);
		else
			sources.insert(detailPrototypes[i].prototypeTexture);
	}
	TempSelectionSet sourcedeps;

	CollectAllDependencies(sources,sourcedeps);
	
	for (TempSelectionSet::iterator i=sourcedeps.begin();i!=sourcedeps.end();i++)
	{
		Texture2D* tex = dynamic_pptr_cast<Texture2D*> (*i);
		if (tex)
		{
			BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(*tex, assets);
			if (usageTag != NULL)
				usageTag->forceTextureReadable = true;
		}
	}
}


////////////******************************* @TODO: What if we want to strip scene asset data????


// Mark textures used by terrain to always be readable
void ComputeProceduralMaterialFlags (ProceduralMaterial* material, InstanceIDToBuildAsset& assets)
{
	if (material == NULL)
		return;

	SubstanceInputs::iterator it;
	for (it=material->GetSubstanceInputs().begin();it!=material->GetSubstanceInputs().end();++it)
	{
		Texture2D* texture = it->value.texture;
		if (texture != NULL)
		{
			BuildUsageTag* usageTag = GetBuildUsageTagFromAssets(*texture, assets);
			if (usageTag)
				usageTag->forceTextureReadable = true;
		}
	}
}

template<class T>
T* LoadObjectIfInheritsFromClassID (int instanceID, int classID)
{
	if (Object::IsDerivedFromClassID(classID, T::GetClassIDStatic()))
		return dynamic_instanceID_cast<T*> (instanceID);
	else
		return NULL;
}

// A lot of additional code is spread around here trying to prevent loading of textures.
// Loading textures increases memory usage during building massively to the point of crashing Unity.
// Also it increases build time a lot.

void ComputeObjectUsage (const set<SInt32>& allObjects, set<int>& usedClassIds, InstanceIDToBuildAsset& assets)
{
	for (set<SInt32>::const_iterator i=allObjects.begin();i != allObjects.end();i++)
	{
		int instanceID = *i;
		int classID = GetClassIDWithoutLoadingObject(instanceID);

		if (classID != -1)
			usedClassIds.insert(classID);
		else
		{
			AssertString("ClassID could not be determined. Object disappeared while building?");
		}

		// Mark meshes that will be used by mesh collider with the proper mesh usage flags
		ComputeMeshColliderUsageFlags(LoadObjectIfInheritsFromClassID<MeshCollider>(instanceID, classID), assets);

		// Compute what mesh channels are actually used
		ComputeRendererSupportedMeshChannelsAndReadable(LoadObjectIfInheritsFromClassID<Renderer>(instanceID, classID), assets);

		// Mark textures used by terrain to always be readable
		ComputeTerrainTexturesReadableFlags(LoadObjectIfInheritsFromClassID<TerrainData>(instanceID, classID), assets);

		// Register procedural material
		ComputeProceduralMaterialFlags(LoadObjectIfInheritsFromClassID<ProceduralMaterial>(instanceID, classID), assets);

		// Register particle system usage flags
		ComputeParticleSystemUsageFlags(LoadObjectIfInheritsFromClassID<ParticleSystem>(instanceID, classID), assets);

		// Register particle system renderer usage flags
		ComputeParticleSystemRendererUsageFlags(LoadObjectIfInheritsFromClassID<ParticleSystemRenderer>(instanceID, classID), assets);
	}
}

static string GetPathNameFromInstanceID (int instanceID)
{
	PersistentManager& pm = GetPersistentManager ();
	return pm.GetPathName (instanceID);
}

bool IsDefaultResourcesObject (int instanceID)
{
	return StrICmp (GetPathNameFromInstanceID (instanceID).c_str (), kDefaultResourcePathTemplate) == 0;
}

bool IsExtraResourcesObject (int instanceID)
{
	return StrICmp (GetPathNameFromInstanceID (instanceID).c_str (), kDefaultExtraResourcesPathTemplate) == 0;
}

bool IsAnyDefaultResourcesObject (int instanceID)
{
	PersistentManager& pm = GetPersistentManager ();
	string pathName = pm.GetPathName (instanceID);
	return
		StrICmp(pathName.c_str(), kDefaultResourcePathTemplate) == 0 ||
		StrICmp(pathName.c_str(), kDefaultExtraResourcesPathTemplate) == 0 ||
		StrICmp(pathName.c_str(), kOldWebResourcePathTemplate) == 0;
}

bool IsAlwaysIncludedShaderOrDependency (int instanceID)
{
	// Check whether it's even a shader (without actually loading the object).
	int classID = GetClassIDWithoutLoadingObject (instanceID);
	if (classID == -1 || !Object::IsDerivedFromClassID (classID, ClassID (Shader)))
		return false;

	// Try to find the shader in the always-included list or in the dependencies
	// of one of the shaders on that list.
	PPtr<Shader> shader (instanceID);
	const GraphicsSettings::ShaderArray& alwaysIncludedShaders = GetGraphicsSettings ().GetAlwaysIncludedShaders ();
	for (int i = 0; i < alwaysIncludedShaders.size (); ++i)
	{
		const PPtr<Shader> element = alwaysIncludedShaders[i];
		if (element.IsNull ())
			continue;

		if (element.GetInstanceID () == instanceID ||
		    element->IsDependentOn (shader))
			return true;
	}

	return false;
}

void CreateScriptCompatibilityInfo (AssetBundle& assetBundle, InstanceIDToBuildAsset& assets, string const& targetPath)
{
	for (InstanceIDToBuildAsset::iterator it = assets.begin (), end = assets.end (); it != end; ++it)
	{
		if (MonoScript* script = dynamic_instanceID_cast<MonoScript*>(it->first))
		{
			BuildAsset const& oldFileInfo = it->second;

			if ((targetPath.empty () || targetPath == oldFileInfo.temporaryPathName) && !script->IsEditorScript ())
			{
				if (script->GetPropertiesHash () == 0)
				{
					WarningStringMsg ("Hash for script %s has not been generated\n", script->GetScriptFullClassName().c_str());
					continue;
				}

				assetBundle.AddScriptCompatibilityInfo (script->GetScriptClassName (), script->GetNameSpace (), script->GetAssemblyName (), script->GetPropertiesHash ());
			}
		}
	}
}

TemporaryAssetLookup TemporaryFileToAssetPathMap (const InstanceIDToBuildAsset& assets)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager();

	TemporaryAssetLookup temporaryToAssetName;
	for (InstanceIDToBuildAsset::const_iterator i=assets.begin();i != assets.end();i++)
	{
		string dataPath = pm.GetPathName(i->first);
		string assetPath = pm.AssetPathNameFromAnySerializedPath(dataPath);

		// Built-in resources do not have a source file. So instead print their class & name
		if (assetPath.empty())
		{
			NamedObject* no = dynamic_instanceID_cast<NamedObject*>(i->first);
			if (no != NULL)
			{
				assetPath = Format("Built-in %s: %s", no->GetClassName().c_str(), no->GetName());
			}
		}
		temporaryToAssetName[make_pair(i->second.temporaryPathName, i->second.temporaryObjectIdentifier.localIdentifierInFile)] = assetPath;
	}
	return temporaryToAssetName;
}

