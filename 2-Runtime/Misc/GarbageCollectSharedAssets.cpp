#include "UnityPrefix.h"
#if !ENABLE_OLD_GARBAGE_COLLECT_SHARED_ASSETS
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Mono/Coroutine.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "GameObjectUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/CollectProfilerStats.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "BatchDeleteObjects.h"
#include "Runtime/Interfaces/IPhysics.h"

/// All these includes are used by AddManagerRoots
#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetServer/ASCache.h"
#include "Editor/Src/EditorBuildSettings.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/HierarchyState.h"
#include "Editor/Src/InspectorExpandedState.h"
#include "Editor/Src/AnnotationManager.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Editor/Src/EditorAssetGarbageCollectManager.h"
#endif
#include "Runtime/Allocator/MemoryManager.h"

#include "Runtime/Scripting/Scripting.h"

PROFILER_INFORMATION(gGarbageCollectSharedAssetsProfile, "GarbageCollectAssetsProfile", kProfilerLoading)
PROFILER_INFORMATION(gGCFindLiveObjects, "GC.FindLiveObjects", kProfilerLoading)
PROFILER_INFORMATION(gGCBuildLiveObjectMaps, "GC.BuildLiveObjectMaps", kProfilerLoading)
PROFILER_INFORMATION(gGCMarkDependencies, "GC.MarkDependencies", kProfilerLoading)
PROFILER_INFORMATION(gGCDeletedUnusedAssets, "GC.DeleteUnusedAssets", kProfilerLoading)
PROFILER_INFORMATION(gGCOnDestroyCallback, "ScriptableObject.OnDestroy", kProfilerLoading)

#if UNITY_EDITOR
static int gPreventGarbageCollectionOfInstanceID = 0;
#endif

struct ObjectState
{
	Object* object;
	UInt32  classID       : 29;
	UInt32  marked        : 1;
	UInt32  isPersistent  : 1;
};


struct ObjectHashFunctor
{
	inline size_t operator()(const Object* x) const
	{
		return (size_t)x / 32;		
	}
};


#define ASSET_REMAP_TABLE 1
#define USE_MONO_LIVENESS ENABLE_MONO

// Cap on how much memory we can use for the dense remap table for assets
enum { kMaximumAssetRemapTableSize = 250 * 1024 };

struct GarbageCollectorState;

struct GenericSlowGarbageCollector : public GenerateIDFunctor
{
	GarbageCollectorState* gcState;
	
	GenericSlowGarbageCollector () { }
	virtual ~GenericSlowGarbageCollector () {}
	
	inline void ProcessReference (SInt32 oldInstanceID);
	
	// General purpose GarbageCollector callback
	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag);
};

struct GarbageCollectorState
{
	typedef pair<const int, int> InstanceIDToIndexPair;
	
#if ASSET_REMAP_TABLE
	typedef dense_hash_map<int, int, InstanceIDHashFunctor, std::equal_to<int>, STL_ALLOCATOR( kMemTempAlloc, InstanceIDToIndexPair )> InstanceIDToIndex;
#else
	typedef map<int, int>       InstanceIDToIndex;
#endif
	InstanceIDToIndex           instanceIDToIndex;
	dynamic_array<UInt32>       assetRemapTable;
	
	dynamic_array<ObjectState>  liveObjects;
	dynamic_array<UInt32>       needsProcessing;

	int                         originalObjectCount;
	
	void*                       livenessState;

	bool                        followMonoReferences;
	
	#if ENABLE_MEM_PROFILER
	bool                        alwaysAddToNeedsProcessing;
	#endif
	
	RemapPPtrTransfer           genericSlowTransfer;
	GenericSlowGarbageCollector genericCollector;

	GarbageCollectorState ()
	  : assetRemapTable (kMemTempAlloc),
		liveObjects (kMemTempAlloc),
		needsProcessing (kMemTempAlloc),
		genericSlowTransfer (kDontRequireAllMetaFlags | kPerformUnloadDependencyTracking, false)
	{
		genericCollector.gcState = this;
#if ENABLE_MEM_PROFILER
		alwaysAddToNeedsProcessing = false;
#endif
		genericSlowTransfer.SetGenerateIDFunctor (&genericCollector);
	}
};


///@TODO: Make a function that can let us find all classes that have references
static bool DoesClassIDHaveReferences (int classID)
{
#if UNITY_EDITOR
		
	bool classHasNoReferences =
		classID == ClassID(AssetDatabase) ||
		classID == ClassID(HierarchyState) ||
		classID == ClassID(GUIDSerializer) ||
		classID == ClassID(AssetServerCache);

	if(classHasNoReferences)	
		return false;

#endif	
		
	return 
	classID != ClassID (ScriptMapper) &&
	classID != ClassID (MonoScript) &&
	classID != ClassID (NetworkManager) &&
	classID != ClassID (ResourceManager) &&
	classID != ClassID (PreloadData) &&
	classID != ClassID (Texture) &&
	classID != ClassID (Texture2D) &&
	classID != ClassID (Texture3D) &&
	classID != ClassID (Cubemap) &&
	classID != ClassID (WebCamTexture) &&
	classID != ClassID (RenderTexture) &&
	classID != ClassID (AssetBundle) &&
	classID != ClassID (Mesh) &&
	classID != ClassID (TagManager);
}

