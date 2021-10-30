#include "UnityPrefix.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "D3D9Context.h"
#include "Runtime/Utilities/Utility.h"
#include "PlatformDependent/Win/WinDriverUtils.h"
#include "D3D9Utils.h"
#include <Shlwapi.h>

#define CAPS_DEBUG_DISABLE_RT 0


extern D3DFORMAT kD3D9RenderTextureFormats[kRTFormatCount];


extern D3DDEVTYPE g_D3DDevType;
extern DWORD g_D3DAdapter;

static bool IsTextureFormatSupported( D3DFORMAT format )
{
	if( format == D3DFMT_UNKNOWN )
		return false;
	HRESULT hr = GetD3DObject()->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), 0, D3DRTYPE_TEXTURE, format );
	return SUCCEEDED( hr );
}
static bool IsSRGBTextureReadSupported( D3DFORMAT format )
{
	if( format == D3DFMT_UNKNOWN )
		return false;
	HRESULT hr = GetD3DObject()->CheckDeviceFormat (g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_QUERY_SRGBREAD, D3DRTYPE_TEXTURE, format);
	return SUCCEEDED( hr );
}
static bool IsSRGBTextureWriteSupported( D3DFORMAT format )
{
	if( format == D3DFMT_UNKNOWN )
		return false;
	HRESULT hr = GetD3DObject()->CheckDeviceFormat (g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_TEXTURE, format);
	return SUCCEEDED( hr );
}
static bool IsRenderTextureFormatSupported( D3DFORMAT format )
{
	if( format == D3DFMT_UNKNOWN )
		return false;
	HRESULT hr = GetD3DObject()->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format );
	return SUCCEEDED( hr );
}

D3DFORMAT GetD3D9TextureFormat( TextureFormat inFormat ); // TexturesD3D9.cpp


enum {
	kVendorDummyRef = 0x0000,
	kVendor3DLabs	= 0x3d3d,
	kVendorMatrox	= 0x102b,
	kVendorS3		= 0x5333,
	kVendorSIS		= 0x1039,
	kVendorXGI		= 0x18ca,
	kVendorIntel	= 0x8086,
	kVendorATI		= 0x1002,
	kVendorNVIDIA	= 0x10de,
	kVendorTrident	= 0x1023,
	kVendorImgTech	= 0x104a,
	kVendorVIAS3G	= 0x1106,
	kVendor3dfx		= 0x121a,
	kVendorParallels= 0x1ab8,
	kVendorMicrosoft= 0x1414,
	kVendorVMWare	= 0x15ad,
};
struct KnownVendors {
	DWORD vendorId;
	const char* name;
};
static KnownVendors s_KnownVendors[] = {
	{ kVendorDummyRef,	"REFERENCE" },
	{ kVendor3DLabs,	"3dLabs" },
	{ kVendorMatrox,	"Matrox" },
	{ kVendorS3,		"S3" },
	{ kVendorSIS,		"SIS" },
	{ kVendorXGI,		"XGI" },
	{ kVendorIntel,		"Intel" },
	{ kVendorATI,		"ATI" },
	{ kVendorNVIDIA,	"NVIDIA" },
	{ kVendorTrident,	"Trident" },
	{ kVendorImgTech,	"Imagination Technologies" },
	{ kVendorVIAS3G,	"VIA/S3" },
	{ kVendor3dfx,		"3dfx" },
	{ kVendorParallels,	"Parallels" },
	{ kVendorMicrosoft,	"Microsoft" },
	{ kVendorVMWare,	"VMWare" },
};
static int kKnownVendorsSize = sizeof(s_KnownVendors)/sizeof(s_KnownVendors[0]);


