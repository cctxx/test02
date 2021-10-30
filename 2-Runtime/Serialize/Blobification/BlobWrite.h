#ifndef BLOBWRITE_H
#define BLOBWRITE_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Runtime/Animation/MecanimArraySerialization.h"
#include "offsetptr.h"
#include "BlobSize.h"
#include "ReduceCopyData.h"
#include <stack>
#include "Runtime/Modules/ExportModules.h"

class EXPORT_COREMODULE BlobWrite : public TransferBase
{
public:

	typedef dynamic_array<UInt8, 16> container_type;

private:

	container_type&          m_Blob;
	int			             m_TargetPlatform;
	bool                     m_CopyData;
	bool                     m_ReduceCopy;
	bool                     m_Use64BitOffsetPtr;
	bool                     m_SwapEndianess;
	
	struct TypeContext
	{
		TypeContext(size_t root, size_t offset, UInt8* srcDataPtr,size_t srcDataSize):m_Root(root),m_Offset(offset),m_SourceDataPtr(srcDataPtr),m_SourceDataSize(srcDataSize) {}

		size_t		m_Root;
		size_t		m_Offset;

		UInt8*      m_SourceDataPtr;
		size_t      m_SourceDataSize;
	};
	std::stack<TypeContext> m_Context;

	std::size_t	AlignAddress(std::size_t addr, std::size_t align)
	{
		return addr + ((~addr + 1U) & (align - 1U));
	}

	size_t GetActiveOffset () const
	{
		return m_Context.top().m_Root + m_Context.top().m_Offset;
	}
	
	UInt8* GetActiveBlobPtr ()
	{
		return &m_Blob[GetActiveOffset ()];
	}
	
	void WritePtrValueAtLocation (size_t locationInBlob, SInt64 value);

	void ValidateSerializedLayout (void* srcData, const char* name);
	
	void Push (size_t size, void* srcDataPtr, size_t align);

	void TransferPtrImpl (bool isValidPtr, ReduceCopyData* reduceCopyData, size_t alignOfT);
	void ReduceCopyImpl (const ReduceCopyData& reduce, size_t alignOfT);

	bool HasOffsetPtrWithDebugPtr () const;	
	bool Use64BitOffsetPtr() const { return m_Use64BitOffsetPtr; }
	bool AllowDataLayoutValidation () const;

public:

	BlobWrite (container_type& blob, TransferInstructionFlags flags, BuildTargetPlatform targetPlatform);

	void SetReduceCopy (bool reduce) { m_ReduceCopy = reduce; }
		
	bool IsWriting ()                 { return true; }
	bool IsWritingPPtr ()             { return true; }
	bool NeedsInstanceIDRemapping ()          { return m_Flags & kNeedsInstanceIDRemapping; }
	bool ConvertEndianess ()          { return m_SwapEndianess; }
	bool IsWritingGameReleaseData ()
	{ 	
		return true;
	}
	bool IsSerializingForGameRelease ()
	{
		return true;
	}
	bool IsBuildingTargetPlatform (BuildTargetPlatform platform)
	{
		if (platform == kBuildAnyPlayerData)
			return m_TargetPlatform >= kBuildValidPlayer;
		else
			return m_TargetPlatform == platform;
	}

	BuildTargetPlatform GetBuildingTargetPlatform () { return static_cast<BuildTargetPlatform>(m_TargetPlatform); }
		
	template<class T>	
	void Transfer (T& data, const char* name, TransferMetaFlags metaFlag = kNoTransferFlags);
	
	template<class T>	
	void TransferWithTypeString (T& data, const char* name, const char* typeName, TransferMetaFlags metaFlag = kNoTransferFlags);
	
	template<class T>
	void TransferBasicData (T& data);

	template<class T>	
	void TransferPtr (bool isValidPtr, ReduceCopyData* reduceCopyData);

	template<class T>	
	void ReduceCopy (const ReduceCopyData& reduce);
	
	template<class T>
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);
	
	template<class> friend class BlobWriteTransferSTLStyleArrayImpl;
};

