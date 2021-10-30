#include "UnityPrefix.h"
#include "Renderer.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Camera/UnityScene.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Culler.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/BaseClasses/EventIDs.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Modules/ExportModules.h"
#include "RendererAnimationBinding.h"

using namespace Unity;

IMPLEMENT_CLASS_HAS_POSTINIT (Renderer)
IMPLEMENT_OBJECT_SERIALIZE (Renderer)
INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED (Renderer)

typedef List< ListNode<Renderer> > RendererList;
static RendererList gRenderersToUpdate;


static Transform *gIdentityTransform = NULL;

Transform* GetIdentityTransform() { return gIdentityTransform; }

Renderer::Renderer (RendererType type, MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	BaseRenderer(type)
,	m_LODGroup (NULL)
,	m_SceneHandle(kInvalidSceneHandle)
,	m_RenderersListNode(this)
,	m_Enabled(true)
,	m_Visible(true)
//,	m_Materials (Renderer::MaterialArray::allocator_type (*baseAllocator))
#if UNITY_EDITOR
,	m_SelectedWireframeHidden(false)
#endif
,	m_UseLightProbes(false)
,	m_LastLightProbeTetIndex(-1)
,	m_SortingLayer(0)
,	m_SortingOrder(0)
#if UNITY_EDITOR
,	m_SortingLayerID(0)
#endif
{

}

void Renderer::SmartReset ()
{
	Super::SmartReset();
	SetMaterialCount (1);
}

Renderer::~Renderer ()
{
	DebugAssert (!IsInScene());
	delete m_CustomProperties;

	if (m_LODGroup)
		m_LODGroup->RemoveFromCachedRenderers(this);
}

void Renderer::CleanupClass()
{
	Assert(gIdentityTransform != NULL);
	gIdentityTransform = NULL;
	
	CleanupRendererAnimationBindingInterface ();
}

void Renderer::InitializeClass()
{
	REGISTER_MESSAGE (Renderer, kTransformChanged, TransformChanged, int);
	REGISTER_MESSAGE_VOID (Renderer, kLayerChanged, LayerChanged);
	
	InitializeRendererAnimationBindingInterface ();
}

void Renderer::PostInitializeClass()
{
	Assert(gIdentityTransform == NULL);

	GameObject* go = CreateObjectFromCode<GameObject> ();
	gIdentityTransform = CreateObjectFromCode<Transform> ();
	GameObject::AddComponentInternal(*go, *gIdentityTransform);
	go->SetHideFlags(kHideAndDontSave);
	Assert(!go->IsActive());
}

bool Renderer::IsVisibleInScene () const
{
	Assert ((m_IsVisibleInScene && IsInScene()) == m_IsVisibleInScene);
	return m_IsVisibleInScene;
}

void Renderer::RendererBecameVisible()
{
	BaseRenderer::RendererBecameVisible ();

	InvokeEvent (kBecameVisibleEvent);

	SendMessage (kBecameVisible);
}

void Renderer::RendererBecameInvisible()
{
	BaseRenderer::RendererBecameInvisible ();

	SendMessage (kBecameInvisible);

	InvokeEvent (kBecameInvisibleEvent);
}

int Renderer::GetLayer() const
{
	return GetGameObject().GetLayer();
}


void Renderer::SetVisible (bool visible)
{
	m_Visible = visible;

	bool shouldBeInScene = ShouldBeInScene();
	if (shouldBeInScene == IsInScene())
		return;

	if (!shouldBeInScene)
	{
		// Remove from scene immediately
		RemoveFromScene ();
		UpdateManagerState( false );
		
		InvokeEvent(kBecameInvisibleEvent);
	}
	else
	{
		// Add to scene in renderers update
		UpdateManagerState( true );
	}
}

void Renderer::BoundsChanged ()
{
	m_BoundsDirty = true;
	if (IsInScene())
		GetScene().SetDirtyAABB(m_SceneHandle);
}

void Renderer::LayerMaskChanged ()
{
	if (IsInScene())
		GetScene().SetRendererLayer(m_SceneHandle, GetLayer());
}

void Renderer::UpdateManagerState( bool needsUpdate )
{
	if( needsUpdate == m_RenderersListNode.IsInList() )
		return;
	if( needsUpdate )
		gRenderersToUpdate.push_front(m_RenderersListNode);
	else
		m_RenderersListNode.RemoveFromList();
}

void Renderer::UpdateAllRenderersInternal()
{
	// Update the renderers from the update list:
	// - before updating each, remove from the list
	// - a renderer can add itself again, so only process the original list length
	RendererList::iterator next, listEnd = gRenderersToUpdate.end();
	for( RendererList::iterator i = gRenderersToUpdate.begin(); i != listEnd; i = next )
	{
		next = i;
		next++;
		Renderer& renderer = **i;
		renderer.m_RenderersListNode.RemoveFromList();
		renderer.UpdateRenderer();
	}
}

void Renderer::NotifySceneHandleChange (SceneHandle handle)
{
	m_SceneHandle = handle;
}

void Renderer::UpdateLODGroup ()
{
	if (!IsInScene())
		return;
	Unity::Scene& scene = GetScene();
	DebugAssert (scene.GetRendererNode(m_SceneHandle).renderer == this);

	UInt32 lodGroup = 0;
	UInt32 lodIndexMask = 0;
	if (m_LODGroup != NULL)
	{
		m_LODGroup->GetLODGroupIndexAndMask(this, &lodGroup, &lodIndexMask);
	}
	scene.SetRendererLODGroup(m_SceneHandle, lodGroup);
	scene.SetRendererLODIndexMask(m_SceneHandle, lodIndexMask);
}

void Renderer::UpdateSceneHandle ()
{
	if (!IsInScene())
		return;
	Unity::Scene& scene = GetScene();
	DebugAssert (scene.GetRendererNode(m_SceneHandle).renderer == this);

	AABB worldAABB;
	GetWorldAABB (worldAABB);
	scene.SetRendererAABB(m_SceneHandle, worldAABB);

	bool needsCullCallback = (GetGameObject().GetSupportedMessages() & kHasOnWillRenderObject) != 0;
	scene.SetRendererNeedsCullCallback(m_SceneHandle, needsCullCallback);
	scene.SetRendererLayer(m_SceneHandle, GetLayer());

	UpdateLODGroup();
}

void Renderer::UpdateTransformInfo ()
{
	Transform const& transform = GetTransform();
	if(m_TransformDirty)
	{
		m_TransformInfo.invScale = 1.0f;
		// will return a cached matrix most of the time
		m_TransformInfo.transformType = transform.CalculateTransformMatrix (m_TransformInfo.worldMatrix);
	}

	if(m_BoundsDirty)
		UpdateLocalAABB ();

	TransformAABB( m_TransformInfo.localAABB, m_TransformInfo.worldMatrix, m_TransformInfo.worldAABB );

	if (IsNoScaleTransform(m_TransformInfo.transformType))
		return;

	// run slow path for non uniform scale
	Matrix4x4f scaleOnly;
	float scale;
	TransformType type = transform.CalculateTransformMatrixDisableNonUniformScale (m_TransformInfo.worldMatrix, scaleOnly, scale);
	Assert (type == m_TransformInfo.transformType);

	m_TransformInfo.invScale = 1.0F / scale;

	if (IsNonUniformScaleTransform(type))
	{
		// must recalculate this since it is changed with the following transform
		UpdateLocalAABB ();
		TransformAABB (m_TransformInfo.localAABB, scaleOnly, m_TransformInfo.localAABB);
	}
}

void Renderer::UpdateRenderer ()
{
	if (ShouldBeInScene ())
	{
		if (!IsInScene())
		{
			m_SceneHandle = GetScene().AddRenderer (this);
		}
		Assert (m_SceneHandle != kInvalidSceneHandle);
		UpdateSceneHandle ();
	}
	else
	{
		// This should not be necessary but fixes a weird bug where a mesh is
		// being leaked when disabled before loading a scene. Happens in the fps tutorial.
		RemoveFromScene ();
	}
}

void  Renderer::SetPropertyBlock (const MaterialPropertyBlock& block)
{
	delete m_CustomProperties;
	m_CustomProperties = new MaterialPropertyBlock (block);
	ComputeCustomPropertiesHash();
}

void  Renderer::GetPropertyBlock (MaterialPropertyBlock& outBlock)
{
	if (!m_CustomProperties)
	{
		outBlock.Clear();
		return;
	}
	outBlock = *m_CustomProperties;
}

void Renderer::ClearPropertyBlock ()
{
	delete m_CustomProperties;
	m_CustomProperties = NULL;
	ComputeCustomPropertiesHash();
}

MaterialPropertyBlock& Renderer::GetPropertyBlockRememberToUpdateHash ()
{
	if (!m_CustomProperties)
		m_CustomProperties = new MaterialPropertyBlock ();
	return *m_CustomProperties;
}


void Renderer::SetEnabled (bool newEnabled)
{
	m_Enabled = newEnabled;
	SetDirty();
	SetVisible(m_Visible);
}

void Renderer::Deactivate (DeactivateOperation operation)
{
	RemoveFromScene ();
	UpdateManagerState( false );
	Super::Deactivate (operation);
}

/// Set how many materials this renderer uses
/// @param size the number of materials.
void Renderer::SetMaterialCount (int size)
{
	const size_t oldSize = m_Materials.size ();

	if (size != (int)oldSize)
	{
		Assert(m_SubsetIndices.empty() || m_SubsetIndices.size() == m_Materials.size());

		resize_trimmed (m_Materials, size);
		HealSubsetIndices();

		SetDirty ();
		BoundsChanged();
	}
}

void Renderer::HealSubsetIndices()
{
	// We are using batching and our subset indices have gone out of sync
	if (!m_SubsetIndices.empty() && m_SubsetIndices.size() != m_Materials.size())
	{
		int oldSize = m_SubsetIndices.size();
		resize_trimmed (m_SubsetIndices, m_Materials.size());
		// All new subset indices get the
		for (size_t q = oldSize; q < m_SubsetIndices.size(); ++q)
			m_SubsetIndices[q] = (UInt32)q;
		BoundsChanged();
	}
}

void Renderer::RemoveFromScene ()
{
	if (!IsInScene())
		return;

	bool wasVisible = IsVisibleInScene ();

	BaseRenderer* remRenderer = GetScene ().RemoveRenderer (m_SceneHandle);
	Assert(remRenderer == this);

	m_SceneHandle = kInvalidSceneHandle;

	if (wasVisible)
		RendererBecameInvisible ();
}

void Renderer::CheckConsistency()
{
	Super::CheckConsistency();

	HealSubsetIndices();
}

void Renderer::ClearSubsetIndices()
{
	m_SubsetIndices.clear();
	SetDirty ();
	BoundsChanged();
}

void Renderer::SetSubsetIndex (int index, int subsetIndex)
{
	if (m_SubsetIndices.empty())
	{
		resize_trimmed (m_SubsetIndices, m_Materials.size());
		for (size_t q = 0; q < m_Materials.size(); ++q)
			m_SubsetIndices[q] = (UInt32)q;
	}

	Assert(index < m_SubsetIndices.size());
	Assert (m_Materials.size() == m_SubsetIndices.size());
	m_SubsetIndices[index] = subsetIndex;
	SetDirty ();
	BoundsChanged();
}

/// Set a given Material.
/// @param material the material to assign
/// @param index the index to assign the material to.
void Renderer::SetMaterial (PPtr<Material> material, int index)
{
	Assert (index < m_Materials.size());
	m_Materials[index] = material;

	/*
	#if !DEPLOY_OPTIMIZED
	Material* materialPtr = material;
	if (materialPtr && materialPtr->GetOwner ().GetInstanceID () != 0 && materialPtr->GetOwner() != PPtr<Object> (this))
	{
		ErrorString("Assigning an instantiated material is not a good idea. Since the material is owned by another game object, it will be destroyed when the game object is destroyed.\nYou probably want to explicitly instantiate the material.");
	}
	#endif
	*/

	SetDirty ();
}

void Renderer::SetMaterialArray( const MaterialArray& m, const IndexArray& i )
{
	m_Materials = m;
	m_SubsetIndices = i;
}

Material* Renderer::GetAndAssignInstantiatedMaterial(int i, bool allowFromEditMode)
{
	// Grab shared material
	Material* material = NULL;
	if (GetMaterialCount () > i)
		material = GetMaterial (i);

	// instantiate material if necessary
	Material* instantiated = &Material::GetInstantiatedMaterial (material, *this, allowFromEditMode);

	// Assign material
	if (material != instantiated)
	{
		SetMaterialCount (std::max(GetMaterialCount (), i + 1));
		SetMaterial (instantiated, i);
	}

	return instantiated;
}

Transform& Renderer::GetTransform()
{
	if (!IsPartOfStaticBatch())
		return GetComponent(Transform);

	if (!m_StaticBatchRoot.IsNull())
		return *m_StaticBatchRoot;

	return *gIdentityTransform;
}

Transform const& Renderer::GetTransform() const
{
	return const_cast<Renderer*>(this)->GetTransform();
}

Matrix4x4f Renderer::GetWorldToLocalMatrix () const
{
	return GetTransform().GetWorldToLocalMatrix();
}

Matrix4x4f Renderer::GetLocalToWorldMatrix () const
{
	return GetTransform().GetLocalToWorldMatrix();
}


// Receiver for the TransformChanged message.
// Updates the scene info with the new bounds.
void Renderer::TransformChanged (int changeMask)
{
	m_TransformDirty = true;
	BoundsChanged ();
}

// Receiver for the LayerChanged message.
// Updates the scene info with the new layerMask.
void Renderer::LayerChanged ()
{
	LayerMaskChanged ();
}

void Renderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	// Materials might have been changed eg. from the property editor
	// When loading from disk the materials are only being restored not changed
	if ((awakeMode & kDidLoadFromDisk) == 0)
	{
		// m_Enabled might have changed - update visibility status
		SetVisible(m_Visible);
	}

	UpdateManagerState( IsActive() );
	SetupSortingOverride();
}

