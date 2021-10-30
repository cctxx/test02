#include "UnityPrefix.h"
#include "ImageConversion.h"
#include "Runtime/Utilities/File.h"
#include "Texture2D.h"
#include "Image.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Math/Color.h"
#include "DXTCompression.h"
#if ENABLE_PNG_JPG
#include "External/ProphecySDK/src/extlib/pnglib/png.h"
#include "External/ProphecySDK/src/extlib/jpglib/jpeglib.h"
#include "Runtime/Export/JPEGMemsrc.h"
#endif
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include <setjmp.h>
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"

using namespace std;

// --------------------------------------------------------------------------
//  PNG

#if ENABLE_PNG_JPG
static void PngWriteToMemoryFunc( png_structp png, png_bytep data, png_size_t size )
{
	MemoryBuffer* buffer = (MemoryBuffer*)png->io_ptr;
	buffer->insert( buffer->end(), data, data+size );
}
static void PngWriteFlushFunc( png_structp png )
{
}


bool ConvertImageToPNGBuffer( const ImageReference& inputImage, MemoryBuffer& buffer )
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,(png_voidp)NULL, NULL, NULL);
	if (!png_ptr)
		return false;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return false;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	buffer.reserve( 4096 );
	png_set_write_fn( png_ptr, &buffer, PngWriteToMemoryFunc, PngWriteFlushFunc );

	png_set_compression_level(png_ptr, Z_BEST_SPEED);

	int format = kTexFormatRGBA32;
	if (inputImage.GetFormat() == kTexFormatRGB24 || inputImage.GetFormat() == kTexFormatRGB565)
		format = kTexFormatRGB24;

	Image image( inputImage.GetWidth(), inputImage.GetHeight(), format );
	image.BlitImage( inputImage );

	png_set_IHDR(png_ptr, info_ptr,
		inputImage.GetWidth(),
		inputImage.GetHeight(),
		8,
		format == kTexFormatRGB24 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);
	for (int i = 0; i < image.GetHeight(); i++)
		png_write_row(png_ptr, image.GetRowPtr(image.GetHeight() - i - 1));

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	return !buffer.empty();
}


#if CAPTURE_SCREENSHOT_AVAILABLE

bool ConvertImageToPNGFile (const ImageReference& inputImage, const string& path)
{
	MemoryBuffer buffer;
	if( !ConvertImageToPNGBuffer( inputImage, buffer ) )
		return false;

	#if UNITY_PEPPER
	// no file access in pepper. write screenshot to stdout.
	// we need to write hex, as string reading in mono may perform
	// string encoding translation on stdout, breaking the binary data.
	printf_console("SCREENSHOT_MARKER=%sSCREENSHOTSTART_MARKER", path.c_str());
	for (int i=0; i<buffer.size(); i++)
		printf_console("%02x", buffer[i]);
	printf_console("SCREENSHOTEND_MARKER\n");
	return true;
	#else
#if ENABLE_PLAYERCONNECTION
	TransferFileOverPlayerConnection(path, &buffer[0], buffer.size());
#endif
	return WriteBytesToFile (&buffer[0], buffer.size(), path);
	#endif
}

#endif

struct PngMemoryReadContext {
	const unsigned char* inputPointer;
	size_t inputSizeLeft;
};

static void PngReadFromMemoryFunc( png_structp png_ptr, png_bytep data, png_size_t len )
{
	PngMemoryReadContext* context = (PngMemoryReadContext*)png_ptr->io_ptr;
	// check for overflow
	if( len > context->inputSizeLeft )
		len = context->inputSizeLeft;

	memcpy( data, context->inputPointer, len );
	context->inputPointer += len;
	context->inputSizeLeft -= len;
}


static void PngReadWarningFunc( png_struct* png_ptr, png_const_charp warning_msg )
{
}

bool ImageSizeBoundCheck(unsigned int width, unsigned int height) {
	int tmp, tmp2;
	if (width + 3 < width ||
		height + 3 < height) {
		return 0;
	}

	tmp = width * height;
	if (width != 0 && height != tmp / width) {
		return 0;
	}

	tmp2 = tmp * 16;
	if (tmp != tmp2/16) {
		return 0;
	}
	return 1;
}

