#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "VertexData.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include <algorithm>

/*
	On most platforms, for skinning/non-uniform-scaling of meshes you would want to split your data into 
	a hot data stream (position, normal and tangent) and a cold data stream (diffuse and uvs) in order to maximize CPU cache access patterns and 
	reduce bandwidth and computation ( you won't need to copy the cold data )
*/

VertexStreamsLayout VertexDataInfo::kVertexStreamsDefault = {{ kShaderChannelsAll, 0, 0, 0 }};
#if UNITY_PS3
	VertexStreamsLayout VertexDataInfo::kVertexStreamsSkinnedHotColdSplit = {{ VERTEX_FORMAT1(Vertex), VERTEX_FORMAT1(Normal), VERTEX_FORMAT1(Tangent), kShaderChannelsCold }};
#else
	VertexStreamsLayout VertexDataInfo::kVertexStreamsSkinnedHotColdSplit = {{ kShaderChannelsHot, kShaderChannelsCold, 0, 0 }};
#	if UNITY_EDITOR
		VertexStreamsLayout VertexDataInfo::kVertexStreamsSkinnedHotColdSplitPS3 = {{ VERTEX_FORMAT1(Vertex), VERTEX_FORMAT1(Normal), VERTEX_FORMAT1(Tangent), kShaderChannelsCold }};
#	endif
#endif

#define MAKE_CHANNEL(fmt, dim) VertexChannelsLayout::Channel(kChannelFormat##fmt, dim)
VertexChannelsLayout VertexDataInfo::kVertexChannelsDefault =
{{	// Array wrapped by struct requires double braces
	MAKE_CHANNEL(Float, 3),		// position
	MAKE_CHANNEL(Float, 3),		// normal
	MAKE_CHANNEL(Color, 1),		// color
	MAKE_CHANNEL(Float, 2),		// texcoord0
	MAKE_CHANNEL(Float, 2),		// texcoord1
	MAKE_CHANNEL(Float, 4)		// tangent
}};
VertexChannelsLayout VertexDataInfo::kVertexChannelsCompressed =
{{	// Array wrapped by struct requires double braces
	MAKE_CHANNEL(Float,   3),	// position
	MAKE_CHANNEL(Float16, 4),	// normal
	MAKE_CHANNEL(Color,   1),	// color
	MAKE_CHANNEL(Float16, 2),	// texcoord0
	MAKE_CHANNEL(Float16, 2),	// texcoord1
	MAKE_CHANNEL(Float16, 4)	// tangent
}};
VertexChannelsLayout VertexDataInfo::kVertexChannelsCompressedAggressive =
{{  // Array wrapped by struct requires double braces
    MAKE_CHANNEL(Float,   3),   // position
    MAKE_CHANNEL(Byte,    4),   // normal
    MAKE_CHANNEL(Color,   1),   // color
    MAKE_CHANNEL(Float16, 2),   // texcoord0
    MAKE_CHANNEL(Float16, 2),   // texcoord1
    MAKE_CHANNEL(Byte,    4)    // tangent
}};
#undef MAKE_CHANNEL

static const UInt8 kVertexChannelFormatSizes[kChannelFormatCount] = {
    4,  // kChannelFormatFloat
    2,  // kChannelFormatFloat16
    4,  // kChannelFormatColor
    1   // kChannelFormatByte
};

size_t GetChannelFormatSize(UInt8 format)
{
	Assert (format < kChannelFormatCount);
    return kVertexChannelFormatSizes[format];
}

static bool operator == (const VertexStreamsLayout& lhs, const VertexStreamsLayout& rhs)
{
	return CompareArrays(lhs.channelMasks, rhs.channelMasks, kMaxVertexStreams);
}

