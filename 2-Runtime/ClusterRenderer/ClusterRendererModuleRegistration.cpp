#include "UnityPrefix.h"
#if ENABLE_CLUSTER_SYNC
#include "ClusterRendererModule.h"

extern "C" EXPORT_MODULE void RegisterModule_ClusterRenderer ()
{
	InitializeClusterRendererModule ();
}

#endif