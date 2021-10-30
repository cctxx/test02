#pragma once

#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Utilities/dynamic_array.h"

// dynamic_block_vector
//
// Allocates dynamic_arrays to hold the data in small blocks. 
// Growing pushbacks allocates one new block at a time.
// Calls inplace constructor on all elements, and destroys the elements when removing from the list
// Resize preserves the elements in the list and pushes default initialized elements to reach size
// If resizing to something smaller, elements are popped (and destroyed) from the vector


template <typename T>
struct dynamic_block_vector
{
private:
	typedef     dynamic_array<T>					internal_container;
	typedef     dynamic_array<internal_container*>	container;

public:

	dynamic_block_vector (size_t allocationBlockSize) 
		: m_blockSize(allocationBlockSize), m_size(0), m_label(kMemDynamicArrayId, GET_CURRENT_ALLOC_ROOT_HEADER())
	{
	}

	dynamic_block_vector (size_t allocationBlockSize, MemLabelId label) 
		: m_blockSize(allocationBlockSize), m_size(0), m_label(label)
	{
	}
	
	dynamic_block_vector (const dynamic_block_vector& rhs)
		: m_blockSize(rhs.m_blockSize), m_size(0), m_label(rhs.m_label)
	{
		*this = rhs;
	}
	
	~dynamic_block_vector ()
	{	
		clear();
	}		
	
	void clear()
	{
		for(int i = 0; i < m_size; i++) 
			(*this)[i].~T();

		for(int i = 0; i < m_data.size(); i++)
			UNITY_DELETE(m_data[i],m_label);
	
		m_data.clear();
		m_size = 0;
	}
	
	void resize (size_t size)
	{
		while (m_size < size)
			push_back();
		while (m_size > size)
			pop_back();
	}

	dynamic_block_vector& operator=(const dynamic_block_vector& other)
	{
		if(this == &other)
			return *this;

		clear();
		for( int i = 0; i < other.size(); i++)
			push_back(other[i]);
		return *this;
	}

	template<class Iter>
	void assign (Iter first, Iter last)
	{
		clear();
		for( ; first != last; ++first)
			push_back(*first);
	}

	void push_back ()
	{
		int outerindex = m_size/m_blockSize;
		int innerindex = m_size%m_blockSize;
		if(outerindex == m_data.size())
		{
			m_data.push_back(UNITY_NEW(internal_container,m_label)(m_blockSize,m_label));
		}
		new (&(*m_data[outerindex])[innerindex]) T();
		m_size++;
	}

	void push_back (const T& t)
	{
		int outerindex = m_size/m_blockSize;
		int innerindex = m_size%m_blockSize;
		if(outerindex == m_data.size())
		{
			m_data.push_back(UNITY_NEW(internal_container,m_label)(m_blockSize,m_label));
		}
		new (&(*m_data[outerindex])[innerindex]) T(t);
		m_size++;
	}

	void pop_back ()
	{
		(*this)[m_size-1].~T();
		m_size--;
		int outersize = m_size/m_blockSize + 1;
		if (outersize < m_data.size())
		{
			UNITY_DELETE(m_data.back(),m_label);
			m_data.pop_back();
		}
	}

	size_t size () const { return m_size; }

	T& back() { Assert (m_size != 0); return (*this)[m_size - 1]; }

	T const& operator[] (size_t index) const { DebugAssert(index < m_size); return (*m_data[index/m_blockSize])[index%m_blockSize]; }
	T& operator[] (size_t index) { DebugAssert(index < m_size); return (*m_data[index/m_blockSize])[index%m_blockSize]; }

private:
	
	container   m_data;
	MemLabelId  m_label;
	size_t      m_size;
	size_t      m_blockSize;
};


