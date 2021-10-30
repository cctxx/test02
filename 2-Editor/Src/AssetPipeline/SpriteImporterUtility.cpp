#include "UnityPrefix.h"
#include "SpriteImporterUtility.h"

#if ENABLE_SPRITES

#include "Editor/Src/AssetPipeline/SpriteFrameMetaData.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/SpriteUtility.h"
#include "Runtime/Math/FloatConversion.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/AssetPipeline/AssetPathUtilities.h"

static bool Overlap (const Rectf& lhs, const Rectf& rhs)
{
	return (lhs.x < (rhs.x + rhs.width) && (lhs.x + lhs.width) > rhs.x &&
			lhs.y < (rhs.y + rhs.height) && (lhs.y + lhs.height) > rhs.y);
}

static bool Contained (Rectf rect, float x, float y)
{
	int rectX1 = RoundfToInt(rect.x);
	int rectY1 = RoundfToInt(rect.y);
	int rectX2 = RoundfToInt(rect.x + rect.width);
	int rectY2 = RoundfToInt(rect.y + rect.height);
	int pointX = RoundfToInt(x);
	int pointY = RoundfToInt(y);

	return (pointX >= rectX1) && (pointX <= rectX2) && (pointY >= rectY1) && (pointY <= rectY2);
}

static bool Contained (const Rectf& lhs, const Rectf& rhs)
{
	return 
		Contained (lhs, rhs.x,				rhs.y) &&
		Contained (lhs, rhs.x + rhs.width,	rhs.y) &&
		Contained (lhs, rhs.x,				rhs.y + rhs.height) &&
		Contained (lhs, rhs.x + rhs.width,	rhs.y + rhs.height);
}

static float GetDistance (const Rectf& lhs, const Rectf& rhs)
{
	if (Overlap(lhs, rhs))
		return 0;

	// -1 = Rect1 is before rect2 along the axis
	//  0 = Rect1 is inside rect2 along the axis
	//  1 = Rect1 is after rect2 along the axis

	int xAxisOrder = 0;
	int yAxisOrder = 0;

	if (lhs.x >= rhs.GetRight())
		xAxisOrder = 1;
	else if (lhs.GetRight() <= rhs.x)
		xAxisOrder = -1;

	if (lhs.y >= rhs.GetBottom())
 		yAxisOrder = 1;
	else if (lhs.GetBottom() <= rhs.y)
		yAxisOrder = -1;
		
	if (xAxisOrder == 0 && yAxisOrder == -1)
		return rhs.y - lhs.GetBottom ();
	if (xAxisOrder == 0 && yAxisOrder == 1)
		return lhs.y - rhs.GetBottom ();
	if (xAxisOrder == -1 && yAxisOrder == 0)
		return rhs.x - lhs.GetRight ();
	if (xAxisOrder == 1 && yAxisOrder == 0)
		return lhs.x - rhs.GetRight ();
	if (xAxisOrder == -1 && yAxisOrder == -1) 
		return Magnitude (Vector2f(lhs.GetRight(), lhs.GetBottom()) - Vector2f(rhs.x, rhs.y));
	if (xAxisOrder == 1 && yAxisOrder == -1)
		return Magnitude (Vector2f(lhs.x, lhs.GetBottom()) - Vector2f(rhs.GetRight(), rhs.y));
	if (xAxisOrder == -1 && yAxisOrder == 1)
		return Magnitude (Vector2f(lhs.GetRight(), lhs.y) - Vector2f(rhs.x, rhs.GetBottom()));
	if (xAxisOrder == 1 && yAxisOrder == 1)
		return Magnitude (Vector2f(lhs.x, lhs.y) - Vector2f(rhs.GetRight(), rhs.GetBottom()));

	ErrorString ("GetDistance undefined behaviour");

	return 0;
}

static int FindNearestRectIndex (const Rectf rect, const dynamic_array<Rectf>& rects)
{
	int nearest = -1;
	float nearestDistance = std::numeric_limits<float>::max ();

	for (int i = 0; i < rects.size(); i++)
	{
		float distance = GetDistance (rect, rects[i]);

		if (CompareApproximately(distance, 0, FLT_EPSILON))
		{
			return i;
		}

		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = i;
		}
	}

	return nearest;
}


static void InitAlphaLookupTable (const ImageReference& image, dynamic_bitset& alphaLookupTable)
{
	int height = image.GetHeight ();
	int width = image.GetWidth ();
	
	for (int y = 0; y < height; y++)
	{
		int yIndexOffset = y * width;

		for (int x = 0; x < width; x++)
		{
			ColorRGBA32 color = GetImagePixel(image.GetImageData(), width, height, image.GetFormat(), kTexWrapClamp, x, y);
			alphaLookupTable[yIndexOffset + x] = color.a > 0 ? true : false;
		}
	}
}

