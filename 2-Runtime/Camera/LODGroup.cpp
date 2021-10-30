#include "UnityPrefix.h"
#include "LODGroup.h"
#include "LODGroupManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/AABBUtility.h"
#include "UnityScene.h"

/*
// @TODO:

Ask aras:
 RenderQueue.cpp
 Create( "LIGHTMAP_OFF" );
 Create( "LIGHTMAP_ON" );
HUH???
 
PRI 1
 * Integrate with lightmaps? Probably need an option to reuse lightmap data if the uv's match up exactly between LOD's. Maybe we can automate it?
 * multi_compile doesn't work. Seems like using more properties in a multicompile causes some shaders to drop.
 
 ///@TODO: This should probably be 0. But for now we don't have proper ifdef support for switching to a different subshader.
 #define LOD_FADE_DISABLED 0.999F

 /////****** SHADER LOD FADING  **** 
 ///@TODO: Make it so that fading is automatically disabled based on a shader tag or some shit like that.
 ///@TODO: Expose the fade distance and visualize in inspector
 ///@TODO: Add easy support for LOD fade in surface shaders
 ///@TODO: Switch to a shader when it is not fading to reduce shader complexity
 
PRI 2
 ///@TODO: IntegrationTest: create lodgroup, attach renderer, delete lod group. Enable / disable renderer
 ///@TODO: IntegrationTest: Make sure that m_LODs is never bigger than 8 (because of the lodIndex bitmask)
 
 ///@TODO: GraphicsFunctionalTest / FunctionalTest: Write graphics functional test for LOD & layer based culling and projector especially when an object is being culled by the camera (Make sure it is also culled by projector)
 
 
 ///@TODO: Does SceneManager really have to be recreated on every level load?
 //      This is probably related to the super weird behaviour of PlayerLoadLevel deactivate / activate ...
 
 
 
PRI 3
 *  When calculating static objects pvs data. We could precalculate which static objects can be visible.
    Have to be careful with runtime tweakable distance fudge...

///@TODO: Use case: 
// "I assume a higher LOD can be triggered, for example, for an explosion effect,
// swapping out the unbroken model for a broken one and passing the pieces to the physics engine with random velocities.."

 
*/
 
LODGroup::LODGroup (MemLabelId label, ObjectCreationMode mode)
	:	Super(label, mode)
	,   m_LODGroup (-1)
{
	m_Enabled = true;
}

LODGroup::~LODGroup ()
{
	Assert(m_LODGroup == kInvalidLODGroup);
	Assert(m_CachedRenderers.empty());
}

void LODGroup::Reset ()
{
	Super::Reset();
	m_LocalReferencePoint = Vector3f(0, 0, 0);
	m_Size = 1.0F;
	m_ScreenRelativeTransitionHeight = 0.0;
	m_LODs.clear();
}

void LODGroup::SmartReset ()
{
	Super::SmartReset();
	
	LODGroup::LOD lod;
	lod.screenRelativeHeight = 0.6f;
	m_LODs.push_back( lod );
	lod.screenRelativeHeight = 0.3f;
	m_LODs.push_back( lod );
	lod.screenRelativeHeight = 0.1f;
	m_LODs.push_back( lod );
}

void LODGroup::CheckConsistency ()
{
	Super::CheckConsistency();
	m_LODs.resize(std::min<size_t>(m_LODs.size(), kMaximumLODLevels));
}

void LODGroup::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad (mode);
	UpdateEnabledState(IsActive());
	SyncLODGroupManager();
}

void LODGroup::Deactivate (DeactivateOperation operation)
{
	UpdateEnabledState(false);
	Super::Deactivate (operation);
}

void LODGroup::SetLocalReferencePoint (const Vector3f& ref)
{
	m_LocalReferencePoint = ref;
	SyncLODGroupManager();
	SetDirty();
}

void LODGroup::SetSize (float size)
{
	m_Size = size;
	SyncLODGroupManager();
	SetDirty();
}

void LODGroup::SyncLODGroupManager ()
{
	// Super inefficient...
	if (m_LODGroup != kInvalidLODGroup)
	{
		Cleanup ();
		Create ();
	}
}

