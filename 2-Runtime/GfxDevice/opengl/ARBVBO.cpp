#include "UnityPrefix.h"
#include "ARBVBO.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GLAssert.h"
#include "ChannelsGL.h"
#include "UnityGL.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#include "Runtime/Misc/Allocator.h"


extern GLenum kTopologyGL[kPrimitiveTypeCount];


// -----------------------------------------------------------------------------

// 0 = no stats, 1 = overview stats, 2 = detailed stats
#define DEBUG_GL_VBO 0

#if DEBUG_GL_VBO == 2
#define LOGVBO(x) printf_console( "vbo: " x "\n" )
#else
#define LOGVBO(x)
#endif



#if DEBUG_GL_VBO
static int gActiveVBOs = 0;
static int gActiveVBs = 0;
static int gActiveIBs = 0;
static int gUpdatedVBs = 0;
static int gUpdatedIBs = 0;
#endif


// In VBOs, the pointers are just offsets. Just fill start
// of VBO with this number of bytes so that the real offsets are never null.
const int kDummyVBStartBytes = 32;


// Cache the current vertex/index buffers. Don't ever call
// glBindBuffer from anywhere, always use these calls!
// Also make sure to call UnbindVertexBuffersGL() whenever you're starting
// immediate mode rendering or drawing from plain arrays.

static int s_VBOCurrentVB = 0;
static int s_VBOCurrentIB = 0;

void BindARBVertexBuffer( int id )
{
	if( s_VBOCurrentVB != id )
	{
		OGL_CALL(glBindBufferARB( GL_ARRAY_BUFFER_ARB, id ));
		s_VBOCurrentVB = id;
	}
	
	#if GFX_DEVICE_VERIFY_ENABLE
	#ifndef DUMMY_OPENGL_CALLS
	int glbuffer;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != id )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, id ) );
	}
	#endif
	#endif
}

void BindARBIndexBuffer( int id )
{
	if( s_VBOCurrentIB != id )
	{
		OGL_CALL(glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, id ));
		s_VBOCurrentIB = id;
	}
	
	#if GFX_DEVICE_VERIFY_ENABLE
	#ifndef DUMMY_OPENGL_CALLS
	int glbuffer;
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != id )
	{
		ErrorString( Format( "VBO index buffer binding differs from cache (%i != %i)\n", glbuffer, id ) );
	}
	#endif
	#endif
}

void UnbindVertexBuffersGL()
{
	OGL_CALL(glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 ));
	OGL_CALL(glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 ));
	s_VBOCurrentVB = 0;
	s_VBOCurrentIB = 0;
}


// -----------------------------------------------------------------------------

ARBVBO::ARBVBO()
:	m_VertexCount(0)
,	m_VertexBindID(0)
,	m_IndexBindID(0)
,	m_VBSize(0)
,	m_IBSize(0)
{
}

ARBVBO::~ARBVBO ()
{
	if( m_VertexBindID )
	{
		if (m_VertexBindID == s_VBOCurrentVB)
			s_VBOCurrentVB = 0;
			
		glDeleteBuffersARB( 1, (GLuint*)&m_VertexBindID );
	}

	if( m_IndexBindID )
	{
		if (m_IndexBindID == s_VBOCurrentIB)
			s_VBOCurrentIB = 0;

		glDeleteBuffersARB( 1, (GLuint*)&m_IndexBindID );
	}
}

void ARBVBO::VerifyVertexBuffer()
{
	#if GFX_DEVICE_VERIFY_ENABLE && !defined(DUMMY_OPENGL_CALLS)
	int glbuffer;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentVB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentVB ) );
	}
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentIB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentIB ) );
	}
	#endif
}


