#include "UnityPrefix.h"
#include "TerrainRenderer.h"

#if ENABLE_TERRAIN

#ifdef TERRAIN_PREFER_VBO_OVER_MESH
#	include "Runtime/GfxDevice/GfxDevice.h"
#	include "Runtime/Terrain/QuadTreeNodeRenderer.h"
#else
#	include "Runtime/Camera/IntermediateRenderer.h"
#endif
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Camera/Camera.h"

TerrainRenderer::TerrainRenderer (SInt32 instanceID, PPtr<TerrainData> terrainData, const Vector3f &position, int lightmapIndex)
#if UNITY_EDITOR
:	m_InstanceID(instanceID)
#endif
{
	m_Scale = terrainData->GetHeightmap().GetScale();
	m_Levels = terrainData->GetHeightmap().GetMipLevels();
	m_TerrainData = terrainData;
	m_Position = position;
	m_LeftNeighbor = NULL;
	m_RightNeighbor = NULL;
	m_BottomNeighbor = NULL;
	m_TopNeighbor = NULL;

	m_SplatMaterials = new SplatMaterials (m_TerrainData);

	m_LightmapIndex = lightmapIndex;
	
	RebuildNodes ();
}

TerrainRenderer::~TerrainRenderer ()
{
	delete m_SplatMaterials;

	for (std::vector<QuadTreeNode>::iterator it = m_Quadtree.begin(), itEnd = m_Quadtree.end(); it != itEnd; ++it)
	{
		RemoveMesh(*it);
	}

#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	for (std::list<VBO*>::iterator it = m_FreeVBOPool.begin(), itEnd = m_FreeVBOPool.end(); it != itEnd; ++it)
	{
		GetGfxDevice().DeleteVBO(*it);
	}
#endif
}

void TerrainRenderer::SetNeighbors (TerrainRenderer *left, TerrainRenderer *top, TerrainRenderer *right, TerrainRenderer *bottom)
{
	m_TopNeighbor = top;
	m_RightNeighbor = right;
	m_BottomNeighbor = bottom;
	m_LeftNeighbor = left;
}

void TerrainRenderer::SetTerrainData(PPtr<TerrainData> value) 
{
	m_TerrainData = value;
}

#ifdef TERRAIN_PREFER_VBO_OVER_MESH

VBO* TerrainRenderer::CreateVBO()
{
	if (!m_FreeVBOPool.empty())
	{
		VBO* vbo = m_FreeVBOPool.back();
		m_FreeVBOPool.pop_back();
		return vbo;
	}

	// create new VBO
	return GetGfxDevice().CreateVBO();
}

void TerrainRenderer::ReclaimVBO(VBO* vbo)
{
	AssertIf(vbo == NULL);
	m_FreeVBOPool.push_back(vbo);
}

#endif
		
int TerrainRenderer::GetIndex (int x, int y, int level)
{
	int index = 0;
	for (int i=0;i<level;i++)
	{
		int size = 1 << (m_Levels - i);
		index += size * size;
	}
	
	int width = 1 << (m_Levels - level);
	index += width * y;
	index += x;

	Assert (index >= 0 && index < m_Quadtree.size());

	return index;
}
	
QuadTreeNode *TerrainRenderer::GetNode (int x, int y, int level)
{
	if (level < 0 || level > m_Levels)
		return NULL;
	int size = 1 << (m_Levels - level);

	if (x < 0 || x >= size || y < 0 || y >= size)
	{
		// Left
		if (x == -1 && m_LeftNeighbor != NULL)
			return m_LeftNeighbor->GetNode (size - 1, y, level);
		
		// right
		if (x == size && m_RightNeighbor != NULL)
			return m_RightNeighbor->GetNode (0, y, level);

		// top
		if (y == size && m_TopNeighbor != NULL)
			return m_TopNeighbor->GetNode (x, 0, level);

		// bottom
		if (y == -1 && m_BottomNeighbor != NULL)
			return m_BottomNeighbor->GetNode (x, size-1, level);

		return NULL;
	}
	
	return &m_Quadtree[GetIndex(x, y, level)];
}

