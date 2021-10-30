#pragma once

#include "Runtime/Shaders/VBO.h"
#include "D3D11Includes.h"

class GfxDevice;

class D3D11VBO : public VBO {
public:
	D3D11VBO();
	virtual ~D3D11VBO();

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount, GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount );
	#if GFX_ENABLE_DRAW_CALL_BATCHING
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount ) ;
	#endif
	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );
	virtual bool IsVertexBufferLost() const;

	virtual void SetIndicesDynamic(bool dynamic);

	virtual int GetRuntimeMemorySize() const;
	
	static void CleanupSharedBuffers();

	virtual void UseAsStreamOutput() { m_useForSO = true; }
	void BindToStreamOutput();
	void UnbindFromStreamOutput();

private:
	void UpdateVertexStream (const VertexBufferData& sourceData, unsigned stream);
	void UpdateIndexBufferData (const IndexBufferData& sourceData);
	void BindVertexStreams (GfxDevice& device, ID3D11DeviceContext* ctx, const ChannelAssigns& channels);

	static UInt16* MapDynamicIndexBuffer (int indexCount, UInt32& outBytesUsed);
	static void UnmapDynamicIndexBuffer ();

	static ID3D11Buffer* GetAllWhiteBuffer();

private:
	int		m_VertexCount;

	ID3D11Buffer*			m_VBStreams[kMaxVertexStreams];
	ChannelInfoArray		m_ChannelInfo;
	ID3D11Buffer*			m_StagingVB[kMaxVertexStreams];
	ID3D11Buffer*			m_IB;
	ID3D11Buffer*			m_StagingIB;
	UInt16*					m_IBReadable;
	int		m_IBSize;
	bool					m_useForSO;

	static ID3D11Buffer*	ms_CustomIB;
	static int				ms_CustomIBSize;
	static UInt32			ms_CustomIBUsedBytes;
	static ID3D11Buffer*	ms_AllWhiteBuffer;
};


class DynamicD3D11VBO : public DynamicVBO {
public:
	DynamicD3D11VBO( UInt32 vbSize, UInt32 ibSize );
	virtual ~DynamicD3D11VBO();

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode mode, void** outVB, void** outIB );
	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );
	virtual void DrawChunk (const ChannelAssigns& channels);

	ID3D11Buffer* GetQuadsIB()
	{
		if (!m_QuadsIB)
			InitializeQuadsIB();
		return m_QuadsIB;
	}

	#if UNITY_EDITOR
	void DrawChunkUserPrimitives (GfxPrimitiveType type);
	#endif

private:
	void InitializeQuadsIB();

private:
	UInt32	m_VBSize;
	UInt32	m_VBUsedBytes;
	UInt32	m_IBSize;
	UInt32	m_IBUsedBytes;

	ID3D11Buffer*	m_VB;
	ID3D11Buffer*	m_IB;

	UInt32 m_LastChunkStartVertex;
	UInt32 m_LastChunkStartIndex;

	ID3D11Buffer*	m_QuadsIB;
};
