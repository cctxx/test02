#include "UnityPrefix.h"
#include "Texture3D.h"
#include "Image.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"


static UInt32 CalculateMipOffset3D (int mipCount, int width, int height, int depth, TextureFormat format)
{
	UInt32 imageSize = 0;
	const int bpp = GetBytesFromTextureFormat(format);
	for (int mip = 0; mip < mipCount; ++mip)
	{
		int mipWidth = std::max(width >> mip,1);
		int mipHeight = std::max(height >> mip,1);
		int mipDepth = std::max(depth >> mip,1);
		imageSize += bpp * mipWidth * mipHeight * mipDepth;
	}
	return imageSize;
}


Texture3D::Texture3D (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Width(0)
,	m_Height(0)
,	m_Depth(0)
,	m_Format(kTexFormatARGB32)
,	m_Data(NULL)
,	m_DataSize(0)
,	m_MipMap(false)
,	m_TextureUploaded(false)
{
}

Texture3D::~Texture3D ()
{
	DestroyTexture();
}

bool Texture3D::MainThreadCleanup ()
{
	DeleteGfxTexture();
	return Super::MainThreadCleanup();
}



void Texture3D::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	if ((awakeMode == kDefaultAwakeFromLoad || awakeMode == kInstantiateOrCreateFromCodeAwakeFromLoad) && m_Data == NULL)
		return;

	UploadTexture (false);
}


void Texture3D::Reset ()
{
	Super::Reset();
	m_TextureSettings.Reset();
}


bool Texture3D::InitTexture (int width, int height, int depth, TextureFormat format, bool mipMaps)
{
	if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height) || !IsPowerOfTwo(depth))
	{
		ErrorStringObject ("Texture3D has non-power of two size", this);
		return false;
	}
	if (format >= kTexFormatDXT1 || !IsValidTextureFormat(format))
	{
		ErrorStringObject ("Invalid texture format for Texture3D", this);
		return false;
	}

	const int kMax3DTextureSize = 1024;
	if (width < 0 || width > kMax3DTextureSize || height < 0 || height > kMax3DTextureSize || depth < 0 || depth > kMax3DTextureSize)
	{
		ErrorStringObject ("Texture3D has out of range width / height / depth", this);
		return false;
	}

	m_Width = width;
	m_Height = height;
	m_Depth = depth;
	m_Format = format;
	m_MipMap = mipMaps;

	const int mipCount = CountMipmaps();
	UInt32 imageSize = CalculateMipOffset3D (mipCount, width, height, depth, format);

	UInt8* buffer = AllocateTextureData(imageSize, m_Format, true);
	if (!buffer)
		return false;

	// Cleanup any old memory
	DestroyTexture ();

	m_Data = buffer;
	m_DataSize = imageSize;

	SetTexelSize (1.0f/m_Width, 1.0f/m_Height);

	SetDirty ();
	return true;
}

UInt8* Texture3D::AllocateTextureData (int imageSize, TextureFormat format, bool initMemory)
{
	// Allocate one more pixel because software bi-linear filtering might require it.
	int allocSize = imageSize + GetBytesFromTextureFormat(format);
	void* buffer = UNITY_MALLOC_ALIGNED (kMemTexture, allocSize, 32);

	// In the past the memory manager cleared textures created from script to 0xcd.
	// Let's keep doing that even though a color like white makes more sense (case 564961).
	if (initMemory && buffer)
		memset(buffer, 0xcd, allocSize);

	return static_cast<UInt8*>(buffer);
}

void Texture3D::DeleteGfxTexture()
{
	if (m_TextureUploaded)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		GetGfxDevice().DeleteTexture(GetTextureID());
		m_TextureUploaded = false;
	}
}


void Texture3D::DestroyTexture()
{
	UNITY_FREE (kMemTexture, m_Data);
	m_Data = NULL;
	m_DataSize = 0;

	DeleteGfxTexture();
}

void Texture3D::UploadTexture (bool dontUseSubImage)
{
	if (!gGraphicsCaps.has3DTexture)
		return;

	const BuildSettings* buildSettings = GetBuildSettingsPtr();
	if (buildSettings && !buildSettings->hasPROVersion)
		return;

	Assert (GetImageDataPointer() != NULL);

	UInt32 uploadFlags = (dontUseSubImage || !m_TextureUploaded) ? GfxDevice::kUploadTextureDontUseSubImage : GfxDevice::kUploadTextureDefault;
	GetGfxDevice().UploadTexture3D (GetTextureID(), GetImageDataPointer(), GetImageDataSize(), m_Width, m_Height, m_Depth, m_Format, CountMipmaps(), uploadFlags);
	Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
	ApplySettings();
#if UNITY_XENON && !MASTER_BUILD
	GetGfxDevice().SetTextureName( GetTextureID(), GetName() );
#endif
	m_TextureUploaded = true;
}

