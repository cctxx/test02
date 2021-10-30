#include "UnityPrefix.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "UnityGL.h"
#include <string>
#include "Runtime/Utilities/Word.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Shaders/ShaderKeywords.h"
#include "GLContext.h"

#define GL_RT_COMMON_GL 1
#include "Runtime/GfxDevice/GLRTCommon.h"
#undef GL_RT_COMMON_GL


#if UNITY_OSX
#include <OpenGL/OpenGL.h>
#include "GLContext.h"
#endif

#if UNITY_LINUX
#include <dirent.h>
#include <GL/glx.h>
#endif

using namespace std;


bool QueryExtensionGL (const char* ext)
{
	static const char* extensions = NULL;
	if (!extensions)
		extensions = (const char*)glGetString(GL_EXTENSIONS);
	return FindGLExtension (extensions, ext);
}

#if UNITY_LINUX
static bool QueryExtensionGLX (const char* ext)
{
	static const char* glxExtensions = NULL;
	if (!glxExtensions) {
		Display *display = reinterpret_cast<Display*> (GetScreenManager ().GetDisplay ());
		glxExtensions = glXQueryExtensionsString (display, 0);
		printf_console ("GLX Extensions: %s\n", glxExtensions);
	}
	return FindGLExtension (glxExtensions, ext);
}
#endif


#if UNITY_WIN

#include "PlatformDependent/Win/wglext.h"
#include "PlatformDependent/Win/WinDriverUtils.h"

static bool WglQueryExtension( const char* extension )
{
	static bool gotExtensions = false;
	static const char* extensions = NULL;

	if( !gotExtensions )
	{
		gotExtensions = true;

		if (wglGetExtensionsStringARB)
			extensions = wglGetExtensionsStringARB( wglGetCurrentDC() );
		else if (wglGetExtensionsStringEXT)
			extensions = wglGetExtensionsStringEXT();

		// always print WGL extensions
		if( extensions )
			printf_console( "    WGL extensions: %s\n", extensions );
		else
			printf_console( "    WGL extensions: none!\n" );
	}

	if( extensions == NULL || extensions[0] == '\0' )
		return false;

	int extLen = strlen( extension );
	const char* start;
	const char* where, *terminator;
	start = extensions;
	for(;;) {
		where = strstr(start, extension);
		if(!where) break;
		terminator = where + extLen;
		if( where == start || *(where - 1) == ' ' ) {
			if(*terminator == ' ' || *terminator == '\0')
				return true;
		}
		start = terminator;
	}

	return false;
}

#endif


// Checks if all given formats are supported. Terminate the array with -1.
static bool CheckCompressedFormatsSupport( const GLint* formats )
{
	if( !QueryExtensionGL( "GL_EXT_texture_compression_s3tc" ) )
		return false;

	GLint numSupportedFormats = 0;
	glGetIntegerv( GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &numSupportedFormats );
	GLint* supportedFormats = new GLint[numSupportedFormats+1];
	glGetIntegerv( GL_COMPRESSED_TEXTURE_FORMATS_ARB, supportedFormats );
	GLint* supportedFormatsEnd = supportedFormats + numSupportedFormats;

	bool result = true;

	// for each given format, try to find it in the supported formats list
	while( *formats != -1 )
	{
		if( std::find( supportedFormats, supportedFormatsEnd, *formats ) == supportedFormatsEnd )
		{
			result = false;
			break;
		}
		++formats;
	}

	delete[] supportedFormats;

	return result;
}

static bool IsRendererAppleFX5200( const std::string& renderer )
{
	return
		(renderer.find("NVIDIA NV34MAP OpenGL Engine") != string::npos) ||
		(renderer.find("NVIDIA GeForce FX 5200") != string::npos);
}


#if UNITY_WIN
static void GetVideoCardIDsWin (int& outVendorID, int& outDeviceID)
{
	outVendorID = 0;
	outDeviceID = 0;

	DISPLAY_DEVICEA dev;
	dev.cb = sizeof(dev);
	int i = 0;
	while (EnumDisplayDevicesA(NULL, i, &dev, 0))
	{
		if (dev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
		{
			const char* sVendorID = strstr (dev.DeviceID, "VEN_");
			if (sVendorID)
				sscanf(sVendorID, "VEN_%x", &outVendorID);
			const char *sDeviceID = strstr (dev.DeviceID, "DEV_");
			if (sDeviceID)
				sscanf(sDeviceID, "DEV_%x", &outDeviceID);
			return;
		}
		++i;
	}
}
#endif

#if UNITY_OSX
static UInt32 GetOSXVersion()
{
    UInt32 response = 0;

    Gestalt(gestaltSystemVersion, (MacSInt32*) &response);
    return response;
}

static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort, CFStringRef propertyName)
{
	return IORegistryEntrySearchCFProperty(dspPort,
										   kIOServicePlane,
										   propertyName,
										   kCFAllocatorDefault,
										   kIORegistryIterateRecursively | kIORegistryIterateParents);
}

static UInt32 IntValueOfCFData(CFDataRef d)
{
	UInt32 value = 0;
	if (d) {
		const UInt32 *vp = reinterpret_cast<const UInt32*>(CFDataGetBytePtr(d));
		if (vp != NULL)
			value = *vp;
	}
	return value;
}

static void GetVideoCardIDsOSX (int& outVendorID, int& outDeviceID)
{
	CFStringRef strVendorID = CFStringCreateWithCString (NULL, "vendor-id", kCFStringEncodingASCII);
	CFStringRef strDeviceID = CFStringCreateWithCString (NULL, "device-id", kCFStringEncodingASCII);

	CGDirectDisplayID displayID = kCGDirectMainDisplay;
	io_registry_entry_t dspPort = CGDisplayIOServicePort (displayID);
	CFTypeRef vendorIDRef = SearchPortForProperty (dspPort, strVendorID);
	CFTypeRef deviceIDRef = SearchPortForProperty (dspPort, strDeviceID);

	CFRelease (strDeviceID);
	CFRelease (strVendorID);

	outVendorID = IntValueOfCFData ((CFDataRef)vendorIDRef);
	outDeviceID = IntValueOfCFData ((CFDataRef)deviceIDRef);

	if (vendorIDRef) CFRelease (vendorIDRef);
	if (deviceIDRef) CFRelease (deviceIDRef);
}

