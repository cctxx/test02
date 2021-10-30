#ifndef REMAPPER_H
#define REMAPPER_H

#include <limits>
#include "Runtime/Utilities/MemoryPool.h"
#include "Configuration/UnityConfigure.h"

class Remapper
{
	public:
#if ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP
	MemoryPool      m_SerializedObjectIdentifierPool;
	typedef std::map<SerializedObjectIdentifier, SInt32, std::less<SerializedObjectIdentifier>, memory_pool_explicit<std::pair<const SerializedObjectIdentifier, SInt32> > > FileToHeapIDMap;
	typedef std::map<SInt32, SerializedObjectIdentifier, std::less<SInt32>, memory_pool_explicit<std::pair<const SInt32, SerializedObjectIdentifier> > > HeapIDToFileMap;
#else
	typedef std::map<SerializedObjectIdentifier, SInt32, std::less<SerializedObjectIdentifier> > FileToHeapIDMap;
	typedef std::map<SInt32, SerializedObjectIdentifier, std::less<SInt32> > HeapIDToFileMap;
#endif

	typedef FileToHeapIDMap::iterator FileToHeapIDIterator;
	typedef HeapIDToFileMap::iterator HeapIDToFileIterator;

	FileToHeapIDMap	m_FileToHeapID;
	HeapIDToFileMap	m_HeapIDToFile;
	#if UNITY_EDITOR
	vector_set<SInt32> m_LoadingSceneInstanceIDs;
	#endif
	

	
	// Instance ID's are simply allocated in an increasing index
	int 											m_HighestMemoryID;
	
	// When loading scenes we can fast path because objects are not kept persistent / unloaded / loaded again etc.
	// So we just preallocate a bunch of id's and use those without going through a lot of table lookups.
	int                                             m_ActivePreallocatedIDBase;
	int                                             m_ActivePreallocatedIDEnd;
	int                                             m_ActivePreallocatedPathID;

