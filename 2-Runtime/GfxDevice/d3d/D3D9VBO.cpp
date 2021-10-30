#include "UnityPrefix.h"
#include "D3D9VBO.h"
#include "D3D9Context.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "D3D9Utils.h"
#include "GfxDeviceD3D9.h"
#include "Runtime/Profiler/MemoryProfiler.h"


// defined in GfxDeviceD3D9.cpp
IDirect3DVertexDeclaration9* GetD3DVertexDeclaration( UInt32 shaderChannelsMap );
void UpdateChannelBindingsD3D( const ChannelAssigns& channels );


// Define this to 1 to make VBO operations randomly fail.
// Use this to test error checking code.
#define DEBUG_RANDOMLY_FAIL_D3D_VBO 0


#if !DEBUGMODE && DEBUG_RANDOMLY_FAIL_D3D_VBO
#error Never enable random VBO failures on release code!
#endif

#if DEBUG_RANDOMLY_FAIL_D3D_VBO
#define RANDOM_FAIL_FOR_DEBUG - ((rand()%8==0) ? 100000000 : 0)
#else
#define RANDOM_FAIL_FOR_DEBUG
#endif


static const D3DPRIMITIVETYPE kTopologyD3D9[kPrimitiveTypeCount] =
{
	D3DPT_TRIANGLELIST,
	D3DPT_TRIANGLESTRIP,
	D3DPT_TRIANGLELIST, //@TODO: make work
	D3DPT_LINELIST,
	D3DPT_LINESTRIP,
	D3DPT_POINTLIST,
};


// -----------------------------------------------------------------------------

IDirect3DIndexBuffer9*	D3D9VBO::ms_CustomIB = NULL;
int	D3D9VBO::ms_CustomIBSize = 0;
UInt32 D3D9VBO::ms_CustomIBUsedBytes = 0;

D3D9VBO::D3D9VBO()
:	m_IB(NULL)
,	m_IBSize(0)
{
	memset(m_VertexDecls, 0, sizeof(m_VertexDecls));
	memset(m_VBStreams, 0, sizeof(m_VBStreams));
}

D3D9VBO::~D3D9VBO ()
{
	for( int s = 0; s < kMaxVertexStreams; s++ )
	{
		if( m_VBStreams[s] ) {
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[s]);
			ULONG refCount = m_VBStreams[s]->Release();
			AssertIf( refCount != 0 );
			m_VBStreams[s] = NULL;
		}
	}
	if( m_IB ) {
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
		ULONG refCount = m_IB->Release();
		AssertIf( refCount != 0 );
		m_IB = NULL;
	}

}


void D3D9VBO::ResetDynamicVB()
{
	// Gets called on all VBs and ignores non-dynamic ones
	for( int s = 0; s < kMaxVertexStreams; s++ )
	{
		if( m_StreamModes[s] == kStreamModeDynamic )
		{
			// Vertex buffer can be null when switching fullscreen in web player.
			// There we lose device a couple of times, and ResetDynamicVB is called several
			// times in succession.
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[s]);
			SAFE_RELEASE( m_VBStreams[s] );
		}
	}
}

void D3D9VBO::CleanupSharedIndexBuffer()
{
	if( ms_CustomIB ) 
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(ms_CustomIB);
		ULONG refCount = ms_CustomIB->Release();
		AssertIf( refCount != 0 );
		ms_CustomIBSize = 0;
		ms_CustomIBUsedBytes = 0;
		ms_CustomIB = NULL;
	}
}

