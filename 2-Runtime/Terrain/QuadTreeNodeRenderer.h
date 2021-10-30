#pragma once

#include "Runtime/Camera/IntermediateRenderer.h"

struct QuadTreeNode;
class Camera;
class TerrainRenderer;

class QuadTreeNodeRenderer : public MeshIntermediateRenderer
{
public:
	QuadTreeNodeRenderer ();

	void Initialize (const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows);

	virtual ~QuadTreeNodeRenderer();

	void Setup(TerrainRenderer* terrainRenderer, QuadTreeNode* node);

	virtual void Render( int materialIndex, const ChannelAssigns& channels );

	static void StaticInitialize ();
	static void StaticDestroy ();

protected:


private:
	// Note: not using per-frame linear allocator, because in the editor
	// it can render multiple frames using single player loop run (e.g. when editor is paused).
	// Clearing per-frame data and then trying to use it later leads to Bad Things.
	DECLARE_POOLED_ALLOC(QuadTreeNodeRenderer);

	TerrainRenderer*	m_TerrainRenderer;	// It's safe to store raw pointer to the TerrainRender object here
											// because TerrainRenderers are deleted before Render() happens
											// and the life-time of TerrainVBORenderer object is only one frame.
	QuadTreeNode*		m_QuadTreeNode;
};

QuadTreeNodeRenderer* AddQuadTreeNodeRenderer( const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, Camera* camera );
