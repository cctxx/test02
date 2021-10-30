#include "UnityPrefix.h"

#if GFX_SUPPORTS_OPENGLES20
#include "VBOGLES20.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/GfxDevice/BatchRendering.h"
#include "IncludesGLES20.h"
#include "AssertGLES20.h"
#include "GpuProgramsGLES20.h"
#include "DebugGLES20.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Runtime/GfxDevice/GLESChannels.h"
#include "Runtime/GfxDevice/GLDataBufferCommon.h"
#include "Runtime/Profiler/MemoryProfiler.h"


static const GLenum kTopologyGLES2[kPrimitiveTypeCount] = {
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLES,
	GL_LINES,
	GL_LINE_STRIP,
	GL_POINTS,
};

template <typename T>
inline T Align (T v, size_t alignment)
{
	return (v + (alignment-1)) & ~(alignment-1);
}

extern void VBOContainsColorGLES20 (bool flag);
extern void GfxDeviceGLES20_SetDrawCallTopology(GfxPrimitiveType topology);

#define DISABLE_GLES_CALLS 0
#define DISABLE_DRAW_CALLS_ONLY 0


static UInt32 sCurrentTargetMap = 0;
void InvalidateVertexInputCacheGLES20()
{
	GLES_CHK(glDisableVertexAttribArray(GL_VERTEX_ARRAY));
	GLES_CHK(glDisableVertexAttribArray(GL_NORMAL_ARRAY));
	GLES_CHK(glDisableVertexAttribArray(GL_COLOR_ARRAY));
	for (size_t q = 0; q < gGraphicsCaps.maxTexImageUnits; ++q)
	{
		if (GL_TEXTURE_ARRAY0 + q < gGraphicsCaps.gles20.maxAttributes)
		{
			GLES_CHK(glDisableVertexAttribArray(GL_TEXTURE_ARRAY0 + q));
		}
	}
	sCurrentTargetMap = 0;
}

static UInt32 MaskUnavailableChannels(UInt32 targetMap, UInt32 unavailableChannels)
{
	if (unavailableChannels == 0)
		return targetMap;

	#define MASK_UNAVAILABLE_CHANNEL(schnl, vchnl) if (unavailableChannels & schnl) targetMap &= ~vchnl;

	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(Vertex), kVtxChnVertex);
	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(Color), kVtxChnColor);
	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(Normal), kVtxChnNormal);
	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(TexCoord0), kVtxChnTexCoord0);
	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(TexCoord1), kVtxChnTexCoord1);
	MASK_UNAVAILABLE_CHANNEL(VERTEX_FORMAT1(Tangent), kVtxChnTexCoord2);

	#undef MASK_UNAVAILABLE_CHANNEL

	return targetMap;
}

#if NV_STATE_FILTERING
typedef struct
{
	GLint			size;
	GLenum			type;
	GLboolean		normalized;
	GLsizei			stride;
	GLuint			buffer;
	const GLvoid*	pointer;
} FilteredVertexAttribPointer;
static GLuint boundBuffers[2];
static FilteredVertexAttribPointer* currPointers = 0;
static GLint max_attribs;

void filteredInitGLES20()
{
	static bool firstCall = true;
	if (!firstCall)
		return;
	firstCall = false;

	memset(boundBuffers, 0, 2*sizeof(GLuint));

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
	currPointers = new FilteredVertexAttribPointer[max_attribs];    // this memory is considered 'static' and is never freed.
	memset(currPointers, 0x00, max_attribs * sizeof(FilteredVertexAttribPointer));
}

void filteredBindBufferGLES20(GLenum target, GLuint buffer, bool isImmediate)
{
	int index = 0;
	if (target == GL_ELEMENT_ARRAY_BUFFER)
		index = 1;

	if (    (target != GL_ELEMENT_ARRAY_BUFFER && target != GL_ARRAY_BUFFER) ||
	        (boundBuffers[index] != buffer))
	{
		boundBuffers[index] = buffer;
		glBindBuffer(target, buffer);

		if (buffer && !isImmediate)
		{
			// TODO: This is to flush the matrix to the shader, but why is it needed?
			//       Isn't the matrix set when switching buffer elsewhere?
			void GfxDeviceGLES20_MarkWorldViewProjDirty();
			GfxDeviceGLES20_MarkWorldViewProjDirty();
		}
	}
}

void filteredVertexAttribPointerGLES20(GLuint  index,  GLint  size,  GLenum  type,  GLboolean  normalized,  GLsizei  stride,  const GLvoid *  pointer)
{
	bool doCall = false;
	if (index < 0 || index >= max_attribs)
	{
		doCall = true;
	}
	else
	{
		FilteredVertexAttribPointer &p = currPointers[index];
		if (p.buffer != boundBuffers[0] || p.size != size || p.type != type || p.normalized != normalized || p.stride != stride || p.pointer != pointer)
		{
			doCall = true;
			p.buffer = boundBuffers[0];
			p.size = size;
			p.type = type;
			p.normalized = normalized;
			p.stride = stride;
			p.pointer = pointer;
		}
	}
	if (doCall)
	{
		glVertexAttribPointer(index, size, type, normalized, stride, pointer);
	}
}

void filteredDeleteBuffersGLES20(GLsizei n, const GLuint *buffers)
{
	if (buffers && n)
	{
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < max_attribs; j++)
			{
				if (currPointers[j].buffer == buffers[i])
				{
					memset(&currPointers[j], 0, sizeof(FilteredVertexAttribPointer));
				}
			}
		}
	}
	glDeleteBuffers(n, buffers);
}

#ifndef glBindBuffer
#define glBindBuffer filteredBindBufferGLES20
#endif
#ifndef glDeleteBuffers
#define glDeleteBuffers filteredDeleteBuffersGLES20
#endif
#ifndef glVertexAttribPointer
#define glVertexAttribPointer filteredVertexAttribPointerGLES20
#endif

void StateFiltering_InvalidateVBOCacheGLES20()
{
	memset(boundBuffers, 0, 2*sizeof(GLuint));
	if(currPointers)
		memset(currPointers, 0x00, max_attribs * sizeof(FilteredVertexAttribPointer));
}


#endif


#define SETUP_VERTEX_CHANNEL(vchnl, vcomp, glArray, norm, channelSize, channelType, stride, ptr)	\
	if (targetMap & vchnl)													\
	{																		\
		if (channelsToEnable & vchnl) {										\
			GLES_CHK(glEnableVertexAttribArray(glArray));						\
		}																	\
		const ShaderChannel src = channels.GetSourceForTarget( vcomp );		\
		GLES_CHK(glVertexAttribPointer(glArray, channelSize, channelType, norm, stride, ptr));\
	} else if (channelsToDisable & vchnl) {									\
		GLES_CHK(glDisableVertexAttribArray(glArray));								\
	}																		\