QuadTreeNode *TerrainRenderer::GetNodeAndRenderer (int x, int y, int level, TerrainRenderer *&renderer)
{
	if (level < 0 || level > m_Levels)
	{
		renderer = NULL;
		return NULL;
	}
	int size = 1 << (m_Levels - level);

	if (x < 0 || x >= size || y < 0 || y >= size)
	{
		// Left
		if (x == -1 && m_LeftNeighbor != NULL)
		{
			renderer = m_LeftNeighbor;
			return m_LeftNeighbor->GetNode (size - 1, y, level);
		}

		// right
		if (x == size && m_RightNeighbor != NULL)
		{
			renderer = m_RightNeighbor;
			return m_RightNeighbor->GetNode (0, y, level);
		}

		// top
		if (y == size && m_TopNeighbor != NULL)
		{
			renderer = m_TopNeighbor;
			return m_TopNeighbor->GetNode (x, 0, level);
		}
		
		// bottom
		if (y == -1 && m_BottomNeighbor != NULL)
		{
			renderer = m_BottomNeighbor;
			return m_BottomNeighbor->GetNode (x, size-1, level);
		}
		
		renderer = NULL;
		return NULL;
	}
	
	renderer = this;
	return &m_Quadtree[GetIndex(x, y, level)];
}

int TerrainRenderer::GetPatchCountX (int level)
{
	return 1 << (m_Levels - level);
}

int TerrainRenderer::GetPatchCountY (int level)
{
	return 1 << (m_Levels - level);
}

void TerrainRenderer::RebuildNodes ()
{
	Heightmap &heightmap = m_TerrainData->GetHeightmap();
	int totalSize = heightmap.GetTotalPatchCount();

	m_Quadtree.resize(totalSize);
	for (int level=0;level <= m_Levels;level++)
	{
		for (int y=0;y<GetPatchCountY(level);y++)
		{
			for (int x=0;x<GetPatchCountX(level);x++)
			{
				int index = GetIndex(x, y, level);
				m_Quadtree[index].x = x;
				m_Quadtree[index].y = y;
				m_Quadtree[index].level = level;
				m_Quadtree[index].maxHeightError = heightmap.GetMaximumHeightError(x, y, level);
				m_Quadtree[index].bounds = heightmap.GetBounds(x, y, level);
				m_Quadtree[index].bounds.GetCenter() += m_Position;
			}
		}
	}
}

Vector3f TerrainRenderer::GetNodePosition (QuadTreeNode &node)
{
	Vector3f position = Vector3f::zero;
	position += m_Position;
	return position;
}
	
int TerrainRenderer::CalculateEdgeMask (QuadTreeNode &node)
{
	//@TODO: Handle case where invisible (dont need to switch index buffers)
	int mask = 0;
	for (int i=kDirectionLeft;i<=kDirectionDown;i++)
	{
		QuadTreeNode *neighbor = FindNeighbor(node, i);
		if (neighbor != NULL)
		{
			if (neighbor->visibility == kVisibilityDrawChild || neighbor->visibility == kVisibilityDrawSelf)
				mask |= 1 << i;
		}
		else
			mask |= 1 << i;
	}
	return mask;
}
	
QuadTreeNode *TerrainRenderer::FindNeighbor (QuadTreeNode &node, int direction)
{
	if (direction == kDirectionUp)
		return GetNode(node.x, node.y + 1, node.level);
	else if (direction == kDirectionDown)
		return GetNode(node.x, node.y - 1, node.level);
	else if (direction == kDirectionLeft)
		return GetNode(node.x - 1, node.y, node.level);
	else
		return GetNode(node.x + 1, node.y, node.level);
}

QuadTreeNode *TerrainRenderer::FindNeighborAndRenderer (QuadTreeNode &node, int direction, TerrainRenderer *&renderer)
{
	if (direction == kDirectionUp)
		return GetNodeAndRenderer(node.x, node.y + 1, node.level, renderer);
	else if (direction == kDirectionDown)
		return GetNodeAndRenderer(node.x, node.y - 1, node.level, renderer);
	else if (direction == kDirectionLeft)
		return GetNodeAndRenderer(node.x - 1, node.y, node.level, renderer);
	else
		return GetNodeAndRenderer(node.x + 1, node.y, node.level, renderer);
}
	
QuadTreeNode *TerrainRenderer::FindChild (QuadTreeNode &node, int direction)
{
	if (direction == kDirectionLeftUp)
		return GetNode(node.x * 2, node.y * 2, node.level - 1);
	else if (direction == kDirectionRightUp)
		return GetNode(node.x * 2 + 1, node.y * 2, node.level - 1);
	else if (direction == kDirectionLeftDown)
		return GetNode(node.x * 2, node.y * 2 + 1, node.level - 1);
	else
		return GetNode(node.x * 2 + 1, node.y * 2 + 1, node.level - 1);
}
	
