#pragma once

#include <string>
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

enum ShaderCapsLevel
{
	kShaderLevel2 = 20,		// shader model 2.0 & 2.x
	kShaderLevel3 = 30,		// shader model 3.0
	kShaderLevel4 = 40,		// shader model 4.0
	kShaderLevel4_1 = 41,	// shader model 4.1
	kShaderLevel5 = 50,		// shader model 5.0
};

enum NPOTCaps
{
	kNPOTNone = 0,		// no NPOT texture capability
	kNPOTRestricted,	// NPOT available with restrictions (no mips, no wrap mode, no compression)
	kNPOTFull,			// NPOT available, no restrictions
};


#if GFX_SUPPORTS_D3D9
#include "PlatformDependent/Win/WinDriverUtils.h"
#include "Runtime/GfxDevice/d3d/D3D9Includes.h"
struct GraphicsCapsD3D9
{
	D3DCAPS9	d3dcaps;
	bool		hasBaseTextureFormat[kTexFormatPCCount];
	bool		hasTextureFormatA8;
	bool		hasTextureFormatL8;
	bool		hasTextureFormatA8L8;
	bool		hasTextureFormatL16;
	bool		hasATIDepthFormat16; // Has ATI DF16 render texture format?
	bool		hasNVDepthFormatINTZ;
	bool		hasNVDepthFormatRAWZ;
	bool		hasNULLFormat;
	bool		hasDepthResolveRESZ;

	// ---- hardware/driver workarounds
	bool		slowINTZSampling;	// very slow at using depth testing and sampling INTZ at the same time; prefer RESZ copy into separate surface
};
#endif

#if GFX_SUPPORTS_D3D11
enum DX11FeatureLevel {
	kDX11Level9_1,
	kDX11Level9_2,
	kDX11Level9_3,
	kDX11Level10_0,
	kDX11Level10_1,
	kDX11Level11_0,
	kDX11LevelCount
};

struct GraphicsCapsD3D11
{
	UInt32			msaa; // regular backbuffer, bit for each sample count we want
	UInt32			msaaSRGB; // sRGB backbuffer, bit for each sample count we want
	DX11FeatureLevel	featureLevel;
	bool			hasShadows10Level9;
	bool			buggyPartialPrecision10Level9; // half precision broken on 10level9 shaders (crashes)
};
#endif

#if GFX_SUPPORTS_OPENGL
struct GraphicsCapsGL
{
	int  glVersion; // e.g. 20 for 2.0, 43 for 4.3
	int  vertexAttribCount;		// How many generic vertex attributes are supported?

	int	 nativeVPInstructions;
	int  nativeFPInstructions;
	int  nativeFPTexInstructions;
	int  nativeFPALUInstructions;
	int  nativeFPTemporaries;

	int  maxSamples;

	bool hasGLSL;				// GLSL

	bool hasTextureEnvCombine3ATI;	// GL_ATI_texture_env_combine3 - the preferred way
	bool hasTextureEnvCombine3NV;	// GL_NV_texture_env_combine4 - fallback to emulate with TNT combiners

	#if UNITY_WIN
	bool hasWglARBPixelFormat;
	bool hasWglSwapControl;
	#endif

	bool hasArbMapBufferRange;
	bool hasArbSync;

	#if UNITY_OSX
	bool hasAppleFence;
	bool hasAppleFlushBufferRange;
	#endif

	bool hasFrameBufferBlit;

	// ---- hardware/driver workarounds
	bool buggyArbPrecisionHint;		// ARB precision hint in FPs is broken, postprocess programs to remove it
	bool cacheFPParamsWithEnvs;		// For fragment programs, use Env parameters instead of Local ones, and cache their values
	bool forceColorBufferWithDepthFBO; // For depth FBO, always use color buffer, with no color buffer it will be wrong
	bool force24DepthForFBO; // 16 bit depth does not work for FBO depth textures
	bool buggyPackedDepthStencil;
	bool mustWriteToDepthBufferBeforeClear; // depth buffer will contain garbage if it isn't written to at least once per frame.
	bool originalHasNativeShadowMaps;
};
#endif



