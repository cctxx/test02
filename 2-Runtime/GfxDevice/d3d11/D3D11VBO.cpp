#include "UnityPrefix.h"
#include "D3D11VBO.h"
#include "D3D11Context.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/TriStripper.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "D3D11Utils.h"



// defined in GfxDeviceD3D11.cpp
ID3D11InputLayout* GetD3D11VertexDeclaration (const ChannelInfoArray& channels);
void UpdateChannelBindingsD3D11 (const ChannelAssigns& channels);

ID3D11InputLayout* g_ActiveInputLayoutD3D11;
D3D11_PRIMITIVE_TOPOLOGY g_ActiveTopologyD3D11 = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;



static const D3D11_PRIMITIVE_TOPOLOGY kTopologyD3D11[kPrimitiveTypeCount] =
{
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
};

static const D3D11_PRIMITIVE_TOPOLOGY kTopologyD3D11Tess[kPrimitiveTypeCount] =
{
	D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,
	D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
	D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
	D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,
	D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
	D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
};


void SetInputLayoutD3D11 (ID3D11DeviceContext* ctx, ID3D11InputLayout* layout)
{
	if (g_ActiveInputLayoutD3D11 != layout)
	{
		g_ActiveInputLayoutD3D11 = layout;
		D3D11_CALL (ctx->IASetInputLayout (layout));
	}
}


static ID3D11InputLayout* GetD3D11VertexDeclaration (UInt32 shaderChannelsMap)
{
	ChannelInfoArray channels;
	int offset = 0;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		ChannelInfo& info = channels[i];
		if (shaderChannelsMap & (1 << i))
		{
			info.stream = 0;
			info.offset = offset;
			info.format = VBO::GetDefaultChannelFormat( i );
			info.dimension = VBO::GetDefaultChannelDimension( i );
			offset += VBO::GetDefaultChannelByteSize( i );
		}
		else
			info.Reset();
	}
	return GetD3D11VertexDeclaration (channels);
}


// -----------------------------------------------------------------------------

ID3D11Buffer* D3D11VBO::ms_CustomIB = NULL;
int	D3D11VBO::ms_CustomIBSize = 0;
UInt32 D3D11VBO::ms_CustomIBUsedBytes = 0;

ID3D11Buffer* D3D11VBO::ms_AllWhiteBuffer = NULL;


D3D11VBO::D3D11VBO()
:	m_IB(NULL)
,	m_StagingIB(NULL)
,	m_IBReadable(NULL)
,	m_IBSize(0)
,	m_useForSO(false)
{
	memset(m_VBStreams, 0, sizeof(m_VBStreams));
	memset(m_StagingVB, 0, sizeof(m_StagingVB));
}

D3D11VBO::~D3D11VBO ()
{
	for (int s = 0; s < kMaxVertexStreams; ++s)
	{
		//std::string tmp = GetDebugNameD3D11(m_StagingVB[s]);
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[s]);
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingVB[s]);
		SAFE_RELEASE(m_VBStreams[s]);
		SAFE_RELEASE(m_StagingVB[s]);
	}
	if (m_IB)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
		SAFE_RELEASE(m_IB);
	}
	if (m_StagingIB)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingIB);
		SAFE_RELEASE(m_StagingIB);
	}
	delete[] m_IBReadable;
}


static ID3D11Buffer* CreateStagingBuffer (int size)
{
	ID3D11Device* dev = GetD3D11Device();
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	ID3D11Buffer* buffer = NULL;
	HRESULT hr = dev->CreateBuffer (&desc, NULL, &buffer);
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(buffer,size,NULL);
	AssertIf (FAILED(hr));
	return buffer;
}

void D3D11VBO::CleanupSharedBuffers()
{
	REGISTER_EXTERNAL_GFX_DEALLOCATION(ms_CustomIB);
	REGISTER_EXTERNAL_GFX_DEALLOCATION(ms_AllWhiteBuffer);
	SAFE_RELEASE (ms_CustomIB);
	SAFE_RELEASE (ms_AllWhiteBuffer);
	ms_CustomIBSize = 0;
	ms_CustomIBUsedBytes = 0;
}


