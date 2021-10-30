#include "UnityPrefix.h"

#if ENABLE_MULTITHREADED_CODE
#if ENABLE_SPRITES
#include "Runtime/Graphics/SpriteFrame.h"
#endif
#include "Runtime/GfxDevice/threaded/GfxDeviceClient.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceWorker.h"
#include "Runtime/GfxDevice/threaded/GfxCommands.h"
#include "Runtime/GfxDevice/threaded/GfxReturnStructs.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/GfxDevice/threaded/ThreadedVBO.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/GfxDevice/threaded/ThreadedWindow.h"
#include "Runtime/GfxDevice/threaded/ThreadedDisplayList.h"
#include "Runtime/GfxDevice/threaded/ThreadedTimerQuery.h"
#include "External/shaderlab/Library/program.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/texenv.h"
#include "External/shaderlab/Library/pass.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/GfxDevice/GPUSkinningInfo.h"
#include "Runtime/Profiler/ProfilerImpl.h"
#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/GfxDevice/GfxXenonVBO.h"
#include "PlatformDependent/Xbox360/Source/Services/VideoPlayer.h"
#endif

#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
#include <ppapi/cpp/instance.h>
#include "External/NPAPI2NaCl/Common/UnityInterfaces.h"
#include "PlatformDependent/PepperPlugin/UnityInstance.h"

void *CreateSharedGfxDeviceAndBuffer(size_t size)
{
	struct UNITY_GfxDevice_1_0 *gfxInterface = (UNITY_GfxDevice_1_0*)pp::Module::Get()->GetBrowserInterface(UNITY_GFXDEVICE_INTERFACE_1_0);
	PP_Resource res = gfxInterface->Create(GetUnityInstance().pp_instance(), size);
	return gfxInterface->GetSharedMemoryBufferAddress(res);
}
#endif

PROFILER_INFORMATION(gGfxWaitForPresentProf, "Gfx.WaitForPresent", kProfilerOverhead)

class ClientGPUSkinningInfo : public GPUSkinningInfo
{
	friend class GfxDeviceClient;
private:
	ClientGPUSkinningInfo(GPUSkinningInfo *realSkinInfo): GPUSkinningInfo(), m_realSkinInfo(realSkinInfo) {}
	virtual ~ClientGPUSkinningInfo() {}

	GPUSkinningInfo *m_realSkinInfo;

public:
	virtual UInt32	GetVertexCount() { return m_realSkinInfo->GetVertexCount(); }
	virtual UInt32	GetChannelMap() { return m_realSkinInfo->GetChannelMap(); }
	virtual int		GetStride() { return m_realSkinInfo->GetStride(); }
	virtual VBO *	GetDestVBO() { return m_DestVBO; }
	virtual UInt32  GetBonesPerVertex() { return m_realSkinInfo->GetBonesPerVertex(); }

	/** Update vertex count */
	virtual void SetVertexCount(UInt32 count) { m_realSkinInfo->SetVertexCount(count); }

	/** Update channel map */
	virtual void SetChannelMap(UInt32 channelmap) { m_realSkinInfo->SetChannelMap(channelmap); }

	/** Update stride of the vertices in bytes (not including skin data). */
	virtual void SetStride(int stride) { m_realSkinInfo->SetStride(stride); }

	/** Update destination VBO */
	virtual void SetDestVBO(VBO *vbo)
	{
		m_DestVBO = vbo;
		// realskininfo->m_destVBO will be updated lazily on the actual skinning call
	}

	virtual void SetBonesPerVertex(UInt32 bones)
	{
		m_realSkinInfo->SetBonesPerVertex(bones);
	}

	/** Threading support: Read from stream. */
	virtual void Read(ThreadedStreamBuffer& stream)
	{
		AssertBreak(false);
	}
	/** Threading support: Write to stream. */
	virtual void Write(ThreadedStreamBuffer& stream)
	{
		AssertBreak(false);
	}

	/** Helper function for thread worker: clear the internal structures without releasing,
	   so the internal resources won't be double-released. */
	virtual void Clear()
	{
		AssertBreak(false);
	}

};




GfxDevice* CreateClientGfxDevice(GfxDeviceRenderer renderer, UInt32 flags, size_t bufferSize, void *buffer)
{
	bool forceRef = (flags & kClientDeviceForceRef) != 0;

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	if (flags & kClientDeviceUseRealDevice)
	{
		return CreateRealGfxDevice (renderer, forceRef);
	}
#endif

	bool threaded = (flags & kClientDeviceThreaded) != 0;
	bool clientProcess = (flags & kClientDeviceClientProcess) != 0;
	bool workerProcess = (flags & kClientDeviceWorkerProcess) != 0;

	printf_console("GfxDevice: creating device client; threaded=%i\n", (int)threaded);

	// Threading mode must be set before creating device (so D3D9 gets correct flags)
	SetGfxThreadingMode (threaded ? kGfxThreadingModeThreaded : kGfxThreadingModeNonThreaded);

	if (bufferSize == 0)
	{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
		// The native client uses a larger default size for the ring buffer, since it cannot
		// flush the buffer at arbitrary times, so it needs to be big enough to hold complete
		// data for texture upload commands.
		bufferSize = 64*1024*1024;
#else
		bufferSize = 8*1024*1024;
#endif
	}
	
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	if (clientProcess)
		buffer = CreateSharedGfxDeviceAndBuffer(bufferSize);
#endif

	GfxDeviceClient* device = UNITY_NEW_AS_ROOT(GfxDeviceClient(threaded, clientProcess, bufferSize, buffer), kMemGfxDevice, "GfxClientDevice", "");
	if (clientProcess)
	{
		device->SetRealGfxDevice (NULL);
		device->QueryGraphicsCaps();
		return device;
	}

	GfxDeviceWorker* worker = device->GetGfxDeviceWorker();
	if(renderer == kGfxRendererOpenGLES30)
		SetGfxDevice(device); // Set this on GLES30 as we do stuff in OnDeviceCreated that requires this. Can't set it for all as it breaks NaCL build in teamcity.
	GfxThreadableDevice* realDevice = worker->Startup(renderer, threaded && !workerProcess, forceRef);

	if (realDevice)
	{
		device->SetRealGfxDevice (realDevice);
		device->AcquireThreadOwnership ();
		realDevice->OnDeviceCreated (false);
		device->ReleaseThreadOwnership ();
		return device;
	}

	// Failed to create threaded device
	SetGfxThreadingMode (kGfxThreadingModeDirect);

	// Client device deletes worker
	UNITY_DELETE(device, kMemGfxDevice);
	return NULL;
}

bool GfxDeviceWorkerProcessRunCommand()
{
	GfxDeviceClient& device = (GfxDeviceClient&)GetGfxDevice();
	GfxDeviceWorker* worker = device.GetGfxDeviceWorker();
	return worker->RunCommandIfDataIsAvailable();
}

void GfxDeviceClient::WaitForSignal ()
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	struct UNITY_GfxDevice_1_0 *gfxInterface = (UNITY_GfxDevice_1_0*)pp::Module::Get()->GetBrowserInterface(UNITY_GFXDEVICE_INTERFACE_1_0);
	return gfxInterface->WaitForSignal(0);
#else
	if (m_DeviceWorker)
		m_DeviceWorker->WaitForSignal();
#endif
}

void GfxDeviceClient::QueryGraphicsCaps()
{
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_QueryGraphicsCaps);

	ReadbackData(m_ReadbackData);

	// no, really, we need to properly serialize this stuff with all the strings. this is just a hack to get running.
	size_t offset = (char*)&gGraphicsCaps.vendorID - (char*)&gGraphicsCaps;
	memcpy (&gGraphicsCaps.vendorID, m_ReadbackData.data(), std::min(m_ReadbackData.size(), sizeof(GraphicsCaps) - offset));
}


GfxDeviceClient::GfxDeviceClient(bool threaded, bool clientProcess, size_t bufferSize, void *buffer) :
	m_RealDevice(NULL),
	m_Threaded(threaded),
	m_Serialize(threaded),
	m_RecordDepth(0),
	m_MaxCallDepth(1),
	m_DynamicVBO(NULL),
	m_sRGBWrite(false)
{
	// Create stack of display lists (including "top" level)
	m_DisplayListStack = new DisplayListContext[m_MaxCallDepth + 1];
	if (m_Threaded)
	{
		if (buffer)
			m_DisplayListStack[0].commandQueue.CreateFromMemory(ThreadedStreamBuffer::kModeCrossProcess, bufferSize, buffer);
		else
			m_DisplayListStack[0].commandQueue.Create(ThreadedStreamBuffer::kModeThreaded, bufferSize);
		m_DynamicVBO = new ThreadedDynamicVBO(*this);
	}
	for (int i = 1; i <= m_MaxCallDepth; i++)
		m_DisplayListStack[i].commandQueue.Create(ThreadedStreamBuffer::kModeGrowable, 0);
	m_CurrentContext = &m_DisplayListStack[0];
	m_CommandQueue = &m_CurrentContext->commandQueue;
	m_InvertProjectionMatrix = false;
	#if GFX_USES_VIEWPORT_OFFSET
	m_ViewportOffset.Set(0.0f, 0.0f);
	#endif
	m_TransformState.Invalidate(m_BuiltinParamValues);
	m_ScissorEnabled = -1;
	m_PresentPending = false;
	m_Wireframe = false;
	m_CurrentTargetWidth = 0;
	m_CurrentTargetHeight = 0;
	m_CurrentWindowWidth = 0;
	m_CurrentWindowHeight = 0;
	m_ThreadOwnershipCount = 0;
	m_CurrentCPUFence = 0;
	m_PresentFrameID = 0;
	if (clientProcess)
		m_DeviceWorker = NULL;
	else
		m_DeviceWorker = new GfxDeviceWorker(4, m_CommandQueue);

	OnCreate();

	{
		ClientDeviceRenderSurface* color = new ClientDeviceRenderSurface(0, 0);
		RenderSurfaceBase_InitColor(*color);
		color->backBuffer = true;
		color->internalHandle = m_RealDevice ? m_RealDevice->GetBackBufferColorSurface() : RenderSurfaceHandle();
		SetBackBufferColorSurface(color);
	}
	{
		ClientDeviceRenderSurface* depth = new ClientDeviceRenderSurface(0, 0);
		RenderSurfaceBase_InitDepth(*depth);
		depth->backBuffer = true;
		depth->internalHandle = m_RealDevice ? m_RealDevice->GetBackBufferDepthSurface() : RenderSurfaceHandle();
		SetBackBufferDepthSurface(depth);
	}

#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	posForTexGen = 0;
	nrmForTexGen = 0;
	CompileTimeAssert(sizeof(posForTexGen) * 8 == kMaxSupportedTextureUnitsGLES, "posForTexGen should have enough bits for tex units");
	CompileTimeAssert(sizeof(nrmForTexGen) * 8 == kMaxSupportedTextureUnitsGLES, "nrmForTexGen should have enough bits for tex units");
#endif
}

GfxDeviceClient::~GfxDeviceClient()
{
	CheckMainThread();
	if (m_Threaded && m_RealDevice)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_Quit);
		SubmitCommands();
		WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	// m_CommandQueue was not allocated
	m_CommandQueue = NULL;
	delete[] m_DisplayListStack;
	if (m_Threaded)
		delete m_DynamicVBO;
	delete m_DeviceWorker;
}

void GfxDeviceClient::InvalidateState()
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_TransformState.Invalidate(m_BuiltinParamValues);
	m_FogParams.Invalidate();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_InvalidateState);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->InvalidateState();
}

#if GFX_DEVICE_VERIFY_ENABLE
void GfxDeviceClient::VerifyState()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_VerifyState);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->VerifyState();
}
#endif


void GfxDeviceClient::SetMaxBufferedFrames (int bufferSize)
{
	CheckMainThread();
	GfxDevice::SetMaxBufferedFrames(bufferSize);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetMaxBufferedFrames);
		m_CommandQueue->WriteValueType<int>(bufferSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetMaxBufferedFrames(bufferSize);
}


void GfxDeviceClient::Clear( UInt32 clearFlags, const float color[4], float depth, int stencil )
{
	CheckMainThread();

	// mark cleared surfaces as "no contents" if we're tracking that
	if (GetFrameStats().m_StatsEnabled)
	{
		if (clearFlags & kGfxClearColor)
		{
			for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
			{
				if (!m_ActiveRenderColorSurfaces[i].IsValid ())
					continue;
				ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)m_ActiveRenderColorSurfaces[i].object;
				colorSurf->state = ClientDeviceRenderSurface::kCleared;
			}
		}
		if ((clearFlags & kGfxClearDepthStencil) && m_ActiveRenderDepthSurface.IsValid ())
		{
			ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)m_ActiveRenderDepthSurface.object;
			if (depthSurf)
				depthSurf->state = ClientDeviceRenderSurface::kCleared;
		}
	}

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_Clear);
		GfxCmdClear clear = { clearFlags, Vector4f(color), depth, stencil };
		m_CommandQueue->WriteValueType<GfxCmdClear>(clear);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->Clear(clearFlags, color, depth, stencil);
}

void GfxDeviceClient::SetUserBackfaceMode( bool enable )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetUserBackfaceMode);
		m_CommandQueue->WriteValueType<GfxCmdBool>(enable);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetUserBackfaceMode(enable);
}

void GfxDeviceClient::SetWireframe(bool wire)
{
	CheckMainThread();
	m_Wireframe = wire;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetWireframe);
		m_CommandQueue->WriteValueType<GfxCmdBool>(wire);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetWireframe(wire);
}

bool GfxDeviceClient::GetWireframe() const
{
	return m_Wireframe;
}


void GfxDeviceClient::SetInvertProjectionMatrix( bool enable )
{
	CheckMainThread();
	m_InvertProjectionMatrix = enable;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetInvertProjectionMatrix);
		m_CommandQueue->WriteValueType<GfxCmdBool>(enable);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetInvertProjectionMatrix(enable);
}

bool GfxDeviceClient::GetInvertProjectionMatrix() const
{
	CheckMainThread();
	return m_InvertProjectionMatrix;
}

#if GFX_USES_VIEWPORT_OFFSET
void GfxDeviceClient::SetViewportOffset( float x, float y )
{
	CheckMainThread();
	m_ViewportOffset = Vector2f(x, y);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetViewportOffset);
		m_CommandQueue->WriteValueType<Vector2f>(m_ViewportOffset);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetViewportOffset(x, y);
}

void GfxDeviceClient::GetViewportOffset( float &x, float &y ) const
{
	x = m_ViewportOffset.x;
	y = m_ViewportOffset.y;
}
#endif

