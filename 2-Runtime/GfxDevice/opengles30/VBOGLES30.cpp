#include "UnityPrefix.h"

#if GFX_SUPPORTS_OPENGLES30
#include "VBOGLES30.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "IncludesGLES30.h"
#include "AssertGLES30.h"
#include "GpuProgramsGLES30.h"
#include "DebugGLES30.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/GfxDevice/GLESChannels.h"
#include "Runtime/GfxDevice/GLDataBufferCommon.h"
#include "Runtime/Profiler/MemoryProfiler.h"

#include <algorithm>

#if 1
	#define DBG_LOG_VBO_GLES30(...) {}
#else
	#define DBG_LOG_VBO_GLES30(...) {printf_console(__VA_ARGS__);printf_console("\n");}
#endif

enum
{
	kDefaultBufferAlign		= 64
};

template <typename T>
inline T Align (T v, size_t alignment)
{
	return (v + (alignment-1)) & ~(alignment-1);
}

template <typename T>
inline T AlignToDefault (T v)
{
	return Align(v, kDefaultBufferAlign);
}

// Comparison operators for VertexArrayInfoGLES30 (used in caching)

static inline bool operator< (const VertexInputInfoGLES30& a, const VertexInputInfoGLES30& b)
{
	return a.componentType	< b.componentType	&&
		   a.numComponents	< b.numComponents	&&
		   a.pointer		< b.pointer			&&
		   a.stride			< b.stride;
}

static inline bool operator!= (const VertexInputInfoGLES30& a, const VertexInputInfoGLES30& b)
{
	return a.componentType	!= b.componentType	||
		   a.numComponents	!= b.numComponents	||
		   a.pointer		!= b.pointer		||
		   a.stride			!= b.stride;
}

static inline bool operator== (const VertexInputInfoGLES30& a, const VertexInputInfoGLES30& b) { return !(a != b); }

static bool operator< (const VertexArrayInfoGLES30& a, const VertexArrayInfoGLES30& b)
{
	if		(a.enabledArrays < b.enabledArrays) return true;
	else if	(b.enabledArrays < a.enabledArrays) return false;

	// Compare buffers first as they are more likely to not match.
	for (int ndx = 0; ndx < kGLES3MaxVertexAttribs; ndx++)
	{
		if (a.enabledArrays & (1<<ndx))
		{
			if		(a.buffers[ndx] < b.buffers[ndx]) return true;
			else if	(b.buffers[ndx] < a.buffers[ndx]) return false;
		}
	}

	for (int ndx = 0; ndx < kGLES3MaxVertexAttribs; ndx++)
	{
		if (a.enabledArrays & (1<<ndx))
		{
			if		(a.arrays[ndx] < b.arrays[ndx]) return true;
			else if	(b.arrays[ndx] < a.arrays[ndx]) return false;
		}
	}

	return false; // Equal
}

static bool operator!= (const VertexArrayInfoGLES30& a, const VertexArrayInfoGLES30& b)
{
	if (a.enabledArrays != b.enabledArrays)
		return true;

	// Compare buffers first as they are more likely to not match.
	for (int ndx = 0; ndx < kGLES3MaxVertexAttribs; ndx++)
	{
		if (a.enabledArrays & (1<<ndx))
		{
			if (a.buffers[ndx] != b.buffers[ndx])
				return true;
		}
	}

	for (int ndx = 0; ndx < kGLES3MaxVertexAttribs; ndx++)
	{
		if (a.enabledArrays & (1<<ndx))
		{
			if (a.arrays[ndx] != b.arrays[ndx])
				return true;
		}
	}

	return false;
}

static bool operator== (const VertexArrayInfoGLES30& a, const VertexArrayInfoGLES30& b) { return !(a != b); }

struct CompareVertexArrayInfoGLES30
{
	bool operator() (const VertexArrayInfoGLES30* a, const VertexArrayInfoGLES30* b) const;
};

class VertexArrayObjectGLES30
{
public:
									VertexArrayObjectGLES30		(const VertexArrayInfoGLES30& info);
									~VertexArrayObjectGLES30	(void);

	UInt32							GetVAO						(void) const { return m_vao;	}
	const VertexArrayInfoGLES30*	GetInfo						(void) const { return &m_info;	}

private:
									VertexArrayObjectGLES30		(const VertexArrayObjectGLES30& other); // Not allowed!
	VertexArrayObjectGLES30&		operator=					(const VertexArrayObjectGLES30& other); // Not allowed!

	VertexArrayInfoGLES30			m_info;
	UInt32							m_vao;
};

typedef std::map<const VertexArrayInfoGLES30*, VertexArrayObjectGLES30*, CompareVertexArrayInfoGLES30> VertexArrayMapGLES30;

// Utilities

static const GLenum kTopologyGLES3[] =
{
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLES,
	GL_LINES,
	GL_LINE_STRIP,
	GL_POINTS,
};
typedef char kTopologyGLES3SizeAssert[ARRAY_SIZE(kTopologyGLES3) == kPrimitiveTypeCount ? 1 : -1];

static const GLenum kVertexTypeGLES3[] =
{
	GL_FLOAT,			// kChannelFormatFloat
	GL_HALF_FLOAT,		// kChannelFormatFloat16
	GL_UNSIGNED_BYTE,	// kChannelFormatColor
	GL_BYTE,			// kChannelFormatByte
};
typedef char kVertexTypeGLES3Assert[ARRAY_SIZE(kVertexTypeGLES3) == kChannelFormatCount ? 1 : -1];

// Targets by attribute location
static const VertexComponent kVertexCompTargetsGLES3[] =
{
	// \note Indices must match to attribute locations.
	kVertexCompVertex,
	kVertexCompColor,
	kVertexCompNormal,
	kVertexCompTexCoord0,
	kVertexCompTexCoord1,
	kVertexCompTexCoord2,
	kVertexCompTexCoord3,
	kVertexCompTexCoord4,
	kVertexCompTexCoord5,
	kVertexCompTexCoord6,
	kVertexCompTexCoord7
};
typedef char kVertexCompTargetsGLES3Assert[ARRAY_SIZE(kVertexCompTargetsGLES3) == kGLES3MaxVertexAttribs ? 1 : -1];

// For some reason GfxDevice needs to know whether VBO contains color
// data in order to set up state properly. So this must be called before
// doing BeforeDrawCall().
void VBOContainsColorGLES30 (bool flag); // defined in GfxDeviceGLES30.cpp

