#pragma once

#include "D3D9Includes.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

//#define DUMMY_D3D9_CALLS

#ifndef DUMMY_D3D9_CALLS
#define D3D9_CALL(x) x
#define D3D9_CALL_HR(x) x
#else
HRESULT CallDummyD3D9Function();
#define D3D9_CALL(x) CallDummyD3D9Function()
#define D3D9_CALL_HR(x) CallDummyD3D9Function()
#endif


const char* GetD3D9Error( HRESULT hr );
int GetBPPFromD3DFormat( D3DFORMAT format );
int GetStencilBitsFromD3DFormat (D3DFORMAT fmt);
D3DMULTISAMPLE_TYPE GetD3DMultiSampleType (int samples);

bool CheckD3D9DebugRuntime (IDirect3DDevice9* dev);

struct D3D9DepthStencilTexture {
	D3D9DepthStencilTexture() : m_Texture(NULL), m_Surface(NULL) {}

	IDirect3DTexture9*	m_Texture;
	IDirect3DSurface9*	m_Surface;

	void Release() {
		if (m_Texture) {
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_Texture); 
			m_Texture->Release();
			m_Texture = NULL;
		}
		if (m_Surface) {
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_Surface); 
			m_Surface->Release();
			m_Surface = NULL;
		}
	}
};

const D3DFORMAT kD3D9FormatDF16 = (D3DFORMAT)MAKEFOURCC('D','F','1','6');
const D3DFORMAT kD3D9FormatINTZ = (D3DFORMAT)MAKEFOURCC('I','N','T','Z');
const D3DFORMAT kD3D9FormatRAWZ = (D3DFORMAT)MAKEFOURCC('R','A','W','Z');
const D3DFORMAT kD3D9FormatNULL = (D3DFORMAT)MAKEFOURCC('N','U','L','L');
const D3DFORMAT kD3D9FormatRESZ = (D3DFORMAT)MAKEFOURCC('R','E','S','Z');


D3D9DepthStencilTexture CreateDepthStencilTextureD3D9 (
	IDirect3DDevice9* dev, int width, int height, D3DFORMAT format,
	D3DMULTISAMPLE_TYPE msType, DWORD msQuality, BOOL discardable );

static inline DWORD GetD3D9SamplerIndex (ShaderType type, int unit)
{
	switch (type) {
	case kShaderVertex:
		DebugAssert (unit >= 0 && unit < 4); // DX9 has limit of 4 vertex samplers
		return unit + D3DVERTEXTEXTURESAMPLER0;
	case kShaderFragment:
		DebugAssert (unit >= 0 && unit < kMaxSupportedTextureUnits);
		return unit;
	default:
		Assert ("Unsupported shader type for sampler");
		return 0;
	}
}
