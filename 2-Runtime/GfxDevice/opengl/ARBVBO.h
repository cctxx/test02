#pragma once

#include "Runtime/Shaders/VBO.h"

void BindARBVertexBuffer( int id );
void BindARBIndexBuffer( int id );

//	Implements VBO through ARB_vertex_buffer_object.
class ARBVBO : public VBO {
public:
	ARBVBO();
	virtual ~ARBVBO();

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
					  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount);
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount );
	virtual int GetRuntimeMemorySize() const { return m_VBSize + m_IBSize; }

	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );
	virtual bool IsVertexBufferLost() const { return false; }


private:
	void DrawInternal( const ChannelAssigns& channels, const void* indices, UInt32 indexCount,
					  GfxPrimitiveType topology, UInt32 drawVertexCount );
	void VerifyVertexBuffer();
	void UpdateIndexBufferData (const IndexBufferData& sourceData);

private:
	ChannelInfoArray m_Channels;
	int     m_VertexCount;
	int		m_VertexBindID;
	int		m_IndexBindID;
	int     m_VBSize;
	int     m_IBSize;
};


#if 0

// Ok, dynamic VBOs with updating just the chunks with BufferSubData seem to be slower (by 30% - 50%)
// than just rendering from memory arrays. This is on MacBook Pro RX1600 (10.4.9), MBP GeForce8600 (10.5.7)
// and PC GeForce 6800 (93.xx). Using APPLE_flush_buffer_range does not help with performance.
// So we just don't use them for now!

// This is dynamic vertex buffer, with index buffer just fed from memory.
// The reason is that BufferSubData on index buffers is really buggy on some drivers
// (e.g. Mac OS X 10.4.8/10.4.9 on X1600).
class DynamicARBVBO : public DynamicVBO {
public:
	DynamicARBVBO( UInt32 vbSize );
	virtual ~DynamicARBVBO();

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);

private:
	size_t	m_BufferChannelOffsets[kShaderChannelCount];

	UInt32	m_VBSize;
	UInt32	m_VBUsedBytes;

	int		m_VertexBindID;
	UInt32	m_VBChunkSize;
	UInt8*	m_IBChunk;
	UInt32	m_IBChunkSize;
};

#endif

