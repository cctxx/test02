#include "UnityPrefix.h"
#include "Camera.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "RenderLoops/RenderLoop.h"
#include "UnityScene.h"
#include "SceneSettings.h"
#include "Runtime/Geometry/Ray.h"
#include "Culler.h"
#include "ImageFilters.h"
#include "Runtime/Shaders/Material.h"
#include "RenderSettings.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "RenderManager.h"
#include "Skybox.h"
#include "Flare.h"
#include "Light.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Camera/RenderLayers/GUILayer.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "CameraUtil.h"
#include "CameraCullingParameters.h"
#include "Runtime/Graphics/CubemapTexture.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "IntermediateRenderer.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Misc/QualitySettings.h"
#include "RenderLoops/ReplacementRenderLoop.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Shaders/ShaderKeywords.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Configuration/UnityConfigure.h"
#include "LODGroupManager.h"
#include "ShadowCulling.h"
#include "External/Umbra/builds/interface/runtime/umbraQuery.hpp"
#include "External/Umbra/builds/interface/runtime/umbraTome.hpp"
#include "Runtime/Camera/RenderLoops/BuiltinShaderParamUtility.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "External/shaderlab/Library/intshader.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Geometry/Intersection.h"
#include "Runtime/Interfaces/ITerrainManager.h"

#if UNITY_EDITOR
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Editor/Platform/Interface/RepaintController.h"
#endif

///@TODO: Ensure that we use cullingparameters.renderingPath, otherwise potential inconsistency when switching renderpath after culling.
RenderingPath CalculateRenderingPath (RenderingPath rp);

using namespace std;

static SHADERPROP(CameraDepthTexture);
static SHADERPROP(CameraDepthNormalsTexture);
static SHADERPROP(Reflection);

static ShaderKeyword kKeywordSoftParticles = keywords::Create ("SOFTPARTICLES_ON");

/////***@TODO: Write test for stressing multithreaded breaking when OnWillRenderObjects does nasty things...


void Camera::InitializeClass ()
{
	REGISTER_MESSAGE_VOID (Camera, kTransformChanged, TransformChanged);
	RegisterAllowNameConversion (Camera::GetClassStringStatic(), "is ortho graphic", "orthographic");
}


Camera::Camera (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_DirtyProjectionMatrix(true)
,	m_DirtyWorldToCameraMatrix(true)
,	m_DirtyWorldToClipMatrix(true)
,	m_DepthTextureMode(0)
,	m_DepthTexture(NULL)
,	m_DepthNormalsTexture(NULL)
,	m_ClearStencilAfterLightingPass(false)
#if UNITY_EDITOR
,	m_OnlyRenderIntermediateObjects(false)
,	m_IsSceneCamera(false)
,	m_FilterMode (0)
,	m_AnimateMaterials(false)
,	m_AnimateMaterialsTime(0.0f)
#endif
{
	m_RenderLoop = CreateRenderLoop (*this);

	m_CullingMask.m_Bits = 0xFFFFFFFF;
	m_EventMask.m_Bits = 0xFFFFFFFF;

	for(int i=0;i<32;i++)
		m_LayerCullDistances[i] = 0;
	m_LayerCullSpherical = false;
	m_SortMode = kSortDefault;
	m_ImplicitProjectionMatrix = m_ImplicitWorldToCameraMatrix = true;
	m_ImplicitAspect = true;
	m_HDR = false;
	m_UsingHDR = false;
	m_IsRendering = false;

	m_Velocity = Vector3f::zero;
	m_LastPosition = Vector3f::zero;
	m_WorldToCameraMatrix = m_WorldToClipMatrix = m_ProjectionMatrix = Matrix4x4f::identity;
	m_CurrentTargetTexture = NULL;
	m_CurrentTargetFace = kCubeFaceUnknown;
	m_OcclusionCulling = true;

	m_TargetBuffersOriginatedFrom = 0;
	m_TargetColorBufferCount = 1;
	::memset(m_TargetColorBuffer, 0x00, sizeof(m_TargetColorBuffer));

	m_TargetColorBuffer[0]	= GetUncheckedGfxDevice().GetBackBufferColorSurface();
	m_TargetDepthBuffer		= GetUncheckedGfxDevice().GetBackBufferDepthSurface();

	m_IntermediateRenderers = UNITY_NEW(IntermediateRenderers, GetMemoryLabel());
#if UNITY_EDITOR
	m_OldCameraState.Reset();
#endif
}

void Camera::Reset ()
{
	Super::Reset();

	m_NormalizedViewPortRect = Rectf (0, 0, 1, 1);

	m_BackGroundColor = ColorRGBA32 (49, 77, 121, 5); // very small alpha to not get "everything glows" by default
	m_Depth = 0.0F;
	m_NearClip = 0.3F;
	m_FarClip = 1000.0F;
	m_RenderingPath = -1;
	m_Aspect = 1.0F;
	m_Orthographic = false;
	m_HDR = false;
	m_SortMode = kSortDefault;

	m_OrthographicSize = 5.0F;
	m_FieldOfView = 60.0F;
	m_ClearFlags = kSkybox;
	m_DirtyWorldToCameraMatrix = m_DirtyProjectionMatrix = m_DirtyWorldToClipMatrix = true;
}



void Camera::CheckConsistency ()
{
	Super::CheckConsistency();
	m_RenderingPath = clamp(m_RenderingPath, -1, kRenderPathCount-1);
	if(!m_Orthographic && m_NearClip < 0.01F)
		m_NearClip = 0.01F;
	if(m_FarClip < m_NearClip + 0.01F)
		m_FarClip = m_NearClip + 0.01F;
}


Camera::~Camera ()
{
	CleanupDepthTextures ();
	m_IntermediateRenderers->Clear();
	UNITY_DELETE(m_IntermediateRenderers, GetMemoryLabel());

	DeleteRenderLoop (m_RenderLoop);
}


void Camera::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	if ((awakeMode & kDidLoadFromDisk) == 0 && IsAddedToManager ())
	{
		GetRenderManager().RemoveCamera (this);
		GetRenderManager().AddCamera (this);
	}
	m_DirtyWorldToCameraMatrix = m_DirtyProjectionMatrix = m_DirtyWorldToClipMatrix = true;
	WindowSizeHasChanged ();
	if(m_HDR)
		DisplayHDRWarnings();
}


void Camera::ClearIntermediateRenderers( size_t startIndex )
{
	m_IntermediateRenderers->Clear(startIndex);
}

void Camera::AddToManager ()
{
	GetRenderManager().AddCamera (this);
	WindowSizeHasChanged ();
	m_LastPosition = GetComponent (Transform).GetPosition ();
	m_Velocity = Vector3f (0.0F, 0.0F, 0.0F);
}

void Camera::TransformChanged()
{
	m_DirtyWorldToCameraMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::RemoveFromManager ()
{
	GetRenderManager().RemoveCamera (this);
}



template<class TransferFunction>
void Camera::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	// Note: transfer code for version 1 was just removed. It was around Unity 1.2 times,
	// and now we're fine with losing project folder compatibility with that.
	transfer.SetVersion (2);

	TRANSFER_SIMPLE (m_ClearFlags);
	TRANSFER_SIMPLE (m_BackGroundColor);

	TRANSFER (m_NormalizedViewPortRect);
	transfer.Transfer (m_NearClip, "near clip plane");
	transfer.Transfer (m_FarClip, "far clip plane");
	transfer.Transfer (m_FieldOfView, "field of view", kSimpleEditorMask);
	transfer.Transfer (m_Orthographic, "orthographic");
	transfer.Align();
	transfer.Transfer (m_OrthographicSize, "orthographic size");

	TRANSFER (m_Depth);
	TRANSFER (m_CullingMask);
	//TRANSFER (m_EventMask);
	TRANSFER (m_RenderingPath);

	transfer.Transfer (m_TargetTexture, "m_TargetTexture");
	TRANSFER (m_HDR);
	TRANSFER (m_OcclusionCulling);
}


static inline Rectf GetCameraTargetRect (const Camera& camera, bool zeroOrigin)
{
	RenderTexture* target = camera.GetTargetTexture();
	if (target != NULL)
		return Rectf(0, 0, target->GetWidth(), target->GetHeight());

	RenderSurfaceHandle colorTarget = camera.GetTargetColorBuffer();
	if(colorTarget.IsValid() && !colorTarget.object->backBuffer)
		return Rectf(0, 0, colorTarget.object->width, colorTarget.object->height);

	const RenderManager& renderMgr = GetRenderManager();
	Rectf rect = renderMgr.GetWindowRect();

#if UNITY_EDITOR
	// In the editor, if we're trying to get rect of a regular camera (visible in hierarchy etc.),
	// use game view size instead of "whatever editor window was processed last" size.
	// Otherwise Camera.main.aspect would return aspect of inspector when repainting it, for example.
	//
	// Only do this for regular cameras however; keep hidden cameras (scene view, material preview etc.)
	// using the old behavior.
	Unity::GameObject* go = camera.GetGameObjectPtr();
	if (go && (go->GetHideFlags() & Object::kHideAndDontSave) != Object::kHideAndDontSave)
	{
		// If the current guiview is a GameView then GetRenderManager().GetWindowRect() is already set up correctly and
		// we do not need to find first available game view to get a valid rect. Fix for case 517158
		bool isCurrentGUIViewAGameView = GUIView::GetCurrent() != NULL && GUIView::GetCurrent()->IsGameView();
		if (!isCurrentGUIViewAGameView)
		{
			bool gameViewFocus;
			Rectf gameViewRect;
			GetScreenParamsFromGameView(false, false, &gameViewFocus, &gameViewRect, &rect);
		}
	}
#endif

	if (zeroOrigin)
		rect.x = rect.y = 0.0f;
	return rect;
}


Rectf Camera::GetCameraRect (bool zeroOrigin) const
{
	// Get the screen rect from either the target texture or the viewport we're inside
	Rectf screenRect = GetCameraTargetRect (*this, zeroOrigin);

	// Now figure out how large this camera is depending on the normalized viewRect.
	Rectf viewRect = m_NormalizedViewPortRect;
	viewRect.Scale (screenRect.width, screenRect.height);
	viewRect.Move (screenRect.x, screenRect.y);
	viewRect.Clamp (screenRect);
	return viewRect;
}


void Camera::SetScreenViewportRect (const Rectf& pixelRect)
{
	// Get the screen rect from either the target texture or the viewport we're inside
	// Use zero base the screen rect; all game code assumes that the visible viewport starts at zero.
	Rectf screenRect = GetCameraTargetRect (*this, true);

	// Now translate from pixel to viewport space
	Rectf viewRect = pixelRect;
	viewRect.Move (-screenRect.x, -screenRect.y);
	if (screenRect.width > 0.0f &&  screenRect.height > 0.0f)
		viewRect.Scale (1.0F / screenRect.width, 1.0F / screenRect.height);
	else
		viewRect.Reset();
	SetNormalizedViewportRect(viewRect);
}

static void InitShaderReplaceData (Shader* replacementShader, const std::string& shaderReplaceTag, ShaderReplaceData& output)
{
	// Shader replacement might be passed explicitly (camera.RenderWithShader) OR shader replacement can be setup as camera's state (camera.SetReplacementShader)
	if( replacementShader != NULL )
	{
		output.replacementShader = replacementShader;
		output.replacementTagID = ShaderLab::GetShaderTagID(shaderReplaceTag);
		output.replacementTagSet = !shaderReplaceTag.empty();
	}
	else
	{
		Assert(output.replacementShader == NULL);
	}
}

