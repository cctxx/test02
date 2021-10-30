#include "UnityPrefix.h"
#include "PersistentManager.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "SerializedFile.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Remapper.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/File.h"
#include "AwakeFromLoadQueue.h"

#define DEBUG_THREAD_LOAD 0
#define DEBUG_THREAD_LOAD_LONG_ACTIVATE !UNITY_RELEASE
#define DEBUG_MAINTHREAD_LOADING 0


#if DEBUG_THREAD_LOAD
#define printf_debug_thread printf_console
#else
#define printf_debug_thread 
#endif

#include "Runtime/Threads/ProfilerMutex.h"
#if ENABLE_PROFILER
#include "Runtime/Profiler/MemoryProfiler.h"
#endif 
PROFILER_INFORMATION(gMakeObjectUnpersistentProfiler, "Loading.MakeObjectUnpersistent", kProfilerLoading)
PROFILER_INFORMATION(gMakeObjectPersistentProfiler, "Loading.MakeObjectUnpersistent", kProfilerLoading)
PROFILER_INFORMATION(gAwakeFromLoadManager, "Loading.AwakeFromLoad", kProfilerLoading)
PROFILER_INFORMATION(gIDRemappingProfiler, "Loading.IDRemapping", kProfilerLoading)
PROFILER_INFORMATION(gWriteFileProfiler, "Loading.WriteFile", kProfilerLoading)
PROFILER_INFORMATION(gFindInActivationQueueProfiler, "Loading.FindInThreadedActivationQueue", kProfilerLoading)
PROFILER_INFORMATION(gReadObjectProfiler, "Loading.ReadObject", kProfilerLoading)
PROFILER_INFORMATION(gLoadFileProfiler, "Loading.LoadFile", kProfilerLoading)
PROFILER_INFORMATION(gIsObjectAvailable, "Loading.IsObjectAvailaable", kProfilerLoading)
PROFILER_INFORMATION(gLoadStreamNameSpaceProfiler, "Loading.LoadFileHeaders", kProfilerLoading)
PROFILER_INFORMATION(gLoadLockPersistentManager, "Loading.LockPersistentManager", kProfilerLoading)
PROFILER_INFORMATION(gLoadFromActivationQueueStall, "Loading.LoadFromActivationQueue stalled [wait for loading operation to finish]", kProfilerLoading)
// @TODO: Write test for cross references between monobehaviours in prefab and in scene!


static PersistentManager* gPersistentManager = NULL;
static PersistentManager::InOrderDeleteCallbackFunction* gInOrderDeleteCallback = NULL;
static PersistentManager::SafeBinaryReadCallbackFunction* gSafeBinaryReadCallback = NULL;

static const char* kSerializedFileArea = "SerializedFile";
static const char* kRemapperAllocArea = "PersistentManager.Remapper";

#if UNITY_EDITOR || SUPPORT_RESOURCE_IMAGE_LOADING
static const char* kResourceImageExtensions[] = { "resG", "res", "resS" };
#endif

double GetTimeSinceStartup ();

using namespace std;

bool PersistentManager::InstanceIDToSerializedObjectIdentifier (int instanceID, SerializedObjectIdentifier& identifier)
{
	PROFILER_AUTO_THREAD_SAFE(gIDRemappingProfiler, NULL);
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	return m_Remapper->InstanceIDToSerializedObjectIdentifier(instanceID, identifier);
}

int PersistentManager::SerializedObjectIdentifierToInstanceID (const SerializedObjectIdentifier& identifier)
{
	PROFILER_AUTO_THREAD_SAFE(gIDRemappingProfiler, NULL);
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	return m_Remapper->GetOrGenerateMemoryID (identifier);
}


LocalIdentifierInFileType PersistentManager::GetLocalFileID(SInt32 instanceID)
{
	SerializedObjectIdentifier identifier;
	InstanceIDToSerializedObjectIdentifier (instanceID, identifier);
	return identifier.localIdentifierInFile;
}

SInt32 PersistentManager::GetInstanceIDFromPathAndFileID (const string& path, LocalIdentifierInFileType localIdentifierInFile)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	SerializedObjectIdentifier identifier;
	identifier.serializedFileIndex = InsertPathNameInternal (path, true);
	identifier.localIdentifierInFile = localIdentifierInFile;
	return m_Remapper->GetOrGenerateMemoryID (identifier);
}

int PersistentManager::GetClassIDFromPathAndFileID (const string& path, LocalIdentifierInFileType localIdentifierInFile)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	SerializedObjectIdentifier identifier;
	identifier.serializedFileIndex = InsertPathNameInternal (path, true);
	identifier.localIdentifierInFile = localIdentifierInFile;

	SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
	if (stream == NULL)
		return -1;
	
	if (!stream->IsAvailable (identifier.localIdentifierInFile))
		return -1;
	
	return stream->GetClassID (identifier.localIdentifierInFile);
}

static void CleanupStream (StreamNameSpace& stream)
{
	SerializedFile* oldFile = stream.stream;
	stream.stream = NULL;
	
	UNITY_DELETE (oldFile, kMemSerialization);
}

int PersistentManager::GetSerializedClassID (int instanceID)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	SerializedObjectIdentifier identifier;
	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(instanceID, identifier))
		return -1;

	SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
	if (stream == NULL)
		return -1;
	
	if (!stream->IsAvailable (identifier.localIdentifierInFile))
		return -1;
	
	return stream->GetClassID (identifier.localIdentifierInFile);
}

void PersistentManager::GetAllFileIDs (const string& pathName, vector<LocalIdentifierInFileType>* objects)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	AssertIf (objects == NULL);

	int serializedFileIndex = InsertPathNameInternal (pathName, true);
	SerializedFile* stream = GetSerializedFileInternal (serializedFileIndex);
	if (stream == NULL)
		return;
	
	stream->GetAllFileIDs (objects);
}

bool PersistentManager::RemoveObjectsFromPath (const std::string& pathName)
{
#if DEBUGMODE
	AssertIf(!m_AllowLoadingFromDisk);
#endif

	ASSERT_RUNNING_ON_MAIN_THREAD
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	SInt32 serializedFileIndex = InsertPathNameInternal (pathName, false);	
	if (serializedFileIndex == -1)
		return false;

	vector<SInt32> temp;
	m_Remapper->RemoveCompletePathID(serializedFileIndex, temp);
	
	return true;
}

void PersistentManager::MakeObjectUnpersistent (int memoryID, UnpersistMode mode)
{
	PROFILER_AUTO_THREAD_SAFE(gMakeObjectUnpersistentProfiler, NULL);

#if DEBUGMODE
	AssertIf(!m_AllowLoadingFromDisk);
#endif

	ASSERT_RUNNING_ON_MAIN_THREAD
	
	Object* o = Object::IDToPointer (memoryID);
	if (o && !o->IsPersistent ())
	{
//		#if DEBUGMODE && !UNITY_RELEASE
//		AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, GetMainThreadID(), "PersistentManager.MakeObjectUnpersistent");
//		AssertIf (m_Remapper->GetPathID (memoryID) != -1);
//		#endif
		return;
	}

	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	if (mode == kDestroyFromFile)
		DestroyFromFileInternal (memoryID);

	m_Remapper->Remove (memoryID);
		
	if (o)
		o->SetIsPersistent (false);
}

#if UNITY_EDITOR
void PersistentManager::MakeObjectPersistent (int heapID, const string& pathName)
{
	MakeObjectPersistentAtFileID (heapID, 0, pathName);
}

void PersistentManager::MakeObjectPersistentAtFileID (int heapID, LocalIdentifierInFileType fileID, const string& pathName)
{
	MakeObjectsPersistent (&heapID, &fileID, 1, pathName);
}

void PersistentManager::MakeObjectsPersistent (const int* heapIDs, LocalIdentifierInFileType* fileIDs, int size, const string& pathName, int options)
{
	PROFILER_AUTO_THREAD_SAFE(gMakeObjectPersistentProfiler, NULL);
	
	AssertIf(!m_AllowLoadingFromDisk);
	AssertIf(!Thread::EqualsCurrentThreadID(GetMainThreadID()));

	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	AssertIf(pathName.empty());
	SInt32 globalNameSpace = InsertPathNameInternal (pathName, true);	
	StreamNameSpace* streamNameSpace = NULL;

	for (int i=0;i<size;i++)
	{
		int heapID = heapIDs[i];
		LocalIdentifierInFileType fileID = fileIDs[i];
		
		Object* o = Object::IDToPointer (heapID);
		
		if ((options & kMakePersistentDontRequireToBeLoadedAndDontUnpersist) == 0)
		{
			// Making an object that is not in memory persistent
			if (o == NULL)
			{
				ErrorString("Make Objects Persistent failed because the object can not be loaded");
				continue;
			}

			// Make Object unpersistent first
			if (o->IsPersistent ())
			{
				SerializedObjectIdentifier identifier;
				InstanceIDToSerializedObjectIdentifier(heapID, identifier);
				AssertIf (identifier.serializedFileIndex == -1);
	
				// Return if the file and serializedFileIndex is not going to change
				if (globalNameSpace == identifier.serializedFileIndex)
				{
					if (fileID == 0 || identifier.localIdentifierInFile == fileID)
						continue;
				}
		
				MakeObjectUnpersistent (heapID, kDestroyFromFile);
			}
		}
		
		if (streamNameSpace == NULL)
			streamNameSpace = &GetStreamNameSpaceInternal (globalNameSpace);
		
		// Allocate an fileID for this object in the File
		if (fileID == 0)
		{
			fileID = streamNameSpace->highestID;
			if (streamNameSpace->stream)
				fileID = max (streamNameSpace->highestID, streamNameSpace->stream->GetHighestID ());
			fileID++;
		}	
		streamNameSpace->highestID = max (streamNameSpace->highestID, fileID);
		
		SerializedObjectIdentifier identifier;
		identifier.serializedFileIndex = globalNameSpace;
		identifier.localIdentifierInFile = fileID;
		m_Remapper->SetupRemapping (heapID, identifier);
		fileIDs[i] = fileID;
		
		if (o)
		{
			
			AssertIf (o->TestHideFlag (Object::kDontSave) && (options & kAllowDontSaveObjectsToBePersistent) == 0);
			o->SetIsPersistent (true);
			o->SetDirty ();
		}
	}
}
#endif