template<class TransferFunction>
void VertexData::Transfer (TransferFunction& transfer)
{
	#if SUPPORT_SERIALIZED_TYPETREES
	if (transfer.GetFlags() & kWorkaround35MeshSerializationFuckup)
	{
		TransferWorkaround35SerializationFuckup (transfer);
		return;
	}
	#endif

	transfer.Transfer (m_CurrentChannels, "m_CurrentChannels", kHideInEditorMask);
	transfer.Transfer (m_VertexCount, "m_VertexCount", kHideInEditorMask);

	dynamic_array<ChannelInfo> channels;
	dynamic_array<StreamInfo> streams;
	if (transfer.IsWriting ())
	{
		channels.resize_uninitialized (kShaderChannelCount);
		streams.resize_uninitialized (kMaxVertexStreams);
		std::copy (m_Channels, m_Channels + kShaderChannelCount, channels.begin ());
		std::copy (m_Streams, m_Streams + kMaxVertexStreams, streams.begin ());
	}
	transfer.Transfer (channels, "m_Channels", kHideInEditorMask);
	transfer.Transfer (streams, "m_Streams", kHideInEditorMask);
	
	if (transfer.IsReading ())
	{
		// For compatibility do this even if channels/streams info didn't exist (case 558604)
		// In the past there was only a channels mask, UpdateStreams() generates the info from that
		if (channels.size () == kShaderChannelCount)
			std::copy (channels.begin (), channels.begin () + kShaderChannelCount, m_Channels);
		if (streams.size () == kMaxVertexStreams)
			std::copy (streams.begin (), streams.begin () + kMaxVertexStreams, m_Streams);
		else
			std::fill (m_Streams, m_Streams + kMaxVertexStreams, StreamInfo());

		UInt32 channelsInStreams = 0;
		for (int i = 0; i < kMaxVertexStreams ; i++)
			channelsInStreams |= m_Streams[i].channelMask;
		if (channelsInStreams)
			UpdateStreams(channelsInStreams, m_VertexCount, GetStreamsLayout (), GetChannelsLayout ());
		else
			UpdateStreams(m_CurrentChannels, m_VertexCount, kVertexStreamsDefault, kVertexChannelsDefault);
	}

	transfer.TransferTypeless (&m_DataSize, "m_DataSize", kHideInEditorMask);
	if (transfer.DidReadLastProperty ())
	{
		if (m_Data)
			UNITY_FREE (kMemVertexData, m_Data);
		m_Data = (UInt8*)UNITY_MALLOC_ALIGNED (kMemVertexData, VertexData::GetAllocateDataSize (m_DataSize), kVertexDataAlign);
	}

	transfer.TransferTypelessData (m_DataSize, m_Data);
}	

#if SUPPORT_SERIALIZED_TYPETREES
template<class TransferFunction>
void VertexData::TransferWorkaround35SerializationFuckup (TransferFunction& transfer)
{
	UInt32 currentChannels = m_CurrentChannels;
	transfer.Transfer (currentChannels, "m_CurrentChannels", kHideInEditorMask);
	transfer.Transfer (m_VertexCount, "m_VertexCount", kHideInEditorMask);

	TRANSFER(m_Streams[0]);
	TRANSFER(m_Streams[1]);
	TRANSFER(m_Streams[2]);
	TRANSFER(m_Streams[3]);

	if (transfer.IsReading ())
	{
		if(m_VertexCount && (currentChannels == 0))
		{
			for(int i=0;i<kMaxVertexStreams;i++)
				currentChannels |= m_Streams[i].channelMask;
		}
		UpdateStreams(currentChannels, m_VertexCount);
		//GetComponentInfo(m_Components, currentChannels);
		m_CurrentChannels = currentChannels;
	}

	transfer.TransferTypeless (&m_DataSize, "m_DataSize", kHideInEditorMask);

	if (transfer.IsReading ())
	{
		if (m_Data)
			UNITY_FREE (kMemVertexData, m_Data);
		m_Data = (UInt8*)UNITY_MALLOC_ALIGNED (kMemVertexData, VertexData::GetAllocateDataSize (m_DataSize), kVertexDataAlign);
	}

	transfer.TransferTypelessData (m_DataSize, m_Data);
}
#endif

