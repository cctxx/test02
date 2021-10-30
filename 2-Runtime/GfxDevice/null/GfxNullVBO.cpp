#include "UnityPrefix.h"
#include "Runtime/Shaders/VBO.h"
#include "GfxNullVBO.h"
#include "Runtime/GfxDevice/GfxDevice.h"



void GfxNullVBO::UpdateVertexData( const VertexBufferData& buffer )
{
	BufferAccessibleVertexData(buffer);
		}

void GfxNullVBO::UpdateIndexData (const IndexBufferData& buffer)
{
}

void GfxNullVBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount )
{
}
#if GFX_ENABLE_DRAW_CALL_BATCHING
	void GfxNullVBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
	{
	}
#endif


bool GfxDynamicNullVBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	AssertIf(this->m_LendedChunk);
	AssertIf((maxVertices >= 65536) || (maxIndices >= (65536 * 3)));
	bool indexed = IsIndexed(renderMode);
	DebugAssertIf(indexed && (outIB == NULL));
	DebugAssertIf(!indexed && ((maxIndices != 0) || (outIB != NULL)));
	
	this->m_LendedChunk = true;
	this->m_LastChunkShaderChannelMask = shaderChannelMask;
	this->m_LastRenderMode = renderMode;

	if (maxVertices == 0)
	{
		maxVertices = 8;
	}
	
	this->m_LastChunkStride = 0;

	for (int i = 0; i < kShaderChannelCount; ++i)
	{
		if (shaderChannelMask & (1 << i))
		{
			this->m_LastChunkStride += GfxNullVBO::GetDefaultChannelByteSize(i);
		}
	}

	//

	DebugAssertIf(NULL == outVB);

	UInt32 vbCapacity = (maxVertices * this->m_LastChunkStride);

	if (vbCapacity > this->vertexBufferSize)
	{
		delete[] this->vertexBuffer;
		this->vertexBuffer = new UInt8[vbCapacity];
		this->vertexBufferSize = vbCapacity;
	}

	*outVB = this->vertexBuffer;

	//
	
	if (maxIndices && indexed)
	{
		UInt32 ibCapacity = maxIndices * kVBOIndexSize;

		if (ibCapacity > this->indexBufferSize)
		{
			delete[] this->indexBuffer;
			this->indexBuffer = new UInt8[ibCapacity];
			this->indexBufferSize = ibCapacity;
		}

		*outIB = this->indexBuffer;
	}

	//

	return true;
}

void GfxDynamicNullVBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	AssertIf(!m_LendedChunk);
	this->m_LendedChunk = false;
	
	this->m_LastChunkVertices = actualVertices;
	this->m_LastChunkIndices = actualIndices;
	
	if (!actualVertices || (IsIndexed(m_LastRenderMode) && !actualIndices))
	{
		this->m_LastChunkShaderChannelMask = 0;
	}
}

void GfxDynamicNullVBO::DrawChunk (const ChannelAssigns& channels)
{
}
