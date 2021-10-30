#ifndef PERSISTENTMANAGER_H
#define PERSISTENTMANAGER_H


#define SUPPORT_INSTANCE_ID_REMAP_ON_LOAD (UNITY_EDITOR || WEBPLUG)

class SerializedFile;
class Object;
class TypeTree;
struct FileIdentifier;

#include <map>
#include <string>
#include <vector>
#include <stack>
#include <deque>
#include <set>
#include "Runtime/Utilities/vector_map.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "WriteData.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "LoadProgress.h"

using std::map;
using std::set;
using std::vector;
using std::string;
using std::stack;

class AwakeFromLoadQueue;
class Remapper;

struct StreamNameSpace
{
	SerializedFile* stream;
	LocalIdentifierInFileType highestID;

	StreamNameSpace ()		{ stream = NULL; highestID = 0; }
};

enum
{
	kNoError = 0,
	kFileCouldNotBeRead = 1,
	kTypeTreeIsDifferent = 2,
	kFileCouldNotBeWritten = 3
};

enum UnpersistMode {
	kDontDestroyFromFile = 0,
	kDestroyFromFile = 1
};


struct ThreadedAwakeData
{
	SInt32  instanceID;
	TypeTree* oldType;
	Object* object;
	bool checkConsistency; /// refactor to safeLoaded

	// Has the object been fully loaded with AwakeFromLoadThreaded.
	// We have to make sure the Object* is available already, so that recursive PPtr's to each other from Mono can correctly be resolved.
	// In this case, neither object has been fully created, but we can setup pointers between them already.
	bool completedThreadAwake;
};

struct SerializedObjectIdentifier
{
	SInt32 serializedFileIndex;
	LocalIdentifierInFileType localIdentifierInFile;

	SerializedObjectIdentifier (SInt32 inSerializedFileIndex, LocalIdentifierInFileType inLocalIdentifierInFile)
	: serializedFileIndex (inSerializedFileIndex)
	, localIdentifierInFile (inLocalIdentifierInFile)
	{  }

	SerializedObjectIdentifier ()
	: serializedFileIndex (0)
	, localIdentifierInFile (0)
	{  }


	friend bool operator < (const SerializedObjectIdentifier& lhs, const SerializedObjectIdentifier& rhs)
	{
		if (lhs.serializedFileIndex < rhs.serializedFileIndex)
			return true;
		else if (lhs.serializedFileIndex > rhs.serializedFileIndex)
			return false;
		else
			return lhs.localIdentifierInFile < rhs.localIdentifierInFile;
	}

	friend bool operator != (const SerializedObjectIdentifier& lhs, const SerializedObjectIdentifier& rhs)
	{
		return lhs.serializedFileIndex != rhs.serializedFileIndex || lhs.localIdentifierInFile != rhs.localIdentifierInFile;
	}
};

struct LocalSerializedObjectIdentifier;

// There are three types of ids.
// fileID, is an id that is local to a file. It ranges from [1 ... kNameSpaceSize]
// heapID, is an id that was allocated for an object that was not loaded from disk. [1 ... infinity]
// pathID, is an id to a file. Every file has a unique id. They are not recycled unless you delete the PersistentManager

class PersistentManager
{
	protected:
	enum
	{
// Recursive serialization causes the deflated stream to be reset repeatedly - use bigger cache chunks to limit the impact on load times.
#if UNITY_ANDROID
		kCacheSize = 1024 * 256
#elif UNITY_XBOX360
		kCacheSize = 1024 * 32
#elif UNITY_WINRT || UNITY_BB10
		kCacheSize = 1024 * 64
#else
		kCacheSize = 1024 * 7
#endif
	};
	
	typedef std::pair<SInt32, SInt32> IntPair;
	typedef vector_map<SInt32, SInt32, std::less<SInt32>, STL_ALLOCATOR(kMemSerialization, IntPair) > IDRemap;
	typedef UNITY_VECTOR(kMemSerialization,StreamNameSpace)  StreamContainer;
	
	StreamContainer                         m_Streams;
	UNITY_VECTOR(kMemSerialization,IDRemap) m_GlobalToLocalNameSpace;
	UNITY_VECTOR(kMemSerialization,IDRemap) m_LocalToGlobalNameSpace;
	Remapper*                               m_Remapper;
	