void D3D9VBO::BindVertexStreams( IDirect3DDevice9* dev, const ChannelAssigns& channels )
{
	int freeStream = -1;
	for( int s = 0; s < kMaxVertexStreams; s++ )
	{
		if( m_VBStreams[s] )
			D3D9_CALL( dev->SetStreamSource( s, m_VBStreams[s], 0, m_Streams[s].stride ) );
		else
			freeStream = s;
	}
	int declIndex = kVertexDeclDefault;
	if ((channels.GetSourceMap() & VERTEX_FORMAT1(Color)) && !m_ChannelInfo[kShaderChannelColor].IsValid())
	{
		if (freeStream != -1)
		{
			declIndex = kVertexDeclAllWhiteStream;
			if (!m_VertexDecls[declIndex])
			{
				ChannelInfoArray channelInfo;
				memcpy(&channelInfo, m_ChannelInfo, sizeof(channelInfo));
				ChannelInfo& colorInfo = channelInfo[kShaderChannelColor];
				colorInfo.stream = freeStream;
				colorInfo.offset = 0;
				colorInfo.format = kChannelFormatColor;
				colorInfo.dimension = 1;
				m_VertexDecls[declIndex] = GetD3D9GfxDevice().GetVertexDecls().GetVertexDecl( channelInfo );
			}
			IDirect3DVertexBuffer9* whiteVB = GetD3D9GfxDevice().GetAllWhiteVertexStream();
			D3D9_CALL( dev->SetStreamSource( freeStream, whiteVB, 0, sizeof(D3DCOLOR) ) );
		}
		else
			ErrorString("Need a free stream to add default vertex colors!");
	}
	D3D9_CALL( dev->SetVertexDeclaration( m_VertexDecls[declIndex] ) );
	UpdateChannelBindingsD3D( channels );
}

void D3D9VBO::UpdateVertexStream( const VertexBufferData& sourceData, unsigned stream )
{
	DebugAssert( !m_IsStreamMapped[stream] );
	const StreamInfo& srcStream = sourceData.streams[stream];
	int oldSize = CalculateVertexStreamSize(m_Streams[stream], m_VertexCount);
	int newSize = CalculateVertexStreamSize(srcStream, sourceData.vertexCount);
	m_Streams[stream] = srcStream;
	if (newSize == 0)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[stream]);
		SAFE_RELEASE( m_VBStreams[stream] );
		return;
	}

	const bool isDynamic = (m_StreamModes[stream] == kStreamModeDynamic);
	DWORD usage = isDynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : (D3DUSAGE_WRITEONLY);
	D3DPOOL pool = isDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	
	if( m_VBStreams[stream] == NULL || newSize != oldSize )
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[stream]);
		SAFE_RELEASE( m_VBStreams[stream] );
		IDirect3DDevice9* dev = GetD3DDevice();
		HRESULT hr = dev->CreateVertexBuffer( newSize RANDOM_FAIL_FOR_DEBUG, usage, 0, pool, &m_VBStreams[stream], NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_VBStreams[stream],newSize,this);
		if( FAILED(hr) )
		{
			printf_console( "d3d: failed to create vertex buffer of size %d [%s]\n", newSize, GetD3D9Error(hr) );
			return;
		}
	}

	// Don't update contents if there is no source data.
	// This is used to update the vertex declaration only, leaving buffer intact.
	// Also to create an empty buffer that is written to later.
	if (!sourceData.buffer)
		return;

	UInt8* buffer;
	HRESULT hr = m_VBStreams[stream]->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, (void**)&buffer, isDynamic ? D3DLOCK_DISCARD : 0 );
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to lock vertex buffer %p [%s]\n", m_VBStreams[stream], GetD3D9Error(hr) );
		return;
	}
	CopyVertexStream( sourceData, buffer, stream );

	m_VBStreams[stream]->Unlock();
}


void D3D9VBO::UpdateIndexBufferData (const IndexBufferData& sourceData)
{
	if( !sourceData.indices )
	{
		m_IBSize = 0;
		return;
	}

	AssertIf( !m_IB );
	UInt8* buffer;
	HRESULT hr = m_IB->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, (void**)&buffer, 0 );
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to lock index buffer %p [%s]\n", m_IB, GetD3D9Error(hr) );
		return;
	}

	memcpy (buffer, sourceData.indices, sourceData.count * kVBOIndexSize);

	m_IB->Unlock();
}

