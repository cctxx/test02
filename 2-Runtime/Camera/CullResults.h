#pragma once

#include "BaseRenderer.h"
#include "Lighting.h"
#include "ShaderReplaceData.h"
#include "CullingParameters.h"
#include "Runtime/Math/Rect.h"
#include "UmbraTomeData.h"
#include "SceneNode.h"

class Light;
struct ShadowCullData;
struct ShadowedLight;
namespace Umbra { class Visibility; class Tome; }


struct VisibleNode : public TransformInfo
{
	void SetTransformInfo(const TransformInfo& src) { TransformInfo::operator=(src); }
	BaseRenderer* renderer;
	float         lodFade;
};

struct ActiveLight
{
	Light*         light;
	ShadowedLight* shadowedLight;
	
#if ENABLE_SHADOWS
	bool insideShadowRange;
#endif
	
	// For vertex lights we have to keep all lights around when doing per-object culling.
	// This is because vertex lights can still affect objects (vertex interpolation on large triangles) although the light itself might be completely invisible.
	bool isVisibleInPrepass;
	int  lightmappingForRender;
	UInt32 cullingMask;
	bool intersectsNear;
	bool intersectsFar;
	AABB boundingBox;
	Rectf screenRect;
	bool hasCookie;
	int lightRenderMode;
	int lightType;

	// Some lights are offscreen
	bool  isOffscreenVertexLight;
	float visibilityFade;
};

template <typename T>
struct CullingDynamicArray : public dynamic_array<T, AlignOfType<T>::align, kMemTempAllocId> {};

struct ActiveLights
{
	typedef CullingDynamicArray<ActiveLight> Array;
	Array lights;

	// If there is a main directional light, it will be the first one in lights.
	bool hasMainLight;

	// Lights are sorted by type in the following order
	size_t numDirLights;
	size_t numSpotLights;
	size_t numPointLights;
	size_t numOffScreenSpotLights;
	size_t numOffScreenPointLights;
};

struct ShadowedLight
{
	// Index into CullResults.activeLights array
	int           lightIndex;
};

typedef CullingDynamicArray<VisibleNode> VisibleNodes;
typedef CullingDynamicArray<UInt32> ObjectLightIndices;
typedef CullingDynamicArray<UInt32> ObjectLightOffsets;
typedef dynamic_array<ShadowedLight> ShadowedLights;

/// CullResults lives during the entire duration of rendering one frame.
/// Shadow culling uses data from the frustum/occlusion cull pass
/// The render loop uses CullResults to render the scene.
struct CullResults
{
	CullResults();
	~CullResults();

	void Init (const UmbraTomeData& tome, const RendererCullData* rendererCullData);

	CullingOutput          sceneCullingOutput;
	VisibleNodes           nodes;
	
	// All lights that might affect any visible objects
	ActiveLights           activeLights;

	// Forward rendering needs to know on a per renderer basis which lights affect it.
	ObjectLightIndices     forwardLightIndices; // index array for all objects
	ObjectLightOffsets     forwardLightOffsets; // offset for each object (in the forwardLightIndices array)

	// All lights that cast shadows on any objects in the scene
	ShadowedLights         shadowedLights;
	
	SceneCullingParameters sceneCullParameters;
	
	CullingDynamicArray<UInt8> lodMasks;
	CullingDynamicArray<float> lodFades;

	dynamic_array<SceneNode> treeSceneNodes;
	dynamic_array<AABB> treeBoundingBoxes;

	///@TODO: Whats up with this thing? Does seem related...
	ShadowCullData* shadowCullData;
	
	ShaderReplaceData shaderReplaceData;
};

void InitIndexList (IndexList& list, size_t count);
void DestroyIndexList (IndexList& list);

void CreateCullingOutput (const RendererCullData* rendererCullData, CullingOutput& cullingOutput);
void DestroyCullingOutput (CullingOutput& cullingOutput);

inline const ActiveLight* GetMainActiveLight(const ActiveLights& activeLights)
{
	return activeLights.hasMainLight ? &activeLights.lights[0] : NULL;
}

inline const UInt32* GetObjectLightIndices(const CullResults& cullResults, UInt32 roIndex)
{
	if (cullResults.forwardLightOffsets.size() == 0)
		return 0;
	
	DebugAssert(roIndex < (cullResults.forwardLightOffsets.size()-1));
	// Get beginning of light indices for object
	UInt32 lightOffset = cullResults.forwardLightOffsets[roIndex];
	return cullResults.forwardLightIndices.data() + lightOffset;
}

inline UInt32 GetObjectLightCount(const CullResults& cullResults, UInt32 visibleNodeIndex)
{
	if (cullResults.forwardLightOffsets.size() == 0)
		return 0;
	
	DebugAssert(visibleNodeIndex < (cullResults.forwardLightOffsets.size()-1));
	// We store an extra offset at the end of forwardLightOffsets
	// This means it's always safe to check next offset to get the size
	UInt32 lightOffset = cullResults.forwardLightOffsets[visibleNodeIndex];
	UInt32 nextLightOffset = cullResults.forwardLightOffsets[visibleNodeIndex + 1];
	DebugAssert(nextLightOffset >= lightOffset);
	DebugAssert(nextLightOffset <= cullResults.forwardLightIndices.size());
	return nextLightOffset - lightOffset;
}
