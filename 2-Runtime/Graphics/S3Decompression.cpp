#include "UnityPrefix.h"
#include "S3Decompression.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/FlashATFDecompression.h"
#include "Runtime/Graphics/ETC2Decompression.h"

#define PRINT_DECOMPRESSION_TIMES 0
#if PRINT_DECOMPRESSION_TIMES
#include "Runtime/Input/TimeManager.h"
#endif


// \todo [2010-02-09 petri] Define in some config file?
#define HAS_ETC_DECOMPRESSOR (UNITY_ANDROID && !i386 || UNITY_EDITOR || UNITY_BB10 || UNITY_TIZEN)
#define HAS_ATC_DECOMPRESSOR (UNITY_ANDROID && !i386 || UNITY_EDITOR)

#define HAS_ASTC_DECOMPRESSOR (UNITY_ANDROID || UNITY_EDITOR)

#if HAS_ATC_DECOMPRESSOR
	#include "External/Qualcomm_TextureConverter/TextureConverter.h"
	#if UNITY_WIN
		#include <process.h>
		// On Windows, Qonvert is compiled with MSVCRT, but we link against LIBCMT; filling the gaps here
		extern "C" int _imp___getpid(void){ return _getpid(); }
	#elif UNITY_OSX
		// On OSX, Qonvert is compiled with -fstack-protector, but we don't use it for the MacEditor
		extern "C"
		{
			void* __stack_chk_guard = (void*)0xdeadc0de;
			void __stack_chk_fail() { ErrorString("stack_chk_fail failed"); }
		}
	#elif defined(ARM_ARCH_VFP)
		// On Android, Qonvert is compiled with armv5 (no vfp), but we don't want to include the softfp emulation
		extern "C" unsigned __aeabi_f2uiz(float f){return (int)f;}
	#endif
#endif

// ------------------------------------------------------------------------
//  DXT


#if HAS_DXT_DECOMPRESSOR

struct DXTColBlock
{
	UInt16 col0;
	UInt16 col1;
	UInt8 row[4];
};

struct DXTAlphaBlockExplicit
{
	UInt16 row[4];
};

struct DXTAlphaBlock3BitLinear
{
	UInt8 alpha0;
	UInt8 alpha1;
	UInt8 stuff[6];
};

#if UNITY_BIG_ENDIAN

struct Color8888
{
	UInt8 b;
	UInt8 g;
	UInt8 r;
	UInt8 a;
};

static inline UInt16 GetByteSwap16( UInt16 i )
{
	return static_cast<UInt16>((i << 8) | (i >> 8));
}

static inline UInt32 GetByteSwap32( UInt32 i )
{
	return static_cast<UInt32>((i >> 24) | (i >> 8) & 0x0000ff00 | (i << 8) & 0x00ff0000 | (i << 24));
}

struct Color565
{
	unsigned b : 5;
	unsigned g : 6;
	unsigned r : 5;
};

#else

static inline UInt16 GetByteSwap16( UInt16 i ) { return i; }
static inline UInt32 GetByteSwap32( UInt32 i ) { return i; }

struct Color8888 // EH? This seems to be wrong!
{
	UInt8 r;
	UInt8 g;
	UInt8 b;
	UInt8 a;
};

struct Color565
{
	unsigned b : 5;
	unsigned g : 6;
	unsigned r : 5;
};

#endif


inline void GetColorBlockColors( const DXTColBlock* block, Color8888 colors[4] )
{
	union
	{
		Color565 color565;
		UInt16	color16;
	} col0, col1;
	col0.color16 = GetByteSwap16( block->col0 );
	col1.color16 = GetByteSwap16( block->col1 );

	const Color565* col;

	// It's not enough just to shift bits to full 8 bit precision - the lower bits
	// must also be filled to match the way hardware does the rounding.
	col = &col0.color565;
	colors[0].r = ( col->r << 3 ) | ( col->r >> 2 );
	colors[0].g = ( col->g << 2 ) | ( col->g >> 4 );
	colors[0].b = ( col->b << 3 ) | ( col->b >> 2 );
	colors[0].a = 0xff;

	col = &col1.color565;
	colors[1].r = ( col->r << 3 ) | ( col->r >> 2 );
	colors[1].g = ( col->g << 2 ) | ( col->g >> 4 );
	colors[1].b = ( col->b << 3 ) | ( col->b >> 2 );
	colors[1].a = 0xff;

	if( col0.color16 > col1.color16 )
	{
		// Four-color block: derive the other two colors.
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields
		// stored in the 64-bit block.

		colors[2].r = (UInt8)(((UInt16)colors[0].r * 2 + (UInt16)colors[1].r )/3);
		colors[2].g = (UInt8)(((UInt16)colors[0].g * 2 + (UInt16)colors[1].g )/3);
		colors[2].b = (UInt8)(((UInt16)colors[0].b * 2 + (UInt16)colors[1].b )/3);
		colors[2].a = 0xff;

		colors[3].r = (UInt8)(((UInt16)colors[0].r + (UInt16)colors[1].r *2 )/3);
		colors[3].g = (UInt8)(((UInt16)colors[0].g + (UInt16)colors[1].g *2 )/3);
		colors[3].b = (UInt8)(((UInt16)colors[0].b + (UInt16)colors[1].b *2 )/3);
		colors[3].a = 0xff;
	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2, 11 = transparent.
		// These two bit codes correspond to the 2-bit fields
		// stored in the 64-bit block.

		colors[2].r = (UInt8)(((UInt16)colors[0].r + (UInt16)colors[1].r )/2);
		colors[2].g = (UInt8)(((UInt16)colors[0].g + (UInt16)colors[1].g )/2);
		colors[2].b = (UInt8)(((UInt16)colors[0].b + (UInt16)colors[1].b )/2);
		colors[2].a = 0xff;

		// set transparent to black to match DXT specs
		colors[3].r = 0x00;
		colors[3].g = 0x00;
		colors[3].b = 0x00;
		colors[3].a = 0x00;
	}
}

// width is width of destination image in pixels
inline void DecodeColorBlock( UInt32* dest, const DXTColBlock& colorBlock, int width, const UInt32 colors[4] )
{
	// r steps through lines in y
	for( int r=0; r < 4; r++, dest += width-4 )
	{
		// width * 4 bytes per pixel per line
		// each j block row is 4 lines of pixels

		// Do four pixels, step in twos because n is only used for the shift
		// n steps through pixels
		for( int n = 0; n < 8; n += 2 )
		{
			UInt32 bits = (colorBlock.row[r] >> n) & 3;
			DebugAssert (bits <= 3);
			*dest = colors[bits];
			++dest;
		}
	}
}

inline void DecodeAlphaExplicit( UInt32* dest, const DXTAlphaBlockExplicit& alphaBlock, int width, UInt32 alphazero )
{
	// alphazero is a bit mask that when ANDed with the image color
	//  will zero the alpha bits, so if the image DWORDs are
	//  ARGB then alphazero will be 0x00FFFFFF or if
	//  RGBA then alphazero will be 0xFFFFFF00
	//  alphazero constructed automatically from field order of Color8888 structure

	union
	{
		Color8888 col;
		UInt32 col32;
	} u;
	u.col.r = u.col.g = u.col.b = 0;

	for( int row=0; row < 4; row++, dest += width-4 )
	{
		UInt16 wrd = GetByteSwap16( alphaBlock.row[ row ] );

		for( int pix = 0; pix < 4; ++pix )
		{
			// zero the alpha bits of image pixel
			*dest &= alphazero;

			u.col.a = wrd & 0x000f; // get lowest 4 bits
			u.col.a = u.col.a | (u.col.a << 4);

			*dest |= u.col32; // OR into the previously nulled alpha

			wrd >>= 4;
			++dest;
		}
	}
}

inline void DecodeAlpha3BitLinear( UInt32* dest, const DXTAlphaBlock3BitLinear& alphaBlock, int width, UInt32 alphazero )
{
	union
	{
		Color8888 alphaCol[4][4];
		UInt32 alphaCol32[4][4];
	} u;
	UInt32 alphamask = ~alphazero;
	UInt16	alphas[8];

	alphas[0] = alphaBlock.alpha0;
	alphas[1] = alphaBlock.alpha1;

	// 8-alpha or 6-alpha block?
	if( alphas[0] > alphas[1] )
	{
		// 8-alpha block:  derive the other 6 alphas.
		// 000 = alpha[0], 001 = alpha[1], others are interpolated

		alphas[2] = ( 6 * alphas[0] +     alphas[1] + 3) / 7;	// bit code 010
		alphas[3] = ( 5 * alphas[0] + 2 * alphas[1] + 3) / 7;	// Bit code 011
		alphas[4] = ( 4 * alphas[0] + 3 * alphas[1] + 3) / 7;	// Bit code 100
		alphas[5] = ( 3 * alphas[0] + 4 * alphas[1] + 3) / 7;	// Bit code 101
		alphas[6] = ( 2 * alphas[0] + 5 * alphas[1] + 3) / 7;	// Bit code 110
		alphas[7] = (     alphas[0] + 6 * alphas[1] + 3) / 7;	// Bit code 111
	}
	else
	{
		// 6-alpha block:  derive the other alphas.
		// 000 = alpha[0], 001 = alpha[1], others are interpolated

		alphas[2] = (4 * alphas[0] +     alphas[1] + 2) / 5;	// Bit code 010
		alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5;	// Bit code 011
		alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5;	// Bit code 100
		alphas[5] = (    alphas[0] + 4 * alphas[1] + 2) / 5;	// Bit code 101
		alphas[6] = 0;										// Bit code 110
		alphas[7] = 255;									// Bit code 111
	}

	// Decode 3-bit fields into array of 16 bytes with same value
	UInt8	blockBits[4][4];

	// first two rows of 4 pixels each:
	const UInt32 mask = 7; // three bits

	// TBD: Ouch! Unaligned reads there! Poor old PPC!

    UInt32 bits = GetByteSwap32( *(UInt32*)&alphaBlock.stuff[0] ); // first 3 bytes

	blockBits[0][0] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[0][1] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[0][2] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[0][3] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[1][0] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[1][1] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[1][2] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[1][3] = (UInt8)( bits & mask );

	// now last two rows
	// it's ok to fetch one byte too much because there will always be color block after alpha
	bits = GetByteSwap32( *(UInt32*)&alphaBlock.stuff[3] ); // last 3 bytes

	blockBits[2][0] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[2][1] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[2][2] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[2][3] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[3][0] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[3][1] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[3][2] = (UInt8)( bits & mask );
	bits >>= 3;
	blockBits[3][3] = (UInt8)( bits & mask );

	// decode the codes into alpha values
	int row, pix;

	for( row = 0; row < 4; row++ )
	{
		for( pix=0; pix < 4; pix++ )
		{
			u.alphaCol[row][pix].a = (UInt8) alphas[ blockBits[row][pix] ];
		}
	}

	// Write out alpha values to the image bits
	for( row=0; row < 4; row++, dest += width-4 )
	{
		for( pix = 0; pix < 4; pix++ )
		{
			*dest &= alphazero; // zero the alpha bits of image pixel
			*dest |= u.alphaCol32[row][pix] & alphamask; // or the bits into the prev. nulled alpha
			dest++;
		}
	}
}