	Remapper () 
// map node contains 3 pointers (left, right, parent)

#if ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP
: 	m_SerializedObjectIdentifierPool( false, "Remapper pool", sizeof (SerializedObjectIdentifier) + sizeof(SInt32)*2 + sizeof(void*)*3, 16 * 1024)
,	m_FileToHeapID (std::less<SerializedObjectIdentifier> (), m_SerializedObjectIdentifierPool)
,	m_HeapIDToFile (std::less<SInt32>  (), m_SerializedObjectIdentifierPool)
#endif
	{ m_HighestMemoryID = 0; m_ActivePreallocatedIDBase = 0; m_ActivePreallocatedIDEnd = 0; m_ActivePreallocatedPathID = -1;  }
	
	
	void PreallocateIDs (LocalIdentifierInFileType highestFileID, int pathID)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		AssertIf(pathID == -1);
		m_HighestMemoryID += 2;
		m_ActivePreallocatedIDBase = m_HighestMemoryID;
		m_HighestMemoryID  += highestFileID * 2;
		m_ActivePreallocatedIDEnd = m_HighestMemoryID;
		//printf_console("Preallocating %d .. %d\n", m_ActivePreallocatedIDBase, m_ActivePreallocatedIDEnd);
		m_ActivePreallocatedPathID = pathID;
	}

	void ClearPreallocateIDs ()
	{
		AssertIf(m_ActivePreallocatedPathID == -1);
		m_ActivePreallocatedIDBase = 0;
		m_ActivePreallocatedIDEnd = 0;
		m_ActivePreallocatedPathID = -1;
	}

	void Remove (int memoryID)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		
		HeapIDToFileIterator i = m_HeapIDToFile.find (memoryID);
		if (i == m_HeapIDToFile.end ())
			return;

		FileToHeapIDIterator j = m_FileToHeapID.find (i->second);
		AssertIf (j == m_FileToHeapID.end ());
		SerializedObjectIdentifier bug = j->first;
		
		m_HeapIDToFile.erase (i);
		m_FileToHeapID.erase (j);
		AssertIf (m_FileToHeapID.find (bug) != m_FileToHeapID.end ());
	}

	void RemoveCompletePathID (int serializedFileIndex, vector<SInt32>& objects)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);

		SerializedObjectIdentifier proxy;
		proxy.serializedFileIndex = serializedFileIndex;
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::min();
		
		FileToHeapIDIterator begin = m_FileToHeapID.lower_bound (proxy);
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::max();
		FileToHeapIDIterator end = m_FileToHeapID.upper_bound (proxy);
		for (FileToHeapIDIterator i=begin;i != end;i++)
		{
			ErrorIf(i->first.serializedFileIndex != serializedFileIndex);
			m_HeapIDToFile.erase (m_HeapIDToFile.find(i->second));
			objects.push_back(i->second);
		}
		m_FileToHeapID.erase(begin, end);
	}
	
	bool IsSceneID (int memoryID)
	{
#if UNITY_EDITOR
		return m_LoadingSceneInstanceIDs.count(memoryID) != 0;
#endif		
		return IsPreallocatedID (memoryID);
	}
	
	bool IsPreallocatedID (int memoryID)
	{
		return m_ActivePreallocatedPathID != -1 && memoryID >= m_ActivePreallocatedIDBase && memoryID <= m_ActivePreallocatedIDEnd;
	}
	
	bool InstanceIDToSerializedObjectIdentifier (int instanceID, SerializedObjectIdentifier& identifier)
	{
		if (IsPreallocatedID(instanceID))
		{
			identifier.serializedFileIndex = m_ActivePreallocatedPathID;
			identifier.localIdentifierInFile = (instanceID - m_ActivePreallocatedIDBase) / 2;
			return true;
		}
		
		HeapIDToFileIterator i = m_HeapIDToFile.find (instanceID);
		if (i == m_HeapIDToFile.end ())
		{
			identifier.serializedFileIndex = -1;
			identifier.localIdentifierInFile = 0;
			return false;
		}
		identifier = i->second;
		
		#if LOCAL_IDENTIFIER_IN_FILE_SIZE != 32	
		-- fix this, should we use UInt32 for localIdentifierInFile?
			USInt64 debugLocalIdentifier = identifier.localIdentifierInFile;
		AssertIf(debugLocalIdentifier >= (1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE) || debugLocalIdentifier <= -(1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE));
		#endif
		
		return true;
	}

	
	int GetSerializedFileIndex (int memoryID)
	{
		SerializedObjectIdentifier identifier;
		InstanceIDToSerializedObjectIdentifier (memoryID, identifier);
		return identifier.serializedFileIndex;
	}
	
	bool IsSetup (const SerializedObjectIdentifier& identifier)
	{
		return m_FileToHeapID.find (identifier) != m_FileToHeapID.end ();
	}

	bool IsHeapIDSetup (int memoryID)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		return m_HeapIDToFile.count(memoryID);
	}

	int GetOrGenerateMemoryID (const SerializedObjectIdentifier& identifier)
	{
		if (identifier.serializedFileIndex == -1)
			return 0;
		
		if (m_ActivePreallocatedPathID != -1 && m_ActivePreallocatedPathID == identifier.serializedFileIndex)
		{
			return identifier.localIdentifierInFile * 2 + m_ActivePreallocatedIDBase;
		}
		
		#if LOCAL_IDENTIFIER_IN_FILE_SIZE != 32	
		-- fix this, should we use UInt32 for localIdentifierInFile?
		USInt64 debugLocalIdentifier = identifier.localIdentifierInFile;
		AssertIf(debugLocalIdentifier >= (1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE) || debugLocalIdentifier <= -(1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE));
		#endif
		std::pair<FileToHeapIDIterator, bool> inserted = m_FileToHeapID.insert (std::make_pair (identifier, 0));
		if (inserted.second)
		{
			int memoryID = 0;
			
			m_HighestMemoryID += 2;
			memoryID = m_HighestMemoryID;
		
			inserted.first->second = memoryID;

			AssertIf (m_HeapIDToFile.find (memoryID) != m_HeapIDToFile.end ());
			m_HeapIDToFile.insert (std::make_pair (memoryID, identifier));
			
			return memoryID;
		}
		else
			return inserted.first->second;
	}
	
	void SetupRemapping (int memoryID, const SerializedObjectIdentifier& identifier)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		#if LOCAL_IDENTIFIER_IN_FILE_SIZE != 32	
		-- fix this, should we use UInt32 for localIdentifierInFile?
			USInt64 debugLocalIdentifier = identifier.localIdentifierInFile;
		AssertIf(debugLocalIdentifier >= (1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE) || debugLocalIdentifier <= -(1ULL << LOCAL_IDENTIFIER_IN_FILE_SIZE));
		#endif
		
		if (m_HeapIDToFile.find (memoryID) != m_HeapIDToFile.end ())
		{
			m_FileToHeapID.erase(m_HeapIDToFile.find (memoryID)->second);
			m_HeapIDToFile.erase(memoryID);
		}

		if (m_FileToHeapID.find (identifier) != m_FileToHeapID.end ())
		{
			m_HeapIDToFile.erase(m_FileToHeapID.find (identifier)->second);
			m_FileToHeapID.erase(identifier);
		}

		m_HeapIDToFile[memoryID] = identifier;
		m_FileToHeapID[identifier] = memoryID;
		