void LODGroup::NotifyLODGroupManagerIndexChange (int newIndex)
{
	m_LODGroup = newIndex;
	for (int i=0;i<m_CachedRenderers.size();i++)
	{
		SceneHandle handle = m_CachedRenderers[i]->GetSceneHandle();
		if (handle != kInvalidSceneHandle)
			GetScene().SetRendererLODGroup(handle, newIndex);
	}
}

bool DoesRendererSupportLODFade (Renderer& renderer)
{
	//@TODO:
//	MaterialArray& materials = renderer.GetMaterialArray();
	return false;
}

// Goes through Renderers in the LODArray and sets up their LODGroup pointers & group indices and masks

void LODGroup::RegisterCachedRenderers ()
{
	Assert(m_CachedRenderers.empty());
	Assert(m_LODGroup != kInvalidLODGroup);
	
	bool supportsLODFade = false;
	
	Unity::Scene& scene = GetScene();

	for (int i=0;i<m_LODs.size();i++)
	{
		LODRenderers& renderers = m_LODs[i].renderers;
		
		for (int r=0;r<renderers.size();r++)
		{
			Renderer* renderer = renderers[r].renderer;
			if (renderer == NULL)
				continue;
			
			supportsLODFade |= DoesRendererSupportLODFade (*renderer);
			
			SceneHandle handle = renderer->GetSceneHandle();

			// If the renderer has no LODGroup attached yet, then this is the first time that specific Renderer is used in this LODGroup.
			// Thus we initialize the Group index & LODIndexMask with the current LOD Level
			if (renderer->GetLODGroup () == NULL)
			{
				renderer->SetLODGroup (this);

				// Initialize cull node lodgroup values
				if (handle != kInvalidSceneHandle)
				{
					scene.SetRendererLODGroup(handle, m_LODGroup);
					scene.SetRendererLODIndexMask(handle, 1 << i);
				}
				m_CachedRenderers.push_back(renderer);
			}
			// The renderer is attached to the same LOD group in a previous LOD level.
			// Thus we add the current LOD level to the LODIndexMask
			else if (renderer->GetLODGroup () == this)
			{
				if (handle != kInvalidSceneHandle)
				{
					UInt32 lodIndexMask = GetScene().GetRendererNode(handle).lodIndexMask;
					lodIndexMask |= 1 << i;
					scene.SetRendererLODIndexMask(handle, lodIndexMask);
				}
			}
			// Fail (renderer is used in multiple LODGroups...)
			else
			{
				string warningString = Format("Renderer '%s' is registered with more than one LODGroup ('%s' and '%s').", renderer->GetName(), GetName(), renderer->GetLODGroup ()->GetName());
				WarningStringObject(warningString, renderer);
			}
		}
	}
}

void LODGroup::ClearCachedRenderers ()
{
	for (int i=0;i<m_CachedRenderers.size();i++)
	{
		m_CachedRenderers[i]->SetLODGroup (NULL);
		SceneHandle handle = m_CachedRenderers[i]->GetSceneHandle();
		if (handle != kInvalidSceneHandle)
		{
			Unity::Scene& scene = GetScene();
			scene.SetRendererLODGroup(handle, 0);
			scene.SetRendererLODIndexMask(handle, 0);
		}
	}
	m_CachedRenderers.resize_uninitialized(0);
}

void LODGroup::RemoveFromCachedRenderers (Renderer* renderer)
{
	for (int i=0;i<m_CachedRenderers.size();i++)
	{
		if (m_CachedRenderers[i] == renderer)
		{
			m_CachedRenderers[i] = m_CachedRenderers.back();
			m_CachedRenderers.pop_back();
			return;
		}
	}
}

