#ifndef _MEMORY_PROFILER_STATS_H_
#define _MEMORY_PROFILER_STATS_H_

#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/dynamic_array.h"


#if ENABLE_PROFILER


class Object;

class MemoryProfilerStats
{
public:
	MemoryProfilerStats();
	~MemoryProfilerStats();

	void RegisterObject ( Object* obj );
	void UnregisterObject ( Object* obj );
	void ChangePersistancyflag (int instanceID, bool oldvalue, bool newvalue);

	typedef dynamic_array<Object*> ObjectVector;
	const ObjectVector& GetTextures() const {return textures;}
	const ObjectVector& GetMeshes() const {return meshes;}
	const ObjectVector& GetMaterials() const {return materials;}
	const ObjectVector& GetAnimationClips() const {return animations;}
	const ObjectVector& GetAudioClips() const {return audioclips;}

	const dynamic_array<int>& GetClassCount() const {return classCount;}

	int GetAssetCount() const {return assetCount;}
	int GetSceneObjectCount() const {return sceneObjectCount;}
	int GetGameObjectCount() const {return gameObjectCount;}
private:

	ObjectVector textures;
	ObjectVector meshes;
	ObjectVector materials;
	ObjectVector animations;
	ObjectVector audioclips;

	volatile int assetCount;
	volatile int sceneObjectCount;
	volatile int gameObjectCount;

	dynamic_array<int> classCount; 

	void AddDynamicObjectCount(Object* obj, int classID);
	void RemoveDynamicObjectCount(Object* obj, int classID);

};

MemoryProfilerStats& GetMemoryProfilerStats();
void InitializeMemoryProfilerStats();
void CleanupMemoryProfilerStats();

void profiler_register_object(Object* obj);
void profiler_unregister_object(Object* obj);
void profiler_change_persistancy(int instanceID, bool oldvalue, bool newvalue);

#define PROFILER_REGISTER_OBJECT(obj) profiler_register_object(obj);
#define PROFILER_UNREGISTER_OBJECT(obj) profiler_unregister_object(obj);
#define PROFILER_CHANGE_PERSISTANCY(id,oldvalue,newvalue) profiler_change_persistancy(id,oldvalue,newvalue);

#else

#define PROFILER_REGISTER_OBJECT(obj) 
#define PROFILER_UNREGISTER_OBJECT(obj) 
#define PROFILER_CHANGE_PERSISTANCY(id,oldvalue,newvalue)

#endif


#endif