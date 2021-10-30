#include "UnityPrefix.h"
#include "AwakeFromLoadQueue.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Misc/BuildSettings.h"
#if UNITY_EDITOR
#include "Editor/Src/Prefabs/PrefabBackwardsCompatibility.h"
#endif

PROFILER_INFORMATION(gAwakeFromLoadQueue, "Loading.AwakeFromLoad", kProfilerLoading)

AwakeFromLoadQueue::AwakeFromLoadQueue (MemLabelRef label)
{
	for(int i=0;i<kMaxQueues;++i)
		m_ItemArrays[i].set_memory_label(label);
}

void AwakeFromLoadQueue::Reserve (unsigned size)
{
	for (int i=0;i<kMaxQueues;i++)
	{
		if (i == kManagersQueue)
			continue;
		
		m_ItemArrays[i].reserve(size);
	}
}

void AwakeFromLoadQueue::RegisterObjectInstanceIDs ()
{
	LockObjectCreation();
	for (int i=0;i<kMaxQueues;i++)
		RegisterObjectInstanceIDsInternal(m_ItemArrays[i].begin(), m_ItemArrays[i].size());	
	UnlockObjectCreation();
}

void AwakeFromLoadQueue::RegisterObjectInstanceIDsInternal (Item* objects, unsigned size)
{
	for (int i=0;i<size;i++)
	{
		Object* ptr = objects[i].registerObjectPtr;
		Assert(ptr != NULL);
		Object::RegisterInstanceIDNoLock(ptr);
	}
}

int AwakeFromLoadQueue::DetermineQueueIndex(ClassIDType classID)
{
	if (classID == ClassID(MonoBehaviour))
		return kMonoBehaviourQueue;
	else if (classID == ClassID(TerrainData))
		return kTerrainsQueue;
	else if (classID == ClassID(Animator))
		return kAnimatorQueue;
	else if (classID == ClassID(Rigidbody)
#if ENABLE_2D_PHYSICS
		|| classID == ClassID(Rigidbody2D)
#endif
		)
		return kRigidbodyQueue;
	else if (classID == ClassID(GameObject) || Object::IsDerivedFromClassID(classID, ClassID(Component)))
		return kGameObjectAndComponentQueue;
	else if (Object::IsDerivedFromClassID(classID, ClassID(GameManager)))
		return kManagersQueue;
	else
		return kAssetQueue;

}

bool AwakeFromLoadQueue::IsInQueue(Object& target)
{
	int queueIndex = DetermineQueueIndex(target.GetClassID());

	for (int i = 0; i < m_ItemArrays[queueIndex].size(); i++)
	{
		if (m_ItemArrays[queueIndex][i].objectPPtr == PPtr<Object>(target.GetInstanceID()))
			return true;
	}

	return false;
}


void AwakeFromLoadQueue::Add (Object& target, TypeTree* oldType, bool safeBinaryLoaded, AwakeFromLoadMode awakeOverride)
{
	Item item;
	item.registerObjectPtr = &target;
	item.objectPPtr = &target;
	item.classID = target.GetClassID();
	#if UNITY_EDITOR
	item.oldType = oldType;
	item.safeBinaryLoaded = safeBinaryLoaded;
	item.awakeModeOverride = awakeOverride;
	#else
	Assert(awakeOverride == -1);
	#endif
	
	int queueIndex = DetermineQueueIndex(item.classID);

	m_ItemArrays[queueIndex].push_back(item);
}

bool SortItemByInstanceID (const AwakeFromLoadQueue::Item& lhs, const AwakeFromLoadQueue::Item& rhs)
{
	return lhs.objectPPtr.GetInstanceID() < rhs.objectPPtr.GetInstanceID();
}

static int GetScriptExecutionOrder (int instanceID)
{
	#if ENABLE_SCRIPTING
	MonoBehaviour* behaviour = dynamic_instanceID_cast<MonoBehaviour*> (instanceID);
	if (behaviour != NULL)
	{
		MonoScript* script = behaviour->GetScript();
		if (script)
			return script->GetExecutionOrder();
	}
	#endif
	return 0;
}

bool AwakeFromLoadQueue::SortBehaviourByExecutionOrderAndInstanceID (int lhs, int rhs)
{
	int lhsExecutionOrder = GetScriptExecutionOrder(lhs);
	int rhsExecutionOrder = GetScriptExecutionOrder(rhs);
	
	if (lhsExecutionOrder != rhsExecutionOrder)
		return lhsExecutionOrder < rhsExecutionOrder;
	else
		return lhs < rhs;
}

bool SortBehaviourByExecutionOrderAndReverseInstanceID (int lhs, int rhs)
{
	int lhsExecutionOrder = GetScriptExecutionOrder(lhs);
	int rhsExecutionOrder = GetScriptExecutionOrder(rhs);
	
	if (lhsExecutionOrder != rhsExecutionOrder)
		return lhsExecutionOrder < rhsExecutionOrder;
	else
		return lhs > rhs;
}

