#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "Texture2D.h"
#include "Image.h"
#include "RenderTexture.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "S3Decompression.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Math/Color.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "ImageConversion.h"
#include "Runtime/Misc/Allocator.h"
#include "DXTCompression.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/MemoryProfiler.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Math/ColorSpaceConversion.h"
#include "Runtime/Misc/BuildSettings.h"

#if UNITY_EDITOR
#include "CubemapTexture.h"
#endif

#if UNITY_WII
#include "PlatformDependent/wii/WiiUtility.h"
#endif

#if ENABLE_TEXTUREID_MAP
#include "Runtime/GfxDevice/TextureIdMap.h"
#endif


#define USE_IMMEDIATE_INTEGRATION (UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_PS3 || UNITY_XENON || UNITY_TIZEN)


PROFILER_INFORMATION(gIntegrateLoadedImmediately, "Texture.IntegrateLoadedImmediately", kProfilerLoading)
PROFILER_INFORMATION(gAwakeFromLoadTex2D, "Texture.AwakeFromLoad", kProfilerLoading)

/*
 Regular and non-power-of-two textures:

 For regular textures, all TextureRepresentation's are exactly the same; and the data is allocated just
 once (all representations point to the same data).

 For NPOT textures:
 *	m_TexData is what is serialized (non power of two). Mip maps use floor(size/2) convention,
	DXT compression is actually next multiple of 4.
 *	If no mipmaps are specified and NPOTRestricted is supported - act as regular texture
 *	If mipmaps are specified and NPOTFULL is supported - act as regular texture
 *  If NPOT textures are not supported:
 *	m_Tex is scaled up to POT at load time. DXT source gets decompressed into ARGB32.
 *		m_TexPadded is POT size, but the original is padded by repeating border pixels. GUITexture uses it.
*/

bool Texture2D::s_ScreenReadAllowed = true;

static inline UInt32 GetBytesForOnePixel( TextureFormat format )
{
	return IsAnyCompressedTextureFormat(format) ? 0 : GetBytesFromTextureFormat(format);
}

bool IsNPOTTextureAllowed(bool hasMipMap)
{
	if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
		return hasMipMap ? gGraphicsCaps.npot == kNPOTFull : gGraphicsCaps.npot >= kNPOTRestricted;
	else
		return false;
}

static int GetNextAllowedTextureSize(int size, bool hasMipMap, TextureFormat format)
{
	int multipleMask = GetTextureSizeAllowedMultiple(format) - 1;
	size = (size + multipleMask) & ~multipleMask;
	if (!IsNPOTTextureAllowed(hasMipMap))
		size = NextPowerOfTwo(size);
	return size;
}


Texture2D::TextureRepresentation::TextureRepresentation()
:	data(NULL)
,	width(0)
,	height(0)
,	format(kTexFormatARGB32)
,	imageSize(0)
{
}

Texture2D::Texture2D (MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
, m_InitFlags(0)
{
	#if UNITY_EDITOR
	m_IgnoreMasterTextureLimit = false;
	#endif

	m_MipMap = false;
	m_TextureDimension = kTexDimNone;

#if UNITY_EDITOR
	m_EditorDontWriteTextureData = false;
	m_AlphaIsTransparency = false;
#endif

	m_PowerOfTwo = true;

	m_ImageCount = 0;

	m_IsReadable = true;
	m_IsUnreloadable = false;
	m_ReadAllowed = true;
	m_TextureUploaded = false;
	m_UnscaledTextureUploaded = false;

	// We use unchecked version since we may not be on the main thread
	// This means CreateTextureID() implementation must be thread safe!
	m_UnscaledTexID = GetUncheckedGfxDevice().CreateTextureID();
}

void Texture2D::Reset ()
{
	Super::Reset();

	m_TextureSettings.Reset ();
}

//#define LOG_AWAKE(x, arg) printf_console(x, arg);
#define LOG_AWAKE(x, arg)

typedef std::vector<Texture2D*> TexturesT;
static TexturesT g_TexturesToUploadOnMainThread;
static Mutex g_UploadMutex;

void Texture2D::AwakeFromLoadThreaded()
{
	Super::AwakeFromLoadThreaded();

#if USE_IMMEDIATE_INTEGRATION
	LOG_AWAKE("Texture2D: AwakeFromLoadThreaded (%s)\n", GetName());
	g_UploadMutex.Lock();
	g_TexturesToUploadOnMainThread.push_back(this);
	g_UploadMutex.Unlock();
	// Pause loading for a while
	// this will allow main thread to start integrating previously loaded textures
	Thread::Sleep(0.001);
#endif // USE_IMMEDIATE_INTEGRATION
}

// Helps to avoid memory peak while loading the level on platforms where graphics driver creates an internal copy of texture data
//
// Memory peak arrises in the following scenario:
// 1) [Load thread] all texture assets are loaded
// 2) [Main thread] for every asset (OnAwake)
//	 a) texture data is uploaded to graphics driver (copy)
//	 b) texture data is released (in case of NonReadable textures)
//
// Instead we do:
// 1) [Load thread] load 1 texture asset
// 2) [Load thread] stall for a moment and allow Main thread to start uploading
// 3) [Main thread] upload as much texture data as possible
// 4) [Main thread] if significantly behind Load thread, then block Load thread
// 5) continue for all assets
//
// NOTE: Main thread calls IntegrateLoadedImmediately() from PreloadManager::UpdatePreloadingSingleStep()
void Texture2D::IntegrateLoadedImmediately()
{
#if USE_IMMEDIATE_INTEGRATION

	PROFILER_AUTO(gIntegrateLoadedImmediately, NULL)

	g_UploadMutex.Lock();
	TexturesT toUpload = g_TexturesToUploadOnMainThread;
	g_TexturesToUploadOnMainThread.clear();

	size_t totalTextureSize = 0;
	for (size_t q = 0; q < toUpload.size(); ++q)
		totalTextureSize += toUpload[q]->m_TexData.imageSize;

	// Evalute texture data ready for immediate integration
	// If we have too much data to upload, then block the loading thread!
	// Loading thread will stall until all pending data is uploaded.
	bool blockLoadingThread = totalTextureSize > (512*1024);

	// Avoid stalling on every texture (if possible) to improve loading speed
	if (!blockLoadingThread)
	{
		g_UploadMutex.Unlock();
	}
	else
	{
		LOG_AWAKE("Texture2D: IntegrateLoadedImmediately() will block loader thread, pending texture data size: %d bytes\n", totalTextureSize);
	}

	for (size_t q = 0; q < toUpload.size(); ++q)
	{
		Texture2D* tex = toUpload[q];
		LOG_AWAKE("Texture2D: Immediately upload texture data (%s)\n", tex->GetName());
		if (tex->m_TexData.data != NULL)
			tex->UploadTexture (false);
	}

	if (blockLoadingThread)
		g_UploadMutex.Unlock();

#endif // USE_IMMEDIATE_INTEGRATION
}

void Texture2D::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	SET_ALLOC_OWNER(this);
	Super::AwakeFromLoad (awakeMode);

	if(    (awakeMode == kDefaultAwakeFromLoad || awakeMode == kInstantiateOrCreateFromCodeAwakeFromLoad)
		&& m_TexData.data == NULL
	  )
	{
		// actually pretty valid
		return;
	}

#if USE_IMMEDIATE_INTEGRATION
	// Check if texture data was already uploaded by IntegrateLoadedImmediately()
	if ((awakeMode & kDidLoadThreaded) != 0)
	{
		bool dynamicTexture = m_TexData.imageSize == 0;
		Assert(m_TextureUploaded || dynamicTexture);

		if (m_TextureUploaded)
			return;
	}
	LOG_AWAKE("Texture2D: Upload texture data in Awake (%s)\n", GetName());
#endif // USE_IMMEDIATE_INTEGRATION

	PROFILER_AUTO(gAwakeFromLoadTex2D, this)
	UploadTexture (false);
}