void ARBVBO::UpdateIndexBufferData (const IndexBufferData& sourceData)
{
	if( !sourceData.indices )
	{
		return;
	}

	LOGVBO( "Update IB" );
	UInt8* buffer = (UInt8*)glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	if( !buffer )
	{
		LOGVBO( "Error mapping IB!" );
		return; // TBD: error!
	}

	// Setup index buffer
	memcpy (buffer, sourceData.indices, sourceData.count * kVBOIndexSize);

	glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );

	#if GFX_DEVICE_VERIFY_ENABLE && !defined(DUMMY_OPENGL_CALLS)
	int glbuffer;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentVB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentVB ) );
	}
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentIB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentIB ) );
	}
	#endif
}

bool ARBVBO::MapVertexStream( VertexStreamData& outData, unsigned stream )
{
	Assert(stream == 0);
	DebugAssertIf( m_VertexBindID == 0 );
	AssertIf( m_IsStreamMapped[stream] );
	m_IsStreamMapped[stream] = true;
	const StreamInfo& info = m_Streams[stream];
	int streamSize = m_VertexCount * info.stride;
	
	LOGVBO( "Map VB" );
	BindARBVertexBuffer( m_VertexBindID );

	// Check if there are other streams in buffer
	bool mapRange = false;
	for (int s = 0; s < kMaxVertexStreams && !mapRange; s++)
		if (s != stream && m_Streams[s].channelMask)
			mapRange = true;

	bool isDynamic = (m_StreamModes[stream] != kStreamModeDynamic);

	UInt8* buffer = NULL;
	if (!UNITY_OSX && gGraphicsCaps.gl.hasArbMapBufferRange)
	{
		GLbitfield access = GL_MAP_WRITE_BIT;
		if (isDynamic)
		{
			// With streams we invalidate the range but preserve the rest of the buffer
			access |= mapRange ? GL_MAP_INVALIDATE_RANGE_BIT : GL_MAP_INVALIDATE_BUFFER_BIT;
		}
		buffer = (UInt8*)glMapBufferRange( GL_ARRAY_BUFFER_ARB, info.offset, streamSize, access );
	}
	else
	{
		// Wipe vertex buffer if we rewrite the whole thing
		if (isDynamic && !mapRange)
			glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_VBSize, NULL, GL_DYNAMIC_DRAW_ARB );

		#if UNITY_OSX
		// Enable explicit flushing of modified data
		if (gGraphicsCaps.gl.hasAppleFlushBufferRange)
			glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
		#endif

		buffer = (UInt8*)glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );

		// We got the start of the buffer so apply offset
		if (buffer)
			buffer += info.offset;
	}

	if( !buffer )
	{
		LOGVBO( "Error mapping VB!" );
		return false;
	}
	
	outData.buffer = buffer;
	outData.channelMask = info.channelMask;
	outData.stride = info.stride;
	outData.vertexCount = m_VertexCount;

	#if GFX_DEVICE_VERIFY_ENABLE && !defined(DUMMY_OPENGL_CALLS)
	int glbuffer;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentVB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentVB ) );
	}
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &glbuffer );
	if( glbuffer != s_VBOCurrentIB )
	{
		ErrorString( Format( "VBO vertex buffer binding differs from cache (%i != %i)\n", glbuffer, s_VBOCurrentIB ) );
	}
	#endif
	
	GetRealGfxDevice().GetFrameStats().AddUploadVBO( streamSize );
	
	return true;
}

void ARBVBO::UnmapVertexStream( unsigned stream )
{
	AssertIf( !m_IsStreamMapped[stream] );
	m_IsStreamMapped[stream] = false;
		
	// Important: bind the needed buffer. Mostly because of multithreaded skinning, the code does not necessarily
	// follow the pattern of bind,map,unmap, bind,map,unmap.
	BindARBVertexBuffer( m_VertexBindID );
	
	#if UNITY_OSX
	// We disabled implicit flushing on unmap, explicitly flush the range we wrote to
	const StreamInfo& info = m_Streams[stream];
	int streamSize = m_VertexCount * info.stride;
	if (gGraphicsCaps.gl.hasAppleFlushBufferRange)
		glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER, info.offset, streamSize);
	#endif

	glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
}


void ARBVBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
				  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount )
{
	// just return if no indices
	if (m_IBSize == 0)
		return;
	
	BindARBIndexBuffer( m_IndexBindID );
	
	// With an index buffer bound OpenGL interprets pointer as an offset
	DrawInternal(channels, reinterpret_cast<const void*>(firstIndexByte), indexCount, topology, vertexCount);
}

void ARBVBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
							   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
{
	BindARBIndexBuffer( 0 );

	DrawInternal(channels, indices, indexCount, topology, drawVertexCount);
}

void ARBVBO::DrawInternal( const ChannelAssigns& channels, const void* indices, UInt32 indexCount,
						  GfxPrimitiveType topology, UInt32 drawVertexCount )
{
	// setup VBO
	DebugAssertIf( IsAnyStreamMapped() );
	DebugAssertIf( m_VertexBindID == 0 || m_IndexBindID == 0 );
	
	BindARBVertexBuffer( m_VertexBindID );
	
	ClearActiveChannelsGL();
	UInt32 targetMap = channels.GetTargetMap();
	for( int i = 0; i < kVertexCompCount; ++i )
	{
		if( !( targetMap & (1<<i) ) )
			continue;
		ShaderChannel src = channels.GetSourceForTarget( (VertexComponent)i );
		if( !m_Channels[src].IsValid() )
			continue;

		SetChannelDataGL( m_Channels[src], m_Streams, (VertexComponent)i );
	}
	GfxDevice& device = GetRealGfxDevice();
	ActivateChannelsGL();
	device.BeforeDrawCall( false );
	
	// draw
	UInt16* indices16 = (UInt16*)indices;
	static UInt32 maxIndices = 63000/6*6;
	// must render in multiples of 3 (triangles) and 2 (tri strips)
	UInt32 offset = 0;
	while( offset < indexCount )
	{
		int drawIndices = std::min( indexCount - offset, maxIndices );
		OGL_CALL(glDrawElements(kTopologyGL[topology], drawIndices, GL_UNSIGNED_SHORT, &indices16[offset] ));	
		offset += drawIndices;
		if (offset < indexCount)
		{
			if (topology == kPrimitiveTriangleStripDeprecated)
				offset -= 2; // primitives overlap by 2 indices
			else if (topology == kPrimitiveLineStrip)
				offset -= 1; // primitives overlap by 2 indices
		}
	}
	
	device.GetFrameStats().AddDrawCall (GetPrimitiveCount(indexCount, topology, true), drawVertexCount);
}



void ARBVBO::UpdateVertexData( const VertexBufferData& buffer )
{
	std::copy(buffer.channels, buffer.channels + kShaderChannelCount, m_Channels);
	std::copy(buffer.streams, buffer.streams + kMaxVertexStreams, m_Streams);
	
	int bufferMode = GL_STATIC_DRAW_ARB;
	// Use dynamic if uploading the second time
	// Or when forced to dynamic
	if (GetVertexStreamMode(0) == kStreamModeNoAccess && m_VertexBindID)
		bufferMode = GL_DYNAMIC_DRAW_ARB;
	else if (GetVertexStreamMode(0) == kStreamModeDynamic)
		bufferMode = GL_DYNAMIC_DRAW_ARB;
			
	if( !m_VertexBindID )
		glGenBuffersARB( 1, (GLuint*)&m_VertexBindID );

	BindARBVertexBuffer( m_VertexBindID );
	glBufferDataARB( GL_ARRAY_BUFFER_ARB, buffer.bufferSize, buffer.buffer, bufferMode );
	GetRealGfxDevice().GetFrameStats().AddUploadVBO( buffer.bufferSize );
	m_VBSize = buffer.bufferSize;
	m_VertexCount = buffer.vertexCount;
	
	VerifyVertexBuffer();
	
	DebugAssertIf(m_VertexBindID == 0);
}

