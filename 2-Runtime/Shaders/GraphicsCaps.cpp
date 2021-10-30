#include "UnityPrefix.h"
#include "GraphicsCaps.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/SystemInfo.h"

GraphicsCaps gGraphicsCaps;

#if UNITY_EDITOR

#include "Runtime/Graphics/GraphicsHelper.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Camera/Light.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Modules/ExportModules.h"

static bool  gDidInitializeOriginalCaps = false;
GraphicsCaps EXPORT_COREMODULE gOriginalCaps;

void InvalidateGraphicsStateInEditorWindows(); // EditorWindow


#if GFX_SUPPORTS_D3D9
void InitializeCombinerCapsD3D9(); // CombinerD3D.cpp
#endif

inline bool RarelySupportedFormat(RenderTextureFormat format)
{
	return (format == kRTFormatARGB4444) || (format == kRTFormatARGB1555);
}

bool CheckEmulationValid( const GraphicsCaps& origCaps, GraphicsCaps& emulatedCaps )
{
	if (origCaps.shaderCaps < emulatedCaps.shaderCaps)
		return false;
	if (!origCaps.hasFixedFunction && emulatedCaps.hasFixedFunction)
		return false;
	for (int i = 0; i < kRTFormatCount; ++i)
	{
		if (!RarelySupportedFormat((RenderTextureFormat)i))
			if (!origCaps.supportsRenderTextureFormat[i] && emulatedCaps.supportsRenderTextureFormat[i])
				return false;
	}
	if (origCaps.maxTexUnits < emulatedCaps.maxTexUnits)
		return false;
	if (!origCaps.has3DTexture && emulatedCaps.has3DTexture)
		return false;
	if (origCaps.npot < emulatedCaps.npot)
		return false;
	if (!origCaps.hasAutoMipMapGeneration && emulatedCaps.hasAutoMipMapGeneration)
		return false;
	if (!origCaps.hasRenderTargetStencil && emulatedCaps.hasRenderTargetStencil)
		return false;

	#if GFX_SUPPORTS_OPENGL
	if (GetGfxDevice().GetRenderer() == kGfxRendererOpenGL)
	{
		if (!origCaps.gl.hasGLSL && emulatedCaps.gl.hasGLSL)
			return false;
		if (!origCaps.gl.hasTextureEnvCombine3ATI && emulatedCaps.gl.hasTextureEnvCombine3ATI)
			return false;
	}
	#endif

	return true;
}

