#include "UnityPrefix.h"
#include "ITerrainManager.h"

static ITerrainManager* gTerrainManager;

ITerrainManager* GetITerrainManager ()
{
	return gTerrainManager;
}

void SetITerrainManager (ITerrainManager* manager)
{
	gTerrainManager = manager;
}
