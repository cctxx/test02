#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Graphics/Image.h"
#include "D3D9Context.h"
#include "TexturesD3D9.h"
#include "RenderTextureD3D.h"
#include "D3D9Utils.h"


// defined in GfxDeviceD3D9.cpp
void UnbindTextureD3D9( TextureID texture );


// define to 1 to print lots of activity info
#define DEBUG_RENDER_TEXTURES 0


D3DFORMAT kD3D9RenderTextureFormats[kRTFormatCount] = {
	D3DFMT_A8R8G8B8,
	D3DFMT_R32F, // Depth
	D3DFMT_A16B16G16R16F,
	D3DFMT_D16, // Shadowmap
	D3DFMT_R5G6B5,
	D3DFMT_A4R4G4B4,
	D3DFMT_A1R5G5B5,
	(D3DFORMAT)-1, // Default
	D3DFMT_A2R10G10B10,
	(D3DFORMAT)-1, // DefaultHDR
	D3DFMT_A16B16G16R16,
	D3DFMT_A32B32G32R32F,
	D3DFMT_G32R32F,
	D3DFMT_G16R16F,
	D3DFMT_R32F,
	D3DFMT_R16F,
	D3DFMT_L8, // R8
	(D3DFORMAT)-1, // ARGBInt
	(D3DFORMAT)-1, // RGInt
	(D3DFORMAT)-1, // RInt
	(D3DFORMAT)-1, // BGRA32
};


static D3DMULTISAMPLE_TYPE FindSupportedD3DMultiSampleType (D3DFORMAT d3dformat, int maxSamples)
{
	BOOL windowed = !GetScreenManager().IsFullScreen();
	for (int samples = maxSamples; samples >= 1; samples--)
	{
		D3DMULTISAMPLE_TYPE msaa = GetD3DMultiSampleType( samples );
		HRESULT hr = GetD3DObject()->CheckDeviceMultiSampleType( g_D3DAdapter, g_D3DDevType, d3dformat, windowed, msaa, NULL );
		if (SUCCEEDED(hr))
			return msaa;
	}
	return D3DMULTISAMPLE_NONE;
}

static bool InitD3DRenderColorSurface (RenderColorSurfaceD3D9& rs, TexturesD3D9& textures)
{
	IDirect3DDevice9* dev = GetD3DDevice();

	HRESULT hr;
	DWORD usage;

	if (rs.textureID.m_ID)
	{
		// Regular render texture
		usage = D3DUSAGE_RENDERTARGET;
		int mipCount = 1;
		if (rs.flags & kSurfaceCreateMipmap && !IsDepthRTFormat(rs.format))
		{
			Assert(gGraphicsCaps.hasAutoMipMapGeneration);
			if (rs.flags & kSurfaceCreateAutoGenMips)
			usage |= D3DUSAGE_AUTOGENMIPMAP;
			else
				mipCount = CalculateMipMapCount3D (rs.width, rs.height, 1);
		}
		if (rs.dim == kTexDim2D)
		{
			IDirect3DTexture9* rt;
			D3DFORMAT d3dformat = D3DFMT_UNKNOWN;
			d3dformat = kD3D9RenderTextureFormats[rs.format];
			hr = dev->CreateTexture (rs.width, rs.height, mipCount, usage, d3dformat, D3DPOOL_DEFAULT, &rt, NULL);
			if( FAILED(hr) )
			{
				ErrorString( Format( "RenderTexture creation error: CreateTexture failed [%s]", GetD3D9Error(hr) ) );
				return false;
			}
			rs.m_Texture = rt;
			rt->GetSurfaceLevel( 0, &rs.m_Surface );
		}
		else if (rs.dim == kTexDimCUBE)
		{
			Assert(rs.width == rs.height);
			IDirect3DCubeTexture9* rt;
			hr = dev->CreateCubeTexture (rs.width, mipCount, usage, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &rt, NULL);
			if( FAILED(hr) )
			{
				ErrorString( Format( "RenderTexture creation error: CreateCubeTexture failed [%s]", GetD3D9Error(hr) ) );
				return false;
			}
			rs.m_Texture = rt;
		}
		else
		{
			ErrorString("RenderTexture creation error: D3D9 only supports 2D or CUBE textures");
			return false;
		}
	}
	else
	{
		D3DFORMAT d3dformat = D3DFMT_UNKNOWN;
		D3DMULTISAMPLE_TYPE msaa = D3DMULTISAMPLE_NONE;
		if (!(rs.flags & kSurfaceCreateNeverUsed))
		{
			// Create surface without texture to resolve from
			// Find supported MSAA type based on device and format
			d3dformat = kD3D9RenderTextureFormats[rs.format];
			msaa = FindSupportedD3DMultiSampleType( d3dformat, rs.samples );
		}
		else
		{
			// Dummy render target surface (only needed to make D3D runtime happy)
			d3dformat = gGraphicsCaps.d3d.hasNULLFormat ? kD3D9FormatNULL : D3DFMT_A8R8G8B8;
		}
		IDirect3DSurface9* ds = NULL;
		hr = dev->CreateRenderTarget( rs.width, rs.height, d3dformat, msaa, 0, FALSE, &ds, NULL );
		if (FAILED(hr))
		{
			ErrorString( Format( "RenderTexture creation error: CreateRenderTarget failed [%s]", GetD3D9Error(hr) ) );
			return false;
		}
		rs.m_Surface = ds;
	}

	// add to textures map
	if (rs.textureID.m_ID)
		textures.AddTexture( rs.textureID, rs.m_Texture );

	return true;
}

