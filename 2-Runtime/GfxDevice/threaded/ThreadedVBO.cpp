#include "UnityPrefix.h"
#include "ThreadedVBO.h"
#include "Runtime/Threads/ThreadedStreamBuffer.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceClient.h"
#include "Runtime/GfxDevice/threaded/GfxDeviceWorker.h"
#include "Runtime/GfxDevice/threaded/GfxCommands.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"


ThreadedVBO::ThreadedVBO(GfxDeviceClient& device) :
	m_ClientDevice(device),
	m_ClientVBO(NULL),
	m_NonThreadedVBO(NULL),
	m_MappedFromRenderThread(false),
	m_VertexBufferLost(false),
	m_IndexBufferLost(false),
	m_VertexBufferSize(0),
	m_IndexBufferSize(0)
{
	SET_ALLOC_OWNER(NULL);
	DebugAssert(Thread::CurrentThreadIsMainThread());
	m_ClientVBO = UNITY_NEW(ClientDeviceVBO, kMemGfxThread);
	if (device.IsThreaded())
	{

		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_Constructor);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		m_ClientVBO->internalVBO = m_ClientDevice.m_VBOMapper.CreateID();
		GetCommandQueue().WriteValueType<ClientDeviceVBO>(*m_ClientVBO);
#else
		GetCommandQueue().WriteValueType<ClientDeviceVBO*>(m_ClientVBO);
#endif
		GetCommandQueue().WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	else
	{
		m_NonThreadedVBO = GetRealGfxDevice().CreateVBO();
		m_ClientVBO->internalVBO = m_NonThreadedVBO;
	}
#endif
}

ThreadedVBO::~ThreadedVBO()
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	DebugAssert(!m_ClientDevice.IsRecording());
	if (m_ClientDevice.IsThreaded())
	{
		DebugAssert(!m_NonThreadedVBO);
		// m_ClientVBO deleted by server
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_Destructor);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GetCommandQueue().WriteValueType<ClientDeviceVBO>(*m_ClientVBO);
#else
		GetCommandQueue().WriteValueType<ClientDeviceVBO*>(m_ClientVBO);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		GetRealGfxDevice().DeleteVBO(m_NonThreadedVBO);
		UNITY_DELETE(m_ClientVBO, kMemGfxThread);
	}
	m_NonThreadedVBO = NULL;
	m_ClientVBO = NULL;
}

void ThreadedVBO::UpdateVertexData( const VertexBufferData& buffer )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_UpdateVertexData);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GetCommandQueue().WriteValueType<ClientDeviceVBO>(*m_ClientVBO);
		ClientVertexBufferData client;
		memcpy (client.channels, buffer.channels, sizeof(ChannelInfoArray));
		memcpy (client.streams, buffer.streams, sizeof(StreamInfoArray));
		client.bufferSize = buffer.bufferSize;
		client.vertexCount = buffer.vertexCount;
		client.hasData = buffer.buffer != NULL;
		GetCommandQueue().WriteValueType<ClientVertexBufferData>(client);
#else
		GetCommandQueue().WriteValueType<ClientDeviceVBO*>(m_ClientVBO);
		GetCommandQueue().WriteValueType<VertexBufferData>(buffer);
#endif
		if (buffer.buffer)
			m_ClientDevice.WriteBufferData(buffer.buffer, buffer.bufferSize);
		if (m_MappedFromRenderThread)
			UnbufferVertexData();
		else
			BufferAccessibleVertexData(buffer);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->UpdateVertexData(buffer);
	}
	m_VertexBufferSize = buffer.bufferSize;
	m_VertexBufferLost = false;
}

void ThreadedVBO::UpdateIndexData (const IndexBufferData& buffer)
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_UpdateIndexData);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GetCommandQueue().WriteValueType<ClientDeviceVBO>(*m_ClientVBO);
#else
		GetCommandQueue().WriteValueType<ClientDeviceVBO*>(m_ClientVBO);
