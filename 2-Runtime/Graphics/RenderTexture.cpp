#include "UnityPrefix.h"
#include "RenderTexture.h"
#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/GfxDevice/TexturesXenon.h"
#endif
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/texenv.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Profiler/Profiler.h"

PROFILER_INFORMATION(gGrabPixels, "RenderTexture.GrabPixels", kProfilerRender);
PROFILER_INFORMATION(gSetRenderTargets, "RenderTexture.SetActive", kProfilerRender);

static const int kRenderTextureFormatBPP[kRTFormatCount] = {
	4, // ARGB32
	4, // Depth various amounts on various HW, but we'll assume 4
	8, // ARGBHalf
	4, // Shadowmap various amounts on various HW, but we'll assume 4
	2, // RGB565
	2, // ARGB4444
	2, // ARGB1555
	4, // Default
	4, // A2R10G10B10
	8, // DefaultHDR
	8, // ARGB64
	16,// ARGBFloat
	8, // RGFloat
	4, // RGHalf
	4, // RFloat
	2, // RHalf
	1, // R8
	16,// ARGBInt
	8, // RGInt
	4, // RInt
	4, // BGRA32
};

static int kDepthFormatBPP[kDepthFormatCount] = {
	0, // None
	2, // 16
	4, // 24
};


typedef List< ListNode<RenderTexture> > RenderTextureList;
RenderTextureList gRenderTextures;

static bool gIsRenderTexEnabled = true;
int gTemporarilyAllowIndieRenderTextures = 0;

void RenderTexture::SetTemporarilyAllowIndieRenderTexture (bool allow)
{
	if (allow)
		gTemporarilyAllowIndieRenderTextures++;
	else
		gTemporarilyAllowIndieRenderTextures--;
}

void RenderTexture::FindAndSetSRGBWrite( RenderTexture* newActive )
{
	bool wantLinear = GetActiveColorSpace() == kLinearColorSpace;
	GetGfxDevice().SetSRGBWrite(newActive ? newActive->GetSRGBReadWrite() && wantLinear : wantLinear);
}

void RenderTexture::SetActive (RenderTexture* newActive, int mipLevel, CubemapFace face, UInt32 flags)
{
	newActive = EnsureRenderTextureIsCreated(newActive);

	RenderSurfaceHandle newColorSurface = newActive ? newActive->m_ColorHandle : GetGfxDevice().GetBackBufferColorSurface();
	RenderSurfaceHandle newDepthSurface = newActive ? newActive->m_DepthHandle : GetGfxDevice().GetBackBufferDepthSurface();
	int mips = newActive && newActive->HasMipMap() ? mipLevel : 0;

	SetActive (1, &newColorSurface, newDepthSurface, newActive, mips, face, flags);
}

