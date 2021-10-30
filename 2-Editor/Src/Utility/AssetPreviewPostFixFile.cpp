#include "UnityPrefix.h"
#include "AssetPreviewPostFixFile.h"
#include "Runtime/Utilities/File.h"

static bool ExtractAssetPreviewImageHeader (File& file, size_t length, AssetPreviewImageFooters& outFooters);
static bool ExtractAssetPreviewImage (File& file, size_t length, const AssetPreviewImageFooters& images, LocalIdentifierInFileType identifierInFile, dynamic_array<UInt8>& imageData);

bool ExtractAssetPreviewImage (const std::string& path, LocalIdentifierInFileType identifierInFile, dynamic_array<UInt8>& imageData)
{
	File file;
	if (!file.Open(path, File::kReadPermission, File::kSilentReturnOnOpenFail))
		return false;
	size_t length = file.GetFileLength();

	AssetPreviewImageFooters footers;
	bool result = ExtractAssetPreviewImageHeader (file, length, footers);
	if (result)
		result = ExtractAssetPreviewImage (file, length, footers, identifierInFile, imageData);
	
	file.Close();

	return result; 
}

static bool ExtractAssetPreviewImageHeader (File& file, size_t length, AssetPreviewImageFooters& outFooters)
{
	if (length < sizeof(AssetPreviewFooter))
		return false;
	
	size_t position = length - sizeof(AssetPreviewFooter);
	
	AssetPreviewFooter footer;
	if (file.Read(position, &footer, sizeof(AssetPreviewFooter)) != sizeof(AssetPreviewFooter))
		return false;
	
	if (memcmp(footer.header, PREVIEW_IDENTIFIER, sizeof(footer.header)) != 0)
		return false;
	if (footer.imagesCount > 4096)
		return false;
	if (position < sizeof(AssetPreviewImageFooter) * footer.imagesCount)
		return false;
	position -= sizeof(AssetPreviewImageFooter) * footer.imagesCount;
	
	
	if (footer.imagesCount == 0)
		return false;
	
	outFooters.resize_uninitialized(footer.imagesCount);
	
	size_t readSize = sizeof(AssetPreviewImageFooter) * footer.imagesCount;
	if (file.Read(position, outFooters.begin(), readSize) != readSize)
		return false;
	
	return true;
}


static bool ExtractAssetPreviewImage (File& file, size_t length, const AssetPreviewImageFooters& images, LocalIdentifierInFileType identifierInFile, dynamic_array<UInt8>& imageData)
{
	int i;
	for (i=0;i<images.size();i++)
	{
		if (images[i].localIdentifierInFile == identifierInFile)
			break;
	}	
	if (i == images.size())
		return false;
	
	if (images[i].offset + images[i].size > length)
		return false;
		
	imageData.resize_uninitialized(images[i].size);
	return file.Read(images[i].offset, imageData.begin(), imageData.size()) == imageData.size();
}

bool WriteAssetPreviewFooter (File& file, const AssetPreviewImageFooters& images)
{
	bool success = true;
	// Write image data
	success &= file.Write(images.begin(), images.size () * sizeof(AssetPreviewImageFooter));

	// write final footer
	AssetPreviewFooter footer;
	Assert(strlen(PREVIEW_IDENTIFIER) == 16);
	memcpy(footer.header, PREVIEW_IDENTIFIER, 16);
	footer.imagesCount = images.size();
	success &= file.Write(&footer, sizeof(footer));
	
	return success;
}

bool WriteAssetPreviewImage (File& file, AssetPreviewImageFooters& images, LocalIdentifierInFileType identifier, UInt8* data, size_t size, size_t fileSize)
{
	if (size == 0)
		return false;
	
	AssetPreviewImageFooter image;
	memset(&image, 0, sizeof(image));
	
	image.localIdentifierInFile = identifier;
	image.offset = fileSize;
	image.size = size;
	images.push_back(image);
	
	return file.Write(data, size);
}

// --- Backup asset previews into temporary memory and append to file. (Used when writing serialized data and we want to maintain the previews)

bool ReadAssetPreviewTemporaryStorage (const std::string& path, AssetPreviewTemporaryStorage& output)
{
	File file;
	if (!file.Open(path, File::kReadPermission, File::kSilentReturnOnOpenFail))
		return false;
	size_t length = file.GetFileLength();

	bool result = ExtractAssetPreviewImageHeader (file, length, output.images);
	if (result)
	{
		size_t size = 0;
		for(int i=0;i<output.images.size();i++)
			size += output.images[i].size;
		
		output.data.resize_uninitialized(size);
		
		size = 0;
		for(int i=0;i<output.images.size();i++)
		{
			const AssetPreviewImageFooter& image = output.images[i];
			
			if (file.Read(image.offset, output.data.begin() + size, image.size) != image.size)
			{
				file.Close();
				return false;
			}

			output.images[i].offset = size;
			size += image.size;
		}
	}	
	
	file.Close();
	
	return result; 
}

bool WriteAssetPreviewTemporaryStorage (const std::string& path, const AssetPreviewTemporaryStorage& storage)
{
	if (storage.images.empty())
		return false;
	
	size_t length = ::GetFileLength(path);

	File file;
	if (!file.Open(path, File::kAppendPermission))
		return false;
	
	if (!file.Write(storage.data.begin(), storage.data.size()))
		return false;
	
	AssetPreviewImageFooters newFooters = storage.images;
	
	for (int i=0;i<newFooters.size();i++)
		newFooters[i].offset += length;

	bool success = WriteAssetPreviewFooter (file, newFooters);
	
	file.Close();

	return success;
}