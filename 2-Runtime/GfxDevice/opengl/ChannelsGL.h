#ifndef __CHANNELS_GL_H__
#define __CHANNELS_GL_H__

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Filters/Mesh/VertexData.h"

void ClearActiveChannelsGL();
void SetChannelDataGL( const ChannelInfo& source, const StreamInfoArray streams, VertexComponent target, const UInt8* buffer = NULL );
void SetChannelDataGL( ShaderChannel source, VertexComponent target, const void *pointer, int stride );
#if UNITY_EDITOR
void SetChannelDataGL( ShaderChannel source, const ChannelAssigns& channels, const void *pointer, int stride );
#endif
void ActivateChannelsGL();
void InvalidateChannelStateGL();

#endif
