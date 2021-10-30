#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Modules/ExportModules.h"

class Texture2D;

bool EXPORT_COREMODULE PackTextureAtlasSimple( Texture2D* atlas, int atlasMaximumSize, int textureCount, Texture2D** textures, Rectf* outRects, int padding, bool upload, bool markNoLongerReadable );
void PackAtlases (dynamic_array<Vector2f>& sizes, const int maxAtlasSize, const float padding, dynamic_array<Vector2f>& outOffsets, dynamic_array<int>& outIndices, int& atlasCount);

