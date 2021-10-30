#include "UnityPrefix.h"
#include "IncludesGLES30.h"
#include "UnityGLES30Ext.h"
#include "Runtime/GfxDevice/opengles/ExtensionsGLES.h"

void Gles3ExtFunc::InitExtFunc()
{
	glPushGroupMarkerEXT					= (glPushGroupMarkerEXTFunc)GetGLExtProcAddress("glPushGroupMarkerEXT");
	glPopGroupMarkerEXT						= (glPopGroupMarkerEXTFunc)GetGLExtProcAddress("glPopGroupMarkerEXT");

	glAlphaFuncQCOM							= (glAlphaFuncQCOMFunc)GetGLExtProcAddress("glAlphaFuncQCOM");
}

Gles3ExtFunc gGles3ExtFunc;