void D3D11VBO::UpdateVertexStream (const VertexBufferData& sourceData, unsigned stream)
{
	DebugAssert (!m_IsStreamMapped[stream]);
	const StreamInfo& srcStream = sourceData.streams[stream];
	const int oldSize = CalculateVertexStreamSize(m_Streams[stream], m_VertexCount);

#if UNITY_METRO
			#pragma message("Fix ugly hack CreateStagingBuffer")
			// So honestly I don't how what's happening here, but when running on ARM (Surface) with Feature Level 9.1 and if we create a vertex buffer with size 144
			// Sometimes we crash in D3D11VBO dtor in this line SAFE_RELEASE(m_StagingVB[s]);
			// Sometimes we get an access violation but sometimes it's Data misalignment exception like below
			// First-chance exception at 0x75499B2A (setupapi.dll) in Drift Mania Championship 2.exe: 0x80000002: Datatype misalignment
			// Repro case 495782
			// Not reproducible on Win32 running Feature Level 9.1

			// In any case it seems increasing min size up to 256, solves this issue for now
			int newSize = CalculateVertexStreamSize(srcStream, sourceData.vertexCount);
			int addon = 1;
			while (newSize > 0 && newSize < 256)
			{
				 newSize = CalculateVertexStreamSize(srcStream, sourceData.vertexCount + addon);
				 addon++;
			}

#else
	const int newSize = CalculateVertexStreamSize(srcStream, sourceData.vertexCount);
#endif


	m_Streams[stream] = srcStream;
	if (newSize == 0)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[stream]);
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingVB[stream]);
		SAFE_RELEASE (m_VBStreams[stream]);
		SAFE_RELEASE (m_StagingVB[stream]);
		return;
	}

	const bool isDynamic = (m_StreamModes[stream] == kStreamModeDynamic);
	const bool useStaging = !isDynamic;

	if (m_VBStreams[stream] == NULL || newSize != oldSize)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VBStreams[stream]);
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingVB[stream]);
		SAFE_RELEASE (m_VBStreams[stream]);
		SAFE_RELEASE (m_StagingVB[stream]); // release staging VB as well here

		ID3D11Device* dev = GetD3D11Device();
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = newSize;
		desc.Usage = isDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		if ( m_useForSO )
			desc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;
		desc.CPUAccessFlags = isDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		HRESULT hr = dev->CreateBuffer (&desc, NULL, &m_VBStreams[stream]);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_VBStreams[stream],newSize,this);
		if (FAILED(hr))
		{
			printf_console ("d3d11: failed to create vertex buffer of size %d [0x%X]\n", newSize, hr);
			return;
		}
		SetDebugNameD3D11 (m_VBStreams[stream], Format("VertexBuffer-%d", newSize));

		if (useStaging)
		{
			m_StagingVB[stream] = CreateStagingBuffer (newSize);
			SetDebugNameD3D11 (m_StagingVB[stream], Format("StagingVertexBuffer-%d", newSize));
		}
	}

	// Don't update contents if there is no source data.
	// This is used to update the vertex declaration only, leaving buffer intact.
	// Also to create an empty buffer that is written to later.
	if (!sourceData.buffer)
		return;

	HRESULT hr;

	ID3D11Buffer* mapVB = NULL;
	D3D11_MAP mapType;

	if (useStaging)
	{
		mapVB = m_StagingVB[stream];
		mapType = D3D11_MAP_WRITE;
	}
	else
	{
		mapVB = m_VBStreams[stream];
		mapType = D3D11_MAP_WRITE_DISCARD;
	}

	Assert (mapVB);

	ID3D11DeviceContext* ctx = GetD3D11Context();

	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map (mapVB, 0, mapType, 0, &mapped);
	Assert (SUCCEEDED(hr));
	CopyVertexStream (sourceData, reinterpret_cast<UInt8*>(mapped.pData), stream);
	ctx->Unmap (mapVB, 0);

	if (useStaging)
		ctx->CopyResource (m_VBStreams[stream], m_StagingVB[stream]);
}


void D3D11VBO::UpdateIndexBufferData (const IndexBufferData& sourceData)
{
	if( !sourceData.indices )
	{
		m_IBSize = 0;
		return;
	}

	Assert (m_IB);
	HRESULT hr;

	int size = sourceData.count * kVBOIndexSize;
	if (sourceData.hasTopologies & ((1<<kPrimitiveTriangleStripDeprecated) | (1<<kPrimitiveQuads)))
	{
		delete[] m_IBReadable;
		m_IBReadable = new UInt16[sourceData.count];
		memcpy (m_IBReadable, sourceData.indices, size);
	}

	const D3D11_MAP mapType = m_IndicesDynamic ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
	ID3D11Buffer* mapIB;
	if (m_IndicesDynamic)
		mapIB = m_IB;
	else
	{
		if (!m_StagingIB)
			m_StagingIB = CreateStagingBuffer(m_IBSize);
		mapIB = m_StagingIB;
	}

	ID3D11DeviceContext* ctx = GetD3D11Context();
	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = ctx->Map (mapIB, 0, mapType, 0, &mapped);
	Assert (SUCCEEDED(hr));

	memcpy (mapped.pData, sourceData.indices, size);

	ctx->Unmap (mapIB, 0);
	if (!m_IndicesDynamic)
		ctx->CopyResource (m_IB, m_StagingIB);
}

