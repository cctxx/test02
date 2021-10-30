#include "UnityPrefix.h"
#include "GfxDevice.h"
#include "GfxDeviceStats.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Input/TimeManager.h"

#if UNITY_IPHONE
	#include <mach/mach_time.h>
#elif UNITY_ANDROID
	#include <sys/time.h>
#endif
#include "Runtime/Profiler/MemoryProfiler.h"

GfxDeviceStats::GfxDeviceStats()
{
	#if ENABLE_PROFILER
	m_StatsEnabled = false; // off by default!
	m_ClientFrameTime = 0.0f;
	m_RenderFrameTime = 0.0f;
	m_EnableStartTime = 0.0;
	#endif

	// just initialize to some sane values
	SetScreenParams( 64, 64, 4, 4, 4, 0 );
	m_Memory.renderTextureBytes = 0;
}


void GfxDeviceStats::StateStats::Reset()
{
	renderTexture = 0;
	vboUploads = 0;
	vboUploadBytes = 0;
	ibUploads = 0;
	ibUploadBytes = 0;
}

void GfxDeviceStats::ClientStats::Reset()
{
	shadowCasters = 0;

	ABSOLUTE_TIME_INIT(cullingDt);
	ABSOLUTE_TIME_INIT(clearDt);
}

void GfxDeviceStats::DrawStats::Reset()
{
	calls = 0;
	tris = 0;
	trisSent = 0;
	verts = 0;
	batches = 0;
	batchedCalls = 0;
	batchedTris = 0;
	batchedVerts = 0;

	ABSOLUTE_TIME_INIT(dt);
	ABSOLUTE_TIME_INIT(batchDt);

	usedTextureCount = 0;
	usedTextureBytes = 0;

	#if ENABLE_PROFILER
	usedTextures.clear();
	#endif
}

void GfxDeviceStats::AccumulateUsedTextureUsage()
{
#if ENABLE_PROFILER
	m_Draw.usedTextureCount += m_Draw.usedTextures.size();
	#if ENABLE_MEM_PROFILER
		for (GfxDeviceStats::DrawStats::TextureIDSet::const_iterator i=m_Draw.usedTextures.begin();i != m_Draw.usedTextures.end();i++)
			m_Draw.usedTextureBytes += GetMemoryProfiler()->GetRelatedIDMemorySize(i->m_ID);
	#endif
	m_Draw.usedTextures.clear();
#endif
}

void GfxDeviceStats::ResetFrame()
{
	m_Changes.Reset();
	m_Draw.Reset();
	m_Client.Reset();
	#if ENABLE_PROFILER
	m_ClientFrameTime = 0.0f;
	m_RenderFrameTime = 0.0f;
	#endif
}

void GfxDeviceStats::ResetClientStats()
{
	m_Client.Reset();
	m_ClientFrameTime = 0.0f;
}

#if ENABLE_PROFILER
void GfxDeviceStats::AddUsedTexture(TextureID tex)
{
	CHECK_STATS_ENABLED
	if(m_Draw.usedTextures.find(tex) == m_Draw.usedTextures.end())
		m_Draw.usedTextures.insert(tex);
}
#endif

void GfxDeviceStats::SetScreenParams( int width, int height, int backbufferBPP, int frontbufferBPP, int depthBPP, int fsaa )
{
	m_Memory.screenWidth = width;
	m_Memory.screenHeight = height;

	// can pass -1 for BPP params to keep current ones
	if (frontbufferBPP >= 0)
		m_Memory.screenFrontBPP = frontbufferBPP;
	if (backbufferBPP >= 0)
		m_Memory.screenBackBPP = backbufferBPP;
	if (depthBPP >= 0)
		m_Memory.screenDepthBPP = depthBPP;
	if (fsaa >= 0)
		m_Memory.screenFSAA = fsaa;

	m_Memory.screenBytes = width * height * (std::max(m_Memory.screenFSAA,1) * (m_Memory.screenBackBPP + m_Memory.screenDepthBPP) + m_Memory.screenFrontBPP);
	//printf_console("set screen params: %ix%i %ixAA mem=%.1fMB\n", width, height, fsaa, m_Memory.screenBytes/1024.0f/1024.0f);
}

void GfxDeviceStats::BeginFrameStats()
{
	m_StatsEnabled = true;
	m_EnableStartTime = GetTimeSinceStartup();
}

void GfxDeviceStats::EndClientFrameStats()
{
	m_StatsEnabled = false;
	m_ClientFrameTime += GetTimeSinceStartup() - m_EnableStartTime;
}

void GfxDeviceStats::EndRenderFrameStats()
{
	m_StatsEnabled = false;
	m_RenderFrameTime += GetTimeSinceStartup() - m_EnableStartTime;
}

void GfxDeviceStats::CopyAllDrawStats( const GfxDeviceStats& s )
{
	m_Draw = s.m_Draw;
	m_Changes = s.m_Changes;
	m_RenderFrameTime = s.m_RenderFrameTime;
}

void GfxDeviceStats::CopyClientStats( const GfxDeviceStats& s )
{
	m_Client = s.m_Client;
	m_ClientFrameTime = s.m_ClientFrameTime;
}

#if UNITY_IPHONE || UNITY_ANDROID
ABSOLUTE_TIME GfxDeviceStats::GetHighResolutionAbsTime ()
{
#if UNITY_IPHONE
	return mach_absolute_time ();
#elif UNITY_ANDROID
	timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	long long nanosecs = ts.tv_sec;
	nanosecs <<= 32;
	nanosecs |= ts.tv_nsec;
	return nanosecs;
#endif
}
#endif