#endif
		GetCommandQueue().WriteValueType<int>(buffer.count);
		GetCommandQueue().WriteValueType<UInt32>(buffer.hasTopologies);
		m_ClientDevice.WriteBufferData(buffer.indices, CalculateIndexBufferSize(buffer));
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->UpdateIndexData(buffer);
	}
	m_IndexBufferSize = buffer.count * kVBOIndexSize;
	m_IndexBufferLost = false;
}

void ThreadedVBO::DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
				  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	m_ClientDevice.BeforeDrawCall(false);
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_Draw);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdVBODraw data = { *m_ClientVBO, channels, firstIndexByte, indexCount, topology, firstVertex, vertexCount };
#else
		GfxCmdVBODraw data = { m_ClientVBO, channels, firstIndexByte, indexCount, topology, firstVertex, vertexCount };
#endif
		GetCommandQueue().WriteValueType<GfxCmdVBODraw>(data);
		GetCommandQueue().WriteSubmitData();
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->DrawVBO (channels, firstIndexByte, indexCount, topology, firstVertex, vertexCount);
	}
}

#if GFX_ENABLE_DRAW_CALL_BATCHING
void ThreadedVBO::DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
							   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	m_ClientDevice.BeforeDrawCall(false);
	if (m_ClientDevice.IsSerializing())
	{
		//Note: Presuming that indices are of size UInt16!
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_DrawCustomIndexed);
		GfxCmdVBODrawCustomIndexed data = { m_ClientVBO, channels, indexCount, topology, vertexRangeBegin, vertexRangeEnd, drawVertexCount };
		GetCommandQueue().WriteValueType<GfxCmdVBODrawCustomIndexed>(data);
		GetCommandQueue().WriteStreamingData(indices, indexCount*kVBOIndexSize);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->DrawCustomIndexed(channels, indices, indexCount, topology,
			vertexRangeBegin, vertexRangeEnd, drawVertexCount);
	}
}
#endif

void ThreadedVBO::Recreate()
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_Recreate);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GetCommandQueue().WriteValueType<ClientDeviceVBO>(*m_ClientVBO);
#else
		GetCommandQueue().WriteValueType<ClientDeviceVBO*>(m_ClientVBO);
#endif
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->Recreate();
	}
}

bool ThreadedVBO::MapVertexStream( VertexStreamData& outData, unsigned stream )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		BufferedVBO::MapVertexStream(outData, stream);
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_IsStreamMapped[stream] = m_NonThreadedVBO->MapVertexStream(outData, stream);
	}
	return m_IsStreamMapped[stream];
}

void ThreadedVBO::UnmapVertexStream( unsigned stream )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		BufferedVBO::UnmapVertexStream(stream);

		// Send modified vertices to render thread
		size_t size = CalculateVertexStreamSize(m_VertexData, stream);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdVBOMapVertexStream map = { *m_ClientVBO, stream, size };
#else
		GfxCmdVBOMapVertexStream map = { m_ClientVBO, stream, size };
#endif
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_MapVertexStream);
		GetCommandQueue().WriteValueType<GfxCmdVBOMapVertexStream>(map);
		GetCommandQueue().WriteStreamingData(GetStreamBuffer(stream), size);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->UnmapVertexStream(stream);
	}
	m_IsStreamMapped[0] = false;
}

bool ThreadedVBO::IsVertexBufferLost() const
{
	if (m_NonThreadedVBO)
		return m_NonThreadedVBO->IsVertexBufferLost();
	else
		return m_VertexBufferLost;
}

bool ThreadedVBO::IsIndexBufferLost() const
{
	if (m_NonThreadedVBO)
		return m_NonThreadedVBO->IsIndexBufferLost();
	else
		return m_IndexBufferLost;
}

void ThreadedVBO::SetMappedFromRenderThread( bool renderThread )
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS
	m_MappedFromRenderThread = renderThread;
#endif
}