static bool SortBehaviourItemByExecutionOrderAndInstanceID (const AwakeFromLoadQueue::Item& lhs, const AwakeFromLoadQueue::Item& rhs)
{
	return AwakeFromLoadQueue::SortBehaviourByExecutionOrderAndInstanceID(lhs.objectPPtr.GetInstanceID(), rhs.objectPPtr.GetInstanceID());
}

static bool SortBehaviourItemByExecutionOrderAndReverseInstanceID (const AwakeFromLoadQueue::Item& lhs, const AwakeFromLoadQueue::Item& rhs)
{
	return SortBehaviourByExecutionOrderAndReverseInstanceID(lhs.objectPPtr.GetInstanceID(), rhs.objectPPtr.GetInstanceID());
}

///@TODO: Should check consistency always be called immediately before calling Awake???

void AwakeFromLoadQueue::PersistentManagerAwakeFromLoad (AwakeFromLoadMode mode, SafeBinaryReadCallbackFunction* safeBinaryCallback)
{
	for (int i=0;i<kMaxQueues;i++)
		PersistentManagerAwakeFromLoad(i, mode, safeBinaryCallback);
}

void AwakeFromLoadQueue::ClearQueue (int queueIndex)
{
	m_ItemArrays[queueIndex].clear();
}


void AwakeFromLoadQueue::Clear ()
{
	for (int i=0;i<kMaxQueues;i++)
		ClearQueue (i);
}


void AwakeFromLoadQueue::PersistentManagerAwakeFromLoad (int queueIndex, AwakeFromLoadMode mode, SafeBinaryReadCallbackFunction* safeBinaryCallback)
{
	Item* array = m_ItemArrays[queueIndex].begin();
	size_t size = m_ItemArrays[queueIndex].size();
	
	std::sort(array, array + size, SortItemByInstanceID);
	
	if (queueIndex == kMonoBehaviourQueue)
	{
		if (UNITY_EDITOR)
		{
			// In the editor the execution order can be changed by the user at any time,
			// thus we need to sort on load
			std::sort(array, array + size, SortBehaviourItemByExecutionOrderAndInstanceID);
		}
		else
		{
			// In the player we write the scene files sorted by execution order
			for (int j=1;j<size;j++)
			{
				AssertIf(SortBehaviourItemByExecutionOrderAndInstanceID(array[j], array[j-1]));
			}
		}
	}
	
	InvokePersistentManagerAwake(array, size, mode, safeBinaryCallback);
}

void AwakeFromLoadQueue::AwakeFromLoad (AwakeFromLoadMode mode)
{
	for (int i=0;i<kMaxQueues;i++)
	{
		// In 4.0 we started sorting prefab instantiation by script execution order and instanceID
		if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		{
			if (i == kMonoBehaviourQueue)
			{
				// For Instantiate we need sort by execution order
				// The default order in Instantiate is determined by walking the hierarchy and components.
				// Since instanceIDs are generated negative and decreasing we sort by reverse instanceID for components that do not.
				std::sort(m_ItemArrays[i].begin(), m_ItemArrays[i].end(), SortBehaviourItemByExecutionOrderAndReverseInstanceID);
			}
		}
		
		InvokeAwakeFromLoad(m_ItemArrays[i].begin(), m_ItemArrays[i].size(), mode);
	}
}

void AwakeFromLoadQueue::CheckConsistency ()
{
	for (int i=0;i<kMaxQueues;i++)
		InvokeCheckConsistency(m_ItemArrays[i].begin(), m_ItemArrays[i].size());
}

#if UNITY_EDITOR

void AwakeFromLoadQueue::InsertAwakeFromLoadQueue (ItemArray& src, ItemArray& dst, AwakeFromLoadMode awakeOverride)
{
	std::sort(src.begin(), src.end(), SortItemByInstanceID);
	std::sort(dst.begin(), dst.end(), SortItemByInstanceID);
	
	// Inject any non-duplicate elements into the dst array
	int oldDstSize = dst.size();
	int d = 0;
	for (int i=0;i<src.size();i++)
	{
		while (d < oldDstSize && dst[d].objectPPtr.GetInstanceID() < src[i].objectPPtr.GetInstanceID())
			d++;
		
		if (d >= oldDstSize || dst[d].objectPPtr.GetInstanceID() != src[i].objectPPtr.GetInstanceID())
		{	
			dst.push_back(src[i]);
		}
		// else -> The object is in the destination array already.
	}
}

void AwakeFromLoadQueue::InsertAwakeFromLoadQueue (AwakeFromLoadQueue& awakeFromLoadQueue, AwakeFromLoadMode awakeOverride)
{
	for (int i=0;i<kMaxQueues;i++)
		InsertAwakeFromLoadQueue(awakeFromLoadQueue.m_ItemArrays[i], m_ItemArrays[i], awakeOverride);
}

