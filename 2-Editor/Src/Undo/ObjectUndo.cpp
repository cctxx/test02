#include "UnityPrefix.h"
#include "ObjectUndo.h"
#include "Undo.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "UndoManager.h"
#include "Editor/Src/SceneInspector.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"

static void RegisterUndoInternal (Object* identifier, Object** inputObjects, int size, Object* destroyObjectRedo, const std::string &actionName, int namePriority);

static void AddToAwakeFromLoadQueue (AwakeFromLoadQueue& queue, Object* obj)
{
	if (obj == NULL)
		return;
	
	queue.Add (*obj);

	// However this never happens at runtime thus we are moving the overhead of calling AwakeFromLoad on all components
	// to editor only code.
	GameObject* go = dynamic_pptr_cast<GameObject*>(obj);
	if (go != NULL)
		go->ActivateAwakeRecursivelyInternal (kNormalDeactivate, queue);
	
	// Prefab needs to be re-merged
	Prefab* prefab = dynamic_pptr_cast<Prefab*> (obj);
	if (prefab != NULL && prefab->IsPrefabParent())
		MergeAllPrefabInstances(prefab, queue);
	if (prefab != NULL && !prefab->IsPrefabParent())
		MergePrefabInstance(*prefab, queue);
}

static void ErrorCleanupUndo (ObjectUndo* undo)
{
	ErrorString("Failed to create Object Undo, because the action is too large. Clearing undo buffer");
	UNITY_DELETE(undo, kMemUndo);
}

static void StoreObjectDestroyed (SInt32 instanceID, SingleObjectUndo& undo)
{
	undo.instanceID = instanceID;
	undo.classID = ClassID(Undefined);
	undo.isDestroyed = true;
	undo.localIdentifierInFileHint = 0;
}

static void StoreObject (Object& o, SingleObjectUndo& undo)
{
	undo.instanceID = o.GetInstanceID ();
	undo.isDestroyed = false;
	undo.classID = (ClassIDType)o.GetClassID();
	undo.localIdentifierInFileHint = o.GetFileIDHint();

	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&o);
	if (behaviour)
		undo.script = behaviour->GetScript();
	
	if (o.GetNeedsPerObjectTypeTree ())
	{
		undo.typeTree = UNITY_NEW(TypeTree, kMemUndo) ();
		GenerateTypeTree (o, undo.typeTree);
	}
	
	WriteObjectToVector (o, &undo.state);
}

static void StoreObject (SInt32 instanceID, SingleObjectUndo& undo)
{
	Object* object = PPtr<Object> (instanceID);
	if (object)
		StoreObject(*object, undo);
	else
		StoreObjectDestroyed(instanceID, undo);
}

static void PrepareObject (const SingleObjectUndo& undo)
{
	Object* o = PPtr<Object> (undo.instanceID);
	
	if (o == NULL && !undo.isDestroyed)
	{
		o = Object::Produce(undo.classID, undo.instanceID);
		o->Reset();
		o->SetFileIDHint(undo.localIdentifierInFileHint);
		MonoBehaviour* cloneBehaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
		if (cloneBehaviour)
			cloneBehaviour->SetScript(undo.script);
	}
	else if (o != NULL && undo.isDestroyed)
	{
		DestroyPrefabObject (o);
	}
}

static bool RestoreObject (const SingleObjectUndo& undo)
{
	Object* o = PPtr<Object> (undo.instanceID);
	if (o == NULL)
		return false;
	
	if (o->GetNeedsPerObjectTypeTree ())
		ReadObjectFromVector (o, undo.state, *undo.typeTree);
	else
		ReadObjectFromVector (o, undo.state);
	
	o->SetDirty ();

	return true;
}

static unsigned GetAllocatedObjectSize (const SingleObjectUndo& undo)
{
	return undo.state.capacity() + sizeof(SingleObjectUndo);
}

ObjectUndo::ObjectUndo (const std::string& actionName, int inNamePriority)
{
	m_Name = actionName;
	m_NamePriority = inNamePriority;
	m_DestroyObjectRedo = 0;
	m_Identifier = 0;
}

ObjectUndo::~ObjectUndo()
{
	// The memory for the typetree is allocated by ObjectUndo, so lets destroy it here as well to symmetrical
	for (int i = 0; i < m_Undos.size(); i++)
		UNITY_DELETE(m_Undos[i].typeTree, kMemUndo);
}

bool ObjectUndo::Store (Object* identifier, Object** objects, int size, Object* destroyObjectRedo)
{
	SInt32* instanceIDs;
	ALLOC_TEMP(instanceIDs, SInt32, size)
	
	for (int i=0;i<size;i++)
		instanceIDs[i] = objects[i]->GetInstanceID();
	
	SInt32 redoDestroyInstanceID = destroyObjectRedo ? destroyObjectRedo->GetInstanceID() : 0;
	
	return Store (identifier->GetInstanceID(), instanceIDs, size, redoDestroyInstanceID);
}