void GraphicsCaps::ApplyEmulationSetingsOnly( const GraphicsCaps& origCaps, const Emulation emul )
{
	for (int q = 0; q < kTexFormatPCCount; ++q)
		supportsTextureFormat[q] = true;

	bool glesTargetAndroid = GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget()) == kPlatformAndroid;

	switch( emul )
	{
	case kEmulNone:
		*this = origCaps;
		break;
	case kEmulSM3:
		// do not set texture units; just let all SM3.0 hardware use whatever they support
		shaderCaps = kShaderLevel3;
		supportsRenderTextureFormat[kRTFormatDepth]		= true;
		supportsRenderTextureFormat[kRTFormatARGBHalf]	= true;
		hasComputeShader = false;
		// GL specific
		#if GFX_SUPPORTS_OPENGL
		gl.hasGLSL = true;
		#endif
		// names
		rendererString = "Emulated Shader Model 3.0";
		vendorString = "Emulated";
		#if UNITY_WIN
		fixedVersionString = "Direct3D 9.0c [emulated]";
		#else
		fixedVersionString = "OpenGL 2.0 [emulated]";
		#endif
		break;
	case kEmulSM2:
		shaderCaps = kShaderLevel2;
		supportsRenderTextureFormat[kRTFormatDepth]		= true;
		supportsRenderTextureFormat[kRTFormatARGBHalf]	= false;
		maxTexUnits = 4;
		maxTexCoords = 8;
		hasComputeShader = false;
		hasInstancing = false;
		// GL specific
		#if GFX_SUPPORTS_OPENGL
		gl.hasGLSL = true;
		#endif
		// names
		rendererString = "Emulated Shader Model 2.0";
		vendorString = "Emulated";
		#if UNITY_WIN
		fixedVersionString = "Direct3D 9.0c [emulated]";
		#else
		fixedVersionString = "OpenGL 2.0 [emulated]";
		#endif
		break;
	case kEmulGLES20:
		shaderCaps = kShaderLevel3;
		//hasSRGBReadWrite = glesTargetAndroid; // while only supported on handful devices, lets allow people to use it in emu
		hasSRGBReadWrite = false;
		hasComputeShader = false;
		hasInstancing = false;
		supportsTextureFormat[kTexFormatAlphaLum16]		= false;
		supportsTextureFormat[kTexFormatDXT1]			= glesTargetAndroid;
		supportsTextureFormat[kTexFormatDXT3]			= glesTargetAndroid;
		supportsTextureFormat[kTexFormatDXT5]			= glesTargetAndroid;
		supportsTextureFormat[kTexFormatPVRTC_RGB2]		= true;
		supportsTextureFormat[kTexFormatPVRTC_RGBA2]	= true;
		supportsTextureFormat[kTexFormatPVRTC_RGB4]		= true;
		supportsTextureFormat[kTexFormatPVRTC_RGBA4]	= true;
		supportsTextureFormat[kTexFormatETC_RGB4]		= true;
		supportsTextureFormat[kTexFormatATC_RGB4]		= glesTargetAndroid;
		supportsTextureFormat[kTexFormatATC_RGBA8]		= glesTargetAndroid;
		supportsRenderTextureFormat[kRTFormatARGB32]	= true;
		supportsRenderTextureFormat[kRTFormatARGBHalf]	= true;	// supported on high-end mobiles ;-)
		hasRenderToTexture = true;
		hasTiledGPU = true; // assume tiled GPU when emulating ES2
		warnRenderTargetUnresolves = true;

		// don't touch 16bit rt formats, as they depend on underlying hw

		hasNativeShadowMap = false;
		supportsRenderTextureFormat[kRTFormatDepth]		 = true;
		supportsRenderTextureFormat[kRTFormatShadowMap]	 = false; // will use depth texture for shadows

		// There is no fixed function texture units in ES2
		maxTexUnits = maxTexImageUnits = maxTexCoords = 8;
		maxMRTs = 1;
		has3DTexture = false;

		// gles2.0 has hard shadows.
		disableSoftShadows = true;

		hasRenderToCubemap = true;
		npotRT = npot = kNPOTFull;
		maxTextureSize = 4096;
		maxCubeMapSize = 512;
		// GL specific
		#if GFX_SUPPORTS_OPENGL
		gl.hasGLSL = true;
		gl.hasTextureEnvCombine3ATI = false;
		gl.hasTextureEnvCombine3NV = false;
		#endif
		// D3D9 specific
		#if GFX_SUPPORTS_D3D9
		d3d.hasTextureFormatL16 = false;
		d3d.hasTextureFormatA8L8 = false;
		d3d.d3dcaps.MaxSimultaneousTextures = maxTexUnits;
		d3d.d3dcaps.RasterCaps &= ~D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS;
		#endif
		rendererString = "Emulated GPU running OpenGL ES 2.0";
		vendorString = "Emulated";
		fixedVersionString = "OpenGL ES 2.0 [emulated]";
		break;

	case kEmulXBox360:
	case kEmulPS3:
		shaderCaps = kShaderLevel3;
		hasFixedFunction = false;

		maxLights = 8;
		maxMRTs = 1; //@TODO: possibly needs change for MRTs on console emulation
		hasAnisoFilter = true;
		maxAnisoLevel = 16;
		hasMipLevelBias = true;

		hasMultiSample = (emul == kEmulPS3);

		hasComputeShader = false;
		hasInstancing = false;

		hasBlendSquare = true;
		hasSeparateAlphaBlend = true;

		hasS3TCCompression = true;
		hasVertexTextures = true;

		hasAutoMipMapGeneration = false;

		maxTexImageUnits = 16;
		maxTexCoords = 8;

		maxTexUnits = 8;

		maxTextureSize = 4096;
		maxCubeMapSize = 512;

		supportsRenderTextureFormat[kRTFormatARGB32] = true;
		supportsRenderTextureFormat[kRTFormatDepth] = true;
		supportsRenderTextureFormat[kRTFormatARGBHalf] = true;
		supportsRenderTextureFormat[kRTFormatShadowMap] = (emul == kEmulPS3);

		has3DTexture = true;
		npotRT = npot = kNPOTFull;

		hasRenderToTexture = true;
		hasRenderToCubemap = true;
		hasTwoSidedStencil = true;
		hasSRGBReadWrite = true;
		hasNativeDepthTexture = true;
		hasStencilInDepthTexture = true;
		hasNativeShadowMap = (emul == kEmulPS3);

		rendererString = "Emulated Shader Model 3.0 (XBOX360/PS3)";
		vendorString = "Emulated";
		fixedVersionString = "XBOX360/PS3 (emulated)";
		break;
	case kEmulDX11_9_1:
	case kEmulDX11_9_3:
		shaderCaps = emul == kEmulDX11_9_3  ? kShaderLevel3 : kShaderLevel2; 
		hasFixedFunction =  emul == kEmulDX11_9_3;

		maxLights = 8;
		maxMRTs = 1;
		hasAnisoFilter = true;
		maxAnisoLevel = emul == kEmulDX11_9_3 ? 16 : 2;
		hasMipLevelBias = true;

		hasMultiSample = true;

		hasComputeShader = false;
		hasInstancing = false;

		hasBlendSquare = true;
		hasSeparateAlphaBlend = false;

		hasS3TCCompression = true;
		hasVertexTextures = true;

		maxTexImageUnits = 16;
		maxTexCoords = 8;

		maxTexUnits = 8;

		maxTextureSize = emul == kEmulDX11_9_3 ? 4096 : 2048;
		maxCubeMapSize = emul == kEmulDX11_9_3 ? 4096 : 512;

		supportsRenderTextureFormat[kRTFormatARGB32] = true;
		supportsRenderTextureFormat[kRTFormatDepth] = true;
		supportsRenderTextureFormat[kRTFormatShadowMap] = true;

		has3DTexture = true;
		npotRT = npot = kNPOTRestricted;

		hasTiledGPU = true; // assume tiled GPU when emulating DX11 9.x
		warnRenderTargetUnresolves = true;

		hasRenderToTexture = true;
		hasRenderToCubemap = false;
		hasTwoSidedStencil = true;
		hasSRGBReadWrite = true;
		hasNativeDepthTexture = true;
		hasStencilInDepthTexture = true;
		hasNativeShadowMap = true;
#if GFX_SUPPORTS_D3D11
		d3d11.hasShadows10Level9 = true;
#endif

		rendererString = "Emulated DirectX 11 9.1 No Fixed Function Shaders";
		vendorString = "Emulated";
		fixedVersionString = "DirectX 11 9.1 (emulated)";
		break;
	}
}

