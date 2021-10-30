#pragma once
#if ENABLE_SPRITES

#include "Editor/Src/AssetPipeline/SpriteFrameMetaData.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"

// Creates an array of rectangles by using non-recursive flood fill algorithm. 
void GenerateAutomaticSpriteRectangles (const int autoDetectMinSpriteSize, const int extrudeSize, Texture2D& texture, dynamic_array<Rectf>& spriteRectangles);
// Creates an array of rectangles from grid
void GenerateGridSpriteRectangles (Texture2D& texture, int offsetX, int offsetY, int sizeX, int sizeY, int padding, dynamic_array<Rectf>& spriteRectangles);
// Creates sprites from rectangles
void GenerateSpritesFromRectangles (const dynamic_array<Rectf>& spriteRectangles, SpriteSheetMetaData::TSprites& spriteData);
// Sets alignment to sprites from texture settings
void SetAlignmentToSprites (const TextureImporter::Settings& settings, SpriteSheetMetaData::TSprites& spriteData);
// Copies SpriteMetaData from oldSpriteData if the rectangle is the same
void CopyPersistentSprites (const SpriteSheetMetaData::TSprites& oldSpriteData, SpriteSheetMetaData::TSprites& newSpriteData);
// Ensures that the newly created frames get an unique name. 
void GenerateSpriteNames (const string textureName, const SpriteSheetMetaData::TSprites& oldSpriteData, SpriteSheetMetaData::TSprites& newSpriteData);
// Finds the asset importer for the texture and tries to extract the first sprite
Sprite* GetFirstGeneratedSpriteFromTexture (Texture2D* texture);
// Bound check for sprite metadata against texture size
bool ValidateSpriteRect(const Rectf& spriteRect, const Texture2D& texture);

#endif //ENABLE_SPRITES