bool RenderTexture::SetActive (int count, RenderSurfaceHandle* newColorSurfaces, RenderSurfaceHandle newDepthSurface, RenderTexture* rt, int mipLevel, CubemapFace face, UInt32 flags)
{
	DebugAssert(count > 0);
	if( !IsEnabled() )
	{
		for (int i = 0; i < count; ++i)
			newColorSurfaces[i].Reset();

		// ??? should we keep them invalid just in case?
		newColorSurfaces[0] = GetGfxDevice().GetBackBufferColorSurface();
		newDepthSurface		= GetGfxDevice().GetBackBufferDepthSurface();
	}

	int width = 1, height = 1;
	if(newColorSurfaces[0].IsValid() && !newColorSurfaces[0].object->backBuffer)
	{
		width = newColorSurfaces[0].object->width;
		height= newColorSurfaces[0].object->height;
	}

	// Clamp mip level to valid range
	const int mipCount = CalculateMipMapCount3D(width, height, 1);
	mipLevel = clamp(mipLevel, 0, mipCount-1);

	GfxDevice& device = GetGfxDevice();
	RenderSurfaceHandle oldColorSurface = device.GetActiveRenderColorSurface(0);
	RenderSurfaceHandle oldDepthSurface = device.GetActiveRenderDepthSurface();

	Assert(newColorSurfaces[0].IsValid() == newDepthSurface.IsValid());
	bool renderTarget = newColorSurfaces[0].IsValid() && !newColorSurfaces[0].object->backBuffer;

	if( oldColorSurface != newColorSurfaces[0] || (flags & kFlagForceResolve) )
	{
		// MSAA surface has to be resolved
		RenderTexture* oldRt = GetActive();
		if( oldRt && oldRt->IsAntiAliased() )
			oldRt->ResolveAntiAliasedSurface();
	}

	UInt32 rtFlags = 0;
	rtFlags |= (flags & kFlagDontRestoreColor) ? GfxDevice::kFlagDontRestoreColor : 0;
	rtFlags |= (flags & kFlagDontRestoreDepth) ? GfxDevice::kFlagDontRestoreDepth : 0;
	rtFlags |= (flags & kFlagForceResolve) ? GfxDevice::kFlagForceResolve : 0;

	PROFILER_AUTO(gSetRenderTargets, rt);
	device.SetRenderTargets (count, newColorSurfaces, newDepthSurface, mipLevel, face, rtFlags);
	GPU_TIMESTAMP();

	// we can call RenderTexture::SetActive before device init (d3d11)
	if(		oldColorSurface == newColorSurfaces[0] && oldDepthSurface == newDepthSurface
		&&	newColorSurfaces[0].IsValid() && !newColorSurfaces[0].object->backBuffer
	  )
	{
		return false;
	}


	// NOTE: important to set ActiveRenderTexture to null/non-null value before setting up viewport (as that might involve
	// flipping vertically based on whether there is a RT or not).
	GetGfxDevice().SetActiveRenderTexture(rt);

	// Setup the viewport for the render texture
	if( !(flags & kFlagDontSetViewport) )
	{
		if (renderTarget)
		{
			width  >>= mipLevel;
			height >>= mipLevel;
			device.SetViewport (0, 0, width, height);
		}
		else
		{
			// When switching to main window, restore the viewport to the one of the current
			// camera.
			const int* cameraViewPort = GetRenderManager().GetCurrentViewPort();
			int viewcoord[4];
			viewcoord[0] = cameraViewPort[0];
			viewcoord[1] = cameraViewPort[1];
			viewcoord[2] = cameraViewPort[2];
			viewcoord[3] = cameraViewPort[3];

			// Flip viewport just before applying to device, but don't store it flipped in the render manager
			FlipScreenRectIfNeeded( device, viewcoord );
			device.SetViewport( viewcoord[0], viewcoord[1], viewcoord[2], viewcoord[3] );
		}
	}

	if (renderTarget)
	{
		// If texture coordinates go from top to bottom, then we need to
		// invert projection matrix when rendering into a texture.
		#if UNITY_WII
		device.SetInvertProjectionMatrix( true );
		#else
		device.SetInvertProjectionMatrix( !device.UsesOpenGLTextureCoords() );
		#endif
	}
	else
	{
		device.SetInvertProjectionMatrix( false );
	}

	return true;
}


bool RenderTexture::GetSupportedMipMapFlag(bool mipMap) const
{
	if (!gGraphicsCaps.hasAutoMipMapGeneration)
		mipMap = false;
	if (m_Dimension == kTexDimCUBE && gGraphicsCaps.buggyMipmappedCubemaps)
		mipMap = false;
	if (m_Dimension == kTexDim3D && gGraphicsCaps.buggyMipmapped3DTextures)
		mipMap = false;
	return mipMap;
}


