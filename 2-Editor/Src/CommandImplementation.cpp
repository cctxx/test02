#include "UnityPrefix.h"
#include "CommandImplementation.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Selection.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "EditorHelper.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Platform/Interface/Pasteboard.h"
#include "DragAndDropForwarding.h"
#include "GUIDPersistentManager.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Editor/Src/Graphs/GraphUtils.h"
#include "Editor/Src/Animation/StateMachine.h"
#include "Runtime/Mono/MonoUtility.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Scripting/Scripting.h"

using namespace std;
using namespace Unity;

class GameObjectCopyPaste;
static bool PasteGameObjectPasteboardData (GameObjectCopyPaste& pasteData);
static GameObjectCopyPaste* CopyGameObjectPasteboardData ();

const char* kGameObjectPasteboardCopy = "GameObjectPasteboardCopy";
const char* kComponentPasteboardCopy = "ComponentPasteboardCopy";
const char* kGraphAndNodesPasteboardCopy = "GraphAndNodesPasteboardCopy";
const char* kStatePasteboardCopy = "StatePasteboardCopy";
const char* kStateMachinePasteboardCopy = "StateMachinePasteboardCopy";

struct SerializedData
{
	int instanceID;
	int classID;
	PPtr<MonoScript> script;
	TypeTree typeTree;
	dynamic_array<UInt8> data;
};

class GameObjectCopyPaste : public PasteboardData
{
	public:
	
	typedef list<SerializedData> SerializedDataList;
	SerializedDataList data;
	string name;
};

class ComponentCopyPaste : public PasteboardData
{
public:
	std::vector<SerializedData> data;
	string name;
};

class GraphCopyPaste : public GameObjectCopyPaste
{
	public:
	int sourceGraphBehaviourInstanceID;
};

class ObjectCopyPaste : public PasteboardData
{
	public:
	
	typedef list<SerializedData> SerializedDataList;
	SerializedDataList data;
	string name;
};

class StateMachineObjectCopyPaste : public ObjectCopyPaste  // used for State and StateMachine copy
{
	public:
	AnimatorControllerParameterVector parameters;

};

inline SerializedData& AddPrioritizedSerializedData(list<SerializedData>& serializedData, SInt32 instanceID)
{
	// Logic nodes need to be instantiated before graphs
	if (IsNode(dynamic_instanceID_cast<Object*> (instanceID)))
	{
		serializedData.push_front (SerializedData());
		return serializedData.front();
	}
	else
	{
		serializedData.push_back (SerializedData());
		return serializedData.back();
	}
}

void MakeSerializedDataRootTransform (Transform& root, dynamic_array<UInt8>& data)
{
	vector<PropertyModification> modifications;
	
	Vector3f pos = root.GetPosition ();
	modifications.push_back(CreatePropertyModification ("m_LocalPosition.x", FloatToString(pos.x)));
	modifications.push_back(CreatePropertyModification ("m_LocalPosition.y", FloatToString(pos.y)));
	modifications.push_back(CreatePropertyModification ("m_LocalPosition.z", FloatToString(pos.z)));
	
	Quaternionf rot = root.GetRotation ();
	modifications.push_back(CreatePropertyModification ("m_LocalRotation.x", FloatToString(rot.x)));
	modifications.push_back(CreatePropertyModification ("m_LocalRotation.y", FloatToString(rot.y)));
	modifications.push_back(CreatePropertyModification ("m_LocalRotation.z", FloatToString(rot.z)));
	modifications.push_back(CreatePropertyModification ("m_LocalRotation.w", FloatToString(rot.w)));
	
	Vector3f scale = root.GetWorldScaleLossy ();
	modifications.push_back(CreatePropertyModification ("m_LocalScale.x", FloatToString(scale.x)));
	modifications.push_back(CreatePropertyModification ("m_LocalScale.y", FloatToString(scale.y)));
	modifications.push_back(CreatePropertyModification ("m_LocalScale.z", FloatToString(scale.z)));
	
	modifications.push_back(CreatePropertyModification ("m_Father", PPtr<Object> ()));
	
	const TypeTree& transformTypeTree = GenerateCachedTypeTree(root, 0);
	ErrorIf(!ApplyPropertyModification (transformTypeTree, data, &modifications[0], modifications.size()));
}


