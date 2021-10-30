#include "UnityPrefix.h"
#if ENABLE_CLUSTER_SYNC
#include "ClusterNode.h"
#include "ClusterNetwork.h"
#include "ClusterTransfer.h"

#ifdef DEBUG
#include "Runtime/Input/InputManager.h"
#include "ClusterRendererModule.h"
#endif

MasterNode::~MasterNode()
{
	delete m_Socket;
	delete m_Synchronizer;
}


void MasterNode::Sync()
{
#ifdef DEBUG
	if(ClusterRendererModule::IsInClusterTestMode)
	{
		static int count = 0;
		if(count++ == 400)
		{
			GetInputManager().QuitApplication();
			return;
		}
	}
#endif
	
	// this may early out
	WaitForSlavesToConnect();
	
	// the write buffer
	dynamic_array<UInt8> buffer(kMemTempAlloc);
	// Transfer
	m_Synchronizer->TransferToBuffer(buffer);
	// in the end, send out the buffer via zmq
	m_Socket->Publish(buffer);
	
	// sync up, this should block
	WaitForSlavesToAck();
}


void MasterNode::WaitForSlavesToConnect()
{
	// don't try if we have all initialy connected
	if(m_AllSlavesConnected) return;
	// check and wait for each slave
	for(int i = 0; i < m_InitialSlaveCount;)
	{
		// This will block
		if(m_Socket->WaitForSubscriber())
			i++;
	}
	// only do it once
	m_AllSlavesConnected = true;
}


void MasterNode::WaitForSlavesToAck()
{
	int checkedInCount = 0;
	// while there are slaves unaccounted for
	while((m_CurrentSlaveCount - checkedInCount) > 0)
	{
		// check for unsubscription
		if(m_Socket->CheckForUnsubscribe())
			m_CurrentSlaveCount--;
		
		// check control signal
		if(m_Socket->GetAck())
			checkedInCount++;
	}
}


SlaveNode::SlaveNode(SlaveSocket* socket, ClusterTransfer* sync, int param)
: ClusterNode(false)
, m_Socket(socket)
, m_Synchronizer(sync)
, m_SlaveId(param)
{
	// should this be here?
	m_Socket->Subscribe(m_SlaveId);
}


SlaveNode::~SlaveNode()
{
	// send the ubsubscribe signal
	m_Socket->Unsubscribe(m_SlaveId);
	// destroy stuff
	delete m_Socket;
	delete m_Synchronizer;
}


void SlaveNode::Sync()
{
#ifdef DEBUG
	if(ClusterRendererModule::IsInClusterTestMode)
	{
		static int count = 0;
		if(count++ == 300)
		{
			m_Synchronizer->TransferToFile(m_SlaveId);
			GetInputManager().QuitApplication();
			return;
		}
	}
#endif
	
	// TODO: fix potential buffer overflow
	// the read buffer
	dynamic_array<UInt8> buffer(4096, kMemTempAlloc);
	// wait for the server to send the buffer over
	m_Socket->Listen(buffer);
	// send control signal
	m_Socket->SendAck();
	// use the data
	m_Synchronizer->TransferFromBuffer(buffer);
}

#endif