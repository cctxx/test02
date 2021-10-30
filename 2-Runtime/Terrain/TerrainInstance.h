#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Utilities/NonCopyable.h"

class Camera;
class DetailRenderer;
class TerrainManager;
class TerrainRenderer;
class TreeRenderer;

class TerrainInstance 
{
public:
	enum RenderFlags
	{
		kRenderHeightmap = 1,
		kRenderTrees = 2,
		kRenderDetails = 4,
		kRenderAll = kRenderHeightmap | kRenderTrees | kRenderDetails
	};

	TerrainInstance (GameObject* go);
	~TerrainInstance ();

	static void InitializeClass ();
	static void CleanupClass ();

	void OnEnable ();
	void OnDisable ();
	GameObject* GetGameObject() const { return m_GameObject; }

	const Vector3f GetPosition () const;
	void SetNeighbors (TerrainInstance* left, TerrainInstance* top, TerrainInstance* right, TerrainInstance* bottom);

	void Flush ();
	void FlushDirty ();
	void GarbageCollectRenderers ();

	int GetLightmapIndex() const { return m_LightmapIndex; }
	void SetLightmapIndex (int value);

	const int GetLightmapSize () const { return m_LightmapSize; }
	void SetLightmapSize (int value);

	GET_SET(PPtr<TerrainData>, TerrainData, m_TerrainData);
	GET_SET(float, HeightmapPixelError, m_HeightmapPixelError);
	GET_SET(int, HeightmapMaximumLOD, m_HeightmapMaximumLOD);
	GET_SET(float, BasemapDistance, m_SplatMapDistance);
	GET_SET(float, TreeDistance, m_TreeDistance);
	GET_SET(float, TreeBillboardDistance, m_TreeBillboardDistance);
	GET_SET(float, TreeCrossFadeLength, m_TreeCrossFadeLength);
	GET_SET(int, TreeMaximumFullLODCount, m_TreeMaximumFullLODCount);
	GET_SET(float, DetailObjectDistance, m_DetailObjectDistance);
	GET_SET(bool, CastShadows, m_CastShadows);
	GET_SET(bool, DrawTreesAndFoliage, m_DrawTreesAndFoliage);
	GET_SET(Material*, MaterialTemplate, m_MaterialTemplate);
	GET_SET(RenderFlags, EditorRenderFlags, m_EditorRenderFlags);

	const float GetDetailObjectDensity () const { return m_DetailObjectDensity; }
	void SetDetailObjectDensity (float value);

	float SampleHeight (Vector3f worldPosition) const;

	void ApplyDelayedHeightmapModification ();

	void AddTreeInstance (const TreeInstance& tree);
	void RemoveTrees (const Vector2f& position, float radius, int prototypeIndex);

	void OnTerrainChanged(TerrainData::ChangedFlags flags);

	TerrainRenderer* GetTerrainRendererDontCreate ();

	bool NeedRenderTerrainGeometry() const { return (m_EditorRenderFlags & kRenderHeightmap) != 0; }
	bool NeedRenderDetails() const { return (m_EditorRenderFlags & kRenderDetails) != 0 && m_DrawTreesAndFoliage && m_DetailObjectDistance > 0.001; }
	bool NeedRenderTrees() const { return (m_EditorRenderFlags & kRenderTrees) != 0 && m_DrawTreesAndFoliage && m_TreeDistance > 0.001; }

private:
	friend class TerrainManager;

	struct Renderer
	{
		Camera*           camera;
		TerrainRenderer*  terrain;
		TreeRenderer*     trees;
		DetailRenderer*   details;
		int              lastUsedFrame;

		Renderer() : camera(NULL), terrain(NULL), trees(NULL), details(NULL), lastUsedFrame(0) {}
	};

	const Renderer* GetRenderer();

	PPtr<TerrainData>	m_TerrainData;
	Vector3f			m_Position;

	GameObject* m_GameObject;

	float m_HeightmapPixelError;
	int   m_HeightmapMaximumLOD;
	float m_SplatMapDistance;
	int m_LightmapIndex;
	int m_LightmapSize;
	float m_TreeDistance;
	float m_TreeBillboardDistance;
	float m_TreeCrossFadeLength;
	int   m_TreeMaximumFullLODCount;
	float m_DetailObjectDistance;
	float m_DetailObjectDensity;
	bool  m_CastShadows;
	bool m_DrawTreesAndFoliage;
	PPtr<Material> m_MaterialTemplate;

	TerrainInstance*	m_LeftNeighbor;
	TerrainInstance*	m_RightNeighbor;
	TerrainInstance*	m_BottomNeighbor;
	TerrainInstance*	m_TopNeighbor;

	RenderFlags m_EditorRenderFlags;

	// Which part of the terrain is dirty
	TerrainData::ChangedFlags m_DirtyFlags;

	dynamic_array<Renderer> m_Renderers;
};

#endif