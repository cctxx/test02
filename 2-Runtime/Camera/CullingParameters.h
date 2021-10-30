#ifndef CULLING_PARAMETERS_H
#define CULLING_PARAMETERS_H

#include "Runtime/Geometry/Plane.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Geometry/Intersection.h"
#include "UmbraTomeData.h"

struct SceneNode;
class AABB;

namespace Umbra { class DebugRenderer; class Visibility; class Tome; class QueryExt; }

enum CullingType   { kNoOcclusionCulling = 0, kOcclusionCulling = 1,       kShadowCasterOcclusionCulling = 2 };
enum CullFiltering { kFilterModeOff = 0,      kFilterModeShowFiltered = 1, kFilterModeShowRest = 2 };

struct IndexList
{
	int* indices;
	int  size;
	int  reservedSize;
	
	int& operator [] (int index)
	{
		DebugAssert(index < reservedSize);
		return indices[index];
	}

	int operator [] (int index) const
	{
		DebugAssert(index < reservedSize);
		return indices[index];
	}
	
	IndexList ()
	{
		indices = NULL;
		reservedSize = size = 0;
	}

	IndexList (int* i, int sz, int rs)
	{
		indices = i;
		size = sz;
		reservedSize = rs;
	}
	
};

enum
{
	kStaticRenderers = 0,
	kDynamicRenderer,
	kSceneIntermediate,
	kCameraIntermediate,
#if ENABLE_TERRAIN
	kTreeRenderer,
#endif
	kVisibleListCount
};

// Output for all culling operations.
// simple index list indexing into the different places where renderers can be found.
struct CullingOutput
{
	IndexList          visible[kVisibleListCount];
	
	Umbra::Visibility* umbraVisibility;
	
	CullingOutput () : umbraVisibility (NULL) {  }
};

struct RendererCullData
{
	const AABB*      bounds;
	const SceneNode* nodes;
	size_t           rendererCount;
	
	RendererCullData () { bounds = NULL; nodes = NULL; rendererCount = 0; }
};

struct CullingParameters
{
	enum { kMaxPlanes = 10 };
	enum LayerCull
	{
		kLayerCullNone,
		kLayerCullPlanar,
		kLayerCullSpherical
	};

	Vector3f  lodPosition;
	float     lodFieldOfView;
	float     orthoSize;
	int       cameraPixelHeight;
	Plane     cullingPlanes[kMaxPlanes];
	int       cullingPlaneCount;
	UInt32    cullingMask;
	float     layerFarCullDistances[kNumLayers];
	LayerCull layerCull;
	bool      isOrthographic;
    Vector3f  lightDir;

	// Used for Umbra
	Matrix4x4f worldToClipMatrix;
    Vector3f  position;
};

struct SceneCullingParameters : CullingParameters
{
	RendererCullData renderers[kVisibleListCount];
	
	UInt8*			lodMasks;
	size_t			lodGroupCount;
	
	bool			useOcclusionCulling;
	bool			useShadowCasterCulling;
	bool			useLightOcclusionCulling;
	bool			excludeLightmappedShadowCasters;
	bool			cullPerObjectLights;
	bool			cullLights;
	int				renderPath;
	CullFiltering   filterMode;
	
	/// This stores the visibility of previous culling operations.
	/// For example, shadow caster culling uses the visibility of the visible objects from the camera.
	const CullingOutput* sceneVisbilityForShadowCulling;
	
	UmbraTomeData		  umbraTome;
	Umbra::DebugRenderer* umbraDebugRenderer;
	Umbra::QueryExt*         umbraQuery;
	UInt32                umbraDebugFlags;
};



#endif
