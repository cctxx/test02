#ifndef HEAP_ALLOCATOR_H_
#define HEAP_ALLOCATOR_H_

#include "BaseAllocator.h"

#define FIXED_HEAP_ALLOC_COUNT_USED !MASTER_BUILD

class FixedHeapAllocator : public BaseAllocator
{
	void* m_TlsfPool;
	void* m_pMemoryBase;
	UInt32 m_nMemorySize;
#if FIXED_HEAP_ALLOC_COUNT_USED
	UInt32 m_nSizeUsed;
#endif

public:
	FixedHeapAllocator(void* pMemoryBase, UInt32 nMemorySize, const char* name);
	~FixedHeapAllocator();

	virtual void* Allocate (size_t size, int align);
	virtual void* Reallocate (void* p, size_t size, size_t align);
	virtual void  Deallocate (void* p);

	virtual bool  Contains (const void* p);
	virtual bool  CheckIntegrity();

	virtual UInt32 GetPtrSize(void* p);
};

#endif
