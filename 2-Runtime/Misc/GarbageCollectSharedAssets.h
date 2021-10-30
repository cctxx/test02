#ifndef GARBAGE_COLLECT_SHARED_ASSETS_H
#define GARBAGE_COLLECT_SHARED_ASSETS_H

/// Unloads all assets that are not referenced using dependency tracking and mono GC.
/// You must call GetPreloadManager().LockPreloading(); GetPreloadManager().UnlockPreloading(); around GarbageCollectSharedAssets.
void GarbageCollectSharedAssets (bool includeMonoReferencesAsRoots);

/// Calculates all direct references of all loaded objects and additional categories
/// * loadedObjects is an array of all loaded objects
/// * additionalCategories is an array of all custom categories (eg. "SceneObject", "HideAndDontSave" etc)
/// * referencedObjectCount is the number of referenced objects (referencedObjectIndices holds the indices)
/// * referencedObjectIndices is a combined array of all indices (referencedObjectCount)
///   This is a stream layout so to make sense of the data you have to walk referencedObjectCount in linear order
#if ENABLE_MEM_PROFILER
void CalculateAllObjectReferences (dynamic_array<Object*>& loadedObjects, dynamic_array<const char*>& additionalCategories, dynamic_array<UInt32>& referencedObjectCount, dynamic_array<UInt32>& referencedObjectIndices);
#endif

#if UNITY_EDITOR
bool ShouldPersistentDirtyObjectBeKeptAlive (int instanceID);

/// Prevents an Object from being destroyed by GarbageCollectSharedAssets().
/// This is useful in cases where an Object is internally created by code without ties
/// to the scene and needs to be kept alive across operations that may trigger an
/// asset GC run.
void SetPreventGarbageCollectionOfAsset (int instanceID);
/// Reverts previous call to SetPreventGarbageCollectionOfAsset().
void ClearPreventGarbageCollectionOfAsset (int instanceID);

#endif

#endif
