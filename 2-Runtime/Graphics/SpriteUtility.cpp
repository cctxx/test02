#include "UnityPrefix.h"
#include "SpriteUtility.h"

#if ENABLE_SPRITES

#include "Runtime/Geometry/SpriteMeshGenerator.h"

static const float kSpriteEdgeBias  = 1.0f;

bool GetSpriteMeshRectPixelBounds (const Texture2D& texture, const Rectf& rectInaccurate, int& rectX, int& rectY, int& rectRight, int& rectBottom)
{
	rectX = Floorf(rectInaccurate.x);
	rectY = Floorf(rectInaccurate.y);
	rectRight = Ceilf(rectInaccurate.GetRight());
	rectBottom = Ceilf(rectInaccurate.GetBottom());

	// Sanity check to see if out of texture rect or if error is more than 1 pixel for whole-texture sprites.
	const bool errorX = (rectX < 0);
	const bool errorY = (rectY < 0);
	const bool errorR = (rectRight > texture.GetGLWidth() + 1);
	const bool errorB = (rectBottom > texture.GetGLHeight() + 1);
	const bool hasError = (errorX || errorY || errorR || errorB);
	Assert(!hasError);

	// In rare cases when a texture is downscaled to match the max texture size limit it might introduce an odd scaling factor, cause a 1 pixel error and grow beyond the texture size.
	rectRight = std::min(rectRight, texture.GetGLWidth());
	rectBottom = std::min(rectBottom, texture.GetGLHeight());

	return hasError;
}

void GenerateSpriteOutline(PPtr<Texture2D> texture, float pixelsToUnits, const Rectf& rectInaccurate, const Vector2f& rectOffset, float detail, unsigned char alphaTolerance, bool holeDetection, unsigned int extrude, int simplifyMode, std::vector<dynamic_array<Vector2f> >* outLine, std::vector<SpriteVertex>* outVertices, std::vector<UInt16>* outIndices, Rectf* meshRect)
{
	if(texture.IsNull())
		return;

	const int texW = texture->GetGLWidth();
	const int texH = texture->GetGLHeight();

	// Figure out what pixels to use.
	// If a platform texture size limit is set, rectInaccurate will not define actual pixels.
	int rectX, rectY, rectRight, rectBottom;
	GetSpriteMeshRectPixelBounds(*texture, rectInaccurate, rectX, rectY, rectRight, rectBottom);
	const int rectWidth = rectRight - rectX;
	const int rectHeight = rectBottom - rectY;
	const float rectAdjustX = rectInaccurate.x - rectX;
	const float rectAdjustY = rectInaccurate.y - rectY;

	// Extract rectangle
	const int imageSize = rectWidth * rectHeight * sizeof(ColorRGBA32);
	ColorRGBA32* imageData = (ColorRGBA32*)UNITY_MALLOC(kMemDefault, imageSize);
	{
		const int textureSize = texW * texH * sizeof(ColorRGBA32);
		ColorRGBA32* texData = (ColorRGBA32*)UNITY_MALLOC(kMemDefault, textureSize);
		texture->GetPixels32(0, texData);

		for (int row = 0; row < rectHeight; ++row)
		{
			ColorRGBA32* dest = imageData + row * rectWidth;
			ColorRGBA32* src = texData + (rectY + row) * texW + rectX;
			UNITY_MEMCPY(dest, src, sizeof(ColorRGBA32) * rectWidth);
		}

		UNITY_FREE(kMemDefault, texData);
	}
	
	// Detect shape
	const float hullTolerance = (detail>=0.0f) ? 1.0f - clamp01(detail) : detail;
	extrude = clamp<unsigned int>(extrude, 0, 32);
	SpriteMeshGenerator smg;
	smg.MakeShape(imageData, rectWidth, rectHeight, hullTolerance, alphaTolerance, holeDetection, extrude, kSpriteEdgeBias, simplifyMode);

	// Adjustments for offset and pixel-to-unity scale.
	const float scale = 1.0 / pixelsToUnits;
    const float scale_x = 1.0 / texW;
	const float scale_y = 1.0 / texH;
	const Vector2f oxy(float(rectWidth) * 0.5f + rectOffset.x - rectAdjustX, float(rectHeight) * 0.5f + rectOffset.y - rectAdjustY);

	// Fill outline
	if (outLine)
	{
		const std::vector<SpriteMeshGenerator::path>& paths = smg.GetPaths();
		outLine->resize(paths.size());
	
		int ci = 0;
		for (std::vector<SpriteMeshGenerator::path>::const_iterator p = paths.begin(); p != paths.end(); ++p)
		{
			const SpriteMeshGenerator::path& path = *p;
			const std::vector<SpriteMeshGenerator::vertex>& poly = path.m_path;
			dynamic_array<Vector2f> finalPath;
			finalPath.reserve(poly.size());
			for (std::vector<SpriteMeshGenerator::vertex>::const_iterator it = poly.begin(); it != poly.end(); ++it)
			{
				Vector2f polyPoint = it->p;
				Vector2f finalPoint((polyPoint.x-oxy.x)*scale, (polyPoint.y-oxy.y)*scale);
				finalPath.push_back(finalPoint);
			}
			(*outLine)[ci++] = finalPath;
		}
	}

	// Fill mesh
	if (outVertices)
	{
		Assert(outIndices);

		// Decompose outline
		std::vector<Vector2f> vertices;
		std::vector<int> indices;
		smg.Decompose(&vertices, &indices);
		
		if (indices.size() > 0)
		{
			outVertices->clear();
			outIndices->clear();
			// Assign indices
			std::reverse(indices.begin(), indices.end());
			outIndices->assign(indices.begin(), indices.end());
		
			// Assign vertices
			outVertices->reserve(vertices.size());
			for (std::vector<SpriteMeshGenerator::vertex>::size_type i = 0; i < vertices.size(); ++i)
			{
				SpriteVertex v;
				v.pos = Vector3f((vertices[i].x-oxy.x)*scale, (vertices[i].y-oxy.y)*scale, 0.0f);
				v.uv  = Vector2f((vertices[i].x+rectInaccurate.x)*scale_x, (vertices[i].y+rectInaccurate.y)*scale_y);
				outVertices->push_back(v);
			}
		}
	}
	else
	{
		Assert(outIndices == NULL);
	}

	// Set mesh rect
	if (meshRect)
	{
		if (!smg.FindBounds(*meshRect))
			*meshRect = Rectf(rectX, rectY, rectWidth, rectHeight);
	}
	
	// Clean up
	UNITY_FREE(kMemDefault, imageData);
}

void GetAABBVerticesForSprite (const AABB& aabb, Vector3f* outVertices)
{
	outVertices[0] = aabb.m_Center + Vector3f (-aabb.m_Extent.x, -aabb.m_Extent.y, 0.0f);
	outVertices[1] = aabb.m_Center + Vector3f (+aabb.m_Extent.x, -aabb.m_Extent.y, 0.0f);
	outVertices[2] = aabb.m_Center + Vector3f (-aabb.m_Extent.x, +aabb.m_Extent.y, 0.0f);
	outVertices[3] = aabb.m_Center + Vector3f (+aabb.m_Extent.x, +aabb.m_Extent.y, 0.0f);
}

#endif //ENABLE_SPRITES