static bool IsVertexDataValid (const VertexBufferData& buffer)
{
	// Verify streams.
	{
		UInt32 shaderChannels = 0;
		for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
		{
			if ((buffer.streams[streamNdx].channelMask == 0) != (buffer.streams[streamNdx].stride == 0))
				return false; // No data but enabled channels, or other way around.

			if ((shaderChannels & buffer.streams[streamNdx].channelMask) != 0)
				return false; // Duplicate channels!

			shaderChannels |= buffer.streams[streamNdx].channelMask;
		}
	}

	// Make sure channels point to correct streams.
	for (int chanNdx = 0; chanNdx < kShaderChannelCount; chanNdx++)
	{
		if (buffer.channels[chanNdx].dimension == ChannelInfo::kInvalidDimension)
			continue;

		int streamNdx = buffer.channels[chanNdx].stream;

		if (streamNdx < 0 || streamNdx >= kMaxVertexStreams)
			return false;

		if (buffer.streams[streamNdx].channelMask & (1<<chanNdx) == 0)
			return false; // No such channel in stream.
	}

	// Verify that streams do not overlap (or otherwise we waste memory).
	// \todo [pyry] O(n^2), but then again kMaxVertexStreams = 4..
	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		if (buffer.streams[streamNdx].channelMask == 0)
			continue;

		const int	start		= buffer.streams[streamNdx].offset;
		const int	end			= start + buffer.streams[streamNdx].stride*buffer.vertexCount;

		for (int otherStreamNdx = 0; otherStreamNdx < kMaxVertexStreams; otherStreamNdx++)
		{
			if (otherStreamNdx == streamNdx ||
				buffer.streams[otherStreamNdx].channelMask == 0)
				continue;

			const int	otherStart	= buffer.streams[otherStreamNdx].offset;
			const int	otherEnd	= otherStart + buffer.streams[otherStreamNdx].stride*buffer.vertexCount;

			if ((start <= otherStart && otherStart < end) || // Start lies inside buffer
				(start < otherEnd && otherEnd <= end)) // End lies inside buffer
				return false;
		}
	}

	return true;
}

static bool IsVertexArrayNormalized (int attribNdx, VertexChannelFormat format)
{
	if (attribNdx == 0)
		return false; // Position is never normalized.
	else
		return (format != kChannelFormatFloat && format != kChannelFormatFloat16);
}

// VAOCacheGLES30

VAOCacheGLES30::VAOCacheGLES30 (void)
	: m_NextEntryNdx(0)
{
}

VAOCacheGLES30::~VAOCacheGLES30 (void)
{
	Clear();
}

void VAOCacheGLES30::Clear (void)
{
	for (int ndx = 0; ndx < kCacheSize; ndx++)
	{
		delete m_Entries[ndx].vao;
		m_Entries[ndx].vao	= 0;
		m_Entries[ndx].key	= VAOCacheKeyGLES30();
	}

	m_NextEntryNdx = 0; // Not necessary, but this will place first VAOs in first slots.
}

inline const VertexArrayObjectGLES30* VAOCacheGLES30::Find (const VAOCacheKeyGLES30& key) const
{
	for (int ndx = 0; ndx < kCacheSize; ndx++)
	{
		if (m_Entries[ndx].key == key)
			return m_Entries[ndx].vao;
	}

	return 0;
}

void VAOCacheGLES30::Insert (const VAOCacheKeyGLES30& key, VertexArrayObjectGLES30* vao)
{
	Assert(!Find(key));

	// Replace last
	delete m_Entries[m_NextEntryNdx].vao;
	m_Entries[m_NextEntryNdx].key = key;
	m_Entries[m_NextEntryNdx].vao = vao;
	m_NextEntryNdx = (m_NextEntryNdx + 1) % kCacheSize;
}

bool VAOCacheGLES30::IsFull (void) const
{
	return m_Entries[m_NextEntryNdx].vao != 0;
}

// VertexArrayObjectGLES30

VertexArrayObjectGLES30::VertexArrayObjectGLES30 (const VertexArrayInfoGLES30& info)
	: m_info(info)
	, m_vao	(0)
{
	UInt32 boundBuffer = 0;

	GLES_CHK(glGenVertexArrays(1, (GLuint*)&m_vao));
	GLES_CHK(glBindVertexArray(m_vao));

	for (int attribNdx = 0; attribNdx < kGLES3MaxVertexAttribs; attribNdx++)
	{
		if ((info.enabledArrays & (1<<attribNdx)) == 0)
			continue;

		const UInt32	buffer				= info.buffers[attribNdx];
		const int		numComponents		= info.arrays[attribNdx].numComponents;
		const GLenum	compType			= kVertexTypeGLES3[info.arrays[attribNdx].componentType];
		const bool		normalized			= IsVertexArrayNormalized(attribNdx, (VertexChannelFormat)info.arrays[attribNdx].componentType);
		const int		stride				= info.arrays[attribNdx].stride;
		const void*		pointer				= info.arrays[attribNdx].pointer;

		if (buffer != boundBuffer)
		{
			GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, buffer));
			boundBuffer = buffer;
		}

		GLES_CHK(glEnableVertexAttribArray(attribNdx));
		GLES_CHK(glVertexAttribPointer(attribNdx, numComponents, compType, normalized ? GL_TRUE : GL_FALSE, stride, pointer));
	}

	GLES_CHK(glBindVertexArray(0));
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

VertexArrayObjectGLES30::~VertexArrayObjectGLES30 (void)
{
	glDeleteVertexArrays(1, (const GLuint*)&m_vao);
}

// GLES3VBO

GLES3VBO::GLES3VBO (void)
	: m_IndexBuffer(0)
{
}

GLES3VBO::~GLES3VBO (void)
{
	Cleanup();
}

static UInt32 MapStreamModeToBufferUsage (VBO::StreamMode mode)
{
	switch (mode)
	{
		case VBO::kStreamModeNoAccess:		return GL_STATIC_DRAW; // ???
		case VBO::kStreamModeWritePersist:	return GL_STATIC_DRAW;
		case VBO::kStreamModeDynamic:		return GL_STREAM_DRAW;
		default:
			return GL_STATIC_DRAW;
	}
}

inline DataBufferGLES30* GLES3VBO::GetCurrentBuffer (int streamNdx)
{
	return m_StreamBuffers[streamNdx].buffers[m_StreamBuffers[streamNdx].curBufferNdx];
}

