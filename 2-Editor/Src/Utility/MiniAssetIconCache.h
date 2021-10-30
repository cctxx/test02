#pragma once

#include "Runtime/Utilities/GUID.h"

struct Asset;
struct LibraryRepresentation;
class Texture;

void PostprocessAssetsUpdateCachedIcon (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
void UpdateCachedIcon (int instanceID);

Texture* GetCachedAssetDatabaseIcon (const std::string& assetPath);
Texture* GetCachedAssetDatabaseIcon (const UnityGUID& guid, const Asset& asset, const LibraryRepresentation& rep);
