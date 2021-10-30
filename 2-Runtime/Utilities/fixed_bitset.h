#pragma once

#include "Runtime/Utilities/StaticAssert.h"


// Fixed size bitset; size (N) must be a multiple of 32.
// Similar to dynamic_bitset, but does not do dynamic allocations and stuff.
template<int N>
class fixed_bitset {
public:
	enum { kBlockSize = 32, kBlockCount = N/kBlockSize };
public:
	fixed_bitset()
	{
		CompileTimeAssert(N % kBlockSize == 0, "size should be multiple fo 32");
		CompileTimeAssert(sizeof(m_Bits[0])*8 == kBlockSize, "size of internal array type should be 4" );
		for( int i = 0; i < kBlockCount; ++i )
			m_Bits[i] = 0;
	}
	// note: default copy constructor and assignment operator are ok

	void set( int index ) {
		AssertIf( index < 0 || index >= N );
		m_Bits[index/kBlockSize] |= 1 << (index & (kBlockSize-1));
	}
	void reset( int index ) {
		AssertIf( index < 0 || index >= N );
		m_Bits[index/kBlockSize] &= ~( 1 << (index & (kBlockSize-1)) );
	}
	bool test( int index ) const {
		AssertIf( index < 0 || index >= N );
		return m_Bits[index/kBlockSize] & ( 1 << (index & (kBlockSize-1)) );
	}
	void reset_all() {
		memset( m_Bits, 0, sizeof(m_Bits) );
	}

	bool operator==( const fixed_bitset<N>& o ) const {
		for( int i = 0; i < kBlockCount; ++i )
			if( m_Bits[i] != o.m_Bits[i] )
				return false;
		return true;
	}
	bool operator!=( const fixed_bitset<N>& o ) const {
		for( int i = 0; i < kBlockCount; ++i )
			if( m_Bits[i] == o.m_Bits[i] )
				return false;
		return true;
	}

private:
	UInt32	m_Bits[kBlockCount];
};
