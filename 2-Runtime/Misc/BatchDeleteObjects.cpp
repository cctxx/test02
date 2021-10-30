#include "UnityPrefix.h"
#include "BatchDeleteObjects.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"

PROFILER_INFORMATION(gBatchDeleteObjects, "BatchDeleteObjects", kProfilerLoading)
PROFILER_INFORMATION(gBatchDeleteObjectsThread, "BatchDeleteObjectsThread", kProfilerLoading)

// @TODO: On Angrybots on OS X. We spent 4ms deleting objects in singlethreaded mode on the main thread and 1ms in multithreaded mode on the main thread.
#define MULTI_THREADED_DELETE 0

// Some classes do not support being deallocated on the main thread.
bool DoesClassRequireMainThreadDeallocation (int classID)
{
	if (classID == ClassID(Shader) || classID == ClassID(PhysicMaterial) || classID == ClassID(Mesh) ||
		classID == ClassID(Texture) || classID == ClassID(Texture2D) || classID == ClassID(Texture3D) || classID == ClassID(Cubemap) )
		return true;
	else
		return false;
}

#if MULTI_THREADED_DELETE

#define kTerminateInstruction reinterpret_cast<Object*> (0x1)

struct BatchDeleteManager;
static BatchDeleteManager* gBatchDeleteManager = NULL;

static void* BatchDeleteStep2Threaded (void* userData);

struct BatchDeleteManager
{
	Thread               thread;
	ThreadedStreamBuffer streamBuffer;
	
	///////@TODO: Handle when we are out of ring buffer memory.
	
	BatchDeleteManager ()
	: streamBuffer (ThreadedStreamBuffer::kModeThreaded, 1024 * 256 * sizeof(Object*) )
	{
		
	}
};

void InitializeBatchDelete ()
{
	gBatchDeleteManager = UNITY_NEW(BatchDeleteManager, kMemGarbageCollector);
	gBatchDeleteManager->thread.SetName ("BatchDeleteObjects");
	gBatchDeleteManager->thread.Run(BatchDeleteStep2Threaded, mono_domain_get());
}

void CleanupBatchDelete ()
{
	// Send terminate instruction & wait for thread to complete
	gBatchDeleteManager->streamBuffer.WriteValueType<Object*> (kTerminateInstruction);
	gBatchDeleteManager->streamBuffer.WriteSubmitData ();
	
	gBatchDeleteManager->thread.WaitForExit();

	UNITY_DELETE(gBatchDeleteManager, kMemGarbageCollector);
}

BatchDelete CreateBatchDelete (size_t size)
{
	size_t allocationSize = sizeof(Object*)*size;
	void* objectArray = gBatchDeleteManager->streamBuffer.GetWriteDataPointer (allocationSize, sizeof(Object*));
	
	BatchDelete batchInfo;
	batchInfo.reservedObjectCount = size;
	batchInfo.objectCount = 0;
	batchInfo.objects = reinterpret_cast<Object**> (objectArray);
	
	return batchInfo;
}

static void* BatchDeleteStep2Threaded (void* userData)
{
	// Attach mono thread
	MonoThread* thread = mono_thread_attach((MonoDomain*)userData);

	ThreadedStreamBuffer& threadedStreamBuffer = gBatchDeleteManager->streamBuffer;
	
	while (true)
	{
		Object* object = threadedStreamBuffer.ReadValueType<Object*> ();
		
		// Terminate instruction. Stop the thread.
		if (object == kTerminateInstruction)
			return NULL;
		
		// Delete the object
		if (object != NULL)
			delete_object_internal_step2 (object);
		
		threadedStreamBuffer.ReadReleaseData ();
	}
	
	// Detach mono thread
	mono_thread_detach(thread);
	
	return NULL;
}

void SharkBeginRemoteProfiling ();
void SharkEndRemoteProfiling ();

/// Callbacks like ScriptableObject.OnDestroy etc must be called before invoking this function.
void BatchDeleteObjectInternal (const SInt32* unloadObjects, int size)
{
	if (size == 0)
		return;
	
	PROFILER_AUTO(gBatchDeleteObjects, NULL)
	
	BatchDelete batch = CreateBatchDelete (size);
	int destroyObjectIndex = 0;
	for (int i=0;i<size;i++)
	{
		Object* object = Object::IDToPointer(unloadObjects[i]);
		batch.objects[destroyObjectIndex] = object;
	}
	batch.objectCount = destroyObjectIndex;
	
	CommitBatchDelete(batch);
}

void CommitBatchDelete (BatchDelete& batchDelete)
{
	Assert(batchDelete.reservedObjectCount >= batchDelete.objectCount);

	LockObjectCreation();

	for (int i=0;i<batchDelete.objectCount;i++)
	{
		Object* object = batchDelete.objects[i];

		if (object == NULL)
			continue;
		
		delete_object_internal_step1 (object);
		
		int classID = object->GetClassID();
		if (DoesClassRequireMainThreadDeallocation (classID))
		{
			bool requiresThreadCleanup = object->MainThreadCleanup ();
			/// DoesClassRequireMainThreadDeallocation does not agree with MainThreadCleanup
			/// Fix DoesClassRequireMainThreadDeallocation or MainThreadCleanup for that class.
			Assert(requiresThreadCleanup);
		}
	}
	
	for (int i=batchDelete.objectCount;i<batchDelete.reservedObjectCount;i++)
		batchDelete.objects[i] = NULL;
	
	UnlockObjectCreation();
	
	gBatchDeleteManager->streamBuffer.WriteSubmitData ();
}


#else

BatchDelete CreateBatchDelete (size_t size)
{
	BatchDelete batch;
	
	batch.objects = (Object**)UNITY_MALLOC(kMemGarbageCollector, sizeof(Object*)*size);
	batch.reservedObjectCount = size;
	batch.objectCount = 0;
	
	return batch;
}

void CommitBatchDelete (BatchDelete& batchDelete)
{
	PROFILER_AUTO(gBatchDeleteObjectsThread, NULL)
	
	Assert(batchDelete.reservedObjectCount >= batchDelete.objectCount);
	
	LockObjectCreation();
	
	for (int i=0;i<batchDelete.objectCount;i++)
	{
		Object* object = batchDelete.objects[i];
		if (object != NULL)
		{
			delete_object_internal_step1 (object);
			delete_object_internal_step2 (object);
		}
	}
	
	UnlockObjectCreation();

	// Cleanup temp storage
	UNITY_FREE(kMemGarbageCollector, batchDelete.objects);
}

void BatchDeleteObjectInternal (const SInt32* unloadObjects, int size)
{
	PROFILER_AUTO(gBatchDeleteObjectsThread, NULL)
	
	BatchDelete batchInfo = CreateBatchDelete (size);
	batchInfo.objectCount = size;
	for (int i=0;i<size;i++)
		batchInfo.objects[i] = Object::IDToPointer(unloadObjects[i]);

	CommitBatchDelete (batchInfo);
}

void InitializeBatchDelete ()
{
}

void CleanupBatchDelete ()
{
}

#endif	
