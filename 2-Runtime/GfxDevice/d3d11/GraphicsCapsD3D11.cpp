#include "UnityPrefix.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "D3D11Context.h"


extern DXGI_FORMAT kD3D11RenderTextureFormats[kRTFormatCount];



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
	kVendorQualcomm	= 0x4d4f4351,
};

enum {
	kRendererIntel3150	= 0xa011, //Intel(R) Graphics Media Accelerator 3150 (Microsoft Corporation - WDDM 1.0)
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
	{ kVendorQualcomm,	"Qualcomm" },
};
static int kKnownVendorsSize = sizeof(s_KnownVendors)/sizeof(s_KnownVendors[0]);

DX11FeatureLevel kD3D11RenderTextureFeatureLevels[kRTFormatCount] = {
	kDX11Level9_1, // ARGB32: 9.1
	kDX11Level10_0, // Depth: 10.0
	kDX11Level9_3, // ARGBHalf: 9.3
	kDX11Level10_0, // Shadowmap: 10.0
	kDX11LevelCount, // RGB565, unsupported
	kDX11LevelCount, // ARGB4444, unsupported
	kDX11LevelCount, // ARGB1555, unsupported
	kDX11LevelCount, // Default
	kDX11Level10_0, // A2RGB10: 10.0
	kDX11LevelCount, // DefaultHDR
	kDX11Level10_1, // ARGB64: 10.1
	kDX11Level10_0, // ARGBFloat: 10.0
	kDX11Level10_0, // RGFloat: 10.0
	kDX11Level10_0, // RGHalf: 10.0
	kDX11Level10_0, // RFloat: 10.0
	kDX11Level10_0, // RHalf: 10.0
	kDX11Level10_0, // R8: 10.0
	kDX11Level10_0,	// ARGBInt: 10.0
	kDX11Level10_0,	// RGInt: 10.0
	kDX11Level10_0,	// RInt: 10.0
};



void GraphicsCaps::InitD3D11()
{
	ID3D11Device* d3d = GetD3D11Device();
	HRESULT hr;

	// get device information
#if UNITY_WINRT
	DXGI_ADAPTER_DESC2 adapterDesc;

	IDXGIDevice2* dxgiDevice2;
	hr = d3d->QueryInterface(__uuidof(IDXGIDevice2), (void**)&dxgiDevice2);
	IDXGIAdapter* dxgiAdapter;
	IDXGIAdapter2* dxgiAdapter2;
	dxgiDevice2->GetAdapter (&dxgiAdapter);
	dxgiAdapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&dxgiAdapter2);
	dxgiAdapter2->GetDesc2 (&adapterDesc);
	dxgiAdapter2->Release();
	dxgiAdapter->Release();
	dxgiDevice2->Release();
#else
	DXGI_ADAPTER_DESC adapterDesc;

	IDXGIDevice* dxgiDevice;
	IDXGIAdapter* dxgiAdapter;
	hr = d3d->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
	dxgiDevice->GetAdapter (&dxgiAdapter);
	dxgiAdapter->GetDesc (&adapterDesc); 
	dxgiAdapter->Release();
	dxgiDevice->Release();
