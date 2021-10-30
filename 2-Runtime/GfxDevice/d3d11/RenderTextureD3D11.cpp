#include "UnityPrefix.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "D3D11Context.h"
#include "D3D11Utils.h"
#include "TexturesD3D11.h"

void UnbindTextureD3D11 (TextureID texture);

//Resource format
DXGI_FORMAT kD3D11RenderResourceFormats[kRTFormatCount] = {
	DXGI_FORMAT_R8G8B8A8_TYPELESS,
	DXGI_FORMAT_R24G8_TYPELESS,
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	(DXGI_FORMAT)-1, // RGB565, unsupported
	(DXGI_FORMAT)-1, // ARGB4444, unsupported
	(DXGI_FORMAT)-1, // ARGB1555, unsupported
	(DXGI_FORMAT)-1, // Default
	DXGI_FORMAT_R10G10B10A2_TYPELESS,
	(DXGI_FORMAT)-1, // DefaultHDR
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R32G32B32A32_TYPELESS,
	DXGI_FORMAT_R32G32_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R8_TYPELESS, // R8
	DXGI_FORMAT_R32G32B32A32_TYPELESS, // ARGBInt
	DXGI_FORMAT_R32G32_TYPELESS, // RGInt
	DXGI_FORMAT_R32_TYPELESS, // RInt
	DXGI_FORMAT_B8G8R8A8_TYPELESS,
};

//Standard view
DXGI_FORMAT kD3D11RenderTextureFormatsNorm[kRTFormatCount] = {
	DXGI_FORMAT_R8G8B8A8_UNORM,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_R16G16B16A16_FLOAT,
	DXGI_FORMAT_D16_UNORM,
	(DXGI_FORMAT)-1, // RGB565, unsupported
	(DXGI_FORMAT)-1, // ARGB4444, unsupported
	(DXGI_FORMAT)-1, // ARGB1555, unsupported
	(DXGI_FORMAT)-1, // Default
	DXGI_FORMAT_R10G10B10A2_UNORM,
	(DXGI_FORMAT)-1, // DefaultHDR
	DXGI_FORMAT_R16G16B16A16_UNORM,
	DXGI_FORMAT_R32G32B32A32_FLOAT,
	DXGI_FORMAT_R32G32_FLOAT,
	DXGI_FORMAT_R16G16_FLOAT,
	DXGI_FORMAT_R32_FLOAT,
	DXGI_FORMAT_R16_FLOAT,
	DXGI_FORMAT_R8_UNORM, // R8
	DXGI_FORMAT_R32G32B32A32_SINT, // ARGBInt
	DXGI_FORMAT_R32G32_SINT, // RGInt
	DXGI_FORMAT_R32_SINT, // RInt
	DXGI_FORMAT_B8G8R8A8_UNORM, // BGRA32
};

// SRGBView... only used for RGBA8 buffers really.
DXGI_FORMAT kD3D11RenderTextureFormatsSRGB[kRTFormatCount] = {
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_R16G16B16A16_FLOAT,
	DXGI_FORMAT_D16_UNORM,
	(DXGI_FORMAT)-1, // RGB565, unsupported
	(DXGI_FORMAT)-1, // ARGB4444, unsupported
	(DXGI_FORMAT)-1, // ARGB1555, unsupported
	(DXGI_FORMAT)-1, // Default
	DXGI_FORMAT_R10G10B10A2_UNORM,
	(DXGI_FORMAT)-1, // DefaultHDR
	DXGI_FORMAT_R16G16B16A16_UNORM,
	DXGI_FORMAT_R32G32B32A32_FLOAT,
	DXGI_FORMAT_R32G32_FLOAT,
	DXGI_FORMAT_R16G16_FLOAT,
	DXGI_FORMAT_R32_FLOAT,
	DXGI_FORMAT_R16_FLOAT,
	DXGI_FORMAT_R8_UNORM, // R8
	DXGI_FORMAT_R32G32B32A32_SINT, // ARGBInt
	DXGI_FORMAT_R32G32_SINT, // RGInt
	DXGI_FORMAT_R32_SINT, // RInt
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
};