QuadTreeNode *TerrainRenderer::FindParent (QuadTreeNode &node)
{
	return GetNode(node.x / 2, node.y / 2, node.level + 1);
}
	
void TerrainRenderer::RenderStep1 (Camera *camera, int maxLodLevel, float tau, float splatDistance)
{
	// clamp LOD level to valid values
	maxLodLevel = clamp(maxLodLevel, 0, m_Levels);

	float nearClip = camera->GetNear();
	float vTop = nearClip * tanf( Deg2Rad( camera->GetFov() / 2.0F )  );
	float vres = camera->GetScreenViewportRect().height;
	
	float A = nearClip / fabs(vTop);
	float T = 2 * tau / (float)vres;
	float kC = A / T;

	QuadTreeNode& root = GetRootNode();

	m_CachedCameraPosition = camera-> QueryComponent(Transform)->GetPosition();
	m_CachedMaxLodLevel = maxLodLevel;
	m_CachedkC = kC;
	m_CachedSqrSplatDistance = splatDistance * splatDistance;

	RecursiveCalculateLod(root);
}
	
void TerrainRenderer::RenderStep2 ()
{
	QuadTreeNode& root = GetRootNode();
	
	EnforceLodTransitions(root);
}
	
void TerrainRenderer::RenderStep3 (Camera *camera, int layer, bool castShadows, Material* mat)
{
	QuadTreeNode &root = GetRootNode();
	
	m_CurrentLayer = layer;
	m_CurrentCamera = camera;
	m_CastShadows = castShadows;

	// Get Splat materials
	m_CurrentMaterials = m_SplatMaterials->GetMaterials (mat, m_CurrentMaterialCount);
	m_CurrentBaseMaterial = m_SplatMaterials->GetSplatBaseMaterial (mat);

	RecursiveRenderMeshes(root, m_TerrainData->GetHeightmap());
}

	
void TerrainRenderer::CleanupMeshes ()
{
	for (std::vector<QuadTreeNode>::iterator node=m_Quadtree.begin();node != m_Quadtree.end(); node++)
	{
		if (node->visibility == kVisibilityDrawSelf)
			RemoveMesh(*node);
		node->visibility = kVisibilityLoddedAway;
		node->oldVisibility = kVisibilityLoddedAway;
	}
}
	
void TerrainRenderer::ReloadAll () {
	Cleanup ();

	m_Scale = m_TerrainData->GetHeightmap().GetScale();
	m_Levels = m_TerrainData->GetHeightmap().GetMipLevels();

	
	delete m_SplatMaterials;
	m_SplatMaterials = new SplatMaterials (m_TerrainData);
	
	RebuildNodes ();
}

void TerrainRenderer::Cleanup () {
	CleanupMeshes();
	m_SplatMaterials->Cleanup ();
}

void TerrainRenderer::UnloadVBOFromGfxDevice ()
{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	// This will put all VBOs back into m_FreeVBOPool
	for (std::vector<QuadTreeNode>::iterator node=m_Quadtree.begin();node != m_Quadtree.end(); node++)
	{
		RemoveMesh(*node);
		node->updateIndices = true;
		node->updateMesh = true;
	}

	for (std::list<VBO*>::iterator it = m_FreeVBOPool.begin(), itEnd = m_FreeVBOPool.end(); it != itEnd; ++it)
	{
		GetGfxDevice().DeleteVBO(*it);
	}
	m_FreeVBOPool.clear();
#endif
}

void TerrainRenderer::ReloadVBOToGfxDevice ()
{
	// No need to do anything, since QuadTreeNodeRenderers will recreate themselves
}
	
float TerrainRenderer::CalculateSqrDistance (Vector3f &rkPoint, AABB &rkBox)
{
	// compute coordinates of point in box coordinate system
	Vector3f kClosest = rkPoint - rkBox.GetCenter();

	// project test point onto box
	float fSqrDistance = 0.0f;
	float fDelta;
	Vector3f& extents = rkBox.GetExtent();
	
	for (int i=0;i<3;i++)
	{
		if ( kClosest[i] < -extents[i] )
		{
			fDelta = kClosest[i] + extents[i];
			fSqrDistance += fDelta * fDelta;
			kClosest[i] = -extents[i];
		}
		else if ( kClosest[i] > extents[i] )
		{
			fDelta = kClosest[i] - extents[i];
			fSqrDistance += fDelta * fDelta;
			kClosest[i] = extents[i];
		}
	}

	return fSqrDistance;
}

