#include "UnityPrefix.h"
#include "DDSImporter.h"
#include "DDSFormat.h"
#include "AssetDatabase.h"
#include "ImageOperations.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Editor/Mono/MonoEditorUtility.h"


IMPLEMENT_CLASS_HAS_INIT (DDSImporter)
IMPLEMENT_OBJECT_SERIALIZE (DDSImporter)

// Increasing the version number causes an automatic reimport when the last imported version is a different.
// Only increase this number if an older version of the importer generated bad data or the importer applies data optimizations that can only be done in the importer.
// Usually when adding a new feature to the importer, there is no need to increase the number since the feature should probably not auto-enable itself on old import settings.
enum { kDDSImporterVersion = 2 };

struct DXTColBlock
{
	UInt16 col0;
	UInt16 col1;
	UInt8 row[4];
};

struct DXTAlphaBlock
{
	UInt16 row[4];
};

struct DXTAlphaBlock3BitLinear
{
	UInt8 alpha0;
	UInt8 alpha1;
	UInt8 stuff[6];
};

struct DXT3ColAlphaBlock
{
	DXTAlphaBlock alpha;
	DXTColBlock col;
};

struct DXT5ColAlphaBlock
{
	DXTAlphaBlock3BitLinear alpha;
	DXTColBlock col;
};

