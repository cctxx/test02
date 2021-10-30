#include "UnityPrefix.h"
#include "SpritePackerBlockMask.h"

#if ENABLE_SPRITES
#include "Editor/Src/SpritePacker/CoverageRaster.h"
#include "Runtime/Graphics/SpriteFrame.h"

void BlockMask::FillRect()
{
	rectFill = true;

	UInt32 unpaddedWidth = GetRawWidth();
	UInt32 unpaddedHeight = GetRawHeight();

	bits.clear();
	bits.resize(width * height);

	for (UInt32 y = 0; y < unpaddedHeight; ++y)
	{
		for (UInt32 x = 0; x < unpaddedWidth; ++x)
		{
			bits.set(x + y * width, true);
		}
	}

	fillMinX = 0;
	fillMaxX = unpaddedWidth-1;
	fillMinY = 0;
	fillMaxY = unpaddedHeight-1;
}

void BlockMask::FillSprite(const Sprite& frame, bool floodRect)
{
	rectFill = false;

	UInt32 unpaddedWidth = GetRawWidth();
	UInt32 unpaddedHeight = GetRawHeight();

	bits.clear();
	bits.resize(width * height);

	const SpriteRenderData& rd = frame.GetRenderData(false); // Use non-atlased RenderData as input.
	const UInt32 srcTexWidth = rd.texture->GetGLWidth();
	const UInt32 srcTexHeight = rd.texture->GetGLHeight();
	
	unsigned char* cover = 0;
	ALLOC_TEMP(cover, unsigned char, srcTexWidth * srcTexHeight);
	memset(cover, 0, srcTexWidth * srcTexHeight);
	RasterizeCoverage(rd.indices.size() / 3, rd.indices.data(), rd.vertices.data(), srcTexWidth, srcTexHeight, cover);
	
	fillMinX = fillMinY = std::numeric_limits<int>::max();
	fillMaxX = fillMaxY = std::numeric_limits<int>::min();
	for (int y = 0; y < unpaddedHeight; ++y)
	{
		for (int x = 0; x < unpaddedWidth; ++x)
		{
			UInt32 sx = srcX + x;
			UInt32 sy = srcY + y;
			if (cover[sx + sy * srcTexWidth] > 0)
			{
				bits.set(x + y * width);
				fillMinX = std::min(fillMinX, x);
				fillMinY = std::min(fillMinY, y);
				fillMaxX = std::max(fillMaxX, x);
				fillMaxY = std::max(fillMaxY, y);
			}
		}
	}

	if (floodRect)
	{
		for (int y = fillMinY; y <= fillMaxY; ++y)
			for (int x = fillMinX; x <= fillMaxX; ++x)
				bits.set(x + y * width);
	}
}

void BlockMask::GeneratePadding()
{
	UInt32 unpaddedWidth = GetRawWidth();
	UInt32 unpaddedHeight = GetRawHeight();

	dynamic_bitset source;
	dynamic_bitset padded = bits;

	for (UInt32 i = 0; i < padding; ++i)
	{
		source = padded;
		UInt32 iWidth = unpaddedWidth + i;
		UInt32 iHeight = unpaddedHeight + i;

		for (UInt32 y = 0; y < iHeight; ++y)
		{
			for (UInt32 x = 0; x < iWidth; ++x)
			{
				if (source.test(x + y * width))
				{
					padded.set((y+1) * width + (x  ), true);
					padded.set((y+1) * width + (x+1), true);
					padded.set((y  ) * width + (x+1), true);
				}
			}
		}
	}

	bits = padded;
}

bool BlockMask::OverlapsBlockMaskPadded (const BlockMask& dst, int px, int py) const
{
	if (px < 0 || py < 0)
		return true;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int dx = px + x;
			int dy = py + y;
			if (dx < dst.width && dy < dst.height)
			{
				bool srcSet = bits.test(x + y * width);
				bool dstSet = dst.bits.test(dx + dy * dst.width);
				if (srcSet == true && dstSet == true)
					return true;
			}
			else
			{
				// Sanity check if we're only skipping padding
				Assert((dx - dst.width < padding) || (dy - dst.height < padding));
			}
		}
	}

	return false;
}

void BlockMask::BurnInBlockMaskPadded(const BlockMask& other, UInt32 px, UInt32 py)
{
	int maxX = 0;
	int maxY = 0;

	for (int y = 0; y < other.height; ++y)
	{
		for (int x = 0; x < other.width; ++x)
		{
			bool srcSet = other.bits.test(x + y * other.width);
			if (srcSet)
			{
				int dx = px + x;
				int dy = py + y;
				if (dx < width && dy < height)
				{
					bits.set(dx + dy * width, true);
					maxX = std::max(maxX, dx);
					maxY = std::max(maxY, dy);
				}
				else
				{
					// Sanity check if we're only skipping padding
					Assert((dx - width < other.padding) || (dy - height < other.padding));
				}
			}
		}
	}

	fillMaxX = std::max<int>(maxX - other.padding, fillMaxX);
	fillMaxY = std::max<int>(maxY - other.padding, fillMaxY);
}

#endif //ENABLE_SPRITES
