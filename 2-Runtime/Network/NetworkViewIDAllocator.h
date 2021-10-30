#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK
#include "NetworkEnums.h"

class NetworkViewIDAllocator
{

	struct AvailableBatch
	{
		UInt32 first;
		UInt32 count;
	};
	
	typedef	std::vector<NetworkPlayer> AllocatedViewIDBatches;
	AllocatedViewIDBatches   m_AllocatedViewIDBatches; // Used by the server to track who own which NetworkViewID's

	typedef std::vector<UInt32> ReceivedBatches;
	ReceivedBatches              m_ReceivedBatches; // Used by the client to track which batches were received by him.

	typedef std::vector<AvailableBatch> AvailableBatches;
	AvailableBatches  m_AvailableBatches; // Used by client/server to allocate ViewID's from

	int                          m_BatchSize;
	int                          m_MinAvailableViewIDs; // We always make sure that m_MinAvailableViewIDs are around. If not we request more view id's
	int                          m_RequestedBatches;
	NetworkPlayer 				 m_ClientPlayer;
	NetworkPlayer 				 m_ServerPlayer;
	
	public:
	
	NetworkViewIDAllocator();
	
	void Clear (int batchSize, int minimumViewIDs, NetworkPlayer server, NetworkPlayer client);
	
	NetworkViewID AllocateViewID ();
	
	UInt32 AllocateBatch (NetworkPlayer player);
	
	void FeedAvailableBatchOnClient (UInt32 batchIndex);
	void FeedAvailableBatchOnServer (UInt32 batchIndex);
	UInt32 GetBatchSize () { return m_BatchSize; }
	
	// How many more view id batches should be requested!
	// You are expected to actually request or allocate those view id's
	int ShouldRequestMoreBatches ();
	void AddRequestedBatches (int requestedBatches) { m_RequestedBatches += requestedBatches; }
	
	void SetMinAvailableViewIDs(int size) { m_MinAvailableViewIDs = size; }
	
	/// On Server: Find Owner returns the player who allocated the view id. If it hasn't been allocated, it returns kUndefindedPlayerIndex.
	/// On Clients: FindOwner returns the clients player ID for its own objects and otherwise the server, since he can't possibly know the owner.
	NetworkPlayer FindOwner (NetworkViewID viewID);
};

#endif