static bool InitD3DRenderDepthSurface (RenderDepthSurfaceD3D9& rs, TexturesD3D9& textures)
{
	IDirect3DDevice9* dev = GetD3DDevice();

	HRESULT hr;

	if (!rs.textureID.m_ID)
	{
		// Create depth buffer surface
		if( rs.depthFormat == kDepthFormatNone )
		{
			rs.m_Surface = NULL;
		}
		else
		{
			// Create surface without texture to resolve from
			// Find supported MSAA type based on device and format
			D3DFORMAT d3dformat = (rs.depthFormat == kDepthFormat16 ? D3DFMT_D16 : D3DFMT_D24S8);
			D3DMULTISAMPLE_TYPE msaa = FindSupportedD3DMultiSampleType( d3dformat, rs.samples );
			hr = dev->CreateDepthStencilSurface( rs.width, rs.height, d3dformat, msaa, 0, TRUE, &rs.m_Surface, NULL );
			REGISTER_EXTERNAL_GFX_ALLOCATION_REF(rs.m_Surface, rs.width * rs.height * GetBPPFromD3DFormat(d3dformat), &rs);
			if( FAILED(hr) )
			{
				ErrorString( Format( "RenderTexture creation error: CreateDepthStencilSurface failed [%s]", GetD3D9Error(hr) ) );
				return false;
			}
		}
	}
	else
	{
		// Create depth buffer as texture
		D3DFORMAT d3dformat = D3DFMT_UNKNOWN;
		if (rs.flags & kSurfaceCreateShadowmap)
		{
			Assert (rs.depthFormat == kDepthFormat16);
			Assert (gGraphicsCaps.hasNativeShadowMap);
			d3dformat = D3DFMT_D16;
		}
		else
		{
			Assert (gGraphicsCaps.hasNativeDepthTexture);
			if (gGraphicsCaps.d3d.hasNVDepthFormatINTZ)
				d3dformat = kD3D9FormatINTZ;
			else if (gGraphicsCaps.d3d.hasATIDepthFormat16)
				d3dformat = kD3D9FormatDF16;
			else
			{
				AssertString ("No available native depth format");
			}
		}
		IDirect3DTexture9* texture = NULL;
		hr = dev->CreateTexture (rs.width, rs.height, 1, D3DUSAGE_DEPTHSTENCIL, d3dformat, D3DPOOL_DEFAULT, &texture, NULL);
		if( FAILED(hr) )
		{
			ErrorString( Format( "RenderTexture creation error: CreateTexture failed [%s]", GetD3D9Error(hr) ) );
			return false;
		}
		rs.m_Texture = texture;
		texture->GetSurfaceLevel (0, &rs.m_Surface);
	}

	if (rs.textureID.m_ID)
		textures.AddTexture( rs.textureID, rs.m_Texture );

	return true;
}


static RenderColorSurfaceD3D9* s_ActiveColorTargets[kMaxSupportedRenderTargets];
static int s_ActiveColorTargetCount;
static RenderDepthSurfaceD3D9* s_ActiveDepthTarget = NULL;
static int s_ActiveMip = 0;
static CubemapFace s_ActiveFace = kCubeFaceUnknown;

