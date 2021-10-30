#pragma once

#include "Runtime/Utilities/dynamic_array.h"

// The asset pipeline stores preview images at the end of the metadata file.
// Here we have some utility functions to extract the image data from the metadata file.

class File;

#define PREVIEW_IDENTIFIER "PreviewAssetData"

struct AssetPreviewFooter
{
	UInt8  header[16];
	UInt32 imagesCount;
};

struct AssetPreviewImageFooter
{
	UInt32                    localIdentifierInFile;
	UInt32                    futurePad[3];
	UInt32                    size;
	UInt32                    offset;
};
typedef dynamic_array<AssetPreviewImageFooter> AssetPreviewImageFooters;
bool ExtractAssetPreviewImage (const std::string& path, LocalIdentifierInFileType identifierInFile, dynamic_array<UInt8>& imageData);

bool WriteAssetPreviewFooter (File& file, const AssetPreviewImageFooters& images);
bool WriteAssetPreviewImage (File& file, AssetPreviewImageFooters& images, LocalIdentifierInFileType identifier, UInt8* data, size_t size, size_t fileSize);


struct AssetPreviewTemporaryStorage
{
	dynamic_array<UInt8>      data;
	AssetPreviewImageFooters  images;
};

bool ReadAssetPreviewTemporaryStorage (const std::string& path, AssetPreviewTemporaryStorage& output);
bool WriteAssetPreviewTemporaryStorage (const std::string& path, const AssetPreviewTemporaryStorage& output);