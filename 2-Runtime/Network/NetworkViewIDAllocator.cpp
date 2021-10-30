#include "UnityPrefix.h"
#include "NetworkViewIDAllocator.h"

#if ENABLE_NETWORK

NetworkViewIDAllocator::NetworkViewIDAllocator()
{
	Clear(kDefaultViewIDBatchSize, kMinimumViewIDs, 0, kUndefindedPlayerIndex);
}

UInt32 NetworkViewIDAllocator::AllocateBatch (NetworkPlayer player)
{
	UInt32 batchIndex = m_AllocatedViewIDBatches.size();
	m_AllocatedViewIDBatches.push_back(player);
	return batchIndex;
}

NetworkViewID NetworkViewIDAllocator::AllocateViewID ()
{
	if (!m_AvailableBatches.empty())
	{
		AvailableBatch& batch = m_AvailableBatches.front();

		NetworkViewID viewID;
		viewID.SetAllocatedID(batch.first);		
		batch.first++;
		batch.count--;
		if (batch.count == 0)
			m_AvailableBatches.erase(m_AvailableBatches.begin());
		
		return viewID;
	}
	else
	{
		return NetworkViewID();
	}
}


NetworkPlayer NetworkViewIDAllocator::FindOwner (NetworkViewID viewID)
{
	if (viewID.IsSceneID())
	{
		return m_ServerPlayer;
	}
	else
	{
		UInt32 index = viewID.GetIndex();
		index /= m_BatchSize;

		// On Clients we use received batches. On the client we can only find out if the batch is ours.
		// Otherwise it defaults to the server
		if (!m_ReceivedBatches.empty())
		{
			for (ReceivedBatches::iterator i=m_ReceivedBatches.begin();i != m_ReceivedBatches.end();i++)
			{
				if (*i == index)
					return m_ClientPlayer;
			}

			return m_ServerPlayer;
		}
		// The Server allocates all network view id's so he knows all players for all view ids
		else
		{
			if (index < m_AllocatedViewIDBatches.size())
				return m_AllocatedViewIDBatches[index];
			else
				return kUndefindedPlayerIndex;
		}

		return 0;
	}
}

void NetworkViewIDAllocator::FeedAvailableBatchOnClient (UInt32 batchIndex)
{
	m_ReceivedBatches.push_back(batchIndex);
	
	AvailableBatch batch;
	batch.first = batchIndex * m_BatchSize;
	batch.count = m_BatchSize;
	m_AvailableBatches.push_back(batch);
}

void NetworkViewIDAllocator::FeedAvailableBatchOnServer (UInt32 batchIndex)
{
	AvailableBatch batch;
	batch.first = batchIndex * m_BatchSize;
	batch.count = m_BatchSize;
	if (batchIndex == 0)
	{
		batch.first++;
		batch.count--;
	}
	m_AvailableBatches.push_back(batch);
}

int NetworkViewIDAllocator::ShouldRequestMoreBatches ()
{
	// Count how many id's we have left
	int viewIDsLeft = 0;
	for (int i=0;i<m_AvailableBatches.size();i++)
		viewIDsLeft += m_AvailableBatches[i].count;
	
	// Do we need to request new id batches
	viewIDsLeft += m_BatchSize * m_RequestedBatches;
	if (viewIDsLeft < m_MinAvailableViewIDs)
	{
		int extraRequiredViewIDs = m_MinAvailableViewIDs - viewIDsLeft;
		int requestedBatches = ((extraRequiredViewIDs -1) / m_BatchSize) + 1;
		return requestedBatches;
	}
	
	return 0;
}

void NetworkViewIDAllocator::Clear (int batchSize, int minimumViewIDs, NetworkPlayer server, NetworkPlayer client)
{
	m_MinAvailableViewIDs = minimumViewIDs;
	m_BatchSize = batchSize;
	m_AllocatedViewIDBatches.clear();
	m_AvailableBatches.clear();
	m_ReceivedBatches.clear();
	m_RequestedBatches = 0;
	m_ClientPlayer = client;
	m_ServerPlayer = server;
}

#endif