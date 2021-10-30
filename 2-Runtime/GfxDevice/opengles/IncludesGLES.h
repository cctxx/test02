#pragma once

#ifdef UNITYGL_H
#	error "Don't Mix with UnityGL!!!"
#endif

#define GFX_SUPPORTS_OPENGLESXX (GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30)

#if GFX_SUPPORTS_OPENGLESXX

#define GFX_SUPPORTS_EGL (UNITY_WIN || UNITY_LINUX || UNITY_ANDROID || UNITY_TIZEN || UNITY_BB10)

#if UNITY_ANDROID || UNITY_WEBGL
#	define GL_GLEXT_PROTOTYPES
#endif

#if GFX_SUPPORTS_OPENGLES30
#	if UNITY_WIN
#		include "PlatformDependent/WinPlayer/unityes_egl.h"
#		include "PlatformDependent/WinPlayer/unityes_gl2.h"
		// \todo [pyry] Remove gl2ext include since most of the extensions are now in core
#		include "PlatformDependent/WinPlayer/unityes_gl2ext.h"
#		include "PlatformDependent/WinPlayer/unityes_gl3.h"
#		define INCLUDE_GLES_2X 1
#		define INCLUDE_GLES_3X 1
#		define DEF(ret,name,args) extern ret (WINAPI *name) args
#		include "PlatformDependent/WinPlayer/GLESFunctionDefs.h"
#	elif UNITY_ANDROID
#		include <EGL/egl.h>
#		include "PlatformDependent/AndroidPlayer/unityes_gl3.h"
		// \todo [pyry] Remove gl2ext include since most of the extensions are now in core
#		include <GLES2/gl2ext.h>
#		include "Runtime/GfxDevice/opengles20/UnityGLES20Ext.h"
#   else
#		error "Unknown platform"
#	endif
#
#   include "Runtime/GfxDevice/opengles30//UnityGLES30Ext.h"

#elif GFX_SUPPORTS_OPENGLES20
#	if UNITY_WIN
#		include "PlatformDependent/WinPlayer/unityes_egl.h"
#		include "PlatformDependent/WinPlayer/unityes_gl2.h"
#		include "PlatformDependent/WinPlayer/unityes_gl2ext.h"
#		define INCLUDE_GLES_2X 1
#		define DEF(ret,name,args) extern ret (WINAPI *name) args
#		include "PlatformDependent/WinPlayer/GLESFunctionDefs.h"
#	elif UNITY_IPHONE
#		include <OpenGLES/ES2/gl.h>
#		include <OpenGLES/ES2/glext.h>
#	elif UNITY_LINUX || UNITY_ANDROID || UNITY_BB10
#		include <EGL/egl.h>
#		include <GLES2/gl2.h>
#		include <GLES2/gl2ext.h>
#	elif UNITY_PEPPER || UNITY_WEBGL
#		include <GLES2/gl2.h>
#		include <GLES2/gl2ext.h>
#	elif UNITY_TIZEN
#		include <FGraphics.h>
#		include <FGraphicsOpengl2.h>
using namespace Tizen::Graphics;
using namespace Tizen::Graphics::Opengl;
#	else
#		error "Unknown platform"
#	endif
#
#   include "Runtime/GfxDevice/opengles20/UnityGLES20Ext.h"
#
#endif

#endif // GFX_SUPPORTS_OPENGLESXX