void FlipTextureData(int width, int height, TextureFormat format, int mipMapCount, UInt8 *src, UInt8 *dst)
{
	int offset = 0;
	
	if(mipMapCount == 0)
		mipMapCount = 1;
	
	while(mipMapCount)
	{
		int blockBytes;
		int numBlocks;
		
		switch(format)
		{
			case kTexFormatDXT1:
				blockBytes = width * 2;
				numBlocks = height/4;
				
				if(blockBytes < 8)
					blockBytes = 8;
				if(numBlocks < 1)
					numBlocks = 1;
				break;

			case kTexFormatDXT3:
			case kTexFormatDXT5:
				blockBytes = width * 4;
				numBlocks = height/4;

				if(blockBytes < 16)
					blockBytes = 16;
				if(numBlocks < 1)
					numBlocks = 1;
				break;

			
			case kTexFormatARGB32:
			case kTexFormatRGBA32:
				blockBytes = width * 4;
				numBlocks = height;
				break;
				
			case kTexFormatRGB24:
				blockBytes = width * 3;
				numBlocks = height;
				break;

			case kTexFormatARGB4444:
			case kTexFormatRGBA4444:
			case kTexFormatRGB565:
				blockBytes = width * 2;
				numBlocks = height;
				break;
				
			default:
				return;
		}

		int size = numBlocks*blockBytes;
		
		for(int block=0;block<numBlocks;block++)
		{
			memcpy( dst+offset+blockBytes*block, src+offset+blockBytes*(numBlocks-1-block), blockBytes);
		}		
		
		if(height>4)
		{
			if(format == kTexFormatDXT1)
				for(int i=0; i<size/sizeof(DXTColBlock); i++)
				{
					DXTColBlock tmp = ((DXTColBlock*)(dst+offset))[i];
					((DXTColBlock*)(dst+offset))[i].row[0] = tmp.row[3];
					((DXTColBlock*)(dst+offset))[i].row[1] = tmp.row[2];
					((DXTColBlock*)(dst+offset))[i].row[2] = tmp.row[1];
					((DXTColBlock*)(dst+offset))[i].row[3] = tmp.row[0];
				}
			else if(format == kTexFormatDXT3)
				for(int i=0; i<size/sizeof(DXT3ColAlphaBlock); i++)
				{
					DXT3ColAlphaBlock tmp = ((DXT3ColAlphaBlock*)(dst+offset))[i];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[0] = tmp.col.row[3];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[1] = tmp.col.row[2];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[2] = tmp.col.row[1];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[3] = tmp.col.row[0];

					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[0] = tmp.alpha.row[3];
					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[1] = tmp.alpha.row[2];
					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[2] = tmp.alpha.row[1];
					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[3] = tmp.alpha.row[0];
				}
			else if(format == kTexFormatDXT5)
				for(int i=0; i<size/sizeof(DXT5ColAlphaBlock); i++)
				{
					DXT5ColAlphaBlock tmp = ((DXT5ColAlphaBlock*)(dst+offset))[i];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[0] = tmp.col.row[3];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[1] = tmp.col.row[2];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[2] = tmp.col.row[1];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[3] = tmp.col.row[0];

					UInt16 row0 = ((tmp.alpha.stuff[0]&0xff) << 0) | ((tmp.alpha.stuff[1]&0x0f) << 8);
					UInt16 row1 = ((tmp.alpha.stuff[1]&0xf0) >> 4) | ((tmp.alpha.stuff[2]&0xff) << 4);
					UInt16 row2 = ((tmp.alpha.stuff[3]&0xff) << 0) | ((tmp.alpha.stuff[4]&0x0f) << 8);
					UInt16 row3 = ((tmp.alpha.stuff[4]&0xf0) >> 4) | ((tmp.alpha.stuff[5]&0xff) << 4);
					
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[0] = row3;
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[1] = (row3 >> 8) | (row2 << 4);
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[2] = row2 >> 4;
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[3] = row1 ;
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[4] = (row1 >> 8) | (row0 << 4);
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[5] = row0 >> 4;
				}
		}
		else if(height==2)
		{
			if(format == kTexFormatDXT1)
				for(int i=0; i<size/sizeof(DXTColBlock); i++)
				{
					DXTColBlock tmp = ((DXTColBlock*)(dst+offset))[i];
					((DXTColBlock*)(dst+offset))[i].row[0] = tmp.row[1];
					((DXTColBlock*)(dst+offset))[i].row[1] = tmp.row[0];
				}
			else if(format == kTexFormatDXT3)
				for(int i=0; i<size/sizeof(DXT3ColAlphaBlock); i++)
				{
					DXT3ColAlphaBlock tmp = ((DXT3ColAlphaBlock*)(dst+offset))[i];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[0] = tmp.col.row[1];
					((DXT3ColAlphaBlock*)(dst+offset))[i].col.row[1] = tmp.col.row[0];

					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[0] = tmp.alpha.row[1];
					((DXT3ColAlphaBlock*)(dst+offset))[i].alpha.row[1] = tmp.alpha.row[0];
				}
			else if(format == kTexFormatDXT5)
				for(int i=0; i<size/sizeof(DXT5ColAlphaBlock); i++)
				{
					DXT5ColAlphaBlock tmp = ((DXT5ColAlphaBlock*)(dst+offset))[i];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[0] = tmp.col.row[1];
					((DXT5ColAlphaBlock*)(dst+offset))[i].col.row[1] = tmp.col.row[0];

					UInt16 row0 = ((tmp.alpha.stuff[0]&0xff) << 0) | ((tmp.alpha.stuff[1]&0x0f) << 8);
					UInt16 row1 = ((tmp.alpha.stuff[1]&0xf0) >> 4) | ((tmp.alpha.stuff[2]&0xff) << 4);
										
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[0] = row1;
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[1] = (row1 >> 8) | (row0 << 4);
					((DXT5ColAlphaBlock*)(dst+offset))[i].alpha.stuff[2] = row0 >> 4;
				}
		}

		offset += size;
		width/=2;
		height/=2;
		if(width==0)
			width=1;
		if(height==0)
			height=1;
		mipMapCount--;
	}
}

DDSImporter::DDSImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

DDSImporter::~DDSImporter ()
{}