void AwakeFromLoadQueue::PatchPrefabBackwardsCompatibility ()
{
	for (int q=0;q<kMaxQueues;q++)
	{	
		int size = m_ItemArrays[q].size();
		Item* objects = m_ItemArrays[q].begin();
		
		for (int i=0;i<size;i++)
		{
			Object* ptr = objects[i].objectPPtr;
			if (ptr)
			{
				TypeTree* typeTree = objects[i].oldType;
				if (typeTree != NULL && objects[i].safeBinaryLoaded)
				{
					objects[i].safeBinaryLoaded = false;
					RemapOldPrefabOverrideFromLoading (*ptr, *typeTree);
				}

				Prefab* prefab = dynamic_pptr_cast<Prefab*> (ptr);
				if (prefab)
					prefab->PatchPrefabBackwardsCompatibility();

				EditorExtension* extension = dynamic_pptr_cast<EditorExtension*> (ptr);
				if (extension)
					extension->PatchPrefabBackwardsCompatibility();
			}
		}
	}
}
#endif


void AwakeFromLoadQueue::InvokePersistentManagerAwake (Item* objects, unsigned size, AwakeFromLoadMode awakeMode, SafeBinaryReadCallbackFunction* safeBinaryCallback)
{
	#if DEBUGMODE
	int previousInstanceID = 0;
	#endif
	
	for (int i=0;i<size;i++)
	{
		PROFILER_AUTO(gAwakeFromLoadQueue, NULL)

		// The AwakeFromLoadQueue should never have any duplicate elements.
		#if DEBUGMODE
		Assert(objects[i].objectPPtr.GetInstanceID() != previousInstanceID);
		previousInstanceID = objects[i].objectPPtr.GetInstanceID();
		#endif
		
		Object* ptr = objects[i].objectPPtr;
		if (ptr == NULL)
			continue;
		
		#if UNITY_EDITOR
		// Loaded with SafeBinaryRead thus needs the callback
		if (objects[i].safeBinaryLoaded && safeBinaryCallback != NULL)
			safeBinaryCallback (*ptr, *objects[i].oldType);
		ptr->CheckConsistency ();

		#endif

		AwakeFromLoadMode objectAwakeMode = awakeMode;
		#if UNITY_EDITOR
		if (objects[i].awakeModeOverride != kDefaultAwakeFromLoadInvalid)
			objectAwakeMode = objects[i].awakeModeOverride;
		#endif

		ptr->AwakeFromLoad (objectAwakeMode);
		ptr->ClearPersistentDirty ();
	}
}

void AwakeFromLoadQueue::PersistentManagerAwakeSingleObject (Object& o, TypeTree* oldType, AwakeFromLoadMode awakeMode, bool safeBinaryLoaded, SafeBinaryReadCallbackFunction* safeBinaryCallback)
{
	PROFILER_AUTO(gAwakeFromLoadQueue, &o)
	
	#if UNITY_EDITOR
	// Loaded with SafeBinaryRead thus needs the callback
	if (safeBinaryLoaded && safeBinaryCallback != NULL)
		safeBinaryCallback (o, *oldType);
	o.CheckConsistency ();
	#endif
	
	o.AwakeFromLoad (awakeMode);
	o.ClearPersistentDirty ();
}

void AwakeFromLoadQueue::InvokeAwakeFromLoad (Item* objects, unsigned size, AwakeFromLoadMode mode)
{
	for (int i=0;i<size;i++)
	{
		Object* ptr = objects[i].objectPPtr;
		if (ptr)
		{
			#if UNITY_EDITOR
			if (objects[i].awakeModeOverride != kDefaultAwakeFromLoadInvalid)
			{
				ptr->AwakeFromLoad(objects[i].awakeModeOverride);
				continue;
			}	
				
			#endif
				
				
			ptr->AwakeFromLoad(mode);
		}
	}
}

void AwakeFromLoadQueue::InvokeCheckConsistency (Item* objects, unsigned size)
{
	for (int i=0;i<size;i++)
	{
		Object* ptr = objects[i].objectPPtr;
		if (ptr)
			ptr->CheckConsistency();
	}
}

void AwakeFromLoadQueue::ExtractAllObjects (dynamic_array<PPtr<Object> >& outObjects)
{
	Assert(outObjects.empty());
	
	int count = 0;
	for (int q=0;q<kMaxQueues;q++)
		count += m_ItemArrays[q].size();
	outObjects.reserve(count);
	
	for (int q=0;q<kMaxQueues;q++)
	{	
		int size = m_ItemArrays[q].size();
		Item* objects = m_ItemArrays[q].begin();
		
		for (int i=0;i<size;i++)
			outObjects.push_back(objects[i].objectPPtr);
	}
}
