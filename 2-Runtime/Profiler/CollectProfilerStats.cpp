#include "UnityPrefix.h"
#include "CollectProfilerStats.h"
#include "Runtime/Allocator/MemoryManager.h"

#if ENABLE_PROFILER

#if UNITY_OSX
#include <mach/mach.h>
#elif UNITY_WIN && !UNITY_WP8
#include "Psapi.h"
#elif UNITY_XENON
#include "PlatformDependent/Xbox360/Source/XenonMemory.h"
#endif

#include "Runtime/Profiler/ProfilerStats.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Profiler/ProfilerImpl.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Profiler/MemoryProfilerStats.h"
#include "MemoryProfiler.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Dynamics/PhysicsModule.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "Runtime/Interfaces/IPhysics2D.h"
#include "Runtime/Interfaces/IAudio.h"

void CollectDrawStats (DrawStats& drawStats)
{
	// Read GFX Device and populate rendering statistics with obtained information
	drawStats.drawCalls = GetGfxDevice().GetFrameStats().GetDrawStats().calls;
	drawStats.triangles = GetGfxDevice().GetFrameStats().GetDrawStats().tris;
	drawStats.vertices  = GetGfxDevice().GetFrameStats().GetDrawStats().verts;
	
	drawStats.batchedDrawCalls = GetGfxDevice().GetFrameStats().GetDrawStats().batchedCalls;
	drawStats.batchedTriangles = GetGfxDevice().GetFrameStats().GetDrawStats().batchedTris;
	drawStats.batchedVertices  = GetGfxDevice().GetFrameStats().GetDrawStats().batchedVerts;

	drawStats.shadowCasters = GetGfxDevice().GetFrameStats().GetClientStats().shadowCasters;
	
	GetGfxDevice().GetFrameStats().AccumulateUsedTextureUsage();	
	drawStats.usedTextureCount = GetGfxDevice().GetFrameStats().GetDrawStats().usedTextureCount;
	drawStats.usedTextureBytes = GetGfxDevice().GetFrameStats().GetDrawStats().usedTextureBytes;
	
	drawStats.renderTextureCount = RenderTexture::GetCreatedRenderTextureCount();
	drawStats.renderTextureBytes = RenderTexture::GetCreatedRenderTextureBytes();
	drawStats.renderTextureStateChanges = GetGfxDevice().GetFrameStats().GetStateChanges().renderTexture;
	
	drawStats.screenWidth = GetGfxDevice().GetFrameStats().GetMemoryStats().screenWidth;
	drawStats.screenHeight = GetGfxDevice().GetFrameStats().GetMemoryStats().screenHeight;
	drawStats.screenFSAA = GetGfxDevice().GetFrameStats().GetMemoryStats().screenFSAA;
	drawStats.screenBytes = GetGfxDevice().GetFrameStats().GetMemoryStats().screenBytes;
	
	drawStats.vboTotal = GetGfxDevice().GetTotalVBOCount();
	drawStats.vboTotalBytes = GetGfxDevice().GetTotalVBOBytes();
	drawStats.vboUploads = GetGfxDevice().GetFrameStats().GetStateChanges().vboUploads;
	drawStats.vboUploadBytes = GetGfxDevice().GetFrameStats().GetStateChanges().vboUploadBytes;
	drawStats.ibUploads = GetGfxDevice().GetFrameStats().GetStateChanges().ibUploads;
	drawStats.ibUploadBytes = GetGfxDevice().GetFrameStats().GetStateChanges().ibUploadBytes;
	drawStats.totalAvailableVRamMBytes = RoundfToInt(gGraphicsCaps.videoMemoryMB);
	
	drawStats.visibleSkinnedMeshes = SkinnedMeshRenderer::GetVisibleSkinnedMeshRendererCount();
}



static void GatherObjectAllocationInformation(const dynamic_array<Object*>& objs, int& nbObjects, int& bytes)
{
	nbObjects = objs.size();
	bytes = 0;
	for (int i=0; i<nbObjects; i++)
		bytes += objs[i]->GetRuntimeMemorySize();
}

