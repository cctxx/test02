#ifndef VBO_H
#define VBO_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Filters/Mesh/VertexData.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Modules/ExportModules.h"

class ChannelAssigns;
class Matrix4x4f;


struct MeshPartitionInfo
{
	UInt32 submeshStart;
	UInt32 partitionCount;
	MeshPartitionInfo() {submeshStart=partitionCount=0;}

	DECLARE_SERIALIZE_NO_PPTR (MeshPartitionInfo);
};

struct MeshPartition
{
	int vertexCount;
	int vertexOffset;
	int indexCount;
	int indexByteOffset;
	MeshPartition() { vertexCount = vertexOffset = indexCount = indexByteOffset = 0; }

	DECLARE_SERIALIZE_NO_PPTR (MeshPartition);
};

struct VertexBufferData
{
	ChannelInfoArray channels;
	StreamInfoArray streams;
	UInt8* buffer;
	int bufferSize;
	int vertexCount;

	VertexBufferData()
	{
		// Channels and streams have default constructors
		buffer = 0;
		bufferSize = 0;
		vertexCount = 0;
#if UNITY_PS3
		numBones = 0;
		numInfluences = 0;
		inflPerVertex = 0;
		bones = NULL;
		influences = NULL;
#endif
	}

#if UNITY_PS3
	UInt32 numBones;
	UInt32 numInfluences;
	UInt32 inflPerVertex;
	Matrix4x4f* bones;
	void* influences;

	UNITY_VECTOR(kMemVertexData, MeshPartitionInfo) partInfo;
	UNITY_VECTOR(kMemVertexData, MeshPartition) partitions;
#endif
};

struct IndexBufferData
{
	void*	indices;
	int		count;
	UInt32	hasTopologies; // bitmask
};

struct VertexStreamData
{
	UInt8* buffer;
	UInt32 channelMask;
	int stride;
	int vertexCount;
};

size_t GetChannelFormatSize (UInt8 format);

class EXPORT_COREMODULE VBO : public ListElement
{
public:
	virtual ~VBO() { }

	virtual void UpdateVertexData( const VertexBufferData& buffer ) = 0;
	virtual void UpdateIndexData (const IndexBufferData& buffer) = 0;
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
					  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount ) = 0;
	#if GFX_ENABLE_DRAW_CALL_BATCHING
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount ) = 0;
	#endif

	// recreate hardware buffers
	virtual void Recreate() { }

	enum { kMaxQuads = 65536/4 - 4 }; // so we fit into 16 bit indices, minus some more just in case

	// For writing directly to VBO. VBO must be filled (UpdateData)
	// at least once; and vertex layout + topology from the last fill
	// is used. For example, for skinned meshes you have to call
	// UpdateData on start and each time layout/topology changes;
	// then map,write,unmap for each skinning.
	//
	// In some situations a vertex buffer might become lost; then you need to do UpdateData
	// again before using Map.
	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream ) = 0;
	virtual void UnmapVertexStream( unsigned stream ) = 0;
	virtual bool IsVertexStreamMapped( unsigned stream ) const { return m_IsStreamMapped[stream]; }

	virtual bool IsVertexBufferLost() const = 0;
	virtual bool IsIndexBufferLost() const { return false; }

	virtual bool IsUsingSourceVertices() const { return false; }
	virtual bool IsUsingSourceIndices() const { return false; }

	// WARNING: no checks will be done. So use wuth caution.
	// Actually you should only use it coupled with Mesh unloading its VertexData
	virtual void UnloadSourceVertices()	{}

	// Tell vertices will be mapped from render thread
	virtual void SetMappedFromRenderThread( bool ) {}

	// Whats the access pattern for modifying vertices?
	enum StreamMode
	{
		kStreamModeNoAccess,
		kStreamModeWritePersist,
		kStreamModeDynamic,
		kStreamModeCount
	};

	virtual void SetVertexStreamMode( unsigned stream, StreamMode mode ) { m_StreamModes[stream] = mode; }
	StreamMode GetVertexStreamMode( unsigned stream ) const { return StreamMode(m_StreamModes[stream]); }
	int GetStreamStride( unsigned stream =  0) const { return m_Streams[stream].stride; }

	// TODO: probably unify with vertex streams mode, or extract ibo data altogether in different class
	virtual void	SetIndicesDynamic(bool dynamic)	{ m_IndicesDynamic = dynamic; }
	bool			AreIndicesDynamic() const		{ return m_IndicesDynamic; }

	static int GetDefaultChannelByteSize (int channelNum);
	static int GetDefaultChannelFormat(int channelNum);
    static int GetDefaultChannelDimension(int channelNum);

	virtual int GetRuntimeMemorySize() const = 0;

	bool	GetHideFromRuntimeStats() const			{ return m_HideFromRuntimeStats; }
	void	SetHideFromRuntimeStats( bool flag )	{ m_HideFromRuntimeStats = flag; }

#if GFX_SUPPORTS_D3D9
	virtual void ResetDynamicVB() {}
#endif

	// TODO: that is actually how it would/should work in the feature
	// or at least what i understood speaking with kaspar
	// for now lets limit to gles2 where it is really needed
#if GFX_SUPPORTS_OPENGLES20
	virtual void MarkBuffersLost()	{};