void ThreadedVBO::SetVertexStreamMode( unsigned stream, StreamMode mode )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (mode == GetVertexStreamMode(stream))
		return;
	VBO::SetVertexStreamMode(stream, mode);
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_SetVertexStreamMode);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdVBOSetVertexStreamMode vsmode = { *m_ClientVBO, stream, mode };
#else
		GfxCmdVBOSetVertexStreamMode vsmode = { m_ClientVBO, stream, mode };
#endif
		GetCommandQueue().WriteValueType<GfxCmdVBOSetVertexStreamMode>(vsmode);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->SetVertexStreamMode(stream, mode);
	}
}

void ThreadedVBO::SetIndicesDynamic(bool dynamic)
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (dynamic == AreIndicesDynamic())
		return;
	VBO::SetIndicesDynamic(dynamic);
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_SetIndicesDynamic);
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		GfxCmdVBOSetSetIndicesDynamic vsdyn = { *m_ClientVBO, (int)dynamic };
#else
		GfxCmdVBOSetSetIndicesDynamic vsdyn = { m_ClientVBO, (int)dynamic };
#endif
		GetCommandQueue().WriteValueType<GfxCmdVBOSetSetIndicesDynamic>(vsdyn);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->SetIndicesDynamic(dynamic);
	}
}

void ThreadedVBO::ResetDynamicVB()
{
	for (int s = 0; s < kMaxVertexStreams; s++)
	{
		if (m_StreamModes[s] == kStreamModeDynamic)
			m_VertexBufferLost = true;
	}
}

void ThreadedVBO::MarkBuffersLost()
{
	m_VertexBufferLost = m_IndexBufferLost = true;
}


int ThreadedVBO::GetRuntimeMemorySize() const
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
#if ENABLE_GFXDEVICE_REMOTE_PROCESS
		return 0;
#else
		return m_ClientVBO->GetInternal()?m_ClientVBO->GetInternal()->GetRuntimeMemorySize():0;
#endif
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		return m_NonThreadedVBO->GetRuntimeMemorySize();
	}
}

void ThreadedVBO::UseAsStreamOutput()
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_UseAsStreamOutput);
		GetCommandQueue().WriteValueType<ClientDeviceVBO *>(m_ClientVBO);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->UseAsStreamOutput();
	}

}

#if UNITY_XENON
void ThreadedVBO::AddExtraUvChannels( const UInt8* data, UInt32 size, int extraUvCount )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_AddExtraUvChannels);
		GfxCmdVBOAddExtraUvChannels adduv = { m_ClientVBO, size, extraUvCount };
		GetCommandQueue().WriteValueType<GfxCmdVBOAddExtraUvChannels>(adduv);
		GetCommandQueue().WriteStreamingData(data, size);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->AddExtraUvChannels(data, size, extraUvCount);
	}
}

void ThreadedVBO::CopyExtraUvChannels( VBO* source )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	DebugAssert(source != NULL);
	ThreadedVBO* src = static_cast<ThreadedVBO*>(source);
	if (m_ClientDevice.IsSerializing())
	{
		GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_VBO_CopyExtraUvChannels);
		GfxCmdVBOCopyExtraUvChannels copyuv = { m_ClientVBO, src->m_ClientVBO };
		GetCommandQueue().WriteValueType<GfxCmdVBOCopyExtraUvChannels>(copyuv);
		GFXDEVICE_LOCKSTEP_CLIENT();
	}
	else
	{
		DebugAssert(m_NonThreadedVBO);
		m_NonThreadedVBO->CopyExtraUvChannels(src->m_NonThreadedVBO);
	}
}
#endif

ThreadedStreamBuffer& ThreadedVBO::GetCommandQueue()
{
	return *m_ClientDevice.GetCommandQueue();
}

GfxDeviceWorker* ThreadedVBO::GetGfxDeviceWorker()
{
	return m_ClientDevice.GetGfxDeviceWorker();
}

void ThreadedVBO::SubmitCommands()
{
	m_ClientDevice.SubmitCommands();
}