static float GetVideoMemoryMBOSX()
{
	float vramMB = 0.0f;

	// Adapted from "Custom Cocoa OpenGL" sample.
	// We used code from VideoHardwareInfo sample before, but that reports wrong VRAM
	// amount in some cases (e.g. Intel GMA 950 reports 256MB, but it has 64MB; GeForce 9600 reports
	// 256MB, but it has 512MB).

	const int kMaxDisplays = 8;
	CGDirectDisplayID   displays[kMaxDisplays];
	CGDisplayCount displayCount;
	CGGetActiveDisplayList( kMaxDisplays, displays, &displayCount );
	CGOpenGLDisplayMask openGLDisplayMask = CGDisplayIDToOpenGLDisplayMask(displays[0]);

	CGLRendererInfoObj info;
	CGLint numRenderers = 0;
	CGLError err = CGLQueryRendererInfo( openGLDisplayMask, &info, &numRenderers );
	if( 0 == err ) {
		CGLDescribeRenderer( info, 0, kCGLRPRendererCount, &numRenderers );
		for( int j = 0; j < numRenderers; ++j )
		{
			CGLint rv = 0;
			// find accelerated renderer (assume only one)
			CGLDescribeRenderer( info, j, kCGLRPAccelerated, &rv );
			if( true == rv )
			{
				CGLint vram = 0;
				CGLDescribeRenderer( info, j, kCGLRPVideoMemory, &vram );
				vramMB = double(vram)/1024.0/1024.0;
				break;
			}
		}
		CGLDestroyRendererInfo (info);
    }

	// safe guards
	if( vramMB == 0.0f ) // if some device reports zero (integrated cards?), treat as 64MB
		vramMB = 64.0f;
	return vramMB;
}
#endif

#if UNITY_LINUX
typedef struct {
    unsigned int vendor;
    unsigned int device;
    unsigned long vramSize;
} card_info_t;

static void GetPrimaryGraphicsCardInfo(card_info_t *info)
{
	struct dirent *pDir;

	// Open the /sys/devices directory and iterate over for PCI busses
	// It appears that most modern graphics cards will be mapped as a PCI device.
	// That includes integrated chipsets and AGP cards as well.  AGP cards get connected
	// through an AGP to PCI bus link
	const char *dirPath = "/sys/devices/";
	const char *PCI_BASE_CLASS_DISPLAY = "0x03";

	DIR *devicesDir = opendir(dirPath);
	while ((pDir=readdir(devicesDir)) != NULL)
	{
		if(strncmp(pDir->d_name, "pci", 3) == 0)
		{
			char busID[PATH_MAX];
			memcpy(busID, pDir->d_name+3, strlen(pDir->d_name));

			// Now iterate over each directory for this bus
			struct dirent *pPCIDir;
			char domainPath[PATH_MAX];
			strcpy(domainPath, dirPath);
			strcat(domainPath, pDir->d_name);
			DIR *pciDeviceDir = opendir(domainPath);
			while ((pPCIDir=readdir(pciDeviceDir)) != NULL)
			{
				if(strstr(pPCIDir->d_name, busID) != NULL)
				{
					char filePath[PATH_MAX];
					char line[512];
					char pciSubDevPath[PATH_MAX];
					char domain[PATH_MAX];
					struct dirent *pSubDevDir;

					// Get a copy of the PCI domain
					memcpy(domain, pPCIDir->d_name, 4);

					// Ok, now open and read the first line of the class file.
					// We're only concerned with display class devices
					sprintf(filePath, "%s/%s", domainPath, pPCIDir->d_name);

					FILE *classFile = fopen(filePath, "r");
					fgets(line, sizeof(line), classFile);
					fclose(classFile);

					// Now iterate over each directory for this device

					sprintf(pciSubDevPath, "%s/%s", domainPath, pPCIDir->d_name);
					DIR *pciSubDeviceDir = opendir(pciSubDevPath);
					while ((pSubDevDir=readdir(pciSubDeviceDir)) != NULL)
					{
						char devPath[PATH_MAX];

						// Ok, now open and read the first line of the class file.
						// We're only concerned with display class devices
						sprintf(filePath, "%s/%s", pciSubDevPath, pSubDevDir->d_name);
						strcpy(devPath, filePath);
						strcat(filePath, "/class");

						FILE *classFile = fopen(filePath, "r");
						if(classFile)
						{
							fgets(line, sizeof(line), classFile);
							fclose(classFile);

							if(strstr(line, PCI_BASE_CLASS_DISPLAY) != NULL)
							{
								// Device ID is in the device file
								strcpy(filePath, devPath);
								strcat(filePath, "/device");
								FILE *f = fopen(filePath, "r");
								if(f)
								{
									fgets(line, sizeof(line), f);
									fclose(f);
									info->device = strtol(line, NULL, 16);
								}

								// Vendor ID is in the vendor file
								strcpy(filePath, devPath);
								strcat(filePath, "/vendor");
								f = fopen(filePath, "r");
								if(f)
								{
									fgets(line, sizeof(line), f);
									fclose(f);
									info->vendor = strtol(line, NULL, 16);
								}

								// Now for the tricky part. VRAM size.
								// Each line in the resource file represents the memory range of a PCI resource
								// Start address, end address and flags. Separated by spaces. VRAM is most often
								// the only prefetchable resource so we can identify it by checking for that flag.
								strcpy(filePath, devPath);
								strcat(filePath, "/resource");
								f = fopen(filePath, "r");
								if(f)
								{
									// Loop through each resource
									while(fgets(line, sizeof(line), f))
									{
										unsigned long low, high, flags;
										char *next = line;
										low = strtoull(next, &next, 16);
										high = strtoull(next, &next, 16);
										flags = strtoull(next, &next, 16);
										// Check for the prefetchable flag
										if((flags & 0x08))
										{
											info->vramSize += (high-low)+1;
										}
									}
									fclose(f);
								}
							}
						}
					}
				}
			}
			closedir(pciDeviceDir);
		}
	}
	closedir(devicesDir);
}