static void MarkManagedStaticVariableRoots (GarbageCollectorState& gcState);
static void FindAllLiveObjects (GarbageCollectorState& gcState);
static void MarkSceneRootsAndReduceLiveObjects (GarbageCollectorState& gcState);
static void MarkManagerRoots (GarbageCollectorState& gcState);
static void MarkSelectedObjectsAsRoots (GarbageCollectorState& gcState);
static void MarkAllDependencies (GarbageCollectorState& gcState);
static void CleanupUnusedObjects (GarbageCollectorState& gcState);
static void ValidateNoObjectsWereLoaded (GarbageCollectorState& state);
static void CreateObjectToIndexMappingFromNonRootObjects (GarbageCollectorState& gcState);
static void CleanupOtherUnusedMemory (GarbageCollectorState& gcState);
static void BeginLivenessChecking( GarbageCollectorState& gcState);
static void EndLivenessChecking( GarbageCollectorState& gcState);


// Rename
void GarbageCollectSharedAssets (bool monoReferences)
{
	PROFILER_AUTO(gGarbageCollectSharedAssetsProfile, NULL);

	ABSOLUTE_TIME totalTime;
	ABSOLUTE_TIME findAllLiveObjectsTime;
	ABSOLUTE_TIME buildLiveObjectMapTime;
	ABSOLUTE_TIME markTime;
	ABSOLUTE_TIME unloadTime;

#if ENABLE_SERIALIZATION_BY_CODEGENERATION
	// Gab: ManagedLivenessAnalysis needs to be reset every time before executing the liveness check.
	static ScriptingInvocation resetManagedAnalysis(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","ManagedLivenessAnalysis","ResetState"));
	resetManagedAnalysis.Invoke();
#endif
	
	totalTime = START_TIME;
	int originalObjectCount = Object::GetLoadedObjectCount();
#if ENABLE_PROFILER
	printf_console("System memory in use before: %s.\n", FormatBytes(GetUsedHeapSize()).c_str());
#endif
	// This is essentially a Mark & Sweep Garbage Collector.
	
	GarbageCollectorState state;
	
	state.followMonoReferences = monoReferences;
	state.originalObjectCount = originalObjectCount;
	
	findAllLiveObjectsTime = START_TIME;

	//@TODO: This stage can easily be jobified
	// Fills all live objects and extracts their classID's for use by the marking algorithm
	FindAllLiveObjects(state);
	MarkSceneRootsAndReduceLiveObjects (state);

	findAllLiveObjectsTime = ELAPSED_TIME(findAllLiveObjectsTime);

	buildLiveObjectMapTime = START_TIME;
	// Create mapping to quickly look up all object references
	CreateObjectToIndexMappingFromNonRootObjects(state);
	buildLiveObjectMapTime = ELAPSED_TIME(buildLiveObjectMapTime);

	markTime = START_TIME;

	MarkManagerRoots (state);
	MarkSelectedObjectsAsRoots (state);

	ValidateNoObjectsWereLoaded (state);
	
	{
		// Once we call BeginLivenessChecking we can not use the profiler because it might allocate memory.
		PROFILER_AUTO(gGCMarkDependencies, NULL);

		BeginLivenessChecking(state);
		
		// Mark static variables
		MarkManagedStaticVariableRoots (state);
		// Process roots to find all referenced objects
		MarkAllDependencies (state);
		
		EndLivenessChecking(state);
	}
	markTime = ELAPSED_TIME(markTime);
	
	ValidateNoObjectsWereLoaded (state);

	if (state.originalObjectCount != Object::GetLoadedObjectCount())
	{
		ErrorString("UnloadUnusedAssets incorrect caused some assets to load. This can easily cause deadlocks or crashes.");
	}
	
	unloadTime = START_TIME;
	
	// Cleanup all objects not marked as dependencies or roots
	CleanupUnusedObjects (state);

	unloadTime = ELAPSED_TIME(unloadTime);

	int unloadedObjects = originalObjectCount - Object::GetLoadedObjectCount();
	
	// Unload PersistentManager memory that is not needed
	CleanupOtherUnusedMemory (state);

	totalTime = ELAPSED_TIME(totalTime);

#if ENABLE_PROFILER
	printf_console("System memory in use after: %s.\n", FormatBytes(GetUsedHeapSize()).c_str());
#endif
	printf_console("\nUnloading %d unused Assets to reduce memory usage. Loaded Objects now: %d.\n", unloadedObjects, (int)Object::GetLoadedObjectCount());
	printf_console("Total: %f ms (FindLiveObjects: %f ms CreateObjectMapping: %f ms MarkObjects: %f ms  DeleteObjects: %f ms)\n\n", 
				   AbsoluteTimeToMilliseconds(totalTime), AbsoluteTimeToMilliseconds(findAllLiveObjectsTime), AbsoluteTimeToMilliseconds(buildLiveObjectMapTime), AbsoluteTimeToMilliseconds(markTime), AbsoluteTimeToMilliseconds(unloadTime) );
	
	#if UNITY_EDITOR
	EditorAssetGarbageCollectManager::Get()->SetPostCollectMemoryUsage();
	#endif
}
		

#if ASSET_REMAP_TABLE
static void CreateObjectToIndexMappingFromNonRootObjects (GarbageCollectorState& gcState)
{
	PROFILER_AUTO(gGCBuildLiveObjectMaps, NULL);
	
	gcState.instanceIDToIndex.set_empty_key (-1);
	gcState.instanceIDToIndex.set_deleted_key (-2);
		
	int largestInstanceID = 0;
	
	for (int i=0;i<gcState.liveObjects.size();++i)
	{
		ObjectState& state = gcState.liveObjects[i];
		if (state.marked == 0)
		{
			int instanceID = state.object->GetInstanceID();
			
			if (instanceID > 0)
			{
				largestInstanceID = std::max<SInt32>(largestInstanceID, instanceID);
			}
			else
			{
				gcState.instanceIDToIndex.insert(std::make_pair(instanceID, i));
			}
		}
	}
	
	// Cap the memory of dense assetRemapTable
	if (largestInstanceID < (kMaximumAssetRemapTableSize / sizeof(UInt32)))
	{
		gcState.assetRemapTable.resize_initialized(largestInstanceID + 1, -1);
		for (int i=0;i<gcState.liveObjects.size();++i)
		{
			ObjectState& state = gcState.liveObjects[i];
			if (state.marked == 0)
			{
				int instanceID = state.object->GetInstanceID();
				
				if (instanceID > 0)
					gcState.assetRemapTable[instanceID] = i;
			}
		}
	}
	// Use hashtable for all instance ids as a fallback
	else
	{
		for (int i=0;i<gcState.liveObjects.size();++i)
		{
			ObjectState& state = gcState.liveObjects[i];
			if (state.marked == 0)
			{
				int instanceID = state.object->GetInstanceID();
				
				if (instanceID > 0)
					gcState.instanceIDToIndex.insert(std::make_pair(instanceID, i));
			}
		}
	}
}

#else
static void CreateObjectToIndexMappingFromNonRootObjects (GarbageCollectorState& gcState)
{
	///@TODO: Should we recreate gcState.liveObjects here? There is no reason to ever visit objects that are already marked at this point...
	////      Try it...

	for (int i=0;i<gcState.liveObjects.size();++i)
	{
		ObjectState& state = gcState.liveObjects[i];
		if (state.marked == 0)
		{
			int instanceID = state.object->GetInstanceID();
			
			gcState.instanceIDToIndex.insert(make_pair(instanceID, i));
		}
	}
}
#endif
			

static inline int LookupInstanceIDIndex (int instanceID, GarbageCollectorState& gcState)
{
	if (instanceID == 0)
		return -1;
					
#if ASSET_REMAP_TABLE
	
	size_t size = gcState.assetRemapTable.size();
	if (instanceID > 0 && size != 0)
	{	
		if (instanceID < size)
			return gcState.assetRemapTable[instanceID];
		else
			return -1;
	}
#endif	
	else
	{
		GarbageCollectorState::InstanceIDToIndex::const_iterator found = gcState.instanceIDToIndex.find(instanceID);
		if (found == gcState.instanceIDToIndex.end())
			return -1;
		else
			return found->second;
	}
}

static inline int LookupObjectIndex (const Object& object, GarbageCollectorState& gcState)
{
	return LookupInstanceIDIndex (object.GetInstanceID(), gcState);
}


static void MarkIndexAsRoot (int index, GarbageCollectorState& gcState)
{
	ObjectState& state = gcState.liveObjects[index];

	Assert(state.marked == 0);
	state.marked = 1;
	
	// If we have references to other objects in the marked object -> then we need to process it
	// No processing is needed if the class has no references
	// (Eg. a texture has no references to other objects, thus it does not have to be processed)
	bool addToNeedsProcessing = DoesClassIDHaveReferences (state.classID);
	Assert(state.object->ShouldIgnoreInGarbageDependencyTracking() == !addToNeedsProcessing);
	
	#if ENABLE_MEM_PROFILER
	// When extracting references for the memory profiler we use the needsProcessing array as the object which have been marked.
	// Thus we can not use the fastpath for objects that have no references to other objects
	addToNeedsProcessing |= gcState.alwaysAddToNeedsProcessing;
	#endif
	
	if (addToNeedsProcessing)
	{
		gcState.needsProcessing.push_back(index);
	}
}
		
static void MarkObjectAsRoot (const Object& object, GarbageCollectorState& gcState)
{
	int index = LookupObjectIndex (object, gcState);
	if (index != -1)
		MarkIndexAsRoot(index, gcState);
}

static void MarkInstanceIDAsRoot (int instanceID, GarbageCollectorState& gcState)
{
	int index = LookupInstanceIDIndex (instanceID, gcState);
	if (index != -1)
	{
		if (gcState.liveObjects[index].marked == 0)
			MarkIndexAsRoot(index, gcState);
	}
}

static void MarkObjectAsRootUnknownMarkState (const Object& object, GarbageCollectorState& gcState)
{
	int index = LookupObjectIndex (object, gcState);
	if (index != -1 && gcState.liveObjects[index].marked == 0)
		MarkIndexAsRoot(index, gcState);
}
				
static void MarkObjectAsRootCheckNull (const Object* object, GarbageCollectorState& gcState)
{
	if (object != NULL)
		MarkObjectAsRootUnknownMarkState (*object, gcState);
}
				
#if USE_MONO_LIVENESS
static void RegisterFilteredObjectCallback(gpointer* arr, int count, void* userdata)
{
	GarbageCollectorState* gcStatePtr = (GarbageCollectorState*)userdata;
	for (int i = 0; i < count ;i++) 
	{
		SInt32 instanceID = Scripting::GetInstanceIDFromScriptingWrapper((MonoObject*)arr[i]);
		MarkInstanceIDAsRoot(instanceID, *gcStatePtr);
	}
}
#endif
					
static void BeginLivenessChecking( GarbageCollectorState& gcState)
{
	if(!gcState.followMonoReferences)
		return;
#if USE_MONO_LIVENESS
	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().DisallowAllocationsOnThisThread();
	#endif
	gcState.livenessState = mono_unity_liveness_calculation_begin(ScriptingClassFor(Object), gcState.liveObjects.size(), RegisterFilteredObjectCallback, (void*)&gcState);
#endif	
}

static void EndLivenessChecking( GarbageCollectorState& gcState)
{
	if(!gcState.followMonoReferences)
		return;
#if USE_MONO_LIVENESS
	mono_unity_liveness_calculation_end(gcState.livenessState);
	#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().ReallowAllocationsOnThisThread();
	#endif
#endif
}

static void MarkManagedStaticVariableRoots (GarbageCollectorState& gcState)
{
	if(!gcState.followMonoReferences)
		return;

#if USE_MONO_LIVENESS
	ValidateNoObjectsWereLoaded (gcState);
	mono_unity_liveness_calculation_from_statics (gcState.livenessState);
#endif
}

#if UNITY_EDITOR
void SetPreventGarbageCollectionOfAsset (int instanceID)
{
	Assert(gPreventGarbageCollectionOfInstanceID == 0);
	gPreventGarbageCollectionOfInstanceID = instanceID;
}

void ClearPreventGarbageCollectionOfAsset (int instanceID)
{
	Assert(gPreventGarbageCollectionOfInstanceID == instanceID);
	gPreventGarbageCollectionOfInstanceID = 0;
}
#endif


static void MarkSelectedObjectsAsRoots (GarbageCollectorState& gcState)
{
#if UNITY_EDITOR
	// Add all selected objects as GC Roots because they are being shown in the inspector.
	set<int> selection = GetSceneTracker().GetSelectionID ();
	for (set<int>::iterator i=selection.begin();i!=selection.end();i++)
		MarkInstanceIDAsRoot(*i, gcState);
#endif
}


static void MarkManagerRoots (GarbageCollectorState& gcState)
{
	// All managers are roots (except script mapper)
	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		if (GetManagerPtrFromContext(i) != NULL)
			MarkObjectAsRootUnknownMarkState (*GetManagerPtrFromContext(i), gcState);
	}

	#if UNITY_EDITOR
	MarkObjectAsRootUnknownMarkState (AssetDatabase::Get(), gcState);
	MarkObjectAsRootUnknownMarkState (AssetServerCache::Get(), gcState);
	MarkObjectAsRootUnknownMarkState (GetEditorBuildSettings(), gcState);
	MarkObjectAsRootUnknownMarkState (GetEditorUserBuildSettings(), gcState);
	MarkObjectAsRootUnknownMarkState (GetEditorSettings(), gcState);
	MarkObjectAsRootUnknownMarkState (GetEditorUserSettings(), gcState);
	MarkObjectAsRootCheckNull (AssetInterface::Get().GetGUIDSerializer(), gcState);
	MarkObjectAsRootCheckNull (GetProjectWindowHierarchyStateIfLoaded (), gcState);
	MarkObjectAsRootUnknownMarkState (GetInspectorExpandedState (), gcState);
	MarkObjectAsRootUnknownMarkState (GetAnnotationManager (), gcState);
	
	if (gPreventGarbageCollectionOfInstanceID != 0)
		MarkInstanceIDAsRoot(gPreventGarbageCollectionOfInstanceID, gcState);
	
	#endif
	
	ValidateNoObjectsWereLoaded (gcState);
}

