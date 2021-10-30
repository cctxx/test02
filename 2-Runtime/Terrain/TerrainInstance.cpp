#include "UnityPrefix.h"
#include "TerrainInstance.h"

#if ENABLE_TERRAIN
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Terrain/TerrainManager.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/Culler.h"
#include "Runtime/Camera/LightManager.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/RenderSettings.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Input/TimeManager.h"

#include "DetailRenderer.h"
#include "TreeRenderer.h"
#include "TerrainRenderer.h"

#include <list>

using namespace std;

TerrainInstance::TerrainInstance (GameObject* go)
: m_Position(0,0,0)
, m_GameObject(go)
, m_HeightmapPixelError(5.0f)
, m_HeightmapMaximumLOD(0)
, m_SplatMapDistance(1000.0f)
, m_TreeDistance(5000.0f)
, m_TreeBillboardDistance(50.0f)
, m_TreeCrossFadeLength(5.0f)
, m_TreeMaximumFullLODCount(50)
, m_DetailObjectDistance(80.0f)
, m_DetailObjectDensity(1.0f)
, m_CastShadows(true)
, m_DrawTreesAndFoliage(true)
, m_LeftNeighbor(NULL)
, m_RightNeighbor(NULL)
, m_BottomNeighbor(NULL)
, m_TopNeighbor(NULL)
, m_EditorRenderFlags(kRenderAll)
, m_LightmapIndex(-1)
, m_LightmapSize(1024)
{
	Assert(go != NULL);
}

TerrainInstance::~TerrainInstance()
{
	// Determine what to clean up 
}

void TerrainInstance::OnEnable()
{
	if (m_TerrainData.IsValid())
	{
		Assert(m_GameObject != NULL);
		m_TerrainData->AddUser(m_GameObject);
	}
}

void TerrainInstance::OnDisable()
{
	if (m_TerrainData.IsValid())
	{
		Assert(m_GameObject != NULL);
		m_TerrainData->RemoveUser(m_GameObject);
	}
}

const Vector3f TerrainInstance::GetPosition() const
{
	return m_GameObject->GetComponentT<Transform>(Transform::GetClassIDStatic()).GetPosition();
}

void TerrainInstance::Flush()
{
	for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
	{
		UNITY_DELETE ((*i).trees, kMemTerrain);
		UNITY_DELETE ((*i).terrain, kMemTerrain);
		UNITY_DELETE ((*i).details, kMemTerrain);
	}

	m_Renderers.clear();
}

void TerrainInstance::GarbageCollectRenderers()
{
	int frame = GetTimeManager ().GetRenderFrameCount ();
	// traverse backwards so we can remove renderers
	dynamic_array<Renderer>::iterator i = m_Renderers.begin();
	while (i != m_Renderers.end())
	{
		int frameDiff = frame - (*i).lastUsedFrame;
		// cleanup renderers after they are unused for some frames; handle wrap-around just in case
		// also cleanup immediately when camera is destroyed
		if( frameDiff > 100 || frameDiff < 0 || (*i).camera == NULL) {
			UNITY_DELETE((*i).trees, kMemTerrain);
			UNITY_DELETE((*i).terrain, kMemTerrain);
			UNITY_DELETE((*i).details, kMemTerrain);
			i = m_Renderers.erase(i);
		}
		else
			++i;
	}
}

void TerrainInstance::FlushDirty () 
{		
	bool reloadDetails = false;
	bool reloadTrees = false;

	// Figure out what we need to recalc depending on what to update.
	// we build some bool flags so we never do more than one update no matter 
	// what was changed and which dependencies they have of each other.
	if ((m_DirtyFlags & TerrainData::kHeightmap) != 0)
		reloadDetails = reloadTrees = true;
	if ((m_DirtyFlags & TerrainData::kTreeInstances) != 0)
		reloadTrees = true;

	// Optimized live terrain painting update mode
	if ((m_DirtyFlags & TerrainData::kDelayedHeightmapUpdate) != 0)
	{
		// Reload precomputed error, this will make affected patches reload the vertices!
		for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
			(*i).terrain->ReloadPrecomputedError ();
	}

	if (reloadTrees)
	{
		for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
			(*i).trees->ReloadTrees();
	}

	if (reloadDetails)
	{
		for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
			(*i).details->ReloadAllDetails ();
	}


	if ((m_DirtyFlags & TerrainData::kHeightmap) != 0)
	{
		for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
			(*i).terrain->ReloadAll();
	}

	m_DirtyFlags = TerrainData::kNoChange;
}

