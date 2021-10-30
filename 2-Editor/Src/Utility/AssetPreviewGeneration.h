#pragma once

#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/vector_map.h"
#include "Editor/Src/AssetPipeline/AssetDatabaseStructs.h"

class Texture2D;
class AssetImporter;

typedef vector_map<int, LocalIdentifierInFileType> InstanceIDToLocalIdentifier;

bool AppendAssetPreviewToMetaData (const std::string& previewDestinationPath, LibraryRepresentation& mainAsset, std::vector<LibraryRepresentation>& objects, const std::string& assetPath, const InstanceIDToLocalIdentifier& instanceIDToLocalIdentifier);

Texture2D* MonoCreateAssetPreview (Object* asset, const std::vector<Object*>& subAssets, const std::string& assetPath);