static card_info_t gCardInfo;
static void InitCardInfoLinux(){
	gCardInfo.device = 0;
	gCardInfo.vendor = 0;
	gCardInfo.vramSize = 0;
	GetPrimaryGraphicsCardInfo(&gCardInfo);

	if(gCardInfo.vramSize < (64*1024*1024)){
		// Default to 64MB
		gCardInfo.vramSize = 64*1024*1024;
	}
}

static void GetVideoCardIDsLinux (int& outVendorID, int& outDeviceID)
{
	outVendorID = gCardInfo.vendor;
	outDeviceID = gCardInfo.device;
}

static float GetVideoMemoryMBLinux()
{
	// Nvidia
	if (QueryExtensionGL ("GL_NVX_gpu_memory_info")) {
		GLint vram = 0;
		glGetIntegerv (GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &vram);
		printf_console ("GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX: %ld\n", vram);
		GLAssert ();
		return vram / 1024.0f;
	}
	// ATI
	if (QueryExtensionGL ("GL_ATI_meminfo")) {
		GLint vram[4] = {0,0,0,0};
		glGetIntegerv (VBO_FREE_MEMORY_ATI, vram);
		printf_console ("VBO_FREE_MEMORY_ATI: %ld\n", vram[0]);
		glGetIntegerv (RENDERBUFFER_FREE_MEMORY_ATI, vram);
		printf_console ("RENDERBUFFER_FREE_MEMORY_ATI: %ld\n", vram[0]);
		glGetIntegerv (TEXTURE_FREE_MEMORY_ATI, vram);
		printf_console ("TEXTURE_FREE_MEMORY_ATI: %ld\n", vram[0]);
		GLAssert ();
		return vram[0] / 1024.0f;
	}

	// Fallback to pci aperture size
	return gCardInfo.vramSize / 1048576.0f;
}
#endif