void ThreadedVBO::DoLockstep()
{
	m_ClientDevice.DoLockstep();
}


ThreadedDynamicVBO::ThreadedDynamicVBO(GfxDeviceClient& device) :
	m_ClientDevice(device),
	m_ValidChunk(false)
{
}

bool ThreadedDynamicVBO::GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	Assert( !m_LendedChunk );
	DebugAssert( outVB != NULL && maxVertices > 0 );
	DebugAssert(
		(renderMode == kDrawIndexedTriangles		&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawIndexedTriangleStrip	&& (outIB != NULL && maxIndices > 0)) ||
		(renderMode == kDrawTriangleStrip			&& (outIB == NULL && maxIndices == 0)) ||
		(renderMode == kDrawQuads					&& (outIB == NULL && maxIndices == 0)));

	m_LendedChunk = true;
	m_LastChunkShaderChannelMask = shaderChannelMask;
	m_LastRenderMode = renderMode;

	m_LastChunkStride = 0;
	for( int i = 0; i < kShaderChannelCount; ++i ) {
		if( shaderChannelMask & (1<<i) )
			m_LastChunkStride += VBO::GetDefaultChannelByteSize(i);
	}
	m_LastChunkVertices = maxVertices;
	m_LastChunkIndices = maxIndices;
	int vertexChunkSize = m_LastChunkStride * maxVertices;
	m_ChunkVertices.resize_uninitialized(vertexChunkSize);
	m_ChunkIndices.resize_uninitialized(maxIndices);
	*outVB = &m_ChunkVertices[0];
	if (outIB)
		*outIB = &m_ChunkIndices[0];
	return true;
}

void ThreadedDynamicVBO::ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices )
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	Assert( m_LendedChunk );
	m_LendedChunk = false;
	m_ValidChunk = (actualVertices > 0) && (m_LastChunkIndices == 0 || actualIndices > 0);
	if (!m_ValidChunk)
		return;
	Assert(actualVertices <= m_LastChunkVertices);
	Assert(actualIndices <= m_LastChunkIndices);
	GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_DynVBO_Chunk);
	GfxCmdDynVboChunk chunk = { m_LastChunkShaderChannelMask, m_LastChunkStride, actualVertices, actualIndices, m_LastRenderMode };
	GetCommandQueue().WriteValueType<GfxCmdDynVboChunk>(chunk);
	GetCommandQueue().WriteStreamingData(&m_ChunkVertices[0], actualVertices * m_LastChunkStride);
	if (actualIndices > 0)
		GetCommandQueue().WriteStreamingData(&m_ChunkIndices[0], actualIndices * kVBOIndexSize);
	GetCommandQueue().WriteSubmitData();
	GFXDEVICE_LOCKSTEP_CLIENT();
}

void ThreadedDynamicVBO::DrawChunk (const ChannelAssigns& channels)
{
	DebugAssert(Thread::CurrentThreadIsMainThread());
	Assert( !m_LendedChunk );
	if (!m_ValidChunk)
		return;
	m_ClientDevice.BeforeDrawCall(false);
	GetCommandQueue().WriteValueType<GfxCommand>(kGfxCmd_DynVBO_DrawChunk);
	GetCommandQueue().WriteValueType<ChannelAssigns>(channels);
	GetCommandQueue().WriteSubmitData();
	GFXDEVICE_LOCKSTEP_CLIENT();
}

ThreadedStreamBuffer& ThreadedDynamicVBO::GetCommandQueue()
{
	return *m_ClientDevice.GetCommandQueue();
}

GfxDeviceWorker* ThreadedDynamicVBO::GetGfxDeviceWorker()
{
	return m_ClientDevice.GetGfxDeviceWorker();
}

void ThreadedDynamicVBO::SubmitCommands()
{
	m_ClientDevice.SubmitCommands();
}

void ThreadedDynamicVBO::DoLockstep()
{
	m_ClientDevice.DoLockstep();
}