DeviceBlendState* GfxDeviceClient::CreateBlendState(const GfxBlendState& state)
{
	CheckMainThread();
	SET_ALLOC_OWNER(this);
	DebugAssert(!IsRecording());
	std::pair<CachedBlendStates::iterator, bool> added = m_CachedBlendStates.insert(std::make_pair(state, ClientDeviceBlendState(state)));
	ClientDeviceBlendState* result = &added.first->second;
	if (!added.second)
		return result;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateBlendState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		result->internalState = m_BlendStateMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceBlendState>(*result);
#else
		m_CommandQueue->WriteValueType<ClientDeviceBlendState*>(result);
#endif
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		result->internalState = m_RealDevice->CreateBlendState(state);
#endif

	return result;
}

DeviceDepthState* GfxDeviceClient::CreateDepthState(const GfxDepthState& state)
{
	CheckMainThread();
	SET_ALLOC_OWNER(this);
	DebugAssert(!IsRecording());
	std::pair<CachedDepthStates::iterator, bool> added = m_CachedDepthStates.insert(std::make_pair(state, ClientDeviceDepthState(state)));
	ClientDeviceDepthState* result = &added.first->second;
	if (!added.second)
		return result;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateDepthState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		result->internalState = m_DepthStateMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceDepthState>(*result);
#else
		m_CommandQueue->WriteValueType<ClientDeviceDepthState*>(result);
#endif
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		result->internalState = m_RealDevice->CreateDepthState(state);
#endif
	return result;
}

DeviceStencilState* GfxDeviceClient::CreateStencilState(const GfxStencilState& state)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	SET_ALLOC_OWNER(this);
	std::pair<CachedStencilStates::iterator, bool> added = m_CachedStencilStates.insert(std::make_pair(state, ClientDeviceStencilState(state)));
	ClientDeviceStencilState* result = &added.first->second;
	if (!added.second)
		return result;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateStencilState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		result->internalState = m_StencilStateMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceStencilState>(*result);
#else
		m_CommandQueue->WriteValueType<ClientDeviceStencilState*>(result);
#endif
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		result->internalState = m_RealDevice->CreateStencilState(state);
#endif
	return result;
}

DeviceRasterState* GfxDeviceClient::CreateRasterState(const GfxRasterState& state)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	SET_ALLOC_OWNER(this);
	std::pair<CachedRasterStates::iterator, bool> added = m_CachedRasterStates.insert(std::make_pair(state, ClientDeviceRasterState(state)));
	ClientDeviceRasterState* result = &added.first->second;
	if (!added.second)
		return result;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateRasterState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		result->internalState = m_RasterStateMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceRasterState>(*result);
#else
		m_CommandQueue->WriteValueType<ClientDeviceRasterState*>(result);
#endif
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		result->internalState = m_RealDevice->CreateRasterState(state);
#endif

	return result;
}

void GfxDeviceClient::RecordSetBlendState(const DeviceBlendState* state, const ShaderLab::FloatVal& alphaRef, const ShaderLab::PropertySheet* props)
{
	CheckMainThread();
	DebugAssert(IsRecording());
	const ClientDeviceBlendState* clientState = static_cast<const ClientDeviceBlendState*>(state);
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetBlendState);
	m_CommandQueue->WriteValueType<const ClientDeviceBlendState*>(clientState);
	float* dest = m_CommandQueue->GetWritePointer<float>();
	// Must call GetBuffer() after GetWritePointer() since it might be reallocated!
	const void* bufferStart = m_CommandQueue->GetBuffer();
	m_CurrentContext->patchInfo.AddPatchableFloat(alphaRef, *dest, bufferStart, props);
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void GfxDeviceClient::SetBlendState(const DeviceBlendState* state, float alphaRef)
{
	CheckMainThread();
	const ClientDeviceBlendState* clientState = static_cast<const ClientDeviceBlendState*>(state);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetBlendState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<const ClientDeviceBlendState>(*clientState);
#else
		m_CommandQueue->WriteValueType<const ClientDeviceBlendState*>(clientState);
#endif
		m_CommandQueue->WriteValueType<float>(alphaRef);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->SetBlendState(clientState->internalState, alphaRef);
#endif
}

void GfxDeviceClient::SetDepthState(const DeviceDepthState* state)
{
	CheckMainThread();
	const ClientDeviceDepthState* clientState = static_cast<const ClientDeviceDepthState*>(state);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetDepthState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<const ClientDeviceDepthState>(*clientState);
#else
		m_CommandQueue->WriteValueType<const ClientDeviceDepthState*>(clientState);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->SetDepthState(clientState->internalState);
#endif
}

void GfxDeviceClient::SetStencilState(const DeviceStencilState* state, int stencilRef)
{
	CheckMainThread();
	const ClientDeviceStencilState* clientState = static_cast<const ClientDeviceStencilState*>(state);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetStencilState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<const ClientDeviceStencilState>(*clientState);
#else
		m_CommandQueue->WriteValueType<const ClientDeviceStencilState*>(clientState);
#endif
		m_CommandQueue->WriteValueType<int>(stencilRef);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->SetStencilState(clientState->internalState, stencilRef);
#endif
}

#if UNITY_XENON
void GfxDeviceClient::SetNullPixelShader()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetNullPixelShader);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetNullPixelShader();
}

void GfxDeviceClient::SetHiZEnable( const HiZstate hiz )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EnableHiZ);
		m_CommandQueue->WriteValueType<HiZstate>( hiz );
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetHiZEnable( hiz );
}

void GfxDeviceClient::SetHiStencilState( const bool hiStencilEnable, const bool hiStencilWriteEnable, const int hiStencilRef, const CompareFunction cmpFunc )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetHiStencilState);
		m_CommandQueue->WriteValueType<bool>(hiStencilEnable);
		m_CommandQueue->WriteValueType<bool>(hiStencilWriteEnable);
		m_CommandQueue->WriteValueType<int>(hiStencilRef);
		m_CommandQueue->WriteValueType<CompareFunction>(cmpFunc);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetHiStencilState( hiStencilEnable, hiStencilWriteEnable, hiStencilRef, cmpFunc );
}

void GfxDeviceClient::HiStencilFlush( const HiSflush flushtype )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_HiStencilFlush);
		m_CommandQueue->WriteValueType<HiSflush>(flushtype);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->HiStencilFlush(flushtype);
}
#endif

void GfxDeviceClient::SetRasterState(const DeviceRasterState* state)
{
	CheckMainThread();
	const ClientDeviceRasterState* clientState = static_cast<const ClientDeviceRasterState*>(state);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetRasterState);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<const ClientDeviceRasterState>(*clientState);
#else
		m_CommandQueue->WriteValueType<const ClientDeviceRasterState*>(clientState);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->SetRasterState(clientState->internalState);
#endif
}

void GfxDeviceClient::SetSRGBWrite (bool enable)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_sRGBWrite = enable;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetSRGBState);
		m_CommandQueue->WriteValueType<GfxCmdBool>(enable);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetSRGBWrite(enable);
}

bool GfxDeviceClient::GetSRGBWrite ()
{
	CheckMainThread();
	return m_sRGBWrite;
}

void GfxDeviceClient::SetWorldMatrix( const float matrix[16] )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_TransformState.dirtyFlags |= TransformState::kWorldDirty;
	memcpy(m_TransformState.worldMatrix.GetPtr(), matrix, 16 * sizeof(float));
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetWorldMatrix);
		m_CommandQueue->WriteValueType<Matrix4x4f>(m_TransformState.worldMatrix);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetWorldMatrix(matrix);
}

void GfxDeviceClient::SetViewMatrix( const float matrix[16] )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_TransformState.SetViewMatrix (matrix, m_BuiltinParamValues);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetViewMatrix);
		m_CommandQueue->WriteValueType<Matrix4x4f>(m_BuiltinParamValues.GetMatrixParam(kShaderMatView));
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetViewMatrix(matrix);
}

void GfxDeviceClient::SetProjectionMatrix (const Matrix4x4f& matrix)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_TransformState.dirtyFlags |= TransformState::kProjDirty;
	Matrix4x4f& m = m_BuiltinParamValues.GetWritableMatrixParam(kShaderMatProj);
	CopyMatrix (matrix.GetPtr(), m.GetPtr());
	CopyMatrix (matrix.GetPtr(), m_TransformState.projectionMatrixOriginal.GetPtr());
	CalculateDeviceProjectionMatrix (m, m_UsesOpenGLTextureCoords, m_InvertProjectionMatrix);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetProjectionMatrix);
		m_CommandQueue->WriteValueType<Matrix4x4f>(matrix);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetProjectionMatrix(matrix);
}

void GfxDeviceClient::GetMatrix( float outMatrix[16] ) const
{
	m_TransformState.UpdateWorldViewMatrix(m_BuiltinParamValues);
	memcpy(outMatrix, m_TransformState.worldViewMatrix.GetPtr(), 16 * sizeof(float));
}

const float* GfxDeviceClient::GetWorldMatrix() const
{
	return m_TransformState.worldMatrix.GetPtr();
}

const float* GfxDeviceClient::GetViewMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatView).GetPtr();
}

const float* GfxDeviceClient::GetProjectionMatrix() const
{
	return m_TransformState.projectionMatrixOriginal.GetPtr();
}

const float* GfxDeviceClient::GetDeviceProjectionMatrix() const
{
	return m_BuiltinParamValues.GetMatrixParam(kShaderMatProj).GetPtr();
}

void GfxDeviceClient::SetInverseScale( float invScale )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	GfxDevice::SetInverseScale(invScale);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetInverseScale);
		m_CommandQueue->WriteValueType<float>(invScale);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetInverseScale(invScale);
}

void GfxDeviceClient::SetNormalizationBackface( NormalizationMode mode, bool backface )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetNormalizationBackface);
		GfxCmdSetNormalizationBackface data = { mode, backface};
		m_CommandQueue->WriteValueType<GfxCmdSetNormalizationBackface>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetNormalizationBackface(mode, backface);
}

void GfxDeviceClient::SetFFLighting( bool on, bool separateSpecular, ColorMaterialMode colorMaterial )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetFFLighting);
		GfxCmdSetFFLighting data = { on, separateSpecular, colorMaterial };
		m_CommandQueue->WriteValueType<GfxCmdSetFFLighting>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetFFLighting(on, separateSpecular, colorMaterial);
}

void GfxDeviceClient::RecordSetMaterial( const ShaderLab::VectorVal& ambient, const ShaderLab::VectorVal& diffuse, const ShaderLab::VectorVal& specular, const ShaderLab::VectorVal& emissive, const ShaderLab::FloatVal& shininess, const ShaderLab::PropertySheet* props )
{
	CheckMainThread();
	DebugAssert(IsRecording());
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetMaterial);
	GfxMaterialParams* dest = m_CommandQueue->GetWritePointer<GfxMaterialParams>();
	// Must call GetBuffer() after GetWritePointer() since it might be reallocated!
	const void* bufferStart = m_CommandQueue->GetBuffer();
	GfxPatchInfo& patchInfo = m_CurrentContext->patchInfo;
	patchInfo.AddPatchableVector(ambient, dest->ambient, bufferStart, props);
	patchInfo.AddPatchableVector(diffuse, dest->diffuse, bufferStart, props);
	patchInfo.AddPatchableVector(specular, dest->specular, bufferStart, props);
	patchInfo.AddPatchableVector(emissive, dest->emissive, bufferStart, props);
	patchInfo.AddPatchableFloat(shininess, dest->shininess, bufferStart, props);
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void GfxDeviceClient::SetMaterial( const float ambient[4], const float diffuse[4], const float specular[4], const float emissive[4], const float shininess )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetMaterial);
		GfxMaterialParams mat = { Vector4f(ambient), Vector4f(diffuse), Vector4f(specular), Vector4f(emissive), shininess };
		m_CommandQueue->WriteValueType<GfxMaterialParams>(mat);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetMaterial(ambient, diffuse, specular, emissive, shininess);
}

void GfxDeviceClient::RecordSetColor( const ShaderLab::VectorVal& color, const ShaderLab::PropertySheet* props )
{
	CheckMainThread();
	DebugAssert(IsRecording());
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetColor);
	Vector4f* dest = m_CommandQueue->GetWritePointer<Vector4f>();
	// Must call GetBuffer() after GetWritePointer() since it might be reallocated!
	const void* bufferStart = m_CommandQueue->GetBuffer();
	m_CurrentContext->patchInfo.AddPatchableVector(color, *dest, bufferStart, props);
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void GfxDeviceClient::SetColor( const float color[4] )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetColor);
		m_CommandQueue->WriteValueType<Vector4f>(Vector4f(color));
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetColor(color);
}

void GfxDeviceClient::SetViewport( int x, int y, int width, int height )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_Viewport.x = x;
	m_Viewport.y = y;
	m_Viewport.width = width;
	m_Viewport.height = height;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetViewport);
		m_CommandQueue->WriteValueType<ClientDeviceRect>(m_Viewport);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetViewport(x, y, width, height);
}

void GfxDeviceClient::GetViewport( int* values ) const
{
	values[0] = m_Viewport.x;
	values[1] = m_Viewport.y;
	values[2] = m_Viewport.width;
	values[3] = m_Viewport.height;
}

void GfxDeviceClient::SetScissorRect( int x, int y, int width, int height )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_ScissorRect.x = x;
	m_ScissorRect.y = y;
	m_ScissorRect.width = width;
	m_ScissorRect.height = height;
	m_ScissorEnabled = 1;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetScissorRect);
		m_CommandQueue->WriteValueType<ClientDeviceRect>(m_ScissorRect);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetScissorRect(x, y, width, height);
}

void GfxDeviceClient::DisableScissor()
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	m_ScissorEnabled = 0;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DisableScissor);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DisableScissor();
}

bool GfxDeviceClient::IsScissorEnabled() const
{
	return m_ScissorEnabled == 1;
}

void GfxDeviceClient::GetScissorRect( int values[4] ) const
{
	values[0] = m_ScissorRect.x;
	values[1] = m_ScissorRect.y;
	values[2] = m_ScissorRect.width;
	values[3] = m_ScissorRect.height;
}

