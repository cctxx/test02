#pragma once
#if ENABLE_MULTITHREADED_CODE

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/threaded/WorkerIDMapper.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Threads/JobScheduler.h"
#include "External/shaderlab/Library/TextureBinding.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include <deque>


#define GFXDEVICE_USE_CACHED_STATE 0
#define DEBUG_GFXDEVICE_LOCKSTEP 0

#if DEBUG_GFXDEVICE_LOCKSTEP
	#define GFXDEVICE_LOCKSTEP_CLIENT() { DoLockstep(); }
	#define GFXDEVICE_LOCKSTEP_WORKER() { DoLockstep(pos, cmd); }
#else
	#define GFXDEVICE_LOCKSTEP_CLIENT()
	#define GFXDEVICE_LOCKSTEP_WORKER()
#endif

struct ClientDeviceTimerQuery;
class ThreadedStreamBuffer;
class ThreadedDisplayList;
class Thread;

class GfxDeviceWorker : public NonCopyable
{
public:
	GfxDeviceWorker(int maxCallDepth, ThreadedStreamBuffer* commandQueue);
	~GfxDeviceWorker();

	GfxThreadableDevice* Startup(GfxDeviceRenderer renderer, bool threaded, bool forceRef);

	void	WaitForSignal();
	void	LockstepWait();

	void	GetLastFrameStats(GfxDeviceStats& stats);

	void	CallImmediate(ThreadedDisplayList* dlist);

	enum EventType
	{
		kEventTypePresent,
		kEventTypeTimerQueries,
		kEventTypeCount
	};

	void	WaitForEvent(EventType type);

	void	WaitOnCPUFence(UInt32 fence);

	bool	DidPresentFrame(UInt32 frameID) const;

	bool	RunCommandIfDataIsAvailable();

private:
	void	SignalEvent(EventType type);

	static void* RunGfxDeviceWorker(void *data);

	void Run();
	void RunCommand(ThreadedStreamBuffer& stream);
	void Signal();
	void DoLockstep(int pos, int cmd);

	UInt8* ReadBufferData(ThreadedStreamBuffer& stream, int size);
	void WritebackData(ThreadedStreamBuffer& stream, const void* data, int size);

#if ENABLE_PROFILER
	void PollTimerQueries();
	bool PollNextTimerQuery(bool wait);
#endif

#if GFXDEVICE_USE_CACHED_STATE
	struct CachedState
	{
		CachedState();
		NormalizationMode normalization;
		int backface;
		Vector4f ambient;
		int fogEnabled;
		GfxFogParams fogParams;
	};
#endif

	int m_CallDepth;
	int m_MaxCallDepth;
	Thread* m_WorkerThread;
	ThreadedStreamBuffer* m_CommandQueue;
	ThreadedStreamBuffer* m_MainCommandQueue;
	ThreadedStreamBuffer* m_PlaybackCommandQueues;
	ThreadedDisplayList** m_PlaybackDisplayLists;
	dynamic_array<UInt8> m_TempBuffer;
	dynamic_array<SkinMeshInfo> m_ActiveSkins;
	dynamic_array<VBO*> m_MappedSkinVBOs;
	JobScheduler::JobGroupID m_SkinJobGroup;
	Semaphore m_EventSemaphores[kEventTypeCount];
	Semaphore m_LockstepSemaphore;
	Semaphore m_WaitSemaphore;
	volatile UInt32 m_CurrentCPUFence;
	volatile UInt32 m_PresentFrameID;
	Mutex m_StatsMutex;
	GfxDeviceStats m_FrameStats;
	GfxThreadableDevice* m_Device;
	bool m_IsThreadOwner;
	bool m_Quit;
#if GFXDEVICE_USE_CACHED_STATE
	CachedState m_Cached;
#endif
#if ENABLE_PROFILER
	// Timer queries for GPU profiling
	typedef std::deque<ClientDeviceTimerQuery*> TimerQueryList;
	TimerQueryList m_PolledTimerQueries;
#endif
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
	WorkerIDMapper<DeviceBlendState> m_BlendStateMapper;
	WorkerIDMapper<DeviceDepthState> m_DepthStateMapper;
	WorkerIDMapper<DeviceStencilState> m_StencilStateMapper;
	WorkerIDMapper<DeviceRasterState> m_RasterStateMapper;
	WorkerIDMapper<VBO> m_VBOMapper;
	WorkerIDMapper<void> m_TextureCombinerMapper;
	WorkerIDMapper<void> m_RenderSurfaceMapper;
	WorkerIDMapper<GpuProgramParameters> m_GpuProgramParametersMapper;
	WorkerIDMapper<GpuProgram> m_GpuProgramMapper;
	ClientIDMapper m_GpuProgramClientMapper;
#endif
};

#endif
