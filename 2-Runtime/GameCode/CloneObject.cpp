#include "UnityPrefix.h"
#include "CloneObject.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/StreamedBinaryWrite.h"
#include "Runtime/Serialize/TransferFunctions/StreamedBinaryRead.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Serialize/AwakeFromLoadQueue.h"
#include "Runtime/Filters/Misc/Font.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include <map>

using namespace std;
using namespace Unity;

Object& ProduceClone (Object& object)
{
	Object* clone = Object::Produce (object.GetClassID ());
	
#if ENABLE_SCRIPTING
	MonoBehaviour* cloneBehaviour = dynamic_pptr_cast<MonoBehaviour*> (clone);
	if (cloneBehaviour)
	{
		MonoBehaviour& cloneSrc = static_cast<MonoBehaviour&> (object);
		cloneBehaviour->SetScript(cloneSrc.GetScript());
	}
#endif
	return *clone;
}

void CollectAndProduceSingleObject (Object& singleObject, TempRemapTable* remappedPtrs)
{
	Object& clone = ProduceClone (singleObject);
	
	remappedPtrs->insert(make_pair(singleObject.GetInstanceID(), clone.GetInstanceID()));
}

Transform* CollectAndProduceGameObjectHierarchy (GameObject& go, Transform* transform, TempRemapTable* remappedPtrs)
{
	GameObject* cloneGO = static_cast<GameObject*> (Object::Produce (ClassID(GameObject)));
	remappedPtrs->insert(make_pair(go.GetInstanceID(), cloneGO->GetInstanceID()));

	GameObject::Container& goContainer = go.GetComponentContainerInternal();
	GameObject::Container& clonedContainer = cloneGO->GetComponentContainerInternal();

	clonedContainer.resize(goContainer.size());
	for (int i=0;i<goContainer.size();i++)
	{
		Unity::Component& component = *goContainer[i].second;
		Unity::Component& clone = static_cast<Unity::Component&> (ProduceClone(component));
		
		clonedContainer[i].first = goContainer[i].first;
		clonedContainer[i].second = &clone;
		clone.SetGameObjectInternal(cloneGO);
		
		remappedPtrs->insert(make_pair(component.GetInstanceID(), clone.GetInstanceID()));
	}
	
	if (transform)
	{
		Transform& cloneTransform = cloneGO->GetComponent(Transform);
		
		Transform::TransformComList& srcTransformArray = transform->GetChildrenInternal();
		Transform::TransformComList& dstTransformArray = cloneTransform.GetChildrenInternal();
		
		dstTransformArray.resize_uninitialized(srcTransformArray.size(), false);
		for (int i=0;i<srcTransformArray.size();i++)
		{
			Transform& curT = *srcTransformArray[i];
			GameObject& curGO = curT.GetGameObject();

			Transform* curCloneTransform = CollectAndProduceGameObjectHierarchy(curGO, &curT, remappedPtrs);
			curCloneTransform->GetParentPtrInternal() = &cloneTransform;
			dstTransformArray[i] = curCloneTransform;
		}
		return &cloneTransform;
	}
	else
	{
		return NULL;
	}
}

inline GameObject* GetGameObjectPtr (Object& o)
{
	GameObject* go = dynamic_pptr_cast<GameObject*>(&o);
	Unity::Component* component = dynamic_pptr_cast<Unity::Component*>(&o);
	if (component != NULL && component->GetGameObjectPtr())
		go = component->GetGameObjectPtr();
	
	return go;
}

void CollectAndProduceClonedIsland (Object& o, TempRemapTable* remappedPtrs)
{
	AssertIf(!remappedPtrs->empty());
	
	remappedPtrs->reserve(64);
	
	GameObject* go = GetGameObjectPtr(o);
	if (go)
	{
		///@TODO: It would be useful to lock object creation around a long instantiate call.
		// Butwe have to be careful that we dont load anything during the object creation in order to avoid 
		// a deadlock: case 389317
		
		// LockObjectCreation();
		
		CollectAndProduceGameObjectHierarchy(*go, go->QueryComponent(Transform), remappedPtrs);
		
		// UnlockObjectCreation();
	}
	else
		CollectAndProduceSingleObject(o, remappedPtrs);

	remappedPtrs->sort();
}

void AwakeAndActivateClonedObjects (const TempRemapTable& ptrs)
{
	AwakeFromLoadQueue queue (kMemTempAlloc);
	queue.Reserve(ptrs.size());

	for (TempRemapTable::const_iterator i=ptrs.begin ();i!=ptrs.end ();++i)
	{
		Object& clone = *PPtr<Object> (i->second);
		clone.SetHideFlags (0);
		clone.SetDirty ();
		
		#if !UNITY_RELEASE
		// we will clone that object - no need to call Reset as we will construct it fully
		clone.HackSetResetWasCalled();
		#endif
	
		queue.Add(*PPtr<Object> (i->second));
	}

	queue.AwakeFromLoad ((AwakeFromLoadMode)(kDefaultAwakeFromLoad | kInstantiateOrCreateFromCodeAwakeFromLoad));
}

class RemapFunctorTempRemapTable : public GenerateIDFunctor
{
public:
	const TempRemapTable& remap;
	