static bool LoadPngIntoTexture( Texture2D& texture, const void* data, size_t size, bool compressTexture, UInt8** outRGBABaseLevelForDXTMips )
{
	PngMemoryReadContext context;
	context.inputPointer = static_cast<const unsigned char*>( data );
	context.inputSizeLeft = size;

	if( !data )
		return false;

	// check png header
	if( size < 8 || !png_check_sig( const_cast<unsigned char*>(context.inputPointer), 8 ) )
		return false;

	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;

	double image_gamma = 0.45;
	int number_passes = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,&PngReadWarningFunc);
	if( png_ptr == NULL )
		return false;

	info_ptr = png_create_info_struct(png_ptr);
	if( info_ptr == NULL )
	{
		png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL);
		return false;
	}

	if( setjmp(png_ptr->jmpbuf) )
	{
		png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL);
		return false;
	}

	png_set_read_fn( png_ptr, &context, &PngReadFromMemoryFunc );
	png_read_info( png_ptr, info_ptr );

	png_get_IHDR( png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL );

	png_set_strip_16(png_ptr); // strip 16 bit channels to 8 bit
	png_set_packing(png_ptr);  // separate palettized channels

	// palette -> rgb
	if( color_type == PNG_COLOR_TYPE_PALETTE )
	{
		png_set_expand(png_ptr);
	}

	// grayscale -> 8 bits
	if( !(color_type & PNG_COLOR_MASK_COLOR) && bit_depth < 8 ) png_set_expand(png_ptr);

	// if exists, expand tRNS to alpha channel
	if( png_get_valid(png_ptr,info_ptr,PNG_INFO_tRNS) ) png_set_expand(png_ptr);

	// expand gray to RGB
	if( color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb(png_ptr);

	// we need to get ARGB format for raw textures, and RGBA if we will compress it
	if( !compressTexture )
		png_set_swap_alpha(png_ptr);

	png_set_filler( png_ptr, 0xFF, PNG_FILLER_BEFORE ); // force alpha byte

	// Only apply gamma correction if the image has gamma information. In this case
	// assume our display gamma is 2.0 (a compromise between Mac and PC...).
	double screen_gamma = 2.0f;
	image_gamma = 0.0;
	if ( png_get_gAMA(png_ptr,info_ptr,&image_gamma) )
	{
		png_set_gamma(png_ptr,screen_gamma,image_gamma);
	}

	number_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr,info_ptr); // update gamma, etc.

	// If texture size or format differs, reformat the texture.
	TextureFormat wantedFormat = compressTexture ? kTexFormatDXT5 : kTexFormatARGB32;
	bool mipMaps = texture.HasMipMap();
	if( width != texture.GetDataWidth () || height != texture.GetDataHeight () || kTexFormatARGB32 != texture.GetTextureFormat())
		texture.InitTexture( width, height, wantedFormat, mipMaps ? Texture2D::kMipmapMask : Texture2D::kNoMipmap, 1 );

	ImageReference ref;
	AssertIf( !outRGBABaseLevelForDXTMips );
	if( compressTexture ) {
		int imageByteSize;
		if (mipMaps) {
			int miplevel = CalculateMipMapCount3D(width, height, 1);
			int iter;
			unsigned int completeSize = 0;
			unsigned int prevcompleteSize = 0;

			if (!ImageSizeBoundCheck(width, height)) {
				longjmp(png_ptr->jmpbuf, 1);
			}

			for (iter = 0; iter < miplevel; iter++) {
				prevcompleteSize = completeSize;
				completeSize += CalculateImageSize (std::max (width >> iter, (png_uint_32)1), std::max (height >> iter, (png_uint_32)1), kTexFormatRGBA32);
				if (completeSize < prevcompleteSize) {
					longjmp(png_ptr->jmpbuf, 1);
				}
			}
			imageByteSize = CalculateImageMipMapSize( width, height, kTexFormatRGBA32 );
		} else {
			if (!ImageSizeBoundCheck(width, height)) {
				longjmp(png_ptr->jmpbuf, 1);
			}
			imageByteSize = CalculateImageSize( width, height, kTexFormatRGBA32 );
		}

		*outRGBABaseLevelForDXTMips = new UInt8[imageByteSize];
		ref = ImageReference( width, height, width*4, kTexFormatRGBA32, *outRGBABaseLevelForDXTMips );
	} else {
		if( !texture.GetWriteImageReference(&ref, 0, 0) ) {
			png_destroy_read_struct( &png_ptr,&info_ptr,(png_infopp)NULL );
			return false;
		}
	}

	size_t len = sizeof(png_bytep) * height;

	/* boundcheck for integer overflow */
	if (height != len / sizeof(png_bytep) ) {
		png_destroy_read_struct( &png_ptr,&info_ptr,(png_infopp)NULL );
		return false;
	}

	png_bytep* row_pointers = new png_bytep[height];
	for( png_uint_32 row = 0; row<height; ++row )
	{
		row_pointers[row] = ref.GetRowPtr(height-1-row);
	}

	for( int pass = 0; pass < number_passes; pass++ )
	{
		png_read_rows( png_ptr,row_pointers,NULL,height );
	}

	// cleanup
	png_read_end( png_ptr,info_ptr );
	png_destroy_read_struct( &png_ptr,&info_ptr,(png_infopp)NULL );
	delete[] row_pointers;

	return true;
}

