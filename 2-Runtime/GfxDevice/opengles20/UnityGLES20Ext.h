#pragma once
#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/opengles/ExtensionsGLES.h"

// The reason to have this file is that we use plenty of gles extensions,
//   and not all of them are handled in (all) sdk we build with.
// Still we do guard all the usages with runtime ifs, so there is no need to intro addidtional preprocessor magic


// ----------------------------------------------------------------------------
// Texture Formats
//

#ifndef GL_BGRA_EXT
	#define GL_BGRA_EXT								0x80E1
#endif
#ifndef GL_ETC1_RGB8_OES
	#define GL_ETC1_RGB8_OES						0x8D64
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
	#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT			0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
	#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT		0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
	#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT		0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
	#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT		0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_NV
	#define GL_COMPRESSED_SRGB_S3TC_DXT1_NV			0x8C4C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV	0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV	0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV
	#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV	0x8C4F
#endif
#ifndef GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
	#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG		0x8C00
#endif
#ifndef GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
	#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG		0x8C01
#endif
#ifndef GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
	#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG		0x8C02
#endif
#ifndef GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
	#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG		0x8C03
#endif
#ifndef GL_ATC_RGB_AMD
	#define GL_ATC_RGB_AMD							0x8C92
#endif
#ifndef GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD
	#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD		0x87EE
#endif

#ifndef GL_HALF_FLOAT_OES
	#define GL_HALF_FLOAT_OES						0x8D61
#endif
#ifndef GL_SRGB_EXT
	#define GL_SRGB_EXT								0x8C40
#endif
#ifndef GL_SRGB_ALPHA_EXT
	#define GL_SRGB_ALPHA_EXT						0x8C42
#endif


// ----------------------------------------------------------------------------
// GL_EXT_blend_minmax
//

#ifndef GL_MIN_EXT
	#define GL_MIN_EXT								0x8007
#endif

#ifndef GL_MAX_EXT
	#define GL_MAX_EXT								0x8008
#endif

// ----------------------------------------------------------------------------
// GL_EXT_debug_marker
//

typedef void (*glPushGroupMarkerEXTFunc)(int len, const char* name);
typedef void (*glPopGroupMarkerEXTFunc)();


// ----------------------------------------------------------------------------
// GL_EXT_discard_framebuffer
//

#ifndef GL_COLOR_EXT
	#define GL_COLOR_EXT							0x1800
#endif
#ifndef GL_DEPTH_EXT
	#define GL_DEPTH_EXT							0x1801
#endif
#ifndef GL_STENCIL_EXT
	#define GL_STENCIL_EXT							0x1802
#endif

typedef void (*glDiscardFramebufferEXTFunc)(GLenum target, GLsizei numAttachments, const GLenum *attachments);


// ----------------------------------------------------------------------------
// GL_EXT_occlusion_query_boolean
// Note: while we dont use occlusion queries, all queries ext are based on that one
//

#ifndef GL_QUERY_RESULT_EXT
	#define GL_QUERY_RESULT_EXT						0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE_EXT
	#define GL_QUERY_RESULT_AVAILABLE_EXT			0x8867
#endif

typedef void (*glGenQueriesEXTFunc)(GLuint n, GLuint *ids);
typedef void (*glDeleteQueriesEXTFunc)(GLuint n, const GLuint *ids);
typedef void (*glBeginQueryEXTFunc)(GLuint target, GLuint id);
typedef void (*glEndQueryEXTFunc)(GLuint target);
typedef void (*glGetQueryObjectuivEXTFunc)(GLuint id, GLuint pname, GLuint *params);


// ----------------------------------------------------------------------------
// GL_EXT_shadow_samplers
//

#ifndef GL_TEXTURE_COMPARE_MODE_EXT
	#define GL_TEXTURE_COMPARE_MODE_EXT				0x884C
#endif
#ifndef GL_TEXTURE_COMPARE_FUNC_EXT
	#define GL_TEXTURE_COMPARE_FUNC_EXT				0x884D
