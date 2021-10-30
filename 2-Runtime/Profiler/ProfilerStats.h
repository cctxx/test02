#ifndef _PROFILERSTATS_H_
#define _PROFILERSTATS_H_

#include "Configuration/UnityConfigure.h"

enum ValueFormat
{
	kFormatTime, // milliseconds
	kFormatCount, // number (optionally with k/M)
	kFormatBytes, // number b/kB/m
	kFormatPercentage // Percentage in % * 10  as an int, so that we can represent 10.1%
};

enum ProfilerArea
{
	kProfilerAreaCPU,
	kProfilerAreaGPU,
	kProfilerAreaRendering,
	kProfilerAreaMemory,
	kProfilerAreaAudio,
	kProfilerAreaPhysics,
	kProfilerAreaPhysics2D,
	kProfilerAreaDebug,
	kProfilerAreaCount = kProfilerAreaDebug
};

#if ENABLE_PROFILER

#include "TimeHelper.h"
#include "Runtime/Utilities/dynamic_array.h"

struct MemoryStats
{
	// used bytes: Total, unity(-profiler), mono, DX/OGL, Profiler, FMOD??, Executable??
	// reserved bytes: Total, unity, mono, DX/OGL, Profiler, FMOD??, Executable??
	size_t bytesUsedTotal;
	size_t bytesUsedUnity;
	size_t bytesUsedMono;
	size_t bytesUsedGFX;
	size_t bytesUsedFMOD;
	size_t bytesUsedProfiler;

	size_t bytesReservedTotal;
	size_t bytesReservedUnity;
	size_t bytesReservedMono;
	size_t bytesReservedGFX;
	size_t bytesReservedFMOD;
	size_t bytesReservedProfiler;
	
	size_t bytesVirtual;
	size_t bytesCommitedLimit;
	size_t bytesCommitedTotal;

	int bytesUsedDelta;
	
	int textureCount;
	int textureBytes;
	
	int meshCount;
	int meshBytes;

	int materialCount;
	int materialBytes;

	int animationClipCount;
	int animationClipBytes;
	
	int audioCount;
	int audioBytes;

	int assetCount;
	int sceneObjectCount;
	int gameObjectCount;

	int totalObjectsCount;
	int profilerMemUsed;
	int profilerNumAllocations;
	// NB! Everything above here will be cleared with a memset() in the constructor! ^^^^^^^^^^^^
	dynamic_array<int> classCount;
	ProfilerString memoryOverview;

	MemoryStats () : classCount(kMemProfiler) { memset(this, 0, ptrdiff_t(&classCount) - ptrdiff_t(this)); classCount.clear(); memoryOverview.clear(); }
#if ENABLE_PLAYERCONNECTION
	void Serialize( dynamic_array<int>& bitstream );
	void Deserialize( int** bitstream, bool swapdata );
#endif
	
	ProfilerString ToString () const;
};

struct DrawStats
{
	int drawCalls;
	int triangles;
	int vertices;
	
	int batchedDrawCalls;
	int batchedTriangles;
	int batchedVertices;

	int shadowCasters;
	
	int usedTextureCount;
	int usedTextureBytes;
	
	int renderTextureCount;
	int renderTextureBytes;
	int renderTextureStateChanges;
	
	int screenWidth;
	int screenHeight;
	int screenFSAA;
	int screenBytes;
	
	int vboTotal;
	int vboTotalBytes;
	int vboUploads;
	int vboUploadBytes;
	int ibUploads;
	int ibUploadBytes;		
	
	int visibleSkinnedMeshes;

	int totalAvailableVRamMBytes;

	//		int textureStateChanges;
	//		int lightStateChanges;
	//		int pixelShaderStateChanges;
	//		int vertexShaderStateChanges;
	
	DrawStats () { memset(this, 0, sizeof(*this)); }
	ProfilerString GetScreenResString ()  const;
	
	
	ProfilerString ToString () const;
};

struct PhysicsStats
{
	int activeRigidbodies;	
	int sleepingRigidbodies;	
	
	int numberOfShapePairs;	
	
	int numberOfStaticColliders;	
	int numberOfDynamicColliders;	
	
	PhysicsStats () { memset(this, 0, sizeof(*this)); }
};

struct Physics2DStats
{
	int m_TotalBodyCount;
	int m_ActiveBodyCount;
	int m_SleepingBodyCount;
	int m_DynamicBodyCount;
	int m_KinematicBodyCount;
	int m_DiscreteBodyCount;
	int m_ContinuousBodyCount;
	int m_JointCount;
	int m_ContactCount;
	int m_ActiveColliderShapesCount;
	int m_SleepingColliderShapesCount;

	int m_StepTime;
	int m_CollideTime;
	int m_SolveTime;
	int m_SolveInitialization;
	int m_SolveVelocity;
	int m_SolvePosition;
	int m_SolveBroadphase;
	int m_SolveTimeOfImpact;

	Physics2DStats () { memset(this, 0, sizeof(*this)); }
};

struct DebugStats
{
	DebugStats () { memset(this, 0, sizeof(*this)); }
	
	int 	          m_ProfilerMemoryUsage;
	int 	          m_ProfilerMemoryUsageOthers;
	int               m_AllocatedProfileSamples;
	
	ProfilerString ToString () const;
#if ENABLE_PLAYERCONNECTION
	void Serialize( dynamic_array<int>& bitstream );
	void Deserialize( int** bitstream );
#endif
};

struct AudioStats
{
	AudioStats () { memset(this, 0, sizeof(*this)); }
	