static RenderColorSurfaceD3D9* s_ActiveColorBackBuffer = NULL;
static RenderDepthSurfaceD3D9* s_ActiveDepthBackBuffer = NULL;

// on dx editor we can switch swapchain underneath
// so lets do smth like gl's default FBO
// it will be used only from "user" code and we will select proper swap chain here
static RenderColorSurfaceD3D9* s_DummyColorBackBuffer = NULL;
static RenderDepthSurfaceD3D9* s_DummyDepthBackBuffer = NULL;

RenderSurfaceBase* DummyColorBackBuferD3D9()
{
	if(s_DummyColorBackBuffer == 0)
	{
		static RenderColorSurfaceD3D9 __bb;
		RenderSurfaceBase_InitColor(__bb);
		__bb.backBuffer = true;

		s_DummyColorBackBuffer = &__bb;
	}
	return s_DummyColorBackBuffer;
}

RenderSurfaceBase* DummyDepthBackBuferD3D9()
{
	if(s_DummyDepthBackBuffer == 0)
	{
		static RenderDepthSurfaceD3D9 __bb;
		RenderSurfaceBase_InitDepth(__bb);
		__bb.backBuffer = true;

		s_DummyDepthBackBuffer = &__bb;
	}
	return s_DummyDepthBackBuffer;
}

