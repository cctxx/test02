#include "UnityPrefix.h"
#include "AttachedPrefabAsset.h"
#include "Prefab.h"
#include "Runtime/GameCode/CloneObject.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Renderer.h"

using namespace std;

void GetInstantiatedAssetsToObjectArray (const std::vector<ObjectData>& instanceObjects, std::vector<ObjectData>& instancedAssets)
{
	set<Object*> instancedAssetsSet;
	
	for (int i=0;i<instanceObjects.size();i++)
	{
		Renderer* renderer = dynamic_pptr_cast<Renderer*> (instanceObjects[i].objectInstance);
		if (renderer != NULL)
		{
			for (int i=0;i<renderer->GetMaterialCount();i++)
			{
				Material* material = renderer->GetMaterial(i);
				
				instancedAssetsSet.insert(material);
			}
		}
	}
	
	instancedAssets.reserve(instancedAssetsSet.size());
	///@TODO:
	// * What to do if two instantiated assets have modifications on the same parent prefab asset. Which one will get applied?
	//        SHould the other one get a material instance in the prefab? Outside of the prefab in the materials folder???
	
	for (set<Object*>::iterator i=instancedAssetsSet.begin();i != instancedAssetsSet.end();i++)
	{
		Object& target = **i;
		// Material is an asset already, no need to upload it
//		if (target.IsPersistent())
//			continue;
		
		// We can only replace the parent if you used the Instantiate function
		Object* parentObject = GetPrefabParentObject(&target);
		if (parentObject == NULL)
			continue;
		
		instancedAssets.push_back(ObjectData(&target, parentObject));
	}
}

void RevertInstantiatedAssetReferencesToParentObject (vector<ObjectData>& instanceObjects)
{
	////@TODO: Should verify that the material is in instanceObjects???
	
	for (int i=0;i<instanceObjects.size();i++)
	{
		Renderer* renderer = dynamic_pptr_cast<Renderer*> (instanceObjects[i].objectInstance);
		if (renderer != NULL)
		{
			for (int i=0;i<renderer->GetMaterialCount();i++)
			{
				Material* material = renderer->GetMaterial(i);
				
				Material* prefabParent = dynamic_pptr_cast<Material*> (GetPrefabParentObject(material));
				if (prefabParent)
					renderer->SetMaterial(prefabParent, i);
			}
		}
	}
}

Object* InstantiateAttachedAsset (Object& asset)
{
	Object& instance = CloneObject(asset);
	
	EditorExtension* connectInstanceObject = dynamic_pptr_cast<EditorExtension*> (&instance);
	if (connectInstanceObject)
		connectInstanceObject->m_PrefabParentObject = dynamic_pptr_cast<EditorExtension*> (&asset);
	
	return &instance;
}