bool D3D9VBO::MapVertexStream( VertexStreamData& outData, unsigned stream )
{
	if( m_VBStreams[stream] == NULL )
	{
		printf_console( "d3d: attempt to map null vertex buffer\n" );
		return false;
	}
	DebugAssertIf( IsVertexBufferLost() );
	AssertIf( m_IsStreamMapped[stream] );
	
	const bool isDynamic = (m_StreamModes[stream] == kStreamModeDynamic);

	UInt8* buffer;
	int vbSize = CalculateVertexStreamSize(m_Streams[stream], m_VertexCount);
	HRESULT hr = m_VBStreams[stream]->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, (void**)&buffer, isDynamic ? D3DLOCK_DISCARD : 0 );
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to map vertex buffer %p of size %i [%s]\n", m_VBStreams[stream], vbSize, GetD3D9Error(hr) );
		return false;
	}
	m_IsStreamMapped[stream] = true;
	
	outData.buffer = buffer;
	outData.channelMask = m_Streams[stream].channelMask;
	outData.stride = m_Streams[stream].stride;
	outData.vertexCount = m_VertexCount;
	
	GetRealGfxDevice().GetFrameStats().AddUploadVBO( vbSize );

	return true;
}

void D3D9VBO::UnmapVertexStream( unsigned stream )
{
	DebugAssert( m_VBStreams[stream] );
	AssertIf( !m_IsStreamMapped[stream] );
	m_IsStreamMapped[stream] = false;
	m_VBStreams[stream]->Unlock();
}
	
bool D3D9VBO::IsVertexBufferLost() const
{
	for( int s = 0; s < kMaxVertexStreams; s++ )
		if( m_Streams[s].channelMask && !m_VBStreams[s] )
			return true;

	return false;
}

int D3D9VBO::GetRuntimeMemorySize() const
{
#if ENABLE_MEM_PROFILER
	return GetMemoryProfiler()->GetRelatedMemorySize(this) 
		+ GetMemoryProfiler()->GetRelatedIDMemorySize((UInt32)this);
#else
	return 0;
#endif
/*	int vertexSize = 0;
	for( int s = 0; s < kMaxVertexStreams; s++ )
		vertexSize += m_Streams[s].stride;

	return vertexSize * m_VertexCount + m_IBSize;*/
}


void D3D9VBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount)
{
	// just return if no indices
	if( m_IBSize == 0 )
		return;

	HRESULT hr;

	if( m_VBStreams[0] == NULL || m_IB == NULL )
	{
		printf_console( "d3d: VB or IB is null\n" );
		return;
	}
	
	GfxDevice& device = GetRealGfxDevice();
	IDirect3DDevice9* dev = GetD3DDevice();

	BindVertexStreams( dev, channels );
	device.BeforeDrawCall( false );

	if (topology == kPrimitiveQuads)
	{
		UInt32 ibBytesLocked;
		UInt16* ibPtr = MapDynamicIndexBuffer (indexCount/4*6, ibBytesLocked);
		if (!ibPtr)
			return;
		const UInt16* ibSrc = NULL;
		hr = m_IB->Lock (firstIndexByte, indexCount*kVBOIndexSize, (void**)&ibSrc, D3DLOCK_READONLY);
		if (FAILED(hr))
		{
			UnmapDynamicIndexBuffer();
			return;
		}
		FillIndexBufferForQuads (ibPtr, ibBytesLocked, ibSrc, indexCount/4);
		m_IB->Unlock ();
		UnmapDynamicIndexBuffer ();
		firstIndexByte = ms_CustomIBUsedBytes;
		ms_CustomIBUsedBytes += ibBytesLocked;
		D3D9_CALL(dev->SetIndices(ms_CustomIB));
	}
	else
	{
		D3D9_CALL(dev->SetIndices( m_IB ));
	}

	// draw
	D3DPRIMITIVETYPE primType = kTopologyD3D9[topology];
	int primCount = GetPrimitiveCount (indexCount, topology, false);
	hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive (primType, 0, firstVertex, vertexCount, firstIndexByte/2, primCount));
	Assert(SUCCEEDED(hr));

	device.GetFrameStats().AddDrawCall (primCount, vertexCount);
}

