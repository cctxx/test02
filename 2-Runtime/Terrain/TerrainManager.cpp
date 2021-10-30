#include "UnityPrefix.h"
#if ENABLE_TERRAIN
#include "TerrainManager.h"
#include "TerrainData.h"
#include "TerrainInstance.h"
#include "DetailRenderer.h"
#include "TreeRenderer.h"
#include "TerrainRenderer.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"

#include "Runtime/Camera/LightManager.h"
using namespace std;

#define LISTFOREACH(TYPE, X, Y) for (UNITY_LIST(kMemRenderer, TYPE)::iterator Y = X.begin(); Y != X.end(); ++Y)

PROFILER_INFORMATION(gTerrainFlushDirty, "TerrainInstance.FlushDirty", kProfilerRender);
PROFILER_INFORMATION(gTerrainRenderStep1, "Terrain.Heightmap.RenderStep1", kProfilerRender);
PROFILER_INFORMATION(gTerrainRenderStep2, "Terrain.Heightmap.RenderStep2", kProfilerRender);
PROFILER_INFORMATION(gTerrainRenderStep3, "Terrain.Heightmap.RenderStep3", kProfilerRender);
PROFILER_INFORMATION(gTerrainDetailsRender, "Terrain.Details.Render", kProfilerRender);
PROFILER_INFORMATION(gTerrainTreesRender, "Terrain.Trees.Render", kProfilerRender);

void TerrainManager::InitializeClass ()
{
	SetITerrainManager (new TerrainManager());
}

void TerrainManager::CleanupClass ()
{
	TerrainManager* manager = reinterpret_cast<TerrainManager*> (GetITerrainManager());
	delete manager;
	SetITerrainManager (NULL);
}

TerrainManager::TerrainManager()
: m_ActiveTerrain(NULL)
{
}


bool TerrainManager::GetInterpolatedHeight (const Object* inTerrainData, const Vector3f& terrainPosition, const Vector3f& position, float& outputHeight)
{
	TerrainData* terrainData = dynamic_pptr_cast<TerrainData*> (inTerrainData);
	if (terrainData == NULL)
		return false;
	
	const Vector3f terrainSize (terrainData->GetHeightmap ().GetSize ());
	const Vector3f localPosition = Scale (position-terrainPosition, Inverse (terrainSize));
	
	// Outside of terrain.. Skip.
	if (localPosition.x > 1.0f || localPosition.x < 0.0f || localPosition.z > 1.0f || localPosition.z < 0.0f)
		return false;
	
	outputHeight = terrainPosition.y + terrainData->GetHeightmap ().GetInterpolatedHeight (localPosition.x, localPosition.z);
	return true;
}