INSTANTIATE_TEMPLATE_TRANSFER(VertexData)

void VertexDataInfo::UpdateStreams(unsigned newChannelMask, size_t newVertexCount, const VertexStreamsLayout& streams, const VertexChannelsLayout& channels)
{
	m_VertexCount = newVertexCount;
	m_CurrentChannels = 0;
	m_VertexSize = 0;
	size_t streamOffset = 0;
	for (int s = 0; s < kMaxVertexStreams; s++)
	{
		StreamInfo& stream = m_Streams[s];
		m_Streams[s].Reset();
		stream.channelMask = streams.channelMasks[s] & newChannelMask;
		if (stream.channelMask == 0)
			continue;
		m_CurrentChannels |= stream.channelMask;
		for (int c = 0; c < kShaderChannelCount; c++)
		{
			if (stream.channelMask & (1 << c))
			{
				ChannelInfo& channel = m_Channels[c];
				const VertexChannelsLayout::Channel& srcChannel = channels.channels[c];
				channel.stream = s;
				channel.offset = stream.stride;
				channel.format = srcChannel.format;
				channel.dimension = srcChannel.dimension;
				stream.stride += channel.dimension * GetChannelFormatSize(channel.format);
			}
		}
		streamOffset = AlignStreamSize(streamOffset);
		stream.offset = streamOffset; 
		streamOffset += stream.stride * newVertexCount;
		m_VertexSize += stream.stride;
	}
	for (int c = 0; c < kShaderChannelCount; c++)
	{
		// Reset channels that were removed
		if (!(m_CurrentChannels & (1 << c)))
			m_Channels[c].Reset();
	}
	m_DataSize = streamOffset;
}

size_t VertexDataInfo::GetActiveStreamCount() const 
{
	size_t activeStreamCount = 0;
	for (int i=0; i<kMaxVertexStreams; i++)
	{
		if(m_Streams[i].channelMask != 0)
			activeStreamCount++;
	}
	return activeStreamCount;
}

size_t VertexDataInfo::GetStreamIndex(ShaderChannel channel) const
{
	UInt32 channelMask = 1 << channel;
	for (int i=0; i<kMaxVertexStreams; i++)
	{
		if(m_Streams[i].channelMask & channelMask)
			return i;
	}
	return -1;
}

VertexStreamsLayout VertexDataInfo::GetStreamsLayout() const
{
	VertexStreamsLayout result;
	for (int i = 0; i < kMaxVertexStreams; i++)
		result.channelMasks[i] = m_Streams[i].channelMask;
	return result;
}

VertexChannelsLayout VertexDataInfo::GetChannelsLayout() const
{
	VertexChannelsLayout result;
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		result.channels[i] = VertexChannelsLayout::Channel(m_Channels[i].format, m_Channels[i].dimension);
	}
	return result;
}

bool VertexDataInfo::ConformsToStreamsLayout(const VertexStreamsLayout& streams) const
{
	for (int i = 0; i < kMaxVertexStreams; i++)
	{
		// Fail if we have a channel that's not in the layout
		if (m_Streams[i].channelMask & ~streams.channelMasks[i])
			return false;
	}
	return true;
}

bool VertexDataInfo::ConformsToChannelsLayout(const VertexChannelsLayout& channels) const
{
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		if (m_Channels[i].IsValid())
		{
			const VertexChannelsLayout::Channel& channel = channels.channels[i];
			if (m_Channels[i].format != channel.format ||
				m_Channels[i].dimension != channel.dimension)
				return false;
		}
	}
	return true;
}

signed char f32_to_s8(float fval)
{
   return ((fval * 255.0f) - 1.0f) / 2.0f;
}

float s8_to_f32(signed char val)
{
   return (2*(val/255.0f)-1.0f);
}