UInt16* D3D9VBO::MapDynamicIndexBuffer (int indexCount, UInt32& outBytesUsed)
{
	HRESULT hr;
	const UInt32 kMaxIndices = 64000; // Smaller threshold than absolutely necessary
	Assert (indexCount <= kMaxIndices);
	indexCount = std::min<UInt32>(indexCount, kMaxIndices);
	int ibCapacity = indexCount * kVBOIndexSize;
	int newIBSize = std::max (ibCapacity, 32*1024); // 32k IB at least

	if (newIBSize > ms_CustomIBSize)
	{
		if (ms_CustomIB)
		{
			REGISTER_EXTERNAL_GFX_DEALLOCATION(ms_CustomIB);
			ms_CustomIB->Release();
		}
		ms_CustomIBSize = newIBSize;
		ms_CustomIBUsedBytes = 0;

		IDirect3DDevice9* dev = GetD3DDevice();
		HRESULT hr = dev->CreateIndexBuffer (ms_CustomIBSize RANDOM_FAIL_FOR_DEBUG, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT , &ms_CustomIB, NULL);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(ms_CustomIB,ms_CustomIBSize,0);

		if( FAILED(hr) )
		{
			printf_console ("d3d: failed to create custom index buffer of size %d [%s]\n", newIBSize, GetD3D9Error(hr));
			return NULL;
		}
	}

	UInt16* buffer;
	if (ms_CustomIBUsedBytes + ibCapacity > ms_CustomIBSize)
	{
		hr = ms_CustomIB->Lock (0 RANDOM_FAIL_FOR_DEBUG, ibCapacity, (void**)&buffer, D3DLOCK_DISCARD);
		if (FAILED(hr))
		{
			printf_console ("d3d: failed to lock shared index buffer with discard [%s]\n", GetD3D9Error(hr));
			return NULL;
		}
		ms_CustomIBUsedBytes = 0;
	} 
	else 
	{
		hr = ms_CustomIB->Lock (ms_CustomIBUsedBytes RANDOM_FAIL_FOR_DEBUG, ibCapacity, (void**)&buffer, D3DLOCK_NOOVERWRITE);
		if (FAILED(hr))
		{
			printf_console ("d3d: failed to lock shared index buffer, offset %i size %i [%s]\n", ms_CustomIBUsedBytes, ibCapacity, GetD3D9Error(hr));
			return NULL;
		}
	}
	outBytesUsed = ibCapacity;

	return buffer;
}

void D3D9VBO::UnmapDynamicIndexBuffer ()
{
	ms_CustomIB->Unlock();
}


#if GFX_ENABLE_DRAW_CALL_BATCHING
	void D3D9VBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
	{
		Assert(!m_IsStreamMapped[0]);

		if (m_VBStreams[0] == NULL)
		{
			printf_console( "d3d: VB is null\n" );
			return;
		}
		UInt32 ibBytesUsed;
		UInt16* ibPtr = MapDynamicIndexBuffer (indexCount, ibBytesUsed);
		if (!ibPtr)
			return;
		memcpy (ibPtr, indices, ibBytesUsed);
		UnmapDynamicIndexBuffer ();

		GfxDevice& device = GetRealGfxDevice();
		IDirect3DDevice9* dev = GetD3DDevice();
		HRESULT hr;

		BindVertexStreams( dev, channels );
		device.BeforeDrawCall( false );

		D3D9_CALL(dev->SetIndices( ms_CustomIB ));

		D3DPRIMITIVETYPE primType = kTopologyD3D9[topology];
		int primCount = GetPrimitiveCount (indexCount, topology, false);
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive(primType, 0, vertexRangeBegin, vertexRangeEnd-vertexRangeBegin, ms_CustomIBUsedBytes / kVBOIndexSize, primCount));
		Assert(SUCCEEDED(hr));
		ms_CustomIBUsedBytes += ibBytesUsed;

		device.GetFrameStats().AddDrawCall (primCount, drawVertexCount);
	}