Texture2D::~Texture2D ()
{
	DestroyTexture ();
	MainThreadCleanup ();
}

bool Texture2D::MainThreadCleanup ()
{
	DeleteGfxTexture ();

	Texture::s_TextureIDMap.erase (m_UnscaledTexID);

	// FreeTextureID() implementation must be thread safe!
	GetUncheckedGfxDevice().FreeTextureID(m_UnscaledTexID);
	m_UnscaledTexID = TextureID();

	return Super::MainThreadCleanup ();
}


static int SourceMipLevelForBlit( int srcWidth, int srcHeight, int dstWidth, int dstHeight )
{
	int level = HighestBit( NextPowerOfTwo(srcWidth) ) - HighestBit( NextPowerOfTwo(dstWidth) );
	level = std::max( level, HighestBit( NextPowerOfTwo(srcHeight) ) - HighestBit( NextPowerOfTwo(dstHeight) ) );
	level = std::max( 0, level );
	return level;
}

// Compressed->compressed extraction. Copies compressed blocks and pads the remainder with
// transparent black.
void Texture2D::ExtractCompressedImageInternal( UInt8* dst, int dstWidth, int dstHeight, int imageIndex ) const
{
	AssertIf( imageIndex < 0 || imageIndex >= m_ImageCount );

	TextureRepresentation const& rep =m_TexData;
	AssertIf( !IsAnyCompressedTextureFormat( rep.format ) );
	if (m_TexData.data == NULL)
	{
		ErrorString ("Texture data can not be accessed");
		return;
	}

	int mipmapLevel = SourceMipLevelForBlit( rep.width, rep.height, dstWidth, dstHeight );
	mipmapLevel = std::min( mipmapLevel, CountDataMipmaps() - 1 );
	int mipmapoffset = CalculateMipMapOffset( rep.width, rep.height, rep.format, mipmapLevel );
	int width = std::max( rep.width >> mipmapLevel, 1 );
	int height = std::max( rep.height >> mipmapLevel, 1 );

	// pad-copy compressed->compressed
	BlitCopyCompressedImage( rep.format, rep.data + imageIndex * rep.imageSize + mipmapoffset,
							 width, height, dst, dstWidth, dstHeight, true );
}


bool Texture2D::ExtractImageInternal( ImageReference* image, bool scaleToSize, int imageIndex ) const
{
	AssertIf (imageIndex < 0 || imageIndex >= m_ImageCount);

	TextureRepresentation const& rep = m_TexData;
	if(rep.data == NULL)
	{
		ErrorString ("Texture is not accessible.");
		return false;
	}

	int mipmapLevel = SourceMipLevelForBlit( rep.width, rep.height, image->GetWidth(), image->GetHeight() );
	mipmapLevel = std::min( mipmapLevel, CountDataMipmaps() - 1 );
	int mipmapoffset = CalculateMipMapOffset( rep.width, rep.height, rep.format, mipmapLevel );
	int width = std::max( rep.width >> mipmapLevel, 1 );
	int height = std::max( rep.height >> mipmapLevel, 1 );

	if( IsAnyCompressedTextureFormat(rep.format) )
	{
		// Decompress into upper multiple-of-4 size
		int decompWidth = (width + 3) / 4 * 4;
		int decompHeight = (height + 3) / 4 * 4;

		Image decompressed( decompWidth, decompHeight, kTexFormatRGBA32 );
		UInt8* compressed = rep.data + imageIndex * rep.imageSize + mipmapoffset;

		if (!DecompressNativeTextureFormatWithMipLevel( rep.format, width, height, mipmapLevel, (UInt32*)compressed, decompWidth, decompHeight, (UInt32*)decompressed.GetImageData() ))
			return false;

		// Clip this back to original size
		ImageReference clipped = decompressed.ClipImage( 0, 0, width, height );

		if( scaleToSize )
		{
			image->BlitImage( clipped, ImageReference::BLIT_BILINEAR_SCALE );
		}
		else
		{
			AssertIf( width > image->GetWidth() || height > image->GetHeight() );
			image->BlitImage( clipped, ImageReference::BLIT_COPY );
			PadImageBorder( *image, width, height );
		}
		return true;
	}
	else
	{
		ImageReference source( width, height, width * GetBytesFromTextureFormat(rep.format),
							   rep.format, rep.data + imageIndex * rep.imageSize + mipmapoffset);

		if( scaleToSize )
		{
			image->BlitImage( source, ImageReference::BLIT_BILINEAR_SCALE );
		}
		else
		{
			AssertIf( width > image->GetWidth() || height > image->GetHeight() );
			image->BlitImage( source, ImageReference::BLIT_COPY );
			PadImageBorder( *image, width, height );
		}
		return true;
	}

	return false;
}
bool Texture2D::ExtractImage (ImageReference* image, int imageIndex) const
{
	return ExtractImageInternal( image, true, imageIndex );
}

