#pragma once

#include "Runtime/Geometry/AABB.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Camera/CullResults.h"
#include "Runtime/Camera/LightTypes.h"

class Light;
struct ShadowCullData;
class ColorRGBAf;
class BuiltinShaderParamValues;
namespace Umbra { class OcclusionBuffer; }

const int kMaxForwardVertexLights = 4;

// We reserve light buffers for this number of lights per object (in forward/vertex lit)
const int kEstimatedLightsPerObject = 3;


class LightManager
{
public:
	LightManager ();
	~LightManager ();

	static void InitializeClass ();
	static void CleanupClass ();

	// Figures out and sorts lights for the object
	// Puts results in dest as VertexLightsBlock + variable number of lights
	void FindVertexLightsForObject (dynamic_array<UInt8>& dest, const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights, const VisibleNode& node);

	// Figures out and sorts lights for the object
	// Puts results in dest as ForwardLightsBlock + variable number of lights
	void FindForwardLightsForObject (dynamic_array<UInt8>& dest, const UInt32* lightIndices, UInt32 lightCount, const ActiveLights& activeLights, const VisibleNode& node, bool lightmappedObject, bool dualLightmapsMode, bool useVertexLights, int maxAutoAddLights, bool disableAddLights, const ColorRGBAf& ambient);

	static void SetupVertexLights (int lightCount, const ActiveLight* const* lights);

	static void SetupForwardBaseLights (const ForwardLightsBlock& lights);
	static void SetupForwardAddLight (Light* light, float blend);

	void AddLight (Light* source);
	void RemoveLight (Light* source);

	typedef List<Light> Lights;
	Lights& GetAllLights () { return m_Lights; }
	const Lights& GetAllLights () const { return m_Lights; }

	///Terrain engine only
	UNITY_VECTOR(kMemRenderer, Light*) GetLights (LightType type, int layer);

	Light* GetLastMainLight() { return m_LastMainLight; }


private:
	Lights	m_Lights;
	Light*	m_LastMainLight; // brightest directional light found in last render
};

void SetSHConstants (const float sh[9][3], BuiltinShaderParamValues& params);
LightManager& GetLightManager();
