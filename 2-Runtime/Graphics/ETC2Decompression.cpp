#include "ETC2Decompression.h"

// Utilities

template<typename T>
static inline T InRange (T v, T a, T b)
{
	return a <= v && v <= b;
}

template<typename T>
static inline T Clamp (T v, T a, T b)
{
	if (v < a)		return a;
	else if (v > b)	return b;
	else			return v;
}

static inline int DivRoundUp (int a, int b)
{
	return a/b + ((a%b) ? 1 : 0);
}

static inline UInt32 GetSingleBit (UInt64 src, int bit)
{
	return (src >> bit) & 1;
}

static inline UInt32 GetBitRange (UInt64 src, int low, int high)
{
	int numBits = (high-low) + 1;
	return (src >> low) & ((1<<numBits)-1);
}

static inline UInt8 ExtendTo8 (UInt8 src, UInt32 fromBits)
{
	return (src << (8-fromBits)) | (src >> (2*fromBits - 8));
}

static inline SInt8 Extend3BitSignedDelta (UInt8 src)
{
	bool isNeg = (src & (1<<2)) != 0;
	return (SInt8)((isNeg ? ~((1<<3)-1) : 0) | src);
}

static inline UInt16 Extend11To16 (UInt16 src)
{
	return (src << 5) | (src >> 6);
}

static inline SInt16 Extend11To16WithSign (SInt16 src)
{
	if (src < 0)
		return -(SInt16)Extend11To16(-src);
	else
		return (SInt16)Extend11To16(src);
}

// Block access utilities

static inline UInt64 Get64BitBlock (const UInt8* src, int blockNdx)
{
	UInt64 block = 0;
	for (int i = 0; i < 8; i++)
		block = (block << 8ull) | (UInt64)(src[blockNdx*8+i]);
	return block;
}

static inline UInt64 Get128BitBlockStart (const UInt8* src, int blockNdx)
{
	return Get64BitBlock(src, 2*blockNdx);
}

static inline UInt64 Get128BitBlockEnd (const UInt8* src, int blockNdx)
{
	return Get64BitBlock(src, 2*blockNdx + 1);
}

// Block decompression routines

