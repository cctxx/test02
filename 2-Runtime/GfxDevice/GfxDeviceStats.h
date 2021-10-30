#pragma once

#include "GfxDeviceTypes.h"
#include "Runtime/Profiler/TimeHelper.h"

#if UNITY_IPHONE || UNITY_ANDROID || UNITY_WII
#define CHECK_STATS_ENABLED
#else
#define CHECK_STATS_ENABLED if( !m_StatsEnabled ) return;
#endif

class GfxDeviceStats {
public:
	struct StateStats {
		void Reset();
		
		int renderTexture;
		int vboUploads;
		int vboUploadBytes;
		int ibUploads;
		int ibUploadBytes;
	};
	struct ClientStats {
		// Main thread stats
		void Reset();

		int shadowCasters;

		ABSOLUTE_TIME cullingDt;
		ABSOLUTE_TIME clearDt;
	};
	struct DrawStats {
		// Render thread stats
		void Reset();
		
		int calls;
		int tris, trisSent;
		int verts;
		int batches;
		int batchedCalls;
		int batchedTris;
		int batchedVerts;

		ABSOLUTE_TIME dt;
		ABSOLUTE_TIME batchDt;

		int usedTextureCount;
		int usedTextureBytes;

		#if ENABLE_PROFILER
		typedef std::set<TextureID, std::less<TextureID>, STL_ALLOCATOR(kMemProfiler, TextureID) > TextureIDSet;
		TextureIDSet usedTextures;  
		#endif
	};
	struct MemoryStats {
		int screenWidth, screenHeight;
		int screenFrontBPP, screenBackBPP, screenDepthBPP;
		int	screenFSAA; // actual level of anti-aliasing used
		int screenBytes; // memory for backbuffer + frontbuffer
		SInt64 renderTextureBytes;
	};
	
	GfxDeviceStats();
	
	void AddRenderTextureChange() { CHECK_STATS_ENABLED ++m_Changes.renderTexture; }
	void AddUploadVBO(int vertexData) { CHECK_STATS_ENABLED ++m_Changes.vboUploads; m_Changes.vboUploadBytes += vertexData;  }
	void AddUploadIB(int indexData) { CHECK_STATS_ENABLED ++m_Changes.ibUploads; m_Changes.ibUploadBytes += indexData; }
	#if ENABLE_PROFILER
	void AddUsedTexture(TextureID tex);
	#else
	void AddUsedTexture(TextureID tex) {}
	#endif
	
	void AddDrawCall( int tris, int verts, ABSOLUTE_TIME dt, int trisSent = -1) { CHECK_STATS_ENABLED
		++m_Draw.calls; m_Draw.tris += tris; m_Draw.verts += verts; m_Draw.dt = COMBINED_TIME(m_Draw.dt, dt);
		m_Draw.trisSent += ((trisSent > 0)? trisSent: tris);
	}
	void AddDrawCall( int tris, int verts, int trisSent = -1) { CHECK_STATS_ENABLED
		ABSOLUTE_TIME dt;
		ABSOLUTE_TIME_INIT(dt);
		AddDrawCall(tris, verts, dt, trisSent);
	}
	void AddBatch(int trisSent, int batchedVerts, int batchedCalls, ABSOLUTE_TIME batchDt) { CHECK_STATS_ENABLED
		m_Draw.batches++;
		m_Draw.batchedCalls += batchedCalls; m_Draw.batchDt = COMBINED_TIME(m_Draw.batchDt, batchDt);
		m_Draw.batchedTris += trisSent;
		m_Draw.batchedVerts += batchedVerts;
	}
	void AddCulling(ABSOLUTE_TIME cullingDt) { CHECK_STATS_ENABLED
		m_Client.cullingDt = COMBINED_TIME(m_Client.cullingDt, cullingDt);
	}
	void AddClear(ABSOLUTE_TIME clearDt) { CHECK_STATS_ENABLED
		m_Client.clearDt = COMBINED_TIME(m_Client.clearDt, clearDt);
	}
	void AddShadowCaster() { CHECK_STATS_ENABLED
		m_Client.shadowCasters++;
	}
	void AddShadowCasters(int count) { CHECK_STATS_ENABLED
		m_Client.shadowCasters+=count;
	}

	
	virtual void SetScreenParams( int width, int height, int backbufferBPP, int frontbufferBPP, int depthBPP, int fsaa );

	void ChangeRenderTextureBytes (int deltaBytes)
	{
		SInt64 deltaBytes64 = static_cast<SInt64>(deltaBytes);
		m_Memory.renderTextureBytes += deltaBytes64;
		Assert (m_Memory.renderTextureBytes >= 0);
	}
	
	const StateStats& GetStateChanges() const { return m_Changes; }
	const DrawStats& GetDrawStats() const { return m_Draw; }
	const ClientStats& GetClientStats() const { return m_Client; }
	const MemoryStats& GetMemoryStats() const { return m_Memory; }
	
	float GetClientFrameTime() const { return m_ClientFrameTime; }
	float GetRenderFrameTime() const { return m_RenderFrameTime; }
	void AddToClientFrameTime (float dt) { m_ClientFrameTime += dt; }

	// Calculates texture memory usage, and clears the set (to avoid copying the set in multithreaded rendering)
	virtual void AccumulateUsedTextureUsage();
		
#if UNITY_IPHONE || UNITY_ANDROID
	ABSOLUTE_TIME GetHighResolutionAbsTime ();
#endif

private:
	friend class GfxDevice;
	friend class GfxDeviceClient;
	friend class GfxDeviceWorker;

	// Changing stats from the application should always be done through the device
	void ResetFrame();
	void ResetClientStats();
	void BeginFrameStats();
	void EndClientFrameStats();
	void EndRenderFrameStats();

	// Saves/restores everything except the memory stats (those have to be properly tracked
	// all the time).
	void CopyAllDrawStats( const GfxDeviceStats& s );
	void CopyClientStats( const GfxDeviceStats& s );

private:
	StateStats	m_Changes;
	DrawStats	m_Draw;
	ClientStats	m_Client;
	MemoryStats m_Memory;
	double		m_EnableStartTime;
	float		m_ClientFrameTime;
	float		m_RenderFrameTime;
	bool		m_StatsEnabled;
};
