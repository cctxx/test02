#include "UnityPrefix.h"

#if ENABLE_MULTITHREADED_CODE

#include "Runtime/GfxDevice/threaded/GfxDeviceWorker.h"
#include "Runtime/GfxDevice/threaded/GfxCommands.h"
#include "Runtime/GfxDevice/threaded/GfxReturnStructs.h"
#include "Runtime/GfxDevice/threaded/ThreadedDeviceStates.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Threads/ThreadUtility.h"
#include "Runtime/GfxDevice/GfxTimerQuery.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Runtime/GfxDevice/GPUSkinningInfo.h"
#include "Runtime/GfxDevice/threaded/ThreadedWindow.h"
#include "Runtime/GfxDevice/threaded/ThreadedDisplayList.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "Runtime/Profiler/Profiler.h"
#include "External/shaderlab/Library/program.h"
#include "External/shaderlab/Library/properties.h"
#include "External/shaderlab/Library/TextureBinding.h"

#if GFXDEVICE_USE_CACHED_STATE
	#define CHECK_CACHED_STATE(x) if (x)
	#define SET_CACHED_STATE(dst, src) dst = src;
#else
	#define CHECK_CACHED_STATE(x)
	#define SET_CACHED_STATE(dst, src)
#endif

#if UNITY_XENON
	#include "PlatformDependent/Xbox360/Source/Services/VideoPlayer.h"
	#include "PlatformDependent/Xbox360/Source/GfxDevice/GfxXenonVBO.h"
	#include "PlatformDependent/Xbox360/Source/GfxDevice/GfxDeviceXenon.h"
	#define GFX_DEVICE_WORKER_PROCESSOR 2
#else
	#define GFX_DEVICE_WORKER_PROCESSOR DEFAULT_UNITY_THREAD_PROCESSOR
#endif

PROFILER_INFORMATION(gMTDrawProf, "Gfx.Draw", kProfilerRender)
PROFILER_INFORMATION(gMTDrawDynamicProf, "Gfx.DrawDynamic", kProfilerRender)
PROFILER_INFORMATION(gMTDrawStaticBatch, "Gfx.DrawStaticBatch", kProfilerRender)
PROFILER_INFORMATION(gMTDrawDynamicBatch, "Gfx.DrawDynamicBatch", kProfilerRender)

PROFILER_INFORMATION(gMTSetRT, "Gfx.SetRenderTarget", kProfilerRender)
PROFILER_INFORMATION(gMTPresentFrame, "Gfx.PresentFrame", kProfilerRender)
PROFILER_INFORMATION(gMTBeginQueriesProf, "GPUProfiler.BeginQueries", kProfilerOverhead)
PROFILER_INFORMATION(gMTEndQueriesProf, "GPUProfiler.EndQueries", kProfilerOverhead)


#if GFXDEVICE_USE_CACHED_STATE
GfxDeviceWorker::CachedState::CachedState()
{
	normalization = kNormalizationUnknown;
	backface = -1;
	ambient = Vector4f(-1, -1, -1, -1);
	fogEnabled = -1;
	fogParams.Invalidate();
}
#endif

GfxDeviceWorker::GfxDeviceWorker(int maxCallDepth, ThreadedStreamBuffer* commandQueue) :
	m_CallDepth(0),
	m_MaxCallDepth(maxCallDepth),
	m_WorkerThread(NULL),
	m_CommandQueue(commandQueue),
	m_MainCommandQueue(commandQueue),
	m_CurrentCPUFence(0),
	m_PresentFrameID(0),
	m_IsThreadOwner(true),
	m_Quit(false)
{
	m_FrameStats.ResetFrame();
	m_PlaybackCommandQueues = new ThreadedStreamBuffer[maxCallDepth];
	m_PlaybackDisplayLists = new ThreadedDisplayList*[maxCallDepth];
	memset(m_PlaybackDisplayLists, 0, sizeof(m_PlaybackDisplayLists[0]) * maxCallDepth);
	// Event starts signaled so it doesn't block immediately
	SignalEvent(kEventTypeTimerQueries);
}

GfxDeviceWorker::~GfxDeviceWorker()
{
	if (m_WorkerThread)
	{
		m_WorkerThread->WaitForExit();
		delete m_WorkerThread;
	}
	SetRealGfxDeviceThreadOwnership();
	DestroyRealGfxDevice();
	delete[] m_PlaybackCommandQueues;
	delete[] m_PlaybackDisplayLists;
}


GfxThreadableDevice* GfxDeviceWorker::Startup(GfxDeviceRenderer renderer, bool threaded, bool forceRef)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	Assert (IsThreadableGfxDevice(renderer));

	// Create actual device
	GfxDevice* dev = CreateRealGfxDevice (renderer, forceRef);
	if (!dev)
		return NULL;
	
	m_Device = static_cast<GfxThreadableDevice*>(dev);
	SetRealGfxDevice(dev);

	// Create worker thread
	if (threaded)
	{
		m_WorkerThread = new Thread();
		m_WorkerThread->SetName ("UnityGfxDeviceWorker");
		m_Device->ReleaseThreadOwnership();
		m_WorkerThread->Run(GfxDeviceWorker::RunGfxDeviceWorker, this, DEFAULT_UNITY_THREAD_STACK_SIZE, GFX_DEVICE_WORKER_PROCESSOR);

		// In D3D11 we don't want to block on Present(), instead we block on IsReadyToBeginFrame()
		if (renderer == kGfxRendererD3D11)
			SignalEvent(kEventTypePresent);
	}

	return m_Device;
#else
	return NULL;
#endif
}

void GfxDeviceWorker::Signal()
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	m_WaitSemaphore.Signal();
#else
	UnityMemoryBarrier();
#endif
}

void GfxDeviceWorker::WaitForSignal()
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	m_WaitSemaphore.WaitForSignal();
#endif
}

void GfxDeviceWorker::LockstepWait()
{
	m_LockstepSemaphore.WaitForSignal();
}

void GfxDeviceWorker::GetLastFrameStats(GfxDeviceStats& stats)
{
	Mutex::AutoLock lock(m_StatsMutex);
	stats.CopyAllDrawStats(m_FrameStats);
}

void GfxDeviceWorker::CallImmediate(ThreadedDisplayList* dlist)
{
	Assert(m_CallDepth == 0);
	dlist->AddRef();
	m_PlaybackDisplayLists[m_CallDepth] = dlist;
	m_CommandQueue = &m_PlaybackCommandQueues[m_CallDepth];
	m_CommandQueue->CreateReadOnly(dlist->GetData(), dlist->GetSize());
	m_CallDepth++;
	while (m_CallDepth > 0)
	{
		RunCommand(*m_CommandQueue);
	}
}

void GfxDeviceWorker::WaitForEvent(EventType type)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	m_EventSemaphores[type].WaitForSignal();
#endif
}

void GfxDeviceWorker::SignalEvent(EventType type)
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	m_EventSemaphores[type].Signal();
#endif
}

void GfxDeviceWorker::WaitOnCPUFence(UInt32 fence)
{
	while (SInt32(fence - m_CurrentCPUFence) > 0)
	{
		Thread::Sleep(0.001);
	}
}

bool GfxDeviceWorker::DidPresentFrame(UInt32 frameID) const
{
	return m_PresentFrameID == frameID;
}

void* GfxDeviceWorker::RunGfxDeviceWorker(void* data)
{
	#if ENABLE_PROFILER
	profiler_initialize_thread ("Render Thread", true);
	#endif
	
	GfxDeviceWorker* worker = (GfxDeviceWorker*) data;
	worker->m_Device->AcquireThreadOwnership();
	SetRealGfxDeviceThreadOwnership();
	worker->m_Device->OnDeviceCreated (true);

	worker->Run();
	
	#if ENABLE_PROFILER
	profiler_cleanup_thread();
	#endif
	
	return NULL;
}

// Soft-toggle for spin-loop
bool g_enableGfxDeviceWorkerSpinLoop = false;

GfxCommand lastCmd;
void GfxDeviceWorker::Run()
{

#define SPIN_WORKER_LOOP UNITY_XENON
#if SPIN_WORKER_LOOP
	while (!m_Quit)
	{
		if (g_enableGfxDeviceWorkerSpinLoop == false || m_CommandQueue->HasDataToRead())
		{
			RunCommand(*m_CommandQueue);
		}
		else
		{
			const bool presented = IsRealGfxDeviceThreadOwner() ? GetGfxDeviceX360().PresentFrameForTCR022() : false;
			if (!presented)
				Thread::Sleep(0.001);
		}
	}
#else
	while (!m_Quit)
	{
		RunCommand(*m_CommandQueue);
	}
#endif
}

bool GfxDeviceWorker::RunCommandIfDataIsAvailable()
{
	if (!m_CommandQueue->HasData())
		return false;

	RunCommand(*m_CommandQueue);
	return true;
}