bool RenderTexture::Create ()
{
	if( !IsEnabled() )
		return false;

	DestroySurfaces();
	GfxDevice& device = GetGfxDevice();

	if (m_Width <= 0 || m_Height <= 0)
	{
		ErrorString("RenderTexture.Create failed: width & height must be larger than 0");
		return false;
	}

	if (m_Dimension == kTexDimCUBE && (!GetIsPowerOfTwo() || m_Width != m_Height))
	{
		ErrorString("RenderTexture.Create failed: cube maps must be power of two and width must match height");
		return false;
	}

	if( !device.IsRenderTargetConfigValid(m_Width, m_Height, static_cast<RenderTextureFormat>(m_ColorFormat), static_cast<DepthBufferFormat>(m_DepthFormat)) )
	{
		if( GetIsPowerOfTwo() )
		{
			if (gGraphicsCaps.maxRenderTextureSize < 4)
			{
				ErrorString("RenderTexture.Create failed: maxRenderTextureSize is too small");
				return false;
			}

				// Decrease too large render textures while maintaining aspect ratio
				do
				{
				m_Width = std::max( m_Width / 2, 4 );
				m_Height = std::max( m_Height / 2, 4 );
			}
				while ( !device.IsRenderTargetConfigValid(m_Width, m_Height, static_cast<RenderTextureFormat>(m_ColorFormat), static_cast<DepthBufferFormat>(m_DepthFormat)) );
		}
		else
		{
				ErrorString("RenderTexture.Create failed: requested size is too large.");
				return false;
		}
	}

	if( !gGraphicsCaps.supportsRenderTextureFormat[m_ColorFormat] )
	{
		ErrorString("RenderTexture.Create failed: format unsupported.");
		return false;
	}

	if (!GetIsPowerOfTwo() && (gGraphicsCaps.npotRT == kNPOTNone))
	{
		ErrorString("RenderTexture.Create failed: non-power-of-two sizes not supported.");
		return false;
	}

	if (m_Dimension == kTexDimCUBE && (!gGraphicsCaps.hasRenderToCubemap || IsDepthRTFormat((RenderTextureFormat)m_ColorFormat)))
	{
		ErrorString("RenderTexture.Create failed: cubemap not supported.");
		return false;
	}

	if (m_Dimension == kTexDim3D && (!gGraphicsCaps.has3DTexture || !gGraphicsCaps.hasRenderTo3D))
	{
		ErrorString("RenderTexture.Create failed: volume texture not supported.");
		return false;
	}


	bool mipMap = GetSupportedMipMapFlag(m_MipMap);
	if (!GetIsPowerOfTwo())
		mipMap = false;

	int samples = m_AntiAliasing;
	samples = clamp<int>(samples, 1, 8);
	if (!gGraphicsCaps.hasMultiSample)
		samples = 1;
	if (m_Dimension != kTexDim2D)
		samples = 1;

	if (samples > 1)
		mipMap = false;

	// If we have native depth textures, we should use our regular textureID for the depth
	// surface.
	TextureID colorTID, resolvedColorTID, depthTID;
	if ((m_ColorFormat == kRTFormatDepth && gGraphicsCaps.hasNativeDepthTexture) ||
		(m_ColorFormat == kRTFormatShadowMap && gGraphicsCaps.hasNativeShadowMap))
	{
		depthTID = m_TexID;
		m_SecondaryTexIDUsed = false;
	}
	else
	{
		// Use resolved surface as texture if MSAA enabled
		if (samples > 1 && !gGraphicsCaps.hasMultiSampleAutoResolve)
			resolvedColorTID = m_TexID;
		else
			colorTID = m_TexID;
		// For regular non-MSAA render textures, also provide capability to treat depth buffer as a texture.
		// Only do this if HW supports native depth textures AND texture is 2D AND
		// we actually have a depth buffer AND driver is not broken for this case.
		bool useDepthAsTexture = false;
		if (m_Dimension == kTexDim2D && m_DepthFormat != kDepthFormatNone && samples <= 1)
			useDepthAsTexture = gGraphicsCaps.hasStencilInDepthTexture && !gGraphicsCaps.buggyTextureBothColorAndDepth;
		if (useDepthAsTexture)
		{
			depthTID = m_SecondaryTexID;
			m_SecondaryTexIDUsed = true;
		}
		else
			m_SecondaryTexIDUsed = false;
	}

	UInt32 colorFlags = 0;
	if (mipMap) colorFlags |= kSurfaceCreateMipmap;
	if (m_GenerateMips) colorFlags |= kSurfaceCreateAutoGenMips;
	if (m_SRGB) colorFlags |= kSurfaceCreateSRGB;
	if (m_EnableRandomWrite) colorFlags |= kSurfaceCreateRandomWrite;
	if (colorTID == TextureID() && samples <= 1) colorFlags |= kSurfaceCreateNeverUsed; // never sampled or resolved
	m_ColorHandle = device.CreateRenderColorSurface (colorTID,
		m_Width, m_Height, samples, m_VolumeDepth, m_Dimension, static_cast<RenderTextureFormat>(m_ColorFormat), colorFlags);

	if (samples > 1 && !gGraphicsCaps.hasMultiSampleAutoResolve)
	{
		m_ResolvedColorHandle = device.CreateRenderColorSurface (resolvedColorTID,
			m_Width, m_Height, 1, m_VolumeDepth, m_Dimension, static_cast<RenderTextureFormat>(m_ColorFormat), colorFlags);
	}

	UInt32 depthFlags = 0;
	if (m_ColorFormat==kRTFormatShadowMap) depthFlags |= kSurfaceCreateShadowmap;
	if (m_SampleOnlyDepth) depthFlags |= kSurfaceCreateSampleOnly;
	m_DepthHandle = device.CreateRenderDepthSurface (depthTID,
		m_Width, m_Height, samples, m_Dimension, static_cast<DepthBufferFormat>(m_DepthFormat), depthFlags);

	if (!(m_ColorHandle.IsValid() || m_DepthHandle.IsValid()))
	{
		ErrorString("RenderTexture.Create failed");
		return false;
	}

	if (IsCreated())
	{
		Assert(m_RegisteredSizeForStats == 0);
		m_RegisteredSizeForStats = GetRuntimeMemorySize();
		device.GetFrameStats().ChangeRenderTextureBytes (m_RegisteredSizeForStats);
	}

	if (m_CreatedFromScript)
	{
		// We can't currently read the minds of users, so let's always restore their render targets.
		// TODO: extend RenderTexture API
		device.SetSurfaceFlags(m_ColorHandle, GfxDevice::kSurfaceAlwaysRestore, ~GfxDevice::kSurfaceRestoreMask);
		device.SetSurfaceFlags(m_DepthHandle, GfxDevice::kSurfaceAlwaysRestore, ~GfxDevice::kSurfaceRestoreMask);
	}

	SetStoredColorSpaceNoDirtyNoApply(m_SRGB ? kTexColorSpaceSRGB : kTexColorSpaceLinear);

	// Set filtering, etc. mode
	ApplySettings();

	UpdateTexelSize();

	return true;
}