#define SETUP_TEXCOORD_CHANNEL(vchnl, vcomp, glTex, setFunc)				\
	if (targetMap & vchnl)													\
	{																		\
		if (channelsToEnable & vchnl) {										\
			GLES_CHK(glEnableVertexAttribArray(GL_TEXTURE_COORD_ARRAY));		\
		}																	\
		const ShaderChannel src = channels.GetSourceForTarget( vcomp );		\
		setFunc;															\
	} else if (channelsToDisable & vchnl) {									\
		GLES_CHK(glEnableVertexAttribArray(GL_TEXTURE_COORD_ARRAY));			\
	}

// TODO: normalize normals???
#define LINK_TEXCOORD_CHANNEL(glTex)														\
	GLES_CHK(glEnableVertexAttribArray(glTex));												\
	GLES_CHK(glVertexAttribPointer(glTex, channelSizes[src], channelTypes[src], false,	\
		strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src])));


#if UNITY_ANDROID || UNITY_TIZEN
static void WorkaroundMaliBug(const UInt32 strides[kShaderChannelCount], void* dataChannel[kShaderChannelCount])
{
	// on Mali (first devices) there is bug in driver
	// that results in attributes from interleaved streams
	// remain active, even though they are disabled
	// as a workaround, find any non null ptr in dataChannel and set it

	void* readPtr = 0;
	UInt32 stride = 0;
	for( unsigned i = 0 ; i < kShaderChannelCount && !readPtr ; ++i)
	{
		readPtr = dataChannel[i];
		stride = strides[i];
	}

	GLES_CHK(glVertexAttribPointer(GL_VERTEX_ARRAY, 3, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE, true, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_NORMAL_ARRAY, 3, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY0, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY1, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY2, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY3, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY4, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY5, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY6, 2, GL_FLOAT, false, stride, readPtr));
	GLES_CHK(glVertexAttribPointer(GL_TEXTURE_ARRAY7, 2, GL_FLOAT, false, stride, readPtr));

	InvalidateVertexInputCacheGLES20();
}
#endif


static void SetupVertexInput(const ChannelAssigns& channels, void* dataChannel[kShaderChannelCount], const UInt32 strides[kShaderChannelCount], const int channelSizes[kShaderChannelCount], const GLenum channelTypes[kShaderChannelCount], UInt32 unavailableChannels = 0)
{
#if DISABLE_GLES_CALLS
	return;
#endif
	GfxDevice& device = GetRealGfxDevice();

#if UNITY_ANDROID || UNITY_TIZEN
	if( gGraphicsCaps.gles20.buggyDisableVAttrKeepsActive )
		WorkaroundMaliBug(strides, dataChannel);
#endif

	UInt32 targetMap = MaskUnavailableChannels(channels.GetTargetMap(), unavailableChannels);

	const UInt32 channelsDiff = sCurrentTargetMap ^ targetMap;
	const UInt32 channelsToEnable = channelsDiff & targetMap;
	const UInt32 channelsToDisable = channelsDiff & (~targetMap);

	SETUP_VERTEX_CHANNEL(kVtxChnVertex, kVertexCompVertex, GL_VERTEX_ARRAY, false,
						 channelSizes[src], channelTypes[src],
					     strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));

	SETUP_VERTEX_CHANNEL(kVtxChnColor, kVertexCompColor, GL_COLOR_ARRAY, true,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));

	SETUP_VERTEX_CHANNEL(kVtxChnNormal, kVertexCompNormal, GL_NORMAL_ARRAY, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));


	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord0, kVertexCompTexCoord0, GL_TEXTURE_ARRAY0, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord1, kVertexCompTexCoord1, GL_TEXTURE_ARRAY1, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord2, kVertexCompTexCoord2, GL_TEXTURE_ARRAY2, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord3, kVertexCompTexCoord3, GL_TEXTURE_ARRAY3, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord4, kVertexCompTexCoord4, GL_TEXTURE_ARRAY4, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord5, kVertexCompTexCoord5, GL_TEXTURE_ARRAY5, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord6, kVertexCompTexCoord6, GL_TEXTURE_ARRAY6, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));
	SETUP_VERTEX_CHANNEL(kVtxChnTexCoord7, kVertexCompTexCoord7, GL_TEXTURE_ARRAY7, channelTypes[src] == GL_BYTE,
						 channelSizes[src], channelTypes[src],
						 strides[src], reinterpret_cast<GLvoid const*>(dataChannel[src]));

	sCurrentTargetMap = targetMap;

	// setup fixed function texGens
	const UInt32 sourceMap = channels.GetSourceMap();
	{
		const ShaderChannel src = kShaderChannelVertex;
		if( device.IsPositionRequiredForTexGen() && (sourceMap & (1 << src)) )
		{
			for (int texUnit = 0; texUnit < gGraphicsCaps.maxTexImageUnits; ++texUnit)
			{
				if( device.IsPositionRequiredForTexGen(texUnit) )
				{
					// pass position as tex-coord, if required by texgen operation
					LINK_TEXCOORD_CHANNEL(GL_TEXTURE_ARRAY0 + texUnit);
					Assert(texUnit < ARRAY_SIZE(sTexCoordChannels));
					sCurrentTargetMap |= sTexCoordChannels[texUnit];
				}
			}
		}
	}

	{
		const ShaderChannel src = kShaderChannelNormal;
		if( device.IsNormalRequiredForTexGen() && (sourceMap & (1 << src)) )
		{
			for (int texUnit = 0; texUnit < gGraphicsCaps.maxTexImageUnits; ++texUnit)
			{
				if( device.IsNormalRequiredForTexGen(texUnit) )
				{
					// pass normal as tex-coord, if required by texgen operation
					LINK_TEXCOORD_CHANNEL(GL_TEXTURE_ARRAY0 + texUnit);
					Assert(texUnit < ARRAY_SIZE(sTexCoordChannels));
					sCurrentTargetMap |= sTexCoordChannels[texUnit];
				}
			}
		}
	}
}

void GLES2VBO::DrawInternal(int vertexBufferID, int indexBufferID, const ChannelAssigns& channels, void* indices, UInt32 indexCount, GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount)
{
	UInt32 unavailableChannels = GetUnavailableChannels(channels);

	// should never happen; a dummy all-white vertex color array is always created by Mesh code
	AssertIf( unavailableChannels & (1<<kShaderChannelColor) );

#if DISABLE_GLES_CALLS
	return;
#endif

	DBG_LOG_GLES20("---> GLES2VBO::DrawVBO indexCount:%d channels: %04X/%04X, unavailable: %04X", (int)indexCount, channels.GetTargetMap(), channels.GetSourceMap(), unavailableChannels, unavailableChannels);

	VBOContainsColorGLES20 (channels.GetSourceForTarget (kVertexCompColor) == kShaderChannelColor);
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID));

	// setup vertex state
		GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID));
		void* channelData[kShaderChannelCount];
		UInt32 channelStrides[kShaderChannelCount];
		if (vertexBufferID != 0)
			GetChannelOffsetsAndStrides(channelData, channelStrides);
		else
			GetChannelDataAndStrides(channelData, channelStrides);

		int channelSizes[kShaderChannelCount];
		GLenum channelTypes[kShaderChannelCount];
		SetupGLESChannelSizes(m_Channels, channelSizes);
		SetupGLESChannelTypes(m_Channels, channelTypes);
		SetupVertexInput(channels, channelData, channelStrides, channelSizes, channelTypes, unavailableChannels);

	GfxDeviceGLES20_SetDrawCallTopology(topology);

	GetRealGfxDevice().BeforeDrawCall(false);

