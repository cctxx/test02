#pragma once

#include "D3D11Includes.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Graphics/RenderSurface.h"
#include "Runtime/Threads/AtomicOps.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <map>

class ImageReference;

struct ComputeBuffer11
{
	ID3D11Buffer* buffer;
	ID3D11ShaderResourceView* srv;
	ID3D11UnorderedAccessView* uav;
};


class TexturesD3D11
{
public:
	enum { kSamplerHasMipMap = (1<<0), kSamplerShadowMap = (1<<1) };
	struct D3D11Sampler {
		D3D11Sampler() {
			memset (this, 0, sizeof(*this));
		}
		float bias;
		UInt8 filter; // TextureFilterMode
		UInt8 wrap; // TextureWrapMode
		UInt8 anisoLevel;
		UInt8 flags;
	};
	struct D3D11Texture {
		D3D11Texture() : m_Texture(NULL), m_SRV(NULL), m_UAV(NULL) { }
		explicit D3D11Texture (ID3D11Resource* tex, ID3D11ShaderResourceView* srv, ID3D11UnorderedAccessView* uav, bool shadowMap)
			: m_Texture(tex), m_SRV(srv), m_UAV(uav) { if (shadowMap) m_Sampler.flags |= kSamplerShadowMap; }
		ID3D11Resource*	m_Texture;
		ID3D11ShaderResourceView* m_SRV;
		ID3D11UnorderedAccessView* m_UAV;
		D3D11Sampler	m_Sampler;
	};

public:
	TexturesD3D11();
	~TexturesD3D11();

	void ClearTextureResources();

	bool SetTexture (ShaderType shaderType, int unit, int sampler, TextureID textureID, float bias);
	void SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace );

	void DeleteTexture( TextureID textureID );

	void UploadTexture2D(
		TextureID tid, TextureDimension dimension, UInt8* srcData, int width, int height,
		TextureFormat format, int mipCount, UInt32 uploadFlags, int masterTextureLimit, TextureUsageMode usageMode, TextureColorSpace colorSpace );

	void UploadTextureSubData2D(
		TextureID tid, UInt8* srcData, int mipLevel,
		int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace );

	void UploadTextureCube(
		TextureID tid, UInt8* srcData, int faceDataSize, int size,
		TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace );

	void UploadTexture3D(
		TextureID tid, UInt8* srcData, int width, int height, int depth,
		TextureFormat format, int mipCount, UInt32 uploadFlags );

	void AddTexture (TextureID textureID, ID3D11Resource* texture, ID3D11ShaderResourceView* srv, ID3D11UnorderedAccessView* uav, bool shadowMap);
	void RemoveTexture( TextureID textureID );
	D3D11Texture* GetTexture(TextureID textureID);
	ID3D11SamplerState* GetSampler(const D3D11Sampler& texSampler);
	ID3D11SamplerState* GetSampler(BuiltinSamplerState sampler);

	void AddComputeBuffer (ComputeBufferID id, const ComputeBuffer11& buf);
	void RemoveComputeBuffer (ComputeBufferID id);
	ComputeBuffer11* GetComputeBuffer (ComputeBufferID id);

	void InvalidateSamplers();
	void InvalidateSampler (ShaderType shader, int samplerUnit) { m_ActiveD3DSamplers[shader][samplerUnit] = NULL; }

	intptr_t	RegisterNativeTexture(ID3D11ShaderResourceView* resourceView) const;
	void		UpdateNativeTexture(TextureID textureID, ID3D11ShaderResourceView* resourceView);

private:
	void Upload2DData (const UInt8* dataPtr, TextureFormat dataFormat, int width, int height, bool decompressData, ID3D11Resource* dst, DXGI_FORMAT dstFormat, bool bgra, int dstSubResource);