DXGI_FORMAT GetRenderTextureFormat (RenderTextureFormat format, bool sRGB)
{
	return sRGB ? kD3D11RenderTextureFormatsSRGB[format] : kD3D11RenderTextureFormatsNorm[format];
}

DXGI_FORMAT GetShaderResourceViewFormat (RenderTextureFormat format, bool sRGB)
{
	if (format == kRTFormatDepth)
	{
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	else if (format == kRTFormatShadowMap)
	{
		return DXGI_FORMAT_R16_UNORM;
	}
	else
	{
		return sRGB ? kD3D11RenderTextureFormatsSRGB[format] : kD3D11RenderTextureFormatsNorm[format];
	}
}


static ID3D11Resource* CreateTextureD3D11 (int width, int height, int depth, int mipLevels, DXGI_FORMAT format, UINT bindFlags, TextureDimension dim, int antiAlias)
{
	if (dim == kTexDim3D)
	{
		if (gGraphicsCaps.buggyMipmapped3DTextures)
			mipLevels = 1;
		D3D11_TEXTURE3D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.Depth = depth;
		desc.MipLevels = mipLevels;
		desc.Format = format;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = bindFlags;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		if (mipLevels > 1) desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		ID3D11Texture3D* res = NULL;
		HRESULT hr = GetD3D11Device()->CreateTexture3D (&desc, NULL, &res);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (res, Format("RenderTexture-3D-%dx%dx%d", width, height, depth));
		return res;
	}
	else
	{
		if (dim == kTexDimCUBE && gGraphicsCaps.buggyMipmappedCubemaps)
			mipLevels = 1;
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = mipLevels;
		desc.ArraySize = dim==kTexDimCUBE ? 6 : 1;
		desc.Format = format;
		desc.SampleDesc.Count = antiAlias;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = bindFlags;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		if (dim == kTexDimCUBE) desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (mipLevels > 1) desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		ID3D11Texture2D* res = NULL;
		HRESULT hr = GetD3D11Device()->CreateTexture2D (&desc, NULL, &res);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (res, Format("RenderTexture-2D-%dx%d", width, height));
		return res;
	}
}


static bool InitD3D11RenderColorSurface (RenderColorSurfaceD3D11& rs, TexturesD3D11& textures)
{
	HRESULT hr;
	ID3D11Device* dev = GetD3D11Device();

	bool sRGBPrimary = (rs.flags & kSurfaceCreateSRGB);

	UINT bindFlags = 0;
	if (!IsDepthRTFormat (rs.format))
		bindFlags |= D3D11_BIND_RENDER_TARGET;
	if (rs.textureID.m_ID)
		bindFlags |= D3D11_BIND_SHADER_RESOURCE;
	if (rs.flags & kSurfaceCreateRandomWrite && gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0)
		bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	int mipLevels = 1;
	int accessibleMipLevels = 1;
	if ((rs.flags & kSurfaceCreateMipmap) && !IsDepthRTFormat(rs.format))
	{
		mipLevels = CalculateMipMapCount3D (rs.width, rs.height, rs.depth);
		if (!(rs.flags & kSurfaceCreateAutoGenMips))
			accessibleMipLevels = mipLevels;
	}
	const DXGI_FORMAT texFormat = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 ? kD3D11RenderResourceFormats[rs.format] : kD3D11RenderTextureFormatsNorm[rs.format]);
	if (bindFlags != 0)
		rs.m_Texture = CreateTextureD3D11 (rs.width, rs.height, rs.depth, mipLevels, texFormat, bindFlags, rs.dim, rs.samples);
	else
		rs.m_Texture = NULL;
	// Render Target View
	if (!IsDepthRTFormat (rs.format))
	{
		D3D11_RENDER_TARGET_VIEW_DESC desc, descSecondary;
		desc.Format = GetRenderTextureFormat (rs.format, gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 ? sRGBPrimary : false);
		descSecondary.Format = GetRenderTextureFormat (rs.format, gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 ? !sRGBPrimary : false);
		ID3D11RenderTargetView* rtv = NULL;
		if (rs.dim == kTexDim2D)
		{
			desc.ViewDimension = descSecondary.ViewDimension = rs.samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
			for (int im = 0; im < accessibleMipLevels; ++im)
			{
				desc.Texture2D.MipSlice = descSecondary.Texture2D.MipSlice = im;
				hr = dev->CreateRenderTargetView (rs.m_Texture, &desc, &rtv);
			Assert (SUCCEEDED(hr));
				rs.SetRTV (0, im, false, rtv);
				hr = dev->CreateRenderTargetView (rs.m_Texture, &descSecondary, &rtv);
			Assert (SUCCEEDED(hr));
				rs.SetRTV (0, im, true, rtv);
			}
		}
		else if (rs.dim == kTexDimCUBE)
		{
			desc.ViewDimension = descSecondary.ViewDimension = rs.samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = descSecondary.Texture2DArray.ArraySize = 1;
			for (int im = 0; im < accessibleMipLevels; ++im)
			{
				desc.Texture2DArray.MipSlice = descSecondary.Texture2DArray.MipSlice = im;
			for (int i = 0; i < 6; ++i)
			{
				desc.Texture2DArray.FirstArraySlice = descSecondary.Texture2DArray.FirstArraySlice =  i;
					hr = dev->CreateRenderTargetView (rs.m_Texture, &desc, &rtv);
				Assert (SUCCEEDED(hr));
					rs.SetRTV (i, im, false, rtv);

					hr = dev->CreateRenderTargetView (rs.m_Texture, &descSecondary, &rtv);
				Assert (SUCCEEDED(hr));
					rs.SetRTV (i, im, true, rtv);
				}
			}
		}
		else if (rs.dim == kTexDim3D)
		{
			desc.ViewDimension = descSecondary.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipSlice = descSecondary.Texture3D.MipSlice = 0;
			desc.Texture3D.FirstWSlice = descSecondary.Texture3D.FirstWSlice = 0;
			desc.Texture3D.WSize = descSecondary.Texture3D.WSize = -1;
			hr = dev->CreateRenderTargetView (rs.m_Texture, &desc, &rtv);
			Assert (SUCCEEDED(hr));
			rs.SetRTV (0, 0, false, rtv);

			hr = dev->CreateRenderTargetView (rs.m_Texture, &descSecondary, &rtv);
			Assert (SUCCEEDED(hr));
			rs.SetRTV (0, 0, true, rtv);
		}
	}

	// Shader Resource View if needed
	if (rs.textureID.m_ID)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = GetShaderResourceViewFormat (rs.format, (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0) ? sRGBPrimary : false);
		switch (rs.dim) {
		case kTexDimCUBE: desc.ViewDimension = rs.samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE; break;
		case kTexDim3D: desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D; break;
		default: desc.ViewDimension = rs.samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D; break;
		}
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = mipLevels;

		ID3D11ShaderResourceView* srView = NULL;
		hr = dev->CreateShaderResourceView (rs.m_Texture, &desc, &rs.m_SRView);
		Assert (SUCCEEDED(hr));

		SetDebugNameD3D11 (rs.m_SRView, Format("RenderTexture-SRV-%d-color-%dx%d", rs.textureID.m_ID, rs.width, rs.height));

		// If we need to generate mips, create view without sRGB format. Seems like some drivers
		// have slow paths for updating sRGB formats, and from reading the docs, it's not clear if
		// mip update for sRGB formats is supported everywhere.
		if (mipLevels > 1)
		{
			desc.Format = GetShaderResourceViewFormat (rs.format, false);
			hr = dev->CreateShaderResourceView (rs.m_Texture, &desc, &rs.m_SRViewForMips);
			Assert (SUCCEEDED(hr));
			SetDebugNameD3D11 (rs.m_SRViewForMips, Format("RenderTexture-SRV-%d-color-%dx%d-mips", rs.textureID.m_ID, rs.width, rs.height));
		}
	}

	// UAV if needed
	if (rs.flags & kSurfaceCreateRandomWrite && gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Format = GetShaderResourceViewFormat (rs.format, false); // UAV formats don't support sRGB
		if (rs.dim == kTexDim3D)
		{
			desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
			desc.Texture3D.MipSlice = 0;
			desc.Texture3D.FirstWSlice = 0;
			desc.Texture3D.WSize = -1;
		}
		else
		{
			desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipSlice = 0;
		}

		hr = dev->CreateUnorderedAccessView (rs.m_Texture, &desc, &rs.m_UAView);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (rs.m_UAView, Format("RenderTexture-UAV-%d-color-%dx%d", rs.textureID.m_ID, rs.width, rs.height));
	}

	// add to textures map
	if (rs.textureID.m_ID)
	{
		textures.AddTexture (rs.textureID, rs.m_Texture, rs.m_SRView, rs.m_UAView, false);
	}

	return true;
}

