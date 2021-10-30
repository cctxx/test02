#include "UnityPrefix.h"
#include "FixedHeapAllocator.h"
#include "tlsf/tlsf.h"

#if FIXED_HEAP_ALLOC_COUNT_USED
#define ALLOC_USED_MEM_INC(ptr) if (ptr) { m_nSizeUsed += tlsf_block_size(ptr); }
#define ALLOC_USED_MEM_DEC(ptr) if (ptr) { m_nSizeUsed -= tlsf_block_size(ptr); }
#else
#define ALLOC_USED_MEM_INC(ptr)
#define ALLOC_USED_MEM_DEC(ptr)
#endif

FixedHeapAllocator::FixedHeapAllocator(void* pMemoryBase, UInt32 nMemorySize, const char* name)
: BaseAllocator(name)
, m_TlsfPool(0)
, m_pMemoryBase(pMemoryBase)
, m_nMemorySize(nMemorySize)
#if FIXED_HEAP_ALLOC_COUNT_USED
, m_nSizeUsed(0)
#endif
{
	m_TlsfPool = tlsf_create(pMemoryBase, nMemorySize);
}


FixedHeapAllocator::~FixedHeapAllocator()
{
	tlsf_destroy(m_TlsfPool);
}

void* FixedHeapAllocator::Allocate (size_t size, int align)
{
	void* addr = tlsf_memalign(m_TlsfPool, align, size);
	ALLOC_USED_MEM_INC(addr);
	return addr;
}

void* FixedHeapAllocator::Reallocate(void* p, size_t size, size_t align)
{
	ALLOC_USED_MEM_DEC(p);
	void* addr = tlsf_realloc(m_TlsfPool, p, size);
	ALLOC_USED_MEM_INC(addr);
	return addr;
}

void FixedHeapAllocator::Deallocate(void* p)
{
	ALLOC_USED_MEM_DEC(p);
	return tlsf_free(m_TlsfPool, p);
}

UInt32 FixedHeapAllocator::GetPtrSize(void* p)
{
	return (UInt32)tlsf_block_size(p);
}

bool FixedHeapAllocator::Contains(const void* p)
{
	return (p >= m_pMemoryBase && p < (char*)m_pMemoryBase + m_nMemorySize);
}

bool FixedHeapAllocator::CheckIntegrity()
{
	return tlsf_check_heap(m_TlsfPool);
}

#undef ALLOC_USED_MEM_INC
#undef ALLOC_USED_MEM_DEC
