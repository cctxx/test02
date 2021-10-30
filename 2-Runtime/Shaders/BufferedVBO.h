#ifndef BUFFEREDVBO_H
#define BUFFEREDVBO_H

#include "VBO.h"
#include "Runtime/Utilities/dynamic_array.h"

class BufferedVBO : public VBO
{
public:
	BufferedVBO();
	~BufferedVBO();

	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );

	virtual int GetRuntimeMemorySize() const;

	virtual void UnloadSourceVertices();

protected:
	void BufferAllVertexData( const VertexBufferData& buffer );
	void BufferAccessibleVertexData( const VertexBufferData& buffer );
	void BufferVertexData( const VertexBufferData& buffer, bool copyModes[kStreamModeCount] );
	void UnbufferVertexData();

	UInt8* GetStreamBuffer(unsigned stream);
	UInt8* GetChannelDataAndStride(ShaderChannel channel, UInt32& outStride);
	void GetChannelDataAndStrides(void* channelData[kShaderChannelCount], UInt32 outStrides[kShaderChannelCount]);
	void GetChannelOffsetsAndStrides(void* channelOffsets[kShaderChannelCount], UInt32 outStrides[kShaderChannelCount]);

	VertexBufferData m_VertexData;
	size_t m_AllocatedSize;
};

#endif