bool InitD3D11RenderDepthSurface (RenderDepthSurfaceD3D11& rs, TexturesD3D11* textures, bool sampleOnly)
{
	HRESULT hr;
	ID3D11Device* dev = GetD3D11Device();

	const bool shadowMap = rs.flags & kSurfaceCreateShadowmap;
	const bool useTypeless = (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 || gGraphicsCaps.d3d11.hasShadows10Level9);
	const bool createZ = (rs.depthFormat != kDepthFormatNone);

	rs.m_Texture = NULL;
	rs.m_DSView = NULL;
	rs.m_SRView = NULL;

	DXGI_FORMAT formatResource;
	DXGI_FORMAT formatDSV;
	DXGI_FORMAT formatSRV;
	if (shadowMap)
	{
		formatResource = DXGI_FORMAT_R16_TYPELESS;
		formatDSV = DXGI_FORMAT_D16_UNORM;
		formatSRV = DXGI_FORMAT_R16_UNORM;
	}
	else if (rs.depthFormat == kDepthFormat16)
	{
		formatResource = useTypeless ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM;
		formatDSV = DXGI_FORMAT_D16_UNORM;
		formatSRV = useTypeless ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_D16_UNORM;
	}
	else
	{
		formatResource = useTypeless ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
		formatDSV = DXGI_FORMAT_D24_UNORM_S8_UINT;
		formatSRV = useTypeless ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	
	// Pre-DX11 feature level hardware can't do depth buffer sampling & have it as a depth stencil,
	// but DX11 can.
	if (gGraphicsCaps.d3d11.featureLevel >= kDX11Level11_0)
		sampleOnly = false;

	if (rs.dim != kTexDim2D)
	{
		// Starting with 10.1 level, we can have a cubemap color surface and a regular 2D depth surface. Before that,
		// have to create the depth surface as a fake cubemap as well. Leave dimension as cubemap only when
		// cubemap was requested AND we're below 10.1
		if (rs.dim != kTexDimCUBE || gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_1)
			rs.dim = kTexDim2D;
	}

	if (createZ)
	{
		UINT bindFlags = sampleOnly ? 0 : D3D11_BIND_DEPTH_STENCIL;
		if (rs.textureID.m_ID && (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 || gGraphicsCaps.d3d11.hasShadows10Level9))
			bindFlags |= D3D11_BIND_SHADER_RESOURCE;
		rs.m_Texture = CreateTextureD3D11 (rs.width, rs.height, 1, 1, formatResource, bindFlags, rs.dim, rs.samples);
	}

	// Depth Stencil view if needed
	if (createZ && !sampleOnly)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = formatDSV;
		desc.ViewDimension = rs.samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Flags = 0;
		desc.Texture2D.MipSlice = 0;
		hr = dev->CreateDepthStencilView (rs.m_Texture, &desc, &rs.m_DSView);
		Assert (SUCCEEDED(hr));
	}

	// Shader Resource View if needed
	if (createZ && rs.textureID.m_ID && (gGraphicsCaps.d3d11.featureLevel >= kDX11Level10_0 || gGraphicsCaps.d3d11.hasShadows10Level9))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = formatSRV;
		desc.ViewDimension = rs.samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView* srView = NULL;
		hr = dev->CreateShaderResourceView (rs.m_Texture, &desc, &rs.m_SRView);
		Assert (SUCCEEDED(hr));
		SetDebugNameD3D11 (rs.m_SRView, Format("RenderTexture-SRV-%d-depth-%dx%d", rs.textureID.m_ID, rs.width, rs.height));
	}

	if (createZ && rs.textureID.m_ID && textures)
		textures->AddTexture (rs.textureID, rs.m_Texture, rs.m_SRView, rs.m_UAView, rs.flags & kSurfaceCreateShadowmap);

	return true;
}