// --------------------------------------------------------------------------
//  JPG

/* Custom error manager. As the default one in libjpeg crashes Unity on error */
struct unity_jpeg_error_mgr {
	struct jpeg_error_mgr pub;    /* "public" fields */
	jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct unity_jpeg_error_mgr * unity_jpeg_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */
METHODDEF(void)
unity_jpeg_error_exit (j_common_ptr cinfo)
{
	/* cinfo->err really points to a unity_jpeg_error_mgr struct, so coerce pointer */
	unity_jpeg_error_ptr myerr = (unity_jpeg_error_ptr) cinfo->err;
	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message) (cinfo);
	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

static void HandleError (struct jpeg_decompress_struct& cinfo, Texture2D* tex)
{
	jpeg_destroy_decompress (&cinfo);
}

static int LoadJpegIntoTexture(Texture2D& tex, const UInt8* jpegData, size_t jpegDataSz, bool compressTexture, UInt8** outRGBABaseLevelForDXTMips)
{
#if UNITY_WII
	AssertIf ("ERROR: LoadJpegIntoTexture is not supported!");
	return 0;
#else
	struct jpeg_decompress_struct cinfo;

	/* We use our private extension JPEG error handler.
	* Note that this struct must live as long as the main JPEG parameter
	* struct, to avoid dangling-pointer problems.
	*/
	struct unity_jpeg_error_mgr jerr;

    JSAMPARRAY in;
    int row_stride;
    unsigned char *out;

    // set up the decompression.
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = unity_jpeg_error_exit;

	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) {
    	/* If we get here, the JPEG code has signaled an error.
		* We need to clean up the JPEG object, close the input file, and return.
		*/
		HandleError(cinfo, &tex);
		return 0;
	}

	jpeg_create_decompress (&cinfo);

    // inititalize the source
	jpeg_memory_src (&cinfo, (unsigned char*)jpegData, jpegDataSz);

    // initialize decompression
	(void) jpeg_read_header (&cinfo, TRUE);
	(void) jpeg_start_decompress (&cinfo);

    // set up the width and height for return
	int width = cinfo.image_width;
	int height = cinfo.image_height;

    // initialize the input buffer - we'll use the in-built memory management routines in the
    // JPEG library because it will automatically free the used memory for us when we destroy
    // the decompression structure. cool.
    row_stride = cinfo.output_width * cinfo.output_components;
	if (cinfo.output_width != 0 && cinfo.output_components != row_stride / cinfo.output_width) {
		HandleError(cinfo, &tex);
		return 0;
	}
    in = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // we only support three channel RGB or one channel grayscale formats

    if (cinfo.output_components != 3 && cinfo.output_components != 1)
	{
		HandleError(cinfo, &tex);
		return 0;
    }

	// If texture size or format differs, reformat the texture.
	TextureFormat wantedFormat = compressTexture ? kTexFormatDXT1 : kTexFormatRGB24;
	bool mipMaps = tex.HasMipMap();
    if (width != tex.GetDataWidth () || height != tex.GetDataHeight () || wantedFormat != tex.GetTextureFormat())
		tex.InitTexture (width, height, wantedFormat, mipMaps ? Texture2D::kMipmapMask : Texture2D::kNoMipmap, 1);

