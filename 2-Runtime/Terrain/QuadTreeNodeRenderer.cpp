#include "UnityPrefix.h"
#include "QuadTreeNodeRenderer.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Terrain/TerrainRenderer.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/UnityScene.h"
#if UNITY_PS3
#	include "Runtime/GfxDevice/ps3/GfxGCMVBO.h"
#endif

DEFINE_POOLED_ALLOC(QuadTreeNodeRenderer, 64 * 1024);

void QuadTreeNodeRenderer::StaticInitialize()
{
	STATIC_INITIALIZE_POOL(QuadTreeNodeRenderer);
}

void QuadTreeNodeRenderer::StaticDestroy()
{
	STATIC_DESTROY_POOL(QuadTreeNodeRenderer);
}

static RegisterRuntimeInitializeAndCleanup s_QuadTreeNodeRendererCallbacks(QuadTreeNodeRenderer::StaticInitialize, QuadTreeNodeRenderer::StaticDestroy);

#ifdef TERRAIN_PREFER_VBO_OVER_MESH

QuadTreeNodeRenderer::QuadTreeNodeRenderer()
: m_TerrainRenderer(NULL)
, m_QuadTreeNode(NULL)
{
}

void QuadTreeNodeRenderer::Initialize(const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows)
{
	MeshIntermediateRenderer::Initialize(matrix, NULL, localAABB, material, layer, castShadows, receiveShadows, 0);
}

QuadTreeNodeRenderer::~QuadTreeNodeRenderer()
{
}

void QuadTreeNodeRenderer::Setup(TerrainRenderer* terrainRenderer, QuadTreeNode* quadTreeNode)
{
	AssertIf(terrainRenderer == NULL || quadTreeNode == NULL);

	m_TerrainRenderer = terrainRenderer;
	m_QuadTreeNode = quadTreeNode;
}


void QuadTreeNodeRenderer::Render( int /*subsetIndex*/, const ChannelAssigns& channels )
{
	if (m_TerrainRenderer == NULL || m_QuadTreeNode == NULL)
	{
		return;
	}

	if (m_QuadTreeNode->vbo == NULL)
	{
		m_QuadTreeNode->vbo = m_TerrainRenderer->CreateVBO();

		// these two flags shall always be set if a new VBO is requested
		AssertIf(!m_QuadTreeNode->updateMesh || !m_QuadTreeNode->updateIndices);
	}

	if (m_QuadTreeNode->updateMesh)
	{
		m_TerrainRenderer->GetTerrainData()->GetHeightmap().UpdatePatchMesh(
			*m_QuadTreeNode->vbo,
			m_QuadTreeNode->subMesh,
			m_QuadTreeNode->x,
			m_QuadTreeNode->y,
			m_QuadTreeNode->level,
			m_QuadTreeNode->edgeMask, m_TerrainRenderer);
		m_QuadTreeNode->updateMesh = false;
	}

	if (m_QuadTreeNode->updateIndices)
	{
		m_TerrainRenderer->GetTerrainData()->GetHeightmap().UpdatePatchIndices(
			*m_QuadTreeNode->vbo,
			m_QuadTreeNode->subMesh,
			m_QuadTreeNode->x,
			m_QuadTreeNode->y,
			m_QuadTreeNode->level,
			m_QuadTreeNode->edgeMask);
		m_QuadTreeNode->updateIndices = false;
	}

	if (m_CustomProperties)
	{
		GetGfxDevice().SetMaterialProperties (*m_CustomProperties);
	}

	SubMesh& subMesh = m_QuadTreeNode->subMesh;

	//PROFILER_AUTO(gDrawMeshVBOProfile, &mesh)
#if UNITY_PS3
	GfxGCMVBO* gcmVBO = static_cast<GfxGCMVBO*>(m_QuadTreeNode->vbo);
	gcmVBO->DrawSubmesh(channels, 0, &subMesh);
#else
	m_QuadTreeNode->vbo->DrawVBO(channels, subMesh.firstByte, subMesh.indexCount, subMesh.topology, subMesh.firstVertex, subMesh.vertexCount);
#endif
	GPU_TIMESTAMP();
}

QuadTreeNodeRenderer*  AddQuadTreeNodeRenderer( const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, Camera* camera )
{
	QuadTreeNodeRenderer* renderer = new QuadTreeNodeRenderer ();
	renderer->Initialize(matrix, localAABB, material, layer, castShadows, receiveShadows);

	IntermediateRenderers* renderers;
	if (camera != NULL)
		renderers = &camera->GetIntermediateRenderers();
	else
		renderers = &GetScene().GetIntermediateRenderers();
	renderers->Add(renderer, layer);

	return renderer;
}

#endif