void LODGroup::GetLODGroupIndexAndMask (Renderer* renderer, UInt32* outGroup, UInt32* outMask)
{
	Assert(m_LODGroup != kInvalidLODGroup);
	
	PPtr<Renderer> rendererPPtr (renderer);
	
	// Compute mask of which LOD 
	UInt32 mask = 0;
	for (int i=0;i<m_LODs.size();i++)
	{
		LODRenderers& renderers = m_LODs[i].renderers;
		for (int r=0;r<renderers.size();r++)
		{
			if (renderers[r].renderer == rendererPPtr)
				mask |= 1 << i;
		}
	}
	
	*outMask = mask;
	*outGroup = m_LODGroup;
}

void LODGroup::SetLODArray (const LODArray& lodArray)
{
	m_LODs = lodArray;
	SyncLODGroupManager();
	SetDirty();
}

const LODGroup::LOD& LODGroup::GetLOD (int index)
{
	Assert (index < GetLODCount());
	return m_LODs[index];
}


Vector3f LODGroup::GetWorldReferencePoint ()
{
	return GetComponent(Transform).TransformPoint(m_LocalReferencePoint);
}

float LODGroup::GetWorldSpaceScale ()
{
	Vector3f scale = GetComponent(Transform).GetWorldScaleLossy();
	float largestAxis;
	largestAxis = Abs(scale.x);
	largestAxis = std::max (largestAxis, Abs(scale.y));
	largestAxis = std::max (largestAxis, Abs(scale.z));
	return largestAxis;
}

float LODGroup::GetWorldSpaceSize ()
{
	return GetWorldSpaceScale () * m_Size;
}

void LODGroup::UpdateEnabledState (bool active)
{
	Cleanup();
	if (active)
		Create();
}

void LODGroup::Create()
{
	if (m_Enabled)
	{
		GetLODGroupManager().AddLODGroup(*this, GetWorldReferencePoint(), GetWorldSpaceSize());
	}
	else
	{
		m_LODGroup = kDisabledLODGroup;
	}
	
	RegisterCachedRenderers();	
}


bool LODGroup::GetEnabled() 
{
	return m_Enabled;
}

void LODGroup::SetEnabled(bool enabled) 
{
	if ((bool)m_Enabled == enabled)
		return;
	m_Enabled = enabled;
	UpdateEnabledState (IsActive ());
	SetDirty ();
}

void LODGroup::Cleanup ()
{
	if (m_LODGroup != kInvalidLODGroup)
	{
		ClearCachedRenderers();
		if (m_LODGroup == kDisabledLODGroup)
			m_LODGroup = kInvalidLODGroup;
		else
			GetLODGroupManager().RemoveLODGroup(*this);
	}
}

void LODGroup::OnTransformChanged (int options) 
{
	if (m_LODGroup != kInvalidLODGroup)
	{
		// Scale changed: update all parameters
		if (options & Transform::kScaleChanged)
			GetLODGroupManager().UpdateLODGroupParameters(m_LODGroup, *this, GetWorldReferencePoint(), GetWorldSpaceSize());	
		// rotation or position changed: fastpath for just changing the reference point
		else
		{
			GetLODGroupManager().UpdateLODGroupPosition(m_LODGroup, GetWorldReferencePoint());	
		}
	}
}

template<class TransferFunction> inline
void LODGroup::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER (m_LocalReferencePoint);
	TRANSFER (m_Size);
	TRANSFER (m_ScreenRelativeTransitionHeight);
	TRANSFER (m_LODs);
	transfer.Transfer (m_Enabled, "m_Enabled", kHideInEditorMask);
}

template<class TransferFunction> inline
void LODGroup::LODRenderer::Transfer (TransferFunction& transfer) 
{
	TRANSFER (renderer);
}

template<class TransferFunction> inline
void LODGroup::LOD::Transfer (TransferFunction& transfer) 
{
	TRANSFER (screenRelativeHeight);
	TRANSFER (renderers);
}

void LODGroup::InitializeClass ()
{
	REGISTER_MESSAGE (LODGroup, kTransformChanged, OnTransformChanged, int);

	InitializeLODGroupManager();
}

void LODGroup::CleanupClass ()
{
	if (GetLODGroupManagerPtr())
		CleanupLODGroupManager();
}

IMPLEMENT_CLASS_HAS_INIT(LODGroup)
IMPLEMENT_OBJECT_SERIALIZE(LODGroup)
