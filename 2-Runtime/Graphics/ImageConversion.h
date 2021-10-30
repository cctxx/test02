#ifndef _IMAGECONVERSION_H
#define _IMAGECONVERSION_H

#include "Runtime/Utilities/dynamic_array.h"

class ImageReference;
class Texture2D;
typedef dynamic_array<UInt8> MemoryBuffer;

bool ConvertImageToPNGBuffer( const ImageReference& image, MemoryBuffer& buffer );

enum LoadImageCompression {
	kLoadImageUncompressed = 0,
	kLoadImageDXTCompress,
	kLoadImageDXTCompressDithered,
};
bool LoadMemoryBufferIntoTexture( Texture2D& tex, const UInt8* data, size_t size, LoadImageCompression compression, bool markNonReadable=false );


// hack: this is used only by capture screenshot. Compile code out if it's not
// available
#include "Runtime/Misc/CaptureScreenshot.h"
#if CAPTURE_SCREENSHOT_AVAILABLE
bool ConvertImageToPNGFile( const ImageReference& image, const std::string& path );
#endif


#endif