static bool HasAlphaContent (const ImageReference& image, const dynamic_bitset& alphaLookupTable, const Rectf& rect) 
{
	// Make sure that we don't read outside bounds
	int xMin = std::max<int>((int)(rect.x), 0);
	int xMax = std::min<int>((int)(rect.GetRight()), image.GetWidth() - 1);	
	int yMin = std::max<int>((int)(rect.y), 0);
	int yMax = std::min<int>((int)(rect.GetBottom()), image.GetHeight() - 1);

	for (int y = yMin; y <= yMax; y++)
	{
		for (int x = xMin; x <= xMax; x++)
		{
			UInt8* pixel = image.GetRowPtr(y) + x * 4;

			if(pixel[0] > 0)
			{
				return true;
			}
		}
	}

	return false;
}

// non-recursive flood fill algorithm. 
// 1. Starts from pixel at startIndex
// 2. Finds connected alpha pixels (also diagonally)
// 3. Returns shape boundaries.
// marks visited pixels into m_FillVisitedTable so they are not revisited later
Rectf DetectShapeBounds (const ImageReference& image, const dynamic_bitset& alphaLookupTable, const int startIndex, dynamic_bitset& fillVisitedTable)
{
	int width = image.GetWidth ();
	int height = image.GetHeight ();

	int startX = startIndex % width;
	int startY = startIndex / width;
	
	stack<int> indexStack;	

	indexStack.push (startIndex);

	Vector2f minBound (startX, startY);
	Vector2f maxBound (startX, startY);

	while (!indexStack.empty())
	{
		int i = indexStack.top ();
		indexStack.pop ();

		//  Have we visited this pixel already AND is the alpha > 0?
		if (!fillVisitedTable[i] && alphaLookupTable[i])
		{
			int x = i % width;
			int y = i / width;

			minBound = min (minBound, Vector2f(x, y));
			maxBound = max (maxBound, Vector2f(x, y));

			fillVisitedTable[i] = true;

			if (y > 0)
				indexStack.push (i - width); // up
			if (y < height - 1)
				indexStack.push (i + width); // down
			if (x > 0)
				indexStack.push (i - 1); // left
			if (x < width - 1)
				indexStack.push (i + 1); // right

			if (y > 0 && x > 0)
				indexStack.push (i - width - 1); // up-left
			if (y > 0 && x < width - 1)
				indexStack.push (i - width + 1); // up-right
			if (y < height - 1 && x > 0)
				indexStack.push (i + width - 1); // down-left
			if (y < height - 1 && x < width - 1)
				indexStack.push (i + width + 1); // down-right
		}
	}	

	return Rectf (minBound.x, minBound.y, maxBound.x - minBound.x + 1, maxBound.y - minBound.y + 1);
}

void GenerateAutomaticSpriteRectangles (const int autoDetectMinSpriteSize, const int extrudeSize, Texture2D& texture, dynamic_array<Rectf>& spriteRectangles)
{
	ImageReference image;
	texture.GetWriteImageReference (&image, 0, 0);

	int width = image.GetWidth();
	int height = image.GetHeight();
	int arraySize = width * height;

	dynamic_bitset alphaLookupTable (sizeof(bool) * arraySize);
	InitAlphaLookupTable(image, alphaLookupTable);

	dynamic_bitset fillVisitedTable (sizeof(bool) * arraySize);	

	stack<Rectf> tooSmall;	
		
	for(int i=0; i<arraySize; i++)
	{
		if (!fillVisitedTable[i])
		{
			if (alphaLookupTable[i])
			{		
				Rectf rect = DetectShapeBounds(image, alphaLookupTable, i, fillVisitedTable);

				if (rect.width < autoDetectMinSpriteSize || rect.height < autoDetectMinSpriteSize)
				{
					tooSmall.push (rect);
				}
				else
				{
					bool skip = false;

					for (int r = 0; r < spriteRectangles.size (); r++)
					{
						if (Overlap(rect, spriteRectangles[r]))
						{
							spriteRectangles[r] = rect;
							skip = true;
							break;
						}
					}

					if (!skip)
						spriteRectangles.push_back (rect);
				}
			}
			fillVisitedTable[i] = true;
		}
	}

	while (tooSmall.size() > 0)
	{
		Rectf rect = tooSmall.top ();
		tooSmall.pop ();
		
		int nearestIndex = FindNearestRectIndex (rect, spriteRectangles);

		if (nearestIndex >= 0)
		{
			Rectf nearest = spriteRectangles[nearestIndex];
			Rectf sum;

			sum.x = std::min (nearest.x, rect.x);
			sum.y = std::min (nearest.y, rect.y);
			sum.SetRight (std::max(nearest.GetRight(), rect.GetRight()));
			sum.SetBottom (std::max(nearest.GetBottom(), rect.GetBottom()));

			spriteRectangles[nearestIndex] = sum;
		}
	}

	// Extrude
	if (extrudeSize > 0)
	{
		dynamic_array<Rectf>::iterator it = spriteRectangles.begin();
		for (; it != spriteRectangles.end(); ++it)
		{
			Rectf& rect = *it;
			rect.SetLeft (std::max<float> (0, rect.x - extrudeSize));
			rect.SetTop (std::max<float> (0, rect.y - extrudeSize));
			rect.SetRight (std::min<float> (rect.GetRight() + extrudeSize, image.GetWidth()));
			rect.SetBottom (std::min<float> (rect.GetBottom() + extrudeSize, image.GetHeight()));
		}
	}
}

