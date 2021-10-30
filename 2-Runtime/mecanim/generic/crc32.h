#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/string.h"

namespace mecanim
{
	template < std::size_t Bits > struct reflector
    {
		typedef typename mecanim::uint_t<Bits>::value_type  value_type;
        static  value_type  reflect( value_type x );
    };

	// Function that reflects its argument
    template < std::size_t Bits > typename reflector<Bits>::value_type reflector<Bits>::reflect( typename reflector<Bits>::value_type  x)
    {
        value_type        reflection = 0;
        value_type const  one = 1;

        for ( std::size_t i = 0 ; i < Bits ; ++i, x >>= 1 )
        {
            if ( x & one )
            {
                reflection |= ( one << (Bits - 1u - i) );
            }
        }

        return reflection;
    }

	template <uint32_t TruncPoly> struct crc32_table_t
    {
        static const uint32_t byte_combos = (1ul << CHAR_BIT);

		typedef uint32_t	value_type;
        typedef value_type	table_type[byte_combos];

        static  void  init_table();

        static  table_type  table;
		static  bool		isInitialized;
    };

	template <uint32_t TruncPoly> typename crc32_table_t<TruncPoly>::table_type crc32_table_t<TruncPoly>::table = { 0 };
	template <uint32_t TruncPoly> bool crc32_table_t<TruncPoly>::isInitialized = false;

	// Populate CRC lookup table
    template<uint32_t TruncPoly> void crc32_table_t<TruncPoly>::init_table()
    {
        // factor-out constants to avoid recalculation
        value_type const     fast_hi_bit = 1ul << ( 32 - 1u );
        unsigned char const  byte_hi_bit = 1u << (CHAR_BIT - 1u);

        // loop over every possible dividend value
        unsigned char  dividend = 0;
        do
        {
            value_type  remainder = 0;

            // go through all the dividend's bits
            for ( unsigned char mask = byte_hi_bit ; mask ; mask >>= 1 )
            {
                // check if divisor fits
                if ( dividend & mask )
                {
                    remainder ^= fast_hi_bit;
                }

                // do polynominal division
                if ( remainder & fast_hi_bit )
                {
                    remainder <<= 1;
                    remainder ^= TruncPoly;
                }
                else
                {
                    remainder <<= 1;
                }
            }

            table[ reflector<CHAR_BIT>::reflect(dividend) ] = reflector<32>::reflect( remainder );
        }
        while ( ++dividend );

        isInitialized = true;
    }

	class crc32
	{	
	public:
		// Type
		typedef uint32_t								value_type;
		
		// Constants for the template parameters
		static const std::size_t	bit_count = 32;
		static const value_type		truncated_polynominal = 0x04C11DB7;
		static const value_type		initial_remainder = 0xFFFFFFFF;
		static const value_type		final_xor_value = 0xFFFFFFFF;

		// Constructor
		explicit	crc32( value_type init_rem = crc32::initial_remainder ):rem(reflector<bit_count>::reflect(init_rem)){}

		inline void		process_block(void const *bytes_begin, void const *bytes_end);
		inline void		process_block_skip2(void const *  bytes_begin, void const *  bytes_end);

		inline void		process_bytes(void const *buffer, std::size_t byte_count );
		inline void		process_bytes_skip2(void const *   buffer, std::size_t  byte_count);

		inline value_type  checksum() const;

		typedef crc32_table_t<truncated_polynominal>	crc32_table_type;

	protected:

		inline value_type get_truncated_polynominal() const{ return truncated_polynominal; }
		inline value_type get_initial_remainder() const{return initial_remainder;}
		inline value_type get_final_xor_value() const{return final_xor_value;}

		static  unsigned char  index( value_type rem, unsigned char x ){ return static_cast<unsigned char>(x ^ rem); }

        // Shift out the remainder's highest byte
        static  value_type  shift( value_type rem ){ return rem >> CHAR_BIT; }
		
		 // Member data
		value_type  rem;
	};

	inline void crc32::process_block(void const *  bytes_begin, void const *  bytes_end)
	{
		Assert(crc32_table_type::isInitialized);

		// Recompute the CRC for each byte passed
		for ( unsigned char const * p = static_cast<unsigned char const *>(bytes_begin) ; p < bytes_end ; ++p )
		{
			// Compare the new byte with the remainder's higher bits to
			// get the new bits, shift out the remainder's current higher
			// bits, and update the remainder with the polynominal division
			// of the new bits.
			unsigned char const  byte_index = index( rem, *p );
			rem = shift( rem );
			rem ^= crc32_table_type::table[ byte_index ];
		}
	}

	inline void crc32::process_block_skip2(void const *  bytes_begin, void const *  bytes_end)
	{
		Assert(crc32_table_type::isInitialized);
		unsigned char const * p;

#if UNITY_BIG_ENDIAN
		p = static_cast<unsigned char const *>(bytes_begin) + 1; 
#else
		p = static_cast<unsigned char const *>(bytes_begin); 
#endif
		
		// Recompute the CRC for every second byte passed. This is useful for hashing a UTF16 string that is known to actually be an ascii string.
		for ( ; p < bytes_end ; p += 2 )
		{
			// Compare the new byte with the remainder's higher bits to
			// get the new bits, shift out the remainder's current higher
			// bits, and update the remainder with the polynominal division
			// of the new bits.
			unsigned char const  byte_index = index( rem, *p );
			rem = shift( rem );
			rem ^= crc32_table_type::table[ byte_index ];
		}
	}
	
	
	inline void crc32::process_bytes(void const *   buffer, std::size_t  byte_count)
	{
		unsigned char const * const  b = static_cast<unsigned char const *>( buffer );
		process_block( b, b + byte_count );
	}

	inline void crc32::process_bytes_skip2(void const *   buffer, std::size_t  byte_count)
	{
		unsigned char const * const  b = static_cast<unsigned char const *>( buffer );
		process_block_skip2( b, b + byte_count );
	}
	
	inline crc32::value_type crc32::checksum() const
	{
		return ( rem ^ get_final_xor_value() );
	}

	static inline int processCRC32(String const& string) 
	{     
		crc32 result;
		result.process_bytes(string.c_str(), string.size());     
		return result.checksum(); 
	}

	static inline int processCRC32(char const* string) 
	{     
		crc32 result;
		result.process_bytes(string, strlen(string));     
		return result.checksum(); 
	}

	static inline int processCRC32UTF16Ascii(unsigned short const* string, std::size_t stringLength) 
	{     
		crc32 result;
		result.process_bytes_skip2(string, stringLength * 2);     
		return result.checksum(); 
	}
}