void WriteSerializedCopy (const set<SInt32>& ptrs, list<SerializedData>& serializedData)
{
	// Generate serialized Data, convert non-shared pptrs to indexed pptrs
	for (set<SInt32>::const_iterator i = ptrs.begin ();i != ptrs.end ();i++)
	{
		SerializedData& data = AddPrioritizedSerializedData(serializedData, *i);
		
		// setup serialize data
		Object& object = *dynamic_instanceID_cast<Object*> (*i);

		if (object.GetNeedsPerObjectTypeTree())
			GenerateTypeTree (object, &data.typeTree);
		data.instanceID = object.GetInstanceID ();
		data.classID = object.GetClassID ();
		MonoBehaviour* beh = dynamic_pptr_cast<MonoBehaviour*>(&object);
		if (beh)
			data.script = beh->GetScript();
		
		// Save object state, clear the prefab pointer if the prefab is not part of what we are duplicating
		EditorExtension* extension = dynamic_pptr_cast<EditorExtension*>(&object);
		PPtr<Prefab> oldPrefabValue;
		if (extension)
		{
			oldPrefabValue = extension->m_Prefab;
			if (ptrs.count(oldPrefabValue.GetInstanceID()) == 0)
				extension->m_Prefab = NULL;
		}
		
		WriteObjectToVector (object, &data.data);
	
		Transform* transform = dynamic_pptr_cast<Transform*>(&object);
		if (transform && transform->GetParent () != NULL && ptrs.count (transform->GetParent ()->GetInstanceID ()) == 0)
			MakeSerializedDataRootTransform(*transform, data.data);
		
		if (extension)
			extension->m_Prefab = oldPrefabValue;
	}
}

struct RemapPPtrsFunctor
{
	map<SInt32, SInt32>& m_OldIDToNewID;
	bool	m_AllowSceneRefs;
	
	RemapPPtrsFunctor (map<SInt32, SInt32>& m, bool allowSceneRefs) : m_OldIDToNewID (m), m_AllowSceneRefs(allowSceneRefs) {}
	
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (!IsTypeTreePPtr (typeTree))
			return true;
		
		SInt32 instanceID = ExtractPPtrInstanceID(data, bytePosition);
		map<SInt32, SInt32>::iterator found = m_OldIDToNewID.find(instanceID);
		if (found != m_OldIDToNewID.end())
			instanceID = found->second;
		else if (!m_AllowSceneRefs && GetPersistentManager().GetPathName(instanceID).empty())
			instanceID = 0;
		
		SetPPtrInstanceID(instanceID, data, bytePosition);
		return false;
	}
};

void LoadAndSetupInstantiatedLogicGraphs(const vector<PPtr<Object> > instantiatedObjects)
{
	vector<PPtr<Object> >::const_iterator i;
	for (i = instantiatedObjects.begin(); i != instantiatedObjects.end(); i++)
	{
		MonoBehaviour* graphData = GetEditorGraphData(i->GetInstanceID());
		if (!graphData)
			continue;

		// set new name for logic graph
		graphData->SetName(GenerateGraphName().c_str());

		// underlying script of the monobehaviour was changed, need to load mono side representation
		MonoBehaviour *newBehaviour = dynamic_pptr_cast<MonoBehaviour*>(*i);
		newBehaviour->RebuildMonoInstance (NULL);
	}
}

static Object& ProduceAndReset (const SerializedData& data)
{
	Object* o = Object::Produce (data.classID);
	AssertIf (o == NULL);

	o->Reset();

	MonoBehaviour* cloneBehaviour = dynamic_pptr_cast<MonoBehaviour*> (o);
	if (cloneBehaviour)
		cloneBehaviour->SetScript(data.script);
	
	return *o;
}


