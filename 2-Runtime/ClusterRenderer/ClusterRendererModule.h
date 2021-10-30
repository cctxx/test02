#pragma once
#if ENABLE_CLUSTER_SYNC
#include "Runtime/Interfaces/IClusterRenderer.h"

class ClusterNode;

using namespace std;

class ClusterRendererModule : public IClusterRenderer
{
public:
	ClusterRendererModule();
	virtual ~ClusterRendererModule();
	virtual void InitCluster();
	virtual void SynchronizeCluster();
	virtual bool IsMasterOfCluster();
	virtual void ShutdownCluster();
private:
	void ProcessServerArgs(vector<string> args);
	void ProcessClientArgs(vector<string> args);
	ClusterNode* m_Node;
	void* m_Context;
#ifdef DEBUG
public:
	static bool IsInClusterTestMode;
#endif

};

void InitializeClusterRendererModule ();
void CleanupClusterRendererModule ();
#endif