bool Camera::IsValidToRender() const
{
	if( m_NormalizedViewPortRect.IsEmpty() )
		return false;
	if( m_NormalizedViewPortRect.x >= 1.0F || m_NormalizedViewPortRect.GetRight() <= 0.0F)
		return false;
	if( m_NormalizedViewPortRect.y >= 1.0F || m_NormalizedViewPortRect.GetBottom() <= 0.0F)
		return false;

	if( m_FarClip <= m_NearClip )
		return false;
	if( !m_Orthographic )
	{
		if( m_NearClip <= 0.0f )
			return false; // perspective camera needs positive near plane
		if( Abs(m_FieldOfView) < 1.0e-6f )
			return false; // field of view has to be non zero
	}
	else
	{
		if( Abs(m_OrthographicSize) < 1.0e-6f )
			return false; // orthographic size has to be non zero
	}
	return true;
}

bool Camera::GetUsesScreenForCompositing (bool forceIntoRT) const
{
	// If rendering into a texture, don't composite to screen
	if (forceIntoRT || m_TargetTexture.IsValid() || !m_TargetColorBuffer[0].IsValid() || !m_TargetColorBuffer[0].object->backBuffer)
		return false;

	#if UNITY_OSX && WEBPLUG && !UNITY_PEPPER
	// In CoreAnimation plugin, we use frame buffer blit extension for ImageFX with FSAA.
	// I assume any mac which does core animation supports that, but better safe then sorry :)
	if (GetScreenManager().IsUsingCoreAnimation() && !gGraphicsCaps.gl.hasFrameBufferBlit )
		return false;
	#endif

	// If FSAA is used: composite to screen!
	if (GetQualitySettings().GetCurrent().antiAliasing > 1 && gGraphicsCaps.hasMultiSample)
		return true;

	// If camera is part of multi-layer setup (does not clear): composite to screen!
	// Except if this is a scene view camera; do not composite it to screen because it would break
	// Image FX + AA + Shadows
	#if UNITY_EDITOR
	if (m_IsSceneCamera)
		return false;
	#endif
	if (m_ClearFlags != kSkybox && m_ClearFlags != kSolidColor)
		return true;

	// Otherwise, it's a clearing camera with no AA used
	return false;
}


void Camera::SetupRender( int renderFlags )
{
	GfxDevice& device = GetGfxDevice();

	// Cache whether we use HDR for rendering.
	m_UsingHDR = CalculateUsingHDR();

	bool forceIntoRT = CalculateNeedsToRenderIntoRT();
	int antiAliasing = CalculateAntiAliasingForRT();

	if (renderFlags & kRenderFlagPrepareImageFilters)
	{
		// figure out if we need to render into a texture & prepare for that
		GetRenderLoopImageFilters(*m_RenderLoop).Prepare (forceIntoRT, GetUsingHDR(), antiAliasing);
	}

	// Set the current target texture to be the one calculated.
	m_CurrentTargetTexture = NULL;
	if (!GetUsesScreenForCompositing(forceIntoRT))
	{
		ImageFilters& imageFilters = GetRenderLoopImageFilters(*m_RenderLoop);

		// If kFlagSetRenderTargetFinal is set we want to set the current target to the one image filters blitted to.
		// This is the target transparent objects were rendered to and the lens flare needs to be rendered to as well. (case 443687)
		m_CurrentTargetTexture = (renderFlags & kRenderFlagSetRenderTargetFinal) ? imageFilters.GetTargetFinal() : imageFilters.GetTargetBeforeOpaque ();

		if(!m_CurrentTargetTexture)
			m_CurrentTargetTexture = m_TargetTexture;
	}

	// Compute the viewport rect in the window
	// This is only used when setting current render texture to NULL,
	// so that it can restore the viewport.
	int* viewPortCoords = GetRenderManager().GetCurrentViewPortWriteable();
	RectfToViewport( GetPhysicalViewportRect(), viewPortCoords );

	if(renderFlags & kRenderFlagSetRenderTarget)
	{
		m_CurrentTargetTexture = EnsureRenderTextureIsCreated(m_CurrentTargetTexture);

		CubemapFace curFace = kCubeFaceUnknown;
		if(m_CurrentTargetTexture && m_CurrentTargetTexture->GetDimension() == kTexDimCUBE)
			curFace = m_CurrentTargetFace;

		// while we could return const ref (and grab address and use uniformly)
		// we pass non const pointer to SetActive (because surfaces can be reset internally)
		// so create local handle copy if we draw to real texture
		RenderSurfaceHandle rtcolor = m_CurrentTargetTexture ? m_CurrentTargetTexture->GetColorSurfaceHandle() : RenderSurfaceHandle();

		RenderSurfaceHandle* color = m_CurrentTargetTexture ? &rtcolor : m_TargetColorBuffer;
		RenderSurfaceHandle  depth = m_CurrentTargetTexture ? m_CurrentTargetTexture->GetDepthSurfaceHandle() : m_TargetDepthBuffer;
		int					 count = m_CurrentTargetTexture ? 1 : m_TargetColorBufferCount;

		if(!m_CurrentTargetTexture)
			m_CurrentTargetTexture = m_TargetBuffersOriginatedFrom;

		RenderTexture::SetActive(count, color, depth, m_CurrentTargetTexture, 0, curFace, RenderTexture::kFlagDontSetViewport);

		int viewcoord[4];
		if(color[0].IsValid() && color[0].object->backBuffer)
			::memcpy(viewcoord, viewPortCoords, 4*sizeof(int));
		else
			RectfToViewport(GetRenderRectangle(), viewcoord);

		FlipScreenRectIfNeeded(device, viewcoord);
		device.SetViewport(viewcoord[0], viewcoord[1], viewcoord[2], viewcoord[3]);
	}

	device.SetProjectionMatrix (GetProjectionMatrix());
	device.SetViewMatrix( GetWorldToCameraMatrix().GetPtr() );
	SetCameraShaderProps();
}


void Camera::SetCameraShaderProps()
{
	GfxDevice& device = GetGfxDevice();
	BuiltinShaderParamValues& params = device.GetBuiltinParamValues();

	Transform &tc = GetComponent (Transform);

	Vector3f pos = tc.GetPosition ();
	params.SetVectorParam(kShaderVecWorldSpaceCameraPos, Vector4f(pos, 0.0f));


	Matrix4x4f temp;

	// World to camera matrix
	params.SetMatrixParam(kShaderMatWorldToCamera, tc.GetWorldToLocalMatrixNoScale());

	// Camera to world matrix
	temp = tc.GetLocalToWorldMatrixNoScale ();
	params.SetMatrixParam(kShaderMatCameraToWorld, temp);

	// Get the matrix to use for cubemap reflections.
	// It's camera to world matrix; rotation only, and mirrored on Y.
	temp.GetPtr()[12] = temp.GetPtr()[13] =  temp.GetPtr()[14] = 0;	// clear translation
	Matrix4x4f invertY;
	invertY.SetScale(Vector3f (1,-1,1));
	Matrix4x4f reflMat;
	MultiplyMatrices4x4 (&temp, &invertY, &reflMat);
	ShaderLab::g_GlobalProperties->SetValueProp (kSLPropReflection, 16, reflMat.GetPtr());

	// Camera clipping planes
	SetClippingPlaneShaderProps();

	// Setup time & misc properties

	const TimeManager& timeMgr = GetTimeManager();
	float time;
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_5_a1))
		time = timeMgr.GetTimeSinceLevelLoad ();
	else
		time = timeMgr.GetCurTime ();

#if UNITY_EDITOR
	if (m_AnimateMaterials)
		time = m_AnimateMaterialsTime;
#endif

	const float kMinDT = 0.005f;
	const float kMaxDT = 0.2f;
	const float deltaTime = clamp(timeMgr.GetDeltaTime(), kMinDT, kMaxDT);
	const float smoothDeltaTime = clamp(timeMgr.GetSmoothDeltaTime(), kMinDT, kMaxDT);

	// The 0.05 in kShaderVecTime is a typo, but can't change it now. There are water shaders out there that
	// use exactly .x component :(

	params.SetVectorParam(kShaderVecTime, Vector4f(0.05f*time, time, 2.0f*time, 3.0f*time));
	params.SetVectorParam(kShaderVecSinTime, Vector4f(sinf(0.125f*time), sinf(0.25f*time), sinf(0.5f*time), sinf(time)));
	params.SetVectorParam(kShaderVecCosTime, Vector4f(cosf(0.125f*time), cosf(0.25f*time), cosf(0.5f*time), cosf(time)));
	params.SetVectorParam(kShaderVecPiTime, Vector4f(fmodf(time,kPI), fmodf(2.0f*time, kPI), fmodf(3.0f*time, kPI), fmodf(4.0f*time, kPI)));
	params.SetVectorParam(kShaderVecDeltaTime, Vector4f(deltaTime, 1.0f/deltaTime, smoothDeltaTime, 1.0f/smoothDeltaTime));

	float projNear = GetProjectionNear();
	float projFar = GetProjectionFar();
	params.SetVectorParam(kShaderVecProjectionParams, Vector4f(device.GetInvertProjectionMatrix() ? -1.0f : 1.0f, projNear, projFar, 1.0 / projFar));

	Rectf view = GetScreenViewportRect();
	params.SetVectorParam(kShaderVecScreenParams, Vector4f(view.width, view.height, 1.0f+1.0f/view.width, 1.0f+1.0f/view.height));

	// From http://www.humus.name/temp/Linearize%20depth.txt
	// But as depth component textures on OpenGL always return in 0..1 range (as in D3D), we have to use
	// the same constants for both D3D and OpenGL here.
	double zc0, zc1;
	// OpenGL would be this:
	// zc0 = (1.0 - projFar / projNear) / 2.0;
	// zc1 = (1.0 + projFar / projNear) / 2.0;
	// D3D is this:
	zc0 = 1.0 - projFar / projNear;
	zc1 = projFar / projNear;
	params.SetVectorParam(kShaderVecZBufferParams, Vector4f(zc0, zc1, zc0/projFar, zc1/projFar));

	// make sure we have a gamma correct grey value available
	float correctGreyValue = GammaToActiveColorSpace(0.5f);
	params.SetVectorParam(kShaderVecColorSpaceGrey, Vector4f(correctGreyValue,correctGreyValue,correctGreyValue,0.5f));
}

PROFILER_INFORMATION(gCameraClearProfile, "Clear", kProfilerRender)

static const ColorRGBAf ConvertColorToActiveColorSpace(const ColorRGBAf& color)
{
#if UNITY_PS3
	return color;
#else
	return GammaToActiveColorSpace(color);
#endif
}