void TerrainRenderer::MarkChildVisibilityRecurse (QuadTreeNode &node, int newVisibility)
{
	if (node.level == 0)
		return;
		
	for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
	{
		QuadTreeNode* childNode = FindChild (node, i);
		if (childNode->visibility != newVisibility)
		{
			childNode->visibility = newVisibility;
			MarkChildVisibilityRecurse(*childNode, newVisibility);
		}
	}
}

void TerrainRenderer::ReloadPrecomputedError ()
{
	Heightmap &heightmap = m_TerrainData->GetHeightmap();
	for (std::vector<QuadTreeNode>::iterator node=m_Quadtree.begin();node != m_Quadtree.end(); node++)
		node->maxHeightError = heightmap.GetMaximumHeightError(node->x, node->y, node->level);
}

void TerrainRenderer::ReloadBounds ()
{
	Heightmap &heightmap = m_TerrainData->GetHeightmap();
	for (std::vector<QuadTreeNode>::iterator node=m_Quadtree.begin();node != m_Quadtree.end(); node++)
	{
		node->bounds = heightmap.GetBounds(node->x, node->y, node->level);
		node->bounds.GetCenter() += m_Position;
	}
}
		
void TerrainRenderer::RecursiveCalculateLod (QuadTreeNode &node)
{
	// Distance to bounding volume based
	float distance = CalculateSqrDistance(m_CachedCameraPosition, node.bounds);

	float D2 = m_CachedkC * node.maxHeightError;
	D2 *= D2;

	// Node has good enough lod, render it
	if (distance > D2 || node.level == m_CachedMaxLodLevel)
	{
		node.visibility = kVisibilityDrawSelf;
		node.useSplatmap = distance < m_CachedSqrSplatDistance;
		MarkChildVisibilityRecurse(node, kVisibilityLoddedAway);
	}
	// Node needs subdivision
	else
	{
		node.visibility = kVisibilityDrawChild;
		
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode* childNode = FindChild (node, i);
			RecursiveCalculateLod (*childNode);
		}
	}
}

void TerrainRenderer::ForceSplitParent (QuadTreeNode &node)
{
	QuadTreeNode* parent = FindParent(node);
	Assert (parent != NULL);
	if (parent->visibility == kVisibilityLoddedAway)
		ForceSplitParent (*parent);

	if (parent->visibility == kVisibilityDrawSelf)
	{
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode* childNode = FindChild (*parent, i);
			Assert (childNode != NULL);
			
			childNode->visibility = kVisibilityDrawSelf;
			childNode->useSplatmap = CalculateSqrDistance(m_CachedCameraPosition, childNode->bounds) < m_CachedSqrSplatDistance;
			
			MarkChildVisibilityRecurse(*childNode, kVisibilityLoddedAway);
		}
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode *childNode = FindChild (*parent, i);
			Assert (childNode != NULL);

			EnforceLodTransitions(*childNode);
		}
		parent->visibility = kVisibilityDrawChild;
	}
}

// Go through all nodes that are being rendered
// Check if the parent neighbor 
void TerrainRenderer::EnforceLodTransitions (QuadTreeNode &node)
{
	if (node.visibility == kVisibilityLoddedAway)
		return;

	if (node.visibility == kVisibilityDrawSelf)
	{
		for (int i=kDirectionLeft;i<=kDirectionDown;i++)
		{
			TerrainRenderer *renderer;
			
			// If we have a neighbor, make sure it's either a root node that's completely LOD'd away
			// or that it's at most one detail level away from us (i.e. that the neighbor's tree is split down at
			// least to the level of the neighbor's parent).
			QuadTreeNode *neighbor = FindNeighborAndRenderer (node, i, renderer);
			if (neighbor != NULL && neighbor->visibility == kVisibilityLoddedAway && !renderer->IsRootNode (*neighbor))
			{
				QuadTreeNode *neighborParent = renderer->FindParent(*neighbor);
				Assert (neighborParent != NULL);

				if (neighborParent->visibility == kVisibilityLoddedAway)
				{
					renderer->ForceSplitParent(*neighborParent);
				}
			}
		}
	}
	else
	{
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode *childNode = FindChild (node, i);
			Assert (childNode != NULL);

			EnforceLodTransitions(*childNode);
		}
	}
}