TextureCombinersHandle GfxDeviceClient::CreateTextureCombiners( int count, const ShaderLab::TextureBinding* texEnvs, const ShaderLab::PropertySheet* props, bool hasVertexColorOrLighting, bool usesAddSpecular )
{
	SET_ALLOC_OWNER(NULL); // Not set to this since these are leaked. Some shaders are created staticly and are not cleaned up.
	CheckMainThread();

	if (count > gGraphicsCaps.maxTexUnits)
		return TextureCombinersHandle( NULL );

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	for (int i = 0; i < count; i++)
		if (!m_RealDevice->IsCombineModeSupported( texEnvs[i].m_CombColor ))
			return TextureCombinersHandle( NULL );

	// check texgen modes & texture dimension are supported
	for (int i = 0; i < count; i++)
	{
		TextureDimension texDim;
		TexGenMode texGen;
		ShaderLab::shaderprops::GetTexEnvInfo( props, texEnvs[i].m_TextureName, texDim, texGen );
		if (!ShaderLab::IsTexEnvSupported( texEnvs[i].m_TextureName, texDim, texGen ))
			return TextureCombinersHandle( NULL );
	}
#endif

	ClientDeviceTextureCombiners* combiners = UNITY_NEW(ClientDeviceTextureCombiners, kMemGfxThread);
	combiners->bindings = (ShaderLab::TextureBinding*)UNITY_MALLOC(kMemGfxThread,sizeof(ShaderLab::TextureBinding)*count);
	for(int i = 0; i < count; i++) new ((void*)(&combiners->bindings[i])) ShaderLab::TextureBinding;
	combiners->count = count;
	for (int i = 0; i < count; i++)
		combiners->bindings[i] = texEnvs[i];

	if (m_Serialize)
	{
		// Don't want to create texture combiners from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateTextureCombiners);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		combiners->internalHandle = m_TextureCombinerMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientIDMapper::ClientID>(combiners->internalHandle);
		m_CommandQueue->WriteValueType<int>(combiners->count);
		for (int i = 0; i < count; i++)
			m_CommandQueue->WriteValueType<ShaderLab::TextureBinding>(combiners->bindings[i]);
#else
		m_CommandQueue->WriteValueType<ClientDeviceTextureCombiners*>(combiners);
#endif
		GfxCmdCreateTextureCombiners texcomb = { count, hasVertexColorOrLighting, usesAddSpecular };
		m_CommandQueue->WriteValueType<GfxCmdCreateTextureCombiners>(texcomb);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		combiners->internalHandle = m_RealDevice->CreateTextureCombiners(count, texEnvs, props, hasVertexColorOrLighting, usesAddSpecular);
#endif

	return TextureCombinersHandle( combiners );
}
void GfxDeviceClient::DeleteTextureCombiners( TextureCombinersHandle& textureCombiners )
{
	CheckMainThread();
	if (!textureCombiners.IsValid())
		return;

	ClientDeviceTextureCombiners* combiners = static_cast<ClientDeviceTextureCombiners*>(textureCombiners.object);

	if (m_Serialize)
	{
		// Must delete when message received
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DeleteTextureCombiners);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientIDMapper::ClientID>(combiners->internalHandle);
		m_CommandQueue->WriteValueType<int>(combiners->count);
		delete[] combiners->bindings;
		delete combiners;
#else
		m_CommandQueue->WriteValueType<ClientDeviceTextureCombiners*>(combiners);
		textureCombiners.Reset();
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{

		for(int i = 0; i < combiners->count; i++) (&combiners->bindings[i])->~TextureBinding();
		UNITY_FREE(kMemGfxThread, combiners->bindings);
		UNITY_DELETE(combiners,kMemGfxThread);
	}
}

void GfxDeviceClient::SetTextureCombiners( TextureCombinersHandle textureCombiners, const ShaderLab::PropertySheet* props )
{
	CheckMainThread();
	if (!textureCombiners.IsValid())
		return;

	ClientDeviceTextureCombiners* combiners = static_cast<ClientDeviceTextureCombiners*>(textureCombiners.object);

#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	for(int i = 0, n = combiners->count ; i < n ; ++i)
		SetTexGen(i, GetTexEnvForBinding(combiners->bindings[i], props)->GetTexGen());
	for(int i = combiners->count ; i < kMaxSupportedTextureUnitsGLES ; ++i)
		DropTexGen(i);
#endif

	if (m_Serialize)
	{
		int count = combiners->count;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTextureCombiners);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientIDMapper::ClientID>(combiners->internalHandle);
		m_CommandQueue->WriteValueType<int>(combiners->count);
#else
		m_CommandQueue->WriteValueType<ClientDeviceTextureCombiners*>(combiners);
#endif
		void* data = m_CommandQueue->GetWriteDataPointer(count * sizeof(TexEnvData), ALIGN_OF(TexEnvData));
		// Must call GetBuffer() after GetWriteDataPointer() since it might be reallocated!
		const UInt8* bufferStart = IsRecording() ? static_cast<const UInt8*>(m_CommandQueue->GetBuffer()) : NULL;
		TexEnvData* dest = static_cast<TexEnvData*>(data);
		for (int i = 0; i < count; i++)
		{
			using namespace ShaderLab::shaderprops;
			ShaderLab::TextureBinding& binding = combiners->bindings[i];
			if (IsRecording())
			{
				if (!m_CurrentContext->patchInfo.AddPatchableTexEnv(binding.m_TextureName, binding.m_MatrixName,
						kTexDimAny, &dest[i], bufferStart, props))
					m_CurrentContext->recordFailed = true;
			}
			else
			{
				ShaderLab::TexEnv* te = GetTexEnvForBinding(binding, props);
				Assert(te != NULL);
				te->PrepareData(binding.m_TextureName.index, binding.m_MatrixName, props, &dest[i]);
			}
		}
		data = m_CommandQueue->GetWriteDataPointer(count * sizeof(Vector4f), ALIGN_OF(Vector4f));
		Vector4f* texColors = static_cast<Vector4f*>(data);
		for (int i = 0; i < count; i++)
		{
			const ShaderLab::TextureBinding& binding = combiners->bindings[i];
			texColors[i] = binding.GetTexColor().Get (props);
		}
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->SetTextureCombiners(combiners->internalHandle, props);
#endif
}

void GfxDeviceClient::SetTexture (ShaderType shaderType, int unit, int samplerUnit, TextureID texture, TextureDimension dim, float bias)
{
	CheckMainThread();
	DebugAssert( dim >= kTexDim2D && dim <= kTexDimCUBE );
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTexture);
		GfxCmdSetTexture tex = {shaderType, unit, samplerUnit, texture, dim, bias};
		m_CommandQueue->WriteValueType<GfxCmdSetTexture>(tex);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetTexture (shaderType, unit, samplerUnit, texture, dim, bias);
}

void GfxDeviceClient::SetTextureParams( TextureID texture, TextureDimension texDim, TextureFilterMode filter, TextureWrapMode wrap, int anisoLevel, bool hasMipMap, TextureColorSpace colorSpace )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTextureParams);
		GfxCmdSetTextureParams params = { texture, texDim, filter, wrap, anisoLevel, hasMipMap, colorSpace };
		m_CommandQueue->WriteValueType<GfxCmdSetTextureParams>(params);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetTextureParams(texture, texDim, filter, wrap, anisoLevel, hasMipMap, colorSpace);
}

void GfxDeviceClient::SetTextureTransform( int unit, TextureDimension dim, TexGenMode texGen, bool identity, const float matrix[16])
{
	CheckMainThread();

#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	SetTexGen(unit, texGen);
#endif

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTextureTransform);
		GfxCmdSetTextureTransform trans = { unit, dim, texGen, identity, Matrix4x4f(matrix) };
		m_CommandQueue->WriteValueType<GfxCmdSetTextureTransform>(trans);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetTextureTransform(unit, dim, texGen, identity, matrix);
}

void GfxDeviceClient::SetTextureName( TextureID texture, char const* name )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTextureName);
		GfxCmdSetTextureName texName = { texture, strlen(name)+1 };
		m_CommandQueue->WriteValueType<GfxCmdSetTextureName>(texName);
		m_CommandQueue->WriteArrayType<char>(name, strlen(name)+1);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetTextureName( texture, name );
}

void GfxDeviceClient::SetMaterialProperties( const MaterialPropertyBlock& block )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetMaterialProperties);
		int propertyCount = block.GetPropertiesEnd() - block.GetPropertiesBegin();
		int bufferSize = block.GetBufferEnd() - block.GetBufferBegin();
		typedef MaterialPropertyBlock::Property Property;
		GfxCmdSetMaterialProperties matprops = { propertyCount, bufferSize };
		m_CommandQueue->WriteValueType<GfxCmdSetMaterialProperties>(matprops);
		m_CommandQueue->WriteArrayType<Property>(block.GetPropertiesBegin(), propertyCount);
		m_CommandQueue->WriteArrayType<float>(block.GetBufferBegin(), bufferSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetMaterialProperties(block);
}

GpuProgram* GfxDeviceClient::CreateGpuProgram( const std::string& source, CreateGpuProgramOutput& output )
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateGpuProgram);
		GpuProgram* result = NULL;
		//GfxCmdCreateGpuProgram gpuprog = { parent, source.c_str(), outErrors, &result };
		//m_CommandQueue->WriteValueType<GfxCmdCreateGpuProgram>(gpuprog);
		//SubmitCommands();
		//WaitForSignal();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdCreateGpuProgram gpuprog = { source.c_str(), &output, &result };
		m_CommandQueue->WriteValueType<GfxCmdCreateGpuProgram>(gpuprog);
		SubmitCommands();
		m_DeviceWorker->WaitForSignal();
#else
		size_t len = source.length();
		m_CommandQueue->WriteValueType<UInt32>(len);
		m_CommandQueue->WriteArrayType<char>(source.c_str(), len + 1);
		SubmitCommands();
		ReadbackData(m_ReadbackData);
		const GfxRet_CreateGpuProgram* returnData = reinterpret_cast<GfxRet_CreateGpuProgram*>(m_ReadbackData.data());
		returnData->GetOutput(output);
		result = (GpuProgram*)returnData->gpuProgram;
#endif
		UnityMemoryBarrier();
		GFXDEVICE_LOCKSTEP_CLIENT();
		return result;
	}
	else
		return m_RealDevice->CreateGpuProgram(source, output);
}

void GfxDeviceClient::SetShadersMainThread( ShaderLab::SubProgram* programs[kShaderTypeCount], const ShaderLab::PropertySheet* props )
{
	CheckMainThread();

	FogMode fogMode = m_FogParams.mode;
	DisplayListContext& context = *m_CurrentContext;
	for (int pt = 0; pt < kShaderTypeCount; ++pt)
		context.shadersActive[pt] = false;

	// Fill in arrays of GPU programs, parameters and buffer sizes
	GfxCmdSetShaders shadersCmd;
	for (int pt = 0; pt < kShaderTypeCount; ++pt)
	{
		if (!programs[pt])
		{
			shadersCmd.programs[pt] = NULL;
			shadersCmd.params[pt] = NULL;
			shadersCmd.paramsBufferSize[pt] = 0;
			continue;
		}
		GpuProgram& gpuProg = programs[pt]->GetGpuProgram();
		GpuProgramParameters& params = programs[pt]->GetParams(fogMode);
		shadersCmd.programs[pt] = (ClientIDWrapper(GpuProgram))&gpuProg;
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
		shadersCmd.params[pt] = &params;
#else
		if (!params.m_InternalHandle)
		{
			m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetGpuProgramParameters);
			params.m_InternalHandle = m_GpuProgramParametersMapper.CreateID();
			m_CommandQueue->WriteValueType<ClientIDMapper::ClientID>(params.m_InternalHandle);
			dynamic_array<UInt8> outBuf;
			dynamic_array<char> strBuf;
			Gfx_GpuProgramParameters gfxParams (params, outBuf, strBuf);
			m_CommandQueue->WriteValueType<Gfx_GpuProgramParameters>(gfxParams);

			int outSize = outBuf.size();
			m_CommandQueue->WriteValueType<UInt32>(outSize);
			m_CommandQueue->WriteValueType<UInt32>(strBuf.size());
			outBuf.resize_uninitialized(outSize + strBuf.size());
			std::copy(strBuf.begin(), strBuf.end(), outBuf.begin()+outSize);
			void* buffer = m_CommandQueue->GetWriteDataPointer(outBuf.size(), 1);
			memcpy (buffer, outBuf.begin(), outBuf.size());
		}
		shadersCmd.params[pt] = params.m_InternalHandle;
#endif
		if (!params.IsReady())
		{
			CreateShaderParameters (programs[pt], fogMode);
			params.MakeReady();
		}
		shadersCmd.paramsBufferSize[pt] = params.GetValuesSize();
		ShaderImplType implType = programs[pt]->GetGpuProgram().GetImplType();
		context.shadersActive[pt] = implType == pt;

		// GLSL case, where a single vertex shader SubProgram can encompass multiple stages
		if (implType == kShaderImplBoth)
		{
			context.shadersActive[kShaderVertex] |= true;
			context.shadersActive[kShaderFragment] |= true;
		}
	}

	context.hasSetShaders = true;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetShaders);
		m_CommandQueue->WriteValueType<GfxCmdSetShaders>(shadersCmd);
		for (int pt = 0; pt < kShaderTypeCount; ++pt)
		{
			int bufferSize = shadersCmd.paramsBufferSize[pt];
			if (bufferSize <= 0)
				continue;

			void* buffer = m_CommandQueue->GetWriteDataPointer(bufferSize, 1);
			UInt8* dest = static_cast<UInt8*>(buffer);

			// Must call GetBuffer() after GetWriteDataPointer() since it might be reallocated!
			const UInt8* bufferStart = IsRecording() ? static_cast<const UInt8*>(m_CommandQueue->GetBuffer()) : NULL;
			GfxPatchInfo* patchInfo = IsRecording() ? &context.patchInfo : NULL;
			programs[pt]->GetParams(fogMode).PrepareValues(props, dest, bufferStart, patchInfo, &context.recordFailed);
		}
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		GraphicsHelper::SetShaders(*m_RealDevice, programs, props);
}

void GfxDeviceClient::CreateShaderParameters( ShaderLab::SubProgram* program, FogMode fogMode )
{
	CheckMainThread();
	if (m_Threaded)
	{
		// Get main command queue even when recording
		ThreadedStreamBuffer& stream = m_DisplayListStack[0].commandQueue;
		stream.WriteValueType<GfxCommand>(kGfxCmd_CreateShaderParameters);
		GfxCmdCreateShaderParameters params = { program, fogMode };
		stream.WriteValueType<GfxCmdCreateShaderParameters>(params);
		stream.WriteSubmitData();
		WaitForSignal();
#if DEBUG_GFXDEVICE_LOCKSTEP
		// Lockstep even if recording
		GetGfxDeviceWorker()->LockstepWait();
#endif
	}
	else
	{
		m_RealDevice->CreateShaderParameters(program, fogMode);
	}
}

bool GfxDeviceClient::IsShaderActive( ShaderType type ) const
{
	return m_CurrentContext->shadersActive[type];
}

void GfxDeviceClient::DestroySubProgram( ShaderLab::SubProgram* subprogram )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DestroySubProgram);
		m_CommandQueue->WriteValueType<ShaderLab::SubProgram*>(subprogram);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DestroySubProgram(subprogram);
}

void GfxDeviceClient::SetConstantBufferInfo (int id, int size)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetConstantBufferInfo);
		m_CommandQueue->WriteValueType<int>(id);
		m_CommandQueue->WriteValueType<int>(size);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetConstantBufferInfo (id, size);
}

void GfxDeviceClient::DisableLights( int startLight )
{
	CheckMainThread();
	const Vector4f black(0.0F, 0.0F, 0.0F, 0.0F);
	for (int i = startLight; i < kMaxSupportedVertexLights; ++i)
	{
		m_BuiltinParamValues.SetVectorParam(BuiltinShaderVectorParam(kShaderVecLight0Diffuse + i), black);
	}

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DisableLights);
		m_CommandQueue->WriteValueType<int>(startLight);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DisableLights(startLight);
}