static void ConvertCopyChannel(size_t vertexCount, 
                        const UInt8* srcPtr, UInt8 srcStride, UInt8 srcType, UInt8 srcDim, 
                        UInt8* dstPtr, UInt8 dstStride, UInt8 dstType, UInt8 dstDim)
{
	UInt8 minDim = std::min(srcDim, dstDim);
	if (srcType == kChannelFormatFloat16 && dstType == kChannelFormatFloat)
	{
		// decompressing
		for (size_t i = 0; i < vertexCount; i++)
		{
			UInt8 comp = 0;
			for ( ; comp < minDim; comp++)
				HalfToFloat(reinterpret_cast<const UInt16*>(srcPtr)[comp], reinterpret_cast<float*>(dstPtr)[comp]);
			for ( ; comp < dstDim; comp++)
				reinterpret_cast<float*>(dstPtr)[comp] = 0.0f;
			srcPtr += srcStride;
			dstPtr += dstStride;
		}
	}
	else if (srcType == kChannelFormatByte && dstType == kChannelFormatFloat)
	{
		// decompressing
		for (size_t i = 0; i < vertexCount; i++)
		{
			UInt8 comp = 0;
			for ( ; comp < minDim; comp++)
				reinterpret_cast<float*>(dstPtr)[comp] = s8_to_f32(reinterpret_cast<const SInt8*>(srcPtr)[comp]);
			for ( ; comp < dstDim; comp++)
				reinterpret_cast<float*>(dstPtr)[comp] = 0.0f;
			srcPtr += srcStride;
			dstPtr += dstStride;
		}
	}
#if UNITY_EDITOR
	else if (srcType == kChannelFormatFloat && dstType == kChannelFormatFloat16)
	{
		// compressing
		for (size_t i = 0; i < vertexCount; i++)
		{
			UInt8 comp = 0;
			for ( ; comp < minDim; comp++)
				g_FloatToHalf.Convert(reinterpret_cast<const float*>(srcPtr)[comp], reinterpret_cast<UInt16*>(dstPtr)[comp]);
			for ( ; comp < dstDim; comp++)
				reinterpret_cast<UInt16*>(dstPtr)[comp] = 0;
			srcPtr += srcStride;
			dstPtr += dstStride;
		}
	}
	else if (srcType == kChannelFormatFloat && dstType == kChannelFormatByte)
	{
		// compressing
		for (size_t i = 0; i < vertexCount; i++)
		{
			UInt8 comp = 0;
			for ( ; comp < minDim; comp++)
				reinterpret_cast<SInt8*>(dstPtr)[comp] = f32_to_s8(reinterpret_cast<const float*>(srcPtr)[comp]);
			for ( ; comp < dstDim; comp++)
				reinterpret_cast<SInt8*>(dstPtr)[comp] = 0;
			srcPtr += srcStride;
			dstPtr += dstStride;
		}
	}
#endif
	else
		ErrorString("Unsupported conversion of vertex formats");
}

