#ifndef UNITYGL_H
#define UNITYGL_H

#ifdef GLES_INCLUDES_H
#	error "Don't include UnityGL.h and GLESIncludes.h at the same time!"
#endif

#if UNITY_OSX
	#include "unity_gl.h"
	#undef GL_VERSION_1_2
	#include "unity_glext.h"
#elif UNITY_WIN
	#undef NOMINMAX
	#define NOMINMAX 1
	#include <windows.h>
	#include "unity_gl.h"
	#undef GL_VERSION_1_2
	#include "unity_glext.h"
	#include "PlatformDependent/Win/wglext.h"
#elif UNITY_WII
	#include <revolution.h>
#elif UNITY_ANDROID
	#include <GLES/gl.h>
#elif UNITY_LINUX
	#include "unity_gl.h"
	#undef GL_VERSION_1_2
	#include "unity_glext.h"
#else
#error "Unknown platform"
#endif

#if UNITY_WII

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

void glBegin (GLenum mode);
void glEnd (void);

void glVertex2f (GLfloat x, GLfloat y);
void glVertex3f (GLfloat x, GLfloat y, GLfloat z);
void glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void glVertex3fv (const GLfloat *v);
void glColor4fv (const GLfloat *v);
void glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glTexCoord1f (GLfloat s);
void glTexCoord2f (GLfloat s, GLfloat t);
void glTexCoord3f (GLfloat s, GLfloat t, GLfloat r);
void glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void glTexCoord1fv (const GLfloat *v);
void glTexCoord2fv (const GLfloat *v);
void glTexCoord3fv (const GLfloat *v);
void glTexCoord4fv (const GLfloat *v);
//void glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz);
void glNormal3fv (const GLfloat *v);
void glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz);

void glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void glDisableClientState (GLenum array);
void glClientActiveTextureARB (GLenum);
void glPolygonMode (GLenum face, GLenum mode);
void glActiveTextureARB (GLenum);
void glMultiTexCoord3fvARB (GLenum, const GLfloat *);
void glMultiTexCoord3fARB (GLenum, GLfloat, GLfloat, GLfloat);
void glMultiTexCoord4fARB (GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
void glMultiTexCoord4fvARB (GLenum, const GLfloat *);
void glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void glDrawArrays (GLenum mode, GLint first, GLsizei count);
void glDisableVertexAttribArrayARB (GLuint);
void glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void glBindTexture (GLenum target, GLuint texture);
void glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params);
void glTexParameteri (GLenum target, GLenum pname, GLint param);
void glTranslatef (GLfloat x, GLfloat y, GLfloat z);
void glScalef (GLfloat x, GLfloat y, GLfloat z);

void glMatrixMode (GLenum mode);
void glMultMatrixf (const GLfloat *m);
void glPushMatrix (void);
void glPopMatrix (void);
void glLoadIdentity (void);

#define GL_UNSIGNED_BYTE                  0x1401
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_FLOAT                          0x1406

#define GL_ZERO                           0
#define GL_ONE                            1
//#define GL_SRC_COLOR                      0x0300
//#define GL_ONE_MINUS_SRC_COLOR            0x0301
//#define GL_SRC_ALPHA                      0x0302
//#define GL_ONE_MINUS_SRC_ALPHA            0x0303
//#define GL_DST_ALPHA                      0x0304
//#define GL_ONE_MINUS_DST_ALPHA            0x0305

#define GL_FRONT_AND_BACK                 0x0408

#define GL_LINES                          0x0001
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_QUADS                          0x0007

#define GL_TEXTURE_2D                     0x0DE1

#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB 0x851A

#define GL_TEXTURE_BORDER_COLOR           0x1004
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_CLAMP_TO_BORDER_ARB            0x812D

#define GL_LINE                           0x1B01
#define GL_FILL                           0x1B02

#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_TEXTURE                        0x1702

#define GL_COLOR_ARRAY                    0x8076
#define GL_VERTEX_ARRAY                   0x8074
#define GL_NORMAL_ARRAY                   0x8075
#define GL_TEXTURE_COORD_ARRAY            0x8078

#define GL_TEXTURE0_ARB                   0x84C0

#elif UNITY_ANDROID
#else

#if GFX_SUPPORTS_OPENGL
#define DEF(a,b) extern a UNITYGL_##b
#include "GLExtensionDefs.h"
#undef DEF
#endif

#endif

#if GFX_SUPPORTS_OPENGL
void InitGLExtensions();
void CleanupGLExtensions();
void UnbindVertexBuffersGL(); // defined in GfxDeviceGL.cpp
#include "GLAssert.h"
#endif


//#define DUMMY_OPENGL_CALLS


#ifndef DUMMY_OPENGL_CALLS
#define OGL_CALL(x) do { x; GLAssert(); } while(0)
#else
void DummyOpenGLFunction();
#define OGL_CALL(x) DummyOpenGLFunction()
#endif



#endif