void GfxDeviceClient::SetLight( int light, const GfxVertexLight& data)
{
	CheckMainThread();
	DebugAssert(!IsRecording());

	if (light >= kMaxSupportedVertexLights)
		return;

	SetupVertexLightParams (light, data);

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetLight);
		m_CommandQueue->WriteValueType<int>(light);
		m_CommandQueue->WriteValueType<GfxVertexLight>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetLight(light, data);
}

void GfxDeviceClient::SetAmbient( const float ambient[4] )
{
	CheckMainThread();
	m_BuiltinParamValues.SetVectorParam(kShaderVecLightModelAmbient, Vector4f(ambient));
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetAmbient);
		m_CommandQueue->WriteValueType<Vector4f>(Vector4f(ambient));
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetAmbient(ambient);
}

void GfxDeviceClient::RecordEnableFog( FogMode fogMode, const ShaderLab::FloatVal& fogStart, const ShaderLab::FloatVal& fogEnd, const ShaderLab::FloatVal& fogDensity, const ShaderLab::VectorVal& fogColor, const ShaderLab::PropertySheet* props )
{
	CheckMainThread();
	DebugAssert(IsRecording());
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EnableFog);
	m_CurrentContext->fogParamsOffset = m_CommandQueue->GetCurrentSize();
	GfxFogParams* fog = m_CommandQueue->GetWritePointer<GfxFogParams>();
	fog->mode = fogMode;

	// Must call GetBuffer() after GetWritePointer() since it might be reallocated!
	const void* bufferStart = m_CommandQueue->GetBuffer();
	GfxPatchInfo& patchInfo = m_CurrentContext->patchInfo;
	patchInfo.AddPatchableVector(fogColor, fog->color, bufferStart, props);
	patchInfo.AddPatchableFloat(fogStart, fog->start, bufferStart, props);
	patchInfo.AddPatchableFloat(fogEnd, fog->end, bufferStart, props);
	patchInfo.AddPatchableFloat(fogDensity, fog->density, bufferStart, props);
	m_FogParams = *fog;
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void GfxDeviceClient::EnableFog (const GfxFogParams& fog)
{
	CheckMainThread();
	m_FogParams = fog;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EnableFog);
		if (IsRecording())
			m_CurrentContext->fogParamsOffset = m_CommandQueue->GetCurrentSize();
		m_CommandQueue->WriteValueType<GfxFogParams>(fog);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EnableFog(fog);
}

void GfxDeviceClient::DisableFog()
{
	CheckMainThread();
	m_FogParams.mode = kFogDisabled;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DisableFog);
		if (IsRecording())
			m_CurrentContext->fogParamsOffset = DisplayListContext::kFogParamsDisable;
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DisableFog();
}

VBO* GfxDeviceClient::CreateVBO()
{
	CheckMainThread();
	Assert(!IsRecording());
	VBO* vbo = new ThreadedVBO(*this);
	OnCreateVBO(vbo);
	return vbo;
}

void GfxDeviceClient::DeleteVBO( VBO* vbo )
{
	CheckMainThread();
	Assert(!IsRecording());
	OnDeleteVBO(vbo);
	delete vbo;
}

DynamicVBO&	GfxDeviceClient::GetDynamicVBO()
{
	CheckMainThread();
	if (!m_DynamicVBO)
	{
		Assert(!m_Threaded);
		return m_RealDevice->GetDynamicVBO();
	}
	return *m_DynamicVBO;
}

void GfxDeviceClient::BeginSkinning( int maxSkinCount )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginSkinning);
		m_CommandQueue->WriteValueType<int>(maxSkinCount);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	// Call this even when threaded
	GfxDevice::BeginSkinning(maxSkinCount);
}

bool GfxDeviceClient::SkinMesh( const SkinMeshInfo& skin, VBO* vbo )
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	// Only skin on render thread if buffer is not read back
	if (m_Threaded && skin.outVertices == NULL)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SkinMesh);
		m_CommandQueue->WriteValueType<SkinMeshInfo>(skin);
		ThreadedVBO* threadedVBO = static_cast<ThreadedVBO*>(vbo);
		m_CommandQueue->WriteValueType<ClientDeviceVBO*>(threadedVBO->GetClientDeviceVBO());
		m_CommandQueue->WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
		return true;
	}
	else
#endif
		return GfxDevice::SkinMesh(skin, vbo);
}

void GfxDeviceClient::EndSkinning()
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndSkinning);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	// Call this even when threaded
	GfxDevice::EndSkinning();
}

#if GFX_ENABLE_DRAW_CALL_BATCHING
void GfxDeviceClient::BeginStaticBatching(const ChannelAssigns& channels, GfxPrimitiveType topology)
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginStaticBatching);
		GfxCmdBeginStaticBatching statbat = { channels, topology };
		m_CommandQueue->WriteValueType<GfxCmdBeginStaticBatching>(statbat);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginStaticBatching(channels, topology);
}

void GfxDeviceClient::StaticBatchMesh( UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_StaticBatchMesh);
		GfxCmdStaticBatchMesh batch = { firstVertex, vertexCount, indices, firstIndexByte, indexCount };
		m_CommandQueue->WriteValueType<GfxCmdStaticBatchMesh>(batch);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->StaticBatchMesh(firstVertex, vertexCount, indices, firstIndexByte, indexCount);
}

void GfxDeviceClient::EndStaticBatching( VBO& vbo, const Matrix4x4f& matrix, TransformType transformType, int sourceChannels )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndStaticBatching);
		ThreadedVBO& threadedVBO = static_cast<ThreadedVBO&>(vbo);
		ClientDeviceVBO* clientVBO = threadedVBO.GetClientDeviceVBO();
		GfxCmdEndStaticBatching endbat = { clientVBO, matrix, transformType, sourceChannels };
		m_CommandQueue->WriteValueType<GfxCmdEndStaticBatching>(endbat);
		m_CommandQueue->WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndStaticBatching(vbo, matrix, transformType, sourceChannels);
}

void GfxDeviceClient::BeginDynamicBatching( const ChannelAssigns& shaderChannels, UInt32 channelsInVBO, size_t maxVertices, size_t maxIndices, GfxPrimitiveType topology)
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginDynamicBatching);
		GfxCmdBeginDynamicBatching dynbat = { shaderChannels, channelsInVBO, maxVertices, maxIndices, topology };
		m_CommandQueue->WriteValueType<GfxCmdBeginDynamicBatching>(dynbat);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginDynamicBatching(shaderChannels, channelsInVBO, maxVertices, maxIndices, topology);
}

void GfxDeviceClient::DynamicBatchMesh( const Matrix4x4f& matrix, const VertexBufferData& vertices, UInt32 firstVertex, UInt32 vertexCount, const IndexBufferData& indices, UInt32 firstIndexByte, UInt32 indexCount )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DynamicBatchMesh);
		GfxCmdDynamicBatchMesh batch = { matrix, vertices, firstVertex, vertexCount, indices, firstIndexByte, indexCount };
		m_CommandQueue->WriteValueType<GfxCmdDynamicBatchMesh>(batch);
		m_CommandQueue->WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DynamicBatchMesh(matrix, vertices, firstVertex, vertexCount, indices, firstIndexByte, indexCount);

}
#if ENABLE_SPRITES
void GfxDeviceClient::DynamicBatchSprite(const Matrix4x4f* matrix, const SpriteRenderData* rd, ColorRGBA32 color)
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DynamicBatchSprite);
		GfxCmdDynamicBatchSprite batch = { *matrix, rd, color };
		m_CommandQueue->WriteValueType<GfxCmdDynamicBatchSprite>(batch);
		m_CommandQueue->WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DynamicBatchSprite(matrix, rd, color);
}
#endif
void GfxDeviceClient::EndDynamicBatching( TransformType transformType )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndDynamicBatching);
		GfxCmdEndDynamicBatching endbat = { transformType };
		m_CommandQueue->WriteValueType<GfxCmdEndDynamicBatching>(endbat);
		m_CommandQueue->WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndDynamicBatching(transformType);
}
#endif

void GfxDeviceClient::AddBatchingStats( int batchedTris, int batchedVerts, int batchedCalls )
{
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_AddBatchingStats);
		GfxCmdAddBatchingStats stats = { batchedTris, batchedVerts, batchedCalls };
		m_CommandQueue->WriteValueType<GfxCmdAddBatchingStats>(stats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->AddBatchingStats(batchedTris, batchedVerts, batchedCalls);
}


GPUSkinningInfo* GfxDeviceClient::CreateGPUSkinningInfo()
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	SET_ALLOC_OWNER(this);

	// Call create directly. Interface spec says this is safe.
	GPUSkinningInfo *realInfo = m_RealDevice->CreateGPUSkinningInfo();
	if(!realInfo)
		return NULL;
	return new ClientGPUSkinningInfo(realInfo);
}

void GfxDeviceClient::DeleteGPUSkinningInfo(GPUSkinningInfo *info)
{
	CheckMainThread();
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DeleteGPUSkinningInfo);
		m_CommandQueue->WriteValueType<GPUSkinningInfo *>(((ClientGPUSkinningInfo *)info)->m_realSkinInfo);
		SubmitCommands();
		delete (ClientGPUSkinningInfo *)info;
		GFXDEVICE_LOCKSTEP_CLIENT();
}
	else
	{
		m_RealDevice->DeleteGPUSkinningInfo(((ClientGPUSkinningInfo *)info)->m_realSkinInfo);
		delete (ClientGPUSkinningInfo *)info;
	}

}

void GfxDeviceClient::SkinOnGPU(GPUSkinningInfo * info, bool lastThisFrame)
{
	CheckMainThread();
	if (m_Threaded)
	{
		ClientGPUSkinningInfo *ci = (ClientGPUSkinningInfo *)info;

		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SkinOnGPU);
		m_CommandQueue->WriteValueType<GPUSkinningInfo *>(ci->m_realSkinInfo);
		// Add the vbo object pointer to the stream, ci->m_realSkinInfo->m_destVBO will get updated in the worker.
		ThreadedVBO* vbo = (ThreadedVBO*)ci->m_DestVBO;
		m_CommandQueue->WriteValueType<ClientDeviceVBO *>(vbo->GetClientDeviceVBO());
		m_CommandQueue->WriteValueType<bool>(lastThisFrame);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		ClientGPUSkinningInfo *ci = (ClientGPUSkinningInfo *)info;
		ThreadedVBO* vbo = (ThreadedVBO*)ci->m_DestVBO;
		ci->m_realSkinInfo->SetDestVBO(vbo->GetNonThreadedVBO());
		m_RealDevice->SkinOnGPU(ci->m_realSkinInfo, lastThisFrame);
	}
}

void GfxDeviceClient::UpdateSkinSourceData(GPUSkinningInfo *info, const void *vertData, const BoneInfluence *skinData, bool dirty)
{
	CheckMainThread();
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UpdateSkinSourceData);
		m_CommandQueue->WriteValueType<GPUSkinningInfo *>(((ClientGPUSkinningInfo *)info)->m_realSkinInfo);
		m_CommandQueue->WriteValueType<const void *>(vertData);
		m_CommandQueue->WriteValueType<const BoneInfluence *>(skinData);
		m_CommandQueue->WriteValueType<bool>(dirty);
		SubmitCommands();
		UInt32 fence = InsertCPUFence();
		WaitOnCPUFence(fence);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		m_RealDevice->UpdateSkinSourceData(((ClientGPUSkinningInfo *)info)->m_realSkinInfo, vertData, skinData, dirty);
	}

}

void GfxDeviceClient::UpdateSkinBonePoses(GPUSkinningInfo *info, const int boneCount, const Matrix4x4f* poses)
{
	CheckMainThread();
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UpdateSkinBonePoses);
		m_CommandQueue->WriteValueType<GPUSkinningInfo *>(((ClientGPUSkinningInfo *)info)->m_realSkinInfo);
		m_CommandQueue->WriteValueType<int>(boneCount);
		m_CommandQueue->WriteArrayType<Matrix4x4f>(poses, boneCount);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		m_RealDevice->UpdateSkinBonePoses(((ClientGPUSkinningInfo *)info)->m_realSkinInfo, boneCount, poses);
	}

}



#if UNITY_XENON
RawVBO* GfxDeviceClient::CreateRawVBO(UInt32 size, UInt32 flags)
{
	CheckMainThread();
	Assert(!IsRecording());
	RawVBO* vbo = new RawThreadedVBO(*this, size, flags);
	return vbo;
}

void GfxDeviceClient::DeleteRawVBO(RawVBO* vbo)
{
	CheckMainThread();
	Assert(!IsRecording());
	delete vbo;
}


void GfxDeviceClient::EnablePersistDisplayOnQuit(bool enabled)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EnablePersistDisplayOnQuit);
		m_CommandQueue->WriteValueType<bool>(enabled);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EnablePersistDisplayOnQuit(enabled);
}

void GfxDeviceClient::RegisterTexture2D( TextureID tid, IDirect3DBaseTexture9* texture )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_RegisterTexture2D);
		m_CommandQueue->WriteValueType<TextureID>(tid);
		m_CommandQueue->WriteValueType<IDirect3DBaseTexture9*>(texture);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->RegisterTexture2D(tid, texture);
}

void GfxDeviceClient::PatchTexture2D( TextureID tid, IDirect3DBaseTexture9* texture )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_PatchTexture2D);
		m_CommandQueue->WriteValueType<TextureID>(tid);
		m_CommandQueue->WriteValueType<IDirect3DBaseTexture9*>(texture);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->PatchTexture2D(tid, texture);
}

void GfxDeviceClient::DeleteTextureEntryOnly( TextureID textureID )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DeleteTextureEntryOnly);
		m_CommandQueue->WriteValueType<TextureID>(textureID);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DeleteTextureEntryOnly(textureID);
}

void GfxDeviceClient::UnbindAndDelayReleaseTexture( IDirect3DBaseTexture9* texture )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UnbindAndDelayReleaseTexture);
		m_CommandQueue->WriteValueType<IDirect3DBaseTexture9*>(texture);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UnbindAndDelayReleaseTexture(texture);
}

void GfxDeviceClient::SetTextureWrapModes( TextureID textureID, TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetTextureWrapModes);
		m_CommandQueue->WriteValueType<TextureID>(textureID);
		m_CommandQueue->WriteValueType<TextureWrapMode>(wrapU);
		m_CommandQueue->WriteValueType<TextureWrapMode>(wrapV);
		m_CommandQueue->WriteValueType<TextureWrapMode>(wrapW);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetTextureWrapModes(textureID, wrapU, wrapV, wrapW);
}

void GfxDeviceClient::OnLastFrameCallback()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_OnLastFrameCallback);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->OnLastFrameCallback();
}

xenon::IVideoPlayer* GfxDeviceClient::CreateVideoPlayer(bool fullscreen)
{
	CheckMainThread();
	Assert(!IsRecording());
	xenon::VideoPlayerThreaded* vp = new xenon::VideoPlayerThreaded(*this, fullscreen);
	return vp;
}