void ARBVBO::UpdateIndexData (const IndexBufferData& buffer)
{
	int buffersize = CalculateIndexBufferSize(buffer);
	m_IBSize = buffersize;
	
	if (m_IBSize == 0)
	{
		Assert (buffer.count == 0);
		return;
	}
	
	if( !m_IndexBindID )
	{
		// initially, generate buffer, set its size and static usage
		glGenBuffersARB( 1, (GLuint*)&m_IndexBindID );
		BindARBIndexBuffer( m_IndexBindID );
		glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_IBSize, NULL, GL_STATIC_DRAW_ARB );
		GetRealGfxDevice().GetFrameStats().AddUploadIB( m_IBSize );
	}
	else
	{
		// discard old and rebuild whole buffer
		#if DEBUG_GL_VBO
		++gUpdatedIBs;
		#endif
		BindARBIndexBuffer( m_IndexBindID );
		glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_IBSize, NULL, GL_DYNAMIC_DRAW_ARB ); // discard old and mark new as dynamic
		GetRealGfxDevice().GetFrameStats().AddUploadIB( m_IBSize );
	}
	
	UpdateIndexBufferData(buffer);
	DebugAssertIf(m_IndexBindID == 0);
}


// -----------------------------------------------------------------------------

#if 0 // see comment in header file

DynamicARBVBO::DynamicARBVBO( UInt32 vbSize )
:	DynamicVBO()
,	m_VBSize(vbSize)
,	m_VBUsedBytes(0)
,	m_VertexBindID(0)
,	m_VBChunkSize(0)
,	m_IBChunk(NULL)
,	m_IBChunkSize(0)
{
}

DynamicARBVBO::~DynamicARBVBO ()
{
	if( m_VertexBindID )
		glDeleteBuffersARB( 1, (GLuint*)&m_VertexBindID );
	delete[] m_IBChunk;
}

void DynamicARBVBO::DrawChunk (const ChannelAssigns& channels)
{
	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;
	
	AssertIf( !m_LastChunkShaderChannelMask || !m_LastChunkStride );
	AssertIf( m_LendedChunk );

	// setup VBO
	DebugAssertIf( m_VertexBindID == 0 );

	BindARBIndexBuffer( 0 );
	BindARBVertexBuffer( m_VertexBindID );

	ClearActiveChannelsGL();
	UInt32 targetMap = channels.GetTargetMap();
	for( int i = 0; i < kVertexCompCount; ++i )
	{
		if( !( targetMap & (1<<i) ) )
			continue;
		ShaderChannel src = channels.GetSourceForTarget( (VertexComponent)i );
		if( !( m_LastChunkShaderChannelMask & (1<<src) ) )
			continue;
		
		SetChannelDataGL( src, (VertexComponent)i, reinterpret_cast<const void*>(m_BufferChannelOffsets[src]), m_LastChunkStride );
	}
	GfxDevice& device = GetRealGfxDevice();
	ActivateChannelsGL();
	device.BeforeDrawCall( false );
	
	// draw
	GfxDeviceStats& stats = device.GetFrameStats();
	int primCount = 0;
	if( m_LastRenderMode == kDrawTriangleStrip )
	{
		OGL_CALL(glDrawArrays( GL_TRIANGLE_STRIP, 0, m_LastChunkVertices ));
		primCount = m_LastChunkVertices-2;
	}
	else if (m_LastRenderMode == kDrawQuads)
	{
		OGL_CALL(glDrawArrays( GL_QUADS, 0, m_LastChunkVertices ));
		primCount = m_LastChunkVertices/2;
	}
	else
	{
		DebugAssertIf( !m_IBChunk );
		OGL_CALL(glDrawElements( GL_TRIANGLES, m_LastChunkIndices, GL_UNSIGNED_SHORT, m_IBChunk ));
		primCount = (m_LastRenderMode == kDrawIndexedTriangleStrip) ? m_LastChunkIndices-2 : m_LastChunkIndices/3;
	}
	stats.AddDrawCall (primCount, m_LastChunkVertices);
	
	GLAssert();
}