#endif

	virtual void UseAsStreamOutput() { }

#if UNITY_XENON
	virtual void AddExtraUvChannels( const UInt8* data, UInt32 size, int extraUvCount ) = 0;
	virtual void CopyExtraUvChannels( VBO* source ) = 0;
#endif

protected:
	VBO()
	{
		for (int i=0; i<kMaxVertexStreams; i++)
		{
			m_Streams[i].Reset();
			m_StreamModes[i] = kStreamModeNoAccess;
			m_IsStreamMapped[i] = false;
		}
		m_HideFromRuntimeStats = false;
		m_IndicesDynamic = false;
	}

	bool IsAnyStreamMapped() const;
	bool HasStreamWithMode(StreamMode mode) const;

	StreamInfoArray	m_Streams;
	UInt8	m_StreamModes[kMaxVertexStreams];
	bool	m_IsStreamMapped[kMaxVertexStreams];
	bool	m_HideFromRuntimeStats;
	bool	m_IndicesDynamic;
};


// Usage:
// 1) GetChunk
//    if this returns false, don't use data pointers, bail out
// 2) fill with data
// 3) ReleaseChunk
// 4) DrawChunk (possibly multiple times)
//
// The key is that drawing must happen immediately after filling the chunk, because the next
// GetChunk might destroy the previous chunk's data. So never count on chunks being persistent.
class DynamicVBO {
public:
	virtual ~DynamicVBO() { }

	enum RenderMode {
		kDrawIndexedTriangles,		// arbitrary triangle list
		kDrawTriangleStrip,			// no index buffer, one strip
		kDrawQuads,					// no index buffer, four vertices per quad
		kDrawIndexedTriangleStrip,	// arbitrary triangle strip
		kDrawIndexedLines,			// arbitraty line list
		kDrawIndexedPoints,			// arbitraty point lits
		kDrawIndexedQuads,			// arbitraty quad lits
#if UNITY_FLASH
		// ONLY IMPLEMENTED there for better immediate performance
		kDrawTriangles,				// no index buffer triangle list
#endif
	};

	// Gets a chunk of vertex/index buffer to write into.
	//
	// maxVertices/maxIndices is the capacity of the returned chunk; you have to pass actually used
	// amounts in ReleaseChunk afterwards.
	//
	// maxIndices and outIB are only used for kDrawIndexedTriangles render mode.
	// For other ones they must be 0/NULL.
	//
	// Returns false if can't obtain a chunk for whatever reason.
	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB ) = 0;

	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices ) = 0;

	virtual void DrawChunk (const ChannelAssigns& channels) = 0;

	// dynamic vbo might be backed by real vbo that needs to be recreated
	virtual void Recreate() {}

	static bool IsIndexed(RenderMode mode) { return mode == kDrawIndexedTriangles || mode == kDrawIndexedTriangleStrip; }

protected:
	DynamicVBO();

protected:
	UInt32	m_LastChunkShaderChannelMask;
	UInt32	m_LastChunkStride, m_LastChunkVertices, m_LastChunkIndices;
	RenderMode	m_LastRenderMode;
	bool	m_LendedChunk;
};


void CopyVertexStream( const VertexBufferData& sourceData, void* buffer, unsigned stream );
void CopyVertexBuffer( const VertexBufferData& sourceData, void* buffer );

void FillIndexBufferForQuads (UInt16* dst, int dstSize, const UInt16* src, int quadCount);

void GetVertexStreamOffsets( const VertexBufferData& sourceData, size_t dest[kShaderChannelCount], size_t baseOffset, unsigned stream );
void GetVertexStreamPointers( const VertexBufferData& sourceData, void* dest[kShaderChannelCount], void* basePtr, unsigned stream );

static inline int GetVertexChannelSize( const VertexBufferData& buffer, int i )
{
	return VBO::GetDefaultChannelByteSize(i) * buffer.vertexCount;
}

int CalculateOffset( UInt32 channelMask );

static inline int CalculateVertexStreamSize (const StreamInfo& stream, int vertexCount)
{
	return stream.stride * vertexCount;
}

static inline int CalculateVertexStreamSize (const VertexBufferData& buffer, unsigned stream)
{
	Assert(stream < kMaxVertexStreams);
	return CalculateVertexStreamSize(buffer.streams[stream], buffer.vertexCount);
}

const int kVBOIndexSize = sizeof(UInt16);
inline int CalculateIndexBufferSize (const IndexBufferData& buffer)
{
	int size = 0;
	if (buffer.indices)
		size += buffer.count * kVBOIndexSize;
	return size;
}

static inline int GetPrimitiveCount (int indexCount, GfxPrimitiveType topology, bool nativeQuads)
{
	switch (topology) {
	case kPrimitiveTriangles: return indexCount / 3;
	case kPrimitiveTriangleStripDeprecated: return indexCount - 2;
	case kPrimitiveQuads: return nativeQuads ? indexCount/4 : indexCount/4*2;
	case kPrimitiveLines: return indexCount / 2;
	case kPrimitiveLineStrip: return indexCount - 1;
	case kPrimitivePoints: return indexCount;
	default: Assert ("unknown primitive type"); return 0;
	};
}

// Return vertex size in bytes, with components present as specified by the argument
size_t GetVertexSize (unsigned shaderChannelsMask);

#endif
