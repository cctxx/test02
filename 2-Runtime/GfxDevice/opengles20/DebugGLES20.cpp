#include "UnityPrefix.h"
#include "IncludesGLES20.h"
#include "DebugGLES20.h"
#include "AssertGLES20.h"


void DumpVertexArrayStateGLES20()
{
#if GFX_SUPPORTS_OPENGLES20
	GLint maxVertexAttribs = 0;
	GLES_CHK(glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs));
	
	GLint vbo = 0;
	GLint ibo = 0;
	GLES_CHK(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vbo));
	GLES_CHK(glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ibo));
	
	printf_console("---> VertexArray State: vbo:%d ibo:%d\n", vbo, ibo);
	
	for (int q = 0; q < maxVertexAttribs; ++q)
	{
		int enabled, size, stride, normalized, type, vbo;
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled));
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size));
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride));
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized));
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type));
		GLES_CHK(glGetVertexAttribiv(q, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo));
		
		GLvoid* ptr;
		GLES_CHK(glGetVertexAttribPointerv(q, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr));
		
		printf_console("       attr[%d] --- %s type:%d size:%d stride:%d norm:%d vbo:%d, %p\n",
					   q, enabled? "On ": "Off",
					   type, size, stride, normalized, vbo, ptr);
	}
#endif				  
}
