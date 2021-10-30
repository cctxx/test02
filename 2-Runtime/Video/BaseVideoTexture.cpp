#include "UnityPrefix.h"
#include "BaseVideoTexture.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/GfxDevice/GfxDevice.h"

using std::vector;

extern void GenerateLookupTables ();
extern bool IsNPOTTextureAllowed(bool hasMipMap);

void BaseVideoTexture::UploadTextureData()	//Upload image buffer to texture memory
{
	if(m_ImageBuffer)
	{
		// pad to assure clamping works
		if (m_PaddedHeight > m_VideoHeight && m_PaddedHeight > 1)
			for (int x = 0;x < m_PaddedWidth;x++)
				m_ImageBuffer[m_VideoHeight * m_PaddedWidth + x] = m_ImageBuffer[(m_VideoHeight - 1)*m_PaddedWidth + x];
		if (m_PaddedWidth > m_VideoWidth && m_PaddedWidth > 1)
			for (int y = 0;y < m_PaddedHeight;y++)
				m_ImageBuffer[y * m_PaddedWidth + m_VideoWidth] = m_ImageBuffer[y * m_PaddedWidth + m_VideoWidth - 1];

		// Image buffer is 32 bits per pixel
		int dataSize = m_PaddedWidth * m_PaddedHeight * sizeof(UInt32);
		GetGfxDevice().UploadTextureSubData2D( GetTextureID(), (UInt8*)m_ImageBuffer, dataSize, 0, 0, 0, m_PaddedWidth, m_PaddedHeight, GetBufferTextureFormat(), GetActiveTextureColorSpace() );
	}

	m_DidUpdateThisFrame = m_ImageBuffer || !m_IsReadable;
}

///@TODO: THIS IS NOT THREAD SAFE

typedef UNITY_VECTOR(kMemTexture, BaseVideoTexture*) VideoList;
VideoList gVideoList;

void BaseVideoTexture::UpdateVideoTextures()
{
	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();++i)
	{
		(**i).m_DidUpdateThisFrame = false;
		if((**i).m_EnableUpdates)
			(**i).Update();
	}
}

void BaseVideoTexture::PauseVideoTextures()
{
	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();++i)
		(**i).Pause();
}

void BaseVideoTexture::StopVideoTextures()
{
	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();++i)
	{
		(**i).Stop();

		// Call this to reset video texture frame contents to black.
		(**i).UnloadFromGfxDevice(false);
		(**i).UploadToGfxDevice();
	}
}

void BaseVideoTexture::SuspendVideoTextures()
{
	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();++i)
		(**i).Suspend();
}

void BaseVideoTexture::ResumeVideoTextures()
{
	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();++i)
		(**i).Resume();
}

void BaseVideoTexture::InitVideoMemory(int width, int height)
{
	m_VideoWidth = width;
	m_VideoHeight = height;

	m_TextureWidth  = IsNPOTTextureAllowed(false) ? m_VideoWidth  : NextPowerOfTwo(m_VideoWidth);
	m_TextureHeight = IsNPOTTextureAllowed(false) ? m_VideoHeight : NextPowerOfTwo(m_VideoHeight);

	m_PaddedHeight = m_VideoHeight + 1;
	m_PaddedWidth = m_VideoWidth + 1;

	if( m_PaddedHeight > m_TextureHeight )
		m_PaddedHeight = m_TextureHeight;
	if( m_PaddedWidth > m_TextureWidth )
		m_PaddedWidth = m_TextureWidth;

	if(m_IsReadable)
	{
		// The allocated buffer for one frame has extra line before the pointer
		// we use in all operations. YUV decoding code operates two lines at a time,
		// goes backwards and thus needs extra line before the buffer in case of odd
		// movie sizes.


		if (m_PaddedHeight+1 < m_PaddedHeight)
		{
			ErrorString("integer overflow in addition");
			return;
		}

		int tmp = m_PaddedWidth * (m_PaddedHeight+1);
		if ((m_PaddedHeight+1) != tmp / m_PaddedWidth)
		{
			ErrorString("integer overflow in multiplication");
			return;
		}

		int tmp2 = tmp * sizeof(UInt32);

		if (tmp != tmp2/sizeof(UInt32))
		{
			ErrorString("integer overflow in multiplication");
			return;
		}

		UInt32* realBuffer = (UInt32*)UNITY_MALLOC(GetMemoryLabel(), m_PaddedWidth * (m_PaddedHeight+1) * sizeof(UInt32));
		m_ImageBuffer = realBuffer + m_PaddedWidth;
		// Make sure to set the alpha in the image buffer, as it is not updated from the movie data.
		for(int i=0; i<m_PaddedWidth * m_PaddedHeight;i++)
		#if UNITY_LITTLE_ENDIAN
			m_ImageBuffer[i]=0x000000ff;
		#else
			m_ImageBuffer[i]=0xff000000;
		#endif
	}

	CreateGfxTextureAndUploadData(false);
}