void GLES3VBO::UpdateVertexData (const VertexBufferData& buffer)
{
	Assert(IsVertexDataValid(buffer));

	bool	streamHasData[kMaxVertexStreams];
	int		streamBufSize[kMaxVertexStreams];

	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		const bool	hasData		= (buffer.streams[streamNdx].channelMask != 0);
		const int	size		= buffer.streams[streamNdx].stride * buffer.vertexCount;

		Assert(hasData == (size != 0));

		streamHasData[streamNdx]	= hasData;
		streamBufSize[streamNdx]	= size;
	}

	// Discard all existing buffers.
	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		Stream& stream = m_StreamBuffers[streamNdx];

		for (int bufNdx = 0; bufNdx < kBufferSwapChainSize; bufNdx++)
		{
			if (stream.buffers[bufNdx])
			{
				stream.buffers[bufNdx]->Release();
				stream.buffers[bufNdx] = 0;
			}
		}

		UNITY_FREE(kMemVertexData, stream.cpuBuf);
		stream.cpuBuf = 0;
	}

	m_VAOCache.Clear(); // VAO cache must be cleared

	// Allocate buffers and upload data.
	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		if (!streamHasData[streamNdx])
			continue;

		const UInt32	usage		= MapStreamModeToBufferUsage((StreamMode)m_StreamModes[streamNdx]);
		const int		size		= streamBufSize[streamNdx];
		const UInt8*	streamData	= buffer.buffer + buffer.streams[streamNdx].offset;
		Stream&			dstStream	= m_StreamBuffers[streamNdx];

		Assert(!dstStream.buffers[dstStream.curBufferNdx]);

		dstStream.buffers[dstStream.curBufferNdx] = GetBufferManagerGLES30()->AcquireBuffer(size, usage);
		dstStream.buffers[dstStream.curBufferNdx]->RecreateWithData(size, usage, streamData);

		dstStream.channelMask	= buffer.streams[streamNdx].channelMask;
		dstStream.stride		= buffer.streams[streamNdx].stride;

		// \todo [2013-06-19 pyry] Allocate cpuBuf on-demand once it is not required for Recreate()
		dstStream.cpuBuf		= (UInt8*)UNITY_MALLOC(kMemVertexData, size);
		::memcpy(dstStream.cpuBuf, streamData, size);
	}

	m_VertexCount = buffer.vertexCount;
	memcpy(&m_Channels[0], &buffer.channels[0], sizeof(ChannelInfoArray));
}

void GLES3VBO::UpdateIndexData (const IndexBufferData& buffer)
{
	const int		bufferSize		= CalculateIndexBufferSize(buffer);
	const UInt32	bufferUsage		= m_IndicesDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

	Assert(bufferSize > 0);

	if (m_IndexBuffer && BufferUpdateCausesStallGLES30(m_IndexBuffer))
	{
		m_IndexBuffer->Release();
		m_IndexBuffer = 0;
	}

	if (!m_IndexBuffer)
		m_IndexBuffer = GetBufferManagerGLES30()->AcquireBuffer(bufferSize, bufferUsage);

	m_IndexBuffer->RecreateWithData(bufferSize, bufferUsage, buffer.indices);

	// Take copy for emulating quads.
	Assert(kVBOIndexSize == sizeof(UInt16));
	m_Indices.resize(buffer.count);
	std::copy((const UInt16*)buffer.indices, (const UInt16*)buffer.indices + buffer.count, m_Indices.begin());
}

bool GLES3VBO::MapVertexStream (VertexStreamData& outData, unsigned stream)
{
	Assert(0 <= stream && stream < kMaxVertexStreams);
	Assert(!m_IsStreamMapped[stream]); // \note Multiple mappings will screw up buffer swap chain.
	Assert(m_StreamBuffers[stream].buffers[m_StreamBuffers[stream].curBufferNdx]);

	Stream&		mapStream	= m_StreamBuffers[stream];
	const int	size		= m_VertexCount * mapStream.stride;
	void*		mapPtr		= 0;

	if (BufferUpdateCausesStallGLES30(mapStream.buffers[mapStream.curBufferNdx]))
	{
		// Advance to next slot.
		mapStream.curBufferNdx = (mapStream.curBufferNdx + 1) % kBufferSwapChainSize;

		if (!mapStream.buffers[mapStream.curBufferNdx])
		{
			const UInt32		usage		= MapStreamModeToBufferUsage((StreamMode)m_StreamModes[stream]);
			DataBufferGLES30*	mapBuffer	= GetBufferManagerGLES30()->AcquireBuffer(size, usage);

			if (mapBuffer->GetSize() < size)
				mapBuffer->RecreateStorage(size, usage);

			mapStream.buffers[mapStream.curBufferNdx] = mapBuffer;
		}
	}

	if (gGraphicsCaps.gles30.useMapBuffer)
	{
		// \note Using write-only mapping always as map is not guaranteed to return old contents anyway.
		mapPtr = mapStream.buffers[mapStream.curBufferNdx]->Map(0, size, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
	}
	else
	{
		// \todo [2013-06-19 pyry] Allocate cpuBuf on-demand once it is not required for Recreate()
		Assert(mapStream.cpuBuf);
		mapPtr = mapStream.cpuBuf;
	}

	outData.channelMask	= mapStream.channelMask;
	outData.stride		= mapStream.stride;
	outData.vertexCount	= m_VertexCount;
	outData.buffer		= (UInt8*)mapPtr;

	m_IsStreamMapped[stream] = true;

	return true;
}

void GLES3VBO::UnmapVertexStream (unsigned stream)
{
	Assert(0 <= stream && stream < kMaxVertexStreams);
	Assert(m_IsStreamMapped[stream]);

	if (gGraphicsCaps.gles30.useMapBuffer)
	{
		GetCurrentBuffer(stream)->Unmap();
	}
	else
	{
		const int		size	= m_StreamBuffers[stream].stride*m_VertexCount;
		const UInt32	usage	= MapStreamModeToBufferUsage((StreamMode)m_StreamModes[stream]);

		// \note Buffer slot was advanced in MapVertexStream()
		GetCurrentBuffer(stream)->RecreateWithData(size, usage, m_StreamBuffers[stream].cpuBuf);
	}

	m_IsStreamMapped[stream] = false;
}

void GLES3VBO::Cleanup (void)
{
	if (m_IndexBuffer)
	{
		m_IndexBuffer->Release();
		m_IndexBuffer = 0;
	}

	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		for (int bufNdx = 0; bufNdx < kBufferSwapChainSize; bufNdx++)
		{
			if (m_StreamBuffers[streamNdx].buffers[bufNdx])
			{
				m_StreamBuffers[streamNdx].buffers[bufNdx]->Release();
				m_StreamBuffers[streamNdx].buffers[bufNdx] = 0;
			}
		}

		UNITY_FREE(kMemVertexData, m_StreamBuffers[streamNdx].cpuBuf);
		m_StreamBuffers[streamNdx].cpuBuf = 0;
	}

	m_VAOCache.Clear();
}

