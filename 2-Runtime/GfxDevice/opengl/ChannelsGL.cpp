#include "UnityPrefix.h"
#include "ChannelsGL.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "UnityGL.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "GLAssert.h"

// dimensionality of the different channels (element count)
static const int kDefaultChannelSizes[kShaderChannelCount] = {
	3, // pos
	3, // normal
	4, // color
	2, // uv
	2, // uv2
	4, // tangent
};

static const GLenum kDefaultChannelTypes[kShaderChannelCount] = {
	GL_FLOAT, // pos
	GL_FLOAT, // normal
	GL_UNSIGNED_BYTE, // color
	GL_FLOAT, // UV0
	GL_FLOAT, // UV1
	GL_FLOAT, // tangent
};


static void SetVertexComponentData( VertexComponent comp, int size, unsigned int type, int stride, const void *pointer )
{
	// We allow pointer to be used as an offset
	//DebugAssertIf( !pointer );

	switch (comp) {
	case kVertexCompColor:
		OGL_CALL(glColorPointer (size, type, stride, pointer));
		break;
	case kVertexCompVertex:
		OGL_CALL(glVertexPointer (size, type, stride, pointer));
		break;
	case kVertexCompNormal:
		DebugAssert( size >= 3 );
		OGL_CALL(glNormalPointer (type, stride, pointer));
		break;
	case kVertexCompTexCoord0:
	case kVertexCompTexCoord1:
	case kVertexCompTexCoord2:
	case kVertexCompTexCoord3:
	case kVertexCompTexCoord4:
	case kVertexCompTexCoord5:
	case kVertexCompTexCoord6:
	case kVertexCompTexCoord7:
		if (gGraphicsCaps.maxTexCoords > 1) {
			OGL_CALL(glClientActiveTextureARB( GL_TEXTURE0_ARB + comp - kVertexCompTexCoord0 ));
		}
		OGL_CALL(glTexCoordPointer (size, type, stride, pointer));
		break;
	case kVertexCompTexCoord:
		printf_console( "Warning: unspecified texcoord bound\n" );
		break;
	case kVertexCompAttrib0: case kVertexCompAttrib1: case kVertexCompAttrib2: case kVertexCompAttrib3:
	case kVertexCompAttrib4: case kVertexCompAttrib5: case kVertexCompAttrib6: case kVertexCompAttrib7:
	case kVertexCompAttrib8: case kVertexCompAttrib9: case kVertexCompAttrib10: case kVertexCompAttrib11:
	case kVertexCompAttrib12: case kVertexCompAttrib13: case kVertexCompAttrib14: case kVertexCompAttrib15:
		OGL_CALL(glVertexAttribPointerARB( comp - kVertexCompAttrib0, size, type, true, stride, pointer ));
		break;
	}
}

static signed char s_EnabledVertexComps[kVertexCompCount]; // 0/1 or -1 if unknown
static unsigned int s_EnabledTargetsMask; // Bitfield of which targets are currently valid

void ClearActiveChannelsGL()
{
	s_EnabledTargetsMask = 0;
}

static UInt32 GetGLType(const ChannelInfo& source)
{
	switch (source.format)
	{
		case kChannelFormatFloat:	return GL_FLOAT;
		case kChannelFormatFloat16:	return GL_HALF_FLOAT_ARB;
		case kChannelFormatColor:	return GL_UNSIGNED_BYTE;
		case kChannelFormatByte:	return GL_BYTE;
	}
	ErrorString("Unknown channel format!");
	return 0;
}

static int GetGLDimension(const ChannelInfo& source)
{
	return (source.format == kChannelFormatColor) ? 4 : source.dimension;
}

void SetChannelDataGL( const ChannelInfo& source, const StreamInfoArray streams, VertexComponent target, const UInt8* buffer )
{
	AssertIf( target == kVertexCompNone || target == kVertexCompTexCoord );
	int stride = source.CalcStride( streams );
	int offset = source.CalcOffset( streams );
	SetVertexComponentData( target, GetGLDimension(source), GetGLType(source), stride, buffer + offset );
	s_EnabledTargetsMask |= (1 << target);
}

void SetChannelDataGL( ShaderChannel source, VertexComponent target, const void *pointer, int stride )
{
	AssertIf( source == kShaderChannelNone );
	if( !pointer )
		return;

	DebugAssertIf( target == kVertexCompNone || target == kVertexCompTexCoord );
	SetVertexComponentData( target, kDefaultChannelSizes[source], kDefaultChannelTypes[source], stride, pointer );
	s_EnabledTargetsMask |= (1 << target);
}

