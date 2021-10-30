#ifndef VBO_GLES20_H
#define VBO_GLES20_H

#include "Runtime/Shaders/BufferedVBO.h"
#include "IncludesGLES20.h"
#include <map>

class GLES2VBO : public BufferedVBO {
public:
	GLES2VBO();
	virtual ~GLES2VBO ();

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
					  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount );
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount );

	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );

	virtual void Cleanup();
	virtual void Recreate();
	virtual bool IsVertexBufferLost() const;
	virtual bool IsIndexBufferLost() const;
	virtual void MarkBuffersLost();

	virtual bool IsUsingSourceVertices() const { return m_VertexBufferID[0] == 0; }
	virtual bool IsUsingSourceIndices() const { return m_IndexBufferID == 0; }

	virtual int GetRuntimeMemorySize() const;

private:

	void DrawInternal(int vertexBufferID, int indexBufferID, const ChannelAssigns& channels, void* indices, UInt32 indexCount, GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount);

	UInt32 GetUnavailableChannels(const ChannelAssigns& channels) const;

	ChannelInfoArray	m_Channels;
	IndexBufferData		m_IBData;
	enum { DynamicVertexBufferCount = 3 };
	int					m_VertexBufferID[DynamicVertexBufferCount];
	int					m_CurrentBufferIndex;
	int					m_IndexBufferID;
	int					m_IBSize;
	void*				m_ReadableIndices;

	int					m_VBOUsage;
	int					m_IBOUsage;


	bool				m_UsesVBO;
	bool				m_UsesIBO;

	// for now these are only called on UpdateVertexData/UpdateIndexData
	// so they are extracted only for convinience
	bool				ShouldUseVBO();
	bool				ShouldUseIBO();

	void				EnsureVerticesInited(bool newBuffers);
	void				EnsureIndicesInited();
};

class SharedBuffer;
class DynamicGLES2VBO : public DynamicVBO
{
public:
	DynamicGLES2VBO();
	~DynamicGLES2VBO();

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);

	virtual void Recreate();

	SharedBuffer*	GetSharedVB (size_t bytes);
	SharedBuffer*	GetSharedIB (size_t bytes);

	void*			GetVertexMemory(size_t bytes);
	void*			GetIndexMemory(size_t bytes);

private:
	void InitializeQuadsIB();

private:
	void*			m_BufferChannel[kShaderChannelCount];
	RenderMode		m_LastRenderMode;

	SharedBuffer*	m_VB;
	SharedBuffer*	m_LargeVB;
	SharedBuffer*	m_ActiveVB;
	SharedBuffer*	m_IB;
	SharedBuffer*	m_LargeIB;
	SharedBuffer*	m_ActiveIB;

	UInt16*			m_QuadsIB;
	int				m_IndexBufferQuadsID;

	UInt8*			m_VtxSysMemStorage;
	unsigned		m_VtxSysMemStorageSize;

	UInt16*			m_IdxSysMemStorage;
	unsigned		m_IdxSysMemStorageSize;
};

void InvalidateVertexInputCacheGLES20();
// WARNING: forceBufferObject is just a temp solution, as we really need to clean up all this mess
// we need it for bigger dynamic vbos
void* LockSharedBufferGLES20 (int bufferType, size_t bytes, bool forceBufferObject=false);
int UnlockSharedBufferGLES20 (size_t actualBytes = 0, bool forceBufferObject=false);

#define GL_VERTEX_ARRAY		0
#define GL_COLOR_ARRAY		1
#define GL_NORMAL_ARRAY		2
#define GL_TEXTURE_ARRAY0	3
#define GL_TEXTURE_ARRAY1	GL_TEXTURE_ARRAY0 + 1
#define GL_TEXTURE_ARRAY2	GL_TEXTURE_ARRAY0 + 2
#define GL_TEXTURE_ARRAY3	GL_TEXTURE_ARRAY0 + 3
#define GL_TEXTURE_ARRAY4	GL_TEXTURE_ARRAY0 + 4
#define GL_TEXTURE_ARRAY5	GL_TEXTURE_ARRAY0 + 5
#define GL_TEXTURE_ARRAY6	GL_TEXTURE_ARRAY0 + 6
#define GL_TEXTURE_ARRAY7	GL_TEXTURE_ARRAY0 + 7

#if NV_STATE_FILTERING
void filteredBindBufferGLES20(GLenum target, GLuint buffer,bool isImmediate=false);
void filteredVertexAttribPointerGLES20(GLuint  index,  GLint  size,  GLenum  type,  GLboolean  normalized,  GLsizei  stride,  const GLvoid *  pointer);
void StateFiltering_InvalidateVBOCacheGLES20();
#endif

#endif