void TerrainRenderer::RemoveMesh (QuadTreeNode &node)
{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	if (node.vbo != NULL)
	{
		ReclaimVBO(node.vbo);
		node.vbo = NULL;
		node.updateMesh = false;
		node.updateIndices = false;
	}
#else
	// Cleanup self
	if(node.mesh)
	{
		DestroySingleObject (node.mesh);
		node.mesh = NULL;
	}
#endif
}
	
void TerrainRenderer::RecursiveRemoveMeshes (QuadTreeNode &node)
{
	if (node.oldVisibility == kVisibilityLoddedAway)
		return;
	
	if (node.oldVisibility == kVisibilityDrawSelf)
	{
		RemoveMesh(node);
	}
	else if (node.oldVisibility == kVisibilityDrawChild)
	{
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode* childNode = FindChild (node, i);
			RecursiveRemoveMeshes(*childNode);
		}
	}

	node.oldVisibility = kVisibilityLoddedAway;
}

void TerrainRenderer::RenderNode (QuadTreeNode &node)
{
	int layer = m_CurrentLayer;
	Camera *camera = m_CurrentCamera;
	Vector3f position = GetNodePosition(node);
	
	Matrix4x4f matrix;
	matrix.SetTranslate( position );
	bool castShadows = m_CastShadows;
	
	if (node.useSplatmap)
	{
		int count = m_CurrentMaterialCount;
		for (int m=0;m<count;++m)
		{
			Material* mat = m_CurrentMaterials[m];
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
			QuadTreeNodeRenderer* r = AddQuadTreeNodeRenderer( matrix, node.subMesh.localAABB, mat, layer, castShadows, true, camera );
			r->Setup( this, &node );
#else
			IntermediateRenderer* r = AddIntermediateRenderer (matrix, node.mesh, mat, layer, castShadows, true, 0, camera);
#endif
			r->SetLightmapIndexIntNoDirty(m_LightmapIndex);
			#if UNITY_EDITOR
			r->SetScaleInLightmap(m_LightmapSize);
			r->SetInstanceID (m_InstanceID);
			#endif
		}
	}
	else 
	{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
		QuadTreeNodeRenderer* r = AddQuadTreeNodeRenderer( matrix, node.subMesh.localAABB, m_CurrentBaseMaterial, layer, castShadows, true, camera );
		r->Setup( this, &node );
#else
		IntermediateRenderer* r = new IntermediateRenderer( matrix, node.mesh, m_CurrentBaseMaterial, layer, castShadows, true, 0 );
#endif
		r->SetLightmapIndexIntNoDirty(m_LightmapIndex);
		#if UNITY_EDITOR
		r->SetScaleInLightmap(m_LightmapSize);
		r->SetInstanceID (m_InstanceID);
		#endif
	}
}

void TerrainRenderer::BuildRenderer (QuadTreeNode &node, int edgeMask)
{
	// Build mesh
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	RemoveMesh(node);
#else
	DestroySingleObject (node.mesh);

	node.mesh = NEW_OBJECT (Mesh);
	node.mesh->Reset();
	node.mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);

	node.mesh->SetHideFlags(Object::kDontSave);
#endif

	AABB bounds = m_TerrainData->GetHeightmap().GetBounds(node.x, node.y, node.level);

#ifdef TERRAIN_PREFER_VBO_OVER_MESH
	node.subMesh.localAABB = bounds;
	node.updateMesh = true;
	node.updateIndices = true;
#else
	node.mesh->SetBounds(bounds);
	m_TerrainData->GetHeightmap().UpdatePatchMesh(*node.mesh, node.x, node.y, node.level, edgeMask, this);
	m_TerrainData->GetHeightmap().UpdatePatchIndices(*node.mesh, node.x, node.y, node.level, edgeMask);
	Assert(node.mesh->GetBounds().GetCenter () == bounds.GetCenter() && node.mesh->GetBounds().GetExtent() == bounds.GetExtent ());
#endif
}