static void DecompressETC2Block (UInt64 src, UInt8 dst[kETC2UncompressedBlockSizeRGB8], UInt8 alphaDst[kETC2UncompressedBlockSizeA8], bool alphaMode)
{
	typedef enum BlockMode_e
	{
		kBlockModeIndividual = 0,
		kBlockModeDifferential,
		kBlockModeT,
		kBlockModeH,
		kBlockModePlanar,
		kBlockModeCount
	} BlockMode;

	int				diffOpaqueBit	= (int)GetSingleBit(src, 33);
	SInt8			selBR			= (SInt8)GetBitRange(src, 59, 63);	// 5 bits.
	SInt8			selBG			= (SInt8)GetBitRange(src, 51, 55);
	SInt8			selBB			= (SInt8)GetBitRange(src, 43, 47);
	SInt8			selDR			= Extend3BitSignedDelta((UInt8)GetBitRange(src, 56, 58)); // 3 bits.
	SInt8			selDG			= Extend3BitSignedDelta((UInt8)GetBitRange(src, 48, 50));
	SInt8			selDB			= Extend3BitSignedDelta((UInt8)GetBitRange(src, 40, 42));
	BlockMode		mode;

	if (!alphaMode && diffOpaqueBit == 0)
		mode = kBlockModeIndividual;
	else if (!InRange(selBR + selDR, 0, 31))
		mode = kBlockModeT;
	else if (!InRange(selBG + selDG, 0, 31))
		mode = kBlockModeH;
	else if (!InRange(selBB + selDB, 0, 31))
		mode = kBlockModePlanar;
	else
		mode = kBlockModeDifferential;

	if (mode == kBlockModeIndividual || mode == kBlockModeDifferential)
	{
		// A lot of logic is shared between individual and differential mode
		static const int modifierLUT[8][4] =
		{
		//	  00   01   10    11
			{  2,   8,  -2,   -8 },
			{  5,  17,  -5,  -17 },
			{  9,  29,  -9,  -29 },
			{ 13,  42, -13,  -42 },
			{ 18,  60, -18,  -60 },
			{ 24,  80, -24,  -80 },
			{ 33, 106, -33, -106 },
			{ 47, 183, -47, -183 }
		};

		int			flipBit		= (int)GetSingleBit(src, 32);
		UInt32		table[2]	= { GetBitRange(src, 37, 39), GetBitRange(src, 34, 36) };
		UInt8		baseR[2];
		UInt8		baseG[2];
		UInt8		baseB[2];

		// Base values per mode
		if (mode == kBlockModeIndividual)
		{
			baseR[0] = ExtendTo8((UInt8)GetBitRange(src, 60, 63), 4);
			baseR[1] = ExtendTo8((UInt8)GetBitRange(src, 56, 59), 4);
			baseG[0] = ExtendTo8((UInt8)GetBitRange(src, 52, 55), 4);
			baseG[1] = ExtendTo8((UInt8)GetBitRange(src, 48, 51), 4);
			baseB[0] = ExtendTo8((UInt8)GetBitRange(src, 44, 47), 4);
			baseB[1] = ExtendTo8((UInt8)GetBitRange(src, 40, 43), 4);
		}
		else
		{
			baseR[0] = ExtendTo8(selBR, 5);
			baseG[0] = ExtendTo8(selBG, 5);
			baseB[0] = ExtendTo8(selBB, 5);

			baseR[1] = ExtendTo8((UInt8)(selBR + selDR), 5);
			baseG[1] = ExtendTo8((UInt8)(selBG + selDG), 5);
			baseB[1] = ExtendTo8((UInt8)(selBB + selDB), 5);
		}

		for (int pixelNdx = 0; pixelNdx < kETC2BlockHeight*kETC2BlockWidth; pixelNdx++)
		{
			int			x				= pixelNdx / kETC2BlockHeight;
			int			y				= pixelNdx % kETC2BlockHeight;
			int			dstOffset		= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8;
			int			subBlock		= ((flipBit ? y : x) >= 2) ? 1 : 0;
			UInt32		tableNdx		= table[subBlock];
			UInt32		modifierNdx		= (GetSingleBit(src, 16+pixelNdx) << 1) | GetSingleBit(src, pixelNdx);
			int			alphaDstOffset	= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8; // Only needed for PUNCHTHROUGH version.

			if (alphaMode && diffOpaqueBit == 0 && modifierNdx == 2)
			{
				// If doing PUNCHTHROUGH version (alphaMode), opaque bit may affect colors.
				dst[dstOffset+0]			= 0;
				dst[dstOffset+1]			= 0;
				dst[dstOffset+2]			= 0;
				alphaDst[alphaDstOffset]	= 0;
			}
			else
			{
				int modifier;

				// PUNCHTHROUGH version and opaque bit may also affect modifiers.
				if (alphaMode && diffOpaqueBit == 0 && (modifierNdx == 0 || modifierNdx == 2))
					modifier = 0;
				else
					modifier = modifierLUT[tableNdx][modifierNdx];

				dst[dstOffset+0] = (UInt8)Clamp<int>((int)baseR[subBlock] + modifier, 0, 255);
				dst[dstOffset+1] = (UInt8)Clamp<int>((int)baseG[subBlock] + modifier, 0, 255);
				dst[dstOffset+2] = (UInt8)Clamp<int>((int)baseB[subBlock] + modifier, 0, 255);

				if (alphaMode)
					alphaDst[alphaDstOffset] = 255;
			}
		}
	}
	else if (mode == kBlockModeT || mode == kBlockModeH)
	{
		static const int distTable[8] = { 3, 6, 11, 16, 23, 32, 41, 64 };

		UInt8		paintR[4];
		UInt8		paintG[4];
		UInt8		paintB[4];

		if (mode == kBlockModeT)
		{
			// T mode, calculate paint values.
			UInt8		R1a			= (UInt8)GetBitRange(src, 59, 60);
			UInt8		R1b			= (UInt8)GetBitRange(src, 56, 57);
			UInt8		G1			= (UInt8)GetBitRange(src, 52, 55);
			UInt8		B1			= (UInt8)GetBitRange(src, 48, 51);
			UInt8		R2			= (UInt8)GetBitRange(src, 44, 47);
			UInt8		G2			= (UInt8)GetBitRange(src, 40, 43);
			UInt8		B2			= (UInt8)GetBitRange(src, 36, 39);
			UInt32		distNdx		= (GetBitRange(src, 34, 35) << 1) | GetSingleBit(src, 32);
			int			dist		= distTable[distNdx];

			paintR[0] = ExtendTo8(((R1a << 2) | R1b), 4);
			paintG[0] = ExtendTo8(G1, 4);
			paintB[0] = ExtendTo8(B1, 4);
			paintR[2] = ExtendTo8(R2, 4);
			paintG[2] = ExtendTo8(G2, 4);
			paintB[2] = ExtendTo8(B2, 4);
			paintR[1] = (UInt8)Clamp<int>((int)paintR[2] + dist, 0, 255);
			paintG[1] = (UInt8)Clamp<int>((int)paintG[2] + dist, 0, 255);
			paintB[1] = (UInt8)Clamp<int>((int)paintB[2] + dist, 0, 255);
			paintR[3] = (UInt8)Clamp<int>((int)paintR[2] - dist, 0, 255);
			paintG[3] = (UInt8)Clamp<int>((int)paintG[2] - dist, 0, 255);
			paintB[3] = (UInt8)Clamp<int>((int)paintB[2] - dist, 0, 255);
		}
		else
		{
			// H mode, calculate paint values.
			UInt8		R1			= (UInt8)GetBitRange(src, 59, 62);
			UInt8		G1a			= (UInt8)GetBitRange(src, 56, 58);
			UInt8		G1b			= (UInt8)GetSingleBit(src, 52);
			UInt8		B1a			= (UInt8)GetSingleBit(src, 51);
			UInt8		B1b			= (UInt8)GetBitRange(src, 47, 49);
			UInt8		R2			= (UInt8)GetBitRange(src, 43, 46);
			UInt8		G2			= (UInt8)GetBitRange(src, 39, 42);
			UInt8		B2			= (UInt8)GetBitRange(src, 35, 38);
			UInt8		baseR[2];
			UInt8		baseG[2];
			UInt8		baseB[2];
			UInt32		baseValue[2];
			UInt32		distNdx;
			int			dist;

			baseR[0]		= ExtendTo8(R1, 4);
			baseG[0]		= ExtendTo8(((G1a << 1) | G1b), 4);
			baseB[0]		= ExtendTo8(((B1a << 3) | B1b), 4);
			baseR[1]		= ExtendTo8(R2, 4);
			baseG[1]		= ExtendTo8(G2, 4);
			baseB[1]		= ExtendTo8(B2, 4);
			baseValue[0]	= (((UInt32)baseR[0]) << 16) | (((UInt32)baseG[0]) << 8) | baseB[0];
			baseValue[1]	= (((UInt32)baseR[1]) << 16) | (((UInt32)baseG[1]) << 8) | baseB[1];
			distNdx			= (GetSingleBit(src, 34) << 2) | (GetSingleBit(src, 32) << 1) | (UInt32)(baseValue[0] >= baseValue[1]);
			dist			= distTable[distNdx];

			paintR[0]		= (UInt8)Clamp<int>((int)baseR[0] + dist, 0, 255);
			paintG[0]		= (UInt8)Clamp<int>((int)baseG[0] + dist, 0, 255);
			paintB[0]		= (UInt8)Clamp<int>((int)baseB[0] + dist, 0, 255);
			paintR[1]		= (UInt8)Clamp<int>((int)baseR[0] - dist, 0, 255);
			paintG[1]		= (UInt8)Clamp<int>((int)baseG[0] - dist, 0, 255);
			paintB[1]		= (UInt8)Clamp<int>((int)baseB[0] - dist, 0, 255);
			paintR[2]		= (UInt8)Clamp<int>((int)baseR[1] + dist, 0, 255);
			paintG[2]		= (UInt8)Clamp<int>((int)baseG[1] + dist, 0, 255);
			paintB[2]		= (UInt8)Clamp<int>((int)baseB[1] + dist, 0, 255);
			paintR[3]		= (UInt8)Clamp<int>((int)baseR[1] - dist, 0, 255);
			paintG[3]		= (UInt8)Clamp<int>((int)baseG[1] - dist, 0, 255);
			paintB[3]		= (UInt8)Clamp<int>((int)baseB[1] - dist, 0, 255);
		}

		for (int pixelNdx = 0; pixelNdx < kETC2BlockHeight*kETC2BlockWidth; pixelNdx++)
		{
			int		x				= pixelNdx / kETC2BlockHeight;
			int		y				= pixelNdx % kETC2BlockHeight;
			int		dstOffset		= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8;
			UInt32	paintNdx		= (GetSingleBit(src, 16+pixelNdx) << 1) | GetSingleBit(src, pixelNdx);
			int		alphaDstOffset	= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8; // Only needed for PUNCHTHROUGH version.

			if (alphaMode && diffOpaqueBit == 0 && paintNdx == 2)
			{
				dst[dstOffset+0]			= 0;
				dst[dstOffset+1]			= 0;
				dst[dstOffset+2]			= 0;
				alphaDst[alphaDstOffset]	= 0;
			}
			else
			{
				dst[dstOffset+0] = (UInt8)Clamp<int>((int)paintR[paintNdx], 0, 255);
				dst[dstOffset+1] = (UInt8)Clamp<int>((int)paintG[paintNdx], 0, 255);
				dst[dstOffset+2] = (UInt8)Clamp<int>((int)paintB[paintNdx], 0, 255);

				if (alphaMode)
					alphaDst[alphaDstOffset] = 255;
			}
		}
	}
	else
	{
		// Planar mode.
		UInt8 GO1		= (UInt8)GetSingleBit(src, 56);
		UInt8 GO2		= (UInt8)GetBitRange(src, 49, 54);
		UInt8 BO1		= (UInt8)GetSingleBit(src, 48);
		UInt8 BO2		= (UInt8)GetBitRange(src, 43, 44);
		UInt8 BO3		= (UInt8)GetBitRange(src, 39, 41);
		UInt8 RH1		= (UInt8)GetBitRange(src, 34, 38);
		UInt8 RH2		= (UInt8)GetSingleBit(src, 32);
		UInt8 RO		= ExtendTo8((UInt8)GetBitRange(src, 57, 62),	6);
		UInt8 GO		= ExtendTo8(((GO1 << 6) | GO2),					7);
		UInt8 BO		= ExtendTo8(((BO1 << 5) | (BO2 << 3) | BO3),	6);
		UInt8 RH		= ExtendTo8(((RH1 << 1) | RH2),					6);
		UInt8 GH		= ExtendTo8((UInt8)GetBitRange(src, 25, 31),	7);
		UInt8 BH		= ExtendTo8((UInt8)GetBitRange(src, 19, 24),	6);
		UInt8 RV		= ExtendTo8((UInt8)GetBitRange(src, 13, 18),	6);
		UInt8 GV		= ExtendTo8((UInt8)GetBitRange(src, 6, 12),		7);
		UInt8 BV		= ExtendTo8((UInt8)GetBitRange(src, 0, 5),		6);

		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 4; x++)
			{
				int			dstOffset		= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8;
				int			unclampedR		= (x * ((int)RH-(int)RO) + y * ((int)RV-(int)RO) + 4*(int)RO + 2) >> 2;
				int			unclampedG		= (x * ((int)GH-(int)GO) + y * ((int)GV-(int)GO) + 4*(int)GO + 2) >> 2;
				int			unclampedB		= (x * ((int)BH-(int)BO) + y * ((int)BV-(int)BO) + 4*(int)BO + 2) >> 2;
				int			alphaDstOffset	= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8; // Only needed for PUNCHTHROUGH version.

				dst[dstOffset+0] = (UInt8)Clamp<int>(unclampedR, 0, 255);
				dst[dstOffset+1] = (UInt8)Clamp<int>(unclampedG, 0, 255);
				dst[dstOffset+2] = (UInt8)Clamp<int>(unclampedB, 0, 255);

				if (alphaMode)
					alphaDst[alphaDstOffset] = 255;
			}
		}
	}
}