void DDSImporter::GenerateAssetData () 
{
	string pathName = GetAssetPathName ();
	
	InputString contents;
	if (!ReadStringFromFile (&contents, pathName))
	{
		LogImportError ("File couldn't be read");
		return;
	}

	if (contents.size() < sizeof(DDS_header))
	{
		LogImportError ("Unexpected EOF.");
		return;
	}
	
	DDS_header* hdr = (DDS_header*)&*contents.begin();
	
	#if UNITY_BIG_ENDIAN
	//Swap Endians. Header is only 32 bit unsigned ints.
	SwapEndianArray(hdr->data, sizeof(UInt32), sizeof(DDS_header)/sizeof(UInt32));
	#endif
	
	if (hdr->dwMagic != DDS_MAGIC || hdr->dwSize != 124 || !(hdr->dwFlags & DDSD_PIXELFORMAT) || !(hdr->dwFlags & DDSD_CAPS))
	{
		LogImportError ("Unity does not understand this file. Are you sure it is a valid DDS file?");
		return;
	}

	int xSize = hdr->dwWidth;
	int ySize = hdr->dwHeight;
	unsigned int mipMapCount = (hdr->dwFlags & DDSD_MIPMAPCOUNT) ? hdr->dwMipMapCount : 1;
			
	TextureFormat format;

	if( PF_IS_DXT1( hdr->sPixelFormat ) ) {
		format = kTexFormatDXT1;
	}
	else if( PF_IS_DXT3( hdr->sPixelFormat ) ) {
		format = kTexFormatDXT3;
	}
	else if( PF_IS_DXT5( hdr->sPixelFormat ) ) {
		format = kTexFormatDXT5;
	}
	else if( PF_IS_BGRA8( hdr->sPixelFormat ) ) {
		format = kTexFormatARGB32;
	}
	else if( PF_IS_BGRA4( hdr->sPixelFormat ) ) {
		format = kTexFormatARGB4444;
	}
	else if( PF_IS_BGR8( hdr->sPixelFormat ) ) {
		format = kTexFormatRGB24;
	}
	else if( PF_IS_BGR5A1( hdr->sPixelFormat ) ) {
		LogImportError ("DDS BGR5A1 not yet supported.");
		return;
	}
	else if( PF_IS_BGR565( hdr->sPixelFormat ) ) {
		format = kTexFormatRGB565;
	}
	else if( PF_IS_INDEX8( hdr->sPixelFormat ) ) {
		LogImportError ("DDS indexed pixel format not yet supported.");
		return;
	}	
	else {
		//fixme: do cube maps later
		//fixme: do 3d later
		LogImportError ("Unknown DDS pixel format.");
		return;
	}
	
	// Create 2D Texture2D
	Texture2D& texture = ProduceAssetObject<Texture2D> ();
	texture.AwakeFromLoad(kDefaultAwakeFromLoad);

	int textureOptions = (mipMapCount > 1) ? Texture2D::kMipmapMask : Texture2D::kNoMipmap;
	texture.InitTexture (xSize, ySize, format, textureOptions, 1);
	
	int expectedMipmapCount = texture.CountDataMipmaps();
	if (expectedMipmapCount != mipMapCount) {
		LogImportError (Format ("Invalid number of mip maps. Expected %d found %d", expectedMipmapCount, mipMapCount));
		return;
	}	
	
	UInt8* dstData = texture.GetRawImageData(0);
	UInt8* srcData = hdr->data+sizeof(DDS_header);
	
	FlipTextureData(xSize, ySize, format, mipMapCount, srcData, dstData );

	//should we also swap 16 bit formats? they appear right in fusion fall.
	if (format == kTexFormatARGB32)
		SwapEndianArray(dstData, 4, texture.GetStorageMemorySize()/4);

	MonoPostprocessTexture (texture, GetAssetPathName());

	texture.UpdateImageDataDontTouchMipmap();

	// Setup thumbnail
	Image thumbnail;
	GenerateThumbnailFromTexture (thumbnail, texture);
	SetThumbnail (thumbnail, texture.GetInstanceID ());
}

static int CanLoadPathName (const string& pathName, int* queue)
{
	string ext = ToLower(GetPathNameExtension (pathName));
	*queue = -10000;
	return strcmp (ext.c_str(), "dds") == 0;
}

void DDSImporter::InitializeClass () 
{
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (DDSImporter), kDDSImporterVersion);
}

template<class TransferFunction>
void DDSImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}