void DecompressDXT1( int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* decompBytes )
{
	union
	{
		Color8888 colors8888[4];
		UInt32 colors32[4];
	} u;

	for( int j = 0; j < yblocks; ++j )
	{
		const DXTColBlock* block = (const DXTColBlock*)( m_pCompBytes + j * xblocks * 2 ); // 8 bytes per block

		for( int i = 0; i < xblocks; ++i, ++block )
		{
			GetColorBlockColors( block, u.colors8888 );
			UInt32* dest = (UInt32*)((UInt8*)decompBytes + i*16 + (j*4) * destWidth * 4 );
			DecodeColorBlock( dest, *block, destWidth, u.colors32 );
		}
	}
}


void DecompressDXT3( int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* decompBytes )
{
	union
	{
		Color8888 colors[4];
		UInt32 colors32[4];
	} u;

	// fill alphazero with appropriate value to zero out alpha when
	//  alphazero is ANDed with the image color
	u.colors[0].a = 0;
	u.colors[0].r = u.colors[0].g = u.colors[0].b = 0xff;
	UInt32 alphazero = u.colors32[0];

	for( int j = 0; j < yblocks; ++j )
	{
		const DXTColBlock* block = (const DXTColBlock*)( m_pCompBytes + j * xblocks * 4 ); // 16 bytes for alpha+color block

		for( int i = 0; i < xblocks; ++i, ++block )
		{
			// Get alpha block
			const DXTAlphaBlockExplicit* alphaBlock = (const DXTAlphaBlockExplicit*)block;
			++block;

			// Get color block & colors
			GetColorBlockColors( block, u.colors );

			// Decode the color block into the bitmap bits
			UInt32* dest = (UInt32*)((UInt8*)decompBytes + i*16 + (j*4) * destWidth * 4 );
			DecodeColorBlock( dest, *block, destWidth, u.colors32 );

			// Overwrite the previous alpha bits with the alpha block info
			DecodeAlphaExplicit( dest, *alphaBlock, destWidth, alphazero );
		}
	}
}


void DecompressDXT5( int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* decompBytes )
{
	union
	{
		Color8888 colors[4];
		UInt32 colors32[4];
	} u;

	// fill alphazero with appropriate value to zero out alpha when
	//  alphazero is ANDed with the image color 32 bit
	u.colors[0].a = 0;
	u.colors[0].r = u.colors[0].g = u.colors[0].b = 0xff;
	UInt32 alphazero = u.colors32[0];

	for( int j = 0; j < yblocks; ++j )
	{
		const DXTColBlock* block = (const DXTColBlock*) ( m_pCompBytes + j * xblocks * 4 ); // 16 bytes for alpha+color block

		for( int i = 0; i < xblocks; ++i, ++block )
		{
			// Get alpha block
			const DXTAlphaBlock3BitLinear* alphaBlock = (const DXTAlphaBlock3BitLinear*)block;
			block++;

			// Get color block & colors
			GetColorBlockColors( block, u.colors );

			// Decode the color block into the bitmap bits
			UInt32* dest = (UInt32*)((UInt8*)decompBytes + i*16 + (j*4) * destWidth * 4 );
			DecodeColorBlock( dest, *block, destWidth, u.colors32 );

			// Overwrite the previous alpha bits with the alpha block info
			DecodeAlpha3BitLinear( dest, *alphaBlock, destWidth, alphazero );
		}
	}
}

#endif



// ------------------------------------------------------------------------
//  PVRTC



#if HAS_PVRTC_DECOMPRESSOR

const int kPVRSizeX2 = 8; // block width 8 pixels in 2BPP case
const int kPVRSizeX4 = 4; // block width 4 pixels in 4BPP case
const int kPVRBlockSizeY = 4; // block height always 4 pixels

const int kPVRPunchThroughIndex = 2;

#define PVR_WRAP_POT_COORD(Val, Size) ((Val) & ((Size)-1))

#define CLAMP(X, lower, upper) (std::min(std::max((X),(lower)), (upper)))

#define PVR_LIMIT_COORD(Val, Size, AssumeImageTiles) \
      ((AssumeImageTiles) ? PVR_WRAP_POT_COORD((Val), (Size)) : CLAMP((Val), 0, (Size)-1))

// 64 bits per PVRTC block
struct PVRTCBlock
{
	UInt32 packedData[2];
};



// Given a block, extract the color information and convert to 5554 formats
static void Unpack5554Colour (const PVRTCBlock *pBlock, int ABColours[2][4])
{
	UInt32 RawBits[2];

	int i;

	/*
	// Extract A and B
	*/
	RawBits[0] = pBlock->packedData[1] & (0xFFFE); /*15 bits (shifted up by one)*/
	RawBits[1] = pBlock->packedData[1] >> 16;	   /*16 bits*/

	/*
	//step through both colours
	*/
	for(i = 0; i < 2; i++)
	{
		/*
		// if completely opaque
		*/
		if(RawBits[i] & (1<<15))
		{
			/*
			// Extract R and G (both 5 bit)
			*/
			ABColours[i][0] = (RawBits[i] >> 10) & 0x1F;
			ABColours[i][1] = (RawBits[i] >>  5) & 0x1F;

			/*
			// The precision of Blue depends on  A or B. If A then we need to
			// replicate the top bit to get 5 bits in total
			*/
			ABColours[i][2] = RawBits[i] & 0x1F;
			if(i==0)
			{
				ABColours[0][2] |= ABColours[0][2] >> 4;
			}

			/*
			// set 4bit alpha fully on...
			*/
			ABColours[i][3] = 0xF;
		}
		/*
		// Else if colour has variable translucency
		*/
		else
		{
			/*
			// Extract R and G (both 4 bit).
			// (Leave a space on the end for the replication of bits
			*/
			ABColours[i][0] = (RawBits[i] >>  (8-1)) & 0x1E;
			ABColours[i][1] = (RawBits[i] >>  (4-1)) & 0x1E;

			/*
			// replicate bits to truly expand to 5 bits
			*/
			ABColours[i][0] |= ABColours[i][0] >> 4;
			ABColours[i][1] |= ABColours[i][1] >> 4;

			/*
			// grab the 3(+padding) or 4 bits of blue and add an extra padding bit
			*/
			ABColours[i][2] = (RawBits[i] & 0xF) << 1;

			/*
			// expand from 3 to 5 bits if this is from colour A, or 4 to 5 bits if from
			// colour B
			*/
			if(i==0)
			{
				ABColours[0][2] |= ABColours[0][2] >> 3;
			}
			else
			{
				ABColours[0][2] |= ABColours[0][2] >> 4;
			}

			/*
			// Set the alpha bits to be 3 + a zero on the end
			*/
			ABColours[i][3] = (RawBits[i] >> 11) & 0xE;
		}/*end if variable alpha*/
	}/*end for i*/

}


// Given the block and the texture type and it's relative position in the
// 2x2 group of blocks, extract the bit patterns for the fully defined pixels.
template<bool Do2bitMode>
static void	UnpackModulations(const PVRTCBlock *pBlock,
							  int ModulationVals[8][16],
							  int ModulationModes[8][16],
							  int StartX,
							  int StartY)
{
	int BlockModMode= pBlock->packedData[1] & 1;
	UInt32 ModulationBits	= pBlock->packedData[0];

	// if it's in an interpolated mode
	if(Do2bitMode && BlockModMode)
	{
		// run through all the pixels in the block. Note we can now treat all the
		// "stored" values as if they have 2bits (even when they didn't!)
		for(int y = 0; y < kPVRBlockSizeY; y++)
		{
			for(int x = 0; x < kPVRSizeX2; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;

				// if this is a stored value...
				if(((x^y)&1) == 0)
				{
					ModulationVals[y+StartY][x+StartX] = ModulationBits & 3;
					ModulationBits >>= 2;
				}
			}
		}
	}
	// else if direct encoded 2bit mode - i.e. 1 mode bit per pixel
	else if(Do2bitMode)
	{
		for(int y = 0; y < kPVRBlockSizeY; y++)
		{
			for(int x = 0; x < kPVRSizeX2; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;

				// double the bits so 0=> 00, and 1=>11
				if(ModulationBits & 1)
				{
					ModulationVals[y+StartY][x+StartX] = 0x3;
				}
				else
				{
					ModulationVals[y+StartY][x+StartX] = 0x0;
				}
				ModulationBits >>= 1;
			}
		}
	}
	// else its the 4bpp mode so each value has 2 bits
	else
	{
		for(int y = 0; y < kPVRBlockSizeY; y++)
		{
			for(int x = 0; x < kPVRSizeX4; x++)
			{
				ModulationModes[y+StartY][x+StartX] = BlockModMode;
				ModulationVals[y+StartY][x+StartX] = ModulationBits & 3;
				ModulationBits >>= 2;
			}
		}
	}

	// make sure nothing is left over
	DebugAssert(ModulationBits==0);
}