#endif
	adapterDesc.Description[127] = 0;

	char bufUtf8[1024];
	WideCharToMultiByte (CP_UTF8, 0, adapterDesc.Description, -1, bufUtf8, 1024, NULL, NULL );
	rendererString = bufUtf8;

	int i;
	for( i = 0; i < kKnownVendorsSize; ++i )
	{
		if( s_KnownVendors[i].vendorId == adapterDesc.VendorId )
		{
			vendorString = s_KnownVendors[i].name;
			break;
		}
	}
	if( i == kKnownVendorsSize )
	{
		vendorString = Format( "Unknown (ID=%x)", adapterDesc.VendorId );
	}

	vendorID = adapterDesc.VendorId;
	rendererID = adapterDesc.DeviceId;

	// No easy way to get driver information in DXGI...
	driverLibraryString.clear();
	driverVersionString.clear();

	D3D_FEATURE_LEVEL d3dlevel = d3d->GetFeatureLevel();
	DX11FeatureLevel level = kDX11Level9_1;
	switch (d3dlevel) {
	case D3D_FEATURE_LEVEL_9_1: level = kDX11Level9_1; break;
	case D3D_FEATURE_LEVEL_9_2: level = kDX11Level9_2; break;
	case D3D_FEATURE_LEVEL_9_3: level = kDX11Level9_3; break;
	case D3D_FEATURE_LEVEL_10_0: level = kDX11Level10_0; break;
	case D3D_FEATURE_LEVEL_10_1: level = kDX11Level10_1; break;
	case D3D_FEATURE_LEVEL_11_0: level = kDX11Level11_0; break;
	default: AssertString ("Unknown feature level");
	}
	d3d11.featureLevel = level;

	fixedVersionString = Format("Direct3D 11.0 [level %i.%i]",
		(d3dlevel & 0xF000) >> 12,
		(d3dlevel & 0x0F00) >> 8);

#if UNITY_WINRT
	// Note: On Intel HD 4000, adapterDesc.DedicatedVideoMemory was returning 32 MB, which wasn't enough to even have on render texture on Surface Pro
	// That's why there were bugs with shadows
	// If you change something here, do check https://fogbugz.unity3d.com/default.asp?546560#1066534481 on Surface Pro, if it's still works
	#if defined(__arm__)
		const float kSaneMinVRAM = 64;
	#else
		const float kSaneMinVRAM = 128;
	#endif
	videoMemoryMB = (adapterDesc.DedicatedVideoMemory == 0) ? (adapterDesc.SharedSystemMemory / 2) : adapterDesc.DedicatedVideoMemory; 
	videoMemoryMB = std::max(videoMemoryMB / (1024 * 1024), kSaneMinVRAM);
#if UNITY_METRO && defined(__arm__)
	// tested on Tegra Tablet (Tegra T300?) and Surface (Tegra 3) - tested on 2013.02.20
	gGraphicsCaps.buggyShadowMapBilinearSampling = true;
#else 
	gGraphicsCaps.buggyShadowMapBilinearSampling = false;
#endif
#else
	videoMemoryMB = adapterDesc.DedicatedVideoMemory / 1024 / 1024;
#endif

	// Output D3D info to console
	printf_console( "Direct3D:\n" );
	printf_console( "    Version:  %s\n", fixedVersionString.c_str() );
	printf_console( "    Renderer: %s (ID=0x%x)\n", rendererString.c_str(), rendererID);
	printf_console( "    Vendor:   %s\n", vendorString.c_str() );
	printf_console( "    VRAM:     %i MB\n", (int)videoMemoryMB );

	needsToSwizzleVertexColors = false;
	maxVSyncInterval = 4;
	maxLights = 8;

	// Texture sizes
	static const int kTextureSizes[kDX11LevelCount] = {2048, 2048, 4096, 8192, 8192, 16384};
	static const int kCubemapSizes[kDX11LevelCount] = { 512,  512, 4096, 8192, 8192, 16384};
	maxTextureSize = kTextureSizes[level];
	maxRenderTextureSize = maxTextureSize;
	maxCubeMapSize = kCubemapSizes[level];

	// Vertex/Fragment program parts
	hasFixedFunction = true;
#if UNITY_METRO
	// Disable fixed function shaders on crappy devices
	// One fix would be to rewrite byte code emitter to target 9.1 feature level instead of 9.3...
	// For now, let's just disable it on all Intel devices with feature level < 9.3
	if (vendorID == kVendorIntel && level < kDX11Level9_3) // Internal driver error occurs when first fixed function shader is loaded
	{
		printf_console("WARNING: Disabling fixed function shaders.\n");
		hasFixedFunction = false;
	}
	else