#if GFX_SUPPORTS_OPENGLES20
struct GraphicsCapsGLES20
{
	int	 maxAttributes;
	int  maxVaryings;
	int  maxSamples;
	bool hasGLSL;
	bool hasVBOOrphaning;
	bool slowAlphaTest;
	bool slowDynamicVBO;			// will force dynamic draw from mem (except static batch indices)
	bool forceStaticBatchFromMem;	// will force static batch indices to be used from mem

	// features

	bool hasBinaryShaders;
	bool has24DepthForFBO;
	bool hasMapbuffer;
	bool hasMapbufferRange;
	bool hasDebugMarkers;
	bool hasDiscardFramebuffer;
	bool hasHalfLinearFilter;

	bool hasAppleMSAA;
	bool hasImgMSAA;

	// bugs workaround

	bool buggyColorMaskBlendMSAA;	// on some ios msaa+color mask+blend results in crazy rendering artefacts
	bool buggyDisableVAttrKeepsActive;	// on some mali drivers vattrs are kept active even after disabling them
	bool forceHighpFSPrec;		// on some nvidia drivers there is a bug: they fail to report samplers if default prec is not high
	bool buggyVFetchPatching;	// some adreno drivers fails to patch vfetch instructions (if vertex layout changed)
	bool buggyVprogTextures;	// some mali drivers have crashes in shader compiler if vprog texture is used
	bool needToRenderWatermarkDueToDrawFromMemoryBuggy;	// on some pvr it may crash when rendering from mem (it seems one shader used is culprit)
	bool needFlushAfterTextureUpload;

	// vendor specific

	bool hasNLZ;				// nvidia non-linear z
	bool hasNVMRT;				// nvidia mrt support
	bool hasAlphaTestQCOM;		// adreno special gles11 alpha test
};
#endif

#if GFX_SUPPORTS_OPENGLES30
struct GraphicsCapsGLES30
{
	GraphicsCapsGLES30 (void)
		: maxAttributes				(16)
		, maxVaryings				(16)
		, maxSamples				(4)
		, useProgramBinary			(true)
		, useMapBuffer				(true)
		, useTFSkinning				(true)
		, hasDebugMarkers			(false)
		, hasAlphaTestQCOM			(false)
	{
	}

	// Limits.
	int		maxAttributes;
	int		maxVaryings;
	int		maxSamples;					// Max sample count for renderbuffers.

	// Feature configuration.
	bool	useProgramBinary;			// ProgramBinary support is in core, but in case of we want to ignore it on some platforms.
	bool	useMapBuffer;				// One some platforms mapping buffers is very slow.
	bool	useTFSkinning;				// Use transform feedback for skinning.

	// Extension bits.
	bool	hasDebugMarkers;
	bool	hasAlphaTestQCOM;
};
#endif

#if GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES20
bool FindGLExtension (const char* extensions, const char* ext);
#endif

#if GFX_SUPPORTS_OPENGLES20
bool  QueryExtension (const char* ext);
#endif


struct GraphicsCaps
{
	GraphicsCaps()
	{
		::memset(this, 0x00, sizeof(GraphicsCaps));

		for (int i = 0 ; i < kTexFormatPCCount; ++i)
			supportsTextureFormat[i] = true;

		// Default RT format is always enabled until we don't have render-to-texture at all
		// It is dummy value used to select most appropriate rt format
		supportsRenderTextureFormat[kRTFormatDefault] = true;
		supportsRenderTextureFormat[kRTFormatDefaultHDR] = true;

		new (&rendererString) StaticString();
		new (&vendorString) StaticString();
		new (&driverVersionString) StaticString();
		new (&fixedVersionString) StaticString();
		new (&driverLibraryString) StaticString();

		shaderCaps = kShaderLevel2;
		videoMemoryMB = 16.0f;

		maxLights = 8;
		maxAnisoLevel = 1;
		maxTexImageUnits = 8;
		maxTexCoords = 1;
		maxTexUnits = 1;

		maxTextureSize = 256;
		maxCubeMapSize = 64;
		maxRenderTextureSize = 128;
		maxMRTs = 1;

		hasFixedFunction = true;
		npotRT = npot = kNPOTNone;
		hasNonFullscreenClear = true;
		hasShadowCollectorPass = true;
		hasHighPrecisionTextureCombiners = true;

		hasGrabPass = true;
	}