void Renderer::SupportedMessagesDidChange (int mask)
{
	Super::SupportedMessagesDidChange(mask);

	if (IsInScene())
	{
		bool needsCullCallback = (GetGameObject().GetSupportedMessages() & kHasOnWillRenderObject) != 0;
		GetScene().SetRendererNeedsCullCallback(m_SceneHandle, needsCullCallback);
	}
}

void Renderer::SetLightmapIndexInt(int index)
{
	UInt8 oldIndex = m_LightmapIndex;
	SetLightmapIndexIntNoDirty (index);
	if (oldIndex != m_LightmapIndex)
		SetDirty();
}

void Renderer::SetLightmapST( const Vector4f& st )
{
	if (st != m_LightmapST)
	{
		m_LightmapST = st;
		SetDirty();
	}
}


#if UNITY_EDITOR
bool Renderer::CanSelectedWireframeBeRendered () const
{
	return m_Enabled && m_Visible && IsInScene() && !m_SelectedWireframeHidden;
}
#endif

template<class TransferFunction>
void Renderer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer(m_Enabled, "m_Enabled", kHideInEditorMask);
	transfer.Transfer(m_CastShadows, "m_CastShadows");
	transfer.Transfer(m_ReceiveShadows, "m_ReceiveShadows");
	transfer.Transfer(m_LightmapIndex, "m_LightmapIndex", kHideInEditorMask | kDontAnimate);
	transfer.Transfer(m_LightmapST, "m_LightmapTilingOffset", kHideInEditorMask | kDontAnimate);
	transfer.Transfer (m_Materials, "m_Materials");
	transfer.Transfer (m_SubsetIndices, "m_SubsetIndices", kHideInEditorMask);
	transfer.Transfer (m_StaticBatchRoot, "m_StaticBatchRoot", kHideInEditorMask);
	TRANSFER (m_UseLightProbes);
	transfer.Align();
	TRANSFER (m_LightProbeAnchor);
	#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease())
		transfer.Transfer (m_ScaleInLightmap, "m_ScaleInLightmap", kHideInEditorMask | kDontAnimate);
	#endif
	transfer.Align();

	transfer.Transfer (m_SortingLayer, "m_SortingLayer", kHideInEditorMask);
	transfer.Transfer (m_SortingOrder, "m_SortingOrder", kHideInEditorMask);
	// In the editor, we also transfer global sorting layer ID, so we can derive final sorting value
	// in case layers are reordered / etc.
	TRANSFER_EDITOR_ONLY_HIDDEN(m_SortingLayerID);
}

