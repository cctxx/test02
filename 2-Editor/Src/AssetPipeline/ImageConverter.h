#ifndef IMAGECONVERTER_H
#define IMAGECONVERTER_H
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/NonCopyable.h"

#include <memory>

struct FIBITMAP;

// This class is used as a wrapper for FIBITMAP image
// It allows us to use FIBITMAP image directly instead of making copy into Image - this means
// that TextureImporter allocates about 50% less memory (i.e. for 4096x4096 textures 64mb less)
class FreeImageWrapper : public NonCopyable
{
public:
	FreeImageWrapper(FIBITMAP* srcImage, int width, int height, UInt32 pitch, TextureFormat textureFormat);
	~FreeImageWrapper();

	const ImageReference& GetImage() const { return m_Image; }

private:
	FIBITMAP* m_SrcImage;
	ImageReference m_Image;
};

// Creates a buffer with the loader image and the specified textureformat
bool LoadImageAtPath (const std::string &path, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha);
bool LoadImageFromBuffer (const UInt8* data, size_t size, std::auto_ptr<FreeImageWrapper>& image, bool premultiplyAlpha);

bool SaveImageToFile (UInt8* inData, int width, int height, TextureFormat format, const std::string& path, long fileType);
bool SaveHDRImageToFile (float* inData, int width, int height, const std::string& path);

#endif
