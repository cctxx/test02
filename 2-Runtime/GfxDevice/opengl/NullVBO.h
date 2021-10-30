#ifndef NULL_VBO_H
#define NULL_VBO_H

#include "Runtime/Shaders/BufferedVBO.h"
#include "Configuration/UnityConfigure.h"


class DynamicNullVBO : public DynamicVBO {
public:
	DynamicNullVBO();
	virtual ~DynamicNullVBO();
	
	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);
	
private:
	void*	m_BufferChannel[kShaderChannelCount];
	
	UInt8*	m_VBChunk;
	UInt32	m_VBChunkSize;
	UInt8*	m_IBChunk;
	UInt32	m_IBChunkSize;
};


#endif
