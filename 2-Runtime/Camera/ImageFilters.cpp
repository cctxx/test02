#include "UnityPrefix.h"
#include "ImageFilters.h"
#include "Renderable.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Shaders/Material.h"
#include "CameraUtil.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"

PROFILER_INFORMATION(gImageFxProfile, "Camera.ImageEffects", kProfilerRender);
PROFILER_INFORMATION(gGraphicsBlitProfile, "Graphics.Blit", kProfilerRender);

namespace ImageFilters_Static
{

static SHADERPROP(MainTex);

} // namespace ImageFilters_Static

static bool s_InsideFilterChain = false;
static RenderTexture* s_CurrentSrcRT;
static RenderTexture* s_CurrentFinalRT;


// -----------------------------------------------------------------------------

static int GetImageFilterSortIndex (Unity::Component* component)
{
	GameObject* go = component->GetGameObjectPtr();
	int count = go ? go->GetComponentCount() : 0;
	for (int i = 0; i < count; ++i)
	{
		if (&go->GetComponentAtIndex(i) == component)
			return i;
	}
	return -1;
}


void ImageFilters::AddImageFilter (const ImageFilter& filter)
{
	// When importing a package over a live image filter, it does not get removed.
	// So remove them explicitly instead of adding it multiple times.
	#if UNITY_EDITOR
	RemoveImageFilter (filter);
	#endif

	Filters& filters = filter.afterOpaque ? m_AfterOpaque : m_AfterEverything;

	// Insert the image filter by sort index.
	// Search backwards because in most cases the filters are added in sorted order.
	int insertIndex = GetImageFilterSortIndex(filter.component);
	for( int i = filters.size()-1; i >= 0; --i )
	{
		if (insertIndex >= GetImageFilterSortIndex(filters[i].component))
		{
			Filters::iterator insertion = filters.begin() + i + 1;
			filters.insert (insertion, filter);
			return;
		}
	}
	filters.insert (filters.begin(), filter);
}

void ImageFilters::RemoveImageFilter (const ImageFilter& filter)
{
	for (Filters::iterator i = m_AfterOpaque.begin(); i != m_AfterOpaque.end(); /**/)
	{
		if (*i == filter)
			i = m_AfterOpaque.erase (i);
		else
			++i;
	}
	for (Filters::iterator i = m_AfterEverything.begin(); i != m_AfterEverything.end(); /**/)
	{
		if (*i == filter)
			i = m_AfterEverything.erase (i);
		else
			++i;
	}
}

RenderTexture* ImageFilters::GetTargetBeforeOpaque ()
{
	return m_FirstTargetTexture;
}

RenderTexture* ImageFilters::GetTargetAfterOpaque (bool forceIntoRT, bool usingScreenToComposite)
{
	if (m_AfterOpaque.empty())
		return m_FirstTargetTexture;
	if (m_AfterEverything.empty() && !forceIntoRT)
		return m_FirstTargetTexture;
	if (usingScreenToComposite && !forceIntoRT)
		return m_FirstTargetTexture;
	return m_SecondTargetTexture;
}

RenderTexture* ImageFilters::GetTargetFinal ()
{
	return m_FinalTargetTexture;
}

static RenderTexture* GetTemporaryRT (bool depthBuffer, bool requestHDR = false, bool requestLinear = false, int antiAliasing = 1)
{
	RenderBufferManager& rbm = GetRenderBufferManager ();
	RenderTexture* rt = rbm.GetTempBuffer (
		RenderBufferManager::kFullSize,
		RenderBufferManager::kFullSize,
		depthBuffer ? kDepthFormat24 : kDepthFormatNone,
		// by gl/gles spec blitting won't work if dst have components missing from src
		// which is the case often on mobiles
		requestHDR ? GetGfxDevice().GetDefaultHDRRTFormat() : GetGfxDevice().GetDefaultRTFormat(),
		0,
		(requestLinear && !requestHDR) ? kRTReadWriteSRGB : kRTReadWriteLinear,
		antiAliasing);

	if (rt) rt->CorrectVerticalTexelSize(true);
	return rt;
}

RenderTexture* ImageFilters::SwitchTargetToLDR (RenderTexture* oldRt, bool requestLinear)
{
	if(!oldRt)
		return NULL;

	RenderTexture* newRt = GetTemporaryRT (false, false, requestLinear);
	if (oldRt == m_FirstTargetTexture) {
		GetRenderBufferManager().ReleaseTempBuffer (oldRt);
		m_FirstTargetTexture = newRt;
	}
	else if (oldRt == m_SecondTargetTexture) {
		GetRenderBufferManager().ReleaseTempBuffer (oldRt);
		m_SecondTargetTexture = newRt;
	}
	else {
		GetRenderBufferManager().ReleaseTempBuffer (oldRt);
	}

	return newRt;
}