#if UNITY_EDITOR
bool ShouldPersistentDirtyObjectBeKeptAlive (int instanceID)
{
	// The Object is not mapped to disk
	SerializedObjectIdentifier identifier;
	if (!GetPersistentManager().InstanceIDToSerializedObjectIdentifier(instanceID, identifier))
	{	
		// #if !UNITY_RELEASE
		// WarningString("Persistent object is known in persistent manager will unload Might happen due to Assetbundle.Unload(false)");
		// #endif
		return false;
	}

	// Cached asset file assets should just be unloaded. They contain nothing that can not be reloaded.
	// Eg a texture in an imported .psd file, while scripts might call SetDirty on it, it doesn't actually ever get serialized back.
	// Because the only thing that save an texture is the asset importer. Any modifications done in memory will never be saved.
	// -> Thus a texture that is marked dirty can be unloaded
	// -> But a MetaData object or AssetImporter settings object (eg. class TextureImporter) should not be unloaded if it is marked dirty
	FileIdentifier file = GetGUIDPersistentManager().PathIDToFileIdentifierInternal(identifier.serializedFileIndex);
	if (file.type == FileIdentifier::kMetaAssetType)
	{
		if (identifier.localIdentifierInFile == kAssetMetaDataFileID)
		{
			// We can't have this warning since changing an import setting and then starting a long import operation will trigger it.
			//WarningString(Format("ImportSettings '%s' has been modified but AssetDatabase.ImportAsset has not been called. Please fix the scripts code or Import the asset manually.", GetAssetPathFromInstanceID(instanceID).c_str()));
			return true;
		}
		else if (identifier.localIdentifierInFile == kAssetImporterFileID)
		{
			// We can't have this warning since changing an import setting and then starting a long import operation will trigger it.
			//WarningString(Format("ImportSettings '%s' has been modified but AssetDatabase.ImportAsset has not been called. Please fix the scripts code or Import the asset manually.", GetAssetPathFromInstanceID(instanceID).c_str()));
			return true;
		}
		
		return false;
	}
	// Any other assets are will not be unloaded when dirtied.
	// Eg. scriptmapper, guidmapper, builtin resources...
	else
	{
		return true;
	}
}
#endif

