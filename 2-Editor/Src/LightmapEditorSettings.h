#pragma once

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Math/Color.h"

class LightmapEditorSettings {
public:
	DECLARE_SERIALIZE(LightmapEditorSettings)

	LightmapEditorSettings ();

	enum BakeQuality {
		kBakeQualityHigh = 0,
		kBakeQualityLow = 1
	};

	GET_SET(float, BounceBoost, m_BounceBoost);
	GET_SET(float, BounceIntensity, m_BounceIntensity);
	GET_SET(int, TextureWidth, m_TextureWidth);
	GET_SET(int, TextureHeight, m_TextureHeight);
	GET_SET(float, Resolution, m_Resolution);
	GET_SET(float, LastUsedResolution, m_LastUsedResolution);
	GET_SET(float, TempResolution, m_TempResolution);
	GET_SET(ColorRGBAf, SkyLightColor, m_SkyLightColor);
	GET_SET(float, SkyLightIntensity, m_SkyLightIntensity);
	GET_SET(int, Quality, m_Quality);
	GET_SET(bool, TextureCompression, m_TextureCompression);
	GET_SET(int, Bounces, m_Bounces);
	GET_SET(int, FinalGatherRays, m_FinalGatherRays);
	GET_SET(float, FinalGatherContrastThreshold, m_FinalGatherContrastThreshold);
	GET_SET(float, FinalGatherGradientThreshold, m_FinalGatherGradientThreshold);
	GET_SET(float, FinalGatherInterpolationPoints, m_FinalGatherInterpolationPoints);
	GET_SET(float, AOAmount, m_AOAmount);
	GET_SET(float, AOMaxDistance, m_AOMaxDistance);
	GET_SET(float, AOContrast, m_AOContrast);
	GET_SET(bool, LockAtlas, m_LockAtlas);
	GET_SET(int, Padding, m_Padding);
	GET_SET(float, LODSurfaceMappingDistance, m_LODSurfaceMappingDistance);

	void UpdateResolutionOnBakeStart() { m_TempResolution = m_Resolution; }
	void UpdateResolutionOnBakeSuccess() { m_LastUsedResolution = m_TempResolution; }

private:
	float m_BounceBoost;			// Beast: diffuseBoost
	float m_BounceIntensity;		// Beast: primaryIntensity
	int m_TextureWidth;
	int m_TextureHeight;
	float m_Resolution;				// Number of texels per world unit.
	int m_Padding;					// Texel separation between shapes.
	float m_LastUsedResolution;		// Resolution of the last successful bake.
	float m_TempResolution;			// Resolution of the currently running bake. Will be copied into m_LastUsedResolution if the bake succeeds.
	ColorRGBAf m_SkyLightColor;
	float m_SkyLightIntensity;
	int m_Quality;
	bool m_TextureCompression;
	int m_Bounces;					// number of light bounces in the GI simulation (Beast: fgDepth)
	int m_FinalGatherRays;
	float m_FinalGatherContrastThreshold;
	float m_FinalGatherGradientThreshold;
	float m_LODSurfaceMappingDistance;
	int m_FinalGatherInterpolationPoints; // (Beast: fgInterpolationPoints)

	float m_AOAmount;
	float m_AOMaxDistance;
	float m_AOContrast;
	bool m_LockAtlas;				// if enabled, don't ask Beast for atlasing and don't change lightmap index, scale and offset on renderers
	bool m_LightmapVisualisation;
};

LightmapEditorSettings& GetLightmapEditorSettings();

template<class TransferFunction>
void LightmapEditorSettings::Transfer (TransferFunction& transfer)
{
		TRANSFER(m_Resolution);
		TRANSFER(m_LastUsedResolution);
		TRANSFER(m_TextureWidth);
		TRANSFER(m_TextureHeight);
		TRANSFER(m_BounceBoost);
		TRANSFER(m_BounceIntensity);
		TRANSFER(m_SkyLightColor);
		TRANSFER(m_SkyLightIntensity);
		TRANSFER(m_Quality);
		TRANSFER(m_Bounces);
		TRANSFER(m_FinalGatherRays);
		TRANSFER(m_FinalGatherContrastThreshold);
		TRANSFER(m_FinalGatherGradientThreshold);
		TRANSFER(m_FinalGatherInterpolationPoints);
		TRANSFER(m_AOAmount);
		TRANSFER(m_AOMaxDistance);
		TRANSFER(m_AOContrast);
		TRANSFER(m_LODSurfaceMappingDistance);
		TRANSFER(m_Padding);
		TRANSFER(m_TextureCompression);
		TRANSFER(m_LockAtlas);
}