#endif


void D3D9VBO::UpdateVertexData( const VertexBufferData& buffer )
{
	// Old vertex count and streams are still used here
	for (unsigned stream = 0; stream < kMaxVertexStreams; stream++)
		UpdateVertexStream( buffer, stream );

	memcpy( m_ChannelInfo, buffer.channels, sizeof(m_ChannelInfo) );
	memset( m_VertexDecls, 0, sizeof(m_VertexDecls) );
	m_VertexDecls[kVertexDeclDefault] = GetD3D9GfxDevice().GetVertexDecls().GetVertexDecl( m_ChannelInfo );
	m_VertexCount = buffer.vertexCount;
}

void D3D9VBO::UpdateIndexData (const IndexBufferData& buffer)
{
	IDirect3DDevice9* dev = GetD3DDevice();
	int newSize = CalculateIndexBufferSize(buffer);

	if( !m_IB )
	{
		// initially, create a static buffer
		HRESULT hr = dev->CreateIndexBuffer( newSize RANDOM_FAIL_FOR_DEBUG, (buffer.hasTopologies & (1<<kPrimitiveQuads)) ? 0 : D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &m_IB, NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_IB,newSize,this);
		if( FAILED(hr) )
		{
			printf_console( "d3d: failed to create index buffer of size %d [%s]\n", newSize, GetD3D9Error(hr) );
			return;
		}
	}
	else
	{
		if( newSize != m_IBSize )
		{
			IDirect3DIndexBuffer9* oldIB = m_IB;
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
			m_IB->Release();
			HRESULT hr = dev->CreateIndexBuffer( newSize RANDOM_FAIL_FOR_DEBUG, (buffer.hasTopologies & (1<<kPrimitiveQuads)) ? 0 : D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &m_IB, NULL );
			REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_IB,newSize,this);
			if( FAILED(hr) )
			{
				printf_console( "d3d: failed to resize index buffer %p to size %d [%s]\n", oldIB, newSize, GetD3D9Error(hr) );
				return;
			}
		}
	}
	m_IBSize = newSize;
	UpdateIndexBufferData(buffer);
}

// -----------------------------------------------------------------------------


DynamicD3D9VBO::DynamicD3D9VBO( UInt32 vbSize, UInt32 ibSize )
:	DynamicVBO()
,	m_VBSize(vbSize)
,	m_VBUsedBytes(0)
,	m_IBSize(ibSize)
,	m_IBUsedBytes(0)
,	m_VB(NULL)
,	m_IB(NULL)
,	m_VertexDecl(NULL)
,	m_LastChunkStartVertex(0)
,	m_LastChunkStartIndex(0)
,	m_QuadsIB(NULL)
,	m_QuadsIBFailed(false)
{
}

DynamicD3D9VBO::~DynamicD3D9VBO ()
{
	if( m_VB ) {
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VB);
		ULONG refCount = m_VB->Release();
		AssertIf( refCount != 0 );
	}
	if( m_IB ) {
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
		ULONG refCount = m_IB->Release();
		AssertIf( refCount != 0 );
	}
	if( m_QuadsIB ) {
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_QuadsIB);
		ULONG refCount = m_QuadsIB->Release();
		AssertIf( refCount != 0 );
	}
}