static RenderColorSurfaceD3D11* s_ActiveColorTargets[kMaxSupportedRenderTargets];
static int s_ActiveColorTargetCount;
static bool s_SRGBWrite;
static RenderDepthSurfaceD3D11* s_ActiveDepthTarget = NULL;
static CubemapFace s_ActiveFace = kCubeFaceUnknown;
static int s_ActiveMip = 0;

static RenderColorSurfaceD3D11* s_ActiveColorBackBuffer = NULL;
static RenderDepthSurfaceD3D11* s_ActiveDepthBackBuffer = NULL;

// on dx editor we can switch swapchain underneath
// so lets do smth like gl's default FBO
// it will be used only from "user" code and we will select proper swap chain here
static RenderColorSurfaceD3D11* s_DummyColorBackBuffer = NULL;
static RenderDepthSurfaceD3D11* s_DummyDepthBackBuffer = NULL;

RenderSurfaceBase* DummyColorBackBuferD3D11()
{
	if(s_DummyColorBackBuffer == 0)
	{
		static RenderColorSurfaceD3D11 __bb;
		RenderSurfaceBase_InitColor(__bb);
		__bb.backBuffer = true;

		s_DummyColorBackBuffer = &__bb;
	}
	return s_DummyColorBackBuffer;
}

RenderSurfaceBase* DummyDepthBackBuferD3D11()
{
	if(s_DummyDepthBackBuffer == 0)
	{
		static RenderDepthSurfaceD3D11 __bb;
		RenderSurfaceBase_InitDepth(__bb);
		__bb.backBuffer = true;

		s_DummyDepthBackBuffer = &__bb;
	}
	return s_DummyDepthBackBuffer;
}