enum GCRootType
{
	// The object is not a GC root (might be referenced during the marking stage)
	kNoGCRoot = 0,
	// The object is a GC root and is guaranteed to not have any references to non-root objects (Things that are already guaranteed to be marked)
	// For example, game objects and transforms in a scene don't need to be processed, because they can only reference components that are already in the scene and those are already marked as roots.
	kGCRootReferencesOnly = 1,

	// a root for the GC marking, must be processed and it's dependencies must be found
	kGCRootWithReferences = 2
};
		
#if UNITY_EDITOR
#define DoesNotHavePrefabAttached(liveObject) (static_cast<EditorExtension*> (liveObject.object)->GetPrefab().GetInstanceID() == 0)
#else
#define DoesNotHavePrefabAttached(liveObject) true
#endif

inline bool HasValidGameObject (const ObjectState& liveObject)
{
	const Unity::Component* component = static_cast<const Unity::Component*> (liveObject.object);
	return component->GetGameObjectPtr() != NULL;
}

inline bool IsScriptableObject (const ObjectState& liveObject)
{
	const MonoBehaviour* monoBehaviour = static_cast<const MonoBehaviour*> (liveObject.object);
	return monoBehaviour->GetGameObjectPtr() == NULL;
}

static bool IsSceneObject (const ObjectState& liveObject)
{
	if (liveObject.isPersistent)
		return false;
	
	if (liveObject.classID == ClassID(MonoBehaviour) && IsScriptableObject(liveObject))
		return false;
	
	return liveObject.classID == ClassID(GameObject) || Object::IsDerivedFromClassID(liveObject.classID, ClassID(Component));
}