/*
//		This code asserts more when something goes wrong but also in edge cases that are allowed.
		SerializedObjectIdentifier id;
		id.fileID = fileID;
		id.pathID = pathID;

		HeapIDToFileIterator inserted;
		inserted = m_HeapIDToFile.insert (std::make_pair (memoryID, id)).first;
		AssertIf (inserted->second != id);
		inserted->second = id;
				
		FileToHeapIDIterator inserted2;
		#if DEBUGMODE
		inserted2 = m_FileToHeapID.find (id);
		AssertIf (inserted2 != m_FileToHeapID.end () && inserted2->second != memoryID);
		#endif
		
		inserted2 = m_FileToHeapID.insert (std::make_pair (id, memoryID)).first;
		AssertIf (inserted2->second != memoryID);
		inserted2->second = memoryID;
*/
	}
	
	void GetAllLoadedObjectsAtPath (int pathID, set<SInt32>* objects)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		AssertIf (objects == NULL);

		SerializedObjectIdentifier proxy;
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::min ();
		proxy.serializedFileIndex = pathID;
		FileToHeapIDIterator begin = m_FileToHeapID.lower_bound (proxy);
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::max ();
		FileToHeapIDIterator end = m_FileToHeapID.upper_bound (proxy);
		
		for (FileToHeapIDIterator i=begin;i != end;++i)
		{
			int instanceID = i->second;
			Object* o = Object::IDToPointer (instanceID);
			if (o)
				objects->insert (instanceID);
		}
	}

	void GetAllPersistentObjectsAtPath (int pathID, set<SInt32>* objects)
	{
		AssertIf(m_ActivePreallocatedPathID != -1);
		AssertIf (objects == NULL);
		
		SerializedObjectIdentifier proxy;
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::min ();
		proxy.serializedFileIndex = pathID;
		FileToHeapIDIterator begin = m_FileToHeapID.lower_bound (proxy);
		proxy.localIdentifierInFile = std::numeric_limits<LocalIdentifierInFileType>::max ();
		FileToHeapIDIterator end = m_FileToHeapID.upper_bound (proxy);
		
		for (FileToHeapIDIterator i=begin;i != end;++i)
		{
			int instanceID = i->second;
			objects->insert(instanceID);
		}
	}

	int GetHighestInUseHeapID ()
	{
		if (!m_HeapIDToFile.empty())
			return m_HeapIDToFile.rbegin ()->first;
		else
			return 0;	
	}
};

#endif
