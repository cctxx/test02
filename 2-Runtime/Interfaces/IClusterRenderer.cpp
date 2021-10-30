#include "IClusterRenderer.h"
#if ENABLE_CLUSTER_SYNC
static IClusterRenderer* gIClusterRenderer = NULL;

IClusterRenderer* GetIClusterRenderer()
{
	return gIClusterRenderer;
}

void SetIClusterRenderer(IClusterRenderer* value)
{
	gIClusterRenderer = value;
}
#endif