void RenderTexture::UpdateTexelSize()
{
	SetUVScale( 1.0f, 1.0f );
	if (m_Width != 0 && m_Height != 0)
	{
		int width = m_Width;
		int height = m_Height;
		#if GFX_EMULATES_NPOT_RENDERTEXTURES
		width = NextPowerOfTwo (width);
		height = NextPowerOfTwo (height);
		#endif
		SetTexelSize( 1.0f / width, 1.0f / height );
	}
}


void RenderTexture::ApplySettings()
{
	TextureDimension dim = GetDimension();
	bool mip = HasMipMap();
	// Don't ever use Aniso on depth textures or textures where we
	// sample depth buffer.
	bool depth = IsDepthRTFormat((RenderTextureFormat)m_ColorFormat);
	if (depth || m_SecondaryTexIDUsed)
		m_TextureSettings.m_Aniso = 0;

	m_TextureSettings.Apply (GetTextureID(), dim, mip, GetActiveTextureColorSpace());
	if (m_SecondaryTexIDUsed)
		m_TextureSettings.Apply (m_SecondaryTexID, dim, mip, GetActiveTextureColorSpace());
	NotifyMipBiasChanged();
}


void RenderTexture::Release()
{
	if (GetGfxDevice().GetActiveRenderTexture() == this)
	{
		ErrorString("Releasing render texture that is set to be RenderTexture.active!");
		GetGfxDevice().SetActiveRenderTexture(NULL);
	}
	DestroySurfaces();
}

