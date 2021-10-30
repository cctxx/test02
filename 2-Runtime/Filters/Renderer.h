#ifndef RENDERER_H
#define RENDERER_H

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Camera/BaseRenderer.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/LinkedList.h"
#include <vector>
#include "Runtime/Modules/ExportModules.h"
#include "Runtime/Camera/SceneNode.h"

class Animation;
class LODGroup;
class Renderer;
struct VisibleNode;

struct RenderStats
{
	int triangleCount;
	int vertexCount;
	int submeshCount;
};

struct EventEntry;

enum {
	// @TBD: tweak to ideal values per platform / device / rendering API
	kDynamicBatchingVerticesThreshold = 300,			// verts (needed at build time since required channels isn't known)
	kDynamicBatchingVertsByChannelThreshold = 300 * 3,	// verts * channels
	kDynamicBatchingIndicesThreshold = 32000			// >32k causes a slowdown on MBPs with AMD cards (Case 394520)
};


class EXPORT_COREMODULE Renderer : public Unity::Component, public BaseRenderer
{
public:
	
	typedef UNITY_VECTOR(kMemRenderer, PPtr<Material>) MaterialArray;
	typedef UNITY_VECTOR(kMemRenderer, UInt32) IndexArray;
	
	REGISTER_DERIVED_ABSTRACT_CLASS (Renderer, Unity::Component)
	DECLARE_OBJECT_SERIALIZE (Renderer)
	
	static void InitializeClass ();
	static void PostInitializeClass ();
	static void CleanupClass ();
	
	Renderer (RendererType type, MemLabelId label, ObjectCreationMode mode);
	// ~Renderer (); declared-in-macro
	
	virtual void SmartReset ();
	virtual void CheckConsistency();
	
	// BaseRenderer
	virtual void RendererBecameVisible();
	virtual void RendererBecameInvisible();
	virtual int GetLayer() const;
	virtual void UpdateTransformInfo();

	// UpdateLocalAABB only needed for derived classes that don't implement UpdateTransformInfo
	virtual void UpdateLocalAABB() {Assert(false);}

	virtual int GetMaterialCount() const		{ return m_Materials.size (); }
	virtual PPtr<Material> GetMaterial(int i) const	{ return m_Materials[i]; }
	Material* GetAndAssignInstantiatedMaterial(int i, bool allowFromEditMode);
	
	virtual int GetSubsetIndex(int i) const { return (m_SubsetIndices.empty())? i: m_SubsetIndices[i]; }
	virtual void SetSubsetIndex(int subsetIndex, int index);
	
	// Set the visibility of a renderer.
	// If false, the renderer doesn't register itself in the scene, and never get any render calls.
	void SetVisible (bool isVisible);
	bool GetVisible () const { return m_Visible; }
	
	bool GetEnabled () const { return m_Enabled; }
	void SetEnabled( bool v );

	void SetMaterialCount (int size);
	void SetMaterial (PPtr<Material> material, int index);
	void ClearSubsetIndices();
	
	PPtr<Transform> GetStaticBatchRoot() const { return m_StaticBatchRoot; }
	void SetStaticBatchRoot (PPtr<Transform> root) { m_StaticBatchRoot = root; }
	
	// Due to batching GameObject and attached Renderer component transforms can differ
	// User following methods to access Renderer transforms or matrix
	Transform const& GetTransform () const;
	Transform& GetTransform ();	
	Matrix4x4f GetWorldToLocalMatrix () const;
	Matrix4x4f GetLocalToWorldMatrix () const;
	
	// from Component
	virtual void Deactivate (DeactivateOperation operation);
	
	void TransformChanged (int changeMask);
	void LayerChanged ();

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	void SetLightmapIndexInt(int index);
	
	void SetLightmapST( const Vector4f& st );
	
	void SetCastShadows( bool f ) { m_CastShadows = f; SetDirty(); }
	void SetReceiveShadows( bool f ) { m_ReceiveShadows = f; SetDirty(); }
	
	void SetMaterialArray( const MaterialArray& m, const IndexArray& i );
	const MaterialArray& GetMaterialArray() { return m_Materials; }
	const IndexArray& GetSubsetIndices() { return m_SubsetIndices; }

	static void UpdateAllRenderersInternal();
	
	#if UNITY_EDITOR
	bool CanSelectedWireframeBeRendered () const;
	void SetSelectedWireframeHidden (bool isHidden) { m_SelectedWireframeHidden = isHidden; };
	virtual void GetRenderStats (RenderStats& renderStats) { memset(&renderStats, 0, sizeof(renderStats)); }
	#endif
	