#endif
	has3DTexture = true;
	maxTexUnits = kMaxSupportedTextureUnits;
	maxTexImageUnits = kMaxSupportedTextureUnits;
	maxTexCoords = 8;

	hasAnisoFilter = true;
	static const int kMaxAniso[kDX11LevelCount] = {2, 16, 16, 16, 16, 16};
	maxAnisoLevel = kMaxAniso[level];
	hasMipLevelBias = true;

	hasS3TCCompression = true;
	npotRT = npot = (level >= kDX11Level10_0) ? kNPOTFull : kNPOTRestricted;

	hasBlendSquare = true;
	hasSeparateAlphaBlend = level > kDX11Level9_1;
#if UNITY_WP8
	hasSeparateAlphaBlend = level > kDX11Level9_3; // WP8 uses 9.3 feature level, but seems to not support separate alpha blending
#endif
	hasBlendSub = true;
	hasBlendMinMax = true;
	
	hasBlendLogicOps = false;
	if(GetD3D11_1Device())
	{
		D3D11_FEATURE_DATA_D3D11_OPTIONS opt;

		HRESULT hr = GetD3D11_1Device()->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, (void *)&opt, sizeof(opt));
		if(!FAILED(hr))
		{
			hasBlendLogicOps = opt.OutputMergerLogicOp;
		}

	}

	hasAutoMipMapGeneration = true;

	#pragma message ("Properly implement supported formats!")
	for (int q = 0; q < kTexFormatPCCount; ++q)
		supportsTextureFormat[q] = true; //@TODO

	if (level < kDX11Level9_3)
	{
		supportsTextureFormat[kTexFormatARGBFloat] = false;
		supportsTextureFormat[kTexFormatRGB565] = false;
	}
	// 9.1 level doesn't really support R16 format. But we pretend we do, and then do unpacking
	// into R8G8B8A8 on load.
	supportsTextureFormat[kTexFormatAlphaLum16] = true;

	hasRenderToTexture = true;
	for (int i = 0; i < kRTFormatCount; ++i)
	{
		if (i == kRTFormatDefault || i == kRTFormatDefaultHDR)
			continue;
		supportsRenderTextureFormat[i] = (level >= kD3D11RenderTextureFeatureLevels[i]);
	}

#if UNITY_METRO	// works on adreno 305 devices (nokia lumia 620) but not adreno 225 (nokia lumia 920 and samsung ativ s)
	if (level < D3D_FEATURE_LEVEL_10_0)
	{
		D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT shadowSupport;
		memset(&shadowSupport, 0, sizeof(shadowSupport));

		// Devices which don't support this:
		// * Intel 3150
		//   D3D11 ERROR: ID3D11Device::CreatePixelShader: Shader uses comparision filtering with shader target ps_4_0_level_9_*, but the device does not support this . To check for support, call the CheckFeatureSupport() API with D3D11_FEATURE_D3D9_SHADOW_SUPPORT.  The SupportsDepthAsTextureWithLessEqualComparisonFilter member indicates support for comparison filtering on level_9 shaders. [ STATE_CREATION ERROR #192: CREATEPIXELSHADER_INVALIDSHADERBYTECODE]
		//	 First-chance exception at 0x74DD277C in New Unity Project 10.exe: Microsoft C++ exception: _com_error at memory location 0x0996ED90.
		//  Would be nice, if you could ignore such shaders if this feature is not supported.

		d3d->CheckFeatureSupport(D3D11_FEATURE_D3D9_SHADOW_SUPPORT, &shadowSupport, sizeof(shadowSupport));
		gGraphicsCaps.supportsRenderTextureFormat[kRTFormatDepth] = shadowSupport.SupportsDepthAsTextureWithLessEqualComparisonFilter;
		gGraphicsCaps.supportsRenderTextureFormat[kRTFormatShadowMap] = shadowSupport.SupportsDepthAsTextureWithLessEqualComparisonFilter;
		gGraphicsCaps.d3d11.hasShadows10Level9 = (shadowSupport.SupportsDepthAsTextureWithLessEqualComparisonFilter != 0);
	}
#else 
	gGraphicsCaps.d3d11.hasShadows10Level9 = false;
