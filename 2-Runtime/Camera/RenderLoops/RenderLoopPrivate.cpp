#include "UnityPrefix.h"
#include "RenderLoopPrivate.h"
#include "RenderLoop.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/ShadowCulling.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Camera/ImageFilters.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/Shaders/Shader.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "ReplacementRenderLoop.h"

#if UNITY_EDITOR
	#include "Editor/Src/EditorUserBuildSettings.h"
#endif

PROFILER_INFORMATION(gRenderPrepareObjects, "Render.Prepare", kProfilerRender)
PROFILER_INFORMATION(gRenderOpaque, "Render.OpaqueGeometry", kProfilerRender)
PROFILER_INFORMATION(gRenderTransparent, "Render.TransparentGeometry", kProfilerRender)
PROFILER_INFORMATION(gPrePassFwdDepthTex, "RenderPrePass.FwdObjectsIntoDepth", kProfilerRender)
PROFILER_INFORMATION(gPrePassFwdDepthNormalsTex, "RenderPrePass.FwdObjectsIntoDepthNormals", kProfilerRender)
PROFILER_INFORMATION(gCameraResolveProfile, "Camera.AAResolve", kProfilerRender)


namespace ShaderLab { void ClearGrabPassFrameState (); } // pass.cpp


struct RenderLoop {
public:
	RenderLoop (Camera& camera);
	~RenderLoop ();

	void PrepareFrame (bool dontRenderRenderables, bool renderingShaderReplace);

public:
	RenderLoopContext m_Context;
	ShadowCullData m_ShadowCullData;
	RenderObjectDataContainer m_Objects[kPartCount];
	ImageFilters	m_ImageFilters;

	enum { kMaxCreatedTempBuffers = 8 };
	RenderTexture* m_TempBuffers[kMaxCreatedTempBuffers];
	int m_TempBufferCount;
};


RenderLoop* CreateRenderLoop (Camera& camera)
{
	return new RenderLoop(camera);
}

void DeleteRenderLoop (RenderLoop* loop)
{
	delete loop;
}

ImageFilters& GetRenderLoopImageFilters (RenderLoop& loop)
{
	return loop.m_ImageFilters;
}


RenderLoop::RenderLoop(Camera& camera)
{
	m_Context.m_Camera = &camera;
	m_Context.m_RenderLoop = this;

	for (int i = 0; i < kMaxCreatedTempBuffers; ++i) {
		m_TempBuffers[i] = NULL;
	}
	m_TempBufferCount = 0;
}

RenderLoop::~RenderLoop()
{
	Assert (m_TempBufferCount == 0);
}

inline float MultiplyPointZ (const Matrix4x4f& m, const Vector3f& v)
{
	return m.m_Data[2] * v.x + m.m_Data[6] * v.y + m.m_Data[10] * v.z + m.m_Data[14];
}

// Both distances become smaller (more negative) when moving forward from the camera.
// outDistanceForSort is for sorting only, and it can be square of the actual distance, and so on.
// outDistnaceAlongView is projection of the center along camera's view.
static void EvaluateObjectDepth (const RenderLoopContext& ctx, const TransformInfo& info, float& outDistanceForSort, float& outDistanceAlongView)
{
	Vector3f center = info.worldAABB.GetCenter();
	if (ctx.m_SortOrthographic)
	{
		const float d = MultiplyPointZ (ctx.m_CurCameraMatrix, center);
		outDistanceForSort = d;
		outDistanceAlongView = d;
	}
	else
	{
		outDistanceAlongView = MultiplyPointZ (ctx.m_CurCameraMatrix, center);
		center -= ctx.m_CurCameraPos;
		outDistanceForSort = -SqrMagnitude(center);
	}

	// A distance of NaN can cause inconsistent sorting results, if input order is inconsistent.
	Assert(IsFinite(outDistanceForSort));
	Assert(IsFinite(outDistanceAlongView));
}


void RenderLoop::PrepareFrame (bool dontRenderRenderables, bool renderingShaderReplace)
{
	Camera& camera = *m_Context.m_Camera;
	m_Context.m_CurCameraMatrix = camera.GetWorldToCameraMatrix();
	m_Context.m_CurCameraPos = camera.GetComponent(Transform).GetPosition();
	m_Context.m_CameraViewport = camera.GetRenderRectangle();
	switch (camera.GetSortMode())
	{
	case Camera::kSortPerspective: m_Context.m_SortOrthographic = false; break;
	case Camera::kSortOrthographic: m_Context.m_SortOrthographic = true; break;
	default: m_Context.m_SortOrthographic = camera.GetOrthographic(); break;
	}
	m_Context.m_DontRenderRenderables = dontRenderRenderables;
	m_Context.m_RenderingShaderReplace = renderingShaderReplace;

	for (int i = 0; i < kPartCount; ++i)
		m_Objects[i].resize_uninitialized(0);

	#if DEBUGMODE
	for (int i = 0; i < kMaxCreatedTempBuffers; ++i) {
		Assert (m_TempBuffers[i] == NULL);
	}
	#endif
	m_TempBufferCount = 0;
}