	bool GetUseLightProbes() const { return m_UseLightProbes; }
	void SetUseLightProbes(bool useLightProbes) { m_UseLightProbes = useLightProbes; SetDirty(); }
	int GetLastLightProbeTetIndex() const { return m_LastLightProbeTetIndex; }
	void SetLastLightProbeTetIndex(int index) { m_LastLightProbeTetIndex = index; }
	PPtr<Transform> GetLightProbeAnchor() const { return m_LightProbeAnchor; }
	void SetLightProbeAnchor (PPtr<Transform> anchor) { m_LightProbeAnchor = anchor; SetDirty(); }

	// Tries to use the light probe anchor, or the RendererNode's AABB center or the WorldAABB's center;
	// likes to get the RendererNode, as that's the fastest
	Vector3f GetLightProbeInterpolationPosition ();
	Vector3f GetLightProbeInterpolationPosition (const AABB& worldBounds);

	
	void SetLODGroup(LODGroup* ptr) { m_LODGroup = ptr; }
	LODGroup* GetLODGroup() { return m_LODGroup; }

	// A callback from the scene to notify
	// that the pointer to the cullnode associated with the renderer has changed
	void NotifySceneHandleChange (SceneHandle cullNode);
	
	SceneHandle GetSceneHandle() { return m_SceneHandle; }

	bool IsInScene() const { return m_SceneHandle != kInvalidSceneHandle; }

	bool IsVisibleInScene () const;

	void UpdateLODGroup ();

	const MaterialPropertyBlock* GetPropertyBlock () const { return m_CustomProperties; }
	void  GetPropertyBlock (MaterialPropertyBlock& outBlock);
	void SetPropertyBlock (const MaterialPropertyBlock& block);
	
	bool HasPropertyBlock() const { return m_CustomProperties != NULL; }
	void ClearPropertyBlock ();
	MaterialPropertyBlock& GetPropertyBlockRememberToUpdateHash ();

	int GetSortingLayer() const { return m_SortingLayer; }
	int GetSortingLayerUserID() const;
	void SetSortingLayerUserID(int id);
	std::string GetSortingLayerName() const;
	void SetSortingLayerName(const std::string& name);
	SInt16 GetSortingOrder() const { return m_SortingOrder; }
	void SetSortingOrder(SInt16 newValue);
	void SetupSortingOverride();

protected:
	
	void UpdateSceneHandle ();
	
	// Update the scene info for the renderer.
	// Registers/unregisters the renderer in the scene.
	// Override in child classes to do custom stuff.
	virtual void UpdateRenderer();
	
	void HealSubsetIndices();
	
	// Should this renderer be in the scene?
	bool ShouldBeInScene() const { return m_Enabled && m_Visible && IsActive (); }

	void SupportedMessagesDidChange (int mask);
	
	void UpdateManagerState( bool needsUpdate );
	
	// Inform a renderer that the aabb has changed.
	void BoundsChanged ();
	
	// Inform a renderer that the layerMask has changed.
	void LayerMaskChanged ();
	
	// Check if material indices are equal to subset indices
	void ValidateSubsetIndexArray ();
	
	bool HasSubsetIndices() const { return !m_SubsetIndices.empty(); }
	bool IsPartOfStaticBatch() const { return HasSubsetIndices(); }
	
	void RemoveFromScene();
		
protected:
	SceneHandle			m_SceneHandle;
	ListNode<Renderer> m_RenderersListNode;
	MaterialArray	    m_Materials;	///< List of materials to use when rendering.
	
private:
	IndexArray		    m_SubsetIndices;
	PPtr<Transform>	    m_StaticBatchRoot;
	
	LODGroup*           m_LODGroup;

  	bool		        m_Enabled;
  	bool		        m_Visible;

	bool		m_UseLightProbes;
	PPtr<Transform> m_LightProbeAnchor; ///< Light probe lighting is interpolated at the center of the renderer's bounds or at the position of the anchor, if assigned.
	int			m_LastLightProbeTetIndex;	// Last light probe tetrahedron this renderer was in, used as a guess for the next frame; don't serialize.

	// Global sorting layer index. Zero is "Default" layer which is always there.
	// Layers before the default one get negative numbers; after default one
	// get positive ones.
	//
	// When the layers are reordered in the inspector, all alive renderers
	// get this updated. Otherwise, at load time the proper sorting layer
	// is fetched from m_SortingLayerID (each layer gets an unique one).
	SInt16       m_SortingLayer;
	SInt16       m_SortingOrder;

#	if UNITY_EDITOR
	// Unique ID of our sorting layer, or zero for default one. Needed to resolve
	// proper final m_SortingLayer value, when the layers were reordered.
	UInt32		m_SortingLayerID;
	bool		        m_SelectedWireframeHidden;
#	endif // if UNITY_EDITOR
};

Transform* GetIdentityTransform();

#endif
