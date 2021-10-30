#pragma once
#if ENABLE_CLUSTER_SYNC

class MasterSocket;
class SlaveSocket;
class ClusterTransfer;

class ClusterNode
{
public:
	virtual ~ClusterNode() {}
	virtual void Sync() = 0;
	bool IsMaster() { return m_Master; }
	
protected:
	ClusterNode(bool master)
	: m_Master(master) {}
	
private:
	bool m_Master;
};


class MasterNode : public ClusterNode
{
public:
	MasterNode(MasterSocket* socket, ClusterTransfer* sync, int param)
	: ClusterNode(true)
	, m_Socket(socket)
	, m_Synchronizer(sync)
	, m_AllSlavesConnected(false)
	, m_InitialSlaveCount(param)
	, m_CurrentSlaveCount(param) {}
	~MasterNode();
	
	virtual void Sync();
	
private:
	void WaitForSlavesToConnect();
	void WaitForSlavesToAck();
	
	MasterSocket* m_Socket;
	ClusterTransfer* m_Synchronizer;
	bool m_AllSlavesConnected;
	int m_InitialSlaveCount;
	int m_CurrentSlaveCount;
};


class SlaveNode : public ClusterNode
{
public:
	SlaveNode(SlaveSocket* socket, ClusterTransfer* sync, int param);
	~SlaveNode();
	
	virtual void Sync();
	
private:
	int m_SlaveId;
	SlaveSocket* m_Socket;
	ClusterTransfer* m_Synchronizer;
};

#endif