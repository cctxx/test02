#include "UnityPrefix.h"
#if ENABLE_PROFILER
#include "ExtractLoadedObjectInfo.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Misc/GarbageCollectSharedAssets.h"

LoadedObjectMemoryType GetLoadedObjectReason (Object* object)
{
	bool referencedOnlyByNativeData = false;

	if(!object->GetCachedScriptingObject ())
		referencedOnlyByNativeData = true;

	bool isPersistent = object->IsPersistent();

	// Builtin resource (Treated as root)
	if (isPersistent)
	{
		string path = GetPersistentManager().GetPathName(object->GetInstanceID());
		if (path == "library/unity editor resources" || path == "library/unity default resources")
		{
			return kBuiltinResource;
		}
	}

	// Objects marked DontSave (Treated as root)
	if (object->TestHideFlag(Object::kDontSave))
	{
		return kMarkedDontSave;
	}

	// Asset marked dirty in the editor (Treated as root)
#if UNITY_EDITOR
	if (isPersistent && object->IsPersistentDirty() && ShouldPersistentDirtyObjectBeKeptAlive(object->GetInstanceID()))
	{
		return kAssetMarkedDirtyInEditor;
	}
#endif		

	// gameobjects and components in the scene (treated as roots)
	if (!isPersistent)
	{
		int classID = object->GetClassID();
		if (classID == ClassID (GameObject))
		{
			return kSceneObject;
		}
		else if (Object::IsDerivedFromClassID(classID, ClassID(Component)))	
		{
			Unity::Component* com = static_cast<Unity::Component*> (object);
			if (com->GetGameObjectPtr())
				return kSceneObject;
		}
	}

	// Asset is being referenced from something, specify what
	if (!isPersistent)
	{
		if (referencedOnlyByNativeData)
			return kSceneAssetReferencedByNativeCodeOnly;
		else
			return kSceneAssetReferenced;
	}
	else
	{
		if (referencedOnlyByNativeData)
			return kAssetReferencedByNativeCodeOnly;
		else
			return kAssetReferenced;
	}

	return kNotApplicable;
}

#endif