	ImageReference ref;
	AssertIf( !outRGBABaseLevelForDXTMips );
	if( compressTexture ) {
		int imageByteSize;
		if (mipMaps) {
			int miplevel = CalculateMipMapCount3D(width, height, 1);
			int iter;
			unsigned int completeSize = 0;
			unsigned int prevcompleteSize = 0;

			if (!ImageSizeBoundCheck(width, height)) {
				HandleError(cinfo, &tex);
				return 0;
			}

			for (iter = 0; iter < miplevel; iter++) {
				prevcompleteSize = completeSize;
				completeSize += CalculateImageSize (std::max (width >> iter, 1), std::max (height >> iter, 1), kTexFormatRGBA32);
				if (completeSize < prevcompleteSize) {
					HandleError(cinfo, &tex);
					return 0;
				}
			}
			imageByteSize = CalculateImageMipMapSize( width, height, kTexFormatRGBA32 );
		} else {
			if (!ImageSizeBoundCheck(width, height)) {
				HandleError(cinfo, &tex);
				return 0;
			}
			imageByteSize = CalculateImageSize( width, height, kTexFormatRGBA32 );
		}
		*outRGBABaseLevelForDXTMips = new UInt8[imageByteSize];
		ref = ImageReference( width, height, width*4, kTexFormatRGBA32, *outRGBABaseLevelForDXTMips );
	} else {
		if( !tex.GetWriteImageReference(&ref, 0, 0) ) {
			jpeg_destroy_decompress (&cinfo);
			return 0;
		}
	}

	UInt8* dst;
	int scanline = height-1;
	int result = 1;

	if (scanline < 0) {
		HandleError(cinfo, &tex);
		return 0;
	}

	// three channel output
	if( cinfo.output_components == 3 )
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			jpeg_read_scanlines(&cinfo, in, 1);
			dst = ref.GetRowPtr(scanline);
			out = in[0];
			if( !compressTexture ) {
				for( int i = 0; i < row_stride; i += 3, dst += 3 )
				{
					dst[0] = out[i+2];
					dst[1] = out[i+1];
					dst[2] = out[i];
				}
			} else {
				for( int i = 0; i < row_stride; i += 3, dst += 4 )
				{
					dst[0] = out[i+2];
					dst[1] = out[i+1];
					dst[2] = out[i];
					dst[3] = 255;
				}
			}
			--scanline;
		}
	}
	// one channel output
	else if( cinfo.output_components == 1 )
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			jpeg_read_scanlines(&cinfo, in, 1);
			dst = ref.GetRowPtr(scanline);
			out = in[0];
			if( !compressTexture ) {
				for( int i = 0; i < row_stride; ++i, dst += 3 )
				{
					UInt8 v = out[i];
					dst[0] = v;
					dst[1] = v;
					dst[2] = v;
				}
			} else {
				for( int i = 0; i < row_stride; ++i, dst += 4 )
				{
					UInt8 v = out[i];
					dst[0] = v;
					dst[1] = v;
					dst[2] = v;
					dst[4] = 255;
				}
			}
			--scanline;
		}
	}
	else
	{
		AssertString("unsupported number of output components in JPEG");
		result = 0;
	}

	// finish decompression and destroy the jpeg
	(void) jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);


	return result;
#endif
}

#endif

// --------------------------------------------------------------------------
//  Generic byte->texture loading


// Simple image to load into the texture on error: "?"
static const char* kDummyErrorImage =
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff"
"\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff"
"\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\xff\xff"
"\xff\xff\xff\xff\xff\xff\xff\x00\x00\xff\x00\x00\xff\x00\x00\xff\x00\x00\xff\xff\xff\xff\xff\xff";

#if UNITY_FLASH
//Leaky temporary workaround to deal with not having setjmp/longjmp on Flash. This is only to be able to return correctly, but doesn't clean up anything...beware!