bool ObjectUndo::Store (SInt32 identifier, SInt32* objects, int size, SInt32 destroyObjectRedo)
{
	m_Identifier = identifier;
	m_DestroyObjectRedo = destroyObjectRedo;
	m_Undos.resize(size);
	SetIsSceneUndo (false);

	unsigned backupMemorySize = 0;
	for (int i=0;i<size;i++)
	{
		Object* obj = Object::IDToPointer(objects[i]);
		if (obj && !obj->IsPersistent())
			SetIsSceneUndo (true);
		StoreObject (objects[i], m_Undos[i]);
		
		backupMemorySize += GetAllocatedObjectSize(m_Undos[i]);
		if (backupMemorySize > kMaximumSingleUndoSize)
			return false;
	}

	return true;
}

void ObjectUndo::StoreObjectDestroyedForAddedComponents (Unity::GameObject& gameObject)
{
	for (int i=0;i<gameObject.GetComponentCount();i++)
	{
		int instanceID = gameObject.GetComponentAtIndex(i).GetInstanceID();
		if (!HasObject (instanceID))
		{
			m_Undos.push_back (SingleObjectUndo ());
			SingleObjectUndo& undo = m_Undos.back();
			StoreObjectDestroyed (instanceID, undo);
		}
	}
}

bool ObjectUndo::RegisterRedo ()
{
	if (m_DestroyObjectRedo != 0)
	{
		RegisterCreatedObjectUndo(PPtr<Object> (m_DestroyObjectRedo), IsSceneUndo(), GetName());
		return true;
	}
	
	SInt32* instanceIDs;
	ALLOC_TEMP(instanceIDs, SInt32, m_Undos.size())
	for (int i=0;i<m_Undos.size();i++)
	{
		instanceIDs[i] = m_Undos[i].instanceID;
	}
	
	ObjectUndo* undo = UNITY_NEW(ObjectUndo, kMemUndo) (m_Name, 0);
	if(!undo->Store(m_Identifier, instanceIDs, m_Undos.size(), NULL))
	{
		ErrorCleanupUndo(undo);
		return false;
	}
	
	undo->SetNamePriority (m_NamePriority);
	// When objects are deleted we can't determine correctly based on the objects if it is a scene undo or not.
	undo->m_UndoType = m_UndoType;
	
	GetUndoManager().RegisterUndo(undo);
	
	return true;
}
 
bool ObjectUndo::Restore (bool registerRedo)
{
	if (registerRedo)
		RegisterRedo ();
	
	for (int i=0;i<m_Undos.size();i++)
	{
		GameObject* go = dynamic_instanceID_cast<GameObject*> (m_Undos[i].instanceID);
		if (go)
			go->Deactivate (kNormalDeactivate);
	}
	
	for (int i=0;i<m_Undos.size();i++)
		PrepareObject(m_Undos[i]);
	
	for (int i=0;i<m_Undos.size();i++)
		RestoreObject (m_Undos[i]);

	AwakeFromLoadQueue awakeQueue (kMemTempAlloc);
	for (int i=0;i<m_Undos.size();i++)
	{
		Object* object = dynamic_instanceID_cast<Object*> (m_Undos[i].instanceID);
		AddToAwakeFromLoadQueue(awakeQueue, object);
	}

	awakeQueue.CheckConsistency();
	awakeQueue.AwakeFromLoad(kDefaultAwakeFromLoad);
	
	// Identifier might have references to the undone objects. So let's give it a chance for recalcualtion (Case 14003)
	Object* identifier = dynamic_instanceID_cast<Object*> (m_Identifier);
	if (identifier)
		identifier->AwakeFromLoad(kDefaultAwakeFromLoad);

	GetSceneTracker().ForceReloadInspector();

	return true;
}
									 

unsigned ObjectUndo::GetAllocatedSize () const
{
	unsigned size = 0;
	for (int i=0;i<m_Undos.size();i++)
		size += GetAllocatedObjectSize(m_Undos[i]);

	return size;
}


bool ObjectUndo::Compare (Object* identifier, Object** objects, int size)
{
	if (identifier != dynamic_instanceID_cast<Object*> (m_Identifier) || size != m_Undos.size())
		return false;
	
	for (int i=0;i<m_Undos.size();i++)
	{
		if (m_Undos[i].isDestroyed)
			return false;
		
		if (objects[i] != dynamic_instanceID_cast<Object*> (m_Undos[i].instanceID))
			return false;
	}
	
	return true;
}

//----------------------------------------------------------------------------------------


void RegisterUndo (Object* o, const std::string &actionName, int namePriority)
{
	RegisterUndo(o, &o, 1, actionName, namePriority);
}

static void RegisterGameObjectHierrchyFull (Object& object, const std::string& actionName)
{
	std::vector<Object*> objects;
	CollectPPtrs (object, objects);
	
	RegisterUndoInternal (&object, &objects[0], objects.size(), &object, actionName, 0);
}

static void GetGameObjectAndComponents (Unity::GameObject& gameObject, vector_set<Object*>& objects)
{
	objects.insert(&gameObject);
	for (int i=0;i<gameObject.GetComponentCount();i++)
		objects.insert(&gameObject.GetComponentAtIndex(i));
}


