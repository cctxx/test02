#ifndef INTERMEDIATE_RENDERER_H
#define INTERMEDIATE_RENDERER_H

#include "BaseRenderer.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "SceneNode.h"
#include "IntermediateUsers.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Modules/ExportModules.h"

class Mesh;
class Camera;
class Matrix4x4f;
class Vector3f;
class Quaternionf;
class Sprite;

class EXPORT_COREMODULE IntermediateRenderer : public BaseRenderer
{
public:
	IntermediateRenderer ();
	virtual ~IntermediateRenderer();
	
	void Initialize(const Matrix4x4f& matrix, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows);

	// BaseRenderer
	virtual UInt32 GetLayerMask() const { return 1<<m_Layer; }
	virtual int GetLayer() const { return m_Layer; }
	virtual int GetMaterialCount() const { return 1; }
	virtual PPtr<Material> GetMaterial(int i) const { return m_Material; }
	
	virtual void OnAssetDeleted() = 0;
	virtual void OnAssetBoundsChanged();
	
	void SetPropertyBlock( const MaterialPropertyBlock& block )
	{
		m_Properties = block;
		m_CustomProperties = &m_Properties;
		ComputeCustomPropertiesHash();
	}

	#if UNITY_EDITOR
	SInt32 GetInstanceID() const { return m_InstanceID; }
	void SetInstanceID (SInt32 id) { m_InstanceID = id; }
	#endif
	
	virtual void UpdateTransformInfo() {};
	virtual void UpdateAABB() {Assert(false);}

	const AABB& GetCachedWorldAABB () const { return m_TransformInfo.worldAABB; }
	
protected:
	ListNode<IntermediateRenderer> m_Node;
	PPtr<Material>	               m_Material;
	MaterialPropertyBlock          m_Properties;
	int                            m_Layer;

	#if UNITY_EDITOR
	SInt32			m_InstanceID;
	#endif
};



class EXPORT_COREMODULE MeshIntermediateRenderer : public IntermediateRenderer
{
public:
	MeshIntermediateRenderer();
	virtual ~MeshIntermediateRenderer();

	void Initialize(const Matrix4x4f& matrix, Mesh* mesh, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex);

	// BaseRenderer
	virtual void Render(int materialIndex, const ChannelAssigns& channels);

	virtual void OnAssetDeleted();

	static void StaticInitialize ();
	static void StaticDestroy ();

private:
	// Note: not using per-frame linear allocator, because in the editor
	// it can render multiple frames using single player loop run (e.g. when editor is paused).
	// Clearing per-frame data and then trying to use it later leads to Bad Things.
	DECLARE_POOLED_ALLOC(MeshIntermediateRenderer);

	Mesh* m_Mesh;
	int   m_SubMeshIndex;
};



#if ENABLE_SPRITES
class EXPORT_COREMODULE SpriteIntermediateRenderer : public IntermediateRenderer
{
public:
	SpriteIntermediateRenderer();
	virtual ~SpriteIntermediateRenderer();

	void Initialize(const Matrix4x4f& matrix, Sprite* sprite, const AABB& localAABB, PPtr<Material> material, int layer, const ColorRGBA32& color);

	// BaseRenderer
	virtual void Render(int materialIndex, const ChannelAssigns& channels);

	virtual void OnAssetDeleted();

	static void StaticInitialize ();
	static void StaticDestroy ();

private:
	// Note: not using per-frame linear allocator, because in the editor
	// it can render multiple frames using single player loop run (e.g. when editor is paused).
	// Clearing per-frame data and then trying to use it later leads to Bad Things.
	DECLARE_POOLED_ALLOC(SpriteIntermediateRenderer);

	Sprite* m_Sprite;
	ColorRGBAf   m_Color;
};
#endif



class IntermediateRenderers
{
public:
	void Clear( size_t startIndex = 0 );
	
	const AABB*      GetBoundingBoxes () const;
	const SceneNode* GetSceneNodes () const;
	size_t           GetRendererCount () const { return m_BoundingBoxes.size(); }
	
	void Add(IntermediateRenderer* renderer, int layer);

private:
	dynamic_array<SceneNode> m_SceneNodes;
	dynamic_array<AABB>      m_BoundingBoxes;
};

IntermediateRenderer*  AddMeshIntermediateRenderer( const Matrix4x4f& matrix, Mesh* mesh, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex, Camera* camera );
IntermediateRenderer*  AddMeshIntermediateRenderer( const Matrix4x4f& matrix, Mesh* mesh, const AABB& localAABB, PPtr<Material> material, int layer, bool castShadows, bool receiveShadows, int submeshIndex , Camera* camera );

#if ENABLE_SPRITES
IntermediateRenderer*  AddSpriteIntermediateRenderer(const Matrix4x4f& matrix, Sprite* sprite, PPtr<Material> material, int layer, const ColorRGBA32& color, Camera* camera);
IntermediateRenderer*  AddSpriteIntermediateRenderer(const Matrix4x4f& matrix, Sprite* sprite, const AABB& localAABB, PPtr<Material> material, int layer, const ColorRGBA32& color, Camera* camera);
#endif

#endif