bool D3D11VBO::MapVertexStream( VertexStreamData& outData, unsigned stream )
{
	if (m_VBStreams[stream] == NULL)
	{
		printf_console ("d3d11: attempt to map null vertex buffer\n");
		return false;
	}
	DebugAssert(!IsVertexBufferLost());
	Assert(!m_IsStreamMapped[stream]);

	const int vbSize = CalculateVertexStreamSize(m_Streams[stream], m_VertexCount);

	const bool dynamic = (m_StreamModes[stream]==kStreamModeDynamic);

	D3D11_MAPPED_SUBRESOURCE mapped;
	ID3D11Buffer* mapVB = dynamic ? m_VBStreams[stream] : m_StagingVB[stream];
	D3D11_MAP mapType = dynamic ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE;
	HRESULT hr = GetD3D11Context()->Map (mapVB, 0, mapType, 0, &mapped);
	if( FAILED(hr) )
	{
		printf_console ("d3d11: failed to map vertex buffer %p of size %i [%x]\n", mapVB, vbSize, hr);
		return false;
	}
	m_IsStreamMapped[stream] = true;

	UInt8* buffer = (UInt8*)mapped.pData;

	outData.buffer = buffer;
	outData.channelMask = m_Streams[stream].channelMask;
	outData.stride = m_Streams[stream].stride;
	outData.vertexCount = m_VertexCount;

	GetRealGfxDevice().GetFrameStats().AddUploadVBO(vbSize);

	return true;
}

void D3D11VBO::UnmapVertexStream (unsigned stream)
{
	DebugAssert(m_VBStreams[stream]);
	Assert(m_IsStreamMapped[stream]);
	m_IsStreamMapped[stream] = false;
	ID3D11DeviceContext* ctx = GetD3D11Context();

	const bool dynamic = (m_StreamModes[stream]==kStreamModeDynamic);
	ID3D11Buffer* mapVB = dynamic ? m_VBStreams[stream] : m_StagingVB[stream];
	ctx->Unmap (mapVB, 0);

	if (!dynamic)
		ctx->CopyResource (m_VBStreams[stream], m_StagingVB[stream]);
}

bool D3D11VBO::IsVertexBufferLost() const
{
	for (int s = 0; s < kMaxVertexStreams; ++s)
		if (m_Streams[s].channelMask && !m_VBStreams[s])
			return true;
	return false;
}

int D3D11VBO::GetRuntimeMemorySize() const
{
	int vertexSize = 0;
	for( int s = 0; s < kMaxVertexStreams; s++ )
		vertexSize += m_Streams[s].stride;
	return vertexSize * m_VertexCount + m_IBSize;
}