#if DBG_LOG_GLES20_ACTIVE
	DumpVertexArrayStateGLES20();
#endif

	// draw
#if !DISABLE_DRAW_CALLS_ONLY
	ABSOLUTE_TIME drawDt = START_TIME;
	GLenum gltopo = kTopologyGLES2[topology];
	GLES_CHK(glDrawElements(gltopo, indexCount, GL_UNSIGNED_SHORT, indices));
	drawDt = ELAPSED_TIME(drawDt);

	int primCount = GetPrimitiveCount(indexCount, topology, false);
	GetRealGfxDevice().GetFrameStats().AddDrawCall (primCount, drawVertexCount, drawDt);
#endif
}

void GLES2VBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
					GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount )
{
	int indexBufferID = m_IndexBufferID;
	void*	indexBufferData	= (UInt8*)(m_IndexBufferID ? 0 : m_IBData.indices) + firstIndexByte;

	// If we're drawing quads, convert them into triangles; into a temporary index buffer area
	void* tempIndexBuffer = NULL;
	if (topology == kPrimitiveQuads)
	{
		UInt32 ibCapacityNeeded = indexCount/4*6*2;

		// Get IB space from shared buffer or just allocate
		void* ibPtr;
		if (gGraphicsCaps.gles20.slowDynamicVBO)
		{
			ibPtr = UNITY_MALLOC(kMemDynamicGeometry, ibCapacityNeeded);
			tempIndexBuffer = ibPtr;
		}
		else
		{
			ibPtr = LockSharedBufferGLES20 (GL_ELEMENT_ARRAY_BUFFER, ibCapacityNeeded);
		}
		DebugAssert (ibPtr);

		// Convert quads into triangles
		FillIndexBufferForQuads ((UInt16*)ibPtr, ibCapacityNeeded, (const UInt16*)((UInt8*)m_ReadableIndices + firstIndexByte), indexCount/4);

		// Finish up with temporary space
		if (gGraphicsCaps.gles20.slowDynamicVBO)
		{
			indexBufferID = 0;
			indexBufferData = ibPtr;
		}
		else
		{
			indexBufferID = UnlockSharedBufferGLES20 ();
			indexBufferData = NULL;
		}
		indexCount = indexCount/4*6;
	}

	// Draw!
	DrawInternal( m_UsesVBO ? m_VertexBufferID[m_CurrentBufferIndex] : 0, indexBufferID, channels, indexBufferData, indexCount,
				 topology, firstVertex, firstVertex+vertexCount, vertexCount);

	// Release any temporary buffer we might have allocated
	if (tempIndexBuffer)
	{
		UNITY_FREE(kMemDynamicGeometry, tempIndexBuffer);
	}
}

void GLES2VBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
							   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
{
	Assert (topology != kPrimitiveQuads); // only called by static batching; which only handles triangles

	int ibo = 0;
	if(!gGraphicsCaps.gles20.forceStaticBatchFromMem)
	{
		// we expect static batches to be quite large ibos, and in that case drawing from memory is worst case scenario
		// sure, unless running on some buggy piece of sh**t
		const size_t ibCapacity = indexCount * kVBOIndexSize;
		void* dstIndices = LockSharedBufferGLES20 (GL_ELEMENT_ARRAY_BUFFER, ibCapacity, true);
		DebugAssert (dstIndices);
		memcpy (dstIndices, indices, ibCapacity);
		ibo = UnlockSharedBufferGLES20 (0, true);
		indices = 0;
	}

	DrawInternal( m_UsesVBO ? m_VertexBufferID[m_CurrentBufferIndex] : 0, ibo, channels, indices, indexCount,
				 topology, vertexRangeBegin, vertexRangeEnd, drawVertexCount);
}

GLES2VBO::GLES2VBO()
:	m_CurrentBufferIndex(0)
,	m_IndexBufferID(0)
,	m_IBSize(0)
,	m_ReadableIndices(0)
,	m_VBOUsage(GL_STATIC_DRAW)
,	m_IBOUsage(GL_STATIC_DRAW)
,	m_UsesVBO(false)
,	m_UsesIBO(false)
{
	::memset(m_VertexBufferID, 0x0, sizeof(m_VertexBufferID));
	::memset(&m_IBData, 0, sizeof(m_IBData));
}

GLES2VBO::~GLES2VBO()
{
	Cleanup ();
}

void GLES2VBO::Cleanup()
{
	int bufferCount = HasStreamWithMode(kStreamModeDynamic) ? DynamicVertexBufferCount : 1;
	glDeregisterBufferData(bufferCount, (GLuint*)m_VertexBufferID);
	GLES_CHK(glDeleteBuffers(bufferCount, (GLuint*)m_VertexBufferID));
	::memset(m_VertexBufferID, 0x0, sizeof(m_VertexBufferID));

	if (m_IndexBufferID)
	{
		glDeregisterBufferData(1, (GLuint*)&m_IndexBufferID);
		GLES_CHK(glDeleteBuffers(1, (GLuint*)&m_IndexBufferID));
		m_IndexBufferID = 0;
	}

	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void GLES2VBO::Recreate()
{
	MarkBuffersLost();

	if(m_VertexData.bufferSize > 0)
		EnsureVerticesInited(true);

	if(m_IBSize > 0)
		EnsureIndicesInited();
}

bool GLES2VBO::IsVertexBufferLost() const
{
	return (m_VertexBufferID[m_CurrentBufferIndex] == 0 && m_UsesVBO);
}

bool GLES2VBO::IsIndexBufferLost() const
{
	return (m_IndexBufferID == 0 && m_UsesIBO);
}

void GLES2VBO::MarkBuffersLost()
{
	::memset(m_VertexBufferID, 0x0, sizeof(m_VertexBufferID));
	m_IndexBufferID = 0;

	// we also want to pretend we use vbo/ibo to hit "lost" path
	// in case of vbo we also need to get rid of bufferedvbo copy

	if(!m_UsesVBO)
	{
		UnbufferVertexData();
		m_UsesVBO = true;
	}
	m_UsesIBO = true;
}


bool GLES2VBO::MapVertexStream(VertexStreamData& outData, unsigned stream)
{
	DebugAssert(!IsAnyStreamMapped());
	Assert(m_VertexData.bufferSize > 0);

	if(HasStreamWithMode(kStreamModeDynamic))
		m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % DynamicVertexBufferCount;
	else
		m_CurrentBufferIndex = 0;

	// TODO: make it possible to change m_UsesVBO at runtime
	// for now once we use mapbuffer, the buffered data is lost so no going back

	if(m_UsesVBO && (gGraphicsCaps.gles20.hasMapbuffer || gGraphicsCaps.gles20.hasMapbufferRange))
	{
		static const int _MapRangeFlags = GL_MAP_WRITE_BIT_EXT | GL_MAP_INVALIDATE_BUFFER_BIT_EXT;

		GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferID[m_CurrentBufferIndex]));

		void* buf = 0;
		if(gGraphicsCaps.gles20.hasMapbufferRange)
			buf = gGlesExtFunc.glMapBufferRangeEXT(GL_ARRAY_BUFFER, 0, m_VertexData.bufferSize, _MapRangeFlags);
		else
			buf = gGlesExtFunc.glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);

		GLESAssert();
		if(buf)
		{
			const StreamInfo& info = m_VertexData.streams[stream];
			outData.buffer = (UInt8*)buf + info.offset;
			outData.channelMask = info.channelMask;
			outData.stride = info.stride;
			outData.vertexCount = m_VertexData.vertexCount;

			UnbufferVertexData();
			m_IsStreamMapped[stream] = true;
		}
		return buf != 0;
	}

	return BufferedVBO::MapVertexStream(outData, stream);
}