bool Texture2D::HasMipMap() const {
	return m_MipMap;
}

int Texture2D::CountMipmaps() const
{
	if (m_MipMap)
		return CalculateMipMapCount3D( m_glWidth, m_glHeight, 1 );
	else
		return 1;
}
int Texture2D::CountDataMipmaps() const
{
	if (m_MipMap)
		return CalculateMipMapCount3D( m_TexData.width, m_TexData.height, 1 );
	else
		return 1;
}

UInt8* Texture2D::AllocateTextureData (int imageSize, TextureFormat format, bool initMemory)
{
	// Allocate one more pixel because software bi-linear filtering might require it.
	int allocSize = imageSize + GetBytesForOnePixel(format);
	void* buffer = UNITY_MALLOC_ALIGNED (GetTextureDataMemoryLabel(), allocSize, 32);

	// In the past the memory manager cleared textures created from script to 0xcd.
	// Let's keep doing that even though a color like white makes more sense (case 564961).
	if (initMemory && buffer)
		memset(buffer, 0xcd, allocSize);

	return static_cast<UInt8*>(buffer);
}

void Texture2D::DeallocateTextureData (UInt8* tex)
{
	UNITY_FREE (GetTextureDataMemoryLabel(), tex);
}

void Texture2D::InitTextureInternal (int width, int height, TextureFormat format, int imageSize, UInt8* buffer, int options, int imageCount)
{
	// Cleanup old memory
	if ((options & kThreadedInitialize) == 0)
	{
		DestroyTexture ();
		SetDirty ();
	}
	else
	{
		AssertIf(m_TexData.data != NULL);
	}

	m_TextureDimension = kTexDim2D;

	m_TexData.width = width;
	m_TexData.height = height;
	m_TexData.format = format;
	m_TexData.imageSize = imageSize;
	m_TexData.data = buffer;
	m_MipMap = options & kMipmapMask;
	m_InitFlags = options;
	m_ImageCount = imageCount;
	UpdatePOTStatus();

	m_glWidth  = GetNextAllowedTextureSize(m_TexData.width, m_MipMap, format);
	m_glHeight = GetNextAllowedTextureSize(m_TexData.height, m_MipMap, format);
	SetTexelSize( 1.0f/m_glWidth, 1.0f/m_glHeight );
}

bool Texture2D::InitTexture (int width, int height, TextureFormat format, int options, int imageCount, intptr_t nativeTex)
{
	SET_ALLOC_OWNER(this);
	if (width < 0 || width > 10000 || height < 0 || height > 10000)
	{
		ErrorStringObject ("Texture has out of range width / height", this);
		return false;
	}

	if (!IsValidTextureFormat(format))
	{
		ErrorStringObject ("TextureFormat is invalid!", this);
		return false;
	}

	int imageSize;
	if( options & kMipmapMask)
		imageSize = CalculateImageMipMapSize( width, height, format );
	else
		imageSize = CalculateImageSize( width, height, format );

	unsigned int tlen = imageSize * imageCount;
	// probably an multiplication overflow
	if (imageSize != 0 && imageCount != tlen / imageSize)
		return false;
	// probably an addition overflow
	if (tlen + GetBytesForOnePixel(format) < tlen)
		return false;

	bool allocData = ENABLE_TEXTUREID_MAP == 0 || nativeTex == 0;

#if ENABLE_TEXTUREID_MAP
	if(nativeTex)
		TextureIdMap::UpdateTexture(m_TexID, GetGfxDevice().CreateExternalTextureFromNative(nativeTex));
#endif

	UInt8* buffer = allocData ? AllocateTextureData(imageSize * imageCount, format, true) : 0;
	InitTextureInternal (width, height, format, imageSize, buffer, options, imageCount);

	return true;
}

void Texture2D::RebuildMipMap ()
{
	TextureRepresentation& rep = m_TexData;

	if( rep.data == NULL || !m_MipMap )
		return;
	if( IsAnyCompressedTextureFormat(rep.format) )
	{
		ErrorString ("Rebuilding mipmaps of compressed textures is not supported");
		return;
	}

	for( int i=0;i<m_ImageCount;i++ )
		CreateMipMap (rep.data + rep.imageSize * i, rep.width, rep.height, 1, rep.format);
}


#if UNITY_EDITOR
void Texture2D::SetImage (const ImageReference& image, int flags)
{
	SET_ALLOC_OWNER(this);
	InitTexture( image.GetWidth (), image.GetHeight (), image.GetFormat (), flags );

	int bytesPerPixel = GetBytesFromTextureFormat (m_TexData.format);

	ImageReference dst( m_TexData.width, m_TexData.height, bytesPerPixel * m_TexData.width, m_TexData.format, m_TexData.data );
	if (image.GetImageData () != NULL)
		dst.BlitImage (image);

	RebuildMipMap ();

	UploadTexture (true);
	SetDirty ();
}
#endif



bool Texture2D::GetImageReferenceInternal (ImageReference* image, int frame, int miplevel) const
{
	TextureRepresentation const& rep = m_TexData;

	if( rep.data == NULL || IsAnyCompressedTextureFormat(rep.format) )
		return false;

	AssertIf (frame >= m_ImageCount);
	AssertIf (miplevel >= CountDataMipmaps ());

	UInt8* base = rep.data + frame * rep.imageSize;
	base += CalculateMipMapOffset( rep.width, rep.height, rep.format, miplevel );
	int width = std::max (rep.width >> miplevel, 1);
	int height = std::max (rep.height >> miplevel, 1);

	*image = ImageReference( width, height, GetRowBytesFromWidthAndFormat(width, rep.format), rep.format, base );
	return true;
}

bool Texture2D::GetWriteImageReference (ImageReference* image, int frame, int miplevel)
{
	return GetImageReferenceInternal (image, frame, miplevel);
}


void Texture2D::ApplySettings()
{
	TextureDimension texdim = GetDimension();
	m_TextureSettings.Apply( GetTextureID(), texdim, HasMipMap(), GetActiveTextureColorSpace());
	if( m_UnscaledTextureUploaded )
		m_TextureSettings.Apply( GetUnscaledTextureID(), texdim, HasMipMap(), GetActiveTextureColorSpace() );

	NotifyMipBiasChanged();
}