static RenderTexture* ResolveScreenToTextureIfNeeded (RenderLoop& loop, bool forceIntoRT, bool beforeOpaqueImageFx)
{
	// If we use screen to composite image effects, resolve screen into the render texture now
	bool usingScreenToComposite = loop.m_ImageFilters.HasImageFilter() && loop.m_Context.m_Camera->GetUsesScreenForCompositing(forceIntoRT);
	RenderTexture* rt = NULL;
	if (usingScreenToComposite)
	{
		// Do a screen to RT resolve here.
		rt = beforeOpaqueImageFx ? loop.m_ImageFilters.GetTargetBeforeOpaque () : loop.m_ImageFilters.GetTargetAfterOpaque (forceIntoRT, usingScreenToComposite);
		if (!rt)
			return NULL;

		PROFILER_AUTO_GFX(gCameraResolveProfile, loop.m_Context.m_Camera)
		GPU_AUTO_SECTION(kGPUSectionPostProcess);

		// We should insert proper discard/clear/... on backbuffer when doing MSAA
		// resolved off it. However that's for the future (case 549705),
		// for now just silence the RT unresolve warning.
		GetGfxDevice().IgnoreNextUnresolveOnCurrentRenderTarget();

		Rectf r = loop.m_Context.m_Camera->GetPhysicalViewportRect();
		int rect[4];
		RectfToViewport( r, rect );
		Assert (rect[2] == rt->GetGLWidth() && rect[3] == rt->GetGLHeight());
		rt->GrabPixels (rect[0], rect[1], rect[2], rect[3]);

		// D3D and GL use different notions of how Y texture coordinates go.
		// In effect, we have to flip any sampling from the first texture in the image filters
		// stack on D3D.
		rt->CorrectVerticalTexelSize(false);
	}

	return rt;
}


void RenderImageFilters (RenderLoop& loop, RenderTexture* targetTexture, bool afterOpaque)
{
	bool forceIntoRT = loop.m_Context.m_Camera->CalculateNeedsToRenderIntoRT();
	ResolveScreenToTextureIfNeeded (loop, forceIntoRT, afterOpaque);
	bool usingScreenToComposite = loop.m_ImageFilters.HasImageFilter() && loop.m_Context.m_Camera->GetUsesScreenForCompositing(forceIntoRT);
	loop.m_ImageFilters.DoRender (targetTexture, forceIntoRT, afterOpaque, usingScreenToComposite, loop.m_Context.m_Camera->GetUsingHDR());
	if (afterOpaque && !usingScreenToComposite)
		loop.m_Context.m_Camera->SetCurrentTargetTexture (loop.m_ImageFilters.GetTargetAfterOpaque(forceIntoRT,usingScreenToComposite));
}


static void UpdateCameraDepthTextures (Camera& camera, RenderTexture* rtDepth, RenderTexture* rtDepthNormals, RenderObjectDataContainer& objects, bool depthWasCopied, bool skipDepthTexture, bool afterOpaque)
{
	if (!rtDepth || objects.size() == 0)
		return;

	// use depth buffer from final target
	RenderTexture* rtFinal = camera.GetCurrentTargetTexture();
	Assert (rtFinal);
	RenderSurfaceHandle rtSurfaceDepth = rtFinal->GetDepthSurfaceHandle();

	int renderFlags = Camera::kRenderFlagSetRenderTarget;
	if (!afterOpaque)
		renderFlags |= Camera::kRenderFlagSetRenderTargetFinal;

	if (!skipDepthTexture && gGraphicsCaps.hasStencilInDepthTexture && (camera.GetDepthTextureMode() & Camera::kDepthTexDepthBit))
	{
		Shader* shader = GetCameraDepthTextureShader ();
		if (shader)
		{
			PROFILER_AUTO_GFX(gPrePassFwdDepthTex, &camera);
			// If we did separate pass or depth resolve in deferred to work around depth+stencil texture bugs,
			// render into the copy in that case.
			if (depthWasCopied)
			{
				RenderTexture::SetActive (rtDepth);
			}
			else
			{
				RenderSurfaceHandle rtSurfaceColor = rtDepth->GetColorSurfaceHandle();
				RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, rtDepth);
			}

			RenderSceneShaderReplacement (objects, shader, "RenderType");
			camera.SetupRender (renderFlags);
		}
	}

	if (rtDepthNormals && (camera.GetDepthTextureMode() & Camera::kDepthTexDepthNormalsBit))
	{
		Shader* shader = GetCameraDepthNormalsTextureShader ();
		if (shader)
		{
			PROFILER_AUTO_GFX(gPrePassFwdDepthNormalsTex, &camera);
			RenderSurfaceHandle rtSurfaceColor = rtDepthNormals->GetColorSurfaceHandle();
			RenderTexture::SetActive (1, &rtSurfaceColor, rtSurfaceDepth, rtDepthNormals);

			RenderSceneShaderReplacement (objects, shader, "RenderType");
			camera.SetupRender (renderFlags);
		}
	}
}