#endif
#ifndef GL_COMPARE_REF_TO_TEXTURE_EXT
	#define GL_COMPARE_REF_TO_TEXTURE_EXT			0x884E
#endif
#ifndef GL_SAMPLER_2D_SHADOW_EXT
	#define GL_SAMPLER_2D_SHADOW_EXT				0x8B62
#endif


// ----------------------------------------------------------------------------
// GL_EXT_texture_rg
//

#ifndef GL_RED_EXT
	#define GL_RED_EXT									0x1903
#endif
#ifndef GL_RG_EXT
	#define GL_RG_EXT									0x8227
#endif


// ----------------------------------------------------------------------------
// GL_OES_get_program_binary
//

#ifndef GL_PROGRAM_BINARY_LENGTH_OES
	#define GL_PROGRAM_BINARY_LENGTH_OES			0x8741
#endif

#ifndef GL_NUM_PROGRAM_BINARY_FORMATS_OES
	#define GL_NUM_PROGRAM_BINARY_FORMATS_OES		0x87FE
#endif

typedef void (*glGetProgramBinaryOESFunc)(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, GLvoid* binary);
typedef void (*glProgramBinaryOESFunc)(GLuint program, GLenum binaryFormat, const GLvoid *binary, GLint length);


// ----------------------------------------------------------------------------
// GL_OES_mapbuffer
//

#ifndef GL_WRITE_ONLY_OES
	#define GL_WRITE_ONLY_OES						0x88B9
#endif

typedef void*		(*glMapBufferOESFunc)(GLenum target, GLenum access);
typedef GLboolean	(*glUnmapBufferOESFunc)(GLenum target);


// ----------------------------------------------------------------------------
// GL_EXT_map_buffer_range
//

#ifndef GL_MAP_READ_BIT_EXT
	#define GL_MAP_READ_BIT_EXT						0x0001
#endif
#ifndef GL_MAP_WRITE_BIT_EXT
	#define GL_MAP_WRITE_BIT_EXT					0x0002
#endif
#ifndef GL_MAP_INVALIDATE_RANGE_BIT_EXT
	#define GL_MAP_INVALIDATE_RANGE_BIT_EXT			0x0004
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT_EXT
	#define GL_MAP_INVALIDATE_BUFFER_BIT_EXT		0x0008
#endif
#ifndef GL_MAP_FLUSH_EXPLICIT_BIT_EXT
	#define GL_MAP_FLUSH_EXPLICIT_BIT_EXT			0x0010
#endif
#ifndef GL_MAP_UNSYNCHRONIZED_BIT_EXT
	#define GL_MAP_UNSYNCHRONIZED_BIT_EXT			0x0020
#endif

typedef void*		(*glMapBufferRangeEXTFunc)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void 		(*glFlushMappedBufferRangeEXTFunc)(GLenum target, GLintptr offset, GLsizeiptr length);



// ----------------------------------------------------------------------------
// GL_APPLE_framebuffer_multisample
//

#ifndef GL_MAX_SAMPLES_APPLE
	#define GL_MAX_SAMPLES_APPLE					0x8D57
#endif
#ifndef GL_READ_FRAMEBUFFER_APPLE
	#define GL_READ_FRAMEBUFFER_APPLE				0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER_APPLE
	#define GL_DRAW_FRAMEBUFFER_APPLE				0x8CA9
#endif


typedef void (*glRenderbufferStorageMultisampleAPPLEFunc)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*glResolveMultisampleFramebufferAPPLEFunc)(void);


// ----------------------------------------------------------------------------
// GL_IMG_multisampled_render_to_texture
//

#ifndef GL_MAX_SAMPLES_IMG
	#define GL_MAX_SAMPLES_IMG						0x9135
#endif

typedef void (*glRenderbufferStorageMultisampleIMGFunc)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*glFramebufferTexture2DMultisampleIMGFunc)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);


