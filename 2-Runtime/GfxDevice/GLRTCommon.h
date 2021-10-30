#ifndef GL_RT_COMMON_H
#define GL_RT_COMMON_H

// internal header
// common RT-related stuff
// as we might need both gles and gles20 linked we a forced to use external defines here

#ifndef GL_RT_COMMON_GLES
#define GL_RT_COMMON_GLES 0
#endif

#ifndef GL_RT_COMMON_GLES2
#define GL_RT_COMMON_GLES2 0
#endif

#ifndef GL_RT_COMMON_GL
#define GL_RT_COMMON_GL 0
#endif

#if GL_RT_COMMON_GLES==0 && GL_RT_COMMON_GLES2==0 && GL_RT_COMMON_GL==0
#error dont include this header without specifying api used
#endif


#if GL_RT_COMMON_GLES
#define CHECK GL_CHK
#define EXT_CALL(f) f##OES
#define EXT_ENUM(x) x##_OES
#define MANGLE_NAME(f) f##GLES
#elif GL_RT_COMMON_GLES2
#define CHECK GLES_CHK
#define EXT_CALL(f) f
#define EXT_ENUM(x) x
#define MANGLE_NAME(f) f##GLES2
#elif GL_RT_COMMON_GL
#define CHECK GL_CHK
#define EXT_CALL(f) f##EXT
#define EXT_ENUM(x) x##_EXT
#define MANGLE_NAME(f) f##GL
#endif


//==============================================================================
// rt format handling
//==============================================================================


inline GLenum MANGLE_NAME(RTColorTextureFormat)(RenderTextureFormat fmt)
{
	switch( fmt )
	{
		case kRTFormatARGB32:
		case kRTFormatARGB4444:
		case kRTFormatARGB1555:
		case kRTFormatARGBHalf:
		case kRTFormatARGBFloat:
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
			return GL_RGBA;
#else
			return GL_BGRA;
#endif

		case kRTFormatRGB565:
			return GL_RGB;

		case kRTFormatR8:
		case kRTFormatRHalf:
		case kRTFormatRFloat:
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
			return 0x1903; // GL_RED_EXT
#else
			return GL_RED;
#endif

		case kRTFormatRGHalf:
		case kRTFormatRGFloat:
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
			return 0x8227; // GL_RG_EXT
#else
			return 0x8227; // GL_RG
#endif

		// int formats are not supported
		case kRTFormatARGBInt:
		case kRTFormatRInt:
		case kRTFormatRGInt:
			break;

		default:
			break;
	}

	Assert( false && "wrong color rt format" );
	return 0;
}


inline GLenum MANGLE_NAME(RTColorInternalFormat)(RenderTextureFormat fmt)
{
	switch( fmt )
	{
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
		case kRTFormatARGB32:		return GL_RGBA;
		case kRTFormatARGB4444:		return GL_RGBA;
		case kRTFormatARGB1555:		return GL_RGBA;
		case kRTFormatRGB565:		return GL_RGB;
		case kRTFormatARGBHalf:		return GL_RGBA;
		case kRTFormatARGBFloat:	return GL_RGBA;
		case kRTFormatR8:			return 0x1903; // GL_RED_EXT
		case kRTFormatRHalf:		return 0x1903; // GL_RED_EXT
		case kRTFormatRFloat:		return 0x1903; // GL_RED_EXT
		case kRTFormatRGHalf:		return 0x8227; // GL_RG_EXT
		case kRTFormatRGFloat:		return 0x8227; // GL_RG_EXT
#else
		case kRTFormatARGB32:		return GL_RGBA;
		case kRTFormatARGB4444:		return GL_RGBA4;
		case kRTFormatARGB1555:		return GL_RGB5_A1;
		case kRTFormatRGB565:		return GL_RGB5;
		case kRTFormatARGBHalf:		return GL_RGBA16F_ARB;
		case kRTFormatARGBFloat:	return GL_RGBA32F_ARB;
		case kRTFormatR8:			return 0x8229; // GL_R8
		case kRTFormatRHalf:		return 0x822D; // GL_R16F
		case kRTFormatRFloat:		return 0x822E; // GL_R32F
		case kRTFormatRGHalf:		return 0x822F; // GL_RG16F
		case kRTFormatRGFloat:		return 0x8230; // GL_RG32F
#endif

		case kRTFormatARGBInt:		break;
		case kRTFormatRInt:			break;
		case kRTFormatRGInt:		break;

		default:
			break;
	}

	Assert( false && "wrong color rt format" );
	return 0;
}

