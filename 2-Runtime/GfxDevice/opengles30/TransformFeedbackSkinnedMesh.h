#ifndef __TRANSFORMFEEDBACKSKINNEDMESH_H__
#define __TRANSFORMFEEDBACKSKINNEDMESH_H__

#include "Runtime/GfxDevice/GPUSkinningInfo.h"
#include "Runtime/GfxDevice/opengles30/IncludesGLES30.h"
#include "Runtime/GfxDevice/opengles30/DataBuffersGLES30.h"
#include <vector>

class GfxDeviceGLES30;

// Transform Feedback mesh skinning data.
// Source and destination VBO formats must match.
class TransformFeedbackSkinningInfo : public GPUSkinningInfo
{
	friend class GfxDeviceGLES30;
private:
	GLuint m_SourceVBO;
	GLsizei m_SourceVBOSize;

	std::vector<float> m_CachedPose;

	DataBufferGLES30 *m_MatrixBuffer;

	//! Stores the bone count from the previous call to UpdateSourceBones. Used to select the most suitable shader version.
	int m_BoneCount;

	UInt32 GetVertexSize();

	bool EnsureBuffer();

	TransformFeedbackSkinningInfo() : GPUSkinningInfo(),
		m_SourceVBO(0), m_SourceVBOSize(0), m_MatrixBuffer(0), m_BoneCount(0)
	{}

	virtual ~TransformFeedbackSkinningInfo();

	virtual void UpdateSourceData(const void *vertData, const BoneInfluence *skinData, bool dirty);
	virtual void UpdateSourceBones(const int boneCount, const Matrix4x4f* cachedPose);
	virtual void SkinMesh(bool last);

public:

	//! Release shaders that have been created for transform feedback use.
	static void CleanupTransformFeedbackShaders(void);
};

#endif
