#include "UnityPrefix.h"
#include "GLAssert.h"
#include "UnityGL.h"

#if UNITY_WIN || UNITY_LINUX
#include <GL/glu.h>
#else
#include <OpenGL/glu.h>
#endif

using namespace std;

void CheckOpenGLError (const char *prefix, const char* file, long line) 
{
	const int kMaxErrors = 10;
	int counter = 0;

	GLenum glerr;
	while( (glerr = glGetError ()) != GL_NO_ERROR )
	{
		if (prefix) 
		{
			string errorString = prefix;
			errorString += ": ";
			const char* gluMsg = reinterpret_cast<const char*>(gluErrorString (glerr));
			errorString += gluMsg ? gluMsg : Format("unknown error 0x%x", glerr);
			DebugStringToFile (errorString.c_str(), 0, file, line, kAssert);
		} 
		else
		{
			const char* gluMsg = reinterpret_cast<const char*>(gluErrorString (glerr));
			string errorString = gluMsg ? gluMsg : Format("unknown error 0x%x", glerr);
			DebugStringToFile (errorString.c_str(), 0, file, line, kAssert);
		}
		++counter;
		if( counter > kMaxErrors )
		{
			printf_console( "GL: error count exceeds %i, stop reporting errors\n", kMaxErrors );
			return;
		}
	}
}