vector<PPtr<Object> > InstantiateSerializedCopy (const list<SerializedData>& serializedData, map<SInt32, SInt32>* additionalRemap = NULL)
{
	// Produce and build remap for all objects
	map<SInt32, SInt32> oldIDToNewID;
	vector<SInt32> objects;
	vector<ObjectData> objectData;
	list<SerializedData>::const_iterator i;

	if (additionalRemap)
		oldIDToNewID.insert(additionalRemap->begin(), additionalRemap->end());

	for (i=serializedData.begin ();i!=serializedData.end ();i++)
	{
		const SerializedData& data = *i;
		
		Object& clone = ProduceAndReset(data);
		
		AssertIf (oldIDToNewID.find (data.instanceID) != oldIDToNewID.end ());
		oldIDToNewID.insert (make_pair (data.instanceID, clone.GetInstanceID ()));
		objects.push_back (clone.GetInstanceID ());
		objectData.push_back(ObjectData(&clone, NULL));
	}
	
	// Transfer state, override with empty data where forceoverride is needed, remap pptrs, copy name
	vector<PPtr<Object> > instantiatedObjects;
	
	dynamic_array<UInt8> tempData(kMemTempAlloc);

	for (i=serializedData.begin ();i!=serializedData.end ();i++)
	{
		const SerializedData& data = *i;
		Object& object = *dynamic_instanceID_cast<Object*> (oldIDToNewID.find (data.instanceID)->second);
		
		// We have to remap the data before reading it because MonoBehaviours might have internal references
		// which will be nulled if we remap afterwards if the source no longer exists
		int position = 0;
		RemapPPtrsFunctor remap (oldIDToNewID, true);
		
		tempData = data.data;
		
		// Read original, override with empty cloned data
		if (object.GetNeedsPerObjectTypeTree ())
		{
			IterateTypeTree (data.typeTree, tempData, &position, remap);
			ReadObjectFromVector (&object, tempData, data.typeTree);
		}
		else
		{
			const TypeTree& tempTypeTree = GenerateCachedTypeTree(object, 0);
			IterateTypeTree (tempTypeTree, tempData, &position, remap);

			ReadObjectFromVector (&object, tempData);
		}
		

		instantiatedObjects.push_back (&object);
	}

	// Awake and set dirty
	DirtyAndAwakeFromLoadAfterMerge (objectData, true);
	
	LoadAndSetupInstantiatedLogicGraphs(instantiatedObjects);

	return instantiatedObjects;
}

bool PasteGameObjectsFromPasteboard ()
{
	GameObjectCopyPaste* pasteData = dynamic_cast<GameObjectCopyPaste*> (Pasteboard::GetPasteboardData(kGameObjectPasteboardCopy));
	if (pasteData == NULL)
		return false;

	return PasteGameObjectPasteboardData (*pasteData);
}

bool CopyGameObjectsToPasteboard ()
{
	GameObjectCopyPaste* data = CopyGameObjectPasteboardData ();
	if (data == NULL)
		return false;
	
	Pasteboard::SetPasteboardData(kGameObjectPasteboardCopy, data);
	return true;
}

static void ExtractLogicGraphsAndNodes(GameObject *go, set<SInt32> &ptrIsland)
{
	vector<int> allGraphs = AllGraphsOnGameObject(go);
	for (vector<int>::iterator i = allGraphs.begin(); i != allGraphs.end(); i++)
	{
		MonoBehaviour* graphData = GetEditorGraphData(*i);
		if (graphData == NULL)
			continue;

		CollectPPtrs(*dynamic_instanceID_cast<MonoBehaviour*>(*i)->GetScript(), &ptrIsland);
		CollectPPtrs(*graphData, &ptrIsland);

		ScriptingInvocation invocation(kGraphsEditorBaseNamespace, "Graph", "GetNodeIdsForSerialization");
		invocation.AddObject(Scripting::ScriptingWrapperFor(graphData));
		MonoObject* nodeIds = invocation.Invoke();

		AssertIf(!nodeIds);
		if (!nodeIds)
			continue;

		vector<int> result;
		MonoArrayToVector((MonoArray*)nodeIds, result);
		ptrIsland.insert(result.begin(), result.end());
	}
}

static GameObjectCopyPaste* CopyGameObjectPasteboardData ()
{
	// Only allow copy&paste of scene gameobjects
	set<GameObject*> selection = GetGameObjectSelection (kExcludePrefabSelection | kOnlyUserModifyableSelection);
	
	set<SInt32> ptrIsland;
	GameObject* rootObject = NULL;
	
	// Extract all objects that we will clone
	for (set<GameObject*>::iterator i = selection.begin ();i != selection.end ();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		CollectPPtrs (*go, &ptrIsland);

		ExtractLogicGraphsAndNodes(*i, ptrIsland);

		rootObject = go;
	}

	if (ptrIsland.empty ())
		return NULL;
	
	// Add all prefabs of which we have copied all objects in the prefab
	set<Prefab*> prefabsAlreadyChecked;
	for (set<SInt32>::iterator i = ptrIsland.begin ();i != ptrIsland.end ();i++)
	{
		Prefab* prefab = GetPrefabFromAnyObjectInPrefab(dynamic_instanceID_cast<Object*> (*i));
		if (prefab == NULL || prefabsAlreadyChecked.count(prefab))
			continue;
		prefabsAlreadyChecked.insert(prefab);
		
		vector<Object*> prefabObjects;
		GetObjectArrayFromPrefabRoot(*prefab, prefabObjects);
		
		bool containsAllObjectsInPrefab = true;
		for (int i=0;i<prefabObjects.size();i++)
		{
			if (ptrIsland.count(prefabObjects[i]->GetInstanceID()) == 0)
				containsAllObjectsInPrefab = false;
		}

		if (containsAllObjectsInPrefab)
			ptrIsland.insert(prefab->GetInstanceID());
	}
	
	GameObjectCopyPaste* data = new GameObjectCopyPaste ();
	WriteSerializedCopy (ptrIsland, data->data);

	if (rootObject)
		data->name = rootObject->GetName();
	
	return data;
}