static void ClearFramebuffer(GfxClearFlags gfxClearFlags, Rectf rect, ColorRGBAf const& color)
{
	PROFILER_AUTO_GFX(gCameraClearProfile, NULL);
	const float depth = 1.0f;
	const int stencil = 0;

	GfxDevice& device = GetGfxDevice();
	// If we're rendering into a temporary texture, we always have (0,0) in the bottom-left corner
	// No matter what the view coords say.
	int si[4];
	RectfToViewport( rect, si );
	FlipScreenRectIfNeeded( device, si );
	device.SetScissorRect( si[0], si[1], si[2], si[3] );

	// seems like the best place to time clear
	ABSOLUTE_TIME clearStart = START_TIME;
#if UNITY_OSX && WEBPLUG && !UNITY_PEPPER
	if (gGraphicsCaps.gl.mustWriteToDepthBufferBeforeClear && gfxClearFlags & kGfxClearDepth)
	{
		// Mac OS X 10.7.2 introduced a bug in the NVidia GPU drivers, where the depth buffer would
		// contain garbage if used in a CoreAnimation content with enabled stencil buffer, if the
		// depth buffer is not written to at least once between buffer clears. This breaks scenes which
		// only read but never write to the depth buffer (such as a scene using only particle shaders).
		// So we render an invisble, depth only triangle in that case.
		DeviceMVPMatricesState preserveMVP;
		LoadFullScreenOrthoMatrix();

		static Material* s_UpdateDepthBufferMaterial = NULL;
		if (!s_UpdateDepthBufferMaterial)
		{
			const char* kUpdateDepthBufferShader =
			"Shader \"Hidden/UpdateDepthBuffer\" {\n"
			"SubShader { Pass {\n"
			"	ZTest Always Cull Off Fog { Mode Off } ColorMask 0\n"
			"}}}";
			s_UpdateDepthBufferMaterial = Material::CreateMaterial (kUpdateDepthBufferShader, Object::kHideAndDontSave);
		}

		s_UpdateDepthBufferMaterial->SetPass (0);
		device.ImmediateBegin (kPrimitiveTriangles);
		device.ImmediateVertex (0.0f, 0.0f, 0.1f);
		device.ImmediateVertex (0.0f, 0.1f, 0.1f);
		device.ImmediateVertex (0.1f, 0.1f, 0.1f);
		device.ImmediateEnd ();
	}
#endif
	GraphicsHelper::Clear (gfxClearFlags, color.GetPtr(), depth, stencil);
	GPU_TIMESTAMP();
	GetGfxDevice().GetFrameStats().AddClear(ELAPSED_TIME(clearStart));

	device.DisableScissor();
}


static void ClearFramebuffer(Camera::ClearMode clearMode, Rectf rect, ColorRGBAf const& color, bool hasSkybox)
{
	GfxClearFlags gfxClearFlags = kGfxClearAll;
	switch (clearMode)
	{
		case Camera::kDontClear:
			return;
		case Camera::kDepthOnly:
			gfxClearFlags = kGfxClearDepthStencil;
			break;
		case Camera::kSolidColor:
			gfxClearFlags = kGfxClearAll;
			break;
		case Camera::kSkybox:
			gfxClearFlags = hasSkybox ? kGfxClearDepthStencil : kGfxClearAll;
			break;
	}
	ClearFramebuffer(gfxClearFlags, rect, color);
}


void Camera::Clear()
{
	//Do not need to convert background color to correct space as this is done in gamma space always.
	ClearFramebuffer(GetClearFlags(), GetRenderRectangle(), m_BackGroundColor, GetSkyboxMaterial() != NULL);
	RenderSkybox();
}

void Camera::ClearNoSkybox(bool noDepth)
{
	ClearMode clearMode = GetClearFlags();
	UInt32 flags = kGfxClearAll;
	switch (clearMode)
	{
		case Camera::kDontClear: flags = 0; break;
		case Camera::kDepthOnly: flags = kGfxClearDepthStencil; break;
		case Camera::kSolidColor: flags = kGfxClearAll; break;
		case Camera::kSkybox: flags = kGfxClearAll; break;
	}
	if (noDepth)
		flags &= ~kGfxClearDepthStencil;
	if (flags == 0)
		return;

	ClearFramebuffer((GfxClearFlags)flags, GetRenderRectangle(), ConvertColorToActiveColorSpace(m_BackGroundColor));
}

void Camera::RenderSkybox()
{
	if (m_ClearFlags != kSkybox)
		return;

	Material* skybox = GetSkyboxMaterial();
	if (!skybox)
		return;

	Skybox::RenderSkybox (skybox, *this);
}

Material *Camera::GetSkyboxMaterial () const
{
	Skybox *sb = QueryComponent (Skybox);
	if (sb && sb->GetEnabled() && sb->GetMaterial())
		return sb->GetMaterial();
	else
		return GetRenderSettings().GetSkyboxMaterial();
}

Rectf Camera::GetRenderRectangle() const
{
	if( m_CurrentTargetTexture && m_CurrentTargetTexture != (RenderTexture*)m_TargetTexture )
	{
		return Rectf (0, 0, m_CurrentTargetTexture->GetWidth(), m_CurrentTargetTexture->GetHeight());
	}
	else
	{
		return GetPhysicalViewportRect();
	}
}

PROFILER_INFORMATION(gCameraRenderProfile, "Camera.Render", kProfilerRender)
PROFILER_INFORMATION(gCameraRenderToCubemapProfile, "Camera.RenderToCubemap", kProfilerRender)
PROFILER_INFORMATION(gCameraCullProfile, "Culling", kProfilerRender)
PROFILER_INFORMATION(gCameraDrawProfile, "Drawing", kProfilerRender)
PROFILER_INFORMATION(gCameraDepthTextureProfile, "UpdateDepthTexture", kProfilerRender)
PROFILER_INFORMATION(gCameraDepthNormalsTextureProfile, "UpdateDepthNormalsTexture", kProfilerRender)

void Camera::CalculateFrustumPlanes(Plane frustum[kPlaneFrustumNum], const Matrix4x4f& overrideWorldToClip, float overrideFarPlane, float& outBaseFarDistance, bool implicitNearFar) const
{
	ExtractProjectionPlanes (overrideWorldToClip, frustum);

	Plane& nearPlane = frustum[kPlaneFrustumNear];
	Plane& farPlane = frustum[kPlaneFrustumFar];

	if (IsImplicitWorldToCameraMatrix() || implicitNearFar)
	{
		// Extracted near and far planes may be unsuitable for culling.
		// E.g. oblique near plane for water refraction busts both planes.
		// Also very large far/near ratio causes precision problems.
		// Instead we calculate the planes from our position/direction.

		Matrix4x4f cam2world = GetCameraToWorldMatrix();
		Vector3f eyePos = cam2world.GetPosition();
		Vector3f viewDir = -NormalizeSafe(cam2world.GetAxisZ());

		nearPlane.SetNormalAndPosition(viewDir, eyePos);
		nearPlane.distance -= m_NearClip;

		farPlane.SetNormalAndPosition(-viewDir, eyePos);
		outBaseFarDistance = farPlane.distance;
		farPlane.distance += overrideFarPlane;
	}
	else
		outBaseFarDistance = farPlane.distance - overrideFarPlane;
}

void Camera::CalculateCullingParameters(CullingParameters& cullingParameters) const
{
	Plane frustum[kPlaneFrustumNum];
	float baseFarDistance;

	Matrix4x4f worldToClipMatrix = GetWorldToClipMatrix();
	cullingParameters.worldToClipMatrix = worldToClipMatrix;
	cullingParameters.position = GetPosition();

	CalculateFrustumPlanes(frustum, worldToClipMatrix, m_FarClip, baseFarDistance, false);
	CalculateCustomCullingParameters(cullingParameters, frustum, kPlaneFrustumNum);

	if (m_LayerCullSpherical)
	{
		std::copy(m_LayerCullDistances, m_LayerCullDistances + kNumLayers, cullingParameters.layerFarCullDistances);
		cullingParameters.layerCull = CullingParameters::kLayerCullSpherical;
	}
	else
	{
		CalculateFarCullDistances(cullingParameters.layerFarCullDistances, baseFarDistance);
		cullingParameters.layerCull = CullingParameters::kLayerCullPlanar;
	}
}

void Camera::CalculateCustomCullingParameters(CullingParameters& cullingParameters, const Plane* planes, int planeCount) const
{
	cullingParameters.lodPosition = GetPosition();
	cullingParameters.lodFieldOfView = m_FieldOfView;
	cullingParameters.orthoSize = m_OrthographicSize;
	cullingParameters.cameraPixelHeight = int(GetPhysicalViewportRect().height);

	// Shadow code handles per-layer cull distances itself

	Assert(planeCount <= CullingParameters::kMaxPlanes);
	for (int i = 0; i < planeCount; i++)
		cullingParameters.cullingPlanes[i] = planes[i];
	cullingParameters.cullingPlaneCount = planeCount;
	cullingParameters.layerCull = CullingParameters::kLayerCullNone;
	cullingParameters.isOrthographic = GetOrthographic();
	cullingParameters.cullingMask = m_CullingMask.m_Bits;

	Matrix4x4f worldToClipMatrix = GetWorldToClipMatrix();
	cullingParameters.worldToClipMatrix = worldToClipMatrix;
	cullingParameters.position = GetPosition();
}


void Camera::StandaloneCull (Shader* replacementShader, const std::string& replacementTag, CullResults& results)
{
	CameraCullingParameters parameters (*this, kCullFlagNeedsLighting | kCullFlagForceEvenIfCameraIsNotActive);
	if (GetUseOcclusionCulling())
		parameters.cullFlag |= kCullFlagOcclusionCull;

	InitShaderReplaceData(replacementShader, replacementTag, parameters.explicitShaderReplace);

	CustomCull(parameters, results);
}

void Camera::Cull (CullResults& results)
{
	CameraCullingParameters parameters (*this, kCullFlagNeedsLighting);
	if (GetUseOcclusionCulling())
		parameters.cullFlag |= kCullFlagOcclusionCull;

	CustomCull(parameters, results);
}

static bool HasValidUmbraShadowCullingData (const Umbra::Visibility* umbraVisibility, const Umbra::Tome* tome)
{
	// Exact visibility is useful for pixel lights only, so check rendering path.
	// Only Portal culling mode support shadow culling

	return umbraVisibility != NULL && tome != NULL;
}

bool static IsCullPerObjectLightsNeeded(SceneCullingParameters& sceneCullParameters, RenderingPath renderPath)
{
	if (renderPath != kRenderPathPrePass)
		return true;

	// Check whether there are objects/shaders that are not handled by deferred, but are rendered with forward path
	for (int i=0; i<kVisibleListCount; i++)
	{
		RendererCullData& cullData = sceneCullParameters.renderers[i];
		for( size_t i = 0; i < cullData.rendererCount; ++i )
		{
			BaseRenderer* r = cullData.nodes[i].renderer;
			
			//Fix for case 570036. TODO: Look if we can catch the null Renderer case sooner 
			if (!r)
				continue;
			
			const int matCount = r->GetMaterialCount();
			for (int mi = 0; mi < matCount; ++mi)
			{
				Material* mat = r->GetMaterial (mi);
				// It is possible for this to be NULL if we have a missing reference.
				if (mat)
				{
					Shader* shader = mat->GetShader();
					int	ss = shader->GetShaderLabShader()->GetDefaultSubshaderIndex (kRenderPathExtPrePass);
					if (ss == -1)
						return true;
				}
			}
		}
	}
	return false;
}

////@TODO: Find a better name for this function

#include "Runtime/Graphics/LightmapSettings.h"

