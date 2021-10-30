#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Graphics/SpriteFrame.h"

enum PathSimplifyMode
{
	kPathReduce=0,
	kPathEmbed
};

void GenerateSpriteOutline(PPtr<Texture2D> texture, float pixelsToUnits, const Rectf& rect, const Vector2f& rectOffset, float detail, unsigned char alphaTolerance, bool holeDetection, unsigned int extrude, int simplifyMode, std::vector<dynamic_array<Vector2f> >* outLine = NULL, std::vector<SpriteVertex>* outVertices = NULL, std::vector<UInt16>* outIndices = NULL, Rectf* meshRect = NULL);

// Returns the front face of the AABB.
// Sprite is on local Z = 0, however the AABB has volume. Flatten it on Z and return 4 vertices only.
void GetAABBVerticesForSprite (const AABB& aabb, Vector3f* outVertices);

bool GetSpriteMeshRectPixelBounds (const Texture2D& texture, const Rectf& rectInaccurate, int& rectX, int& rectY, int& rectRight, int& rectBottom);

#endif //ENABLE_SPRITES