void RenderTexture::DestroySurfaces()
{
	if(!IsCreated())
		return;

	GfxDevice& device = GetGfxDevice();
	// Different graphics caps can change the calculated runtime size (case 555046)
	// so we store the size we added and make sure to subtract the same amount.
	// Graphics caps can change during an editor session due to emulation.
	device.GetFrameStats().ChangeRenderTextureBytes (-m_RegisteredSizeForStats);
	m_RegisteredSizeForStats = 0;
	if( m_ColorHandle.IsValid() )
		device.DestroyRenderSurface (m_ColorHandle);
	if( m_ResolvedColorHandle.IsValid() )
		device.DestroyRenderSurface (m_ResolvedColorHandle);
	if( m_DepthHandle.IsValid() )
		device.DestroyRenderSurface (m_DepthHandle);
}

void RenderTexture::DiscardContents()
{
	if(IsCreated())
		RenderTextureDiscardContents(this, true, true);

}
void RenderTexture::DiscardContents(bool discardColor, bool discardDepth)
{
	if(IsCreated())
		RenderTextureDiscardContents(this, discardColor, discardDepth);
}


void RenderTexture::MarkRestoreExpected()
{
	GfxDevice& device = GetGfxDevice();
	device.IgnoreNextUnresolveOnRS (m_ColorHandle);
	device.IgnoreNextUnresolveOnRS (m_DepthHandle);
	device.IgnoreNextUnresolveOnRS (m_ResolvedColorHandle);
}