void GfxDeviceWorker::RunCommand(ThreadedStreamBuffer& stream)
{
#if DEBUG_GFXDEVICE_LOCKSTEP
	size_t pos = stream.GetDebugReadPosition();
#endif
	GfxCommand cmd = stream.ReadValueType<GfxCommand>();
	DebugAssert(m_IsThreadOwner || cmd == kGfxCmd_AcquireThreadOwnership);
	switch (cmd)
	{
		case kGfxCmd_InvalidateState:
		{
			m_Device->InvalidateState();
			break;
		}
#if GFX_DEVICE_VERIFY_ENABLE
		case kGfxCmd_VerifyState:
		{
			m_Device->VerifyState();
			break;
		}
#endif
		case kGfxCmd_SetMaxBufferedFrames:
		{
			int bufferSize = stream.ReadValueType<int>();
			m_Device->SetMaxBufferedFrames(bufferSize);
			break;
		}
		case kGfxCmd_Clear:
		{
			const GfxCmdClear& clear = stream.ReadValueType<GfxCmdClear>();
			m_Device->Clear(clear.clearFlags, clear.color.GetPtr(), clear.depth, clear.stencil);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetUserBackfaceMode:
		{
			bool enable = stream.ReadValueType<GfxCmdBool>();
			m_Device->SetUserBackfaceMode(enable);
			break;
		}
		case kGfxCmd_SetWireframe:
		{
			bool wire = stream.ReadValueType<GfxCmdBool>();
			m_Device->SetWireframe(wire);
			break;
		}
		case kGfxCmd_SetInvertProjectionMatrix:
		{
			bool enable = stream.ReadValueType<GfxCmdBool>();
			m_Device->SetInvertProjectionMatrix(enable);
			break;
		}
#if GFX_USES_VIEWPORT_OFFSET
		case kGfxCmd_SetViewportOffset:
		{
			Vector2f ofs = stream.ReadValueType<Vector2f>();
			m_Device->SetViewportOffset(ofs.x, ofs.y);
			break;
		}
#endif
		case kGfxCmd_CreateBlendState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceBlendState result = stream.ReadValueType<ClientDeviceBlendState>();
			m_BlendStateMapper[result.internalState] = m_Device->CreateBlendState(result.sourceState);
#else
			ClientDeviceBlendState* result = stream.ReadValueType<ClientDeviceBlendState*>();
			result->internalState = m_Device->CreateBlendState(result->sourceState);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateDepthState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceDepthState result = stream.ReadValueType<ClientDeviceDepthState>();
			m_DepthStateMapper[result.internalState] = m_Device->CreateDepthState(result.sourceState);
#else
			ClientDeviceDepthState* result = stream.ReadValueType<ClientDeviceDepthState*>();
			result->internalState = m_Device->CreateDepthState(result->sourceState);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateStencilState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceStencilState result = stream.ReadValueType<ClientDeviceStencilState>();
			m_StencilStateMapper[result.internalState] = m_Device->CreateStencilState(result.sourceState);
#else
			ClientDeviceStencilState* result = stream.ReadValueType<ClientDeviceStencilState*>();
			result->internalState = m_Device->CreateStencilState(result->sourceState);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateRasterState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRasterState result = stream.ReadValueType<ClientDeviceRasterState>();
			m_RasterStateMapper[result.internalState] = m_Device->CreateRasterState(result.sourceState);
#else
			ClientDeviceRasterState* result = stream.ReadValueType<ClientDeviceRasterState*>();
			result->internalState = m_Device->CreateRasterState(result->sourceState);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetBlendState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const ClientDeviceBlendState result = stream.ReadValueType<const ClientDeviceBlendState>();
			float alphaRef = stream.ReadValueType<float>();
			m_Device->SetBlendState(m_BlendStateMapper[result.internalState], alphaRef);
#else
			const ClientDeviceBlendState* result = stream.ReadValueType<const ClientDeviceBlendState*>();
			float alphaRef = stream.ReadValueType<float>();
			m_Device->SetBlendState(result->internalState, alphaRef);
#endif
			break;
		}
		case kGfxCmd_SetDepthState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const ClientDeviceDepthState result = stream.ReadValueType<const ClientDeviceDepthState>();
			m_Device->SetDepthState(m_DepthStateMapper[result.internalState]);
#else
			const ClientDeviceDepthState* result = stream.ReadValueType<const ClientDeviceDepthState*>();
			m_Device->SetDepthState(result->internalState);
#endif
			break;
		}
		case kGfxCmd_SetStencilState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const ClientDeviceStencilState result = stream.ReadValueType<const ClientDeviceStencilState>();
			int stencilRef = stream.ReadValueType<int>();
			m_Device->SetStencilState(m_StencilStateMapper[result.internalState], stencilRef);
#else
			const ClientDeviceStencilState* result = stream.ReadValueType<const ClientDeviceStencilState*>();
			int stencilRef = stream.ReadValueType<int>();
			m_Device->SetStencilState(result->internalState, stencilRef);
#endif
			break;
		}
		case kGfxCmd_SetRasterState:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const ClientDeviceRasterState result = stream.ReadValueType<const ClientDeviceRasterState>();
			m_Device->SetRasterState(m_RasterStateMapper[result.internalState]);
#else
			const ClientDeviceRasterState* result = stream.ReadValueType<const ClientDeviceRasterState*>();
			m_Device->SetRasterState(result->internalState);
#endif
			break;
		}
		case kGfxCmd_SetSRGBState:
		{
			bool enable = stream.ReadValueType<GfxCmdBool>();
			m_Device->SetSRGBWrite(enable);
			break;
		}
		case kGfxCmd_SetWorldMatrix:
		{
			const Matrix4x4f& matrix = stream.ReadValueType<Matrix4x4f>();
			m_Device->SetWorldMatrix(matrix.GetPtr());
			break;
		}
		case kGfxCmd_SetViewMatrix:
		{
			const Matrix4x4f& matrix = stream.ReadValueType<Matrix4x4f>();
			m_Device->SetViewMatrix(matrix.GetPtr());
			break;
		}
		case kGfxCmd_SetProjectionMatrix:
		{
			const Matrix4x4f& matrix = stream.ReadValueType<Matrix4x4f>();
			m_Device->SetProjectionMatrix(matrix);
			break;
		}
		case kGfxCmd_SetInverseScale:
		{
			float invScale = stream.ReadValueType<float>();
			m_Device->SetInverseScale(invScale);
			break;
		}
		case kGfxCmd_SetNormalizationBackface:
		{
			const GfxCmdSetNormalizationBackface& data = stream.ReadValueType<GfxCmdSetNormalizationBackface>();
			CHECK_CACHED_STATE(data.mode != m_Cached.normalization || int(data.backface) != m_Cached.backface)
			{
				m_Device->SetNormalizationBackface(data.mode, data.backface);
				SET_CACHED_STATE(m_Cached.normalization, data.mode);
				SET_CACHED_STATE(m_Cached.backface, data.backface);
			}
			break;
		}
		case kGfxCmd_SetFFLighting:
		{
			const GfxCmdSetFFLighting& data = stream.ReadValueType<GfxCmdSetFFLighting>();
			m_Device->SetFFLighting(data.on, data.separateSpecular, data.colorMaterial);
			break;
		}
		case kGfxCmd_SetMaterial:
		{
			const GfxMaterialParams& mat = stream.ReadValueType<GfxMaterialParams>();
			m_Device->SetMaterial(mat.ambient.GetPtr(), mat.diffuse.GetPtr(), mat.specular.GetPtr(), mat.emissive.GetPtr(), mat.shininess);
			break;
		}
		case kGfxCmd_SetColor:
		{
			const Vector4f& color = stream.ReadValueType<Vector4f>();
			m_Device->SetColor(color.GetPtr());
			break;
		}
		case kGfxCmd_SetViewport:
		{
			const ClientDeviceRect& rect = stream.ReadValueType<ClientDeviceRect>();
			m_Device->SetViewport(rect.x, rect.y, rect.width, rect.height);
			break;
		}
		case kGfxCmd_SetScissorRect:
		{
			const ClientDeviceRect& rect = stream.ReadValueType<ClientDeviceRect>();
			m_Device->SetScissorRect(rect.x, rect.y, rect.width, rect.height);
			break;
		}
		case kGfxCmd_DisableScissor:
		{
			m_Device->DisableScissor();
			break;
		}
		case kGfxCmd_CreateTextureCombiners:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceTextureCombiners result;
			result.internalHandle = stream.ReadValueType<ClientIDMapper::ClientID>();
			result.count = stream.ReadValueType<int>();
			result.bindings = new ShaderLab::TextureBinding[result.count];
			for (int i = 0; i < result.count; i++)
				result.bindings[i] = stream.ReadValueType<ShaderLab::TextureBinding>();

			const GfxCmdCreateTextureCombiners& param = stream.ReadValueType<GfxCmdCreateTextureCombiners>();
			m_TextureCombinerMapper[result.internalHandle] = m_Device->CreateTextureCombiners(param.count, result.bindings, NULL, param.hasVertexColorOrLighting, param.usesAddSpecular).object;
#else
			ClientDeviceTextureCombiners* result = stream.ReadValueType<ClientDeviceTextureCombiners*>();
			const GfxCmdCreateTextureCombiners& param = stream.ReadValueType<GfxCmdCreateTextureCombiners>();
			result->internalHandle = m_Device->CreateTextureCombiners(param.count, result->bindings, NULL, param.hasVertexColorOrLighting, param.usesAddSpecular);
#endif
			break;
		}
		case kGfxCmd_DeleteTextureCombiners:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceTextureCombiners combiners;
			combiners.internalHandle = stream.ReadValueType<ClientIDMapper::ClientID>();
			combiners.count = stream.ReadValueType<int>();
			TextureCombinersHandle handle(m_TextureCombinerMapper[combiners.internalHandle]);
			m_Device->DeleteTextureCombiners(handle);
#else
			ClientDeviceTextureCombiners* combiners = stream.ReadValueType<ClientDeviceTextureCombiners*>();
			m_Device->DeleteTextureCombiners(combiners->internalHandle);
			for(int i = 0; i < combiners->count; i++) combiners->bindings[i].~TextureBinding();
			UNITY_FREE(kMemGfxThread,combiners->bindings);
			UNITY_DELETE(combiners, kMemGfxThread);
#endif
			break;
		}
		case kGfxCmd_SetTextureCombiners:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceTextureCombiners combiners;
			combiners.internalHandle = stream.ReadValueType<ClientIDMapper::ClientID>();
			combiners.count = stream.ReadValueType<int>();
			int count = combiners.count;
#else
			ClientDeviceTextureCombiners* combiners = stream.ReadValueType<ClientDeviceTextureCombiners*>();
			int count = combiners->count;
#endif
			const void* data = m_CommandQueue->GetReadDataPointer(count * sizeof(TexEnvData), ALIGN_OF(TexEnvData));
			const TexEnvData* texEnvData = static_cast<const TexEnvData*>(data);
			data = m_CommandQueue->GetReadDataPointer(count * sizeof(Vector4f), ALIGN_OF(Vector4f));
			const Vector4f* texColors = static_cast<const Vector4f*>(data);
			// CreateTextureCombiners() might have failed on render thread, unknownst to main thread (case 435703)
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			if (combiners.internalHandle)
				m_Device->SetTextureCombinersThreadable(TextureCombinersHandle(m_TextureCombinerMapper[combiners.internalHandle]), texEnvData, texColors);
#else
			if (combiners->internalHandle.IsValid())
				m_Device->SetTextureCombinersThreadable(combiners->internalHandle, texEnvData, texColors);
