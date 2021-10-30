#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_SPRITES

#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Math/Random/Rand.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#include "Editor/Src/SpritePacker/SpritePackerCache.h"

namespace SpritePacker
{

enum SpritePackerExecution
{
	kSPE_Normal = 0,
	kSPE_ForceRegroup
};

bool RebuildAtlasCacheIfNeeded (BuildTargetPlatform platform, bool displayProgressBar, SpritePackerExecution execution, bool enteringPlayMode);
const SpriteRenderData* GetPackedSpriteRD (const Sprite& sprite, UnityStr& outAtlasName);


struct SpriteAtlasSettings
{
	int               format;
	TextureUsageMode  usageMode;
	ColorSpace        colorSpace;
	int               compressionQuality;
	TextureFilterMode filterMode;
	int               maxWidth;
	int               maxHeight;
};

void ActiveJob_AddAtlas(std::string atlasName, const SpriteAtlasSettings& settings);
void ActiveJob_AssignToAtlas(std::string atlasName, Sprite* sprite, SpritePackingMode packingMode, SpritePackingRotation packingRotation);

} // namespace

#endif // ENABLE_SPRITES