void GLES2VBO::UnmapVertexStream( unsigned stream )
{
	Assert(m_IsStreamMapped[stream]);
	Assert(m_VertexData.bufferSize > 0);

	m_IsStreamMapped[stream] = false;
	if(m_UsesVBO)
	{
		GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferID[m_CurrentBufferIndex]));
		if(gGraphicsCaps.gles20.hasMapbuffer || gGraphicsCaps.gles20.hasMapbufferRange)
			GLES_CHK(gGlesExtFunc.glUnmapBufferOES(GL_ARRAY_BUFFER));
		else
			GLES_CHK(glBufferSubData(GL_ARRAY_BUFFER, 0, m_VertexData.bufferSize, m_VertexData.buffer));
		GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	}
}

// the threshold is pretty arbitrary:
// from our tests on "slowDynamicVBO" gpus "small" buffers are faster drawn from mem, and "bigger" ones as buffers (like normal case)
// so we just pick something as small buffer threshold and be gone

static const int _SmallBufferSizeThreshold = 1024;

bool GLES2VBO::ShouldUseVBO()
{
	if(HasStreamWithMode(kStreamModeDynamic) && gGraphicsCaps.gles20.slowDynamicVBO)
		return m_VertexData.bufferSize > _SmallBufferSizeThreshold;

	return true;
}

bool GLES2VBO::ShouldUseIBO()
{
	if(AreIndicesDynamic() && gGraphicsCaps.gles20.slowDynamicVBO)
		return m_IBData.count * kVBOIndexSize > _SmallBufferSizeThreshold;

	return true;
}


void GLES2VBO::EnsureVerticesInited(bool newBuffers)
{
	bool isDynamic 		= HasStreamWithMode(kStreamModeDynamic);

	int  createStart	= 0;
	int  createCount	= 0;
	if(m_VertexBufferID[0] == 0)
	{
		// initial create
		createCount = isDynamic ? DynamicVertexBufferCount : 1;
		newBuffers  = true;
	}
	else if(m_VertexBufferID[1] == 0 && isDynamic)
	{
		// changed to dynamic
		createStart = 1;
		createCount = DynamicVertexBufferCount - 1;
		newBuffers  = true;
	}
	else if(m_VertexBufferID[1] != 0 && !isDynamic)
	{
		// changed to static
		glDeregisterBufferData(DynamicVertexBufferCount-1, (GLuint*)&m_VertexBufferID[1]);
		GLES_CHK(glDeleteBuffers(DynamicVertexBufferCount-1, (GLuint*)&m_VertexBufferID[1]));
		::memset(&m_VertexBufferID[1], 0x0, (DynamicVertexBufferCount-1)*sizeof(int));
	}

	m_VBOUsage = isDynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW;
	m_UsesVBO  = ShouldUseVBO();
	if(m_UsesVBO)
	{
		if(newBuffers && createCount)
			GLES_CHK(glGenBuffers(createCount, (GLuint*)&m_VertexBufferID[createStart]));

		// TODO: the only reason to fill whole VB is for multi-stream support
		// TODO: somehow check if we can get away with less data copied
		for(int i = 0, count = isDynamic ? DynamicVertexBufferCount : 1 ; i < count ; ++i)
		{
			GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferID[i]));
			if(newBuffers)
			{
				GLES_CHK(glBufferData(GL_ARRAY_BUFFER, m_VertexData.bufferSize, m_VertexData.buffer, m_VBOUsage));
				glRegisterBufferData(m_VertexBufferID[i], m_VertexData.bufferSize, this);
			}
			else
			{
				GLES_CHK(glBufferSubData(GL_ARRAY_BUFFER, 0, m_VertexData.bufferSize, m_VertexData.buffer));
			}
			GetRealGfxDevice().GetFrameStats().AddUploadVBO(m_VertexData.bufferSize);
		}

		// now the hacky stuff - we dont actually need extra mem copy as we will recreate VBO from mesh
		// on the other hand we still need it if we want to map stream
		// by coincidence ;-) we will map only when having dynamic streams, and kStreamModeWritePersist (for cloth)
		if(!HasStreamWithMode(kStreamModeDynamic) && !HasStreamWithMode(kStreamModeWritePersist))
			UnbufferVertexData();
	}
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	m_CurrentBufferIndex = 0;
}

void GLES2VBO::EnsureIndicesInited()
{
	m_IBOUsage = AreIndicesDynamic() ? GL_STREAM_DRAW : GL_STATIC_DRAW;
	m_UsesIBO  = ShouldUseIBO();
	if(m_UsesIBO)
	{
		const size_t size = CalculateIndexBufferSize(m_IBData);

		if(!m_IndexBufferID)
			GLES_CHK(glGenBuffers(1, (GLuint*)&m_IndexBufferID));

		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBufferID));
		GLES_CHK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, m_IBData.indices, m_IBOUsage));
		glRegisterBufferData(m_IndexBufferID, size, this);
		GetRealGfxDevice().GetFrameStats().AddUploadIB(size);
		m_IBSize = size;

		GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	}
	else if(m_IndexBufferID)
	{
		glDeregisterBufferData(1, (GLuint*)&m_IndexBufferID);
		GLES_CHK(glDeleteBuffers(1, (GLuint*)&m_IndexBufferID));
		m_IndexBufferID = 0;
	}
}

void GLES2VBO::UpdateVertexData( const VertexBufferData& srcBuffer )
{
	int oldVBSize = m_VertexData.bufferSize;

	BufferedVBO::BufferAllVertexData(srcBuffer);
	std::copy(srcBuffer.streams, srcBuffer.streams + kMaxVertexStreams, m_Streams);
	std::copy(srcBuffer.channels, srcBuffer.channels + kShaderChannelCount, m_Channels);

	EnsureVerticesInited(oldVBSize < m_VertexData.bufferSize);
}