static void CopyChannels (size_t vertexCount, unsigned copyChannels, 
                   const StreamInfoArray srcStreams, const ChannelInfoArray srcChannels, const UInt8* srcData, 
                   const StreamInfoArray dstStreams, const ChannelInfoArray dstChannels, UInt8* dstData)
{
	for (unsigned chan = copyChannels, i = 0; chan && (i < kShaderChannelCount); i++, chan >>= 1)
	{
		if (0 == (chan & 1))
            continue;
        
        const ChannelInfo& srcChannel = srcChannels[i];
        const ChannelInfo& dstChannel = dstChannels[i];

        const UInt8* srcPtr = srcData + srcChannel.CalcOffset(srcStreams);
        UInt8* dstPtr = dstData + dstChannel.CalcOffset(dstStreams);
        UInt8 srcStride = srcChannel.CalcStride(srcStreams);
        UInt8 dstStride = dstChannel.CalcStride(dstStreams);
        
        if(srcChannel.format == dstChannel.format)
        {
			size_t copySize = srcChannel.dimension * GetChannelFormatSize(srcChannel.format);
            switch (copySize)
            {
                case 4:
                {
                    for (size_t i=0; i<vertexCount; ++i)
                    {
                        *(reinterpret_cast<UInt32*> (dstPtr) + 0) = *(reinterpret_cast<const UInt32*> (srcPtr) + 0);
                        srcPtr += srcStride;
                        dstPtr += dstStride;
                    }
                    break;
                }
                case 8:
                {
                    for (size_t i=0; i<vertexCount; ++i)
                    {
                        *(reinterpret_cast<UInt32*> (dstPtr) + 0) = *(reinterpret_cast<const UInt32*> (srcPtr) + 0);
                        *(reinterpret_cast<UInt32*> (dstPtr) + 1) = *(reinterpret_cast<const UInt32*> (srcPtr) + 1);
                        srcPtr += srcStride;
                        dstPtr += dstStride;
                    }
                    break;
                }
                case 12:
                {
                    for (size_t i=0; i<vertexCount; ++i)
                    {
                        *(reinterpret_cast<UInt32*> (dstPtr) + 0) = *(reinterpret_cast<const UInt32*> (srcPtr) + 0);
                        *(reinterpret_cast<UInt32*> (dstPtr) + 1) = *(reinterpret_cast<const UInt32*> (srcPtr) + 1);
                        *(reinterpret_cast<UInt32*> (dstPtr) + 2) = *(reinterpret_cast<const UInt32*> (srcPtr) + 2);
                        srcPtr += srcStride;
                        dstPtr += dstStride;
                    }
                    break;
                }
                default:
                {
                    for (size_t i=0; i<vertexCount; ++i)
                    {
                        memcpy (dstPtr, srcPtr, copySize);
                        srcPtr += srcStride;
                        dstPtr += dstStride;
                    }
                    break;
                }
            }
        }
        else
        {
            ConvertCopyChannel(vertexCount, srcPtr, srcStride, srcChannel.format, srcChannel.dimension, dstPtr, dstStride, dstChannel.format, dstChannel.dimension);
        }
    }
}

VertexDataInfo::VertexDataInfo ()
:	m_Data(NULL)
,	m_DataSize(0)
,	m_VertexCount(0)
,	m_VertexSize(0)
,	m_CurrentChannels(0)
{
	// Channels and streams have default constructors
}

VertexData::VertexData (VertexData const& src, unsigned copyChannels, const VertexStreamsLayout& streams, const VertexChannelsLayout& channels)
{
	// We do not support inserting new channels that are not present in the source
	Assert ((copyChannels & src.GetChannelMask()) == copyChannels);

	UpdateStreams(copyChannels, src.m_VertexCount, streams, channels);
	m_Data = (UInt8*) UNITY_MALLOC_ALIGNED (kMemVertexData, VertexData::GetAllocateDataSize (m_DataSize), kVertexDataAlign);
	
	const VertexData& dest = *this;
	if (m_DataSize == src.m_DataSize &&
		copyChannels == src.GetChannelMask() && 
		CompareMemory(dest.m_Channels, src.m_Channels) &&
		CompareMemory(dest.m_Streams, src.m_Streams))
	{
		// Simple copy if the format didn't change
		memcpy (m_Data, src.m_Data, m_DataSize);
	}
	else
		CopyChannels (m_VertexCount, copyChannels, src.m_Streams, src.m_Channels, src.m_Data, m_Streams, m_Channels, m_Data);
}

VertexData::~VertexData ()
{
	Deallocate();
}

void VertexData::Deallocate ()
{
	if (m_Data)
		UNITY_FREE(kMemVertexData, m_Data);
	m_Data = NULL;
}