Vector3f Renderer::GetLightProbeInterpolationPosition (const AABB& worldBounds)
{
	Transform* anchor = m_LightProbeAnchor;
	if (anchor != NULL)
		return anchor->GetPosition();
	
	return worldBounds.GetCenter();
}


Vector3f Renderer::GetLightProbeInterpolationPosition ()
{
	Transform* anchor = m_LightProbeAnchor;
	if (anchor != NULL)
		return anchor->GetPosition();

	AABB aabb;
	GetWorldAABB(aabb);
	if (aabb.IsValid())
		return aabb.GetCenter();

	return Vector3f(0,0,0);
}



int Renderer::GetSortingLayerUserID() const
{
	if (m_SortingLayer == 0)
		return 0;
	return ::GetSortingLayerUserIDFromValue(m_SortingLayer);
}

void Renderer::SetSortingLayerUserID(int id)
{
	int layerValue = GetSortingLayerValueFromUserID(id);
	if (m_SortingLayer == layerValue)
		return;

#	if UNITY_EDITOR
	if (layerValue == 0)
		m_SortingLayerID = 0;
	else
		m_SortingLayerID = ::GetSortingLayerUniqueIDFromValue(layerValue);
#	endif

	m_SortingLayer = layerValue;
	SetupSortingOverride();
	SetDirty();
}

std::string Renderer::GetSortingLayerName() const
{
	if (m_SortingLayer == 0)
		return std::string();
	return ::GetSortingLayerNameFromValue(m_SortingLayer);
}

void Renderer::SetSortingLayerName(const std::string& name)
{
	int layerValue = ::GetSortingLayerValueFromName(name);
	int layerUserID = ::GetSortingLayerUserIDFromValue(layerValue);
	SetSortingLayerUserID (layerUserID);
}


void Renderer::SetSortingOrder(SInt16 newValue)
{
	if (m_SortingOrder == newValue)
		return;

	m_SortingOrder = newValue;
	SetupSortingOverride();
}

void Renderer::SetupSortingOverride()
{
	// In editor, make sure our final layer sorting value is up to date
	// (might have changed behind our backs by global layer reordering).
#	if UNITY_EDITOR
	m_SortingLayer = GetSortingLayerValueFromUniqueID(m_SortingLayerID);
#	endif // if UNITY_EDITOR

	GlobalLayeringData gld = GlobalLayeringDataCleared();
	gld.layer = m_SortingLayer;
	gld.order = m_SortingOrder;
	SetGlobalLayeringData(gld);
}