void GLES2VBO::UpdateIndexData (const IndexBufferData& buffer)
{
	m_IBData.indices = buffer.indices;
	m_IBData.count   = buffer.count;
	EnsureIndicesInited();

	m_ReadableIndices = buffer.indices;
}

UInt32 GLES2VBO::GetUnavailableChannels(const ChannelAssigns& channels) const
{
	// Figure out which channels we can't find in the streams
	UInt32 unavailableChannels = channels.GetSourceMap();
	for (int stream = 0; stream < kMaxVertexStreams; stream++)
	{
		unavailableChannels &= ~m_Streams[stream].channelMask;
	}
	return unavailableChannels;
}

int GLES2VBO::GetRuntimeMemorySize() const
{
#if ENABLE_MEM_PROFILER
	return GetMemoryProfiler()->GetRelatedMemorySize(this) +
		GetMemoryProfiler()->GetRelatedIDMemorySize((UInt32)this);
#else
	return 0;
#endif
}


//
class SharedBuffer
{
public:
	SharedBuffer (int bufferType, size_t bytesPerBlock, size_t blocks = 1, bool driverSupportsBufferOrphaning = true);
	~SharedBuffer ();
	void Recreate();

	void* Lock (size_t bytes);
	void Unlock (size_t actualBytes = 0);

	typedef int BufferId;
	BufferId GetDrawable () const;
	void MarkAsDrawn (BufferId bufferId) {}

	size_t GetAvailableBytes () const;

private:
	void* OrphanLock (size_t bytes);
	void OrphanUnlock (size_t actualBytes);

	void* SimpleLock (size_t bytes);
	void SimpleUnlock (size_t actualBytes);

private:
	int					m_BufferType; // GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER
	vector<BufferId>	m_BufferIDs;
	vector<size_t>		m_BufferSizes;
	bool				m_DriverSupportsBufferOrphaning;

	size_t				m_NextBufferIndex;
	size_t				m_ReadyBufferIndex;

	UInt8*				m_TemporaryDataStorage;
	size_t				m_TemporaryDataStorageSize;

	size_t				m_LockedBytes;
};


DynamicGLES2VBO::DynamicGLES2VBO()
:	m_LastRenderMode (kDrawIndexedTriangles)
,	m_VB (0)
,	m_LargeVB (0)
,	m_ActiveVB (0)
,	m_IB (0)
,	m_LargeIB (0)
,	m_ActiveIB (0)
,	m_QuadsIB (0)
,	m_IndexBufferQuadsID (0)
,	m_VtxSysMemStorage(0)
,	m_VtxSysMemStorageSize(0)
,	m_IdxSysMemStorage(0)
,	m_IdxSysMemStorageSize(0)
{
	for( int i = 0; i < kShaderChannelCount; ++i )
	{
		m_BufferChannel[i] = 0;
	}

	const bool willUseMemory = gGraphicsCaps.gles20.slowDynamicVBO;
	const bool willUseOnlyMemory = willUseMemory && gGraphicsCaps.gles20.forceStaticBatchFromMem;

	if(willUseMemory)
	{
		m_VtxSysMemStorageSize 	= 8096;
		m_VtxSysMemStorage 		= (UInt8*)UNITY_MALLOC_ALIGNED(kMemDynamicGeometry, m_VtxSysMemStorageSize, 32);
		m_IdxSysMemStorageSize 	= 4096;
		m_IdxSysMemStorage 		= (UInt16*)UNITY_MALLOC_ALIGNED(kMemDynamicGeometry, m_IdxSysMemStorageSize, 32);
	}

	if(!willUseOnlyMemory)
	{
		if(!gGraphicsCaps.gles20.hasVBOOrphaning)
		{
			// total preallocated memory ~2MB, but it can grow!
			// we expect mostly small VBs
			m_VB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ARRAY_BUFFER, 8096, 32, false);
			m_IB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ELEMENT_ARRAY_BUFFER, 4096, 16, false);
			m_LargeVB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ARRAY_BUFFER, 32768, 32, false);
			m_LargeIB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ELEMENT_ARRAY_BUFFER, 16384, 32, false);
		}
		else
		{
			m_VB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ARRAY_BUFFER, 32768);
			m_IB = UNITY_NEW(SharedBuffer,kMemDynamicGeometry) (GL_ELEMENT_ARRAY_BUFFER, 8096);
		}
	}
}

DynamicGLES2VBO::~DynamicGLES2VBO ()
{
	UNITY_FREE(kMemDynamicGeometry, m_VtxSysMemStorage);
	UNITY_FREE(kMemDynamicGeometry, m_IdxSysMemStorage);

	UNITY_DELETE (m_VB, kMemDynamicGeometry);
	UNITY_DELETE (m_IB, kMemDynamicGeometry);
	UNITY_DELETE (m_LargeVB, kMemDynamicGeometry);
	UNITY_DELETE (m_LargeIB, kMemDynamicGeometry);

	if (m_IndexBufferQuadsID)
	{
		glDeregisterBufferData(1, (GLuint*)&m_IndexBufferQuadsID);
		GLES_CHK(glDeleteBuffers(1, (GLuint*)&m_IndexBufferQuadsID));
	}
	UNITY_FREE (kMemDynamicGeometry, m_QuadsIB);
}

void DynamicGLES2VBO::Recreate()
{
	m_IndexBufferQuadsID = 0;

	if(m_VB)		m_VB->Recreate();
	if(m_IB)		m_IB->Recreate();
	if(m_LargeVB)	m_LargeVB->Recreate();
	if(m_LargeIB)	m_LargeIB->Recreate();
}

inline void DynamicGLES2VBO::InitializeQuadsIB()
{
	Assert(m_IndexBufferQuadsID == 0);

	if(!m_QuadsIB)
	{
		m_QuadsIB = (UInt16*)UNITY_MALLOC_ALIGNED(kMemDynamicGeometry, VBO::kMaxQuads * 6 * kVBOIndexSize, 32);
		Assert (m_QuadsIB);

		UInt16* ib = m_QuadsIB;
		UInt32 baseIndex = 0;
		for( int i = 0; i < VBO::kMaxQuads; ++i )
		{
			ib[0] = baseIndex + 1;
			ib[1] = baseIndex + 2;
			ib[2] = baseIndex;
			ib[3] = baseIndex + 2;
			ib[4] = baseIndex + 3;
			ib[5] = baseIndex;
			baseIndex += 4;
			ib += 6;
		}
	}

	GLES_CHK(glGenBuffers( 1, (GLuint*)&m_IndexBufferQuadsID ));
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBufferQuadsID));
	GLES_CHK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, VBO::kMaxQuads * 6 * kVBOIndexSize, m_QuadsIB, GL_STATIC_DRAW));
	glRegisterBufferData(m_IndexBufferQuadsID, VBO::kMaxQuads * 6 * kVBOIndexSize, this);
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