void GraphicsCaps::InitializeOriginalEmulationCapsIfNeeded()
{
	if (!gDidInitializeOriginalCaps)
	{
		gDidInitializeOriginalCaps = true;
		SET_ALLOC_OWNER(NULL);
		gOriginalCaps = *this;
	}
}

void GraphicsCaps::ResetOriginalEmulationCaps()
{
	gDidInitializeOriginalCaps = false;
}

bool GraphicsCaps::CheckEmulationSupported( Emulation emul )
{
	InitializeOriginalEmulationCapsIfNeeded();

	GraphicsCaps emulatedCaps = gOriginalCaps;
	emulatedCaps.ApplyEmulationSetingsOnly(gOriginalCaps, emul);

	return CheckEmulationValid(gOriginalCaps, emulatedCaps);
}

bool EmulationWants16bitDepth( GraphicsCaps::Emulation emul )
{
	if(    (emul == GraphicsCaps::kEmulGLES20)
	    && GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget()) == kPlatformAndroid
	    && !GetPlayerSettings().GetUse24BitDepthBuffer()
	  )
	{
		return true;
	}

	return false;
}

GraphicsCaps::Emulation _CurrentEmulation = GraphicsCaps::kEmulNone;

void GraphicsCaps::ApplyEmulationSettingsAffectingGUI()
{
	// well, not the best solution...

	if( EmulationWants16bitDepth(_CurrentEmulation) )
		GUIView::RecreateAllOnDepthBitsChange(32, 16);
	else
		GUIView::RecreateAllOnDepthBitsChange(16, 32);
}

GraphicsCaps::Emulation GraphicsCaps::GetEmulation() const
{
	return _CurrentEmulation;
}