void PersistentManager::LocalSerializedObjectIdentifierToInstanceIDInternal (const LocalSerializedObjectIdentifier& localIdentifier, SInt32& outInstanceID)
{
	int activeNameSpace = m_ActiveNameSpace.top();
	LocalSerializedObjectIdentifierToInstanceIDInternal (activeNameSpace, localIdentifier, outInstanceID);
}

void PersistentManager::LocalSerializedObjectIdentifierToInstanceIDInternal (int activeNameSpace, const LocalSerializedObjectIdentifier& localIdentifier, SInt32& outInstanceID)
{
	PROFILER_AUTO_THREAD_SAFE(gIDRemappingProfiler, NULL);

	LocalIdentifierInFileType localIdentifierInFile = localIdentifier.localIdentifierInFile;
	int localSerializedFileIndex = localIdentifier.localSerializedFileIndex;
	
	if (localIdentifierInFile == 0)
	{
		outInstanceID = 0;
		return;
	}

	AssertIf (localSerializedFileIndex == -1);

	int globalFileIndex;
	if (localSerializedFileIndex == 0)
		globalFileIndex = activeNameSpace;
	else
	{
		AssertIf (m_Streams[activeNameSpace].stream == NULL);
		
		AssertIf(activeNameSpace >= m_LocalToGlobalNameSpace.size() || activeNameSpace < 0);

		IDRemap::iterator found = m_LocalToGlobalNameSpace[activeNameSpace].find (localSerializedFileIndex);
			
		if (found != m_LocalToGlobalNameSpace[activeNameSpace].end ())
		{
			globalFileIndex = found->second;
		}
		else
		{
			AssertString ("illegal LocalPathID in persistentmanager");
			outInstanceID = 0;
			return;
		}
	}
	
	SerializedObjectIdentifier globalIdentifier;
	globalIdentifier.serializedFileIndex = globalFileIndex;
	globalIdentifier.localIdentifierInFile = localIdentifierInFile;

	#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
	ApplyInstanceIDRemap(globalIdentifier);
	#endif

	outInstanceID = m_Remapper->GetOrGenerateMemoryID (globalIdentifier);
}

#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
void PersistentManager::ApplyInstanceIDRemap(SerializedObjectIdentifier& id)
{
	InstanceIDRemap::iterator foundIDRemap = m_InstanceIDRemap.find(id);
	if (foundIDRemap != m_InstanceIDRemap.end())
		id = foundIDRemap->second;
}
#endif // #if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD


LocalSerializedObjectIdentifier PersistentManager::GlobalToLocalSerializedFileIndexInternal (const SerializedObjectIdentifier& globalIdentifier)
{
	LocalIdentifierInFileType localIdentifierInFile = globalIdentifier.localIdentifierInFile;
	int localSerializedFileIndex;
	
	// Remap globalPathID to localPathID
	int activeNameSpace = m_ActiveNameSpace.top ();
	
	IDRemap& globalToLocalNameSpace = m_GlobalToLocalNameSpace[activeNameSpace];
	IDRemap& localToGlobalNameSpace = m_LocalToGlobalNameSpace[activeNameSpace];
	
	IDRemap::iterator found = globalToLocalNameSpace.find (globalIdentifier.serializedFileIndex);
	if (found == globalToLocalNameSpace.end ())
	{
		SET_ALLOC_OWNER(NULL);
		AssertIf (activeNameSpace >= m_Streams.size());
		AssertIf (m_Streams[activeNameSpace].stream == NULL);
		SerializedFile& serialize = *m_Streams[activeNameSpace].stream;
		
		serialize.AddExternalRef (PathIDToFileIdentifierInternal (globalIdentifier.serializedFileIndex));
		
		localSerializedFileIndex = serialize.GetExternalRefs ().size ();
		globalToLocalNameSpace[globalIdentifier.serializedFileIndex] = localSerializedFileIndex;
		localToGlobalNameSpace[localSerializedFileIndex] = globalIdentifier.serializedFileIndex;
	}
	else
		localSerializedFileIndex = found->second;
	
	// Setup local identifier
	LocalSerializedObjectIdentifier localIdentifier;
	
	localIdentifier.localSerializedFileIndex = localSerializedFileIndex;
	localIdentifier.localIdentifierInFile = localIdentifierInFile;
	
	return localIdentifier;
}

void PersistentManager::InstanceIDToLocalSerializedObjectIdentifierInternal (SInt32 instanceID, LocalSerializedObjectIdentifier& localIdentifier)
{
	PROFILER_AUTO_THREAD_SAFE(gIDRemappingProfiler, NULL);
	
	AssertIf (m_ActiveNameSpace.empty ());
	if (instanceID == 0)
	{
		localIdentifier.localSerializedFileIndex = 0;
		localIdentifier.localIdentifierInFile = 0;
		return;
	}
	
	SerializedObjectIdentifier globalIdentifier;
	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier (instanceID, globalIdentifier))
	{
		localIdentifier.localSerializedFileIndex = 0;
		localIdentifier.localIdentifierInFile = 0;
		return;
	}
	
	localIdentifier = GlobalToLocalSerializedFileIndexInternal(globalIdentifier);
}

bool PersistentManager::IsInstanceIDFromCurrentFileInternal (SInt32 instanceID)
{
	if (instanceID == 0)
		return false;

	SerializedObjectIdentifier globalIdentifier;

	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier (instanceID, globalIdentifier))
		return false;

	int activeNameSpace = m_ActiveNameSpace.top ();
	return globalIdentifier.serializedFileIndex == activeNameSpace;
}

#if UNITY_EDITOR

int PersistentManager::GetSerializedFileIndexFromPath (const std::string& path)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	return InsertPathNameInternal (path, true);
}

bool PersistentManager::TestNeedWriteFile (const string& pathName, const std::set<int>* cachedDirtyPathsHint)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	int serializedFileIndex = InsertPathNameInternal (pathName, false);
	return TestNeedWriteFileInternal(serializedFileIndex, cachedDirtyPathsHint);
}

bool PersistentManager::TestNeedWriteFile (int globalFileIndex, const std::set<int>* cachedDirtyPathsHint)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	return TestNeedWriteFileInternal(globalFileIndex, cachedDirtyPathsHint);
}

bool PersistentManager::TestNeedWriteFileInternal (int globalFileIndex, const std::set<int>* cachedDirtyPathsHint)
{
	if (globalFileIndex == -1)
		return false;

	SerializedFile* stream = m_Streams[globalFileIndex].stream;
	
	bool isFileDirty = stream != NULL && stream->IsFileDirty ();

	// Something was deleted from the file. Must write it!
	if (isFileDirty)
		return true;

	// Use Dirty path indices to quickly determine if a file needs writing
	if (cachedDirtyPathsHint != NULL)
		return cachedDirtyPathsHint->count (globalFileIndex);

	Object* o;
	// Find out if file needs to write to disk
	// - Get all loaded objects that have registered themselves for being at that file
	set<SInt32> loadedWriteObjects;
	m_Remapper->GetAllLoadedObjectsAtPath (globalFileIndex, &loadedWriteObjects);
	for (set<SInt32>::iterator i=loadedWriteObjects.begin ();i != loadedWriteObjects.end ();i++)
	{
		o = Object::IDToPointer (*i);					
		if (o && o->IsPersistent () && o->IsPersistentDirty ())
			return true;
	}
	
	return false;
}

void PersistentManager::CleanupStreamAndNameSpaceMapping (unsigned serializedFileIndex)
{
	// Unload the file any way
	// This saves memory - especially when reimporting lots of assets like when rebuilding the library
	CleanupStream(m_Streams[serializedFileIndex]);
	
	m_GlobalToLocalNameSpace[serializedFileIndex].clear ();
	m_LocalToGlobalNameSpace[serializedFileIndex].clear ();
}

static bool InitTempWriteFile (FileCacherWrite& writer, const std::string& path, unsigned cacheSize)
{
	string tempWriteFileName = GenerateUniquePathSafe (path);
	if (tempWriteFileName.empty())
		return false;
	
	writer.InitWriteFile(path, cacheSize);
	
	return true;
}

int PersistentManager::WriteFile (const std::string& path, BuildTargetSelection target, int options)
{
	PROFILER_AUTO_THREAD_SAFE(gWriteFileProfiler, NULL);
	
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);
	
	int serializedFileIndex;
	serializedFileIndex = InsertPathNameInternal(path, false);
	if (serializedFileIndex == -1)
		return kNoError;
	
	bool needsWrite = TestNeedWriteFile(serializedFileIndex);
	
	// Early out
	if (!needsWrite)
	{
		// @TODO: THIS SHOULD NOT BE HACKED IN HERE. Make test coverage against increased leaking then remove this and call CleanupStream explicitly.
		CleanupStreamAndNameSpaceMapping(serializedFileIndex);
		return kNoError;
	}
	
	set<SInt32> writeObjects;
	if (options & kDontReadObjectsFromDiskBeforeWriting)
	{
		GetLoadedInstanceIDsAtPath (path, &writeObjects);
		Assert(!writeObjects.empty());
	}
	else
	{
		// Load all writeobjects into memory
		// (dont use LoadFileCompletely, since that reads all objects 
		// even those that might have been changed in memory)
		GetInstanceIDsAtPath (path, &writeObjects);
	}
	
	vector<WriteData> writeData;
	
	for (set<SInt32>::iterator i=writeObjects.begin ();i != writeObjects.end ();i++)
	{
		SInt32 instanceID = *i;
		
		// Force load object from disk.
		Object* o = dynamic_instanceID_cast<Object*> (instanceID);
				
		if (o == NULL)
			continue;
		
		#if UNITY_EDITOR
		// Disable text serialization for terrain data. Just too much data, you'd never want to merge.
		int cid = o->GetClassID();
		if (IsClassNonTextSerialized(cid))
			options &= ~kAllowTextSerialization;
		#endif

		AssertIf (o != NULL && !o->IsPersistent ());
		
		SerializedObjectIdentifier identifier;
		m_Remapper->InstanceIDToSerializedObjectIdentifier(instanceID, identifier);
		
		Assert (identifier.serializedFileIndex == serializedFileIndex);
		
		DebugAssertIf (!o->IsPersistent ());
		DebugAssertIf (m_Remapper->GetSerializedFileIndex (instanceID) != serializedFileIndex);
		DebugAssertIf (!m_Remapper->IsSetup (identifier));
		
		writeData.push_back(WriteData (identifier.localIdentifierInFile, instanceID, BuildUsageTag()));
	}
	
	sort(writeData.begin(), writeData.end());

	int result = WriteFileInternal(path, serializedFileIndex, &writeData[0], writeData.size(), NULL, target, options);
	if (result != kNoError && options & kAllowTextSerialization)
		// Try binary serialization as a fallback.
		result = WriteFileInternal(path, serializedFileIndex, &writeData[0], writeData.size(), NULL, target, options &~kAllowTextSerialization);
		
	return result;
}