	RemapFunctorTempRemapTable (const TempRemapTable& inRemap) : remap (inRemap) { }
	
	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlags = kNoTransferFlags)
	{
		AssertIf (metaFlags & kStrongPPtrMask);

		TempRemapTable::const_iterator found = remap.find (oldInstanceID);
		// No Remap found -> set zero or dont touch instanceID
		if (found == remap.end ())
			return oldInstanceID;
		// Remap
		else
			return found->second;
	}
};

static Object* CloneObjectImpl (Object* object, TempRemapTable& ptrs)
{
	// Since we will be creating a lot of objects here
	// Just Lock the mutex all the time to avoid too many lock / unlock calls
	CollectAndProduceClonedIsland (*object, &ptrs);

	TempRemapTable::iterator it;

#if UNITY_FLASH
	//specialcase for flash, as that needs to be able to assume linear memorylayout.
	dynamic_array<UInt8> buffer(kMemTempAlloc);
	MemoryCacheWriter cacheWriter (buffer);
#else
	BlockMemoryCacheWriter cacheWriter (kMemTempAlloc);
#endif

	RemapFunctorTempRemapTable functor (ptrs);
	RemapPPtrTransfer remapTransfer (kSerializeForPrefabSystem, true);
	remapTransfer.SetGenerateIDFunctor (&functor);

	for (it=ptrs.begin ();it != ptrs.end ();it++)
	{
		Object& original = *PPtr<Object> (it->first);
		
		#if UNITY_EDITOR
		original.WarnInstantiateDisallowed();
		#endif
		
		// Copy Data
		Object& clone = *PPtr<Object> (it->second);

		StreamedBinaryWrite<false> writeStream;
		CachedWriter& writeCache = writeStream.Init (kSerializeForPrefabSystem, BuildTargetSelection::NoTarget());
		writeCache.InitWrite (cacheWriter);
		original.VirtualRedirectTransfer (writeStream);
		writeCache.CompleteWriting();
		
#if UNITY_FLASH
		MemoryCacheReader cacheReader (buffer);
#else
		MemoryCacherReadBlocks cacheReader (cacheWriter.GetCacheBlocks (), cacheWriter.GetFileLength (), cacheWriter.GetCacheSize());
#endif
		StreamedBinaryRead<false> readStream;
		CachedReader& readCache = readStream.Init (kSerializeForPrefabSystem);
		
		readCache.InitRead (cacheReader, 0, writeCache.GetPosition());
		clone.VirtualRedirectTransfer (readStream);
		readCache.End();
		
		if (!IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
		{
			GameObject* clonedGameObject = dynamic_pptr_cast<GameObject*> (&clone);
			if (clonedGameObject)
				clonedGameObject->SetActiveBitInternal(true);
		}
		
		#if UNITY_EDITOR
		clone.CloneAdditionalEditorProperties(original);
		#endif
		
		// Remap references
		clone.VirtualRedirectTransfer (remapTransfer);
	}
	

	TempRemapTable::iterator found = ptrs.find (object->GetInstanceID ());
	AssertIf (found == ptrs.end ());
	object = PPtr<Object> (found->second);

	return object;
}

PROFILER_INFORMATION(gInstantiateProfile, "Instantiate", kProfilerOther)

Object& CloneObject (Object& inObject)
{
	PROFILER_AUTO(gInstantiateProfile, &inObject)

#if !GAMERELEASE
	// For context info see case 499663
	Font* font = dynamic_pptr_cast<Font*> (&inObject);
	if (font && font->GetConvertCase() == Font::kDynamicFont)
		ErrorString("Font Error: Cloning a dynamic font is not supported and may result in incorrect font rendering.");
#endif
		
	TempRemapTable ptrs;
	Object* object = CloneObjectImpl(&inObject, ptrs);

	if (object)
		object->SetName(Append (object->GetName(), "(Clone)").c_str());

	AwakeAndActivateClonedObjects(ptrs);

	ANALYSIS_ASSUME(object);
	return *object;
}

Object& InstantiateObject (Object& inObject, const Vector3f& worldPos, const Quaternionf& worldRot, TempRemapTable& ptrs)
{
	PROFILER_AUTO(gInstantiateProfile, &inObject)
	Object* object = CloneObjectImpl (&inObject, ptrs);
	
	// Get the transformComponent of the first object in the input objects
	Transform *expTransform = NULL;
	if (object)
	{
		Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (object);
		GameObject* go = dynamic_pptr_cast<GameObject*> (object);
		if (com)
			expTransform = com->QueryComponent (Transform);
		else if (go)
			expTransform = go->QueryComponent (Transform);
			
		object->SetName(Append (object->GetName(), "(Clone)").c_str());
	}
		
	// Set position
	if (expTransform)
	{
		expTransform->SetPosition (worldPos);
		expTransform->SetRotationSafe (worldRot);
	}
	
	ANALYSIS_ASSUME(object);
	return *object;
}


Object& InstantiateObject (Object& inObject, const Vector3f& worldPos, const Quaternionf& worldRot)
{
	TempRemapTable ptrs;
	Object& obj = InstantiateObject (inObject, worldPos, worldRot, ptrs);
	
	AwakeAndActivateClonedObjects(ptrs);
	
	return obj;
}
