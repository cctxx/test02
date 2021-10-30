#pragma once
#include "Runtime/BaseClasses/ObjectDefines.h"

class LightmapVisualization {
public:
	LightmapVisualization();
	~LightmapVisualization(){};
	
	GET_SET(bool, Enabled, m_Enabled);
	GET_SET(float, ShadowDistance, m_ShadowDistance);
	GET_SET(bool, UseLightmaps, m_UseLightmaps);
	GET_SET(bool, ShowResolution, m_ShowResolution);

	// if LightmapVisualization is disabled, always render lightmaps
	// if it's enabled, respect the m_RenderLightmaps flag
	bool GetUseLightmapsForRendering() const { return !m_Enabled || m_UseLightmaps; };

private:
	bool m_Enabled;
	float m_ShadowDistance;
	bool m_UseLightmaps;
	bool m_ShowResolution;
};

LightmapVisualization& GetLightmapVisualization();