int PersistentManager::WriteFileInternal (const string& path, int serializedFileIndex, const WriteData* writeObjectData, int size, VerifyWriteObjectCallback* verifyCallback, BuildTargetSelection target, int options)
{
	//printf_console("Writing file %s\n", pathName.c_str());

	// Create writing tools
	CachedWriter writer;

	FileCacherWrite serializedFileWriter;
	FileCacherWrite resourceImageWriters[kNbResourceImages];
	if (!InitTempWriteFile (serializedFileWriter, "Temp/tempFile", kCacheSize))
		return kFileCouldNotBeWritten;
	writer.InitWrite(serializedFileWriter);
	
	if (options & kBuildResourceImage)
	{
		for (int i=0;i<kNbResourceImages;i++)
		{
			string path = AppendPathNameExtension("Temp/tempFile", kResourceImageExtensions[i]);
			if (!InitTempWriteFile (resourceImageWriters[i], path, kCacheSize))
				return kFileCouldNotBeWritten;
			writer.InitResourceImage((ActiveResourceImage)i, resourceImageWriters[i]);
		}
	}
	
	// Cleanup old stream and mapping
	CleanupStreamAndNameSpaceMapping(serializedFileIndex);

	// Setup global to self namespace mapping	
	m_GlobalToLocalNameSpace[serializedFileIndex][serializedFileIndex] = 0;
	m_LocalToGlobalNameSpace[serializedFileIndex][0] = serializedFileIndex;
	
	// Create writable stream
	//@TODO: Object name might want to be 
	SerializedFile* tempSerialize = UNITY_NEW_AS_ROOT(SerializedFile, kMemSerialization, kSerializedFileArea, "");
	#if ENABLE_MEM_PROFILER
	tempSerialize->SetDebugPath(PathIDToPathNameInternal(serializedFileIndex));
	GetMemoryProfiler()->SetRootAllocationObjectName(tempSerialize, tempSerialize->GetDebugPath().c_str());
	#endif
	
	tempSerialize->InitializeWrite (writer, target, options);
	m_Streams[serializedFileIndex].stream = tempSerialize;

	m_ActiveNameSpace.push (serializedFileIndex);

	bool writeSuccess = true;
	// Write Objects in fileID order
	for (int i=0;i<size;i++)
	{
		LocalIdentifierInFileType localIdentifierInFile = writeObjectData[i].localIdentifierInFile;
		SInt32 instanceID = writeObjectData[i].instanceID;
		
		SerializedObjectIdentifier identifier (serializedFileIndex, localIdentifierInFile);
		
		bool shouldUnloadImmediately = false;
		
		Object* o = Object::IDToPointer (instanceID);;
		if (o == NULL)
		{
			if (options & kLoadAndUnloadAssetsDuringBuild)
			{
				o = dynamic_instanceID_cast<Object*> (instanceID);
				shouldUnloadImmediately = true;
			}
			
			// Object can not be loaded, don't write it
			if (o == NULL)
			{
				continue;
			}
		}

		if (verifyCallback != NULL && !verifyCallback (o, target.platform))
			writeSuccess = false;
		
		tempSerialize->WriteObject (*o, localIdentifierInFile, writeObjectData[i].buildUsage);
		o->ClearPersistentDirty ();

		if (shouldUnloadImmediately)
			UnloadObject(o);
	}

	m_ActiveNameSpace.pop();

	writeSuccess = writeSuccess && tempSerialize->FinishWriting() && !tempSerialize->HasErrors();

	// Delete temp stream
	if (m_Streams[serializedFileIndex].stream != tempSerialize)
	{
		writeSuccess = false;
		UNITY_DELETE (tempSerialize, kMemSerialization);
		tempSerialize = NULL;
	}

	// Delete mappings
	CleanupStreamAndNameSpaceMapping(serializedFileIndex);

	if (!writeSuccess)
	{
//		ErrorString ("Writing file: " + path + " failed. The temporary file " + serializedFileWriter.GetPathName() + " couldn't be written.");
		return kFileCouldNotBeWritten;
	}

	// Atomically move the serialized file into the target location
	string actualNewPathName = RemapToAbsolutePath (path);
	if (!MoveReplaceFile (serializedFileWriter.GetPathName(), actualNewPathName))
	{
		ErrorString ("File " + path + " couldn't be written. Because moving " + serializedFileWriter.GetPathName() + " to " + actualNewPathName + " failed.");
		return kFileCouldNotBeWritten;
	}
	SetFileFlags(actualNewPathName, kFileFlagTemporary, 0);

	
	if (options & kBuildResourceImage)
	{
		// Move the resource images into the target location
		for (int i=0;i<kNbResourceImages;i++)
		{
			string targetPath = AppendPathNameExtension(actualNewPathName, kResourceImageExtensions[i]);
			
			::DeleteFile(targetPath);
			
			string tempWriteFileName = resourceImageWriters[i].GetPathName();
			
			if (GetFileLength(tempWriteFileName) > 0)
			{
				if (!MoveReplaceFile (tempWriteFileName, targetPath))
				{
					ErrorString ("File " + path + " couldn't be written. Because moving " + tempWriteFileName + " to " + actualNewPathName + " failed.");
					return kFileCouldNotBeWritten;
				}
				SetFileFlags(targetPath, kFileFlagTemporary, 0);
			}
		}
	}
	
	return kNoError;
}

#endif

string PersistentManager::GetPathName (SInt32 memoryID)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	SInt32 serializedFileIndex = m_Remapper->GetSerializedFileIndex (memoryID);
	if (serializedFileIndex == -1)
		return string ();
	else
		return PathIDToPathNameInternal (serializedFileIndex);
}

void PersistentManager::RegisterAndAwakeThreadedObjectAndUnlockIntegrationMutex (const ThreadedAwakeData& awake)
{
	// Register instance ID first and then unlock integration mutex so there is no chance of an object being loaded twice
	AssertIf(!awake.completedThreadAwake);
	if (awake.object != NULL)
	{
	Object::RegisterInstanceID(awake.object);

	m_IntegrationMutex.Unlock();
	
	AwakeFromLoadMode mode = (AwakeFromLoadMode)(kDidLoadFromDisk | kDidLoadThreaded);
	AwakeFromLoadQueue::PersistentManagerAwakeSingleObject(*awake.object, awake.oldType, mode, awake.checkConsistency, gSafeBinaryReadCallback);
}
	else
	{
		m_IntegrationMutex.Unlock();
	}
}


#if THREADED_LOADING
void PersistentManager::AllowIntegrationWithTimeoutAndWait ()
{
	m_IntegrationMutex.Lock();
	m_AllowIntegrateThreadedObjectsWithTimeout = true;
	m_IntegrationMutex.Unlock();
	
	// Wait until the integration thread has integrated all assets
	while (true)
	{
		m_IntegrationMutex.Lock();

		if (m_ThreadedObjectActivationQueue.empty())
		{
			m_IntegrationMutex.Unlock();
			break;
		}
		else
		{
			m_IntegrationMutex.Unlock();
		}

		Thread::Sleep(0.1F);
	}

	m_IntegrationMutex.Lock();
	m_AllowIntegrateThreadedObjectsWithTimeout = false;
	m_IntegrationMutex.Unlock();
}
#else	
void PersistentManager::AllowIntegrationWithTimeoutAndWait ()
{
	IntegrateAllThreadedObjects();
}
#endif

#if DEBUG_THREAD_LOAD
int gDependencyCounter = 0;
int gDependencyCounterCost = 0;
int gDependencyCounterCostActivation = 0;
int gDependencyCounterCostNonActivation = 0;
int gDependencyCounterCostNotFound = 0;
#endif

Object* PersistentManager::LoadFromActivationQueue (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gFindInActivationQueueProfiler, NULL);
	
	ASSERT_RUNNING_ON_MAIN_THREAD

	LOCK_MUTEX(m_IntegrationMutex, gLoadFromActivationQueueStall);

	ThreadedObjectActivationMap::iterator imap  = m_ThreadedObjectActivationMap.find(heapID);

	if (imap != m_ThreadedObjectActivationMap.end())
	{
		ThreadedObjectActivationQueue::iterator i = imap->second;
		if (!i->completedThreadAwake)
		{
			ErrorString("Internal thread activation error. Activating object that has not been fully thread loaded.");
			m_IntegrationMutex.Unlock();

			return NULL;
		}

		ThreadedAwakeData data = *i;
		m_ThreadedObjectActivationQueue.erase(i);
		m_ThreadedObjectActivationMap.erase(imap);
		#if DEBUG_THREAD_LOAD
//			printf_debug_thread("Activating dependency %d / %d [%d]\n", ++gDependencyCounter, gDependencyCounterCost, thisCost);
//			if (thisCost > 300)
//			{
//				printf_debug_thread("");
//			}
		#endif

		RegisterAndAwakeThreadedObjectAndUnlockIntegrationMutex(data);

		return data.object;
	}

	#if DEBUG_THREAD_LOAD
