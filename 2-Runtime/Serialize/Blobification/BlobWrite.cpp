#include "UnityPrefix.h"
#include "BlobWrite.h"
#include "Configuration/UnityConfigure.h"
#include "BlobWriteTargetSupport.h"

BlobWrite::BlobWrite (container_type& blob, TransferInstructionFlags flags, BuildTargetPlatform targetPlatform)
:	m_Blob(blob),
	m_CopyData(true),
	m_ReduceCopy(false),
	m_TargetPlatform(targetPlatform)
{
	m_Flags = false;
	m_SwapEndianess = m_Flags & kSwapEndianess;
	m_Use64BitOffsetPtr = IsBuildTarget64BitBlob (targetPlatform);
}

void BlobWrite::Push (size_t size, void* srcDataPtr, size_t align)
{
	Assert (m_CopyData);
	
	size_t offset = AlignAddress(m_Blob.size(), align);
	m_Context.push( TypeContext(offset, 0, reinterpret_cast<UInt8*> (srcDataPtr), size) );
	m_Blob.resize_initialized(offset + size, 0);
	m_CopyData = false;
}

void BlobWrite::WritePtrValueAtLocation (size_t locationInBlob, SInt64 value)
{
	if (m_Use64BitOffsetPtr)
	{
		SInt64 offset64 = value;
		
		if (m_SwapEndianess)
			SwapEndianBytes(offset64);
		memcpy (&m_Blob[locationInBlob], &offset64, sizeof(offset64));
	}
	else
	{
		SInt32 offset32 = value;
		
		if (m_SwapEndianess)
			SwapEndianBytes(offset32);
		memcpy (&m_Blob[locationInBlob], &offset32, sizeof(offset32));
	}
}


void BlobWrite::TransferPtrImpl (bool isValidPtr, ReduceCopyData* reduceCopyData, size_t alignOfT)
{
	Assert(!m_CopyData);
	// When the data is null we will not call Transfer.
	m_CopyData = isValidPtr;
	
	// Need to update OffsetPtr's member 'mOffset'
	// compute member offset in memory buffer
	size_t dataPosition = AlignAddress(m_Blob.size(), alignOfT);
	size_t offset = GetActiveOffset();
	offset = dataPosition - offset;
	if (!isValidPtr)
		offset = 0;
	
	// Write the ptr
	WritePtrValueAtLocation(GetActiveOffset (), offset);
	
	// Setup reduce copy data for later use by ReduceCopyImpl
	if (reduceCopyData != NULL)
	{
		if (isValidPtr)
		{
			reduceCopyData->ptrPosition = GetActiveOffset();
			reduceCopyData->dataStart = dataPosition;
			reduceCopyData->blobSize = m_Blob.size();
		}
		else
		{
			reduceCopyData->ptrPosition = 0xFFFFF;
			reduceCopyData->dataStart = 0xFFFFF;
			reduceCopyData->blobSize = 0xFFFFF;
		}
	}
	
	
	// Offset write location in the blob
	m_Context.top().m_Offset += m_Use64BitOffsetPtr ? sizeof(SInt64) : sizeof(SInt32);
	if (HasOffsetPtrWithDebugPtr())
		m_Context.top().m_Offset += sizeof(void*);
}

bool BlobWrite::HasOffsetPtrWithDebugPtr () const
{
	return m_TargetPlatform == kBuildNoTargetPlatform;
}

bool BlobWrite::AllowDataLayoutValidation () const
{
	size_t targetOffsetPtrSize = Use64BitOffsetPtr () ? sizeof(SInt64) : sizeof(SInt32);
	if (HasOffsetPtrWithDebugPtr ())
		targetOffsetPtrSize += sizeof(void*);
	
	size_t srcOffsetPtrSize = sizeof(OffsetPtr<UInt8>);

	return targetOffsetPtrSize == srcOffsetPtrSize;
}

// Ensure that the user has matching Transfer calls & Data layout in the struct
void BlobWrite::ValidateSerializedLayout (void* srcData, const char* name)
{
	UInt8* srcDataPtr = reinterpret_cast<UInt8*> (srcData);
	
	// (float4 and some others transfer functions, transfer temporary data, we ignore layout checks on those and hope for the best)
	int srcDataOffset = srcDataPtr - m_Context.top().m_SourceDataPtr;
	if (srcDataOffset < 0 || srcDataOffset >= m_Context.top().m_SourceDataSize)
		return;
	
	// When targeting a platform with a different layout than our own, obviously these checks dont make sense...
	if (!AllowDataLayoutValidation ())
		return;
	
	int blobOffset = m_Context.top().m_Offset;
	if (srcDataOffset != blobOffset)
	{
		AssertString(Format("BlobWrite: Transfer '%s' is not called in the same order as the struct is laid out. Expected: %d got: %d ", name, srcDataOffset, blobOffset));
	}
}

void BlobWrite::ReduceCopyImpl (const ReduceCopyData& reduce, size_t alignOfT)
{
	if (!m_ReduceCopy || reduce.dataStart == 0xFFFFF)
		return;
	
	// Find any data in the blob that matches the last written data.
	// if we find it, delete it again and reference the previous memory block instead
	size_t dataSize = m_Blob.size() - reduce.dataStart;
	for (int i=0;i < reduce.dataStart;i+=alignOfT)
	{
		if (memcmp(&m_Blob[i], &m_Blob[reduce.dataStart], dataSize) == 0)
		{
			// Update offset pointer
			SInt64 offset = i - reduce.ptrPosition;
			WritePtrValueAtLocation (reduce.ptrPosition, offset);
			
			// resize blob based on the reduce copy
			m_Blob.resize_initialized(reduce.blobSize, 0);
			
			return;
		}
	}
}