void Camera::PrepareSceneCullingParameters (const CameraCullingParameters& parameters, RenderingPath renderPath, CullResults& results)
{
	UmbraTomeData tomeData;
	if ((parameters.cullFlag & kCullFlagOcclusionCull) != 0)
		tomeData = GetScene().GetUmbraTome();

	SceneCullingParameters& sceneCullParameters = results.sceneCullParameters;

	if (parameters.cullingCamera->GetRenderImmediateObjects())
	{
		IntermediateRenderers& cameraIntermediate = parameters.cullingCamera->GetIntermediateRenderers();
		sceneCullParameters.renderers[kCameraIntermediate].bounds = cameraIntermediate.GetBoundingBoxes();
		sceneCullParameters.renderers[kCameraIntermediate].nodes = cameraIntermediate.GetSceneNodes();
		sceneCullParameters.renderers[kCameraIntermediate].rendererCount = cameraIntermediate.GetRendererCount();
	}
	else
	{
		sceneCullParameters.renderers[kStaticRenderers].bounds = GetScene().GetStaticBoundingBoxes();
		sceneCullParameters.renderers[kStaticRenderers].nodes = GetScene().GetStaticSceneNodes();
		sceneCullParameters.renderers[kStaticRenderers].rendererCount = GetScene().GetStaticObjectCount();

		sceneCullParameters.renderers[kDynamicRenderer].bounds = GetScene().GetDynamicBoundingBoxes();
		sceneCullParameters.renderers[kDynamicRenderer].nodes = GetScene().GetDynamicSceneNodes();
		sceneCullParameters.renderers[kDynamicRenderer].rendererCount = GetScene().GetDynamicObjectCount();

		IntermediateRenderers& sceneIntermediate = GetScene().GetIntermediateRenderers();
		sceneCullParameters.renderers[kSceneIntermediate].bounds = sceneIntermediate.GetBoundingBoxes();
		sceneCullParameters.renderers[kSceneIntermediate].nodes = sceneIntermediate.GetSceneNodes();
		sceneCullParameters.renderers[kSceneIntermediate].rendererCount = sceneIntermediate.GetRendererCount();

		IntermediateRenderers& cameraIntermediate = parameters.cullingCamera->GetIntermediateRenderers();
		sceneCullParameters.renderers[kCameraIntermediate].bounds = cameraIntermediate.GetBoundingBoxes();
		sceneCullParameters.renderers[kCameraIntermediate].nodes = cameraIntermediate.GetSceneNodes();
		sceneCullParameters.renderers[kCameraIntermediate].rendererCount = cameraIntermediate.GetRendererCount();

#if ENABLE_TERRAIN
		ITerrainManager* terrainManager = GetITerrainManager();
		if (terrainManager != NULL)
		{
			terrainManager->CollectTreeRenderers(results.treeSceneNodes, results.treeBoundingBoxes);
		}
		sceneCullParameters.renderers[kTreeRenderer].bounds = results.treeBoundingBoxes.data();
		sceneCullParameters.renderers[kTreeRenderer].nodes = results.treeSceneNodes.data();
		sceneCullParameters.renderers[kTreeRenderer].rendererCount = results.treeBoundingBoxes.size();
#endif

	}

	// Prepare cull results and allocate all culling memory
	results.Init(tomeData, sceneCullParameters.renderers);

	parameters.cullingCamera->CalculateCullingParameters(sceneCullParameters);

	sceneCullParameters.useOcclusionCulling = tomeData.HasTome();
	sceneCullParameters.sceneVisbilityForShadowCulling = &results.sceneCullingOutput;
	sceneCullParameters.umbraDebugRenderer = parameters.umbraDebugRenderer;
	sceneCullParameters.umbraDebugFlags = parameters.umbraDebugFlags;
	sceneCullParameters.umbraTome = tomeData;
	sceneCullParameters.umbraQuery = GetScene().GetUmbraQuery();
#if UNITY_EDITOR
	sceneCullParameters.filterMode = (CullFiltering)parameters.cullingCamera->m_FilterMode;
#endif

	// shadow culling is oly supported with the latest version of umbra (not with legacy umbra runtime)
	sceneCullParameters.useShadowCasterCulling = sceneCullParameters.useOcclusionCulling && tomeData.tome != NULL;
	sceneCullParameters.useLightOcclusionCulling = sceneCullParameters.useShadowCasterCulling;
	sceneCullParameters.cullLights = parameters.cullFlag & kCullFlagNeedsLighting;
	sceneCullParameters.excludeLightmappedShadowCasters = (renderPath == kRenderPathForward) ? !GetLightmapSettings().GetUseDualLightmapsInForward() : false;
	sceneCullParameters.cullPerObjectLights = IsCullPerObjectLightsNeeded(sceneCullParameters, renderPath);
	sceneCullParameters.renderPath = renderPath;

	// Prepare LOD data
	LODGroupManager& lodGroupManager = GetLODGroupManager();
	size_t lodGroupCount = lodGroupManager.GetLODGroupCount();
	results.lodMasks.resize_uninitialized(lodGroupCount);
	results.lodFades.resize_uninitialized(lodGroupCount);
	lodGroupManager.CalculateLODMasks(sceneCullParameters, results.lodMasks.begin(), results.lodFades.begin());

	sceneCullParameters.lodMasks = results.lodMasks.begin();
	sceneCullParameters.lodGroupCount = results.lodMasks.size();
}


void Camera::CustomCull (const CameraCullingParameters& parameters, CullResults& results)
{
	Assert(results.sceneCullingOutput.umbraVisibility == NULL);

	PROFILER_AUTO(gCameraCullProfile, this)

	// if camera's viewport rect is empty or invalid, do nothing
	if( !IsValidToRender() )
	{
		return;
	}

	// Send cull message to game object
	SendMessage (kPreCull);

	// OnPreCull message might disable the camera!
	// So we check one last time.
	bool enabledAndActive = IsActive() && GetEnabled();
	if (!enabledAndActive && (parameters.cullFlag & kCullFlagForceEvenIfCameraIsNotActive) == 0)
		return;

#if ENABLE_TERRAIN
	UInt32 cullingMask = m_CullingMask.m_Bits;
	ITerrainManager* terrainManager = GetITerrainManager();
	if (cullingMask != 0 && terrainManager != NULL && !GetRenderImmediateObjects())
	{
		// Pass culllingParameters instead
		terrainManager->CullAllTerrains(cullingMask);
	}
#endif // ENABLE_TERRAIN

	// Update scene dirty bounds
	GetScene().RecalculateDirtyBounds ();

	// Calculate parameters after OnPreCull (case 401765)
	// In case the user moves the camera in OnPreCull
	PrepareSceneCullingParameters(parameters, CalculateRenderingPath(), results);

	// Setup shader replacement
	if (parameters.explicitShaderReplace.replacementShader != NULL)
		results.shaderReplaceData = parameters.explicitShaderReplace;
	else
		InitShaderReplaceData (m_ReplacementShader, m_ReplacementTag, results.shaderReplaceData);

	// Prepare light culling information
	if (results.sceneCullParameters.cullLights)
	{
		ShadowCullData& shadowCullData = *UNITY_NEW(ShadowCullData, kMemTempAlloc);
		SetupShadowCullData(*parameters.cullingCamera, parameters.cullingCamera->GetPosition(), results.shaderReplaceData, &results.sceneCullParameters, shadowCullData);

		if (HasValidUmbraShadowCullingData (results.sceneCullingOutput.umbraVisibility, results.sceneCullParameters.umbraTome.tome))
			shadowCullData.visbilityForShadowCulling = &results.sceneCullingOutput;

		results.shadowCullData = &shadowCullData;
	}

	// Cull
	if (GetRenderImmediateObjects())
		CullIntermediateRenderersOnly(results.sceneCullParameters, results);
	else
		CullScene (results.sceneCullParameters, results);
}


void Camera::CalculateFarCullDistances (float* farCullDistances, float baseFarDistance) const
{
	// baseFarDistance is the distance of the far plane shifted to the camera position
	// This is so layer distances work properly even if the far distance is very large
	for(int i=0; i<kNumLayers; i++)
	{
		if(m_LayerCullDistances[i])
			farCullDistances[i] = baseFarDistance + m_LayerCullDistances[i];
		else
			farCullDistances[i] = baseFarDistance + m_FarClip;
	}
}

void Camera::DoRenderPostLayers ()
{
	FlareLayer* flareLayer = QueryComponent(FlareLayer);
	if (flareLayer && flareLayer->GetEnabled())
	{
		GetFlareManager().RenderFlares();
	}
	GetRenderManager().InvokeOnRenderObjectCallbacks ();
}

void Camera::DoRenderGUILayer()
{
	GUILayer* guiLayer = QueryComponent(GUILayer);
	if( guiLayer && guiLayer->GetEnabled())
		guiLayer->RenderGUILayer ();
}

void Camera::DoRender (CullResults& cullResults, PerformRenderFunction* customRender, int renderFlags)
{
	if (!IsValidToRender())
		return;

	PROFILER_AUTO_GFX(gCameraDrawProfile, this)

	// Shader replacement might be passed explicitly (camera.RenderWithShader), in which case we don't
	// send Pre/Post render events, and don't render image effects.
	// OR shader replacement can be setup as camera's state (camera.SetReplacementShader), in which case
	// camera functions as usually, just with shaders replaced.
	bool preAndPostRenderCallbacks = (renderFlags & kRenderFlagExplicitShaderReplace) == 0;

	if (preAndPostRenderCallbacks)
		SendMessage (kPreRender);


	RenderingPath renderPath = (RenderingPath)cullResults.sceneCullParameters.renderPath;

	// Render the culled contents
	if( customRender )
		customRender (*this, *m_RenderLoop, cullResults);
	else if (cullResults.shaderReplaceData.replacementShader != NULL)
	{
		bool useLitReplace = IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1);

		if (useLitReplace)
		{
			DoRenderLoop (*m_RenderLoop, renderPath, cullResults, GetRenderImmediateObjects ());
		}
		else
		{
			RenderTexture* rt = RenderTexture::GetActive();
			if(rt)
				GetGfxDevice().SetSRGBWrite(rt->GetSRGBReadWrite());
			else
				GetGfxDevice().SetSRGBWrite(GetActiveColorSpace()==kLinearColorSpace);

			ClearFramebuffer(GetClearFlags(),
			                 GetRenderRectangle(),
			                 ConvertColorToActiveColorSpace(m_BackGroundColor),
			                 (cullResults.shaderReplaceData.replacementShader) ? NULL : GetSkyboxMaterial());
			GetGfxDevice().SetSRGBWrite(false);

			RenderSceneShaderReplacement (cullResults.nodes, cullResults.shaderReplaceData);
		}
	}
	else
	{
		DoRenderLoop (*m_RenderLoop, renderPath, cullResults, GetRenderImmediateObjects ());
	}

	if (preAndPostRenderCallbacks)
		SendMessage (kPostRender);

	// The last renderer _might_ have toggled the back facing mode (to deal with mirrored geometry), so we reset this
	// in order to make the back facing well-defined.
	GetGfxDevice().SetNormalizationBackface( kNormalizationDisabled, false );

}


RenderingPath Camera::CalculateRenderingPath () const
{
	// Get rendering path from builds settings or per-camera params
	RenderingPath rp = (m_RenderingPath==-1) ?
		GetPlayerSettings().GetRenderingPathRuntime() :
		static_cast<RenderingPath>(m_RenderingPath);

	// Figure out what we can support on this hardware
	if (rp == kRenderPathPrePass)
	{
		bool canDoPrePass =
			gGraphicsCaps.hasPrePassRenderLoop &&	// basic GPU support
			!m_Orthographic &&													// can't be ortho
			RenderTexture::IsEnabled();				// render textures not disabled right now
		if (!canDoPrePass)
			rp = kRenderPathForward;
	}
	if (rp == kRenderPathForward)
	{
		bool canDoForwardShader = (gGraphicsCaps.shaderCaps >= kShaderLevel2);
		if (!canDoForwardShader)
			rp = kRenderPathVertex;
	}
	return rp;
}

bool Camera::CalculateNeedsToRenderIntoRT() const
{
	// Deferred needs to render into RT
	if (CalculateRenderingPath() == kRenderPathPrePass)
		return true;

	// If we have image filters between opaque & transparent, but no AA: render into RT;
	// much easier to share depth buffer between passes
	const bool aa = gGraphicsCaps.hasMultiSample && GetQualitySettings().GetCurrent().antiAliasing > 1;
	if (!aa && GetRenderLoopImageFilters(*m_RenderLoop).HasAfterOpaqueFilters())
		return true;

	return false;
}