//	if (thisCost > 300)
//	{
//		printf_debug_thread("Expensive load from activation queue and not found [%d]\n", thisCost);
//	}
	#endif

	m_IntegrationMutex.Unlock();

	return NULL;
}

// GetFromActivationQueue is called a lot, so this function must be very efficient.
// In one of the games called Harvest, m_ThreadedObjectActivationQueue contained 70000 items, and this function was called 5678 times
// so linear searching was ubber slow!!!
Object* PersistentManager::GetFromActivationQueue (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gFindInActivationQueueProfiler, NULL);

	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_IntegrationMutex,gLoadFromActivationQueueStall);
	
	ThreadedObjectActivationMap::iterator item = m_ThreadedObjectActivationMap.find(heapID);
	if (item != m_ThreadedObjectActivationMap.end())
	{
		return item->second->object;
	}

	AssertIf(m_OnDemandThreadLoadedObjects.count(heapID));
	
	return NULL;
}

bool PersistentManager::FindInActivationQueue (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gFindInActivationQueueProfiler, NULL);

	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_IntegrationMutex, gLoadFromActivationQueueStall);

	ThreadedObjectActivationMap::iterator item = m_ThreadedObjectActivationMap.find(heapID);
	if (item != m_ThreadedObjectActivationMap.end())
	{
		return true;
	}

	if (m_OnDemandThreadLoadedObjects.count(heapID))
		return true;

	return false;
}

bool PersistentManager::HasThreadedObjectsToIntegrate ()
{
	Mutex::AutoLock lock (m_IntegrationMutex);
	return !m_ThreadedObjectActivationQueue.empty ();
}

///@TODO: Prevent Object destruction during AwakeFromLoad!!!

void PersistentManager::IntegrateThreadedObjects (float timeout)
{
	// Early out if there is nothing to be integrated.
	if (!m_AllowIntegrateThreadedObjectsWithTimeout)
		return;
	
	if (!m_IntegrationMutex.TryLock())
	{
		//printf_debug_thread("\nINTEGRATION THREAD IS LOCKED\n");
		return;
	}
	
	if (m_ThreadedObjectActivationQueue.empty() || !m_AllowIntegrateThreadedObjectsWithTimeout)
	{
		m_IntegrationMutex.Unlock();
		return;
	}

	double startTime = GetTimeSinceStartup();
	#if DEBUG_THREAD_LOAD
	int integratedObjects = 0;
	#endif
	
	while (true)
	{
		ThreadedAwakeData awake = m_ThreadedObjectActivationQueue.front();
		
		if (!awake.completedThreadAwake)
		{
			ErrorString("Stalling Integration because ThreadAwake for object is not completed");
			break;
		}
		
		#if DEBUG_THREAD_LOAD
		integratedObjects++;
		#endif
		m_ThreadedObjectActivationMap.erase(awake.instanceID);
		m_ThreadedObjectActivationQueue.pop_front();
		
		#if DEBUG_THREAD_LOAD_LONG_ACTIVATE
		double time = GetTimeSinceStartup();
		#endif
		
		RegisterAndAwakeThreadedObjectAndUnlockIntegrationMutex(awake);
		
		#if DEBUG_THREAD_LOAD_LONG_ACTIVATE
		time = GetTimeSinceStartup() - time;
		time *= 1000.0F;
		if (time > 30.0F)
		{
			printf_console("Long thread activation time (%d ms) for object %s (%s)\n", (int)time, awake.object->GetName(), awake.object->GetClassName().c_str());
		}
		#endif
		
		if (!m_AllowIntegrateThreadedObjectsWithTimeout || !m_IntegrationMutex.TryLock())
		{
			#if DEBUG_THREAD_LOAD
			startTime = (GetTimeSinceStartup() - startTime) * 1000.0F;
			printf_debug_thread("Integrate Threaded Objects ms: %f %d\n\n", (float)startTime, integratedObjects);
			#endif
			return;
		}

		double delta = (GetTimeSinceStartup() - startTime);
		if (m_ThreadedObjectActivationQueue.empty() || delta > timeout)
			break;
	}
	
	#if DEBUG_THREAD_LOAD
	startTime = (GetTimeSinceStartup() - startTime) * 1000.0F;
	printf_debug_thread("Integrate Threaded Objects ms: %f %d\n\n", (float)startTime, integratedObjects);
	#endif

	m_IntegrationMutex.Unlock();
}

void PersistentManager::IntegrateAllThreadedObjects ()
{
	AwakeFromLoadQueue awakeQueue (kMemTempAlloc);
	
	PrepareAllThreadedObjectsStep1 (awakeQueue);
	awakeQueue.RegisterObjectInstanceIDs();
	IntegrateAllThreadedObjectsStep2 (awakeQueue);
}

void PersistentManager::PrepareAllThreadedObjectsStep1 (AwakeFromLoadQueue& awakeQueue)
{
	AQUIRE_AUTOLOCK (m_IntegrationMutex, gLoadFromActivationQueueStall);
	
	// Add to AwakeFromLoadQueue - this will take care of ensuring sort order
	awakeQueue.Reserve(m_ThreadedObjectActivationQueue.size());
	
	ThreadedObjectActivationQueue::iterator end = m_ThreadedObjectActivationQueue.end();
	for (ThreadedObjectActivationQueue::iterator i=m_ThreadedObjectActivationQueue.begin();i != end;++i )
	{
		ThreadedAwakeData& awake = *i;

		if (awake.object)
		{
			Assert (awake.completedThreadAwake);
			awakeQueue.Add(*awake.object, awake.oldType, awake.checkConsistency);
		}
	}
	m_ThreadedObjectActivationQueue.clear();
	m_ThreadedObjectActivationMap.clear();
}

void PersistentManager::IntegrateAllThreadedObjectsStep2 (AwakeFromLoadQueue& awakeQueue)
{
	Assert(m_ThreadedObjectActivationQueue.empty() && m_ThreadedObjectActivationMap.empty());

	// Invoke AwakeFromLoadQueue in sorted order
	AwakeFromLoadMode awakeMode = (AwakeFromLoadMode)(kDidLoadFromDisk | kDidLoadThreaded);
	
	awakeQueue.PersistentManagerAwakeFromLoad(awakeMode, gSafeBinaryReadCallback);

	Assert(m_ThreadedObjectActivationQueue.empty() && m_ThreadedObjectActivationMap.empty());
}


bool PersistentManager::ReloadFromDisk (Object* obj)
{
	PROFILER_AUTO_THREAD_SAFE(gReadObjectProfiler, obj);
	
#if DEBUGMODE
	AssertIf(!m_AllowLoadingFromDisk);
#endif

	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	SerializedObjectIdentifier identifier;
	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(obj->GetInstanceID(), identifier))
	{
		ErrorString("Trying to reload asset from disk that is not stored on disk");
		return false;
	}
	
	SerializedFile* stream = GetSerializedFileInternal(identifier.serializedFileIndex);
	if (stream == NULL)
	{
		ErrorString("Trying to reload asset but can't find object on disk");
		return false;
	}
	
	m_ActiveNameSpace.push (identifier.serializedFileIndex);
	TypeTree* oldType;
	bool didTypeTreeChange;
	stream->ReadObject (identifier.localIdentifierInFile, obj->GetInstanceID(), kCreateObjectDefault, true, &oldType, &didTypeTreeChange, &obj);
	m_ActiveNameSpace.pop ();
	
	// Awake the object
	if (obj)
	{
		AwakeFromLoadQueue::PersistentManagerAwakeSingleObject (*obj, oldType, kDidLoadFromDisk, didTypeTreeChange, gSafeBinaryReadCallback);
	}

	return true;
}

Object* PersistentManager::ReadObject (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gReadObjectProfiler, NULL);

#if DEBUGMODE
	AssertIf(!m_AllowLoadingFromDisk);
#endif

	#if !UNITY_EDITOR
	AssertIf(heapID < 0);
	#endif

	#if DEBUG_MAINTHREAD_LOADING
	double time = GetTimeSinceStartup ();
	#endif

	ASSERT_RUNNING_ON_MAIN_THREAD

	LOCK_MUTEX(m_Mutex, gLoadLockPersistentManager);

	Object* o = LoadFromActivationQueue(heapID);
	if (o != NULL)
{
		m_Mutex.Unlock();
		return o;
	}

	// Find and load the right stream
	SerializedObjectIdentifier identifier;
	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(heapID, identifier))
	{
		m_Mutex.Unlock();
		return NULL;
	}

#if DEBUGMODE
	AssertIf(!m_IsLoadingSceneFile && StrICmp(GetPathNameExtension(PathIDToPathNameInternal(identifier.serializedFileIndex)),"unity") == 0);
#endif

	SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
	if (stream == NULL)
	{
		#if DEBUG_MAINTHREAD_LOADING
		LogString(Format("--- Loading from main thread failed loading stream %f", (GetTimeSinceStartup () - time) * 1000.0F));
		#endif

		m_Mutex.Unlock();
		return NULL;
}

	AssertIf(Object::IDToPointer (heapID) != NULL);

	// Find file id in stream and read the object

	m_ActiveNameSpace.push (identifier.serializedFileIndex);
	TypeTree* oldType;
	bool didTypeTreeChange;
	o = NULL;
	stream->ReadObject (identifier.localIdentifierInFile, heapID, kCreateObjectDefault, true, &oldType, &didTypeTreeChange, &o);
	m_ActiveNameSpace.pop ();
	
	// Awake the object
	if (o)
	{
		AwakeFromLoadQueue::PersistentManagerAwakeSingleObject (*o, oldType, kDidLoadFromDisk, didTypeTreeChange, gSafeBinaryReadCallback);
	}
	