static bool IsAssetNotYetSavedToDisk (const ObjectState& liveObject)
{
#if UNITY_EDITOR
	if (liveObject.isPersistent && liveObject.object->IsPersistentDirty())
	{
		if (ShouldPersistentDirtyObjectBeKeptAlive (liveObject.object->GetInstanceID()))
			return true;
	}
#endif
	return false;
}


static GCRootType IsObjectAGCRoot (const ObjectState& liveObject)
{
	int classID = liveObject.classID;
	if (!liveObject.isPersistent)
	{
		// Game Objects & Transforms in the scene are roots and have no references beyond other objects that are also in the scene
		// Except for prefabs which we specifically check for in the editor
		if (classID == ClassID(GameObject) && DoesNotHavePrefabAttached(liveObject))
			return kGCRootReferencesOnly;
		else if (classID == ClassID(Transform) && DoesNotHavePrefabAttached(liveObject))
			return kGCRootReferencesOnly;
		// Only MonoBehaviour not ScriptableObject are treated as roots
		else if (classID == ClassID(MonoBehaviour))
		{
			if (!IsScriptableObject(liveObject))
				return kGCRootWithReferences;
		}
		else if (Object::IsDerivedFromClassID(classID, ClassID(Component)))
		{
			Assert(HasValidGameObject (liveObject));
			return kGCRootWithReferences;
		}
	}
		
	// Asset bundles are always explicitly unloaded
	if (classID == ClassID(AssetBundle))
		return kGCRootWithReferences;
	// Objects marked as hide and dont save are treated as roots
	//	else if (liveObject.object->TestHideFlag (Object::kDontSave))
	//		return kGCRootWithReferences;
	else if (liveObject.object->TestHideFlag (Object::kDontSave))
		return kGCRootWithReferences;
#if UNITY_EDITOR
	else if (liveObject.isPersistent && liveObject.object->IsPersistentDirty())
	{
		if (ShouldPersistentDirtyObjectBeKeptAlive (liveObject.object->GetInstanceID()))
			return kGCRootWithReferences;
	}
#endif
	
	return kNoGCRoot;
}
	
static void FindAllLiveObjects (GarbageCollectorState& gcState)
{
	PROFILER_AUTO(gGCFindLiveObjects, NULL);
	gcState.originalObjectCount = Object::GetLoadedObjectCount();

	Object::IDToPointerMap& pointerMap = Object::GetIDToPointerMapInternal ();
	gcState.liveObjects.resize_uninitialized(pointerMap.size());
	gcState.needsProcessing.reserve(pointerMap.size());

	Object::IDToPointerMap::const_iterator end = pointerMap.end();
	Object::IDToPointerMap::const_iterator i=pointerMap.begin();

	int index = 0;
	
	ObjectState* liveObjects = gcState.liveObjects.begin();

	// Extract All live objects
	while (i != end)
	{
		Object& object = *i->second;
		
		ObjectState& state = liveObjects[index];
				
		state.object = &object;
		state.classID = object.GetClassID();
		state.marked = 0;
		state.isPersistent = object.IsPersistent();
				
		index++;
		i++;
	}
		
	ValidateNoObjectsWereLoaded (gcState);
}

static void MarkSceneRootsAndReduceLiveObjects (GarbageCollectorState& gcState)
{
	int size = gcState.liveObjects.size();
	ObjectState* liveObjects = gcState.liveObjects.begin();
	
	// Mark scene roots
	for (int index=0;index<size;)
	{
		ObjectState& state = liveObjects[index];
			
		GCRootType type = IsObjectAGCRoot (state);
		
		// It is a root and is guaranteed to have no references to non-roots
		// We dont need it in any maps or even the liveObjects list.
		if (type == kGCRootReferencesOnly)
		{
			// Reducing the size of this array early on saved around 3% on angrybots so not a significant optimization
			size--;
			liveObjects[index] = liveObjects[size];
		}
		// Root that might have references to other objects, thus must be processed
		else if (type == kGCRootWithReferences )
		{
			MarkIndexAsRoot (index, gcState);
			index++;
		}
		else
		{
			index++;
		}
	}

	gcState.liveObjects.resize_uninitialized(size);

	ValidateNoObjectsWereLoaded (gcState);
}

