#ifndef VERTEX_DATA_H_
#define VERTEX_DATA_H_

#include "Runtime/Utilities/StrideIterator.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/TransferFunctionFwd.h"

class VertexData;

void swap (VertexData& a, VertexData& b);

typedef struct StreamInfo
{
    enum { kDividerOpDivide=0, kDividerOpModulo };
    
	UInt32			channelMask;
	UInt32			offset;
    UInt16          frequency;
	UInt8			stride;
    UInt8           dividerOp;
    
	// We use default constructors instead of memset()
	StreamInfo() : channelMask(0), offset(0), frequency(0), stride(0), dividerOp(kDividerOpDivide) {}
	void Reset() { *this = StreamInfo(); }
    
	bool operator == (const StreamInfo& rhs) const { return (channelMask == rhs.channelMask) && (offset == rhs.offset) && (frequency == rhs.frequency) && (stride == rhs.stride) && (dividerOp == rhs.dividerOp); }
	bool operator != (const StreamInfo& rhs) const { return !(*this == rhs); }

	DECLARE_SERIALIZE_NO_PPTR (StreamInfo);
    
#if SUPPORT_SERIALIZED_TYPETREES
	template<class TransferFunction>
	void TransferWorkaround35SerializationFuckup (TransferFunction& transfer);
#endif

} StreamInfoArray [kMaxVertexStreams];

struct VertexStreamsLayout
{
	UInt32 channelMasks[kMaxVertexStreams];
};

typedef struct ALIGN_TYPE(4) ChannelInfo
{
	UInt8 stream;
	UInt8 offset;
	UInt8 format;
	UInt8 dimension;

	enum { kInvalidDimension = 0 };

	// We use default constructors instead of memset()
	ChannelInfo() : stream(0), offset(0), format(0), dimension(kInvalidDimension) {}

	UInt32 CalcOffset(const StreamInfoArray streams) const { return streams[stream].offset + offset; }
	UInt32 CalcStride(const StreamInfoArray streams) const { return streams[stream].stride; }
	bool IsValid() const { return (kInvalidDimension != dimension); }
	void Reset() { *this = ChannelInfo(); }

	bool operator == (const ChannelInfo& rhs) const { return (stream == rhs.stream) && (offset == rhs.offset) && (format == rhs.format) && (dimension == rhs.dimension); }
	bool operator != (const ChannelInfo& rhs) const { return !(*this == rhs); }

    DECLARE_SERIALIZE_NO_PPTR (ChannelInfo);
    
} ChannelInfoArray [kShaderChannelCount];

struct VertexChannelsLayout
{
	struct Channel
	{
		Channel(UInt8 fmt, UInt8 dim) : format(fmt), dimension(dim) {}
		Channel() : format(0), dimension(0) {}
		UInt8 format;
		UInt8 dimension;
	};
	Channel channels[kShaderChannelCount];
};


template<class TransferFunc>
void StreamInfo::Transfer (TransferFunc& transfer)
{
	#if SUPPORT_SERIALIZED_TYPETREES
	if (transfer.GetFlags() & kWorkaround35MeshSerializationFuckup)
	{
		TransferWorkaround35SerializationFuckup (transfer);
		return;
	}
	#endif

	transfer.Transfer (channelMask, "channelMask", kHideInEditorMask);
	transfer.Transfer (offset, "offset", kHideInEditorMask);
	transfer.Transfer (stride, "stride", kHideInEditorMask);
	transfer.Transfer (dividerOp, "dividerOp", kHideInEditorMask);
	transfer.Transfer (frequency, "frequency", kHideInEditorMask);
}

#if SUPPORT_SERIALIZED_TYPETREES
template<class TransferFunc>
void StreamInfo::TransferWorkaround35SerializationFuckup (TransferFunc& transfer)
{
	transfer.Transfer (channelMask, "channelMask", kHideInEditorMask);
	transfer.Transfer (offset, "offset", kHideInEditorMask);

	UInt32 align;
	UInt32 stride32bit;
	transfer.Transfer (stride32bit, "stride", kHideInEditorMask);
	transfer.Transfer (align, "align", kHideInEditorMask);

	stride = (UInt8) stride32bit;
}
#endif

template<class TransferFunc>
void ChannelInfo::Transfer (TransferFunc& transfer)
{
	transfer.Transfer (stream, "stream", kHideInEditorMask);
	transfer.Transfer (offset, "offset", kHideInEditorMask);
	transfer.Transfer (format, "format", kHideInEditorMask);
	transfer.Transfer (dimension, "dimension", kHideInEditorMask);
}

// Information about all vertex data, but does not own the memory
class VertexDataInfo
{
public:	
	enum
	{
		kVertexDataAlign = 32,
		kVertexStreamAlign = 16,
		kVertexDataPadding = 16
	};

	static VertexStreamsLayout kVertexStreamsDefault;
	static VertexStreamsLayout kVertexStreamsSkinnedHotColdSplit;
	static VertexChannelsLayout kVertexChannelsDefault;
	static VertexChannelsLayout kVertexChannelsCompressed;
	static VertexChannelsLayout kVertexChannelsCompressedAggressive;
#if UNITY_EDITOR
	static VertexStreamsLayout kVertexStreamsSkinnedHotColdSplitPS3;
#endif