void GraphicsCaps::InitD3D9()
{
	IDirect3D9* d3dobject = GetD3DObject();
	d3dobject->GetDeviceCaps( g_D3DAdapter, g_D3DDevType, &d3d.d3dcaps );

	// get renderer, vendor & driver information
	D3DADAPTER_IDENTIFIER9 adapterInfo;
	d3dobject->GetAdapterIdentifier( g_D3DAdapter, 0, &adapterInfo );
	adapterInfo.Driver[MAX_DEVICE_IDENTIFIER_STRING-1] = 0;
	adapterInfo.Description[MAX_DEVICE_IDENTIFIER_STRING-1] = 0;
	adapterInfo.DeviceName[31] = 0;
	rendererString = adapterInfo.Description;

	if (g_D3DDevType == D3DDEVTYPE_REF)
	{
		adapterInfo.VendorId = kVendorDummyRef;
		rendererString = "REF on " + rendererString;
	}

	int i;
	for( i = 0; i < kKnownVendorsSize; ++i )
	{
		if( s_KnownVendors[i].vendorId == adapterInfo.VendorId )
		{
			vendorString = s_KnownVendors[i].name;
			break;
		}
	}
	if( i == kKnownVendorsSize )
	{
		vendorString = Format( "Unknown (ID=%x)", adapterInfo.VendorId );
	}
	windriverutils::VersionInfo driverVersion( HIWORD(adapterInfo.DriverVersion.HighPart), LOWORD(adapterInfo.DriverVersion.HighPart),
		HIWORD(adapterInfo.DriverVersion.LowPart), LOWORD(adapterInfo.DriverVersion.LowPart) );
	driverVersionString = Format( "%s %i.%i.%i.%i", adapterInfo.Driver,
		HIWORD(adapterInfo.DriverVersion.HighPart), LOWORD(adapterInfo.DriverVersion.HighPart),
		HIWORD(adapterInfo.DriverVersion.LowPart), LOWORD(adapterInfo.DriverVersion.LowPart) );
	driverLibraryString = driverVersionString;
	fixedVersionString = "Direct3D 9.0c [" + driverVersionString + ']';

	rendererID = adapterInfo.DeviceId;
	vendorID = adapterInfo.VendorId;

	// We can't use GetAvailableTextureMem here because the device is not created yet!
	// And besides that, it would return much more than VRAM on Vista (virtualization and so on).
	// Use WMI instead.
	int vramMB;
	const char* vramMethod = "";
	if (g_D3DDevType != D3DDEVTYPE_REF)
		vramMB = windriverutils::GetVideoMemorySizeMB (d3dobject->GetAdapterMonitor(g_D3DAdapter), &vramMethod);
	else
		vramMB = 128;
	videoMemoryMB = vramMB;

	// On windows, we always output D3D info. There is so much variety that it always helps!
	printf_console( "Direct3D:\n" );
	printf_console( "    Version:  %s\n", fixedVersionString.c_str() );
	printf_console( "    Renderer: %s\n", rendererString.c_str() );
	printf_console( "    Vendor:   %s\n", vendorString.c_str() );
	printf_console( "    VRAM:     %i MB (via %s)\n", (int)videoMemoryMB, vramMethod );

	maxVSyncInterval = 0;
	if( d3d.d3dcaps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE )
	{
		maxVSyncInterval = 1;
		if( d3d.d3dcaps.PresentationIntervals & D3DPRESENT_INTERVAL_TWO )
			maxVSyncInterval = 2;
	}

	DWORD declTypesFloat16 = D3DDTCAPS_FLOAT16_2 | D3DDTCAPS_FLOAT16_4;
	has16BitFloatVertex = (d3d.d3dcaps.DeclTypes & declTypesFloat16) == declTypesFloat16;
	needsToSwizzleVertexColors = true;

	bool usesSoftwareVP = !(d3d.d3dcaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT);
	if( usesSoftwareVP )
		maxLights = 8; // software T&L always has 8 lights
	else
		maxLights = clamp<unsigned int>( d3d.d3dcaps.MaxActiveLights, 0, 8 );

	// Texture sizes
	maxTextureSize = std::min( d3d.d3dcaps.MaxTextureWidth, d3d.d3dcaps.MaxTextureHeight );
	maxRenderTextureSize = maxTextureSize;
	maxCubeMapSize = maxTextureSize;

	has3DTexture = d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP;
	maxTexUnits = d3d.d3dcaps.MaxSimultaneousTextures;
	maxTexImageUnits = 16;
	maxTexCoords = d3d.d3dcaps.MaxSimultaneousTextures;
	if (maxTexCoords > 8)
		maxTexCoords = 8;

	// In theory, vertex texturing is texture format dependent. However, in practice the caps lie,
	// especially on NVIDIA hardware.
	//
	// ATI cards: all DX10+ GPUs report all texture formats as vertex texture capable (good!)
	// Intel cards: all SM3.0+ GPUs report all texture formats as vertex texture capable (good!)
	// NV cards: all DX10+ GPUs report only floating point formats as capable, but all others actually work as well.
	//           GeForce 6&7 only report R32F and A32R32G32B32F, and only those work.
	//
	// So we check for R16F support; this will return true on all GPUs that can handle ALL
	// texture formats.
	hasVertexTextures = ((LOWORD(d3d.d3dcaps.VertexShaderVersion) >= (3<<8)+0)) &&
		SUCCEEDED(d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_R16F));

	hasAnisoFilter = d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_ANISOTROPY;
	maxAnisoLevel = hasAnisoFilter ? d3d.d3dcaps.MaxAnisotropy : 1;
	hasMipLevelBias = d3d.d3dcaps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS;

	for( i = 0; i < kTexFormatPCCount; ++i )
	{
		d3d.hasBaseTextureFormat[i] = IsTextureFormatSupported( GetD3D9TextureFormat( static_cast<TextureFormat>(i) ) );
		supportsTextureFormat[i] = d3d.hasBaseTextureFormat[i];
	}

	hasS3TCCompression = IsTextureFormatSupported(D3DFMT_DXT1) && IsTextureFormatSupported(D3DFMT_DXT3) && IsTextureFormatSupported(D3DFMT_DXT5);
	d3d.hasTextureFormatA8 = IsTextureFormatSupported(D3DFMT_A8);
	d3d.hasTextureFormatL8 = IsTextureFormatSupported(D3DFMT_L8);
	d3d.hasTextureFormatA8L8 = IsTextureFormatSupported(D3DFMT_A8L8);
	d3d.hasTextureFormatL16 = IsTextureFormatSupported(D3DFMT_L16);

	if (!(d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_POW2))
		npot = kNPOTFull;
	else if (d3d.d3dcaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL)
		npot = kNPOTRestricted;
	else
		npot = kNPOTNone;

	npotRT = npot;

	hasSRGBReadWrite =
		IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatRGB24)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatRGBA32)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatARGB32)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatBGR24)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatDXT1)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatDXT3)))
		&& IsSRGBTextureReadSupported(GetD3D9TextureFormat(static_cast<TextureFormat>(kTexFormatDXT5)));

	// we only do sRGB writes to an 8 bit buffer ...
	hasSRGBReadWrite = hasSRGBReadWrite && IsSRGBTextureWriteSupported(D3DFMT_A8R8G8B8);

	hasInstancing = false; //@TODO: instancing!

	hasBlendSquare = (d3d.d3dcaps.SrcBlendCaps & D3DPBLENDCAPS_SRCCOLOR) && (d3d.d3dcaps.DestBlendCaps & D3DPBLENDCAPS_DESTCOLOR);
	hasSeparateAlphaBlend = d3d.d3dcaps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND;
	hasBlendSub = hasBlendMinMax = d3d.d3dcaps.PrimitiveMiscCaps & D3DPMISCCAPS_BLENDOP;

	hasAutoMipMapGeneration = d3d.d3dcaps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP;

	for (int i = 0; i < kRTFormatCount; ++i)
	{
		if (i == kRTFormatDefault || i == kRTFormatDefaultHDR || i == kRTFormatShadowMap)
			continue;
		supportsRenderTextureFormat[i] = IsRenderTextureFormatSupported(kD3D9RenderTextureFormats[i]);
	}
	hasRenderToTexture = supportsRenderTextureFormat[kRTFormatARGB32];
	supportsRenderTextureFormat[kRTFormatDefault] = hasRenderToTexture;

	hasRenderToCubemap = hasRenderToTexture;
	hasStencil = true;
	hasRenderTargetStencil = true;
	hasTwoSidedStencil = d3d.d3dcaps.StencilCaps & D3DSTENCILCAPS_TWOSIDED;
	maxMRTs = clamp<int> (d3d.d3dcaps.NumSimultaneousRTs, 1, kMaxSupportedRenderTargets);
	if (!(d3d.d3dcaps.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING))
		maxMRTs = 1;

	d3d.hasATIDepthFormat16 = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, kD3D9FormatDF16 ) );
	supportsRenderTextureFormat[kRTFormatDepth] |= d3d.hasATIDepthFormat16;
	d3d.hasNVDepthFormatINTZ = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, kD3D9FormatINTZ ) );
	supportsRenderTextureFormat[kRTFormatDepth] |= d3d.hasNVDepthFormatINTZ;
	d3d.hasNVDepthFormatRAWZ = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, kD3D9FormatRAWZ ) );
	d3d.hasNULLFormat = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, kD3D9FormatNULL ) );
	d3d.hasDepthResolveRESZ = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, kD3D9FormatRESZ ) );

	hasNativeDepthTexture = d3d.hasATIDepthFormat16 || d3d.hasNVDepthFormatINTZ;
	hasStencilInDepthTexture = d3d.hasNVDepthFormatINTZ;
	hasNativeShadowMap = SUCCEEDED( d3dobject->CheckDeviceFormat( g_D3DAdapter, g_D3DDevType, GetD3DFormatForChecks(), D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D16 ) );
	supportsRenderTextureFormat[kRTFormatShadowMap] = hasRenderToTexture && hasNativeShadowMap;

	#if CAPS_DEBUG_DISABLE_RT
	hasRenderToTexture = hasRenderToCubemap = false;
	for (int i = 0; i < kRTFormatCount; ++i)
		supportsRenderTextureFormat[i] = false;
	maxMRTs = 1;
	#endif

	// This is somewhat dummy; actual resolving of FSAA levels and types supported happens later when choosing presentation parameters.
	hasMultiSample = true;

	// Driver bugs/workarounds following
	DetectDriverBugsD3D9( adapterInfo.VendorId, driverVersion );

	// safeguards
	maxRenderTextureSize = std::min( maxRenderTextureSize, maxTextureSize );
	maxCubeMapSize = std::min( maxCubeMapSize, maxTextureSize );

	// in the very end, figure out shader capabilities level (after all workarounds are applied)
	if( LOWORD(d3d.d3dcaps.PixelShaderVersion) < (3<<8)+0 )
	{
		// no ps3.0: 2.x shaders
		shaderCaps = kShaderLevel2;
	}
	else
	{
		// has everything we care about!
		shaderCaps = kShaderLevel3;
	}

	// Print overall caps & D3D9 hacks used
	printf_console( "    Caps:     Shader=%i DepthRT=%i NativeDepth=%i NativeShadow=%i DF16=%i INTZ=%i RAWZ=%i NULL=%i RESZ=%i SlowINTZ=%i\n",
		shaderCaps,
		supportsRenderTextureFormat[kRTFormatDepth], hasNativeDepthTexture, hasNativeShadowMap,
		d3d.hasATIDepthFormat16,
		d3d.hasNVDepthFormatINTZ, d3d.hasNVDepthFormatRAWZ,
		d3d.hasNULLFormat, d3d.hasDepthResolveRESZ,
		d3d.slowINTZSampling
	);
}