void DynamicD3D9VBO::InitializeQuadsIB()
{
	AssertIf( m_QuadsIB );

	IDirect3DDevice9* dev = GetD3DDevice();
	HRESULT hr = dev->CreateIndexBuffer( VBO::kMaxQuads * 6 * kVBOIndexSize RANDOM_FAIL_FOR_DEBUG, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &m_QuadsIB, NULL );
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_QuadsIB,VBO::kMaxQuads * 6 * kVBOIndexSize,this);
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to create quads index buffer [%s]\n", GetD3D9Error(hr) );
		m_QuadsIBFailed = true;
		return;
	}
	UInt16* ib = NULL;
	hr = m_QuadsIB->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, (void**)&ib, 0 );
	if( FAILED(hr) )
	{
		printf_console( "d3d: failed to lock quads index buffer [%s]\n", GetD3D9Error(hr) );
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_QuadsIB);
		m_QuadsIB->Release();
		m_QuadsIB = NULL;
		m_QuadsIBFailed = true;
		return;
	}

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

	m_QuadsIB->Unlock();
}

void DynamicD3D9VBO::DrawChunk (const ChannelAssigns& channels)
{
	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;

	HRESULT hr;

	AssertIf( !m_LastChunkShaderChannelMask || !m_LastChunkStride );
	AssertIf( m_LendedChunk );

	GfxDevice& device = GetRealGfxDevice();
	IDirect3DDevice9* dev = GetD3DDevice();

	// setup VBO
	DebugAssertIf( !m_VB );
	D3D9_CALL(dev->SetStreamSource( 0, m_VB, 0, m_LastChunkStride ));
	D3D9_CALL(dev->SetVertexDeclaration( m_VertexDecl ));
	UpdateChannelBindingsD3D( channels );
	device.BeforeDrawCall( false );

	// draw
	GfxDeviceStats& stats = device.GetFrameStats();
	int primCount = 0;
	if( m_LastRenderMode == kDrawTriangleStrip )
	{
		hr = D3D9_CALL_HR(dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, m_LastChunkStartVertex, m_LastChunkVertices-2 ));
		primCount = m_LastChunkVertices-2;
	}
	else if (m_LastRenderMode == kDrawIndexedTriangleStrip)
	{
		DebugAssertIf( !m_IB );
		D3D9_CALL(dev->SetIndices( m_IB ));
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, m_LastChunkStartVertex, 0, m_LastChunkVertices, m_LastChunkStartIndex, m_LastChunkIndices-2 ));
		primCount = m_LastChunkIndices-2;
	}	
	else if( m_LastRenderMode == kDrawQuads )
	{
		// initialize quads index buffer if needed
		if( !m_QuadsIB )
			InitializeQuadsIB();
		// if quads index buffer has valid data, draw with it
		if( !m_QuadsIBFailed )
		{
			D3D9_CALL(dev->SetIndices( m_QuadsIB ));
			hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, m_LastChunkStartVertex, 0, m_LastChunkVertices, 0, m_LastChunkVertices/2 ));
			primCount = m_LastChunkVertices/2;
		}
	}
	else if (m_LastRenderMode == kDrawIndexedLines)
	{
		DebugAssertIf( !m_IB );
		D3D9_CALL(dev->SetIndices( m_IB ));
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive( D3DPT_LINELIST, m_LastChunkStartVertex, 0, m_LastChunkVertices, m_LastChunkStartIndex, m_LastChunkIndices/2 ));
		primCount = m_LastChunkIndices/2;
	}
	else if (m_LastRenderMode == kDrawIndexedPoints)
	{
		DebugAssertIf( !m_IB );
		D3D9_CALL(dev->SetIndices( m_IB ));
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive( D3DPT_POINTLIST, m_LastChunkStartVertex, 0, m_LastChunkVertices, m_LastChunkStartIndex, m_LastChunkIndices ));
		primCount = m_LastChunkIndices;
	}
	else
	{
		DebugAssertIf( !m_IB );
		D3D9_CALL(dev->SetIndices( m_IB ));
		hr = D3D9_CALL_HR(dev->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, m_LastChunkStartVertex, 0, m_LastChunkVertices, m_LastChunkStartIndex, m_LastChunkIndices/3 ));
		primCount = m_LastChunkIndices/3;
	}
	stats.AddDrawCall (primCount, m_LastChunkVertices);
	AssertIf(FAILED(hr));
}