int Camera::CalculateAntiAliasingForRT() const
{
	// Don't use MSAA for image effects if we're rendering to the back buffer
	// Maybe we should find a way to enable this if people really want it?
	// Previously there were no MSAA RTs so this wasn't an option
	if (!m_TargetTexture)
		return 1;

	if (!gGraphicsCaps.hasMultiSample)
		return 1;

	// Deferred is not compatible with MSAA
	if (CalculateRenderingPath() == kRenderPathPrePass)
		return 1;

	return m_TargetTexture->GetAntiAliasing();
}


void StoreRenderState (CameraRenderOldState& state)
{
	GfxDevice& device = GetGfxDevice();
	device.GetViewport(state.viewport);

	state.activeRT = RenderTexture::GetActive();
	state.camera = GetCurrentCameraPtr ();

	CopyMatrix(device.GetViewMatrix(), state.matView);
	CopyMatrix(device.GetWorldMatrix(), state.matWorld);
	CopyMatrix(device.GetProjectionMatrix(), state.matProj.GetPtr());
}

void RestoreRenderState (CameraRenderOldState& state)
{
	GfxDevice& device = GetGfxDevice();
	Camera* oldCamera = state.camera;
	GetRenderManager ().SetCurrentCamera (oldCamera);

	// We should not pass "prepare image effects" flag here, because we're restoring previous render texture
	// ourselves. I'm not sure if we should even call DoSetup on the camera; can't figure it out right now.
	if (oldCamera)
		oldCamera->SetupRender ();

	RenderTexture::SetActive(state.activeRT);
	device.SetViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
	device.SetViewMatrix(state.matView);
	device.SetWorldMatrix(state.matWorld);
	device.SetProjectionMatrix(state.matProj);
	SetClippingPlaneShaderProps();
}


void Camera::StandaloneRender( UInt32 renderFlags, Shader* replacementShader, const std::string& replacementTag )
{
	PROFILER_AUTO_GFX(gCameraRenderProfile, this)

	renderFlags |= kRenderFlagStandalone;

	RenderManager::UpdateAllRenderers();

	CameraRenderOldState state;
	if( !(renderFlags & kRenderFlagDontRestoreRenderState) )
		StoreRenderState(state);

	GetRenderManager().SetCurrentCamera (this);
	WindowSizeHasChanged ();

	CullResults cullResults;

	StandaloneCull(replacementShader, replacementTag, cullResults);

	// We may need BeginFrame() if we're called from script outside rendering loop (case 464376)
	AutoGfxDeviceBeginEndFrame frame;
	if( !frame.GetSuccess() )
		return;

	// Shader replacement might be passed explicitly (camera.RenderWithShader), in which case we don't
	// send Pre/Post render events, and don't render image effects.
	// OR shader replacement can be setup as camera's state (camera.SetReplacementShader), in which case
	// camera functions as usually, just with shaders replaced.
	if (replacementShader != NULL)
		renderFlags |= kRenderFlagExplicitShaderReplace;

	// Render this camera
	Render( cullResults, renderFlags );

	if( !(renderFlags & kRenderFlagDontRestoreRenderState) )
		RestoreRenderState(state);
}


static const Vector3f kCubemapOrthoBases[6*3] = {
	Vector3f( 0, 0,-1), Vector3f( 0,-1, 0), Vector3f(-1, 0, 0),
	Vector3f( 0, 0, 1), Vector3f( 0,-1, 0), Vector3f( 1, 0, 0),
	Vector3f( 1, 0, 0), Vector3f( 0, 0, 1), Vector3f( 0,-1, 0),
	Vector3f( 1, 0, 0), Vector3f( 0, 0,-1), Vector3f( 0, 1, 0),
	Vector3f( 1, 0, 0), Vector3f( 0,-1, 0), Vector3f( 0, 0,-1),
	Vector3f(-1, 0, 0), Vector3f( 0,-1, 0), Vector3f( 0, 0, 1),
};


bool Camera::StandaloneRenderToCubemap( RenderTexture* rt, int faceMask )
{
	PROFILER_AUTO_GFX(gCameraRenderToCubemapProfile, this)

	if (rt->GetDimension() != kTexDimCUBE)
	{
		ErrorString( "Render texture must be a cubemap" );
		return false;
	}
	if (!gGraphicsCaps.hasRenderToTexture || !gGraphicsCaps.hasRenderToCubemap || gGraphicsCaps.buggyCameraRenderToCubemap)
	{
		//ErrorString( "Render to cubemap is not supported on this hardware" );
		// Do not print the message; if returns false that means unsupported. No need to spam the console.
		return false;
	}

	CameraRenderOldState state;
	StoreRenderState (state);

	GetRenderManager().SetCurrentCamera (this);

	PPtr<RenderTexture> oldTargetTexture = m_TargetTexture;
	m_TargetTexture = rt;

	Matrix4x4f viewMatrix;

	// save FOV, aspect & render path (careful to not cause SetDirty)
	CameraTemporarySettings settings;
	GetTemporarySettings(settings);

	m_FieldOfView = 90.0f;
	m_Aspect = 1.0f;
	m_ImplicitAspect = false;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;

	// rendering into cubemap does not play well with deferred
	if (CalculateRenderingPath() == kRenderPathPrePass)
		m_RenderingPath = kRenderPathForward;
	DebugAssert (CalculateRenderingPath() != kRenderPathPrePass);

	// We may need BeginFrame() if we're called from script outside rendering loop (case 464376)
	AutoGfxDeviceBeginEndFrame frame;
	if( !frame.GetSuccess() )
		return false;

	GfxDevice& device = GetGfxDevice();

	// render each face
	Matrix4x4f translateMat;
	translateMat.SetTranslate( -GetComponent(Transform).GetPosition() );
	for( int i = 0; i < 6; ++i )
	{
		if( !(faceMask & (1<<i)) )
			continue;
		m_CurrentTargetFace = (CubemapFace)i;
		RenderTexture::SetActive (rt, 0, (CubemapFace)i);
		device.SetUserBackfaceMode( true ); // do this for each face (different contexts in GL!)
		viewMatrix.SetOrthoNormalBasisInverse( kCubemapOrthoBases[i*3+0], kCubemapOrthoBases[i*3+1], kCubemapOrthoBases[i*3+2] );
		viewMatrix *= translateMat;
		SetWorldToCameraMatrix( viewMatrix );

		CullResults cullResults;
		StandaloneCull( NULL, "", cullResults );

		Render( cullResults, kRenderFlagStandalone);
	}

	ResetWorldToCameraMatrix();

	// restore FOV, aspect & render path (careful to not cause SetDirty)
	SetTemporarySettings(settings);

	m_TargetTexture = oldTargetTexture;

	RestoreRenderState (state);
	device.SetUserBackfaceMode( false );

	return true;
}

bool Camera::StandaloneRenderToCubemap( Cubemap* cubemap, int faceMask )
{
	PROFILER_AUTO_GFX(gCameraRenderToCubemapProfile, this)

	if( !cubemap )
	{
		ErrorString( "Cubemap is null" );
		return false;
	}
	if( cubemap->GetTextureFormat() != kTexFormatARGB32 && cubemap->GetTextureFormat() != kTexFormatRGB24 )
	{
		ErrorString( "Unsupported cubemap format - needs to be ARGB32 or RGB24" );
		return false;
	}
	if (!gGraphicsCaps.hasRenderToTexture)
	{
		//ErrorString( "Render to cubemap is not supported on this hardware" );
		// Do not print the message; if returns false that means unsupported. No need to spam the console.
		return false;
	}

	RenderManager::UpdateAllRenderers();

	int size = cubemap->GetDataWidth();
	//UInt32 flags = RenderBufferManager::kRBCreatedFromScript;
	RenderTexture* rtt = GetRenderBufferManager().GetTempBuffer (size, size, kDepthFormat16, kRTFormatARGB32, 0, kRTReadWriteDefault);
	if( !rtt )
	{
		ErrorString( "Error while rendering to cubemap - failed to get temporary render texture" );
		return false;
	}

	CameraRenderOldState state;
	StoreRenderState (state);

	GetRenderManager().SetCurrentCamera (this);

	PPtr<RenderTexture> oldTargetTexture = m_TargetTexture;
	m_TargetTexture = rtt;

	Matrix4x4f viewMatrix;

	// save FOV, aspect & render path (careful to not cause SetDirty)
	CameraTemporarySettings oldSettings;
	GetTemporarySettings(oldSettings);
	m_FieldOfView = 90.0f;
	m_Aspect = 1.0f;
	m_ImplicitAspect = false;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
	// rendering into cubemap does not play well with deferred
	if (CalculateRenderingPath() == kRenderPathPrePass)
		m_RenderingPath = kRenderPathForward;
	DebugAssert (CalculateRenderingPath() != kRenderPathPrePass);

	// We may need BeginFrame() if we're called from script outside rendering loop (case 464376)
	AutoGfxDeviceBeginEndFrame frame;
	if( !frame.GetSuccess() )
		return false;

	GfxDevice& device = GetGfxDevice();

	// render each face
	Matrix4x4f translateMat;
	translateMat.SetTranslate( -GetComponent(Transform).GetPosition() );
	RenderTexture::SetActive( rtt );
	device.SetUserBackfaceMode( true );
	for( int i = 0; i < 6; ++i )
	{
		if( !(faceMask & (1<<i)) )
			continue;

		// render the cubemap face
		viewMatrix.SetOrthoNormalBasisInverse( kCubemapOrthoBases[i*3+0], kCubemapOrthoBases[i*3+1], kCubemapOrthoBases[i*3+2] );
		viewMatrix *= translateMat;
		SetWorldToCameraMatrix( viewMatrix );

		CullResults cullResults;
		StandaloneCull( NULL, "", cullResults );

		Render( cullResults, kRenderFlagStandalone );

		// Read back render texture into the cubemap face.
		// If projection matrix is flipped (happens on D3D), we have to flip
		// the image vertically so that result is correct.
		cubemap->ReadPixels( i, 0, 0, size, size, 0, 0, device.GetInvertProjectionMatrix(), false );
	}

	ResetWorldToCameraMatrix();

	SetTemporarySettings (oldSettings);

	m_TargetTexture = oldTargetTexture;

	RestoreRenderState (state);

	device.SetUserBackfaceMode( false );

	GetRenderBufferManager().ReleaseTempBuffer( rtt );

	// Rendering cubemaps takes place for a sRGB color space:
	cubemap->SetStoredColorSpace( kTexColorSpaceSRGB );

	cubemap->UpdateImageData();

	return true;
}

Shader* GetCameraDepthTextureShader ()
{
	Shader* depthShader = GetScriptMapper().FindShader("Hidden/Camera-DepthTexture");
	if (depthShader && !depthShader->IsSupported())
		depthShader = NULL;
	return depthShader;
}

Shader* GetCameraDepthNormalsTextureShader ()
{
	Shader* depthShader = GetScriptMapper().FindShader("Hidden/Camera-DepthNormalTexture");
	if (depthShader && !depthShader->IsSupported())
		depthShader = NULL;
	return depthShader;
}


