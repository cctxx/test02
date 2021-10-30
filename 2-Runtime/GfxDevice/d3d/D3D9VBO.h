#pragma once

#include "D3D9Includes.h"
#include "Runtime/Shaders/VBO.h"


// Implements Direct3D9 VBO
class D3D9VBO : public VBO {
public:
	D3D9VBO();
	virtual ~D3D9VBO();

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount);
	#if GFX_ENABLE_DRAW_CALL_BATCHING
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount );
	#endif	
	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );
	virtual bool IsVertexBufferLost() const;

	virtual void ResetDynamicVB();

	virtual int GetRuntimeMemorySize() const;

	static void CleanupSharedIndexBuffer();

private:
	void BindVertexStreams( IDirect3DDevice9* dev, const ChannelAssigns& channels );
	void UpdateVertexStream( const VertexBufferData& sourceData, unsigned stream );
	void UpdateIndexBufferData (const IndexBufferData& sourceData);
	static UInt16* MapDynamicIndexBuffer (int indexCount, UInt32& outBytesUsed);
	static void UnmapDynamicIndexBuffer ();

private:
	int		m_VertexCount;

	enum
	{
		kVertexDeclDefault,
		kVertexDeclAllWhiteStream,
		kVertexDeclCount
	};

	IDirect3DVertexBuffer9*			m_VBStreams[kMaxVertexStreams];
	IDirect3DIndexBuffer9*			m_IB;
	IDirect3DVertexDeclaration9*	m_VertexDecls[kVertexDeclCount];
	ChannelInfoArray				m_ChannelInfo;
	int								m_IBSize;

	static IDirect3DIndexBuffer9*	ms_CustomIB;
	static int						ms_CustomIBSize;
	static UInt32					ms_CustomIBUsedBytes;
};

class DynamicD3D9VBO : public DynamicVBO {
public:
	DynamicD3D9VBO( UInt32 vbSize, UInt32 ibSize );
	virtual ~DynamicD3D9VBO();

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode mode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);

private:
	void InitializeQuadsIB();

private:
	UInt32	m_VBSize;
	UInt32	m_VBUsedBytes;
	UInt32	m_IBSize;
	UInt32	m_IBUsedBytes;

	IDirect3DVertexBuffer9*	m_VB;
	IDirect3DIndexBuffer9*	m_IB;
	IDirect3DVertexDeclaration9*	m_VertexDecl; // vertex declaration for the last chunk

	UInt32 m_LastChunkStartVertex;
	UInt32 m_LastChunkStartIndex;

	IDirect3DIndexBuffer9*	m_QuadsIB; // static IB for drawing quads
	bool	m_QuadsIBFailed;
};