	typedef std::pair<std::string,std::string> StringPair;
	typedef vector_map<std::string, std::string, compare_string_insensitive,STL_ALLOCATOR(kMemSerialization,StringPair)> UserPathRemap;
	UserPathRemap                           m_UserPathRemap;

	#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
	typedef vector_map<SerializedObjectIdentifier, SerializedObjectIdentifier, 
		std::less<SerializedObjectIdentifier>,STL_ALLOCATOR(kMemSerialization,SerializedObjectIdentifier) > InstanceIDRemap;
	InstanceIDRemap                         m_InstanceIDRemap;
	#endif // #if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD

	#if UNITY_EDITOR
	UNITY_SET(kMemSerialization,int)        m_NonTextSerializedClasses;
	#endif

	SInt32                                  m_CacheCount;

	stack<SInt32, std::deque<SInt32, STL_ALLOCATOR(kMemSerialization,SInt32) > > m_ActiveNameSpace;
	int                                     m_Options;

	#if DEBUGMODE
	bool                                    m_AllowLoadingFromDisk;
	bool                                    m_IsLoadingSceneFile;
	int                                     m_PreventLoadingFromFile;
	#endif

	UNITY_SET(kMemSerialization, std::string) m_MemoryLoadedOrCachedPaths;

	Mutex               m_Mutex;
	Mutex               m_IntegrationMutex;

	Mutex               m_MemoryLoadedOrCachedPathsMutex;

	bool                m_AllowIntegrateThreadedObjectsWithTimeout; // Mutex protected by m_IntegrationMutex

#if ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP
	MemoryPool                             m_ThreadedAwakeDataPool;
	MemoryPool                             m_ThreadedAwakeDataPoolMap;
	typedef  std::list<ThreadedAwakeData, memory_pool_explicit<ThreadedAwakeData> > ThreadedObjectActivationQueue;
	typedef  std::map<SInt32, ThreadedObjectActivationQueue::iterator, std::less<SInt32>, memory_pool_explicit<ThreadedObjectActivationQueue::iterator> > ThreadedObjectActivationMap;
#else
	typedef  std::list<ThreadedAwakeData>  ThreadedObjectActivationQueue;
	typedef  std::map<SInt32,  ThreadedObjectActivationQueue::iterator> ThreadedObjectActivationMap;
#endif
	ThreadedObjectActivationQueue          m_ThreadedObjectActivationQueue; // protected by m_IntegrationMutex
	ThreadedObjectActivationMap            m_ThreadedObjectActivationMap; 	// protected by m_IntegrationMutex
	UNITY_SET(kMemSerialization, SInt32)   m_OnDemandThreadLoadedObjects; /// DONT USE POOL HERE NOT THREAD SAFE

	StreamNameSpace& GetStreamNameSpaceInternal (int nameSpaceID);
	void DestroyFromFileInternal (int memoryID);
	public:

	PersistentManager (int options, int cacheCount);
	virtual ~PersistentManager ();

	/// Loads all objects in pathName
	/// Returns kNoError, kFileCouldNotBeRead
	int LoadFileCompletely (const string& pathname);

	#if UNITY_EDITOR
	// Makes an object persistent and generates a unique fileID in pathName
	// The object can now be referenced by other objects that write to disk
	void MakeObjectPersistent (int heapID, const string& pathName);
	// Makes an object persistent if fileID == 0 a new unique fileID in pathName will be generated
	// The object can now be referenced by other objects that write to disk
	// If the object is already persistent in another file or another fileID it will be destroyed from that file.
	void MakeObjectPersistentAtFileID (int heapID, LocalIdentifierInFileType fileID, const string& pathName);

	/// Batch multiple heapID's and fileID's into one path name.
	/// on return fileID's will contain the file id's that were generated (if fileIds[i] is non-zero that fileID will be used instead)
	enum { kMakePersistentDontRequireToBeLoadedAndDontUnpersist = 1 << 0, kAllowDontSaveObjectsToBePersistent = 1 << 1  };
	void MakeObjectsPersistent (const int* heapIDs, LocalIdentifierInFileType* fileIDs, int size, const string& pathName, int options = 0);
	#endif

	// Makes an object unpersistent
	void MakeObjectUnpersistent (int memoryID, UnpersistMode unpersistMode);

