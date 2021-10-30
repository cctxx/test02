#include "UnityPrefix.h"
#include "PrefabUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoBehaviour.h"


using namespace std;

bool IsTopLevelGameObject (GameObject& go)
{
	Transform* transform = go.QueryComponent (Transform);
	return transform == NULL || transform->GetParent () == NULL;
}

Object* FindInstanceFromPrefabObject (Object& prefabParent, const vector<ObjectData>& outputInstancedObjects)
{
	for (int i=0;i<outputInstancedObjects.size();i++)
	{
		if (outputInstancedObjects[i].prefabParent == &prefabParent)
			return outputInstancedObjects[i].objectInstance;
	}
	return NULL;
}

Object* FindPrefabFromInstanceObject (Object& instanceObject, const vector<ObjectData>& outputInstancedObjects)
{
	for (int i=0;i<outputInstancedObjects.size();i++)
	{
		if (outputInstancedObjects[i].objectInstance == &instanceObject)
			return outputInstancedObjects[i].prefabParent;
	}
	return NULL;
}

Object* FindInstanceFromPrefabObjectCached (Object& prefabParent, const PrefabParentCache& cache)
{
	ObjectData compare (NULL, &prefabParent);
	PrefabParentCache::const_iterator found;
	found = cache.find (compare);
	if (found == cache.end ())
		return NULL;
	else
		return found->objectInstance;
}

bool ClearDuplicatePrefabParentObjects (std::vector<ObjectData>& instanceObjects)
{
	bool didClear = false;
	PrefabParentCache cache;
	BuildPrefabParentCache(instanceObjects, cache);
	
	for (int i=0;i<instanceObjects.size();i++)
	{
		if (instanceObjects[i].prefabParent == NULL)
			continue;
		
		if (instanceObjects[i].objectInstance != FindInstanceFromPrefabObjectCached(*instanceObjects[i].prefabParent, cache))
		{
			instanceObjects[i].prefabParent = NULL;
			didClear = true;
		}
	}
	return didClear;
}

void BuildPrefabParentCache (const vector<ObjectData>& objects, PrefabParentCache& cache)
{
	cache.assign_clear_duplicates(objects.begin(), objects.end());
}

void DestroyPrefabObject (Object* p)
{
	if (p == NULL)
		return;

	p->SetPersistentDirtyIndex(0);
	
	#if !UNITY_RELEASE
	p->HackSetAwakeWasCalled();
	p->HackSetAwakeDidLoadThreadedWasCalled();
	#endif
	
	DestroySingleObject(p);
}

		
void DestroyPrefabObjects (const set<PPtr<Object> >& toDestroyObjects)
{
	set<PPtr<Object> >::const_iterator i;

	for (i=toDestroyObjects.begin ();i != toDestroyObjects.end ();i++)
	{
		Object* p = Object::IDToPointer(i->GetInstanceID());
		DestroyPrefabObject(p);
	}
}

void DirtyAndAwakeFromLoadAfterMerge (const vector<ObjectData>& objects, bool instanceObjects)
{
	Object* cur;
	for (int i=0;i<objects.size();i++)
	{
		cur = instanceObjects ? objects[i].objectInstance : objects[i].prefabParent;
		cur->CheckConsistency ();

	}

	// Validate/Assert active state of prefabs
	for (int i=0;i<objects.size();i++)
	{
		cur = instanceObjects ? objects[i].objectInstance : objects[i].prefabParent;

		// Validate that active state is correct
		GameObject* go = dynamic_pptr_cast<GameObject*> (cur);
		if (go)
		{
			if (instanceObjects)
			{
				Assert(go->IsActive() == go->IsActiveIgnoreImplicitPrefab());
			}
			else
			{
				Assert(!go->IsActive());
			}
		}
	}
	
	// AwakeFromLoad, SetDirty
	for (int i=0;i<objects.size();i++)
	{
		cur = instanceObjects ? objects[i].objectInstance : objects[i].prefabParent;
		cur->AwakeFromLoad (kDefaultAwakeFromLoad);
		cur->SetDirty ();
	}
}

Object& DuplicateEmptyResetObject (Object& obj)
{
	Object* empty = Object::Produce(obj.GetClassID());
	
	MonoBehaviour* dstBehaviour = dynamic_pptr_cast<MonoBehaviour*>(empty);
	MonoBehaviour* srcBehaviour = dynamic_pptr_cast<MonoBehaviour*>(&obj);
	if (dstBehaviour)
		dstBehaviour->SetScript(srcBehaviour->GetScript(), NULL);
	
	empty->Reset();
	
	return *empty;
}


Transform* GetTransformFromComponentOrGameObject (Object* obj)
{
	GameObject* go = NULL;
	if (dynamic_pptr_cast<Unity::Component*> (obj))
		go = static_cast<Unity::Component*> (obj)->GetGameObjectPtr();
	if (go == NULL)
		go = dynamic_pptr_cast<GameObject*> (obj);
	
	if (go)
		return go->QueryComponent(Transform);
	else
		return NULL;
}

bool IsGameObjectOrComponentActive (Object* obj)
{
	Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (obj);
	if (component != NULL)
		return component->IsActive();
	
	GameObject* go = dynamic_pptr_cast<GameObject*> (obj);
	if (go != NULL)
		return go->IsActive();
	return false;
}
