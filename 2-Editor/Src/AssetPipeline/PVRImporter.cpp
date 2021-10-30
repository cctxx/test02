#include "UnityPrefix.h"
#include "PVRImporter.h"
#include "PVRFormat.h"
#include "AssetDatabase.h"
#include "ImageOperations.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Editor/Mono/MonoEditorUtility.h"

enum { kPVRImporterVersion = 2 };

IMPLEMENT_CLASS_HAS_INIT (PVRImporter)
IMPLEMENT_OBJECT_SERIALIZE (PVRImporter)

PVRImporter::PVRImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

PVRImporter::~PVRImporter ()
{}

void PVRImporter::GenerateAssetData () 
{
	string pathName = GetAssetPathName ();
	
	InputString contents;
	if (!ReadStringFromFile (&contents, pathName))
	{
		LogImportError ("File couldn't be read");
		return;
	}

	if (contents.size() < sizeof(PVR_Texture_Header))
	{
		LogImportError ("Unexpected EOF.");
		return;
	}
	
	PVR_Texture_Header* hdr = (PVR_Texture_Header*)&*contents.begin();
	
	#if UNITY_BIG_ENDIAN
	//Swap Endians. Header is only 32 bit unsigned ints.
	SwapEndianArray(hdr, sizeof(UInt32), sizeof(PVR_Texture_Header)/sizeof(UInt32));
	#endif
	
	if (hdr->dwPVR != PVRTEX_IDENTIFIER || hdr->dwHeaderSize != sizeof(PVR_Texture_Header))
	{
		LogImportError ("Unity does not understand this file. Are you sure it is a valid PVR (v2) file?");
		return;
	}

	int xSize = hdr->dwWidth;
	int ySize = hdr->dwHeight;
	unsigned int mipMapCount = (hdr->dwpfFlags & PVRTEX_MIPMAP) ? hdr->dwMipMapCount : 1;
			
	TextureFormat format;

	unsigned int pixelFormat = hdr->dwpfFlags & PVRTEX_PIXELTYPE;
	bool hasAlpha = hdr->dwpfFlags & PVRTEX_ALPHA;
	if (pixelFormat == OGL_PVRTC4 || pixelFormat == MGLPT_PVRTC4) {
		if (hasAlpha)
			format = kTexFormatPVRTC_RGBA4;
		else
			format = kTexFormatPVRTC_RGB4;
	}
	else if (pixelFormat == OGL_PVRTC2 || pixelFormat == MGLPT_PVRTC2) {
		if (hasAlpha)
			format = kTexFormatPVRTC_RGBA2;
		else
			format = kTexFormatPVRTC_RGB2;
	}
	else if (pixelFormat == D3D_DXT1) {
		format = kTexFormatDXT1;
	}
	else if (pixelFormat == OGL_RGBA_8888) {
		format = kTexFormatRGBA32;
	}
	else if (pixelFormat == MGLPT_ARGB_4444) {
		format = kTexFormatARGB4444;
	}
	else if (pixelFormat == OGL_RGBA_4444) {
		format = kTexFormatRGBA4444;
	}
	else if (pixelFormat == MGLPT_RGB_888 || pixelFormat == OGL_RGB_888) {
		format = kTexFormatRGB24;
	}
	else if (pixelFormat == MGLPT_RGB_565 || pixelFormat == OGL_RGB_565) {
		format = kTexFormatRGB565;
	}
	else {
		LogImportError ("Unsupported PVR pixel format.");
		return;
	}
	
	// Create 2D Texture2D
	Texture2D& texture = ProduceAssetObject<Texture2D> ();
	texture.AwakeFromLoad(kDefaultAwakeFromLoad);

	int textureOptions = (mipMapCount > 1) ? Texture2D::kMipmapMask : Texture2D::kNoMipmap;
	texture.InitTexture (xSize, ySize, format, textureOptions, 1);
	
	// NOTE: PVRTC formats have 1 miplevel less, but that is OK as we recalculate lowest miplevel for them anyway
	if (texture.CountDataMipmaps() != mipMapCount && texture.CountDataMipmaps() != mipMapCount + 1)
	{
		LogImportError ("Invalid number of mip maps.");
		return;
	}	
	
	UInt8* dstData = texture.GetRawImageData(0);
	UInt8* srcData = (UInt8*)hdr + sizeof(PVR_Texture_Header);
	size_t dstDataSize = texture.GetRawImageDataSize();
	size_t dataSizeInFile = contents.size() - sizeof(PVR_Texture_Header);
	
	bool isVerticallyFlipped = hdr->dwpfFlags & PVRTEX_VERTICAL_FLIP;
	if (isVerticallyFlipped)
	{
		// flipped PVR means we can read it directly!
		memset(dstData, 0, dstDataSize);
		memcpy(dstData, srcData, std::min(dstDataSize, dataSizeInFile));
	}
	else
	{
		LogImportError ("Non flipped PVR formats are not yet supported.");
		memset(dstData, 0, dstDataSize);
		memcpy(dstData, srcData, std::min(dstDataSize, dataSizeInFile));
		// TODO: implement PVRTC4 & PVRTC2 flipping
		// NOTE: naively flipping PVRTC4 just as DXT1 didn't work
		//FlipPVRTextureData(xSize, ySize, format, mipMapCount, srcData, dstData );
	}

	MonoPostprocessTexture (texture, GetAssetPathName());

	texture.UpdateImageDataDontTouchMipmap();

	// Setup thumbnail
	Image thumbnail;
	GenerateThumbnailFromTexture (thumbnail, texture);
	SetThumbnail (thumbnail, texture.GetInstanceID ());
}

static int CanLoadPathName (const string& pathName, int* queue) {
	string ext = ToLower(GetPathNameExtension (pathName));
	*queue = -10000;
	return strcmp (ext.c_str(), "pvr") == 0;
}

void PVRImporter::InitializeClass () { 
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (PVRImporter), kPVRImporterVersion);
}

template<class TransferFunction>
void PVRImporter::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	PostTransfer (transfer);
}