void Camera::RenderDepthTexture (const CullResults& cullResults, RenderTexture** rt, RenderTextureFormat format, Shader* shader, const ColorRGBAf& clearColor, ShaderLab::FastPropertyName name)
{
	Assert (rt != NULL);

	if (!shader)
		return;

	GPU_AUTO_SECTION(kGPUSectionShadowPass);

	if (*rt)
	{
		GetRenderBufferManager().ReleaseTempBuffer (*rt);
		*rt = NULL;
	}

	DepthBufferFormat depthFormat = UNITY_PS3 ? kDepthFormat24 : kDepthFormat16;
	*rt = GetRenderBufferManager().GetTempBuffer (RenderBufferManager::kFullSize, RenderBufferManager::kFullSize, depthFormat, format, 0, kRTReadWriteLinear);
	if (!*rt)
		return;

	GfxDevice& device = GetGfxDevice();
	RenderTexture::SetActive (*rt);

	GraphicsHelper::Clear (kGfxClearAll, clearColor.GetPtr(), 1.0f, 0);
	GPU_TIMESTAMP();
	SetupRender ();

	RenderSceneShaderReplacement (cullResults.nodes, shader, "RenderType");

	// The last renderer _might_ have toggled the back facing mode (to deal with mirrored geometry), so we reset this
	// in order to make the back facing well-defined.
	device.SetNormalizationBackface( kNormalizationDisabled, false );

	ShaderLab::g_GlobalProperties->SetTexture (name, *rt);
}

void Camera::CleanupDepthTextures ()
{
	if (m_DepthTexture != NULL)
	{
		GetRenderBufferManager().ReleaseTempBuffer (m_DepthTexture);
		m_DepthTexture = NULL;
	}
	if (m_DepthNormalsTexture != NULL)
	{
		GetRenderBufferManager().ReleaseTempBuffer (m_DepthNormalsTexture);
		m_DepthNormalsTexture = NULL;
	}
}

void Camera::UpdateDepthTextures (const CullResults& cullResults)
{
	g_ShaderKeywords.Disable (kKeywordSoftParticles);
	bool softParticles = GetQualitySettings().GetCurrent().softParticles;

	UInt32 depthTexMask = m_DepthTextureMode;
	RenderingPath renderPath = CalculateRenderingPath();

	if (softParticles && renderPath == kRenderPathPrePass)
		g_ShaderKeywords.Enable (kKeywordSoftParticles);

	if (!gGraphicsCaps.hasStencilInDepthTexture && renderPath == kRenderPathPrePass)
		depthTexMask |= kDepthTexDepthBit; // prepass needs to generate depth texture if we don't have native capability

	// In case we need depth texture:
	// If HW supports native depth textures AND we're going to use light pre-pass:
	//     it comes for free, nothing extra to do.
	if ((depthTexMask & kDepthTexDepthBit) && renderPath == kRenderPathPrePass && gGraphicsCaps.hasStencilInDepthTexture)
		depthTexMask &= ~kDepthTexDepthBit;

	// In case we need depth+normals texture:
	// If we're going to use light pre-pass, it will built it for us. Nothing extra to do.
	if ((depthTexMask & kDepthTexDepthNormalsBit) && renderPath == kRenderPathPrePass)
		depthTexMask &= ~kDepthTexDepthNormalsBit;

	// No depth textures needed
	if (depthTexMask == 0)
		return;

	// Depth textures require some hardware support. We'll just say "need SM2.0+ and depth textures".
	if (!RenderTexture::IsEnabled() || (int)gGraphicsCaps.shaderCaps < kShaderLevel2 || !gGraphicsCaps.supportsRenderTextureFormat[kRTFormatDepth])
		return;

	if (softParticles && (depthTexMask & kDepthTexDepthBit))
		g_ShaderKeywords.Enable (kKeywordSoftParticles);

	// if camera's viewport rect is empty or invalid, do nothing
	if( !IsValidToRender() )
		return;

	Assert (depthTexMask != 0);
	Assert (m_DepthTexture == NULL && m_DepthNormalsTexture == NULL);

	if (depthTexMask & kDepthTexDepthBit)
	{
		PROFILER_AUTO_GFX(gCameraDepthTextureProfile, this)
		RenderDepthTexture (cullResults, &m_DepthTexture, kRTFormatDepth, GetCameraDepthTextureShader(), ColorRGBAf(1,1,1,1), kSLPropCameraDepthTexture);

	}
	if (depthTexMask & kDepthTexDepthNormalsBit)
	{
		PROFILER_AUTO_GFX(gCameraDepthNormalsTextureProfile, this)
		RenderDepthTexture (cullResults, &m_DepthNormalsTexture, kRTFormatARGB32, GetCameraDepthNormalsTextureShader(), ColorRGBAf(0.5f,0.5f,1,1), kSLPropCameraDepthNormalsTexture);
	}

#if GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30
	// when we are prepping image filters we need current rt info to determine formats of intermediate RTs
	// so we should reset info from possible depth path
	GfxDeviceRenderer renderer = GetGfxDevice().GetRenderer();
	if (renderer == kGfxRendererOpenGLES20Desktop || renderer == kGfxRendererOpenGLES20Mobile || renderer == kGfxRendererOpenGLES30)
	{
		if (depthTexMask & (kDepthTexDepthBit | kDepthTexDepthNormalsBit))
			RenderTexture::SetActive(m_CurrentTargetTexture);
	}
#endif
}


void Camera::StandaloneSetup ()
{
	GetRenderManager ().SetCurrentCamera (this);
	// This does not setup image filters! The usage pattern (e.g. terrain engine impostors) is:
	// Camera old = Camera.current;
	// newcamera.RenderDontRestore();
	//   ... render our stuff
	// Camera.SetupCurrent(old);
	//
	// So the last call should preserve whatever image filters were used before.
	SetupRender( kRenderFlagStandalone | kRenderFlagSetRenderTarget );
}

void Camera::Render (CullResults& cullResults, int renderFlags)
{
	// if camera's viewport rect is empty or invalid, do nothing
	if( !IsValidToRender () )
		return;

	if (m_IsRendering && IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_1_a3))
	{
		WarningStringObject("Attempting to render from a camera that is currently rendering. Create a copy of the camera (Camear.CopyFrom) if you wish to do this.", this);
		return;
	}

	m_IsRendering = true;

	Vector3f curPosition = GetPosition ();
	m_Velocity = (curPosition - m_LastPosition) * GetInvDeltaTime ();
	m_LastPosition = curPosition;
//	printf_console ("Rendering: %s\n", GetName().c_str());
	GetRenderManager ().SetCurrentCamera (this);

	// Update depth textures if needed
	UpdateDepthTextures (cullResults);

	const bool explicitReplacement = (renderFlags & kRenderFlagExplicitShaderReplace) != 0;

	// Setup for rendering, also sets render texture!
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_1_a3))
		SetupRender ( renderFlags | kRenderFlagPrepareImageFilters );
	else
		SetupRender ( renderFlags | (explicitReplacement ? 0 : kRenderFlagPrepareImageFilters) );

	DoRender ( cullResults, NULL, renderFlags ); // Render all geometry

	if ( (renderFlags & kRenderFlagStandalone) || GetEnabled() ) // camera may be already disabled here (OnPostRender)
	{
		//@TODO: This is inconsistent with renderGUILayer.
		//       Is there any reason for it?
		bool renderPostLayers = cullResults.shaderReplaceData.replacementShader == NULL;

		if (renderPostLayers)
			DoRenderPostLayers ();		// Handle any post-layer

		if (!explicitReplacement || IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_1_a3))
			RenderImageFilters (*m_RenderLoop, m_TargetTexture, false);
	}
	m_CurrentTargetTexture = m_TargetTexture;

	m_IsRendering = false;

	if ( (renderFlags & kRenderFlagStandalone) || GetEnabled() ) { // camera may be already disabled here (OnPostRender)
		// When camera is rendering with replacement that is part of state (and not explicitly passed in),
		// render GUI using regular shaders.
		if (!explicitReplacement)
			DoRenderGUILayer ();
	}

	// Clear camera's intermediate renderers here
	ClearIntermediateRenderers ();

	// Cleanup after all rendering
	CleanupAfterRenderLoop (*m_RenderLoop);

	CleanupDepthTextures ();
}

void Camera::AddImageFilter (const ImageFilter& filter)
{
	GetRenderLoopImageFilters(*m_RenderLoop).AddImageFilter (filter);
}

void Camera::RemoveImageFilter (const ImageFilter& filter)
{
	GetRenderLoopImageFilters(*m_RenderLoop).RemoveImageFilter (filter);
}


Ray Camera::ScreenPointToRay (const Vector2f& viewPortPos) const
{
	int viewPort[4];
	RectfToViewport (GetScreenViewportRect (), viewPort);

	Ray ray;
	Vector3f out;
	Matrix4x4f clipToWorld;
	GetClipToWorldMatrix( clipToWorld );

	const Matrix4x4f& camToWorld = GetCameraToWorldMatrix();
	if( !CameraUnProject( Vector3f(viewPortPos.x, viewPortPos.y, m_NearClip), camToWorld, clipToWorld, viewPort, out ) )
	{
		if(viewPort[0] > 0 || viewPort[1] > 0 || viewPort[2] > 0 || viewPort[3] > 0)
		{
			AssertString (Format("Screen position out of view frustum (screen pos %f, %f) (Camera rect %d %d %d %d)", viewPortPos.x, viewPortPos.y, viewPort[0], viewPort[1], viewPort[2], viewPort[3]));
		}
		return Ray (GetPosition(), Vector3f(0, 0, 1));
	}
	ray.SetOrigin( out );
	if( !CameraUnProject( Vector3f(viewPortPos.x, viewPortPos.y, m_NearClip + 1.0f), camToWorld, clipToWorld, viewPort, out ) )
	{
		if(viewPort[0] > 0 || viewPort[1] > 0 || viewPort[2] > 0 || viewPort[3] > 0)
		{
			AssertString (Format("Screen position out of view frustum (screen pos %f, %f) (Camera rect %d %d %d %d)", viewPortPos.x, viewPortPos.y, viewPort[0], viewPort[1], viewPort[2], viewPort[3]));
		}
		return Ray (GetPosition(), Vector3f(0, 0, 1));
	}
	Vector3f dir = out - ray.GetOrigin();
	ray.SetDirection (Normalize (dir));

	return ray;
}

Vector3f Camera::WorldToScreenPoint (const Vector3f& v, bool* canProject) const
{
	int viewPort[4];
	RectfToViewport (GetScreenViewportRect (), viewPort);

	Vector3f out;
	bool ok = CameraProject( v, GetCameraToWorldMatrix(), GetWorldToClipMatrix(), viewPort, out );
	if( canProject != NULL )
		*canProject = ok;
	return out;
}

Vector3f Camera::ScreenToWorldPoint (const Vector3f& v) const
{
	int viewPort[4];
	RectfToViewport( GetScreenViewportRect(), viewPort );

	Vector3f out;
	Matrix4x4f clipToWorld;
	GetClipToWorldMatrix( clipToWorld );
	if( !CameraUnProject( v, GetCameraToWorldMatrix(), clipToWorld, viewPort, out ) )
	{
		AssertString (Format("Screen position out of view frustum (screen pos %f, %f, %f) (Camera rect %d %d %d %d)", v.x, v.y, v.z, viewPort[0], viewPort[1], viewPort[2], viewPort[3]));
	}
	return out;
}

Vector3f Camera::WorldToViewportPoint (const Vector3f &worldPoint)  const {
	bool tempBool;
	Vector3f screenPoint = WorldToScreenPoint (worldPoint, &tempBool);
	return ScreenToViewportPoint (screenPoint);
}

Vector3f Camera::ViewportToWorldPoint (const Vector3f &viewPortPoint) const {
	Vector3f screenPoint = ViewportToScreenPoint (viewPortPoint);
	return ScreenToWorldPoint (screenPoint);
}

Vector3f Camera::ViewportToCameraPoint (const Vector3f &viewPort) const {
		Vector3f ndc;
		Matrix4x4f invProjection;
		Matrix4x4f::Invert_Full (GetProjectionMatrix(), invProjection);

		ndc.x = Lerp (-1, 1, viewPort.x);
		ndc.y = Lerp (-1, 1, viewPort.y);
		ndc.z = Lerp (-1, 1, (viewPort.z - m_NearClip) / m_FarClip);

		Vector3f cameraPoint;
		invProjection.PerspectiveMultiplyPoint3 (ndc, cameraPoint);

		cameraPoint.z = viewPort.z;

		return cameraPoint;
}

