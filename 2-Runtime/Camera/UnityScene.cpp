#include "UnityPrefix.h"
#include "UnityScene.h"
#include "IntermediateRenderer.h"
#include "SceneSettings.h"
#include "CullingParameters.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Camera/OcclusionPortal.h"
#include "UmbraBackwardsCompatibility.h"

///@TODO: Test for removing static renderers in pvs.
// @TODO: Test to check that enable / disable renderer keeps layer & bounding volume correctly.
//@TODO: Test for destroying all renderers during OnBecameInvisible callback

//* Should we use setDynamicObjects?
//@TODO: When a gate is baked. Make sure that the majority of the area is not tagged as occluded. (Eg. someone tagged static data around it)
//* Culling test when static objects have been deleted after baking...

namespace Unity
{

static Scene* gScene = NULL;

Scene::Scene ()
{
	m_UmbraQuery = NULL;
	m_PreventAddRemoveRenderer = 0;
	m_RequestStaticPVSRebuild = false;
	m_GateState = NULL;
}

Scene::~Scene ()
{
	Assert(m_RendererNodes.empty());

	ClearIntermediateRenderers();
	CleanupUmbra();
}

void Scene::ClearIntermediateRenderers()
{
	m_IntermediateNodes.Clear();
}

void Scene::CleanupUmbra ()
{
	// The scene class does not own the tome (SceneSettings does)
    m_UmbraTome = UmbraTomeData();
	
	if (m_UmbraQuery)
		delete m_UmbraQuery;
	m_UmbraQuery = NULL;
	
	delete[] m_GateState;
	m_GateState = NULL;
	
	// Cleanup references to PVS handle
	dynamic_array<SceneNode>::iterator it, itEnd = m_RendererNodes.end();
	for (it = m_RendererNodes.begin(); it != itEnd; ++it)
	{
		SceneNode& node = *it;
		node.pvsHandle = -1;
	}

	///////@TODO: Cleanup all OcclusionPortals gateIndices
	
	// Cleanup any renderer nodes which have already been deleted, but were kept around to keep the indices of pvs renderers
	// in sync with umbra
	for (int i=0;i<m_RendererNodes.size();i++)
	{
		if (m_RendererNodes[i].renderer == NULL)
		{	
			RemoveRenderer(i);
			i--;
		}
	}
}

void Scene::CleanupPVSAndRequestRebuild	()
{
	CleanupUmbra();
	m_RequestStaticPVSRebuild = true;
}
	
void Scene::InitializeUmbra()
{
	RecalculateDirtyBounds();
	CleanupUmbra();

	m_UmbraTome = GetSceneSettings().GetUmbraTome();
	
	if (!m_UmbraTome.HasTome())
		return;

	Assert(m_PendingRemoval.empty());

	const dynamic_array<PPtr<Renderer> >& pvsObjectArray = GetSceneSettings().GetPVSObjectArray();
	const dynamic_array<PPtr<OcclusionPortal> >& portalsArray = GetSceneSettings().GetPortalsArray();
	
    int objectCount = UMBRA_TOME_METHOD(m_UmbraTome, getObjectCount());
	// Setup pvs handles in renderer nodes
    for (int i = 0; i < objectCount; i++)
	{
		Umbra::UINT32 userId = UMBRA_TOME_METHOD(m_UmbraTome, getObjectUserID(i));
		Assert(userId < pvsObjectArray.size());
		if (userId >= pvsObjectArray.size())
			continue;
		
		// Don't do any loading from disk to avoid weird corner cases and recursion - You never know
		Object* obj = Object::IDToPointer(pvsObjectArray[userId].GetInstanceID());
		Renderer* renderer = dynamic_pptr_cast<Renderer*> (obj);
		SceneHandle handle = -1;
		if (renderer)
			handle = renderer->GetSceneHandle();

		// Objects might have been deleted after baking. In this case we need to create holes in the PVS object array
		// For each pvs object that can't be found we add a NULL renderer
		if (handle < 0 || handle >= m_RendererNodes.size())
			handle = AddRendererInternal (NULL, 0, AABB());
		
		m_RendererNodes[handle].pvsHandle = i;
	}
	
	Assert(objectCount <= m_RendererNodes.size());
	
	// Make sure that the pvs handles match the index in the renderer nodes array
	// This way we can do very fast lookups.
	for (int i=0;i<m_RendererNodes.size();i++)
	{
		while (m_RendererNodes[i].pvsHandle != -1 && m_RendererNodes[i].pvsHandle != i)
		{
			SInt32 pvsIndex = m_RendererNodes[i].pvsHandle;

			std::swap(m_RendererNodes[i], m_RendererNodes[pvsIndex]);
			std::swap(m_BoundingBoxes[i], m_BoundingBoxes[pvsIndex]);
			std::swap(m_VisibilityBits[i], m_VisibilityBits[pvsIndex]);

			Renderer* rendererI = static_cast<Renderer*> (m_RendererNodes[i].renderer);
			if (rendererI)
				rendererI->NotifySceneHandleChange(i);
			
			Renderer* rendererPVS = static_cast<Renderer*> (m_RendererNodes[pvsIndex].renderer);
			if (rendererPVS)
				rendererPVS->NotifySceneHandleChange(pvsIndex);
		}
	}	
	
	for (int i=0;i<m_RendererNodes.size();i++)
	{
		Assert(m_RendererNodes[i].pvsHandle == -1 || m_RendererNodes[i].pvsHandle == i);
		Assert(m_RendererNodes[i].renderer == NULL || static_cast<Renderer*> (m_RendererNodes[i].renderer)->GetSceneHandle() == i);
	}
	// Create query
	m_UmbraQuery = new Umbra::QueryExt(m_UmbraTome.tome);
	
	// Setup portals by querying the renderer and grabbing the OcclusionPortal on the same game object
	int gateCount = UMBRA_TOME_METHOD(m_UmbraTome, getGateCount());
	if (gateCount != 0 && !portalsArray.empty())
	{
		m_GateState = new UInt8[UMBRA_TOME_METHOD(m_UmbraTome, getGateStateSize())];
		memset(m_GateState, 0, UMBRA_TOME_METHOD(m_UmbraTome, getGateStateSize()));
		
		Umbra::GateStateVector gateVector (m_GateState, 0, false);

		SetGateStates (m_UmbraQuery, m_UmbraTome, gateVector);
        
		for (int i = 0; i < gateCount; i++)
		{
			// Umbra userID's need to be unique. They are allocated to be after the static renderers/
			Umbra::UINT32 userId = UMBRA_TOME_METHOD(m_UmbraTome, getGateUserID(i)) - pvsObjectArray.size();
			Assert(userId < portalsArray.size());
			if (userId >= portalsArray.size())
				continue;
			
			// Don't do any loading from disk to avoid weird corner cases and recursion - You never know
			Object* obj = Object::IDToPointer(portalsArray[i].GetInstanceID());
			OcclusionPortal* portal = dynamic_pptr_cast<OcclusionPortal*> (obj);
			if (portal)
			{
				portal->SetPortalIndex(i);
				gateVector.setState(i, portal->CalculatePortalEnabled());
			}
		}
	}
}
	

size_t Scene::GetStaticObjectCount () const
{
    if (!m_UmbraTome.HasTome())
        return 0;

    return UMBRA_TOME_METHOD(m_UmbraTome, getObjectCount());
}

size_t Scene::GetDynamicObjectCount () const
{
	return GetRendererNodeCount() - GetStaticObjectCount();
}
	
size_t Scene::GetIntermediateObjectCount () const
{
	return m_IntermediateNodes.GetRendererCount();
}
	
const SceneNode* Scene::GetStaticSceneNodes () const	
{
	return m_RendererNodes.begin();
}

const SceneNode* Scene::GetDynamicSceneNodes () const
{
	return m_RendererNodes.begin() + GetStaticObjectCount();
}
	
const AABB* Scene::GetStaticBoundingBoxes () const
{
	return m_BoundingBoxes.begin();
}

const AABB* Scene::GetDynamicBoundingBoxes () const
{
	return m_BoundingBoxes.begin() + GetStaticObjectCount();
}

	
#if DEBUGMODE
bool Scene::HasNodeForRenderer( const BaseRenderer* r )
{
	for (dynamic_array<SceneNode>::iterator j = m_RendererNodes.begin(); j != m_RendererNodes.end(); ++j)
	{
		if (j->renderer == r)
			return true;
	}
	
	return false;
}

#endif

SceneHandle Scene::AddRendererInternal (Renderer *renderer, int layer, const AABB& aabb)
{
	SceneHandle handle = m_RendererNodes.size();
	Assert(m_BoundingBoxes.size() == handle);
	Assert(m_VisibilityBits.size() == handle);
	
	SceneNode node;
	node.renderer = renderer;
	node.layer = layer;
	m_RendererNodes.push_back(node);
	m_BoundingBoxes.push_back(aabb);
	m_VisibilityBits.push_back(0);
	return handle;
}
	
	
SceneHandle Scene::AddRenderer (Renderer *renderer)
{
	Assert (renderer);
#if DEBUGMODE
	DebugAssertIf (HasNodeForRenderer(renderer));
#endif
	if (m_PreventAddRemoveRenderer != 0)
	{
		AssertString("Adding renderer during rendering is not allowed.");
		return kInvalidSceneHandle;
	}
	
	AABB aabb;
	renderer->GetWorldAABB(aabb);
	Assert(aabb.IsValid());
	
	return AddRendererInternal(renderer, renderer->GetLayer(), aabb);
}

BaseRenderer* Scene::RemoveRenderer (SceneHandle handle)
{
	if (handle < 0 || handle >= m_RendererNodes.size())
	{
		ErrorString("Invalid SceneHandle");
		return NULL;
	}
	SceneNode& node = m_RendererNodes[handle];
	BaseRenderer* renderer = node.renderer;

	if (m_PreventAddRemoveRenderer != 0)
	{
		// The current node can be removed during NotifyVisible()
		// This is due to the fact that Animations can be updated during culling and 
		// our animation system allows users to set m_Enabled = false which then
		// results in our node being removed (see fogbugz case 378739).
	
		// We can't actually remove this or reorder nodes until after the render.
		// There are pointers to nodes and AABBs being used during rendering!
		m_PendingRemoval.push_back(handle);
		node.disable = true;
		return renderer;
	}
	
	// Static objects can not be removed from the array
	if (handle < GetStaticObjectCount())
	{
		m_VisibilityBits[handle] = 0;
		node.renderer = NULL;
		node.dirtyAABB = false;
		return renderer;
	}
	
	// Swap with last element (if we are not the last element)
	int lastIndex = m_RendererNodes.size() - 1;
	const SceneNode& lastNode = m_RendererNodes[lastIndex];
	if (handle != lastIndex && lastNode.renderer != NULL)
	{
		const AABB& lastAABB = m_BoundingBoxes[lastIndex];
		bool lastVisibilityBits = m_VisibilityBits[lastIndex];
		m_RendererNodes[handle] = lastNode;
		m_BoundingBoxes[handle] = lastAABB;
		m_VisibilityBits[handle] = lastVisibilityBits;
		// We don't remove old handle from dirty list, just check for invalid ones
		if (lastNode.dirtyAABB)
			m_DirtyAABBList.push_back(handle);

		Renderer* swapRenderer = static_cast<Renderer*>(lastNode.renderer);
		swapRenderer->NotifySceneHandleChange(handle);
	}
	m_RendererNodes.pop_back();
	m_BoundingBoxes.pop_back();
	m_VisibilityBits.pop_back();
	return renderer;
}

#if UNITY_EDITOR

unsigned Scene::GetUmbraDataSize ()
{
    if (!m_UmbraTome.HasTome())
        return 0;

    return UMBRA_TOME_METHOD(m_UmbraTome, getSize());
}
	
bool Scene::IsPositionInPVSVolume (const Vector3f& position)
{
	if (m_UmbraQuery == NULL)
		return false;

	////@TODO: This is no longer working!
	
	return true;
	
//    Umbra::Query::ErrorCode e = m_UmbraQuery->queryPointVisibility(m_QueryMode, NULL, NULL, (Umbra::Vector3&)position);
//    return e == Umbra::Query::ERROR_OK;
}

#endif

void Scene::SetOcclusionPortalEnabled (unsigned int portalIndex, bool enabled)
{
	if (m_UmbraQuery == NULL)
		return;

	if (portalIndex >= UMBRA_TOME_METHOD(m_UmbraTome, getGateStateSize()))
	{
		ErrorString("Invalid portal index");
		return;
	}
	
	Umbra::GateStateVector gateVector (m_GateState, 0, false);
	gateVector.setState(portalIndex, enabled);
}


void Scene::RecalculateDirtyBounds()
{
	int dirtyCount = m_DirtyAABBList.size();
	for (int i = 0; i < dirtyCount; ++i)
	{
		SceneHandle handle = m_DirtyAABBList[i];
		// List may have invalid entries so check range/dirty flag
		if (handle < m_RendererNodes.size())
		{
			SceneNode& node = m_RendererNodes[handle];
			if (node.dirtyAABB)
			{
				node.renderer->GetWorldAABB(m_BoundingBoxes[handle]);
				node.dirtyAABB = false;
			}
		}
	}
	m_DirtyAABBList.resize_uninitialized(0);
}

void Scene::SetPreventAddRemoveRenderer(bool enable)
{
	// Culling can be nested, so we need a count to disable changes to scene
	m_PreventAddRemoveRenderer += enable ? 1 : -1;
}

void Scene::NotifyVisible (const CullingOutput& visibleObjects)
{
	// Update visibility bits for static objects
	for (int i = 0; i < visibleObjects.visible[kStaticRenderers].size; ++i)
	{
		int index = visibleObjects.visible[kStaticRenderers].indices[i];
		m_VisibilityBits[index] |= kVisibleCurrentFrame;
	}
	
	// Update visibility bits for dynamic objects
	size_t offset = GetStaticObjectCount();
	for (int i = 0; i < visibleObjects.visible[kDynamicRenderer].size; ++i)
	{
		int index = visibleObjects.visible[kDynamicRenderer].indices[i] + offset;
		m_VisibilityBits[index] |= kVisibleCurrentFrame;
	}
	
	// We prevent changes to scene here and in OnWillRenderObject(), case 445226.
	// Since array indices and pointers must stay valid, adding nodes is not allowed.
	// We disable nodes instead of removing them, then remove them later.
	SetPreventAddRemoveRenderer(true);
	int nodeCount = m_RendererNodes.size();
	for (int i = 0; i < nodeCount; ++i)
	{
		SceneNode& node = m_RendererNodes[i];
		UInt8& vbits = m_VisibilityBits[i];
		if (vbits == kVisibleCurrentFrame)
		{
			node.renderer->RendererBecameVisible();
			vbits |= kBecameVisibleCalled;
		}
	}
	SetPreventAddRemoveRenderer(false);
}

void Scene::NotifyInvisible ()
{
	// Happens after rendering, so modifying scene is not a problem.
	int nodeCount = m_RendererNodes.size();
	for (int i = 0; i < nodeCount; ++i)
	{
		SceneNode& node = m_RendererNodes[i];
		UInt8& vbits = m_VisibilityBits[i];
		if (vbits == kVisiblePreviousFrame)
		{
			node.renderer->RendererBecameInvisible();
		}
		// Roll visibility over to next frame
		vbits = (vbits & kVisibleCurrentFrame) ? kVisiblePreviousFrame : 0;
	}
}

void Scene::BeginCameraRender ()
{
	// Prepare Static SceneNode array
	if (m_RequestStaticPVSRebuild)
	{
		m_RequestStaticPVSRebuild = false;
		InitializeUmbra();
	}
}

void Scene::EndCameraRender ()
{
	// Removal is done after rendering since we keep pointers around to nodes and AABBs.
	// We do it in reverse order to avoid moving the last entries before deleting them.
	// Do not change this without careful thinking as it is easy to break...
	if (!m_PendingRemoval.empty())
	{
		std::sort(m_PendingRemoval.begin(), m_PendingRemoval.end());
#if DEBUGMODE
		int validateUnique = -1;
#endif
		for (int i=m_PendingRemoval.size()-1; i >= 0;i--)
		{	
#if DEBUGMODE
			Assert(validateUnique != m_PendingRemoval[i]);
			validateUnique = m_PendingRemoval[i];
#endif
			
			RemoveRenderer(m_PendingRemoval[i]);
		}
		m_PendingRemoval.clear();
	}
}



Scene& GetScene ()
{
	return *gScene;
}

void Scene::InitializeClass ()
{
	Assert(gScene == NULL);
	gScene = new Scene ();
}

void Scene::CleanupClass ()
{
	Assert(gScene != NULL);
	delete gScene;
	gScene = NULL;
}


} // namespace Unity