bool SetRenderTargetD3D9 (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, int& outRenderTargetWidth, int& outRenderTargetHeight, bool& outIsBackBuffer)
{
	RenderColorSurfaceD3D9* rcolorZero = reinterpret_cast<RenderColorSurfaceD3D9*>(colorHandles[0].object);
	RenderDepthSurfaceD3D9* rdepth = reinterpret_cast<RenderDepthSurfaceD3D9*>( depthHandle.object );

	#if DEBUG_RENDER_TEXTURES
	printf_console( "RT: SetRenderTargetD3D9 color=%i depth=%i (%x) mip=%i face=%i\n",
		rcolorZero ? rcolorZero->textureID.m_ID : 0,
		rdepth ? rdepth->textureID.m_ID : 0, rdepth ? rdepth->m_Surface : 0,
		mipLevel, face );
	#endif

	outIsBackBuffer = false;

	if (count == s_ActiveColorTargetCount && s_ActiveDepthTarget == rdepth  && s_ActiveMip == mipLevel && s_ActiveFace == face)
	{
		bool colorsSame = true;
		for (int i = 0; i < count; ++i)
		{
			if (s_ActiveColorTargets[i] != reinterpret_cast<RenderColorSurfaceD3D9*>(colorHandles[i].object))
				colorsSame = false;
		}
		if (colorsSame)
			return false;
	}

	IDirect3DDevice9* dev = GetD3DDeviceNoAssert();
	// Happens at startup, when deleting all RenderTextures
	if( !dev )
	{
		Assert (!rcolorZero && !rdepth);
		return false;
	}

	HRESULT hr = S_FALSE;

	Assert(colorHandles[0].IsValid() && depthHandle.IsValid());
	Assert(rcolorZero->backBuffer == rdepth->backBuffer);

	outIsBackBuffer = rcolorZero->backBuffer;
	if (!outIsBackBuffer)
		GetRealGfxDevice().GetFrameStats().AddRenderTextureChange(); // stats

	if(rcolorZero->backBuffer && rcolorZero == s_DummyColorBackBuffer)
		colorHandles[0].object = rcolorZero = s_ActiveColorBackBuffer;
	if(rdepth->backBuffer && rdepth == s_DummyDepthBackBuffer)
		depthHandle.object = rdepth = s_ActiveDepthBackBuffer;


	// color surfaces
	for (int i = 0; i < count; ++i)
	{
		RenderColorSurfaceD3D9* rcolor = reinterpret_cast<RenderColorSurfaceD3D9*>(colorHandles[i].object);
		if(rcolor)
		{
			// color surface
			Assert (rcolor->colorSurface);
			// Make sure this texture is not used when setting it as render target
			if (rcolor->textureID.m_ID)
				UnbindTextureD3D9( rcolor->textureID );

			// Set color surface
			IDirect3DSurface9* surface = NULL;
			bool needsRelease = false;
			if( !rcolor->m_Texture )
			{
				Assert (rcolor->m_Surface);
				surface = rcolor->m_Surface;
				#if DEBUG_RENDER_TEXTURES
				printf_console( "  RT: color buffer plain\n" );
				#endif
			}
			else if (rcolor->dim == kTexDimCUBE)
			{
				Assert (rcolor->m_Texture);
				IDirect3DCubeTexture9* rt = static_cast<IDirect3DCubeTexture9*>( rcolor->m_Texture );
				hr = rt->GetCubeMapSurface((D3DCUBEMAP_FACES)(D3DCUBEMAP_FACE_POSITIVE_X + clamp<int>(face,0,5)), mipLevel, &surface);
				needsRelease = true;
			}
			else
			{
				#if DEBUG_RENDER_TEXTURES
				printf_console( "  RT: color buffer texture %i\n", rcolor->textureID.m_ID );
				#endif
				Assert (rcolor->m_Texture);
				IDirect3DTexture9* rt = static_cast<IDirect3DTexture9*>( rcolor->m_Texture );
				hr = rt->GetSurfaceLevel (mipLevel, &surface);
				needsRelease = true;
			}

			if( surface )
			{
				hr = dev->SetRenderTarget (i, surface);
				if( FAILED(hr) ) {
					ErrorString( Format("RenderTexture error: failed to set render target [%s]", GetD3D9Error(hr)) );
				}
				if (needsRelease)
					surface->Release();
			}
			else
			{
				ErrorString( Format("RenderTexture error: failed to retrieve color surface [%s]", GetD3D9Error(hr)) );
			}
			outRenderTargetWidth = rcolor->width;
			outRenderTargetHeight = rcolor->height;
		}
		else
		{
			hr = dev->SetRenderTarget (i, NULL);
		}
	}
	for (int i = count; i < s_ActiveColorTargetCount; ++i)
	{
		hr = dev->SetRenderTarget (i, NULL);
	}


	// depth surface
	Assert (!rdepth || !rdepth->colorSurface);

	if (rdepth && rdepth->m_Surface)
	{
		// Make sure this texture is not used when setting it as render target
		if (rdepth->textureID.m_ID)
			UnbindTextureD3D9( rdepth->textureID );

		// Set depth surface
		if( rdepth->m_Surface )
		{
			#if DEBUG_RENDER_TEXTURES
			if (rdepth->textureID.m_ID)
				printf_console( "  RT: depth buffer texture %i\n", rdepth->textureID.m_ID );
			else
				printf_console( "  RT: depth buffer plain %x\n", rdepth->m_Surface );
			#endif
			hr = dev->SetDepthStencilSurface( rdepth->m_Surface );
			if( FAILED(hr) ) {
				ErrorString( Format("RenderTexture error: failed to set depth stencil [%s]", GetD3D9Error(hr)) );
			}
			g_D3DHasDepthStencil = true;
			D3DSURFACE_DESC desc;
			desc.Format = D3DFMT_D16;
			rdepth->m_Surface->GetDesc( &desc );
			g_D3DDepthStencilFormat = desc.Format;
		}
	}
	else
	{
		#if DEBUG_RENDER_TEXTURES
		printf_console( "  RT: depth buffer none\n" );
		#endif
		dev->SetDepthStencilSurface( NULL );
		g_D3DHasDepthStencil = false;
		g_D3DDepthStencilFormat = D3DFMT_UNKNOWN;
	}

	for (int i = 0; i < count; ++i)
		s_ActiveColorTargets[i] = reinterpret_cast<RenderColorSurfaceD3D9*>(colorHandles[i].object);
	s_ActiveColorTargetCount = count;
	s_ActiveDepthTarget = rdepth;
	s_ActiveFace = face;
	s_ActiveMip = mipLevel;

	if (outIsBackBuffer)
	{
		s_ActiveColorBackBuffer = (RenderColorSurfaceD3D9*)colorHandles[0].object;
		s_ActiveDepthBackBuffer = (RenderDepthSurfaceD3D9*)depthHandle.object;

		// we are rendering to "default FBO", so current target is dummy
		// as a side effect, if we change swap chain, it will be set correctly, and active remain valid
		s_ActiveColorTargets[0] = s_DummyColorBackBuffer;
		s_ActiveDepthTarget 	= s_DummyDepthBackBuffer;
	}
	return true;
}

RenderSurfaceHandle GetActiveRenderColorSurfaceD3D9(int index)
{
	return RenderSurfaceHandle(s_ActiveColorTargets[index]);
}
RenderSurfaceHandle GetActiveRenderDepthSurfaceD3D9()
{
	return RenderSurfaceHandle(s_ActiveDepthTarget);
}