void ImageFilters::ReleaseTargetForLDR (RenderTexture** oldRt)
{
	if(!(*oldRt))
		return;

	RenderTexture* rt = *oldRt;

	GetRenderBufferManager().ReleaseTempBuffer (rt);
	if(rt == m_FirstTargetTexture)
		m_FirstTargetTexture = NULL;
	if(rt == m_SecondTargetTexture)
		m_SecondTargetTexture = NULL;

	*oldRt = NULL;
}

void ImageFilters::Prepare (bool forceIntoRT, bool hdr, int antiAliasing)
{
	Assert (!m_FirstTargetTexture && !m_SecondTargetTexture);

	// Nothing to do if we have no image filters
	if (!HasImageFilter() && !forceIntoRT)
		return;

	// Ignore image filters if we can't use them
	if (!RenderTexture::IsEnabled() || (gGraphicsCaps.npotRT == kNPOTNone))
	{
		static bool errorShown = false;
		if( !errorShown )
		{
			ErrorString("can't use image filters (npot-RT are not supported or RT are disabled completely)");
			errorShown = true;
		}
		return;
	}

	bool linearColorSpace = GetActiveColorSpace() == kLinearColorSpace;

	m_FirstTargetTexture = GetTemporaryRT (true, hdr, linearColorSpace, antiAliasing);

	// find out if we're still rendering HDR after opaque
	bool hdrAfterOpaque = hdr;
	Filters& filters = m_AfterOpaque;
	size_t n = filters.size();
	for (size_t i = 0; i < n; ++i)
		if (filters[i].transformsToLDR)
			hdrAfterOpaque = false;

	// we need second target only if have both after-opaque
	//if (!m_AfterOpaque.empty())
		m_SecondTargetTexture = GetTemporaryRT (false, hdrAfterOpaque, linearColorSpace, antiAliasing);
}


static void GetDestRenderTargetSurfaces (RenderTexture* dest, RenderSurfaceHandle& outColor, RenderSurfaceHandle& outDepth)
{
	if (dest && !dest->IsCreated())
		dest->Create();

	// Ugly hack: when we have image filters that are between opaque & transparent geometry,
	// we really want to share the depth buffer for before/after rendering of that. However
	// the current scripting API does not allow doing that!
	//
	// So try to detect this situation: if we're inside of image filters loop and destination
	// is not the first target: use depth from the first one.
	if (s_InsideFilterChain && dest && s_CurrentSrcRT != NULL && dest == s_CurrentFinalRT && dest->GetWidth()==s_CurrentSrcRT->GetWidth() && dest->GetHeight()==s_CurrentSrcRT->GetHeight())
	{
		// one more ugly hack (do we even need to mark ugly hacks?)
		// RenderBufferManager returns non-created RT, so at this point we can end up with non-created s_CurrentSrcRT
		//   e.g. first run of image filters
		// so create it before getting depth surface
		if(!s_CurrentSrcRT->IsCreated())
			s_CurrentSrcRT->Create();

		outColor = dest->GetColorSurfaceHandle();
		outDepth = s_CurrentSrcRT->GetDepthSurfaceHandle();
	}
	else if (dest)
	{
		outColor = dest->GetColorSurfaceHandle();
		outDepth = dest->GetDepthSurfaceHandle();
	}
	else
	{
		outColor = GetGfxDevice().GetBackBufferColorSurface();
		outDepth = GetGfxDevice().GetBackBufferDepthSurface();
	}
}