void GfxDeviceClient::DeleteVideoPlayer(xenon::IVideoPlayer* player)
{
	CheckMainThread();
	Assert(!IsRecording());
	delete player;
}
#endif

RenderSurfaceHandle GfxDeviceClient::CreateRenderColorSurface (TextureID textureID, int width, int height, int samples, int depth, TextureDimension dim, RenderTextureFormat format, UInt32 createFlags)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	ClientDeviceRenderSurface* handle = new ClientDeviceRenderSurface(width, height);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateRenderColorSurface);
		GfxCmdCreateRenderColorSurface create = { textureID, width, height, samples, depth, dim, format, createFlags };
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(handle);
#else
		handle->internalHandle = m_RenderSurfaceMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface>(*handle);
#endif
		m_CommandQueue->WriteValueType<GfxCmdCreateRenderColorSurface>(create);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		handle->internalHandle = m_RealDevice->CreateRenderColorSurface(textureID, width, height, samples, depth, dim, format, createFlags);
	}
#endif
	return RenderSurfaceHandle(handle);
}

RenderSurfaceHandle GfxDeviceClient::CreateRenderDepthSurface (TextureID textureID, int width, int height, int samples, TextureDimension dim, DepthBufferFormat depthFormat, UInt32 createFlags)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	ClientDeviceRenderSurface* handle = new ClientDeviceRenderSurface(width, height);
	handle->zformat = depthFormat;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateRenderDepthSurface);
		GfxCmdCreateRenderDepthSurface create = { textureID, width, height, samples, dim, depthFormat, createFlags };
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(handle);
#else
		handle->internalHandle = m_RenderSurfaceMapper.CreateID();
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface>(*handle);
#endif
		m_CommandQueue->WriteValueType<GfxCmdCreateRenderDepthSurface>(create);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		handle->internalHandle = m_RealDevice->CreateRenderDepthSurface(textureID, width, height, samples, dim, depthFormat, createFlags);
	}
#endif
	return RenderSurfaceHandle(handle);
}

void GfxDeviceClient::DestroyRenderSurface (RenderSurfaceHandle& rs)
{
	CheckMainThread();
	if( !rs.IsValid() )
		return;

	ClientDeviceRenderSurface* handle = static_cast<ClientDeviceRenderSurface*>(rs.object);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DestroyRenderSurface);
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(handle);
#else
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface>(*handle);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		m_RealDevice->DestroyRenderSurface(handle->internalHandle);
		delete handle;
	}
#endif
	rs.Reset();
}

void GfxDeviceClient::DiscardContents (RenderSurfaceHandle& rs)
{
	CheckMainThread();
	if( !rs.IsValid() )
		return;

	ClientDeviceRenderSurface* handle = (ClientDeviceRenderSurface*)rs.object;
	handle->state = ClientDeviceRenderSurface::kInitial; // mark as "no contents"


	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DiscardContents);
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(handle);
#else
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface>(*handle);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		m_RealDevice->DiscardContents(handle->internalHandle);
	}
#endif
}

void GfxDeviceClient::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face)
{
	CheckMainThread();
	Assert(!IsRecording());

	BeforeRenderTargetChange(count, colorHandles, depthHandle);
	for (int i = 0; i < count; ++i)
		m_ActiveRenderColorSurfaces[i] = colorHandles[i];
	for (int i = count; i < kMaxSupportedRenderTargets; ++i)
		m_ActiveRenderColorSurfaces[i].Reset();
	m_ActiveRenderDepthSurface = depthHandle;
	AfterRenderTargetChange();

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetRenderTarget);
		GfxCmdSetRenderTarget srt;
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
			srt.colorHandles[i] = m_ActiveRenderColorSurfaces[i];
		srt.depthHandle = depthHandle;
#else
		for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
			srt.colorHandles[i] = ((ClientDeviceRenderSurface*)(m_ActiveRenderColorSurfaces[i].object))->internalHandle;
		srt.depthHandle = ((ClientDeviceRenderSurface*)depthHandle.object)->internalHandle;
#endif
		srt.colorCount = count;
		srt.face = face;
		srt.mipLevel = mipLevel;
		m_CommandQueue->WriteValueType<GfxCmdSetRenderTarget>(srt);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		RenderSurfaceHandle realColorHandle[kMaxSupportedRenderTargets];
		for (int i = 0; i < count; ++i)
		{
			ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(colorHandles[i].object);
			realColorHandle[i].object = colorSurf ? colorSurf->internalHandle.object : NULL;
			if(!realColorHandle[i].IsValid())
				realColorHandle[i] = m_RealDevice->GetBackBufferColorSurface();
		}
		ClientDeviceRenderSurface* depthSurf = static_cast<ClientDeviceRenderSurface*>(depthHandle.object);
		RenderSurfaceHandle realDepthHandle(depthSurf ? depthSurf->internalHandle.object : NULL);
		if(!realDepthHandle.IsValid())
			realDepthHandle = m_RealDevice->GetBackBufferDepthSurface();

		m_RealDevice->SetRenderTargets(count, realColorHandle, realDepthHandle, mipLevel, face);
	}
#endif
}

void GfxDeviceClient::SetRenderTargets (int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle, int mipLevel, CubemapFace face, UInt32 flags)
{
	CheckMainThread();
	Assert(!IsRecording());

	BeforeRenderTargetChange(count, colorHandles, depthHandle);
	for (int i = 0; i < count; ++i)
		m_ActiveRenderColorSurfaces[i] = colorHandles[i];
	for (int i = count; i < kMaxSupportedRenderTargets; ++i)
		m_ActiveRenderColorSurfaces[i].Reset();
	m_ActiveRenderDepthSurface = depthHandle;
	AfterRenderTargetChange();

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetRenderTargetWithFlags);
		GfxCmdSetRenderTarget srt;
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
			srt.colorHandles[i] = m_ActiveRenderColorSurfaces[i];
		srt.depthHandle = depthHandle;
#else
		for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
			srt.colorHandles[i] = m_ActiveRenderColorSurfaces[i].object ? ((ClientDeviceRenderSurface*)(m_ActiveRenderColorSurfaces[i].object))->internalHandle : NULL;
		srt.depthHandle = depthHandle.object ? ((ClientDeviceRenderSurface*)depthHandle.object)->internalHandle: NULL;
#endif
		srt.colorCount = count;
		srt.face = face;
		srt.mipLevel = mipLevel;
		m_CommandQueue->WriteValueType<GfxCmdSetRenderTarget>(srt);
		m_CommandQueue->WriteValueType<UInt32>(flags);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		RenderSurfaceHandle realColorHandle[kMaxSupportedRenderTargets];
		for (int i = 0; i < count; ++i)
		{
			ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(colorHandles[i].object);
			realColorHandle[i].object = colorSurf ? colorSurf->internalHandle.object : NULL;
			if(!realColorHandle[i].IsValid())
				realColorHandle[i] = m_RealDevice->GetBackBufferColorSurface();
		}
		ClientDeviceRenderSurface* depthSurf = static_cast<ClientDeviceRenderSurface*>(depthHandle.object);
		RenderSurfaceHandle realDepthHandle(depthSurf ? depthSurf->internalHandle.object : NULL);
		if(!realDepthHandle.IsValid())
			realDepthHandle = m_RealDevice->GetBackBufferDepthSurface();

		m_RealDevice->SetRenderTargets (count, realColorHandle, realDepthHandle, mipLevel, face, flags);
	}
#endif
}

void GfxDeviceClient::ResolveColorSurface (RenderSurfaceHandle srcHandle, RenderSurfaceHandle dstHandle)
{
	CheckMainThread();
	ClientDeviceRenderSurface* src = static_cast<ClientDeviceRenderSurface*>(srcHandle.object);
	ClientDeviceRenderSurface* dst = static_cast<ClientDeviceRenderSurface*>(dstHandle.object);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ResolveColorSurface);
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(src);
		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(dst);
#else
//		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(src.object ? ((ClientDeviceRenderSurface*)(src.object))->internalHandle : NULL);
//		m_CommandQueue->WriteValueType<ClientDeviceRenderSurface*>(dst.object ? ((ClientDeviceRenderSurface*)(dst.object))->internalHandle : NULL);
		//todo.
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
		m_RealDevice->ResolveColorSurface (src->internalHandle, dst->internalHandle);
#endif
}

void GfxDeviceClient::ResolveDepthIntoTexture (RenderSurfaceHandle colorHandle, RenderSurfaceHandle depthHandle)
{
	CheckMainThread();
	if (m_Serialize)
	{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdResolveDepthIntoTexture resolve = { colorHandle, depthHandle };
#else
		GfxCmdResolveDepthIntoTexture resolve = { colorHandle.object ? ((ClientDeviceRenderSurface*)(colorHandle.object))->internalHandle : NULL , depthHandle.object ? ((ClientDeviceRenderSurface*)(depthHandle.object))->internalHandle : NULL};
#endif
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ResolveDepthIntoTexture);
		m_CommandQueue->WriteValueType<GfxCmdResolveDepthIntoTexture>(resolve);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(colorHandle.object);
		ClientDeviceRenderSurface* depthSurf = static_cast<ClientDeviceRenderSurface*>(depthHandle.object);
		m_RealDevice->ResolveDepthIntoTexture (colorSurf->internalHandle, depthSurf->internalHandle);
	}
#endif
}


RenderSurfaceHandle GfxDeviceClient::GetActiveRenderColorSurface (int index)
{
	CheckMainThread();
	return m_ActiveRenderColorSurfaces[index];
}

RenderSurfaceHandle GfxDeviceClient::GetActiveRenderDepthSurface ()
{
	CheckMainThread();
	return m_ActiveRenderDepthSurface;
}

void GfxDeviceClient::BeforeRenderTargetChange(int count, RenderSurfaceHandle* colorHandles, RenderSurfaceHandle depthHandle)
{
	if (!GetFrameStats().m_StatsEnabled)
		return;

	// mark any rendered-into render target surfaces as "resolved"
	for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
	{
		if (i >= count || colorHandles[i] != m_ActiveRenderColorSurfaces[i])
		{
			ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)m_ActiveRenderColorSurfaces[i].object;
			if (colorSurf && colorSurf->state != ClientDeviceRenderSurface::kInitial)
				colorSurf->state = ClientDeviceRenderSurface::kResolved;
		}
	}

	if (depthHandle != m_ActiveRenderDepthSurface)
	{
		ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)m_ActiveRenderDepthSurface.object;
		if (depthSurf && depthSurf->state != ClientDeviceRenderSurface::kInitial)
			depthSurf->state = ClientDeviceRenderSurface::kResolved;
	}
}


void GfxDeviceClient::AfterRenderTargetChange()
{
	if (m_ActiveRenderColorSurfaces[0].IsValid() && !m_ActiveRenderColorSurfaces[0].object->backBuffer)
	{
		ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)m_ActiveRenderColorSurfaces[0].object;
		if (m_ActiveRenderDepthSurface.IsValid())
		{
			ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)m_ActiveRenderDepthSurface.object;
			if (colorSurf->width != depthSurf->width || colorSurf->height != depthSurf->height)
			{
				ErrorString("Dimensions of color surface does not match dimensions of depth surface");
			}
		}
		m_CurrentTargetWidth = colorSurf->width;
		m_CurrentTargetHeight = colorSurf->height;
	}
	else
	{
#if UNITY_WINRT
		if (0 == m_CurrentWindowWidth && 0 == m_CurrentWindowHeight && NULL != m_RealDevice)
		{
			m_CurrentTargetWidth = m_RealDevice->GetCurrentTargetWidth();
			m_CurrentTargetHeight = m_RealDevice->GetCurrentTargetHeight();
			m_CurrentWindowWidth = m_CurrentTargetWidth;
			m_CurrentWindowHeight = m_CurrentTargetHeight;
		}
#endif
		m_CurrentTargetWidth = m_CurrentWindowWidth;
		m_CurrentTargetHeight = m_CurrentWindowHeight;
	}
}

bool GfxDeviceClient::IsRenderTargetConfigValid(UInt32 width, UInt32 height, RenderTextureFormat colorFormat, DepthBufferFormat depthFormat)
{
	CheckMainThread();
#if ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	return GfxDevice::IsRenderTargetConfigValid(width, height, colorFormat, depthFormat);
#else
	return m_RealDevice->IsRenderTargetConfigValid(width, height, colorFormat, depthFormat);
#endif
}

void GfxDeviceClient::SetSurfaceFlags(RenderSurfaceHandle surf, UInt32 flags, UInt32 keepFlags)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetSurfaceFlags);
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdSetSurfaceFlags sf = { surf, flags, keepFlags };
#else
		GfxCmdSetSurfaceFlags sf = {  surf.object ? ((ClientDeviceRenderSurface*)(surf.object))->internalHandle : NULL , flags, keepFlags };
#endif
		m_CommandQueue->WriteValueType<GfxCmdSetSurfaceFlags>(sf);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		ClientDeviceRenderSurface* surface = (ClientDeviceRenderSurface*)surf.object;
		m_RealDevice->SetSurfaceFlags(surface->internalHandle, flags, keepFlags);
	}
#endif
}

