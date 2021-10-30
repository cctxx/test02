#include "UnityPrefix.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Src/Utility/BaseHierarchyProperty.h"

#include "GraphUtils.h"

using namespace std;

const char* kGraphsEditorBaseNamespace = "UnityEditor.Graphs";
const char* kLogicGraphEditorNamespace = "UnityEditor.Graphs.LogicGraph";
const char* kAnimationBlendTreeNamespace = "UnityEditor.Graphs.AnimationBlendTree";
const char* kAnimationStateMachineNamespace = "UnityEditor.Graphs.AnimationStateMachine";

// From BaseHierarchyProperty.cpp (TODO: make proper interface)
bool IsMonoClassDerivedFromClass (MonoClass* klass, MonoClass* searchForThisMonoClass);


MonoBehaviour* GetEditorGraphData(const MonoBehaviour* behaviour)
{
	if (behaviour == NULL)
		return NULL;
	MonoScript* script = behaviour->GetScript();
	if (script == NULL)
		return NULL;
	return dynamic_pptr_cast<MonoBehaviour*> (script->GetEditorGraphData());
}

MonoBehaviour* GetEditorGraphData(int graphBehaviourComponentsInstanceID)
{
	return GetEditorGraphData(dynamic_instanceID_cast<MonoBehaviour*>(graphBehaviourComponentsInstanceID));
}

GameObject *GetGraphComponentsGameObject(int graphBehaviourComponentsInstanceID)
{
	MonoBehaviour* behaviour = dynamic_instanceID_cast<MonoBehaviour*> (graphBehaviourComponentsInstanceID);
	if (behaviour == NULL)
		return NULL;
	return behaviour->GetGameObjectPtr();
}

bool IsGraphComponent(const MonoBehaviour *behaviour)
{
	return GetEditorGraphData(behaviour) != NULL;
}

//TODO: optimize this
bool IsNode(const Object* object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*>(object);
	if (behaviour == NULL || behaviour->GetClass() == NULL)
		return false;

	string nameSpace = mono_class_get_namespace(behaviour->GetClass());

	MonoClass* searchForThisClass = GetMonoManager().GetMonoClass("Node", "");
	return nameSpace.compare(0, strlen (kGraphsEditorBaseNamespace), kGraphsEditorBaseNamespace) == 0 &&
		IsMonoClassDerivedFromClass(behaviour->GetClass(), searchForThisClass);
}

//TODO: optimize this
bool IsGraph(const Object* object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*>(object);
	if (behaviour == NULL || behaviour->GetClass() == NULL)
		return false;

	string nameSpace = mono_class_get_namespace(behaviour->GetClass());
	
	MonoClass* searchForThisClass = GetMonoManager().GetMonoClass("Graph", "");
	return nameSpace.compare(0, strlen (kGraphsEditorBaseNamespace), kGraphsEditorBaseNamespace) == 0 &&
		IsMonoClassDerivedFromClass(behaviour->GetClass(), searchForThisClass);
}

vector<int> AllGraphsOnGameObject(const GameObject* go)
{
	vector<int> ids;

	for (int i = 0; i < go->GetComponentCount(); i++) 
	{
		Unity::Component* component = &go->GetComponentAtIndex(i);
		if (IsGraphComponent(dynamic_pptr_cast<MonoBehaviour*> (component)))
			ids.push_back(component->GetInstanceID());
	}

	return ids; 
}

vector<int> AllGraphsInScene()
{
	vector<SInt32> monoBehaviourIDs;
	vector<int> ids;

	Object::FindAllDerivedObjects(ClassID (MonoBehaviour), &monoBehaviourIDs);
			
	for (vector<SInt32>::iterator i = monoBehaviourIDs.begin(); i != monoBehaviourIDs.end(); i++)
		if (IsGraphComponent(dynamic_instanceID_cast<MonoBehaviour*>(*i)))
			ids.push_back(*i);

	return ids; 
}

string GenerateGraphName()
{
	UnityGUID guid;
	guid.Init();
	return "Graph_" + GUIDToString(guid);
}