	bool RemoveObjectsFromPath (const std::string& pathName);

	// Returns the pathname the referenced object is stored at, if its not persistent empty string is returned
	string GetPathName (SInt32 memoryID);
	// Returns the localFileID the referenced object has inside its file.

	bool InstanceIDToSerializedObjectIdentifier (int instanceID, SerializedObjectIdentifier& identifier);
	int SerializedObjectIdentifierToInstanceID (const SerializedObjectIdentifier& identifier);

	LocalIdentifierInFileType GetLocalFileID(SInt32 instanceID);

	// Generates or returns an instanceID from path and fileID which can then
	// be used to load the object at that instanceID
	///@TODO: RENAME TO LocalIdentifierInFile
	SInt32 GetInstanceIDFromPathAndFileID (const string& path, LocalIdentifierInFileType localIdentifierInFile);

	// Returns classID from path and fileID.
	int GetClassIDFromPathAndFileID (const string& path, LocalIdentifierInFileType localIdentifierInFile);

	// Reads the object referenced by heapID, if there is no object with heapID, the object will first be produced.
	// Returns the created and read object, or NULL if the object couldnt be found or was destroyed.
	Object* ReadObject (int heapID);

	// Unloads all streams that are open.
	// After UnloadStreams is called files may be safely replaced.
	// May only be called if there are no dirty files open.
	void UnloadStreams ();
	void UnloadStream (const std::string& pathName);

	bool IsStreamLoaded (const std::string& pathName);

	#if UNITY_EDITOR

	typedef bool VerifyWriteObjectCallback (Object* verifyDeployment, BuildTargetPlatform target);

	// Writes all persistent objects in memory that are made peristent at pathname to the file
	// And completes all write operation (including writing the header)
	// Returns the error (kNoError)
	// options: kSerializeGameRelease, kSwapEndianess, kBuildPlayerOnlySerializeBuildProperties
	int WriteFile (const string& pathName, BuildTargetSelection target = BuildTargetSelection::NoTarget(), int options = 0);

	bool IsClassNonTextSerialized(int cid);

	int WriteFileInternal (const std::string& path, int serializedFileIndex, const WriteData* writeData, int size, VerifyWriteObjectCallback* verifyCallback, BuildTargetSelection target, int options);

	#if UNITY_EDITOR
	bool TestNeedWriteFile (const std::string& pathName, const std::set<int>* dirtyPaths = NULL);
	bool TestNeedWriteFile (int pathID, const std::set<int>* dirtyPaths = NULL);
	#endif

	// Delete file deletes the file referenced by pathName
	// Makes all loaded objects unpersistent
	// deleteLoadedObjects & kDeleteLoadedObjects -> All objects on the disk will be attempted to be destroyed
	// deleteLoadedObjects & kDontDeleteLoadedObjects -> Doesn't delete any loaded objects, but marks them unpersistent from the file.
	enum DeletionFlags { kDontDeleteLoadedObjects = 0, kDeleteLoadedObjects = 1 << 0 };
	bool DeleteFile (const string& pathName, DeletionFlags flag);

	void AddNonTextSerializedClass (int classID) { m_NonTextSerializedClasses.insert (classID); }
	#endif

	// On return: objects are the instanceIDs of all objects resident in the file referenced by pathName
	typedef std::set<SInt32> ObjectIDs;
	void GetInstanceIDsAtPath (const string& pathName, ObjectIDs* objects);
	void GetInstanceIDsAtPath (const string& pathName, vector<SInt32>* objects);

	void GetLoadedInstanceIDsAtPath (const string& pathName, ObjectIDs* objects);
	void GetPersistentInstanceIDsAtPath (const string& pathName, std::set<SInt32>* objects);

	int CountInstanceIDsAtPath (const string& pathName);

	void SetAllowIntegrateThreadedObjectsWithTimeout (bool value);

	bool IsFileEmpty (const string& pathName);

	// Finds out if the referenced object can be loaded.
	// By looking for it on the disk. And checking if the classID can be produced.
	bool IsObjectAvailable (int heapID);
	bool IsObjectAvailableDontCheckActualFile (int heapID);

	void GetAllFileIDs (const string& pathName, vector<LocalIdentifierInFileType>* objects);

	// Finds the
	int GetSerializedClassID (int instanceID);




