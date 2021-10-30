
#ifndef _ALLOCATOR_FIXEDSIZE_ALLOCATOR
#define _ALLOCATOR_FIXEDSIZE_ALLOCATOR

#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Misc/AllocatorLabels.h"


template <unsigned BlockSize>
class
FixedSizeAllocator
{
public:
	FixedSizeAllocator(MemLabelId memLabel);
	~FixedSizeAllocator();

    void*	alloc();
    void	free( void* mem );


    void	reset();
	void	free_memory();

	unsigned	total_allocated() const;
	unsigned	total_free() const;
	unsigned	capacity() const;


private:


	// we can do this template parameter
	static const UInt8 BlocksInChunk = 255;

    struct
    Chunk
    {
        UInt8	data[BlocksInChunk*BlockSize];

        Chunk*	next;

        UInt8	first_available;
        UInt8	total_available;
    };

    Chunk*	m_Chunk;

    // we can store pointers -- they will be updated anyway should the new chunk be added
    Chunk*	m_AllocChunk;
    Chunk*	m_DeallocChunk;

    MemLabelId  m_MemLabel;


    void	create_chunk();
    void	reset_chunk( Chunk* chunk );

    void*	alloc_from( Chunk* chunk );
    void	dealloc_from( void* mem, Chunk* chunk );

    bool	mem_from( void* mem, Chunk* chunk );
};


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::reset_chunk( Chunk* chunk )
{
    AssertBreak(chunk);

    chunk->first_available = 0;
    chunk->total_available = BlocksInChunk;

    // store index of next available block in-place, as first byte of the block

    UInt8  next_available_i    = 1;
    UInt8* next_available_mem  = (UInt8*)chunk->data;

    while( next_available_i != BlocksInChunk )
    {
        *next_available_mem = next_available_i;

        ++next_available_i;
        next_available_mem += BlockSize;
    }
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::create_chunk()
{
    Chunk* newChunk = (Chunk*)UNITY_MALLOC(m_MemLabel, sizeof(Chunk));

	reset_chunk(newChunk);
    newChunk->next = 0;

    if( m_Chunk )
    {
        Chunk* tail_chunk = m_Chunk;

        while( tail_chunk->next )
            tail_chunk = tail_chunk->next;

        tail_chunk->next = newChunk;
    }
    else
    {
        m_Chunk = newChunk;
    }

    m_AllocChunk = m_DeallocChunk = newChunk;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline
FixedSizeAllocator<BlockSize>::FixedSizeAllocator(MemLabelId memLabel)
  : m_Chunk(0),
    m_AllocChunk(0),
    m_DeallocChunk(0),
    m_MemLabel(memLabel)
{
	// TODO: create chunk right away?
	//create_chunk()
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline
FixedSizeAllocator<BlockSize>::~FixedSizeAllocator()
{
	free_memory();
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline bool
FixedSizeAllocator<BlockSize>::mem_from( void* mem, Chunk* chunk )
{
    return (    (UInt8*)mem >= (UInt8*)chunk->data
             // TODO: align into account
             && (UInt8*)mem < (UInt8*)chunk->data + BlocksInChunk*BlockSize );
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void*
FixedSizeAllocator<BlockSize>::alloc_from( Chunk* chunk )
{
    AssertBreak( chunk );
    AssertBreak( chunk->total_available );

    UInt8* ret = (UInt8*)chunk->data + chunk->first_available*BlockSize;

    chunk->first_available = *ret;
    --chunk->total_available;

    return ret;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::dealloc_from( void* mem, Chunk* chunk )
{
    AssertBreak( chunk );
    AssertBreak( mem_from(mem, chunk) );

    UInt8* release_ptr = (UInt8*)mem;
    AssertBreak( (release_ptr - (UInt8*)chunk->data) % BlockSize == 0 );

    *release_ptr = chunk->first_available;
    chunk->first_available = (release_ptr - (UInt8*)chunk->data) / BlockSize;

    ++chunk->total_available;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void*
FixedSizeAllocator<BlockSize>::alloc()
{
    if( m_AllocChunk == 0 || m_AllocChunk->total_available == 0 )
    {
        // fallback to linear search

        m_AllocChunk = m_Chunk;
        while( m_AllocChunk )
        {
            if( m_AllocChunk->total_available )
                break;

            m_AllocChunk = m_AllocChunk->next;
        }

        if( !m_AllocChunk )
            create_chunk();
    }

    return alloc_from(m_AllocChunk);
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::free( void* mem )
{
	AssertBreak(m_DeallocChunk);

    if( !mem )
        return;

    if( !mem_from(mem, m_DeallocChunk) )
    {
        // we want to exploit possible locality
        // but for now we store chunks in single-linked list
        // fallback to simple linear search;

        m_DeallocChunk = m_Chunk;
        while( m_DeallocChunk )
        {
            if( mem_from(mem, m_DeallocChunk) )
                break;

            m_DeallocChunk = m_DeallocChunk->next;
        }
    }

    AssertBreak(m_DeallocChunk);
    dealloc_from( mem, m_DeallocChunk );
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::reset()
{
    Chunk* target_chunk = m_Chunk;
    while( target_chunk )
    {
        reset_chunk(target_chunk);
        target_chunk = target_chunk->next;
    }

    m_AllocChunk = m_DeallocChunk = m_Chunk;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline void
FixedSizeAllocator<BlockSize>::free_memory()
{
	Chunk* target_chunk = m_Chunk;
    while( target_chunk )
    {
		Chunk* nextChunk = target_chunk->next;
		UNITY_FREE(m_MemLabel, target_chunk);

        target_chunk = nextChunk;
    }

    m_AllocChunk = m_DeallocChunk = m_Chunk = 0;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline unsigned
FixedSizeAllocator<BlockSize>::capacity() const
{
	unsigned ret = 0;

	Chunk* target_chunk = m_Chunk;
    while( target_chunk )
    {
        ret += BlocksInChunk*BlockSize;
        target_chunk = target_chunk->next;
    }

	return ret;
}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline unsigned
FixedSizeAllocator<BlockSize>::total_free() const
{
	unsigned ret = 0;

	Chunk* target_chunk = m_Chunk;
    while( target_chunk )
    {
        ret += target_chunk->total_available * BlockSize;
        target_chunk = target_chunk->next;
    }

	return ret;

}


//------------------------------------------------------------------------------

template <unsigned BlockSize>
inline unsigned
FixedSizeAllocator<BlockSize>::total_allocated() const
{
	return capacity() - total_free();
}


//==============================================================================

#endif // _ALLOCATOR_FIXEDSIZE_ALLOCATOR