template<typename T> class BlobWriteTransferSTLStyleArrayImpl
{
public:
	void operator()(T& data, TransferMetaFlags metaFlags, BlobWrite& transfer)
	{
		AssertString ("STL array are not support for BlobWrite");		
	}
};

template<typename T> class BlobWriteTransferSTLStyleArrayImpl< OffsetPtrArrayTransfer<T> > 
{
public:
	void operator()(OffsetPtrArrayTransfer<T>& data, TransferMetaFlags metaFlags, BlobWrite& transfer)
	{
		if (data.size() == 0)
		{
			Assert (!transfer.m_CopyData);
			return;
		}
		Assert (transfer.m_CopyData);
		
		size_t arrayByteSize = BlobSize::CalculateSize(*data.begin(), transfer.HasOffsetPtrWithDebugPtr(), transfer.Use64BitOffsetPtr()) * data.size();
		transfer.Push(arrayByteSize, &*data.begin(), ALIGN_OF( typename OffsetPtrArrayTransfer<T>::value_type ));
		
		typename OffsetPtrArrayTransfer<T>::iterator end = data.end ();
		for (typename OffsetPtrArrayTransfer<T>::iterator i = data.begin ();i != end;++i)
			transfer.Transfer (*i, "data");
		
		transfer.m_Context.pop();
	}
};

template<typename T, int SIZE> class BlobWriteTransferSTLStyleArrayImpl< StaticArrayTransfer<T, SIZE> >
{
public:
	void operator()(StaticArrayTransfer<T, SIZE>& data, TransferMetaFlags metaFlags, BlobWrite& transfer)
	{
		typename StaticArrayTransfer<T, SIZE>::iterator end = data.end ();
		for (typename StaticArrayTransfer<T, SIZE>::iterator i = data.begin ();i != end;++i)
			transfer.Transfer (*i, "data");
	}
};

template<class T> inline
void BlobWrite::TransferSTLStyleArray (T& data, TransferMetaFlags metaFlags)
{
	BlobWriteTransferSTLStyleArrayImpl<T> transfer;
	transfer(data, metaFlags, *this);
}

template<class T> inline
void BlobWrite::Transfer (T& data, const char* name, TransferMetaFlags)
{
	bool copyData = m_CopyData;
	if (m_CopyData)
		Push(BlobSize::CalculateSize(data, HasOffsetPtrWithDebugPtr(), Use64BitOffsetPtr()), &data, SerializeTraits<T>::GetAlignOf() );

	// Follow natural c++ alignment
	size_t head = m_Context.top().m_Root;
	size_t& offset = m_Context.top().m_Offset;
	// always align head + offset not only offset otherwise you may get wrong align for nested data structure
	offset = AlignAddress(head + offset, SerializeTraits<T>::GetAlignOf()) - head;	
	
	ValidateSerializedLayout(&data, name);
	
	SerializeTraits<T>::Transfer (data, *this);

	if (copyData)
		m_Context.pop();
}

template<class T> inline
void BlobWrite::TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<class T> inline
void BlobWrite::TransferBasicData (T& srcData)
{
	Assert(m_Blob.size() >= GetActiveOffset() + sizeof(T));

	// Write basic data into blob & endianswap
	UInt8* blobPtr = GetActiveBlobPtr();
	memcpy(blobPtr, &srcData, sizeof(T));
	if (m_SwapEndianess)
		SwapEndianBytes(*reinterpret_cast<T*> (blobPtr));
	
	m_Context.top().m_Offset += sizeof(T);	
}

template<class T> inline
void BlobWrite::TransferPtr (bool isValidPtr, ReduceCopyData* reduceCopyData)
{
	TransferPtrImpl (isValidPtr, reduceCopyData, ALIGN_OF(T));
}

template<class T> inline
void BlobWrite::ReduceCopy (const ReduceCopyData& reduce)
{
	ReduceCopyImpl(reduce, ALIGN_OF(T));
}

#endif
