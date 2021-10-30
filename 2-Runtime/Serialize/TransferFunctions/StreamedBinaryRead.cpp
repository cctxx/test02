#include "UnityPrefix.h"
#include "StreamedBinaryRead.h"

template <bool kSwapEndianess>
void StreamedBinaryRead<kSwapEndianess>::Align ()
{
	m_Cache.Align4Read();
}

template <bool kSwapEndianess>
void StreamedBinaryRead<kSwapEndianess>::TransferTypeless (unsigned* byteSize, const char*/* name*/, TransferMetaFlags/* metaFlag*/)
{
	SInt32 size;
	Transfer (size, "size");
	*byteSize = size;
}

// markerID is the id that was given by TransferTypeless.
// optional copyData: is a pointer to where the data will be written or read from
template <bool kSwapEndianess>
void StreamedBinaryRead<kSwapEndianess>::TransferTypelessData (unsigned byteSize, void* copyData, int metaData)
{
	if (byteSize == 0) 
		return;
	
	if (copyData == NULL)
	{
		// seek byte
		m_Cache.Skip(byteSize);		
	}
	else
		m_Cache.Read (copyData, byteSize);
	Align();
}

template <bool kSwapEndianess>
void StreamedBinaryRead<kSwapEndianess>::ReadDirect (void* data, int byteSize)
{
	AssertIf (kSwapEndianess);
	m_Cache.Read (data, byteSize);
}


template void StreamedBinaryRead<true>::ReadDirect (void* data, int byteSize);
template void StreamedBinaryRead<false>::ReadDirect (void* data, int byteSize);

template void StreamedBinaryRead<true>::Align();
template void StreamedBinaryRead<false>::Align();

template void StreamedBinaryRead<true>::TransferTypelessData (unsigned byteSize, void* copyData, int metaData);
template void StreamedBinaryRead<false>::TransferTypelessData (unsigned byteSize, void* copyData, int metaData);

template void StreamedBinaryRead<true>::TransferTypeless (unsigned* byteSize, const char*/* name*/, TransferMetaFlags/* metaFlag*/);
template void StreamedBinaryRead<false>::TransferTypeless (unsigned* byteSize, const char*/* name*/, TransferMetaFlags/* metaFlag*/);