Ray Camera::ViewportPointToRay (const Vector2f& viewPortPos) const {
	Vector3f screenPos = ViewportToScreenPoint (Vector3f (viewPortPos.x, viewPortPos.y, 0.0F));
	return ScreenPointToRay (Vector2f (screenPos.x, screenPos.y));
}

Vector3f Camera::ScreenToViewportPoint (const Vector3f& screenPos) const
{
	Rectf r = GetScreenViewportRect ();
	float nx = (screenPos.x - r.x) / r.Width ();
	float ny = (screenPos.y - r.y) / r.Height ();
	return Vector3f (nx, ny, screenPos.z);
}

Vector3f Camera::ViewportToScreenPoint (const Vector3f& viewPos) const
{
	Rectf r = GetScreenViewportRect();
	float nx = viewPos.x * r.Width () + r.x;
	float ny = viewPos.y * r.Height () + r.y;
	return Vector3f (nx, ny, viewPos.z);
}

float Camera::CalculateFarPlaneWorldSpaceLength () const
{
	Rectf screenRect = GetScreenViewportRect ();
	Vector3f p0 = ScreenToWorldPoint (Vector3f (screenRect.x, screenRect.y, m_FarClip));
	Vector3f p1 = ScreenToWorldPoint (Vector3f (screenRect.x + screenRect.width, screenRect.y, m_FarClip));

	return Magnitude (p0 - p1);
}

float Camera::CalculateNearPlaneWorldSpaceLength () const
{
	Rectf screenRect = GetScreenViewportRect ();
	Vector3f p0 = ScreenToWorldPoint (Vector3f (screenRect.x, screenRect.y, m_NearClip));
	Vector3f p1 = ScreenToWorldPoint (Vector3f (screenRect.x + screenRect.width, screenRect.y, m_NearClip));
	return Magnitude (p0 - p1);
}

void Camera::SetDepth (float depth)
{
	SetDirty();
	m_Depth = depth;
	if (IsActive () && GetEnabled ()) {
		RemoveFromManager ();
		AddToManager ();
	}
}

void Camera::SetNormalizedViewportRect (const Rectf& normalizedRect) {
	SetDirty();
	m_NormalizedViewPortRect = normalizedRect;
	WindowSizeHasChanged ();
}

void Camera::WindowSizeHasChanged () {
	if (m_ImplicitAspect)
		ResetAspect ();
#if UNITY_WP8
	else
	{
		// Screen rotation is handled when the projection matrix is generated, so
		// it is necessary to mark it dirty here when a custom aspect ratio is set
		m_DirtyProjectionMatrix = true;
		m_DirtyWorldToClipMatrix = true;
	}
#endif
}

void Camera::SetAspect (float aspect)
{
	m_Aspect = aspect;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
	m_ImplicitAspect = false;
}

void Camera::ResetAspect ()
{
	Rectf r = GetScreenViewportRect();
	if (r.Height () != 0)
		m_Aspect = (r.Width () / r.Height ());
	else
		m_Aspect = 1.0f;

	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
	m_ImplicitAspect = true;
}

Vector3f Camera::GetPosition () const {
	return GetComponent (Transform).GetPosition();
}

inline bool IsMatrixValid (const Matrix4x4f& m)
{
	for (int i=0;i<16;i++)
	{
		if (!IsFinite (m.GetPtr ()[i]))
			return false;
	}
	return true;
}

const Matrix4x4f& Camera::GetWorldToCameraMatrix () const
{
	if( m_DirtyWorldToCameraMatrix && m_ImplicitWorldToCameraMatrix )
	{
		m_WorldToCameraMatrix.SetScale (Vector3f (1.0F, 1.0F, -1.0F));
		m_WorldToCameraMatrix *= GetComponent (Transform).GetWorldToLocalMatrixNoScale ();
		m_DirtyWorldToCameraMatrix = false;
	}
	return m_WorldToCameraMatrix;
}

Matrix4x4f Camera::GetCameraToWorldMatrix () const
{
	Matrix4x4f m;
	Matrix4x4f::Invert_Full( GetWorldToCameraMatrix(), m );
	return m;
}

const Matrix4x4f& Camera::GetProjectionMatrix () const
{
	if( m_DirtyProjectionMatrix && m_ImplicitProjectionMatrix )
	{
		if (!m_Orthographic)
			m_ProjectionMatrix.SetPerspective( m_FieldOfView, m_Aspect, m_NearClip, m_FarClip );
		else
			m_ProjectionMatrix.SetOrtho( -m_OrthographicSize * m_Aspect, m_OrthographicSize * m_Aspect, -m_OrthographicSize, m_OrthographicSize, m_NearClip, m_FarClip );

		#if UNITY_WP8
		// Off-screen cameras don't need to be rotated with device orientation changes (case 561859)
		if (GetTargetTexture() == NULL)
		{
			void RotateScreenIfNeeded(Matrix4x4f& mat);
			RotateScreenIfNeeded(m_ProjectionMatrix);
		}
		#endif
		m_DirtyProjectionMatrix = false;
	}
	return m_ProjectionMatrix;
}

void Camera::GetImplicitProjectionMatrix (float overrideNearPlane, Matrix4x4f& outMatrix) const
{
	if( !m_Orthographic )
		outMatrix.SetPerspective( m_FieldOfView, m_Aspect, overrideNearPlane, m_FarClip );
	else
		outMatrix.SetOrtho( -m_OrthographicSize * m_Aspect, m_OrthographicSize * m_Aspect, -m_OrthographicSize, m_OrthographicSize, overrideNearPlane, m_FarClip );
}


void Camera::GetImplicitProjectionMatrix (float overrideNearPlane, float overrideFarPlane, Matrix4x4f& outMatrix) const
{
	if( !m_Orthographic )
		outMatrix.SetPerspective( m_FieldOfView, m_Aspect, overrideNearPlane, overrideFarPlane );
	else
		outMatrix.SetOrtho( -m_OrthographicSize * m_Aspect, m_OrthographicSize * m_Aspect, -m_OrthographicSize, m_OrthographicSize, overrideNearPlane, overrideFarPlane );
}


