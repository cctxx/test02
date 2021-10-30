#ifndef SHURIKENRENDERER_H
#define SHURIKENRENDERER_H

#include "Runtime/Filters/Renderer.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"




class Mesh;
class MinMaxAABB;
struct ParticleSystemVertex;

struct ParticleSystemRendererData
{
	// Must match the one in RendererModuleUI.cs
	enum { kMaxNumParticleMeshes = 4 };

	int		renderMode;				///< enum { Billboard = 0, Stretched = 1, Horizontal Billboard = 2, Vertical Billboard = 3, Mesh = 4 }
	int		sortMode;				///< enum { None = 0, By Distance = 1, Youngest First = 2, Oldest First = 3 }
	float	maxParticleSize;		///< How large is a particle allowed to be on screen at most? 1 is entire viewport. 0.5 is half viewport.
	float	cameraVelocityScale;	///< How much the camera motion is factored in when determining particle stretching.
	float	velocityScale;			///< When Stretch Particles is enabled, defines the length of the particle compared to its velocity.
	float	lengthScale;			///< When Stretch Particles is enabled, defines the length of the particle compared to its width.
	float	sortingFudge;			///< Lower the number, most likely that these particles will appear in front of other transparent objects, including other particles.	
	float	normalDirection;		///< Value between 0.0 and 1.0. If 1.0 is used, normals will point towards camera. If 0.0 is used, normals will point out in the corner direction of the particle.
	Mesh*	cachedMesh[kMaxNumParticleMeshes];

	/// Node hooked into the mesh user list of cached meshes so we get notified
	/// when a mesh goes away.
	///
	/// NOTE: Must be initialized properly after construction to point to the
	///		  ParticleSystemRenderer.
	ListNode<Object> cachedMeshUserNode[kMaxNumParticleMeshes];
};


enum ParticleSystemRenderMode {
	kSRMBillboard = 0,
	kSRMStretch3D = 1,
	kSRMBillboardFixedHorizontal = 2,
	kSRMBillboardFixedVertical = 3,
	kSRMMesh = 4,
};

enum ParticleSystemSortMode
{ 
	kSSMNone,
	kSSMByDistance, 
	kSSMYoungestFirst, 
	kSSMOldestFirst,
};

struct ParticleSystemParticles;
struct ParticleSystemParticlesTempData;
class ParticleSystem;
struct ParticleSystemGeomConstInputData;
class ParticleSystemRenderer : public Renderer {
public:
	REGISTER_DERIVED_CLASS (ParticleSystemRenderer, Renderer)
	DECLARE_OBJECT_SERIALIZE (ParticleSystemRenderer)
	static void InitializeClass ();

	ParticleSystemRenderer (MemLabelId label, ObjectCreationMode mode);
	// ParticleSystemRenderer(); declared-by-macro
	
	virtual void Render (int materialIndex, const ChannelAssigns& channels);
	static void RenderMultiple (const BatchInstanceData* instances, size_t count, const ChannelAssigns& channels);

	virtual void GetLocalAABB (AABB& result);
	virtual void GetWorldAABB (AABB& result);
	virtual float GetSortingFudge () const;
	
	virtual void CheckConsistency ();
	virtual void Reset ();
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
		
	void Update (const AABB& aabb);
	void UpdateLocalAABB();
	
	virtual void RendererBecameVisible();
	virtual void RendererBecameInvisible();

	static void SetUsesAxisOfRotationRec(ParticleSystem& system, bool first);
	static void CombineBoundsRec(ParticleSystem& shuriken, MinMaxAABB& aabb, bool first);

	GET_SET_DIRTY (ParticleSystemRenderMode, RenderMode, m_Data.renderMode) ;
	GET_SET_DIRTY (ParticleSystemSortMode, SortMode, m_Data.sortMode) ;
	GET_SET_DIRTY (float, MaxParticleSize, m_Data.maxParticleSize) ;
	GET_SET_DIRTY (float, CameraVelocityScale, m_Data.cameraVelocityScale) ;
	GET_SET_DIRTY (float, VelocityScale, m_Data.velocityScale) ;
	GET_SET_DIRTY (float, LengthScale, m_Data.lengthScale) ;
	void SetMesh (PPtr<Mesh> mesh) { m_Mesh[0] = mesh; SetDirty(); UpdateCachedMesh (); }
	PPtr<Mesh> GetMesh () const { return m_Mesh[0]; }

	const PPtr<Mesh>* GetMeshes () const { return m_Mesh; }
	const ParticleSystemRendererData& GetData() const { return m_Data; }
	
#if UNITY_EDITOR
	bool GetEditorEnabled() const { return m_EditorEnabled; }
	void SetEditorEnabled(bool value) { m_EditorEnabled = value; }
#endif

	// For mesh we use world space rotation, else screen space
	bool GetScreenSpaceRotation() const { return m_Data.renderMode != kSRMMesh; };
private:
	// from Renderer
	virtual void UpdateRenderer ();
	void UpdateCachedMesh ();
	void OnDidDeleteMesh (Mesh* mesh);

private:
	static void CalculateTotalParticleCount(UInt32& totalNumberOfParticles, ParticleSystem& shuriken, bool first);
	static void CombineParticleBuffersRec(int& offset, ParticleSystemParticles& particles, ParticleSystemParticlesTempData& psTemp, ParticleSystem& shuriken, bool first, bool needsAxisOfRotation);

	static void RenderBatch (const BatchInstanceData* instances, size_t count, size_t numParticles, const ChannelAssigns& channels);
	static void RenderInternal (ParticleSystem& system, const ParticleSystemRenderer& renderer, const ChannelAssigns& channels, ParticleSystemVertex* vbPtr, UInt32 particleCountTotal);

	ParticleSystemRendererData m_Data;
	dynamic_array<UInt16> m_CachedIndexBuffer[ParticleSystemRendererData::kMaxNumParticleMeshes];
	PPtr<Mesh> m_Mesh[ParticleSystemRendererData::kMaxNumParticleMeshes];
	
	AABB	m_LocalSpaceAABB;

#if UNITY_EDITOR
	bool m_EditorEnabled;
#endif
};

#endif // SHURIKENRENDERER_H