bool DynamicARBVBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	AssertIf( m_LendedChunk );
	AssertIf( maxVertices >= 65536 || maxIndices >= 65536*3 );
	DebugAssertIf( renderMode == kDrawIndexedTriangles && outIB == NULL );
	DebugAssertIf( renderMode != kDrawIndexedTriangles && (maxIndices != 0 || outIB != NULL) );
	
	m_LendedChunk = true;
	m_LastChunkShaderChannelMask = shaderChannelMask;
	m_LastRenderMode = renderMode;	
	if( maxVertices == 0 )
		maxVertices = 8;
	
	m_LastChunkStride = 0;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( shaderChannelMask & (1<<i) )
			m_LastChunkStride += VBO::GetChannelByteSize(i);
	}
	
	DebugAssertIf( !outVB );
	UInt32 vbCapacity = maxVertices * m_LastChunkStride;

	bool resizeVB = false;
	if( !m_VertexBindID ) {
		// generate buffer
		glGenBuffersARB( 1, (GLuint*)&m_VertexBindID );
		resizeVB = true;
		m_VBUsedBytes = 0;
	}
	
	// check if requested chunk is larger than current buffer
	if( vbCapacity > m_VBSize ) {
		m_VBSize = vbCapacity * 2; // allocate more up front
		resizeVB = true;
		m_VBUsedBytes = 0;
	}
	
	// if we'll be past the end of buffer, restart from beginning and discard the old one
	if( m_VBUsedBytes + vbCapacity > m_VBSize ) {
		resizeVB = true;
	}
	
	// initialize or resize or discard the buffer
	BindARBVertexBuffer( m_VertexBindID );
	if( resizeVB ) {
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_VBSize + kDummyVBStartBytes, NULL, GL_DYNAMIC_DRAW_ARB );
		#if UNITY_OSX
		if (gGraphicsCaps.gl.hasAppleFlushBufferRange) {
			glBufferParameteriAPPLE(GL_ARRAY_BUFFER, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
		}
		#endif
		m_VBUsedBytes = 0;
	}
	
	// map the buffer
	UInt8* buffer = (UInt8*)glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	if( !buffer ) {
		LOGVBO( "Error mapping VB!" );
		*outVB = NULL;
		m_LendedChunk = false;
		return false;
	}
	
	*outVB = buffer + kDummyVBStartBytes + m_VBUsedBytes;
	
	if( maxIndices && renderMode == kDrawIndexedTriangles )
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

void DynamicARBVBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	AssertIf( !m_LendedChunk );
	AssertIf( actualIndices % 3 != 0 );
	m_LendedChunk = false;
	
	m_LastChunkVertices = actualVertices;
	m_LastChunkIndices = actualIndices;
	
	AssertIf (!m_VertexBindID);
	BindARBVertexBuffer( m_VertexBindID );
	
	UInt32 actualVBSize = actualVertices * m_LastChunkStride;
	#if UNITY_OSX
	if (gGraphicsCaps.gl.hasAppleFlushBufferRange) {
		glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER_ARB, kDummyVBStartBytes + m_VBUsedBytes, actualVBSize);
	}
	#endif
	
	glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	
	if( !actualVertices || (m_LastRenderMode == kDrawIndexedTriangles && !actualIndices) ) {
		m_LastChunkShaderChannelMask = 0;
		return;
	}
	
	// -------- Vertex buffer
	
	size_t channelOffset = kDummyVBStartBytes + m_VBUsedBytes;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( m_LastChunkShaderChannelMask & (1<<i) ) {
			m_BufferChannelOffsets[i] = channelOffset;
			channelOffset += VBO::GetChannelByteSize(i);
		}
	}
	m_VBUsedBytes += actualVBSize;
		
	GLAssert();
}

#endif
