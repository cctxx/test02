#ifndef EXTRACT_LOADED_OBJECT_INFO_H
#define EXTRACT_LOADED_OBJECT_INFO_H

#if ENABLE_PROFILER

// Enum is in sync with ProfilerAPI.txt GarbageCollectReason
enum LoadedObjectMemoryType
{
	kSceneObject = 0,
	kBuiltinResource = 1,
	kMarkedDontSave = 2,
	kAssetMarkedDirtyInEditor = 3,
	
	kSceneAssetReferencedByNativeCodeOnly = 5,
	kSceneAssetReferenced = 6,
	
	kAssetReferencedByNativeCodeOnly = 8,
	kAssetReferenced = 9,
	kNotApplicable = 10
};

LoadedObjectMemoryType GetLoadedObjectReason (Object* object);

#endif

#endif