void GfxDeviceClient::UploadTexture2D( TextureID texture, TextureDimension dimension, UInt8* srcData, int srcSize, int width, int height, TextureFormat format, int mipCount, UInt32 uploadFlags, int skipMipLevels, TextureUsageMode usageMode, TextureColorSpace colorSpace )
{
	CheckMainThread();
	if (m_Serialize)
	{
		// Don't want to upload textures from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UploadTexture2D);
		Assert(width >= 0 && height >= 0);
		GfxCmdUploadTexture2D upload = { texture, dimension, srcSize, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace };
		m_CommandQueue->WriteValueType<GfxCmdUploadTexture2D>(upload);
		WriteBufferData(srcData, srcSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UploadTexture2D(texture, dimension, srcData, srcSize, width, height, format, mipCount, uploadFlags, skipMipLevels, usageMode, colorSpace);
}

void GfxDeviceClient::UploadTextureSubData2D( TextureID texture, UInt8* srcData, int srcSize, int mipLevel, int x, int y, int width, int height, TextureFormat format, TextureColorSpace colorSpace )
{
	CheckMainThread();
	if (m_Serialize)
	{
		// Don't want to upload textures from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UploadTextureSubData2D);
		GfxCmdUploadTextureSubData2D upload = { texture, srcSize, mipLevel, x, y, width, height, format, colorSpace };
		m_CommandQueue->WriteValueType<GfxCmdUploadTextureSubData2D>(upload);
		WriteBufferData(srcData, srcSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UploadTextureSubData2D(texture, srcData, srcSize, mipLevel, x, y, width, height, format, colorSpace);
}

void GfxDeviceClient::UploadTextureCube( TextureID texture, UInt8* srcData, int srcSize, int faceDataSize, int size, TextureFormat format, int mipCount, UInt32 uploadFlags, TextureColorSpace colorSpace )
{
	CheckMainThread();
	if (m_Serialize)
	{
		// Don't want to upload textures from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UploadTextureCube);
		GfxCmdUploadTextureCube upload = { texture, srcSize, faceDataSize, size, format, mipCount, uploadFlags, colorSpace };
		m_CommandQueue->WriteValueType<GfxCmdUploadTextureCube>(upload);
		WriteBufferData(srcData, srcSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UploadTextureCube(texture, srcData, srcSize, faceDataSize, size, format, mipCount, uploadFlags, colorSpace);
}

void GfxDeviceClient::UploadTexture3D( TextureID texture, UInt8* srcData, int srcSize, int width, int height, int depth, TextureFormat format, int mipCount, UInt32 uploadFlags )
{
	CheckMainThread();
	if (m_Serialize)
	{
		// Don't want to upload textures from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UploadTexture3D);
		GfxCmdUploadTexture3D upload = { texture, srcSize, width, height, depth, format, mipCount, uploadFlags };
		m_CommandQueue->WriteValueType<GfxCmdUploadTexture3D>(upload);
		WriteBufferData(srcData, srcSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UploadTexture3D(texture, srcData, srcSize, width, height, depth, format, mipCount, uploadFlags);
}

void GfxDeviceClient::DeleteTexture( TextureID texture )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DeleteTexture);
		m_CommandQueue->WriteValueType<TextureID>(texture);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DeleteTexture(texture);
}

GfxDevice::PresentMode GfxDeviceClient::GetPresentMode()
{
	if (!m_Threaded)
	{
		// If we're not threaded don't change behavior
		return m_RealDevice->GetPresentMode();
	}
	if (!m_RealDevice)
		return kPresentAfterDraw;
	GfxDeviceRenderer renderer = m_RealDevice->GetRenderer();
	switch (renderer)
	{
		case kGfxRendererD3D9:
		{
			// In D3D9 BeginFrame() waits for the last Present() to finish on the render thread
			// so we catch lost device state. It's best to present immediately after drawing.
			return kPresentAfterDraw;
		}
		case kGfxRendererD3D11:
		{
			// We have to to wait for the last Present() to finish before leaving message loop,
			// so it's good to present as early as possible to avoid waiting much (case 488862).
			return kPresentBeforeUpdate;
		}
		default:
		{
			// By default we synchronize like with D3D9, so the render thread won't fall behind.
			return kPresentAfterDraw;
		}
	}
}

void GfxDeviceClient::BeginFrame()
{
	CheckMainThread();
	Assert(!m_InsideFrame);
	m_InsideFrame = true;
	if (m_Serialize)
	{
		WaitForPendingPresent();
		// Worker thread should check GetNeedsBeginFrame()
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginFrame);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginFrame();
}

void GfxDeviceClient::EndFrame()
{
	CheckMainThread();
	if (!m_InsideFrame)
		return;
	m_InsideFrame = false;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndFrame);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndFrame();
}

void GfxDeviceClient::PresentFrame()
{
	CheckMainThread();

	((ClientDeviceRenderSurface*)m_BackBufferColor.object)->state = ClientDeviceRenderSurface::kInitial;
	((ClientDeviceRenderSurface*)m_BackBufferDepth.object)->state = ClientDeviceRenderSurface::kInitial;

	if (m_Serialize)
	{
		// Check that we waited on event before issuing a new one
		bool signalEvent = !m_PresentPending;
		m_PresentPending = true;
		m_PresentFrameID++;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_PresentFrame);
		m_CommandQueue->WriteValueType<GfxCmdBool>(signalEvent);
		m_CommandQueue->WriteValueType<UInt32>(m_PresentFrameID);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->PresentFrame();
}

bool GfxDeviceClient::IsValidState()
{
	CheckMainThread();
	if (!m_RealDevice)
		return true;
	return m_RealDevice->IsValidState();
}

bool GfxDeviceClient::HandleInvalidState()
{
	CheckMainThread();
	if (IsValidState())
		return true;
	Assert(!IsRecording());

#if GFX_SUPPORTS_D3D9
	// Mark threaded dynamic VBOs as lost
	ResetDynamicVBs();
#endif
#if GFX_SUPPORTS_OPENGLES20
	// Only mark VBOs lost for GLES2.0 renderers (case 570721)
	if (m_Renderer == kGfxRendererOpenGLES20Desktop || m_Renderer == kGfxRendererOpenGLES20Mobile)
		MarkAllVBOsLost();
#endif

	CommonReloadResources(kReleaseRenderTextures);

	bool insideFrame = m_InsideFrame;
	if (insideFrame)
		EndFrame();
	AcquireThreadOwnership();
	bool success = m_RealDevice->HandleInvalidState();
	ReleaseThreadOwnership();
	if (success && insideFrame)
		BeginFrame();
	return success;
}

void GfxDeviceClient::ResetDynamicResources()
{
	#if GFX_SUPPORTS_D3D9
	// Mark threaded dynamic VBOs as lost
	ResetDynamicVBs();
	#endif

	// We should have acquired thread ownership here
	Assert(!m_Serialize);
	GetRealGfxDevice().ResetDynamicResources();
}

bool GfxDeviceClient::IsReadyToBeginFrame()
{
	if (m_Threaded && m_RealDevice->GetRenderer() == kGfxRendererD3D11)
	{
		// DXGI requires us to keep processing events while the render thread calls Present()
		// We have to wait for that before we begin preparing the next frame (case 488862)
		return m_DeviceWorker->DidPresentFrame(m_PresentFrameID);
	}
	return true;
}

void GfxDeviceClient::FinishRendering()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_FinishRendering);
		SubmitCommands();
		GetGfxDeviceWorker()->WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->FinishRendering();
}

UInt32 GfxDeviceClient::InsertCPUFence()
{
	CheckMainThread();
	if (m_Threaded)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_InsertCPUFence);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
		return ++m_CurrentCPUFence;
	}
	return 0;
}

UInt32 GfxDeviceClient::GetNextCPUFence()
{
	return m_Threaded ? (m_CurrentCPUFence + 1) : 0;
}

void GfxDeviceClient::WaitOnCPUFence(UInt32 fence)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	CheckMainThread();
	if (m_Threaded)
	{
		// Fence must have been already inserted
		if (SInt32(fence - m_CurrentCPUFence) <= 0)
		{
			m_DeviceWorker->WaitOnCPUFence(fence);
		}
		else
			ErrorString("CPU fence is invalid or very old!");
	}
#endif
}

void GfxDeviceClient::AcquireThreadOwnership()
{
	CheckMainThread();
	if (!m_Threaded)
		return;

	m_ThreadOwnershipCount++;
	if (m_ThreadOwnershipCount > 1)
		return;

	// Worker releases ownership
	Assert(m_Serialize);
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ReleaseThreadOwnership);
	SubmitCommands();
	WaitForSignal();
	GFXDEVICE_LOCKSTEP_CLIENT();

	// Caller acquires ownership
	m_RealDevice->AcquireThreadOwnership();

	SetRealGfxDeviceThreadOwnership();

	// We shouldn't serialize any commands
	m_Serialize = false;
}

void GfxDeviceClient::ReleaseThreadOwnership()
{
	CheckMainThread();
	if (!m_Threaded)
		return;

	Assert(m_ThreadOwnershipCount);
	m_ThreadOwnershipCount--;
	if (m_ThreadOwnershipCount > 0)
		return;

	// Caller releases ownership
	m_RealDevice->ReleaseThreadOwnership();

	// Worker acquires ownership
	m_Serialize = true;
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_AcquireThreadOwnership);
	SubmitCommands();
	WaitForSignal();
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void GfxDeviceClient::ImmediateVertex( float x, float y, float z )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateVertex);
		GfxCmdVector3 data = {x, y, z};
		m_CommandQueue->WriteValueType<GfxCmdVector3>(data);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateVertex(x, y, z);
}

void GfxDeviceClient::ImmediateNormal( float x, float y, float z )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateNormal);
		GfxCmdVector3 data = {x, y, z};
		m_CommandQueue->WriteValueType<GfxCmdVector3>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateNormal(x, y, z);
}

void GfxDeviceClient::ImmediateColor( float r, float g, float b, float a )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateColor);
		GfxCmdVector4 data = {r, g, b, a};
		m_CommandQueue->WriteValueType<GfxCmdVector4>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateColor(r, g, b, a);
}

void GfxDeviceClient::ImmediateTexCoordAll( float x, float y, float z )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateTexCoordAll);
		GfxCmdVector3 data = {x, y, z};
		m_CommandQueue->WriteValueType<GfxCmdVector3>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateTexCoordAll(x, y, z);
}

void GfxDeviceClient::ImmediateTexCoord( int unit, float x, float y, float z )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateTexCoord);
		GfxCmdImmediateTexCoord data = {unit, x, y, z};
		m_CommandQueue->WriteValueType<GfxCmdImmediateTexCoord>(data);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateTexCoord(unit, x, y, z);
}

void GfxDeviceClient::ImmediateBegin( GfxPrimitiveType type )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateBegin);
		m_CommandQueue->WriteValueType<GfxPrimitiveType>(type);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateBegin(type);
}

void GfxDeviceClient::ImmediateEnd()
{
	BeforeDrawCall(true);
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ImmediateEnd);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ImmediateEnd();
}

bool GfxDeviceClient::BeginRecording()
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	return false;
#endif
	Assert(m_RecordDepth < m_MaxCallDepth);
	DisplayListContext& parentContext = *m_CurrentContext;
	m_RecordDepth++;
	m_IsRecording = true;
	m_CurrentContext = &m_DisplayListStack[m_RecordDepth];
	memcpy(m_CurrentContext->shadersActive, parentContext.shadersActive, sizeof(parentContext.shadersActive));
	m_CurrentContext->hasSetShaders = false;
	m_CommandQueue = &m_CurrentContext->commandQueue;
	m_Serialize = true;
	return true;
}

bool GfxDeviceClient::EndRecording( GfxDisplayList** outDisplayList )
{
	Assert(m_RecordDepth > 0);
	m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DisplayList_End);
	const void* data = m_CommandQueue->GetBuffer();
	size_t size = m_CommandQueue->GetCurrentSize();
	bool failed = m_CurrentContext->recordFailed;
	ThreadedDisplayList* displayList = new ThreadedDisplayList(data, size, *m_CurrentContext);
	m_CurrentContext->Reset();

	m_RecordDepth--;
	m_IsRecording = (m_RecordDepth != 0);
	m_Serialize = m_Threaded || m_IsRecording;
	m_CurrentContext = &m_DisplayListStack[m_RecordDepth];
	m_CommandQueue = &m_CurrentContext->commandQueue;

	// Execute just-recorded display list
	displayList->Call();

	if (failed)
		SAFE_RELEASE(displayList);

	Assert(outDisplayList && *outDisplayList==NULL);
	*outDisplayList = displayList;
	return !failed;
}

bool GfxDeviceClient::CaptureScreenshot( int left, int bottom, int width, int height, UInt8* rgba32 )
{
	CheckMainThread();
	if (m_Serialize)
	{
		bool success = false;
		GfxCmdCaptureScreenshot capture = { left, bottom, width, height, rgba32, &success };
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CaptureScreenshot);
		m_CommandQueue->WriteValueType<GfxCmdCaptureScreenshot>(capture);
		SubmitCommands();
		GetGfxDeviceWorker()->WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
		return success;
	}
	else
		return m_RealDevice->CaptureScreenshot(left, bottom, width, height, rgba32);
}

bool GfxDeviceClient::ReadbackImage( ImageReference& image, int left, int bottom, int width, int height, int destX, int destY )
{
	CheckMainThread();
	if (m_Serialize)
	{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		bool success = false;
		GfxCmdReadbackImage read = { image, left, bottom, width, height, destX, destY, &success };
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ReadbackImage);
		m_CommandQueue->WriteValueType<GfxCmdReadbackImage>(read);
		SubmitCommands();
		GetGfxDeviceWorker()->WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
		return success;
#else
		// todo.
		return false;
#endif

	}
	else
		return m_RealDevice->ReadbackImage(image, left, bottom, width, height, destX, destY);
}

void GfxDeviceClient::GrabIntoRenderTexture (RenderSurfaceHandle rs, RenderSurfaceHandle rd, int x, int y, int width, int height)
{
	CheckMainThread();
	if (m_Serialize)
	{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdGrabIntoRenderTexture grab = { rs, rd, x, y, width, height };
#else
		GfxCmdGrabIntoRenderTexture grab = { rs.object ? ((ClientDeviceRenderSurface*)(rs.object))->internalHandle : NULL , rd.object ? ((ClientDeviceRenderSurface*)(rd.object))->internalHandle : NULL , x, y, width, height };
#endif
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_GrabIntoRenderTexture);
		m_CommandQueue->WriteValueType<GfxCmdGrabIntoRenderTexture>(grab);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(rs.object);
		ClientDeviceRenderSurface* depthSurf = static_cast<ClientDeviceRenderSurface*>(rd.object);
		m_RealDevice->GrabIntoRenderTexture(colorSurf->internalHandle, depthSurf->internalHandle, x, y, width, height);
	}
#endif
}


#if ENABLE_PROFILER
// Gets "what is being done" description from profiler sample
// hierarchy, e.g. "Camera.Render/RenderOpaqueGeometry"
static void GetContextDescriptionFromProfiler(std::string& outName)
{
	UnityProfilerPerThread* profiler = UnityProfilerPerThread::ms_InstanceTLS;
	if (profiler)
	{
		// Do not get the last (most child) level, since that's usually always
		// the same like DrawVBO.
		for (int level = 3; level > 0; --level)
		{
			const ProfilerSample* sample = profiler->GetActiveSample(level);
			if (sample && sample->information)
			{
				if (!outName.empty())
					outName += '/';
				outName += sample->information->name;
			}
		}
	}
	if (outName.empty())
		outName = "<run with profiler for info>";
}
#endif // #if ENABLE_PROFILER


void GfxDeviceClient::IgnoreNextUnresolveOnCurrentRenderTarget()
{
	for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
	{
		ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)m_ActiveRenderColorSurfaces[i].object;
		if(colorSurf)
			colorSurf->state = ClientDeviceRenderSurface::kInitial;
	}

	ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)m_ActiveRenderDepthSurface.object;
	depthSurf->state = ClientDeviceRenderSurface::kInitial;
}

void GfxDeviceClient::IgnoreNextUnresolveOnRS(RenderSurfaceHandle rs)
{
	if (!rs.IsValid())
		return;
	((ClientDeviceRenderSurface*)rs.object)->state = ClientDeviceRenderSurface::kInitial;
}