void TerrainManager::CullAllTerrains(int cullingMask)
{
	m_TempCulledTerrains.clear();

	// Generate basic terrain data and do basic tessellation
	LISTFOREACH(TerrainInstance*, m_ActiveTerrains, i)
	{
		TerrainInstance* terrain = (*i);

		int terrainLayer = terrain->m_GameObject->GetLayer();
		if( ((1<<terrainLayer) & cullingMask) == 0 )
			continue;

		m_TempCulledTerrains.push_back(*i);
		const Vector3f& currentTerrrainPosition = terrain->GetPosition();
		if (currentTerrrainPosition != terrain->m_Position)
		{
			terrain->m_Position = currentTerrrainPosition;
			terrain->Flush ();
		}

		terrain->GarbageCollectRenderers ();
		PROFILER_BEGIN(gTerrainFlushDirty, NULL)
		terrain->FlushDirty ();
		PROFILER_END

		PROFILER_BEGIN(gTerrainRenderStep1, NULL)
		const TerrainInstance::Renderer* renderer = terrain->GetRenderer ();
		if (renderer != NULL)
		{
		// Draw terrain
			terrain->m_TerrainData->GetSplatDatabase().RecalculateBasemapIfDirty();
	
			if (terrain->NeedRenderTerrainGeometry())
			{
				float splatMapDistance = terrain->m_EditorRenderFlags == TerrainInstance::kRenderHeightmap ? FLT_MAX : terrain->m_SplatMapDistance;
				renderer->terrain->RenderStep1 (renderer->camera, terrain->m_HeightmapMaximumLOD, terrain->m_HeightmapPixelError, splatMapDistance);
			}
		}
		PROFILER_END
	}

	// Setup neighbors
	LISTFOREACH(TerrainInstance*, m_TempCulledTerrains, i)
	{
		TerrainInstance* terrain = (*i);
		TerrainRenderer* renderer = terrain->GetTerrainRendererDontCreate ();
		if (renderer != NULL && terrain->NeedRenderTerrainGeometry())
		{
			// Find neighbor Terrains and update TerrainRenderer
			TerrainRenderer* left = NULL, * right = NULL, * top = NULL, * bottom = NULL;
			if (terrain->m_LeftNeighbor != NULL)
				left = terrain->m_LeftNeighbor->GetTerrainRendererDontCreate ();
			if (terrain->m_RightNeighbor != NULL)
				right = terrain->m_RightNeighbor->GetTerrainRendererDontCreate ();
			if (terrain->m_TopNeighbor != NULL)
				top = terrain->m_TopNeighbor->GetTerrainRendererDontCreate ();
			if (terrain->m_BottomNeighbor != NULL)
				bottom = terrain->m_BottomNeighbor->GetTerrainRendererDontCreate ();
			renderer->SetNeighbors(left, top, right, bottom);
		}
	}

	// Apply force splitting on boundaries
	LISTFOREACH(TerrainInstance*, m_TempCulledTerrains, i)
	{
		TerrainInstance* terrain = (*i);
		TerrainRenderer* renderer = terrain->GetTerrainRendererDontCreate ();
		if (renderer != NULL && terrain->NeedRenderTerrainGeometry())
		{
			PROFILER_BEGIN(gTerrainRenderStep2, NULL)
			renderer->RenderStep2 ();
			PROFILER_END
		}
	}

	// Do the actual rendering
	LISTFOREACH(TerrainInstance*, m_TempCulledTerrains, i)
	{
		TerrainInstance* terrain = (*i);
		const TerrainInstance::Renderer* renderer = terrain->GetRenderer ();
		if (renderer != NULL)
		{
			int terrainLayer = terrain->m_GameObject->GetLayer();

			UNITY_VECTOR(kMemRenderer, Light*) lights = GetLightManager().GetLights(kLightDirectional, terrainLayer);

			PROFILER_BEGIN(gTerrainRenderStep3, NULL)
			if (terrain->NeedRenderTerrainGeometry())
			{
				renderer->terrain->RenderStep3 (renderer->camera, terrainLayer, terrain->m_CastShadows, terrain->m_MaterialTemplate);
			}
			PROFILER_END

			PROFILER_BEGIN(gTerrainDetailsRender, NULL)
			if (terrain->NeedRenderDetails())
			{
				renderer->details->Render (renderer->camera, terrain->m_DetailObjectDistance, terrainLayer, terrain->m_DetailObjectDensity);
			}
			PROFILER_END

			PROFILER_BEGIN(gTerrainTreesRender, NULL)
			if (terrain->NeedRenderTrees())
			{
				renderer->trees->Render (*renderer->camera, lights, terrain->m_TreeBillboardDistance, terrain->m_TreeDistance, terrain->m_TreeCrossFadeLength, terrain->m_TreeMaximumFullLODCount, terrainLayer);
			}
			PROFILER_END
			//				terrain.m_DebugTreeRenderTex = renderer.trees.GetImposterRenderTexture();
		}
	}
}

void TerrainManager::SetLightmapIndexOnAllTerrains (int lightmapIndex)
{
	LISTFOREACH(TerrainInstance*, m_ActiveTerrains, terrain)
	{
		(*terrain)->SetLightmapIndex(lightmapIndex);
	}
}