void DynamicGLES2VBO::DrawChunk (const ChannelAssigns& channels)
{
#if DISABLE_GLES_CALLS
	return;
#endif

	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;

	AssertIf ( !m_LastChunkShaderChannelMask || !m_LastChunkStride );
	Assert (!m_LendedChunk);

	UInt8* ibPointer = (UInt8*)m_IdxSysMemStorage;

	GLuint vbo = m_VtxSysMemStorage ? 0 : m_ActiveVB->GetDrawable ();
	GLuint ibo = 0;
	if (m_LastRenderMode == kDrawQuads)
	{
		if (!m_IndexBufferQuadsID)
			InitializeQuadsIB();
		ibo = m_IndexBufferQuadsID;
		ibPointer = 0;
	}
	else if (m_ActiveIB)
	{
		ibo = m_ActiveIB->GetDrawable ();
	}

	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
	GLES_CHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo));

	switch(m_LastRenderMode)
	{
		case kDrawIndexedTriangles:
		case kDrawIndexedQuads:
		case kDrawQuads:
			GfxDeviceGLES20_SetDrawCallTopology(kPrimitiveTriangles); 				break;
		case kDrawTriangleStrip:
		case kDrawIndexedTriangleStrip:
			GfxDeviceGLES20_SetDrawCallTopology(kPrimitiveTriangleStripDeprecated);	break;
		case kDrawIndexedLines:
			GfxDeviceGLES20_SetDrawCallTopology(kPrimitiveLines);					break;
		case kDrawIndexedPoints:
			GfxDeviceGLES20_SetDrawCallTopology(kPrimitivePoints);					break;
	}


	VBOContainsColorGLES20(m_LastChunkShaderChannelMask & (1<<kShaderChannelColor));
	GetRealGfxDevice().BeforeDrawCall( false );

	const UInt32 unavailableChannels = channels.GetSourceMap() & ~m_LastChunkShaderChannelMask;
	UInt32 strides[kShaderChannelCount];
	std::fill(strides, strides + kShaderChannelCount, m_LastChunkStride);
	SetupVertexInput(channels, m_BufferChannel, strides, kDefaultChannelSizes, kDefaultChannelTypes, unavailableChannels);

	DBG_LOG_GLES20("--->");
	DBG_LOG_GLES20("--->DrawChunk");
	DBG_LOG_GLES20("--->");
	// draw