static int s_UAVMaxIndex = -1;
static TextureID s_UAVTextures[kMaxSupportedRenderTargets];
static ComputeBufferID s_UAVBuffers[kMaxSupportedRenderTargets];



static bool SetRenderTargetD3D11Internal (int count, RenderColorSurfaceD3D11** colorSurfaces, RenderDepthSurfaceD3D11* depthSurface, int mipLevel, CubemapFace face, int* outTargetWidth, int* outTargetHeight, TexturesD3D11* textures, bool forceRebind)
{
	RenderColorSurfaceD3D11* rcolorZero = colorSurfaces[0];

	// check context is created
	ID3D11DeviceContext* ctx = GetD3D11Context();
	if (!ctx)
	{
		Assert (!rcolorZero && !depthSurface);
		return false;
	}

	bool sRGBWriteDesired = GetRealGfxDevice().GetSRGBWrite();

	// Exit if nothing to do
	if (!forceRebind && count == s_ActiveColorTargetCount && sRGBWriteDesired == s_SRGBWrite && s_ActiveDepthTarget == depthSurface && s_ActiveFace == face && s_ActiveMip == mipLevel)
	{
		bool colorsSame = true;
		for (int i = 0; i < count; ++i)
		{
			if (s_ActiveColorTargets[i] != colorSurfaces[i])
				colorsSame = false;
		}
		if (colorsSame)
		{
			if (rcolorZero != s_DummyColorBackBuffer)
			{
				if (outTargetWidth)		*outTargetWidth = rcolorZero->width;
				if (outTargetHeight)	*outTargetHeight = rcolorZero->height;
			}
			return false;
		}
	}

	Assert(rcolorZero->backBuffer == depthSurface->backBuffer);
	bool isBackBuffer = rcolorZero->backBuffer;


	GfxDevice& device = GetRealGfxDevice();
	if (!isBackBuffer)
		device.GetFrameStats().AddRenderTextureChange(); // stats

	if(rcolorZero->backBuffer && rcolorZero == s_DummyColorBackBuffer)
		rcolorZero = colorSurfaces[0] = s_ActiveColorBackBuffer;
	if(depthSurface->backBuffer && depthSurface == s_DummyDepthBackBuffer)
		depthSurface = s_ActiveDepthBackBuffer;


	HRESULT hr;

	if (rcolorZero)
	{
		if (depthSurface && depthSurface->textureID.m_ID)
			UnbindTextureD3D11 (depthSurface->textureID);

		Assert (rcolorZero->colorSurface);
		Assert (!depthSurface || !depthSurface->colorSurface);

		int faceIndex = clamp<int>(face, 0, 5);
		ID3D11RenderTargetView* rtvs[kMaxSupportedRenderTargets];
		for (int i = 0; i < count; ++i)
		{
			RenderColorSurfaceD3D11* rcolor = colorSurfaces[i];
			if (rcolor->textureID.m_ID)
				UnbindTextureD3D11 (rcolor->textureID);

			bool wantSecondaryView = ((rcolor->flags & kSurfaceCreateSRGB) != 0) != sRGBWriteDesired;
			rtvs[i] = rcolor->GetRTV(faceIndex, mipLevel, wantSecondaryView);
		}

		const int uavCount = s_UAVMaxIndex - (count-1);
		if (uavCount <= 0)
		{
			// set render targets
			ctx->OMSetRenderTargets (count, rtvs, depthSurface ? depthSurface->m_DSView : NULL);
		}
		else
		{
			DebugAssert (uavCount > 0 && uavCount <= kMaxSupportedRenderTargets);
			// set render targets and UAVs
			ID3D11UnorderedAccessView* uavs[kMaxSupportedRenderTargets];
			for (int i = 0; i < uavCount; ++i)
			{
				int idx = i + count;
				DebugAssert (idx >= 0 && idx < kMaxSupportedRenderTargets);
				ID3D11UnorderedAccessView* uav = NULL;
				if (s_UAVTextures[idx].m_ID && textures)
				{
					TexturesD3D11::D3D11Texture* tex = textures->GetTexture (s_UAVTextures[idx]);
					if (tex)
						uav = tex->m_UAV;
				}
				else if (s_UAVBuffers[idx].IsValid() && textures)
				{
					ComputeBuffer11* cb = textures->GetComputeBuffer (s_UAVBuffers[idx]);
					if (cb)
						uav = cb->uav;
				}
				uavs[i] = uav;
			}
			UINT uavInitialCounts[kMaxSupportedRenderTargets];
			for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
				uavInitialCounts[i] = 0; // reset offsets for Countable/Appendable/Consumeable UAVs
			ctx->OMSetRenderTargetsAndUnorderedAccessViews (count, rtvs, depthSurface ? depthSurface->m_DSView : NULL, count, uavCount, uavs, uavInitialCounts);
		}

		g_D3D11CurrRT = rtvs[0];
		g_D3D11CurrColorRT = rcolorZero;
		g_D3D11CurrDS = depthSurface ? depthSurface->m_DSView : NULL;
		g_D3D11CurrDepthRT = depthSurface;
		if (outTargetWidth)	*outTargetWidth = rcolorZero->width;
		if (outTargetHeight)*outTargetHeight = rcolorZero->height;
	}

	// If we previously had a mip-mapped render texture, generate mip levels for it now.
	RenderColorSurfaceD3D11* prevRT = s_ActiveColorTargets[0];
	RenderColorSurfaceD3D11* currRT = colorSurfaces[0];
	if (prevRT &&
		(prevRT->flags & kSurfaceCreateMipmap) &&
		(prevRT->flags & kSurfaceCreateAutoGenMips) &&
		prevRT->m_SRViewForMips &&
		currRT != prevRT)
	{
		ctx->GenerateMips (prevRT->m_SRViewForMips);
	}

	for (int i = 0; i < count; ++i)
		s_ActiveColorTargets[i] = colorSurfaces[i];

	s_ActiveColorTargetCount = count;
	s_ActiveDepthTarget = depthSurface;
	s_ActiveFace = face;
	s_ActiveMip = mipLevel;

	if (isBackBuffer)
	{
		s_ActiveColorBackBuffer = rcolorZero;
		s_ActiveDepthBackBuffer = depthSurface;
		
		// we are rendering to "default FBO", so current target is dummy
		// as a side effect, if we change swap chain, it will be set correctly, and active remain valid
		s_ActiveColorTargets[0] = s_DummyColorBackBuffer;
		s_ActiveDepthTarget 	= s_DummyDepthBackBuffer;
	}

	s_SRGBWrite = sRGBWriteDesired;
	return true;
}