void ImageFilters::DoRender (RenderTexture* finalRT, bool forceIntoRT, bool afterOpaque, bool usingScreenToComposite, bool hdr)
{
	// Ignore image filters if we can't use them
	if (!RenderTexture::IsEnabled() || (gGraphicsCaps.npotRT == kNPOTNone))
		return;

	PROFILER_AUTO_GFX(gImageFxProfile, NULL)
	GPU_AUTO_SECTION(kGPUSectionPostProcess);

	bool buffersInHDR = hdr;
	bool linearColorSpace = GetActiveColorSpace() == kLinearColorSpace;

	RenderBufferManager& rbm = GetRenderBufferManager();
	RenderTexture* srcRT = NULL;
	RenderTexture* dstRT = NULL;

	m_FinalTargetTexture = GetGfxDevice().GetActiveRenderTexture();

	if (afterOpaque)
	{
		srcRT = m_FirstTargetTexture;
		if ((!m_AfterEverything.empty() && !usingScreenToComposite) || forceIntoRT)
			finalRT = m_SecondTargetTexture;
	}
	else
	{
		srcRT = GetTargetAfterOpaque(forceIntoRT, usingScreenToComposite);
	}

	// store current values of global state (to make re-entrancy work)
	bool oldInside = s_InsideFilterChain;
	RenderTexture *oldSrcRT = s_CurrentSrcRT;
	RenderTexture *oldFinalRT = s_CurrentFinalRT;
	s_InsideFilterChain = false;
	s_CurrentSrcRT = srcRT;
	s_CurrentFinalRT = finalRT;
	GfxDevice& device = GetGfxDevice();

	Filters& filters = afterOpaque ? m_AfterOpaque : m_AfterEverything;
	size_t n = filters.size();
	for (size_t i = 0; i < n; ++i)
	{
		RenderTexture* dst;
		if (i == n-1)
			dst = finalRT;
		else
		{
			if (filters[i].transformsToLDR && buffersInHDR) {
				buffersInHDR = false;
				dstRT = SwitchTargetToLDR(dstRT, linearColorSpace);
			}
			if (!dstRT)
			{
				dstRT = GetTemporaryRT(false, buffersInHDR, linearColorSpace);
			}
			dst = dstRT;
		}


		// Render one image effect

		s_InsideFilterChain = true;
		PROFILER_AUTO_GFX(gImageFxProfile, filters[i].component);

		// Discard any destination RT contents before rendering the effect into it
		// NB: do not discard back buffer here
		RenderSurfaceHandle dstRsColor, dstRsDepth;
		GetDestRenderTargetSurfaces (dst, dstRsColor, dstRsDepth);
		if(!dstRsColor.object->backBuffer)
			device.DiscardContents (dstRsColor);
		// However, do not discard depth if we're in the opaque image effects
		// stage and it's our final destination depth - we will still need
		// it for later alpha rendering.
		if (dstRsDepth != s_CurrentSrcRT->GetDepthSurfaceHandle() && !dstRsDepth.object->backBuffer)
			device.DiscardContents (dstRsDepth);
		else
			device.IgnoreNextUnresolveOnRS (dstRsDepth); // we'll have to un-resolve it, so silence up the warning

		// Invoke actual image effect function
		filters[i].renderFunc (filters[i].component, srcRT, dst);
		s_InsideFilterChain = false;


		// if we have just converted to LDR, we need to completely switch to LDR, so let's release src
		if (filters[i].transformsToLDR && hdr && !buffersInHDR) {
			//srcRT = SwitchTargetToLDR(srcRT, linearColorSpace);
			if(srcRT)
				ReleaseTargetForLDR(&srcRT);
		}

		// We are ping-ponging between textures when there are more than 2 image filters.
		// If the very first one was AA-resolved, it's upside down flip is already handled.
		if (srcRT) srcRT->CorrectVerticalTexelSize(true);

		std::swap (srcRT, dstRT);
	}

	bool needsBlitIntoFinalRT = !afterOpaque && forceIntoRT && filters.empty();

	if (needsBlitIntoFinalRT)
	{
		ImageFilters::Blit (srcRT, finalRT);
	}

	if (n > 0 || needsBlitIntoFinalRT)
	{
		// we actually rendered into finalRT
		m_FinalTargetTexture = finalRT;
	}

	if (dstRT && dstRT != m_FirstTargetTexture && dstRT != m_SecondTargetTexture)
		rbm.ReleaseTempBuffer (dstRT);

	if (srcRT && srcRT != m_FirstTargetTexture && srcRT != m_SecondTargetTexture)
		rbm.ReleaseTempBuffer (srcRT);

	if (!afterOpaque)
	{
		if (m_FirstTargetTexture)
		{
			rbm.ReleaseTempBuffer (m_FirstTargetTexture);
			m_FirstTargetTexture = NULL;
		}
		if (m_SecondTargetTexture)
		{
			rbm.ReleaseTempBuffer (m_SecondTargetTexture);
			if (m_SecondTargetTexture == m_FinalTargetTexture)
				m_FinalTargetTexture = NULL;
			m_SecondTargetTexture = NULL;
		}
	}

	GetGfxDevice().SetSRGBWrite(false);

	// resstore old values of global state
	s_InsideFilterChain = oldInside;
	s_CurrentSrcRT = oldSrcRT;
	s_CurrentFinalRT = oldFinalRT;
}

