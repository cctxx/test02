#ifndef __ASSETIMPORTERUTIL_H__
#define __ASSETIMPORTERUTIL_H__

#include "AssetInterface.h"

/**
 * Check if assets needs to reimport (call this at startup and when target platform changes)
 * requireCompressedTextures is enabled if the build process absolutely requires textures to be compressed
 * (On startup textures don't have to be compresed, but when making a build they have to be.)
 **/
void VerifyAssetsForBuildTarget(bool requireCompressedTextures, AssetInterface::CancelBehaviour cancelBehaviour);

void SetApplicationSettingCompressAssetsOnImport(bool value);
bool GetApplicationSettingCompressAssetsOnImport();

void CheckTextureImporterLinearRenderingMode();

#endif // __ASSETIMPORTERUTIL_H__