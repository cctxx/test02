#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/AABB.h"
#include "TerrainData.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Shaders/Material.h"
#include "SplatMaterials.h"

#define TERRAIN_PREFER_VBO_OVER_MESH

class Camera;

enum
{
	kVisibilityLoddedAway = 0,
	kVisibilityDrawChild = 1,
	kVisibilityDrawSelf = 2
};

struct QuadTreeNode
{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	VBO         *vbo;
	SubMesh     subMesh;
#else
	Mesh        *mesh;
#endif
	int         edgeMask;
	float       maxHeightError;
	
	int  visibility;
	int  oldVisibility;
		
	int x;
	int y;
	int level;
	AABB bounds;
	bool useSplatmap		: 1;

#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	bool updateMesh			: 1;
	bool updateIndices		: 1;
#endif
	
	QuadTreeNode()
	{ 
		edgeMask = -1; 
		maxHeightError = 1; 
		visibility = kVisibilityLoddedAway; 
		oldVisibility = kVisibilityLoddedAway;
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
		vbo = NULL;
		updateMesh = false;
		updateIndices = false;
#else
		mesh = NULL;
#endif
	}
	
	~QuadTreeNode()
	{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
		AssertIf(vbo != NULL);
#else
		DestroySingleObject(mesh);
#endif
	}
};

class TerrainRenderer
{
public:
	TerrainRenderer              *m_TopNeighbor;
	TerrainRenderer              *m_RightNeighbor;
	TerrainRenderer              *m_BottomNeighbor;
	TerrainRenderer              *m_LeftNeighbor;
	
	Vector3f					m_Scale;
	std::vector<QuadTreeNode>	m_Quadtree;
	int                         m_Levels;
	SplatMaterials				*m_SplatMaterials;
	Vector3f					m_Position;
	bool						m_CastShadows;
	
	Vector3f m_CachedCameraPosition;
	int m_CachedMaxLodLevel;
	float m_CachedkC;
	float m_CachedSqrSplatDistance;
	
	TerrainRenderer (SInt32 instanceID, PPtr<TerrainData> terrainData, const Vector3f &position, int lightmapIndex);
	~TerrainRenderer ();
	void RenderStep1 (Camera *camera, int maxLodLevel, float tau, float splatDistance);
	void RenderStep2 ();
	void RenderStep3 (Camera *camera, int layer, bool castShadow, Material* mat);
	void ReloadAll ();
	void Cleanup ();
	void UnloadVBOFromGfxDevice ();
	void ReloadVBOToGfxDevice ();
	void ReloadPrecomputedError ();	
	void ReloadBounds ();
	void RecursiveCalculateLod (QuadTreeNode &node);
	void SetNeighbors (TerrainRenderer *left, TerrainRenderer *top, TerrainRenderer *right, TerrainRenderer *bottom);
	
	PPtr<TerrainData> GetTerrainData() const { return m_TerrainData; }
	void SetTerrainData(PPtr<TerrainData> value);

	int GetLightmapIndex() { return m_LightmapIndex; }
	void SetLightmapIndex(int value) { m_LightmapIndex = value; }

	std::vector<Mesh*> GetMeshPatches();
	
	#if UNITY_EDITOR
	void SetLightmapSize(int lightmapSize) { m_LightmapSize = lightmapSize; }
	#endif

private:
	
	int m_CurrentLayer;
	Camera *m_CurrentCamera;
	Material *m_CurrentBaseMaterial;
	int m_CurrentMaterialCount;
	Material **m_CurrentMaterials;
	PPtr<TerrainData> m_TerrainData;
	int m_LightmapIndex;
	#if UNITY_EDITOR
	SInt32 m_InstanceID;
	int m_LightmapSize;
	#endif

	std::list<VBO*> m_FreeVBOPool;
	VBO* CreateVBO();
	void ReclaimVBO(VBO* vbo);

	QuadTreeNode &GetRootNode() { return m_Quadtree.back(); }
	bool IsRootNode (const QuadTreeNode& node) { return (node.level == GetRootNode().level); }


	int GetIndex (int x, int y, int level);
	QuadTreeNode *GetNode (int x, int y, int level);
	QuadTreeNode *GetNodeAndRenderer (int x, int y, int level, TerrainRenderer *&renderer);
	int GetPatchCountX (int level);
	int GetPatchCountY (int level);
	void RebuildNodes ();
	Vector3f GetNodePosition (QuadTreeNode &node);
	int CalculateEdgeMask (QuadTreeNode &node);
	QuadTreeNode *FindNeighbor (QuadTreeNode &node, int direction);
	QuadTreeNode *FindNeighborAndRenderer (QuadTreeNode &node, int direction, TerrainRenderer *&renderer);
	QuadTreeNode *FindChild (QuadTreeNode &node, int direction);
	QuadTreeNode *FindParent (QuadTreeNode &node);
	void CleanupMeshes ();
	static float CalculateSqrDistance (Vector3f &rkPoint, AABB &rkBox);
	void MarkChildVisibilityRecurse (QuadTreeNode &node, int newVisibility);		
	void ForceSplitParent (QuadTreeNode &node);
	void EnforceLodTransitions (QuadTreeNode &node);
	void RemoveMesh (QuadTreeNode &node);
	void RecursiveRemoveMeshes (QuadTreeNode &node);
	void RenderNode (QuadTreeNode &node);
	void BuildRenderer (QuadTreeNode &node, int edgeMask);
	void RecursiveRenderMeshes (QuadTreeNode &node, Heightmap &heightmap);

	friend class QuadTreeNodeRenderer;
};

#endif // ENABLE_TERRAIN