	void SharedCapsPostInitialize();

	// ---------- caps common for all devices
	StaticString		rendererString;	// graphics card name
	StaticString		vendorString;	// graphics card vendor name
	StaticString        driverVersionString; // (GL) version as reported by the driver
	StaticString		fixedVersionString; // (GL) correct GL version appended in front by us
	StaticString		driverLibraryString;	// Name of driver's DLL and version
	int				vendorID;
	int				rendererID;

	ShaderCapsLevel	shaderCaps;	// Generic "shader capability level"

	float videoMemoryMB;		// Approx. amount of video memory in MB. Used to limit texture, render target and so on sizes to sane values.

	int		maxVSyncInterval;	// Max frames that device can wait for vertical blank
	int		maxLights;			// vertex light count
	int		maxAnisoLevel;
	int		maxTexImageUnits;	// Part of arb-fragment program (otherwise they match textureUnitCount)
	int		maxTexCoords;
	int		maxTexUnits;

	int		maxTextureSize;
	int		maxCubeMapSize;
	int		maxRenderTextureSize;	// usually maxTextureSize, except on some crappy cards

	int		maxMRTs;

	bool	hasAnisoFilter;		// has anisotropic filtering?
	bool	hasMipLevelBias;	// can apply bias for mips?
	bool	hasMipMaxLevel;		// can specify max mip level

	bool	hasFixedFunction;	// Supports T&L and texture combine stages

	bool	hasMultiSample;
	bool	hasMultiSampleAutoResolve;	// uses texture instead, and under-the-hood rb will be auto-resolved to it

	bool	hasBlendSquare;
	bool	hasSeparateAlphaBlend;
	bool 	hasBlendSub;		// both sub and revsub
	bool	hasBlendMinMax;		// max,min
	bool	hasBlendLogicOps;	// kBlendOpLogical*

	bool	hasS3TCCompression;
	bool	hasVertexTextures;	// Can read textures in a vertex shader?

	bool    hasTimerQuery;

	bool	hasAutoMipMapGeneration; // can auto-generate mipmaps for textures?

	bool supportsTextureFormat[kTexFormatTotalCount];
	bool supportsRenderTextureFormat[kRTFormatCount];

	bool has3DTexture;

	NPOTCaps	npot;
	NPOTCaps	npotRT;

	bool hasSRGBReadWrite;
	bool hasComputeShader;
	bool hasInstancing;
	bool hasNonFullscreenClear;		// Can do clears on non-full screen? (e.g. D3D11 can not)

	bool hasRenderToTexture;		// We have render-to-texture functionality
	bool hasRenderToCubemap;		// We have render-to-cubemap functionality
	bool hasRenderTo3D;				// We have render-to-volume functionality
	bool hasStencil;				// Stencil support good enough to be used by the users via the shaderlab state.
	bool hasRenderTargetStencil;	// Has sane way of having stencil buffer on render targets
	bool hasNativeDepthTexture;		// Depth textures come from actual depth buffer
	bool hasStencilInDepthTexture;	// Has native depth texture AND stencil buffer of it can be used at the same time
	bool hasNativeShadowMap;		// Depth textures have native shadow map comparison sampling
	bool hasShadowCollectorPass;	// mobiles don't usage collector pass, go direct SMs!
	bool hasTiledGPU; 				// Uses tiled rendering (clear/discard preferred!)

	bool hasTwoSidedStencil;
	bool hasHighPrecisionTextureCombiners;

	bool has16BitFloatVertex;		 // Supports at least 2D and 4D vertex channels as 16 bit floats
	bool needsToSwizzleVertexColors; // Should vertex colors be passed as B,G,R,A bytes instead of R,G,B,A

	bool disableSubTextureUpload;		 // Can we do UploadTextureSubData2D?
	bool warnRenderTargetUnresolves;	// Warn when doing RT un-resolves

	bool hasGrabPass;				// Can read from active render target back into a render texture?

	// cross platform caps initialized in SharedCapsPostInitialize
	bool hasPrePassRenderLoop;

	bool SupportsRGBM() const { return maxTexUnits >= 3 && hasHighPrecisionTextureCombiners; }

	std::string CheckGPUSupported() const;

