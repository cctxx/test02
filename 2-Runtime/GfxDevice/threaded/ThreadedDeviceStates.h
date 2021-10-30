#pragma once
#if ENABLE_MULTITHREADED_CODE

#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceResources.h"
#include "Runtime/GfxDevice/threaded/ClientIDMapper.h"
#include "Runtime/Filters/Mesh/VertexData.h"
#include "Runtime/Graphics/RenderSurface.h"

namespace ShaderLab { struct TextureBinding; }
namespace xenon { class IVideoPlayer; }
class VBO;
class RawVBO;
class GfxTimerQuery;
class GfxDeviceWindow;

struct ClientDeviceRect
{
	ClientDeviceRect() : x(0), y(0), width(0), height(0) {}
	int x;
	int y;
	int width;
	int height;
};

struct ClientDeviceBlendState : public DeviceBlendState
{
	ClientDeviceBlendState(const GfxBlendState& src) : DeviceBlendState(src), internalState(NULL) {}
	ClientIDWrapper(DeviceBlendState) internalState;
};

struct ClientDeviceDepthState : public DeviceDepthState
{
	ClientDeviceDepthState(const GfxDepthState& src) : DeviceDepthState(src), internalState(NULL) {}
	ClientIDWrapper(DeviceDepthState) internalState;
};

struct ClientDeviceStencilState : public DeviceStencilState
{
	ClientDeviceStencilState(const GfxStencilState& src) : DeviceStencilState(src), internalState(NULL) {}
	ClientIDWrapper(DeviceStencilState) internalState;
};

struct ClientDeviceRasterState : public DeviceRasterState
{
	ClientDeviceRasterState(const GfxRasterState& src) : DeviceRasterState(src), internalState(NULL) {}
	ClientIDWrapper(DeviceRasterState) internalState;
};

struct ClientDeviceTextureCombiners
{
	ClientIDWrapperHandle(TextureCombinersHandle) internalHandle;
	ShaderLab::TextureBinding* bindings;
	int count;
};

struct ClientDeviceRenderSurface : RenderSurfaceBase
{
	enum SurfaceState { kInitial, kCleared, kRendered, kResolved };
	ClientDeviceRenderSurface(int w, int h) { RenderSurfaceBase_Init(*this); width=w; height=h; zformat=kDepthFormatNone; state=kInitial; }
	ClientIDWrapperHandle(RenderSurfaceHandle) internalHandle;
	DepthBufferFormat zformat;
	SurfaceState state;
};

struct ClientDeviceVBO
{
	ClientDeviceVBO() : internalVBO(NULL) {}

#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	ClientIDWrapper(VBO) GetInternal() { return internalVBO; }
#else
	VBO* GetInternal() { return const_cast<VBO*>(internalVBO); }
#endif
	volatile ClientIDWrapper(VBO) internalVBO;
};

struct ClientVertexBufferData
{
	ChannelInfoArray channels;
	StreamInfoArray streams;
	int bufferSize;
	int vertexCount;
	int hasData;
};

struct ClientDeviceTimerQuery
{
	ClientDeviceTimerQuery() : internalQuery(NULL), elapsed(0), pending(false) {}
	GfxTimerQuery* GetInternal() { return const_cast<GfxTimerQuery*>(internalQuery); }
	volatile GfxTimerQuery* internalQuery;
	volatile UInt64 elapsed;
	volatile bool pending;
};

struct ClientDeviceWindow
{
	ClientDeviceWindow() : internalWindow(NULL) {}
	GfxDeviceWindow* GetInternal() { return const_cast<GfxDeviceWindow*>(internalWindow); }
	volatile GfxDeviceWindow* internalWindow;
};

struct ClientDeviceConstantBuffer
{
	ClientDeviceConstantBuffer(UInt32 sz) : size(sz) {}
	ConstantBufferHandle internalHandle;
	UInt32 size;
};

struct ClientDeviceComputeProgram
{
	ClientDeviceComputeProgram() {}
	ComputeProgramHandle internalHandle;
};


#if UNITY_XENON
struct ClientDeviceRawVBO
{
	ClientDeviceRawVBO() : internalVBO(NULL) {}
	RawVBO* GetInternal() { return const_cast<RawVBO*>(internalVBO); }
	volatile RawVBO* internalVBO;
};

struct ClientDeviceVideoPlayer
{
	ClientDeviceVideoPlayer() : internalVP(NULL), isDead(false) {}
	xenon::IVideoPlayer* GetInternal() { return const_cast<xenon::IVideoPlayer*>(internalVP); }
	volatile xenon::IVideoPlayer* internalVP;
	volatile bool isDead;
};
#endif

#endif