#endif
			break;
		}
		case kGfxCmd_SetTexture:
		{
			const GfxCmdSetTexture& tex = stream.ReadValueType<GfxCmdSetTexture>();
			m_Device->SetTexture(tex.shaderType, tex.unit, tex.samplerUnit, tex.texture, tex.dim, tex.bias);
			break;
		}
		case kGfxCmd_SetTextureParams:
		{
			const GfxCmdSetTextureParams& params = stream.ReadValueType<GfxCmdSetTextureParams>();
			m_Device->SetTextureParams( params.texture, params.texDim, params.filter, params.wrap, params.anisoLevel, params.hasMipMap, params.colorSpace);
			m_CommandQueue->ReadReleaseData();
			break;
		}
		case kGfxCmd_SetTextureTransform:
		{
			const GfxCmdSetTextureTransform& trans = stream.ReadValueType<GfxCmdSetTextureTransform>();
			m_Device->SetTextureTransform(trans.unit, trans.dim, trans.texGen, trans.identity, trans.matrix.GetPtr());
			break;
		}
		case kGfxCmd_SetTextureName:
		{
			const GfxCmdSetTextureName& texName = stream.ReadValueType<GfxCmdSetTextureName>();
			char* name = m_CommandQueue->ReadArrayType<char>(texName.nameLength);
			m_Device->SetTextureName( texName.texture, name );
			break;
		}
		case kGfxCmd_SetMaterialProperties:
		{
			typedef MaterialPropertyBlock::Property Property;
			GfxCmdSetMaterialProperties matprops = m_CommandQueue->ReadValueType<GfxCmdSetMaterialProperties>();
			Property* props = m_CommandQueue->ReadArrayType<Property>(matprops.propertyCount);
			float* buffer = m_CommandQueue->ReadArrayType<float>(matprops.bufferSize);
			MaterialPropertyBlock block(props, matprops.propertyCount, buffer, matprops.bufferSize);
			m_Device->SetMaterialProperties(block);
			break;
		}
		case kGfxCmd_CreateGpuProgram:
		{
 			#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
 				GfxCmdCreateGpuProgram gpuprog = m_CommandQueue->ReadValueType<GfxCmdCreateGpuProgram>();
 				*gpuprog.result = m_Device->CreateGpuProgram(gpuprog.source, *gpuprog.output);
 				UnityMemoryBarrier();
 				m_WaitSemaphore.Signal();
 				m_CommandQueue->ReadReleaseData();
 			#else
 				size_t len = m_CommandQueue->ReadValueType<UInt32>();
 				const char* source = m_CommandQueue->ReadArrayType<char>(len + 1);
 				CreateGpuProgramOutput output;
 				GpuProgram* program = m_Device->CreateGpuProgram(source, output);
 				Assert(program);
 				// Allocate space for struct, append additional data
 				m_TempBuffer.resize_uninitialized(sizeof(GfxRet_CreateGpuProgram));
 				GfxRet_CreateGpuProgram retStruct(output, m_TempBuffer);
 				retStruct.gpuProgram = m_GpuProgramClientMapper.CreateID();
				m_GpuProgramMapper[retStruct.gpuProgram] = program;
 				// Copy struct to start of buffer
 				*reinterpret_cast<GfxRet_CreateGpuProgram*>(m_TempBuffer.data()) = retStruct;
 				WritebackData(stream, m_TempBuffer.data(), m_TempBuffer.size());
 			#endif
			break;
		}
		case kGfxCmd_SetShaders:
		{
			GfxCmdSetShaders shaders = stream.ReadValueType<GfxCmdSetShaders>();
			UInt8* paramsBuffer[kShaderTypeCount];
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const GpuProgramParameters* params[kShaderTypeCount];
			GpuProgram* programs[kShaderTypeCount];
#endif
			for (int pt = 0; pt < kShaderTypeCount; ++pt)
			{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
				params[pt] = m_GpuProgramParametersMapper[shaders.params[pt]];
				programs[pt] = m_GpuProgramMapper[shaders.programs[pt]];
#endif

				paramsBuffer[pt] = NULL;
				if (shaders.paramsBufferSize[pt] > 0)
				{
					void* buffer = m_CommandQueue->GetReadDataPointer (shaders.paramsBufferSize[pt], 1);
					paramsBuffer[pt] = static_cast<UInt8*>(buffer);
				}
			}
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			m_Device->SetShadersThreadable (programs, params, paramsBuffer);
#else
			m_Device->SetShadersThreadable (shaders.programs, shaders.params, paramsBuffer);
#endif
			break;
		}
		case kGfxCmd_CreateShaderParameters:
		{
			GfxCmdCreateShaderParameters params = stream.ReadValueType<GfxCmdCreateShaderParameters>();
			m_Device->CreateShaderParameters(params.program, params.fogMode);
			Signal();
			break;
		}
		case kGfxCmd_DestroySubProgram:
		{
			ShaderLab::SubProgram* subprogram = stream.ReadValueType<ShaderLab::SubProgram*>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			m_Device->DestroySubProgram(subprogram);
#else
			printf_console("fix ShaderLab::SubProgram\n");
#endif
			break;
		}
		case kGfxCmd_SetConstantBufferInfo:
		{
			int id = stream.ReadValueType<int>();
			int size = stream.ReadValueType<int>();
			m_Device->SetConstantBufferInfo (id, size);
			break;
		}
		case kGfxCmd_DisableLights:
		{
			int startLight = stream.ReadValueType<int>();
			m_Device->DisableLights(startLight);
			break;
		}
		case kGfxCmd_SetLight:
		{
			int light = stream.ReadValueType<int>();
			const GfxVertexLight& lightdata = stream.ReadValueType<GfxVertexLight>();
			m_Device->SetLight(light, lightdata);
			break;
		}
		case kGfxCmd_SetAmbient:
		{
			const Vector4f& ambient = stream.ReadValueType<Vector4f>();
			CHECK_CACHED_STATE(!CompareMemory(ambient, m_Cached.ambient))
			{
				m_Device->SetAmbient(ambient.GetPtr());
				SET_CACHED_STATE(m_Cached.ambient, ambient);
			}
			break;
		}
		case kGfxCmd_EnableFog:
		{
			const GfxFogParams& fog = stream.ReadValueType<GfxFogParams>();
			CHECK_CACHED_STATE(m_Cached.fogEnabled != 1 || !CompareMemory(fog, m_Cached.fogParams))
			{
				m_Device->EnableFog(fog);
				SET_CACHED_STATE(m_Cached.fogEnabled, 1);
				SET_CACHED_STATE(m_Cached.fogParams, fog);
			}
			break;
		}
		case kGfxCmd_DisableFog:
		{
			CHECK_CACHED_STATE(m_Cached.fogEnabled != 0)
			{
				m_Device->DisableFog();
				SET_CACHED_STATE(m_Cached.fogEnabled, 0);
			}
			break;
		}
		case kGfxCmd_BeginSkinning:
		{
			int maxSkinCount = stream.ReadValueType<int>();
			m_SkinJobGroup = GetJobScheduler().BeginGroup(maxSkinCount);
			m_ActiveSkins.reserve(maxSkinCount);
			break;
		}
		case kGfxCmd_SkinMesh:
		{
			// Array must be preallocated to at least the right size
			Assert(m_ActiveSkins.size() < m_ActiveSkins.capacity());
			int skinIndex = m_ActiveSkins.size();
			m_ActiveSkins.resize_uninitialized(skinIndex + 1);
			SkinMeshInfo& skin = m_ActiveSkins[skinIndex];
			skin = stream.ReadValueType<SkinMeshInfo>();
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
			stream.ReadReleaseData();

#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			VertexStreamData mappedVSD;
			if (vbo->GetInternal()->MapVertexStream(mappedVSD, 0))
			{
				skin.outVertices = mappedVSD.buffer;
				GetJobScheduler().SubmitJob(m_SkinJobGroup, DeformSkinnedMeshJob, &skin, NULL);
				m_MappedSkinVBOs.push_back(vbo->GetInternal());
			}
#endif
			break;
		}
		case kGfxCmd_EndSkinning:
		{
			GetJobScheduler().WaitForGroup(m_SkinJobGroup);
			for (int i = 0; i < m_ActiveSkins.size(); i++)
			{
				m_ActiveSkins[i].Release();
			}
			m_ActiveSkins.resize_uninitialized(0);
			for (int i = 0; i < m_MappedSkinVBOs.size(); i++)
			{
				m_MappedSkinVBOs[i]->UnmapVertexStream(0);
			}
			m_MappedSkinVBOs.resize_uninitialized(0);
			break;
		}