inline GLenum MANGLE_NAME(RBColorInternalFormat)(RenderTextureFormat fmt)
{
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
	switch( fmt )
	{
		case kRTFormatARGB32:		return GL_RGBA8_OES;
		case kRTFormatRGB565:		return EXT_ENUM(GL_RGB565);
		default:					break;
	}

	Assert(false && "wrong color rb format");
	return 0;
#else
	return RTColorTextureFormatGL(fmt);
#endif
}


inline GLenum MANGLE_NAME(RTColorTextureFormatSRGB)(RenderTextureFormat fmt)
{
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
	switch( fmt )
	{
		case kRTFormatARGB32:	return 0x8C42; // GL_SRGB_ALPHA_EXT
		default:				break;
	}
	return MANGLE_NAME(RTColorTextureFormat)(fmt);
#else
	return RTColorTextureFormatGL(fmt);
#endif
}


inline GLenum MANGLE_NAME(RTColorInternalFormatSRGB)(RenderTextureFormat fmt)
{
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
	switch( fmt )
	{
		case kRTFormatARGB32:	return 0x8C42; // GL_SRGB_ALPHA_EXT
		default:				break;
	}
	return MANGLE_NAME(RTColorInternalFormat)(fmt);
#else
	switch( fmt )
	{
		case kRTFormatARGB32:	return GL_SRGB8_ALPHA8_EXT;
		default:				break;
	}
	return RTColorInternalFormatGL(fmt);
#endif

	Assert( false && "wrong color rt format" );
	return 0;
}

inline GLenum MANGLE_NAME(RTColorTextureType)(RenderTextureFormat fmt)
{
	switch( fmt )
	{
		case kRTFormatARGB32:	return GL_UNSIGNED_BYTE;
		case kRTFormatARGB4444:	return GL_UNSIGNED_SHORT_4_4_4_4;
		case kRTFormatARGB1555:	return GL_UNSIGNED_SHORT_5_5_5_1;
		case kRTFormatRGB565:	return GL_UNSIGNED_SHORT_5_6_5;

		case kRTFormatR8:		return GL_UNSIGNED_BYTE;

		case kRTFormatARGBHalf:
		case kRTFormatRHalf:
		case kRTFormatRGHalf:
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2
			return 0x8D61; // GL_HALF_FLOAT_OES;
#else
			return GL_HALF_FLOAT_ARB;
#endif

		case kRTFormatARGBFloat:
		case kRTFormatRFloat:
		case kRTFormatRGFloat:
			return GL_FLOAT;

		case kRTFormatARGBInt:
		case kRTFormatRInt:
		case kRTFormatRGInt:
			break;

		default:
			break;
	}

	Assert( false && "wrong color rt format" );
	return 0;
}


//==============================================================================
// rt format support
//==============================================================================


#if GL_RT_COMMON_GLES
#define DEPTH_ENUM GL_DEPTH_COMPONENT16_OES
#elif GL_RT_COMMON_GLES2
#define DEPTH_ENUM GL_DEPTH_COMPONENT16
#elif GL_RT_COMMON_GL
#define DEPTH_ENUM GL_DEPTH_COMPONENT
#endif

struct MANGLE_NAME(FBColorFormatChecker)
{
	GLint   oldFB;
	GLint   oldRB;

	GLuint fb;
	GLuint depth;
	GLuint color;

	static const int FBExt = 8;

	MANGLE_NAME(FBColorFormatChecker)()
	{
	#if UNITY_IPHONE
		CHECK(glGetIntegerv(EXT_ENUM(GL_FRAMEBUFFER_BINDING), &oldFB));
		CHECK(glGetIntegerv(EXT_ENUM(GL_RENDERBUFFER_BINDING), &oldRB));
	#else
		oldFB = oldRB = 0;
	#endif

		CHECK(EXT_CALL(glGenFramebuffers)(1, &fb));
		CHECK(EXT_CALL(glBindFramebuffer)(EXT_ENUM(GL_FRAMEBUFFER), fb));

		CHECK(EXT_CALL(glGenRenderbuffers)(1, &depth));
		CHECK(EXT_CALL(glBindRenderbuffer)(EXT_ENUM(GL_RENDERBUFFER), depth));
		CHECK(EXT_CALL(glRenderbufferStorage)(EXT_ENUM(GL_RENDERBUFFER), DEPTH_ENUM, FBExt, FBExt));

		CHECK(glGenTextures(1, &color));
	}