#if UNITY_EDITOR
static void ProcessEditorExtension (GarbageCollectorState& gcState, EditorExtension& object)
{
	MarkInstanceIDAsRoot (object.GetPrefab().GetInstanceID(), gcState);
}
#else
#define ProcessEditorExtension(x,y)
#endif


static void ProcessGameObject (GarbageCollectorState& gcState, ObjectState& state)
{
	GameObject& go = *static_cast<Unity::GameObject*> (state.object);

	ProcessEditorExtension(gcState, go);

	GameObject::Container& container = go.GetComponentContainerInternal ();
	GameObject::Container::iterator end = container.end();
	for (GameObject::Container::iterator i = container.begin();i != end;++i)
	{
		MarkObjectAsRootUnknownMarkState (*i->second, gcState);
	}
}

static void ProcessValidComponent (GarbageCollectorState& gcState, ObjectState& state)
{
	Unity::Component& com = *static_cast<Unity::Component*> (state.object);
	
	ProcessEditorExtension(gcState, com);
	
	GameObject& go = com.GetGameObject ();
	MarkObjectAsRootUnknownMarkState (go, gcState);
}
		

static void ProcessTransform (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessValidComponent(gcState, state);

	Transform& transform = *static_cast<Transform*> (state.object);
		
	const Transform::TransformComList& children = transform.GetChildrenInternal ();
	Transform::TransformComList::const_iterator end = children.end();
	for (Transform::TransformComList::const_iterator i = children.begin();i != end;++i)
	{
		MarkObjectAsRootUnknownMarkState (**i, gcState);
	}
	
	MarkObjectAsRootCheckNull (transform.GetParentPtrInternal (), gcState);
}

static void ProcessMeshFilter (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessValidComponent (gcState, state);
	
	MeshFilter& filter = *static_cast<MeshFilter*> (state.object);
	MarkInstanceIDAsRoot (filter.GetSharedMesh().GetInstanceID(), gcState);
}

static void ProcessMeshRenderer (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessValidComponent (gcState, state);

	Renderer& renderer = *static_cast<Renderer*> (state.object);

	const Renderer::MaterialArray& materials = renderer.GetMaterialArray ();
	Renderer::MaterialArray::const_iterator end = materials.end();
	for (Renderer::MaterialArray::const_iterator i=materials.begin();i != end;++i)
		MarkInstanceIDAsRoot(i->GetInstanceID(), gcState);

	// Make sure that the static batching root has a game object...
//	MarkInstanceIDAsRoot (renderer.GetStaticBatchRoot ().GetInstanceID(), gcState);


	MarkInstanceIDAsRoot (renderer.GetLightProbeAnchor ().GetInstanceID(), gcState);
}

#if ENABLE_PHYSICS
static void ProcessCollider (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessValidComponent (gcState, state);

	Collider& collider = *static_cast<Collider*> (state.object);
	MarkInstanceIDAsRoot(collider.GetMaterial().GetInstanceID(), gcState);
}

static void ProcessPrimitiveCollider (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessCollider (gcState, state);
}

static void ProcessMeshCollider (GarbageCollectorState& gcState, ObjectState& state)
{
	ProcessCollider (gcState, state);

	MeshCollider& collider = *static_cast<MeshCollider*> (state.object);
	MarkInstanceIDAsRoot(collider.GetSharedMesh().GetInstanceID(), gcState);
}
#else

static void ProcessPrimitiveCollider (GarbageCollectorState& gcState, ObjectState& state)
{
	AssertString("Not supported");
}
static void ProcessMeshCollider (GarbageCollectorState& gcState, ObjectState& state)
{
	AssertString("Not supported");
}
#endif

#if USE_MONO_LIVENESS

static void ProcessLivenessFromScriptingObject (GarbageCollectorState& gcState, ScriptingObjectPtr object)
{
	mono_unity_liveness_calculation_from_root(object, gcState.livenessState);
}
	
static void ProcessMonoBehaviour (GarbageCollectorState& gcState, ObjectState& state)
{
	MonoBehaviour& behaviour = *static_cast<MonoBehaviour*> (state.object);
	
	ProcessEditorExtension(gcState, behaviour);
	
	// Not all MonoBehaviours are attached to a game object.
	// Handle that situation specifically here.
	GameObject* go = behaviour.GetGameObjectPtr ();
	if (go != NULL)
		MarkObjectAsRootUnknownMarkState (*go, gcState);

	MarkInstanceIDAsRoot (behaviour.GetScript().GetInstanceID(), gcState);

	if(!gcState.followMonoReferences)
		return;

	// MonoBehaviour managed instance
	MonoObject* instance = Scripting::ScriptingWrapperFor(state.object);
	if (instance != NULL)
		ProcessLivenessFromScriptingObject(gcState, instance);

	// Coroutine managed instances
	List<Coroutine>& coroutine = behaviour.GetActiveCoroutines();
	ListIterator<Coroutine> end = coroutine.end();
	for (ListIterator<Coroutine> i=coroutine.begin();i != end;++i)
	{
		Coroutine& coroutine = *i;
		ProcessLivenessFromScriptingObject(gcState, coroutine.m_CoroutineEnumerator);
	}
}

#else
	