void Texture3D::UpdateImageData (bool rebuildMipMaps)
{
	if (rebuildMipMaps)
		RebuildMipMap ();
	UploadTexture (false);
	SetDirty ();
}


void Texture3D::RebuildMipMap ()
{
	if (!m_MipMap || m_Data == NULL)
		return;
	if (IsAnyCompressedTextureFormat(m_Format))
	{
		ErrorStringObject ("Rebuilding mipmaps of compressed textures is not supported", this);
		return;
	}

	CreateMipMap (m_Data, m_Width, m_Height, m_Depth, m_Format);
}


bool Texture3D::ExtractImage (ImageReference* image, int imageIndex) const
{
	if (!m_Data)
		return false;

	ImageReference source (m_Width, m_Height, m_Width * GetBytesFromTextureFormat(m_Format), m_Format, m_Data);
	image->BlitImage (source, ImageReference::BLIT_BILINEAR_SCALE);
	return true;
}



int Texture3D::CountMipmaps () const
{
	if (m_MipMap)
		return CalculateMipMapCount3D (m_Width, m_Height, m_Depth);
	else
		return 1;
}


void Texture3D::UnloadFromGfxDevice(bool forceUnloadAll)
{
	DeleteGfxTexture ();
}

void Texture3D::UploadToGfxDevice()
{
	//@TODO: once texture3D gets the option to unload system memory copy,
	// then we need to do needReloadFromDisk dance similar to Texture2D.

	UploadTexture (true);
}


void Texture3D::SetPixels (int pixelCount, const ColorRGBAf* pixels, int miplevel)
{
	if (pixelCount == 0 || pixels == NULL)
		return;

	if (!m_Data)
	{
		ErrorStringObject("Texture has no data", this);
		return;
	}

	int mipcount = CountMipmaps();
	if (miplevel < 0 || miplevel >= mipcount)
	{
		ErrorStringObject ("Invalid mip level", this);
		return;
	}


	UInt8* data = m_Data + CalculateMipOffset3D (miplevel, m_Width, m_Height, m_Depth, m_Format);
	const int mipWidth = std::max(m_Width >> miplevel,1);
	const int mipHeight = std::max(m_Height >> miplevel,1);
	const int mipDepth = std::max(m_Depth >> miplevel,1);

	SetImagePixelBlock (data, mipWidth, mipHeight*mipDepth, m_Format, 0, 0, mipWidth, mipHeight*mipDepth, pixelCount, pixels);
}


bool Texture3D::GetPixels (ColorRGBAf* dest, int miplevel) const
{
	if (!dest)
		return true; // nothing to do

	if (!m_Data)
	{
		ErrorStringObject("Texture has no data", this);
		return false;
	}

	int mipcount = CountMipmaps();
	if (miplevel < 0 || miplevel >= mipcount)
	{
		ErrorStringObject ("Invalid mip level", this);
		return false;
	}

	UInt8* data = m_Data + CalculateMipOffset3D (miplevel, m_Width, m_Height, m_Depth, m_Format);
	const int mipWidth = std::max(m_Width >> miplevel,1);
	const int mipHeight = std::max(m_Height >> miplevel,1);
	const int mipDepth = std::max(m_Depth >> miplevel,1);

	return GetImagePixelBlock (data, mipWidth, mipHeight*mipDepth, m_Format, 0, 0, mipWidth, mipHeight*mipDepth, dest);
}



template<class TransferFunction>
void Texture3D::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	transfer.Transfer (m_Width, "m_Width", kNotEditableMask);
	transfer.Transfer (m_Height, "m_Height", kNotEditableMask);
	transfer.Transfer (m_Depth, "m_Depth", kNotEditableMask);
	transfer.Transfer (m_Format, "m_Format", kNotEditableMask);
	transfer.Transfer (m_MipMap, "m_MipMap", kNotEditableMask);
	transfer.Align();
	transfer.Transfer (m_DataSize, "m_DataSize", kNotEditableMask);	
	transfer.Transfer (m_TextureSettings, "m_TextureSettings");
	
	unsigned dataSize = m_DataSize;	
	transfer.TransferTypeless (&dataSize, "image data", kHideInEditorMask);
	
	if (transfer.IsReading ())
	{
		DestroyTexture ();
		Assert(GetMemoryLabel().label != kMemTextureCacheId);

		m_DataSize = dataSize;
		m_Data = AllocateTextureData(dataSize, m_Format, false);
		
		SetTexelSize (1.0f/m_Width, 1.0f/m_Height);
	}
	
	// texture data
	//@TODO: format / endianess conversions
	transfer.TransferTypelessData (dataSize, m_Data);
	
	Assert (m_DataSize == 0 || m_Data != NULL);
}



IMPLEMENT_CLASS (Texture3D)
IMPLEMENT_OBJECT_SERIALIZE (Texture3D)
