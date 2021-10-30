#pragma once
#if ENABLE_CLUSTER_SYNC
#include "Runtime/Utilities/NonCopyable.h"

class EXPORT_COREMODULE IClusterRenderer : public NonCopyable
{
public:
	virtual void InitCluster() = 0;
	virtual void SynchronizeCluster() = 0;
	virtual bool IsMasterOfCluster() = 0;
	virtual void ShutdownCluster() = 0;
};

EXPORT_COREMODULE IClusterRenderer* GetIClusterRenderer();
EXPORT_COREMODULE void SetIClusterRenderer(IClusterRenderer* value);
#endif