// ----------------------------------------------------------------------------
// GL_EXT_multisampled_render_to_texture
//

#ifndef GL_MAX_SAMPLES_EXT
#define GL_MAX_SAMPLES_EXT						0x8D57
#endif

typedef void (*glRenderbufferStorageMultisampleEXTFunc)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*glFramebufferTexture2DMultisampleEXTFunc)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);


// ----------------------------------------------------------------------------
// GL_NV_draw_buffers
//

#ifndef GL_MAX_DRAW_BUFFERS_NV
	#define GL_MAX_DRAW_BUFFERS_NV					0x8824
#endif
#ifndef GL_COLOR_ATTACHMENT0_NV
	#define GL_COLOR_ATTACHMENT0_NV					0x8CE0
#endif

typedef void (*glDrawBuffersNVFunc)(int len, const GLenum* bufs);


// ----------------------------------------------------------------------------
// GL_NV_timer_query
//

#ifndef GL_TIME_ELAPSED_NV
	#define GL_TIME_ELAPSED_NV						0x88BF
#endif
#ifndef GL_TIMESTAMP_NV
	#define GL_TIMESTAMP_NV							0x8E28
#endif

typedef unsigned long long int	EGLuint64NV;

typedef void (*glQueryCounterNVFunc)(GLuint target, GLuint id);
typedef void (*glGetQueryObjectui64vNVFunc)(GLuint id, GLuint pname, EGLuint64NV *params);


// ----------------------------------------------------------------------------
// GL_QCOM_alpha_test
//

#ifndef GL_ALPHA_TEST_QCOM
	#define GL_ALPHA_TEST_QCOM						0x0BC0
#endif

typedef void (*glAlphaFuncQCOMFunc)(GLenum func, GLfloat ref);


// ----------------------------------------------------------------------------
// common place to get function pointers
//

struct
GlesExtFunc
{
	glPushGroupMarkerEXTFunc					glPushGroupMarkerEXT;
	glPopGroupMarkerEXTFunc						glPopGroupMarkerEXT;
	glDiscardFramebufferEXTFunc					glDiscardFramebufferEXT;
	glGenQueriesEXTFunc							glGenQueriesEXT;
	glDeleteQueriesEXTFunc						glDeleteQueriesEXT;
	glGetQueryObjectuivEXTFunc					glGetQueryObjectuivEXT;

	glGetProgramBinaryOESFunc					glGetProgramBinaryOES;
	glProgramBinaryOESFunc						glProgramBinaryOES;

	glMapBufferOESFunc							glMapBufferOES;
	glUnmapBufferOESFunc						glUnmapBufferOES;

	glMapBufferRangeEXTFunc						glMapBufferRangeEXT;
	glFlushMappedBufferRangeEXTFunc				glFlushMappedBufferRangeEXT;


	glRenderbufferStorageMultisampleAPPLEFunc	glRenderbufferStorageMultisampleAPPLE;
	glResolveMultisampleFramebufferAPPLEFunc	glResolveMultisampleFramebufferAPPLE;

	glRenderbufferStorageMultisampleIMGFunc		glRenderbufferStorageMultisampleIMG;
	glFramebufferTexture2DMultisampleIMGFunc	glFramebufferTexture2DMultisampleIMG;

    glRenderbufferStorageMultisampleEXTFunc		glRenderbufferStorageMultisampleEXT;
	glFramebufferTexture2DMultisampleEXTFunc	glFramebufferTexture2DMultisampleEXT;

	glDrawBuffersNVFunc							glDrawBuffersNV;
	glQueryCounterNVFunc						glQueryCounterNV;
	glGetQueryObjectui64vNVFunc					glGetQueryObjectui64vNV;

	glAlphaFuncQCOMFunc							glAlphaFuncQCOM;

	void InitExtFunc();
};
extern GlesExtFunc gGlesExtFunc;