void GraphicsCaps::InitGL()
{
	// Figure out major & minor version numbers.
	// Some drivers do not report this according to the spec, and sometimes mix in some letters and whatnot (e.g. "1.08a" - SIS, I'm looking at you).
	// So try to parse one full number, a period and one digit, and stop parsing on anything that we can' t recognize.
	const char *versionStr = (const char *)glGetString (GL_VERSION);
	driverVersionString = versionStr;
	int major = 0, minor = 0;
	bool gotMajor = false;
	while( versionStr && *versionStr ) {
		char c = *versionStr;
		if( isdigit(c) )
		{
			if( !gotMajor )
			{
				major = major * 10 + (c-'0');
			}
			else
			{
				minor = c-'0';
				break; // only take first digit of minor version
			}
		}
		else if( c == '.' )
		{
			gotMajor = true;
		}
		else
		{
			// unknown symbol, stop parsing
			break;
		}
		++versionStr;
	}

	// Important: only take first digit of minor version.
	// Some cards have version like 1.12.
	int version = major * 10 + minor % 10;

	rendererString = (const char*)glGetString( GL_RENDERER );
	vendorString = (const char*)glGetString( GL_VENDOR );

	// some drivers are absolutely horrible!
	AdjustBuggyVersionGL( version, major, minor );

	gl.glVersion = version;

	char buffer[200];
	snprintf( buffer, 200, "OpenGL %d.%d [%s]", major, minor, driverVersionString.c_str() );
	fixedVersionString = buffer;

	#if UNITY_WIN
	GetVideoCardIDsWin (vendorID, rendererID);
	#elif UNITY_OSX
	GetVideoCardIDsOSX (vendorID, rendererID);
	#elif UNITY_LINUX
	InitCardInfoLinux();
	GetVideoCardIDsLinux(vendorID, rendererID);
	#else
	#error "Unknown platform"
	#endif

	#if UNITY_OSX
	videoMemoryMB = GetVideoMemoryMBOSX();
	#elif UNITY_LINUX
	videoMemoryMB = GetVideoMemoryMBLinux();
	#endif

	driverLibraryString = "n/a";

	// On windows, we always output GL info. There is so much variety that
	// it always helps!
	printf_console( "OpenGL:\n" );
	printf_console( "    Version:  %s\n", fixedVersionString.c_str() );
	printf_console( "    Renderer: %s\n", rendererString.c_str() );
	printf_console( "    Vendor:   %s\n", vendorString.c_str() );
	#if UNITY_WIN
	windriverutils::VersionInfo driverVersion;
	std::string driverName;
	if( windriverutils::GetDisplayDriverInfoRegistry( NULL, &driverName, driverVersion ) )
	{
		driverLibraryString = Format("%s %i.%i.%i.%i",
			driverName.c_str(),
			driverVersion.hipart >> 16, driverVersion.hipart & 0xFFFF,
			driverVersion.lopart >> 16, driverVersion.lopart & 0xFFFF );
		printf_console( "    Driver:   %s\n", driverLibraryString.c_str() );
	}
	const char* vramMethod = "";
	int vramMB = windriverutils::GetVideoMemorySizeMB (MonitorFromWindow(GetDesktopWindow(),MONITOR_DEFAULTTOPRIMARY), &vramMethod);
	videoMemoryMB = vramMB;
	printf_console( "    VRAM:     %i MB (via %s)\n", (int)videoMemoryMB, vramMethod );
	#else
	driverLibraryString = driverVersionString;
	printf_console( "    VRAM:     %i MB\n", (int)videoMemoryMB );
	#endif

	printf_console( "    Extensions: %s\n", glGetString(GL_EXTENSIONS) );

#if UNITY_OSX
	maxVSyncInterval = 4;
#elif UNITY_LINUX
	if (QueryExtensionGLX ("GLX_SGI_swap_control"))
		maxVSyncInterval = 4;
	printf_console ("Setting maxVSyncInterval to %d\n", maxVSyncInterval);
#else
// TODO: What should max vsync interval be?
#endif
	has16BitFloatVertex = QueryExtensionGL( "GL_ARB_half_float_vertex" );
	needsToSwizzleVertexColors = false;

	glGetIntegerv( GL_MAX_LIGHTS, &maxLights );
	maxLights = clamp<int>( maxLights, 0, 8 );

	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );
	glGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapSize );
	maxRenderTextureSize = maxTextureSize;

	hasBlendSquare = QueryExtensionGL( "GL_NV_blend_square" );
	hasSeparateAlphaBlend = QueryExtensionGL ("GL_EXT_blend_func_separate");

	// Treat blendOps supported only if we have a full set (minmax & subtract; separate
	// blend equation if have separate alpha func)
	hasBlendSub    = QueryExtensionGL("GL_EXT_blend_subtract");
	hasBlendMinMax = QueryExtensionGL("GL_EXT_blend_minmax");
	if (hasSeparateAlphaBlend)
	{
		hasBlendSub &= QueryExtensionGL("GL_EXT_blend_equation_separate");
		hasBlendMinMax &= QueryExtensionGL("GL_EXT_blend_equation_separate");
	}

	// GLSL Vertex/Fragment shaders
	gl.hasGLSL = QueryExtensionGL("GL_ARB_shader_objects") && QueryExtensionGL("GL_ARB_vertex_shader") && QueryExtensionGL("GL_ARB_fragment_shader");


	gl.nativeVPInstructions = gl.vertexAttribCount = 0;
	// Instruction count
	glGetProgramivARB (GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &gl.nativeVPInstructions);
	// Vertex atrribute count
	glGetProgramivARB (GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ATTRIBS_ARB, &gl.vertexAttribCount);
	if( gl.vertexAttribCount > 16 )
		gl.vertexAttribCount = 16;

	gl.nativeFPInstructions = gl.nativeFPTemporaries = gl.nativeFPALUInstructions = gl.nativeFPTexInstructions = 0;
	// FP capabilities
	glGetProgramivARB (GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &gl.nativeFPInstructions);
	glGetProgramivARB (GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB, &gl.nativeFPTemporaries);
	glGetProgramivARB (GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, &gl.nativeFPALUInstructions);
	glGetProgramivARB (GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB, &gl.nativeFPTexInstructions);

	gl.hasTextureEnvCombine3ATI = QueryExtensionGL( "GL_ATI_texture_env_combine3" );
	gl.hasTextureEnvCombine3NV = QueryExtensionGL( "GL_NV_texture_env_combine4" );

	maxTexUnits = maxTexImageUnits = maxTexCoords = 1;
	glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&maxTexUnits);
	glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS_ARB, (GLint *)&maxTexImageUnits);
	glGetIntegerv( GL_MAX_TEXTURE_COORDS_ARB, (GLint *)&maxTexCoords );

	// Clamp max counts. Some recent GPUs (e.g. Radeon 5xxx on Windows)
	// report 16 texture coordinates, which wreak havoc in the channel bindings
	// (those assume 8 coords maximum, and more than that starts overwriting
	// generic attribute bindings)
	maxTexCoords = std::min<int> (maxTexCoords, kMaxSupportedTextureCoords);
	maxTexImageUnits = std::min<int> (maxTexImageUnits, kMaxSupportedTextureUnits);
	maxTexUnits = std::max<int>( std::min<int>( maxTexUnits, kMaxSupportedTextureUnits ), 1 );

	hasAnisoFilter = QueryExtensionGL( "GL_EXT_texture_filter_anisotropic" );
	if( hasAnisoFilter )
		glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisoLevel );
	else
		maxAnisoLevel = 1;
	hasMipLevelBias = version >= 14 || QueryExtensionGL( "GL_EXT_texture_lod_bias" );

	static GLint requiredCompressedFormats[] = { GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, -1 };
	hasS3TCCompression = CheckCompressedFormatsSupport( requiredCompressedFormats );

	// Set bits defined by GL version
	gl.hasArbMapBufferRange = QueryExtensionGL ("GL_ARB_map_buffer_range");

	// ARB_sync seems to be flaky on OS X 10.7
	// Discovered as part of case 441958, when switching fullscreen mode.
	// it will fall back to GL_APPLE_fence which seems to work reliably
	gl.hasArbSync = UNITY_LINUX && QueryExtensionGL ("GL_ARB_sync");

	#if UNITY_OSX
	gl.hasAppleFence = QueryExtensionGL ("GL_APPLE_fence");
	gl.hasAppleFlushBufferRange = QueryExtensionGL ("GL_APPLE_flush_buffer_range");
	#endif

	has3DTexture = QueryExtensionGL ("GL_EXT_texture3D") || version >= 12;

	npot = QueryExtensionGL ("GL_ARB_texture_non_power_of_two") ? kNPOTFull : kNPOTNone;
	// On OS X, pre-DX10 ATI cards can't really fully handle it.
#if UNITY_OSX
	if (npot == kNPOTFull && gl.nativeFPInstructions < 4096) {
		npot = kNPOTRestricted;
	}