// -----------------------------------------------------------------------------

void ImageFilters::Blit (Texture* source, RenderTexture* dest)
{
	static Material* s_BlitMaterial = NULL;
	if (!s_BlitMaterial){
		Shader* shader = GetScriptMapper().FindShader ("Hidden/BlitCopy");
		s_BlitMaterial = Material::CreateMaterial (*shader, Object::kHideAndDontSave);
	}
	Blit (source, dest, s_BlitMaterial, -1, true);
}

void ImageFilters::DrawQuadNoGPUTimestamp (GfxDevice& device, bool invertY, float uvX, float uvY)
{
	device.ImmediateBegin (kPrimitiveQuads);
	float y1, y2;
	if (invertY) {
		y1 = uvY; y2 = 0.0f;
	} else {
		y1 = 0.0f; y2 = uvY;
	}

	// set the vertex color to white, otherwise shader doing the blit might get some random color
	device.ImmediateColor(1.0f, 1.0f, 1.0f, 1.0f);

	device.ImmediateTexCoordAll (0.0f, y1, 0.0f); device.ImmediateVertex (0.0f, 0.0f, 0.1f);
	device.ImmediateTexCoordAll (0.0f, y2, 0.0f); device.ImmediateVertex (0.0f, 1.0f, 0.1f);
	device.ImmediateTexCoordAll (uvX,  y2, 0.0f); device.ImmediateVertex (1.0f, 1.0f, 0.1f);
	device.ImmediateTexCoordAll (uvX,  y1, 0.0f); device.ImmediateVertex (1.0f, 0.0f, 0.1f);
	device.ImmediateEnd ();
}

void ImageFilters::DrawQuad (GfxDevice& device, bool invertY, float uvX, float uvY)
{
	DrawQuadNoGPUTimestamp(device, invertY, uvX, uvY);
	GPU_TIMESTAMP();
}


static void SetMultiTapTexCoords (GfxDevice& device, float invSourceSizeX, float invSourceSizeY, float x, float y, bool invertY, int count, const Vector2f* offsets)
{
	for (int i = 0; i < count; ++i)
	{
		Vector2f offset = offsets[i];
		if (invertY)
			offset.y = -offset.y;
		offset.x *= invSourceSizeX;
		offset.y *= invSourceSizeY;
		device.ImmediateTexCoord (i, x + offset.x, y + offset.y, 0.0f);
	}
}

void ImageFilters::SetCurrentRenderTarget (RenderTexture* dest, UInt32 flags)
{
	RenderSurfaceHandle rsColor, rsDepth;
	GetDestRenderTargetSurfaces (dest, rsColor, rsDepth);
	RenderTexture::SetActive (1, &rsColor, rsDepth, dest, 0, kCubeFaceUnknown, flags);
	RenderTexture::FindAndSetSRGBWrite (dest);
}

static bool IsActiveRenderTextureMSAA ()
{
	RenderTexture* rt = RenderTexture::GetActive();
	return (rt && rt->IsAntiAliased());
}

void ImageFilters::Blit (Texture* source, RenderTexture* dest, Unity::Material* mat, int pass, bool setRT)
{
	using namespace ImageFilters_Static;
	PROFILER_AUTO(gGraphicsBlitProfile, mat->GetShader())

	GfxDevice& device = GetGfxDevice();

	UInt32 rtFlags = 0;
#if UNITY_XENON
	// Xbox 360 must resolve a render target before using it as a texture.
	if (source == dest)
	{
		rtFlags |= RenderTexture::kFlagForceResolve;
	}
	else if (!setRT)
	{
		// Render target was set previously. Get it and compare.
		if (source == device.GetActiveRenderTexture())
		{
			setRT = true;
			rtFlags |= RenderTexture::kFlagForceResolve;
		}
	}
#endif
	// MSAA render targets must be resolved before they are used.
	if (IsActiveRenderTextureMSAA())
	{
		setRT = true;
		rtFlags |= RenderTexture::kFlagForceResolve;
	}
	if (setRT)
		SetCurrentRenderTarget (dest, rtFlags);


	bool setTexture = source && mat->HasProperty(kSLPropMainTex);
	if (setTexture)
		mat->SetTexture (kSLPropMainTex, source);
	bool invertY = source && source->GetTexelSizeY() < 0.0f;

	float uvX = 1.0f, uvY = 1.0f;
	#if GFX_EMULATES_NPOT_RENDERTEXTURES
	if (source)
	{
		int texWidth = source->GetGLWidth();
		int texHeight = source->GetGLHeight();
		uvX = (float)texWidth / (float)NextPowerOfTwo(texWidth);
		uvY = (float)texHeight / (float)NextPowerOfTwo(texHeight);
	}
	#endif

	DeviceMVPMatricesState preserveMVP;

	LoadFullScreenOrthoMatrix();

	int npasses = mat->GetPassCount ();
	if (pass == -1)
	{
		for (int i = 0; i < npasses; ++i)
		{
			mat->SetPass (i);
			DrawQuad (device, invertY, uvX, uvY);
		}
	}
	else
	{
		if (pass >= 0 && pass < npasses)
		{
			mat->SetPass (pass);
			DrawQuad (device, invertY, uvX, uvY);
		}
		else
		{
			ErrorString ("Invalid pass number for Graphics.Blit");
		}
	}

	if (setTexture)
		mat->SetTexture (kSLPropMainTex, NULL);
}

