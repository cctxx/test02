#include "UnityPrefix.h"
#include "VBO.h"
#include "GraphicsCaps.h"

#include <cstring>
#include "Runtime/Utilities/OptimizationUtility.h"
#include "Runtime/Math/Color.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#if UNITY_XENON
#include <ppcintrinsics.h>
#endif



static const int kChannelVertexSize[kShaderChannelCount] = {
	3 * sizeof (float),	// position
	3 * sizeof (float),	// normal
	1 * sizeof (UInt32),// color
	2 * sizeof (float),	// uv0
	2 * sizeof (float),	// uv1
	4 * sizeof (float),	// tangent
};

static const UInt8 kDefaultVertexChannelDimensionCount [kShaderChannelCount] = {
	3, 3, 1, 2, 2, 4
};

// -----------------------------------------------------------------------------
//  VBO common

int VBO::GetDefaultChannelByteSize (int channelNum)
{
	DebugAssertIf( channelNum < 0 || channelNum >= kShaderChannelCount );
	return kChannelVertexSize[channelNum];
}

int VBO::GetDefaultChannelFormat(int channelNum)
{
    DebugAssertIf( channelNum < 0 || channelNum >= kShaderChannelCount );
    return (kShaderChannelColor == channelNum) ? kChannelFormatColor : kChannelFormatFloat;
}

int VBO::GetDefaultChannelDimension(int channelNum)
{
    return kDefaultVertexChannelDimensionCount [channelNum];
}

bool VBO::IsAnyStreamMapped() const
{
	for (int i = 0; i < kMaxVertexStreams; ++i)
		if (m_IsStreamMapped[i])
			return true;

	return false;
}

bool VBO::HasStreamWithMode(StreamMode mode) const
{
	for (int i = 0; i < kMaxVertexStreams; ++i)
		if (m_StreamModes[i] == mode && m_Streams[i].channelMask)
			return true;

	return false;
}

size_t GetVertexSize (unsigned shaderChannelsMask)
{
	size_t size = 0;
	for (int i=0; i<kShaderChannelCount; ++i, shaderChannelsMask >>= 1)
		if (shaderChannelsMask & 1)
			size += kChannelVertexSize[i];
	return size;
}

DynamicVBO::DynamicVBO()
:	m_LastChunkShaderChannelMask(0)
,	m_LastChunkStride(0)
,	m_LastChunkVertices(0)
,	m_LastChunkIndices(0)
,	m_LastRenderMode(kDrawIndexedTriangles)
,	m_LendedChunk(false)
{
}


// ----------------------------------------------------------------------


void CopyVertexStream( const VertexBufferData& sourceData, void* buffer, unsigned stream )
{
	DebugAssert(stream < kMaxVertexStreams);

	if (!sourceData.buffer)
		return;

	const StreamInfo& info = sourceData.streams[stream];
	const UInt8* src = static_cast<const UInt8*>(sourceData.buffer) + info.offset;
	size_t size = CalculateVertexStreamSize(sourceData, stream);
	#if UNITY_XENON
		XMemCpyStreaming_WriteCombined(buffer, src, size);
	#else
		memcpy(buffer, src, size);
	#endif
}

void CopyVertexBuffer( const VertexBufferData& sourceData, void* buffer )
{
	if (sourceData.buffer)
		memcpy(buffer, sourceData.buffer, sourceData.bufferSize);
}

void GetVertexStreamOffsets( const VertexBufferData& sourceData, size_t dest[kShaderChannelCount], size_t baseOffset, unsigned stream )
{
	for (int i = 0; i < kShaderChannelCount; ++i)
	{
        const ChannelInfo& channel = sourceData.channels[i];
        dest[i] = channel.IsValid() ? baseOffset + channel.CalcOffset(sourceData.streams) : 0;
	}
}

void GetVertexStreamPointers( const VertexBufferData& sourceData, void* dest[kShaderChannelCount], void* basePtr, unsigned stream )
{
	for (int i = 0; i < kShaderChannelCount; ++i)
	{
        const ChannelInfo& channel = sourceData.channels[i];
        dest[i] = channel.IsValid() ? (UInt8*)basePtr + channel.CalcOffset(sourceData.streams) : NULL;
	}
}


void FillIndexBufferForQuads (UInt16* dst, int dstSize, const UInt16* src, int quadCount)
{
	int canFitQuads = dstSize / kVBOIndexSize / 6;
	quadCount = std::min(quadCount, canFitQuads);

	for (int i = 0; i < quadCount; ++i)
	{
		*dst++ = src[0];
		*dst++ = src[1];
		*dst++ = src[2];
		*dst++ = src[0];
		*dst++ = src[2];
		*dst++ = src[3];
		src += 4;
	}
}



// --------------------------------------------------------------------------

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (VBOTests)
{

	TEST(FillIndexBufferForQuadsDoesNotOverwritePastEnd)
	{
		UInt16 srcBuffer[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
		UInt16 dstBuffer[] = { 17, 13, 17, 13, 17, 13, 1337 };
		FillIndexBufferForQuads (dstBuffer, sizeof(dstBuffer)-2, srcBuffer, 2);
		CHECK_EQUAL (1337, dstBuffer[6]);
	}

} // SUITE

#endif // ENABLE_UNIT_TESTS