//	printf_console ("Read %d %s (%s)\n", heapID, o ? o->GetName () : "<null>", o ? o->GetClassName ().c_str() : "");
	
	m_Mutex.Unlock();
	
	#if DEBUG_MAINTHREAD_LOADING
	if (o)
	{
		LogString(Format("--- Loading from main thread %s (%s) %f ms", o->GetName(), o->GetClassName().c_str(), (GetTimeSinceStartup () - time) * 1000.0F));
	}
	else
	{
		LogString(Format("--- Loading from main thread (NULL) %f ms", (GetTimeSinceStartup () - time) * 1000.0F));
	}
	#endif
	
	return o;
}

void PersistentManager::SetupThreadActivationQueueObject (ThreadedAwakeData& awakeData, TypeTree* oldType, bool didTypeTreeChange)
{
	Object* obj = awakeData.object;
	if (obj != NULL)
{
	awakeData.oldType = oldType;
	awakeData.checkConsistency = didTypeTreeChange;
	obj->AwakeFromLoadThreaded();
	}
	awakeData.completedThreadAwake = true;
}

ThreadedAwakeData* PersistentManager::CreateThreadActivationQueueEntry (SInt32 instanceID)
{
	DebugAssertIf(Object::IDToPointerThreadSafe(instanceID) != NULL);
	
	ThreadedAwakeData awake;
	awake.instanceID = instanceID;
	awake.checkConsistency = false;
	awake.completedThreadAwake = false;
	awake.oldType = NULL;
	awake.object = NULL;

	ThreadedAwakeData* result;
	m_IntegrationMutex.Lock();

	m_ThreadedObjectActivationQueue.push_back(awake);
	result = &m_ThreadedObjectActivationQueue.back();
	m_ThreadedObjectActivationMap[instanceID] = --m_ThreadedObjectActivationQueue.end();

	m_IntegrationMutex.Unlock();
	DebugAssertIf(Object::IDToPointerThreadSafe(instanceID) != NULL);
	return result;
}

void PersistentManager::CheckInstanceIDsLoaded (SInt32* heapIDs, int size)
{
	m_IntegrationMutex.Lock();
	
	set<SInt32> ids;

	// Search through threaded object activation queue and activate the object if necessary.
	ThreadedObjectActivationQueue::iterator i, end;
	end = m_ThreadedObjectActivationQueue.end();
	for (i=m_ThreadedObjectActivationQueue.begin();i != end;i++)
		ids.insert(i->instanceID);
	
	for (int j=0;j<size;j++)
	{
		if (ids.count(heapIDs[j]))
			heapIDs[j] = 0;
	}
	
	m_IntegrationMutex.Unlock();

	// Check which objects are already loaded all at once to lock object creation only once for a short amount of time
	// Since we have locked persistentmanager already no objects can be loaded in the mean time
	LockObjectCreation();
	Object::CheckInstanceIDsLoaded(heapIDs, size);
	UnlockObjectCreation();
}

Object* PersistentManager::ReadObjectThreaded (int heapID)
{
#if DEBUGMODE
	AssertIf(!m_AllowLoadingFromDisk);
#endif
	
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	Object* o = GetFromActivationQueue(heapID);
	if (o != NULL)
		return o;

	// Find and load the right stream
	SerializedObjectIdentifier identifier;
	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(heapID, identifier))
		return NULL;

	SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
	if (stream == NULL)
		return NULL;
#if DEBUGMODE
	AssertIf(!m_IsLoadingSceneFile && PathIDToPathNameInternal(identifier.serializedFileIndex).find(".unity") != string::npos);
#endif
	AssertIf(Object::IDToPointerThreadSafe (heapID) != NULL);

	// Find file id in stream and read the object
	
	m_ActiveNameSpace.push (identifier.serializedFileIndex);
	TypeTree* oldType;
	bool didTypeTreeChange;

	ThreadedAwakeData* awakeData = CreateThreadActivationQueueEntry(heapID);
	
	// Inject into m_OnDemandThreadLoadedObjects so we can track which objects have been read during a LoadFileCompletelyThreaded or LoadThreadedObjects.
	// This prevents loading objects twice
	m_IntegrationMutex.Lock();
	m_OnDemandThreadLoadedObjects.insert(heapID);
	m_IntegrationMutex.Unlock();
	
	awakeData->object = NULL;
	stream->ReadObject (identifier.localIdentifierInFile, heapID, kCreateObjectFromNonMainThread, !m_Remapper->IsSceneID (heapID), &oldType, &didTypeTreeChange, &awakeData->object);
	
	m_ActiveNameSpace.pop ();
	
	o = awakeData->object;
	SetupThreadActivationQueueObject(*awakeData, oldType, didTypeTreeChange);

	return o;
}

void PersistentManager::LoadObjectsThreaded (SInt32* heapIDs, int size, LoadProgress* loadProgress)
{
	if (size == 0)
		return;

	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	vector<SInt32> heapIDsCopy;
	heapIDsCopy.assign(heapIDs, heapIDs + size);
	
	// Check which objects are already loaded all at once to lock object creation only once for a short amount of time
	// Since we have locked persistentmanager already no objects can be loaded in the mean time
	CheckInstanceIDsLoaded(&heapIDsCopy[0], size);

	#if DEBUGMODE
	m_IsLoadingSceneFile = true;
	#endif

	for (int i=0;i<size;i++)
	{
		SInt32 heapID = heapIDsCopy[i];
		if (heapID == 0)
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			continue;
		}

		m_IntegrationMutex.Lock();
		if (m_OnDemandThreadLoadedObjects.count(heapID))
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			m_IntegrationMutex.Unlock();
			continue;
		}
		m_IntegrationMutex.Unlock();
			
		DebugAssertIf(Object::IDToPointerThreadSafe(heapID) != NULL);
		//DebugAssertIf(FindInActivationQueue(heapID));

		// Find and load the right stream
		SerializedObjectIdentifier identifier;
		if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(heapID, identifier))
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			continue;
		}
		
		SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
		if (stream == NULL)
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			continue;
		}

		// Find file id in stream and read the object

		m_ActiveNameSpace.push (identifier.serializedFileIndex);
		TypeTree* oldType;
		bool didTypeTreeChange;
		
		ThreadedAwakeData* awakeData = CreateThreadActivationQueueEntry (heapID);
		awakeData->object = NULL;
		stream->ReadObject (identifier.localIdentifierInFile, heapID, kCreateObjectFromNonMainThread, true, &oldType, &didTypeTreeChange, &awakeData->object);
		if (loadProgress)
			loadProgress->ItemProcessed ();
		
		AssertIf (m_Remapper->IsSceneID (heapID));

		SetupThreadActivationQueueObject(*awakeData, oldType, didTypeTreeChange);
	
		m_ActiveNameSpace.pop ();
}

	m_IntegrationMutex.Lock();
	m_OnDemandThreadLoadedObjects.clear();
	m_IntegrationMutex.Unlock();
	
	#if DEBUGMODE
	m_IsLoadingSceneFile = false;
	#endif
			}
			
int PersistentManager::LoadFileCompletelyThreaded (const std::string& pathname, LocalIdentifierInFileType* fileIDs, SInt32* instanceIDs, int size, bool loadScene, LoadProgress* loadProgress)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadFromActivationQueueStall);

	// Find Stream
	int pathID = InsertPathNameInternal (pathname, true);
	SerializedFile* stream = GetSerializedFileInternal (pathID);
	if (stream == NULL)
		return kFileCouldNotBeRead;

	AssertIf(fileIDs != NULL && size == -1);
	AssertIf(instanceIDs != NULL && size == -1);
		
	// Get all file IDs we want to load and generate instance ids
	vector<LocalIdentifierInFileType> fileIDsVector;
	vector<SInt32> instanceIDsVector;
	if (size == -1)
	{
		stream->GetAllFileIDs (&fileIDsVector);
		fileIDs = &fileIDsVector[0];
		size = fileIDsVector.size();
		if (loadProgress)
			loadProgress->totalItems += size;
		instanceIDsVector.resize(size);
		instanceIDs = &instanceIDsVector[0];
	}
	
	// In the editor we can not use preallocate ranges since fileID's might be completely arbitrary ranges
	if (loadScene && !UNITY_EDITOR)
	{
		LocalIdentifierInFileType highestFileID = 0;
		for (int i=0;i<size;i++)
		{
			AssertIf(fileIDs[i] < 0);
			highestFileID = max(highestFileID, fileIDs[i]);
		}

		m_Remapper->PreallocateIDs(highestFileID, pathID);
		
		for (int i=0;i<size;i++)
		{
			LocalIdentifierInFileType fileID = fileIDs[i];
			AssertIf(m_Remapper->IsSetup(SerializedObjectIdentifier(pathID, fileID)));
			instanceIDs[i] = m_Remapper->m_ActivePreallocatedIDBase + fileID * 2;
		}

		#if DEBUGMODE
		//SHOULDN"T BE NEEDED!!!!!! - TAKE THIS OUT DEBUG ONLY!!!!
		CheckInstanceIDsLoaded(&instanceIDs[0], size);
		for (int i=0;i<size;i++)
		{
			AssertIf(instanceIDs[i] == 0);
		}
		#endif
	}
	else
	{
		for (int i=0;i<size;i++)
		{
			LocalIdentifierInFileType fileID = fileIDs[i];
			SInt32 heapID = m_Remapper->GetOrGenerateMemoryID (SerializedObjectIdentifier(pathID, fileID));

			if (heapID == 0)
			{
				AssertString ("Loading an object that was made unpersistent but wasn't destroyed before reloading it");
			}
			instanceIDs[i] = heapID;
		}
		// - Figure out which ones are already loaded
		CheckInstanceIDsLoaded(&instanceIDs[0], size);

		#if UNITY_EDITOR
		// Ugly hack to prevent IsPersistent to be on for scene objects.
		// Recursive serialization is to be blamed for this. When that is fixed this can be removed.
		if (loadScene)
		{
			m_Remapper->m_LoadingSceneInstanceIDs.assign_clear_duplicates(instanceIDs, instanceIDs + size);
		}
		#endif
	}

	// Load all objects
	m_ActiveNameSpace.push (pathID);
	#if DEBUGMODE
	m_IsLoadingSceneFile = true;
	#endif
	
	for (int i=0;i<size;i++)
	{
		SInt32 heapID = instanceIDs[i];
		SInt32 fileID = fileIDs[i];
		
		if (heapID == 0)
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			continue;
		}

		m_IntegrationMutex.Lock();
		if (m_OnDemandThreadLoadedObjects.count(heapID))
		{
			if (loadProgress)
				loadProgress->ItemProcessed ();
			m_IntegrationMutex.Unlock();
			continue;
		}
		m_IntegrationMutex.Unlock();
		
		DebugAssertIf(Object::IDToPointerThreadSafe (heapID) != NULL);

		TypeTree* oldType;
		bool didTypeTreeChange;
		ThreadedAwakeData* awakeData = CreateThreadActivationQueueEntry(heapID);
		awakeData->object = NULL;
		stream->ReadObject (fileID, heapID, kCreateObjectFromNonMainThread, !loadScene, &oldType, &didTypeTreeChange, &awakeData->object);
		if (loadProgress)
			loadProgress->ItemProcessed ();
		
		////@TODO: NEED TO HANDLE DELETION OF OBJECTS WHEN THAT HAPPENS BETWEEN threaded loading and activation
		SetupThreadActivationQueueObject(*awakeData, oldType, didTypeTreeChange);
	}
	
	#if DEBUGMODE
	m_IsLoadingSceneFile = false;
	#endif

	m_ActiveNameSpace.pop ();
	
	m_IntegrationMutex.Lock();
	m_OnDemandThreadLoadedObjects.clear();
	m_IntegrationMutex.Unlock();
	if (loadScene)
	{
		#if UNITY_EDITOR
		for (int i=0;i<size;i++)
			m_Remapper->Remove (instanceIDs[i]);
		m_Remapper->m_LoadingSceneInstanceIDs.clear();
		#else
		m_Remapper->ClearPreallocateIDs();
		#endif
		
	}
	
	return kNoError;
}

