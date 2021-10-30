#include "UnityPrefix.h"
#if ENABLE_CLUSTER_SYNC
#include "ClusterRendererModule.h"
#include "ClusterNode.h"
#include "ClusterNetwork.h"
#include "ClusterTransfer.h"
#include "Runtime/Interfaces/IClusterRenderer.h"
#include "Runtime/Utilities/Argv.h"

using namespace std;

bool ClusterRendererModule::IsInClusterTestMode;

ClusterRendererModule::ClusterRendererModule()
: m_Node(NULL)
, m_Context(NULL)
{
	
}

ClusterRendererModule::~ClusterRendererModule()
{
	if(m_Node != NULL)
		delete m_Node;
	
	if(m_Context != NULL)
		DestroyZMQContext(m_Context);
}

void ClusterRendererModule::ProcessServerArgs(vector<string> args)
{
	m_Context = CreateZMQContext();
	ClusterTransfer* sync = new ClusterTransfer();
	MasterSocket* socket = new MasterSocket(m_Context, args[1].c_str(), args[2].c_str());
	m_Node = new MasterNode(socket, sync, atoi(args[0].c_str()));
}

void ClusterRendererModule::ProcessClientArgs(vector<string> args)
{
	m_Context = CreateZMQContext();
	ClusterTransfer* sync = new ClusterTransfer();
	SlaveSocket* socket = new SlaveSocket(m_Context, args[1].c_str(), args[2].c_str());
	m_Node = new SlaveNode(socket, sync, atoi(args[0].c_str()));
}


// TODO: user memory manager NEW
void ClusterRendererModule::InitCluster()
{
	AssertIf(m_Node != NULL);
	vector<string> values;
	
	if(HasARGV("server"))
	{
		ProcessServerArgs(GetValuesForARGV("server"));
	}
	else if(HasARGV("client"))
	{
		ProcessClientArgs(GetValuesForARGV("client"));
	}
	
#ifdef DEBUG
	IsInClusterTestMode = HasARGV("it");
	// output some logs to indicate we are ready to go
	// for IntegrationTest
	LogString("ClusterReady");
#endif
}


void ClusterRendererModule::SynchronizeCluster()
{
	// TODO check for NULL sockets
	if(m_Node!= NULL)
		m_Node->Sync();
}

bool ClusterRendererModule::IsMasterOfCluster()
{
	return (m_Node != NULL) ? m_Node->IsMaster() : true;
}

void ClusterRendererModule::ShutdownCluster()
{
	if(m_Node != NULL)
	{
		delete m_Node;
		m_Node = NULL;
	}
	
	if(m_Context != NULL)
	{
		DestroyZMQContext(m_Context);
	}
}

void InitializeClusterRendererModule ()
{
	SetIClusterRenderer(UNITY_NEW_AS_ROOT(ClusterRendererModule, kMemClusterRenderer, "ClusterRendererInterface", ""));
}

void CleanupClusterRendererModule ()
{
	ClusterRendererModule* module = reinterpret_cast<ClusterRendererModule*> (GetIClusterRenderer ());
	UNITY_DELETE(module, kMemClusterRenderer);
	SetIClusterRenderer (NULL);
}

#endif