	// ---- hardware/driver workarounds for all renderers

	bool disableSoftShadows;		// Soft shadows should work, but the driver is buggy on those shaders
	bool buggyCameraRenderToCubemap;// Render to cubemap sort-of-does-not-work (but works for point light shadows).
	bool buggyFullscreenFSAA;		// FSAA in fullscreen is buggy.
	bool buggyTextureBothColorAndDepth; // Can't have render target where both color & depth will be sampled as textures
	bool buggyDynamicVBOWithTangents;	// DynamicVBO with tangents is buggy
	bool buggyMipmappedCubemaps;		// Cubemaps with mipmaps are buggy; keep only 1st mip level
	bool buggyMipmapped3DTextures;		// 3D textures with mipmaps are buggy; keep only 1st mip level
	bool buggyShadowMapBilinearSampling; // Never use hardware bilinear PCF on shadow maps due to bugs?
	bool buggySpotNativeShadowMap; // Never use hardware shadow maps on spot lights
	bool buggyTimerQuery; // Has timer queries, but they aren't very good!
	
	// ---- caps specific for renderers
	
#if GFX_SUPPORTS_OPENGL
	GraphicsCapsGL gl;
#endif
#if GFX_SUPPORTS_OPENGLES20
	GraphicsCapsGLES20 gles20;
#endif
#if GFX_SUPPORTS_D3D9
	GraphicsCapsD3D9 d3d;
#endif
#if GFX_SUPPORTS_D3D11
	GraphicsCapsD3D11 d3d11;
#endif
#if GFX_SUPPORTS_OPENGLES30
	GraphicsCapsGLES30 gles30;
#endif
	
	#if GFX_SUPPORTS_OPENGL
	void InitGL();
	#endif
	#if GFX_SUPPORTS_D3D9
	void InitD3D9();
	#endif
	#if GFX_SUPPORTS_D3D11
	void InitD3D11();
	#endif
	#if GFX_SUPPORTS_NULL
	void InitNull();
	#endif
	#if GFX_SUPPORTS_MOLEHILL
	void InitMolehill();
	#endif
	#if GFX_SUPPORTS_XENON
	void InitXenon();
	#endif
	#if GFX_SUPPORTS_GCM
	void InitGcm();
	UInt32 videoMemoryStart;
	#endif
	#if GFX_SUPPORTS_HOLLYWOOD
	void InitHollywood();
	#endif
	#if GFX_SUPPORTS_OPENGLES20
	void InitGLES20();
	#endif
	#if GFX_SUPPORTS_OPENGLES30
	void InitGLES30();
	#endif

	void InitWithBuildVersion();


	// Implemented in GraphicsCaps.cpp
	#if UNITY_EDITOR
	enum Emulation {
		kEmulNone,		// whatever card naturally has
		kEmulSM3,		// Shader Model 3.0
		kEmulSM2,		// Shader Model 2.0, but with 4 texture stages
		kEmulGLES20,	// OpenGL ES 2.0
		kEmulXBox360,
		kEmulPS3,
		kEmulDX11_9_1,	// DirectX 11 Feature Level 9.1, No Fixed Function shaders (Shader Model 2)
		kEmulDX11_9_3, // DirectX 11 Feature Level 9.1, Fixed Function shaders (Shader Model 3)
		kEmulCount
	};
	Emulation GetEmulation() const;
	void SetEmulation( Emulation emul );
	bool IsEmulatingGLES20() const;
	bool CheckEmulationSupported( Emulation emul );
	void InitializeOriginalEmulationCapsIfNeeded();
	void ResetOriginalEmulationCaps();

	static void ApplyEmulationSettingsAffectingGUI();

	private:
	void ApplyEmulationSetingsOnly( const GraphicsCaps& origCaps, const Emulation emul );
	#endif


private:

	#if GFX_SUPPORTS_OPENGL
	void AdjustBuggyVersionGL( int& version, int& major, int& minor ) const;
	void DetectDriverBugsGL( int version );
	#endif
	#if GFX_SUPPORTS_D3D9
	void DetectDriverBugsD3D9( UInt32 vendorCode, const windriverutils::VersionInfo& version );
	#endif
};

extern GraphicsCaps gGraphicsCaps;