int PersistentManager::LoadFileCompletely (const string& path)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int result = GetPersistentManager().LoadFileCompletelyThreaded(path, NULL, NULL, -1, false, (LoadProgress*)NULL);
	GetPersistentManager().IntegrateAllThreadedObjects ();

	return result;
}


#if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD
void PersistentManager::RemapInstanceIDOnLoad (const std::string& srcPath, LocalIdentifierInFileType srcLocalFileID, const std::string& dstPath, LocalIdentifierInFileType dstLocalFileID)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	SerializedObjectIdentifier src;
	SerializedObjectIdentifier dst;

	src.serializedFileIndex = InsertPathNameInternal (srcPath, true);
	src.localIdentifierInFile = srcLocalFileID;
	Assert(src.serializedFileIndex != -1);

	// Destination path needs to be inserted (in unity_web_old case)
	dst.serializedFileIndex = InsertPathNameInternal (dstPath, true);
	dst.localIdentifierInFile = dstLocalFileID;
	Assert(dst.serializedFileIndex != -1);
	
	m_InstanceIDRemap[src] = dst;
}
#endif // #if SUPPORT_INSTANCE_ID_REMAP_ON_LOAD



#if UNITY_EDITOR

void PersistentManager::SuggestFileIDToHeapIDs (const string& pathname, map<LocalIdentifierInFileType, SInt32>& fileIDToHeapIDHint)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	int serializedFileIndex = InsertPathNameInternal (pathname, true);

	SerializedFile* stream = GetSerializedFileInternal (serializedFileIndex);
	if (stream == NULL)
		return;

	vector<LocalIdentifierInFileType> fileIDs;
	stream->GetAllFileIDs (&fileIDs);
	
	for (vector<LocalIdentifierInFileType>::iterator i=fileIDs.begin ();i!=fileIDs.end ();++i)
	{
		LocalIdentifierInFileType fileID = *i;
		SerializedObjectIdentifier identifier (serializedFileIndex, fileID);
		
		// fileIDToHeapIDHint is a hint which is used to keep the same fileID when entering/exiting playmode for example.
		// It is only a hint and may not be fulfilled
		if (!m_Remapper->IsSetup(identifier))
		{
			if (fileIDToHeapIDHint.count(fileID))
			{
				int suggestedHeapID = fileIDToHeapIDHint.find(fileID)->second;
				if (Object::IDToPointer (suggestedHeapID) == NULL && !m_Remapper->IsHeapIDSetup(suggestedHeapID))
				{
					m_Remapper->SetupRemapping(suggestedHeapID, identifier);
				}
			}
		}
	}
}
#endif

void PersistentManager::GetLoadedInstanceIDsAtPath (const string& pathName, set<SInt32>* objects)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	AssertIf (objects == NULL);

	int serializedFileIndex = InsertPathNameInternal (pathName, false);
	if (serializedFileIndex != -1)
	{
		// Get all objects that were made persistent but might not already be written to the file
		m_Remapper->GetAllLoadedObjectsAtPath (serializedFileIndex, objects);
	}
}

void PersistentManager::GetPersistentInstanceIDsAtPath (const string& pathName, set<SInt32>* objects)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	AssertIf (objects == NULL);

	int pathID = InsertPathNameInternal (pathName, true);
	if (pathID == -1)
		return;

	// Get all objects that were made persistent but might not already be written to the file
	m_Remapper->GetAllPersistentObjectsAtPath (pathID, objects);
}

void PersistentManager::GetInstanceIDsAtPath (const string& pathName, set<SInt32>* objects)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	AssertIf (objects == NULL);

	int serializedFileIndex = InsertPathNameInternal (pathName, true);
	if (serializedFileIndex == -1)
		return;
	
	SerializedFile* serialize = GetSerializedFileInternal (serializedFileIndex);
	if (serialize)
	{
		// Get all objects in the file
		vector<LocalIdentifierInFileType> fileIDs;
		serialize->GetAllFileIDs (&fileIDs);
		for (vector<LocalIdentifierInFileType>::iterator i=fileIDs.begin ();i!=fileIDs.end ();++i)
		{
			SerializedObjectIdentifier identifier (serializedFileIndex, *i);
			SInt32 memoryID = m_Remapper->GetOrGenerateMemoryID (identifier);

			if (memoryID != 0)
				objects->insert (memoryID);
		}
	}

	// Get all objects that were made persistent but might not already be written to the file
	m_Remapper->GetAllLoadedObjectsAtPath (serializedFileIndex, objects);
}

void PersistentManager::GetInstanceIDsAtPath (const string& pathName, vector<SInt32>* objects)
{
	set<SInt32> temp;
	GetInstanceIDsAtPath(pathName, &temp);
	objects->assign(temp.begin(), temp.end());
}

int PersistentManager::CountInstanceIDsAtPath (const string& pathName)
{
	set<SInt32> objects;
	GetInstanceIDsAtPath (pathName, &objects);
	return objects.size ();
}

bool PersistentManager::IsObjectAvailable (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gIsObjectAvailable, NULL);

	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	//////////////////////// DO WE WANT THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (FindInActivationQueue(heapID))
		return true;

	SerializedObjectIdentifier identifier;

	if (!m_Remapper->InstanceIDToSerializedObjectIdentifier(heapID, identifier))
		return false;

	SerializedFile* stream = GetSerializedFileInternal (identifier.serializedFileIndex);
	// Stream can't be found
	if (stream == NULL)
		return false;
	
	if (!stream->IsAvailable (identifier.localIdentifierInFile))
		return false;

	// Check if the class can be produced
	int classID = stream->GetClassID (identifier.localIdentifierInFile);
	Object::RTTI* rtti = Object::ClassIDToRTTI (classID);
	if (rtti && !rtti->isAbstract)
		return true;
	else
		return false;
}

bool PersistentManager::IsObjectAvailableDontCheckActualFile (int heapID)
{
	PROFILER_AUTO_THREAD_SAFE(gIsObjectAvailable, NULL);
	
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	int serializedFileIndex = m_Remapper->GetSerializedFileIndex (heapID);
	if (serializedFileIndex == -1)
		return false;
	else
		return true;
}


void PersistentManager::Lock()
{
	LOCK_MUTEX(m_Mutex, gLoadLockPersistentManager);
}

void PersistentManager::Unlock()
{
	m_Mutex.Unlock();
}

void PersistentManager::DoneLoadingManagers() 
{	
	// We are done loading managers. Start instance IDs from a high constant value here,
	// so new managers and built-in resources can be added without changed instanceIDs 
	// used by the content.
	
	//AssertIf(m_Remapper->GetHighestInUseHeapID() > 10000);

	if (m_Remapper->m_HighestMemoryID < 10000)
	{
		m_Remapper->m_HighestMemoryID = 10000; 
	}

	Object::DoneLoadingManagers();
}


PersistentManager& GetPersistentManager ()
{
	AssertIf (gPersistentManager == NULL);
	return *gPersistentManager;
}

PersistentManager* GetPersistentManagerPtr ()
{
	return gPersistentManager;
}

void CleanupPersistentManager()
{
	UNITY_DELETE( gPersistentManager, kMemManager);
	gPersistentManager = NULL;
}

PersistentManager::PersistentManager (int options, int cacheCount)
// list node size is base data type size + two node pointers.
:
#if ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP
	m_ThreadedAwakeDataPool (false, "Remapper pool", sizeof (ThreadedAwakeData) + sizeof(void*)*2, 16 * 1024),
	m_ThreadedAwakeDataPoolMap (false, "RemapperMap pool", sizeof (ThreadedObjectActivationQueue::iterator) + sizeof(void*)*5, 16 * 1024),
	m_ThreadedObjectActivationQueue (m_ThreadedAwakeDataPool),
	m_ThreadedObjectActivationMap (std::less<SInt32>(), m_ThreadedAwakeDataPoolMap),
#endif
	m_AllowIntegrateThreadedObjectsWithTimeout (false)
{
	AssertIf (gPersistentManager);
	gPersistentManager = this;
	m_Options = options;
	m_CacheCount = cacheCount;
	m_Remapper = UNITY_NEW_AS_ROOT(Remapper (),kMemSerialization, kRemapperAllocArea, "");
	#if DEBUGMODE
	m_IsLoadingSceneFile = true;
	#endif
	#if DEBUGMODE
	m_PreventLoadingFromFile = -1;
	m_AllowLoadingFromDisk = true;
	#endif
	
	InitializeStdConverters ();
}

