#ifndef __GPUSKINNINGINFO_H__
#define __GPUSKINNINGINFO_H__

class VBO;
class ThreadedStreamBuffer;
class Matrix4x4f;
struct BoneInfluence;

/**
*	Abstract class for GPU skinning, implemented in each GfxDevice
*/
class GPUSkinningInfo
{
protected: 
	//! Number of vertices in the skin
	UInt32	m_VertexCount;
	//! Channel map for the VBO
	UInt32	m_ChannelMap;
	//! Destination VBO stride
	int		m_Stride;

	//! Destination VBO
	VBO		*m_DestVBO;

	//! Bones per vertex, must be 1, 2 or 4
	UInt32 m_BonesPerVertex;

	// Protected constructor and destructor, can only be created and deleted from GfxDevice impl.
	// For threading purposes, constructor should not perform any GL operations (called directly from main thread).
	GPUSkinningInfo() : m_VertexCount(0), m_ChannelMap(0), m_Stride(0), m_DestVBO(NULL), m_BonesPerVertex(4) {}
	virtual ~GPUSkinningInfo() {};

public:
	virtual UInt32	GetVertexCount() const { return m_VertexCount; }
	virtual UInt32	GetChannelMap() const { return m_ChannelMap; }
	virtual int		GetStride() const { return m_Stride; }
	virtual VBO *	GetDestVBO() const { return m_DestVBO; }
	virtual UInt32	GetBonesPerVertex() const { return m_BonesPerVertex; }

	/** Update vertex count */
	virtual void SetVertexCount(UInt32 count) { m_VertexCount = count; }

	/** Update channel map */
	virtual void SetChannelMap(UInt32 channelmap) { m_ChannelMap = channelmap; }

	/** Update stride of the vertices in bytes (not including skin data). */
	virtual void SetStride(int stride) { m_Stride = stride; }

	/** Update destination VBO */
	virtual void SetDestVBO(VBO *vbo) { m_DestVBO = vbo; }

	/** Update bones-per-vertex */
	virtual void SetBonesPerVertex(UInt32 bones) { m_BonesPerVertex = bones; }

};





#endif