bool SetTopologyD3D11 (GfxPrimitiveType topology, GfxDevice& device, ID3D11DeviceContext* ctx)
{
	bool tessellation = device.IsShaderActive (kShaderHull) || device.IsShaderActive (kShaderDomain);
	D3D11_PRIMITIVE_TOPOLOGY topod3d = tessellation ? kTopologyD3D11Tess[topology] : kTopologyD3D11[topology];
	if (topod3d == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return false;

	if (topod3d != g_ActiveTopologyD3D11)
	{
		g_ActiveTopologyD3D11 = topod3d;
		D3D11_CALL (ctx->IASetPrimitiveTopology (topod3d));
	}
	return true;
}


ID3D11Buffer* D3D11VBO::GetAllWhiteBuffer()
{
	if (!ms_AllWhiteBuffer)
	{
		int maxVerts = 0x10000;
		int size = maxVerts * sizeof(UInt32);
		UInt8* data = new UInt8[size];
		memset (data, 0xFF, size);

		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA sdata;
		sdata.pSysMem = data;
		sdata.SysMemPitch = 0;
		sdata.SysMemSlicePitch = 0;
		HRESULT hr = GetD3D11Device()->CreateBuffer (&desc, &sdata, &ms_AllWhiteBuffer);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(ms_AllWhiteBuffer,size,NULL);
		delete[] data;
	}
	return ms_AllWhiteBuffer;
}


void D3D11VBO::BindVertexStreams(GfxDevice& device, ID3D11DeviceContext* ctx, const ChannelAssigns& channels)
{
	DX11_LOG_ENTER_FUNCTION("D3D11VBO::BindVertexStreams");
	int freeStream = -1;
	for (int s = 0; s < kMaxVertexStreams; ++s)
	{
		if (m_VBStreams[s])
		{
			UINT offset = 0;
			UINT stride = m_Streams[s].stride;
			D3D11_CALL (ctx->IASetVertexBuffers(s, 1, &m_VBStreams[s], &stride, &offset));
		}
		else
			freeStream = s;
	}

	UpdateChannelBindingsD3D11(channels);

	device.BeforeDrawCall( false );

	const ChannelInfoArray* channelInfo = &m_ChannelInfo;
	if ((channels.GetSourceMap() & VERTEX_FORMAT1(Color)) && !m_ChannelInfo[kShaderChannelColor].IsValid())
	{
		if (freeStream != -1)
		{
			static ChannelInfoArray colorChannelInfo;
			memcpy(&colorChannelInfo, m_ChannelInfo, sizeof(colorChannelInfo));
			ChannelInfo& colorInfo = colorChannelInfo[kShaderChannelColor];
			colorInfo.stream = freeStream;
			colorInfo.offset = 0;
			colorInfo.format = kChannelFormatColor;
			colorInfo.dimension = 1;
			channelInfo = &colorChannelInfo;
			ID3D11Buffer* whiteVB = GetAllWhiteBuffer();
			UINT stride = 4;
			UINT offset = 0;
			D3D11_CALL (ctx->IASetVertexBuffers(freeStream, 1, &whiteVB, &stride, &offset));
		}
		else
			ErrorString("Need a free stream to add default vertex colors!");
	}
	ID3D11InputLayout* inputLayout = GetD3D11VertexDeclaration(m_ChannelInfo);
	SetInputLayoutD3D11 (ctx, inputLayout);
}

void D3D11VBO::BindToStreamOutput()
{
	const UINT offsets[] = { 0 };
	GetD3D11Context()->SOSetTargets(1, m_VBStreams, offsets);
}

void D3D11VBO::UnbindFromStreamOutput()
{
	ID3D11Buffer* const buffers[] = { 0 };
	const UINT offsets[] = { 0 };
	GetD3D11Context()->SOSetTargets(1, buffers, offsets);
}


void D3D11VBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount)
{
	DX11_LOG_ENTER_FUNCTION("D3D11VBO::DrawVBO");
	// just return if no indices
	if( m_IBSize == 0 )
		return;

	Assert(!m_IsStreamMapped[0]);
	if (m_VBStreams[0] == 0 || m_IB == 0)
	{
		printf_console( "d3d: VB or IB is null\n" );
		return;
	}

	GfxDevice& device = GetRealGfxDevice();
	ID3D11DeviceContext* ctx = GetD3D11Context();
	BindVertexStreams (device, ctx, channels);

	bool tessellation = device.IsShaderActive (kShaderHull) || device.IsShaderActive (kShaderDomain);
	if (tessellation && topology == kPrimitiveTriangleStripDeprecated)
	{
		if (!m_IBReadable)
			return;

		const UInt16* ibSrc = (const UInt16*)((const UInt8*)m_IBReadable + firstIndexByte);
		const int triCount = CountTrianglesInStrip (ibSrc, indexCount);

		UInt32 ibBytesLocked;
		UInt16* ibPtr = MapDynamicIndexBuffer (triCount*3, ibBytesLocked);
		if (!ibPtr)
			return;
		Destripify (ibSrc, indexCount, ibPtr, triCount*3);
		UnmapDynamicIndexBuffer ();
		firstIndexByte = ms_CustomIBUsedBytes;
		ms_CustomIBUsedBytes += ibBytesLocked;
		D3D11_CALL (ctx->IASetIndexBuffer (ms_CustomIB, DXGI_FORMAT_R16_UINT, 0));
		indexCount = ibBytesLocked/kVBOIndexSize;
		topology = kPrimitiveTriangles;
	}
	else if (topology == kPrimitiveQuads && !tessellation)
	{
		if (!m_IBReadable)
			return;
		UInt32 ibBytesLocked;
		UInt16* ibPtr = MapDynamicIndexBuffer (indexCount/4*6, ibBytesLocked);
		if (!ibPtr)
			return;
		const UInt16* ibSrc = (const UInt16*)((const UInt8*)m_IBReadable + firstIndexByte);
		FillIndexBufferForQuads (ibPtr, ibBytesLocked, ibSrc, indexCount/4);
		UnmapDynamicIndexBuffer ();
		firstIndexByte = ms_CustomIBUsedBytes;
		ms_CustomIBUsedBytes += ibBytesLocked;
		D3D11_CALL (ctx->IASetIndexBuffer (ms_CustomIB, DXGI_FORMAT_R16_UINT, 0));
		indexCount = ibBytesLocked/kVBOIndexSize;
	}
	else
	{
		D3D11_CALL (ctx->IASetIndexBuffer (m_IB, DXGI_FORMAT_R16_UINT, 0));
	}

	// draw
	if (!SetTopologyD3D11 (topology, device, ctx))
		return;
	D3D11_CALL (ctx->DrawIndexed (indexCount, firstIndexByte/2, 0));
	device.GetFrameStats().AddDrawCall (GetPrimitiveCount(indexCount,topology,false), vertexCount);
	DX11_MARK_DRAWING(GetPrimitiveCount(indexCount,topology,false), vertexCount);

}

UInt16* D3D11VBO::MapDynamicIndexBuffer (int indexCount, UInt32& outBytesUsed)
{
	HRESULT hr;
	const UInt32 maxIndices = 64000;
	Assert (indexCount <= maxIndices);
	indexCount = std::min<UInt32>(indexCount, maxIndices);

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

		ID3D11Device* dev = GetD3D11Device();

		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = newIBSize;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		hr = dev->CreateBuffer (&desc, NULL, &ms_CustomIB);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(ms_CustomIB,newIBSize,NULL);
		if (FAILED(hr))
		{
			printf_console ("d3d11: failed to create custom index buffer of size %d [%x]\n", newIBSize, hr);
			return NULL;
		}
		SetDebugNameD3D11 (ms_CustomIB, Format("IndexBufferCustomDynamic-%d", newIBSize));
	}

	ID3D11DeviceContext* ctx = GetD3D11Context();
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (ms_CustomIBUsedBytes + ibCapacity > ms_CustomIBSize)
	{
		hr = ctx->Map (ms_CustomIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr))
		{
			printf_console ("d3d11: failed to lock shared index buffer with discard [%x]\n", hr);
			return NULL;
		}
		ms_CustomIBUsedBytes = 0;
	} 
	else 
	{
		hr = ctx->Map (ms_CustomIB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
		if (FAILED(hr))
		{
			printf_console( "d3d11: failed to lock shared index buffer, offset %i size %i [%x]\n", ms_CustomIBUsedBytes, ibCapacity, hr);
			return NULL;
		}
	}
	outBytesUsed = ibCapacity;

	return (UInt16*)((UInt8*)mapped.pData + ms_CustomIBUsedBytes);
}