#if GFX_ENABLE_DRAW_CALL_BATCHING
		case kGfxCmd_BeginStaticBatching:
		{
			PROFILER_BEGIN(gMTDrawStaticBatch, NULL);
			const GfxCmdBeginStaticBatching& dynbat = stream.ReadValueType<GfxCmdBeginStaticBatching>();
			m_Device->BeginStaticBatching(dynbat.channels, dynbat.topology);
			break;
		}
		case kGfxCmd_StaticBatchMesh:
		{
			const GfxCmdStaticBatchMesh& batch = stream.ReadValueType<GfxCmdStaticBatchMesh>();
			m_Device->StaticBatchMesh(batch.firstVertex, batch.vertexCount,
				batch.indices, batch.firstIndexByte, batch.indexCount);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_EndStaticBatching:
		{
			const GfxCmdEndStaticBatching& endbat = stream.ReadValueType<GfxCmdEndStaticBatching>();
			m_Device->EndStaticBatching(*endbat.vbo->GetInternal(), endbat.matrix, endbat.transformType, endbat.sourceChannels);
			stream.ReadReleaseData();
			PROFILER_END;
			break;
		}
		case kGfxCmd_BeginDynamicBatching:
		{
			PROFILER_BEGIN(gMTDrawDynamicBatch, NULL);
			const GfxCmdBeginDynamicBatching& dynbat = stream.ReadValueType<GfxCmdBeginDynamicBatching>();
			m_Device->BeginDynamicBatching(dynbat.shaderChannels, dynbat.channelsInVBO, dynbat.maxVertices, dynbat.maxIndices, dynbat.topology);
			break;
		}
		case kGfxCmd_DynamicBatchMesh:
		{
			const GfxCmdDynamicBatchMesh& batch = stream.ReadValueType<GfxCmdDynamicBatchMesh>();
			m_Device->DynamicBatchMesh(batch.matrix, batch.vertices, batch.firstVertex, batch.vertexCount,
				batch.indices, batch.firstIndexByte, batch.indexCount);
			stream.ReadReleaseData();
			break;
		}
#if ENABLE_SPRITES
		case kGfxCmd_DynamicBatchSprite:
		{
			const GfxCmdDynamicBatchSprite& batch = stream.ReadValueType<GfxCmdDynamicBatchSprite>();
			m_Device->DynamicBatchSprite(&batch.matrix, batch.sprite, batch.color);
			stream.ReadReleaseData();
			break;
		}
#endif
		case kGfxCmd_EndDynamicBatching:
		{
			const GfxCmdEndDynamicBatching& endbat = stream.ReadValueType<GfxCmdEndDynamicBatching>();
			m_Device->EndDynamicBatching(endbat.transformType);
			stream.ReadReleaseData();
			PROFILER_END;
			break;
		}
#endif
		case kGfxCmd_AddBatchingStats:
		{
			const GfxCmdAddBatchingStats& stats = stream.ReadValueType<GfxCmdAddBatchingStats>();
			m_Device->AddBatchingStats(stats.batchedTris, stats.batchedVerts, stats.batchedCalls);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateRenderColorSurface:
		{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* handle = stream.ReadValueType<ClientDeviceRenderSurface*>();
#else
			ClientDeviceRenderSurface handle = stream.ReadValueType<ClientDeviceRenderSurface>();
#endif
			const GfxCmdCreateRenderColorSurface& create = stream.ReadValueType<GfxCmdCreateRenderColorSurface>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			handle->internalHandle = m_Device->CreateRenderColorSurface(create.textureID, create.width, create.height, create.samples, create.depth, create.dim, create.format, create.createFlags);
#else
			m_RenderSurfaceMapper[handle.internalHandle] = m_Device->CreateRenderColorSurface(create.textureID, create.width, create.height, create.samples, create.depth, create.dim, create.format, create.createFlags).object;
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateRenderDepthSurface:
		{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* handle = stream.ReadValueType<ClientDeviceRenderSurface*>();
#else
			ClientDeviceRenderSurface handle = stream.ReadValueType<ClientDeviceRenderSurface>();
#endif
			const GfxCmdCreateRenderDepthSurface& create = stream.ReadValueType<GfxCmdCreateRenderDepthSurface>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			handle->internalHandle = m_Device->CreateRenderDepthSurface(create.textureID, create.width, create.height, create.samples, create.dim, create.depthFormat, create.createFlags);
#else
			m_RenderSurfaceMapper[handle.internalHandle] = m_Device->CreateRenderDepthSurface(create.textureID, create.width, create.height, create.samples, create.dim, create.depthFormat, create.createFlags).object;
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_DestroyRenderSurface:
		{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* handle = stream.ReadValueType<ClientDeviceRenderSurface*>();
			RenderSurfaceHandle rsHandle = handle->internalHandle;
#else
			ClientDeviceRenderSurface handle = stream.ReadValueType<ClientDeviceRenderSurface>();
			RenderSurfaceHandle rsHandle(m_RenderSurfaceMapper[handle.internalHandle]);
#endif
			m_Device->DestroyRenderSurface(rsHandle);
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			delete handle;
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_DiscardContents:
		{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* handle = stream.ReadValueType<ClientDeviceRenderSurface*>();
			RenderSurfaceHandle rsHandle = handle->internalHandle;
#else
			ClientDeviceRenderSurface handle = stream.ReadValueType<ClientDeviceRenderSurface>();
			RenderSurfaceHandle rsHandle(m_RenderSurfaceMapper[handle.internalHandle]);
#endif
			m_Device->DiscardContents(rsHandle);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetRenderTarget:
		{
			PROFILER_AUTO(gMTSetRT, NULL);
			const GfxCmdSetRenderTarget& srt = stream.ReadValueType<GfxCmdSetRenderTarget>();
			RenderSurfaceHandle colorHandle[kMaxSupportedRenderTargets];
			for (int i = 0; i < srt.colorCount; ++i)
			{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
				ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(srt.colorHandles[i].object);
				colorHandle[i].object = colorSurf ? colorSurf->internalHandle.object : NULL;
#else
				colorHandle[i].object = m_RenderSurfaceMapper[srt.colorHandles[i]];
#endif
				// we cant access backbuffer handle in client device, so we write null
				if(!colorHandle[i].IsValid())
					colorHandle[i] = m_Device->GetBackBufferColorSurface();
			}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)srt.depthHandle.object;
			RenderSurfaceHandle depthHandle(depthSurf ? depthSurf->internalHandle.object :NULL);
#else
			RenderSurfaceHandle depthHandle(m_RenderSurfaceMapper[srt.depthHandle]);
#endif
			// we cant access backbuffer handle in client device, so we write null
			if(!depthHandle.IsValid())
				depthHandle = m_Device->GetBackBufferDepthSurface();

			m_Device->SetRenderTargets (srt.colorCount, colorHandle, depthHandle, srt.mipLevel, srt.face);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetRenderTargetWithFlags:
		{
			PROFILER_AUTO(gMTSetRT, NULL);
			const GfxCmdSetRenderTarget& srt = stream.ReadValueType<GfxCmdSetRenderTarget>();
			RenderSurfaceHandle colorHandle[kMaxSupportedRenderTargets];
			for (int i = 0; i < srt.colorCount; ++i)
			{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
				ClientDeviceRenderSurface* colorSurf = static_cast<ClientDeviceRenderSurface*>(srt.colorHandles[i].object);
				colorHandle[i].object = colorSurf ? colorSurf->internalHandle.object : NULL;
#else
				colorHandle[i].object = m_RenderSurfaceMapper[srt.colorHandles[i]];
#endif
				// we cant access backbuffer handle in client device, so we write null
				if(!colorHandle[i].IsValid())
					colorHandle[i] = m_Device->GetBackBufferColorSurface();
			}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)srt.depthHandle.object;
			RenderSurfaceHandle depthHandle(depthSurf ? depthSurf->internalHandle.object :NULL);
#else
			RenderSurfaceHandle depthHandle(m_RenderSurfaceMapper[srt.depthHandle]);
#endif
			// we cant access backbuffer handle in client device, so we write null
			if(!depthHandle.IsValid())
				depthHandle = m_Device->GetBackBufferDepthSurface();

			UInt32 flags = stream.ReadValueType<UInt32>();
			m_Device->SetRenderTargets (srt.colorCount, colorHandle, depthHandle, srt.mipLevel, srt.face, flags);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ResolveColorSurface:
		{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* src = stream.ReadValueType<ClientDeviceRenderSurface*>();
			ClientDeviceRenderSurface* dst = stream.ReadValueType<ClientDeviceRenderSurface*>();
			m_Device->ResolveColorSurface (src->internalHandle, dst->internalHandle);
#else
			//todo
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ResolveDepthIntoTexture:
		{
			const GfxCmdResolveDepthIntoTexture resolve = stream.ReadValueType<GfxCmdResolveDepthIntoTexture>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* colorSurf = (ClientDeviceRenderSurface*)resolve.colorHandle.object;
			RenderSurfaceHandle colorHandle(colorSurf ? colorSurf->internalHandle.object : NULL);

			ClientDeviceRenderSurface* depthSurf = (ClientDeviceRenderSurface*)resolve.depthHandle.object;
			RenderSurfaceHandle depthHandle(depthSurf ? depthSurf->internalHandle.object : NULL);
#else
			RenderSurfaceHandle colorHandle(m_RenderSurfaceMapper[resolve.colorHandle]);
			RenderSurfaceHandle depthHandle(m_RenderSurfaceMapper[resolve.depthHandle]);
#endif
			m_Device->ResolveDepthIntoTexture (colorHandle, depthHandle);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetSurfaceFlags:
		{
			const GfxCmdSetSurfaceFlags& sf = stream.ReadValueType<GfxCmdSetSurfaceFlags>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* surface = (ClientDeviceRenderSurface*)sf.surf.object;
			RenderSurfaceHandle handle = surface->internalHandle;
#else
			RenderSurfaceHandle handle(m_RenderSurfaceMapper[sf.surf]);
#endif
			m_Device->SetSurfaceFlags(handle, sf.flags, sf.keepFlags);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_UploadTexture2D:
		{
			// Must copy since we stream data
			GfxCmdUploadTexture2D upload = stream.ReadValueType<GfxCmdUploadTexture2D>();
			UInt8* srcData = ReadBufferData(stream, upload.srcSize);
			m_Device->UploadTexture2D(upload.texture, upload.dimension, srcData, upload.srcSize,
				upload.width, upload.height, upload.format, upload.mipCount, upload.uploadFlags,
				upload.skipMipLevels, upload.usageMode, upload.colorSpace);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_UploadTextureSubData2D:
		{
			// Must copy since we stream data
			GfxCmdUploadTextureSubData2D upload = stream.ReadValueType<GfxCmdUploadTextureSubData2D>();
			UInt8* srcData = ReadBufferData(stream, upload.srcSize);
			m_Device->UploadTextureSubData2D(upload.texture, srcData, upload.srcSize,
				upload.mipLevel, upload.x, upload.y, upload.width, upload.height, upload.format, upload.colorSpace);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_UploadTextureCube:
		{
			// Must copy since we stream data
			GfxCmdUploadTextureCube upload = stream.ReadValueType<GfxCmdUploadTextureCube>();
			UInt8* srcData = ReadBufferData(stream, upload.srcSize);
			m_Device->UploadTextureCube(upload.texture, srcData, upload.srcSize,
				upload.faceDataSize, upload.size, upload.format, upload.mipCount, upload.uploadFlags, upload.colorSpace);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_UploadTexture3D:
		{
			// Must copy since we stream data
			GfxCmdUploadTexture3D upload = stream.ReadValueType<GfxCmdUploadTexture3D>();
			UInt8* srcData = ReadBufferData(stream, upload.srcSize);
			m_Device->UploadTexture3D(upload.texture, srcData, upload.srcSize,
				upload.width, upload.height, upload.depth, upload.format, upload.mipCount, upload.uploadFlags);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_DeleteTexture:
		{
			TextureID texture = stream.ReadValueType<TextureID>();
			m_Device->DeleteTexture(texture);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_BeginFrame:
		{
			m_Device->BeginFrame();
			break;
		}
		case kGfxCmd_EndFrame:
		{
			m_Device->EndFrame();
			break;
		}
		case kGfxCmd_PresentFrame:
		{
			PROFILER_AUTO(gMTPresentFrame, NULL);
			m_Device->PresentFrame();
			GfxCmdBool signalEvent = stream.ReadValueType<GfxCmdBool>();
			if (signalEvent)
				SignalEvent(kEventTypePresent);
			UnityMemoryBarrier();
			m_PresentFrameID = stream.ReadValueType<UInt32>();
			break;
		}
		case kGfxCmd_HandleInvalidState:
		{
			bool* success = stream.ReadValueType<bool*>();
			*success = m_Device->HandleInvalidState();
			Signal();
			break;
		}
		case kGfxCmd_FinishRendering:
		{
			m_Device->FinishRendering();
			Signal();
			break;
		}
		case kGfxCmd_InsertCPUFence:
		{
			stream.ReadReleaseData();
			m_CurrentCPUFence++;
			break;
		}
		case kGfxCmd_ImmediateVertex:
		{
			const GfxCmdVector3& data = stream.ReadValueType<GfxCmdVector3>();
			m_Device->ImmediateVertex(data.x, data.y, data.z);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ImmediateNormal:
		{
			const GfxCmdVector3& data = stream.ReadValueType<GfxCmdVector3>();
			m_Device->ImmediateNormal(data.x, data.y, data.z);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ImmediateColor:
		{
			const GfxCmdVector4& data = stream.ReadValueType<GfxCmdVector4>();
			m_Device->ImmediateColor(data.x, data.y, data.z, data.w);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ImmediateTexCoordAll:
		{
			const GfxCmdVector3& data = stream.ReadValueType<GfxCmdVector3>();
			m_Device->ImmediateTexCoordAll(data.x, data.y, data.z);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ImmediateTexCoord:
		{
			const GfxCmdImmediateTexCoord& data = stream.ReadValueType<GfxCmdImmediateTexCoord>();
			m_Device->ImmediateTexCoord(data.unit, data.x, data.y, data.z);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_ImmediateBegin:
		{
			const GfxPrimitiveType& data = stream.ReadValueType<GfxPrimitiveType>();
			m_Device->ImmediateBegin(data);
			break;
		}
		case kGfxCmd_ImmediateEnd:
		{
			m_Device->ImmediateEnd();
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CaptureScreenshot:
		{
			const GfxCmdCaptureScreenshot& capture = stream.ReadValueType<GfxCmdCaptureScreenshot>();
			*capture.success = m_Device->CaptureScreenshot(capture.left, capture.bottom, capture.width, capture.height, capture.rgba32);
			stream.ReadReleaseData();
			Signal();
			break;
		}
		case kGfxCmd_ReadbackImage:
		{
			const GfxCmdReadbackImage& read = stream.ReadValueType<GfxCmdReadbackImage>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			*read.success = m_Device->ReadbackImage(read.image, read.left, read.bottom, read.width, read.height, read.destX, read.destY);
#else
			//todo
#endif
			stream.ReadReleaseData();
			Signal();
			break;
		}
		case kGfxCmd_GrabIntoRenderTexture:
		{
			const GfxCmdGrabIntoRenderTexture& grab = stream.ReadValueType<GfxCmdGrabIntoRenderTexture>();
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceRenderSurface* surf = (ClientDeviceRenderSurface*)grab.rs.object;
			RenderSurfaceHandle handle(surf ? surf->internalHandle.object : NULL);
			ClientDeviceRenderSurface* surfZ = (ClientDeviceRenderSurface*)grab.rd.object;
			RenderSurfaceHandle handleZ(surfZ ? surfZ->internalHandle.object : NULL);
#else
			RenderSurfaceHandle handle(m_RenderSurfaceMapper[grab.rs]);
			RenderSurfaceHandle handleZ(m_RenderSurfaceMapper[grab.rd]);
#endif
			m_Device->GrabIntoRenderTexture(handle, handleZ, grab.x, grab.y, grab.width, grab.height);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_SetActiveContext:
		{
			void* ctx = stream.ReadValueType<void*>();
			m_Device->SetActiveContext (ctx);
			Signal();
			break;
		}
		case kGfxCmd_ResetFrameStats:
		{
			m_Device->ResetFrameStats();
			break;
		}
		case kGfxCmd_BeginFrameStats:
		{
			m_Device->GetFrameStats().BeginFrameStats();
			stream.ResetReadWaitTime();
			break;
		}
		case kGfxCmd_EndFrameStats:
		{
			GfxDeviceStats& stats = m_Device->GetFrameStats();
			stats.EndRenderFrameStats();
			stats.m_RenderFrameTime -= stream.GetReadWaitTime();
			break;
		}
		case kGfxCmd_SaveDrawStats:
		{
			m_Device->SaveDrawStats();
			break;
		}
		case kGfxCmd_RestoreDrawStats:
		{
			m_Device->RestoreDrawStats();
			break;
		}
		case kGfxCmd_SynchronizeStats:
		{
			Mutex::AutoLock lock(m_StatsMutex);
			m_Device->GetFrameStats().AccumulateUsedTextureUsage();
			m_FrameStats = m_Device->GetFrameStats();
			break;
		}
#if UNITY_EDITOR
		case kGfxCmd_SetAntiAliasFlag:
		{
			bool aa = stream.ReadValueType<GfxCmdBool>();
			m_Device->SetAntiAliasFlag(aa);
			break;
		}
		case kGfxCmd_DrawUserPrimitives:
		{
			GfxCmdDrawUserPrimitives user = stream.ReadValueType<GfxCmdDrawUserPrimitives>();
			int dataSize = user.vertexCount * user.stride;
			m_TempBuffer.resize_uninitialized(dataSize);
			stream.ReadStreamingData(m_TempBuffer.data(), dataSize);
			m_Device->DrawUserPrimitives(user.type, user.vertexCount, user.vertexChannels, m_TempBuffer.data(), user.stride);
			break;
		}
#endif
		case kGfxCmd_Quit:
		{
			m_Quit = true;
			Signal();
			break;
		}
		case kGfxCmd_InsertCustomMarker:
		{
			int marker = stream.ReadValueType<int>();
			m_Device->InsertCustomMarker (marker);
			break;
		}
		case kGfxCmd_SetComputeBufferData:
		{
			ComputeBufferID buffer = stream.ReadValueType<ComputeBufferID>();
			size_t size = stream.ReadValueType<size_t>();
			DebugAssert (size);
			m_TempBuffer.resize_uninitialized(size);
			stream.ReadStreamingData(&m_TempBuffer[0], size);
			m_Device->SetComputeBufferData (buffer, m_TempBuffer.data(), size);
			break;
		}
		case kGfxCmd_GetComputeBufferData:
		{
			ComputeBufferID buffer = stream.ReadValueType<ComputeBufferID>();
			size_t size = stream.ReadValueType<size_t>();
			void* ptr = stream.ReadValueType<void*>();
			DebugAssert (size && ptr);
			m_Device->GetComputeBufferData (buffer, ptr, size);
			stream.ReadReleaseData();
			Signal();
			break;
		}
		case kGfxCmd_CopyComputeBufferCount:
		{
			ComputeBufferID srcBuffer = stream.ReadValueType<ComputeBufferID>();
			ComputeBufferID dstBuffer = stream.ReadValueType<ComputeBufferID>();
			UInt32 dstOffset = stream.ReadValueType<UInt32>();
			m_Device->CopyComputeBufferCount (srcBuffer, dstBuffer, dstOffset);
			break;
		}
		case kGfxCmd_SetRandomWriteTargetTexture:
		{
			int index = stream.ReadValueType<int>();
			TextureID tid = stream.ReadValueType<TextureID>();
			m_Device->SetRandomWriteTargetTexture (index, tid);
			break;
		}
		case kGfxCmd_SetRandomWriteTargetBuffer:
		{
			int index = stream.ReadValueType<int>();
			ComputeBufferID buffer = stream.ReadValueType<ComputeBufferID>();
			m_Device->SetRandomWriteTargetBuffer (index, buffer);
			break;
		}
		case kGfxCmd_ClearRandomWriteTargets:
		{
			m_Device->ClearRandomWriteTargets ();
			break;
		}
		case kGfxCmd_CreateComputeProgram:
		{
			ClientDeviceComputeProgram* handle = stream.ReadValueType<ClientDeviceComputeProgram*>();
			size_t size = stream.ReadValueType<size_t>();
			m_TempBuffer.resize_uninitialized(size);
			stream.ReadStreamingData(&m_TempBuffer[0], size);
			handle->internalHandle = m_Device->CreateComputeProgram (&m_TempBuffer[0], m_TempBuffer.size());
			break;
		}
		case kGfxCmd_DestroyComputeProgram:
		{
			ClientDeviceComputeProgram* handle = stream.ReadValueType<ClientDeviceComputeProgram*>();
			DebugAssert (handle);
			m_Device->DestroyComputeProgram (handle->internalHandle);
			UNITY_DELETE(handle, kMemGfxThread);
			break;
		}
		case kGfxCmd_CreateComputeConstantBuffers:
		{
			unsigned count = stream.ReadValueType<unsigned>();
			Assert (count <= kMaxSupportedConstantBuffers);
			ClientDeviceConstantBuffer* clientCBs[kMaxSupportedConstantBuffers];
			UInt32 cbSizes[kMaxSupportedConstantBuffers];
			for (unsigned i = 0; i < count; ++i)
			{
				clientCBs[i] = stream.ReadValueType<ClientDeviceConstantBuffer*>();
				DebugAssert (clientCBs[i]);
				cbSizes[i] = clientCBs[i]->size;
			}

			ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
			m_Device->CreateComputeConstantBuffers (count, cbSizes, cbHandles);
			for (unsigned i = 0; i < count; ++i)
				clientCBs[i]->internalHandle = cbHandles[i];

			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_DestroyComputeConstantBuffers:
		{
			unsigned count = stream.ReadValueType<unsigned>();
			Assert (count <= kMaxSupportedConstantBuffers);
			ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
			for (unsigned i = 0; i < count; ++i)
			{
				ClientDeviceConstantBuffer* handle = stream.ReadValueType<ClientDeviceConstantBuffer*>();
				if (handle)
				{
					cbHandles[i] = handle->internalHandle;
					UNITY_DELETE(handle, kMemGfxThread);
				}
			}
			m_Device->DestroyComputeConstantBuffers (count, cbHandles);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_CreateComputeBuffer:
		{
			ComputeBufferID buffer = stream.ReadValueType<ComputeBufferID>();
			size_t count = stream.ReadValueType<size_t>();
			size_t stride = stream.ReadValueType<size_t>();
			UInt32 flags = stream.ReadValueType<UInt32>();
			m_Device->CreateComputeBuffer (buffer, count, stride, flags);
			break;
		}
		case kGfxCmd_DestroyComputeBuffer:
		{
			ComputeBufferID buffer = stream.ReadValueType<ComputeBufferID>();
			m_Device->DestroyComputeBuffer (buffer);
			break;
		}
		case kGfxCmd_UpdateComputeConstantBuffers:
		{
			unsigned count = stream.ReadValueType<unsigned>();
			Assert (count <= kMaxSupportedConstantBuffers);
			UInt32 cbDirty = stream.ReadValueType<UInt32>();

			ConstantBufferHandle cbHandles[kMaxSupportedConstantBuffers];
			UInt32 cbSizes[kMaxSupportedConstantBuffers];
			UInt32 cbOffsets[kMaxSupportedConstantBuffers];
			int bindPoints[kMaxSupportedConstantBuffers];
			for (unsigned i = 0; i < count; ++i)
			{
				ClientDeviceConstantBuffer* handle = stream.ReadValueType<ClientDeviceConstantBuffer*>();
				if (handle)
					cbHandles[i] = handle->internalHandle;
				cbSizes[i] = stream.ReadValueType<UInt32>();
				cbOffsets[i] = stream.ReadValueType<UInt32>();
				bindPoints[i] = stream.ReadValueType<int>();
			}
			size_t dataSize = stream.ReadValueType<size_t>();
			DebugAssert (dataSize);
			m_TempBuffer.resize_uninitialized(dataSize);
			stream.ReadStreamingData(&m_TempBuffer[0], dataSize);

			m_Device->UpdateComputeConstantBuffers (count, cbHandles, cbDirty, dataSize, &m_TempBuffer[0], cbSizes, cbOffsets, bindPoints);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_UpdateComputeResources:
		{
			TextureID textures[kMaxSupportedComputeResources];
			int texBindPoints[kMaxSupportedComputeResources];
			unsigned samplers[kMaxSupportedComputeResources];
			ComputeBufferID inBuffers[kMaxSupportedComputeResources];
			int inBufferBindPoints[kMaxSupportedComputeResources];
			ComputeBufferID outBuffers[kMaxSupportedComputeResources];
			TextureID outTextures[kMaxSupportedComputeResources];
			UInt32 outBufferBindPoints[kMaxSupportedComputeResources];

			unsigned texCount = stream.ReadValueType<unsigned>();
			for (int i = 0; i < texCount; ++i)
			{
				textures[i] = stream.ReadValueType<TextureID>();
				texBindPoints[i] = stream.ReadValueType<int>();
			}
			unsigned samplerCount = stream.ReadValueType<unsigned>();
			for (int i = 0; i < samplerCount; ++i)
			{
				samplers[i] = stream.ReadValueType<unsigned>();
			}
			unsigned inBufferCount = stream.ReadValueType<unsigned>();
			for (int i = 0; i < inBufferCount; ++i)
			{
				inBuffers[i] = stream.ReadValueType<ComputeBufferID>();
				inBufferBindPoints[i] = stream.ReadValueType<int>();
			}
			unsigned outBufferCount = stream.ReadValueType<unsigned>();
			for (int i = 0; i < outBufferCount; ++i)
			{
				outBuffers[i] = stream.ReadValueType<ComputeBufferID>();
				outTextures[i] = stream.ReadValueType<TextureID>();
				outBufferBindPoints[i] = stream.ReadValueType<UInt32>();
			}
			m_Device->UpdateComputeResources (texCount, textures, texBindPoints, samplerCount, samplers, inBufferCount, inBuffers, inBufferBindPoints, outBufferCount, outBuffers, outTextures, outBufferBindPoints);
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_DispatchComputeProgram:
		{
			ClientDeviceComputeProgram* cp = stream.ReadValueType<ClientDeviceComputeProgram*>();
			DebugAssert (cp);
			unsigned threadsX = stream.ReadValueType<unsigned>();
			unsigned threadsY = stream.ReadValueType<unsigned>();
			unsigned threadsZ = stream.ReadValueType<unsigned>();
			m_Device->DispatchComputeProgram (cp->internalHandle, threadsX, threadsY, threadsZ);
			break;
		}
		case kGfxCmd_DrawNullGeometry:
		{
			GfxPrimitiveType topology = stream.ReadValueType<GfxPrimitiveType>();
			int vertexCount = stream.ReadValueType<int>();
			int instanceCount = stream.ReadValueType<int>();
			m_Device->DrawNullGeometry (topology, vertexCount, instanceCount);
			break;
		}
		case kGfxCmd_DrawNullGeometryIndirect:
		{
			GfxPrimitiveType topology = stream.ReadValueType<GfxPrimitiveType>();
			ComputeBufferID bufferHandle = stream.ReadValueType<ComputeBufferID>();
			UInt32 bufferOffset = stream.ReadValueType<UInt32>();
			m_Device->DrawNullGeometryIndirect (topology, bufferHandle, bufferOffset);
			break;
		}
		case kGfxCmd_VBO_UpdateVertexData:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
			ClientVertexBufferData client;
			client = stream.ReadValueType<ClientVertexBufferData>();
			VertexBufferData data;
			memcpy (data.channels, client.channels, sizeof(ChannelInfoArray));
			memcpy (data.streams, client.streams, sizeof(StreamInfoArray));
			data.bufferSize = client.bufferSize;
			data.vertexCount = client.vertexCount;
			data.buffer = (UInt8*)client.hasData;
#else
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
			VertexBufferData data = stream.ReadValueType<VertexBufferData>();
#endif
			// Note! Buffer pointer is no longer valid but we use it to tell if data was written or not
			if (data.buffer)
				data.buffer = ReadBufferData(stream, data.bufferSize);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
				m_VBOMapper[vbo.internalVBO]->UpdateVertexData(data);
#else
				vbo->GetInternal()->UpdateVertexData(data);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_VBO_UpdateIndexData:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
#else
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
#endif
			IndexBufferData data;
			data.count = stream.ReadValueType<int>();
			data.hasTopologies = stream.ReadValueType<UInt32>();
			int bufferSize = data.count * kVBOIndexSize;
			data.indices = ReadBufferData(stream, bufferSize);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			m_VBOMapper[vbo.internalVBO]->UpdateIndexData(data);
#else
			vbo->GetInternal()->UpdateIndexData(data);
#endif
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_VBO_Draw:
		{
			PROFILER_AUTO(gMTDrawProf, NULL);
			const GfxCmdVBODraw& data = stream.ReadValueType<GfxCmdVBODraw>();
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			Assert(data.vbo.internalVBO);
			m_VBOMapper[data.vbo.internalVBO]->DrawVBO (data.channels, data.firstIndexByte, data.indexCount,
											  data.topology, data.firstVertex, data.vertexCount);
#else
			Assert(data.vbo && data.vbo->GetInternal());
			data.vbo->GetInternal()->DrawVBO (data.channels, data.firstIndexByte, data.indexCount,
				data.topology, data.firstVertex, data.vertexCount);
#endif
			stream.ReadReleaseData();
			break;
		}
#if GFX_ENABLE_DRAW_CALL_BATCHING
		case kGfxCmd_VBO_DrawCustomIndexed:
		{
			PROFILER_AUTO(gMTDrawProf, NULL);
			GfxCmdVBODrawCustomIndexed data = stream.ReadValueType<GfxCmdVBODrawCustomIndexed>();
			int dataSize = data.indexCount * kVBOIndexSize;
			m_TempBuffer.resize_uninitialized(dataSize);

			stream.ReadStreamingData(m_TempBuffer.data(), dataSize);
			data.vbo->GetInternal()->DrawCustomIndexed(data.channels, m_TempBuffer.data(), data.indexCount,
				data.topology, data.vertexRangeBegin, data.vertexRangeEnd, data.drawVertexCount);
			break;
		}
#endif
#if UNITY_XENON
		case kGfxCmd_VBO_AddExtraUvChannels:
		{
			GfxCmdVBOAddExtraUvChannels adduv = stream.ReadValueType<GfxCmdVBOAddExtraUvChannels>();
			m_TempBuffer.resize_uninitialized(adduv.size);
			stream.ReadStreamingData(m_TempBuffer.data(), adduv.size);
			adduv.vbo->GetInternal()->AddExtraUvChannels(m_TempBuffer.data(), adduv.size, adduv.extraUvCount);
			break;
		}
		case kGfxCmd_VBO_CopyExtraUvChannels:
		{
			GfxCmdVBOCopyExtraUvChannels copyuv = stream.ReadValueType<GfxCmdVBOCopyExtraUvChannels>();
			copyuv.dest->GetInternal()->CopyExtraUvChannels(copyuv.source->GetInternal());
			break;
		}
#endif
		case kGfxCmd_VBO_Recreate:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
			m_VBOMapper[vbo.GetInternal()]->Recreate();
#else
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
			vbo->GetInternal()->Recreate();
#endif
			break;
		}
		case kGfxCmd_VBO_MapVertexStream:
		{
			// Release any old data
			stream.ReadReleaseData();
			// This must be a reference since struct is updated by client thread
			const GfxCmdVBOMapVertexStream& map = stream.ReadValueType<GfxCmdVBOMapVertexStream>();
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			const ClientDeviceVBO& vbo = map.vbo;
#else
			ClientDeviceVBO* vbo = map.vbo;
#endif
			int size = map.size;
			void* dest = NULL;
			VertexStreamData mappedVSD;
			if (m_Device->IsValidState())
			{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
				if (m_VBOMapper[vbo.internalVBO]->MapVertexStream(mappedVSD, map.stream))
#else
				if (vbo->GetInternal()->MapVertexStream(mappedVSD, map.stream))
#endif
				{
					Assert(mappedVSD.buffer);
					dest = mappedVSD.buffer;
				}
				else
					ErrorString("Failed to map vertex stream!");
			}

			// ReadStreamingData will skip data if dest is NULL
			stream.ReadStreamingData(dest, size);

			if (dest)
			{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
				m_VBOMapper[vbo.internalVBO]->UnmapVertexStream(map.stream);
#else
				vbo->GetInternal()->UnmapVertexStream(map.stream);
#endif
			}
			break;
		}
		case kGfxCmd_VBO_SetVertexStreamMode:
		{
			GfxCmdVBOSetVertexStreamMode vsmode = stream.ReadValueType<GfxCmdVBOSetVertexStreamMode>();
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			m_VBOMapper[vsmode.vbo.GetInternal()]->SetVertexStreamMode(vsmode.stream, VBO::StreamMode(vsmode.mode));
#else
			vsmode.vbo->GetInternal()->SetVertexStreamMode(vsmode.stream, VBO::StreamMode(vsmode.mode));
#endif
			break;
		}
		case kGfxCmd_VBO_SetIndicesDynamic:
		{
			GfxCmdVBOSetSetIndicesDynamic vsdyn = stream.ReadValueType<GfxCmdVBOSetSetIndicesDynamic>();
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			m_VBOMapper[vsdyn.vbo.GetInternal()]->SetIndicesDynamic(vsdyn.dynamic);
#else
			vsdyn.vbo->GetInternal()->SetIndicesDynamic(vsdyn.dynamic);
#endif
			break;
		}
		case kGfxCmd_VBO_UseAsStreamOutput:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
			m_VBOMapper[vbo.internalVBO]->UseAsStreamOutput();
#else
			ClientDeviceVBO *vbo = stream.ReadValueType<ClientDeviceVBO *>();
			vbo->GetInternal()->UseAsStreamOutput();
#endif
			break;
		}
		case kGfxCmd_VBO_Constructor:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
			m_VBOMapper[vbo.internalVBO] = m_Device->CreateVBO();
#else
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
			vbo->internalVBO = m_Device->CreateVBO();
#endif
			break;
		}
		case kGfxCmd_VBO_Destructor:
		{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			ClientDeviceVBO vbo = stream.ReadValueType<ClientDeviceVBO>();
			m_Device->DeleteVBO(m_VBOMapper[vbo.internalVBO]);
#else
			ClientDeviceVBO* vbo = stream.ReadValueType<ClientDeviceVBO*>();
			m_Device->DeleteVBO(vbo->GetInternal());
			UNITY_DELETE( vbo, kMemGfxThread);
#endif
			break;
		}
		case kGfxCmd_DynVBO_Chunk:
		{
			GfxCmdDynVboChunk chunk = m_CommandQueue->ReadValueType<GfxCmdDynVboChunk>();
			DynamicVBO& vbo = m_Device->GetDynamicVBO();
			void* vertexData;
			void* indexData;
			void** indexDataPtr = (chunk.actualIndices > 0) ? &indexData :NULL;
			bool res = vbo.GetChunk(chunk.channelMask, chunk.actualVertices, chunk.actualIndices, DynamicVBO::RenderMode(chunk.renderMode), &vertexData, indexDataPtr);
			if (!res) vertexData = indexData = NULL;

			m_CommandQueue->ReadStreamingData(vertexData, chunk.actualVertices * chunk.vertexStride);
			if (chunk.actualIndices > 0)
				m_CommandQueue->ReadStreamingData(indexData, chunk.actualIndices * kVBOIndexSize);

			if (res) vbo.ReleaseChunk(chunk.actualVertices, chunk.actualIndices);

			m_CommandQueue->ReadReleaseData();
			break;
		}
		case kGfxCmd_DynVBO_DrawChunk:
		{
			PROFILER_AUTO(gMTDrawDynamicProf, NULL);
			const ChannelAssigns& channels = m_CommandQueue->ReadValueType<ChannelAssigns>();
			DynamicVBO& vbo = m_Device->GetDynamicVBO();
			vbo.DrawChunk(channels);
			m_CommandQueue->ReadReleaseData();
			break;
		}
		case kGfxCmd_DisplayList_Call:
		{
			ThreadedDisplayList* dlist = m_CommandQueue->ReadValueType<ThreadedDisplayList*>();
			Assert(m_CallDepth < m_MaxCallDepth);
			dlist->Patch(*m_CommandQueue);
			m_PlaybackDisplayLists[m_CallDepth] = dlist;
			m_CommandQueue = &m_PlaybackCommandQueues[m_CallDepth];
			m_CommandQueue->CreateReadOnly(dlist->GetData(), dlist->GetSize());
			m_CallDepth++;
			break;
		}
		case kGfxCmd_DisplayList_End:
		{
			Assert(m_CallDepth > 0);
			m_CallDepth--;
			SAFE_RELEASE(m_PlaybackDisplayLists[m_CallDepth]);
			if (m_CallDepth > 0)
				m_CommandQueue = &m_PlaybackCommandQueues[m_CallDepth - 1];
			else
				m_CommandQueue = m_MainCommandQueue;
			break;
		}
		case kGfxCmd_QueryGraphicsCaps:
		{
			// no, really, we need to properly serialize this stuff with all the strings. this is just a hack to get running.
			size_t offset = (char*)&gGraphicsCaps.vendorID - (char*)&gGraphicsCaps;
			WritebackData(stream, &gGraphicsCaps.vendorID, sizeof(GraphicsCaps) - offset);
			break;
		}
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		case kGfxCmd_SetGpuProgramParameters:
		{
			ClientIDMapper::ClientID internalHandle = stream.ReadValueType<ClientIDMapper::ClientID>();
			GpuProgramParameters* gpu = new GpuProgramParameters;
			Gfx_GpuProgramParameters gfxParams = stream.ReadValueType<Gfx_GpuProgramParameters>();
			int outSize = stream.ReadValueType<UInt32>();
			int strSize = stream.ReadValueType<UInt32>();
			
			char* buffer = (char*)m_CommandQueue->GetReadDataPointer (outSize+strSize, 1);
			char* strBuf = buffer + outSize;
			gfxParams.GetOutput(*gpu, buffer, strBuf, strSize);
			m_GpuProgramParametersMapper[internalHandle] = gpu;
			break;
		}
#endif
#if UNITY_EDITOR && UNITY_WIN
		case kGfxCmd_CreateWindow:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			const GfxCmdCreateWindow& upload = stream.ReadValueType<GfxCmdCreateWindow>();
			handle->internalWindow = m_Device->CreateGfxWindow(upload.window, upload.width, upload.height, upload.depthFormat, upload.antiAlias);
			break;
		}
		case kGfxCmd_SetActiveWindow:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			handle->GetInternal()->SetAsActiveWindow();
			break;
		}
		case kGfxCmd_WindowReshape:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			const GfxCmdWindowReshape& upload  = stream.ReadValueType<GfxCmdWindowReshape>();
			handle->GetInternal()->Reshape(upload.width, upload.height, upload.depthFormat, upload.antiAlias);
			Signal();
			break;
		}
		case kGfxCmd_WindowDestroy:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			delete handle->GetInternal();
			Signal();
			break;
		}
		case kGfxCmd_BeginRendering:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			handle->GetInternal()->BeginRendering();
			break;
		}
		case kGfxCmd_EndRendering:
		{
			ClientDeviceWindow* handle = stream.ReadValueType<ClientDeviceWindow*>();
			bool presentContent = stream.ReadValueType<GfxCmdBool>();
			handle->GetInternal()->EndRendering(presentContent);
			GfxCmdBool signalEvent = stream.ReadValueType<GfxCmdBool>();
			if (signalEvent)
				SignalEvent(kEventTypePresent);
			break;
		}
#endif
		case kGfxCmd_AcquireThreadOwnership:
		{
			m_Device->AcquireThreadOwnership();
			SetRealGfxDeviceThreadOwnership();
			Signal();
			m_IsThreadOwner = true;
			break;
		}
		case kGfxCmd_ReleaseThreadOwnership:
		{
			m_Device->ReleaseThreadOwnership();
			Signal();
			m_IsThreadOwner = false;
			break;
		}

		#if ENABLE_PROFILER
		case kGfxCmd_BeginProfileEvent:
		{
			const char* name = stream.ReadValueType<const char*>();
			m_Device->BeginProfileEvent (name);
			break;
		}
		case kGfxCmd_EndProfileEvent:
		{
			m_Device->EndProfileEvent ();
			break;
		}
		case kGfxCmd_ProfileControl:
		{
			GfxDevice::GfxProfileControl ctrl = stream.ReadValueType<GfxDevice::GfxProfileControl>();
			unsigned param = stream.ReadValueType<unsigned>();
			
			#if ENABLE_PROFILER
			switch (ctrl)
			{
				case GfxDevice::kGfxProfBeginFrame: profiler_begin_frame_seperate_thread((ProfilerMode)param); break;
				case GfxDevice::kGfxProfEndFrame: profiler_end_frame_seperate_thread(param); break;
				case GfxDevice::kGfxProfDisableSampling: profiler_disable_sampling_seperate_thread(); break;
				case GfxDevice::kGfxProfSetActive: profiler_set_active_seperate_thread(param!=0); break;
			}
			#else
			AssertString("shouldn't be invoked");
			#endif
			break;
		}
		case kGfxCmd_BeginTimerQueries:
		{
			PROFILER_AUTO(gMTBeginQueriesProf, NULL);
			m_Device->BeginTimerQueries();
			// Implicitly poll queries
			PollTimerQueries();
			break;
		}
		case kGfxCmd_EndTimerQueries:
		{
			PROFILER_AUTO(gMTEndQueriesProf, NULL);
			m_Device->EndTimerQueries();
			stream.ReadReleaseData();
			break;
		}
		case kGfxCmd_TimerQuery_Constructor:
		{
			ClientDeviceTimerQuery* query = stream.ReadValueType<ClientDeviceTimerQuery*>();
			query->internalQuery = m_Device->CreateTimerQuery();
			break;
		}
		case kGfxCmd_TimerQuery_Destructor:
		{
			ClientDeviceTimerQuery* query = stream.ReadValueType<ClientDeviceTimerQuery*>();
			m_Device->DeleteTimerQuery(query->GetInternal());
			break;
		}
		case kGfxCmd_TimerQuery_Measure:
		{
			ClientDeviceTimerQuery* query = stream.ReadValueType<ClientDeviceTimerQuery*>();
			if (query->GetInternal())
			{
				Assert(!query->pending);
				query->pending = true;
				query->GetInternal()->Measure();
				m_PolledTimerQueries.push_back(query);
			}
			break;
		}
		case kGfxCmd_TimerQuery_GetElapsed:
		{
			ClientDeviceTimerQuery* query = stream.ReadValueType<ClientDeviceTimerQuery*>();
			UInt32 flags = stream.ReadValueType<UInt32>();
			bool wait = (flags & GfxTimerQuery::kWaitRenderThread) != 0;
			while (query->pending)
				PollNextTimerQuery(wait);
			if (flags & GfxTimerQuery::kWaitClientThread)
				Signal();
			break;
		}
		#endif

		case kGfxCmd_DeleteGPUSkinningInfo:
		{
			GPUSkinningInfo *info = stream.ReadValueType<GPUSkinningInfo *>();
			m_Device->DeleteGPUSkinningInfo(info);
			break;
		}
		case kGfxCmd_SkinOnGPU:
		{
		#if ENABLE_GFXDEVICE_REMOTE_PROCESS
			// todo.
		#else
			GPUSkinningInfo *info = stream.ReadValueType<GPUSkinningInfo *>();
			ClientDeviceVBO* destVBO = stream.ReadValueType<ClientDeviceVBO*>();
			bool lastThisFrame = stream.ReadValueType<bool>();
			info->SetDestVBO(destVBO->GetInternal());

			m_Device->SkinOnGPU(info, lastThisFrame);
		#endif
			break;
		}

		case kGfxCmd_UpdateSkinSourceData:
		{
			GPUSkinningInfo *info = stream.ReadValueType<GPUSkinningInfo *>();
			void *vboData = stream.ReadValueType<void *>();
			BoneInfluence *bones = stream.ReadValueType<BoneInfluence *>();
			bool dirty = stream.ReadValueType<bool>();

			m_Device->UpdateSkinSourceData(info, vboData, bones, dirty);

			break;
		}

		case kGfxCmd_UpdateSkinBonePoses:
		{
			GPUSkinningInfo *info = stream.ReadValueType<GPUSkinningInfo *>();
			int boneSize = stream.ReadValueType<int>();
			Matrix4x4f *bones = stream.ReadArrayType<Matrix4x4f>(boneSize);

			m_Device->UpdateSkinBonePoses(info, boneSize, bones);

			break;
		}

#if UNITY_XENON
		case kGfxCmd_RawVBO_Constructor:
		{
			ClientDeviceRawVBO* vbo = stream.ReadValueType<ClientDeviceRawVBO*>();
			UInt32 size = stream.ReadValueType<UInt32>();
			UInt32 flags = stream.ReadValueType<UInt32>();
			vbo->internalVBO = m_Device->CreateRawVBO(size, flags);
			break;
		}
		case kGfxCmd_RawVBO_Destructor:
		{
			ClientDeviceRawVBO* vbo = stream.ReadValueType<ClientDeviceRawVBO*>();
			m_Device->DeleteRawVBO(vbo->GetInternal());
			delete vbo;
			break;
		}
		case kGfxCmd_RawVBO_Next:
		{
			ClientDeviceRawVBO* vbo = stream.ReadValueType<ClientDeviceRawVBO*>();
			vbo->GetInternal()->Next();
			break;
		}
		case kGfxCmd_RawVBO_Write:
		{
			ClientDeviceRawVBO* vbo = stream.ReadValueType<ClientDeviceRawVBO*>();
			UInt32 offset = stream.ReadValueType<UInt32>();
			UInt32 size = stream.ReadValueType<UInt32>();
			void* dest = vbo->GetInternal()->GetMemory(offset, size);
			stream.ReadStreamingData(dest, size);
			break;
		}
		case kGfxCmd_RawVBO_InvalidateGpuCache:
		{
			ClientDeviceRawVBO* vbo = stream.ReadValueType<ClientDeviceRawVBO*>();
			vbo->GetInternal()->InvalidateGpuCache();
			break;
		}
		case kGfxCmd_EnablePersistDisplayOnQuit:
		{
			bool enabled = stream.ReadValueType<bool>();
			m_Device->EnablePersistDisplayOnQuit(enabled);
			break;
		}
		case kGfxCmd_RegisterTexture2D:
		{
			TextureID tid = stream.ReadValueType<TextureID>();
			IDirect3DBaseTexture9* tex = stream.ReadValueType<IDirect3DBaseTexture9*>();
			m_Device->RegisterTexture2D(tid, tex);
			break;
		}
		case kGfxCmd_PatchTexture2D:
		{
			TextureID tid = stream.ReadValueType<TextureID>();
			IDirect3DBaseTexture9* tex = stream.ReadValueType<IDirect3DBaseTexture9*>();
			m_Device->PatchTexture2D(tid, tex);
			break;
		}
		case kGfxCmd_DeleteTextureEntryOnly:
		{
			TextureID tid = stream.ReadValueType<TextureID>();
			m_Device->DeleteTextureEntryOnly(tid);
			break;
		}
		case kGfxCmd_UnbindAndDelayReleaseTexture:
		{
			IDirect3DBaseTexture9* tex = stream.ReadValueType<IDirect3DBaseTexture9*>();
			m_Device->UnbindAndDelayReleaseTexture(tex);
			break;
		}
		case kGfxCmd_SetTextureWrapModes:
		{
			TextureID tid = stream.ReadValueType<TextureID>();
			TextureWrapMode wrapU = stream.ReadValueType<TextureWrapMode>();
			TextureWrapMode wrapV = stream.ReadValueType<TextureWrapMode>();
			TextureWrapMode wrapW = stream.ReadValueType<TextureWrapMode>();
			m_Device->SetTextureWrapModes(tid, wrapU, wrapV, wrapW);
			break;
		}
		case kGfxCmd_OnLastFrameCallback:
		{
			m_Device->OnLastFrameCallback();
			break;
		}
		case kGfxCmd_VideoPlayer_Constructor:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			bool fullscreen = stream.ReadValueType<bool>();
			vp->internalVP = m_Device->CreateVideoPlayer(fullscreen);
			break;
		}
		case kGfxCmd_VideoPlayer_Destructor:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			m_Device->DeleteVideoPlayer(vp->GetInternal());
			delete vp;
			break;
		}
		case kGfxCmd_VideoPlayer_Render:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			vp->GetInternal()->Render();
			vp->isDead = vp->GetInternal()->IsDead();
			break;
		}
		case kGfxCmd_VideoPlayer_Pause:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			vp->GetInternal()->Pause();
			break;
		}
		case kGfxCmd_VideoPlayer_Resume:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			vp->GetInternal()->Resume();
			break;
		}
		case kGfxCmd_VideoPlayer_Stop:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			vp->GetInternal()->Stop();
			break;
		}
		case kGfxCmd_VideoPlayer_SetPlaySpeed:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			float speed = stream.ReadValueType<float>();
			vp->GetInternal()->SetPlaySpeed(speed);
			break;
		}
		case kGfxCmd_VideoPlayer_Play:
		{
			ClientDeviceVideoPlayer* vp = stream.ReadValueType<ClientDeviceVideoPlayer*>();
			XMEDIA_XMV_CREATE_PARAMETERS xmvParams = stream.ReadValueType<XMEDIA_XMV_CREATE_PARAMETERS>();
			bool loop = stream.ReadValueType<bool>();
			if (xmvParams.createType == XMEDIA_CREATE_FROM_FILE)
			{
				size_t len = stream.ReadValueType<size_t>();
				char* fileName = (char*)_alloca(len);
				stream.ReadStreamingData(fileName, len);
				xmvParams.createFromFile.szFileName = fileName;
			}
			bool hasRect = stream.ReadValueType<bool>();
			RECT rect;
			RECT* rectPtr = 0;
			if (hasRect)
			{
				rect = stream.ReadValueType<RECT>();
				rectPtr = &rect;
			}
			vp->GetInternal()->Play(xmvParams, loop, rectPtr);
			break;
		}
		case kGfxCmd_SetNullPixelShader:
		{
			m_Device->SetNullPixelShader();
			break;
		}
		case kGfxCmd_EnableHiZ:
		{
			HiZstate hiz_enable = stream.ReadValueType<HiZstate>();
			m_Device->SetHiZEnable( hiz_enable );
			break;
		}
		case kGfxCmd_SetHiStencilState:
		{
			bool hiStencilEnable      = stream.ReadValueType<bool>();
			bool hiStencilWriteEnable = stream.ReadValueType<bool>();
			int hiStencilRef          = stream.ReadValueType<int>();			
			CompareFunction cmpFunc   = stream.ReadValueType<CompareFunction>();
		
			Assert( hiStencilRef < 256 );
			Assert( cmpFunc == kFuncEqual || cmpFunc == kFuncNotEqual );
			m_Device->SetHiStencilState( hiStencilEnable, hiStencilWriteEnable, hiStencilRef, cmpFunc );
			break;
		}
		case kGfxCmd_HiStencilFlush:
		{
			HiSflush flushtype = stream.ReadValueType<HiSflush>();
			m_Device->HiStencilFlush( flushtype );
			break;
		}
#endif // UNITY_XENON
		default:
			ErrorString(Format("Gfx command not handled: %d (Last command: %d)", cmd, lastCmd));
	}

	GFXDEVICE_LOCKSTEP_WORKER();
	lastCmd = cmd;
}

void GfxDeviceWorker::DoLockstep(int pos, int cmd)
{
	if (m_CallDepth == 0)
	{
		//printf_console("MT: worker pos %i cmd %i\n", pos, cmd);
		m_LockstepSemaphore.Signal();
	}
}

UInt8* GfxDeviceWorker::ReadBufferData (ThreadedStreamBuffer& stream, int size)
{
	int maxNonStreamedSize = stream.GetAllocatedSize() / 2;
	if (size <= maxNonStreamedSize || m_CallDepth > 0)
	{
		stream.ReadReleaseData();
		void* data = stream.GetReadDataPointer(size, ThreadedStreamBuffer::kDefaultAlignment);
		return reinterpret_cast<UInt8*>(data);
	}
	else
	{
		m_TempBuffer.resize_uninitialized(size);
		stream.ReadStreamingData(m_TempBuffer.data(), size);
		return m_TempBuffer.data();
	}
}

void GfxDeviceWorker::WritebackData (ThreadedStreamBuffer& stream, const void* data, int size)
{
	const SInt32 maxSize = m_CommandQueue->ReadValueType<SInt32>();
	SInt32 writtenSize = 0;
	do
	{
		void* chunkPtr = m_CommandQueue->GetReadDataPointer(sizeof(SInt32) + maxSize, ThreadedStreamBuffer::kDefaultAlignment);
		SInt32* returnedSizePtr = static_cast<SInt32*>(chunkPtr);
		void* returnedDataPtr = &returnedSizePtr[1];
		SInt32 chunkSize = std::min<SInt32>(size, maxSize);
		if (chunkSize > 0)
			memcpy(returnedDataPtr, static_cast<const UInt8*>(data) + writtenSize, chunkSize);
		UnityMemoryBarrier();
		*returnedSizePtr = chunkSize;
		stream.ReadReleaseData();
		writtenSize += size;
	} while (writtenSize < size);
}

#if ENABLE_PROFILER
void GfxDeviceWorker::PollTimerQueries()
{
	for (;;)
	{
		if (!PollNextTimerQuery(false))
			break;
	}
}

bool GfxDeviceWorker::PollNextTimerQuery(bool wait)
{
	if (m_PolledTimerQueries.empty())
		return false;

	ClientDeviceTimerQuery* query = m_PolledTimerQueries.front();
	UInt32 flags = wait ? GfxTimerQuery::kWaitAll : 0;
	ProfileTimeFormat elapsed = query->GetInternal()->GetElapsed(flags);
	if (elapsed != kInvalidProfileTime)
	{
		m_PolledTimerQueries.pop_front();
		// Make result available to client thread
		query->elapsed = elapsed;
		UnityMemoryBarrier();
		query->pending = false;
		return true;
	}
	return false;
}
#endif

#endif