#if UNITY_EDITOR
// only used by GizmoUtil in the editor
void SetChannelDataGL( ShaderChannel source, const ChannelAssigns& channels, const void *pointer, int stride )
{
	AssertIf( source == kShaderChannelNone );
	if( !pointer )
		return;

	for( int i = 0; i < kVertexCompCount; ++i )
	{
		if( channels.GetSourceForTarget( (VertexComponent)i ) == source )
		{
			AssertIf( i == kVertexCompNone || i == kVertexCompTexCoord );
			SetVertexComponentData( (VertexComponent)i, kDefaultChannelSizes[source], kDefaultChannelTypes[source], stride, pointer );
			s_EnabledTargetsMask |= (1 << i);
		}
	}
}

#endif


void ActivateChannelsGL()
{
	unsigned int compMask = s_EnabledTargetsMask;
	int i;

	// fetch now to prevent aliasing
	GLint vaCount = gGraphicsCaps.gl.vertexAttribCount;
	//GLint texUnitCount = gGraphicsCaps.maxTexUnits;
	GLint texCoordCount = gGraphicsCaps.maxTexCoords;

	static GLenum gltargets[] = { 0, GL_VERTEX_ARRAY, GL_COLOR_ARRAY, GL_NORMAL_ARRAY };

	// There's a dark corner of GL spec that says that generic and conventional vertex
	// attributes (and arrays) may be aliased (most decent drivers don't alias them).
	// http://oss.sgi.com/projects/ogl-sample/registry/NV/vertex_program.txt
	//
	// So we first disable all vertex components that we don't use; then enable all
	// that we do use. This should work both in aliased and non-aliased implementations.
	//
	// On some systems (XP, Catalyst 5.7, R9600) this makes a difference!
	// Without it e.g. a single GUIText is not visible.

	// --------------------------------
	// First disable components we don't use

	for( i = kVertexCompVertex; i <= kVertexCompNormal; ++i )
	{
		if( !(compMask & (1<<i)) )
		{
			if( s_EnabledVertexComps[i] != 0 )
			{
				OGL_CALL(glDisableClientState( gltargets[i] ));
				s_EnabledVertexComps[i] = 0;
			}
		}
	}

	for( i = 0; i < texCoordCount; ++i )
	{
		int compIndex = kVertexCompTexCoord0 + i;
		if( !(compMask & (1 << compIndex)) )
		{
			if( s_EnabledVertexComps[compIndex] != 0 )
			{
				if( texCoordCount > 1 )
					OGL_CALL(glClientActiveTextureARB(GL_TEXTURE0_ARB + i));
				OGL_CALL(glDisableClientState (GL_TEXTURE_COORD_ARRAY));
				s_EnabledVertexComps[compIndex] = 0;
			}
		}
	}

	for( i = 0; i < vaCount; ++i )
	{
		int compIndex = kVertexCompAttrib0 + i;
		if( !(compMask & (1 << compIndex)) )
		{
			if( s_EnabledVertexComps[compIndex] != 0 )
			{
				OGL_CALL(glDisableVertexAttribArrayARB (i));
				s_EnabledVertexComps[compIndex] = 0;
			}
		}
	}

	// --------------------------------
	// Then enable components we use

	for( i = kVertexCompVertex; i <= kVertexCompNormal; ++i )
	{
		if( compMask & (1<<i) )
		{
			if( s_EnabledVertexComps[i] != 1 )
			{
				OGL_CALL(glEnableClientState( gltargets[i] ));
				s_EnabledVertexComps[i] = 1;
			}
		}
	}

	for( i = 0; i < texCoordCount; ++i )
	{
		int compIndex = kVertexCompTexCoord0 + i;
		if( compMask & (1 << compIndex) )
		{
			if( s_EnabledVertexComps[compIndex] != 1 )
			{
				if( texCoordCount > 1 )
					OGL_CALL(glClientActiveTextureARB(GL_TEXTURE0_ARB + i));
				OGL_CALL(glEnableClientState (GL_TEXTURE_COORD_ARRAY));
				s_EnabledVertexComps[compIndex] = 1;
			}
		}
	}

	for( i = 0; i < vaCount; ++i )
	{
		int compIndex = kVertexCompAttrib0 + i;
		if( compMask & (1 << compIndex) )
		{
			if( s_EnabledVertexComps[compIndex] != 1 )
			{
				OGL_CALL(glEnableVertexAttribArrayARB( i ));
				s_EnabledVertexComps[compIndex] = 1;
			}
		}
	}

	GLAssert();
}

void InvalidateChannelStateGL()
{
	for( int i = 0; i < kVertexCompCount; ++i )
		s_EnabledVertexComps[i] = -1;
}