void GLES3VBO::Recreate (void)
{
	// \note Called on context loss.

	if (m_IndexBuffer)
	{
		const int		bufferSize	= (int)m_Indices.size() * kVBOIndexSize;
		const UInt32	bufferUsage	= m_IndicesDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

		m_IndexBuffer->Disown();
		delete m_IndexBuffer;

		m_IndexBuffer = GetBufferManagerGLES30()->AcquireBuffer(bufferSize, bufferUsage);
		m_IndexBuffer->RecreateWithData(bufferSize, bufferUsage, &m_Indices[0]);
	}

	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		Stream&			stream		= m_StreamBuffers[streamNdx];
		const int		bufSize		= stream.stride*m_VertexCount;
		const UInt32	usage		= MapStreamModeToBufferUsage((StreamMode)m_StreamModes[streamNdx]);

		for (int bufNdx = 0; bufNdx < kBufferSwapChainSize; bufNdx++)
		{
			if (stream.buffers[bufNdx])
			{
				stream.buffers[bufNdx]->Disown();
				delete stream.buffers[bufNdx];
				stream.buffers[bufNdx] = 0;
			}
		}

		stream.curBufferNdx = 0;

		if (bufSize > 0)
		{
			Assert(stream.cpuBuf);
			stream.buffers[0] = GetBufferManagerGLES30()->AcquireBuffer(bufSize, usage);
			stream.buffers[0]->RecreateWithData(bufSize, usage, stream.cpuBuf);
		}
	}

	m_VAOCache.Clear();
}

bool GLES3VBO::IsVertexBufferLost (void) const
{
	return false;
}

bool GLES3VBO::IsUsingSourceVertices (void) const
{
	return false;
}

bool GLES3VBO::IsUsingSourceIndices (void) const
{
	return true;
}

int GLES3VBO::GetRuntimeMemorySize (void) const
{
	int totalSize = 0;

	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
	{
		for (int bufNdx = 0; bufNdx < kBufferSwapChainSize; bufNdx++)
		{
			if (m_StreamBuffers[streamNdx].buffers[bufNdx])
				totalSize += m_StreamBuffers[streamNdx].buffers[bufNdx]->GetSize();
		}
	}

	if (m_IndexBuffer)
		totalSize += m_IndexBuffer->GetSize();

	return totalSize;
}

void GLES3VBO::ComputeVertexInputState (VertexArrayInfoGLES30& dst, const ChannelAssigns& channelAssigns)
{
	UInt32	availableShaderChannels		= 0; // Channels that have data in any of streams.
	UInt32	enabledTargets				= channelAssigns.GetTargetMap();

	for (int streamNdx = 0; streamNdx < kMaxVertexStreams; streamNdx++)
		availableShaderChannels |= m_StreamBuffers[streamNdx].channelMask;

	for (int attribNdx = 0; attribNdx < kGLES3MaxVertexAttribs; attribNdx++)
	{
		const VertexComponent target = kVertexCompTargetsGLES3[attribNdx];

		if ((enabledTargets & (1<<target)) == 0)
			continue; // Not enabled.

		const ShaderChannel	sourceChannel = channelAssigns.GetSourceForTarget(target);

		// \todo [pyry] Uh, what? Channel is enabled, but no valid source exists.
		if (sourceChannel < 0)
			continue;

		Assert(0 <= sourceChannel && sourceChannel < kShaderChannelCount);

		if ((availableShaderChannels & (1<<sourceChannel)) == 0)
			continue; // Not available.

		const Stream& stream = m_StreamBuffers[m_Channels[sourceChannel].stream];

		dst.arrays[attribNdx].componentType		= m_Channels[sourceChannel].format;
		dst.arrays[attribNdx].numComponents		= m_Channels[sourceChannel].format == kChannelFormatColor ? 4 : m_Channels[sourceChannel].dimension;
		dst.arrays[attribNdx].pointer			= reinterpret_cast<const void*>(m_Channels[sourceChannel].offset);
		dst.arrays[attribNdx].stride			= stream.stride;
		dst.buffers[attribNdx]					= GetCurrentBuffer(m_Channels[sourceChannel].stream)->GetBuffer();

		dst.enabledArrays |= (1<<attribNdx);
	}

	// Fixed-function texgen stuff.
	{
		GfxDevice&	device			= GetRealGfxDevice();
		const int	texArrayBase	= kGLES3AttribLocationTexCoord0;
		const int	maxTexArrays	= std::min(gGraphicsCaps.maxTexUnits, kGLES3MaxVertexAttribs-texArrayBase);
		
		if (device.IsPositionRequiredForTexGen() && (availableShaderChannels & (1 << kShaderChannelVertex)))
		{
			const ChannelInfo&	posInfo		= m_Channels[kShaderChannelVertex];
			const Stream&		posStream	= m_StreamBuffers[posInfo.stream];

			for (int texUnit = 0; texUnit < maxTexArrays; ++texUnit)
			{
				if (device.IsPositionRequiredForTexGen(texUnit))
				{
					const int arrNdx = texArrayBase + texUnit;
					dst.arrays[arrNdx].componentType		= posInfo.format;
					dst.arrays[arrNdx].numComponents		= posInfo.format == kChannelFormatColor ? 4 : posInfo.dimension;
					dst.arrays[arrNdx].pointer				= reinterpret_cast<const void*>(posInfo.offset);
					dst.arrays[arrNdx].stride				= posStream.stride;
					dst.buffers[arrNdx]						= GetCurrentBuffer(posInfo.stream)->GetBuffer();

					dst.enabledArrays |= (1<<arrNdx);
				}
			}
		}

		if (device.IsNormalRequiredForTexGen() && (availableShaderChannels & (1 << kShaderChannelNormal)))
		{
			const ChannelInfo&	normInfo	= m_Channels[kShaderChannelNormal];
			const Stream&		normStream	= m_StreamBuffers[normInfo.stream];

			for (int texUnit = 0; texUnit < maxTexArrays; ++texUnit)
			{
				if (device.IsNormalRequiredForTexGen(texUnit))
				{
					const int arrNdx = texArrayBase + texUnit;
					dst.arrays[arrNdx].componentType		= normInfo.format;
					dst.arrays[arrNdx].numComponents		= normInfo.format == kChannelFormatColor ? 4 : normInfo.dimension;
					dst.arrays[arrNdx].pointer				= reinterpret_cast<const void*>(normInfo.offset);
					dst.arrays[arrNdx].stride				= normStream.stride;
					dst.buffers[arrNdx]						= GetCurrentBuffer(normInfo.stream)->GetBuffer();

					dst.enabledArrays |= (1<<arrNdx);
				}
			}
		}
	}
}

