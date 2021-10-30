#ifndef GLES_CHANNELS_H
#define GLES_CHANNELS_H

#if GFX_SUPPORTS_OPENGLES20
	#include "Runtime/GfxDevice/opengles20/UnityGLES20Ext.h"
#endif

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

enum VertexChannel {
	kVtxChnVertex = 1 << kVertexCompVertex,
	kVtxChnColor = 1 << kVertexCompColor,
	kVtxChnNormal = 1 << kVertexCompNormal,
	kVtxChnTexCoord0 = 1 << kVertexCompTexCoord0,
	kVtxChnTexCoord1 = 1 << kVertexCompTexCoord1,
	kVtxChnTexCoord2 = 1 << kVertexCompTexCoord2,
	kVtxChnTexCoord3 = 1 << kVertexCompTexCoord3,
	kVtxChnTexCoord4 = 1 << kVertexCompTexCoord4,
	kVtxChnTexCoord5 = 1 << kVertexCompTexCoord5,
	kVtxChnTexCoord6 = 1 << kVertexCompTexCoord6,
	kVtxChnTexCoord7 = 1 << kVertexCompTexCoord7
};

static const VertexChannel sTexCoordChannels[] = {
	kVtxChnTexCoord0, kVtxChnTexCoord1, kVtxChnTexCoord2, kVtxChnTexCoord3,
	kVtxChnTexCoord4, kVtxChnTexCoord5, kVtxChnTexCoord6, kVtxChnTexCoord7
};

static void SetupGLESChannelSizes(const ChannelInfoArray channels, int outSizes[kShaderChannelCount])
{
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		if (channels[i].format == kChannelFormatColor)
			outSizes[i] = 4;
		else
			outSizes[i] = channels[i].dimension;
	}
}

static void SetupGLESChannelTypes(const ChannelInfoArray channels, GLenum outTypes[kShaderChannelCount])
{
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		switch (channels[i].format)
		{
			case kChannelFormatFloat:
				outTypes[i] = GL_FLOAT;
				break;
#if GFX_SUPPORTS_OPENGLES20
			case kChannelFormatFloat16:
				outTypes[i] = GL_HALF_FLOAT_OES;
				break;
#endif
			case kChannelFormatColor:
				outTypes[i] = GL_UNSIGNED_BYTE;
				break;
			case kChannelFormatByte:
				outTypes[i] = GL_BYTE;
				break;
			default:
				if (channels[i].IsValid())
					ErrorString("Vertex channel has invalid type for GLES");
		}
	}
}

#endif