static bool PasteGameObjectPasteboardData (GameObjectCopyPaste& pasteData)
{
	// Find the transform we should paste this as a child of.
	Transform* pasteAsChildOf = NULL;
	TempSelectionSet selection;
	Selection::GetSelection(selection);
	for (TempSelectionSet::iterator i=selection.begin ();i != selection.end ();i++)
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (*i);
		if (go != NULL && !go->TestHideFlag(Object::kHideInHierarchy) && !go->IsPrefabParent () && go->QueryComponent (Transform) != NULL)
			pasteAsChildOf = go->QueryComponent (Transform)->GetParent ();
	}
	GameObject* go = Selection::GetActiveGO();
	if (go != NULL && !go->TestHideFlag(Object::kHideInHierarchy) && !go->IsPrefabParent () && go->QueryComponent (Transform) != NULL)	
		pasteAsChildOf = go->QueryComponent (Transform)->GetParent ();
	
	// Instantiate the serialized data
	vector<PPtr<Object> > instantiatedObjects = InstantiateSerializedCopy (pasteData.data);

	// Build the new selection (All instantiated gameobjects)
	selection.clear();
	set<Object*> instantiatedObjectsSet;
	for (vector<PPtr<Object> >::iterator i=instantiatedObjects.begin ();i != instantiatedObjects.end ();i++)
		instantiatedObjectsSet.insert (*i);
	
	for (set<Object*>::iterator i=instantiatedObjectsSet.begin ();i != instantiatedObjectsSet.end ();i++)
	{
		go = dynamic_pptr_cast<GameObject*> (*i);
		if (go && !go->TestHideFlag(Object::kHideInHierarchy) && !HasParentInSelection (*go, instantiatedObjectsSet))
			selection.insert (go);
	}
	
	vector<Object*> selectionArray (selection.begin(), selection.end());

	Selection::SetSelection(selection);

	string actionName = "Paste " + pasteData.name;
	for (int i=0;i<selectionArray.size();i++)
		RegisterCreatedObjectUndo (selectionArray[i], actionName);

	vector<GameObject*> instantiatedRootObjects;
	// Attach instanted Objects as child of pastAsChildOf
	for (vector<PPtr<Object> >::iterator i=instantiatedObjects.begin ();i != instantiatedObjects.end ();i++)
	{
		go = dynamic_pptr_cast<GameObject*> (*i);
		if (go != NULL && !go->TestHideFlag(Object::kHideInHierarchy) && !go->IsPrefabParent () && go->QueryComponent (Transform) != NULL && go->QueryComponent (Transform)->GetParent () == NULL)
		{
			Transform& transform = go->GetComponent (Transform);

			SetTransformParentUndo (transform, pasteAsChildOf, actionName);
			transform.MakeEditorValuesLookNice();
		}
	}
	
	return true;
}

bool DuplicateGameObjectsUsingPasteboard ()
{
	GameObjectCopyPaste* temp = CopyGameObjectPasteboardData();
	bool success = false;
	if (temp && PasteGameObjectPasteboardData (*temp))
		success = true;

	delete temp;
	return success;
}

///@TODO: There was another implementation of this somewhere. FInd and replace...
bool IsParentInSelection (GameObject& go, const set<GameObject*>& gameObjects)
{
	Transform* parent = go.GetComponent(Transform).GetParent();
	while (parent)
	{
		if (gameObjects.count (&parent->GetGameObject()))
			return true;
		
		parent = parent->GetParent();
	}
	
	return false;
}