#endif
	npotRT = npot;

	hasTimerQuery = QueryExtensionGL ("GL_EXT_timer_query");

	// has multi sampling?
	#if UNITY_WIN
	hasMultiSample = QueryExtensionGL( "GL_ARB_multisample" ) && WglQueryExtension( "WGL_ARB_multisample" );
	gl.hasWglARBPixelFormat = WglQueryExtension( "WGL_ARB_pixel_format" ) && wglGetPixelFormatAttribivARB && wglChoosePixelFormatARB;
	if( !gl.hasWglARBPixelFormat )
	{
		hasMultiSample = false;
		printf_console( "GL: disabling multisample because WGL_ARB_pixel_format not fully supported\n" );
	}
	#else
	hasMultiSample = QueryExtensionGL ("GL_ARB_multisample");
	#endif

	hasMultiSample &= QueryExtensionGL ("GL_EXT_framebuffer_multisample");
	gl.maxSamples = 1;
	if (hasMultiSample)
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &gl.maxSamples);

	// has WGL swap control?
	#if UNITY_WIN
	gl.hasWglSwapControl = WglQueryExtension( "WGL_EXT_swap_control" );
	#endif

	hasSRGBReadWrite = QueryExtensionGL("GL_EXT_texture_sRGB");
	hasSRGBReadWrite = hasSRGBReadWrite && QueryExtensionGL("GL_EXT_framebuffer_sRGB");

	// Has render textures? We only support FBOs, not p-buffers.
	hasRenderToTexture = QueryExtensionGL("GL_EXT_framebuffer_object");
	hasRenderToCubemap = hasRenderToTexture;
	hasRenderTargetStencil = QueryExtensionGL("GL_EXT_packed_depth_stencil");
	if (QueryExtensionGL("GL_ARB_draw_buffers"))
	{
		glGetIntegerv (GL_MAX_DRAW_BUFFERS_ARB, &maxMRTs);
		maxMRTs = clamp<int> (maxMRTs, 1, kMaxSupportedRenderTargets);
	}
	else
	{
		maxMRTs = 1;
	}
	hasStencil = true;
	hasTwoSidedStencil = (version >= 20);
	hasNativeDepthTexture = true;
	hasStencilInDepthTexture = true;


	#pragma message ("Properly implement supported formats!")
	for (int q = 0; q < kTexFormatPCCount; ++q)
		supportsTextureFormat[q] = true;


	supportsRenderTextureFormat[kRTFormatARGB32] = hasRenderToTexture;
	supportsRenderTextureFormat[kRTFormatDepth] = QueryExtensionGL ("GL_ARB_depth_texture");
	supportsRenderTextureFormat[kRTFormatA2R10G10B10] = false;
	supportsRenderTextureFormat[kRTFormatARGB64] = false;

	if( hasRenderToTexture )
	{
		FBColorFormatCheckerGL checker;

		supportsRenderTextureFormat[kRTFormatRGB565]	= checker.CheckFormatSupported(GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
		supportsRenderTextureFormat[kRTFormatARGB4444]	= checker.CheckFormatSupported(GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4);
		supportsRenderTextureFormat[kRTFormatARGB1555]	= checker.CheckFormatSupported(GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1);
	}

	{
		const bool supportFloatTex	= QueryExtensionGL("GL_ARB_texture_float");
		const bool supportsHalfRT	= supportFloatTex && QueryExtensionGL("GL_ARB_half_float_pixel");
		const bool supportsFloatRT	= supportFloatTex && (QueryExtensionGL("GL_ARB_color_buffer_float") || QueryExtensionGL("GL_APPLE_float_pixels"));
		const bool supportsIntRT	= QueryExtensionGL("GL_EXT_texture_integer");
		const bool supportsRG		= QueryExtensionGL("GL_ARB_texture_rg");

        supportsRenderTextureFormat[kRTFormatARGBHalf]  = hasRenderToTexture && supportsHalfRT;
        supportsRenderTextureFormat[kRTFormatARGBFloat] = hasRenderToTexture && supportsFloatRT;
        supportsRenderTextureFormat[kRTFormatR8]        = hasRenderToTexture && supportsRG;
        supportsRenderTextureFormat[kRTFormatRHalf]     = hasRenderToTexture && supportsHalfRT && supportsRG;
        supportsRenderTextureFormat[kRTFormatRFloat]	= hasRenderToTexture && supportsFloatRT && supportsRG;
        supportsRenderTextureFormat[kRTFormatRGHalf]    = hasRenderToTexture && supportsHalfRT && supportsRG;
        supportsRenderTextureFormat[kRTFormatRGFloat]	= hasRenderToTexture && supportsFloatRT && supportsRG;
    }

	supportsRenderTextureFormat[kRTFormatDefault]	= hasRenderToTexture;

	hasNativeShadowMap = supportsRenderTextureFormat[kRTFormatDepth] && QueryExtensionGL("GL_ARB_fragment_program_shadow");
	// GL_ARB_fragment_program_shadow does not work on OS X (at least on 10.4.8-10.4.10)
	// Seems to work on 10.6.7 & 10.7.x (nvidia), so for safety just disable it on < 10.6.7.
	#if UNITY_OSX
	if (GetOSXVersion() < 0x01067)
		hasNativeShadowMap = false;
	#endif
	gl.originalHasNativeShadowMaps = hasNativeShadowMap;
	supportsRenderTextureFormat[kRTFormatShadowMap] = hasNativeShadowMap;

	// Framebuffer blit
	gl.hasFrameBufferBlit = QueryExtensionGL( "GL_EXT_framebuffer_blit" );

	hasAutoMipMapGeneration = version >= 14 || QueryExtensionGL("GL_SGIS_generate_mipmap");

	//// *** Unity bug workarounds following:

	// Driver bugs/workarounds following
	DetectDriverBugsGL( version );

	// in the very end, figure out shader capabilities level (after all workarounds are applied)
	if (gl.nativeFPInstructions < 1024 || gl.nativeFPTexInstructions < 512 || gl.nativeFPALUInstructions < 512 || gl.nativeFPTemporaries < 32)
	{
		// under 1024 instructions (512 tex + 512 alu) or under 32 temps: 2.x shaders
		shaderCaps = kShaderLevel2;
	}
	else
	{
		// has everything we care about!
		shaderCaps = kShaderLevel3;
	}
}

static bool DisableGraphicsWorkarounds()
{
	// Disable workarounds if we have env set
	if( getenv ("UNITY_DISABLE_GRAPHICS_DRIVER_WORKAROUNDS") && StrICmp (getenv ("UNITY_DISABLE_GRAPHICS_DRIVER_WORKAROUNDS"), "yes") == 0 )
	{
		printf_console( "GL: disabling graphics workarounds\n" );
		return true;
	}
	return false;
}