bool IsActiveRenderTargetWithColorD3D9()
{
	return !s_ActiveColorTargets[0] || s_ActiveColorTargets[0]->backBuffer || !(s_ActiveColorTargets[0]->flags & kSurfaceCreateNeverUsed);
}


RenderSurfaceHandle CreateRenderColorSurfaceD3D9( TextureID textureID, int width, int height, int samples, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, TexturesD3D9& textures )
{
	RenderSurfaceHandle rsHandle;

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;
	if( !gGraphicsCaps.supportsRenderTextureFormat[format] )
		return rsHandle;

	RenderColorSurfaceD3D9* rs = new RenderColorSurfaceD3D9;
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->format = format;
	rs->textureID = textureID;
	rs->dim = dim;
	rs->flags = createFlags;

	// Create it
	if (!InitD3DRenderColorSurface(*rs, textures))
	{
		delete rs;
		return rsHandle;
	}

	rsHandle.object = rs;
	return rsHandle;
}

RenderSurfaceHandle CreateRenderDepthSurfaceD3D9( TextureID textureID, int width, int height, int samples, DepthBufferFormat depthFormat, UInt32 createFlags, TexturesD3D9& textures )
{
	RenderSurfaceHandle rsHandle;

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;

	RenderDepthSurfaceD3D9* rs = new RenderDepthSurfaceD3D9;
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->depthFormat = depthFormat;
	rs->textureID = textureID;
	rs->flags = createFlags;

	// Create it
	if (!InitD3DRenderDepthSurface( *rs, textures))
	{
		delete rs;
		return rsHandle;
	}

	rsHandle.object = rs;
	return rsHandle;
}


void DestroyRenderSurfaceD3D9 (RenderSurfaceD3D9* rs)
{
	Assert(rs);

	if(rs == s_ActiveColorBackBuffer || rs == s_ActiveDepthBackBuffer)
	{
	#if DEBUG_RENDER_TEXTURES
		printf_console( "  RT: Destroying main %s buffer.\n", s == s_ActiveColorBackBuffer ? "color" : "depth" );
	#endif
		s_ActiveColorBackBuffer = NULL;
		s_ActiveDepthBackBuffer = NULL;
	}

	RenderSurfaceHandle defaultColor(s_DummyColorBackBuffer);
	RenderSurfaceHandle defaultDepth(s_DummyDepthBackBuffer);

	if (s_ActiveDepthTarget == rs)
	{
		ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
		int targetWidth, targetHeight;
		bool isBackBuffer;
		SetRenderTargetD3D9 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, targetWidth, targetHeight, isBackBuffer);
	}
	for (int i = 0; i < s_ActiveColorTargetCount; ++i)
	{
		if (s_ActiveColorTargets[i] == rs)
		{
			ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
			int targetWidth, targetHeight;
			bool isBackBuffer;
			SetRenderTargetD3D9 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, targetWidth, targetHeight, isBackBuffer);
		}
	}

	if (rs->m_Surface)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(rs->m_Surface);
		ULONG refCount = rs->m_Surface->Release();
		Assert(refCount == (rs->m_Texture ? 1 : 0));
		rs->m_Surface = NULL;
	}
	if( rs->m_Texture )
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(rs->m_Texture);
		ULONG refCount = rs->m_Texture->Release();
		Assert(refCount == 0);
		rs->m_Texture = NULL;
	}
}

void DestroyRenderSurfaceD3D9 (RenderSurfaceHandle& rsHandle, TexturesD3D9& textures)
{
	if( !rsHandle.IsValid() )
		return;

	RenderSurfaceD3D9* rs = reinterpret_cast<RenderSurfaceD3D9*>( rsHandle.object );
	DestroyRenderSurfaceD3D9( rs );

	if (rs->m_Texture || rs->textureID.m_ID)
		textures.RemoveTexture (rs->textureID);

	delete rs;
	rsHandle.object = NULL;
}



// --------------------------------------------------------------------------


#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

SUITE ( RenderTextureD3DTests )
{
TEST(RenderTextureD3DTests_FormatTableCorrect)
{
	// checks that you did not forget to update format table when adding a new format :)
	for (int i = 0; i < kRTFormatCount; ++i)
	{
		CHECK(kD3D9RenderTextureFormats[i] != 0);
	}
}
}
#endif