	// Resets the fileIDs. This can only be used if the file has just been deleted.
	void ResetHighestFileIDAtPath (const string& pathName);

	// Computes the memoryID (object->GetInstanceID ()) from fileID
	// fileID is relative to the file we are currently writing/reading from.
	// It can only be called when reading/writing objects in order to
	// convert ptrs from file space to global space
	void LocalSerializedObjectIdentifierToInstanceIDInternal (const LocalSerializedObjectIdentifier& identifier, SInt32& memoryID);
	void LocalSerializedObjectIdentifierToInstanceIDInternal (int activeNameSpace, const LocalSerializedObjectIdentifier& localIdentifier, SInt32& outInstanceID);

	// fileID from memory ID (object->GetInstanceID ())
	// It can only be called when reading/writing objects in order
	// to convert ptrs from global space to file space
	void InstanceIDToLocalSerializedObjectIdentifierInternal (SInt32 memoryID, LocalSerializedObjectIdentifier& identifier);

	// Translates globalIdentifier.serializedFileIndex from a global index into the local file index based on what file we are currently writing.
	// It can only be called when reading/writing objects in order
	// to convert ptrs from global space to file space
	LocalSerializedObjectIdentifier GlobalToLocalSerializedFileIndexInternal (const SerializedObjectIdentifier& globalIdentifier);

	/// Is this instanceID mapped to the file we are currently writing,
	/// in other words is the referenced instanceID read or written from the same file then this will return true.
	bool IsInstanceIDFromCurrentFileInternal (SInt32 memoryID);

	#if UNITY_EDITOR
	// Hints a fileID to heap id mapping.
	// This i used to keep similar instanceID's when entering / exiting playmode
	void SuggestFileIDToHeapIDs (const string& pathname, std::map<LocalIdentifierInFileType, SInt32>& fileIDToHeapIDHint);

	int GetSerializedFileIndexFromPath (const std::string& path);

	#endif

	#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
	void RemapInstanceIDOnLoad (const std::string& srcPath, LocalIdentifierInFileType srcLocalFileID, const std::string& dstPath, LocalIdentifierInFileType dstLocalFileID);
	void ApplyInstanceIDRemap(SerializedObjectIdentifier& id);
	#endif // #if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD

	/// NOTE:  Returns an object that has not been completely initialized (Awake has not been called yet)
	Object* ReadObjectThreaded (int heapID);

	/// Check if we have objects to integrate, ie. if calling IntegrateThreadedObjects will perform some useful work
	bool HasThreadedObjectsToIntegrate ();

	/// Integrates all thread loaded objects into the world (Called from PlayerLoop)
	void IntegrateThreadedObjects (float timeout);

	/// Called from outise the loading thread (non-main thread), allows IntegrateThreadedObjects to be called with time slicing
	/// Stalls until all objects have been integrated
	void AllowIntegrationWithTimeoutAndWait ();

	void IntegrateAllThreadedObjects ();


	void PrepareAllThreadedObjectsStep1 (AwakeFromLoadQueue& awakeQueue);
	void IntegrateAllThreadedObjectsStep2 (AwakeFromLoadQueue& awakeQueue/*, bool loadScene*/);



	/// Load the entire file from a different thread
	int LoadFileCompletelyThreaded (const std::string& pathname, LocalIdentifierInFileType* fileIDs, SInt32* instanceIDs, int size, bool loadScene, LoadProgress* loadProgress);

	/// Loads a number of objects threaded
	void LoadObjectsThreaded (SInt32* heapIDs, int size, LoadProgress* loadProgress);

	#if SUPPORT_THREADS
	const Mutex& GetMutex () { return m_Mutex; }
	Thread::ThreadID GetMainThreadID () { return Thread::mainThreadId; }
	#endif

	#if DEBUGMODE
	/// Allows you to assert on any implicit loading operations for a specific file
	void SetDebugAssertLoadingFromFile (const std::string& path);
	/// Allows you to assert on any implicit loading operations, because for example when unloading objects that is usually not desired.
	void SetDebugAssertAllowLoadingAnything (bool allowLoading) { m_AllowLoadingFromDisk = allowLoading; }

	void SetIsLoadingSceneFile(bool inexplicit) { m_IsLoadingSceneFile = inexplicit; }
	#endif