bool gInsideRenderLoop = false;
void StartRenderLoop()
{
	Assert (!gInsideRenderLoop);
	gInsideRenderLoop = true;
}
void EndRenderLoop()
{
	Assert (gInsideRenderLoop);
	gInsideRenderLoop = false;
}
bool IsInsideRenderLoop()
{
	return gInsideRenderLoop;
}

void DoRenderLoop (
	RenderLoop& loop,
	RenderingPath renderPath,
	CullResults& contents,
	bool dontRenderRenderables)
{
	Assert (loop.m_TempBufferCount == 0);
	Assert (contents.shadowCullData);

	loop.m_Context.m_ShadowCullData = contents.shadowCullData;
	loop.m_Context.m_CullResults = &contents;

	// save wireframe state, restore at exit
	SetAndRestoreWireframeMode saveAndRestoreWireframe;

	const bool licenseAllowsStaticBatching = GetBuildSettings().hasAdvancedVersion;
	Camera& camera = *loop.m_Context.m_Camera;

	Shader* replacementShader = contents.shaderReplaceData.replacementShader;
	const bool replacementTagSet = contents.shaderReplaceData.replacementTagSet;
	const int replacementTagID = contents.shaderReplaceData.replacementTagID;


	{
		PROFILER_AUTO(gRenderPrepareObjects, &camera);

		loop.PrepareFrame (dontRenderRenderables, replacementShader);

		const bool useOldRenderQueueLogic = !IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_2_a1);

		// sort out objects into opaque & alpha parts
		VisibleNodes::iterator itEnd = contents.nodes.end();
		for (VisibleNodes::iterator it = contents.nodes.begin(); it != itEnd; ++it)
		{
			if (!IsFinite(it->invScale))
				continue;

			BaseRenderer* renderer = it->renderer;

			float distanceForSort, distanceAlongView;
			EvaluateObjectDepth (loop.m_Context, *it, distanceForSort, distanceAlongView);
			distanceForSort -= renderer->GetSortingFudge ();
			distanceAlongView = -distanceAlongView; // make that so increases with distance

			const int matCount = renderer->GetMaterialCount();
			const int batchIndex = (licenseAllowsStaticBatching)? renderer->GetStaticBatchIndex(): 0;
			const UInt16 lightmapIndex = renderer->GetLightmapIndex();

			for (int mi = 0; mi < matCount; ++mi)
			{
				Material* mat = renderer->GetMaterial (mi);
				if( mat == NULL )
					mat = Material::GetDefault();
				Shader* shader = mat->GetShader();

				int usedSubshaderIndex = -1;
				if (replacementShader)
				{
					if (replacementTagSet)
					{
						int subshaderTypeID = shader->GetShaderLabShader()->GetTag (replacementTagID, true);
						if (subshaderTypeID < 0)
							continue; // skip rendering
						usedSubshaderIndex = replacementShader->GetSubShaderWithTagValue (replacementTagID, subshaderTypeID);
						if (usedSubshaderIndex == -1)
							continue; // skip rendering
					}
					else
					{
						usedSubshaderIndex = 0;
					}
				}

				const int matIndex = renderer->GetSubsetIndex(mi);

				// Figure out rendering queue to use
				int queueIndex = mat->GetCustomRenderQueue(); // any per-material overriden queue takes priority
				if (queueIndex < 0)
				{
					// When no shader replacement or old content, take queue from the shader
					if (!replacementShader || useOldRenderQueueLogic)
					{
						queueIndex = shader->GetShaderLabShader()->GetRenderQueue();
					}
					// Otherwise take from replacement shader
					else
					{
						queueIndex = replacementShader->GetShaderLabShader()->GetRenderQueue(usedSubshaderIndex);
					}
				}

				RenderPart part;
				if (queueIndex <= kGeometryQueueIndexMax)
					part = kPartOpaque;
				else
					part = kPartAfterOpaque;

				RenderObjectData& odata = loop.m_Objects[part].push_back ();
				DebugAssertIf (!mat);
				odata.material = mat;
				odata.queueIndex = queueIndex;
				odata.subsetIndex = matIndex;
				odata.subShaderIndex = usedSubshaderIndex;
				odata.sourceMaterialIndex = (UInt16)mi;
				odata.lightmapIndex = lightmapIndex;
				odata.staticBatchIndex = batchIndex;
				odata.distance = distanceForSort;
				odata.distanceAlongView = distanceAlongView;
				odata.visibleNode = &*it;
				odata.shader = replacementShader ? replacementShader : shader;
				odata.globalLayeringData = renderer->GetGlobalLayeringData();
			}
		}
	}

	// want linear lighting?
	bool linearLighting = GetActiveColorSpace() == kLinearColorSpace;

	// opaque: deferred or forward
	RenderTexture *rtDepth = NULL, *rtDepthNormals = NULL;
	bool prepassDepthWasCopied = false;
	{
		PROFILER_AUTO_GFX(gRenderOpaque, &camera);

		loop.m_Context.m_RenderQueueStart = kQueueIndexMin; loop.m_Context.m_RenderQueueEnd = kGeometryQueueIndexMax+1;
		if (renderPath == kRenderPathPrePass)
		{
			#if GFX_SUPPORTS_RENDERLOOP_PREPASS
			RenderTexture *rtShadowMap = NULL;
			RenderObjectDataContainer remainingObjects;
			DoPrePassRenderLoop (loop.m_Context, loop.m_Objects[kPartOpaque], remainingObjects, rtDepth, rtDepthNormals, rtShadowMap, contents.activeLights, linearLighting, &prepassDepthWasCopied);
			if (remainingObjects.size() != 0)
			{
				// Objects/shaders that don't handle deferred: render with forward path, and pass main shadowmap to it
				// Also disable dynamic batching of those objects. They are already rendered into
				// the depth buffer, and dynamic batching would make them be rendered at slightly
				// different positions, failing depth test at places.
				DoForwardShaderRenderLoop (loop.m_Context, remainingObjects, true, true, rtShadowMap, contents.activeLights, linearLighting, false);

				UpdateCameraDepthTextures (camera, rtDepth, rtDepthNormals, remainingObjects, prepassDepthWasCopied, true, true);
			}
			#else
			ErrorString ("Pre-pass rendering loop should never happen on this platform!");
			#endif
		}
		else if (renderPath == kRenderPathForward)
		{
			DoForwardShaderRenderLoop (loop.m_Context, loop.m_Objects[kPartOpaque], true, false, NULL, contents.activeLights, linearLighting, true);
		}
		else
		{
			DoForwardVertexRenderLoop (loop.m_Context, loop.m_Objects[kPartOpaque], true, contents.activeLights, linearLighting, true);
		}
	}

	// render skybox after opaque (sRGB conversions needed if using linear rendering)
	{
		GetGfxDevice().SetSRGBWrite(linearLighting);
		camera.RenderSkybox();
		GetGfxDevice().SetSRGBWrite(false);
	}

	RenderImageFilters (loop, camera.GetTargetTexture(), true);

	// after opaque: forward
	{
		PROFILER_AUTO_GFX(gRenderTransparent, &camera);

		loop.m_Context.m_RenderQueueStart = kGeometryQueueIndexMax+1; loop.m_Context.m_RenderQueueEnd = kQueueIndexMax;
		if (renderPath != kRenderPathVertex)
		{
			DoForwardShaderRenderLoop (loop.m_Context, loop.m_Objects[kPartAfterOpaque], false, false, NULL, contents.activeLights, linearLighting, false);
		}
		else
		{
			DoForwardVertexRenderLoop (loop.m_Context, loop.m_Objects[kPartAfterOpaque], false, contents.activeLights, linearLighting, false);
		}

		UpdateCameraDepthTextures (camera, rtDepth, rtDepthNormals, loop.m_Objects[kPartAfterOpaque], prepassDepthWasCopied, false, false);
	}

	loop.m_Context.m_ShadowCullData = NULL;
	loop.m_Context.m_CullResults = NULL;
}

void CleanupAfterRenderLoop (RenderLoop& loop)
{
	Assert (loop.m_TempBufferCount >= 0 && loop.m_TempBufferCount < RenderLoop::kMaxCreatedTempBuffers);
	RenderBufferManager& rbm = GetRenderBufferManager();
	for (int i = 0; i < loop.m_TempBufferCount; ++i) {
		Assert (loop.m_TempBuffers[i]);
		rbm.ReleaseTempBuffer (loop.m_TempBuffers[i]);
		loop.m_TempBuffers[i] = NULL;
	}
	loop.m_TempBufferCount = 0;
	ShaderLab::ClearGrabPassFrameState();
}

void AddRenderLoopTempBuffer (RenderLoop* loop, RenderTexture* rt)
{
	Assert (loop && rt);
	Assert (loop->m_TempBufferCount < RenderLoop::kMaxCreatedTempBuffers);

	loop->m_TempBuffers[loop->m_TempBufferCount++] = rt;
}