void GraphicsCaps::AdjustBuggyVersionGL( int& version, int& major, int& minor ) const
{
	if( DisableGraphicsWorkarounds() )
		return;

	// SiS stuff
	if( (vendorString.find("SiS") == 0) )
	{
		// Compatible VGA / MMX: reports as 2.06a.00, seems like 1.1
		if( (rendererString.find("Compatible VGA") != string::npos) && version >= 20 )
		{
			printf_console("GL: Wrong GL version reported, treating as GL 1.1\n");
			version = 11;
			major = 1;
			minor = 1;
			return;
		}
		// 630/730 / MMX: reports as 2.03 to 2.08, seems like 1.1
		if( (rendererString.find("630/730") != string::npos) && version >= 20 )
		{
			printf_console("GL: Wrong GL version reported, treating as GL 1.1\n");
			version = 11;
			major = 1;
			minor = 1;
			return;
		}
	}
}

#if UNITY_WIN
#include "PlatformDependent/Win/WinUtils.h"
#endif

void GraphicsCaps::DetectDriverBugsGL( int version )
{
	gl.buggyArbPrecisionHint = false;
	gl.force24DepthForFBO = false;
	gl.cacheFPParamsWithEnvs = false;
	gl.forceColorBufferWithDepthFBO = false;
	gl.buggyPackedDepthStencil = false;
	gl.mustWriteToDepthBufferBeforeClear = false;

	const bool isNvidia = (vendorString.find("NVIDIA") != string::npos) || (vendorString.find("Nvidia") != string::npos);
	const bool isRadeon = (rendererString.find("RADEON") != string::npos) || (rendererString.find("Radeon") != string::npos);

#if UNITY_WIN || UNITY_LINUX
	// vendorString on Intel Gfx / Linux contains Tungsten graphics instead of Intel so we check the rendererString too...
	bool isIntel = (vendorString.find ("INTEL") != string::npos) || (vendorString.find ("Intel") != string::npos) || (rendererString.find("Intel") != string::npos);
#endif


	// Driver version numbers
#if UNITY_WIN || UNITY_LINUX
	int majorVersion = 0, minorVersion = 0;
	bool parsedDriverVersion = false;
#endif
#if UNITY_WIN
	int buildVersion = 0;
#endif
#if UNITY_LINUX
	int seriesVersion = 0;
	bool parsedSeriesVersion = false;
#endif

#if UNITY_WIN
	int driverFields = sscanf( driverVersionString.c_str(), "%i.%i.%i", &majorVersion, &minorVersion, &buildVersion );
	parsedDriverVersion = (driverFields == 3);
#endif
#if UNITY_LINUX
	if (isNvidia)
	{
		int temp1, temp2, temp3 = 0;
		int driverFields = sscanf (driverVersionString.c_str(), "%i.%i.%i NVIDIA %i.%i", &temp1, &temp2, &temp3, &majorVersion, &minorVersion);
		parsedDriverVersion = (driverFields == 5);
	}
	if (isRadeon)
	{
		char temp[BUFSIZ];
		int seriesFields = sscanf (rendererString.c_str(), "%s Radeon HD %i %*", temp, &seriesVersion);
		parsedSeriesVersion = (seriesFields == 2);
	}
#endif

	if( DisableGraphicsWorkarounds() )
		return;


	// Timer queries are broken on OS X + older NVIDIA cards. Before 10.8 they just return bogus data;
	// starting with 10.8 they actually lock up. It seems that everything before GeForce 6xx is affected
	// (8800, 9400, 9600, GT120, 320, GT330). But GT650 is not affected and has GPU profiler working properly.
	// From https://developer.apple.com/graphicsimaging/opengl/capabilities/ looks like earlier GPUs have ~60
	// varying floats, and 6xx has ~120, so let's detect on that.
	#if UNITY_OSX
	if (isNvidia && gl.hasGLSL)
	{
		// Case 562054: 
		// It looks that timer queries are broken on OS X + Nvidia cards again.
		// For now, just disable timer queries on all Nvidia cars + OS X. 
		
		//GLint maxVarying = 0;
		//glGetIntegerv (GL_MAX_VARYING_FLOATS, &maxVarying);
		//if (maxVarying < 100)
			hasTimerQuery = false;
	}
	#endif // #if UNITY_OSX


	// DynamicVBO with tangents is buggy. From comment by rej in dynamic batching code:
	// "For some (yet) unkown reason DynamicNullVBO under OpenGL fail rendering geometry with Tangents
	// we have to avoid batching for geometry with Tangents as long as DynamicNullVBO is going to be used"
	// No one remembers the OS/GPU details by now (was around Unity 3.0 release), and looks like it's not a problem
	// around Unity 4.0 release. So we'll assume that some recent OS X fixed the underlying issue. Let's say
	// starting with 10.6.8 the problem does not exist...
#if UNITY_OSX
	if (GetOSXVersion() < 0x01068)
		buggyDynamicVBOWithTangents = true;
#endif

#if UNITY_LINUX
	if (isNvidia && parsedDriverVersion && (majorVersion <= 195))
	{
		printf_console ("GL: enabling buggyPackedDepthStencil due to driver bug\n");
		gl.buggyPackedDepthStencil = true;
		buggyTextureBothColorAndDepth = true;
	}
#endif

#if UNITY_OSX || UNITY_LINUX
	bool isGMA950 = (rendererString == "Intel GMA 950 OpenGL Engine");
	bool isGMA3100 = (rendererString == "Intel GMA X3100 OpenGL Engine");
#endif


#if UNITY_OSX
	bool hasLeakingFBOWhenUsingMipMaps = rendererString == "NVIDIA GeForce 8600M GT OpenGL Engine";
	if (hasLeakingFBOWhenUsingMipMaps)
	{
		printf_console( "GL: disabling mip map calculation on Render Textures due to memory leak on NVIDIA GeForce 8600 graphics drivers.\n");
		hasAutoMipMapGeneration = false;
	}
#endif


#if UNITY_OSX
	// On OS X, trying to create depth texture/buffer in 16 bits fails on pre-10.6.
	if (GetOSXVersion() < 0x01062)
	{
		gl.force24DepthForFBO = true;
	}
#endif

	// Macs with Radeon HD cards (2400-4850) have bugs when using depthstencil texture
	// as both depth buffer for testing and reading as a texture. Happens at least on 10.5.8 to 10.6.4.
	// Same happens on Radeon X1600 as well, so we just do the workaround for all Radeons.
	//
	// Also, 10.6.4 graphics driver update broke regular sampling of depth stencil textures. Some words from
	// Apple's Matt Collins:
	// "What I suspect is happening is that ATI doesn't handle sampling from depth
	// textures correctly if Hi-Z/Z-compression is on AND that texture has no mipmapping. I want to see
	// if we can force the driver to alloc mipmaps for you. Typically we check a couple things to
	// determine whether to allocate: whether the app has requested mipmaps, whether one the sampling mode is
	// a mipmap filter, if mipmaps have been manually specified, or if MAX_LEVELS has been changed from
	// the default value."
	// Setting TEXTURE_MAX_LEVELS to zero before creating the texture fixes this particular problem; but sampling
	// of the texture while using it for depth testing is still broken.
#if UNITY_OSX
	if (hasRenderTargetStencil && isRadeon)
	{
		printf_console( "GL: buggy packed depth stencil; Deferred rendering will use slower rendering path\n" );
		gl.buggyPackedDepthStencil = true;
		buggyTextureBothColorAndDepth = true;
	}
#endif

	// Disable 16-bit float vertex channels on Radeon cards as the driver hits a super slow path
	// when using three 16-bit components of a normal and 16-bits of padding to keep it aligned.
	if (isRadeon)
		has16BitFloatVertex = false;

#if UNITY_OSX && WEBPLUG
	if (GetOSXVersion() >= 0x01072 && isNvidia)
		gl.mustWriteToDepthBufferBeforeClear = true;
#endif

#if UNITY_OSX
	// Macs with GeForce 7300 cards have broken packed depth stencil implementation. Happens at least
	// up to OS X 10.6.3. We can tell pre-DX10 cards by looking at max. texture size; it is always less
	// than 8192 on Macs.
	const bool isGeForcePreDX10 = isNvidia && (maxTextureSize < 8192);
	if (hasRenderTargetStencil && isGeForcePreDX10)
	{
		printf_console( "GL: disabling deferred lighting (buggy packed depth stencil on pre-DX10 GeForce cards)\n" );
		hasRenderTargetStencil = false;
		buggyTextureBothColorAndDepth = true;
		gl.force24DepthForFBO = true; // looks like that still can't handle 16 bit FBO depth (OS X 10.6.3)
	}

	// Also, GeForce 7300 seems to bail out on anti-aliasing switches. OS X 10.5.8; anti-aliasing switching
	// graphics tests produce "NVDA(OpenGL): Channel exception!  status = 0xffff info32 = 0x6 = Fifo: Parse Error".
	if (isGeForcePreDX10 && GetOSXVersion() < 0x01062)
	{
		printf_console( "GL: disabling anti-aliasing (buggy drivers on OS X 10.5 on pre-DX10 GeForce cards)\n" );
		hasMultiSample = false;
	}

#	if WEBPLUG
	if (isNvidia && GetOSXVersion () < 0x01090)
	{
		// NVIDIA drivers crash on OS X earlier than 10.9.0 when multisampling is enabled (case 521161):
		// - when right-clicking the Unity content in Firefox
		// - when exiting fullscreen in Chrome and Safari
		printf_console( "GL: disabling anti-aliasing (buggy drivers on OS X pre-10.9.0 on NVIDIA cards give kernel panics)\n" );
		hasMultiSample = false;
	}
#	endif
#endif

#if UNITY_OSX
	// OS X with GMA X3100: lots of problems! (on OS X earlier than 10.5.3)
	if( isGMA3100 && GetOSXVersion() < 0x01053 )
	{
		// Crashes if depth component FBO is used without color attachment.
		if( hasRenderToTexture )
			gl.forceColorBufferWithDepthFBO = true;

		// Depth textures just contain garbage
		printf_console( "GL: disabling shadows (buggy on GMA X3100 pre-10.5.3)\n" );
		supportsRenderTextureFormat[kRTFormatDepth] = false;
		// Cubemap pbuffers/FBOs contain garbage
		printf_console( "GL: disabling render to cubemap (buggy on GMA X3100 pre-10.5.3)\n" );
		hasRenderToCubemap = false;
	}

	// OS X 10.5.3 and 10.5.4 with non-updated drivers on Radeon X1xxx cards: shadows and render textures broken!
	// Do not disable them though, sometimes they work... just show error message.
	if( rendererString.find("ATI Radeon X1") != string::npos && GetOSXVersion() >= 0x01053 )
	{
		// Driver 2.0 ATI-1.5.28 is broken (10.5.3, looks like 10.5.4 has the same driver).
		// Pre-release driver 2.0 ATI-1.5.30 is good.
		int driverBuild = 0;
		sscanf( driverVersionString.c_str(), "2.0 ATI-1.5.%i", &driverBuild );
		if( driverBuild >= 28 && driverBuild < 30 ) {
			ErrorString( "GL: broken Radeon X1xxx driver, shadows and render textures might behave funny. Wait for Apple to fix it..." );
		}
	}
#endif

#if UNITY_WIN || UNITY_OSX || UNITY_LINUX
	// When creating the FBO and not attaching any color buffer, the FBO will return 'FBO fail: INCOMPLETE_DIMENSIONS'.
	gl.forceColorBufferWithDepthFBO = true;
#endif

#if UNITY_OSX
	bool isGeForceGT650M = (rendererString == "NVIDIA GeForce GT 650M OpenGL Engine");
	if (isGeForceGT650M)
	{
		// MBP Retina June 2012 with OS X 10.8.2 freezes when at least one dimension of the texture is 8192
		maxTextureSize = 4096;
	}
#endif

#if UNITY_OSX
	// Some NVIDIA cards have a broken ARB precision hint. We postprocess all fragment
	// programs to not use it.
	// It is definitely broken on a GeForce 5200 mobility on tiger 10.4.1
	gl.buggyArbPrecisionHint |= IsRendererAppleFX5200(rendererString);
#endif

#if UNITY_OSX
	if( isGMA950 )
	{
		printf_console("GL: Disabling soft shadows because of GMA950 driver bugs\n");
		disableSoftShadows = true;

		// OS X: occasional crashes in auto FBO mipmap generation, OS X 10.5 & 10.6, case 372004
		printf_console( "GL: disabling mip map calculation on Render Textures due to GMA950 driver bugs\n");
		hasAutoMipMapGeneration = false;

		// Something wrong with FBOs on GMA 950, try disabling depth+stencil & color+depth FBOs.
		hasRenderTargetStencil = false;
		buggyTextureBothColorAndDepth = true;
	}
#endif

#if UNITY_OSX && UNITY_LITTLE_ENDIAN
	// On Intel Macs with ATI cards, use Env parameters for fragment programs instead of Local
	// ones, and cache the redundant changes.
	// Do not do this on any other cards, as that will make shadows randomly disappear on PPC Macs.
	gl.cacheFPParamsWithEnvs |= vendorString.find("ATI Technologies") != string::npos;
#endif


#if UNITY_WIN
	if( isIntel )
	{
		// Intel 965 seems to be randomly broken with shadows, and sometimes with render
		// textures. Sometimes they hang with them (e.g. in terrain unit test). Just disable.
		if( rendererString.find("965/963") != string::npos || rendererString.find("Broadwater") != string::npos )
		{
			if( hasRenderToTexture )
			{
				printf_console( "GL: Disabling render to texture on Intel card (buggy)\n" );
				hasRenderToTexture = false;
				hasRenderToCubemap = false;
				hasAutoMipMapGeneration = false;
				for (int i = 0; i < kRTFormatCount; ++i)
					supportsRenderTextureFormat[i] = false;
				maxMRTs = 1;
			}
		}
	}
#endif

#if UNITY_LINUX
	// Hardware shadow maps on spot light shadows seem to be weird,
	// at least on gfx test agent (GeForce GTX 260 OpenGL 3.3 [3.3.0 NVIDIA 295.40])
	buggySpotNativeShadowMap = true;
#endif

#if UNITY_WIN || UNITY_LINUX
	if( isIntel )
	{
		// Intel 9xx have lots of problems with shadows. Just disable them.
		printf_console( "GL: disabling shadows on Intel 9xx (buggy)\n" );
		supportsRenderTextureFormat[kRTFormatDepth] = false;
		hasRenderToCubemap = false;
		hasAutoMipMapGeneration = false;
	}
#endif

#if UNITY_LINUX
	if (isIntel)
	{
		printf_console ("GL: disabling framebuffer blit, antialiasing, SRGB on Intel\n");
		gl.hasFrameBufferBlit = false;
		hasMultiSample = false;
		hasSRGBReadWrite = false;

	}
#endif

	// S3 UniChrome does not have 3D textures, even if they say it's OpenGL 1.2.
	// To be safe just disable them on all S3 cards that don't explicitly expose it.
	if( (vendorString.find("S3 ") == 0) && !QueryExtensionGL("GL_EXT_texture3D") )
	{
		printf_console( "GL: Disabling 3D textures because the card does not actually have them\n" );
		has3DTexture = false;
	}


	// Many Radeon drivers on windows are quite bad
#if UNITY_WIN
	if( (isRadeon || (rendererString.find("FIREGL") != string::npos) || (rendererString.find("FireGL") != string::npos)) && (vendorString.find("ATI ") != string::npos) )
	{
		// FireGL with version 2.0.5284 crashes when destroying render textures. The driver is
		// from about year 2005. Disable RTs.
		if( parsedDriverVersion && version <= 20 && buildVersion <= 5284 && rendererString.find("FireGL") != string::npos )
		{
			hasRenderToTexture = hasRenderToCubemap = false;
			for (int i = 0; i < kRTFormatCount; ++i)
				supportsRenderTextureFormat[i] = false;
			maxMRTs = 1;
			printf_console( "GL: disabling render textures due to FireGL driver bugs\n" );
		}

		/* The issue description from Emil Persson <epersson@ati.com>:

		The problem is that the shaders are using constructs like this:
			PARAM c[4] = { program.local[0..2], { 2 } };
		Older drivers didn't handle mixed declaration well. Are you using Cg to
		compile the shaders? They switched to using mixed declarations quite a
		while ago, which exposed the bug. If you revert to an older version of
		the compiler (I think it was 1.3 that caused the problem, so try 1.2)
		that should dodge the problem for 5.3.
		<...>
		It's not the range, but the mix of params and immediate values that
		causes the problem. Both work fine separately, but if you put both in
		the same array it's going to break. So what you can do is split it into
		two arrays, one for program.local and one for immediate values.
			PARAM c[4] = { program.local[0..2], { 2 } };
		becomes
			PARAM c[3] = { program.local[0..2] };
			PARAM d[1] = { { 2 } };
		Then you'll have to replace all occurrences of c[3] with d[0] in the
		shader.
		*/

		// Even with latest drivers today (7.4, May 2007) render-to-cubemap
		// does not quite work (faces inverted/flipped/etc.). Disable!
		printf_console( "GL: disabling render-to-cubemap due to Radeon driver bugs\n" );
		hasRenderToCubemap = false;

		// Mip-mapped render textures seem to crash randomly, at least
		// on MBP/Vista (GL version 2.0.6347). E.g. the mipmapped RT unit test scene crashes.
		// Does work correctly on XP.
		//
		// The same also crashes on iMac Radeon HD 2600, Windows XP (GL version 2.0.6641). So we detect Vista
		// or Radeon HD and disable.
		if( winutils::GetWindowsVersion() >= winutils::kWindowsVista || rendererString.find("Radeon HD") != string::npos )
		{
			printf_console( "GL: disabling mipmapped render textures due to Radeon driver bugs\n" );
			hasAutoMipMapGeneration = false;
		}
	}
#endif

	// Sanitize VRAM amount
	if( videoMemoryMB < 32 ) {
		printf_console("GL: VRAM amount suspiciously low (less than 32MB for fragment program card)\n");
		videoMemoryMB = 32;
	}
}
