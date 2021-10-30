#ifndef VBO_GLES30_H
#define VBO_GLES30_H

#include "Runtime/Shaders/VBO.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "DataBuffersGLES30.h"
#include "IncludesGLES30.h"

#include <map>

enum AttribLocationGLES30
{
	kGLES3AttribLocationPosition	= 0,
	kGLES3AttribLocationColor		= 1,
	kGLES3AttribLocationNormal		= 2,
	kGLES3AttribLocationTexCoord0	= 3,
	kGLES3AttribLocationTexCoord1	= 4,
	kGLES3AttribLocationTexCoord2	= 5,
	kGLES3AttribLocationTexCoord3	= 6,
	kGLES3AttribLocationTexCoord4	= 7,
	kGLES3AttribLocationTexCoord5	= 8,
	kGLES3AttribLocationTexCoord6	= 9,
	kGLES3AttribLocationTexCoord7	= 10,

	kGLES3MaxVertexAttribs,	//!< Although implementations may support more, this limits VertexArrayInfoGLES30 to a reasonable value.
};

struct VertexInputInfoGLES30
{
	const void*	pointer;		//!< Pointer or offset.
	UInt8		componentType;	//!< Component type - of type VertexChannelFormat.
	UInt8		numComponents;	//!< Number of components.
	UInt16		stride;			//!< Attribute stride.

	// Following parameters come from outside:
	//	normalize:		Deduced based on location and type.
	//	buffer:			Comes from VertexArrayInfoGLES30

	VertexInputInfoGLES30 (void)
		: pointer		(0)
		, componentType	(0)
		, numComponents	(0)
		, stride		(0)
	{
	}

	VertexInputInfoGLES30 (const void* pointer_, VertexChannelFormat componentType_, int numComponents_, UInt32 stride_)
		: pointer		(pointer_)
		, componentType	((UInt8)componentType_)
		, numComponents	((UInt8)numComponents_)
		, stride		((UInt16)stride_)
	{
		// Check overflows.
		Assert((VertexChannelFormat)componentType	== componentType_	&&
			   (int)numComponents					== numComponents_	&&
			   (UInt32)stride						== stride_);
	}
};

struct VertexArrayInfoGLES30
{
	UInt32						enabledArrays;						//!< Bitmask of enabled arrays.
	UInt32						buffers[kGLES3MaxVertexAttribs];
	VertexInputInfoGLES30		arrays[kGLES3MaxVertexAttribs];

	VertexArrayInfoGLES30 (void)
		: enabledArrays(0)
	{
	}
};

// Setup vertex array state when no VAO is bound.
void SetupDefaultVertexArrayStateGLES30 (const VertexArrayInfoGLES30& info);

// Invalidate default VA input cache. Call this if you mess up with VA bindings or state gets lost otherwise.
void InvalidateVertexInputCacheGLES30();

struct VertexArrayInfoGLES30;
class VertexArrayObjectGLES30;

struct VAOCacheKeyGLES30
{
	UInt32			bufferIndices;	//!< 4 indices with 8 bits each
	ChannelAssigns	channels;

	inline VAOCacheKeyGLES30 (const ChannelAssigns& channels, UInt32 bufNdx0, UInt32 bufNdx1, UInt32 bufNdx2, UInt32 bufNdx3)
		: bufferIndices	((bufNdx0 << 24) | (bufNdx1 << 16) | (bufNdx2 << 8) | bufNdx3)
		, channels		(channels)
	{
		Assert((bufNdx0 & ~0xff) == 0 &&
			   (bufNdx1 & ~0xff) == 0 &&
			   (bufNdx2 & ~0xff) == 0 &&
			   (bufNdx3 & ~0xff) == 0);
	}

	inline VAOCacheKeyGLES30 (void)
		: bufferIndices	(~0u)
		, channels		()
	{
	}

	inline bool operator== (const VAOCacheKeyGLES30& other) const
	{
		return bufferIndices == other.bufferIndices && channels == other.channels;
	}
};

// Cache key must be changed if stream count changes.
typedef char vaoCacheStreamCountAssert[kMaxVertexStreams == 4 ? 1 : -1];

// VAO cache for single VBO. Can not be shared between VBOs. Cache must be cleared
// if layout or any buffer in VAO is changed.
// Linear search is used since VAO cache is very small and most static VBOs should find
// match in first slot(s) anyway.
class VAOCacheGLES30
{
public:
									VAOCacheGLES30			(void);
									~VAOCacheGLES30			(void);

	const VertexArrayObjectGLES30*	Find					(const VAOCacheKeyGLES30& key) const;
	void							Insert					(const VAOCacheKeyGLES30& key, VertexArrayObjectGLES30* vao);
	bool							IsFull					(void) const;

	void							Clear					(void);

public:
									VAOCacheGLES30			(const VAOCacheGLES30&); // Not allowed!
	VAOCacheGLES30&					operator=				(const VAOCacheGLES30&); // Not allowed!