bool DynamicD3D9VBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	Assert( !m_LendedChunk );
	Assert( maxVertices < 65536 && maxIndices < 65536*3 );
	Assert(!((renderMode == kDrawQuads) && (VBO::kMaxQuads*4 < maxVertices)));
	DebugAssertMsg(outVB != NULL && maxVertices > 0, "DynamicD3D9VBO::GetChunk - outVB: 0x%08x maxVertices: %d", outVB, maxVertices);
	DebugAssertMsg(
				(renderMode == kDrawIndexedQuads			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedPoints			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedLines			&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangles		&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawIndexedTriangleStrip	&& (outIB != NULL && maxIndices > 0)) ||
				(renderMode == kDrawTriangleStrip			&& (outIB == NULL && maxIndices == 0)) ||
				(renderMode == kDrawQuads					&& (outIB == NULL && maxIndices == 0)),
				"DynamicD3D9VBO::GetChunk - renderMode: %d outIB: 0x%08x maxIndices: %d", renderMode, outIB, maxIndices);
	HRESULT hr;
	bool success = true;

	m_LendedChunk = true;
	m_LastRenderMode = renderMode;

	if( maxVertices == 0 )
		maxVertices = 8;

	m_LastChunkStride = 0;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( shaderChannelMask & (1<<i) )
			m_LastChunkStride += VBO::GetDefaultChannelByteSize(i);
	}
	if (shaderChannelMask != m_LastChunkShaderChannelMask)
	{
		m_VertexDecl = GetD3DVertexDeclaration( shaderChannelMask );
		m_LastChunkShaderChannelMask = shaderChannelMask;
	}
	IDirect3DDevice9* dev = GetD3DDevice();

	// -------- vertex buffer

	DebugAssertIf( !outVB );
	UInt32 vbCapacity = maxVertices * m_LastChunkStride;
	// check if requested chunk is larger than current buffer
	if( vbCapacity > m_VBSize ) {
		m_VBSize = vbCapacity * 2; // allocate more up front
		if( m_VB ){
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VB);
			m_VB->Release();
		}
		m_VB = NULL;
	}
	// allocate buffer if don't have it yet
	if( !m_VB ) {
		hr = dev->CreateVertexBuffer( m_VBSize RANDOM_FAIL_FOR_DEBUG, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_VB, NULL );
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_VB,m_VBSize,this);
		if( FAILED(hr) )
		{
			printf_console( "d3d: failed to create dynamic vertex buffer of size %d [%s]\n", m_VBSize, GetD3D9Error(hr) );
			success = false;
			*outVB = NULL;
		}
	}

	// lock, making sure the offset we lock is multiple of vertex stride
	if( m_VB )
	{
		m_VBUsedBytes = ((m_VBUsedBytes + (m_LastChunkStride-1)) / m_LastChunkStride) * m_LastChunkStride;
		if( m_VBUsedBytes + vbCapacity > m_VBSize ) {
			hr = m_VB->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, outVB, D3DLOCK_DISCARD );
			if( FAILED(hr) )
			{
				printf_console( "d3d: failed to lock dynamic vertex buffer with discard [%s]\n", GetD3D9Error(hr) );
				*outVB = NULL;
				success = false;
			}
			m_VBUsedBytes = 0;
		} else {
			hr = m_VB->Lock( m_VBUsedBytes RANDOM_FAIL_FOR_DEBUG, vbCapacity, outVB, D3DLOCK_NOOVERWRITE );
			if( FAILED(hr) )
			{
				printf_console( "d3d: failed to lock vertex index buffer, offset %i size %i [%s]\n", m_VBUsedBytes, vbCapacity, GetD3D9Error(hr) );
				*outVB = NULL;
				success = false;
			}
		}
		m_LastChunkStartVertex = m_VBUsedBytes / m_LastChunkStride;
		DebugAssertIf( m_LastChunkStartVertex * m_LastChunkStride != m_VBUsedBytes );
	}

	// -------- index buffer

	const bool indexed = (renderMode != kDrawQuads) && (renderMode != kDrawTriangleStrip);
	if( success && maxIndices && indexed )
	{
		UInt32 ibCapacity = maxIndices * kVBOIndexSize;
		// check if requested chunk is larger than current buffer
		if( ibCapacity > m_IBSize ) {
			m_IBSize = ibCapacity * 2; // allocate more up front
			if( m_IB ){
				REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
				m_IB->Release();
			}
			m_IB = NULL;
		}
		// allocate buffer if don't have it yet
		if( !m_IB ) {
			hr = dev->CreateIndexBuffer( m_IBSize RANDOM_FAIL_FOR_DEBUG, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_IB, NULL );
			REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_IB,m_IBSize,this);
			if( FAILED(hr) )
			{
				printf_console( "d3d: failed to create dynamic index buffer of size %d [%s]\n", m_IBSize, GetD3D9Error(hr) );
				if( m_VB )
					m_VB->Unlock();
			}
		}
		// lock it if we have IB created successfully
		if( m_IB )
		{
			if( m_IBUsedBytes + ibCapacity > m_IBSize ) {
				hr = m_IB->Lock( 0 RANDOM_FAIL_FOR_DEBUG, 0, outIB, D3DLOCK_DISCARD );
				if( FAILED(hr) )
				{
					printf_console( "d3d: failed to lock dynamic index buffer with discard [%s]\n", GetD3D9Error(hr) );
					*outIB = NULL;
					success = false;
					if( m_VB )
						m_VB->Unlock();
				}
				m_IBUsedBytes = 0;
			} else {
				hr = m_IB->Lock( m_IBUsedBytes RANDOM_FAIL_FOR_DEBUG, ibCapacity, outIB, D3DLOCK_NOOVERWRITE );
				if( FAILED(hr) )
				{
					printf_console( "d3d: failed to lock dynamic index buffer, offset %i size %i [%s]\n", m_IBUsedBytes, ibCapacity, GetD3D9Error(hr) );
					*outIB = NULL;
					success = false;
					if( m_VB )
						m_VB->Unlock();
				}
			}
			m_LastChunkStartIndex = m_IBUsedBytes / 2;
		}
		else
		{
			*outIB = NULL;
			success = false;
		}
	}

	if( !success )
		m_LendedChunk = false;

	return success;
}

void DynamicD3D9VBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	Assert( m_LendedChunk );
	Assert( m_LastRenderMode == kDrawIndexedTriangleStrip || m_LastRenderMode == kDrawIndexedQuads || m_LastRenderMode == kDrawIndexedPoints || m_LastRenderMode == kDrawIndexedLines || actualIndices % 3 == 0 );
	m_LendedChunk = false;
	
	const bool indexed = (m_LastRenderMode != kDrawQuads) && (m_LastRenderMode != kDrawTriangleStrip);

	m_LastChunkVertices = actualVertices;
	m_LastChunkIndices = actualIndices;

	// unlock buffers
	m_VB->Unlock();
	if( indexed )
		m_IB->Unlock();

	if( !actualVertices || (indexed && !actualIndices) ) {
		m_LastChunkShaderChannelMask = 0;
		return;
	}

	UInt32 actualVBSize = actualVertices * m_LastChunkStride;
	m_VBUsedBytes += actualVBSize;
	UInt32 actualIBSize = actualIndices * kVBOIndexSize;
	m_IBUsedBytes += actualIBSize;
}


