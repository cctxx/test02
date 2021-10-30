#ifndef GLES_ASSERTGLES30_H
#define GLES_ASSERTGLES30_H

#include "Runtime/Utilities/LogAssert.h"

void CheckOpenGLES3Error (const char *prefix, const char* file, long line);

#if !UNITY_RELEASE || UNITY_WEBGL
	#ifndef GLESAssert
	/// Asserts for checking the OpenGL error state
	#define GLESAssert()						{ CheckOpenGLES3Error (NULL,  __FILE__, __LINE__); }
	#endif
	#define GLESAssertString(x)					{ CheckOpenGLES3Error (x, __FILE__, __LINE__); }

	#define GLES30_PRINT_GL_TRACE 0
	#if GLES30_PRINT_GL_TRACE
		#define GLESCHKSTRINGIFY(x) GLESCHKSTRINGIFY2(x)
		#define GLESCHKSTRINGIFY2(x) #x

		#define GLES_CHK(x)							do { {printf_console("GLES: %s %s %d\n", GLESCHKSTRINGIFY(x), __FILE__, __LINE__); x;} GLESAssert(); } while(0)
	#else
		#define GLES_CHK(x)							do { {x;} GLESAssert(); } while(0)
	#endif
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
