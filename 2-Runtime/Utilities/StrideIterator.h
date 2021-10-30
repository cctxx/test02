#ifndef STRIDE_ITERATOR_H_
#define STRIDE_ITERATOR_H_

#include <iterator>

template<class T>
class StrideIterator : public std::iterator<std::random_access_iterator_tag, T>
{
public:
	//@TODO: Get rid of all stl usage and remove this crap.
	typedef std::iterator<std::random_access_iterator_tag, T> base_iterator;
	typedef typename base_iterator::value_type value_type;
	typedef typename base_iterator::difference_type difference_type;
	typedef typename base_iterator::pointer pointer;
	typedef typename base_iterator::reference reference;
	typedef typename base_iterator::iterator_category iterator_category;
	
	StrideIterator () : m_Pointer(NULL), m_Stride(1) {}
	StrideIterator (void* p, int stride) : m_Pointer (reinterpret_cast<UInt8*> (p)), m_Stride (stride) {}
	StrideIterator (StrideIterator const& arg) : m_Pointer(arg.m_Pointer), m_Stride(arg.m_Stride) {}
    
    void operator = (StrideIterator const& arg) { m_Pointer = arg.m_Pointer; m_Stride = arg.m_Stride; }
    
    bool operator == (StrideIterator const& arg) const { return m_Pointer == arg.m_Pointer; }
    bool operator != (StrideIterator const& arg) const { return m_Pointer != arg.m_Pointer; }

	bool operator < (StrideIterator const& arg) const { return m_Pointer < arg.m_Pointer; }
	
	void operator++()                       { m_Pointer += m_Stride; }
	void operator++(int)                    { m_Pointer += m_Stride; }

	StrideIterator operator + (difference_type n) const { return StrideIterator (m_Pointer + m_Stride * n, m_Stride); }
	void operator += (difference_type n)                { m_Pointer += m_Stride * n; }
	
	difference_type operator-(StrideIterator const& it) const
	{
		Assert (m_Stride == it.m_Stride && "Iterators stride must be equal");
		Assert (m_Stride != 0 && "Stide must not be zero");
		return ((uintptr_t)m_Pointer - (uintptr_t)it.m_Pointer) / m_Stride;
	}
	
	T& operator[](size_t index)             { return *reinterpret_cast<T*> (m_Pointer + m_Stride * index); }
	const T& operator[](size_t index) const { return *reinterpret_cast<const T*> (m_Pointer + m_Stride * index); }
	
	T& operator*()                          { return *reinterpret_cast<T*> (m_Pointer); }
	const T& operator*() const              { return *reinterpret_cast<const T*> (m_Pointer); }
	
	T* operator->()                         { return reinterpret_cast<T*> (m_Pointer); }
	const T* operator->() const             { return reinterpret_cast<const T*> (m_Pointer); }
	
	// Iterator is NULL if not valid
	bool IsNull () const { return m_Pointer == 0; }
	void* GetPointer() const { return m_Pointer; }
	int GetStride() const { return m_Stride; }

private:
	UInt8* m_Pointer;
	int    m_Stride;
};

template<class T>
void strided_copy (const T* src, const T* srcEnd, StrideIterator<T> dst)
{
	for (; src != srcEnd ; src++, ++dst)
		*dst = *src;
}

template<class T>
void strided_copy (StrideIterator<T> src, StrideIterator<T> srcEnd, StrideIterator<T> dst)
{
	for (; src != srcEnd ; ++src, ++dst)
		*dst = *src;
}

template<class T>
void strided_copy (StrideIterator<T> src, StrideIterator<T> srcEnd, T* dst)
{
	for (; src != srcEnd ;  ++src, ++dst)
		*dst = *src;
}

template<class T, class T2>
void strided_copy_convert (const T* src, const T* srcEnd, StrideIterator<T2> dst)
{
	for (; src != srcEnd ;  ++src, ++dst)
		*dst = *src;
}

#endif // STRIDE_ITERATOR_H_