#if !DISABLE_DRAW_CALLS_ONLY
	ABSOLUTE_TIME drawStartTime = START_TIME;
	int primCount = 0;
	switch (m_LastRenderMode)
	{
	case kDrawIndexedTriangles:
		Assert (m_LastChunkIndices != 0);
		GLES_CHK(glDrawElements(GL_TRIANGLES, m_LastChunkIndices, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkIndices/3;
		break;
	case kDrawTriangleStrip:
		Assert (m_LastChunkIndices == 0);
		GLES_CHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, m_LastChunkVertices));
		primCount = m_LastChunkVertices-2;
		break;
	case kDrawQuads:
		GLES_CHK(glDrawElements(GL_TRIANGLES, (m_LastChunkVertices/2)*3, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkVertices/2;
		break;
	case kDrawIndexedTriangleStrip:
		Assert (m_LastChunkIndices != 0);
		GLES_CHK(glDrawElements(GL_TRIANGLE_STRIP, m_LastChunkIndices, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkIndices-2;
		break;
	case kDrawIndexedLines:
		Assert(m_LastChunkIndices != 0);
		GLES_CHK(glDrawElements(GL_LINES, m_LastChunkIndices, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkIndices/2;
		break;
	case kDrawIndexedPoints:
		Assert(m_LastChunkIndices != 0);
		GLES_CHK(glDrawElements(GL_POINTS, m_LastChunkIndices, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkIndices;
		break;
	case kDrawIndexedQuads:
		Assert(m_LastChunkIndices != 0);
		GLES_CHK(glDrawElements(GL_TRIANGLES, m_LastChunkIndices, GL_UNSIGNED_SHORT, ibPointer));
		primCount = m_LastChunkIndices/3;
		break;
	}
	GetRealGfxDevice().GetFrameStats().AddDrawCall (primCount, m_LastChunkVertices, ELAPSED_TIME(drawStartTime));
#endif
}

bool DynamicGLES2VBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, DynamicVBO::RenderMode renderMode, void** outVB, void** outIB )
{
	Assert( !m_LendedChunk );
	Assert( maxVertices < 65536 && maxIndices < 65536*3 );
	Assert(!((renderMode == kDrawQuads) && (VBO::kMaxQuads*4 < maxVertices)));
	DebugAssert( outVB != NULL && maxVertices > 0 );
	DebugAssert(
				(renderMode == kDrawIndexedQuads			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedPoints			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedLines			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangles		&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangleStrip	&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawTriangleStrip			&& (outIB == NULL && maxIndices == 0)) ||
				(renderMode == kDrawQuads					&& (outIB == NULL && maxIndices == 0)));

	m_LendedChunk = true;
	m_LastChunkShaderChannelMask = shaderChannelMask;
	m_LastRenderMode = renderMode;

	if( maxVertices == 0 )
		maxVertices = 8;

	m_LastChunkStride = 0;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( shaderChannelMask & (1<<i) )
			m_LastChunkStride += VBO::GetDefaultChannelByteSize(i);
	}
	DebugAssert (outVB);
	const size_t vbCapacity = Align (maxVertices * m_LastChunkStride, 1024); // if size would require growing buffer, make sure to grow in descreet steps

	if(m_VtxSysMemStorage)
	{
		*outVB = GetVertexMemory(vbCapacity);
		m_ActiveVB = 0;
	}
	else
	{
		m_ActiveVB = GetSharedVB (vbCapacity);
		*outVB = m_ActiveVB->Lock (vbCapacity);
		if (!*outVB)
			return false;
	}

	const bool indexed = (renderMode != kDrawQuads) && (renderMode != kDrawTriangleStrip);
	if (maxIndices && indexed)
	{
		DebugAssert (outIB);
		const size_t ibCapacity = Align( maxIndices * 2, 1024); // if size would require growing buffer, make sure to grow in discrete steps

		if(m_IdxSysMemStorage)
		{
			*outIB = GetIndexMemory(ibCapacity);
			m_ActiveIB = 0;
		}
		else
		{
			m_ActiveIB = GetSharedIB (ibCapacity);
			*outIB = m_ActiveIB->Lock (ibCapacity);
			if (!*outIB)
				return false;
		}
	}
	else
	{
		DebugAssert (!outIB);
		m_ActiveIB = NULL;
	}

	return true;
}

void DynamicGLES2VBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	Assert( m_LendedChunk );
	Assert( m_LastRenderMode == kDrawIndexedTriangleStrip || m_LastRenderMode == kDrawIndexedQuads || m_LastRenderMode == kDrawIndexedPoints || m_LastRenderMode == kDrawIndexedLines || actualIndices % 3 == 0 );
	m_LendedChunk = false;

	m_LastChunkVertices = actualVertices;
	m_LastChunkIndices = actualIndices;

	if(!m_VtxSysMemStorage)
	{
		DebugAssert (m_ActiveVB);
		m_ActiveVB->Unlock (actualVertices * m_LastChunkStride);
	}

	if(!m_IdxSysMemStorage)
	{
		if (m_ActiveIB)
			m_ActiveIB->Unlock (actualIndices * kVBOIndexSize);
	}

	const bool indexed = (m_LastRenderMode != kDrawQuads) && (m_LastRenderMode != kDrawTriangleStrip);
	if( !actualVertices || (indexed && !actualIndices) )
	{
		m_LastChunkShaderChannelMask = 0;
		return;
	}

	UInt8* channelOffset = m_VtxSysMemStorage;
	for( int i = 0; i < kShaderChannelCount; ++i )
	{
		if( m_LastChunkShaderChannelMask & (1<<i) )
		{
			m_BufferChannel[i] = channelOffset;
			channelOffset += VBO::GetDefaultChannelByteSize(i);
		}
		else
			m_BufferChannel[i] = 0;
	}
}

void* DynamicGLES2VBO::GetVertexMemory(size_t bytes)
{
	if(m_VtxSysMemStorageSize < bytes)
	{
		UNITY_FREE(kMemDynamicGeometry, m_VtxSysMemStorage);

		m_VtxSysMemStorageSize 	= bytes;
		m_VtxSysMemStorage		= (UInt8*)UNITY_MALLOC_ALIGNED(kMemDynamicGeometry, m_VtxSysMemStorageSize, 32);
	}
	return m_VtxSysMemStorage;
}

void* DynamicGLES2VBO::GetIndexMemory(size_t bytes)
{
	if(m_IdxSysMemStorageSize < bytes)
	{
		UNITY_FREE(kMemDynamicGeometry, m_IdxSysMemStorage);

		m_IdxSysMemStorageSize 	= bytes;
		m_IdxSysMemStorage		= (UInt16*)UNITY_MALLOC_ALIGNED(kMemDynamicGeometry, m_IdxSysMemStorageSize, 32);
	}
	return m_IdxSysMemStorage;
}

static SharedBuffer* ChooseBestBuffer (SharedBuffer* smallBuffer, SharedBuffer* largeBuffer, size_t bytes)
{
	if (!largeBuffer)
		return smallBuffer;

	const size_t couldGrow = smallBuffer->GetAvailableBytes () * 2;
	if (couldGrow >= bytes && couldGrow < largeBuffer->GetAvailableBytes () / 2)
		return smallBuffer;

	return largeBuffer;
}

SharedBuffer* DynamicGLES2VBO::GetSharedVB (size_t bytes)
{
	return ChooseBestBuffer (m_VB, m_LargeVB, bytes);
}

SharedBuffer* DynamicGLES2VBO::GetSharedIB (size_t bytes)
{
	return ChooseBestBuffer (m_IB, m_LargeIB, bytes);
}

inline size_t AlignTemporaryDataStorageSize (size_t size)
{
	// NOTE: to simplify neon optimization - pad to 64 bit (neon register size)
	size = Align (size, 8);
	Assert (size % 8 == 0);
	Assert (size > 0);
	return size;
}

SharedBuffer::SharedBuffer (int bufferType, size_t bytesPerBlock, size_t blocks, bool driverSupportsBufferOrphaning)
:	m_BufferType (bufferType)
,	m_DriverSupportsBufferOrphaning (driverSupportsBufferOrphaning)
,	m_NextBufferIndex (0)
,	m_ReadyBufferIndex (~0UL)
,	m_TemporaryDataStorage (NULL)
,	m_TemporaryDataStorageSize (0)
,	m_LockedBytes (0)
{
	Assert (bufferType == GL_ARRAY_BUFFER || bufferType == GL_ELEMENT_ARRAY_BUFFER);
	Assert (blocks > 0);
	Assert (bytesPerBlock > 0);
	bytesPerBlock = AlignTemporaryDataStorageSize (bytesPerBlock);

	if (m_DriverSupportsBufferOrphaning)
	{
		Assert (blocks == 1);
	}

	if (!gGraphicsCaps.gles20.hasMapbuffer || !m_DriverSupportsBufferOrphaning)
	{
		m_TemporaryDataStorageSize = bytesPerBlock;
		m_TemporaryDataStorage = (UInt8*)UNITY_MALLOC_ALIGNED (kMemDynamicGeometry, m_TemporaryDataStorageSize, 32);
		Assert (m_TemporaryDataStorage);
		memset (m_TemporaryDataStorage, 0x00, m_TemporaryDataStorageSize);
	}

	m_BufferSizes.resize (blocks);
	m_BufferIDs.resize (blocks);

	for (size_t q = 0; q < m_BufferSizes.size(); ++q)
		m_BufferSizes[q] = bytesPerBlock;

	Recreate();

	DebugAssert (m_BufferIDs.size() == m_BufferSizes.size ());
}

void SharedBuffer::Recreate()
{
	GLES_CHK (glGenBuffers (m_BufferIDs.size(), (GLuint*)&m_BufferIDs[0]));

	if (!m_DriverSupportsBufferOrphaning)
	{
		for (size_t q = 0; q < m_BufferIDs.size(); ++q)
		{
			GLES_CHK (glBindBuffer (m_BufferType, m_BufferIDs[q]));
			GLES_CHK(glBufferData(m_BufferType, m_BufferSizes[q], m_TemporaryDataStorage, GL_STREAM_DRAW));
			glRegisterBufferData(m_BufferIDs[q], m_BufferSizes[q], this);
		}
		GLES_CHK (glBindBuffer (m_BufferType, 0));
	}
}

SharedBuffer::~SharedBuffer ()
{
	glDeregisterBufferData(m_BufferIDs.size (), (GLuint*)&m_BufferIDs[0]);
	GLES_CHK(glDeleteBuffers(m_BufferIDs.size (), (GLuint*)&m_BufferIDs[0]));
	UNITY_FREE (kMemDynamicGeometry, m_TemporaryDataStorage);
}

void* SharedBuffer::Lock (size_t bytes)
{
	Assert (m_LockedBytes == 0);

	m_LockedBytes = bytes;

	if (m_TemporaryDataStorage && m_TemporaryDataStorageSize < bytes)
	{
		m_TemporaryDataStorageSize = AlignTemporaryDataStorageSize (bytes);
		m_TemporaryDataStorage = (UInt8*)UNITY_REALLOC (kMemDynamicGeometry, m_TemporaryDataStorage, m_TemporaryDataStorageSize, 32);
		Assert (m_TemporaryDataStorage);
	}

	if (m_DriverSupportsBufferOrphaning)
		return OrphanLock (bytes);
	else
		return SimpleLock (bytes);
}

void SharedBuffer::Unlock (size_t actualBytes)
{
	if (actualBytes == 0)
		actualBytes = m_LockedBytes;

	Assert (actualBytes <= m_LockedBytes);

	if (m_DriverSupportsBufferOrphaning)
		OrphanUnlock (actualBytes);
	else
		SimpleUnlock (actualBytes);

	DebugAssert (m_NextBufferIndex != ~0UL);
	DebugAssert (m_ReadyBufferIndex != ~0UL);

	DebugAssert (m_NextBufferIndex < m_BufferIDs.size());
	DebugAssert (m_ReadyBufferIndex < m_BufferIDs.size());

	m_LockedBytes = 0;
}

void* SharedBuffer::OrphanLock (size_t bytes)
{
	DebugAssert (m_DriverSupportsBufferOrphaning);
	DebugAssert (m_BufferIDs.size () == 1);
	DebugAssert (m_NextBufferIndex == 0);

	UInt8* lockedBuffer = NULL;

	if (gGraphicsCaps.gles20.hasMapbuffer)
	{
		GLES_CHK (glBindBuffer (m_BufferType, m_BufferIDs[0]));
		GLES_CHK (glBufferData (m_BufferType, bytes, 0, GL_STREAM_DRAW)); // orphan old buffer, driver will allocate new storage

		lockedBuffer = reinterpret_cast<UInt8*> (gGlesExtFunc.glMapBufferOES(m_BufferType, GL_WRITE_ONLY_OES));
		GLESAssert();
	}
	else
	{
		Assert (m_TemporaryDataStorage);
		lockedBuffer = m_TemporaryDataStorage;

		GLES_CHK (glBindBuffer (m_BufferType, m_BufferIDs[0]));
		GLES_CHK (glBufferData (m_BufferType, 0, 0, GL_STREAM_DRAW));  // orphan old buffer, driver will allocate new storage
	}

	Assert (lockedBuffer);
	return lockedBuffer;
}

void SharedBuffer::OrphanUnlock (size_t actualBytes)
{
	Assert (m_NextBufferIndex == 0);

	GLES_CHK (glBindBuffer (m_BufferType, m_BufferIDs[0]));

	if (gGraphicsCaps.gles20.hasMapbuffer)	GLES_CHK(gGlesExtFunc.glUnmapBufferOES (m_BufferType));
	else									GLES_CHK(glBufferData (m_BufferType, actualBytes, m_TemporaryDataStorage, GL_STREAM_DRAW));

	GLES_CHK (glBindBuffer (m_BufferType, 0));

	m_NextBufferIndex = 0;
	m_ReadyBufferIndex = 0;
}

void* SharedBuffer::SimpleLock (size_t bytes)
{
	DebugAssert (!m_DriverSupportsBufferOrphaning);
	DebugAssert (!m_BufferIDs.empty ());

	Assert (m_TemporaryDataStorageSize >= bytes);
	Assert (m_TemporaryDataStorage);
	return m_TemporaryDataStorage;
}

void SharedBuffer::SimpleUnlock (size_t actualBytes)
{
	DebugAssert (m_BufferIDs.size() == m_BufferSizes.size ());
	// NOTE: current implementation with naively pick next buffer
	// it is possible that GPU haven't rendered it and CPU will stall
	// however we expect that enough buffers are allocated and stall doesn't happen in practice

	m_ReadyBufferIndex = m_NextBufferIndex;
	++m_NextBufferIndex;
	if (m_NextBufferIndex >= m_BufferIDs.size ())
		m_NextBufferIndex = 0;
	DebugAssert (m_NextBufferIndex != m_ReadyBufferIndex);

	const size_t bufferIndex = m_ReadyBufferIndex;
	DebugAssert (bufferIndex < m_BufferIDs.size());

	Assert (m_TemporaryDataStorageSize >= actualBytes);

	GLES_CHK (glBindBuffer (m_BufferType, m_BufferIDs[bufferIndex]));
	if (m_BufferSizes[bufferIndex] < actualBytes)
	{
		GLES_CHK (glBufferData (m_BufferType, actualBytes, m_TemporaryDataStorage, GL_STREAM_DRAW));
		m_BufferSizes[bufferIndex] = actualBytes;
	}
	else
	{
		GLES_CHK (glBufferSubData (m_BufferType, 0, actualBytes, m_TemporaryDataStorage));
	}
	GLES_CHK (glBindBuffer (m_BufferType, 0));
}

size_t SharedBuffer::GetAvailableBytes () const
{
	return m_BufferSizes[m_NextBufferIndex];
}

SharedBuffer::BufferId SharedBuffer::GetDrawable () const
{
	DebugAssert (m_ReadyBufferIndex != ~0UL);
	DebugAssert (m_ReadyBufferIndex < m_BufferIDs.size());
	if (m_ReadyBufferIndex >= m_BufferIDs.size ())
		return 0;

	return m_BufferIDs[m_ReadyBufferIndex];
}


// WARNING: this is just a temp solution, as we really need to clean up all this mess

static SharedBuffer* lockedSharedBuffer = NULL;
void* LockSharedBufferGLES20 (int bufferType, size_t bytes, bool forceBufferObject)
{
	Assert (bufferType == GL_ARRAY_BUFFER || bufferType == GL_ELEMENT_ARRAY_BUFFER);
	DynamicGLES2VBO& dynamicVBO = static_cast<DynamicGLES2VBO&> (GetRealGfxDevice ().GetDynamicVBO ());

	if(gGraphicsCaps.gles20.slowDynamicVBO && !forceBufferObject)
	{
		if (bufferType == GL_ARRAY_BUFFER)
			return dynamicVBO.GetVertexMemory(bytes);
		else if (bufferType == GL_ELEMENT_ARRAY_BUFFER)
			return dynamicVBO.GetIndexMemory(bytes);
	}
	else
	{
		DebugAssert (lockedSharedBuffer == NULL);
		if (bufferType == GL_ARRAY_BUFFER)
			lockedSharedBuffer = dynamicVBO.GetSharedVB (bytes);
		else if (bufferType == GL_ELEMENT_ARRAY_BUFFER)
			lockedSharedBuffer = dynamicVBO.GetSharedIB (bytes);

		Assert (lockedSharedBuffer);
		return lockedSharedBuffer->Lock (bytes);
	}

	return 0;
}

int UnlockSharedBufferGLES20 (size_t actualBytes, bool forceBufferObject)
{
	if(gGraphicsCaps.gles20.slowDynamicVBO && !forceBufferObject)
	{
		return 0;
	}
	else
	{
		Assert (lockedSharedBuffer);
		lockedSharedBuffer->Unlock (actualBytes);
		const int bufferId = lockedSharedBuffer->GetDrawable ();
		lockedSharedBuffer = NULL;
		return bufferId;
	}
}

#if NV_STATE_FILTERING
	#ifdef glDeleteBuffers
		#undef glDeleteBuffers
	#endif
	#ifdef glBindBuffer
		#undef glBindBuffer
	#endif
	#ifdef glVertexAttribPointer
		#undef glVertexAttribPointer
	#endif
#endif

#endif // GFX_SUPPORTS_OPENGLES20
