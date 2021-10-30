#ifndef GLASSERT_H
#define GLASSERT_H

#include "Runtime/Utilities/LogAssert.h"

void CheckOpenGLError (const char *prefix, const char* file, long line);

#if !UNITY_RELEASE
	#ifndef GLAssert
	/// Asserts for checking the OpenGL error state
	#define GLAssert() 				{ CheckOpenGLError (NULL,  __FILE__, __LINE__); }
	#endif
	#define GLAssertString(x) 		{ CheckOpenGLError (x, __FILE__, __LINE__); }
	#define GL_CHK(x)							do { {x;} GLAssert(); } while(0)
#else

	#ifndef GLAssert
	#define GLAssert()
	#endif
	#define GLAssertString(x)
	#define GL_CHK(x)							x

#endif

#endif