PersistentManager::~PersistentManager ()
{
	AssertIf(m_Mutex.IsLocked());
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	AssertIf (!m_ActiveNameSpace.empty ());
	
	for (StreamContainer::iterator i=m_Streams.begin ();i!=m_Streams.end ();++i)
		CleanupStream(*i);

	UNITY_DELETE(m_Remapper, kMemSerialization);
	CleanupStdConverters();
}

#if DEBUGMODE
void PersistentManager::SetDebugAssertLoadingFromFile (const std::string& path)
{
	if (path.empty())
	{
		m_PreventLoadingFromFile = -1;
	}
	else
	{
		AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
		m_PreventLoadingFromFile = InsertPathNameInternal (path, false);
	}
}
#endif


static bool IsPathBuiltinResourceFile (const std::string& pathName)
{
	return StrICmp(pathName, "library/unity default resources") == 0 || StrICmp(pathName, "library/unity_web_old") == 0
		|| StrICmp(pathName, "library/unity editor resources") == 0;
}

StreamNameSpace& PersistentManager::GetStreamNameSpaceInternal (int nameSpaceID)
{
#if DEBUGMODE
	AssertIf(m_PreventLoadingFromFile == nameSpaceID);
#endif
		
	StreamNameSpace& nameSpace = m_Streams[nameSpaceID]; 
	
	// Stream already loaded
	if (nameSpace.stream)
		return nameSpace;
	
	#if SUPPORT_SERIALIZATION_FROM_DISK
	
	PROFILER_AUTO_THREAD_SAFE(gLoadStreamNameSpaceProfiler, NULL);

	// Load Stream
	string pathName = PathIDToPathNameInternal (nameSpaceID);
	if (pathName.empty ())
		return nameSpace;

	// File not found
	string absolutePath = RemapToAbsolutePath (pathName);
	if (!IsFileCreated (absolutePath))
	{
		#if !UNITY_EDITOR
		AssertString("PersistentManager: Failed to open file at path: " + pathName);
		#endif
		return nameSpace;
	}
	
	// Is Builtin resource file?
	int options = 0;
	if (IsPathBuiltinResourceFile(pathName))
		options |= kIsBuiltinResourcesFile;

	nameSpace.stream = UNITY_NEW_AS_ROOT(SerializedFile, kMemSerialization, kSerializedFileArea, "");
	SET_ALLOC_OWNER(nameSpace.stream);
	#if ENABLE_MEM_PROFILER
	nameSpace.stream->SetDebugPath(pathName);
	GetMemoryProfiler()->SetRootAllocationObjectName(nameSpace.stream, nameSpace.stream->GetDebugPath().c_str());
	#endif
	
	// Resource image loading is only supported in the player!
	ResourceImageGroup group;
	#if SUPPORT_RESOURCE_IMAGE_LOADING
	for (int i=0;i<kNbResourceImages;i++)
	{
		string resourceImagePath = AppendPathNameExtension(absolutePath, kResourceImageExtensions[i]);
		if (IsFileCreated (resourceImagePath))
			group.resourceImages[i] = new ResourceImage(resourceImagePath, i == kStreamingResourceImage);
	}
	#endif
	
	if (!nameSpace.stream->InitializeRead (RemapToAbsolutePath (pathName), group, kCacheSize, m_CacheCount, options))
	{
		CleanupStream(nameSpace);
		return nameSpace;
	}

 	PostLoadStreamNameSpace(nameSpace, nameSpaceID);
#endif

	return m_Streams[nameSpaceID];
}

#if SUPPORT_SERIALIZATION_FROM_DISK
bool PersistentManager::LoadCachedFile (const std::string& pathName, const std::string actualAbsolutePath)
{
	PROFILER_AUTO_THREAD_SAFE(gLoadStreamNameSpaceProfiler, NULL);
	
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int nameSpaceID = InsertPathNameInternal (pathName, true);
	if (nameSpaceID == -1)
		return false;
	
	StreamNameSpace& nameSpace = m_Streams[nameSpaceID]; 
	
	// Stream already loaded
	if (nameSpace.stream)
		ErrorString ("Tryng to load a stream which is already loaded.");

	// File not found
	if (!IsFileCreated (actualAbsolutePath))
		return false;

	ResourceImageGroup group;
	nameSpace.stream = UNITY_NEW_AS_ROOT(SerializedFile,kMemSerialization, kSerializedFileArea, "");
	#if ENABLE_MEM_PROFILER
	nameSpace.stream->SetDebugPath(actualAbsolutePath);
	GetMemoryProfiler()->SetRootAllocationObjectName(nameSpace.stream, nameSpace.stream->GetDebugPath().c_str());
	#endif
	if (!nameSpace.stream->InitializeRead (actualAbsolutePath, group, kCacheSize, m_CacheCount, kSerializeGameRelease))
	{
		CleanupStream(nameSpace);
		return false;
	}

	nameSpace.stream->SetIsCachedFileStream(true);
 	PostLoadStreamNameSpace(nameSpace, nameSpaceID);
 	
	Mutex::AutoLock lock2 (m_MemoryLoadedOrCachedPathsMutex);
	m_MemoryLoadedOrCachedPaths.insert(pathName);
	
	return true;
}
#endif

bool PersistentManager::LoadMemoryBlockStream (const std::string& pathName, UInt8** data, int offset, int end, const char* url)
{
	PROFILER_AUTO_THREAD_SAFE(gLoadStreamNameSpaceProfiler, NULL);
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	
	int nameSpaceID = InsertPathNameInternal (pathName, true);
	if (nameSpaceID == -1)
		return false;

	StreamNameSpace& nameSpace = m_Streams[nameSpaceID]; 
	
	// Stream already loaded
	AssertIf (nameSpace.stream);

	nameSpace.stream = UNITY_NEW_AS_ROOT(SerializedFile,kMemSerialization, kSerializedFileArea, "");

#if ENABLE_MEM_PROFILER
	nameSpace.stream->SetDebugPath(url?url:pathName);
	GetMemoryProfiler()->SetRootAllocationObjectName(nameSpace.stream, nameSpace.stream->GetDebugPath().c_str());
#endif

	int options = kSerializeGameRelease;
	
	// In NaCl & Flash, default resources are not loaded from disk,
	// but also from a web stream.
	// Need to set the proper flag here, so they don't get unloaded
	// in Garbage Collection.
	if (IsPathBuiltinResourceFile(pathName))
		options |= kIsBuiltinResourcesFile;

	if (!nameSpace.stream->InitializeMemoryBlocks (RemapToAbsolutePath(pathName), data, end, offset, options))
	{
		CleanupStream(nameSpace);
		return false;
	}

 	PostLoadStreamNameSpace(nameSpace, nameSpaceID);
	
	m_MemoryLoadedOrCachedPathsMutex.Lock();
	m_MemoryLoadedOrCachedPaths.insert(pathName);
	m_MemoryLoadedOrCachedPathsMutex.Unlock();

	return true;
}


#if SUPPORT_SERIALIZATION_FROM_DISK
bool PersistentManager::LoadExternalStream (const std::string& pathName, const std::string& absolutePath, int flags, int readOffset)
{
	PROFILER_AUTO_THREAD_SAFE(gLoadStreamNameSpaceProfiler, NULL);
	
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int nameSpaceID = InsertPathNameInternal (pathName, true);
	if (nameSpaceID == -1)
		return false;
	
	StreamNameSpace& nameSpace = m_Streams[nameSpaceID]; 
	
	// Stream already loaded
	if (nameSpace.stream)
		return false;

	// File not found
	if (!IsFileCreated (absolutePath))
		return false;
	
	ResourceImageGroup group;
	nameSpace.stream = UNITY_NEW_AS_ROOT(SerializedFile,kMemSerialization, kSerializedFileArea, "");

#if ENABLE_MEM_PROFILER
	nameSpace.stream->SetDebugPath(absolutePath);
	GetMemoryProfiler()->SetRootAllocationObjectName(nameSpace.stream, nameSpace.stream->GetDebugPath().c_str());
#endif

	if (!nameSpace.stream->InitializeRead (absolutePath, group, kCacheSize, m_CacheCount, flags, readOffset))
	{
		CleanupStream(nameSpace);
		return false;
	}

	nameSpace.stream->SetIsCachedFileStream(true);
 	PostLoadStreamNameSpace(nameSpace, nameSpaceID);

	m_MemoryLoadedOrCachedPathsMutex.Lock();
	m_MemoryLoadedOrCachedPaths.insert(pathName);
	m_MemoryLoadedOrCachedPathsMutex.Unlock();

	return true;
}
#endif

void PersistentManager::PostLoadStreamNameSpace (StreamNameSpace& nameSpace, int nameSpaceID)
{
	nameSpace.highestID = std::max (nameSpace.highestID, nameSpace.stream->GetHighestID ());
	SET_ALLOC_OWNER ( this );
	const dynamic_block_vector<FileIdentifier>& externalRefs = nameSpace.stream->GetExternalRefs ();
	// Read all local pathnames and generate global<->localnamespace mapping	
	for (unsigned int i=0;i!=externalRefs.size ();i++)
	{
		int serializedFileIndex = InsertFileIdentifierInternal (externalRefs[i], true);
		m_GlobalToLocalNameSpace[nameSpaceID][serializedFileIndex] = i + 1;
		m_LocalToGlobalNameSpace[nameSpaceID][i + 1] = serializedFileIndex;
	}
	
	// Setup global to self namespace mapping	
	m_GlobalToLocalNameSpace[nameSpaceID][nameSpaceID] = 0;
	m_LocalToGlobalNameSpace[nameSpaceID][0] = nameSpaceID;
}

