#include "UnityPrefix.h"
#include "NullVBO.h"
#include "UnityGL.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "ChannelsGL.h"


extern GLenum kTopologyGL[kPrimitiveTypeCount];



DynamicNullVBO::DynamicNullVBO()
:	DynamicVBO()
,	m_VBChunk(NULL)
,	m_VBChunkSize(0)
,	m_IBChunk(NULL)
,	m_IBChunkSize(0)
{
}

DynamicNullVBO::~DynamicNullVBO ()
{
	delete[] m_VBChunk;
	delete[] m_IBChunk;
}

void DynamicNullVBO::DrawChunk (const ChannelAssigns& channels)
{
	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;

	Assert(m_LastChunkShaderChannelMask && m_LastChunkStride);
	Assert(!m_LendedChunk);

	// setup VBO	
	UnbindVertexBuffersGL();
	ClearActiveChannelsGL();
	UInt32 targetMap = channels.GetTargetMap();
	for( int i = 0; i < kVertexCompCount; ++i )
	{
		if( !( targetMap & (1<<i) ) )
			continue;
		ShaderChannel src = channels.GetSourceForTarget( (VertexComponent)i );
		if( !( m_LastChunkShaderChannelMask & (1<<src) ) )
			continue;

		SetChannelDataGL( src, (VertexComponent)i, (UInt8*)m_BufferChannel[src], m_LastChunkStride );
	}
	GfxDevice& device = GetRealGfxDevice();
	ActivateChannelsGL();
	device.BeforeDrawCall( false );

	// draw
	int primCount = 0;
	if( m_LastRenderMode == kDrawTriangleStrip )
	{
		Assert(m_LastChunkIndices == 0);
		OGL_CALL(glDrawArrays( GL_TRIANGLE_STRIP, 0, m_LastChunkVertices ));
		primCount = m_LastChunkVertices-2;
	}
	else if (m_LastRenderMode == kDrawQuads)
	{
		Assert(m_LastChunkIndices == 0);
		OGL_CALL(glDrawArrays( GL_QUADS, 0, m_LastChunkVertices ));
		primCount = m_LastChunkVertices/2;
	}
	else if (m_LastRenderMode == kDrawIndexedTriangleStrip)
	{
		DebugAssert(m_LastChunkIndices > 0);
		OGL_CALL(glDrawElements( GL_TRIANGLE_STRIP, m_LastChunkIndices, GL_UNSIGNED_SHORT, m_IBChunk ));
		primCount = m_LastChunkIndices-2;
	}
	else if (m_LastRenderMode == kDrawIndexedLines)
	{
		DebugAssert(m_LastChunkIndices > 0);
		OGL_CALL(glDrawElements( GL_LINES, m_LastChunkIndices, GL_UNSIGNED_SHORT, m_IBChunk ));
		primCount = m_LastChunkIndices/2;
	}
	else if (m_LastRenderMode == kDrawIndexedPoints)
	{
		DebugAssert(m_LastChunkIndices > 0);
		OGL_CALL(glDrawElements( GL_POINTS, m_LastChunkIndices, GL_UNSIGNED_SHORT, m_IBChunk ));
		primCount = m_LastChunkIndices;
	}
	else
	{
		DebugAssert(m_LastChunkIndices > 0);
		OGL_CALL(glDrawElements( GL_TRIANGLES, m_LastChunkIndices, GL_UNSIGNED_SHORT, m_IBChunk ));
		primCount = m_LastChunkIndices/3;
	}
	device.GetFrameStats().AddDrawCall (primCount, m_LastChunkVertices);
}

bool DynamicNullVBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	Assert( !m_LendedChunk );
	Assert( maxVertices < 65536 && maxIndices < 65536*3 );
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

	UInt32 vbCapacity = maxVertices * m_LastChunkStride;
	if( vbCapacity > m_VBChunkSize )
	{
		delete[] m_VBChunk;
		m_VBChunk = new UInt8[ vbCapacity ];
		m_VBChunkSize = vbCapacity;
	}
	*outVB = m_VBChunk;

	const bool indexed = (renderMode != kDrawQuads) && (renderMode != kDrawTriangleStrip);
	if( maxIndices && indexed)
	{
		UInt32 ibCapacity = maxIndices * kVBOIndexSize;
		if( ibCapacity > m_IBChunkSize )
		{
			delete[] m_IBChunk;
			m_IBChunk = new UInt8[ ibCapacity ];
			m_IBChunkSize = ibCapacity;
		}
		*outIB = m_IBChunk;
	}

	return true;
}

void DynamicNullVBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	Assert( m_LendedChunk );
	Assert( m_LastRenderMode == kDrawIndexedTriangleStrip || m_LastRenderMode == kDrawIndexedQuads || m_LastRenderMode == kDrawIndexedPoints || m_LastRenderMode == kDrawIndexedLines || actualIndices % 3 == 0 );
	m_LendedChunk = false;

	m_LastChunkVertices = actualVertices;
	m_LastChunkIndices = actualIndices;

	const bool indexed = (m_LastRenderMode != kDrawQuads) && (m_LastRenderMode != kDrawTriangleStrip);
	if( !actualVertices || (indexed && !actualIndices) ) {
		m_LastChunkShaderChannelMask = 0;
		return;
	}

	UInt8* channelOffset = (UInt8*)m_VBChunk;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( m_LastChunkShaderChannelMask & (1<<i) ) {
			m_BufferChannel[i] = channelOffset;
			channelOffset += VBO::GetDefaultChannelByteSize(i);
		}
	}
}
