#include "UnityPrefix.h"
#include "CubemapTexture.h"
#include "Runtime/Graphics/CubemapProcessor.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

using namespace std;

Cubemap::Cubemap (MemLabelId label, ObjectCreationMode mode)
: Super (label, mode)
{
	m_SourceTextures.resize (6);
}

Cubemap::~Cubemap ()
{
}

bool Cubemap::InitTexture (int width, int height, TextureFormat format, int flags, int imageCount)
{
	Assert (imageCount == 6);
	Assert (width == height);
	if( !IsPowerOfTwo (width) || !IsPowerOfTwo (height) )
	{
		ErrorStringObject ("Texture has non-power of two size", this);
		return false;
	}
	if( width != height )
	{
		ErrorStringObject ("Cubemap faces must be square", this);
		return false;
	}
	SetDirty();
	return Super::InitTexture (width, width, format, flags, 6);
}


void Cubemap::UploadTexture (bool dontUseSubImage)
{
	Assert (GetRawImageData());

	ErrorIf (GetGLWidth() != GetGLHeight() || m_ImageCount != 6);
	int faceDataSize = GetRawImageData(1)-GetRawImageData(0);
	int dataSize = faceDataSize * 6;
	UInt32 uploadFlags = (dontUseSubImage || !m_TextureUploaded) ? GfxDevice::kUploadTextureDontUseSubImage : GfxDevice::kUploadTextureDefault;
	GetGfxDevice().UploadTextureCube( GetTextureID(), GetRawImageData(), dataSize, faceDataSize, GetGLWidth(), GetTextureFormat(), CountMipmaps(), uploadFlags, GetActiveTextureColorSpace() );
	Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
	
	m_TextureSettings.m_WrapMode = kTexWrapClamp;
	ApplySettings();
	m_TextureUploaded = true;

#if !UNITY_EDITOR
	if(!m_IsReadable)
	{
		DeallocateTextureData (m_TexData.data);
		m_TexData.data = 0;
		m_TexData.imageSize = 0;
	}
#endif
}

static void ConvertImageToFloatArray (float* dst, ImageReference const& src)
{
	ColorRGBAf* dstRGBA = (ColorRGBAf*)dst;
	const int width = src.GetWidth ();
	const int height = src.GetHeight ();
	for (int y = 0; y < width; ++y)
		for (int x = 0; x < height; ++x)
		{
			*dstRGBA++ = GetImagePixel (src.GetImageData (), width, height, src.GetFormat (), kTexWrapClamp, x, y);
		}
}

static void ConvertFloatArrayToImage (ImageReference& dst, float const* src)
{
	ColorRGBAf const* srcRGBA = (ColorRGBAf const*)src;
	const int width = dst.GetWidth ();
	const int height = dst.GetHeight ();
	for (int y = 0; y < width; ++y)
		for (int x = 0; x < height; ++x)
		{
			SetImagePixel (dst, x, y, kTexWrapClamp, *srcRGBA++);
		}
}

void Cubemap::RebuildMipMap ()
{
	TextureRepresentation& rep = m_TexData;
	
	if (rep.data == NULL || !m_MipMap)
		return;
	
	if (IsAnyCompressedTextureFormat(rep.format))
	{
		ErrorStringObject ("Rebuilding mipmaps of compressed textures is not supported", this);
		return;
	}
	if (m_ImageCount != 6)
	{
		ErrorStringObject ("Cubemap must have 6 faces", this);
		return;
	}
	
	Assert (rep.width == rep.height);
	const int edge = rep.width;	
	for (int i = 0; i < 6; i++)
		CreateMipMap (rep.data + rep.imageSize * i, edge, edge, 1, rep.format);
}


void Cubemap::FixupEdges (int fixupWidthInPixels)
{
	TextureRepresentation& rep = m_TexData;
	
	if (rep.data == NULL || !m_MipMap)
		return;
	
	const int edge = rep.width;
	const int channels = 4;	CImageSurface cubemapSurfaces[6];
	for (int q = 0; q < 6; ++q)
	{
		cubemapSurfaces[q].m_ImgData = (float*)UNITY_MALLOC (kMemDefault, edge * edge * channels * sizeof(CP_ITYPE));
	}	
	
	int mipEdge = edge;
	for (int mip = 0; mip < CountMipmaps (); ++mip)
	{
		ImageReference mipImages[6];

		for (int face = 0; face < 6; ++face)
		{
			if (!GetWriteImageReference (&mipImages[face], face, mip))
			{
				ErrorStringObject ("Can't draw into cubemap", this);
				return;
			}
			ConvertImageToFloatArray (cubemapSurfaces[face].m_ImgData, mipImages[face]);
			cubemapSurfaces[face].m_Width = cubemapSurfaces[face].m_Height = mipEdge;
			cubemapSurfaces[face].m_NumChannels = channels;
		}
		
		FixupCubeEdges (cubemapSurfaces, CP_FIXUP_AVERAGE_HERMITE, fixupWidthInPixels);

		for (int face = 0; face < 6; ++face)
		{
			ConvertFloatArrayToImage (mipImages[face], cubemapSurfaces[face].m_ImgData);
		}
		
		mipEdge = max(mipEdge/2, 1);
	}

	for (int q = 0; q < 6; ++q)
	{
		UNITY_FREE (kMemDefault, cubemapSurfaces[q].m_ImgData);
	}
}

void Cubemap::SetSourceTexture (CubemapFace face, PPtr<Texture2D> tex)
{
	Assert (face < m_SourceTextures.size ());
	
	m_SourceTextures[face] = tex;
}

PPtr<Texture2D> Cubemap::GetSourceTexture (CubemapFace face) const
{
	Assert (face < m_SourceTextures.size ());
	
	return m_SourceTextures[face];
}

template<class TransferFunction>
void Cubemap::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_SourceTextures, "m_SourceTextures");
	transfer.Align ();
}

IMPLEMENT_CLASS (Cubemap)
IMPLEMENT_OBJECT_SERIALIZE (Cubemap)

