#ifndef ETC2DECOMPRESSION_H
#define ETC2DECOMPRESSION_H

#include "UnityPrefix.h"

// ETC-2 decompressor
//
// Input is expected to be in standard ETC-2 (+EAC) memory layout. In ETC2 block size is 64b
// and alpha EAC block adds another 64b.
//
// Output must be RGBA with 8 bits for each channel, stored in R, G, B, A order
// (from offset 0 to 3).
//
// Texture sizes that are not exactly divisible by block size are supported. In such
// case block count is rounded up and extra decoded pixels are just ignored.

//! ETC2 memory layout configuration
enum
{
	kETC2BlockWidth						= 4,
	kETC2BlockHeight					= 4,
	kETC2UncompressedPixelSizeA8		= 1,
	kETC2UncompressedPixelSizeRGB8		= 3,
	kETC2UncompressedPixelSizeRGBA8		= 4,
	kETC2UncompressedBlockSizeA8		= kETC2BlockWidth*kETC2BlockHeight*kETC2UncompressedPixelSizeA8,
	kETC2UncompressedBlockSizeRGB8		= kETC2BlockWidth*kETC2BlockHeight*kETC2UncompressedPixelSizeRGB8,
	kETC2UncompressedBlockSizeRGBA8		= kETC2BlockWidth*kETC2BlockHeight*kETC2UncompressedPixelSizeRGBA8
};

//! Decompress ETC2 to RGBA8 buffer
void DecompressETC2_RGB8 (UInt8* dst, const UInt8* src, int width, int height);

//! Decompress ETC2 with punchthrough alpha to RGBA8 buffer
void DecompressETC2_RGB8_A1 (UInt8* dst, const UInt8* src, int width, int height);

//! Decompress ETC2+EAC to RGBA8 buffer
void DecompressETC2_RGBA8 (UInt8* dst, const UInt8* src, int width, int height);

#endif