void Texture2D::UpdatePOTStatus()
{
	m_PowerOfTwo = IsPowerOfTwo(m_TexData.width) && IsPowerOfTwo(m_TexData.height);

	// force clamp if we will keep it npot
	if(!m_PowerOfTwo && !m_MipMap && gGraphicsCaps.npot == kNPOTRestricted)
		m_TextureSettings.m_WrapMode = kTexWrapClamp;
}


void Texture2D::UploadTexture (bool dontUseSubImage)
{
	if (m_TexData.data == NULL)
	{
		ErrorString("No Texture memory available to upload");
		return;
	}
	if (m_TexData.width == 0 || m_TexData.height == 0)
	{
		return;
	}

	TextureRepresentation scaled = m_TexData;
	TextureRepresentation padded = m_TexData;

	InitTextureRepresentations(&scaled, &padded);

	int mipCount = CountMipmaps();

	int masterTextureLimit = Texture::GetMasterTextureLimit();
	#if UNITY_EDITOR
		if (m_IgnoreMasterTextureLimit)
			masterTextureLimit = 0;
	#endif

	// Master texture limit must be clamped to mipCount-1 or otherwise GfxDevice won't upload anything
	masterTextureLimit = min(mipCount-1, masterTextureLimit);

	TextureUsageMode usageMode = GetUsageMode();

	// upload regular texture
	{
		TextureRepresentation& rep = scaled;
		UInt8* srcData = rep.data;
		UInt32 uploadFlags = (dontUseSubImage || !m_TextureUploaded) ? GfxDevice::kUploadTextureDontUseSubImage : GfxDevice::kUploadTextureDefault;
		if (m_InitFlags & kOSDrawingCompatible)
			uploadFlags |= GfxDevice::kUploadTextureOSDrawingCompatible;
		GetGfxDevice().UploadTexture2D( GetTextureID(), GetDimension(), srcData, rep.imageSize, rep.width, rep.height, rep.format, mipCount, uploadFlags, masterTextureLimit, usageMode, GetActiveTextureColorSpace() );
		Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
		m_TextureSettings.Apply( GetTextureID(), GetDimension(), m_MipMap, GetActiveTextureColorSpace());
		m_TextureUploaded = true;
	}

	// upload unscaled one if NPOT and not supported
	if( m_TexData.width != m_glWidth || m_TexData.height != m_glHeight )
	{
		TextureRepresentation& rep = padded;
		UInt8* srcData = rep.data;

		UInt32 uploadFlags = (dontUseSubImage || !m_UnscaledTextureUploaded) ? GfxDevice::kUploadTextureDontUseSubImage : GfxDevice::kUploadTextureDefault;
		if (m_InitFlags & kOSDrawingCompatible)
			uploadFlags |= GfxDevice::kUploadTextureOSDrawingCompatible;

		m_UnscaledTextureUploaded = true; // must be before Upload in order for GetUnscaledGLTextureName() to return the right value
		TextureID tid = GetUnscaledTextureID();
		GetGfxDevice().UploadTexture2D( tid, GetDimension(), srcData, rep.imageSize, rep.width, rep.height, rep.format, mipCount, uploadFlags, masterTextureLimit, usageMode, GetActiveTextureColorSpace() );
		Texture::s_TextureIDMap.insert (std::make_pair(tid,this));
		m_TextureSettings.Apply( tid, GetDimension(), m_MipMap, GetActiveTextureColorSpace() );
	}

#if UNITY_XENON && !MASTER_BUILD
	GetGfxDevice().SetTextureName( GetTextureID(), GetName() );
#endif

#if UNITY_EDITOR
	DestroyTextureRepresentations(&scaled, &padded, false);
#elif UNITY_WII
	// On Wii texture data is being directly referenced from m_TexData.data, so don't delete it after uploading
	DestroyTextureRepresentations(&scaled, &padded, false);
#else
	DestroyTextureRepresentations(&scaled, &padded, !m_IsReadable);
	if(!m_IsReadable)
		m_TexData.imageSize = 0;
#endif
}

void Texture2D::DestroyTextureRepresentation( TextureRepresentation* rep )
{
	if( rep )
	{
		if( rep->data == m_TexData.data )
			rep->data = NULL;

		DeallocateTextureData (rep->data);
		rep->data = NULL;
	}
}

void Texture2D::DestroyTextureRepresentations( TextureRepresentation* scaled, TextureRepresentation* padded, bool freeSourceImage )
{
	DestroyTextureRepresentation(padded);
	DestroyTextureRepresentation(scaled);

	if( freeSourceImage )
	{
		DeallocateTextureData (m_TexData.data);
		m_TexData.data = NULL;
	}
}

void Texture2D::DeleteGfxTexture ()
{
	if (m_TextureUploaded)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		GetGfxDevice().DeleteTexture( GetTextureID() );
		m_TextureUploaded = false;
	}
	if (m_UnscaledTextureUploaded)
	{
		ASSERT_RUNNING_ON_MAIN_THREAD
		GetGfxDevice().DeleteTexture( GetUnscaledTextureID() );
		m_UnscaledTextureUploaded = false;
	}
}


void Texture2D::DestroyTexture ()
{
	DestroyTextureRepresentations (0,0,true);

	DeleteGfxTexture ();
}


void Texture2D::UnloadFromGfxDevice(bool forceUnloadAll)
{
	if (m_IsUnreloadable && !forceUnloadAll)
		return;
	DeleteGfxTexture ();
}

void Texture2D::UploadToGfxDevice()
{
	if (m_IsUnreloadable)
		return;

	GfxDevice& device = GetGfxDevice();

	// We need to load the texture data from disk. Awake FromLoad will be called during ReloadFromDisk.
	bool needReloadFromDisk = (m_TexData.data == NULL && !m_IsReadable);
	if (needReloadFromDisk)
	{
		TextureSettings settings = m_TextureSettings;

		GetPersistentManager().ReloadFromDisk(this);

		m_TextureSettings = settings;
		ApplySettings();
	}
	// Just upload the texture data we already have in memory
	else
	{
		UploadTexture (true);
	}
}


#if UNITY_EDITOR
struct TemporaryTextureSerializationRevert
{
	Texture2D* texture;
	Texture2D::TextureRepresentation texData;
	int imageCount;
	bool isReadable;

	TemporaryTextureSerializationRevert (Texture2D& tex2D, bool doRevert)
	{
		texture = NULL;

		if (doRevert)
		{
			texture = &tex2D;

			texData = texture->m_TexData;
			imageCount = texture->m_ImageCount;
			isReadable = texture->m_IsReadable;
		}
	}
	~TemporaryTextureSerializationRevert ()
	{
		if (texture != NULL)
		{
			texture->m_TexData = texData;
			texture->m_ImageCount = imageCount;
			texture->m_IsReadable = isReadable;
		}
	}
};
#endif