static void ProcessMonoBehaviour (GarbageCollectorState& gcState, ObjectState& state)
{
	AssertString("Not supported");
}
#endif
	
inline void GenericSlowGarbageCollector::ProcessReference (SInt32 oldInstanceID)
{
		int index = LookupInstanceIDIndex (oldInstanceID, *gcState);
		if (index == -1)
			return;

		ObjectState& referencedState = gcState->liveObjects[index];
		if (referencedState.marked == 0)
			MarkIndexAsRoot(index, *gcState);
	}

	// General purpose GarbageCollector callback
SInt32 GenericSlowGarbageCollector::GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlag)
	{
		ProcessReference(oldInstanceID);
		return oldInstanceID;
	}
		
void MarkDependencies (GarbageCollectorState& gcState, UInt32 index)
{
		ObjectState& state = gcState.liveObjects[index];
	
	#if ENABLE_MEM_PROFILER
	Assert(!state.object->ShouldIgnoreInGarbageDependencyTracking() || gcState.alwaysAddToNeedsProcessing);
	#else
		Assert(!state.object->ShouldIgnoreInGarbageDependencyTracking());
	#endif
	
		Assert((bool)state.marked);
	
		if (state.classID == ClassID (GameObject))
			ProcessGameObject(gcState, state);
		else if (state.classID == ClassID (Transform))
			ProcessTransform(gcState, state);
		else if (state.classID == ClassID (MeshRenderer))
			ProcessMeshRenderer(gcState, state);
		else if (state.classID == ClassID (MeshFilter))
			ProcessMeshFilter(gcState, state);
		// Specialized MonoBehaviour code path for mono based platforms, otherwise falls back to the generic RemapPPtrTransfer
		else if ((USE_MONO_LIVENESS) && gcState.followMonoReferences && state.classID == ClassID (MonoBehaviour))
			ProcessMonoBehaviour(gcState, state);
		else if ((ENABLE_PHYSICS) && state.classID == ClassID (MeshCollider))
			ProcessMeshCollider(gcState, state);
		else if ((ENABLE_PHYSICS) && state.classID == ClassID (BoxCollider))
			ProcessPrimitiveCollider(gcState, state);
#if ENABLE_SERIALIZATION_BY_CODEGENERATION
		else if (state.classID == ClassID (MonoBehaviour))
			state.object->DoLivenessCheck(gcState.genericSlowTransfer);
#endif
		else
			state.object->VirtualRedirectTransfer (gcState.genericSlowTransfer);
}

static void MarkAllDependencies (GarbageCollectorState& gcState)
{
	//@TODO: This stage can easily be multi-threaded
	///      The out of this stage is gcState.liveObjects[i].marked = 1;
	//       Find some efficient way multithread that without expensive thread synchronization
	
	dynamic_array<UInt32>& needsProcessing = gcState.needsProcessing;
	while (!needsProcessing.empty ())
	{
		int index = needsProcessing.back();
		needsProcessing.pop_back();
	
		MarkDependencies (gcState, index);
	}
}

#if ENABLE_MEM_PROFILER
static void ExtractObjectArray (GarbageCollectorState& gcState, dynamic_array<Object*>& objects)
{
	objects.resize_uninitialized(gcState.liveObjects.size());
	for (int i=0;i<objects.size();i++)
		objects[i] = gcState.liveObjects[i].object;
}

static void ResetMarkedAndNeedsProcessing (GarbageCollectorState& state, dynamic_array<UInt32>& referencedObjectCount, dynamic_array<UInt32>& referencedObjectIndices)
{
	referencedObjectCount.push_back(state.needsProcessing.size());
	referencedObjectIndices.insert(referencedObjectIndices.end(), state.needsProcessing.begin(), state.needsProcessing.end());
	for (int i=0;i<state.needsProcessing.size();i++)
		state.liveObjects[state.needsProcessing[i]].marked = 0;
	state.needsProcessing.resize_uninitialized(0);
}

static void ClassifyRootObjects (GarbageCollectorState& state, dynamic_array<const char*>& additionalCategories, dynamic_array<UInt32>& referencedObjectCount, dynamic_array<UInt32>& referencedObjectIndices)
{
	// Calculate references for all scene roots
	dynamic_array<UInt32> sceneRootIndices;
	dynamic_array<UInt32> otherRootIndices;
	dynamic_array<UInt32> dirtyAssetIndices;
	
	for (int i=0;i<state.liveObjects.size();i++)
	{
		ObjectState& liveObject = state.liveObjects[i];
		
		if (IsSceneObject (liveObject))
			sceneRootIndices.push_back(i);
		else if (IsAssetNotYetSavedToDisk (liveObject))
			dirtyAssetIndices.push_back(i);
		else if (IsObjectAGCRoot (liveObject) != kNoGCRoot)
			otherRootIndices.push_back(i);
	}
	
	additionalCategories.push_back("Scene Object");
	referencedObjectCount.push_back(sceneRootIndices.size());
	referencedObjectIndices.insert(referencedObjectIndices.end(), sceneRootIndices.begin(), sceneRootIndices.end());
	
	additionalCategories.push_back("HideAndDontSave, Manager or AssetBundle");
	referencedObjectCount.push_back(otherRootIndices.size());
	referencedObjectIndices.insert(referencedObjectIndices.end(), otherRootIndices.begin(), otherRootIndices.end());

	additionalCategories.push_back("Asset has been edited and not yet saved to disk");
	referencedObjectCount.push_back(dirtyAssetIndices.size());
	referencedObjectIndices.insert(referencedObjectIndices.end(), dirtyAssetIndices.begin(), dirtyAssetIndices.end());
}

