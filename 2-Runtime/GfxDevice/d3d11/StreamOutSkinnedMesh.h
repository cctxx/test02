#ifndef __STREAMOUTSKINNEDMESH_H__
#define __STREAMOUTSKINNEDMESH_H__

#include "External/DirectX/builds/dx11include/d3d11.h"
#include "Runtime/GfxDevice/GPUSkinningInfo.h"
class VBO;
class Matrix4x4f;
class ThreadedStreamBuffer;
class GfxDeviceD3D11;

// MemExport mesh skinning data.
class StreamOutSkinningInfo : public GPUSkinningInfo
{
	friend class GfxDeviceD3D11;
private:
	ID3D11Buffer* m_SourceVBO;
	ID3D11Buffer* m_SourceSkin;
	ID3D11Buffer* m_SourceBones;

	//! Stores the bone count from the previous call to UpdateSourceBones. Used to select the most suitable shader version.
	int m_BoneCount;

	// Private constructor and destructor , called from GfxDeviceD3D11
	StreamOutSkinningInfo() : GPUSkinningInfo(), m_SourceVBO(NULL), m_SourceSkin(NULL), m_SourceBones(NULL) {}
	virtual ~StreamOutSkinningInfo();


	// Actual implementation methods, called from GfxDeviceD3D11

	void UpdateSourceData(const void *vertData, const BoneInfluence *skinData, bool dirty);
	void UpdateSourceBones(const int boneCount, const Matrix4x4f* cachedPose);
	void SkinMesh(bool last);

public:

	//! Clean up created shaders
	static void CleanUp();

};


#endif