#endif

	// Looks like 9.x levels can't easily render into cubemaps:
	// * DepthStencil texture must be a cubemap as well (otherwise can't set a cubemap color & regular 2D depth)
	// * But a cubemap doesn't support any of the depth buffer formats
	// A workaround could be to create 2D resources for each cubemap face, render into them, and then blit into a face
	// of the final cubemap. Bah.
	hasRenderToCubemap = (level >= kDX11Level10_0);

	hasRenderTo3D = (level >= kDX11Level11_0);
	hasStencil = true;
	hasRenderTargetStencil = true;
	hasTwoSidedStencil = true;
	hasNativeDepthTexture = true;
	hasStencilInDepthTexture = true;
	hasNativeShadowMap = true;

	hasSRGBReadWrite = true;

	hasComputeShader = (level >= kDX11Level11_0); //@TODO: some sort of limited CS support in SM4?
	hasInstancing = (level >= kDX11Level9_3);
	hasNonFullscreenClear = false;

	hasMultiSample = true;
	d3d11.msaa = d3d11.msaaSRGB = 0;
	static const int kAASampleTests[] = { 2, 4, 8 };
	for (int i = 0; i < ARRAY_SIZE(kAASampleTests); ++i)
	{
		int samples = kAASampleTests[i];
		UInt32 mask = 1<<samples;
		UINT levels = 0;
		hr = d3d->CheckMultisampleQualityLevels (DXGI_FORMAT_R8G8B8A8_UNORM, samples, &levels);
		if (SUCCEEDED(hr) && levels > 0)
			d3d11.msaa |= mask;
		d3d->CheckMultisampleQualityLevels (DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, samples, &levels);
		if (SUCCEEDED(hr) && levels > 0)
			d3d11.msaaSRGB |= mask;
	}


	// Crashes on ARM tablets
#if defined(__arm__) && UNITY_WINRT
	hasTimerQuery = false;
#else
	// In theory just doing a query create with NULL destination, like
	// CreateQuery(&disjointQueryDesc, NULL) == S_FALSE
	// should be enough. But this makes NVIDIA NSight 2.1 crash, so do full query creation.
	const D3D11_QUERY_DESC disjointQueryDesc = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
	const D3D11_QUERY_DESC timestampQueryDesc = { D3D11_QUERY_TIMESTAMP, 0 };
	ID3D11Query *query1 = NULL, *query2 = NULL;
	hasTimerQuery = true;
	hasTimerQuery &= SUCCEEDED(d3d->CreateQuery(&disjointQueryDesc, &query1));
	hasTimerQuery &= SUCCEEDED(d3d->CreateQuery(&timestampQueryDesc, &query2));
	SAFE_RELEASE(query1);
	SAFE_RELEASE(query2);
#endif

	static const int kMRTCount[kDX11LevelCount] = { 1, 1, 4, 8, 8, 8 };
	maxMRTs = std::min<int> (kMRTCount[level], kMaxSupportedRenderTargets);

	// in the very end, figure out shader capabilities level (after all workarounds are applied)
	static const ShaderCapsLevel kShaderLevels[kDX11LevelCount] = {kShaderLevel2, kShaderLevel2, kShaderLevel2, kShaderLevel4, kShaderLevel4_1, kShaderLevel5};
	shaderCaps = kShaderLevels[level];

	// Looks like mipmapped cubemaps & 3D textures are broken on 9.x level;
	// can't update proper faces via CopySubresourceRegion / UpdateSubresource.
	buggyMipmappedCubemaps = buggyMipmapped3DTextures = (level < kDX11Level10_0);

	d3d11.buggyPartialPrecision10Level9 = false;
	if (level < kDX11Level10_0)
	{
		// Lumia 620 crashes on partial precision (on WP8 as of 2013 March), with renderer
		// string "Qualcomm Adreno 305 (WDDM v1.2)". Let's try removing partial precision
		// on all Adreno 3xx.
		if (rendererString.find("Adreno 3") != std::string::npos)
			d3d11.buggyPartialPrecision10Level9 = true;
	}
}