template<class TransferFunction>
void Texture2D::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

#if UNITY_EDITOR

	// Store serialization state temporarliy and revert it when exiting function
	TemporaryTextureSerializationRevert revert(*this, !transfer.IsReading ());

	// When writing dynamic font textures we don't want to write any texture data to disk
	if (m_EditorDontWriteTextureData && !transfer.IsReading ())
	{
		m_ImageCount = m_TexData.imageSize = m_TexData.height = m_TexData.width = 0;
		m_TexData.data = NULL;
	}

	// readable flag gets adjusted by forced readable flag
	if (transfer.IsBuildingTargetPlatform(kBuildAnyPlayerData))
		m_IsReadable |= transfer.GetBuildUsage().forceTextureReadable;

	TRANSFER_EDITOR_ONLY_HIDDEN(m_AlphaIsTransparency);
	transfer.Align();
#endif

	// In case we're converting the texture data to another format, we also properly write out the format ID.
	transfer.Transfer( m_TexData.width, "m_Width", kNotEditableMask );
	transfer.Transfer( m_TexData.height, "m_Height", kNotEditableMask );
	transfer.Transfer( m_TexData.imageSize, "m_CompleteImageSize", kNotEditableMask );
	transfer.Transfer( m_TexData.format, "m_TextureFormat", kHideInEditorMask );
	transfer.Transfer( m_MipMap, "m_MipMap", kNotEditableMask );

	transfer.Transfer( m_IsReadable, "m_IsReadable");
	transfer.Transfer( m_ReadAllowed, "m_ReadAllowed", kNotEditableMask );
	transfer.Align();
	transfer.Transfer( m_ImageCount, "m_ImageCount", kNotEditableMask );
	transfer.Transfer( m_TextureDimension, "m_TextureDimension", kHideInEditorMask );
	transfer.Transfer( m_TextureSettings, "m_TextureSettings");
	transfer.Transfer( m_UsageMode, "m_LightmapFormat");
	transfer.Transfer( m_ColorSpace, "m_ColorSpace");

	unsigned imageSizeXImageCount = m_ImageCount * m_TexData.imageSize;

	if (!transfer.IsWritingGameReleaseData ())
		transfer.TransferTypeless (&imageSizeXImageCount, "image data", kHideInEditorMask);

	if (transfer.IsReading ())
	{
		DestroyTexture ();
		Assert(GetMemoryLabel().label != kMemTextureCacheId);
		m_TexData.data = AllocateTextureData(imageSizeXImageCount, m_TexData.format);
		m_glWidth  = GetNextAllowedTextureSize(m_TexData.width, m_MipMap, TextureFormat(m_TexData.format));
		m_glHeight = GetNextAllowedTextureSize(m_TexData.height, m_MipMap, TextureFormat(m_TexData.format));
		SetTexelSize( 1.0f/m_glWidth, 1.0f/m_glHeight );
	}

	// write tex image
	if (transfer.IsWriting())
	{
		const bool      gpuBE           = UNITY_EDITOR ? transfer.IsBuildingTargetPlatform(kBuildXBOX360) : false;

		UInt8* convertedData = NULL;
		if( transfer.ConvertEndianess() )
		{
			convertedData = AllocateTextureData(imageSizeXImageCount, m_TexData.format);
			ConvertTextureEndianessWrite(m_TexData.format, m_TexData.data, convertedData, imageSizeXImageCount, gpuBE);
		}

		if (transfer.IsWritingGameReleaseData ())
			transfer.TransferTypeless (&imageSizeXImageCount, "image data", kHideInEditorMask);
		transfer.TransferTypelessData (imageSizeXImageCount, convertedData ? convertedData : m_TexData.data );

		DeallocateTextureData(convertedData);
	}
	else
	{
		if( transfer.ConvertEndianess() )
		{
			transfer.TransferTypelessData (imageSizeXImageCount, m_TexData.data);
			ConvertTextureEndianessRead (m_TexData.format, m_TexData.data, imageSizeXImageCount);
		}
		else
		{
			transfer.TransferTypelessData (imageSizeXImageCount, m_TexData.data);
		}
	}

	AssertIf ( m_TexData.imageSize != 0 && m_TexData.data == NULL );

	if( transfer.IsReading() )
		UpdatePOTStatus();
}

void ConvertTextureEndianessWrite (int format, UInt8* src, UInt8* dst, int size, bool bBigEndianGPU)
{
	memcpy (dst, src, size);
	if (format == kTexFormatARGBFloat)
		SwapEndianArray (dst, 4, size / 4);
	else if (format == kTexFormatRGB565 || format == kTexFormatARGB4444 || format == kTexFormatRGBA4444)
		SwapEndianArray (dst, 2, size / 2);
	else if (bBigEndianGPU &&	(format == kTexFormatDXT1 || format == kTexFormatDXT3 || format == kTexFormatDXT5))
		SwapEndianArray (dst, 2, size / 2);

}

void ConvertTextureEndianessRead (int format, UInt8* dst, int size)
{
	if (format == kTexFormatARGBFloat)
		SwapEndianArray (dst, 4, size / 4);
	else if (format == kTexFormatRGB565 || format == kTexFormatARGB4444 || format == kTexFormatRGBA4444)
		SwapEndianArray (dst, 2, size / 2);
}


void Texture2D::UpdateImageData ()
{
	RebuildMipMap ();
	UploadTexture (false);
	SetDirty ();
}

void Texture2D::UpdateImageDataDontTouchMipmap ()
{
	UploadTexture (false);
	SetDirty ();
}


TextureID Texture2D::GetUnscaledTextureID() const
{
	return m_UnscaledTextureUploaded ? m_UnscaledTexID : m_TexID;
}
int Texture2D::GetDataWidth() const
{
	return m_TexData.width;
}
int Texture2D::GetDataHeight() const
{
	return m_TexData.height;
}


