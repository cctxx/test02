#include "UnityPrefix.h"
#include "BufferedVBO.h"


BufferedVBO::BufferedVBO()
{
	m_AllocatedSize = 0;
}

BufferedVBO::~BufferedVBO()
{
	UnbufferVertexData();
}

void BufferedVBO::BufferAllVertexData( const VertexBufferData& buffer )
{
	bool copyModes[kStreamModeCount];
	std::fill(copyModes, copyModes + kStreamModeCount, true);
	BufferVertexData(buffer, copyModes);
}

void BufferedVBO::BufferAccessibleVertexData( const VertexBufferData& buffer )
{
	bool copyModes[kStreamModeCount];
	std::fill(copyModes, copyModes + kStreamModeCount, true);
	copyModes[kStreamModeNoAccess] = false;
	BufferVertexData(buffer, copyModes);
}

void BufferedVBO::BufferVertexData( const VertexBufferData& buffer, bool copyModes[kStreamModeCount] )
{
	std::copy(buffer.channels, buffer.channels + kShaderChannelCount, m_VertexData.channels);

	int streamOffset = 0;
	for (int s = 0; s < kMaxVertexStreams; s++)
	{
		StreamInfo& stream = m_VertexData.streams[s];
		const StreamInfo& srcStream = buffer.streams[s];
		UInt8 mode = m_StreamModes[s];
		Assert(mode < kStreamModeCount);
		if (copyModes[mode] && srcStream.channelMask)
		{
			stream = srcStream;
			stream.offset = streamOffset;
			streamOffset += CalculateVertexStreamSize(buffer, s);
			streamOffset = VertexData::AlignStreamSize(streamOffset);
		}
		else
			stream.Reset();
	}
	size_t allocSize = VertexData::GetAllocateDataSize(streamOffset);
	if (allocSize != m_AllocatedSize)
	{
		UnbufferVertexData();
		m_AllocatedSize = allocSize;
		m_VertexData.buffer = (UInt8*)UNITY_MALLOC_ALIGNED (kMemVertexData, allocSize, VertexData::kVertexDataAlign);
	}
	m_VertexData.bufferSize = streamOffset;
	m_VertexData.vertexCount = buffer.vertexCount;

	for (int s = 0; s < kMaxVertexStreams; s++)
	{
		const StreamInfo& srcStream = buffer.streams[s];
		UInt8 mode = m_StreamModes[s];
		if (copyModes[mode] && srcStream.channelMask)
			CopyVertexStream(buffer, GetStreamBuffer(s), s);
	}
}

void BufferedVBO::UnbufferVertexData()
{
	if (m_AllocatedSize > 0)
		UNITY_FREE(kMemVertexData, m_VertexData.buffer);

	m_AllocatedSize = 0;
	m_VertexData.buffer = NULL;
}

bool BufferedVBO::MapVertexStream( VertexStreamData& outData, unsigned stream )
{
	Assert(!m_IsStreamMapped[stream]);
	DebugAssert(m_StreamModes[stream] != kStreamModeNoAccess);
	if (m_StreamModes[stream] == kStreamModeNoAccess)
		return false;
	DebugAssert(m_VertexData.buffer);
	if (!m_VertexData.buffer)
		return false;
	outData.buffer = m_VertexData.buffer + m_VertexData.streams[stream].offset;
	outData.channelMask = m_VertexData.streams[stream].channelMask;
	outData.stride = m_VertexData.streams[stream].stride;
	outData.vertexCount = m_VertexData.vertexCount;
	m_IsStreamMapped[stream] = true;
	return true;
}

void BufferedVBO::UnmapVertexStream( unsigned stream )
{
	Assert(m_IsStreamMapped[stream]);
	m_IsStreamMapped[stream] = false;
}

int BufferedVBO::GetRuntimeMemorySize() const
{
	return m_AllocatedSize;
}

UInt8* BufferedVBO::GetStreamBuffer(unsigned stream)
{
	return m_VertexData.buffer + m_VertexData.streams[stream].offset;
}

UInt8* BufferedVBO::GetChannelDataAndStride(ShaderChannel channel, UInt32& outStride)
{
	const ChannelInfo& info = m_VertexData.channels[channel];
	if (info.IsValid())
	{
		outStride = info.CalcStride(m_VertexData.streams);
		return m_VertexData.buffer + info.CalcOffset(m_VertexData.streams);
	}
	outStride = 0;
	return NULL;
}

void BufferedVBO::GetChannelDataAndStrides(void* channelData[kShaderChannelCount], UInt32 outStrides[kShaderChannelCount])
{
	for (int i = 0; i < kShaderChannelCount; i++)
		channelData[i] = GetChannelDataAndStride(ShaderChannel(i), outStrides[i]);
}

void BufferedVBO::GetChannelOffsetsAndStrides(void* channelOffsets[kShaderChannelCount], UInt32 outStrides[kShaderChannelCount])
{
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		channelOffsets[i] = reinterpret_cast<void*>(m_VertexData.channels[i].CalcOffset(m_VertexData.streams));
		outStrides[i] = m_VertexData.channels[i].CalcStride(m_VertexData.streams);
	}
}

void BufferedVBO::UnloadSourceVertices()
{
	UnbufferVertexData();
}