private:
	typedef std::map< D3D11Sampler, ID3D11SamplerState*, memcmp_less<D3D11Sampler> > SamplerMap;
	SamplerMap	m_Samplers;

	ID3D11SamplerState*	m_ActiveD3DSamplers[kShaderTypeCount][kMaxSupportedTextureUnits];

	// staging texture key: width, height, dxgi format
	enum {
		kStagingWidthShift = 0ULL,
		kStagingHeightShift = 20ULL,
		kStagingFormatShift = 40ULL,
	};
	typedef std::map<UInt64, ID3D11Resource*> StagingTextureMap;
	StagingTextureMap	m_StagingTextures;

	typedef std::map<ComputeBufferID, ComputeBuffer11> ComputeBufferMap;
	ComputeBufferMap	m_ComputeBuffers;

	void TextureFromShaderResourceView(ID3D11ShaderResourceView* resourceView, ID3D11Texture2D** texture) const;
};


struct RenderSurfaceD3D11 : RenderSurfaceBase, NonCopyable
{
	RenderSurfaceD3D11()
		: m_Texture(NULL)
		, m_SRView(NULL)
		, m_SRViewForMips(NULL)
		, m_UAView(NULL)
		, depth(0)
		, dim(kTexDim2D)
	{
		RenderSurfaceBase_Init(*this);
	}
	ID3D11Resource*	m_Texture;
	ID3D11ShaderResourceView* m_SRView;
	ID3D11ShaderResourceView* m_SRViewForMips; // always without sRGB
	ID3D11UnorderedAccessView* m_UAView;
	int			depth;
	TextureDimension dim;
protected:
	void Reset()
	{
		SAFE_RELEASE(m_Texture);
		SAFE_RELEASE(m_SRView);
		SAFE_RELEASE(m_SRViewForMips);
		SAFE_RELEASE(m_UAView);
		RenderSurfaceBase_Init(*this);
		depth = 0;
		dim = kTexDim2D;
	}
};

struct RenderColorSurfaceD3D11 : public RenderSurfaceD3D11
{
	RenderColorSurfaceD3D11()
		: format(kRTFormatARGB32)
	{
		colorSurface = true;
	}

	static UInt32 GetRTVKey(int face, int mipLevel, bool secondary) {
		return (face) | (secondary ? 8 : 0) | (mipLevel << 4);
	}
	ID3D11RenderTargetView* GetRTV(int face, int mipLevel, bool secondary) {
		UInt32 key = GetRTVKey(face, mipLevel, secondary);
		for (int i = 0, n = m_RTVs.size(); i != n; ++i)
			if (m_RTVs[i].first == key)
				return m_RTVs[i].second;
		return NULL;
		}
	void SetRTV(int face, int mipLevel, bool secondary, ID3D11RenderTargetView* rtv) {
		DebugAssert(GetRTV(face, mipLevel, secondary) == NULL);
		UInt32 key = GetRTVKey(face, mipLevel, secondary);
		m_RTVs.push_back (std::make_pair(key, rtv));
	}

	void Reset()
	{
		RenderSurfaceD3D11::Reset();
		colorSurface = true;
		for (int i = 0, n = m_RTVs.size(); i != n; ++i)
		{
			SAFE_RELEASE(m_RTVs[i].second);
		}
		m_RTVs.resize_uninitialized(0);
	}

	typedef std::pair<UInt32, ID3D11RenderTargetView*> RTVPair;
	dynamic_array<RTVPair> m_RTVs;
	RenderTextureFormat	format;
};

struct RenderDepthSurfaceD3D11 : public RenderSurfaceD3D11
{
	RenderDepthSurfaceD3D11()
		: m_DSView(NULL)
		, depthFormat(kDepthFormatNone)
	{
		colorSurface = false;
	}
	ID3D11DepthStencilView* m_DSView;
	DepthBufferFormat depthFormat;
	void Reset()
	{
		RenderSurfaceD3D11::Reset();
		colorSurface = false;
		SAFE_RELEASE(m_DSView);
		depthFormat = kDepthFormatNone;
	}
};
