#pragma once

#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

#if UNITY_LINUX
#include <X11/Xlib.h>
#include <GLES3/gl2.h>
#endif

#if UNITY_BB10
#include <GLES3/gl2.h>
#include <screen/screen.h>
#endif

#if UNITY_WIN || UNITY_LINUX || UNITY_BB10 || UNITY_ANDROID
bool InitializeGLES30	();
void ShutdownGLES30		();
bool IsContextGLES30Created();
#if UNITY_WIN
bool CreateContextGLES30(HWND hWnd);
#elif UNITY_LINUX
bool CreateContextGLES30(Window window);
#elif UNITY_BB10
bool CreateContextGLES30(screen_window_t window);
void ResizeContextGLES30(screen_window_t window, int width, int height);
#endif
void DestroyContextGLES30();
#endif
void PresentContextGLES();
void PresentContextGLES30();

void ReleaseGLES30Context();

void AcquireGLES30Context();


