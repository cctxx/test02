#pragma once

#include "Runtime/mecanim/memory.h"
#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "Runtime/Serialize/SerializeTraitsBase.h"
#include "Runtime/Serialize/TransferFunctionFwd.h"
#include "Runtime/Utilities/TypeUtilities.h"
#include "ReduceCopyData.h"

template<typename TYPE>
class OffsetPtr
{
public:
	typedef TYPE		value_type;
	typedef TYPE*		ptr_type;
	typedef TYPE const*	const_ptr_type;
	typedef TYPE&		reference_type;
	typedef TYPE const&	const_reference_type;
	typedef size_t		offset_type;

	OffsetPtr():m_Offset(0),m_DebugPtr(0)
	{		
	}

	OffsetPtr (const OffsetPtr<value_type>& ptr):m_Offset(ptr.m_Offset)
	{		
	}

	OffsetPtr& operator = (const OffsetPtr<value_type>& ptr)
	{
		m_Offset = ptr.m_Offset; 
		return *this;
	}

	void reset(ptr_type ptr)
	{
		m_Offset = ptr != 0 ? reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(this) : 0;
#ifdef UNITY_EDITOR
		m_DebugPtr = ptr;
#endif
	}

	OffsetPtr& operator = (const ptr_type ptr)
	{ 
		reset (ptr); 
		return *this; 
	}

	ptr_type operator->()
	{ 
		ptr_type ptr = reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
#ifdef UNITY_EDITOR
		m_DebugPtr = ptr;
#endif
		return ptr; 
	}
	const_ptr_type operator->()const
	{ 
		return reinterpret_cast<const_ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset); 
	}

	reference_type operator*()
	{ 
		ptr_type ptr = reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
#ifdef UNITY_EDITOR
		m_DebugPtr = ptr;
#endif
		return *ptr;
	}
	
	const_reference_type operator*()const
	{ 
		return *reinterpret_cast<const_ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset); 
	}

	value_type& operator[](std::size_t i ) 
	{ 
		assert(i != std::numeric_limits<std::size_t>::max());
		ptr_type ptr = reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
#ifdef UNITY_EDITOR
		m_DebugPtr = ptr;		
#endif		
		return ptr[i];
	}

	value_type const& operator[](std::size_t i ) const
	{ 
		assert(i != std::numeric_limits<std::size_t>::max());
		const_ptr_type ptr = reinterpret_cast<const_ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);		
		return ptr[i];
	}

	bool IsNull()const 
	{
		return m_Offset == 0;
	}

	ptr_type Get()
	{
#ifdef UNITY_EDITOR
		m_DebugPtr = reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
#endif
		// TODO: serialize trait for offset ptr call begin and end which call OffsetPtr::Get
		//Assert(!IsNull());
		return reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
	}

	const_ptr_type Get()const
	{
#ifdef UNITY_EDITOR
		m_DebugPtr = reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
#endif
		// TODO: serialize trait for offset ptr call begin and end which call OffsetPtr::Get
		//Assert(!IsNull());
		return reinterpret_cast<ptr_type>(reinterpret_cast<std::size_t>(this) + m_Offset);
	}
	
	
	size_t get_size () const 
	{ 
		return sizeof(TYPE); 
	}
	
protected:
	offset_type			m_Offset;
#ifdef UNITY_EDITOR
	mutable ptr_type	m_DebugPtr;
#endif
};


template<typename TYPE>
class SerializeTraits< OffsetPtr<TYPE> > : public SerializeTraitsBase< OffsetPtr<TYPE> >
{
	public:

	typedef OffsetPtr<TYPE>	value_type;
	inline static const char* GetTypeString (void*)	{ return "OffsetPtr"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return true; }
	inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		if(IsSameType<TransferFunction, BlobWrite>::result)
		{
			ReduceCopyData reduce;
			transfer.template TransferPtr<TYPE>(!data.IsNull(), &reduce);
			if (!data.IsNull())
			{
				transfer.Transfer(*data, "data");
			}
			transfer.template ReduceCopy<TYPE> (reduce);
		}
		else if(transfer.IsReading () || transfer.IsWriting ())
		{
			bool isNull = data.IsNull();

			transfer.template TransferPtr<TYPE>(true, NULL);
			if (isNull)
			{
				mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (transfer.GetUserData());
				data = allocator->Construct<TYPE>();
			}	

			transfer.Transfer(*data, "data");
		}
		else if(IsSameType<TransferFunction, BlobSize>::result)
		{
			transfer.template TransferPtr<TYPE>(false, NULL);
		}
		// Support for ProxyTransfer
		else
		{
			transfer.template TransferPtr<TYPE>(false, NULL);

			TYPE p;
			transfer.Transfer(p, "data");
		}
	}
};

template<class T>
struct OffsetPtrArrayTransfer
{
	typedef T* iterator;
	typedef T  value_type;
	
	OffsetPtr<T>&   m_Data;
	UInt32&			m_ArraySize;
	void*			m_Allocator;
	bool            m_ClearPtrs;
	
	OffsetPtrArrayTransfer (OffsetPtr<T>& data, UInt32& size, void* allocator, bool clearPtrs)
	: m_Data(data),m_ArraySize(size)
	{
		m_Allocator = allocator;
		m_ClearPtrs = clearPtrs;
	}
	
	T* begin ()         { return m_Data.Get(); }
	T* end ()           { return m_Data.Get() + m_ArraySize; }
	size_t size()       { return m_ArraySize; }
	
	void resize (int newSize)
	{	
		m_ArraySize = newSize;
		
		mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (m_Allocator);
		Assert(allocator != NULL);
		
		if (newSize != 0)
		{
			m_Data = allocator->ConstructArray<value_type> (newSize);
			if (m_ClearPtrs)
				memset(begin(), 0, sizeof(value_type) * newSize);
		}
		else
			m_Data = NULL;
	}
};

template<class T>
class SerializeTraits<OffsetPtrArrayTransfer<T> > : public SerializeTraitsBase<OffsetPtrArrayTransfer<T> >
{
public:
	
	typedef OffsetPtrArrayTransfer<T>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		ReduceCopyData reduceCopy;
		transfer.template TransferPtr<typename value_type::value_type>(transfer.IsReading() || transfer.IsWriting() ? data.m_ArraySize != 0 : false, &reduceCopy);
		
		transfer.TransferSTLStyleArray (data);
		transfer.template ReduceCopy<typename value_type::value_type>(reduceCopy);
	}
	
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)
	{
		data.resize(rs);
	}
	
	static void resource_image_assign_external (value_type& data, void* begin, void* end)
	{
	}
};


#define MANUAL_ARRAY_TRANSFER2(TYPE,DATA,SIZE) 	OffsetPtrArrayTransfer<TYPE> DATA##ArrayTransfer (DATA, SIZE, transfer.GetUserData(), false); transfer.Transfer(DATA##ArrayTransfer, #DATA);
#define TRANSFER_BLOB_ONLY(DATA) 				if (IsSameType<TransferFunction, BlobWrite>::result || IsSameType<TransferFunction, BlobSize>::result)  transfer.Transfer(DATA, #DATA);

