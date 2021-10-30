#pragma once
#if ENABLE_CLUSTER_SYNC
#include "Runtime/Utilities/dynamic_array.h"

void* CreateZMQContext();
void DestroyZMQContext(void* context);

class MasterSocket
{
public:
	MasterSocket(void* context, const char* mainUrl, const char* ctrlUrl);
	~MasterSocket();
	void Publish(dynamic_array<UInt8>& buffer);
	bool WaitForSubscriber();
	bool CheckForUnsubscribe();
	bool GetAck();
private:
	void* m_Context;
	void* m_MainSocket;
	void* m_CtrlSocket;
};


class SlaveSocket
{
public:
	SlaveSocket(void* context, const char* mainUrl, const char* ctrlUrl);
	~SlaveSocket();
	void Listen(dynamic_array<UInt8>& buffer);
	void Subscribe(int slaveId);
	void Unsubscribe(int slaveId);
	void SendAck();
private:
	void* m_Context;
	void* m_MainSocket;
	void* m_CtrlSocket;
};


#endif