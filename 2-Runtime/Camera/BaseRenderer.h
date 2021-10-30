#ifndef BASE_RENDERER_H
#define BASE_RENDERER_H

#include "Runtime/Math/Vector4.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Modules/ExportModules.h"
#include "Runtime/Camera/RenderLoops/GlobalLayeringData.h"

namespace Unity { class Material; }
class ChannelAssigns;
using namespace Unity;
class AABB;
class Matrix4x4f;
class Vector3f;
class Quaternionf;
class MaterialPropertyBlock;
class Shader;
class Renderer;
template<class T> class PPtr;

enum RendererType {
	kRendererUnknown,
	
	kRendererMesh,
	kRendererSkinnedMesh,
	kRendererCloth,
	kRendererSprite,
	
	kRendererParticle,
	kRendererTrail,
	kRendererLine,
	kRendererParticleSystem,	

	kRendererIntermediate,
	
	kRendererTypeCount
};

struct BatchInstanceData
{
	Matrix4x4f xform;			// 64
	Renderer* renderer;			// 4
	int subsetIndex;			// 4
	int xformType;				// 4
	int dummy;					// 4 byte padding
};

struct TransformInfo
{
	Matrix4x4f     worldMatrix;     // 64
	AABB           worldAABB;       // 24
	AABB           localAABB;       // 24	used by LightManager and Shadows
	float          invScale;        // 4
	TransformType  transformType;   // 4
};

// Abstract base class for renderers.
class EXPORT_COREMODULE BaseRenderer {
public:
	BaseRenderer(RendererType type);
	virtual ~BaseRenderer();
	
	// The main Render Function. Implement this in order to draw the graphics
	// When this is called, the correct material and transform have been set up.
	virtual void Render (int materialIndex, const ChannelAssigns& channels) = 0;

	void GetWorldAABB( AABB& result );
	const TransformInfo& GetTransformInfo ();
	void GetLocalAABB( AABB& result );

	virtual void UpdateTransformInfo() = 0;

	virtual void RendererBecameVisible() { m_IsVisibleInScene = true; }
	virtual void RendererBecameInvisible() { m_IsVisibleInScene = false; }
	virtual int GetLayer() const = 0;
	
	virtual float GetSortingFudge () const { return 0.0f; }

	virtual int GetMaterialCount() const = 0;
	virtual PPtr<Material> GetMaterial(int i) const = 0;
	virtual int GetSubsetIndex(int i) const { return i; }
	virtual int GetStaticBatchIndex() const { return 0; }
	virtual UInt32 GetMeshIDSmall() const { return 0; }

	RendererType GetRendererType() const { return static_cast<RendererType>(m_RendererType); }

	UInt32 GetLayerMask() const { return 1<<GetLayer(); }
	
	bool GetCastShadows() const { return m_CastShadows; }
	bool GetReceiveShadows() const { return m_ReceiveShadows; }

	void ApplyCustomProperties(Unity::Material& mat, Shader* shader, int subshaderIndex) const;
	
	const Vector4f& GetLightmapST() const { return m_LightmapST; }
	// If the renderer's mesh is batched, UVs were already transformed by lightmapST, so return identity transform.
	// The static batch index equal to 0 means there is no batched mesh, i.e. the renderer is not batched.
	const Vector4f GetLightmapSTForRendering () const { return GetStaticBatchIndex() == 0 ? m_LightmapST : Vector4f(1,1,0,0); }

	UInt8 GetLightmapIndex() const { return m_LightmapIndex; }
	int GetLightmapIndexInt() const;

	void SetLightmapIndexIntNoDirty(int index);
	
	bool IsLightmappedForRendering() const;
	bool IsLightmappedForShadows() const;

	#if UNITY_EDITOR
	float GetScaleInLightmap() const 		{ return m_ScaleInLightmap; }
	void SetScaleInLightmap(float scale) 	{ m_ScaleInLightmap = scale; }
	#endif
	
	void ComputeCustomPropertiesHash();
	UInt32 GetCustomPropertiesHash() const { return m_CustomPropertiesHash; }
	const MaterialPropertyBlock* GetCustomProperties() const { return m_CustomProperties; }


	GlobalLayeringData GetGlobalLayeringData () const		{ return m_GlobalLayeringData; }
	void SetGlobalLayeringData (GlobalLayeringData data)	{ m_GlobalLayeringData = data; }
protected:
	Vector4f			m_LightmapST;			///< Lightmap tiling and offset
	UInt8				m_RendererType;			// enum RendererType
	UInt8				m_LightmapIndex;
	bool				m_CastShadows;
	bool				m_ReceiveShadows;
	bool				m_IsVisibleInScene;
#if UNITY_EDITOR
	float				m_ScaleInLightmap;		///< A multiplier to object's area used in atlasing
#endif
	bool                m_TransformDirty;
	bool                m_BoundsDirty;
	TransformInfo       m_TransformInfo;
	MaterialPropertyBlock*	m_CustomProperties;
	UInt32				m_CustomPropertiesHash;
	GlobalLayeringData	m_GlobalLayeringData;
};

inline const TransformInfo& BaseRenderer::GetTransformInfo()
{
	if (m_TransformDirty || m_BoundsDirty)
	{
		UpdateTransformInfo();
		m_TransformDirty = false;
		m_BoundsDirty = false;
	}
	return m_TransformInfo;
}
#endif
