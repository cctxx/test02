#if ENABLE_PROFILER

#include "UnityPrefix.h"
#include "MemoryProfilerStats.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Serialize/PersistentManager.h"

void profiler_register_object(Object* obj)
{
	GetMemoryProfilerStats().RegisterObject(obj);
}

void profiler_unregister_object(Object* obj)
{
	GetMemoryProfilerStats().UnregisterObject(obj);
}

void profiler_change_persistancy(int instanceID, bool oldvalue, bool newvalue)
{
	GetMemoryProfilerStats().ChangePersistancyflag(instanceID, oldvalue, newvalue);
}


void TestAndInsertObject(Object* obj, int objClassID, int classID, dynamic_array<Object*>& objs)
{
	if (objClassID == classID)
		objs.push_back(obj);
}

void TestAndRemoveObject(Object* obj, int objClassID, int classID, dynamic_array<Object*>& objs)
{
	if (objClassID == classID)
	{
		// run from the end - last created objects are most likely to be destoyed
		dynamic_array<Object*>::iterator it = objs.end();
		while(it != objs.begin())
		{
			--it;
			if(*it == obj){
				objs.erase(it, it+1);
				return;
			}
		}
		ErrorString(Format("An object that was removed, was not found in the object list for that type (%s)", Object::ClassIDToString(classID).c_str()));
	}
}

void MemoryProfilerStats::ChangePersistancyflag(int instanceID, bool oldvalue, bool newvalue)
{
	if(oldvalue == newvalue)
		return;
#if SUPPORT_THREADS
	if(!Thread::EqualsCurrentThreadID(GetPersistentManager().GetMainThreadID()))
		return;
#endif
	Object* obj = Object::IDToPointer(instanceID);
	if(obj == NULL)
		return;
	
	if(oldvalue == true)
	{
		AtomicDecrement(&assetCount);
		AddDynamicObjectCount(obj, obj->GetClassID());
	}
	else
	{
		AtomicIncrement(&assetCount);
		RemoveDynamicObjectCount(obj, obj->GetClassID());
	}
}

void MemoryProfilerStats::AddDynamicObjectCount(Object* obj, int classID)
{
	AtomicIncrement(&sceneObjectCount);
	if( classID == ClassID(GameObject) )
		AtomicIncrement(&gameObjectCount);
}

void MemoryProfilerStats::RemoveDynamicObjectCount(Object* obj, int classID)
{
	AtomicDecrement(&sceneObjectCount);
	if( classID == ClassID(GameObject) )
		AtomicDecrement(&gameObjectCount);
}

void MemoryProfilerStats::RegisterObject ( Object* obj )
{
	int classID = obj->GetClassID();
	
	TestAndInsertObject(obj, classID, ClassID(Texture2D), textures);
	TestAndInsertObject(obj, classID, ClassID(Mesh), meshes);
	TestAndInsertObject(obj, classID, ClassID(Material), materials);
	TestAndInsertObject(obj, classID, ClassID(AnimationClip), animations);
	TestAndInsertObject(obj, classID, ClassID(AudioClip), audioclips);
	
	if(classCount.size() <= classID)
		classCount.resize_initialized(classID+1,0);
	++classCount[classID];
	
	if(obj->IsPersistent())
		AtomicIncrement(&assetCount);
	else
		AddDynamicObjectCount(obj, classID);
}

void MemoryProfilerStats::UnregisterObject ( Object* obj )
{
	int classID = obj->GetClassID();
	TestAndRemoveObject(obj, classID, ClassID(Texture2D), textures);
	TestAndRemoveObject(obj, classID, ClassID(Mesh), meshes);
	TestAndRemoveObject(obj, classID, ClassID(Material), materials);
	TestAndRemoveObject(obj, classID, ClassID(AnimationClip), animations);
	TestAndRemoveObject(obj, classID, ClassID(AudioClip), audioclips);
	
	Assert (classCount.size() > classID);
	--classCount[classID];
	
	if(obj->IsPersistent())
		AtomicDecrement(&assetCount);
	else
		RemoveDynamicObjectCount(obj, classID);
}

MemoryProfilerStats::MemoryProfilerStats()
: assetCount(0)
, sceneObjectCount(0)
, gameObjectCount(0)
{
}

MemoryProfilerStats::~MemoryProfilerStats()
{
}


MemoryProfilerStats* gMemoryProfilerStats = NULL;
MemoryProfilerStats& GetMemoryProfilerStats()
{
	Assert(gMemoryProfilerStats != NULL);
	return *gMemoryProfilerStats;
}

void InitializeMemoryProfilerStats()
{
	Assert(gMemoryProfilerStats == NULL);
	gMemoryProfilerStats = new MemoryProfilerStats();
}

void CleanupMemoryProfilerStats()
{
	Assert(gMemoryProfilerStats != NULL);
	delete gMemoryProfilerStats;
	gMemoryProfilerStats = NULL;
}

#endif