void GraphicsCaps::SetEmulation( Emulation emul )
{
	if( emul == _CurrentEmulation )
		return;

	if (!CheckEmulationSupported(emul))
	{
		ErrorString("Attempting to switch to unsupported emulation");
		return;
	}

	// must be released before the emulation is set, since the memory usage is estimated from the deviceCaps
	RenderTexture::ReleaseAll();

	_CurrentEmulation = emul;

	InitializeOriginalEmulationCapsIfNeeded();

	*this = gOriginalCaps;

	GfxDevice& device = GetGfxDevice();
	device.InvalidateState();
	ShaderLab::SubProgram* nullShaders[kShaderTypeCount] = {0};
	GraphicsHelper::SetShaders (device, nullShaders, 0);
	InvalidateGraphicsStateInEditorWindows();

	ApplyEmulationSettingsAffectingGUI();
	ApplyEmulationSetingsOnly(gOriginalCaps, emul);
	SharedCapsPostInitialize();

	device.CommonReloadResources(GfxDevice::kReleaseRenderTextures | GfxDevice::kReloadShaders | GfxDevice::kReloadTextures);

	#if GFX_SUPPORTS_D3D9
	InitializeCombinerCapsD3D9();
	#endif

	// Give a chance for image effects (which mostly execute in edit mode
	// as well) to recheck hardware support for them.
	MonoBehaviour::RestartExecuteInEditModeScripts();

	// update all lights
	std::vector<Light*> lights;
	Object::FindObjectsOfType (&lights);
	for( std::vector<Light*>::iterator i = lights.begin(); i != lights.end(); ++i )
	{
		Light& light = (**i);
		light.Precalc();
	}
}

bool GraphicsCaps::IsEmulatingGLES20() const
{
	return _CurrentEmulation == kEmulGLES20;
}

#endif // UNITY_EDITOR


