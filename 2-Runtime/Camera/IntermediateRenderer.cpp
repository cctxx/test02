#include "UnityPrefix.h"
#include "IntermediateRenderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/SpriteRenderer.h"
#include "Camera.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/Graphics/SpriteFrame.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "UnityScene.h"


IntermediateRenderer::IntermediateRenderer()
:	BaseRenderer(kRendererIntermediate)
,	m_Node(this)
{
}

void IntermediateRenderer::Initialize(const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows)
{
	#if UNITY_EDITOR
	m_InstanceID = 0;
	#endif
	m_Material = material;
	
	if (layer < 0 || layer >= 32)
	{
		AssertString ("DrawMesh layer has to be from in [0..31] range!");
		layer = 0;
	}
	m_Layer = layer;

	// TODO: check if render 2 texture required for this material
	m_CastShadows = castShadows;
	m_ReceiveShadows = receiveShadows;
	
	m_TransformInfo.worldMatrix = matrix;
	// detect uniform and non-uniform scale (ignoring non-affine)
	float uniformScale = 1.0f;
	m_TransformInfo.transformType = ComputeTransformType(matrix, uniformScale);
	m_TransformInfo.invScale = 1.0f / uniformScale;
	m_TransformInfo.localAABB = localAABB;
	TransformAABB (localAABB, matrix, m_TransformInfo.worldAABB);

	Assert (m_TransformInfo.localAABB.IsValid());
	Assert (m_TransformInfo.worldAABB.IsValid());
	
	#if UNITY_EDITOR
	m_ScaleInLightmap = -1.0f;
	#endif

	RendererBecameVisible ();

	m_TransformDirty = false;
	m_BoundsDirty = false;
}

IntermediateRenderer::~IntermediateRenderer()
{
	RendererBecameInvisible ();
}

void IntermediateRenderer::OnAssetBoundsChanged()
{
	// Not supported. IntermediateRenderer live only for one frame.
}


// --------------------------------------------------------------------------


DEFINE_POOLED_ALLOC(MeshIntermediateRenderer, 64 * 1024);

void MeshIntermediateRenderer::StaticInitialize()
{
	STATIC_INITIALIZE_POOL(MeshIntermediateRenderer);
}

void MeshIntermediateRenderer::StaticDestroy()
{
	STATIC_DESTROY_POOL(MeshIntermediateRenderer);
}

static RegisterRuntimeInitializeAndCleanup s_MeshIntermediateRendererCallbacks(MeshIntermediateRenderer::StaticInitialize, MeshIntermediateRenderer::StaticDestroy);


MeshIntermediateRenderer::MeshIntermediateRenderer()
{
}

MeshIntermediateRenderer::~MeshIntermediateRenderer()
{
}

void MeshIntermediateRenderer::OnAssetDeleted()
{
	m_Mesh = NULL;
}

void MeshIntermediateRenderer::Render(int subsetIndex, const ChannelAssigns& channels)
{
	if (m_Mesh == NULL)
		return;
	if (m_CustomProperties)
		GetGfxDevice().SetMaterialProperties (*m_CustomProperties);
	DrawUtil::DrawMeshRaw (channels, *m_Mesh, m_SubMeshIndex); //@TODO: why not use subsetIndex here?
}

void MeshIntermediateRenderer::Initialize( const Matrix4x4f& matrix, Mesh* mesh, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex )
{
	m_Mesh = mesh;
	if (m_Mesh)
	{
		m_Mesh->AddIntermediateUser(m_Node);

		if (submeshIndex < 0 || submeshIndex >= m_Mesh->GetSubMeshCount())
		{
			AssertString("Submesh index in intermediate renderer is out of bounds");
			submeshIndex = 0;
		}
	}
	
	m_SubMeshIndex = submeshIndex;

	IntermediateRenderer::Initialize(matrix, localAABB, material, layer, castShadows, receiveShadows);
}


// --------------------------------------------------------------------------

#if ENABLE_SPRITES

DEFINE_POOLED_ALLOC(SpriteIntermediateRenderer, 64 * 1024);

void SpriteIntermediateRenderer::StaticInitialize()
{
	STATIC_INITIALIZE_POOL(SpriteIntermediateRenderer);
}

void SpriteIntermediateRenderer::StaticDestroy()
{
	STATIC_DESTROY_POOL(SpriteIntermediateRenderer);
}

static RegisterRuntimeInitializeAndCleanup s_SpriteIntermediateRendererCallbacks(SpriteIntermediateRenderer::StaticInitialize, SpriteIntermediateRenderer::StaticDestroy);


SpriteIntermediateRenderer::SpriteIntermediateRenderer()
{
}

SpriteIntermediateRenderer::~SpriteIntermediateRenderer()
{
}

