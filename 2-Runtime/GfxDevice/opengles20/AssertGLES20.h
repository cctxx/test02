#ifndef GLES_ASSERTGLES20_H
#define GLES_ASSERTGLES20_H

#include "Runtime/Utilities/LogAssert.h"

void CheckOpenGLES2Error (const char *prefix, const char* file, long line);

#if !UNITY_RELEASE
	#ifndef GLESAssert
	/// Asserts for checking the OpenGL error state
	#define GLESAssert()						{ CheckOpenGLES2Error (NULL,  __FILE__, __LINE__); }
	#endif
	#define GLESAssertString(x)					{ CheckOpenGLES2Error (x, __FILE__, __LINE__); }
	#define GLES_CHK(x)							do { {x;} GLESAssert(); } while(0)
#else

	#ifndef GLESAssert
	#define GLESAssert()
	#endif
	#define GLESAssertString(x)
	#define GLES_CHK(x)							x
#endif


//#define GLES_CHK(x)							do {} while(0)
//#define GLES_CHK(x)
//#define GLES_CHK(x)							do { printf_console("GLES: %s %d\n", __FILE__, __LINE__); } while(0)

#endif
