#pragma once

#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Camera/CullResults.h"
#include "GlobalLayeringData.h"

namespace Unity { class Material; }
class Camera;
class Shader;
class RenderTexture;
struct ShadowCullData;
struct RenderLoop;

struct RenderObjectData {
	Unity::Material*	material;	// 4
	SInt16		queueIndex;			// 2
	UInt16		subsetIndex;		// 2
	SInt16		subShaderIndex;		// 2
	UInt16		sourceMaterialIndex;// 2
	UInt16		lightmapIndex;		// 2
	int			staticBatchIndex;	// 4
	float		distance;			// 4

	//@TODO: cold?
	float		 distanceAlongView;	// 4
	VisibleNode* visibleNode;		// 4
	Shader*		shader;				// 4	shader to use
	GlobalLayeringData
				globalLayeringData; // 4
	// 36 bytes
};

enum RenderPart { kPartOpaque, kPartAfterOpaque, kPartCount };

typedef dynamic_array<RenderObjectData> RenderObjectDataContainer;

struct RenderLoopContext
{
	Camera*			m_Camera;
	
	const CullResults*    m_CullResults;
	const ShadowCullData* m_ShadowCullData;
	Matrix4x4f		m_CurCameraMatrix;
	Rectf			m_CameraViewport;
	Vector3f		m_CurCameraPos;
	bool			m_SortOrthographic;
	bool			m_DontRenderRenderables;
	bool			m_RenderingShaderReplace;

	int				m_RenderQueueStart;
	int				m_RenderQueueEnd;
	
	RenderLoop*		m_RenderLoop;
};

void AddRenderLoopTempBuffer (RenderLoop* loop, RenderTexture* rt);

void DoForwardVertexRenderLoop (RenderLoopContext& ctx, RenderObjectDataContainer& objects, bool opaque, ActiveLights& activeLights, bool linearLighting, bool clearFrameBuffer);
void DoForwardShaderRenderLoop (
	RenderLoopContext& ctx,
	RenderObjectDataContainer& objects,
	bool opaque,
	bool disableDynamicBatching,
	RenderTexture* mainShadowMap,
	ActiveLights& activeLights,
	bool linearLighting,
	bool clearFrameBuffer);

void DoPrePassRenderLoop (
	  RenderLoopContext& ctx,
	  RenderObjectDataContainer& objects,
	  RenderObjectDataContainer& outRemainingObjects,
	  RenderTexture*& outDepthRT,
	  RenderTexture*& outDepthNormalsRT,
	  RenderTexture*& outMainShadowMap,
	  ActiveLights& activeLights,
	  bool linearLighting,
	  bool* outDepthWasCopied);

// This is only usable by GfxDeviceGLES, because GfxDeviceGLES only supports ForwardVertexRenderLoop, you'll only see these functions there
// If IsInsideRenderLoop() == true, no state caching will be performed by GfxDeviceGLES
void StartRenderLoop();
void EndRenderLoop();
bool IsInsideRenderLoop();