template<bool do2bitMode>
static int GetUCoordPVR (int x)
{
	if (do2bitMode)
	{
		int u = (x & 0x7) | ((~x & 0x4) << 1);
		return u - kPVRSizeX2/2;
	}
	else
	{
		int u = (x & 0x3) | ((~x & 0x2) << 1);
		return u - kPVRSizeX4/2;
	}
}

static int GetVCoordPVR (int y)
{
	int v = (y & 0x3) | ((~y & 0x2) << 1);
	return v - kPVRBlockSizeY/2;
}


// Performs a HW bit accurate interpolation of either the
// A or B colors for a particular pixel
//
// NOTE: It is assumed that the source colors are in ARGB 5554 format -
//		 This means that some "preparation" of the values will be necessary.
// NOTE: QP is Q-P, SR is S-R
template<bool Do2bitMode>
static void InterpolateColoursPVRTC(const int* __restrict P,
						  const int* __restrict QP,
						  const int* __restrict R,
						  const int* __restrict SR,
						  const int u,
						  const int v,
						  int* __restrict Result)
{
	int k;
	int tmp1, tmp2;

	int uscale = Do2bitMode ? 8 : 4;

	for(k = 0; k < 4; k++)
	{
		tmp1 = P[k] * uscale + u * QP[k];
		tmp2 = R[k] * uscale + u * SR[k];

		tmp1 = tmp1 * 4 + v * (tmp2 - tmp1);

		Result[k] = tmp1;
	}

	/*
	// Lop off the appropriate number of bits to get us to 8 bit precision
	*/
	if(Do2bitMode)
	{
		/*
		// do RGB
		*/
		for(k = 0; k < 3; k++)
		{
			Result[k] >>= 2;
		}

		Result[3] >>= 1;
	}
	else
	{
		/*
		// do RGB  (A is ok)
		*/
		for(k = 0; k < 3; k++)
		{
			Result[k] >>= 1;
		}
	}

	/*
	// sanity check
	*/
	for(k = 0; k < 4; k++)
	{
		DebugAssert(Result[k] < 256);
	}


	/*
	// Convert from 5554 to 8888
	//
	// do RGB 5.3 => 8
	*/
	for(k = 0; k < 3; k++)
	{
		Result[k] += Result[k] >> 5;
	}
	Result[3] += Result[3] >> 4;

	/*
	// 2nd sanity check
	*/
	for(k = 0; k < 4; k++)
	{
		DebugAssert(Result[k] < 256);
	}

}


// Get the modulation value as a numerator of a fraction of 8ths
template<bool Do2bitMode>
static void GetModulationValue(int x,
							   int y,
							   const int ModulationVals[8][16],
							   const int ModulationModes[8][16],
							   int *Mod,
							   int *DoPT)
{
	static const int RepVals0[4] = {0, 3, 5, 8};
	static const int RepVals1[4] = {0, 4, 4, 8};

	int ModVal;

	/*
	// Map X and Y into the local 2x2 block
	*/
	y = (y & 0x3) | ((~y & 0x2) << 1);
	if(Do2bitMode)
	{
		x = (x & 0x7) | ((~x & 0x4) << 1);

	}
	else
	{
		x = (x & 0x3) | ((~x & 0x2) << 1);
	}

	/*
	// assume no PT for now
	*/
	*DoPT = 0;

	/*
	// extract the modulation value. If a simple encoding
	*/
	if(ModulationModes[y][x]==0)
	{
		ModVal = RepVals0[ModulationVals[y][x]];
	}
	else if(Do2bitMode)
	{
		/*
		// if this is a stored value
		*/
		if(((x^y)&1)==0)
		{
			ModVal = RepVals0[ModulationVals[y][x]];
		}
		/*
		// else average from the neighbours
		//
		// if H&V interpolation...
		*/
		else if(ModulationModes[y][x] == 1)
		{
			ModVal = (RepVals0[ModulationVals[y-1][x]] +
					  RepVals0[ModulationVals[y+1][x]] +
					  RepVals0[ModulationVals[y][x-1]] +
					  RepVals0[ModulationVals[y][x+1]] + 2) / 4;
		}
		/*
		// else if H-Only
		*/
		else if(ModulationModes[y][x] == 2)
		{
			ModVal = (RepVals0[ModulationVals[y][x-1]] +
					  RepVals0[ModulationVals[y][x+1]] + 1) / 2;
		}
		/*
		// else it's V-Only
		*/
		else
		{
			ModVal = (RepVals0[ModulationVals[y-1][x]] +
					  RepVals0[ModulationVals[y+1][x]] + 1) / 2;

		}/*end if/then/else*/
	}
	/*
	// else it's 4BPP and PT encoding
	*/
	else
	{
		ModVal = RepVals1[ModulationVals[y][x]];

		*DoPT = ModulationVals[y][x] == kPVRPunchThroughIndex;
	}

	*Mod =ModVal;
}



// PVRTC UV twiddling interleaves bits of Y & X, like: XYXYXYXYXY.
// If size of one coordinate is larger, those bits are just copied into higher bits;
// e.g. XXXX,YYYYYYYY = YYYYXYXYXYXY.

static UInt32 TwiddleY_PVRTC(UInt32 YSize, UInt32 XSize, UInt32 YPos)
{
	UInt32 Twiddled;

	UInt32 MinDimension;

	UInt32 SrcBitPos;
	UInt32 DstBitPos;

	int ShiftCount;

	DebugAssert(YPos < YSize);
	DebugAssert(IsPowerOfTwo(YSize));
	DebugAssert(IsPowerOfTwo(XSize));

	if (YSize < XSize)
		MinDimension = YSize;
	else
		MinDimension = XSize;

	// Step through all the bits in the "minimum" dimension
	SrcBitPos = 1;
	DstBitPos = 1;
	Twiddled  = 0;
	ShiftCount = 0;

	while (SrcBitPos < MinDimension)
	{
		if(YPos & SrcBitPos)
			Twiddled |= DstBitPos;

		SrcBitPos <<= 1;
		DstBitPos <<= 2;
		ShiftCount += 1;
	}

	// prepend any unused bits, if they were from Y
	if (YSize >= XSize)
	{
		YPos >>= ShiftCount;
		Twiddled |= (YPos << (2*ShiftCount));
	}

	return Twiddled;
}


static UInt32 TwiddleX_PVRTC(UInt32 YSize, UInt32 XSize, UInt32 XPos)
{
	UInt32 Twiddled;

	UInt32 MinDimension;

	UInt32 SrcBitPos;
	UInt32 DstBitPos;

	int ShiftCount;

	DebugAssert(XPos < XSize);
	DebugAssert(IsPowerOfTwo(YSize));
	DebugAssert(IsPowerOfTwo(XSize));

	if (YSize < XSize)
		MinDimension = YSize;
	else
		MinDimension = XSize;

	// Step through all the bits in the "minimum" dimension
	SrcBitPos = 1;
	DstBitPos = 2;
	Twiddled  = 0;
	ShiftCount = 0;

	while (SrcBitPos < MinDimension)
	{
		if (XPos & SrcBitPos)
		{
			Twiddled |= DstBitPos;
		}

		SrcBitPos <<= 1;
		DstBitPos <<= 2;
		ShiftCount += 1;
	}

	// prepend any unused bits, if they were from X
	if (YSize < XSize)
	{
		XPos >>= ShiftCount;
		Twiddled |= (XPos << (2*ShiftCount));
	}

	return Twiddled;
}



static UInt32 TwiddleUVPVRTC(UInt32 YSize, UInt32 XSize, UInt32 YPos, UInt32 XPos)
{
	return TwiddleY_PVRTC (YSize, XSize, YPos) + TwiddleX_PVRTC (YSize, XSize, XPos);
}



