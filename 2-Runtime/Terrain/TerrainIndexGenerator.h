#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

struct TerrainIndexGenerator 
{
	static unsigned int *GetIndexBuffer (int edgeMask, unsigned int &count, int stride);
	static unsigned short *GetOptimizedIndexStrip (int edgeMask, unsigned int &count);
};

#endif // ENABLE_TERRAIN
