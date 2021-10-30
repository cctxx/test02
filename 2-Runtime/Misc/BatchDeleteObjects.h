#pragma once

class Object;

struct BatchDelete
{
	size_t      reservedObjectCount;
	size_t      objectCount;
	Object**    objects;
};

// Creates a batch delete object. When the object array has been filled (All Object* must be set. The function does not set them to null)
// You can call CommitBatchDelete which will make the deletion thread delete the objects.
BatchDelete CreateBatchDelete (size_t size);

// Makes the batch delete
void CommitBatchDelete (BatchDelete& batchDelete);


/// Deletes an array of objects identified by instanceID. Deallocation is done on another thread.
/// Callbacks like ScriptableObject.OnDestroy etc must be called before invoking this function.
void BatchDeleteObjectInternal (const SInt32* unloadObjects, int size);


// Used by the batch deletion to figure out if a specific class must be deallocated on the main thread. Eg. Textures need to be deallocate on the main thread
// Object::MainThreadCleanup will be called if this per class check returns true.
bool DoesClassRequireMainThreadDeallocation (int classID);


void InitializeBatchDelete ();
void CleanupBatchDelete ();