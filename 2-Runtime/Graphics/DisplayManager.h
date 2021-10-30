#pragma once
#include "UnityPrefix.h"

// we do c-style interface, because display management is way too platfrom specific
// these function will be used from script
// UnityDisplayManager prefix is used because some platforms implements this in trampoline

#define SUPPORT_MULTIPLE_DISPLAYS UNITY_IPHONE

#if !SUPPORT_MULTIPLE_DISPLAYS
    #include "ScreenManager.h"
#endif

struct RenderSurfaceBase;

#if SUPPORT_MULTIPLE_DISPLAYS

    extern "C" int	UnityDisplayManager_DisplayCount();
    extern "C" bool	UnityDisplayManager_DisplayAvailable(void* nativeDisplay);
    extern "C" void	UnityDisplayManager_DisplaySystemResolution(void* nativeDisplay, int* w, int* h);
    extern "C" void	UnityDisplayManager_DisplayRenderingResolution(void* nativeDisplay, int* w, int* h);
    extern "C" void	UnityDisplayManager_DisplayRenderingBuffers(void* nativeDisplay, RenderSurfaceBase** colorBuffer, RenderSurfaceBase** depthBuffer);
    extern "C" void	UnityDisplayManager_SetRenderingResolution(void* nativeDisplay, int w, int h);

#else

    inline int  UnityDisplayManager_DisplayCount()
    {
        return 1;
    }
    inline bool UnityDisplayManager_DisplayAvailable(void*)
    {
        return true;
    }
    inline void UnityDisplayManager_DisplaySystemResolution(void*, int* w, int* h)
    {
        *w = GetScreenManager().GetWidth();
        *h = GetScreenManager().GetHeight();
    }
    inline void UnityDisplayManager_DisplayRenderingResolution(void*, int* w, int* h)
    {
        *w = GetScreenManager().GetWidth();
        *h = GetScreenManager().GetHeight();
    }
    inline void UnityDisplayManager_SetRenderingResolution(void*, int w, int h)
    {
        GetScreenManager().RequestResolution(w, h, GetScreenManager ().IsFullScreen (), 0);
    }
    inline void UnityDisplayManager_DisplayRenderingBuffers(void*, RenderSurfaceBase** color, RenderSurfaceBase** depth)
    {
        *color = *depth = 0;
    }

#endif