void DeleteGameObjectSelection ()
{
	set<PPtr<Object> > deleteObjects;
	set<GameObject*> selection = GetGameObjectSelection (kExcludePrefabSelection | kOnlyUserModifyableSelection);

	for (set<GameObject*>::iterator i = selection.begin ();i != selection.end ();i++)
	{
		GameObject* go = *i;
		if (!IsParentInSelection (*go, selection))
			deleteObjects.insert (go);
	}

	for (set<PPtr<Object> >::iterator i = deleteObjects.begin ();i != deleteObjects.end ();i++)
		DestroyObjectUndoable (*i);
}

GraphCopyPaste* CopyGraphAndNodesPasteboardData (int sourceGraphBehaviourInstanceID, const set<SInt32>& ptrs)
{
	if (ptrs.empty())
		return NULL;

	GraphCopyPaste* data = new GraphCopyPaste ();
	WriteSerializedCopy (ptrs, data->data);
	data->sourceGraphBehaviourInstanceID = sourceGraphBehaviourInstanceID;

	return data;
}

bool CopyGraphAndNodesToPasteboard (int sourceGraphBehaviourInstanceID, const set<SInt32>& ptrs)
{
	GraphCopyPaste* data = CopyGraphAndNodesPasteboardData(sourceGraphBehaviourInstanceID, ptrs);
	if (data == NULL)
		return false;

	Pasteboard::SetPasteboardData(kGraphAndNodesPasteboardCopy, data);
	return true;
}

Object* InstantiateGraphAndNodesPasteboardData (int destinationGraphBehaviourInstanceID, GraphCopyPaste* pasteData)
{
	map<SInt32, SInt32> additionalRemap;
	additionalRemap.insert(make_pair (pasteData->sourceGraphBehaviourInstanceID, destinationGraphBehaviourInstanceID) );

	vector<PPtr<Object> > instantiatedObjects = InstantiateSerializedCopy (pasteData->data, &additionalRemap);

	// find the instantiated graph
	for (vector<PPtr<Object> >::const_iterator i = instantiatedObjects.begin(); i != instantiatedObjects.end(); i++)
		if (IsGraph(*i))
			return *i;

	return NULL;
}

Object* InstantiateGraphAndNodesFromPasteboard (int destinationGraphBehaviourInstanceID)
{
	GraphCopyPaste* pasteData = dynamic_cast<GraphCopyPaste*> (Pasteboard::GetPasteboardData(kGraphAndNodesPasteboardCopy));
	if (pasteData == NULL)
		return NULL;
	return InstantiateGraphAndNodesPasteboardData(destinationGraphBehaviourInstanceID, pasteData);
}

Object* DuplicateGraphAndNodesUsingPasteboard (int graphBehaviourInstanceID, const std::set<SInt32>& ptrs)
{
	GraphCopyPaste* temp = CopyGraphAndNodesPasteboardData(graphBehaviourInstanceID, ptrs);
	Object* instance = NULL;
	if (temp)
		instance = InstantiateGraphAndNodesPasteboardData (graphBehaviourInstanceID, temp);

	delete temp;
	return instance;
}

bool CopyComponentToPasteboard (Unity::Component* comp)
{
	if (comp == NULL)
		return false;

	ComponentCopyPaste* pasteData = new ComponentCopyPaste ();
	
	pasteData->name = GetNiceObjectType(comp);
	
	while (comp)
	{
		// Prepend the component's serialized data to the array to make
		// sure we list coupled components before components that depend 
		// on them. This makes sure we add the components in the right 
		// order later (instead of having coupled components automatically 
		// created). 
		pasteData->data.insert(pasteData->data.begin(), SerializedData());
		SerializedData& data = pasteData->data[0];

		// setup serialize data
		Object& object = *comp;

		if (object.GetNeedsPerObjectTypeTree())
			GenerateTypeTree (object, &data.typeTree, kSerializeForPrefabSystem);
		data.instanceID = object.GetInstanceID ();
		data.classID = object.GetClassID ();
		MonoBehaviour* beh = dynamic_pptr_cast<MonoBehaviour*>(&object);
		if (beh)
			data.script = beh->GetScript();
		
		// clear prefab pointer
		EditorExtension* extension = dynamic_pptr_cast<EditorExtension*>(&object);
		PPtr<Prefab> oldPrefabValue;
		if (extension)
		{
			oldPrefabValue = extension->m_Prefab;
			extension->m_Prefab = NULL;
		}

		WriteObjectToVector (object, &data.data, kSerializeForPrefabSystem);

		if (extension)
			extension->m_Prefab = oldPrefabValue;
		
		// continue copying in any coupled components
		Unity::Component* nextComponent = NULL;
		int coupledClassID = comp->GetCoupledComponentClassID();
		if (coupledClassID >= 0)
		{
			GameObject* go = comp->GetGameObjectPtr();
			if (go)
			{
				nextComponent = go->QueryComponentT<Unity::Component>(coupledClassID);
			}
		}
		comp = nextComponent;
	}

	Pasteboard::SetPasteboardData(kComponentPasteboardCopy, pasteData);
	return true;
}

