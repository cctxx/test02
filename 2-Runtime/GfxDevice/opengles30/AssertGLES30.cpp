#include "UnityPrefix.h"
#include "AssertGLES30.h"
#include "IncludesGLES30.h"

#if GFX_SUPPORTS_OPENGLES30

using namespace std;

static const char* GetErrorString(GLenum glerr)
{
	// Error descriptions taken from OpenGLES 1.1 specification (page 13)
	switch(glerr)
	{
	case GL_NO_ERROR:
		return "GL_NO_ERROR: No error occured";
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM: enum argument out of range";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE: Numeric argument out of range";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION: Operation illegal in current state";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY: Not enough memory left to execute command";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "GL_INVALID_FRAMEBUFFER_OPERATION: Framebuffer is not complete or incompatible with command";
	default:
#if UNTIY_WEBGL		
		printf_console("AssertGles20::GetErrorString invoked for unknown error %d",glerr);
#endif		
		return "Unknown error";
	}
}

void CheckOpenGLES3Error (const char *prefix, const char* file, long line) 
{
	const int kMaxErrors = 10;
	int counter = 0;

	GLenum glerr;
	while( (glerr = glGetError ()) != GL_NO_ERROR )
	{
		string errorString(GetErrorString(glerr));
		
		if (prefix) 
			errorString = string(prefix) + ": " + errorString;
			
		DebugStringToFile (errorString.c_str(), 0, file, line, kAssert);
	
		++counter;
		if( counter > kMaxErrors )
		{
			printf_console( "GLES: error count exceeds %i, stop reporting errors\n", kMaxErrors );
			return;
		}
	}
}

#endif // GFX_SUPPORTS_OPENGLES30
