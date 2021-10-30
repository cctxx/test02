#ifndef SKINNEDMESHFILTER_H
#define SKINNEDMESHFILTER_H

#include "Runtime/Filters/Renderer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include "Runtime/Geometry/AABB.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GPUSkinningInfo.h"
#include "Runtime/Modules/ExportModules.h"
#include "Runtime/BaseClasses/NamedObject.h"

class Mesh;
class Animation;
class VBO;
namespace Unity { class SkinnedCloth; }
struct SkinMeshInfo;
struct CalculateSkinMatricesTask;

class EXPORT_COREMODULE SkinnedMeshRenderer : public Renderer
{
	PPtr<Mesh>						m_Mesh;
	Mesh*							m_CachedMesh;

	// Bones for non-optimized mode
	dynamic_array<PPtr<Transform> > m_Bones;
	PPtr<Transform>                 m_RootBone;
	// Bones for optimized mode
	dynamic_array<UInt16>			m_SkeletonIndices;

	Unity::Component*				m_CachedAnimator;
	
	UInt32                          m_CachedBlendShapeCount;
	dynamic_array<float>			m_BlendShapeWeights;

	AABB                            m_AABB;
	int                             m_Quality;	///< enum { Auto = 0, 1 Bone = 1, 2 Bones = 2, 4 Bones = 4  } Number of bones to use per vertex during skinning.
	bool                            m_UpdateWhenOffscreen;///< Unity will calculate an accurate bounding volume representation every frame.
	bool                            m_DirtyAABB;
	bool 						    m_Visible;
	
	VBO*							m_VBO;
	dynamic_array<UInt8>			m_SkinnedVertices;
	UInt32							m_ChannelsInVBO;
	bool							m_SourceMeshDirty;
	bool							m_UpdateBeforeRendering;
	SkinnedCloth*					m_Cloth;

	GPUSkinningInfo*				m_MemExportInfo;

	ListNode<Object>			m_MeshNode;
	
#if UNITY_EDITOR
	float m_CachedSurfaceArea;
#endif
	
#if UNITY_EDITOR || SUPPORT_REPRODUCE_LOG
	PPtr<Animation>					m_DeprecatedDisableAnimationWhenOffscreen;
#endif

	ListNode<SkinnedMeshRenderer> m_SkinNode;
	void DirtyAndClearCache ();

public:
	
	int GetBonesPerVertexCount ();

	REGISTER_DERIVED_CLASS (SkinnedMeshRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (SkinnedMeshRenderer)

	SkinnedMeshRenderer (MemLabelId label, ObjectCreationMode mode);
	// ~SkinnedMeshRenderer (); declared-by-macro
	
	virtual void Reset ();

	static void InitializeClass ();
	static void CleanupClass ();
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);
	
	void Setup (Mesh* lodmesh, const dynamic_array<PPtr<Transform> >& state);

	enum SkinningFlags
	{
		SF_None				= 0,
		SF_NoUpdateVBO		= 1 << 0,
		SF_ReadbackBuffer	= 1 << 1,
		SF_ClothPlaying		= 1 << 2,
		SF_AllowMemExport	= 1 << 3,
	};

	enum UpdateType
	{
		kUpdateCloth,
		kUpdateNonCloth
	};

	bool PrepareSkinCommon( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask=NULL );
	bool PrepareSkinPS3( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask=NULL );
	bool PrepareSkinGPU( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask=NULL );
	bool PrepareSkin( UInt32 requiredChannels, int flags, SkinMeshInfo& skin, CalculateSkinMatricesTask* calcSkinMatricesTask=NULL );

	bool SkinMesh( SkinMeshInfo& skin, bool lastMemExportThisFrame, UInt32 cpuFence, int flags );
	bool SkinMeshImmediate( UInt32 requiredChannels );

	bool CalculateAnimatedPoses (Matrix4x4f* poses, size_t size);

#if UNITY_EDITOR
	float GetCachedSurfaceArea ();
	bool GetSkinnedVerticesAndNormals (dynamic_array<Vector3f>* vertices, dynamic_array<Vector3f>* normals);
	bool CalculateVertexBasedBounds (const Matrix4x4f* poseMatrices, MinMaxAABB& output);
#endif