void VertexData::Resize (size_t vertexCount, unsigned channelMask, const VertexStreamsLayout& streams, const VertexChannelsLayout& channels)
{
	ChannelInfoArray srcChannels;
	StreamInfoArray srcStreams;
	memcpy(srcChannels, m_Channels, sizeof(srcChannels));
	memcpy(srcStreams, m_Streams, sizeof(srcStreams));
	UInt32 srcChannelMask = m_CurrentChannels;
	UInt32 srcVertexCount = m_VertexCount;
	UInt8* srcData = m_Data;
	
    UpdateStreams(channelMask, vertexCount, streams, channels);
	
	// In case the streams and channels don't change, simply reallocate the buffer and return
	// Note that this will rarely be true with multiple streams since the stream offsets change
	if (m_Data && CompareMemory(srcChannels, m_Channels) && CompareMemory(srcStreams, m_Streams))
	{
		m_Data = (UInt8*)UNITY_REALLOC_ALIGNED(kMemVertexData, m_Data, VertexData::GetAllocateDataSize(m_DataSize), kVertexDataAlign);
		return;
	}

	m_Data = (UInt8*)UNITY_MALLOC_ALIGNED(kMemVertexData, VertexData::GetAllocateDataSize(m_DataSize), kVertexDataAlign);
	// copy over the old data
	if (srcData)
	{
		unsigned copyChannels = srcChannelMask & m_CurrentChannels;
		size_t toCopyCount = std::min<size_t>(srcVertexCount, m_VertexCount);
		CopyChannels(toCopyCount, copyChannels, srcStreams, srcChannels, srcData, m_Streams, m_Channels, m_Data);
		UNITY_FREE(kMemVertexData, srcData);
	}
}


void VertexData::SwapEndianess ()
{
	unsigned const kChannelSwapMask = VERTEX_FORMAT5(Vertex, Normal, TexCoord0, TexCoord1, Tangent);
	for (int s = 0; s < kMaxVertexStreams; s++)
	{
		if (m_Streams[s].stride)
		{
			StreamInfo& stream = m_Streams[s];
			size_t stride = stream.stride;
			UInt8* dataStart = m_Data + stream.offset;
			UInt8* dataEnd = dataStart + stream.stride * m_VertexCount;
			UInt32 channelMask = stream.channelMask;
			for (UInt8* p = dataStart, *end = dataEnd; p != end; p += stride)
			{				
				// counting from LSb, 1 denotes that a value should be endian-swapped
				int localOffset = 0;
				for (unsigned i=0, chan = channelMask, swap = kChannelSwapMask; i<kShaderChannelCount; ++i, chan >>= 1, swap >>= 1)
				{
					if (chan & 1)
					{
						size_t componentCount = m_Channels[i].dimension;
						size_t componentSize = GetChannelFormatSize(m_Channels[i].format);
						if(swap & 1)
						{
							Assert (m_Channels [i].IsValid());
							SwapEndianArray (p + localOffset, componentSize, componentCount);
						}
						localOffset += componentCount * componentSize;
					}
				}
			}
		}
	}
}

void swap (VertexData& a, VertexData& b)
{
	std::swap_ranges (a.m_Channels, a.m_Channels + kShaderChannelCount, b.m_Channels);
	std::swap_ranges (a.m_Streams, a.m_Streams + kMaxVertexStreams, b.m_Streams);
	std::swap (a.m_CurrentChannels, b.m_CurrentChannels);
	std::swap (a.m_VertexSize, b.m_VertexSize);
	std::swap (a.m_VertexCount, b.m_VertexCount);
	std::swap (a.m_DataSize, b.m_DataSize);
	std::swap (a.m_Data, b.m_Data);
}

void CopyVertexDataChannels (size_t vertexCount, unsigned copyChannels, const VertexData& srcData, VertexData& dstData)
{
	Assert (vertexCount <= srcData.GetVertexCount() && vertexCount <= dstData.GetVertexCount());
	Assert ((srcData.GetChannelMask() & copyChannels) == copyChannels);
	Assert ((dstData.GetChannelMask() & copyChannels) == copyChannels);
	CopyChannels (vertexCount, copyChannels,
		srcData.GetStreams(), srcData.GetChannels(), srcData.GetDataPtr(),
		dstData.GetStreams(), dstData.GetChannels(), dstData.GetDataPtr());
}

