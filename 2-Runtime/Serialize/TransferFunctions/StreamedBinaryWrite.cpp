#include "UnityPrefix.h"
#include "StreamedBinaryWrite.h"
#include "Configuration/UnityConfigure.h"

template <bool kSwapEndianess>
CachedWriter& StreamedBinaryWrite<kSwapEndianess>::Init (int flags, BuildTargetSelection target)
{
	m_Flags = flags;
	m_UserData = NULL;
	m_Target = target;

#if UNITY_EDITOR && CHECK_SERIALIZE_ALIGNMENT
	m_Cache.SetCheckSerializeAlignment(true);
	#endif
	return m_Cache;
}

template <bool kSwapEndianess>
CachedWriter& StreamedBinaryWrite<kSwapEndianess>::Init (const CachedWriter& cachedWriter, int flags, BuildTargetSelection target, const BuildUsageTag& buildUsageTag)
{
	m_Flags = flags;
	m_Target = target;
	m_Cache = cachedWriter;
	m_UserData = NULL;

	#if UNITY_EDITOR
	m_BuildUsageTag = buildUsageTag;
	#endif

#if UNITY_EDITOR && CHECK_SERIALIZE_ALIGNMENT
	m_Cache.SetCheckSerializeAlignment(true);
#endif
	return m_Cache;
}

template <bool kSwapEndianess>
void StreamedBinaryWrite<kSwapEndianess>::Align ()
{
	m_Cache.Align4Write();
}


template <bool kSwapEndianess>
void StreamedBinaryWrite<kSwapEndianess>::TransferTypeless (unsigned* byteSize, const char* /* name*/, TransferMetaFlags/* metaFlag*/)
{
	SInt32 size = *byteSize;
	Transfer (size, "size");
}

// markerID is the id that was given by TransferTypeless.
// byteStart is
// optional temporaryDataHandle: temporaryDataHandle is a handle to the data
// optional copyData: is a pointer to where the data will be written or read from
template <bool kSwapEndianess>
void StreamedBinaryWrite<kSwapEndianess>::TransferTypelessData (unsigned byteSize, void* copyData, int/* metaData*/)
{
	AssertIf(copyData == NULL && byteSize != 0);
	m_Cache.Write (copyData, byteSize);
	Align();
}


template CachedWriter& StreamedBinaryWrite<false>::Init (int flags, BuildTargetSelection target);
template CachedWriter& StreamedBinaryWrite<true>::Init (int flags, BuildTargetSelection target);

template CachedWriter& StreamedBinaryWrite<false>::Init (const CachedWriter& cachedWriter, int flags, BuildTargetSelection target, const BuildUsageTag& buildUsageTag);
template CachedWriter& StreamedBinaryWrite<true>::Init (const CachedWriter& cachedWriter, int flags, BuildTargetSelection target, const BuildUsageTag& buildUsageTag);

template void StreamedBinaryWrite<false>::Align ();
template void StreamedBinaryWrite<true>::Align ();

template void StreamedBinaryWrite<false>::TransferTypeless (unsigned* byteSize, const char*/* name*/, TransferMetaFlags/* metaFlag*/);
template void StreamedBinaryWrite<true>::TransferTypeless (unsigned* byteSize, const char*/* name*/, TransferMetaFlags/* metaFlag*/);

template void StreamedBinaryWrite<false>::TransferTypelessData (unsigned byteSize, void* copyData, int metaData);
template void StreamedBinaryWrite<true>::TransferTypelessData (unsigned byteSize, void* copyData, int metaData);