template<bool Do2bitMode, bool AssumeImageTiles>
static void DecompressPVRTC(const PVRTCBlock *pCompressedData,
				const int XDim,
				const int YDim,
				unsigned char* pResultImage)
{
	int BlkX, BlkY;
	int BlkXp1, BlkYp1;
	int BlkXDim, BlkYDim;

	int ModulationVals[8][16];
	int ModulationModes[8][16];

	int Mod, DoPT;

	// local neighbourhood of blocks
	const PVRTCBlock *pBlocks[2][2];

	const PVRTCBlock *pPrevious[2][2] = {{NULL, NULL}, {NULL, NULL}};

	// Low precision colors extracted from the blocks.
	// Rightmost colors have leftmost values subtracted from them.
	struct PVRBlockColors {
		int Reps[2][4];
	};
	PVRBlockColors Colours5554[2][2];

	// Interpolated A and B colours for the pixel
	int ASig[4], BSig[4];

	const int XBlockSize = Do2bitMode ? kPVRSizeX2 : kPVRSizeX4;


	// For MBX don't allow the sizes to get too small
	BlkXDim = std::max(2, XDim / XBlockSize);
	BlkYDim = std::max(2, YDim / kPVRBlockSizeY);

	// Step through the pixels of the image decompressing each one in turn
	//
	// Note that this is a hideously inefficient way to do this!
	for (int y = 0; y < YDim; y++)
	{
		BlkY = (y - kPVRBlockSizeY/2);
		BlkY = PVR_LIMIT_COORD(BlkY, YDim, AssumeImageTiles);
		BlkY /= kPVRBlockSizeY;
		//BlkY = PVR_LIMIT_COORD(BlkY, BlkYDim, AssumeImageTiles);
		BlkYp1 = PVR_LIMIT_COORD(BlkY+1, BlkYDim, AssumeImageTiles);

		// Since Y & X coordinates twiddle into separate bits,
		// we can compute Y block twiddle mask here for the whole row.
		const PVRTCBlock* blockPointerY = pCompressedData + TwiddleY_PVRTC (BlkYDim, BlkXDim, BlkY);
		const PVRTCBlock* blockPointerY1 = pCompressedData + TwiddleY_PVRTC (BlkYDim, BlkXDim, BlkYp1);

		int coordV = GetVCoordPVR(y);

		for (int x = 0; x < XDim; x++)
		{
			// map this pixel to the top left neighbourhood of blocks
			BlkX = (x - XBlockSize/2);
			BlkX = PVR_LIMIT_COORD(BlkX, XDim, AssumeImageTiles);
			BlkX /= XBlockSize;
			//BlkX = PVR_LIMIT_COORD(BlkX, BlkXDim, AssumeImageTiles);


			// compute the positions of the other 3 blocks
			BlkXp1 = PVR_LIMIT_COORD(BlkX+1, BlkXDim, AssumeImageTiles);

			// Map to block memory locations

			// block offsets, X bits
			UInt32 blockOffsetX  = TwiddleX_PVRTC (BlkYDim, BlkXDim, BlkX);
			UInt32 blockOffsetX1 = TwiddleX_PVRTC (BlkYDim, BlkXDim, BlkXp1);

			pBlocks[0][0] = blockPointerY + blockOffsetX;
			pBlocks[0][1] = blockPointerY + blockOffsetX1;
			pBlocks[1][0] = blockPointerY1 + blockOffsetX;
			pBlocks[1][1] = blockPointerY1 + blockOffsetX1;


			// extract the colours and the modulation information IF the previous values
			// have changed.
			if (pPrevious[0][0]!=pBlocks[0][0]||pPrevious[0][1]!=pBlocks[0][1]||
				pPrevious[1][0]!=pBlocks[1][0]||pPrevious[1][1]!=pBlocks[1][1])
			{
				int StartY = 0;
				for (int i = 0; i < 2; i++)
				{
					int StartX = 0;
					for (int j = 0; j < 2; j++)
					{
						Unpack5554Colour(pBlocks[i][j], Colours5554[i][j].Reps);

						UnpackModulations<Do2bitMode>(pBlocks[i][j],
										  ModulationVals,
										  ModulationModes,
										  StartX, StartY);

						StartX += XBlockSize;
					} // end for j

					// for rightmost color block, subtract leftmost colors now
					for (int ab = 0; ab < 2; ++ab)
					{
						for (int c = 0; c < 4; ++c)
							Colours5554[i][1].Reps[ab][c] -= Colours5554[i][0].Reps[ab][c];
					}

					StartY += kPVRBlockSizeY;
				} // end for i

				// make a copy of the new pointers
				pPrevious[0][0] = pBlocks[0][0];
				pPrevious[0][1] = pBlocks[0][1];
				pPrevious[1][0] = pBlocks[1][0];
				pPrevious[1][1] = pBlocks[1][1];
			} // end if the blocks have changed


			// decompress the pixel.  First compute the interpolated A and B signals
			int coordU = GetUCoordPVR<Do2bitMode>(x);
			InterpolateColoursPVRTC<Do2bitMode>(Colours5554[0][0].Reps[0],
							   Colours5554[0][1].Reps[0],
							   Colours5554[1][0].Reps[0],
							   Colours5554[1][1].Reps[0],
							   coordU, coordV,
							   ASig);

			InterpolateColoursPVRTC<Do2bitMode>(Colours5554[0][0].Reps[1],
							   Colours5554[0][1].Reps[1],
							   Colours5554[1][0].Reps[1],
							   Colours5554[1][1].Reps[1],
							   coordU, coordV,
							   BSig);

			GetModulationValue<Do2bitMode>(x,y, ModulationVals, ModulationModes,
							   &Mod, &DoPT);


			// compute the modulated color and store in output image
			for (int i = 0; i < 4; i++)
			{
				int res = (ASig[i] * 8 + Mod * (BSig[i] - ASig[i])) >> 3;
				pResultImage[i] = (UInt8)res;
			}
			if (DoPT)
			{
				pResultImage[3] = 0;
			}
			pResultImage += 4;

		} // end for x
	} // end for y
}
#endif




// ------------------------------------------------------------------------
//  ETC



#if HAS_ETC_DECOMPRESSOR

#define GETBITS(source, size, startpos)  (( (source) >> ((startpos)-(size)+1) ) & ((1<<(size)) -1))
#define GETBITSHIGH(source, size, startpos)  (( (source) >> (((startpos)-32)-(size)+1) ) & ((1<<(size)) -1))
inline int clampUByte(int v) { return (v < 0) ? 0 : ((v > 255) ? 255 : v); }

inline void STORE_RGB(UInt32* img, int width, int x, int y, int r, int g, int b)
{
	Assert(r >= 0 && r <= 255);
	Assert(g >= 0 && g <= 255);
	Assert(b >= 0 && b <= 255);
	Assert(x >= 0 && x < width);
	img[y*width + x] = (0xFF<<24) | (r<<0) | (g<<8) | (b<<16);
}