__attribute__ ((noinline)) static int LoadJpegIntoTextureFlash(Texture2D& tex, const UInt8* jpegData, size_t jpegDataSz, bool compressTexture, UInt8** outRGBABaseLevelForDXTMips)
{
	__asm __volatile__("try{");
	int val = LoadJpegIntoTexture(tex,jpegData,jpegDataSz,compressTexture,outRGBABaseLevelForDXTMips);
	__asm __volatile__("}catch(e:Error){r_g0 = 0; return;}");
	return val;
}

__attribute__ ((noinline)) static bool LoadPngIntoTextureFlash( Texture2D& texture, const void* data, size_t size, bool compressTexture, UInt8** outRGBABaseLevelForDXTMips )
{
	__asm __volatile__("try{");
	bool val = LoadPngIntoTexture(texture,data,size,compressTexture,outRGBABaseLevelForDXTMips);
	__asm __volatile__("}catch(e:Error){r_g0 = 0; return;}");
	return val;
}

#define LoadJpegIntoTexture LoadJpegIntoTextureFlash
#define LoadPngIntoTexture LoadPngIntoTextureFlash
#endif

bool LoadMemoryBufferIntoTexture( Texture2D& tex, const UInt8* data, size_t size, LoadImageCompression compression, bool markNonReadable )
{
	UInt8* rgbaBaseLevel = NULL;
	if( !gGraphicsCaps.hasS3TCCompression )
		compression = kLoadImageUncompressed;

#if ENABLE_PNG_JPG
	if( !LoadJpegIntoTexture( tex, data, size, compression!=kLoadImageUncompressed, &rgbaBaseLevel ) )
	{
		delete[] rgbaBaseLevel; rgbaBaseLevel = NULL;
		if( !LoadPngIntoTexture( tex, data, size, compression!=kLoadImageUncompressed, &rgbaBaseLevel ) )
		{
			// Store a dummy image into the texture
			tex.InitTexture( 8, 8, kTexFormatRGB24, Texture2D::kNoMipmap, 1 );
			unsigned char* textureData = (unsigned char*)tex.GetRawImageData();
			memcpy( textureData, kDummyErrorImage, 8*8*3 );
		}
	}
#endif
	// Compress the image if needed
	bool isDXTCompressed = IsCompressedDXTTextureFormat(tex.GetTextureFormat());
	if( isDXTCompressed ) {
		#if !GFX_SUPPORTS_DXT_COMPRESSION
		delete[] rgbaBaseLevel;
		return false;
		#else

		AssertIf( !rgbaBaseLevel );
		// do final image compression
		int width = tex.GetDataWidth();
		int height = tex.GetDataHeight();
		TextureFormat texFormat = tex.GetTextureFormat();
		AssertIf( texFormat != kTexFormatDXT1 && texFormat != kTexFormatDXT5 );
		FastCompressImage( width, height, rgbaBaseLevel, tex.GetRawImageData(), texFormat == kTexFormatDXT5, compression==kLoadImageDXTCompressDithered );

		bool mipMaps = tex.HasMipMap();
		if( mipMaps ) {
			// compute mip representations from the base level
			CreateMipMap (rgbaBaseLevel, width, height, 1, kTexFormatRGBA32);
			// DXT compress them
			int mipCount = tex.CountDataMipmaps();
			for( int mip = 1; mip < mipCount; ++mip )
			{
				const UInt8* srcMipData = rgbaBaseLevel + CalculateMipMapOffset(width, height, kTexFormatRGBA32, mip);
				UInt8* dstMipData = tex.GetRawImageData() + CalculateMipMapOffset(width, height, texFormat, mip);
				FastCompressImage( std::max(width>>mip,1), std::max(height>>mip,1), srcMipData, dstMipData, texFormat == kTexFormatDXT5, compression==kLoadImageDXTCompressDithered );
			}
		}
		#endif // GFX_SUPPORTS_DXT_COMPRESSION
	}
	delete[] rgbaBaseLevel;

	if(markNonReadable)
	{
		tex.SetIsReadable(false);
		tex.SetIsUnreloadable(true);
	}

	if( !IsCompressedDXTTextureFormat(tex.GetTextureFormat()) )
		tex.UpdateImageData();
	else
		tex.UpdateImageDataDontTouchMipmap();

	return true;
}
