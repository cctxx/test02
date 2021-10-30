#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES
#include "Runtime/Utilities/dynamic_bitset.h"

class Sprite;

struct BlockMask
{
	BlockMask(UInt32 w, UInt32 h, UInt32 pad)
	: width(w + pad)
	, height(h + pad)
	, padding(pad)
	, rectFill(true)
	, fitIn(-1)
	, fillMinX(0)
	, fillMinY(0)
	, fillMaxX(0)
	, fillMaxY(0)
	{
		bits.resize(width * height);
	}

	UInt32         width;
	UInt32         height;
	UInt32         padding;
	dynamic_bitset bits;
	bool           rectFill;

	UInt32 GetRawWidth() const { return width - padding; }
	UInt32 GetRawHeight() const { return height - padding; }

	// Fitting data (in blocks). Unused by atlases.
	UInt32         srcX;
	UInt32         srcY;
	int            fitIn;
	UInt32         fitX;
	UInt32         fitY;

	// Filled rect bounds (calculated in FillSprite or BurnInBlockMaskPadded)
	int fillMinX;
	int fillMinY;
	int fillMaxX;
	int fillMaxY;

	void FillRect();
	void FillSprite(const Sprite& frame, bool floodRect);
	void GeneratePadding();

	// Does this BlockMask with padding overlap destination BlockMask.
	// Padding pixels are allowed to not fit into destination BlockMask.
	bool OverlapsBlockMaskPadded(const BlockMask& dst, int px, int py) const;

	// Burn other BlockMask into this BlockMask.
	// Padding pixels are allowed to not fit into this BlockMask.
	void BurnInBlockMaskPadded(const BlockMask& other, UInt32 px, UInt32 py);
};

#endif