static void DecompressEAC8Block (UInt8 dst[kETC2UncompressedBlockSizeA8], UInt64 src)
{
	static const int modifierLUT[16][8] =
	{
		{ -3,  -6,  -9, -15,  2,  5,  8, 14 },
		{ -3,  -7, -10, -13,  2,  6,  9, 12 },
		{ -2,  -5,  -8, -13,  1,  4,  7, 12 },
		{ -2,  -4,  -6, -13,  1,  3,  5, 12 },
		{ -3,  -6,  -8, -12,  2,  5,  7, 11 },
		{ -3,  -7,  -9, -11,  2,  6,  8, 10 },
		{ -4,  -7,  -8, -11,  3,  6,  7, 10 },
		{ -3,  -5,  -8, -11,  2,  4,  7, 10 },
		{ -2,  -6,  -8, -10,  1,  5,  7,  9 },
		{ -2,  -5,  -8, -10,  1,  4,  7,  9 },
		{ -2,  -4,  -8, -10,  1,  3,  7,  9 },
		{ -2,  -5,  -7, -10,  1,  4,  6,  9 },
		{ -3,  -4,  -7, -10,  2,  3,  6,  9 },
		{ -1,  -2,  -3, -10,  0,  1,  2,  9 },
		{ -4,  -6,  -8,  -9,  3,  5,  7,  8 },
		{ -3,  -5,  -7,  -9,  2,  4,  6,  8 }
	};

	UInt8	baseCodeword	= (UInt8)GetBitRange(src, 56, 63);
	UInt8	multiplier		= (UInt8)GetBitRange(src, 52, 55);
	UInt32	tableNdx		= GetBitRange(src, 48, 51);

	for (int pixelNdx = 0; pixelNdx < kETC2BlockHeight*kETC2BlockWidth; pixelNdx++)
	{
		int			x				= pixelNdx / kETC2BlockHeight;
		int			y				= pixelNdx % kETC2BlockHeight;
		int			dstOffset		= (y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8;
		int			pixelBitNdx		= 45 - 3*pixelNdx;
		UInt32		modifierNdx		= (GetSingleBit(src, pixelBitNdx + 2) << 2) | (GetSingleBit(src, pixelBitNdx + 1) << 1) | GetSingleBit(src, pixelBitNdx);
		int			modifier		= modifierLUT[tableNdx][modifierNdx];

		dst[dstOffset] = (UInt8)Clamp<int>((int)baseCodeword + (int)multiplier*modifier, 0, 255);
	}
}

// Decompression API

void DecompressETC2_RGB8 (UInt8* dst, const UInt8* src, int width, int height)
{
	int		numBlocksX		= DivRoundUp(width, 4);
	int		numBlocksY		= DivRoundUp(height, 4);
	int		dstPixelSize	= kETC2UncompressedPixelSizeRGBA8;
	int		dstRowPitch		= width*dstPixelSize;

	for (int blockY = 0; blockY < numBlocksY; blockY++)
	{
		for (int blockX = 0; blockX < numBlocksX; blockX++)
		{
			UInt64	compressedBlock		= Get64BitBlock(src, blockY*numBlocksX + blockX);
			UInt8	uncompressedBlock[kETC2UncompressedBlockSizeRGB8];

			DecompressETC2Block(compressedBlock, uncompressedBlock, NULL /* no alpha */, false);

			int		baseX	= blockX*kETC2BlockWidth;
			int		baseY	= blockY*kETC2BlockHeight;
			for (int y = 0; y < std::min((int)kETC2BlockHeight, height-baseY); y++)
			{
				for (int x = 0; x < std::min((int)kETC2BlockWidth, width-baseX); x++)
				{
					const UInt8*	srcPixel	= &uncompressedBlock[(y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8];
					UInt8*			dstPixel	= dst + (baseY+y)*dstRowPitch + (baseX+x)*dstPixelSize;

					dstPixel[0] = srcPixel[0];
					dstPixel[1] = srcPixel[1];
					dstPixel[2] = srcPixel[2];
					dstPixel[3] = 0xff; // Force alpha to 1
				}
			}
		}
	}
}

void DecompressETC2_RGB8_A1 (UInt8* dst, const UInt8* src, int width, int height)
{
	int		numBlocksX		= DivRoundUp(width, 4);
	int		numBlocksY		= DivRoundUp(height, 4);
	int		dstPixelSize	= kETC2UncompressedPixelSizeRGBA8;
	int		dstRowPitch		= width*dstPixelSize;

	for (int blockY = 0; blockY < numBlocksY; blockY++)
	{
		for (int blockX = 0; blockX < numBlocksX; blockX++)
		{
			UInt64	compressedBlockRGBA	= Get64BitBlock(src, blockY*numBlocksX + blockX);
			UInt8	uncompressedBlockRGB[kETC2UncompressedBlockSizeRGB8];
			UInt8	uncompressedBlockAlpha[kETC2UncompressedBlockSizeA8];

			DecompressETC2Block(compressedBlockRGBA, uncompressedBlockRGB, uncompressedBlockAlpha, true);

			int		baseX	= blockX*kETC2BlockWidth;
			int		baseY	= blockY*kETC2BlockHeight;
			for (int y = 0; y < std::min((int)kETC2BlockHeight, height-baseY); y++)
			{
				for (int x = 0; x < std::min((int)kETC2BlockWidth, width-baseX); x++)
				{
					const UInt8*	srcPixel		= &uncompressedBlockRGB[(y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8];
					const UInt8*	srcPixelAlpha	= &uncompressedBlockAlpha[(y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8];
					UInt8*			dstPixel		= dst + (baseY+y)*dstRowPitch + (baseX+x)*dstPixelSize;

					dstPixel[0] = srcPixel[0];
					dstPixel[1] = srcPixel[1];
					dstPixel[2] = srcPixel[2];
					dstPixel[3] = srcPixelAlpha[0];
				}
			}
		}
	}
}

void DecompressETC2_RGBA8 (UInt8* dst, const UInt8* src, int width, int height)
{
	int		numBlocksX		= DivRoundUp(width, 4);
	int		numBlocksY		= DivRoundUp(height, 4);
	int		dstPixelSize	= kETC2UncompressedPixelSizeRGBA8;
	int		dstRowPitch		= width*dstPixelSize;

	for (int blockY = 0; blockY < numBlocksY; blockY++)
	{
		for (int blockX = 0; blockX < numBlocksX; blockX++)
		{
			UInt64	compressedBlockAlpha	= Get128BitBlockStart(src, blockY*numBlocksX + blockX);
			UInt64	compressedBlockRGB		= Get128BitBlockEnd(src, blockY*numBlocksX + blockX);
			UInt8	uncompressedBlockAlpha[kETC2UncompressedBlockSizeA8];
			UInt8	uncompressedBlockRGB[kETC2UncompressedBlockSizeRGB8];

			DecompressETC2Block(compressedBlockRGB, uncompressedBlockRGB, NULL /* no alpha */, false);
			DecompressEAC8Block(uncompressedBlockAlpha, compressedBlockAlpha);

			int		baseX	= blockX*kETC2BlockWidth;
			int		baseY	= blockY*kETC2BlockHeight;
			for (int y = 0; y < std::min((int)kETC2BlockHeight, height-baseY); y++)
			{
				for (int x = 0; x < std::min((int)kETC2BlockWidth, width-baseX); x++)
				{
					const UInt8*	srcPixelRGB		= &uncompressedBlockRGB[(y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeRGB8];
					const UInt8*	srcPixelAlpha	= &uncompressedBlockAlpha[(y*kETC2BlockWidth + x)*kETC2UncompressedPixelSizeA8];
					UInt8*			dstPixel		= dst + (baseY+y)*dstRowPitch + (baseX+x)*dstPixelSize;

					dstPixel[0] = srcPixelRGB[0];
					dstPixel[1] = srcPixelRGB[1];
					dstPixel[2] = srcPixelRGB[2];
					dstPixel[3] = srcPixelAlpha[0];
				}
			}
		}
	}
}
