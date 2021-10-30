#pragma once

#include "Runtime/mecanim/memory.h"

template<typename T, int SIZE>
struct StaticArrayTransfer
{
	enum
	{
		m_ArraySize = SIZE
	};

	size_t m_Size;

	typedef T* iterator;
	typedef T  value_type;

	T (&m_Data)[SIZE];
	
	StaticArrayTransfer (T (&data)[SIZE]):m_Data(data),m_Size(m_ArraySize)
	{	
	}

	void reserve(size_t size) 
	{ 
		m_Size = std::min<size_t>(size, m_ArraySize); 
	}
	
	iterator begin ()   { return &m_Data[0]; }
	iterator end ()     { return &m_Data[m_Size]; }
	size_t size()       { return m_Size; }
};

template<typename T, int SIZE>
class SerializeTraits< StaticArrayTransfer<T, SIZE> > : public SerializeTraitsBase< StaticArrayTransfer<T, SIZE> >
{
public:

	DEFINE_GET_TYPESTRING_CONTAINER (staticvector)
	
	typedef StaticArrayTransfer<T, SIZE>	value_type;
	
	static size_t GetAlignOf()	{return ALIGN_OF(T);}
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}
	
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)
	{
		data.reserve(rs);
	}
	
	static void resource_image_assign_external (value_type& data, void* begin, void* end)
	{
	}
};

#define STATIC_ARRAY_TRANSFER(TYPE,DATA,SIZE) 	StaticArrayTransfer<TYPE, SIZE> DATA##ArrayTransfer (DATA); transfer.Transfer(DATA##ArrayTransfer, #DATA);


template<class T, class TransferFunction>
struct ManualArrayTransfer
{
	typedef T* iterator;
	typedef T  value_type;

	T**					m_Data;
	mecanim::uint32_t*  m_ArraySize;
	void*				m_Allocator;
	TransferFunction&   m_Transfer;
	
	ManualArrayTransfer (T*& data, mecanim::uint32_t& size, void* allocator, TransferFunction& transfer):m_Transfer(transfer)
	{
		m_Allocator = allocator;
		m_Data = &data;
		m_ArraySize = &size;
	}
	
	T* begin ()         { return *m_Data; }
	T* end ()           { return *m_Data + *m_ArraySize; }
	size_t size()       { return *m_ArraySize; }

	void resize (int size)
	{
		if(m_Transfer.IsReading() || m_Transfer.IsWriting() || m_Transfer.IsRemapPPtrTransfer())
		{
			mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (m_Allocator);
			Assert(allocator != NULL);
		
			*m_Data = allocator->ConstructArray<T> (size);
			*m_ArraySize = size;
		}
	}
};

template<class T, class TransferFunction>
struct ManualArrayTransfer<T*, TransferFunction>
{
	typedef T** iterator;
	typedef T*  value_type;

	value_type**		m_Data;
	mecanim::uint32_t*  m_ArraySize;
	void*				m_Allocator;
	TransferFunction&   m_Transfer;
	
	ManualArrayTransfer (value_type *& data, mecanim::uint32_t& size, void* allocator, TransferFunction& transfer):m_Transfer(transfer)
	{
		m_Allocator = allocator;
		m_Data = &data;
		m_ArraySize = &size;
	}
	
	value_type* begin ()         { return *m_Data; }
	value_type* end ()           { return *m_Data + *m_ArraySize; }
	size_t size()       { return *m_ArraySize; }

	void resize (int size)
	{
		if(m_Transfer.IsReading() || m_Transfer.IsWriting() || m_Transfer.IsRemapPPtrTransfer())
		{
			mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (m_Allocator);
			Assert(allocator != NULL);
		
			*m_Data = allocator->ConstructArray<value_type> (size);
			memset(*m_Data, 0, sizeof(value_type)*size);
			*m_ArraySize = size;
		}
	}
};



template<class T, class TransferFunction2>
class SerializeTraits<ManualArrayTransfer<T, TransferFunction2> > : public SerializeTraitsBase<ManualArrayTransfer<T, TransferFunction2> >
{
public:
	
	typedef ManualArrayTransfer<T, TransferFunction2>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
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

#define TRANSFER_NULLABLE(x,TYPE) \
if (transfer.IsReading () || transfer.IsWriting ()) \
{ \
	if (x == NULL) \
	{ \
		mecanim::memory::ChainedAllocator* allocator = static_cast<mecanim::memory::ChainedAllocator*> (transfer.GetUserData()); \
		x = allocator->Construct<TYPE>(); \
	} \
	transfer.Transfer(*x, #x); \
} \
else \
{ \
	TYPE p; \
	transfer.Transfer(p, #x); \
} 

