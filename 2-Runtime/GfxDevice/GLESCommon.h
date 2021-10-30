#ifndef GLES_COMMON_H
#define GLES_COMMON_H

// internal header
// common gles 1.x/2.x stuff
// also some common android/ios stuff

#include <ctype.h>

inline int GLES_EstimateVRAM_MB(int total_mem_mb)
{
#if UNITY_IPHONE
	// For iOS we estimate 1/4 of POT(total_mem)
	const int physical_mem_mb = 1 << (32 - __builtin_clz(total_mem_mb - 1));
	return physical_mem_mb >> 2;
#elif UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
	// There is (afawk) no way to determine the size of the VRAM
    // So we try to create ('fake') a value that is somewhere along the lines of possibly correct.
    // We make two assumption here:
    // 1) The VRAM is cut-out from the physical memory range and 2) VRAM size is never bigger than
    // a 1/4  of the physical memory size, but still bigger than a 1/16 of the total memory size.
    // Even if not correct it should give an indication of how much memory is available for GPU resources.
    const int physical_mem_mb = 1 << (32 - __builtin_clz(total_mem_mb - 1));
    return std::max(  std::min(physical_mem_mb - total_mem_mb, (physical_mem_mb >> 2)),  (total_mem_mb >> 4) );
#else
    return 256;
#endif
}

namespace systeminfo { int GetPhysicalMemoryMB(); }
struct GraphicsCaps;

inline void GLES_InitCommonCaps(GraphicsCaps* caps)
{
    {
        caps->vendorID   = 0;
        caps->rendererID = 0;

        #define GRAB_STRING(target, name)                           \
        do {                                                        \
            const char* _tmp_str = (const char*)glGetString(name);  \
            GLESAssert();                                           \
            caps->target = _tmp_str ? _tmp_str : "<unknown>";       \
        } while(0)                                                  \

        GRAB_STRING(rendererString, GL_RENDERER);
        GRAB_STRING(vendorString, GL_VENDOR);
        GRAB_STRING(driverVersionString, GL_VERSION);

        #undef GRAB_STRING

        // Distill
        //    driverVersionString = "OpenGL ES 2.0 build 1.8@905891"
        // into
        //    driverLibraryString = "build 1.8@905891"
        //
        // See http://www.khronos.org/opengles/sdk/1.1/docs/man/glGetString.xml
        //
        const char OpenGL[] = "OpenGL";
        const char ES[] = "ES";
        const char* gl_version = caps->driverVersionString.c_str();
        for (int i = 0 ; i < 3; ++i, ++gl_version)
        {
            if(    (i == 0 && strncmp(gl_version, OpenGL, sizeof(OpenGL)-1))
                || (i == 1 && strncmp(gl_version, ES, sizeof(ES)-1))
                || (i == 2 && !isdigit(*gl_version))
              )
            {
                gl_version = NULL;
                break;
            }
            if ( !(gl_version = strstr(gl_version, " ")) )
                break;
        }

        if (gl_version)
            caps->driverLibraryString = gl_version;
        else
            caps->driverLibraryString = "n/a";

        caps->fixedVersionString  = caps->driverVersionString;

        const char* ext = (const char*)glGetString(GL_EXTENSIONS);
        GLESAssert();

        ::printf_console ("Renderer: %s\n", caps->rendererString.c_str());
        ::printf_console ("Vendor:   %s\n", caps->vendorString.c_str());
        ::printf_console ("Version:  %s\n", caps->driverVersionString.c_str());

        if(ext) DebugTextLineByLine(ext);
        else    ::printf_console("glGetString(GL_EXTENSIONS) - failure");
    }

    {
    #if UNITY_ANDROID || UNITY_IPHONE || UNITY_BB10 || UNITY_TIZEN
        caps->videoMemoryMB = GLES_EstimateVRAM_MB(systeminfo::GetPhysicalMemoryMB());
    #else
        caps->videoMemoryMB = 256; // awesome estimation
    #endif
    }
}


#endif // GLES_COMMON_H

