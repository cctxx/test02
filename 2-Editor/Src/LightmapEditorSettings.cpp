#include "UnityPrefix.h"
#include "LightmapEditorSettings.h"
#include "Runtime/Graphics/LightmapSettings.h"

LightmapEditorSettings::LightmapEditorSettings ()
: m_SkyLightColor(0.86f,0.93f,1.0f)
{
	m_Resolution = 50.0f;
	m_LastUsedResolution = 0.0f;
	m_TextureWidth = 1024;
	m_TextureHeight = 1024;
	m_SkyLightIntensity = 0.0f;
	m_BounceBoost = 1.0f;
	m_BounceIntensity = 1.0f;
	m_Quality = 0;
	m_TextureCompression = false;
	m_Bounces = 1;
	m_FinalGatherRays = 1000;
	m_FinalGatherContrastThreshold = 0.05f;
	m_FinalGatherGradientThreshold = 0.0f;
	m_FinalGatherInterpolationPoints = 15;
	m_LODSurfaceMappingDistance = 1.0F;
	m_AOAmount = 0;
	m_AOMaxDistance = 0.1f;
	m_AOContrast = 1;
	m_LockAtlas = false;
	m_Padding = 0.0f;
}

LightmapEditorSettings& GetLightmapEditorSettings()
{
	return GetLightmapSettings().GetLightmapEditorSettings();
}