void decompressBlockDiffFlip(unsigned int block_part1, unsigned int block_part2, UInt32* dstImg, int width, int startx, int starty)
{
	static const int unscramble[4] = {2, 3, 1, 0};

	int compressParams[16][4];

	compressParams[0][0]  =  -8; compressParams[0][1]  =  -2; compressParams[0][2]  =  2; compressParams[0][3]  =   8;
	compressParams[1][0]  =  -8; compressParams[1][1]  =  -2; compressParams[1][2]  =  2; compressParams[1][3]  =   8;
	compressParams[2][0]  = -17; compressParams[2][1]  =  -5; compressParams[2][2]  =  5; compressParams[2][3]  =  17;
	compressParams[3][0]  = -17; compressParams[3][1]  =  -5; compressParams[3][2]  =  5; compressParams[3][3]  =  17;
	compressParams[4][0]  = -29; compressParams[4][1]  =  -9; compressParams[4][2]  =  9; compressParams[4][3]  =  29;
	compressParams[5][0]  = -29; compressParams[5][1]  =  -9; compressParams[5][2]  =  9; compressParams[5][3]  =  29;
	compressParams[6][0]  = -42; compressParams[6][1]  = -13; compressParams[6][2]  = 13; compressParams[6][3]  =  42;
	compressParams[7][0]  = -42; compressParams[7][1]  = -13; compressParams[7][2]  = 13; compressParams[7][3]  =  42;
	compressParams[8][0]  = -60; compressParams[8][1]  = -18; compressParams[8][2]  = 18; compressParams[8][3]  =  60;
	compressParams[9][0]  = -60; compressParams[9][1]  = -18; compressParams[9][2]  = 18; compressParams[9][3]  =  60;
	compressParams[10][0] = -80; compressParams[10][1] = -24; compressParams[10][2] = 24; compressParams[10][3] =  80;
	compressParams[11][0] = -80; compressParams[11][1] = -24; compressParams[11][2] = 24; compressParams[11][3] =  80;
	compressParams[12][0] =-106; compressParams[12][1] = -33; compressParams[12][2] = 33; compressParams[12][3] = 106;
	compressParams[13][0] =-106; compressParams[13][1] = -33; compressParams[13][2] = 33; compressParams[13][3] = 106;
	compressParams[14][0] =-183; compressParams[14][1] = -47; compressParams[14][2] = 47; compressParams[14][3] = 183;
	compressParams[15][0] =-183; compressParams[15][1] = -47; compressParams[15][2] = 47; compressParams[15][3] = 183;

	UInt8 avg_color[3], enc_color1[3], enc_color2[3];
	signed char diff[3];
	int table;
	int index,shift;
	int r,g,b;
	int diffbit;
	int flipbit;

	diffbit = (GETBITSHIGH(block_part1, 1, 33));
	flipbit = (GETBITSHIGH(block_part1, 1, 32));

	if( !diffbit )
	{

		// We have diffbit = 0.

		// First decode left part of block.
		avg_color[0]= GETBITSHIGH(block_part1, 4, 63);
		avg_color[1]= GETBITSHIGH(block_part1, 4, 55);
		avg_color[2]= GETBITSHIGH(block_part1, 4, 47);

		// Here, we should really multiply by 17 instead of 16. This can
		// be done by just copying the four lower bits to the upper ones
		// while keeping the lower bits.
		avg_color[0] |= (avg_color[0] <<4);
		avg_color[1] |= (avg_color[1] <<4);
		avg_color[2] |= (avg_color[2] <<4);

		table = GETBITSHIGH(block_part1, 3, 39) << 1;


		unsigned int pixel_indices_MSB, pixel_indices_LSB;

		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift = 0;
			for(int x=startx; x<startx+2; x++)
			{
				for(int y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
			}
		}
		else
		{
			// We should flip
			shift = 0;
			for(int x=startx; x<startx+4; x++)
			{
				for(int y=starty; y<starty+2; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
				shift+=2;
			}
		}

		// Now decode other part of block.
		avg_color[0]= GETBITSHIGH(block_part1, 4, 59);
		avg_color[1]= GETBITSHIGH(block_part1, 4, 51);
		avg_color[2]= GETBITSHIGH(block_part1, 4, 43);

		// Here, we should really multiply by 17 instead of 16. This can
		// be done by just copying the four lower bits to the upper ones
		// while keeping the lower bits.
		avg_color[0] |= (avg_color[0] <<4);
		avg_color[1] |= (avg_color[1] <<4);
		avg_color[2] |= (avg_color[2] <<4);

		table = GETBITSHIGH(block_part1, 3, 36) << 1;
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift=8;
			for(int x=startx+2; x<startx+4; x++)
			{
				for(int y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
			}
		}
		else
		{
			// We should flip
			shift=2;
			for(int x=startx; x<startx+4; x++)
			{
				for(int y=starty+2; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
				shift += 2;
			}
		}

	}
	else
	{
		// We have diffbit = 1.

		//      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34  33  32
		//      ---------------------------------------------------------------------------------------------------
		//     | base col1    | dcol 2 | base col1    | dcol 2 | base col 1   | dcol 2 | table  | table  |diff|flip|
		//     | R1' (5 bits) | dR2    | G1' (5 bits) | dG2    | B1' (5 bits) | dB2    | cw 1   | cw 2   |bit |bit |
		//      ---------------------------------------------------------------------------------------------------
		//
		//
		//     c) bit layout in bits 31 through 0 (in both cases)
		//
		//      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
		//      --------------------------------------------------------------------------------------------------
		//     |       most significant pixel index bits       |         least significant pixel index bits       |
		//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
		//      --------------------------------------------------------------------------------------------------


		// First decode left part of block.
		enc_color1[0]= GETBITSHIGH(block_part1, 5, 63);
		enc_color1[1]= GETBITSHIGH(block_part1, 5, 55);
		enc_color1[2]= GETBITSHIGH(block_part1, 5, 47);


		// Expand from 5 to 8 bits
		avg_color[0] = (enc_color1[0] <<3) | (enc_color1[0] >> 2);
		avg_color[1] = (enc_color1[1] <<3) | (enc_color1[1] >> 2);
		avg_color[2] = (enc_color1[2] <<3) | (enc_color1[2] >> 2);


		table = GETBITSHIGH(block_part1, 3, 39) << 1;

		unsigned int pixel_indices_MSB, pixel_indices_LSB;

		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift = 0;
			for(int x=startx; x<startx+2; x++)
			{
				for(int y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
			}
		}
		else
		{
			// We should flip
			shift = 0;
			for(int x=startx; x<startx+4; x++)
			{
				for(int y=starty; y<starty+2; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
				shift+=2;
			}
		}


		// Now decode right part of block.


		diff[0]= GETBITSHIGH(block_part1, 3, 58);
		diff[1]= GETBITSHIGH(block_part1, 3, 50);
		diff[2]= GETBITSHIGH(block_part1, 3, 42);

		enc_color2[0]= enc_color1[0] + diff[0];
		enc_color2[1]= enc_color1[1] + diff[1];
		enc_color2[2]= enc_color1[2] + diff[2];

		// Extend sign bit to entire byte.
		diff[0] = (diff[0] << 5);
		diff[1] = (diff[1] << 5);
		diff[2] = (diff[2] << 5);
		diff[0] = diff[0] >> 5;
		diff[1] = diff[1] >> 5;
		diff[2] = diff[2] >> 5;

		//  Calculale second color
		enc_color2[0]= enc_color1[0] + diff[0];
		enc_color2[1]= enc_color1[1] + diff[1];
		enc_color2[2]= enc_color1[2] + diff[2];

		// Expand from 5 to 8 bits
		avg_color[0] = (enc_color2[0] <<3) | (enc_color2[0] >> 2);
		avg_color[1] = (enc_color2[1] <<3) | (enc_color2[1] >> 2);
		avg_color[2] = (enc_color2[2] <<3) | (enc_color2[2] >> 2);


		table = GETBITSHIGH(block_part1, 3, 36) << 1;
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift=8;
			for(int x=startx+2; x<startx+4; x++)
			{
				for(int y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
			}
		}
		else
		{
			// We should flip
			shift=2;
			for(int x=startx; x<startx+4; x++)
			{
				for(int y=starty+2; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

					r=clampUByte(avg_color[0]+compressParams[table][index]);
					g=clampUByte(avg_color[1]+compressParams[table][index]);
					b=clampUByte(avg_color[2]+compressParams[table][index]);
					STORE_RGB(dstImg, width, x, y, r, g, b);
				}
				shift += 2;
			}
		}
	}
}


static void DecompressETC_RGB4 ( int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* m_pDecompBytes )
{
	const UInt8*	srcPtr = (const UInt8*)m_pCompBytes;
	UInt32*			dstPtr = m_pDecompBytes;

	for (int y = 0; y < yblocks; y++)
	{
		for (int x = 0; x < xblocks; x++)
		{
			UInt32 part1 = (srcPtr[0]<<24) | (srcPtr[1]<<16) | (srcPtr[2]<<8) | (srcPtr[3]<<0);
			UInt32 part2 = (srcPtr[4]<<24) | (srcPtr[5]<<16) | (srcPtr[6]<<8) | (srcPtr[7]<<0);
			decompressBlockDiffFlip(part1, part2, dstPtr, destWidth, x*4, y*4);
			srcPtr += 8;
		}
	}
}

#endif // HAS_ETC_DECOMPRESSOR


#if HAS_ATC_DECOMPRESSOR

template<int textureFormat>
static void DecompressATC ( int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* m_pDecompBytes )
{
	bool doAlpha = (textureFormat == kTexFormatATC_RGBA8);

	TQonvertImage src, dst;
	src.nWidth = xblocks * 4;
	src.nHeight = yblocks * 4;
	src.nFormat = doAlpha ? Q_FORMAT_ATC_RGBA_INTERPOLATED_ALPHA : Q_FORMAT_ATC_RGB;
	src.pFormatFlags = NULL;
	src.nDataSize = CalculateImageSize (src.nWidth, src.nHeight, textureFormat);
	src.pData = (unsigned char*)m_pCompBytes;

	dst.nWidth = destWidth;
	dst.nHeight = yblocks * 4;
	dst.nFormat = Q_FORMAT_RGBA_8888;
	dst.pFormatFlags = NULL;
	dst.nDataSize = CalculateImageSize (dst.nWidth, dst.nHeight, kTexFormatRGBA32);
	dst.pData = (unsigned char*)m_pDecompBytes;

	int nRet = Qonvert(&src, &dst);
	if (nRet != Q_SUCCESS)
	{
		ErrorStringMsg("DecompressETC2 failed with nRet = %i", nRet);
	}
}

#endif // HAS_ATC_DECOMPRESSOR

#if HAS_ASTC_DECOMPRESSOR

// ASTC decoding

#include "External/astcenc/Source/astc_codec_internals.h"

// Needed by astcenc
void astc_codec_internal_error(const char *filename, int linenum)
{
	printf_console("ASTCenc: Internal error: File=%s Line=%d\n", filename, linenum);
}
// Define an unlink() function in terms of the Unity DeleteFile function.
int astc_codec_unlink(const char *filename)
{
/*	bool res = DeleteFile(filename);
	return (res ? 0 : -1);*/

	// Not used in runtime
	return -1;
}
int rgb_force_use_of_hdr = 0;
int alpha_force_use_of_hdr = 0;
int perform_srgb_transform = 0;
int print_tile_errors = 0;

// TODO: Make threadsafe when we have multithreaded renderer. Doesn't really hurt to do multiple inits though.
static int astcenc_initialized = 0;

static void DecompressASTC(const UInt32 *srcData, int destWidth, int destHeight, UInt32 *destData, int blockWidth, int blockHeight)
{
	swizzlepattern swz_decode = { 0, 1, 2, 3 };

	if(!astcenc_initialized)
	{
		// initialization routines
		prepare_angular_tables();
		build_quantization_mode_table();
		astcenc_initialized = 1;
	}


	astc_codec_image *img = allocate_image(8, destWidth, destHeight, 1, 0);
	initialize_image(img);

	const int zblocks = 1;
	const int yblocks = CeilfToInt((float)destHeight / (float)blockHeight);
	const int xblocks = CeilfToInt((float)destWidth / (float)blockWidth);

	const int xdim = blockWidth;
	const int ydim = blockHeight;
	const int zdim = 1;
	
	int x, y, z;
	
	imageblock pb;
	for (z = 0; z < zblocks; z++)
		for (y = 0; y < yblocks; y++)
			for (x = 0; x < xblocks; x++)
			{
				int offset = (((z * yblocks + y) * xblocks) + x) * 16;
				uint8_t *bp = ((uint8_t *)srcData) + offset;
				physical_compressed_block pcb = *(physical_compressed_block *) bp;
				symbolic_compressed_block scb;
				physical_to_symbolic(xdim, ydim, zdim, pcb, &scb);
				decompress_symbolic_block(DECODE_LDR, xdim, ydim, zdim, x * xdim, y * ydim, z * zdim, &scb, &pb);
				write_imageblock(img, &pb, xdim, ydim, zdim, x * xdim, y * ydim, z * zdim, swz_decode);
			}

	for(y = 0; y < destHeight; y++)
	{
		memcpy(&destData[y*destWidth], img->imagedata8[0][y], destWidth * 4);
	}
	destroy_image(img);

}

#endif // HAS_ASTC_DECOMPRESSOR

// ------------------------------------------------------------------------
//  Common


bool DecompressNativeTextureFormatWithMipLevel( TextureFormat srcFormat, int srcWidth, int srcHeight, int mipLevel, const UInt32* sourceData,
								   int destWidth, int destHeight, UInt32* destData )
{

#if HAS_FLASH_ATF_DECOMPRESSOR
	// Flash requires that we pass the mipLevel explicitly
	if (IsCompressedFlashATFTextureFormat(srcFormat))
	{
		DecompressFlashATFTexture((const UInt8*)sourceData, destWidth, destHeight, mipLevel, (UInt8*)destData);
		return true;
	}
#endif

	return DecompressNativeTextureFormat (srcFormat, srcWidth, srcHeight, (UInt32*)sourceData, destWidth, destHeight, destData);
}



bool DecompressNativeTextureFormat( TextureFormat srcFormat, int srcWidth, int srcHeight, const UInt32* sourceData,
	int destWidth, int destHeight, UInt32* destData )
{
	Assert( IsAnyCompressedTextureFormat(srcFormat) );
	Assert( destWidth >= srcWidth && destHeight >= srcHeight );
	Assert( destWidth % 4 == 0 && destHeight % 4 == 0 );

#if UNITY_XENON || HAS_DXT_DECOMPRESSOR || HAS_ETC_DECOMPRESSOR || HAS_ATC_DECOMPRESSOR
	int xblocks = (srcWidth + 3) / 4;
	int yblocks = (srcHeight + 3) / 4;
#endif

	const UInt32* srcData = sourceData;

	#if UNITY_XENON
		UInt32 dataSize = (xblocks * yblocks * ((kTexFormatDXT1 == srcFormat) ? 8 : 16)) >> 1;
		UInt16 *s = (UInt16*)srcData;
		UInt16 *d = new UInt16[dataSize];
		for(int i=0;i<dataSize;i++)
			d[i]=GetByteSwap16(s[i]);
		srcData = (UInt32*)d;
	#endif

	#if PRINT_DECOMPRESSION_TIMES
	double t0 = GetTimeSinceStartup();
	#endif


	switch( srcFormat )
	{
	#if HAS_DXT_DECOMPRESSOR
	case kTexFormatDXT1: DecompressDXT1( xblocks, yblocks, destWidth, srcData, destData ); break;
	case kTexFormatDXT3: DecompressDXT3( xblocks, yblocks, destWidth, srcData, destData ); break;
	case kTexFormatDXT5: DecompressDXT5( xblocks, yblocks, destWidth, srcData, destData ); break;
	#endif

	#if HAS_ETC_DECOMPRESSOR
	case kTexFormatETC_RGB4: DecompressETC_RGB4(xblocks, yblocks, destWidth, srcData, destData ); break;
	#endif

	#if HAS_ATC_DECOMPRESSOR
	case kTexFormatATC_RGB4:  DecompressATC<kTexFormatATC_RGB4>(xblocks, yblocks, destWidth, srcData, destData); break;
	case kTexFormatATC_RGBA8: DecompressATC<kTexFormatATC_RGBA8>(xblocks, yblocks, destWidth, srcData, destData); break;
	#endif

	case kTexFormatETC2_RGB:	DecompressETC2_RGB8		((UInt8*)destData, (const UInt8*)srcData, destWidth, destHeight);		break;
	case kTexFormatETC2_RGBA1:	DecompressETC2_RGB8_A1	((UInt8*)destData, (const UInt8*)srcData, destWidth, destHeight);		break;
	case kTexFormatETC2_RGBA8:	DecompressETC2_RGBA8	((UInt8*)destData, (const UInt8*)srcData, destWidth, destHeight);		break;

	#if HAS_PVRTC_DECOMPRESSOR
	case kTexFormatPVRTC_RGB4:
	case kTexFormatPVRTC_RGBA4: DecompressPVRTC<false,true>( reinterpret_cast<const PVRTCBlock*>(srcData), srcWidth, srcHeight, reinterpret_cast<unsigned char*> (destData) ); break;

	case kTexFormatPVRTC_RGB2:
	case kTexFormatPVRTC_RGBA2: DecompressPVRTC<true,true>( reinterpret_cast<const PVRTCBlock*>(srcData), srcWidth, srcHeight, reinterpret_cast<unsigned char*> (destData) ); break;
	#endif

	#if HAS_FLASH_ATF_DECOMPRESSOR
	case kTexFormatFlashATF_RGB_DXT1:
	case kTexFormatFlashATF_RGB_JPG:
	case kTexFormatFlashATF_RGBA_JPG:
		DecompressFlashATFTexture ((const UInt8*)srcData, destWidth, destHeight, 0, (UInt8*)destData);
		break;
	#endif

#if HAS_ASTC_DECOMPRESSOR
#define DO_ASTC(bx,by) case kTexFormatASTC_RGB_##bx##x##by :  case kTexFormatASTC_RGBA_##bx##x##by : DecompressASTC(srcData, destWidth, destHeight, destData, bx, by); break

		DO_ASTC(4, 4);
		DO_ASTC(5, 5);
		DO_ASTC(6, 6);
		DO_ASTC(8, 8);
		DO_ASTC(10, 10);
		DO_ASTC(12, 12);

#undef DO_ASTC
#endif

	default:
		AssertString( "Unknown compressed texture format!" );
		#if UNITY_XENON
			delete[] srcData;
		#endif
		return false;
	}

#if UNITY_XENON
	delete[] srcData;
#endif

	#if PRINT_DECOMPRESSION_TIMES
	double t1 = GetTimeSinceStartup();
	if (srcWidth >= 512 && srcHeight >= 512)
	{
		printf_console("Decompress %x size %ix%i fmt %i time %.3fs\n", srcData, srcWidth, srcHeight, (int)srcFormat, t1-t0);
	}
	#endif

	return true;
}



// ------------------------------------------------------------------------
//  Unit Tests



#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

SUITE (ImageDecompressionTests)
{
	#if UNITY_LITTLE_ENDIAN
	TEST (DecodeDXT5AlphaPalette8a)
	{
		UInt32 res[16];
		memset (res, 0xCC, sizeof(res));
		UInt8 b[] = { 17, 13, 177, 109, 155, 54, 105, 82, 255 };
		DecodeAlpha3BitLinear (res, *(DXTAlphaBlock3BitLinear*)b, 4, 0x00FFFFFF);
		UInt8 r[] = { 13,14,14,14, 14,14,14,15, 14,14,15,15, 14,15,15,16 };
		for (int i = 0; i < 16; ++i) {
			CHECK_EQUAL ((int)r[i], (res[i]&0xFF000000)>>24);
		}
	}
	TEST (DecodeDXT5AlphaPalette8b)
	{
		UInt32 res[16];
		memset (res, 0xCD, sizeof(res));
		UInt8 b[] = { 251, 5, 179, 109, 113, 54, 107, 84, 255 };
		DecodeAlpha3BitLinear (res, *(DXTAlphaBlock3BitLinear*)b, 4, 0x00FFFFFF);
		UInt8 r[] = { 181,75,75,75, 75,216,146,181, 75,75,146,110, 75,251,110,216 };
		for (int i = 0; i < 16; ++i) {
			CHECK_EQUAL ((int)r[i], (res[i]&0xFF000000)>>24);
		}
	}
	TEST (DecodeDXT5AlphaPalette6)
	{
		UInt32 res[16];
		memset (res, 0xCC, sizeof(res));
		UInt8 b[] = { 15, 18, 0, 4, 72, 144, 8, 137, 255 };
		DecodeAlpha3BitLinear (res, *(DXTAlphaBlock3BitLinear*)b, 4, 0x00FFFFFF);
		UInt8 r[] = { 15,15,15,16, 15,15,16,16, 15,16,16,17, 15,16,16,17 };
		for (int i = 0; i < 16; ++i) {
			CHECK_EQUAL ((int)r[i], (res[i]&0xFF000000)>>24);
		}
	}
	#if HAS_PVRTC_DECOMPRESSOR
	TEST (DecodePVRTC_2_16x16)
	{
		const int kSize = 16;
		UInt32 src[kSize*kSize*2/32] = {
			0xeeeeeeee, 0x83eed400, 0xeeeeeeee, 0x8befb006, 0xfefefefe, 0xeed9801e, 0x00fefefe, 0xcf18801e,
			0x0eeeeeee, 0x9ff4fc00, 0x00fefefe, 0xaff58000, 0x00ffffff, 0x83add404, 0x00ffffff, 0xb77d8000,
		};
		UInt32 expected[kSize*kSize] = {
			0xff3f002b,0xffadeb59,0xffa3f24b,0xff99f83b,0xff000056,0xff99f83b,0xffa3f24b,0xffadeb59,0xff3f002b,0xffc0dd77,0xffcbd787,0xffd4d095,0xffdecaa5,0xffd4d095,0xffcbd787,0xffc0dd77,
			0xff5f0040,0xffa0e756,0xff96ef40,0xff8cf72b,0xff000081,0xff8cf72b,0xff96ef40,0xffa0e756,0xff5f0040,0xffb6d780,0xffc0cf96,0xffcbc7ac,0xffd6bfc1,0xffcbc7ac,0xffc0cf96,0xffb6d780,
			0xff7f0056,0xff95e353,0xff8aed37,0xff7ef61b,0xff0000ad,0xff7ef61b,0xff8aed37,0xff95e353,0xff7f0056,0xffacd18b,0xffb7c8a7,0xffc2bec2,0xffceb5de,0xffc2bec2,0xffb7c8a7,0xffacd18b,
			0xff86004d,0xff95e44f,0xff8bee36,0xff7ff61c,0xff0c009a,0xff7ff61c,0xff8bee36,0xff95e44f,0xff86004d,0xffabd381,0xffb6cb9b,0xffc0c1b4,0xffccb9ce,0xffc0c1b4,0xffb6cb9b,0xffabd381,
			0xff8c0044,0xff96e74c,0xff8cef35,0xff81f71e,0xff180088,0xff81f71e,0xff8cef35,0xff96e74c,0xff8c0044,0xffabd679,0xffb5ce90,0xffbfc6a7,0xffcabdbd,0xffbfc6a7,0xffb5ce90,0xffabd679,
			0xff92003a,0xff96e848,0xff8df034,0xff82f71f,0xff250075,0xff82f71f,0xff8df034,0xff96e848,0xff92003a,0xffaad870,0xffb4d185,0xffbdc998,0xffc8c1ad,0xffbdc998,0xffb4d185,0xffaad870,
			0xff980031,0xff97ea45,0xff8ef133,0xff85f822,0xff310063,0xff85f822,0xff8ef133,0xff97ea45,0xff980031,0xffaadb68,0xffb3d479,0xffbccd8b,0xffc6c69c,0xffbccd8b,0xffb3d479,0xffaadb68,
			0xff76005a,0xff95ed3c,0xff90f331,0xff8bf926,0xff25008a,0xff8bf926,0xff90f331,0xff95ed3c,0xff76005a,0xff8a004e,0xff9e0043,0xffb30036,0xffc8002b,0xffb30036,0xff9e0043,0xff8a004e,
			0xff540084,0xff93f134,0xff92f62f,0xff91fa2a,0xff1800b1,0xff91fa2a,0xff92f62f,0xff93f134,0xff94ed39,0xff95e83e,0xff96e344,0xff97de49,0xff98da4e,0xff97de49,0xff96e344,0xff95e83e,
			0xff3200ad,0xff91f52c,0xff94f82d,0xff97fb2e,0xff0c00d8,0xff97fb2e,0xff94f82d,0xff91f52c,0xff8ef22b,0xff8bee2a,0xff88eb29,0xff85e828,0xff81e427,0xff85e828,0xff88eb29,0xff8bee2a,
			0xff1000d6,0xff8ff924,0xff96fb2b,0xff9dfd32,0xff0000ff,0xff9dfd32,0xff96fb2b,0xff8ff924,0xff88f71c,0xff80f515,0xff79f30e,0xff72f107,0xff6bef00,0xff72f107,0xff79f30e,0xff80f515,
			0xff0c00a0,0xff9cf732,0xff9ffa37,0xffa2fc3c,0xff0000bf,0xff0300b7,0xff0600b0,0xff0900a8,0xff0c00a0,0xff0f0098,0xff120091,0xff150089,0xff180081,0xff150089,0xff120091,0xff0f0098,
			0xff08006b,0xffaaf642,0xffaaf945,0xffa9fc47,0xffa9ff4a,0xffa9fc47,0xffaaf945,0xffaaf642,0xffabf33f,0xffabf03c,0xffaced3a,0xffacea37,0xffade735,0xffacea37,0xffaced3a,0xffabf03c,
			0xff040035,0xffb7f451,0xffb3f851,0xffaffb51,0xffabff52,0xffaffb51,0xffb3f851,0xffb7f451,0xffbcf151,0xffc0ed50,0xffc4ea50,0xffc9e550,0xffcee250,0xffc9e550,0xffc4ea50,0xffc0ed50,
			0xff000000,0xffc6f360,0xffbdf75e,0xffb5fb5c,0xffadff5a,0xffb5fb5c,0xffbdf75e,0xffc6f360,0xffceef63,0xffd6eb65,0xffdee767,0xffe7e269,0xffefde6b,0xffe7e269,0xffdee767,0xffd6eb65,
			0xff1f0015,0xff17001a,0xff0f001f,0xff070025,0xff00002b,0xff070025,0xff0f001f,0xff17001a,0xff1f0015,0xff27000f,0xff2f000a,0xff370005,0xff3f0000,0xff370005,0xff2f000a,0xff27000f,
		};
		UInt32 res[kSize*kSize];
		DecompressPVRTC<true,true> (reinterpret_cast<const PVRTCBlock*>(src), kSize, kSize, reinterpret_cast<UInt8*>(res));
		CHECK_ARRAY_EQUAL (expected, res, kSize*kSize);
	}
	TEST (DecodePVRTC_4_16x16)
	{
		const int kSize = 16;
		UInt32 src[kSize*kSize*4/32] = {
			0x32323230,0x7faa30e7,0x32323232,0x7fbc40f9,0x03030303,0x050230f6,0x03030303,0x060330f4,
			0x32323232,0x7faa40f7,0xa802f232,0xffff30e7,0xff030303,0x0f0040e6,0xaa00ff00,0xff9f40e9,
			0x0303035b,0x300f6aca,0xff030303,0x300f68ca,0x409094aa,0x68af5bba,0xff000040,0x200f58ca,
			0xff000000,0x2c0140e6,0xaa00ff00,0xffff41db,0xff000000,0x1c0140e8,0xaa00ff00,0xffff40bb,
		};
		UInt32 expected[kSize*kSize] = {
			0x7f92d62f,0x727ee217,0xf6d2d6ff,0x6a70f100,0xbbb1a5d4,0x727af500,0x777ff700,0x838eef17,0x99c67994,0xa2cca070,0xadd1b56a,0xaed1b971,0xbbe2b688,0xc1e2c193,0xc8e2cc9f,0xc6d4d19f,
			0x00b6cb8d,0x747ee223,0xf2bbc1ff,0x686df200,0x998a7dbe,0x6c72f800,0x6e75fb00,0x8187f121,0x66a93c5e,0xa7aadc64,0xbbbbd286,0xb4bbcd88,0xaebbc88a,0xb2d2b58c,0xb5d2be98,0xc2d0c49e,
			0x00b1c192,0x777fe22f,0xeea5adff,0x666bf300,0x776356a9,0x666bfb00,0x666bff00,0x7f7ff32b,0x338c0029,0xb2a9da81,0xccbdcead,0xc3bdcab1,0xbbbdc6b5,0xb2bdc1b9,0xb6d2b7aa,0xbdccb79e,
			0x00b6b986,0x7d89e72c,0xeeadb1ff,0x6c72f600,0x776958ab,0x6868fc00,0x6663ff00,0x7f79f329,0x338e002b,0xb2a7da7b,0xccbdcea5,0xc3bdcba8,0xbbbdc8ab,0xb2bdc4ae,0xaabdc1b1,0xa7c1b588,
			0x00bbb27a,0x8392eb29,0xeeb5b5ff,0x7279f900,0x776f5aad,0x6a65fd00,0x665aff00,0x7f73f327,0x3390002d,0xb2a5da75,0xccbdce9c,0xc3bdcc9e,0xbbbdcaa0,0xb2bdc8a2,0xaabdc6a5,0x9ec4ac78,
			0x00c0aa6e,0x8a9bef26,0xeebdb9ff,0x7980fc00,0x77755caf,0x6c61fe00,0x6652ff00,0x7f6df325,0x3392002f,0xb2a2da6f,0xccbdce94,0xc3bdcd95,0xbbbdcc96,0xb2bdcb97,0xaabdca98,0x9fb2d672,
			0x00c5a262,0x90a5f323,0xeec6bdff,0x7f88ff00,0x777b5eb1,0x6e5eff00,0x664aff00,0x7f67f323,0x33940031,0xb2a0da69,0xccbdce8c,0xc3bdce8c,0xbbbdce8c,0xb2bdce8c,0xaabdce8c,0xa1b5da69,
			0x00afa366,0x8e99f519,0xeebdb9ff,0x8180fe00,0x77715cc4,0x7461fc00,0x6e52fb00,0x8168f219,0x2e71005e,0x46970049,0x5dbd0033,0x55bd0033,0x4cbd0033,0x44bd0033,0x3bbd0033,0x68bd2e66,
			0x009aa46b,0x8c8ef711,0xeeb5b5ff,0x8379fd00,0x77675ad8,0x7b65f900,0x775af700,0x8369f111,0x9077eb23,0x9d86e434,0xaa94de46,0xa59ade46,0xa1a0de46,0x9da7de46,0x99adde46,0x94a2e734,
			0x0084a56f,0x8a82f908,0xeeadb1ff,0x8572fc00,0x775c58ec,0x8168f600,0x7f63f300,0x856af008,0x8c71ed11,0x9278ea19,0x997fe723,0x9689e723,0x9492e723,0x929be723,0x90a5e723,0x8e99ed19,
			0x006fa673,0x8877fb00,0xeea5adff,0x886bfb00,0x775256ff,0x886bf300,0x886bef00,0x886bef00,0x886bef00,0x886bef00,0x886bef00,0x8877ef00,0x8884ef00,0x8890ef00,0x889cef00,0x8890f300,
			0x0085b776,0x8178f500,0xf2bbc1ff,0x816df800,0x997d7dff,0x6c5e5bff,0x3f3f39ff,0x4c433af6,0x59463ced,0x66493de3,0x724c3fda,0x6c4c3fda,0x664c3fda,0x5f4c3fda,0x594c3fda,0x7f685fe3,
			0x009bc979,0x7b7aef00,0xf6d2d6ff,0x7b70f500,0x7f75f300,0x837af100,0x887fef00,0x8884ed02,0x8888eb04,0x888ce906,0x8890e708,0x8896e206,0x889cde04,0x88a2da02,0x88a9d600,0x8399de00,
			0x00b1d97c,0x747be900,0xfae9ebff,0xebdedbff,0xddd4ccff,0xcecabcff,0xbfbfadff,0xc3c0b1fc,0xc7c1b6f9,0xccc2baf6,0xd0c3bff3,0xcec3bff3,0xccc3bff3,0xc9c3bff3,0xc7c3bff3,0xd4cdcaf6,
			0x00c7ea7f,0x6e7de200,0x666bef00,0x6e75ef00,0x777fef00,0x7f8aef00,0x8894ef00,0x889ceb04,0x88a5e708,0x88ade20c,0x88b5de10,0x88b5d60c,0x88b5ce08,0x88b5c604,0x88b5bd00,0x7fa2ca00,
			0x7b91d617,0x00b5e681,0x00aaed7f,0x00a9e57a,0x00a9de75,0x00a8d76f,0x00a9d06a,0x00b5cf6e,0x00c1ce72,0x00cecd76,0x00dacc7b,0x00dace7e,0x00dad082,0x00dad185,0x00dad488,0x00cdda86,
		};
		UInt32 res[kSize*kSize];
		DecompressPVRTC<false,true> (reinterpret_cast<const PVRTCBlock*>(src), kSize, kSize, reinterpret_cast<UInt8*>(res));
		CHECK_ARRAY_EQUAL (expected, res, kSize*kSize);
	}
	TEST (DecodePVRTC_4_8x8)
	{
		const int kSize = 8;
		UInt32 src[kSize*kSize*4/32] = {
			0x4c4c4c4c,0x63fb3494,0x00fc4c4c,0x68fc2352,0xa9fefefe,0x5bac1078,0x00ff5555,0x68dc2072,
		};
		UInt32 expected[kSize*kSize] = {
			0x444a751c,0xc7bff170,0x55357739,0x7a6fa444,0x9394b95b,0xbfc3d493,0xbbc6c6a5,0xbfc3d493,0x445e7e1e,0xc5bcee61,0x5d3f8a3d,0x7b77ab41,0x909ab858,0xb8c2cb96,0xb2c6b9b1,0xb8c2cb96,
			0x44738821,0xc3b9eb54,0x664a9c42,0x7e80b33e,0x8ea1b856,0xb2c1c19a,0xaac6adbd,0xb2c1c19a,0x445e7e1e,0xc5bcee61,0x5d3f8a3d,0x7b77ab41,0x7282a141,0x87a2ab63,0x7faa9e6e,0x87a2ab63,
			0x444a751c,0xc7bff170,0x55357739,0x7a6fa444,0x73769d41,0x6c7d983f,0x6685923d,0x6c7d983f,0x44356c1a,0xc9c2f47d,0x4c2b6535,0x78669c47,0x756a9a42,0x716d983d,0x6e719639,0x716d983d,
			0x44216318,0xccc6f78c,0xccc6ff8c,0xccc6f78c,0xccc6ef8c,0xccc6e78c,0xccc6de8c,0xccc6e78c,0x44356c1a,0x48306828,0x4c2b6535,0x48306828,0x44356c1a,0x3f3a6f0d,0x3b3f7300,0x3f3a6f0d,
		};
		UInt32 res[kSize*kSize];
		DecompressPVRTC<false,true> (reinterpret_cast<const PVRTCBlock*>(src), kSize, kSize, reinterpret_cast<UInt8*>(res));
		CHECK_ARRAY_EQUAL (expected, res, kSize*kSize);
	}
	TEST (DecodePVRTC_4_8x16)
	{
		const int kSizeX = 8;
		const int kSizeY = 16;
		UInt32 src[kSizeX*kSizeY*4/32] = {
			0x4c4c4c4c,0x63fb3494,0x00fc4c4c,0x68fc2352,0xa9fefefe,0x5bac1078,0x00ff5555,0x68dc2072,
			0x32323232,0x7faa40f7,0xa802f232,0xffff30e7,0xff030303,0x0f0040e6,0xaa00ff00,0xff9f40e9,
		};
		UInt32 expected[kSizeX*kSizeY] = {
			0x5d79bb10,0xe1dcf2aa,0x665ac621,0x9194d24e,0xadb8d47a,0xd8e0d7cd,0xd4e2cade,0xd8e0d7cd,0x5076a118,0xd2cbee7e,0x6652b131,0x878ac246,0x9dadc668,0xc5d1ccb3,0xbfd4bbce,0xc5d1ccb3,
			0x44738821,0xc3b9eb54,0x664a9c42,0x7e80b33e,0x8ea1b856,0xb2c1c19a,0xaac6adbd,0xb2c1c19a,0x445e7e1e,0xc5bcee61,0x5d3f8a3d,0x7b77ab41,0x7282a141,0x87a2ab63,0x7faa9e6e,0x87a2ab63,
			0x444a751c,0xc7bff170,0x55357739,0x7a6fa444,0x73769d41,0x6c7d983f,0x6685923d,0x6c7d983f,0x44356c1a,0xc9c2f47d,0x4c2b6535,0x78669c47,0x756a9a42,0x716d983d,0x6e719639,0x716d983d,
			0x44216318,0xccc6f78c,0xccc6ff8c,0xccc6f78c,0xccc6ef8c,0xccc6e78c,0xccc6de8c,0xccc6e78c,0x55338812,0x5533821b,0x55337d25,0x5533821b,0x55338812,0x55338d09,0x55339200,0x55338d09,
			0x0069a769,0x6646ab12,0xddb5d6c6,0x6646ab12,0xa18ca2c6,0x6646af06,0x6646b100,0x6646af06,0x0063a774,0x7758d309,0xe5adc1e2,0x7758d309,0x8c6f7ce2,0x7758d103,0x7758d000,0x7758d103,
			0x005ea67f,0x886bfb00,0xeea5adff,0x886bfb00,0x775256ff,0x886bf300,0x886bef00,0x886bf300,0x0076b97f,0x816df800,0xf2bbc1ff,0x816df800,0x997d7dff,0x6c5e5bff,0x3f3f39ff,0x6c5e5bff,
			0x008fcc7f,0x7b70f500,0xf6d2d6ff,0x7b70f500,0x7f75f300,0x837af100,0x887fef00,0x837af100,0x00a7de7f,0x7472f200,0xfae9ebff,0xebdedbff,0xddd4ccff,0xcecabcff,0xbfbfadff,0xcecabcff,
			0x00bff17f,0x6e75ef00,0x666bef00,0x6e75ef00,0x777fef00,0x7f8aef00,0x8894ef00,0x7f8aef00,0x6a7cd508,0x00aee670,0x00a8ec6e,0x00aee670,0x00b5e072,0x00bcd974,0x00c3d477,0x00bcd974,
		};
		UInt32 res[kSizeX*kSizeY];
		DecompressPVRTC<false,true> (reinterpret_cast<const PVRTCBlock*>(src), kSizeX, kSizeY, reinterpret_cast<UInt8*>(res));
		CHECK_ARRAY_EQUAL (expected, res, kSizeX*kSizeY);
	}
	TEST (DecodePVRTC_4_16x8)
	{
		const int kSizeX = 16;
		const int kSizeY = 8;
		UInt32 src[kSizeX*kSizeY*4/32] = {
			0x4c4c4c4c,0x63fb3494,0x00fc4c4c,0x68fc2352,0xa9fefefe,0x5bac1078,0x00ff5555,0x68dc2072,
			0x32323232,0x7faa40f7,0xa802f232,0xffff30e7,0xff030303,0x0f0040e6,0xaa00ff00,0xff9f40e9,
		};
		UInt32 expected[kSizeX*kSizeY] = {
			0x6e5ab31c,0xb8aedc87,0x55357739,0x7a6fa444,0x9394b95b,0xbfc3d493,0xbbc6c6a5,0xc9c9cabb,0x0098c169,0x6668d600,0xf6d2d6ff,0x7b70f500,0xbba9a5ff,0x837af100,0x887fef00,0x7b6dd10e,
			0x725abc1e,0xa89ace75,0x5d3f8a3d,0x7b77ab41,0x909ab858,0xb8c2cb96,0xb2c6b9b1,0xc2c2bbc4,0x009aba6c,0x6a6fd900,0xf2bbc1ff,0x816df800,0x997d7dff,0x8572f200,0x8875ef00,0x7d68d50f,
			0x775ac621,0x9988bf65,0x664a9c42,0x7e80b33e,0x8ea1b856,0xb2c1c19a,0xaac6adbd,0xbbbdadce,0x009cb36f,0x6e77dc00,0xeea5adff,0x886bfb00,0x775256ff,0x886bf300,0x886bef00,0x7f63da10,
			0x725abc1e,0xa89ace75,0x5d3f8a3d,0x7b77ab41,0x7282a141,0x87a2ab63,0x7faa9e6e,0x90a6ac7a,0x009aba6c,0x6a6fd900,0xf2bbc1ff,0x816df800,0x997d7dff,0x6c5e5bff,0x3f3f39ff,0x625d6bd1,
			0x6e5ab31c,0xb8aedc87,0x55357739,0x7a6fa444,0x73769d41,0x6c7d983f,0x6685923d,0x7588a846,0x0098c169,0x6668d600,0xf6d2d6ff,0x7b70f500,0x7f75f300,0x837af100,0x887fef00,0x7b6dd10e,
			0x6a5aaa1a,0xc8c0eb97,0x4c2b6535,0x78669c47,0x756a9a42,0x716d983d,0x6e719639,0x7b7bac42,0x0096c866,0x615fd300,0xfae9ebff,0xebdedbff,0xddd4ccff,0xcecabcff,0xbfbfadff,0xc2bfc1dc,
			0x665aa018,0xd8d4f9a9,0xccc6ff8c,0xccc6f78c,0xccc6ef8c,0xccc6e78c,0xccc6de8c,0xd8d4e7a9,0x0094d063,0x5d58d000,0x666bef00,0x6e75ef00,0x777fef00,0x7f8aef00,0x8894ef00,0x7777c80c,
			0x6a5aaa1a,0x5b438728,0x4c2b6535,0x48306828,0x44356c1a,0x3f3a6f0d,0x3b3f7300,0x484a9300,0x5555b300,0x009fdb72,0x00aaef7f,0x00a8e67f,0x00a7de7f,0x00a5d67f,0x00a4ce7f,0x0098c674,
		};
		UInt32 res[kSizeX*kSizeY];
		DecompressPVRTC<false,true> (reinterpret_cast<const PVRTCBlock*>(src), kSizeX, kSizeY, reinterpret_cast<UInt8*>(res));
		CHECK_ARRAY_EQUAL (expected, res, kSizeX*kSizeY);
	}
	TEST (TwiddleUVPVRTC)
	{
		// 00000000, 11111111 = 0101010101010101
		CHECK_EQUAL (0x5555, TwiddleUVPVRTC (0x100, 0x100, 0xFF, 0x00));
		// 00011011, 11110000 = 0101011110001010
		CHECK_EQUAL (0x578A, TwiddleUVPVRTC (0x100, 0x100, 0xF0, 0x1B));

		// 10100000, 1111 = 101001010101
		CHECK_EQUAL (0xA55, TwiddleUVPVRTC (0x10, 0x100, 0xF, 0xA0));
		// 0000, 11101111 = 111001010101
		CHECK_EQUAL (0xE55, TwiddleUVPVRTC (0x100, 0x10, 0xEF, 0x0));
	}
	#endif // HAS_PVRTC_DECOMPRESSOR
	#endif

} // SUITE

#endif // ENABLE_UNIT_TESTS