void GLES3VBO::MarkBuffersRendered (const ChannelAssigns& channelAssigns)
{
	// \todo [pyry] Mark based on channel assignments
	for (int ndx = 0; ndx < kMaxVertexStreams; ndx++)
	{
		DataBufferGLES30* buffer = GetCurrentBuffer(ndx);
		if (buffer)
			buffer->RecordRender();
	}
}

void GLES3VBO::DrawVBO (const ChannelAssigns&	channels,
						UInt32					firstIndexByte,
						UInt32					indexCount,
						GfxPrimitiveType		topology,
						UInt32					firstVertex,
						UInt32					vertexCount)
{
	Assert(0 <= firstVertex && firstVertex+vertexCount <= m_VertexCount);

	if (topology == kPrimitiveQuads)
	{
		// Need to emulate - oh well.
		// \todo [2013-05-24 pyry] Cache this?
		const int			numQuads			= indexCount/4;
		const int			emulatedIndexCount	= numQuads * 6;
		const int			emulatedBufSize		= emulatedIndexCount * sizeof(UInt16);
		const UInt32		bufUsage			= GL_DYNAMIC_DRAW;
		std::vector<UInt16>	quadIndices			(emulatedIndexCount);
		DataBufferGLES30*	indexBuffer			= GetBufferManagerGLES30()->AcquireBuffer(emulatedBufSize, bufUsage);

		FillIndexBufferForQuads(&quadIndices[0], emulatedBufSize, (const UInt16*)((UInt8*)&m_Indices[0] + firstIndexByte), numQuads);
		indexBuffer->RecreateWithData(emulatedBufSize, bufUsage, &quadIndices[0]);

		Draw(indexBuffer, channels, kPrimitiveTriangles, emulatedIndexCount, 0, vertexCount);

		indexBuffer->Release();
	}
	else
		Draw(m_IndexBuffer, channels, topology, indexCount, firstIndexByte, vertexCount);
}

void GLES3VBO::DrawCustomIndexed (const ChannelAssigns&	channels,
								  void*					indices,
								  UInt32				indexCount,
								  GfxPrimitiveType		topology,
								  UInt32				vertexRangeBegin,
								  UInt32				vertexRangeEnd,
								  UInt32				drawVertexCount)
{
	Assert(0 <= vertexRangeBegin && vertexRangeBegin <= vertexRangeEnd && vertexRangeEnd <= m_VertexCount);

	// \note Called only in static batching mode which doesn't do quads.
	Assert(topology != kPrimitiveQuads);

	const int			ndxBufSize		= indexCount * kVBOIndexSize;
	const UInt32		ndxBufUsage		= GL_DYNAMIC_DRAW;
	DataBufferGLES30*	indexBuffer		= GetBufferManagerGLES30()->AcquireBuffer(ndxBufSize, ndxBufUsage);

	indexBuffer->RecreateWithData(ndxBufSize, ndxBufUsage, indices);

	Draw(indexBuffer, channels, topology, indexCount, 0, vertexRangeEnd-vertexRangeBegin);

	indexBuffer->Release();
}

void GLES3VBO::Draw (DataBufferGLES30*		indexBuffer,
					 const ChannelAssigns&	channels,
					 GfxPrimitiveType		topology,
					 UInt32					indexCount,
					 UInt32					indexOffset,
					 UInt32					vertexCountForStats)
{
	const bool						useVAO	= true; // \todo [pyry] Get from GfxDevice caps
	const VertexArrayObjectGLES30*	vao		= useVAO ? TryGetVAO(channels) : 0;

	Assert(topology != kPrimitiveQuads);

	// Setup other render state
	VBOContainsColorGLES30(channels.GetSourceForTarget (kVertexCompColor) == kShaderChannelColor);
	GetRealGfxDevice().BeforeDrawCall(false);

	if (vao)
	{
		GLES_CHK(glBindVertexArray(vao->GetVAO()));
	}
	else
	{
		VertexArrayInfoGLES30 vertexState;
		ComputeVertexInputState(vertexState, channels);
		SetupDefaultVertexArrayStateGLES30(vertexState);
	}

	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetBuffer()));

	{
		ABSOLUTE_TIME	drawTime	= START_TIME;
		const int		primCount	= GetPrimitiveCount(indexCount, topology, false);

		GLES_CHK(glDrawElements(kTopologyGLES3[topology], indexCount, GL_UNSIGNED_SHORT, (const void*)indexOffset));

		drawTime = ELAPSED_TIME(drawTime);
		GetRealGfxDevice().GetFrameStats().AddDrawCall(primCount, vertexCountForStats, drawTime);
	}

	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

	if (vao)
	{
		GLES_CHK(glBindVertexArray(0));
	}

	// Record render event for used buffers
	MarkBuffersRendered(channels);
	indexBuffer->RecordRender();
}

const VertexArrayObjectGLES30* GLES3VBO::TryGetVAO (const ChannelAssigns& channels)
{
	// \note [pyry] VAO cache keys don't handle texgen. It is a rare case and we don't want to pay the
	//				cost of extra logic & storage.
	{
		GfxDevice& device = GetRealGfxDevice();
		if (device.IsPositionRequiredForTexGen() || device.IsNormalRequiredForTexGen())
			return 0;
	}

	const VAOCacheKeyGLES30 cacheKey(channels, m_StreamBuffers[0].curBufferNdx,
											   m_StreamBuffers[1].curBufferNdx,
											   m_StreamBuffers[2].curBufferNdx,
											   m_StreamBuffers[3].curBufferNdx);

	const VertexArrayObjectGLES30* cachedVAO = m_VAOCache.Find(cacheKey);

	if (cachedVAO)
	{
		return cachedVAO;
	}
	else if (!m_VAOCache.IsFull())
	{
		DBG_LOG_VBO_GLES30("GLES3VBO::GetVAO(): cache miss, creating new VAO");

		// Map channel assigns + current layout to full vertex state
		VertexArrayInfoGLES30 vertexState;
		ComputeVertexInputState(vertexState, channels);

		VertexArrayObjectGLES30* vao = new VertexArrayObjectGLES30(vertexState);
		m_VAOCache.Insert(cacheKey, vao);

		return vao;
	}
	else
	{
		// If VAO cache gets full, VBO falls back to using default VAO. That
		// way we avoid constantly creating new objects in extreme cases.
		return 0;
	}
}