void BaseVideoTexture::UploadGfxTextureBuffer(UInt32* imgBuf)
{
	TextureID texName = GetTextureID();

	int const dataSize = m_TextureWidth * m_TextureHeight * 4;
	GetGfxDevice().UploadTexture2D( texName, kTexDim2D, reinterpret_cast<UInt8*>(imgBuf), dataSize, 
		m_TextureWidth, m_TextureHeight, GetBufferTextureFormat(), 1, GfxDevice::kUploadTextureDontUseSubImage, 0, kTexUsageNone, GetActiveTextureColorSpace() );
	Texture::s_TextureIDMap.insert (std::make_pair(texName,this));
}

void BaseVideoTexture::CreateGfxTextureAndUploadData(bool uploadCurrentBuffer)
{
	if(m_IsReadable)
	{
		if (m_TextureWidth == m_PaddedWidth && m_TextureHeight == m_PaddedHeight)
		{
			// Simply upload the buffer that we currently have; its size is the same
			// as the size of the temporary buffer that we would create.
			Assert(m_ImageBuffer != NULL);

			UploadGfxTextureBuffer(m_ImageBuffer);
			
			// The image buffer already uploaded, no need to duplicate the work
			uploadCurrentBuffer = false;
		}
		else // image buffer size differs from texture size
		{
			// Since we are using a buffer smaller then the actual texture for continuous updates,
			// we need a bigger temp buffer once to initialize the texture contents.
			UInt32* tmpBuffer;
			ALLOC_TEMP_ALIGNED (tmpBuffer, UInt32, m_TextureWidth * m_TextureHeight, sizeof(UInt32));

			// Don't upload garbage texture.
			for(int i=0; i<m_TextureWidth * m_TextureHeight;i++)
			#if UNITY_LITTLE_ENDIAN
				tmpBuffer[i]=0x000000ff;
			#else
				tmpBuffer[i]=0xff000000;
			#endif

			UploadGfxTextureBuffer(tmpBuffer);
		}

		if (uploadCurrentBuffer)
			UploadTextureData(); // Upload the buffer to the created texture

		m_DidUpdateThisFrame = true;
	}

	GetGfxDevice().SetTextureParams( GetTextureID(), kTexDim2D, kTexFilterBilinear, kTexWrapClamp, 1, false, GetActiveTextureColorSpace() );
	// uvScale, so we can use the texture as if it was a normal power-of-two texture
	SetUVScale( m_VideoWidth/(float)m_TextureWidth, m_VideoHeight/(float)m_TextureHeight );
}

void BaseVideoTexture::ReleaseVideoMemory()
{
	if( m_ImageBuffer )
	{
		// The allocated buffer for one frame has extra line before the pointer
		// we use in all operations. YUV decoding code operates two lines at a time,
		// goes backwards and thus needs extra line before the buffer in case of odd
		// movie sizes.
		UInt32* realBuffer = m_ImageBuffer - m_PaddedWidth;
		UNITY_FREE(GetMemoryLabel(), realBuffer);

		m_ImageBuffer = NULL;
	}
}

BaseVideoTexture::BaseVideoTexture(MemLabelId label, ObjectCreationMode mode)
:	Texture(label, mode)
{
	m_VideoWidth = m_VideoHeight = 16;
	m_TextureWidth = m_TextureHeight = 16;
	m_PaddedWidth = m_PaddedHeight = 0;
	m_DidUpdateThisFrame = false;

	m_ImageBuffer = NULL;

	m_EnableUpdates = false;
	m_IsReadable = true;

	{
		SET_ALLOC_OWNER(NULL);
		gVideoList.push_back(this);
		GenerateLookupTables ();
	}
}

BaseVideoTexture::~BaseVideoTexture()
{
	ReleaseVideoMemory ();

	for(VideoList::iterator i=gVideoList.begin();i!=gVideoList.end();i++)
	{
		if(*i==this)
		{
			gVideoList.erase(i);
			break;
		}
	}

	GetGfxDevice().DeleteTexture( GetTextureID() );
}

void BaseVideoTexture::SetReadable(bool readable)
{
	if(CanSetReadable(readable))
		m_IsReadable = readable;
}

bool BaseVideoTexture::ExtractImage (ImageReference* image, int imageIndex) const
{
	if(m_ImageBuffer)
	{
		ImageReference source (m_VideoWidth, m_VideoHeight, m_PaddedWidth*4,GetBufferTextureFormat(),m_ImageBuffer);
		image->BlitImage( source, ImageReference::BLIT_COPY );

		return true;
	}
	else
		return false;
}