bool SetRenderTargetD3D11 (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, int* outTargetWidth, int* outTargetHeight, TexturesD3D11* textures)
{
	RenderColorSurfaceD3D11* colorTargets[kMaxSupportedRenderTargets];
	RenderDepthSurfaceD3D11* rdepth = reinterpret_cast<RenderDepthSurfaceD3D11*>( depthHandle.object );
	for (int i = 0; i < count; ++i)
	{
		colorTargets[i] = reinterpret_cast<RenderColorSurfaceD3D11*>(colorHandles[i].object);
	}

	return SetRenderTargetD3D11Internal (count, colorTargets, rdepth, mipLevel, face, outTargetWidth, outTargetHeight, textures, false);
}

bool RebindActiveRenderTargets(TexturesD3D11* textures)
{
	int width, height;
	return SetRenderTargetD3D11Internal (s_ActiveColorTargetCount, s_ActiveColorTargets, s_ActiveDepthTarget, s_ActiveMip, s_ActiveFace, &width, &height, textures, true);
}


void SetRandomWriteTargetTextureD3D11 (int index, TextureID tid)
{
	Assert (index >= 0 && index < ARRAY_SIZE(s_UAVTextures));
	s_UAVMaxIndex = std::max (s_UAVMaxIndex, index);
	s_UAVTextures[index] = tid;
	s_UAVBuffers[index] = ComputeBufferID();
}