enum WindowsVersion {
	kWindows2000 = 50,	// 5.0
	kWindowsXP = 51,	// 5.1
	kWindows2003 = 52,	// 5.2
	kWindowsVista = 60,	// 6.0
};

static int GetWindowsVersion()
{
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if( !GetVersionEx(&osinfo) )
		return 0;

	if( osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
		return osinfo.dwMajorVersion * 10 + osinfo.dwMinorVersion % 10;
	else
		return 0;
}


void GraphicsCaps::DetectDriverBugsD3D9( UInt32 vendorCode, const windriverutils::VersionInfo& driverVersion )
{
	d3d.slowINTZSampling = false;


	if( vendorCode == kVendorNVIDIA )
	{
		// GeForceFX and earlier have sort-of-buggy render to cubemap. E.g. skybox draws correctly,
		// but objects do not appear. Huh.
		const int kShaderVersion30 = (3 << 8) + 0;
		bool isFXOrEarlier = LOWORD(gGraphicsCaps.d3d.d3dcaps.PixelShaderVersion) < kShaderVersion30;
		if( isFXOrEarlier )
		{
			printf_console( "D3D: disabling render to cubemap on pre-GeForce6\n" );
			buggyCameraRenderToCubemap = true;
		}

		// Also, native shadow maps seem to have problems on GeForce FX; perhaps it needs to use tex2Dproj instead of tex2D,
		// or something (FX 5200). Since FX cards are really dying, and the only left ones are FX 5200/5500,
		// let's just turn shadows off. You don't want them on those cards anyway!
		if (isFXOrEarlier)
		{
			printf_console ("D3D: disabling shadows on pre-GeForce6\n");
			hasNativeShadowMap = false;
			hasNativeDepthTexture = false;
			supportsRenderTextureFormat[kRTFormatDepth] = false;
		}

		// GeForceFX on 6.14.10.9147 drivers has buggy fullscreen FSAA.
		// It displays everything stretched, as if AA samples map to pixels directly.
		if( isFXOrEarlier && driverVersion <= windriverutils::VersionInfo(6,14,10,9147) )
		{
			printf_console( "D3D: disabling fullscreen AA (buggy pre-GeForce6 driver)\n" );
			buggyFullscreenFSAA = true;
		}
	}
	if( vendorCode == kVendorATI )
	{
		// On D3D9 Radeon HD cards have big performance hit when using INTZ texture for both sampling & depth testing
		// (Radeon HD 3xxx-5xxx, Catalyst 9.10 to 10.5). Talking with AMD, we found that using RESZ to copy it into a separate
		// texture is a decent workaround that results in ok performance.
		if (d3d.hasDepthResolveRESZ)
			d3d.slowINTZSampling = true;
	}

	// Sanitize VRAM amount
	if( videoMemoryMB < 32 ) {
		printf_console("D3D: VRAM amount suspiciously low (less than 32MB)\n");
		videoMemoryMB = 32;
	}
}