	static size_t AlignStreamSize (size_t size) { return (size + (kVertexStreamAlign-1)) & ~(kVertexStreamAlign-1); }

	friend void ::swap (VertexData& a, VertexData& b);
	
	VertexDataInfo ();
	
	bool HasChannel (ShaderChannel shaderChannelIndex) const 
    {
		Assert ((m_Channels[shaderChannelIndex].dimension != 0) == (((m_CurrentChannels & (1 << shaderChannelIndex)) != 0)));
		return m_Channels[shaderChannelIndex].dimension != 0;
	}
	
	void UpdateStreams(unsigned newChannelMask, size_t newVertexCount, const VertexStreamsLayout& streams = kVertexStreamsDefault, const VertexChannelsLayout& channels = kVertexChannelsDefault);

	size_t GetActiveStreamCount() const ;
	size_t GetStreamIndex(ShaderChannel channel) const ;
	const StreamInfo* GetStreams() const { return m_Streams; }
	const StreamInfo& GetStream(int index) const { return m_Streams[index]; }
    
	const ChannelInfo* GetChannels() const { return m_Channels; }
	const ChannelInfo& GetChannel(int index) const { return m_Channels[index]; }
    
	VertexStreamsLayout GetStreamsLayout() const;
	VertexChannelsLayout GetChannelsLayout() const;

	bool ConformsToStreamsLayout(const VertexStreamsLayout& streams) const;
	bool ConformsToChannelsLayout(const VertexChannelsLayout& channels) const;

	unsigned GetChannelMask () const { return m_CurrentChannels; }
	size_t GetDataSize () const { return m_DataSize; }
	size_t GetVertexSize () const { return m_VertexSize; }
	size_t GetVertexCount () const { return m_VertexCount; }
	size_t GetChannelOffset (unsigned channel) const { return m_Channels[channel].CalcOffset(m_Streams); }
	size_t GetChannelStride (unsigned channel) const { return m_Channels[channel].CalcStride(m_Streams); }
	UInt8* GetDataPtr () const { return m_Data; }
	
	template<class T>
	StrideIterator<T> MakeStrideIterator (ShaderChannel shaderChannelIndex) const
	{
		Assert (shaderChannelIndex < kShaderChannelCount);
		void* p = m_Data + GetChannelOffset(shaderChannelIndex);
		return HasChannel (shaderChannelIndex) ? StrideIterator<T> (p, GetChannelStride (shaderChannelIndex)) : StrideIterator<T> (NULL, GetChannelStride (shaderChannelIndex));
	}
	
	template<class T>
	StrideIterator<T> MakeEndIterator (ShaderChannel shaderChannelIndex) const
	{
		T* end = GetEndPointer<T> (shaderChannelIndex);
		return StrideIterator<T> (end, GetChannelStride (shaderChannelIndex));
	}
	
	template<class T>
	T* GetEndPointer (ShaderChannel shaderChannelIndex) const
	{
		Assert (shaderChannelIndex < kShaderChannelCount);
		void* p = HasChannel (shaderChannelIndex) ? (m_Data + GetChannelOffset(shaderChannelIndex) + m_VertexCount * GetChannelStride (shaderChannelIndex)) : NULL;
		return reinterpret_cast<T*> (p);
	}

protected:
	ChannelInfoArray m_Channels;
	StreamInfoArray	m_Streams;

	size_t m_VertexSize; // must match m_CurrentChannels
	UInt8* m_Data;

	// The following are being serialized. Their size must match in both 32 and 64 bit platforms
	UInt32 m_CurrentChannels; // kShaderChannel bitmask
	UInt32 m_VertexCount;
	unsigned m_DataSize;
};


// Owns the vertex memory
class VertexData : public VertexDataInfo
{
public:
	
	DECLARE_SERIALIZE (VertexData)
	
	VertexData () : VertexDataInfo() { }
	VertexData (VertexData const& src, unsigned copyChannels, const VertexStreamsLayout& streams = kVertexStreamsDefault, const VertexChannelsLayout& channels = kVertexChannelsDefault);
	~VertexData ();
	
	static size_t GetAllocateDataSize (size_t accesibleBufferSize) { return accesibleBufferSize + kVertexDataPadding; }
	
	void Deallocate ();
	void Resize (size_t vertexCount, unsigned channelMask, const VertexStreamsLayout& streams = kVertexStreamsDefault, const VertexChannelsLayout& channels = kVertexChannelsDefault);
	void SwapEndianess ();
	
private:
	VertexData (const VertexData& o);
	void operator= (const VertexData& o);
	VertexData (const VertexDataInfo& o);
	void operator= (const VertexDataInfo& o);

#if SUPPORT_SERIALIZED_TYPETREES
	template<class TransferFunction>
	void TransferWorkaround35SerializationFuckup (TransferFunction& transfer);
#endif
};


void CopyVertexDataChannels (size_t vertexCount, unsigned copyChannels, const VertexData& srcData, VertexData& dstData);
size_t GetChannelFormatSize(UInt8 format);



#endif
