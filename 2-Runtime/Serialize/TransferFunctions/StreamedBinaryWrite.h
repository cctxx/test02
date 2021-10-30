#ifndef STREAMEDBINARYWRITE_H
#define STREAMEDBINARYWRITE_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Runtime/Serialize/SwapEndianBytes.h"

template<bool kSwapEndianess>
class EXPORT_COREMODULE StreamedBinaryWrite : public TransferBase
{
	CachedWriter m_Cache;
	BuildTargetSelection m_Target;

	#if UNITY_EDITOR
	BuildUsageTag m_BuildUsageTag;
	#endif
	friend class MonoBehaviour;

public:

	CachedWriter& Init (int flags, BuildTargetSelection target);
	CachedWriter& Init (const CachedWriter& cachedWriter, int flags, BuildTargetSelection target, const BuildUsageTag& buildUsageTag);

	bool IsWriting ()                 { return true; }
	bool IsWritingPPtr ()             { return true; }
	bool NeedsInstanceIDRemapping ()          { return m_Flags & kNeedsInstanceIDRemapping; }
	bool ConvertEndianess ()          { return kSwapEndianess; }
	bool IsWritingGameReleaseData ()
	{
		return IsSerializingForGameRelease ();
	}
	bool IsBuildingTargetPlatform (BuildTargetPlatform platform)
	{
		#if UNITY_EDITOR
		if (platform == kBuildAnyPlayerData)
			return m_Target.platform >= kBuildValidPlayer;
		else
			return m_Target.platform == platform;
		#else
		return false;
		#endif
	}

	#if UNITY_EDITOR
	BuildUsageTag GetBuildUsage ()
	{
		return m_BuildUsageTag;
	}
	#endif

	BuildTargetSelection GetBuildingTarget () { return m_Target; }

	template<class T>
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag = kNoTransferFlags);

	void EnableResourceImage (ActiveResourceImage targetResourceImage)
	{
		#if UNITY_EDITOR
		m_Cache.BeginResourceImage (targetResourceImage);
		#endif
	}

	/// In order to transfer typeless data (Read: transfer data real fast)
	/// Call TransferTypeless. You have to always do this. Even for a proxytransfer. Then when you want to access the datablock.
	/// Call TransferTypelessData
	/// On return:
	/// When reading bytesize will contain the size of the data block that should be read,
	/// when writing bytesize has to contain the size of the datablock.
	/// MarkerID will contain an marker which you have to give TransferTypelessData when you want to start the actual transfer.
	/// optional: A serializedFile will be seperated into two chunks. One is the normal object data. (It is assumed that they are all relatively small)
	/// So caching them makes a lot of sense. Big datachunks will be stored in another part of the file.
	/// They will not be cached but usually read directly into the allocated memory, probably reading them asynchronously
	void TransferTypeless (unsigned* byteSize, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);

	// markerID is the id that was given by TransferTypeless.
	// copyData is a pointer to where the data will be written or read from
	void TransferTypelessData (unsigned byteSize, void* copyData, int metaFlag = 0);

	bool GetTransferFileInfo(unsigned* position, const char** filePath) const;

	template<class T>
	void TransferBasicData (T& data);

	template<class T>
	void TransferPtr (bool, ReduceCopyData*){}

	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	template<class T>
	void TransferSTLStyleMap (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);

	void Align ();

	CachedWriter& GetCachedWriter() { return m_Cache; }
};



template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryWrite<kSwapEndianess>::TransferSTLStyleArray (T& data, TransferMetaFlags /*metaFlags*/)
{
	#if UNITY_EDITOR
	if (m_Cache.IsWritingResourceImage())
	{
		// Grab the offset where the resourceImage is currently at
		UInt32 offsetInResourceImage = m_Cache.GetPosition();

		// Write the actual data to the resource image
		typename T::iterator end = data.end ();
		for (typename T::iterator i = data.begin ();i != end;++i)
			Transfer (*i, "data");

		Assert (m_Cache.IsWritingResourceImage());
		m_Cache.EndResourceImage ();
		Assert (!m_Cache.IsWritingResourceImage());

		UInt32 size = data.size ();

		// Writ ethe size & offset to the serialized file
		Transfer (size, "ri_size");
		Transfer (offsetInResourceImage, "ri_offset");
	}
	else
	#endif
	{
		SInt32 size = data.size ();
		Transfer (size, "size");
		typename T::iterator end = data.end ();
		for (typename T::iterator i = data.begin ();i != end;++i)
			Transfer (*i, "data");
	}
}


template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryWrite<kSwapEndianess>::TransferSTLStyleMap (T& data, TransferMetaFlags)
{
	SInt32 size = data.size ();
	Transfer (size, "size");

	// maps value_type is: pair<const First, Second>
	// So we have to write to maps non-const value type
	typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;

	typename T::iterator end = data.end ();
	for (typename T::iterator i = data.begin ();i != end;++i)
	{
		non_const_value_type& p = (non_const_value_type&)(*i);
		Transfer (p, "data");
	}
}

template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryWrite<kSwapEndianess>::Transfer (T& data, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryWrite<kSwapEndianess>::TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<bool kSwapEndianess>
template<class T> inline
void StreamedBinaryWrite<kSwapEndianess>::TransferBasicData (T& data)
{
	if (kSwapEndianess)
	{
		T temp = data;
		SwapEndianBytes (temp);
		m_Cache.Write (temp);
	}
	else
		m_Cache.Write (data);
}

#endif