	~MANGLE_NAME(FBColorFormatChecker)()
	{
		// gives out warning under emu
	#if !UNITY_GLES_EMU
		CHECK(EXT_CALL(glFramebufferTexture2D)(EXT_ENUM(GL_FRAMEBUFFER), EXT_ENUM(GL_COLOR_ATTACHMENT0), GL_TEXTURE_2D, 0, 0));
		CHECK(EXT_CALL(glFramebufferRenderbuffer)(EXT_ENUM(GL_FRAMEBUFFER), EXT_ENUM(GL_DEPTH_ATTACHMENT), EXT_ENUM(GL_RENDERBUFFER), 0));
	#endif

		CHECK(EXT_CALL(glBindFramebuffer)(EXT_ENUM(GL_FRAMEBUFFER), oldFB));
		CHECK(EXT_CALL(glDeleteFramebuffers)(1, &fb));

		CHECK(EXT_CALL(glBindRenderbuffer)(EXT_ENUM(GL_RENDERBUFFER), oldRB));
		CHECK(EXT_CALL(glDeleteRenderbuffers)(1, &depth));

		CHECK(glDeleteTextures(1, &color));
	}

	bool CheckFormatSupported(GLint internalFormat, GLenum format, GLenum type)
	{
	#if !GL_RT_COMMON_GLES2
		CHECK(glEnable(GL_TEXTURE_2D));
	#endif
		CHECK(glBindTexture(GL_TEXTURE_2D, color));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

	#if GL_RT_COMMON_GL
		CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0));
	#endif

		CHECK(glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, FBExt, FBExt, 0, format, type, 0));

		CHECK(EXT_CALL(glFramebufferTexture2D)(EXT_ENUM(GL_FRAMEBUFFER), EXT_ENUM(GL_COLOR_ATTACHMENT0), GL_TEXTURE_2D, color, 0));
		CHECK(EXT_CALL(glFramebufferRenderbuffer)(EXT_ENUM(GL_FRAMEBUFFER), EXT_ENUM(GL_DEPTH_ATTACHMENT), EXT_ENUM(GL_RENDERBUFFER), depth));

		GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_ENUM(GL_FRAMEBUFFER));

		CHECK(glBindTexture(GL_TEXTURE_2D, 0));

		return (status == EXT_ENUM(GL_FRAMEBUFFER_COMPLETE));
	}
};


//==============================================================================
// fb/rt format query
//==============================================================================

// for now
#if GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2

inline RenderTextureFormat MANGLE_NAME(QueryFBColorFormat)()
{
	GLint rbits=0, gbits=0, bbits=0, abits=0;
	CHECK(glGetIntegerv(GL_RED_BITS,   &rbits));
	CHECK(glGetIntegerv(GL_GREEN_BITS, &gbits));
	CHECK(glGetIntegerv(GL_BLUE_BITS,  &bbits));
	CHECK(glGetIntegerv(GL_ALPHA_BITS, &abits));

	if(rbits==8 && gbits==8 && bbits==8 && abits==8)
		return kRTFormatARGB32;
	else if(rbits==4 && gbits==4 && bbits==4 && abits==4)
		return kRTFormatARGB4444;
	else if(rbits==5 && gbits==5 && bbits==5 && abits==1)
		return kRTFormatARGB1555;
	else if(rbits==5 && gbits==6 && bbits==5 && abits==0)
		return kRTFormatRGB565;

#if UNITY_ANDROID
	//we can end up with 32bits without alpha
	if(rbits==8 && gbits==8 && bbits==8)
		return kRTFormatARGB32;
#endif

	return kRTFormatARGB32;
}

inline DepthBufferFormat MANGLE_NAME(QueryFBDepthFormat)()
{
	GLint dbits=0;
	CHECK(glGetIntegerv(GL_DEPTH_BITS, &dbits));

	if(dbits == 0)
		return kDepthFormatNone;

	return dbits == 16 ? kDepthFormat16 : kDepthFormat24;
}

#endif // GL_RT_COMMON_GLES || GL_RT_COMMON_GLES2

#endif // GL_RT_COMMON_H