void D3D11VBO::UnmapDynamicIndexBuffer ()
{
	GetD3D11Context()->Unmap (ms_CustomIB, 0);
}


#if GFX_ENABLE_DRAW_CALL_BATCHING
void D3D11VBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								 GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
{
	HRESULT hr;
	Assert (!m_IsStreamMapped[0]);

	if (m_VBStreams[0] == 0)
	{
		printf_console( "d3d11: VB is null\n" );
		return;
	}

	ID3D11DeviceContext* ctx = GetD3D11Context();

	UInt32 ibBytesUsed;
	UInt16* ibPtr = MapDynamicIndexBuffer (indexCount, ibBytesUsed);
	if (!ibPtr)
		return;
	memcpy (ibPtr, indices, ibBytesUsed);
	UnmapDynamicIndexBuffer ();

	// draw
	GfxDevice& device = GetRealGfxDevice();
	BindVertexStreams(device, ctx, channels);

	D3D11_CALL (ctx->IASetIndexBuffer (ms_CustomIB, DXGI_FORMAT_R16_UINT, 0));

	// draw
	if (!SetTopologyD3D11 (topology, device, ctx))
		return;
	D3D11_CALL (ctx->DrawIndexed (indexCount, ms_CustomIBUsedBytes / kVBOIndexSize, 0));
	device.GetFrameStats().AddDrawCall (GetPrimitiveCount(indexCount,topology,false), drawVertexCount);

	ms_CustomIBUsedBytes += ibBytesUsed;
}

#endif // GFX_ENABLE_DRAW_CALL_BATCHING



void D3D11VBO::UpdateVertexData( const VertexBufferData& buffer )
{
	for (unsigned stream = 0; stream < kMaxVertexStreams; stream++)
		UpdateVertexStream (buffer, stream);

	memcpy (m_ChannelInfo, buffer.channels, sizeof(m_ChannelInfo));
	m_VertexCount = buffer.vertexCount;
}

void D3D11VBO::UpdateIndexData (const IndexBufferData& buffer)
{
	int newSize = CalculateIndexBufferSize(buffer);

	// If we have old buffer, but need different size: delete old one
	if (newSize != m_IBSize)
	{
		if (m_IB)
		{
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
			SAFE_RELEASE(m_IB);
		}
		if (m_StagingIB)
		{
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingIB);
			SAFE_RELEASE(m_StagingIB);
		}
	}

	// Create buffer if we need to
	if (!m_IB)
	{
		ID3D11Device* dev = GetD3D11Device();
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = newSize;
		desc.Usage = m_IndicesDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = m_IndicesDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		HRESULT hr = dev->CreateBuffer (&desc, NULL, &m_IB);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_IB,newSize,this);
		if( FAILED(hr) )
		{
			printf_console( "d3d11: failed to create index buffer of size %d [0x%X]\n", newSize, hr );
			return;
		}
		SetDebugNameD3D11 (m_IB, Format("IndexBuffer-%d", newSize));
	}

	m_IBSize = newSize;
	UpdateIndexBufferData(buffer);
}

void D3D11VBO::SetIndicesDynamic(bool dynamic)
{
	// do nothing if a no-op
	if (dynamic == m_IndicesDynamic)
		return;

	VBO::SetIndicesDynamic(dynamic);

	// release current index buffers
	if (m_IB)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
		SAFE_RELEASE(m_IB);
	}
	if (m_StagingIB)
	{
		REGISTER_EXTERNAL_GFX_DEALLOCATION(m_StagingIB);
		SAFE_RELEASE(m_StagingIB);
	}
}



// -----------------------------------------------------------------------------


DynamicD3D11VBO::DynamicD3D11VBO( UInt32 vbSize, UInt32 ibSize )
:	DynamicVBO()
,	m_VBSize(vbSize)
,	m_VBUsedBytes(0)
,	m_IBSize(ibSize)
,	m_IBUsedBytes(0)
,	m_VB(NULL)
,	m_IB(NULL)
,	m_LastChunkStartVertex(0)
,	m_LastChunkStartIndex(0)
,	m_QuadsIB(NULL)
{
}

DynamicD3D11VBO::~DynamicD3D11VBO ()
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