void RenderTexture::GrabPixels (int left, int bottom, int width, int height)
{
	if (!IsCreated())
		Create();

	const RenderSurfaceHandle& handle = IsAntiAliased() ? m_ResolvedColorHandle : m_ColorHandle;

	if (!handle.IsValid())
		return;

	if (left < 0) {
		width += left;
		left = 0;
	}
	if (bottom < 0) {
		height += bottom;
		bottom = 0;
	}
	if (width > m_Width)
		width = m_Width;
	if (height > m_Height)
		height = m_Height;

	PROFILER_AUTO(gGrabPixels, NULL);
	GfxDevice& device = GetGfxDevice();
	device.GrabIntoRenderTexture (handle, m_DepthHandle, left, bottom, width, height);
	GPU_TIMESTAMP();
	device.GetFrameStats().AddRenderTextureChange(); // treat resolve as RT change
}
void RenderTexture::CorrectVerticalTexelSize(bool shouldBePositive)
{
	if (!GetGfxDevice().UsesOpenGLTextureCoords())
	{
		if (m_TexelSizeY < 0.0f && shouldBePositive) m_TexelSizeY = -m_TexelSizeY;
		else if (m_TexelSizeY > 0.0f && !shouldBePositive) m_TexelSizeY = -m_TexelSizeY;
	}
}
RenderTexture::RenderTexture (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_DepthFormat(kDepthFormat24)
,	m_ColorFormat(kRTFormatARGB32)
,	m_Dimension(kTexDim2D)
,	m_MipMap(false)
,	m_GenerateMips(true)
,	m_SRGB(false)
,	m_EnableRandomWrite(false)
,	m_CreatedFromScript(false)
,	m_SampleOnlyDepth(false)
,	m_RegisteredSizeForStats(0)
,	m_RenderTexturesNode(this)
{
	m_Width = 256;
	m_Height = 256;
	m_VolumeDepth = 1;
	m_AntiAliasing = 1;

	GetSettings().m_WrapMode = kTexWrapClamp;

	// We use unchecked version since we may not be on the main thread
	// This means CreateTextureID() implementation must be thread safe!
	m_SecondaryTexID = GetUncheckedGfxDevice().CreateTextureID();
	m_SecondaryTexIDUsed = false;
}

RenderTexture::~RenderTexture ()
{
	Release();
	m_RenderTexturesNode.RemoveFromList();
	Texture::s_TextureIDMap.erase (m_SecondaryTexID);
	// FreeTextureID() implementation must be thread safe!
	GetUncheckedGfxDevice().FreeTextureID(m_SecondaryTexID);
}

void RenderTexture::SetDimension (TextureDimension dim)
{
	if (m_Dimension == dim)
		return;

	if (!IsCreated ())
		m_Dimension = dim;
	else
		ErrorString ("Setting dimension of already created render texture is not supported!");
}

void RenderTexture::SetVolumeDepth (int v)
{
	if (m_VolumeDepth == v)
		return;

	if (!IsCreated ()) {
		m_VolumeDepth = v;
	} else
		ErrorString ("Setting volume depth of already created render texture is not supported!");
}

void RenderTexture::SetAntiAliasing (int aa)
{
	if (m_AntiAliasing == aa)
		return;

	if (aa < 1 || aa > 8 || !IsPowerOfTwo(aa))
	{
		ErrorString( "Invalid antiAliasing value (must be 1, 2, 4 or 8)" );
		return;
	}

	if (!IsCreated ()) {
		m_AntiAliasing = aa;
	} else
		ErrorString ("Setting anti-aliasing of already created render texture is not supported!");
}


void RenderTexture::SetMipMap( bool mipMap )
{
	mipMap = GetSupportedMipMapFlag(mipMap);

	if (mipMap == m_MipMap)
		return;

	if (!IsCreated ()) {
		m_MipMap = mipMap;
	} else {
		ErrorString ("Setting mipmap mode of render texture that is loaded not supported!");
	}
}

void RenderTexture::SetGenerateMips (bool v)
{
	if (v == m_GenerateMips)
		return;

	if (!IsCreated ())
	{
		if (m_MipMap && m_DepthFormat != kDepthFormatNone && !v)
		{
			WarningStringObject ("Mipmapped RenderTextures with manual mip generation can't have depth buffer", this);
			v = true;
		}
		m_GenerateMips = v;
	}
	else
		ErrorString ("Can't change mipmap generation of already created RenderTexture");
}

void RenderTexture::SetSRGBReadWrite ( bool sRGB )
{
	if (!IsCreated ()) {
		bool setSRGB = sRGB ? (GetActiveColorSpace() == kLinearColorSpace) : false;
		setSRGB = setSRGB && (m_ColorFormat != GetGfxDevice().GetDefaultHDRRTFormat());
		m_SRGB = setSRGB;
	} else
		ErrorString ("Changing srgb mode of render texture that is loaded not supported!");
}

void RenderTexture::SetEnableRandomWrite (bool v)
{
	if (v == m_EnableRandomWrite)
		return;

	if (!IsCreated ()) {
		m_EnableRandomWrite = v;
	} else
		ErrorString ("Can't change random write mode of already created render texture!");
}

void RenderTexture::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	m_Width = std::max (1, m_Width);
	m_Height = std::max (1, m_Height);
	m_VolumeDepth = std::max (1, m_VolumeDepth);
	m_AntiAliasing = clamp<int> (m_AntiAliasing, 1, 8);

	if( IsDepthRTFormat( (RenderTextureFormat)m_ColorFormat ) )
		m_MipMap = false;
	if (m_Dimension==kTexDimCUBE)
		m_Height = m_Width;

	if( !GetIsPowerOfTwo() && GetSettings().m_WrapMode == kTexWrapRepeat )
		GetSettings().m_WrapMode = kTexWrapClamp;

	if( IsDepthRTFormat((RenderTextureFormat)m_ColorFormat) ) // always clamp depth textures
		GetSettings().m_WrapMode = kTexWrapClamp;

#if GFX_OPENGLESxx_ONLY
	// currently i have no idea about possibility of linear filter for fp rts (seems like gl do support it)
	if( IsHalfRTFormat((RenderTextureFormat)m_ColorFormat) && !gGraphicsCaps.gles20.hasHalfLinearFilter )
		GetSettings().m_FilterMode = kTexFilterNearest;
#endif

	gRenderTextures.push_back(m_RenderTexturesNode);

	UpdateTexelSize();

	Super::AwakeFromLoad (awakeMode);
}