	void SetUpdateWhenOffscreen (bool onlyIfVisible);
	bool GetUpdateWhenOffscreen () { return m_UpdateWhenOffscreen; }
	
	void SetDisableAnimationWhenOffscreen (Animation* animation);
	Animation* GetDisableAnimationWhenOffscreen ();

	const dynamic_array<PPtr<Transform> >& GetBones () const { return m_Bones; }
	void SetBones (const dynamic_array<PPtr<Transform> >& bones);

	bool IsOptimized () { return m_Bones.size()==0 && GetBindposeCount()>0; }
	// Before invoking this function, please make sure the SkinnedMeshRenderer is in optimized mode.
	const dynamic_array<UInt16>& GetSkeletonIndices();
	
	int GetBindposeCount () const;
	int GetBoneCount () const { return m_Bones.size(); }

	void SetQuality (int quality);
	int GetQuality () { return m_Quality; }

	void SetMesh (Mesh*  mesh);
	Mesh* GetMesh ();
	
	void SetCloth (SkinnedCloth *value) { m_Cloth = value; }
	void SetRootBone (Transform* rootBone) { m_RootBone = rootBone; }
	Transform* GetRootBone () { return m_RootBone; }
	
	Transform& GetActualRootBone ();
	
	static void UpdateAllSkinnedMeshes(UpdateType updateType, dynamic_array<SkinnedMeshRenderer*>* outMeshes = NULL);
	static void UploadSkinnedClothes(const dynamic_array<SkinnedMeshRenderer*>& skinnedMeshes);
	void ReadSkinningDataForCloth(const SkinMeshInfo& skin);
#if UNITY_EDITOR
	void UpdateClothDataForEditing(const SkinMeshInfo& skin);
#endif
	
	/// Handlers so we only skin when somebody sees us. 
	virtual void BecameVisible ();
	virtual void BecameInvisible ();
	
	#if UNITY_EDITOR || SUPPORT_REPRODUCE_LOG
	void HandleOldSkinnedFilter ();
	#endif

	virtual void UpdateTransformInfo();

	void SetLocalAABB(const AABB& bounds);
	void GetSkinnedMeshLocalAABB(AABB& bounds);
	
	void DidDeleteMesh ();
	void DidModifyMesh ();
	void UpdateCachedMesh ();
	
	void BakeMesh (Mesh& mesh);
	
	virtual void Render (int materialIndex, const ChannelAssigns& channels);

	float GetBlendShapeWeight(UInt32 index) const;
	void SetBlendShapeWeight(UInt32 index, float weight);
	
#if ENABLE_PROFILER	
	static int GetVisibleSkinnedMeshRendererCount ();
#endif

	bool DoesQualifyForMemExport() const;

	bool CalculateRootLocalSpaceBounds (MinMaxAABB& minMaxAAbb);

	void UnloadVBOFromGfxDevice();
	void ReloadVBOToGfxDevice();

protected:

	size_t GetValidBlendShapeWeightCount () const;
	
	bool ShouldRecalculateBoundingVolumeEveryFrame () { return m_UpdateWhenOffscreen || m_RootBone.GetInstanceID() != 0; }
	
	
	// Puts the renderer into the queue of all visible skinned meshes depending on if it is currently visible.
	void UpdateVisibleSkinnedMeshQueue (bool active);
	
	bool CalculateAnimatedPosesWithRoot (const Matrix4x4f& rootMatrix, Matrix4x4f* poses, size_t size);
	// Main thread only.
	bool CalculateSkinningMatrices (const Matrix4x4f& rootPose, Matrix4x4f* poses, size_t size);

	bool PrepareVBO (bool hasSkin, bool hasBlendshape, bool doMemExport, int flags);

	virtual void UpdateRenderer();

	Unity::Component* GetAnimator ();
	void CreateCachedAnimatorBinding ();
	void ClearCachedAnimatorBinding ();
	bool CalculateBoneBasedBounds (const Matrix4x4f* animatedPoses, size_t size, MinMaxAABB& output);

	static void AnimatorModifiedCallback(void* userData, void* sender, int eventID);
};

#endif