void ClearComponentInPasteboard ()
{
	Pasteboard::SetPasteboardData ("__dummy", NULL);
}

bool HasComponentInPasteboard (int& outClassID)
{
	ComponentCopyPaste* pasteData = dynamic_cast<ComponentCopyPaste*> (Pasteboard::GetPasteboardData(kComponentPasteboardCopy));
	if (pasteData == NULL)
		return false;
	if (pasteData->data.empty())
		return false;

	outClassID = pasteData->data.front().classID;
	return true;
}

bool HasMatchingComponentInPasteboard (Unity::Component* comp)
{
	if (comp == NULL)
		return false;
	ComponentCopyPaste* pasteData = dynamic_cast<ComponentCopyPaste*> (Pasteboard::GetPasteboardData(kComponentPasteboardCopy));
	if (pasteData == NULL)
		return false;

	if (GetNiceObjectType(comp) != pasteData->name)
		return false;

	return true;
}

static void ApplyComponentPasteboardData (const SerializedData& data, Object& object, bool allowSceneRefs)
{
	map<SInt32, SInt32> oldIDToNewID;
	oldIDToNewID.insert (make_pair (data.instanceID, object.GetInstanceID ()));

	dynamic_array<UInt8> tempData(kMemTempAlloc);

	// We have to remap the data before reading it because MonoBehaviours might have internal references
	// which will be nulled if we remap afterwards if the source no longer exists
	int position = 0;
	RemapPPtrsFunctor remap (oldIDToNewID, allowSceneRefs);

	tempData = data.data;

	// Read original, override with empty cloned data
	if (object.GetNeedsPerObjectTypeTree ())
	{
		IterateTypeTree (data.typeTree, tempData, &position, remap);
		ReadObjectFromVector (&object, tempData, data.typeTree, kSerializeForPrefabSystem);
	}
	else
	{
		const TypeTree& tempTypeTree = GenerateCachedTypeTree(object, kSerializeForPrefabSystem);
		IterateTypeTree (tempTypeTree, tempData, &position, remap);

		ReadObjectFromVector (&object, tempData, kSerializeForPrefabSystem);
	}
}

bool PasteComponentFromPasteboard (Unity::GameObject* go)
{
	ComponentCopyPaste* pasteData = dynamic_cast<ComponentCopyPaste*> (Pasteboard::GetPasteboardData(kComponentPasteboardCopy));
	if (pasteData == NULL)
		return false;

	if (!go)
		return false;

	const bool pastingOntoPrefab = go->IsPrefabParent();

	string actionName = "Paste New " + pasteData->name;
	vector<ObjectData> objectData;
	
	for (size_t i = 0; i < pasteData->data.size(); ++i)
	{
		const SerializedData& data = pasteData->data[i];

		if (!Object::IsDerivedFromClassID (data.classID, ClassID(Component)))
		{
			AssertString ("Pasted component data not derived from component");
			return false;
		}

		// Instantiate the serialized data
		Unity::Component* clone = AddComponentUndoable(*go, data.classID, data.script);
		if (clone == NULL)
			return false;

		ApplyComponentPasteboardData (data, *clone, !pastingOntoPrefab);
		
		Assert(clone->GetGameObjectPtr() == go);

		objectData.push_back(ObjectData(clone, NULL));
	}

	DirtyAndAwakeFromLoadAfterMerge (objectData, true);

	return true;
}