static void RegisterGameObjectFull (Unity::GameObject& gameObject, Object* destroyComponent, const std::string& actionName)
{
	vector_set<Object*> objects;
	GetGameObjectAndComponents(gameObject, objects); 
	
	RegisterUndoInternal(&gameObject, &objects[0], objects.size(), destroyComponent, actionName, 0);
}


static void RegisterDestroyGameObjectUndoable (GameObject& go)
{
	// Unparent
	SetTransformParentUndo (go.GetComponent(Transform), NULL, Transform::kLocalPositionStays | Transform::kAllowParentingFromPrefab, "Delete");
	// Backup game object hierarchy
	RegisterGameObjectHierrchyFull (go, "Delete");
}

void RegisterFullObjectHierarchyUndo (Object* o)
{
	RegisterSelectionUndo();
	
	GameObject* go = dynamic_pptr_cast<GameObject*> (o);
	Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (o);
	if (go != NULL)
	{
		RegisterDestroyGameObjectUndoable(*go);
	}
	else if (com != NULL && com->GetGameObjectPtr())
	{
		RegisterGameObjectFull (com->GetGameObject(), com, "Delete");
	}	
	else
	{	
		RegisterUndo(o, "Delete", 0);
	}
}

void DestroyObjectUndoable (Object* o)
{
	RegisterFullObjectHierarchyUndo(o);
	
	DestroyObjectHighLevel(o, true);
}

static bool IsObjectArrayNonNull (Object** objects, int size)
{
	for (int i=0;i<size;i++)
	{
		if (objects[i] == NULL)
			return false;
	}
	
	return true;
}

static void InjectPrefabObjects (Object** inputObjects, int size, vector_set<Object*>& outputObjects)
{
	// Add any prefab objects into the undoable object streams too.
	for (int i=0;i<size;i++)
	{
		outputObjects.insert(inputObjects[i]);
		if (IsPrefabInstanceWithValidParent(inputObjects[i]))
			outputObjects.insert(GetPrefabFromAnyObjectInPrefab(inputObjects[i]));
	}
}


static void RegisterUndoInternal (Object* identifier, Object** inputObjects, int size, Object* destroyObjectRedo, const std::string &actionName, int namePriority)
{
	if (identifier == NULL || !IsObjectArrayNonNull (inputObjects, size))
	{
		ErrorString("Undo objects may not be null.");
		return;
	}

	vector_set<Object*> remappedObjects;
	InjectPrefabObjects (inputObjects, size, remappedObjects);
		
	// Don't register the undo if it snapshots the exact same set of objects like the last undo event,
	// and at the same time
	if (!GetUndoManager().IsUndoing () && !GetUndoManager().IsRedoing())
	{
		if (GetUndoManager().CompareLastUndoEvent (identifier, &remappedObjects[0], remappedObjects.size(), actionName, GetUndoManager().GetCurrentGroup()))
			return;
	}
	
	SET_ALLOC_OWNER(&GetUndoManager());
	ObjectUndo* undo = UNITY_NEW(ObjectUndo, kMemUndo) (actionName, namePriority);
	if (!undo->Store(identifier, &remappedObjects[0], remappedObjects.size(), destroyObjectRedo))
	{
		ErrorCleanupUndo (undo);
		return;
	}
	
	GetUndoManager().RegisterUndo(undo);
}

void RegisterUndo (Object* identifier, Object** inputObjects, int size, const std::string &actionName, int namePriority)
{
	RegisterUndoInternal (identifier, inputObjects, size, NULL, actionName, namePriority);
}

bool ObjectUndo::HasObject (int instanceID)
{
	for (int i=0;i<m_Undos.size();i++)
	{
		if (m_Undos[i].instanceID == instanceID)
			return true;
	}
	
	return false;
}

ObjectUndo* PrepareAddComponentUndo (GameObject& go)
{
	vector_set<Object*> objects;
	vector_set<Object*> objectsWithPrefab;
	
	GetGameObjectAndComponents (go, objects);
	InjectPrefabObjects (&objects[0], objects.size(), objectsWithPrefab);
	
	SET_ALLOC_OWNER(&GetUndoManager());
	ObjectUndo* undo = UNITY_NEW(ObjectUndo, kMemUndo) ("AddComponent", 0);
	if (!undo->Store(&go, &objectsWithPrefab[0], objectsWithPrefab.size(), NULL))
	{
		ErrorCleanupUndo (undo);
		return NULL;
	}
	
	return undo;
}

void PostAddComponentUndo (ObjectUndo* undo, GameObject& go)
{
	if (undo)
	{
		undo->StoreObjectDestroyedForAddedComponents (go);
		GetUndoManager().RegisterUndo(undo);
	}
}

Unity::Component* AddComponentUndoable (GameObject& go, int classID, MonoScript* script, std::string* error)
{
	ObjectUndo* undo = PrepareAddComponentUndo (go);
	Unity::Component* com = AddComponent(go, classID, script, error);
	PostAddComponentUndo (undo, go);
	return com;
}