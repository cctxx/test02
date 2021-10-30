#include "UnityPrefix.h"
#if ENABLE_CLUSTER_SYNC

#include "ClusterNetwork.h"
#include "External/zmq/include/zmq.h"

void* CreateZMQContext()
{
	return zmq_ctx_new();
}


void DestroyZMQContext(void* context)
{
	zmq_term(context);
}


MasterSocket::MasterSocket(void* context, const char* mainUrl, const char* ctrlUrl)
: m_Context(context)
{
	// create them
	m_MainSocket = zmq_socket(m_Context, ZMQ_XPUB);
	m_CtrlSocket = zmq_socket(m_Context, ZMQ_PULL);
	// bind them
	zmq_bind(m_MainSocket, mainUrl);
	zmq_bind(m_CtrlSocket, ctrlUrl);
}


MasterSocket::~MasterSocket()
{
	if(m_MainSocket)
		zmq_close(m_MainSocket);
	if(m_CtrlSocket)
		zmq_close(m_CtrlSocket);
}


void MasterSocket::Publish(dynamic_array<UInt8>& buffer)
{
	zmq_send(m_MainSocket, buffer.data(), buffer.size(), 0);
}


bool MasterSocket::WaitForSubscriber()
{
	UInt8 buffer[3];
	int rc = zmq_recv(m_MainSocket, &buffer, 3, 0);
	return (rc == 3 && buffer[0] == 1 && buffer[1] == 'S');
}


bool MasterSocket::CheckForUnsubscribe()
{
	UInt8 buffer[3];
	int rc = zmq_recv(m_MainSocket, &buffer, 3, ZMQ_DONTWAIT);
	return (rc == 3 && buffer[0] == 0 && buffer[1] == 'S');
}


bool MasterSocket::GetAck()
{
	UInt8 buffer[1];
	int rc = zmq_recv(m_CtrlSocket, &buffer, 1, ZMQ_DONTWAIT);
	return (rc == 1 && buffer[0] == 1);
}


SlaveSocket::SlaveSocket(void* context, const char* mainUrl, const char* ctrlUrl)
: m_Context(context)
{
	// create them
	m_MainSocket = zmq_socket(m_Context, ZMQ_XSUB);
	m_CtrlSocket = zmq_socket(m_Context, ZMQ_PUSH);
	// connect them
	zmq_connect(m_MainSocket, mainUrl);
	zmq_connect(m_CtrlSocket, ctrlUrl);
}


SlaveSocket::~SlaveSocket()
{
	if(m_MainSocket)
		zmq_close(m_MainSocket);
	if(m_CtrlSocket)
		zmq_close(m_CtrlSocket);
}


void SlaveSocket::Listen(dynamic_array<UInt8>& buffer)
{
	zmq_recv(m_MainSocket, buffer.data(), buffer.size(), 0);
}


void SlaveSocket::Subscribe(int slaveId)
{
	UInt8 buffer[] = {1, 'S', slaveId};
	zmq_send(m_MainSocket, buffer, 3, 0);
	zmq_send(m_MainSocket, buffer, 1, 0);
}


void SlaveSocket::Unsubscribe(int slaveId)
{
	UInt8 buffer[] = {0, 'S', slaveId};
	zmq_send(m_MainSocket, buffer, 3, 0);
}


void SlaveSocket::SendAck()
{
	UInt8 buffer[] = {1};
	zmq_send(m_CtrlSocket, buffer, 1, 0);
}

#endif