/// Creates a Terrain including collider from [[TerrainData]]
PPtr<GameObject> TerrainManager::CreateTerrainGameObject (const TerrainData& assignTerrain)
{
/*	// Also create the renderer game object
#if ENABLE_PHYSICS
	GameObject* go = &CreateGameObjectWithHideFlags ("Terrain", true, 0, "Terrain", NULL, NULL); 
#else
	GameObject* go = &CreateGameObjectWithHideFlags ("Terrain", true, 0, "Terrain", NULL, NULL); 
#endif
	go->SetIsStatic (true);
	TerrainInstance* terrain = go.GetComponent(typeof(Terrain)) as Terrain;
#if ENABLE_PHYSICS
	TerrainCollider collider = go.GetComponent(typeof(TerrainCollider)) as TerrainCollider;
	collider.terrainData = assignTerrain;
	terrain.terrainData = assignTerrain;
#endif
	// The terrain already got an OnEnable, but the terrain data had not been set up correctly.
	terrain->OnEnable ();

	return go;*/
	return NULL;
}

void TerrainManager::AddTerrainAndSetActive(TerrainInstance* terrain)
{
	if (std::find(m_ActiveTerrains.begin(), m_ActiveTerrains.end(), terrain) == m_ActiveTerrains.end())
		m_ActiveTerrains.push_back(terrain);

	m_ActiveTerrain = terrain;
}

void TerrainManager::RemoveTerrain(TerrainInstance* terrain)
{
	TerrainList::iterator i = std::find(m_ActiveTerrains.begin(), m_ActiveTerrains.end(), terrain);
	if (i != m_ActiveTerrains.end())
		m_ActiveTerrains.erase(i);

	if (m_ActiveTerrain == terrain)
		m_ActiveTerrain = NULL;
}

void TerrainManager::UnloadTerrainsFromGfxDevice()
{
	LISTFOREACH(TerrainInstance*, m_ActiveTerrains, terrain)
	{
		dynamic_array<TerrainInstance::Renderer> renderers = (*terrain)->m_Renderers;
		for (int i = 0; i < renderers.size(); ++i)
		{
			renderers[i].terrain->UnloadVBOFromGfxDevice();
		}
	}
}

void TerrainManager::ReloadTerrainsToGfxDevice()
{
	LISTFOREACH(TerrainInstance*, m_ActiveTerrains, terrain)
	{
		dynamic_array<TerrainInstance::Renderer> renderers = (*terrain)->m_Renderers;
		for (int i = 0; i < renderers.size(); ++i)
		{
			renderers[i].terrain->ReloadVBOToGfxDevice();
		}
	}
}

#if ENABLE_PHYSICS
NxHeightField* TerrainManager::Heightmap_GetNxHeightField(Heightmap& heightmap)
{
	return heightmap.GetNxHeightField();
}
#endif

int TerrainManager::Heightmap_GetMaterialIndex(Heightmap& heightmap)
{
	return heightmap.GetMaterialIndex();
}

Vector3f TerrainManager::Heightmap_GetSize(Heightmap& heightmap)
{
	return heightmap.GetSize();
}

void TerrainManager::CollectTreeRenderers(dynamic_array<SceneNode>& sceneNodes, dynamic_array<AABB>& boundingBoxes) const
{
	for (TerrainList::const_iterator it = m_ActiveTerrains.begin(); it != m_ActiveTerrains.end(); ++it)
	{
		if (!(*it)->NeedRenderTrees())
		{
			continue;
		}

		const TerrainInstance::Renderer* renderer = (*it)->GetRenderer();
		if (renderer != NULL && renderer->trees != NULL)
		{
			renderer->trees->CollectTreeRenderers(sceneNodes, boundingBoxes);
		}
	}
}

#undef  LISTFOREACH

#endif
