#ifndef STREAMEDBINARYREAD_H
#define STREAMEDBINARYREAD_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Serialize/SwapEndianBytes.h"


template<bool kSwapEndianess>
class StreamedBinaryRead : public TransferBase
{
	CachedReader	m_Cache;

	friend class MonoBehaviour;

public:

	CachedReader& Init (int flags)	{ m_UserData = NULL; m_Flags = flags; return m_Cache; }

	bool IsReading ()                 { return true; }
	bool IsReadingPPtr ()             { return true; }
	bool NeedsInstanceIDRemapping ()          { return m_Flags & kNeedsInstanceIDRemapping; }
	bool ConvertEndianess ()          { return kSwapEndianess; }

	bool DidReadLastProperty ()       { return true; }
	bool DidReadLastPPtrProperty ()   { return true; }

	void EnableResourceImage (ActiveResourceImage targetResourceImage) { m_Cache.BeginResourceImage(targetResourceImage); }
	bool ReadStreamingInfo(StreamingInfo* streamingInfo);

	bool ShouldChannelOverride ();
	CachedReader& GetCachedReader () { return m_Cache; }

	const char* GetSerializedFilePathName() { return m_Cache.GetSerializedFilePathName(); }

	template<class T>
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	template<class T>
	void TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag = kNoTransferFlags);

	void TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);

	// markerID is the id that was given by TransferTypeless.
	// optional copyData: is a pointer to where the data will be written or read from
	void TransferTypelessData (unsigned byteSize, void* copyData, int metaData = 0);

	/// Reads byteSize bytes into data. This may onle be used if UseOptimizedReading returns true.
	void EXPORT_COREMODULE ReadDirect (void* data, int byteSize);

	void EXPORT_COREMODULE Align ();

	template<class T>
	void TransferBasicData (T& data);

	template<class T>
	void TransferPtr (bool, ReduceCopyData*){}

	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);
};

template <bool kSwapEndianess>
bool StreamedBinaryRead<kSwapEndianess>::ReadStreamingInfo(StreamingInfo* streamingInfo)
{
	Assert(streamingInfo != NULL);

	if (!m_Cache.IsReadingResourceImage())
		return false;

	// Read the size & offset values from the serialized file
	// The size & offset describes where the data is in the streamed file
	UInt32 offset, size;
	Transfer (size, "ri_size");
	Transfer (offset, "ri_offset");

	m_Cache.GetStreamingInfo (offset, size, streamingInfo);
	return true;
}



template<bool kSwapEndianess>
template<class T>
void StreamedBinaryRead<kSwapEndianess>::TransferSTLStyleArray (T& data, TransferMetaFlags /*metaFlags*/)
{
	if (m_Cache.IsReadingResourceImage())
	{
		// Read the size & offset from the serialized file
		UInt32 offset, size;
		Transfer (size, "ri_size");
		Transfer (offset, "ri_offset");

		// Fetch the pointer from the pre-loaded resource image.
		unsigned bufferSize = sizeof (typename T::value_type) * size;
		UInt8* buffer = m_Cache.FetchResourceImageData (offset, bufferSize);
		SerializeTraits<T>::resource_image_assign_external (data, buffer, buffer + bufferSize);

		m_Cache.EndResourceImage();
	}
	else
	{
		SInt32 size;
		Transfer (size, "size");

		SerializeTraits<T>::ResizeSTLStyleArray (data, size);

		if (!kSwapEndianess && SerializeTraits<typename T::value_type>::AllowTransferOptimization () && SerializeTraits<T>::IsContinousMemoryArray ())
		{
			//AssertIf (size != distance (data.begin (), data.end ()));
			if( size != 0 )
				ReadDirect (&*data.begin (), size * sizeof (typename T::value_type));
		}
		else
		{
			typename T::iterator i;
			typename T::iterator end = data.end ();
			//AssertIf (size != distance (data.begin (), end));
			for (i = data.begin ();i != end;++i)
				Transfer (*i, "data");
		}
	}
}

template<bool kSwapEndianess>
template<class T>
void StreamedBinaryRead<kSwapEndianess>::TransferSTLStyleMap (T& data, TransferMetaFlags)
{
	SInt32 size;
	Transfer (size, "size");

	// maps value_type is: pair<const First, Second>
	// So we have to write to maps non-const value type
	typename NonConstContainerValueType<T>::value_type p;

	data.clear ();
	for (int i=0;i<size;i++)
	{
		Transfer (p, "data");
		data.insert (p);
	}
}

template<bool kSwapEndianess>
template<class T>
void StreamedBinaryRead<kSwapEndianess>::Transfer (T& data, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<bool kSwapEndianess>
template<class T>
void StreamedBinaryRead<kSwapEndianess>::TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryRead<kSwapEndianess>::TransferBasicData (T& data)
{
	AssertIf (sizeof (T) > 8);
	m_Cache.Read (data);
	if (kSwapEndianess)
	{
		SwapEndianBytes (data);
	}
}
#endif



