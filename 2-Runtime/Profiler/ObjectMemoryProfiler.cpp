#include "UnityPrefix.h"
#include "ObjectMemoryProfiler.h"
#include "MemoryProfiler.h"
#include "SerializationUtility.h"
#include "Runtime/Misc/GarbageCollectSharedAssets.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "ExtractLoadedObjectInfo.h"

#if ENABLE_MEM_PROFILER

#if UNITY_EDITOR
#include "Editor/Src/Prefabs/Prefab.h"
#endif

namespace ObjectMemoryProfiler
{

#if ENABLE_MEM_PROFILER && ENABLE_PLAYERCONNECTION
#define USE_MONO_LIVENESS (ENABLE_MONO && !UNITY_NACL)
#endif


typedef std::vector<Object*> ObjectList;
static const UInt32 OBJECT_MEMORY_STREAM_VERSION = 0x00000002;
static const UInt32 OBJECT_MEMORY_STREAM_TAIL = 0xAFAFAFAF;


#if UNITY_EDITOR
struct MonoObjectMemoryInfo
{
	int                instanceId;
	UInt32             memorySize;
	int                count;
	int                reason;
	ScriptingStringPtr name;
	ScriptingStringPtr className;
};
#endif

static void Serialize(dynamic_array<int>& stream, const char* customAreaName, const char* objectName, size_t memorySize)
{
	stream.push_back(0);
	stream.push_back(memorySize);
	stream.push_back(0);
	stream.push_back(kNotApplicable);
	WriteString(stream, objectName);
	WriteString(stream, customAreaName);
}

static void Serialize(dynamic_array<int>& stream, const char* objectName, int count )
{
	stream.push_back(0);
	stream.push_back(0);
	stream.push_back(count);
	stream.push_back(kNotApplicable);
	WriteString(stream, objectName);
	WriteString(stream, "");
}

static void Serialize(dynamic_array<int>& stream, Object* object, int count )
{
	const char* objectName = object->GetName();
	const std::string& className = object->GetClassName();

	stream.push_back(object->GetInstanceID());
	stream.push_back(object->GetRuntimeMemorySize());
	stream.push_back(count);
	stream.push_back(GetLoadedObjectReason(object));
	if(object->GetClassID() == ClassID (MonoBehaviour))
		WriteString(stream, ((MonoBehaviour*)(object))->GetScriptFullClassName().c_str());
#if UNITY_EDITOR
	else if(object->GetClassID() == ClassID (Prefab))
		WriteString(stream, ((Prefab*)(object))->GetRootGameObject()->GetName());
#endif
	else
		WriteString(stream, objectName);
	WriteString(stream, className.c_str());
}

static void SerializeHeader(dynamic_array<int>& stream)
{
	stream.push_back(UNITY_LITTLE_ENDIAN);
	int version = OBJECT_MEMORY_STREAM_VERSION;
#if ENABLE_STACKS_ON_ALL_ALLOCS
	version += 0x10000000;
#endif
	stream.push_back(version);
}

void TakeMemorySnapshot(dynamic_array<int>& stream)
{
	dynamic_array<Object*> loadedObjects;
	dynamic_array<const char*> additionalCategories;
	dynamic_array<UInt32> indexCounts;
	dynamic_array<UInt32> referencedObjectIndices;

	CalculateAllObjectReferences (loadedObjects, additionalCategories, indexCounts, referencedObjectIndices);

	// MemoryProfiler Roots
	MemoryProfiler::RootAllocationInfos rootInfos (kMemProfiler);
	GetMemoryProfiler ()->GetRootAllocationInfos(rootInfos);
	
	// loaded objects contain all loaded game objects
	// this is followed by additional strings that has references as well
	// indexCounts contain the count for each of the previous object references
	// the indices into the object array for the references

	SerializeHeader(stream);

	// serialize the referenceIndices
	stream.push_back(referencedObjectIndices.size());
	WriteIntArray(stream, (int*)&referencedObjectIndices[0],referencedObjectIndices.size());
	
	// serialize loaded objects followed by additionalCats
	int totalObjects = loadedObjects.size() + additionalCategories.size() + rootInfos.size() + 1
	#if ENABLE_MONO
		+ 2
	#endif
		;
	stream.push_back(totalObjects);
	
	for(int i = 0 ; i < loadedObjects.size() ; i++)
	{
		Serialize(stream, loadedObjects[i], indexCounts[i]);
	}

	for(int i=0;i<additionalCategories.size();i++)
	{
		Serialize(stream, additionalCategories[i], indexCounts[i+loadedObjects.size()]);
	}

	for(int i=0;i<rootInfos.size();i++)
		Serialize(stream, rootInfos[i].areaName, rootInfos[i].objectName, rootInfos[i].memorySize);

	Serialize(stream, "System.ExecutableAndDlls", "", systeminfo::GetExecutableSizeMB()*1024*1024);

	#if ENABLE_MONO
	Serialize(stream, "ManagedHeap.UsedSize", "", mono_gc_get_used_size ());
	Serialize(stream, "ManagedHeap.ReservedUnusedSize", "", mono_gc_get_heap_size () - mono_gc_get_used_size ());
	
	///@TOOD: Other mono metadata
	#endif
	stream.push_back(OBJECT_MEMORY_STREAM_TAIL);
}


#if UNITY_EDITOR

static void Deserialize(const void* data, size_t size, MonoArray ** objectArray, MonoArray ** referenceArray)
{
	int* current_offset = (int*)data;
	int wordsize = size/sizeof(int);
	int* endBuffer = current_offset + wordsize;

	int dataIsLittleEndian = *current_offset++;
	bool swapdata = UNITY_LITTLE_ENDIAN ? dataIsLittleEndian == 0 : dataIsLittleEndian != 0;
	if(swapdata)
	{
		int* ptr = current_offset;
		while(ptr < endBuffer)
			SwapEndianBytes(*(ptr++));
	}

	// header

	int version = OBJECT_MEMORY_STREAM_VERSION;
#if ENABLE_STACKS_ON_ALL_ALLOCS
	version += 0x10000000;
#endif
	if(*current_offset!=version)
		return;
	current_offset++;

	// deserialize the referenceIndices
	int numberOfReferences = *current_offset++;
	MonoArray* reference_array = mono_array_new(mono_domain_get(), MONO_COMMON.int_32, numberOfReferences);
	ReadIntArray(&current_offset, &Scripting::GetScriptingArrayElement<int> (reference_array, 0), numberOfReferences);

	// objectCount
	int numberOfObjects = *current_offset++;
	MonoClass* klass = GetMonoManager().GetMonoClass ("ObjectMemoryInfo", "UnityEditorInternal");
	MonoArray* object_array = mono_array_new(mono_domain_get(), klass, numberOfObjects);

	for (int i = 0; i < numberOfObjects;i++)
	{
		MonoObject* obj = mono_object_new (mono_domain_get(), klass);
		GetMonoArrayElement<MonoObject*> (object_array,i) = obj;
	}

	for (int i = 0; i < numberOfObjects;i++)
	{
		MonoObjectMemoryInfo& memInfo = ExtractMonoObjectData<MonoObjectMemoryInfo> (GetMonoArrayElement<MonoObject*> (object_array,i));
		memInfo.instanceId = *current_offset++;
		memInfo.memorySize = *current_offset++;
		memInfo.count = *current_offset++;
		memInfo.reason = *current_offset++;
		std::string name;
		std::string className;
		ReadString(&current_offset, name, swapdata);
		ReadString(&current_offset, className, swapdata);
		memInfo.name = MonoStringNew(name);
		memInfo.className = MonoStringNew(className);
	}

	Assert(*current_offset==OBJECT_MEMORY_STREAM_TAIL);

	*objectArray = object_array;
	*referenceArray = reference_array;
}

void DeserializeAndApply (const void* data, size_t size)
{
	MonoArray* objectArray;
	MonoArray* indexArray;
	Deserialize (data, size, &objectArray, &indexArray);

	ScriptingInvocation invocation ("UnityEditor", "ProfilerWindow", "SetMemoryProfilerInfo");
	invocation.AddArray(objectArray);
	invocation.AddArray(indexArray);
	invocation.Invoke();
}

void SetDataFromEditor ()
{
	dynamic_array<int> data;
	TakeMemorySnapshot(data);
	DeserializeAndApply(data.begin(), data.size()*sizeof(int));

#if RECORD_ALLOCATION_SITES
	MemoryProfiler::MemoryStackEntry* stack = GetMemoryProfiler()->GetStackOverview();
	MonoObject* obj = stack->Deserialize ();
	void* arguments[] = { obj };
	CallStaticMonoMethod("MemoryProfiler", "SetMemoryProfilerStackInfo", arguments);
	GetMemoryProfiler()->ClearStackOverview(stack);

	ProfilerString unrooted = GetMemoryProfiler()->GetUnrootedAllocationsOverview();
#endif
}

#endif	
}

#endif