void Texture2D::InitTextureRepresentation( TextureRepresentation* rep, int format, const char* tag )
{
	Assert(rep);

	rep->width  = GetNextAllowedTextureSize(m_TexData.width, m_MipMap, TextureFormat(format));
	rep->height = GetNextAllowedTextureSize(m_TexData.height, m_MipMap, TextureFormat(format));
	rep->format = format;

	if( m_MipMap )
		rep->imageSize = CalculateImageMipMapSize( rep->width, rep->height, rep->format );
	else
		rep->imageSize = CalculateImageSize( rep->width, rep->height, rep->format );

	rep->data = AllocateTextureData(rep->imageSize * m_ImageCount, rep->format);
}

void Texture2D::ExtractMipLevel( TextureRepresentation* dst, int frame, int mipLevel, bool checkCompression, bool scaleToSize )
{
	if (dst->width == 0 || dst->height == 0)
		return;

	UInt8* data = dst->data + frame * dst->imageSize + CalculateMipMapOffset(dst->width, dst->height, dst->format, mipLevel );


	int width  = std::max( dst->width >> mipLevel, 1 );
	int height = std::max( dst->height >> mipLevel, 1 );

	if( checkCompression && IsAnyCompressedTextureFormat(dst->format) )
	{
		ExtractCompressedImageInternal( data, width, height, frame );
	}
	else
	{
		ImageReference ref( width, height, width*GetBytesFromTextureFormat(dst->format), dst->format, data );
		ExtractImageInternal( &ref, scaleToSize, frame );
	}
}


void Texture2D::InitTextureRepresentations(Texture2D::TextureRepresentation* scaled, Texture2D::TextureRepresentation* padded)
{
	Assert(scaled);
	Assert(padded);

	if( m_TextureDimension == kTexDimDeprecated1D )
		m_TextureDimension = kTexDim2D;

	int multipleMask = GetTextureSizeAllowedMultiple(TextureFormat(m_TexData.format)) - 1;
	bool isAllowedMultiple = (m_TexData.width & multipleMask) == 0 && (m_TexData.height & multipleMask) == 0;
	if (isAllowedMultiple && (m_PowerOfTwo || IsNPOTTextureAllowed(m_MipMap)))
	{
		*scaled = *padded = m_TexData;
		SetTexelSize( 1.0f / m_TexData.width, 1.0f / m_TexData.height );
		return;
	}

	DebugAssertIf( m_PowerOfTwo );
	DebugAssertIf( !m_TexData.data );

	InitTextureRepresentation(scaled, IsAnyCompressedTextureFormat(m_TexData.format) ? kTexFormatRGBA32 : m_TexData.format, "tex.scaled");
	InitTextureRepresentation(padded, m_TexData.format, "tex.padded");

	int mipcount = CountMipmaps();
	for( int frame = 0; frame < m_ImageCount; ++frame )
	{
		for( int mip = 0; mip < mipcount; ++mip )
		{
			ExtractMipLevel(scaled, frame, mip, false, true);
			ExtractMipLevel(padded, frame, mip, true, false);
		}
	}
}

int Texture2D::GetRuntimeMemorySize() const
{
#if ENABLE_MEM_PROFILER
	return Super::GetRuntimeMemorySize() +
		GetMemoryProfiler()->GetRelatedIDMemorySize(m_TexID.m_ID) +
		(m_UnscaledTextureUploaded?GetMemoryProfiler()->GetRelatedIDMemorySize(GetUnscaledTextureID().m_ID):0);
#endif
	return sizeof(Texture2D);
}


IMPLEMENT_CLASS (Texture2D)
IMPLEMENT_OBJECT_SERIALIZE (Texture2D)
INSTANTIATE_TEMPLATE_TRANSFER (Texture2D)

bool Texture2D::CheckHasPixelData () const
{
	if (m_TexData.data != NULL)
		return true;

	if (!m_IsReadable)
	{
		ErrorString(Format("Texture '%s' is not readable, the texture memory can not be accessed from scripts. You can make the texture readable in the Texture Import Settings.", GetName()));
	}
	else
	{
		ErrorString(Format("Texture '%s' has no data", GetName()));
	}
	return false;
}


void Texture2D::SetPixel (int frame, int x, int y, const ColorRGBAf& c)
{
	if (!CheckHasPixelData())
		return;

	if (frame > m_ImageCount) {
		ErrorString (Format ("SetPixel called on an undefined image (valid values are 0 - %d", m_ImageCount - 1));
		return;
	}

	ImageReference image;
	if (GetWriteImageReference(&image, frame, 0))
	{
		SetImagePixel (image, x, y, static_cast<TextureWrapMode>(GetSettings().m_WrapMode), c);
	}
	else
	{
		if( IsAnyCompressedTextureFormat(m_TexData.format) )
		{
			ErrorString(kUnsupportedSetPixelOpFormatMessage);
		}
		else
		{
			ErrorString("Unable to retrieve image reference");
		}
	}
}

void Texture2D::SetPixels( int x, int y, int width, int height, int pixelCount, const ColorRGBAf* pixels, int miplevel, int frame )
{
	if (width == 0 || height == 0)
		return; // nothing to do

	if (!CheckHasPixelData())
		return;

	int mipcount = CountMipmaps();
	if (miplevel < 0 || miplevel >= mipcount)
	{
		ErrorString ("Invalid mip level");
		return;
	}

	UInt8* data = m_TexData.data + frame * m_TexData.imageSize;
	data += CalculateMipMapOffset (m_TexData.width, m_TexData.height, m_TexData.format, miplevel);
	int dataWidth = std::max (m_TexData.width >> miplevel, 1);
	int dataHeight = std::max (m_TexData.height >> miplevel, 1);

	SetImagePixelBlock (data, dataWidth, dataHeight, m_TexData.format, x, y, width, height, pixelCount, pixels);
}



ColorRGBAf Texture2D::GetPixel (int frame,int x, int y) const
{
	if (!CheckHasPixelData())
		return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);

	if (frame > m_ImageCount) {
		ErrorString (Format ("GetPixel called on an undefined image (valid values are 0 - %d", m_ImageCount - 1));
		return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);
	}

	return GetImagePixel (m_TexData.data + frame*m_TexData.imageSize, m_TexData.width, m_TexData.height, m_TexData.format, static_cast<TextureWrapMode>(GetSettings().m_WrapMode), x, y);
}

ColorRGBAf Texture2D::GetPixelBilinear (int frame, float u, float v) const
{
	if (!CheckHasPixelData())
		return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);
	if (frame > m_ImageCount) {
		ErrorString (Format ("GetPixelBilinear called on an undefined image (valid values are 0 - %d", m_ImageCount - 1));
		return ColorRGBAf(1.0F,1.0F,1.0F,1.0F);
	}
	return GetImagePixelBilinear (m_TexData.data + frame*m_TexData.imageSize, m_TexData.width, m_TexData.height, m_TexData.format, static_cast<TextureWrapMode>(GetSettings().m_WrapMode), u, v);
}

