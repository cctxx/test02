#ifndef THREADEDVBO_H
#define THREADEDVBO_H

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Shaders/BufferedVBO.h"

class Mutex;
class GfxDeviceClient;
struct ClientDeviceVBO;
class GfxDeviceWorker;
class ThreadedStreamBuffer;

class ThreadedVBO : public BufferedVBO {
public:
	ThreadedVBO(GfxDeviceClient& device);
	virtual ~ThreadedVBO();

	virtual void UpdateVertexData( const VertexBufferData& buffer );
	virtual void UpdateIndexData (const IndexBufferData& buffer);
	virtual void DrawVBO (const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
					  GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount );
	#if GFX_ENABLE_DRAW_CALL_BATCHING
	virtual void DrawCustomIndexed( const ChannelAssigns& channels, void* indices, UInt32 indexCount,
								   GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount );
	#endif

	virtual void Recreate();

	// For writing directly to VBO. VBO must be filled (UpdateData)
	// at least once; and vertex layout + topology from the last fill
	// is used. For example, for skinned meshes you have to call
	// UpdateData on start and each time layout/topology changes;
	// then map,write,unmap for each skinning.
	//
	// In some situations a vertex buffer might become lost; then you need to do UpdateData
	// again before using Map.
	virtual bool MapVertexStream( VertexStreamData& outData, unsigned stream );
	virtual void UnmapVertexStream( unsigned stream );
	virtual bool IsVertexBufferLost() const;
	virtual bool IsIndexBufferLost() const;

	virtual void SetMappedFromRenderThread( bool renderThread );
	virtual void SetVertexStreamMode( unsigned stream, StreamMode mode );
	virtual void SetIndicesDynamic(bool dynamic);

	virtual void ResetDynamicVB();
	virtual void MarkBuffersLost();

	virtual int GetRuntimeMemorySize() const;

	virtual void UseAsStreamOutput();

#if UNITY_XENON
	virtual void AddExtraUvChannels( const UInt8* data, UInt32 size, int extraUvCount );
	virtual void CopyExtraUvChannels( VBO* source );
#endif

	ClientDeviceVBO* GetClientDeviceVBO() { return m_ClientVBO; } //Todo:any nicer way?
	VBO* GetNonThreadedVBO() { return m_NonThreadedVBO; }

protected:
	ThreadedStreamBuffer& GetCommandQueue();
	GfxDeviceWorker* GetGfxDeviceWorker();
	void SubmitCommands();
	void DoLockstep();

	GfxDeviceClient&		m_ClientDevice;
	ClientDeviceVBO*		m_ClientVBO;
	VBO*					m_NonThreadedVBO;
	int						m_VertexBufferSize;
	int						m_IndexBufferSize;
	bool					m_MappedFromRenderThread;
	bool					m_VertexBufferLost;
	bool					m_IndexBufferLost;
};


class ThreadedDynamicVBO : public DynamicVBO {
public:
	ThreadedDynamicVBO(GfxDeviceClient& device);
	virtual ~ThreadedDynamicVBO() { }

	virtual bool GetChunk( UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB );

	virtual void ReleaseChunk( UInt32 actualVertices, UInt32 actualIndices );

	virtual void DrawChunk (const ChannelAssigns& channels);

private:
	ThreadedStreamBuffer& GetCommandQueue();
	GfxDeviceWorker* GetGfxDeviceWorker();
	void SubmitCommands();
	void DoLockstep();

	GfxDeviceClient& m_ClientDevice;
	ThreadedStreamBuffer* m_CommandQueue;
	dynamic_array<UInt8> m_ChunkVertices;
	dynamic_array<UInt16> m_ChunkIndices;
	bool m_ValidChunk;
};


#endif