	enum
	{
		kCacheSize		= 8
	};

	struct Entry
	{
		VAOCacheKeyGLES30			key;
		VertexArrayObjectGLES30*	vao;

		inline Entry (void) : vao(0) {}
	};

	Entry							m_Entries[kCacheSize];
	int								m_NextEntryNdx;
};

class GLES3VBO : public VBO
{
public:
									GLES3VBO				(void);
	virtual							~GLES3VBO				(void);

	virtual void					UpdateVertexData		(const VertexBufferData& buffer);
	virtual void					UpdateIndexData			(const IndexBufferData& buffer);

	virtual bool					MapVertexStream			(VertexStreamData& outData, unsigned stream);
	virtual void					UnmapVertexStream		(unsigned stream);

	virtual void					Cleanup					(void);
	virtual void					Recreate				(void);
	virtual bool					IsVertexBufferLost		(void) const;

	virtual bool					IsUsingSourceVertices	(void) const;
	virtual bool					IsUsingSourceIndices	(void) const;

	virtual int						GetRuntimeMemorySize	(void) const;

	virtual void					DrawVBO					(const ChannelAssigns& channels, UInt32 firstIndexByte, UInt32 indexCount,
															 GfxPrimitiveType topology, UInt32 firstVertex, UInt32 vertexCount);
	virtual void					DrawCustomIndexed		(const ChannelAssigns& channels, void* indices, UInt32 indexCount,
															 GfxPrimitiveType topology, UInt32 vertexRangeBegin, UInt32 vertexRangeEnd, UInt32 drawVertexCount);

	// This will return VBO for skinned (first) stream
	UInt32							GetSkinningTargetVBO	(void);


	virtual void MarkBuffersLost()	{}

private:
	void							ComputeVertexInputState	(VertexArrayInfoGLES30& dst, const ChannelAssigns& channels);
	void							MarkBuffersRendered		(const ChannelAssigns& channels);

	DataBufferGLES30*				GetCurrentBuffer		(int streamNdx);

	const VertexArrayObjectGLES30*	TryGetVAO				(const ChannelAssigns& channels);

	void							Draw					(DataBufferGLES30*		indexBuffer,
															 const ChannelAssigns&	channels,
															 GfxPrimitiveType		topology,
															 UInt32					indexCount,
															 UInt32					indexOffset,
															 UInt32					vertexCountForStats);

	enum
	{
		kBufferSwapChainSize = kBufferUpdateMinAgeGLES30+1
	};

	struct Stream
	{
		UInt32					channelMask;		//!< Shader channels which this stream contains.
		UInt32					stride;
		int						curBufferNdx;		//!< Current buffer in swap chain
		DataBufferGLES30*		buffers[kBufferSwapChainSize];
		UInt8*					cpuBuf;				//!< CPU-side copy, used for Recreate() and emulating mapbuffer.

		Stream (void) : channelMask(0), stride(0), curBufferNdx(0), cpuBuf(0) { memset(&buffers[0], 0, sizeof(buffers)); }
	};

	// Vertex data
	Stream						m_StreamBuffers[kMaxVertexStreams];
	ChannelInfoArray			m_Channels;
	int							m_VertexCount;

	// Index data
	std::vector<UInt16>			m_Indices;			//!< Index data. Copy is kept for emulating quad primitive type.
	DataBufferGLES30*			m_IndexBuffer;

	VAOCacheGLES30				m_VAOCache;
};

class DynamicGLES3VBO : public DynamicVBO
{
public:
						DynamicGLES3VBO			(void);
						~DynamicGLES3VBO		(void);

	virtual bool		GetChunk				(UInt32 shaderChannelMask, UInt32 maxVertices, UInt32 maxIndices, RenderMode renderMode, void** outVB, void** outIB);
	virtual void		ReleaseChunk			(UInt32 actualVertices, UInt32 actualIndices);
	virtual void		DrawChunk				(const ChannelAssigns& channels);

	virtual void		Recreate				(void);

private:
	void				ComputeVertexInputState	(VertexArrayInfoGLES30& info, const ChannelAssigns& channels);

	void				Cleanup					(void);

	DataBufferGLES30*	GetQuadArrayIndexBuffer	(int vertexCount);

	enum
	{
		kDataBufferThreshold	= 1024
	};

	std::vector<UInt8>	m_CurVertexData;
	std::vector<UInt16>	m_CurIndexData;

	DataBufferGLES30*	m_CurVertexBuffer;
	DataBufferGLES30*	m_CurIndexBuffer;

	RenderMode			m_CurRenderMode;
	UInt32				m_CurShaderChannelMask;
	UInt32				m_CurStride;
	UInt32				m_CurVertexCount;
	UInt32				m_CurIndexCount;

	DataBufferGLES30*	m_QuadArrayIndexBuffer;	//!< Used for kDrawQuads mode.
};

#endif