void CollectMemoryAllocationStats(MemoryStats& memoryStats)
{
	GatherObjectAllocationInformation( GetMemoryProfilerStats().GetTextures(), memoryStats.textureCount, memoryStats.textureBytes);
	GatherObjectAllocationInformation( GetMemoryProfilerStats().GetMeshes(), memoryStats.meshCount, memoryStats.meshBytes);
	GatherObjectAllocationInformation( GetMemoryProfilerStats().GetMaterials(), memoryStats.materialCount, memoryStats.materialBytes);
	GatherObjectAllocationInformation( GetMemoryProfilerStats().GetAnimationClips(), memoryStats.animationClipCount, memoryStats.animationClipBytes);
	GatherObjectAllocationInformation( GetMemoryProfilerStats().GetAudioClips(), memoryStats.audioCount, memoryStats.audioBytes);
	memoryStats.totalObjectsCount = Object::GetLoadedObjectCount();
	
#if ENABLE_MEMORY_MANAGER
	
	#if UNITY_XENON
	size_t additionalUsedMemoryUnity = xenon::GetOtherMemoryAllocated();
	size_t additionalUsedMemorySystem = 32 * 1024 * 1024; // OS reserved
	#else
	size_t additionalUsedMemoryUnity = 0;
	size_t additionalUsedMemorySystem = 0;
	#endif

	memoryStats.bytesUsedProfiler = GetMemoryManager().GetAllocator(kMemProfiler)->GetAllocatedMemorySize();
	memoryStats.bytesUsedFMOD = GetMemoryManager().GetAllocatedMemory(kMemFMOD);
	memoryStats.bytesUsedUnity = GetUsedHeapSize() - memoryStats.bytesUsedProfiler - memoryStats.bytesUsedFMOD + additionalUsedMemoryUnity;
	#if ENABLE_MONO	
	memoryStats.bytesUsedMono = mono_gc_get_used_size();
	#else
	memoryStats.bytesUsedMono = 0;
	#endif
	memoryStats.bytesUsedGFX = GetMemoryManager().GetRegisteredGFXDriverMemory();
	memoryStats.bytesUsedTotal = memoryStats.bytesUsedUnity + memoryStats.bytesUsedMono + memoryStats.bytesUsedGFX + memoryStats.bytesUsedProfiler + additionalUsedMemorySystem;

	memoryStats.bytesReservedProfiler = GetMemoryManager().GetAllocator(kMemProfiler)->GetReservedSizeTotal();
	memoryStats.bytesReservedFMOD = GetMemoryManager().GetAllocatedMemory(kMemFMOD);
	memoryStats.bytesReservedUnity = GetMemoryManager().GetTotalReservedMemory() - memoryStats.bytesReservedProfiler - memoryStats.bytesReservedFMOD + additionalUsedMemoryUnity;
	#if ENABLE_MONO	
	memoryStats.bytesReservedMono = mono_gc_get_heap_size();
	#else
	memoryStats.bytesReservedMono = 0;
	#endif
	memoryStats.bytesReservedGFX = GetMemoryManager().GetRegisteredGFXDriverMemory();
	memoryStats.bytesReservedTotal = memoryStats.bytesReservedUnity + memoryStats.bytesReservedMono + memoryStats.bytesReservedGFX + memoryStats.bytesReservedProfiler + additionalUsedMemorySystem;
	
	memoryStats.assetCount =  GetMemoryProfilerStats().GetAssetCount();
	memoryStats.sceneObjectCount =  GetMemoryProfilerStats().GetSceneObjectCount();
	memoryStats.gameObjectCount = GetMemoryProfilerStats().GetGameObjectCount();
	memoryStats.classCount =  GetMemoryProfilerStats().GetClassCount();
	
	memoryStats.bytesVirtual = systeminfo::GetUsedVirtualMemoryMB() * 1024*1024;

	#if UNITY_WP8
	memoryStats.bytesCommitedLimit = systeminfo::GetCommitedMemoryLimitMB() * 1024 * 1024;
	memoryStats.bytesCommitedTotal = systeminfo::GetCommitedMemoryMB() * 1024 * 1024;
	#else
	memoryStats.bytesCommitedLimit = 0;
	memoryStats.bytesCommitedTotal = 0;
	#endif

#if ENABLE_MEM_PROFILER
	//memoryStats.memoryOverview = GetMemoryProfiler()->GetOverview();
	#endif

#endif // #if ENABLE_MEMORY_MANAGER
}

void CollectProfilerStats (AllProfilerStats& stats)
{		
	CollectMemoryAllocationStats(stats.memoryStats);
	CollectDrawStats(stats.drawStats);
	
	UnityProfiler::Get().GetDebugStats(stats.debugStats);

	IAudio* audioModule = GetIAudio();
	if (audioModule)
		audioModule->GetProfilerStats(stats.audioStats);

	IPhysics* physicsModule = GetIPhysics();
	if (physicsModule)
		physicsModule->GetProfilerStats(stats.physicsStats);

	IPhysics2D* physics2DModule = GetIPhysics2D ();
	if (physics2DModule)
		physics2DModule->GetProfilerStats (stats.physics2DStats);
}

ProfilerString GetMiniMemoryOverview()
{
#if ENABLE_MEMORY_MANAGER
	return ("Allocated: " + FormatBytes(GetUsedHeapSize()) + " Objects: " + IntToString(Object::GetLoadedObjectCount ())).c_str();
#else 
	return "";
#endif
}

#endif

unsigned GetUsedHeapSize()
{
#if ENABLE_MEMORY_MANAGER

#if (UNITY_OSX && UNITY_EDITOR)
	UInt32 osxversion = 0;
    Gestalt(gestaltSystemVersion, (MacSInt32 *) &osxversion);
	if(osxversion >= 0x01060)
		return MemoryManager::m_LowLevelAllocated - GetMemoryManager().GetTotalUnusedReservedMemory();
	else 
		return 0;
#else
	return GetMemoryManager().GetTotalAllocatedMemory();
#endif
	
#else
	return 0;
#endif
}