UInt32 GLES3VBO::GetSkinningTargetVBO (void)
{
	const int	skinStreamNdx	= 0; // \todo [2013-05-31 pyry] Can this change?
	Stream&		skinStream		= m_StreamBuffers[skinStreamNdx];

	Assert(skinStream.buffers[skinStream.curBufferNdx]);

	if (BufferUpdateCausesStallGLES30(skinStream.buffers[skinStream.curBufferNdx]))
	{
		// Move to next slot.
		skinStream.curBufferNdx = (skinStream.curBufferNdx + 1) % kBufferSwapChainSize;

		if (!skinStream.buffers[skinStream.curBufferNdx])
		{
			const UInt32		usage	= GL_STREAM_DRAW;
			const int			size	= m_VertexCount*skinStream.stride;
			DataBufferGLES30*	buffer	= GetBufferManagerGLES30()->AcquireBuffer(size, usage);

			if (buffer->GetSize() < size)
				buffer->RecreateStorage(size, usage);

			skinStream.buffers[skinStream.curBufferNdx] = buffer;
		}
	}

	skinStream.buffers[skinStream.curBufferNdx]->RecordUpdate();
	return skinStream.buffers[skinStream.curBufferNdx]->GetBuffer();
}

DynamicGLES3VBO::DynamicGLES3VBO (void)
	: m_CurVertexBuffer			(0)
	, m_CurIndexBuffer			(0)
	, m_CurRenderMode			((RenderMode)0)
	, m_CurShaderChannelMask	(0)
	, m_CurStride				(0)
	, m_CurVertexCount			(0)
	, m_CurIndexCount			(0)
	, m_QuadArrayIndexBuffer	(0)
{
}

DynamicGLES3VBO::~DynamicGLES3VBO (void)
{
	Cleanup();
}

static UInt32 GetDynamicChunkStride (UInt32 shaderChannelMask)
{
	UInt32 stride = 0;
	for (int i = 0; i < kShaderChannelCount; ++i)
	{
		if (shaderChannelMask & (1<<i))
			stride += VBO::GetDefaultChannelByteSize(i);
	}
	return stride;
}

bool DynamicGLES3VBO::GetChunk (UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB)
{
	Assert(maxVertices < 65536 && maxIndices < 65536*3);
	Assert(!((renderMode == kDrawQuads) && (VBO::kMaxQuads*4 < maxVertices)));
	DebugAssert(outVB != NULL && maxVertices > 0);
	DebugAssert((renderMode == kDrawIndexedQuads			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedPoints			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedLines			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangles		&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangleStrip	&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawTriangleStrip			&& (outIB == NULL && maxIndices == 0)) ||
				(renderMode == kDrawQuads					&& (outIB == NULL && maxIndices == 0)));
	DebugAssert(!m_CurVertexBuffer && !m_CurIndexBuffer);

	const UInt32		stride				= GetDynamicChunkStride(shaderChannelMask);
	const UInt32		usage				= GL_STREAM_DRAW;
	const UInt32		vertexBufferSize	= AlignToDefault(stride*maxVertices);
	const UInt32		indexBufferSize		= AlignToDefault(kVBOIndexSize*maxIndices);

	const bool			useMapBuffer		= gGraphicsCaps.gles30.useMapBuffer;
	const bool			mapVertexBuffer		= useMapBuffer && vertexBufferSize >= kDataBufferThreshold;
	const bool			mapIndexBuffer		= useMapBuffer && indexBufferSize > 0 && indexBufferSize >= kDataBufferThreshold;

	DataBufferGLES30*	vertexBuffer		= mapVertexBuffer ? GetBufferManagerGLES30()->AcquireBuffer(maxVertices*stride, usage) : 0;
	DataBufferGLES30*	indexBuffer			= mapIndexBuffer ? GetBufferManagerGLES30()->AcquireBuffer(indexBufferSize, usage) : 0;

	// \todo [2013-05-31 pyry] Grow buffers in reasonable steps (align to 1k?)
	if (vertexBuffer && vertexBuffer->GetSize() < vertexBufferSize)
		vertexBuffer->RecreateStorage(vertexBufferSize, usage);

	if (indexBuffer && indexBuffer->GetSize() < indexBufferSize)
		indexBuffer->RecreateStorage(indexBufferSize, usage);

	if (!mapVertexBuffer && m_CurVertexData.size() < vertexBufferSize)
		m_CurVertexData.resize(vertexBufferSize);

	if (!mapIndexBuffer && m_CurIndexData.size() < indexBufferSize)
		m_CurIndexData.resize(indexBufferSize);

	if (vertexBuffer)
		*outVB = vertexBuffer->Map(0, vertexBufferSize, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_FLUSH_EXPLICIT_BIT);
	else if (!mapVertexBuffer && vertexBufferSize > 0)
		*outVB = &m_CurVertexData[0];

	if (indexBuffer)
		*outIB = indexBuffer->Map(0, indexBufferSize, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_FLUSH_EXPLICIT_BIT);
	else if (!mapIndexBuffer && indexBufferSize > 0)
		*outIB = &m_CurIndexData[0];

	m_CurVertexBuffer		= vertexBuffer;
	m_CurIndexBuffer		= indexBuffer;
	m_CurRenderMode			= renderMode;
	m_CurShaderChannelMask	= shaderChannelMask;
	m_CurStride				= stride;

	return true;
}

void DynamicGLES3VBO::ReleaseChunk (UInt32 actualVertices, UInt32 actualIndices)
{
	Assert(m_CurVertexCount == 0 && m_CurIndexCount == 0);

	if (m_CurVertexBuffer)
	{
		m_CurVertexBuffer->FlushMappedRange(0, AlignToDefault(m_CurStride*actualVertices));
		m_CurVertexBuffer->Unmap();
	}
	else if (actualVertices*m_CurStride >= kDataBufferThreshold)
	{
		// Migrate to buffer.
		const int		size	= AlignToDefault(actualVertices*m_CurStride);
		const UInt32	usage	= GL_STREAM_DRAW;

		m_CurVertexBuffer = GetBufferManagerGLES30()->AcquireBuffer(size, usage);
		m_CurVertexBuffer->RecreateWithData(size, usage, &m_CurVertexData[0]);
	}

	if (m_CurIndexBuffer)
	{
		m_CurIndexBuffer->FlushMappedRange(0, AlignToDefault(actualIndices*kVBOIndexSize));
		m_CurIndexBuffer->Unmap();
	}
	else if (actualIndices*kVBOIndexSize >= kDataBufferThreshold)
	{
		// Migrate to buffer.
		const int		size	= AlignToDefault(actualIndices*kVBOIndexSize);
		const UInt32	usage	= GL_STREAM_DRAW;

		m_CurIndexBuffer = GetBufferManagerGLES30()->AcquireBuffer(size, usage);
		m_CurIndexBuffer->RecreateWithData(size, usage, &m_CurIndexData[0]);
	}

	m_CurVertexCount	= actualVertices;
	m_CurIndexCount		= actualIndices;
}