void DynamicD3D11VBO::InitializeQuadsIB()
{
	AssertIf( m_QuadsIB );

	const int kMaxQuads = 65536/4 - 4; // so we fit into 16 bit indices, minus some more just in case

	UInt16* data = new UInt16[kMaxQuads*6];
	UInt16* ib = data;
	UInt32 baseIndex = 0;
	for( int i = 0; i < kMaxQuads; ++i )
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

	ID3D11Device* dev = GetD3D11Device();
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = kMaxQuads * 6 * kVBOIndexSize;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA srData;
	srData.pSysMem = data;
	srData.SysMemPitch = 0;
	srData.SysMemSlicePitch = 0;
	HRESULT hr = dev->CreateBuffer (&desc, &srData, &m_QuadsIB);
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_QuadsIB,desc.ByteWidth,this);
	delete[] data;
	if (FAILED(hr))
	{
		printf_console ("d3d11: failed to create quads index buffer [%x]\n", hr);
	}
	SetDebugNameD3D11 (m_QuadsIB, "IndexBufferQuads");
}


void DynamicD3D11VBO::DrawChunk (const ChannelAssigns& channels)
{
	DX11_LOG_ENTER_FUNCTION("DynamicD3D11VBO::DrawChunk");
	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;

	HRESULT hr;

	AssertIf( !m_LastChunkShaderChannelMask || !m_LastChunkStride );
	AssertIf( m_LendedChunk );

	GfxDevice& device = GetRealGfxDevice();
	ID3D11DeviceContext* ctx = GetD3D11Context();

	// setup VBO
	DebugAssert (m_VB);
	UINT strides = m_LastChunkStride;
	UINT offsets = 0;
	D3D11_CALL (ctx->IASetVertexBuffers(0, 1, &m_VB, &strides, &offsets));

	UpdateChannelBindingsD3D11(channels);
	device.BeforeDrawCall (false);

	ID3D11InputLayout* inputLayout = GetD3D11VertexDeclaration (m_LastChunkShaderChannelMask);
	SetInputLayoutD3D11 (ctx, inputLayout);

	// draw
	GfxDeviceStats& stats = device.GetFrameStats();
	int primCount = 0;
	if (m_LastRenderMode == kDrawTriangleStrip)
	{
		if (!SetTopologyD3D11(kPrimitiveTriangleStripDeprecated,device,ctx))
			return;
		D3D11_CALL (ctx->Draw (m_LastChunkVertices, m_LastChunkStartVertex));
		primCount = m_LastChunkVertices-2;
	}
	else if (m_LastRenderMode == kDrawIndexedTriangleStrip)
	{
		DebugAssert (m_IB);
		if (!SetTopologyD3D11(kPrimitiveTriangleStripDeprecated,device,ctx))
			return;
		D3D11_CALL (ctx->IASetIndexBuffer (m_IB, DXGI_FORMAT_R16_UINT, 0));
		D3D11_CALL (ctx->DrawIndexed (m_LastChunkIndices, m_LastChunkStartIndex, m_LastChunkStartVertex));
		primCount = m_LastChunkIndices-2;
	}	
	else if( m_LastRenderMode == kDrawQuads )
	{
		if (!SetTopologyD3D11(kPrimitiveTriangles,device,ctx))
			return;
		// initialize quads index buffer if needed
		if (!m_QuadsIB)
			InitializeQuadsIB();
		// if quads index buffer has valid data, draw with it
		if (m_QuadsIB)
		{
			D3D11_CALL (ctx->IASetIndexBuffer (m_QuadsIB, DXGI_FORMAT_R16_UINT, 0));
			D3D11_CALL (ctx->DrawIndexed (m_LastChunkVertices/4*6, 0, m_LastChunkStartVertex));
			primCount = m_LastChunkVertices/2;
		}
	}
	else if (m_LastRenderMode == kDrawIndexedLines)
	{
		DebugAssert( m_IB );
		if (!SetTopologyD3D11(kPrimitiveLines,device,ctx))
			return;
		D3D11_CALL (ctx->IASetIndexBuffer (m_IB, DXGI_FORMAT_R16_UINT, 0));
		D3D11_CALL (ctx->DrawIndexed (m_LastChunkIndices, m_LastChunkStartIndex, m_LastChunkStartVertex));
		primCount = m_LastChunkIndices/2;
	}
	else if (m_LastRenderMode == kDrawIndexedPoints)
	{
		DebugAssert (m_IB);
		D3D11_CALL (ctx->IASetIndexBuffer (m_IB, DXGI_FORMAT_R16_UINT, 0));
		if (!SetTopologyD3D11 (kPrimitivePoints, device, ctx))
			return;
		D3D11_CALL (ctx->DrawIndexed (m_LastChunkIndices, m_LastChunkStartIndex, m_LastChunkStartVertex));
		primCount = m_LastChunkIndices;
	}
	else
	{
		DebugAssert (m_IB);
		D3D11_CALL (ctx->IASetIndexBuffer (m_IB, DXGI_FORMAT_R16_UINT, 0));
		if (!SetTopologyD3D11 (kPrimitiveTriangles, device, ctx))
			return;
		D3D11_CALL (ctx->DrawIndexed (m_LastChunkIndices, m_LastChunkStartIndex, m_LastChunkStartVertex));
		primCount = m_LastChunkIndices/3;
	}
	stats.AddDrawCall (primCount, m_LastChunkVertices);
	DX11_MARK_DRAWING(primCount, m_LastChunkVertices);
}


