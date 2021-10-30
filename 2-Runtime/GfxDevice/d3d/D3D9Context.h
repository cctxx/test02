#pragma once

#include "D3D9Includes.h"
#include "D3D9Enumeration.h"

bool InitializeD3D(D3DDEVTYPE devtype);
void CleanupD3D();
bool InitializeOrResetD3DDevice(
	class GfxDevice* device,
	HWND window, int width, int height,
	int refreshRate, bool fullscreen, int vBlankCount, int fsaa,
	int& outBackbufferBPP, int& outFrontbufferBPP, int& outDepthBPP, int& outFSAA );
void GetBackBuffersAfterDeviceReset();
bool ResetD3DDevice();
#if UNITY_EDITOR
void EditorInitializeD3D(GfxDevice* device);
#endif
bool FullResetD3DDevice();
bool HandleD3DDeviceLost();
void DestroyD3DDevice();
extern D3DDEVTYPE g_D3DDevType;
extern DWORD g_D3DAdapter;
extern bool g_D3DUsesMixedVP;
extern bool g_D3DHasDepthStencil;
extern D3DFORMAT g_D3DDepthStencilFormat;

IDirect3DDevice9* GetD3DDevice();
IDirect3DDevice9* GetD3DDeviceNoAssert();
IDirect3D9* GetD3DObject();
D3D9FormatCaps* GetD3DFormatCaps();
D3DFORMAT GetD3DFormatForChecks();

typedef int (WINAPI* D3DPERF_BeginEventFunc)(D3DCOLOR, LPCWSTR);
typedef int (WINAPI* D3DPERF_EndEventFunc)();
extern D3DPERF_BeginEventFunc g_D3D9BeginEventFunc;
extern D3DPERF_EndEventFunc g_D3D9EndEventFunc;


#if UNITY_EDITOR
bool CreateHiddenWindowD3D();
void DestroyHiddenWindowD3D();
extern HWND s_HiddenWindowD3D;
#endif