void GfxDeviceClient::BeforeDrawCall(bool immediateMode)
{
	if (!GetFrameStats().m_StatsEnabled)
		return;

	ClientDeviceRenderSurface* colorWarn = NULL;
	ClientDeviceRenderSurface* depthWarn = NULL;
	bool backColorWarn = false;
	bool backDepthWarn = false;

	// Check if any of surfaces have been resolved, not cleared/discarded and now
	// we want to render into them -- warn about this situation if needed.
	// Then set all surfaces as "rendered into".
	for (int i = 0; i < kMaxSupportedRenderTargets; ++i)
	{
		ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)m_ActiveRenderColorSurfaces[i].object;
		if(colorSurf)
		{
			if (colorSurf->state == ClientDeviceRenderSurface::kResolved)
				colorWarn = colorSurf;
			colorSurf->state = ClientDeviceRenderSurface::kRendered;
		}
	}

	ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)m_ActiveRenderDepthSurface.object;
	if(depthSurf)
	{	
		if (depthSurf->zformat != kDepthFormatNone && depthSurf->state == ClientDeviceRenderSurface::kResolved)
			depthWarn = depthSurf;
		depthSurf->state = ClientDeviceRenderSurface::kRendered;
	}

	#if ENABLE_PROFILER
	// In development builds, emit warnings if we're emulating
	// a tiled GPU.
	if (gGraphicsCaps.warnRenderTargetUnresolves && (colorWarn || depthWarn || backColorWarn || backDepthWarn))
	{
		std::string desc;
		GetContextDescriptionFromProfiler(desc);
		if (colorWarn)
		{
			WarningStringMsg ("Tiled GPU perf. warning: RenderTexture %s (%dx%d) was not cleared/discarded, doing %s", depthWarn ? "" : "color surface ", colorWarn->width, colorWarn->height, desc.c_str());
		}
		else if (depthWarn)
		{
			WarningStringMsg ("Tiled GPU perf. warning: RenderTexture depth surface (%dx%d) was not cleared/discarded, doing %s", depthWarn->width, depthWarn->height, desc.c_str());
		}
		else if (backColorWarn)
		{
			WarningStringMsg ("Tiled GPU perf. warning: Backbuffer %s was not cleared/discarded, doing %s", backDepthWarn ? "" : "color surface ", desc.c_str());
		}
		else if (backDepthWarn)
		{
			WarningStringMsg ("Tiled GPU perf. warning: Backbuffer depth surface was not cleared/discarded, doing %s", desc.c_str());
		}
	}
	#endif
}

bool GfxDeviceClient::IsPositionRequiredForTexGen (int texStageIndex) const
{
#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	return m_CurrentContext->shadersActive[kShaderVertex] == 0 && (posForTexGen & (1<<texStageIndex) != 0);
#else
	return m_RealDevice->IsPositionRequiredForTexGen(texStageIndex);
#endif
}

bool GfxDeviceClient::IsNormalRequiredForTexGen (int texStageIndex) const
{
#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	return m_CurrentContext->shadersActive[kShaderVertex] == 0 && (nrmForTexGen & (1<<texStageIndex) != 0);
#else
	return m_RealDevice->IsNormalRequiredForTexGen(texStageIndex);
#endif
}

bool GfxDeviceClient::IsPositionRequiredForTexGen() const
{
#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	return m_CurrentContext->shadersActive[kShaderVertex] == 0 && posForTexGen != 0;
#else
	return m_RealDevice->IsPositionRequiredForTexGen();
#endif
}

bool GfxDeviceClient::IsNormalRequiredForTexGen() const
{
#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	return m_CurrentContext->shadersActive[kShaderVertex] == 0 && nrmForTexGen != 0;
#else
	return m_RealDevice->IsNormalRequiredForTexGen();
#endif
}

void GfxDeviceClient::SetActiveContext (void* ctx)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetActiveContext);
		m_CommandQueue->WriteValueType<void*>(ctx);
		SubmitCommands();
		GetGfxDeviceWorker()->WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetActiveContext (ctx);
}


void GfxDeviceClient::ResetFrameStats()
{
	CheckMainThread();
	m_Stats.ResetClientStats();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ResetFrameStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ResetFrameStats();
}

void GfxDeviceClient::BeginFrameStats()
{
	((ClientDeviceRenderSurface*)(m_BackBufferColor.object))->state = ClientDeviceRenderSurface::kInitial;
	((ClientDeviceRenderSurface*)(m_BackBufferDepth.object))->state = ClientDeviceRenderSurface::kInitial;

	CheckMainThread();
	m_Stats.BeginFrameStats();
	if (m_Serialize)
	{
		m_CommandQueue->ResetWriteWaitTime();
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginFrameStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginFrameStats();
}

void GfxDeviceClient::EndFrameStats()
{
	CheckMainThread();
	m_Stats.EndClientFrameStats();
	if (m_Serialize)
	{
		m_Stats.m_ClientFrameTime -= m_CommandQueue->GetWriteWaitTime();
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndFrameStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndFrameStats();
}

void GfxDeviceClient::SaveDrawStats()
{
	CheckMainThread();
	m_SavedStats.CopyClientStats(m_Stats);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SaveDrawStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SaveDrawStats();
}

void GfxDeviceClient::RestoreDrawStats()
{
	CheckMainThread();
	m_Stats.CopyClientStats(m_SavedStats);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_RestoreDrawStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->RestoreDrawStats();
}

void GfxDeviceClient::SynchronizeStats()
{
	CheckMainThread();
	if (m_Threaded)
	{
		GetGfxDeviceWorker()->GetLastFrameStats(m_Stats);
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SynchronizeStats);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_Stats.CopyAllDrawStats(m_RealDevice->GetFrameStats());
}

void* GfxDeviceClient::GetNativeGfxDevice()
{
	AcquireThreadOwnership ();
	void* result = m_RealDevice->GetNativeGfxDevice();
	ReleaseThreadOwnership ();
	return result;
}

void* GfxDeviceClient::GetNativeTexturePointer(TextureID id)
{
	AcquireThreadOwnership ();
	void* result = m_RealDevice->GetNativeTexturePointer(id);
	ReleaseThreadOwnership ();
	return result;
}

UInt32 GfxDeviceClient::GetNativeTextureID(TextureID id)
{
#if ENABLE_TEXTUREID_MAP
	AcquireThreadOwnership ();
	UInt32 result = m_RealDevice->GetNativeTextureID(id);
	ReleaseThreadOwnership ();
	return result;
#else
	return id.m_ID;
#endif
}

#if ENABLE_TEXTUREID_MAP
	intptr_t GfxDeviceClient::CreateExternalTextureFromNative(intptr_t nativeTex)
	{
		AcquireThreadOwnership ();
		intptr_t result = m_RealDevice->CreateExternalTextureFromNative(nativeTex);
		ReleaseThreadOwnership ();
		return result;
	}
	void GfxDeviceClient::UpdateExternalTextureFromNative(TextureID tex, intptr_t nativeTex)
	{
		AcquireThreadOwnership ();
		m_RealDevice->UpdateExternalTextureFromNative(tex, nativeTex);
		ReleaseThreadOwnership ();
	}
#endif

void GfxDeviceClient::InsertCustomMarker (int marker)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_InsertCustomMarker);
		m_CommandQueue->WriteValueType<int>(marker);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->InsertCustomMarker (marker);
}


void GfxDeviceClient::SetComputeBufferData (ComputeBufferID bufferHandle, const void* data, size_t size)
{
	CheckMainThread();
	DebugAssert (bufferHandle.IsValid() && data && size);
	if (m_Serialize)
	{
		// Don't want to upload data from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetComputeBufferData);
		m_CommandQueue->WriteValueType<ComputeBufferID>(bufferHandle);
		m_CommandQueue->WriteValueType<size_t>(size);
		m_CommandQueue->WriteStreamingData(data, size);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetComputeBufferData (bufferHandle, data, size);
}


void GfxDeviceClient::GetComputeBufferData (ComputeBufferID bufferHandle, void* dest, size_t destSize)
{
	CheckMainThread();
	DebugAssert (bufferHandle.IsValid() && dest && destSize);
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_GetComputeBufferData);
		m_CommandQueue->WriteValueType<ComputeBufferID>(bufferHandle);
		m_CommandQueue->WriteValueType<size_t>(destSize);
		m_CommandQueue->WriteValueType<void*>(dest);
		SubmitCommands();
		GetGfxDeviceWorker()->WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->GetComputeBufferData (bufferHandle, dest, destSize);
}


void GfxDeviceClient::CopyComputeBufferCount (ComputeBufferID srcBuffer, ComputeBufferID dstBuffer, UInt32 dstOffset)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CopyComputeBufferCount);
		m_CommandQueue->WriteValueType<ComputeBufferID>(srcBuffer);
		m_CommandQueue->WriteValueType<ComputeBufferID>(dstBuffer);
		m_CommandQueue->WriteValueType<UInt32>(dstOffset);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->CopyComputeBufferCount (srcBuffer, dstBuffer, dstOffset);
}

void GfxDeviceClient::SetRandomWriteTargetTexture (int index, TextureID tid)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetRandomWriteTargetTexture);
		m_CommandQueue->WriteValueType<int>(index);
		m_CommandQueue->WriteValueType<TextureID>(tid);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetRandomWriteTargetTexture (index, tid);
}

void GfxDeviceClient::SetRandomWriteTargetBuffer (int index, ComputeBufferID bufferHandle)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetRandomWriteTargetBuffer);
		m_CommandQueue->WriteValueType<int>(index);
		m_CommandQueue->WriteValueType<ComputeBufferID>(bufferHandle);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetRandomWriteTargetBuffer (index, bufferHandle);
}

void GfxDeviceClient::ClearRandomWriteTargets ()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ClearRandomWriteTargets);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ClearRandomWriteTargets ();
}

ComputeProgramHandle GfxDeviceClient::CreateComputeProgram (const UInt8* code, size_t codeSize)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	ClientDeviceComputeProgram* handle = UNITY_NEW(ClientDeviceComputeProgram, kMemGfxThread);
	if (m_Serialize)
	{
		// Don't want to upload shaders from a display list
		m_CurrentContext->recordFailed = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateComputeProgram);
		m_CommandQueue->WriteValueType<ClientDeviceComputeProgram*>(handle);
		m_CommandQueue->WriteValueType<size_t>(codeSize);
		m_CommandQueue->WriteStreamingData(code, codeSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		handle->internalHandle = m_RealDevice->CreateComputeProgram (code, codeSize);
	}
	return ComputeProgramHandle(handle);
}

void GfxDeviceClient::DestroyComputeProgram (ComputeProgramHandle& cpHandle)
{
	CheckMainThread();

	ClientDeviceComputeProgram* cp = static_cast<ClientDeviceComputeProgram*>(cpHandle.object);
	if (!cp)
		return;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DestroyComputeProgram);
		m_CommandQueue->WriteValueType<ClientDeviceComputeProgram*>(cp);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		m_RealDevice->DestroyComputeProgram (cp->internalHandle);
		UNITY_DELETE (cp, kMemGfxThread);
	}

	cpHandle.Reset();
}

void GfxDeviceClient::CreateComputeConstantBuffers (unsigned count, const UInt32* sizes, ConstantBufferHandle* outCBs)
{
	Assert (count <= kMaxSupportedConstantBuffers);

	CheckMainThread();

	for (unsigned i = 0; i < count; ++i)
		outCBs[i].object = UNITY_NEW(ClientDeviceConstantBuffer(sizes[i]), kMemGfxThread);

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateComputeConstantBuffers);
		m_CommandQueue->WriteValueType<unsigned>(count);
		for (unsigned i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(outCBs[i].object);
			m_CommandQueue->WriteValueType<ClientDeviceConstantBuffer*>(handle);
		}
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
		m_RealDevice->CreateComputeConstantBuffers (count, sizes, cbHandles);
		for (unsigned i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(outCBs[i].object);
			Assert (handle);
			handle->internalHandle = cbHandles[i];
		}
	}
}

void GfxDeviceClient::DestroyComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs)
{
	Assert (count <= kMaxSupportedConstantBuffers);

	CheckMainThread();

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DestroyComputeConstantBuffers);
		m_CommandQueue->WriteValueType<unsigned>(count);
		for (unsigned i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(cbs[i].object);
			m_CommandQueue->WriteValueType<ClientDeviceConstantBuffer*>(handle);
		}
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
		for (unsigned i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(cbs[i].object);
			if (handle)
				cbHandles[i] = handle->internalHandle;

			UNITY_DELETE (handle, kMemGfxThread);
		}
		m_RealDevice->DestroyComputeConstantBuffers (count, cbHandles);
	}

	for (unsigned i = 0; i < count; ++i)
		cbs[i].Reset();
}

void GfxDeviceClient::CreateComputeBuffer (ComputeBufferID id, size_t count, size_t stride, UInt32 flags)
{
	CheckMainThread();
	DebugAssert(!IsRecording());
	Assert (id.IsValid());
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateComputeBuffer);
		m_CommandQueue->WriteValueType<ComputeBufferID>(id);
		m_CommandQueue->WriteValueType<size_t>(count);
		m_CommandQueue->WriteValueType<size_t>(stride);
		m_CommandQueue->WriteValueType<UInt32>(flags);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->CreateComputeBuffer (id, count, stride, flags);
}

void GfxDeviceClient::DestroyComputeBuffer (ComputeBufferID handle)
{
	if (!handle.IsValid())
		return;

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DestroyComputeBuffer);
		m_CommandQueue->WriteValueType<ComputeBufferID>(handle);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DestroyComputeBuffer (handle);
}

void GfxDeviceClient::UpdateComputeConstantBuffers (unsigned count, ConstantBufferHandle* cbs, UInt32 cbDirty, size_t dataSize, const UInt8* data, const UInt32* cbSizes, const UInt32* cbOffsets, const int* bindPoints)
{
	CheckMainThread();

	if (!count)
	{
		DebugAssert (dataSize == 0);
		return;
	}
	DebugAssert (dataSize != 0);

	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UpdateComputeConstantBuffers);
		m_CommandQueue->WriteValueType<unsigned>(count);
		m_CommandQueue->WriteValueType<UInt32>(cbDirty);
		for (int i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(cbs[i].object);
			m_CommandQueue->WriteValueType<ClientDeviceConstantBuffer*>(handle);
			m_CommandQueue->WriteValueType<UInt32>(cbSizes[i]);
			m_CommandQueue->WriteValueType<UInt32>(cbOffsets[i]);
			m_CommandQueue->WriteValueType<int>(bindPoints[i]);
		}
		m_CommandQueue->WriteValueType<size_t>(dataSize);
		m_CommandQueue->WriteStreamingData(data, dataSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
		for (unsigned i = 0; i < count; ++i)
		{
			ClientDeviceConstantBuffer* handle = static_cast<ClientDeviceConstantBuffer*>(cbs[i].object);
			if (handle)
				cbHandles[i] = handle->internalHandle;
		}
		m_RealDevice->UpdateComputeConstantBuffers (count, cbHandles, cbDirty, dataSize, data, cbSizes, cbOffsets, bindPoints);
	}
}