static int sAdjCrr[256];
static int sAdjCrg[256];
static int sAdjCbg[256];
static int sAdjCbb[256];
static int sAdjY[256];
static UInt8  sClampBuff[1024];
static UInt8* sClamp = sClampBuff + 384;


#define PROFILE_YUV_CONVERSION 0

#if PROFILE_YUV_CONVERSION
static __int64 GetCpuTicks ()
{
#if defined(GEKKO)
	return OSGetTick ();
#else
	__asm rdtsc;
	// eax/edx returned
#endif
}
#endif

// precalculate adjusted YUV values for faster RGB conversion
void GenerateLookupTables ()
{
	static bool generated = false;
	if (generated)
		return;
	int i;

	for (i = 0; i < 256; i++)
	{
		sAdjCrr[i] = (409 * (i - 128) + 128) >> 8;
		sAdjCrg[i] = (208 * (i - 128) + 128) >> 8;
		sAdjCbg[i] = (100 * (i - 128) + 128) >> 8;
		sAdjCbb[i] = (516 * (i - 128) + 128) >> 8;
		sAdjY[i] = (298 * (i - 16)) >> 8;
	}

	// and setup LUT clamp range
	for (i = -384; i < 0; i++)
		sClamp[i] = 0;
	for (i = 0; i < 256; i++)
		sClamp[i] = i;
	for (i = 256; i < 640; i++)
		sClamp[i] = 255;
	generated = true;
}

void BaseVideoTexture::YuvToRgb (const YuvFrame *yuv)
{
	#if PROFILE_YUV_CONVERSION
	__int64 time0 = GetCpuTicks();
	#endif
	UInt8 *rgbBuffer = (UInt8*)GetImageBuffer ();
	int const rowBytes = GetRowBytesFromWidthAndFormat(GetPaddedWidth(), GetBufferTextureFormat());

	// Somehow related to audio track being placed into an audio source in the
	// scene with play on load checked causes the first frame decoded to return
	// garbage (with yuv->u set to NULL).
	if ( yuv->u == NULL ) {
		return;
	}

	// NOTE: this code goes backwards in lines, two lines at a time. Thus for
	// odd image sizes it can under-run rgbBuffer. BaseVideoTexture code makes
	// sure there's one line worth of allocated memory before the passed
	// rgbBuffer.

	// get destination buffer (and 1 row offset)
	UInt8* dst0 = rgbBuffer + (yuv->height - 1)*rowBytes;
	UInt8 *dst1 = dst0 - rowBytes;

	// find picture offset
	int yOffset  = yuv->y_stride * yuv->offset_y + yuv->offset_x;
	int uvOffset  = yuv->uv_stride * (yuv->offset_y / 2) + (yuv->offset_x / 2);
	const int uvStep = yuv->uv_step;

	for ( int y = 0; y < yuv->height; y += 2 )
	{
		UInt8 *lineStart = dst1;

		// set pointers into yuv buffers (2 lines for y)
		const UInt8 *pY0 = yuv->y + yOffset + y * (yuv->y_stride);
		const UInt8 *pY1 = yuv->y + yOffset + (y | 1) * (yuv->y_stride);
		const UInt8 *pU = yuv->u + uvOffset + ((y * (yuv->uv_stride)) >> 1);
		const UInt8 *pV = yuv->v + uvOffset + ((y * (yuv->uv_stride)) >> 1);

		for (int x = 0; x < yuv->width; x += 2)
		{
			// convert a 2x2 block over
			const int yy00 = sAdjY[pY0[0]];
			const int yy10 = sAdjY[pY0[1]];
			const int yy01 = sAdjY[pY1[0]];
			const int yy11 = sAdjY[pY1[1]];

			// Compute RGB offsets
			const int vv = *pV;
			const int uu = *pU;
			const int R = sAdjCrr[vv];
			const int G = sAdjCrg[vv] + sAdjCbg[uu];
			const int B = sAdjCbb[uu];

			// pixel 0x0
			dst0++;
			*dst0++ = sClamp[yy00 + R];
			*dst0++ = sClamp[yy00 - G];
			*dst0++ = sClamp[yy00 + B];

			// pixel 1x0
			dst0++;
			*dst0++ = sClamp[yy10 + R];
			*dst0++ = sClamp[yy10 - G];
			*dst0++ = sClamp[yy10 + B];

			// pixel 0x1
			dst1++;
			*dst1++ = sClamp[yy01 + R];
			*dst1++ = sClamp[yy01 - G];
			*dst1++ = sClamp[yy01 + B];

			// pixel 1x1
			dst1++;
			*dst1++ = sClamp[yy11 + R];
			*dst1++ = sClamp[yy11 - G];
			*dst1++ = sClamp[yy11 + B];


			pY0 += 2;
			pY1 += 2;
			pV += uvStep;
			pU += uvStep;
		}

		// shift the destination pointers a row (loop increments 2 at a time)
		dst0 = lineStart - rowBytes;
		dst1 = dst0 - rowBytes;
	}

	#if PROFILE_YUV_CONVERSION
	__int64 time1 = GetCpuTicks();
	{
		__int64 deltaTime = (time1 - time0) / 1000;
		static __int64 accumTime = 0;
		static int counter = 0;
		accumTime += deltaTime;
		++counter;
		if ( counter == 20 )
		{
			printf_console( "YUV Kclocks per frame: %i\n", (int)(accumTime / counter) );
			counter = 0;
			accumTime = 0;
		}
	}
	#endif
}