bool PasteComponentValuesFromPasteboard (Unity::Component* mainDest)
{
	ComponentCopyPaste* pasteData = dynamic_cast<ComponentCopyPaste*> (Pasteboard::GetPasteboardData(kComponentPasteboardCopy));
	if (pasteData == NULL)
		return false;

	if (!mainDest)
		return false;
	GameObject* go = mainDest->GetGameObjectPtr();
	const bool pastingOntoPrefab = go->IsPrefabParent();

	Unity::Component* dest;
	vector<ObjectData> objectData;
	
	for (size_t i = 0; i < pasteData->data.size(); ++i)
	{
		const SerializedData& data = pasteData->data[i];

		if (!Object::IsDerivedFromClassID (data.classID, ClassID(Component)))
		{
			AssertString ("Pasted component data not derived from component");
			return false;
		}
		
		if (i == pasteData->data.size()-1)
		{
			//The main component we are copying/pasting will be at the last on in the vector
			//since we start copying the dependencies first.
			dest = mainDest;
		} 
		else 
		{
			//Find the Coupled Components dependency we should past to.
			dest = go->QueryComponentT<Unity::Component>(data.classID);
			if (!dest)
				break;
		}

		RecordUndoDiff (dest, "Paste " + pasteData->name + " Values");

		ApplyComponentPasteboardData (data, *dest, !pastingOntoPrefab);

		dest->SetGameObjectInternal (go);

		objectData.push_back(ObjectData(dest, NULL));
	}
	
	DirtyAndAwakeFromLoadAfterMerge (objectData, true);

	return true;
}

static set<SInt32> CollectAndFilterPPtrs (Object& obj, std::set<int> filterClassId = std::set<int>())
{
	set<SInt32> ptrIsland;

	CollectPPtrs (obj, &ptrIsland);

	if (ptrIsland.empty ())
		return ptrIsland;

	// Add all prefabs of which we have copied all objects in the prefab
	set<Prefab*> prefabsAlreadyChecked;
	for (set<SInt32>::iterator i = ptrIsland.begin ();i != ptrIsland.end ();i++)
	{
		Prefab* prefab = GetPrefabFromAnyObjectInPrefab(dynamic_instanceID_cast<Object*> (*i));
		if (prefab == NULL || prefabsAlreadyChecked.count(prefab))
			continue;
		prefabsAlreadyChecked.insert(prefab);
		
		vector<Object*> prefabObjects;
		GetObjectArrayFromPrefabRoot(*prefab, prefabObjects);
		
		bool containsAllObjectsInPrefab = true;
		for (int i=0;i<prefabObjects.size();i++)
		{
			if (ptrIsland.count(prefabObjects[i]->GetInstanceID()) == 0)
				containsAllObjectsInPrefab = false;
		}

		if (containsAllObjectsInPrefab)
			ptrIsland.insert(prefab->GetInstanceID());
	}

	for ( set<SInt32>::iterator it = ptrIsland.begin(); it != ptrIsland.end(); )
	{
		Object& object = *dynamic_instanceID_cast<Object*>(*it);
		if(filterClassId.count(object.GetClassID()) > 0)
		{
			set<SInt32>::iterator deleteIt = it;
			++it;
			ptrIsland.erase(deleteIt);
		}
		else
			++it;
	}

	return ptrIsland;
	
		
}

static bool CopyObjectToPasteboard (Object* obj, char const* type, set<int> filter = set<int>())
{
	if (obj == NULL)
		return false;

	set<SInt32> ptrIsland =  CollectAndFilterPPtrs(*obj, filter);
	if(ptrIsland.empty())
		return false;

	ObjectCopyPaste* pasteData = new ObjectCopyPaste ();

	WriteSerializedCopy (ptrIsland, pasteData->data);
	pasteData->name = obj->GetName();

	Pasteboard::SetPasteboardData(type, pasteData);
	return true;
}

bool HasStateMachineDataInPasteboard ()
{
	ObjectCopyPaste* pasteData = NULL;

	pasteData = dynamic_cast<ObjectCopyPaste*> (Pasteboard::GetPasteboardData(kStatePasteboardCopy));
	if (pasteData != NULL)
		return true;

	pasteData = dynamic_cast<ObjectCopyPaste*> (Pasteboard::GetPasteboardData(kStateMachinePasteboardCopy));		
	if (pasteData != NULL)
		return true;

	return false;
}


void AddParametersToPasterBoard(StateMachineObjectCopyPaste* pasteData, const BlendParameterList&  requiredParameters,  AnimatorController const* animatorController)
{	
	for(BlendParameterList::const_iterator it = requiredParameters.begin() ; it != requiredParameters.end() ; it++)
	{
		int parameterIndex = animatorController->FindParameter((*it));

		if(parameterIndex != -1)
		{			
			pasteData->parameters.push_back(*animatorController->GetParameter(parameterIndex));
		}
	}	
}