void GfxDeviceClient::UpdateComputeResources (
							 unsigned texCount, const TextureID* textures, const int* texBindPoints,
							 unsigned samplerCount, const unsigned* samplers,
							 unsigned inBufferCount, const ComputeBufferID* inBuffers, const int* inBufferBindPoints,
							 unsigned outBufferCount, const ComputeBufferID* outBuffers, const TextureID* outTextures, const UInt32* outBufferBindPoints)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_UpdateComputeResources);
		m_CommandQueue->WriteValueType<unsigned>(texCount);
		for (int i = 0; i < texCount; ++i)
		{
			m_CommandQueue->WriteValueType<TextureID>(textures[i]);
			m_CommandQueue->WriteValueType<int>(texBindPoints[i]);
		}
		m_CommandQueue->WriteValueType<unsigned>(samplerCount);
		for (int i = 0; i < samplerCount; ++i)
		{
			m_CommandQueue->WriteValueType<unsigned>(samplers[i]);
		}
		m_CommandQueue->WriteValueType<unsigned>(inBufferCount);
		for (int i = 0; i < inBufferCount; ++i)
		{
			m_CommandQueue->WriteValueType<ComputeBufferID>(inBuffers[i]);
			m_CommandQueue->WriteValueType<int>(inBufferBindPoints[i]);
		}
		m_CommandQueue->WriteValueType<unsigned>(outBufferCount);
		for (int i = 0; i < outBufferCount; ++i)
		{
			m_CommandQueue->WriteValueType<ComputeBufferID>(outBuffers[i]);
			m_CommandQueue->WriteValueType<TextureID>(outTextures[i]);
			m_CommandQueue->WriteValueType<UInt32>(outBufferBindPoints[i]);
		}
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->UpdateComputeResources (texCount, textures, texBindPoints, samplerCount, samplers, inBufferCount, inBuffers, inBufferBindPoints, outBufferCount, outBuffers, outTextures, outBufferBindPoints);
}

void GfxDeviceClient::DispatchComputeProgram (ComputeProgramHandle cpHandle, unsigned threadsX, unsigned threadsY, unsigned threadsZ)
{
	CheckMainThread();

	ClientDeviceComputeProgram* cp = static_cast<ClientDeviceComputeProgram*>(cpHandle.object);
	if (!cp)
		return;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DispatchComputeProgram);
		m_CommandQueue->WriteValueType<ClientDeviceComputeProgram*>(cp);
		m_CommandQueue->WriteValueType<unsigned>(threadsX);
		m_CommandQueue->WriteValueType<unsigned>(threadsY);
		m_CommandQueue->WriteValueType<unsigned>(threadsZ);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		m_RealDevice->DispatchComputeProgram (cp->internalHandle, threadsX, threadsY, threadsZ);
	}
}

void GfxDeviceClient::DrawNullGeometry (GfxPrimitiveType topology, int vertexCount, int instanceCount)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DrawNullGeometry);
		m_CommandQueue->WriteValueType<GfxPrimitiveType>(topology);
		m_CommandQueue->WriteValueType<int>(vertexCount);
		m_CommandQueue->WriteValueType<int>(instanceCount);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DrawNullGeometry (topology, vertexCount, instanceCount);
}

void GfxDeviceClient::DrawNullGeometryIndirect (GfxPrimitiveType topology, ComputeBufferID bufferHandle, UInt32 bufferOffset)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DrawNullGeometryIndirect);
		m_CommandQueue->WriteValueType<GfxPrimitiveType>(topology);
		m_CommandQueue->WriteValueType<ComputeBufferID>(bufferHandle);
		m_CommandQueue->WriteValueType<UInt32>(bufferOffset);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DrawNullGeometryIndirect (topology, bufferHandle, bufferOffset);
}



#if ENABLE_PROFILER

void GfxDeviceClient::BeginProfileEvent (const char* name)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginProfileEvent);
		m_CommandQueue->WriteValueType<const char*>(name); // assuming the pointer doesn't possibly go away!
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginProfileEvent (name);
}
void GfxDeviceClient::EndProfileEvent ()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndProfileEvent);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndProfileEvent ();
}

void GfxDeviceClient::ProfileControl (GfxProfileControl ctrl, unsigned param)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_ProfileControl);
		m_CommandQueue->WriteValueType<GfxProfileControl>(ctrl);
		m_CommandQueue->WriteValueType<unsigned>(param);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->ProfileControl (ctrl, param);
}


GfxTimerQuery* GfxDeviceClient::CreateTimerQuery()
{
	CheckMainThread();
	Assert(!IsRecording());
	return new ThreadedTimerQuery(*this);
}

void GfxDeviceClient::DeleteTimerQuery(GfxTimerQuery* query)
{
	CheckMainThread();
	Assert(!IsRecording());
	delete query;
}

void GfxDeviceClient::BeginTimerQueries()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginTimerQueries);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->BeginTimerQueries();
}

void GfxDeviceClient::EndTimerQueries()
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndTimerQueries);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->EndTimerQueries();
}

#endif

// Editor-only stuff
#if UNITY_EDITOR

void GfxDeviceClient::SetAntiAliasFlag( bool aa )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetAntiAliasFlag);
		m_CommandQueue->WriteValueType<GfxCmdBool>(aa);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->SetAntiAliasFlag(aa);
}


void GfxDeviceClient::DrawUserPrimitives( GfxPrimitiveType type, int vertexCount, UInt32 vertexChannels, const void* data, int stride )
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_DrawUserPrimitives);
		GfxCmdDrawUserPrimitives user = { type, vertexCount, vertexChannels, stride };
		m_CommandQueue->WriteValueType<GfxCmdDrawUserPrimitives>(user);
		m_CommandQueue->WriteStreamingData(data, vertexCount * stride);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		m_RealDevice->DrawUserPrimitives(type, vertexCount, vertexChannels, data, stride);
}

int GfxDeviceClient::GetCurrentTargetAA() const
{
#if UNITY_WIN
	return ThreadedWindow::GetCurrentFSAALevel();
#else
	return 1; // fix this
#endif
}

#endif

#if UNITY_EDITOR && UNITY_WIN
//ToDo: This is windows specific code, we should replace HWND window with something more abstract
GfxDeviceWindow* GfxDeviceClient::CreateGfxWindow( HWND window, int width, int height, DepthBufferFormat depthFormat, int antiAlias )
{
	CheckMainThread();
	ThreadedWindow* result = new ThreadedWindow(window, width, height, depthFormat, antiAlias);
	ClientDeviceWindow* handle = result->m_ClientWindow;
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_CreateWindow);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		GfxCmdCreateWindow create = { window, width, height, depthFormat, antiAlias };
		m_CommandQueue->WriteValueType<GfxCmdCreateWindow>(create);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		handle->internalWindow = m_RealDevice->CreateGfxWindow(window, width, height, depthFormat, antiAlias);
	}

	return result;
}

void GfxDeviceClient::SetActiveWindow(ClientDeviceWindow* handle)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_SetActiveWindow);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		handle->GetInternal()->SetAsActiveWindow();
}

void GfxDeviceClient::WindowReshape(ClientDeviceWindow* handle, int width, int height, DepthBufferFormat depthFormat, int antiAlias)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_WindowReshape);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		GfxCmdWindowReshape reshape = { width, height, depthFormat, antiAlias };
		m_CommandQueue->WriteValueType<GfxCmdWindowReshape>(reshape);
		SubmitCommands();
		WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		handle->GetInternal()->Reshape(width, height, depthFormat, antiAlias);
}

void GfxDeviceClient::WindowDestroy(ClientDeviceWindow* handle)
{
	CheckMainThread();
	if (m_Serialize)
	{
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_WindowDestroy);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		SubmitCommands();
		WaitForSignal();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		delete handle->GetInternal();
}

void GfxDeviceClient::BeginRendering(ClientDeviceWindow* handle)
{
	CheckMainThread();
	m_InsideFrame = true;
	if (m_Serialize)
	{
		WaitForPendingPresent();
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_BeginRendering);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		handle->GetInternal()->BeginRendering();
}

void GfxDeviceClient::EndRendering(ClientDeviceWindow* handle, bool presentContent)
{
	CheckMainThread();
	m_InsideFrame = false;
	if (m_Serialize)
	{
		// Check that we waited on event before issuing a new one
		bool signalEvent = !m_PresentPending;
		m_PresentPending = true;
		m_CommandQueue->WriteValueType<GfxCommand>(kGfxCmd_EndRendering);
		m_CommandQueue->WriteValueType<ClientDeviceWindow*>(handle);
		m_CommandQueue->WriteValueType<GfxCmdBool>(presentContent);
		m_CommandQueue->WriteValueType<GfxCmdBool>(signalEvent);
		SubmitCommands();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
		handle->GetInternal()->EndRendering(presentContent);
}

#endif

#if UNITY_WIN
int GfxDeviceClient::GetCurrentTargetWidth() const
{
	return m_CurrentTargetWidth;
}
int GfxDeviceClient::GetCurrentTargetHeight() const
{
	return m_CurrentTargetHeight;
}

void GfxDeviceClient::SetCurrentTargetSize(int width, int height)
{
	m_CurrentTargetWidth = width;
	m_CurrentTargetHeight = height;
}

void GfxDeviceClient::SetCurrentWindowSize(int width, int height)
{
	m_CurrentWindowWidth = m_CurrentTargetWidth = width;
	m_CurrentWindowHeight = m_CurrentTargetHeight = height;
}
#endif

void GfxDeviceClient::SetRealGfxDevice(GfxThreadableDevice* realDevice)
{
	if (realDevice)
	{
	m_RealDevice = realDevice;
	m_Renderer = realDevice->GetRenderer();
	m_UsesOpenGLTextureCoords = realDevice->UsesOpenGLTextureCoords();
	m_UsesHalfTexelOffset = realDevice->UsesHalfTexelOffset();
	m_MaxBufferedFrames = realDevice->GetMaxBufferedFrames();
	m_FramebufferDepthFormat = realDevice->GetFramebufferDepthFormat();
#if UNITY_WIN
	m_CurrentTargetWidth = realDevice->GetCurrentTargetWidth();
	m_CurrentTargetHeight = realDevice->GetCurrentTargetHeight();
	m_CurrentWindowWidth = m_CurrentTargetWidth;
	m_CurrentWindowHeight = m_CurrentTargetHeight;
#endif
	}
	else
	{
		m_RealDevice = NULL;
		m_Renderer = kGfxRendererOpenGL;
		m_UsesOpenGLTextureCoords = true;
		m_UsesHalfTexelOffset = false;
		m_MaxBufferedFrames = 1;
	}
}

void GfxDeviceClient::UpdateFogDisabled()
{
	m_FogParams.mode = kFogDisabled;
}

void GfxDeviceClient::UpdateFogEnabled(const GfxFogParams& fogParams)
{
	m_FogParams = fogParams;
}

void GfxDeviceClient::UpdateShadersActive(bool shadersActive[kShaderTypeCount])
{
	memcpy(m_CurrentContext->shadersActive, shadersActive, sizeof(bool[kShaderTypeCount]));
}

void GfxDeviceClient::CheckMainThread() const
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
}

void GfxDeviceClient::WriteBufferData(const void* data, int size)
{
	int maxNonStreamedSize = m_CommandQueue->GetAllocatedSize() / 2;
	if (size <= maxNonStreamedSize || IsRecording())
	{
		// In the NaCl Web Player, make sure that only complete commands are submitted, as we are not truely
		// asynchronous.
	#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
		SubmitCommands();
	#endif
		void* dest = m_CommandQueue->GetWriteDataPointer(size, ThreadedStreamBuffer::kDefaultAlignment);
		memcpy(dest, data, size);
		SubmitCommands();
	}
	else
		m_CommandQueue->WriteStreamingData(data, size);
}

void GfxDeviceClient::ReadbackData(dynamic_array<UInt8>& data, const SInt32 chunkSize)
{
	data.resize_uninitialized(0);
	m_CommandQueue->WriteValueType<SInt32>(chunkSize);
	for (;;)
	{
		volatile void* chunkPtr = m_CommandQueue->GetWriteDataPointer(sizeof(SInt32) + chunkSize, ThreadedStreamBuffer::kDefaultAlignment);
		volatile SInt32* returnedSizePtr = static_cast<volatile SInt32*>(chunkPtr);
		volatile void* returnedDataPtr = &returnedSizePtr[1];
		*returnedSizePtr = -1;
		m_CommandQueue->WriteSubmitData();
		while (*returnedSizePtr == -1)
		{
			WaitForSignal();
			// Busy wait
		}
		SInt32 size = *returnedSizePtr;
		UnityMemoryBarrier();
		if (size > 0 && size <= chunkSize)
		{
			size_t oldSize = data.size();
			data.resize_uninitialized(oldSize + size);
			// Const_cast needed to cast away volatile
			memcpy(&data[oldSize], const_cast<const void*>(returnedDataPtr), size);
		}
		if (size < chunkSize)
			break;
	}
}

void GfxDeviceClient::SubmitCommands()
{
	m_CommandQueue->WriteSubmitData();
}

void GfxDeviceClient::DoLockstep()
{
	if (!IsRecording())
	{
		SubmitCommands();
		GetGfxDeviceWorker()->LockstepWait();
	}
}

void GfxDeviceClient::WaitForPendingPresent()
{
	if (m_PresentPending)
	{
		PROFILER_AUTO(gGfxWaitForPresentProf, NULL);

		// We must wait for the last Present() to finish to figure out if device was lost.
		// Beginning a new frame on a lost device will cause D3D debug runtime to complain.
		// We may also lose any resources we upload on the render thread after seeing D3DERR_DEVICELOST.
		m_DeviceWorker->WaitForEvent(GfxDeviceWorker::kEventTypePresent);
		m_PresentPending = false;
	}
}

RenderTextureFormat GfxDeviceClient::GetDefaultRTFormat() const
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	return kRTFormatARGB32;
#else
	return m_RealDevice->GetDefaultRTFormat();
#endif
}

RenderTextureFormat GfxDeviceClient::GetDefaultHDRRTFormat() const
{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	return kRTFormatARGBHalf;
#else
	return  m_RealDevice->GetDefaultHDRRTFormat();;
#endif
}

#if GFX_DEVICE_CLIENT_TRACK_TEXGEN
	void GfxDeviceClient::SetTexGen(int unit, TexGenMode mode)
	{
		bool posNeeded = (mode == kTexGenObject || mode == kTexGenEyeLinear);
		bool nrmNeeded = (mode == kTexGenSphereMap || mode == kTexGenCubeReflect || mode == kTexGenCubeNormal);

		if(posNeeded)	posForTexGen |= (1<<unit);
		else			posForTexGen &= ~(1<<unit);
		if(nrmNeeded)	nrmForTexGen |= (1<<unit);
		else			nrmForTexGen &= ~(1<<unit);
	}
	void GfxDeviceClient::DropTexGen(int unit)
	{
		posForTexGen &= ~(1<<unit);
		nrmForTexGen &= ~(1<<unit);
	}
#endif

#endif //ENABLE_MULTITHREADED_CODE
