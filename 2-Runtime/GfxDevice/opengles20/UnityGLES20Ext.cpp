#include "UnityPrefix.h"
#include "IncludesGLES20.h"
#include "UnityGLES20Ext.h"
#include "Runtime/GfxDevice/opengles/ExtensionsGLES.h"

void GlesExtFunc::InitExtFunc()
{
	glPushGroupMarkerEXT					= (glPushGroupMarkerEXTFunc)GetGLExtProcAddress("glPushGroupMarkerEXT");
	glPopGroupMarkerEXT						= (glPopGroupMarkerEXTFunc)GetGLExtProcAddress("glPopGroupMarkerEXT");
	glDiscardFramebufferEXT					= (glDiscardFramebufferEXTFunc)GetGLExtProcAddress("glDiscardFramebufferEXT");
	glGenQueriesEXT							= (glGenQueriesEXTFunc)GetGLExtProcAddress("glGenQueriesEXT");
	glDeleteQueriesEXT						= (glDeleteQueriesEXTFunc)GetGLExtProcAddress("glDeleteQueriesEXT");
	glGetQueryObjectuivEXT					= (glGetQueryObjectuivEXTFunc)GetGLExtProcAddress("glGetQueryObjectuivEXT");

	glGetProgramBinaryOES					= (glGetProgramBinaryOESFunc)GetGLExtProcAddress("glGetProgramBinaryOES");
	glProgramBinaryOES						= (glProgramBinaryOESFunc)GetGLExtProcAddress("glProgramBinaryOES");

	glMapBufferOES							= (glMapBufferOESFunc)GetGLExtProcAddress("glMapBufferOES");
	glUnmapBufferOES						= (glUnmapBufferOESFunc)GetGLExtProcAddress("glUnmapBufferOES");

	glMapBufferRangeEXT						= (glMapBufferRangeEXTFunc)GetGLExtProcAddress("glMapBufferRangeEXT");
	glFlushMappedBufferRangeEXT				= (glFlushMappedBufferRangeEXTFunc)GetGLExtProcAddress("glFlushMappedBufferRangeEXT");

	glRenderbufferStorageMultisampleAPPLE	= (glRenderbufferStorageMultisampleAPPLEFunc)GetGLExtProcAddress("glRenderbufferStorageMultisampleAPPLE");
	glResolveMultisampleFramebufferAPPLE	= (glResolveMultisampleFramebufferAPPLEFunc)GetGLExtProcAddress("glResolveMultisampleFramebufferAPPLE");

	glRenderbufferStorageMultisampleIMG		= (glRenderbufferStorageMultisampleIMGFunc)GetGLExtProcAddress("glRenderbufferStorageMultisampleIMG");
	glFramebufferTexture2DMultisampleIMG	= (glFramebufferTexture2DMultisampleIMGFunc)GetGLExtProcAddress("glFramebufferTexture2DMultisampleIMG");

    glRenderbufferStorageMultisampleEXT		= (glRenderbufferStorageMultisampleEXTFunc)GetGLExtProcAddress("glRenderbufferStorageMultisampleEXT");
	glFramebufferTexture2DMultisampleEXT	= (glFramebufferTexture2DMultisampleEXTFunc)GetGLExtProcAddress("glFramebufferTexture2DMultisampleEXT");

	glDrawBuffersNV							= (glDrawBuffersNVFunc)GetGLExtProcAddress("glDrawBuffersNV");
	glQueryCounterNV						= (glQueryCounterNVFunc)GetGLExtProcAddress("glQueryCounterNV");
	glGetQueryObjectui64vNV					= (glGetQueryObjectui64vNVFunc)GetGLExtProcAddress("glGetQueryObjectui64vNV");

	glAlphaFuncQCOM							= (glAlphaFuncQCOMFunc)GetGLExtProcAddress("glAlphaFuncQCOM");
}

GlesExtFunc gGlesExtFunc;
