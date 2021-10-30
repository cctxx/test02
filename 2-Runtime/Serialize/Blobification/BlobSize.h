#ifndef BLOBSIZE_H
#define BLOBSIZE_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"

struct ReduceCopyData;

class BlobSize : public TransferBase
{
private:

	typedef OffsetPtr<void*>::offset_type offset_type;

	size_t                   m_Size;
	bool                     m_IgnorePtr;
	bool                     m_HasDebugOffsetPtr;
	bool                     m_Use64Ptr;
	int			             m_TargetPlatform;

	std::size_t	AlignAddress(std::size_t addr, std::size_t align)
	{
		return addr + ((~addr + 1U) & (align - 1U));
	}
	
public:

	
	BlobSize (bool hasDebugOffsetPtr, bool use64BitOffsetPtr)
		: m_Size (0)
		, m_IgnorePtr (false)
		, m_HasDebugOffsetPtr (hasDebugOffsetPtr)
		, m_Use64Ptr (use64BitOffsetPtr) { }

	bool NeedsInstanceIDRemapping ()          { return m_Flags & kNeedsInstanceIDRemapping; }
	int  GetFlags ()                  { return m_Flags; }
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
	void TransferSTLStyleArray (T& data, TransferMetaFlags metaFlag = kNoTransferFlags);
		
	template<class T>
	static size_t CalculateSize (T& data, bool hasDebugOffsetPtr, bool use64BitOffsetPtr)
	{
		BlobSize sizer (hasDebugOffsetPtr, use64BitOffsetPtr);
		sizer.Transfer(data, "Base");
		
		// size_t size = sizeof(T);
		// Assert( sizeof(T) == sizer.m_Size);
		
		return sizer.m_Size;
	}

	template<class> friend class BlobSizeTransferSTLStyleArrayImpl;
};


template<typename T> class BlobSizeTransferSTLStyleArrayImpl
{
public:
	void operator()(T& data, TransferMetaFlags metaFlags, BlobSize& transfer)
	{
		AssertString ("STL array are not support for BlobWrite");		
	}
};

template<typename T> class BlobSizeTransferSTLStyleArrayImpl< OffsetPtrArrayTransfer<T> > 
{
public:
	void operator()(OffsetPtrArrayTransfer<T>& data, TransferMetaFlags metaFlags, BlobSize& transfer)
	{
		transfer.m_IgnorePtr = false;
	}
};

template<typename T, int SIZE> class BlobSizeTransferSTLStyleArrayImpl< StaticArrayTransfer<T, SIZE> >
{
public:
	void operator()(StaticArrayTransfer<T, SIZE>& data, TransferMetaFlags metaFlags, BlobSize& transfer)
	{
		transfer.m_Size = transfer.AlignAddress(transfer.m_Size, ALIGN_OF(T));
		transfer.m_Size += sizeof(T)*data.size();
	}
};


template<class T> inline
void BlobSize::TransferSTLStyleArray (T& data, TransferMetaFlags metaFlags)
{
	BlobSizeTransferSTLStyleArrayImpl<T> transfer;
	transfer(data, metaFlags, *this);
}

template<class T> inline
void BlobSize::Transfer (T& data, const char* name, TransferMetaFlags)
{
	if (m_IgnorePtr)
	{
		m_IgnorePtr = false;
		return;
	}

	m_Size = AlignAddress(m_Size, SerializeTraits<T>::GetAlignOf() );	
	
	SerializeTraits<T>::Transfer (data, *this);
	
	m_Size = AlignAddress(m_Size, SerializeTraits<T>::GetAlignOf() );
}

template<class T> inline
void BlobSize::TransferWithTypeString (T& data, const char*, const char*, TransferMetaFlags)
{
	SerializeTraits<T>::Transfer (data, *this);
}

template<class T> inline
void BlobSize::TransferBasicData (T& srcData)
{
	m_Size += sizeof(T);	
}

template<class T> inline
void BlobSize::TransferPtr (bool isValidPtr, ReduceCopyData* reduceCopyData)
{
	m_Size += m_Use64Ptr ? sizeof(SInt64) : sizeof(SInt32);
	
	if (m_HasDebugOffsetPtr)
		m_Size += sizeof(void*);
	
	if (isValidPtr)
		m_IgnorePtr = true;
}

#endif