void RenderTexture::SetWidth (int width)
{
	if (!IsCreated ()) {
		m_Width = width;
		UpdateTexelSize();
		SetDirty();
	} else
		ErrorString ("Resizing of render texture that is loaded not supported!");
}

void RenderTexture::SetHeight (int height)
{
	if (!IsCreated ()) {
		m_Height = height;
		UpdateTexelSize();
		SetDirty();
	} else
		ErrorString ("Resizing of render texture that is loaded not supported!");
}

void RenderTexture::SetDepthFormat( DepthBufferFormat depth )
{
	if (!IsCreated ())
		m_DepthFormat = depth;
	else
		ErrorString ("Setting depth of render texture that is loaded not supported!");
}

void RenderTexture::SetColorFormat( RenderTextureFormat format )
{
	if (!IsCreated ())
	{
		if( format == kRTFormatDefault )
			format = GetGfxDevice().GetDefaultRTFormat();

		m_ColorFormat = format;
		if (IsDepthRTFormat(format)) m_TextureSettings.m_Aniso = 0; // never use Anisotropic on depth textures
	} else
		ErrorString ("Changing format of render texture that is loaded not supported!");
}


RenderTexture* RenderTexture::GetActive ()
{
	return GetGfxDevice().GetActiveRenderTexture();
}

ShaderLab::TexEnv *RenderTexture::SetGlobalProperty (const ShaderLab::FastPropertyName& name)
{
	ShaderLab::TexEnv *te = ShaderLab::g_GlobalProperties->SetTexture (name, this);
	te->ClearMatrix();
	return te;
}

bool RenderTexture::IsEnabled ()
{
	if (gGraphicsCaps.hasRenderToTexture)
		return gIsRenderTexEnabled && (GetBuildSettings().hasRenderTexture || gTemporarilyAllowIndieRenderTextures);
	else
		return false;
}

void RenderTexture::SetEnabled (bool enable)
{
	if (!enable)
		ReleaseAll();

	gIsRenderTexEnabled = enable;
}

void RenderTexture::ReleaseAll ()
{
	SetActive (NULL);

	for (RenderTextureList::iterator i=gRenderTextures.begin ();i != gRenderTextures.end ();i++)
		(**i).Release ();
}


#if ENABLE_PROFILER || UNITY_EDITOR

int RenderTexture::GetCreatedRenderTextureCount ()
{
	int count = 0;
	for (RenderTextureList::iterator i=gRenderTextures.begin ();i != gRenderTextures.end ();i++)
	{
		if ((**i).IsCreated ())
			count++;
	}

	return count;
}

int RenderTexture::GetCreatedRenderTextureBytes ()
{
	int count = 0;
	for (RenderTextureList::iterator i=gRenderTextures.begin ();i != gRenderTextures.end ();i++)
	{
		if ((**i).IsCreated ())
			count += (**i).GetRuntimeMemorySize();
	}

	return count;
}

#endif



int EstimateRenderTextureSize (int width, int height, int depth, RenderTextureFormat format, DepthBufferFormat depthFormat, TextureDimension dim, bool mipMap)
{
	// color buffer
	int colorBPP;
	if (format == kRTFormatDepth && gGraphicsCaps.hasNativeDepthTexture)
		colorBPP = 0;
	else if (format == kRTFormatShadowMap && gGraphicsCaps.hasNativeShadowMap)
		colorBPP = 0;
	else
		colorBPP = kRenderTextureFormatBPP[format];
	int size = width * height * colorBPP;

	if (dim == kTexDim3D)
		size *= depth;
	else if (dim == kTexDimCUBE)
		size *= 6;
	if (mipMap && gGraphicsCaps.hasAutoMipMapGeneration)
		size += size / 3;

	// depth buffer
	size += width * height * kDepthFormatBPP[depthFormat];
	return size;
}



