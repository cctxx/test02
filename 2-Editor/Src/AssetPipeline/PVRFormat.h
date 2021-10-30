#ifndef PVRFORMAT_H
#define PVRFORMAT_H

#define PVRTuint32 unsigned int

// PVR format description from PVRTTexture.h

struct PVR_Texture_Header
{
	PVRTuint32 dwHeaderSize;                /*!< size of the structure */
	PVRTuint32 dwHeight;                    /*!< height of surface to be created */
	PVRTuint32 dwWidth;                     /*!< width of input surface */
	PVRTuint32 dwMipMapCount;               /*!< number of mip-map levels requested */
	PVRTuint32 dwpfFlags;                   /*!< pixel format flags */
	PVRTuint32 dwTextureDataSize;           /*!< Total size in bytes */
	PVRTuint32 dwBitCount;                  /*!< number of bits per pixel  */
	PVRTuint32 dwRBitMask;                  /*!< mask for red bit */
	PVRTuint32 dwGBitMask;                  /*!< mask for green bits */
	PVRTuint32 dwBBitMask;                  /*!< mask for blue bits */
	PVRTuint32 dwAlphaBitMask;              /*!< mask for alpha channel */
	PVRTuint32 dwPVR;                       /*!< magic number identifying pvr file */
	PVRTuint32 dwNumSurfs;                  /*!< the number of surfaces present in the pvr */
};

enum PVR_PixelType
{
	MGLPT_ARGB_4444 = 0x00,
	MGLPT_ARGB_1555,
	MGLPT_RGB_565,
	MGLPT_RGB_555,
	MGLPT_RGB_888,
	MGLPT_ARGB_8888,
	MGLPT_ARGB_8332,
	MGLPT_I_8,
	MGLPT_AI_88,
	MGLPT_1_BPP,
	MGLPT_VY1UY0,
	MGLPT_Y1VY0U,
	MGLPT_PVRTC2,
	MGLPT_PVRTC4,
	MGLPT_PVRTC2_2,
	MGLPT_PVRTC2_4,
	
	OGL_RGBA_4444= 0x10,
	OGL_RGBA_5551,
	OGL_RGBA_8888,
	OGL_RGB_565,
	OGL_RGB_555,
	OGL_RGB_888,
	OGL_I_8,
	OGL_AI_88,
	OGL_PVRTC2,
	OGL_PVRTC4,
	
	// OGL_BGRA_8888 extension
	OGL_BGRA_8888,
	
	D3D_DXT1 = 0x20,
	D3D_DXT2,
	D3D_DXT3,
	D3D_DXT4,
	D3D_DXT5,
	
	D3D_RGB_332,
	D3D_AI_44,
	D3D_LVU_655,
	D3D_XLVU_8888,
	D3D_QWVU_8888,
	
	//10 bits per channel
	D3D_ABGR_2101010,
	D3D_ARGB_2101010,
	D3D_AWVU_2101010,
	
	//16 bits per channel
	D3D_GR_1616,
	D3D_VU_1616,
	D3D_ABGR_16161616,
	
	//HDR formats
	D3D_R16F,
	D3D_GR_1616F,
	D3D_ABGR_16161616F,
	
	//32 bits per channel
	D3D_R32F,
	D3D_GR_3232F,
	D3D_ABGR_32323232F,
	
	// Ericsson
	ETC_RGB_4BPP,
	ETC_RGBA_EXPLICIT,
	ETC_RGBA_INTERPOLATED,
	
	// DX10 and OpenVG removed
	
	MGLPT_NOTYPE = 0xff
	
};

const PVRTuint32 PVRTEX_MIPMAP          = (1<<8);               // has mip map levels
const PVRTuint32 PVRTEX_TWIDDLE         = (1<<9);               // is twiddled
const PVRTuint32 PVRTEX_BUMPMAP         = (1<<10);              // has normals encoded for a bump map
const PVRTuint32 PVRTEX_TILING          = (1<<11);              // is bordered for tiled pvr
const PVRTuint32 PVRTEX_CUBEMAP         = (1<<12);              // is a cubemap/skybox
const PVRTuint32 PVRTEX_FALSEMIPCOL     = (1<<13);              // are there false coloured MIP levels
const PVRTuint32 PVRTEX_VOLUME          = (1<<14);              // is this a volume texture
const PVRTuint32 PVRTEX_ALPHA           = (1<<15);              // v2.1 is there transparency info in the texture
const PVRTuint32 PVRTEX_VERTICAL_FLIP   = (1<<16);              // v2.1 is the texture vertically flipped

const PVRTuint32 PVRTEX_PIXELTYPE       = 0xff;                 // pixel type is always in the last 16bits of the flags
const PVRTuint32 PVRTEX_IDENTIFIER      = 0x21525650;           // the pvr identifier is the characters 'P','V','R'

const PVRTuint32 PVRTEX_V1_HEADER_SIZE  = 44;                   // old header size was 44 for identification purposes

const PVRTuint32 PVRTC2_MIN_TEXWIDTH    = 16;
const PVRTuint32 PVRTC2_MIN_TEXHEIGHT   = 8;
const PVRTuint32 PVRTC4_MIN_TEXWIDTH    = 8;
const PVRTuint32 PVRTC4_MIN_TEXHEIGHT   = 8;
const PVRTuint32 ETC_MIN_TEXWIDTH       = 4;
const PVRTuint32 ETC_MIN_TEXHEIGHT      = 4;
const PVRTuint32 DXT_MIN_TEXWIDTH       = 4;
const PVRTuint32 DXT_MIN_TEXHEIGHT      = 4;

#endif