bool CopyStateToPasteboard(State* state, AnimatorController const* animatorController)
{
	set<int> filter;
	filter.insert( ClassID(AnimationClip));

	set<SInt32> ptrIsland =  CollectAndFilterPPtrs(*state, filter);
	if(ptrIsland.empty())
		return false;

	StateMachineObjectCopyPaste* pasteData = new StateMachineObjectCopyPaste();

	WriteSerializedCopy (ptrIsland, pasteData->data);
	pasteData->name = state->GetName();
	AddParametersToPasterBoard(pasteData, state->CollectParameters(), animatorController);
	

	Pasteboard::SetPasteboardData(kStatePasteboardCopy, pasteData);

	return true;
	
}

bool CopyStateMachineToPasteboard(StateMachine* stateMachine, AnimatorController const* animatorController)
{
	stateMachine->SyncTransitionsFromRoot();
	set<int> filter;
	filter.insert( ClassID(AnimationClip));

	set<SInt32> ptrIsland =  CollectAndFilterPPtrs(*stateMachine, filter);
	if(ptrIsland.empty())
		return false;

	StateMachineObjectCopyPaste* pasteData = new StateMachineObjectCopyPaste();

	WriteSerializedCopy (ptrIsland, pasteData->data);
	pasteData->name = stateMachine->GetName();	
	AddParametersToPasterBoard(pasteData, stateMachine->CollectParameters(), animatorController);
	
	Pasteboard::SetPasteboardData(kStateMachinePasteboardCopy, pasteData);	
	
	return true;
}

bool PasteToStateMachineFromPasteboard(StateMachine* stateMachine, AnimatorController* animatorController)
{
	if (stateMachine == NULL)
		return false;

	StateMachineObjectCopyPaste* pasteData = dynamic_cast<StateMachineObjectCopyPaste*> (Pasteboard::GetPasteboardData(kStatePasteboardCopy));
	if (pasteData == NULL)
	{
		pasteData = dynamic_cast<StateMachineObjectCopyPaste*> (Pasteboard::GetPasteboardData(kStateMachinePasteboardCopy));
		if (pasteData == NULL)
			return false;
	}

	vector< PPtr<Object> > instantiatedObjects = InstantiateSerializedCopy (pasteData->data);
	
	if(instantiatedObjects.size() > 0)
	{
		PPtr<Object> rootObject = *instantiatedObjects.begin();
		for (vector<PPtr<Object> >::iterator i=instantiatedObjects.begin();i != instantiatedObjects.end ();i++)
		{
			PPtr<Object> object = *i;
			if (object.IsValid() && object->GetName() == pasteData->name)
				rootObject = *i;

			State* state = dynamic_pptr_cast<State*>( object );
			if(state)
				state->SetHideFlags(Object::kHideInHierarchy | Object::kHideInspector);

			Transition* transition = dynamic_pptr_cast<Transition*>( object );
			if(transition)
				transition->SetHideFlags(Object::kHideInHierarchy | Object::kHideInspector);			
		}

		if(rootObject.IsValid())
		{
			State* state = dynamic_pptr_cast<State*>( rootObject );
			if(state)
				stateMachine->AddState(state);

			StateMachine* sm = dynamic_pptr_cast<StateMachine*>( rootObject );
			if(sm)
			{
				stateMachine->AddStateMachine(sm);				
				sm->FixStateParent();
			} 

			for(AnimatorControllerParameterVector::const_iterator it = pasteData->parameters.begin(); it != pasteData->parameters.end() ; it++)
			{				
				int parameterIndex = animatorController->FindParameter((it)->GetName()) ;								
				if( parameterIndex == -1 || animatorController->GetParameter(parameterIndex)->GetType() !=  (it)->GetType())
				{					
					animatorController->AddParameter((it)->GetName(), (it)->GetType());
					UnityStr newParameterName = animatorController->GetParameter(animatorController->GetParameterCount()-1)->GetName();
					if(newParameterName != (it)->GetName())
						stateMachine->RenameParameter(newParameterName, (it)->GetName());
				}
			}
		}

		// Add all pasted asset to 'stateMachine' asset file, don't do rootObject because it already done by 
		//	AddState and AddStateMachine
		for (vector<PPtr<Object> >::iterator i=instantiatedObjects.begin();i != instantiatedObjects.end ();i++)
		{
			PPtr<Object> object = *i;
			if(object != rootObject)
			{
				EditorExtension* editorExtension = dynamic_pptr_cast<EditorExtension*>(object);
				if(stateMachine->IsPersistent() && editorExtension != NULL) 
					AddAssetToSameFile(*editorExtension, *stateMachine, true);
			}
		}
	}

	return true;
}