int RenderTexture::GetRuntimeMemorySize() const
{
	int bytes = EstimateRenderTextureSize (m_Width, m_Height, m_VolumeDepth, (RenderTextureFormat)m_ColorFormat, (DepthBufferFormat)m_DepthFormat, m_Dimension, m_MipMap);
	bytes *= m_AntiAliasing;
	return bytes;// + Super::GetRuntimeMemorySize(); Since the name is assigned after this value is used for gfxstats accumulate, the string will add to the mem usage later - Causes Assert
}

void RenderTexture::ResolveAntiAliasedSurface()
{
	if (!m_ResolvedColorHandle.IsValid())
		return;

	GfxDevice& device = GetGfxDevice();
	device.ResolveColorSurface(m_ColorHandle, m_ResolvedColorHandle);
}

RenderTexture* EnsureRenderTextureIsCreated(RenderTexture* tex)
{
	RenderTexture* ret = tex;

	if(!RenderTexture::IsEnabled())
		ret = 0;

	if(ret && !ret->IsCreated())
		ret->Create();

	// check if we could create
	if(ret && !ret->IsCreated())
		ret = 0;

	return ret;
}

template<class TransferFunction>
void RenderTexture::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_Width);
	TRANSFER (m_Height);
	TRANSFER (m_AntiAliasing);
	TRANSFER (m_DepthFormat);
	TRANSFER (m_ColorFormat);
	TRANSFER (m_MipMap);
	TRANSFER (m_GenerateMips);
	TRANSFER (m_SRGB);
	transfer.Align();
	TRANSFER (m_TextureSettings);
}

IMPLEMENT_CLASS (RenderTexture)
IMPLEMENT_OBJECT_SERIALIZE (RenderTexture)

bool RenderTextureSupportsStencil (RenderTexture* rt)
{
	if(!(gGraphicsCaps.hasStencil && GetBuildSettings ().hasAdvancedVersion))
		return false;

	if (rt)
		return rt->GetDepthFormat() >= kDepthFormat24;

	return GetGfxDevice().GetFramebufferDepthFormat() >= kDepthFormat24;
}

void RenderTextureDiscardContents(RenderTexture* rt, bool discardColor, bool discardDepth)
{
	GfxDevice& device = GetGfxDevice();

	RenderSurfaceHandle color  = rt ? rt->GetColorSurfaceHandle() : device.GetBackBufferColorSurface();
	RenderSurfaceHandle rcolor = rt ? rt->GetResolvedColorSurfaceHandle() : RenderSurfaceHandle();
	RenderSurfaceHandle depth  = rt ? rt->GetDepthSurfaceHandle() : device.GetBackBufferDepthSurface();

	if(discardColor && color.IsValid())
		device.DiscardContents(color);
	if(discardColor && rcolor.IsValid())
		device.DiscardContents(rcolor);
	if(discardDepth && depth.IsValid())
		device.DiscardContents(depth);
}


// --------------------------------------------------------------------------


#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"
SUITE (RenderTextureTests)
{
TEST(RenderTextureTests_BPPTableCorrect)
{
	// checks that you did not forget to update BPP counts when you added a new format :)
	for (int i = 0; i < kRTFormatCount; ++i)
	{
		CHECK(kRenderTextureFormatBPP[i] != 0);
	}
	for (int i = 1; i < kDepthFormatCount; ++i) // skip DepthFormatNone, since it is expected to have zero
	{
		CHECK(kDepthFormatBPP[i] != 0);
	}
}
}

#endif
