#include "PrefabMerging.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "PrefabUtility.h"
#include "Runtime/Camera/Light.h"
#include "PrefabReplacing.h"

using namespace std;
/// @TODO: Write test where the prefab is referenced from the instance
/// @TODO: Write test for when the reference to script of a MonoBehaviour has been modified in the PropertyModification
/// @TODO: Write test to check if classID protection against assigning wrong parent prefab works correctly
/// @TODO: Write functional test that loads prefab data from old version of unity that had different C++ and C# script code layout and make sure merging works

void GetObjectDataArrayFromGameObjectHierarchyWithoutPrefabParent (GameObject& root, std::vector<ObjectData>& instanceObjects)
{
	instanceObjects.clear();
	
	vector<Object*> prefabObjects;
	CollectPPtrs(root, prefabObjects);
	
	instanceObjects.resize(prefabObjects.size());
	for (int i=0;i<instanceObjects.size();i++)
		instanceObjects[i] = ObjectData (prefabObjects[i], NULL);
}



void TestPrefabModification ()
{
	PrefabModification parentModification;

	// Create game object
	GameObject& prefab = *CreateObjectFromCode<GameObject>();
	AddComponent(prefab, ClassID(Transform), NULL, NULL);
	AddComponent(prefab, ClassID(Light), NULL, NULL);
	Assert(!prefab.IsActive());

	// Grab prefab objects
	vector<Object*> prefabObjects;
	CollectPPtrs(prefab, prefabObjects);
	
	vector<ObjectData> output;
	vector<ObjectData> input;
	vector<Object*> additionalObjects;
	vector<Object*> additionalObjectsOutput;
	PrefabModification modification;
	
	// Merge template changes with no modification
	MergePrefabChanges (prefabObjects, input, output, additionalObjects, additionalObjectsOutput, modification);

	GameObject* cloneGO = dynamic_pptr_cast<GameObject*> (FindInstanceFromPrefabObject (prefab, output));
	
	// Record the property change
	cloneGO->GetComponent(Light).SetRange(66);
	Assert(GeneratePrefabPropertyDiff(prefab.GetComponent(Light), cloneGO->GetComponent(Light), NULL, modification.m_Modifications) == kAddedOrUpdatedProperties);
	Assert(modification.m_Modifications.size() == 1);
	Assert(modification.m_Modifications[0].value == "66");

	cloneGO->GetComponent(Light).SetRange(67);
	// Make sure that property changes overwrite and don't accumulate
	Assert(GeneratePrefabPropertyDiff(prefab.GetComponent(Light), cloneGO->GetComponent(Light), NULL, modification.m_Modifications) == kAddedOrUpdatedProperties);
	Assert(modification.m_Modifications.size() == 1);
	Assert(modification.m_Modifications[0].value == "67");
	
	Assert(GeneratePrefabPropertyDiff(prefab.GetComponent(Light), cloneGO->GetComponent(Light), NULL, modification.m_Modifications) == kNoChangesDiffResult);
	Assert(modification.m_Modifications.size() == 1);
	Assert(modification.m_Modifications[0].value == "67");
	
	
	// Kill light & merge again, make sure that the light gets recreated
	// and has the overridden light range setup correctly
	Light* light = cloneGO->QueryComponent(Light);
	
	light->AwakeFromLoad(kDefaultAwakeFromLoad);
	
	DestroyObjectHighLevel(light);
	
	output.clear();
	input.push_back(ObjectData(cloneGO, &prefab));
	input.push_back(ObjectData(cloneGO->QueryComponent(Transform), prefab.QueryComponent(Transform)));
	
	MergePrefabChanges (prefabObjects, input, output, additionalObjects, additionalObjectsOutput, modification);

	Assert(cloneGO->QueryComponent(Light) != NULL);
	Assert(cloneGO->GetComponent(Light).GetRange() == 67);
	Assert(dynamic_pptr_cast<GameObject*> (FindInstanceFromPrefabObject (prefab, output)) == cloneGO);
	
	DirtyAndAwakeFromLoadAfterMerge(output, true);
	
	DestroyObjectHighLevel(cloneGO);
	DestroyObjectHighLevel(&prefab);
}