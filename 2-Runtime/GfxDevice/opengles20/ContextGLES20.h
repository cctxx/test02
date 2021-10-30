#pragma once

#include "Runtime/GfxDevice/GfxDeviceObjects.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

#if UNITY_LINUX
#include <X11/Xlib.h>
#include <GLES2/gl2.h>
#endif

#if UNITY_BB10
#include <GLES2/gl2.h>
#include <screen/screen.h>
#endif

#if UNITY_WIN || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN || UNITY_ANDROID
bool InitializeGLES20	();
void ShutdownGLES20		();
bool IsContextGLES20Created();
#if UNITY_WIN
bool CreateContextGLES20(HWND hWnd);
#elif UNITY_LINUX
bool CreateContextGLES20(Window window);
#elif UNITY_BB10
bool CreateContextGLES20(screen_window_t window);
void ResizeContextGLES20(screen_window_t window, int width, int height);
void AdjustVsync(int val);
#elif UNITY_TIZEN
bool CreateContextGLES20();
void ResizeContextGLES20(int width, int height);
#endif
#if !UNITY_ANDROID
void DestroyContextGLES20();
#endif
#endif
void PresentContextGLES();
void PresentContextGLES20();

#if UNITY_ANDROID
void ReleaseGLES20Context();

void AcquireGLES20Context();
#endif