void TerrainRenderer::RecursiveRenderMeshes (QuadTreeNode &node, Heightmap &heightmap)
{
	// The node itself needs to be rendered
	if (node.visibility == kVisibilityDrawSelf)
	{
		int newEdgeMask = CalculateEdgeMask(node);
		// We didn't render the node last frame. So we need to build from scratch
		if (node.oldVisibility != kVisibilityDrawSelf)
		{
			BuildRenderer(node, newEdgeMask);
			node.edgeMask = newEdgeMask;
			RenderNode(node);
		}
		// If  highest lod error is infinity we rebuild the mesh constantly
		// (This is set while the terrain is being painted)
		else if (node.maxHeightError == std::numeric_limits<float>::infinity())
		{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
			node.updateMesh = true;
#else
			heightmap.UpdatePatchMesh(*node.mesh, node.x, node.y, node.level, newEdgeMask, this);
#endif
			if (newEdgeMask != node.edgeMask)
			{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
				node.updateIndices = true;
#else
				heightmap.UpdatePatchIndices(*node.mesh, node.x, node.y, node.level, newEdgeMask);
#endif
				node.edgeMask = newEdgeMask;		
			}
			
			RenderNode(node);
		}
		// The edge mask has changed, we need to update the triangle indices
		else if (newEdgeMask != node.edgeMask)
		{
#ifdef TERRAIN_PREFER_VBO_OVER_MESH
			node.updateIndices = true;
#else
			heightmap.UpdatePatchIndices(*node.mesh, node.x, node.y, node.level, newEdgeMask);
#endif
			node.edgeMask = newEdgeMask;
			RenderNode(node);
		}
		// Nothing changed just paint
		else
		{
			RenderNode(node);
		}
		
		// All old children need to be removed!
		if (node.oldVisibility == kVisibilityDrawChild)
		{
			for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
			{
				QuadTreeNode *childNode = FindChild (node, i);
				Assert (childNode != NULL);
				RecursiveRemoveMeshes (*childNode);
			}
		}
	}
	// Some children of the node need to be rendered
	else if (node.visibility == kVisibilityDrawChild)
	{
		// Remove the old render mesh
		if (node.oldVisibility == kVisibilityDrawSelf)
			RemoveMesh (node);
		
		// Recurse into children
		for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
		{
			QuadTreeNode *childNode = FindChild (node, i);
			RecursiveRenderMeshes (*childNode, heightmap);
		}
	}
	// Node is invisible
	else
	{
		// All old children need to be removed!
		if (node.oldVisibility == kVisibilityDrawChild)
		{
			for (int i=kDirectionLeftUp;i<=kDirectionRightDown;i++)
			{
				QuadTreeNode *childNode = FindChild (node, i);
				RecursiveRemoveMeshes (*childNode);
			}
		}
		// Remove the old render mesh
		else if (node.oldVisibility == kVisibilityDrawSelf)
			RemoveMesh (node);
	}
		
	node.oldVisibility = node.visibility;
}

Mesh* GetMeshForPatch(int x, int y, int level, Heightmap& heightmap, TerrainRenderer* terrainRenderer)
{
	// Build mesh		
	Mesh* mesh = NEW_OBJECT (Mesh);
	mesh->Reset();
	mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
	
	mesh->SetHideFlags(Object::kDontSave);
	
	// judging by CalculateEdgeMask() the edgeMask equal to 15 means all the neighbouring
	// patches are visible	
	heightmap.UpdatePatchMesh(*mesh, x, y, level, 15, terrainRenderer);
	heightmap.UpdatePatchIndices(*mesh, x, y, level, 15);
	
	mesh->RecalculateBounds();
	
	return mesh;
}

std::vector<Mesh*> TerrainRenderer::GetMeshPatches()
{
	std::vector<Mesh*> meshes;
	Heightmap &heightmap = m_TerrainData->GetHeightmap();
	
	const int level = 0;
	int xPatches = GetPatchCountX(level);
	int yPatches = GetPatchCountY(level);
	
	for (int y = 0; y < yPatches; y++)
	{
		for (int x = 0; x < xPatches; x++)
		{
			Mesh* mesh = GetMeshForPatch(x, y, level, heightmap, this);
			mesh->SetName(Format("%s[%i][%i]", m_TerrainData->GetName(), x, y).c_str());
			meshes.push_back(mesh);
		}
	}
	
	return meshes;
}

#endif // ENABLE_TERRAIN
