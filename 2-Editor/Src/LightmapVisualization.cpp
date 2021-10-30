#include "UnityPrefix.h"
#include "LightmapVisualization.h"

LightmapVisualization* g_LightmapVisualization = NULL;

LightmapVisualization& GetLightmapVisualization()
{
	if (!g_LightmapVisualization)
		g_LightmapVisualization = new LightmapVisualization();
	return *g_LightmapVisualization;
}

LightmapVisualization::LightmapVisualization()
: m_Enabled(false),
m_ShadowDistance(100.0f),
m_UseLightmaps(true),
m_ShowResolution(false)
{
}