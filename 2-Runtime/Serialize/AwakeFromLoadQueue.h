#ifndef AWAKE_FROM_LOAD_QUEUE_H
#define AWAKE_FROM_LOAD_QUEUE_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/dynamic_array.h"
class TypeTree;


// Rigidbodies must run before Colliders
// Animators must come after all other components because they bind properties (eg. SkinnedMeshRenderer needs to be fully initialized to bind blendshapes)
// Terrains depend on prefabs for trees and textures to be fully prepared.
// MonoBehaviours always come last since they can call anything on any component or asset.

enum { kManagersQueue = 0, kAssetQueue, kRigidbodyQueue, kGameObjectAndComponentQueue, kAnimatorQueue, kTerrainsQueue, kMonoBehaviourQueue, kMaxQueues };

class AwakeFromLoadQueue
{
public:
	
	struct Item
	{
		Object*           registerObjectPtr;
		
		PPtr<Object>      objectPPtr;
		ClassIDType       classID;
		
#if UNITY_EDITOR
		TypeTree*         oldType;
		bool              safeBinaryLoaded;
		AwakeFromLoadMode awakeModeOverride; 
#endif
	};
	
	AwakeFromLoadQueue (MemLabelRef label);
	
	typedef dynamic_array<Item> ItemArray;
	
	typedef void SafeBinaryReadCallbackFunction (Object& object, const TypeTree& oldTypeTree);
	
	bool IsInQueue(Object& target);

	void Reserve (unsigned size);
	void Add (Object& target, TypeTree* oldType = NULL, bool safeBinaryLoaded = false, AwakeFromLoadMode awakeOverride = kDefaultAwakeFromLoadInvalid);
	
	static void PersistentManagerAwakeSingleObject (Object& o, TypeTree* oldType, AwakeFromLoadMode awakeMode, bool safeBinaryLoaded, SafeBinaryReadCallbackFunction* safeBinaryCallback);
	
	void PersistentManagerAwakeFromLoad (AwakeFromLoadMode mode, SafeBinaryReadCallbackFunction* safeBinaryCallback);
	void PersistentManagerAwakeFromLoad (int queueIndex, AwakeFromLoadMode mode, SafeBinaryReadCallbackFunction* safeBinaryCallback);

	void ClearQueue (int queueIndex);
	void Clear ();

	void AwakeFromLoad (AwakeFromLoadMode mode);
	void CheckConsistency ();
	
	void RegisterObjectInstanceIDs ();
	
	#if UNITY_EDITOR
	void PatchPrefabBackwardsCompatibility ();
	void InsertAwakeFromLoadQueue (AwakeFromLoadQueue& awakeFromLoadQueue, AwakeFromLoadMode awakeOverride);
	#endif
	
	void ExtractAllObjects (dynamic_array<PPtr<Object> >& outObjects);
	
	ItemArray& GetItemArray (int queueIndex) { return m_ItemArrays[queueIndex]; }

	static bool SortBehaviourByExecutionOrderAndInstanceID (int lhs, int rhs);

private:
	int DetermineQueueIndex(ClassIDType classID);

	static void InvokeAwakeFromLoad (Item* objects, unsigned size, AwakeFromLoadMode mode);
	static void InvokeCheckConsistency (Item* objects, unsigned size);
	static void InvokePersistentManagerAwake (Item* objects, unsigned size, AwakeFromLoadMode awakeMode, SafeBinaryReadCallbackFunction* safeBinaryCallback);
	static void RegisterObjectInstanceIDsInternal (Item* objects, unsigned size);
	
	void InsertAwakeFromLoadQueue (dynamic_array<Item>& src, dynamic_array<Item>& dst, AwakeFromLoadMode awakeOverride);

	
	ItemArray m_ItemArrays[kMaxQueues];
};

#endif