bool IsNameInUse(const string name, const SpriteSheetMetaData::TSprites& spriteData)
{
	for (SpriteSheetMetaData::TSprites::const_iterator oldFrame = spriteData.begin(); oldFrame != spriteData.end(); oldFrame++)
		if (oldFrame->m_Name == name)
			return true;

	return false;
}

void GenerateGridSpriteRectangles (Texture2D& texture, int offsetX, int offsetY, int sizeX, int sizeY, int padding, dynamic_array<Rectf>& spriteRectangles)
{
	ImageReference image;
	texture.GetWriteImageReference (&image, 0, 0);

	dynamic_bitset alphaLookupTable (sizeof(bool) * image.GetWidth() * image.GetHeight(), 0);
	InitAlphaLookupTable (image, alphaLookupTable);

	for (int y = offsetY; y + sizeY - padding <= image.GetHeight(); y += sizeY)
	{
		for (int x = offsetX; x + sizeX - padding <= image.GetWidth(); x += sizeX)
		{
			Rectf* rect = new Rectf (
						(float)(x + padding),
						(float)(image.GetHeight() - (y + padding) - sizeY),
						(float)(sizeX - padding * 2),
						(float)(sizeY - padding * 2)
						);			

			// Create the rect only if there is some content
			if (HasAlphaContent (image, alphaLookupTable, *rect))
			{
				spriteRectangles.push_back (*rect);
			}
		}
	}
}
	
void GenerateSpritesFromRectangles (const dynamic_array<Rectf>& spriteRectangles, SpriteSheetMetaData::TSprites& spriteData)
{
	for (dynamic_array<Rectf>::const_iterator it = spriteRectangles.begin(); it != spriteRectangles.end(); it++)
	{
		const Rectf& rect = *it;
		
		SpriteMetaData *frame = new SpriteMetaData();	
		frame->m_Name = "";
		frame->m_Rect = rect;

		spriteData.push_back (*frame);
	}
}

void GenerateSpriteNames (const string textureName, const SpriteSheetMetaData::TSprites& oldSpriteData, SpriteSheetMetaData::TSprites& newSpriteData)
{
	int nameSuffix = 0;
	string name = "";

	for (SpriteSheetMetaData::TSprites::iterator newFrame = newSpriteData.begin(); newFrame != newSpriteData.end(); newFrame++)
	{
		if (newFrame->m_Name == "")
		{				
			name = textureName + "_" + IntToString (nameSuffix++);

			while (IsNameInUse(name, oldSpriteData))
			{
				name = textureName + "_" + IntToString (nameSuffix++);
			}

			newFrame->m_Name = name;
		}
	}
}

void SetAlignmentToSprites (const TextureImporter::Settings& settings, SpriteSheetMetaData::TSprites& spriteData)
{
	for (SpriteSheetMetaData::TSprites::iterator it = spriteData.begin(); it != spriteData.end(); it++)
	{
		it->m_Alignment = (SpriteAlignment)settings.m_Alignment;
		it->m_Pivot = settings.m_SpritePivot;
	}
}

void CopyPersistentSprites (const SpriteSheetMetaData::TSprites& oldSpriteData, SpriteSheetMetaData::TSprites& newSpriteData)
{
	for (SpriteSheetMetaData::TSprites::iterator newFrame = newSpriteData.begin(); newFrame != newSpriteData.end(); newFrame++)
	{
		for (SpriteSheetMetaData::TSprites::const_iterator oldFrame = oldSpriteData.begin(); oldFrame != oldSpriteData.end(); oldFrame++)
		{
			// Use name and alignment from meta if the new rectangle overlaps with old one
			if (Contained (newFrame->m_Rect, oldFrame->m_Rect) || Contained (oldFrame->m_Rect, newFrame->m_Rect))
			{				
				newFrame->m_Name = oldFrame->m_Name;
				newFrame->m_Alignment = oldFrame->m_Alignment;
				newFrame->m_Pivot = oldFrame->m_Pivot;
				break;
			}
		}
	}
}

Sprite* GetFirstGeneratedSpriteFromTexture (Texture2D* texture)
{
	Assert(texture);

	std::string assetPath = GetAssetPathFromObject(texture);
	std::vector<Object*> allAssets = FindAllAssetsAtPath(assetPath, ClassID(Sprite));
	if (allAssets.size() > 0)
		return dynamic_pptr_cast<Sprite*>(allAssets[0]);

	return NULL;
}

bool ValidateSpriteRect(const Rectf& spriteRect, const Texture2D& texture)
{
	int rectX, rectY, rectRight, rectBottom;
	bool error = GetSpriteMeshRectPixelBounds(texture, spriteRect, rectX, rectY, rectRight, rectBottom);
	return !error;
}

#endif //ENABLE_SPRITES