#if UNITY_EDITOR
void DynamicD3D11VBO::DrawChunkUserPrimitives (GfxPrimitiveType type)
{
	// just return if nothing to render
	if( !m_LastChunkShaderChannelMask )
		return;

	HRESULT hr;

	AssertIf( !m_LastChunkShaderChannelMask || !m_LastChunkStride );
	AssertIf( m_LendedChunk );

	ChannelAssigns channels;
	for( int i = 0; i < kShaderChannelCount; ++i )
	{
		if (!(m_LastChunkShaderChannelMask & (1<<i)))
			continue;
		VertexComponent destComponent = kSuitableVertexComponentForChannel[i];
		channels.Bind ((ShaderChannel)i, destComponent);
	}

	GfxDevice& device = GetRealGfxDevice();
	ID3D11DeviceContext* ctx = GetD3D11Context();

	// setup VBO
	DebugAssert (m_VB);
	UINT strides = m_LastChunkStride;
	UINT offsets = 0;
	D3D11_CALL (ctx->IASetVertexBuffers(0, 1, &m_VB, &strides, &offsets));

	UpdateChannelBindingsD3D11(channels);
	device.BeforeDrawCall (false);

	ID3D11InputLayout* inputLayout = GetD3D11VertexDeclaration (m_LastChunkShaderChannelMask);
	SetInputLayoutD3D11 (ctx, inputLayout);

	// draw
	GfxDeviceStats& stats = device.GetFrameStats();
	int primCount = 0;
	switch (type)
	{
	case kPrimitiveTriangles:
		if (!SetTopologyD3D11(kPrimitiveTriangles,device,ctx))
			return;
		D3D11_CALL (ctx->Draw (m_LastChunkVertices, m_LastChunkStartVertex));
		primCount = m_LastChunkVertices/3;
		break;
	case kPrimitiveQuads:
		if (!SetTopologyD3D11(kPrimitiveTriangles,device,ctx))
			return;
		// initialize quads index buffer if needed
		if (!m_QuadsIB)
			InitializeQuadsIB();
		// if quads index buffer has valid data, draw with it
		if (m_QuadsIB)
		{
			D3D11_CALL (ctx->IASetIndexBuffer (m_QuadsIB, DXGI_FORMAT_R16_UINT, 0));
			D3D11_CALL (ctx->DrawIndexed (m_LastChunkVertices/4*6, 0, m_LastChunkStartVertex));
			primCount = m_LastChunkVertices/2;
		}
		break;
	case kPrimitiveLines:
		if (!SetTopologyD3D11(kPrimitiveLines,device,ctx))
			return;
		D3D11_CALL (ctx->Draw (m_LastChunkVertices, m_LastChunkStartVertex));
		primCount = m_LastChunkVertices/2;
		break;
	case kPrimitiveLineStrip:
		if (!SetTopologyD3D11(kPrimitiveLineStrip,device,ctx))
			return;
		D3D11_CALL (ctx->Draw (m_LastChunkVertices, m_LastChunkStartVertex));
		primCount = m_LastChunkVertices-1;
		break;
	default:
		ErrorString("Primitive type not supported");
		return;
	}
	stats.AddDrawCall (primCount, m_LastChunkVertices);
}
#endif // UNITY_EDITOR


