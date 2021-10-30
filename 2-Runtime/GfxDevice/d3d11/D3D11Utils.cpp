#include "UnityPrefix.h"
#include "D3D11Utils.h"


#ifdef DUMMY_D3D11_CALLS
HRESULT CallDummyD3D11Function()
{
	return S_OK;
}
#endif

void ReportLiveObjectsD3D11 (ID3D11Device* dev)
{
	ID3D11Debug* dbg = NULL;
	if (FAILED(dev->QueryInterface(IID_ID3D11Debug, (void**)&dbg)))
		return;
	if (!dbg)
		return;

	dbg->ReportLiveDeviceObjects (D3D11_RLDO_DETAIL);
	dbg->Release();
}


void SetDebugNameD3D11 (ID3D11DeviceChild* obj, const std::string& name)
{
	if (obj)
		obj->SetPrivateData (WKPDID_D3DDebugObjectName, name.size(), name.c_str());
}

std::string GetDebugNameD3D11 (ID3D11DeviceChild* obj)
{
	if (obj)
	{
		char tmp[1024];
		int maxLength = sizeof(tmp) - 1;
		UINT size = maxLength;
		obj->GetPrivateData (WKPDID_D3DDebugObjectName, &size, tmp);
		tmp[size] = '\0';
		tmp[maxLength] = '\0';
		return tmp;
	}
	return "";
}



int GetBPPFromDXGIFormat (DXGI_FORMAT fmt)
{
	if (fmt == DXGI_FORMAT_UNKNOWN)
		return 0;
	if (fmt >= DXGI_FORMAT_R32G32B32A32_TYPELESS && fmt <= DXGI_FORMAT_R32G32B32A32_SINT)
		return 128;
	if (fmt >= DXGI_FORMAT_R32G32B32_TYPELESS && fmt <= DXGI_FORMAT_R32G32B32_SINT)
		return 96;
	if (fmt >= DXGI_FORMAT_R16G16B16A16_TYPELESS && fmt <= DXGI_FORMAT_X32_TYPELESS_G8X24_UINT)
		return 64;
	if (fmt >= DXGI_FORMAT_R10G10B10A2_TYPELESS && fmt <= DXGI_FORMAT_X24_TYPELESS_G8_UINT)
		return 32;
	if (fmt >= DXGI_FORMAT_R8G8_TYPELESS && fmt <= DXGI_FORMAT_R16_SINT)
		return 16;
	if (fmt >= DXGI_FORMAT_R8_TYPELESS && fmt <= DXGI_FORMAT_A8_UNORM)
		return 8;
	if (fmt == DXGI_FORMAT_R1_UNORM)
		return 1;
	if (fmt >= DXGI_FORMAT_R9G9B9E5_SHAREDEXP && fmt <= DXGI_FORMAT_G8R8_G8B8_UNORM)
		return 32;
	if (fmt >= DXGI_FORMAT_BC1_TYPELESS && fmt <= DXGI_FORMAT_BC1_UNORM_SRGB) // DXT1
		return 4;
	if (fmt >= DXGI_FORMAT_BC2_TYPELESS && fmt <= DXGI_FORMAT_BC3_UNORM_SRGB) // DXT3/5
		return 8;
	if (fmt >= DXGI_FORMAT_BC4_TYPELESS && fmt <= DXGI_FORMAT_BC4_SNORM)
		return 4;
	if (fmt >= DXGI_FORMAT_BC5_TYPELESS && fmt <= DXGI_FORMAT_BC5_SNORM)
		return 8;
	if (fmt >= DXGI_FORMAT_B5G6R5_UNORM && fmt <= DXGI_FORMAT_B5G5R5A1_UNORM)
		return 16;
	if (fmt >= DXGI_FORMAT_B8G8R8A8_UNORM && fmt <= DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)
		return 32;
	if (fmt >= DXGI_FORMAT_BC6H_TYPELESS && fmt <= DXGI_FORMAT_BC7_UNORM_SRGB)
		return 8;
	AssertString ("Unknown DXGI format");
	return 0;
}