void SpriteIntermediateRenderer::OnAssetDeleted()
{
	m_Sprite = NULL;
}

void SpriteIntermediateRenderer::Render(int subsetIndex, const ChannelAssigns& channels)
{
	if (m_Sprite == NULL)
		return;
	if (m_CustomProperties)
		GetGfxDevice().SetMaterialProperties(*m_CustomProperties);
	DrawUtil::DrawSpriteRaw(channels, *m_Sprite, m_Color);
}

void SpriteIntermediateRenderer::Initialize(const Matrix4x4f& matrix, Sprite* sprite, const AABB& localAABB, PPtr<Material> material, int layer, const ColorRGBA32& color)
{
	m_Sprite = sprite;
	if (m_Sprite)
		m_Sprite->AddIntermediateUser(m_Node);
	
	m_Color = color;

	if (!material)
		material = SpriteRenderer::GetDefaultSpriteMaterial();

	// Patch sprite texture and apply material property block
	PPtr<Texture2D> spriteTexture = m_Sprite->GetRenderData(false).texture; // Use non-atlased RenderData as input.
	MaterialPropertyBlock block;
	SpriteRenderer::SetupMaterialPropertyBlock(block, spriteTexture);
	SetPropertyBlock(block);

	IntermediateRenderer::Initialize(matrix, localAABB, material, layer, false, false);
}

#endif

// --------------------------------------------------------------------------


void IntermediateRenderers::Clear( size_t startIndex )
{
	size_t n = m_SceneNodes.size();
	AssertIf( startIndex > n );

	for( size_t i = startIndex; i < n; ++i )
	{
		IntermediateRenderer* renderer = static_cast<IntermediateRenderer*> (m_SceneNodes[i].renderer);
		delete renderer;
	}
	m_SceneNodes.resize_uninitialized( startIndex );
	m_BoundingBoxes.resize_uninitialized( startIndex );
}

const AABB* IntermediateRenderers::GetBoundingBoxes () const
{
	return m_BoundingBoxes.begin();
}

const SceneNode* IntermediateRenderers::GetSceneNodes () const
{
	return m_SceneNodes.begin();
}

void IntermediateRenderers::Add(IntermediateRenderer* renderer, int layer)
{
	m_SceneNodes.push_back(SceneNode ());

	SceneNode& node = m_SceneNodes.back();
	node.renderer = renderer;
	node.layer = layer;
	
	renderer->GetWorldAABB(m_BoundingBoxes.push_back());
}

IntermediateRenderer* AddMeshIntermediateRenderer( const Matrix4x4f& matrix, Mesh* mesh, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex, Camera* camera )
{
	AABB bounds;
	if (mesh)
		bounds = mesh->GetBounds();
	else
		bounds.SetCenterAndExtent( Vector3f::zero, Vector3f::zero );
	
	return AddMeshIntermediateRenderer (matrix, mesh, bounds, material, layer, castShadows, receiveShadows, submeshIndex, camera);
}

IntermediateRenderer* AddMeshIntermediateRenderer( const Matrix4x4f& matrix, Mesh* mesh, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex , Camera* camera )
{
	MeshIntermediateRenderer* renderer = new MeshIntermediateRenderer();
	renderer->Initialize(matrix, mesh, localAABB, material, layer, castShadows, receiveShadows, submeshIndex);

	IntermediateRenderers* renderers;
	if (camera != NULL)
		renderers = &camera->GetIntermediateRenderers();
	else
		renderers = &GetScene().GetIntermediateRenderers();
	renderers->Add(renderer, layer);

	return renderer;
}

#if ENABLE_SPRITES
IntermediateRenderer* AddSpriteIntermediateRenderer(const Matrix4x4f& matrix, Sprite* sprite, PPtr<Material> material, int layer, const ColorRGBA32& color, Camera* camera)
{
	AABB bounds;
	if (sprite)
		bounds = sprite->GetBounds();
	else
		bounds.SetCenterAndExtent( Vector3f::zero, Vector3f::zero );
	
	return AddSpriteIntermediateRenderer (matrix, sprite, bounds, material, layer, color, camera);
}

IntermediateRenderer* AddSpriteIntermediateRenderer(const Matrix4x4f& matrix, Sprite* sprite, const AABB& localAABB, PPtr<Material> material, int layer, const ColorRGBA32& color, Camera* camera)
{
	SpriteIntermediateRenderer* renderer = new SpriteIntermediateRenderer();
	renderer->Initialize(matrix, sprite, localAABB, material, layer, color);

	IntermediateRenderers* renderers;
	if (camera != NULL)
		renderers = &camera->GetIntermediateRenderers();
	else
		renderers = &GetScene().GetIntermediateRenderers();
	renderers->Add(renderer, layer);

	return renderer;
}
#endif
