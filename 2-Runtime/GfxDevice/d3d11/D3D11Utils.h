#pragma once

#include "D3D11Includes.h"

//#define DUMMY_D3D11_CALLS

#ifndef DUMMY_D3D11_CALLS
#define D3D11_CALL(x) x
#define D3D11_CALL_HR(x) x
#else
HRESULT CallDummyD3D11Function();
#define D3D11_CALL(x) CallDummyD3D11Function()
#define D3D11_CALL_HR(x) CallDummyD3D11Function()
#endif

int GetBPPFromDXGIFormat (DXGI_FORMAT fmt);
void ReportLiveObjectsD3D11 (ID3D11Device* dev);
void SetDebugNameD3D11 (ID3D11DeviceChild* obj, const std::string& name);
std::string GetDebugNameD3D11 (ID3D11DeviceChild* obj);