void Camera::SetWorldToCameraMatrix (const Matrix4x4f& matrix)
{
	Assert (IsMatrixValid (matrix));
	m_WorldToCameraMatrix = matrix;
	m_ImplicitWorldToCameraMatrix = false;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetProjectionMatrix (const Matrix4x4f& matrix)
{
	Assert (IsMatrixValid (matrix));
	m_ProjectionMatrix = matrix;
	m_ImplicitProjectionMatrix = false;
	m_DirtyWorldToClipMatrix = true;
}

const Matrix4x4f& Camera::GetWorldToClipMatrix() const
{
	if( m_DirtyWorldToClipMatrix )
	{
		MultiplyMatrices4x4 (&GetProjectionMatrix(), &GetWorldToCameraMatrix(), &m_WorldToClipMatrix);
		m_DirtyWorldToClipMatrix = false;
	}
	return m_WorldToClipMatrix;
}

void Camera::GetClipToWorldMatrix( Matrix4x4f& outMatrix ) const
{
	Matrix4x4f::Invert_Full( GetWorldToClipMatrix(), outMatrix );
}

void Camera::SetTargetTextureBuffers(RenderTexture* tex, int colorCount, RenderSurfaceHandle* color, RenderSurfaceHandle depth, RenderTexture* rbOrigin)
{
	if (m_TargetTexture == PPtr<RenderTexture>(tex))
	{
		bool buffSame =		colorCount == m_TargetColorBufferCount
						&&	::memcmp(color, m_TargetColorBuffer, colorCount*sizeof(RenderSurfaceHandle)) == 0
						&&	depth == m_TargetDepthBuffer;

		if(tex != 0 || buffSame)
			return;
	}

	bool wasCurrent		= GetRenderManager().GetCurrentCameraPtr() == this;
	bool wasOffscreen 	= (RenderTexture*)m_TargetTexture != 0 || m_TargetBuffersOriginatedFrom != 0;

	m_TargetTexture = tex;

		::memcpy(m_TargetColorBuffer, color, colorCount*sizeof(RenderSurfaceHandle));
	if(colorCount < kMaxSupportedRenderTargets)
		::memset(m_TargetColorBuffer+colorCount, 0x00, (kMaxSupportedRenderTargets-colorCount)*sizeof(RenderSurfaceHandle));

	m_TargetColorBufferCount = colorCount;
	m_TargetDepthBuffer = depth;
	m_TargetBuffersOriginatedFrom = rbOrigin;

	SetDirty();
	if (IsAddedToManager ()) {
		GetRenderManager().RemoveCamera (this);
		GetRenderManager().AddCamera (this);

		// special case: if we were rendering to offscreen camera and changed rt in process - reactivate it
		// other possible cases:
		//    wasn't current: nothing changes (? maybe check that was rendered already, what was the intention?)
		//    onscreen -> offscreen: we shouldn'd draw in here in that pass (and if we really wants?)
		//    offscreen -> onscreen: will be correctly drawn next time we draw onscreen cameras
		if( wasCurrent && wasOffscreen && (tex || rbOrigin) )
			GetRenderManager().SetCurrentCamera(this);
	}
}

void Camera::SetTargetBuffers (int colorCount, RenderSurfaceHandle* color, RenderSurfaceHandle depth, RenderTexture* originatedFrom)
{
	SetTargetTextureBuffers(0, colorCount, color, depth, originatedFrom);
}

void Camera::SetTargetBuffersScript (int colorCount, const ScriptingRenderBuffer* colorScript, ScriptingRenderBuffer* depthScript)
{
	#define RETURN_WITH_ERROR(msg)	do { ErrorString(msg); return; } while(0)

	RenderSurfaceHandle color[kMaxSupportedRenderTargets];
	for (int i = 0; i < colorCount; ++i)
		color[i] = colorScript[i].m_BufferPtr ? RenderSurfaceHandle(colorScript[i].m_BufferPtr) : GetGfxDevice().GetBackBufferColorSurface();

	RenderSurfaceHandle depth = depthScript->m_BufferPtr ? RenderSurfaceHandle(depthScript->m_BufferPtr) : GetGfxDevice().GetBackBufferDepthSurface();

	// check rt/screen originated (cant mix)
	// TODO: maybe we should simply check backBuffer flag?
	PPtr<RenderTexture> originatedFrom(colorScript[0].m_RenderTextureInstanceID);
	{
		bool onScreen = originatedFrom.IsNull();
		for(int i = 1 ; i < colorCount ; ++i)
		{
			if( PPtr<RenderTexture>(colorScript[i].m_RenderTextureInstanceID).IsNull() != onScreen )
				RETURN_WITH_ERROR("You're trying to mix color buffers from RenderTexture and from screen.");
		}

		if( PPtr<RenderTexture>(depthScript->m_RenderTextureInstanceID).IsNull() != onScreen )
			RETURN_WITH_ERROR("You're trying to mix color and depth buffers from RenderTexture and from screen.");
	}

	// check that we have matching exts
	{
		int colorW = color[0].object->width;
		int colorH = color[0].object->height;
		int depthW = depth.object->width;
		int depthH = depth.object->height;

		for(int i = 1 ; i < colorCount ; ++i)
		{
			int w = color[i].object->width;
			int h = color[i].object->height;
			if(colorW != w || colorH != h)
				RETURN_WITH_ERROR("Camera.SetTargetBuffers can only accept RenderBuffers with same size.");
		}

		if(colorW != depthW || colorH != depthH)
			RETURN_WITH_ERROR("Camera.SetTargetBuffers can only accept RenderBuffers with same size.");
	}

	SetTargetTextureBuffers(0, colorCount, color, depth, PPtr<RenderTexture>(colorScript[0].m_RenderTextureInstanceID));

	return;
	#undef RETURN_WITH_ERROR
}


void Camera::SetTargetTexture (RenderTexture *tex)
{
	RenderSurfaceHandle color = tex ? tex->GetColorSurfaceHandle() : GetGfxDevice().GetBackBufferColorSurface();
	RenderSurfaceHandle depth = tex ? tex->GetDepthSurfaceHandle() : GetGfxDevice().GetBackBufferDepthSurface();
	SetTargetTextureBuffers(tex, 1, &color, depth, 0);
}

void Camera::CopyFrom( const Camera& other )
{
	// copy transform from other
	Transform& transform = GetComponent (Transform);
	const Transform& otherTransform = other.GetComponent (Transform);
	transform.SetLocalScale( otherTransform.GetLocalScale() );
	transform.SetPosition( otherTransform.GetPosition() );

	// normalize this... the camera can come from a very deep
	// hierarchy. This can lead to float rounding errors :(
	Quaternionf quat = Normalize (otherTransform.GetRotation());
	transform.SetRotation(quat);

	// copy layer of this gameobject from other
	GetGameObject().SetLayer( other.GetGameObject().GetLayer() );

	// copy camera's variables from other
	m_ClearFlags = other.m_ClearFlags;
	m_BackGroundColor = other.m_BackGroundColor;
	m_NormalizedViewPortRect = other.m_NormalizedViewPortRect;
	m_CullingMask = other.m_CullingMask;
	m_EventMask = other.m_EventMask;

	m_Depth = other.m_Depth;
	m_Velocity = other.m_Velocity;
	m_LastPosition = other.m_LastPosition;
	m_OrthographicSize = other.m_OrthographicSize;
	m_FieldOfView = other.m_FieldOfView;
	m_NearClip = other.m_NearClip;
	m_FarClip = other.m_FarClip;
	m_Aspect = other.m_Aspect;

	m_WorldToCameraMatrix = other.m_WorldToCameraMatrix;
	m_ProjectionMatrix = other.m_ProjectionMatrix;
	m_WorldToClipMatrix = other.m_WorldToClipMatrix;
	m_DirtyWorldToCameraMatrix = other.m_DirtyWorldToCameraMatrix;
	m_DirtyProjectionMatrix = other.m_DirtyProjectionMatrix;
	m_DirtyWorldToClipMatrix = other.m_DirtyWorldToClipMatrix;
	m_ImplicitWorldToCameraMatrix = other.m_ImplicitWorldToCameraMatrix;
	m_ImplicitAspect = other.m_ImplicitAspect;
	m_Orthographic = other.m_Orthographic;

	m_TargetTexture = other.m_TargetTexture;
	m_CurrentTargetTexture = other.m_CurrentTargetTexture;
	m_TargetColorBufferCount = other.m_TargetColorBufferCount;
	::memcpy(m_TargetColorBuffer, other.m_TargetColorBuffer, sizeof(m_TargetColorBuffer));
	m_TargetDepthBuffer = other.m_TargetDepthBuffer;
	m_TargetBuffersOriginatedFrom = other.m_TargetBuffersOriginatedFrom;

	m_ReplacementShader = other.m_ReplacementShader;
	m_ReplacementTag = other.m_ReplacementTag;

	m_DepthTextureMode = other.m_DepthTextureMode;
	m_ClearStencilAfterLightingPass = other.m_ClearStencilAfterLightingPass;

	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
	{
		m_RenderingPath = other.m_RenderingPath;
		memcpy(m_LayerCullDistances, other.m_LayerCullDistances, sizeof(m_LayerCullDistances));
		m_SortMode = other.m_SortMode;
		m_LayerCullSpherical = other.m_LayerCullSpherical;
		m_OcclusionCulling = other.m_OcclusionCulling;
		m_ImplicitProjectionMatrix = other.m_ImplicitProjectionMatrix;
	}

#if UNITY_EDITOR
	m_AnimateMaterials = other.m_AnimateMaterials;
	m_AnimateMaterialsTime = other.m_AnimateMaterialsTime;
#endif

	SetDirty();
}

void Camera::GetTemporarySettings (CameraTemporarySettings& settings) const
{
	settings.renderingPath = m_RenderingPath;
	settings.fieldOfView = m_FieldOfView;
	settings.aspect = m_Aspect;
	settings.implicitAspect = m_ImplicitAspect;
}

void Camera::SetTemporarySettings (const CameraTemporarySettings& settings)
{
	m_RenderingPath = settings.renderingPath;
	m_FieldOfView = settings.fieldOfView;
	m_Aspect = settings.aspect;
	m_ImplicitAspect = settings.implicitAspect;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetReplacementShader( Shader* shader, const std::string& replacementTag )
{
	m_ReplacementShader = shader;
	m_ReplacementTag = replacementTag;
}

void Camera::SetFov (float deg)
{
	SetDirty();
	m_FieldOfView = deg;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetNear (float n)
{
	SetDirty();
	m_NearClip = n;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetFar (float f)
{
	SetDirty();
	m_FarClip = f;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

static bool IsNonStandardProjection(const Matrix4x4f& mat)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			if (i != j && mat.Get(i, j) != 0.0f)
				return true;
	return false;
}

float Camera::GetProjectionNear () const
{
	if (m_ImplicitProjectionMatrix)
		return m_NearClip;

	const Matrix4x4f& proj = GetProjectionMatrix();
	if (IsNonStandardProjection(proj))
		return m_NearClip;

	Vector4f nearPlane = proj.GetRow(3) + proj.GetRow(2);
	Vector3f nearNormal(nearPlane.x, nearPlane.y, nearPlane.z);
	return -nearPlane.w / Magnitude(nearNormal);
}

float Camera::GetProjectionFar () const
{
	if (m_ImplicitProjectionMatrix)
		return m_FarClip;

	const Matrix4x4f& proj = GetProjectionMatrix();
	if (IsNonStandardProjection(proj))
		return m_FarClip;

	Vector4f farPlane = proj.GetRow(3) - proj.GetRow(2);
	Vector3f farNormal(farPlane.x, farPlane.y, farPlane.z);
	return farPlane.w / Magnitude(farNormal);
}

bool Camera::CalculateUsingHDR () const
{
	// some HDR sanity checks
	return (m_HDR &&
			GetBuildSettings().hasRenderTexture && gGraphicsCaps.supportsRenderTextureFormat[GetGfxDevice().GetDefaultHDRRTFormat()] &&
			(!GetQualitySettings().GetCurrent().antiAliasing || (CalculateRenderingPath () == kRenderPathPrePass)));
}

void Camera::SetOrthographicSize (float f)
{
	SetDirty();
	m_OrthographicSize = f;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetOrthographic (bool v)
{
	SetDirty();
	m_Orthographic = v;
	m_DirtyProjectionMatrix = true;
	m_DirtyWorldToClipMatrix = true;
}

void Camera::SetBackgroundColor (const ColorRGBAf& color)
{
	m_BackGroundColor = color;
	SetDirty();
}

void Camera::SetClearFlags (int flags)
{
	m_ClearFlags = flags;
	SetDirty();
}

void Camera::SetCullingMask (UInt32 cullingMask)
{
	m_CullingMask.m_Bits = cullingMask;
	SetDirty();
}

void Camera::SetEventMask (UInt32 eventMask)
{
	m_EventMask.m_Bits = eventMask;
	SetDirty();
}

void Camera::DisplayHDRWarnings() const
{
	if((GetQualitySettings().GetCurrent().antiAliasing > 0) && (CalculateRenderingPath () == kRenderPathForward))
		WarningStringObject("HDR and MultisampleAntiAliasing (in Forward Rendering Path) is not supported. This camera will render without HDR buffers. Disable Antialiasing in the Quality settings if you want to use HDR.", this);
	if((!gGraphicsCaps.supportsRenderTextureFormat[GetGfxDevice().GetDefaultHDRRTFormat()]) || (!GetBuildSettings().hasRenderTexture))
		WarningStringObject("HDR RenderTexture format is not supported on this platform. This camera will render without HDR buffers.", this);
}

bool Camera::GetRenderImmediateObjects () const
{
#if UNITY_EDITOR
	return m_OnlyRenderIntermediateObjects;
#else
	return false;
#endif
}

#if UNITY_EDITOR
bool Camera::IsFiltered (Unity::GameObject& gameObject) const
{
	return IsGameObjectFiltered (gameObject, (CullFiltering)m_FilterMode);
}
#endif

IMPLEMENT_CLASS_HAS_INIT (Camera)
IMPLEMENT_OBJECT_SERIALIZE (Camera)


void ClearWithSkybox (bool clearDepth, Camera const* camera)
{
	if (!camera)
		return;

	Material* skybox = camera->GetSkyboxMaterial ();
	if (!skybox)
		return;

	GfxDevice& device = GetGfxDevice();
	device.SetProjectionMatrix (camera->GetProjectionMatrix());
	device.SetViewMatrix (camera->GetWorldToCameraMatrix().GetPtr());
	SetClippingPlaneShaderProps();

	if (clearDepth) {
		float zero[4] = {0,0,0,0};
		GraphicsHelper::Clear (kGfxClearDepthStencil, zero, 1.0f, 0);
		GPU_TIMESTAMP();
	}
	Skybox::RenderSkybox (skybox, *camera);
}


Rectf GetCameraOrWindowRect (const Camera* camera)
{
	if (camera)
		return camera->GetScreenViewportRect ();
	else
	{
		Rectf rect = GetRenderManager().GetWindowRect();
		rect.x = rect.y = 0.0f;
		return rect;
	}
}

/*
Texture* Camera::GetUmbraOcclusionBufferTexture ()
{
	Umbra::OcclusionBuffer::BufferDesc SrcDesc;
	Umbra::OcclusionBuffer* occlusionBuffer = m_UmbraVisibility->getOutputBuffer();
	occlusionBuffer->getBufferDesc(SrcDesc);

	if (!SrcDesc.width || !SrcDesc.height)
		return NULL;

	Assert(SrcDesc.width == 128 && SrcDesc.height == 128);

	UInt8 TempBuffer[128*128];
	occlusionBuffer->getBuffer(TempBuffer);

	if (!m_OcclusionBufferTexture)
	{
		m_OcclusionBufferTexture = CreateObjectFromCode<Texture2D>();
		m_OcclusionBufferTexture->InitTexture(SrcDesc.width, SrcDesc.height, kTexFormatRGBA32, 0);
	}

	ImageReference DstDesc;
	m_OcclusionBufferTexture->GetWriteImageReference(&DstDesc, 0, 0);

	UInt32* DstBuffer = (UInt32*)DstDesc.GetImageData();
	int DstStride = DstDesc.GetRowBytes();

	for (int y=0; y<DstDesc.GetHeight(); y++)
	{
		for (int x=0; x<DstDesc.GetWidth(); x++)
		{
			int c = (int)TempBuffer[y*128+x];
			int a = 0xff;
#if UNITY_LITTLE_ENDIAN
			DstBuffer[y*DstDesc.GetWidth()+x] = (a<<24) + (c<<16) + (c<<8) + c;
#else
			DstBuffer[y*DstDesc.GetWidth()+x] = (c<<24) + (c<<16) + (c<<8) + a;
#endif
		}
	}

	return dynamic_pptr_cast<Texture*>(m_OcclusionBufferTexture);
}
 */