bool PersistentManager::IsFileEmpty (const string& pathName)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	SerializedFile* serialize = GetSerializedFileInternal (InsertPathNameInternal (pathName, true));
	if (serialize == NULL)
		return true;
	else
		return serialize->IsEmpty ();
}

#if UNITY_EDITOR
bool PersistentManager::DeleteFile (const string& pathName, DeletionFlags flag)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int globalNameSpace = InsertPathNameInternal (pathName, true);
	if (globalNameSpace == -1)
		return false;
	
	StreamNameSpace& stream = GetStreamNameSpaceInternal (globalNameSpace);

	set<SInt32> objectsInFile;
	GetInstanceIDsAtPath (pathName, &objectsInFile);

	for (set<SInt32>::iterator i=objectsInFile.begin ();i != objectsInFile.end ();++i)
		MakeObjectUnpersistent (*i, kDontDestroyFromFile);

	if (flag & kDeleteLoadedObjects)
	{
		for (set<SInt32>::iterator i=objectsInFile.begin ();i != objectsInFile.end ();++i)
		{
			Object* obj = Object::IDToPointer(*i);
			UnloadObject(obj);
		}
	
		#if DEBUGMODE
			for (set<SInt32>::iterator i=objectsInFile.begin ();i != objectsInFile.end ();++i)
				AssertIf (PPtr<Object> (*i));
		#endif
	}

	#if DEBUGMODE
	for (set<SInt32>::iterator i=objectsInFile.begin ();i != objectsInFile.end ();++i)
	{
		if (m_Remapper->GetSerializedFileIndex (*i) != -1)
		{
			Object* obj = PPtr<Object>(*i);
			ErrorStringObject("NOT CORRECTLY UNLOADED!!!!", obj);
		}
	}
	#endif

	if (stream.stream)
		CleanupStream(stream);

	m_GlobalToLocalNameSpace[globalNameSpace].clear ();
	m_LocalToGlobalNameSpace[globalNameSpace].clear ();		
	
	string absolutePath = RemapToAbsolutePath (pathName);
	if (IsFileCreated (absolutePath))
	{
		if (!::DeleteFile (absolutePath))
			return false;
	}
	return true;
}

#endif

void PersistentManager::UnloadNonDirtyStreams ()
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);
	// printf_console("------ Unloading non dirty stream\n");
	int dirtyStreams = 0;
	int loadedStreams = 0;
	int unloadedStreams = 0;
	for (int i=0;i<m_Streams.size ();i++)
	{
		StreamNameSpace& nameSpace = m_Streams[i];
		AssertIf (nameSpace.stream == NULL && !m_GlobalToLocalNameSpace[i].empty ());
		if (nameSpace.stream == NULL)
			continue;

		if (nameSpace.stream->IsMemoryStream () || nameSpace.stream->IsCachedFileStream())
		{
			loadedStreams++;
			continue;
		}
		
		bool unloadStream = true;
		#if UNITY_EDITOR
		unloadStream = !nameSpace.stream->IsFileDirty();
		#endif
		
		if (unloadStream)
		{
			unloadedStreams++;
			CleanupStream(nameSpace);
			m_GlobalToLocalNameSpace[i].clear ();
			m_LocalToGlobalNameSpace[i].clear ();
		}
		else
		{
			FileIdentifier identifier = PathIDToFileIdentifierInternal(i);
			printf_console("Can't unload serialized file because it is dirty: %s\n", identifier.pathName.c_str());
			dirtyStreams++;
			loadedStreams++;
		}
	}

	printf_console("Unloading %d Unused Serialized files (Serialized files now loaded: %d / Dirty serialized files: %d)\n", unloadedStreams, loadedStreams, dirtyStreams);
//	printf_console("Streams that can't be unloaded Files %d\n", loadedStreams);
//	printf_console("ID mapping count %d -- %d \n", m_Remapper->m_FileToHeapID.size(), m_Remapper->m_FileToHeapID.size() * 24);
//	printf_console("RESURRRECT count %d -- %d \n", m_Remapper->m_ResurrectHeapID.size(), m_Remapper->m_ResurrectHeapID.size() * 24);
}


void PersistentManager::UnloadStreams ()
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	for (int i=0;i<m_Streams.size ();i++)
	{
		StreamNameSpace& nameSpace = m_Streams[i];
		AssertIf (nameSpace.stream == NULL && !m_GlobalToLocalNameSpace[i].empty ());
		if (nameSpace.stream == NULL)
			continue;
/*		
		#if DEBUGMODE
		set<SInt32> debug;
		m_Remapper->GetAllLoadedObjectsAtPath (i, &debug);
		AssertIf (!debug.empty ());
		#endif
*/		
		CleanupStream(nameSpace);
		
		m_GlobalToLocalNameSpace[i].clear ();
		m_LocalToGlobalNameSpace[i].clear ();
	}
}

void PersistentManager::UnloadMemoryStreams ()
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	for (int i=0;i<m_Streams.size ();i++)
	{
		StreamNameSpace& nameSpace = m_Streams[i];
		AssertIf (nameSpace.stream == NULL && !m_GlobalToLocalNameSpace[i].empty ());
		if (nameSpace.stream == NULL )
			continue;

		if (!nameSpace.stream->IsMemoryStream () && !nameSpace.stream->IsCachedFileStream())
			continue;

		#if DEBUGMODE
		set<SInt32> debug;
		m_Remapper->GetAllLoadedObjectsAtPath (i, &debug);
		AssertIf (!debug.empty ());
		#endif

		CleanupStream(nameSpace);
		
		m_GlobalToLocalNameSpace[i].clear ();
		m_LocalToGlobalNameSpace[i].clear ();
	}
	
	Mutex::AutoLock lock2 (m_MemoryLoadedOrCachedPathsMutex);
	m_MemoryLoadedOrCachedPaths.clear();
}

void PersistentManager::UnloadStream (const std::string& pathName)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int nameSpaceID = InsertPathNameInternal (pathName, false);
	if (nameSpaceID == -1)
		return;
		
	StreamNameSpace& nameSpace = m_Streams[nameSpaceID]; 
	if (nameSpace.stream == NULL)
		return;

	CleanupStream(nameSpace);
	
	m_GlobalToLocalNameSpace[nameSpaceID].clear ();
	m_LocalToGlobalNameSpace[nameSpaceID].clear ();
	
	Mutex::AutoLock lock2 (m_MemoryLoadedOrCachedPathsMutex);
	m_MemoryLoadedOrCachedPaths.erase(pathName);
}

bool PersistentManager::HasMemoryOrCachedSerializedFile (const std::string& path)
{
	Mutex::AutoLock lock2 (m_MemoryLoadedOrCachedPathsMutex);
	return m_MemoryLoadedOrCachedPaths.count(path) == 1;
}

void PersistentManager::ResetHighestFileIDAtPath (const string& pathName)
{
	AQUIRE_AUTOLOCK(m_Mutex, gLoadLockPersistentManager);

	int globalNameSpace = InsertPathNameInternal (pathName, true);
	if (globalNameSpace == -1)
		return;
	
	AssertIf (m_Streams[globalNameSpace].stream != NULL);
	m_Streams[globalNameSpace].highestID = 0;
}

#if UNITY_EDITOR
// AwakeFromLoadQueue only supports this in the editor
void PersistentManager::RegisterSafeBinaryReadCallback (SafeBinaryReadCallbackFunction* callback)
{
	AssertIf (gSafeBinaryReadCallback);
	gSafeBinaryReadCallback = callback;
}
#endif

void PersistentManager::RegisterInOrderDeleteCallback (InOrderDeleteCallbackFunction* callback)
{
	AssertIf (gInOrderDeleteCallback);
	gInOrderDeleteCallback = callback;
}

SerializedFile* PersistentManager::GetSerializedFileInternal (const string& path)
{
	return GetSerializedFileInternal(InsertPathNameInternal (path, true));
}

SerializedFile* PersistentManager::GetSerializedFileInternal (int serializedFileIndex)
{
	if (serializedFileIndex == -1)
		return NULL;
	
	StreamNameSpace& stream = GetStreamNameSpaceInternal (serializedFileIndex);
	return stream.stream;
}


bool PersistentManager::IsStreamLoaded (const std::string& pathName)
{
	AQUIRE_AUTOLOCK_WARN_MAIN_THREAD(m_Mutex, gLoadLockPersistentManager);

	int globalNameSpace = InsertPathNameInternal (pathName, false);
	if (globalNameSpace == -1)
		return false;
	
	StreamNameSpace& nameSpace = m_Streams[globalNameSpace]; 
	return nameSpace.stream != NULL;
}

void PersistentManager::AddStream ()
{
	m_Streams.push_back (StreamNameSpace ());
	m_GlobalToLocalNameSpace.push_back (IDRemap ());
	m_LocalToGlobalNameSpace.push_back (IDRemap ());
}

void PersistentManager::DestroyFromFileInternal (int memoryID)
{
	SerializedObjectIdentifier identifier;
	m_Remapper->InstanceIDToSerializedObjectIdentifier(memoryID, identifier);
	SerializedFile* serialize = GetSerializedFileInternal (identifier.serializedFileIndex);
	if (serialize)
		serialize->DestroyObject (identifier.localIdentifierInFile);
}

string PersistentManager::RemapToAbsolutePath (const string& path)
{
	UserPathRemap::iterator found = m_UserPathRemap.find(path);
	if (found != m_UserPathRemap.end())
		return found->second;

	return PathToAbsolutePath(path);
}

void PersistentManager::SetPathRemap (const string& path, const string& absoluteRemappedPath)
{
	if (!absoluteRemappedPath.empty())
	{
		Assert (m_UserPathRemap.count(path) == 0);
		m_UserPathRemap.insert(make_pair(path, absoluteRemappedPath));
	}
	else
	{
		Assert (m_UserPathRemap.count(path) == 1);
		m_UserPathRemap.erase(path);
	}
}

#if UNITY_EDITOR
bool PersistentManager::IsClassNonTextSerialized( int cid )
{
	return m_NonTextSerializedClasses.find (cid) != m_NonTextSerializedClasses.end();
}
#endif