	int playingSources;
	int pausedSources;
	
	int audioCPUusage;
	int audioMemUsage;
	int audioMaxMemUsage;
	int audioVoices;

	int audioClipCount;
	int audioSourceCount;

	unsigned int audioMemDetailsUsage;

	struct Details 
	{
		unsigned int other;                          /* [out] Memory not accounted for by other types */
		unsigned int string;                         /* [out] String data */
		unsigned int system;                         /* [out] System object and various internals */
		unsigned int plugins;                        /* [out] Plugin objects and internals */
		unsigned int output;                         /* [out] Output module object and internals */
		unsigned int channel;                        /* [out] Channel related memory */
		unsigned int channelgroup;                   /* [out] ChannelGroup objects and internals */
		unsigned int codec;                          /* [out] Codecs allocated for streaming */
		unsigned int file;                           /* [out] File buffers and structures */
		unsigned int sound;                          /* [out] Sound objects and internals */
		unsigned int secondaryram;                   /* [out] Sound data stored in secondary RAM */
		unsigned int soundgroup;                     /* [out] SoundGroup objects and internals */
		unsigned int streambuffer;                   /* [out] Stream buffer memory */
		unsigned int dspconnection;                  /* [out] DSPConnection objects and internals */
		unsigned int dsp;                            /* [out] DSP implementation objects */
		unsigned int dspcodec;                       /* [out] Realtime file format decoding DSP objects */
		unsigned int profile;                        /* [out] Profiler memory footprint. */
		unsigned int recordbuffer;                   /* [out] Buffer used to store recorded data from microphone */
		unsigned int reverb;                         /* [out] Reverb implementation objects */
		unsigned int reverbchannelprops;             /* [out] Reverb channel properties structs */
		unsigned int geometry;                       /* [out] Geometry objects and internals */
		unsigned int syncpoint;                      /* [out] Sync point memory. */
		unsigned int eventsystem;                    /* [out] EventSystem and various internals */
		unsigned int musicsystem;                    /* [out] MusicSystem and various internals */
		unsigned int fev;                            /* [out] Definition of objects contained in all loaded projects e.g. events, groups, categories */
		unsigned int memoryfsb;                      /* [out] Data loaded with preloadFSB */
		unsigned int eventproject;                   /* [out] EventProject objects and internals */
		unsigned int eventgroupi;                    /* [out] EventGroup objects and internals */
		unsigned int soundbankclass;                 /* [out] Objects used to manage wave banks */
		unsigned int soundbanklist;                  /* [out] Data used to manage lists of wave bank usage */
		unsigned int streaminstance;                 /* [out] Stream objects and internals */
		unsigned int sounddefclass;                  /* [out] Sound definition objects */
		unsigned int sounddefdefclass;               /* [out] Sound definition static data objects */
		unsigned int sounddefpool;                   /* [out] Sound definition pool data */
		unsigned int reverbdef;                      /* [out] Reverb definition objects */
		unsigned int eventreverb;                    /* [out] Reverb objects */
		unsigned int userproperty;                   /* [out] User property objects */
		unsigned int eventinstance;                  /* [out] Event instance base objects */
		unsigned int eventinstance_complex;          /* [out] Complex event instance objects */
		unsigned int eventinstance_simple;           /* [out] Simple event instance objects */
		unsigned int eventinstance_layer;            /* [out] Event layer instance objects */
		unsigned int eventinstance_sound;            /* [out] Event sound instance objects */
		unsigned int eventenvelope;                  /* [out] Event envelope objects */
		unsigned int eventenvelopedef;               /* [out] Event envelope definition objects */
		unsigned int eventparameter;                 /* [out] Event parameter objects */
		unsigned int eventcategory;                  /* [out] Event category objects */
		unsigned int eventenvelopepoint;             /* [out] Event envelope point objects */
		unsigned int eventinstancepool;              /* [out] Event instance pool memory */
	};

	Details audioMemDetails;
};

// Stores samples for profiler charts
struct ChartSample
{
	ChartSample () { memset(this, 0, sizeof(*this)); }
	
	int rendering;
	int scripts;
	int physics;
	int gc;
	int vsync;
	int others;

	int gpuOpaque;
	int gpuTransparent;
	int gpuShadows;
	int gpuPostProcess;
	int gpuDeferredPrePass;
	int gpuDeferredLighting;
	int gpuOther;
	
	int hasGPUProfiler;
};

struct AllProfilerStats
{
	MemoryStats  memoryStats;
	DrawStats    drawStats;
	PhysicsStats physicsStats;
	Physics2DStats physics2DStats;
	DebugStats   debugStats;
	AudioStats   audioStats;
	ChartSample  chartSample;
	ChartSample  chartSampleSelected;
#if ENABLE_PLAYERCONNECTION
	void Serialize( dynamic_array<int>& bitstream );
	void Deserialize( int** bitstream, bool swapdata );
#endif
};

struct StatisticsProperty
{
	std::string	name;
	int			offset;
	ValueFormat	format;
	ProfilerArea area;
	bool         showGraph;
};


void InitializeStatisticsProperties (dynamic_array<StatisticsProperty>& statisticProperties);

inline int GetStatisticsValue (int offset, AllProfilerStats& stats)
{
	AssertIf( offset < 0 || offset >= sizeof(AllProfilerStats) );
	UInt8* dataPtr = reinterpret_cast<UInt8*>(&stats) + offset;
	return *reinterpret_cast<int*> (dataPtr);
}

#endif
#endif