void SetRandomWriteTargetBufferD3D11 (int index, ComputeBufferID bufferHandle)
{
	Assert (index >= 0 && index < ARRAY_SIZE(s_UAVBuffers));
	s_UAVMaxIndex = std::max (s_UAVMaxIndex, index);
	s_UAVBuffers[index] = bufferHandle;
	s_UAVTextures[index].m_ID = 0;
}

void ClearRandomWriteTargetsD3D11 (TexturesD3D11* textures)
{
	const bool resetRenderTargets = (s_UAVMaxIndex != -1);

	s_UAVMaxIndex = -1;
	for (int i = 0; i < ARRAY_SIZE(s_UAVTextures); ++i)
		s_UAVTextures[i].m_ID = 0;
	for (int i = 0; i < ARRAY_SIZE(s_UAVBuffers); ++i)
		s_UAVBuffers[i] = ComputeBufferID();

	if (resetRenderTargets)
		RebindActiveRenderTargets (textures);
}


RenderSurfaceHandle GetActiveRenderColorSurfaceD3D11(int index)
{
	return RenderSurfaceHandle(s_ActiveColorTargets[index]);
}
RenderSurfaceHandle GetActiveRenderDepthSurfaceD3D11()
{
	return RenderSurfaceHandle(s_ActiveDepthTarget);
}

RenderSurfaceHandle GetActiveRenderColorSurfaceBBD3D11()
{
	RenderColorSurfaceD3D11* ret = s_ActiveColorTargets[0];
	if(ret == s_DummyColorBackBuffer) 
		ret = s_ActiveColorBackBuffer;

	return RenderSurfaceHandle(ret);
}

bool IsActiveRenderTargetWithColorD3D11()
{
	return !s_ActiveColorTargets[0] || s_ActiveColorTargets[0]->backBuffer || s_ActiveColorTargets[0]->m_Texture;
}

