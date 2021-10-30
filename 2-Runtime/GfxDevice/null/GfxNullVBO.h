#pragma once

#include "Runtime/Shaders/BufferedVBO.h"


class GfxNullVBO : public BufferedVBO
{
public:
	inline GfxNullVBO(void) {}
	virtual ~GfxNullVBO(void) {}

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount );
	#if GFX_ENABLE_DRAW_CALL_BATCHING
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount );
	#endif

	virtual bool IsVertexBufferLost() const { return false; }

	virtual int GetRuntimeMemorySize() const { return 0; }
};



class GfxDynamicNullVBO :
	public DynamicVBO
{
private:
	UInt8 *vertexBuffer;
	UInt32 vertexBufferSize;
	UInt8 *indexBuffer;
	UInt32 indexBufferSize;

public:
	inline GfxDynamicNullVBO(void) :
		vertexBuffer(NULL),
		vertexBufferSize(0),
		indexBuffer(NULL),
		indexBufferSize(0)
	{
	}

	virtual ~GfxDynamicNullVBO(void)
	{
		delete[] this->vertexBuffer;
		delete[] this->indexBuffer;
	}

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);

};