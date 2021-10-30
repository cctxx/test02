#include "UnityPrefix.h"
#if ENABLE_CLUSTER_SYNC
#include "ClusterTransfer.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Serialize/TransferFunctions/StreamedBinaryWrite.h"
#include "Runtime/Serialize/TransferFunctions/StreamedBinaryRead.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Dynamics/PhysicsManager.h"

#ifdef DEBUG
#include "Runtime/Dynamics/Rigidbody.h"
#include "Runtime/Graphics/Transform.h"
#endif

template<class TransferFunc>
void TransferManagerStates (TransferFunc& transfer)
{
	// TODO: use macro incase manager is not present
	GetInputManager().ClusterTransfer(transfer);
    GetTimeManager().ClusterTransfer(transfer);
	GetPhysicsManager().ClusterTransfer(transfer);
}

void ClusterTransfer::TransferToBuffer(dynamic_array<UInt8>& buffer)
{
	StreamedBinaryWrite<false> writeStream;
	CachedWriter& writeCache = writeStream.Init (0, BuildTargetSelection::NoTarget());
	MemoryCacheWriter memoryCache (buffer);
	writeCache.InitWrite (memoryCache);
	// transfer stuff
	TransferManagerStates(writeStream);
	// end of transfer
	writeCache.CompleteWriting();
}

void ClusterTransfer::TransferFromBuffer(dynamic_array<UInt8>& buffer)
{
	// transfer setup
	StreamedBinaryRead<false> readStream;
	CachedReader& readCache = readStream.Init (0);
	MemoryCacheReader memoryCache (buffer);
	readCache.InitRead (memoryCache, 0, buffer.size());
	// transfer stuff
	TransferManagerStates(readStream);
	// end of transfer
	readCache.End();
}

#ifdef DEBUG
void ClusterTransfer::TransferToFile(int slaveId)
{
	// create the buffer
	dynamic_array<UInt8> buffer(kMemTempAlloc);
	
	// create the stream
	StreamedBinaryWrite<false> writeStream;
	CachedWriter& writeCache = writeStream.Init (0, BuildTargetSelection::NoTarget());
	MemoryCacheWriter memoryCache (buffer);
	writeCache.InitWrite (memoryCache);
	
	// now write the data out to a file
	for (int level=0;level<PhysicsManager::kMaxSortedActorsDepth;level++)
	{
		PhysicsManager::RigidbodyList& bodies = GetPhysicsManager().m_SortedActors[level];
		for (PhysicsManager::RigidbodyList::iterator i=bodies.begin();i != bodies.end();i++)
		{
			Rigidbody& body = **i;
			
			if (body.m_DisableReadUpdateTransform == 0)
			{
				GameObject& go = body.GetGameObject();
				Transform& transform = go.GetComponent (Transform);
				transform.Transfer(writeStream);
			}
		}
	}
	
	// end of transfer
	writeCache.CompleteWriting();
	
	
	// now we have the buffer, we put it into a file, and compare that with other slaves.
	char fileName[32];
	sprintf(fileName, "dump%d.dat\0", slaveId);
	FILE* dump = fopen(fileName, "wb");
	fwrite(buffer.data(), sizeof(UInt8), buffer.size(), dump);
	fclose(dump);
}
#endif // DEBUG

#endif