DataBufferGLES30* DynamicGLES3VBO::GetQuadArrayIndexBuffer (int vertexCount)
{
	const int		quadCount			= vertexCount/4;
	const int		quadIndexCount		= quadCount * 6;
	const int		indexBufferSize		= quadIndexCount * sizeof(UInt16);
	const UInt32	indexBufferUsage	= GL_STATIC_DRAW;

	if (!m_QuadArrayIndexBuffer || m_QuadArrayIndexBuffer->GetSize() < indexBufferSize)
	{
		// Need to re-specify index buffer since current is too small
		std::vector<UInt16> quadIndices(quadIndexCount);

		for (int quadNdx = 0; quadNdx < quadCount; ++quadNdx)
		{
			const UInt16	srcBaseNdx	= quadNdx*4;
			const int		dstBaseNdx	= quadNdx*6;
			quadIndices[dstBaseNdx + 0] = srcBaseNdx + 1;
			quadIndices[dstBaseNdx + 1] = srcBaseNdx + 2;
			quadIndices[dstBaseNdx + 2] = srcBaseNdx;
			quadIndices[dstBaseNdx + 3] = srcBaseNdx + 2;
			quadIndices[dstBaseNdx + 4] = srcBaseNdx + 3;
			quadIndices[dstBaseNdx + 5] = srcBaseNdx;
		}

		if (m_QuadArrayIndexBuffer && BufferUpdateCausesStallGLES30(m_QuadArrayIndexBuffer))
		{
			m_QuadArrayIndexBuffer->Release();
			m_QuadArrayIndexBuffer = 0;
		}

		if (!m_QuadArrayIndexBuffer)
			m_QuadArrayIndexBuffer = GetBufferManagerGLES30()->AcquireBuffer(indexBufferSize, indexBufferUsage);

		m_QuadArrayIndexBuffer->RecreateWithData(indexBufferSize, indexBufferUsage, &quadIndices[0]);
	}

	return m_QuadArrayIndexBuffer;
}

void DynamicGLES3VBO::DrawChunk (const ChannelAssigns& channels)
{
	// Compute input state
	VertexArrayInfoGLES30 vertexInputState;
	ComputeVertexInputState(vertexInputState, channels);

	// \todo [2013-05-31 pyry] Do we want to use VAOs here?

	// Setup state
	VBOContainsColorGLES30(channels.GetSourceForTarget (kVertexCompColor) == kShaderChannelColor);
	GetRealGfxDevice().BeforeDrawCall(false);
	SetupDefaultVertexArrayStateGLES30(vertexInputState);

	const void*	indexPtr			= m_CurIndexBuffer ? 0 : (m_CurIndexData.empty() ? 0 : &m_CurIndexData[0]);
	int			trianglesForStats	= 0;

	if (m_CurIndexBuffer)
		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_CurIndexBuffer->GetBuffer()));

	switch (m_CurRenderMode)
	{
		case kDrawIndexedQuads: // \todo [2013-06-13 pyry] This enum shouldn't even be here.
		case kDrawIndexedTriangles:
			GLES_CHK(glDrawElements(GL_TRIANGLES, m_CurIndexCount, GL_UNSIGNED_SHORT, indexPtr));
			trianglesForStats = m_CurIndexCount/3;
			break;

		case kDrawIndexedTriangleStrip:
			GLES_CHK(glDrawElements(GL_TRIANGLE_STRIP, m_CurIndexCount, GL_UNSIGNED_SHORT, indexPtr));
			trianglesForStats = std::max<int>(0, m_CurIndexCount-2);
			break;
		case kDrawIndexedPoints:
			GLES_CHK(glDrawElements(GL_POINTS, m_CurIndexCount, GL_UNSIGNED_SHORT, indexPtr));
			trianglesForStats = m_CurIndexCount*2; // Assuming one quad
			break;

		case kDrawIndexedLines:
			GLES_CHK(glDrawElements(GL_LINES, m_CurIndexCount, GL_UNSIGNED_SHORT, indexPtr));
			trianglesForStats = m_CurIndexCount;
			break;

		case kDrawTriangleStrip:
			GLES_CHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, m_CurVertexCount));
			trianglesForStats = m_CurVertexCount-2;
			break;

		case kDrawQuads:
		{
			// Need to emulate with indices.
			DataBufferGLES30* quadIndexBuf = GetQuadArrayIndexBuffer(m_CurVertexCount);
			GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuf->GetBuffer()));
			GLES_CHK(glDrawElements(GL_TRIANGLES, m_CurVertexCount/4 * 6, GL_UNSIGNED_SHORT, 0));
			GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			trianglesForStats = m_CurVertexCount/2;
			quadIndexBuf->RecordRender();
			break;
		}

		default:
			Assert(false);
	}

	if (m_CurIndexBuffer)
	{
		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
		m_CurIndexBuffer->RecordRender();
	}

	GetRealGfxDevice().GetFrameStats().AddDrawCall(trianglesForStats, m_CurVertexCount);

	// Release buffers and reset state
	Cleanup();
}

void DynamicGLES3VBO::Recreate (void)
{
	Cleanup();
}

void DynamicGLES3VBO::Cleanup (void)
{
	if (m_CurVertexBuffer)
	{
		m_CurVertexBuffer->Release();
		m_CurVertexBuffer = 0;
	}

	if (m_CurIndexBuffer)
	{
		m_CurIndexBuffer->Release();
		m_CurIndexBuffer = 0;
	}

	if (m_QuadArrayIndexBuffer)
	{
		m_QuadArrayIndexBuffer->Release();
		m_QuadArrayIndexBuffer = 0;
	}

	m_CurRenderMode			= (RenderMode)0;
	m_CurShaderChannelMask	= 0;
	m_CurStride				= 0;
	m_CurVertexCount		= 0;
	m_CurIndexCount			= 0;

	m_CurVertexData.clear();
	m_CurIndexData.clear();
}

