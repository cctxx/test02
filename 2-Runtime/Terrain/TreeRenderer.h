#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "ImposterRenderTexture.h"

#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Camera/SceneNode.h"

namespace Unity
{
	class Material;
}

class TreeDatabase;
class Mesh;
struct TreeBinaryTree;
class Camera;
class Light;
class Plane;
struct TreeInstance;
struct ShadowCasterCull;
class Matrix4x4f;

class TreeRenderer
{
public:
	TreeRenderer(TreeDatabase& database, const Vector3f& position, int lightmapIndex);
	~TreeRenderer();

	void InjectTree(const TreeInstance& newTree);
	void RemoveTrees(const Vector3f& pos, float radius, int prototypeIndex);

	void ReloadTrees();
	void Render(Camera& camera, const UNITY_VECTOR(kMemRenderer, Light*)& lights, float meshTreeDistance, float billboardTreeDistance, float crossFadeLength, int maximumMeshTrees, int layer);
	void InvalidateImposter () { m_ImposterRenderTexture->InvalidateAngles(); }

	int GetLightmapIndex() { return m_LightmapIndex; }
	void SetLightmapIndex(int value) { m_LightmapIndex = value; }

	void CollectTreeRenderers(dynamic_array<SceneNode>& sceneNodes, dynamic_array<AABB>& boundingBoxes);

private:
	void ReloadTrees(int treesPerBatch);
	void CleanupBillboardMeshes();
	void RenderRecurse(TreeBinaryTree* binTree, const Plane* planes, std::vector<int>& closeupBillboards, const Vector3f& cameraPos);
	void UpdateShaderProps(const Camera& cam);
	void GenerateBillboardMesh(Mesh& mesh, const std::vector<int>& instances, bool buildTriangles);
	Vector3f GetPosition(const TreeInstance& instance) const;
	void RenderBatch(TreeBinaryTree& binTree, float sqrDistance);
	void CreateTreeRenderer(int instance);
	void DeleteTreeRenderers();

private:
	MaterialPropertyBlock m_PropertyBlock;
	TreeDatabase* m_Database;
	Material*	m_BillboardMaterial;
	Vector3f 			m_TerrainSize;
	Vector3f 			m_Position;
	std::auto_ptr<TreeBinaryTree> m_TreeBinaryTree;

	float				m_SqrBillboardTreeDistance;
	float				m_SqrMeshTreeDistance;
	float				m_CrossFadeLength;
	float				m_SqrCrossFadeEndDistance;
	int					m_LightmapIndex;
	
	Mesh*				m_CloseBillboardMesh;	

	std::vector<int> m_FullTrees;
	std::vector<TreeBinaryTree*> m_RenderedBatches;

	std::auto_ptr<ImposterRenderTexture> m_ImposterRenderTexture;

	ForwardLinearAllocator m_RendererAllocator;

	// scene nodes and bounding boxes for all trees
	// let's call them legacy here for easier merging with SpeedTree branch in the future
	dynamic_array<SceneNode> m_LegacyTreeSceneNodes;
	dynamic_array<AABB> m_LegacyTreeBoundingBoxes;
	dynamic_array<std::pair<int, int> > m_TreeIndexToSceneNode; // {first renderer, number of renderers}
};

#endif // ENABLE_TERRAIN