bool Texture2D::GetPixels( int x, int y, int width, int height, int miplevel, ColorRGBAf* colors, int frame ) const
{
	if (width == 0 || height == 0)
		return true; // nothing to do

	if (!CheckHasPixelData())
		return false;

	int mipcount = CountMipmaps();
	if (miplevel < 0 || miplevel >= mipcount)
	{
		ErrorString ("Invalid mip level");
		return false;
	}

	UInt8* data = m_TexData.data + frame * m_TexData.imageSize;
	data += CalculateMipMapOffset (m_TexData.width, m_TexData.height, m_TexData.format, miplevel);
	int dataWidth = std::max (m_TexData.width >> miplevel, 1);
	int dataHeight = std::max (m_TexData.height >> miplevel, 1);

	return GetImagePixelBlock (data, dataWidth, dataHeight, m_TexData.format, x, y, width, height, colors);
}

bool Texture2D::GetPixels32( int miplevel, ColorRGBA32* colors ) const
{
	AssertIf( miplevel < 0 || miplevel >= CountDataMipmaps() );

	ImageReference texImage;
	if( !GetImageReferenceInternal(&texImage, 0, miplevel) )
	{
		if (m_TexData.data != NULL && IsAnyCompressedTextureFormat(m_TexData.format))
		{
			UInt8* data = m_TexData.data + CalculateMipMapOffset( m_TexData.width, m_TexData.height, m_TexData.format, miplevel );
			const int minSize = GetMinimumTextureMipSizeForFormat(m_TexData.format);
			// the decompress size is at least minSize x minSize
			int width = std::max (m_TexData.width >> miplevel, minSize);
			int height = std::max (m_TexData.height >> miplevel, minSize);
			// also, it should be a multiple of minSize
			if (width % minSize == 0 && height % minSize == 0)
			{
				DecompressNativeTextureFormatWithMipLevel( m_TexData.format, width, height,miplevel, reinterpret_cast<const UInt32*>(data), width, height, reinterpret_cast<UInt32*>(colors) );
				return true;
			}
			else
			{
				// make it an upper multiple of minSize
				int decompWidth = (width + minSize - 1) / minSize * minSize;
				int decompHeight = (height + minSize - 1) / minSize * minSize;
				Image decompressed( decompWidth, decompHeight, kTexFormatRGBA32 );
				DecompressNativeTextureFormatWithMipLevel( m_TexData.format, width, height,miplevel, reinterpret_cast<const UInt32*>(data), decompWidth, decompHeight, (UInt32*)decompressed.GetImageData() );
				// clip it back to the original size
				ImageReference clipped = decompressed.ClipImage( 0, 0, width, height );
				// blit it over to the colors array
				ImageReference resultImage( width, height, GetRowBytesFromWidthAndFormat(width,kTexFormatRGBA32), kTexFormatRGBA32, colors );
				resultImage.BlitImage( clipped, ImageReference::BLIT_COPY );
				return true;
			}
		}
		AssertString( "Invalid texture" );
		return false;
	}

	int texWidth = texImage.GetWidth();
	int texHeight = texImage.GetHeight();

	ImageReference resultImage( texWidth, texHeight, GetRowBytesFromWidthAndFormat(texWidth,kTexFormatRGBA32), kTexFormatRGBA32, colors );
	resultImage.BlitImage( texImage, ImageReference::BLIT_COPY );

	return true;
}


void Texture2D::SetPixels32( int miplevel, const ColorRGBA32* pixels, const int pixelCount )
{
	AssertIf( miplevel < 0 || miplevel >= CountMipmaps() );

	ImageReference texImage;
	if( !GetWriteImageReference(&texImage, 0, miplevel) )
	{
		AssertString( "Invalid texture format" );
		return;
	}

	int texWidth = texImage.GetWidth();
	int texHeight = texImage.GetHeight();

	if(texWidth * texHeight != pixelCount)
	{
		AssertString( "SetPixels32 called with invalid number if pixels in the array" );
		return;
	}

	ImageReference inputImage( texWidth, texHeight, GetRowBytesFromWidthAndFormat(texWidth,kTexFormatRGBA32), kTexFormatRGBA32, (void*)pixels );
	texImage.BlitImage( inputImage, ImageReference::BLIT_COPY );
}

static const char* kUnsupportedColorPixelFormatMessage = "Unsupported image format - the texture needs to be ARGB32 or RGB24";

static bool IsAllowedToReadPixels ()
{
#if UNITY_EDITOR
	// from Player.h
	bool IsInsidePlayerLoop ();
	// Editor needs to read frame buffer outside player loop
	if( !IsInsidePlayerLoop() )
		return true;
#elif WEBPLUG
	// Allow old web content for compatibility
	if( !IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1) )
		return true;
#endif
	return GetGfxDevice().IsInsideFrame() || RenderTexture::GetActive();
}

// TODO: Check dimensions, make everything work.
void Texture2D::ReadPixels (int frame, int left, int bottom, int width, int height, int destX, int destY, bool flipped, bool computeMipMap)
{
	//Prevent out of bounds reads (case 562067)
	if (destX < 0 || destY < 0 || destX >= GetDataWidth() || destY >= GetDataHeight())
	{
		ErrorString("Trying to read pixels out of bounds");
		return;
	}
	if (width < 0 || height < 0) 
	{
		ErrorString("Negative read pixels rectangle width|height");
		return;
	}

	if( !IsAllowedToReadPixels() )
	{
		ErrorString ("ReadPixels was called to read pixels from system frame buffer, while not inside drawing frame.");
		// Our tests rely on calling ReadImage() during Update() so we allow it anyway...
	}

	if( frame >= m_ImageCount )
	{
		ErrorString (Format ("ReadPixels called on undefined image %d (valid values are 0 - %d", frame, m_ImageCount - 1));
		return;
	}

	GfxDeviceRenderer renderer = GetGfxDevice().GetRenderer();
	bool isGLES = (renderer == kGfxRendererOpenGLES20Mobile || renderer == kGfxRendererOpenGLES30);
	int texFormat = GetTextureFormat();
	bool validFormat =	texFormat == kTexFormatARGB32	||
						texFormat == kTexFormatRGB24	||
						texFormat == (kTexFormatRGBA32 && isGLES)	||
						texFormat == (kTexFormatRGB565 && isGLES);
	if( !validFormat )
	{
		ErrorString(kUnsupportedColorPixelFormatMessage);
		return;
	}

	ImageReference image;
	if( !GetWriteImageReference(&image, frame, 0) )
	{
		ErrorString("Unable to retrieve image reference");
		return;
	}

	if (RenderTexture::GetActive() == NULL)
	{
		Rectf rc = GetRenderManager().GetWindowRect();
		left += rc.x;
		bottom += rc.y;
	}

	if (left < 0) {
		width += left;
		left = 0;
	}
	if (bottom < 0) {
		height += bottom;
		bottom = 0;
	}
	if (destX + width > GetDataWidth())
		width = GetDataWidth() - destX;
	if (destY + height > GetDataHeight())
		height = GetDataHeight() - destY;

	GetGfxDevice().ReadbackImage( image, left, bottom, width, height, destX, destY );

	if (flipped)
	{
		ImageReference subImage = image.ClipImage (destX, destY, width, height);
		subImage.FlipImageY();
	}

	if( computeMipMap && m_MipMap )
		RebuildMipMap();
}