const TerrainInstance::Renderer* TerrainInstance::GetRenderer()
{
	Camera* cam = GetCurrentCameraPtr();

	if ((cam->GetCullingMask() & (1 << m_GameObject->GetLayer())) == 0)
		return NULL;

#if UNITY_EDITOR
	if (!cam->IsFiltered(*m_GameObject))
		return NULL;
#endif

	int frame = GetTimeManager ().GetRenderFrameCount ();
	for (dynamic_array<Renderer>::iterator i = m_Renderers.begin(); i != m_Renderers.end(); ++i)
	{
		if (i->camera == cam)
		{
			if (i->terrain->GetTerrainData().IsNull())
			{
				Flush();
				break;	
			}
			i->lastUsedFrame = frame;
			return &*i;
		}
	}

	SET_ALLOC_OWNER(m_GameObject);

	if (m_TerrainData.IsValid())
	{
		Vector3f position = GetPosition();

		m_Renderers.resize_uninitialized(m_Renderers.size() + 1, false);
		Renderer& renderer = m_Renderers.back();
		renderer.camera = cam;
		renderer.terrain = UNITY_NEW(TerrainRenderer, kMemTerrain) (m_GameObject->GetInstanceID(), m_TerrainData, position, m_LightmapIndex);
#if UNITY_EDITOR
		renderer.terrain->SetLightmapSize(m_LightmapSize);
#endif
		renderer.trees = UNITY_NEW(TreeRenderer, kMemTerrain) (m_TerrainData->GetTreeDatabase(), position, m_LightmapIndex);
		renderer.details = UNITY_NEW(DetailRenderer, kMemTerrain) (m_TerrainData, position, m_LightmapIndex);
		renderer.lastUsedFrame = frame;
		return &renderer;
	}
	else
	{
		return NULL;
	}
}

TerrainRenderer* TerrainInstance::GetTerrainRendererDontCreate()
{
	Camera* cam = GetCurrentCameraPtr();

	if ((cam->GetCullingMask() & (1 << m_GameObject->GetLayer())) == 0)
		return NULL;

	for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
	{
		if (r->camera == cam)
			return r->terrain;
	}

	return NULL;
}

void TerrainInstance::SetLightmapIndex(int value)
{
	m_LightmapIndex = value;
	for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
	{
		r->terrain->SetLightmapIndex(value);
		r->trees->SetLightmapIndex(value);
		r->details->SetLightmapIndex(value);
	}
}

void TerrainInstance::InitializeClass()
{
}

void TerrainInstance::CleanupClass()
{
}

void TerrainInstance::SetDetailObjectDensity(float value)
{
	value = ::clamp(value,0.0f,1.0f);
	bool changed = (value != m_DetailObjectDensity);
	m_DetailObjectDensity = value;
	if (changed)
	{
		for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
			r->details->ReloadAllDetails();
	}
}

void TerrainInstance::SetLightmapSize(int value)
{
#if UNITY_EDITOR
	m_LightmapSize = value > 0 ? value : 1;
	for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
		r->terrain->SetLightmapSize(value);
#endif
}

void TerrainInstance::ApplyDelayedHeightmapModification()
{
	UNITY_TEMP_VECTOR(int) invalidPatches;
	m_TerrainData->GetHeightmap().RecomputeInvalidPatches(invalidPatches);
	if (invalidPatches.size() != 0)
	{
		m_TerrainData->GetTreeDatabase().RecalculateTreePositions();

		for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
		{
			r->terrain->ReloadPrecomputedError ();
			r->terrain->ReloadBounds ();
			r->details->ReloadAllDetails ();
		}
	}

}

///TODO: This should be moved to TerrainData. Each TreeRenderer should register when its using a TerrainData with a linked list and when a tree is added,
/// a callback should be invoked which lets the TreeRenderer ineject the tree into the spatial database
void TerrainInstance::AddTreeInstance(const TreeInstance& tree)
{
	bool hasTrees = !m_TerrainData->GetTreeDatabase().GetInstances().empty();
	m_TerrainData->GetTreeDatabase().AddTree(tree);

	for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
	{
		if (hasTrees)
		{
			r->trees->InjectTree (m_TerrainData->GetTreeDatabase().GetInstances().back());
		}
		else
		{
			delete r->trees;
			r->trees = new TreeRenderer (m_TerrainData->GetTreeDatabase(), GetPosition(), m_LightmapIndex);
		}
	}
}

void TerrainInstance::RemoveTrees(const Vector2f& position, float radius, int prototypeIndex)
{
	int trees = m_TerrainData->GetTreeDatabase().RemoveTrees(position, radius, prototypeIndex);
	if (trees != 0)
	{
		for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
		{
			r->trees->RemoveTrees(Vector3f(position[0], position[1], 0.0f), radius, prototypeIndex);
		}
	}
}

void TerrainInstance::OnTerrainChanged(TerrainData::ChangedFlags flags)
{
	// Dirty details must be deleted immediately because we are clearing data that stores what was set dirty immediately afterwards.
	if ((flags & TerrainData::kRemoveDirtyDetailsImmediately) != 0)
	{
		for (dynamic_array<Renderer>::iterator r = m_Renderers.begin(); r != m_Renderers.end(); ++r)
		{
			r->details->ReloadDirtyDetails();
		}
	}

	if ((flags & TerrainData::kFlushEverythingImmediately) != 0)
		Flush ();
	else
		m_DirtyFlags |= flags;
}

void TerrainInstance::SetNeighbors (TerrainInstance* left, TerrainInstance* top, TerrainInstance* right, TerrainInstance* bottom)
{
	m_TopNeighbor = top;
	m_LeftNeighbor = left;
	m_RightNeighbor = right;
	m_BottomNeighbor = bottom;
}

float TerrainInstance::SampleHeight(Vector3f worldPosition) const
{
	worldPosition -= GetPosition();
	worldPosition.x /= m_TerrainData->GetHeightmap().GetSize().x;
	worldPosition.z /= m_TerrainData->GetHeightmap().GetSize().z;
	return m_TerrainData->GetHeightmap().GetInterpolatedHeight(worldPosition.x, worldPosition.z);
}
#undef LISTFOREACH

#endif //ENABLE_TERRAIN
