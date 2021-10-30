#ifndef S3DECOMPRESSION_H
#define S3DECOMPRESSION_H

#include "TextureFormat.h"

// Decompresses into RGBA32
bool DecompressNativeTextureFormat (
	TextureFormat srcFormat, int srcWidth, int srcHeight, const UInt32* srcData,
	int destWidth, int destHeight, UInt32* destData );

// Some texture formats also need to know the mipLevel
bool DecompressNativeTextureFormatWithMipLevel( TextureFormat srcFormat, int srcWidth, int srcHeight, int mipLevel, const UInt32* sourceData,
											   int destWidth, int destHeight, UInt32* destData );


void DecompressDXT1 (int xblocks, int yblocks, int destWidth, const UInt32* m_pCompBytes, UInt32* m_pDecompBytes);

#endif