#if ENABLE_PNG_JPG
bool Texture2D::EncodeToPNG( dynamic_array<UInt8>& outBuffer )
{
	if( IsAnyCompressedTextureFormat(GetTextureFormat()) )
	{
		ErrorString(kUnsupportedSetPixelOpFormatMessage);
		return false;
	}
	ImageReference image;
	if( !GetWriteImageReference( &image, 0, 0 ) )
	{
		ErrorString( "Unable to retrieve image reference" );
		return false;
	}
	if( !ConvertImageToPNGBuffer( image, outBuffer ) )
	{
		ErrorString( "Failed to encode to PNG" );
		return false;
	}

	return true;
}
#endif



bool Texture2D::ResizeWithFormat (int width, int height, TextureFormat format, int flags)
{
	if (!m_IsReadable)
	{
		ErrorString ("Texture is not readable.");
		return false;
	}

	if( IsAnyCompressedTextureFormat(format) )
	{
		ErrorStringObject ("Can't resize to a compressed texture format", this);
		return false;
	}

	return InitTexture(width, height, format, flags);
}


void Texture2D::Compress (bool dither)
{
	if (!m_IsReadable)
	{
		ErrorString(Format("Texture '%s' is not readable, Compress will not work. You can make the texture readable in the Texture Import Settings.", GetName()));
		return;
	}

	SET_ALLOC_OWNER(this);
	// If hardware does not support DXT, then nothing to do.
	if( !gGraphicsCaps.hasS3TCCompression )
		return;

	#if !GFX_SUPPORTS_DXT_COMPRESSION
	Assert(!"GraphicsCaps.hasS3TCCompression is invalid");
	return;
	#else

	TextureFormat format = GetTextureFormat();
	// If already compressed, then nothing to do.
	if( IsAnyCompressedTextureFormat(format) )
		return;

	// Copy out old data into RGBA32 format.
	bool mipMaps = HasMipMap();
	int width = GetDataWidth();
	int height = GetDataHeight();
	int rgbaByteSize = mipMaps ?
		CalculateImageMipMapSize( width, height, kTexFormatRGBA32 ) :
		CalculateImageSize( width, height, kTexFormatRGBA32 );
	UInt8* rgbaData = new UInt8[rgbaByteSize];
	int mipCount = CountDataMipmaps();
	for( int mip = 0; mip < mipCount; ++mip )
	{
		UInt8* rgbaMipData = rgbaData + CalculateMipMapOffset(width, height, kTexFormatRGBA32, mip);
		int mipWidth = std::max( width >> mip, 1 );
		int mipHeight = std::max( height >> mip, 1 );
		ImageReference mipDst( mipWidth, mipHeight, mipWidth * 4, kTexFormatRGBA32, rgbaMipData );
		ExtractImageInternal( &mipDst, false, 0 );
	}

	// Reformat texture into DXT format
	bool hasAlpha = HasAlphaTextureFormat(format);
	TextureFormat compressedFormat = hasAlpha ? kTexFormatDXT5 : kTexFormatDXT1;
	InitTexture( width, height, compressedFormat, mipMaps ? kMipmapMask : kNoMipmap );

	// DXT compress RGBA data
	for( int mip = 0; mip < mipCount; ++mip )
	{
		const UInt8* srcMipData = rgbaData + CalculateMipMapOffset(width, height, kTexFormatRGBA32, mip);
		UInt8* dstMipData = GetRawImageData() + CalculateMipMapOffset(width, height, compressedFormat, mip);
		int mipWidth = std::max( width >> mip, 1 );
		int mipHeight = std::max( height >> mip, 1 );
		FastCompressImage( mipWidth, mipHeight, srcMipData, dstMipData, hasAlpha, dither );
	}

	delete[] rgbaData;

	UpdateImageDataDontTouchMipmap();
	#endif //GFX_SUPPORTS_DXT_COMPRESSION
}

#if UNITY_EDITOR
void Texture2D::WarnInstantiateDisallowed ()
{
	if (!m_IsReadable)
	{
		ErrorStringObject(Format("Instantiating a non-readable '%s' texture is not allowed! Please mark the texture readable in the inspector or don't instantiate it.", GetName()), this);
	}
}

bool Texture2D::IgnoreMasterTextureLimit () const
{
	return m_IgnoreMasterTextureLimit;
}

void Texture2D::SetIgnoreMasterTextureLimit (bool ignore)
{
	m_IgnoreMasterTextureLimit = ignore;
}

bool Texture2D::GetAlphaIsTransparency() const
{
	return m_AlphaIsTransparency;
}

void Texture2D::SetAlphaIsTransparency(bool is)
{
	m_AlphaIsTransparency = is;
}
#endif // UNITY_EDITOR


void Texture2D::Apply(bool updateMipmaps, bool makeNoLongerReadable)
{
	if( makeNoLongerReadable )
	{
		SetIsReadable(false);
		SetIsUnreloadable(true);
	}

	if( IsAnyCompressedTextureFormat(GetTextureFormat()) )
		updateMipmaps = false;

	if (updateMipmaps) 	UpdateImageData();
	else				UpdateImageDataDontTouchMipmap();
}