void DynamicGLES3VBO::ComputeVertexInputState (VertexArrayInfoGLES30& dst, const ChannelAssigns& channelAssigns)
{
	const UInt32	availableShaderChannels		= m_CurShaderChannelMask; // Channels that have data in any of streams.
	const UInt32	enabledTargets				= channelAssigns.GetTargetMap();
	UInt32			channelOffsets[kShaderChannelCount];

	const UInt8*	basePointer					= m_CurVertexBuffer ? 0 : &m_CurVertexData[0];
	const UInt32	buffer						= m_CurVertexBuffer ? m_CurVertexBuffer->GetBuffer() : 0;

	// Compute offsets per enabled shader channel.
	{
		UInt32 curOffset = 0;
		for (int ndx = 0; ndx < kShaderChannelCount; ndx++)
		{
			if (availableShaderChannels & (1<<ndx))
			{
				channelOffsets[ndx] = curOffset;
				curOffset += VBO::GetDefaultChannelByteSize(ndx);
			}
			else
				channelOffsets[ndx] = 0;
		}
	}

	for (int attribNdx = 0; attribNdx < kGLES3MaxVertexAttribs; attribNdx++)
	{
		const VertexComponent target = kVertexCompTargetsGLES3[attribNdx];

		if ((enabledTargets & (1<<target)) == 0)
			continue; // Not enabled.

		const ShaderChannel	sourceChannel = channelAssigns.GetSourceForTarget(target);

		// \todo [pyry] Uh, what? Channel is enabled, but no valid source exists.
		if (sourceChannel < 0)
			continue;

		Assert(0 <= sourceChannel && sourceChannel < kShaderChannelCount);

		if ((availableShaderChannels & (1<<sourceChannel)) == 0)
			continue; // Not available.

		dst.arrays[attribNdx].componentType		= VBO::GetDefaultChannelFormat(sourceChannel);
		dst.arrays[attribNdx].numComponents		= dst.arrays[attribNdx].componentType == kChannelFormatColor ? 4 : VBO::GetDefaultChannelDimension(sourceChannel);
		dst.arrays[attribNdx].pointer			= basePointer + channelOffsets[sourceChannel];
		dst.arrays[attribNdx].stride			= m_CurStride;
		dst.buffers[attribNdx]					= buffer;

		dst.enabledArrays |= (1<<attribNdx);
	}

	// Fixed-function texgen stuff. \todo [pyry] Are these even used?
	{
		GfxDevice&	device			= GetRealGfxDevice();
		const int	texArrayBase	= 3;
		const int	maxTexArrays	= std::min(gGraphicsCaps.maxTexUnits, kGLES3MaxVertexAttribs-texArrayBase);
		
		if (device.IsPositionRequiredForTexGen() && (availableShaderChannels & (1 << kShaderChannelVertex)))
		{
			const ShaderChannel	srcChannel	= kShaderChannelVertex;
			const int			format		= VBO::GetDefaultChannelFormat(srcChannel);
			const int			numComps	= VBO::GetDefaultChannelDimension(srcChannel);
			const UInt32		offset		= channelOffsets[srcChannel];

			for (int texUnit = 0; texUnit < maxTexArrays; ++texUnit)
			{
				if (device.IsPositionRequiredForTexGen(texUnit))
				{
					const int arrNdx = texArrayBase + texUnit;
					dst.arrays[arrNdx].componentType		= format;
					dst.arrays[arrNdx].numComponents		= numComps;
					dst.arrays[arrNdx].pointer				= basePointer + offset;
					dst.arrays[arrNdx].stride				= m_CurStride;
					dst.buffers[arrNdx]						= buffer;

					dst.enabledArrays |= (1<<arrNdx);
				}
			}
		}

		if (device.IsNormalRequiredForTexGen() && (availableShaderChannels & (1 << kShaderChannelNormal)))
		{
			const ShaderChannel	srcChannel	= kShaderChannelNormal;
			const int			format		= VBO::GetDefaultChannelFormat(srcChannel);
			const int			numComps	= VBO::GetDefaultChannelDimension(srcChannel);
			const UInt32		offset		= channelOffsets[srcChannel];

			for (int texUnit = 0; texUnit < maxTexArrays; ++texUnit)
			{
				if (device.IsNormalRequiredForTexGen(texUnit))
				{
					const int arrNdx = texArrayBase + texUnit;
					dst.arrays[arrNdx].componentType		= format;
					dst.arrays[arrNdx].numComponents		= numComps;
					dst.arrays[arrNdx].pointer				= basePointer + offset;
					dst.arrays[arrNdx].stride				= m_CurStride;
					dst.buffers[arrNdx]						= buffer;

					dst.enabledArrays |= (1<<arrNdx);
				}
			}
		}
	}
}

// \todo [2013-05-31 pyry] Better, more generic state cache

// \note In theory we could use whole VertexArrayInfoGLES30 as state cache, but alas
//		 bound buffers can be destroyed, recreated and state cache would be oblivious
//		 to the fact that binding is now empty.
static UInt32 sEnabledArrays = 0;

void InvalidateVertexInputCacheGLES30()
{
	sEnabledArrays = 0;

	for (int attribNdx = 0; attribNdx < gGraphicsCaps.gles30.maxAttributes; attribNdx++)
		GLES_CHK(glDisableVertexAttribArray(attribNdx));
}

void SetupDefaultVertexArrayStateGLES30 (const VertexArrayInfoGLES30& info)
{
	UInt32 curBoundBuffer = 0;
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));

	for (int attribNdx = 0; attribNdx < kGLES3MaxVertexAttribs; attribNdx++)
	{
		const UInt32 enableBit = 1<<attribNdx;

		if (info.enabledArrays & enableBit)
		{
			if (!(sEnabledArrays & enableBit))
				GLES_CHK(glEnableVertexAttribArray(attribNdx));

			const UInt32	buffer				= info.buffers[attribNdx];
			const int		numComponents		= info.arrays[attribNdx].numComponents;
			const GLenum	compType			= kVertexTypeGLES3[info.arrays[attribNdx].componentType];
			const bool		normalized			= IsVertexArrayNormalized(attribNdx, (VertexChannelFormat)info.arrays[attribNdx].componentType);
			const int		stride				= info.arrays[attribNdx].stride;
			const void*		pointer				= info.arrays[attribNdx].pointer;

			if (curBoundBuffer != buffer)
			{
				GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, buffer));
				curBoundBuffer = buffer;
			}

			GLES_CHK(glVertexAttribPointer(attribNdx, numComponents, compType, normalized ? GL_TRUE : GL_FALSE, stride, pointer));
		}
		else if (sEnabledArrays & enableBit)
			GLES_CHK(glDisableVertexAttribArray(attribNdx));
	}

	sEnabledArrays = info.enabledArrays;
}

#endif // GFX_SUPPORTS_OPENGLES30
