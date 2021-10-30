#ifndef PREFAB_UTILITY_H
#define PREFAB_UTILITY_H

#include "Runtime/Utilities/vector_set.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/EnumFlags.h"

class Transform;

enum ReplacePrefabOptions
{
	kDefaultRelacePrefab = 0,
	kAutoConnectPrefab = 1 << 0,
	kReplacePrefabNameBased = 1 << 1,
	kDontMakePersistentOrReimport = 1 << 3,
};
ENUM_FLAGS(ReplacePrefabOptions)



/////@TODO: verification when building object data that the types actually match (classID & ScriptType)???

struct ObjectData
{
	Object*      objectInstance;
	Object*      prefabParent;

	ObjectData ()
	{
		objectInstance = NULL;
		prefabParent = NULL;
	}
	
	ObjectData (Object* objInst, Object* prefabPar)
	{
		objectInstance = objInst;
		prefabParent = prefabPar;
	}
	
	friend bool operator < (const ObjectData& lhs, const ObjectData& rhs)
	{
		return lhs.prefabParent < rhs.prefabParent;
	}

	friend bool operator == (const ObjectData& lhs, const ObjectData& rhs)
	{
		return lhs.prefabParent == rhs.prefabParent;
	}
};

typedef vector_set<ObjectData> PrefabParentCache;

/// Uses the 
Object* FindInstanceFromPrefabObject (Object& prefabParent, const std::vector<ObjectData>& outputInstancedObjects);
Object* FindPrefabFromInstanceObject (Object& instanceObject, const std::vector<ObjectData>& outputInstancedObjects);

void BuildPrefabParentCache (const std::vector<ObjectData>& objects, PrefabParentCache& cache);
Object* FindInstanceFromPrefabObjectCached (Object& prefabParent, const PrefabParentCache& cache);

bool ClearDuplicatePrefabParentObjects (std::vector<ObjectData>& instanceObjects);

void DeactivateGameObjectPrefabInstances (const std::vector<ObjectData>& instancedObjects);

void DestroyPrefabObjects (const std::set<PPtr<Object> >& toDestroyObjects);
void DestroyPrefabObject (Object* p);

/// Calls AwakeFromLoad, CheckConsistency, SetDirty on the instancedObjects after merging
void DirtyAndAwakeFromLoadAfterMerge (const std::vector<ObjectData>& instancedObjects, bool instanceObjects);

bool IsTopLevelGameObject (GameObject& go);

Object& DuplicateEmptyResetObject (Object& obj);

Transform* GetTransformFromComponentOrGameObject (Object* obj);

bool IsGameObjectOrComponentActive (Object* obj);

#endif