RenderSurfaceHandle CreateRenderColorSurfaceD3D11 (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, UInt32 createFlags, RenderTextureFormat format, TexturesD3D11& textures)
{
	RenderSurfaceHandle rsHandle;

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;
	if( !gGraphicsCaps.supportsRenderTextureFormat[format] )
		return rsHandle;

	RenderColorSurfaceD3D11* rs = new RenderColorSurfaceD3D11;
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->depth = depth;
	rs->format = format;
	rs->textureID = textureID;
	rs->dim = dim;
	rs->flags = createFlags;

	// Create it
	if (!InitD3D11RenderColorSurface(*rs, textures))
	{
		delete rs;
		return rsHandle;
	}

	rsHandle.object = rs;
	return rsHandle;
}

RenderSurfaceHandle CreateRenderDepthSurfaceD3D11 (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags, TexturesD3D11& textures)
{
	RenderSurfaceHandle rsHandle;

	if( !gGraphicsCaps.hasRenderToTexture )
		return rsHandle;

	RenderDepthSurfaceD3D11* rs = new RenderDepthSurfaceD3D11;
	rs->width = width;
	rs->height = height;
	rs->samples = samples;
	rs->dim = dim;
	rs->depthFormat = depthFormat;
	rs->textureID = textureID;
	rs->flags = createFlags;

	// Create it
	bool sampleOnly = (createFlags & kSurfaceCreateSampleOnly) != 0;
	if (!InitD3D11RenderDepthSurface (*rs, &textures, sampleOnly))
	{
		delete rs;
		return rsHandle;
	}

	rsHandle.object = rs;
	return rsHandle;
}

void InternalDestroyRenderSurfaceD3D11 (RenderSurfaceD3D11* rs, TexturesD3D11* textures)
{
	AssertIf( !rs );

	if(rs == s_ActiveColorBackBuffer || rs == s_ActiveDepthBackBuffer)
	{
		s_ActiveColorBackBuffer = NULL;
		s_ActiveDepthBackBuffer = NULL;
	}

	RenderSurfaceHandle defaultColor(s_DummyColorBackBuffer);
	RenderSurfaceHandle defaultDepth(s_DummyDepthBackBuffer);

	for (int i = 0; i < s_ActiveColorTargetCount; ++i)
	{
		if (s_ActiveColorTargets[i] == rs)
		{
			ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
			SetRenderTargetD3D11 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, NULL, NULL, textures);
		}
	}
	if (s_ActiveDepthTarget == rs)
	{
		ErrorString( "RenderTexture warning: Destroying active render texture. Switching to main context." );
		SetRenderTargetD3D11 (1, &defaultColor, defaultDepth, 0, kCubeFaceUnknown, NULL, NULL, textures);
	}

	if (rs->m_Texture || rs->textureID.m_ID)
	{
		UnbindTextureD3D11 (rs->textureID);
		if (textures)
			textures->RemoveTexture (rs->textureID);
	}

	REGISTER_EXTERNAL_GFX_DEALLOCATION(rs->m_Texture);
	if (rs->colorSurface)
	{
		RenderColorSurfaceD3D11* colorRS = static_cast<RenderColorSurfaceD3D11*>(rs);
		colorRS->Reset();
	}
	else
	{
		RenderDepthSurfaceD3D11* depthRS = static_cast<RenderDepthSurfaceD3D11*>(rs);
		depthRS->Reset();
	}
}

void DestroyRenderSurfaceD3D11 (RenderSurfaceHandle& rsHandle, TexturesD3D11& textures)
{
	if( !rsHandle.IsValid() )
		return;

	RenderSurfaceD3D11* rs = reinterpret_cast<RenderSurfaceD3D11*>( rsHandle.object );
	InternalDestroyRenderSurfaceD3D11 (rs, &textures);
	delete rs;
	rsHandle.object = NULL;
}



// --------------------------------------------------------------------------


#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

SUITE (RenderTextureD3D11Tests)
{
TEST(RenderTextureD3D11_FormatTableCorrect)
{
	// checks that you did not forget to update format table when adding a new format :)
	for (int i = 0; i < kRTFormatCount; ++i)
	{
		CHECK(kD3D11RenderResourceFormats[i] != 0);
		CHECK(kD3D11RenderTextureFormatsNorm[i] != 0);
		CHECK(kD3D11RenderTextureFormatsSRGB[i] != 0);
	}
}
}

#endif