	void Lock();
	void Unlock();
	/// Loads the contents of the object from disk again
	/// Performs serialization and calls AwakeFromLoad
	bool ReloadFromDisk (Object* obj);

	/// Load a memory stream directly from memory
	/// - You should call this function only on assets that have been writting using kSerializeGameReleaswe
	bool LoadMemoryBlockStream (const std::string& pathName, UInt8** data, int offset, int end, const char* url = NULL);

	// Loads a file at actualAbsolutePath and pretends that it is actually at path.
	/// - You should call this function only on assets that have been writting using kSerializeGameReleaswe
	bool LoadCachedFile (const std::string& path, const std::string actualAbsolutePath);

	void UnloadMemoryStreams ();

	// A registered SafeBinaryReadCallbackFunction will be called when an objects old typetree is different from the new one
	// and variables might have gone away, added, or changed
	typedef void SafeBinaryReadCallbackFunction (Object& object, const TypeTree& oldTypeTree);
	static void RegisterSafeBinaryReadCallback (SafeBinaryReadCallbackFunction* callback);

	// In order for DeleteFile to work you have to support a callback that deletes the objects referenced by instanceID
	typedef void InOrderDeleteCallbackFunction (const set<SInt32>& objects, bool safeDestruction);
	static void RegisterInOrderDeleteCallback (InOrderDeleteCallbackFunction* callback);

	/// Thread locking must be performed from outside using Lock/Unlock
	SerializedFile* GetSerializedFileInternal (const string& path);
	SerializedFile* GetSerializedFileInternal (int serializedFileIndex);

	void SetPathRemap (const string& path, const string& absoluteRemappedPath);

	// Non-Locking method to find out if a memorystream or cached file is set up.
	bool HasMemoryOrCachedSerializedFile (const std::string& path);

	#if SUPPORT_SERIALIZATION_FROM_DISK
	bool LoadExternalStream (const std::string& pathName, const std::string& absolutePath, int flags, int readOffset = 0);
	#endif

	void UnloadNonDirtyStreams ();

	/// NOTE: Function postfixed Internal are not thread safe and you have to call PersistentManager.Lock / Unlock prior to calling them from outside persistentmanager


	//// Subclasses have to override these methods which map from PathIDs to FileIdentifier
	/// Maps a pathname/fileidentifier to a pathID. If the pathname is not yet known, you have to call AddStream ().
	/// The pathIDs start at 0 and increment by 1
	virtual int InsertFileIdentifierInternal (FileIdentifier file, bool create) = 0;

	std::string RemapToAbsolutePath (const std::string& path);

	void DoneLoadingManagers();

	protected:

	virtual int InsertPathNameInternal (const std::string& pathname, bool create) = 0;

	///  maps a pathID to a pathname/file guid/fileidentifier.
	/// (pathID can be assumed to be allocated before with InsertPathName)
	virtual string PathIDToPathNameInternal (int pathID) = 0;
	virtual FileIdentifier PathIDToFileIdentifierInternal (int pathID) = 0;

	/// Adds a new empty stream. Used by subclasses inside InsertPathName when a new pathID has to be added
	void AddStream ();

	private:

	void CleanupStreamAndNameSpaceMapping (unsigned pathID);

	void RegisterAndAwakeThreadedObjectAndUnlockIntegrationMutex (const ThreadedAwakeData& awake);

	ThreadedAwakeData* CreateThreadActivationQueueEntry (SInt32 instanceID);
	void SetupThreadActivationQueueObject (ThreadedAwakeData& data, TypeTree* oldType, bool didTypeTreeChange);

	/// Goes through object activation queue and calls AwakeFromLoad if it has been serialized already but not AwakeFromLoad called.
	Object* LoadFromActivationQueue (int heapID);
	Object* GetFromActivationQueue (int heapID);
	bool FindInActivationQueue (int heapID);
	void CheckInstanceIDsLoaded (SInt32* heapIDs, int size);

	protected:

	void PostLoadStreamNameSpace (StreamNameSpace& nameSpace, int namespaceID);

#if UNITY_EDITOR
	bool TestNeedWriteFileInternal (int pathID, const std::set<int>* cachedDirtyPathsHint);
#endif

	friend class Object;
};

PersistentManager& GetPersistentManager ();
PersistentManager* GetPersistentManagerPtr ();

void CleanupPersistentManager();

#endif
