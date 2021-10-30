#pragma once

#include "UnityPrefix.h"
#include "Runtime/Graphics/RenderTexture.h"

// Shared GLES3 utilities

class TransferFormatGLES30
{
public:
	UInt32	format;
	UInt32	dataType;

	TransferFormatGLES30 (UInt32 format_, UInt32 dataType_)
		: format	(format_)
		, dataType	(dataType_)
	{
	}
};

// Map RenderTextureFormat to closest GL sized internal format.
UInt32 GetColorFormatGLES30 (RenderTextureFormat format);

// Get closest depth internal format.
UInt32 GetDepthOnlyFormatGLES30 (DepthBufferFormat format);

// Get closest depth&stencil internal format.
UInt32 GetDepthStencilFormatGLES30 (DepthBufferFormat format);

// Get transfer (upload) format, dataType pair for internal format.
TransferFormatGLES30 GetTransferFormatGLES30 (UInt32 internalFormat);

// Get default framebuffer (0) internal format (guess based on bits)
UInt32 GetDefaultFramebufferColorFormatGLES30 (void);

// Get default framebuffer (0) depth format
UInt32 GetDefaultFramebufferDepthFormatGLES30 (void);