void CalculateAllObjectReferences (dynamic_array<Object*>& loadedObjects, dynamic_array<const char*>& additionalCategories, dynamic_array<UInt32>& referencedObjectCount, dynamic_array<UInt32>& referencedObjectIndices)
{
	enum { kNbAdditionalCategoriesReserve = 20 };
	
	GarbageCollectorState state;
	state.alwaysAddToNeedsProcessing = true;
	state.followMonoReferences = true;
	
	// Setup live object mapping
	FindAllLiveObjects(state);
	CreateObjectToIndexMappingFromNonRootObjects (state);
	
	// Build loadedObjects output
	ExtractObjectArray (state, loadedObjects);

	referencedObjectIndices.reserve (loadedObjects.size() * 2);
	referencedObjectCount.reserve(loadedObjects.size() + kNbAdditionalCategoriesReserve);

	// Calculate references for all loaded objects
	for (int i=0;i<loadedObjects.size();i++)
	{
		ObjectState& liveObject = state.liveObjects[i];
		if (liveObject.classID == ClassID(MonoBehaviour))
			BeginLivenessChecking(state);
		
		Assert(!liveObject.marked);
		if (DoesClassIDHaveReferences (liveObject.classID))
		{
			liveObject.marked = 1;
			MarkDependencies (state, i);
			liveObject.marked = 0;
		}

		if (liveObject.classID == ClassID(MonoBehaviour))
			EndLivenessChecking(state);
		
		ResetMarkedAndNeedsProcessing (state, referencedObjectCount, referencedObjectIndices);
	}

	// Calculate references for managed static references
	additionalCategories.push_back("ManagedStaticReferences");
	BeginLivenessChecking(state);
	MarkManagedStaticVariableRoots (state);
	EndLivenessChecking(state);
	ResetMarkedAndNeedsProcessing (state, referencedObjectCount, referencedObjectIndices);
	
	// Calculate references from any managers
	additionalCategories.push_back("Managers");
	MarkManagerRoots(state);
	ResetMarkedAndNeedsProcessing (state, referencedObjectCount, referencedObjectIndices);

	// Calculate references from any managers
	additionalCategories.push_back("Selection");
	MarkSelectedObjectsAsRoots(state);
	ResetMarkedAndNeedsProcessing (state, referencedObjectCount, referencedObjectIndices);
	
	// Classify root objects (Scene objects, hide and dontsave etc)
	ClassifyRootObjects (state, additionalCategories, referencedObjectCount, referencedObjectIndices);
}
#endif

static void InvokeScriptableObjectUnloadCallbacks (const SInt32* scriptableObjectCallbacks, int size)
{
	PROFILER_AUTO(gGCOnDestroyCallback, NULL);
		
#if ENABLE_SCRIPTING
	for (int i=0;i<size;i++)
	{
		MonoBehaviour* behaviour = reinterpret_cast<MonoBehaviour*>(Object::IDToPointer(scriptableObjectCallbacks[i]));
		if (behaviour)
		{
			behaviour->WillUnloadScriptableObject();
		}
	}
#endif
}

static void CleanupUnusedObjects (GarbageCollectorState& gcState)
{
	PROFILER_AUTO(gGCDeletedUnusedAssets, NULL);
	
	dynamic_array<SInt32> unloadObjects;
	dynamic_array<SInt32> scriptableObjectUnloadCallbacks;
	unloadObjects.reserve(gcState.liveObjects.size());
	scriptableObjectUnloadCallbacks.reserve(gcState.liveObjects.size());
	
	// Classify
	for (int i=0;i<gcState.liveObjects.size();i++)
	{
		const ObjectState& objectState = gcState.liveObjects[i];

		if (objectState.marked == 0)
		{
			SInt32 instanceID = objectState.object->GetInstanceID();
			unloadObjects.push_back(instanceID);
			
			if (objectState.classID == ClassID(MonoBehaviour))
				scriptableObjectUnloadCallbacks.push_back(instanceID);
		}
	}

	// ScriptableObject.OnDestroy
	InvokeScriptableObjectUnloadCallbacks (scriptableObjectUnloadCallbacks.begin(), scriptableObjectUnloadCallbacks.size());

	// Delete objects
	BatchDeleteObjectInternal (unloadObjects.begin(), unloadObjects.size());
}

static void CleanupOtherUnusedMemory (GarbageCollectorState& gcState)
{
	// Unload streams that are not dirty
	GetPersistentManager().UnloadNonDirtyStreams ();
}
	

static void ValidateNoObjectsWereLoaded (GarbageCollectorState& state)
{
	Assert(state.originalObjectCount == Object::GetLoadedObjectCount());
}


/*

TODO:

Tests: 
 - ScriptableObject referenced from static variable and no longer referenced from static variable
 - Dirty assets are not unloaded
 - Get rid of Validation of bound animation targets on every frame again. Profiler new event system for performance of deleting characters in that case.
 
Important:
-- When unloading make sure we dont unload other objects during destruction. Maybe we can use it to get rid of Object* lookups this way???
  @TODO: Try if we can enable this safely.
 #if DEBUGMODE
 int count = Object::GetLoadedObjectCount ();
#endif
  Do deletion
 #if DEBUGMODE
 if (count-1 != Object::GetLoadedObjectCount ())
 {
 	AssertString("Unloading this object deleted more than the object alone. This is not allowed.");
 }
#endif
*/
#endif