//// Math! (Reference implementation)
//// http://en.wikipedia.org/wiki/YUV#Y.27UV422_to_RGB888_conversion
//static inline UInt32 ConvertYUYVtoRGBImpl(int c, int d, int e)
//{
//	int red = 0,
//	    green = 0,
//	    blue = 0;
//
//	red =   std::min (UCHAR_MAX, (298 * c           + 409 * e + 128) >> 8);
//	green = std::min (UCHAR_MAX, (298 * c - 100 * d - 208 * e + 128) >> 8);
//	blue =  std::min (UCHAR_MAX, (298 * c + 516 * d           + 128) >> 8);
//
//	return (red << 8) | (green << 16) | (blue << 24) | 0xff;
//}
//
//static inline UInt32 ConvertYCrCbToRGB(int y, int u, int v)
//{
//	return ConvertYUYVtoRGBImpl(y - 16, u - 128, v - 128);
//}

// LUT-based implementation
void BaseVideoTexture::YUYVToRGBA (UInt16 *const src)
{
	YUYVToRGBA(src, GetPaddedWidth ());
}

void BaseVideoTexture::YUYVToRGBA (UInt16 *const src, int srcStride)
{
	#if PROFILE_YUV_CONVERSION
	__int64 time0 = GetCpuTicks();
	#endif

	UInt16 *srcYUYV = src;
	UInt8 *destRGBA = reinterpret_cast<UInt8*> (GetImageBuffer ());
	int const destStride = GetRowBytesFromWidthAndFormat(GetPaddedWidth(), GetBufferTextureFormat());
	int const widthInPixels  = GetDataWidth ();
	int const heightInPixels = GetDataHeight ();
	int y0;
	int u;
	int y1;
	int v;
	int red;
	int green;
	int blue;

	destRGBA += (heightInPixels - 1) * destStride;

	// Lines within the destination rectangle.
	for (int y = 0; y < heightInPixels; ++y)
	{
		UInt8 *srcPixel = reinterpret_cast<UInt8*> (srcYUYV);

		// Increment by widthInPixels does not necessarily
		// mean that dstPixel is incremented by the stride, 
		// so we keep a separate pointer.
		UInt8 *dstPixel = destRGBA;

		for (int x = 0; (x + 1) < widthInPixels; x += 2)
		{
			// Byte order is Y0 U0 Y1 V0
			// Each word is a byte pair (Y, U/V)
			y0 = sAdjY[*srcPixel++];
			u = *srcPixel++;
			y1 = sAdjY[*srcPixel++];
			v = *srcPixel++;
			red = sAdjCrr[v];
			green = sAdjCrg[v] + sAdjCbg[u];
			blue = sAdjCbb[u];

			*dstPixel++ = 0xFF;
			*dstPixel++ = sClamp[y0 +  red];
			*dstPixel++ = sClamp[y0 - green];
			*dstPixel++ = sClamp[y0 + blue];
			*dstPixel++ = 0xFF;
			*dstPixel++ = sClamp[y1 + red];
			*dstPixel++ = sClamp[y1 - green];
			*dstPixel++ = sClamp[y1 + blue];
		}
		destRGBA -= destStride;
		srcYUYV += srcStride;
	}

	#if PROFILE_YUV_CONVERSION
	__int64 time1 = GetCpuTicks();
	{
		__int64 deltaTime = (time1 - time0) / 1000;
		static __int64 accumTime = 0;
		static int counter = 0;
		accumTime += deltaTime;
		++counter;
		if ( counter == 20 )
		{
			printf_console( "YUYV Kclocks per frame: %i\n", (int)(accumTime / counter) );
			counter = 0;
			accumTime = 0;
		}
	}
	#endif
}