std::string GraphicsCaps::CheckGPUSupported() const
{
#if !ENABLE_GFXDEVICE_REMOTE_PROCESS_CLIENT
	GfxDeviceRenderer type = GetGfxDevice().GetRenderer();

	// OpenGL
#	if GFX_SUPPORTS_OPENGL
	if (type == kGfxRendererOpenGL)
	{
		extern bool QueryExtensionGL (const char* ext);

		// OS X 10.5 on GMA 950 has GL1.2. When we drop 10.5 support we can move
		// up to GL1.4.
		if (gl.glVersion < 12)
		{
			return Format("OpenGL 1.2 is required. Your GPU (%s) only supports OpenGL %i.%i", rendererString.c_str(), gl.glVersion/10, gl.glVersion%10);
		}
		// Require GLSL
		if (!gl.hasGLSL)
		{
			return Format("OpenGL GLSL support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}
		// Require ARB VP/FP
		if (!QueryExtensionGL("GL_ARB_vertex_program") || !QueryExtensionGL("GL_ARB_fragment_program"))
		{
			return Format("OpenGL vertex/fragment program support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}

		// Require VBO, multitexture, ...
		if (gl.glVersion < 15 && !QueryExtensionGL("GL_ARB_vertex_buffer_object"))
		{
			return Format("OpenGL vertex buffer support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}
		if (gl.glVersion < 13 && !QueryExtensionGL ("GL_ARB_multitexture"))
		{
			return Format("OpenGL multitexture support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}
		if (gl.glVersion < 13 && !QueryExtensionGL ("GL_ARB_texture_cube_map"))
		{
			return Format("OpenGL cubemap support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}
		if (gl.glVersion < 13 && !QueryExtensionGL ("GL_ARB_texture_env_dot3"))
		{
			return Format("OpenGL dot3 combiner support is required. Your GPU (%s) does not support it", rendererString.c_str());
		}
	}
#	endif // if GFX_SUPPORTS_OPENGL


	// D3D9
#	if GFX_SUPPORTS_D3D9
	if (type == kGfxRendererD3D9)
	{
		// Require SM2.0 at least.
		// Note, vertex shaders can be reported as zero in case of software vertex processing.
		// That is a valid & supported case (d3d runtime will run shaders on the CPU, but
		// otherwise they will behave as supported).
		const int kShaderVersion20 = (2 << 8) + 0;
		const int d3dVS = LOWORD(d3d.d3dcaps.VertexShaderVersion);
		const int d3dPS = LOWORD(d3d.d3dcaps.PixelShaderVersion);
		if ((d3dVS != 0 && d3dVS < kShaderVersion20) || (d3dPS < kShaderVersion20))
		{
			return Format("DirectX9 GPU (Shader Model 2.0) is required.\r\nYour GPU (%s)\r\nonly supports Shader Model %i.%i", rendererString.c_str(), d3dPS>>8, d3dPS&0xFF);
		}
	}
#	endif // if GFX_SUPPORTS_D3D9

	UNUSED(type);

#endif
	return ""; // all ok
}


void GraphicsCaps::SharedCapsPostInitialize()
{
	// do some sanity checks
	Assert (!hasStencilInDepthTexture || (hasStencilInDepthTexture && hasNativeDepthTexture)); // stencil in depth texture implies native depth texture

	// requirements for deferred lighting
	const int systemMemoryMB = systeminfo::GetPhysicalMemoryMB();

	hasPrePassRenderLoop = 
#		if GFX_SUPPORTS_RENDERLOOP_PREPASS
		(shaderCaps >= kShaderLevel3) &&				// needs SM3.0
		hasRenderToTexture &&							// needs render textures
		supportsRenderTextureFormat[kRTFormatDepth] &&	// needs depth RT
		hasRenderTargetStencil &&						// needs stencil in RT
		hasTwoSidedStencil &&							// needs two sided stencil
		(systemMemoryMB==0 || systemMemoryMB >= 450)	// disable on old devices that have <512MB RAM (check for zero in case platform doesn't implement it at all...)
#		else
		false
#		endif
		;
}


void GraphicsCaps::InitWithBuildVersion()
{
	#if GFX_SUPPORTS_OPENGL
	// Before 4.2, we never used native shadow maps on GL, so keep that behavior.
	if (GetGfxDevice().GetRenderer() == kGfxRendererOpenGL)
	{
		// First restore original flag (same player instance can be
		// reused across different content versions)
		hasNativeShadowMap = gl.originalHasNativeShadowMaps;
		supportsRenderTextureFormat[kRTFormatShadowMap] = hasNativeShadowMap;
		
		if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
		{
			hasNativeShadowMap = false;
			supportsRenderTextureFormat[kRTFormatShadowMap] = false;
		}
	}
	#endif
}


#if GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES20
bool FindGLExtension (const char* extensions, const char* ext)
{
	// Extension names should not have spaces, be NULL or empty
	if (!ext || ext[0] == 0)
		return false;
	if (strchr(ext, ' '))
		return false;

	Assert (extensions != NULL);
	if (!extensions)
		return false;

	const char* start = extensions;
	for (;;)
	{
		const char* where = strstr (start, ext);
		if (!where)
			break;
		const char* terminator = where + strlen (ext);
		if (where == start || *(where - 1) == ' ')
		{
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		}
		start = terminator;
	}
	return false;
}
#endif // GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES20

#if GFX_SUPPORTS_OPENGLES20
#if GFX_SUPPORTS_OPENGLES20
#	include "Runtime/GfxDevice/opengles20/IncludesGLES20.h"
#endif

bool QueryExtension (const char* ext)
{
	static const char* extensions = NULL;
	if (!extensions)
		extensions = (const char*)glGetString(GL_EXTENSIONS);
	if (!extensions)
		return false;
	return FindGLExtension (extensions, ext);
}

#endif // GFX_SUPPORTS_OPENGLES20



// -------------------------------------------------------------------



#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

SUITE (GraphicsCapsTest)
{
	TEST (GraphicsCaps_DeviceIDs)
	{
		#if UNITY_EDITOR
		if (gGraphicsCaps.GetEmulation() != GraphicsCaps::kEmulNone)
			return;
		#endif

		const int vendorID = gGraphicsCaps.vendorID;
		if (vendorID == 0)
			return;

		std::string vendor = ToLower(gGraphicsCaps.vendorString);

		if (vendorID == 0x10de)
		{
			CHECK (vendor.find("nvidia") != std::string::npos);
		}
		if (vendorID == 0x1002)
		{
			CHECK (vendor.find("ati") != std::string::npos || vendor.find("amd") != std::string::npos);
		}
		if (vendorID == 0x8086)
		{
			CHECK (vendor.find("intel") != std::string::npos);
		}
	}
}

#endif // #if ENABLE_UNIT_TESTS