void ImageFilters::BlitMultiTap (Texture* source, RenderTexture* dest, Material* mat, int count, const Vector2f* offsets)
{
	using namespace ImageFilters_Static;

	PROFILER_AUTO(gGraphicsBlitProfile, mat->GetShader())

	UInt32 rtFlags = 0;
#if UNITY_XENON
	// Xbox 360 must resolve a render target before using it as a texture.
	// MSAA render targets also need to be resolved before they are used.
	if (source == dest)
		rtFlags |= RenderTexture::kFlagForceResolve;
#endif
	// MSAA render targets must be resolved before they are used.
	if (IsActiveRenderTextureMSAA())
	{
		rtFlags |= RenderTexture::kFlagForceResolve;
	}
	SetCurrentRenderTarget (dest, rtFlags);

	bool setTexture = source && mat->HasProperty(kSLPropMainTex);
	if (setTexture)
		mat->SetTexture (kSLPropMainTex, source);
	bool invertY = source && source->GetTexelSizeY() < 0.0f;

	float uvX = 1.0f, uvY = 1.0f;
	int texWidth = 0, texHeight = 0;
	if (source)
	{
		texWidth = source->GetGLWidth();
		texHeight = source->GetGLHeight();
		#if GFX_EMULATES_NPOT_RENDERTEXTURES
		int potWidth = NextPowerOfTwo(texWidth);
		int potHeight = NextPowerOfTwo(texHeight);
		uvX = (float)texWidth / (float)potWidth;
		uvY = (float)texHeight / (float)potHeight;
		texWidth = potWidth;
		texHeight = potHeight;
		#endif
	}

	GfxDevice& device = GetGfxDevice();
	DeviceMVPMatricesState preserveMVP;
	LoadFullScreenOrthoMatrix();

	int npasses = mat->GetPassCount ();
	for (int i = 0; i < npasses; ++i)
	{
		float y1, y2;
		if (invertY)
		{
			y1 = uvY; y2 = 0.0f;
		}
		else
		{
			y1 = 0.0f; y2 = uvY;
		}
		float invSizeX = source ? 1.0f / texWidth : 0.0f;
		float invSizeY = source ? 1.0f / texHeight : 0.0f;

		mat->SetColor(ShaderLab::Property("_BlurOffsets"), ColorRGBAf(offsets[0].x, offsets[0].y, 0.0f, y1));

		mat->SetPass (i);

		device.ImmediateBegin (kPrimitiveQuads);

		SetMultiTapTexCoords( device, invSizeX, invSizeY, 0.0f, y1, invertY, count, offsets );
		device.ImmediateVertex (0.0f, 0.0f, 0.1f);

		SetMultiTapTexCoords( device, invSizeX, invSizeY, 0.0f, y2, invertY, count, offsets );
		device.ImmediateVertex (0.0f, 1.0f, 0.1f);

		SetMultiTapTexCoords( device, invSizeX, invSizeY, uvX, y2, invertY, count, offsets );
		device.ImmediateVertex (1.0f, 1.0f, 0.1f);

		SetMultiTapTexCoords( device, invSizeX, invSizeY, uvX, y1, invertY, count, offsets );
		device.ImmediateVertex (1.0f, 0.0f, 0.1f);

		device.ImmediateEnd ();
		GPU_TIMESTAMP();
	}

	if (setTexture)
		mat->SetTexture (kSLPropMainTex, NULL);

}