bool DynamicD3D11VBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	Assert( !m_LendedChunk );
	Assert( maxVertices < 65536 && maxIndices < 65536*3 );
	DebugAssertMsg(outVB != NULL && maxVertices > 0, "DynamicD3D11VBO::GetChunk - outVB: 0x%08x maxVertices: %d", outVB, maxVertices);
	DebugAssertMsg(
		(renderMode == kDrawIndexedQuads			&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawIndexedPoints			&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawIndexedLines			&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawIndexedTriangles		&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawIndexedTriangleStrip	&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawTriangleStrip			&& (outIB == NULL && maxIndices == 0)) ||
		(renderMode == kDrawQuads					&& (outIB == NULL && maxIndices == 0)),
		"DynamicD3D11VBO::GetChunk - renderMode: %d outIB: 0x%08x maxIndices: %d", renderMode, outIB, maxIndices);

	HRESULT hr;
	bool success = true;

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
	ID3D11Device* dev = GetD3D11Device();
	ID3D11DeviceContext* ctx = GetD3D11Context();

	// -------- vertex buffer

	DebugAssertIf( !outVB );
	UInt32 vbCapacity = maxVertices * m_LastChunkStride;
	// check if requested chunk is larger than current buffer
	if( vbCapacity > m_VBSize ) {
		m_VBSize = vbCapacity * 2; // allocate more up front
		if( m_VB )
		{
			REGISTER_EXTERNAL_GFX_DEALLOCATION(m_VB);
			m_VB->Release();
		}
		m_VB = NULL;
	}
	// allocate buffer if don't have it yet
	if( !m_VB )
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = m_VBSize;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		hr = dev->CreateBuffer (&desc, NULL, &m_VB);
		REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_VB,m_VBSize,this);
		if (FAILED(hr))
		{
			printf_console ("d3d11: failed to create dynamic vertex buffer of size %d [%x]\n", m_VBSize, hr);
			success = false;
			*outVB = NULL;
		}
		SetDebugNameD3D11 (m_VB, Format("VertexBufferDynamic-%d", m_VBSize));
	}

	// map, making sure the offset we lock is multiple of vertex stride
	if (m_VB)
	{
		m_VBUsedBytes = ((m_VBUsedBytes + (m_LastChunkStride-1)) / m_LastChunkStride) * m_LastChunkStride;
		if (m_VBUsedBytes + vbCapacity > m_VBSize)
		{
			D3D11_MAPPED_SUBRESOURCE mapped;
			hr = ctx->Map (m_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			if (FAILED(hr))
			{
				printf_console ("d3d11: failed to lock dynamic vertex buffer with discard [%x]\n", hr);
				*outVB = NULL;
				success = false;
			}
			*outVB = mapped.pData;
			m_VBUsedBytes = 0;
		} else {
			D3D11_MAPPED_SUBRESOURCE mapped;
			hr = ctx->Map (m_VB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
			if (FAILED(hr))
			{
				printf_console ("d3d11: failed to lock vertex index buffer, offset %i size %i [%x]\n", m_VBUsedBytes, vbCapacity, hr);
				*outVB = NULL;
				success = false;
			}
			*outVB = ((UInt8*)mapped.pData) + m_VBUsedBytes;
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
			if( m_IB )
			{
				REGISTER_EXTERNAL_GFX_DEALLOCATION(m_IB);
				m_IB->Release();
			}
			m_IB = NULL;
		}
		// allocate buffer if don't have it yet
		if( !m_IB )
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = m_IBSize;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;
			hr = dev->CreateBuffer (&desc, NULL, &m_IB);
			REGISTER_EXTERNAL_GFX_ALLOCATION_REF(m_IB,m_IBSize,this);
			if (FAILED(hr))
			{
				printf_console ("d3d11: failed to create dynamic index buffer of size %d [%x]\n", m_IBSize, hr);
				if (m_VB)
					ctx->Unmap (m_VB, 0);
			}
			SetDebugNameD3D11 (m_IB, Format("IndexBufferDynamic-%d", m_IBSize));
		}
		// lock it if we have IB created successfully
		if( m_IB )
		{
			if( m_IBUsedBytes + ibCapacity > m_IBSize )
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				hr = ctx->Map (m_IB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if (FAILED(hr))
				{
					printf_console ("d3d11: failed to lock dynamic index buffer with discard [%x]\n", hr);
					*outIB = NULL;
					success = false;
					if (m_VB)
						ctx->Unmap (m_VB, 0);
				}
				*outIB = mapped.pData;
				m_IBUsedBytes = 0;
			} else {
				D3D11_MAPPED_SUBRESOURCE mapped;
				hr = ctx->Map (m_IB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
				if (FAILED(hr))
				{
					printf_console ("d3d11: failed to lock dynamic index buffer, offset %i size %i [%x]\n", m_IBUsedBytes, ibCapacity, hr);
					*outIB = NULL;
					success = false;
					if (m_VB)
						ctx->Unmap (m_VB, 0);
				}
				*outIB = ((UInt8*)mapped.pData) + m_IBUsedBytes;
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

void DynamicD3D11VBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	Assert( m_LendedChunk );
	Assert( m_LastRenderMode == kDrawIndexedTriangleStrip || m_LastRenderMode == kDrawIndexedQuads || m_LastRenderMode == kDrawIndexedPoints || m_LastRenderMode == kDrawIndexedLines || actualIndices % 3 == 0 );
	m_LendedChunk = false;

	const bool indexed = (m_LastRenderMode != kDrawQuads) && (m_LastRenderMode != kDrawTriangleStrip);
	
	m_LastChunkVertices = actualVertices;
	m_LastChunkIndices = actualIndices;

	// unlock buffers
	ID3D11DeviceContext* ctx = GetD3D11Context();
	ctx->Unmap (m_VB, 0);
	if (indexed)
		ctx->Unmap (m_IB, 0);

	if( !actualVertices || (indexed && !actualIndices) ) {
		m_LastChunkShaderChannelMask = 0;
		return;
	}

	UInt32 actualVBSize = actualVertices * m_LastChunkStride;
	m_VBUsedBytes += actualVBSize;
	UInt32 actualIBSize = actualIndices * kVBOIndexSize;
	m_IBUsedBytes += actualIBSize;
}


