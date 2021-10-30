#include "UnityPrefix.h"
#include "OcclusionCulling.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Filters/Mesh/LODMesh.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "AsyncProgressBar.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Camera/OcclusionArea.h"
#include "EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Terrain/TerrainRenderer.h"
#include "ExtractTerrainMesh.h"
#include "Editor/Src/Utility/Analytics.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Camera/OcclusionPortal.h"
#include "Runtime/Misc/ResourceManager.h"
#include "External/Umbra/builds/interface/runtime/umbraQuery.hpp"
#include "Editor/Src/Utility/StaticEditorFlags.h"
#include "Runtime/Shaders/MaterialIsTransparent.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Camera/SceneSettings.h"
#include "Runtime/Misc/BuildSettings.h"

#define ENABLE_OCCLUSION 1


////@TODO: When a gate is baked. Make sure that the majority of the area is not tagged as occluded. (Eg. someone tagged static data around it)

using namespace std;

const char* kTempOcclusionFolder = "Library/Occlusion";

ABSOLUTE_TIME OcclusionCullingTask::s_lastTime;
dynamic_array<gizmos::ColorVertex> OcclusionCullingTask::s_lines;

#if ENABLE_OCCLUSION

#include "External/Umbra/builds/interface/optimizer/umbraScene.hpp"
#include "External/Umbra/builds/interface/optimizer/umbraTask.hpp"

// Typedefs

typedef std::map<const Mesh*, const Umbra::SceneModel*> ModelCache;

// Static funtion prototypes

static void GenerateUmbraScene (Umbra::Scene& scene, const dynamic_array<PPtr<Renderer> >& staticRenderers, const dynamic_array<PPtr<OcclusionPortal> >& portals, const dynamic_array<TerrainBakeInfo>& staticTerrains, const MinMaxAABB& sceneAABB);
static void	ExtractStaticRendererAndBounds (dynamic_array<PPtr<Renderer> >* outputStaticRenderers, dynamic_array<TerrainBakeInfo>* outputStaticTerrains, MinMaxAABB* sceneAABB);
static void	ExtractPortals (dynamic_array<PPtr<OcclusionPortal> >& outputPortals);
static const Umbra::SceneModel* InsertCachedMesh (const Mesh& mesh, Umbra::Scene& scene, ModelCache& cache);
static void AddTerrainPatches(Umbra::Scene& scene, TerrainData* terrainData, const Vector3f& terrainPosition);
static void AddTrees(Umbra::Scene& scene, TerrainData* terrainData, const Vector3f& terrainPosition, ModelCache& cache);
static void clampUmbraParam(Umbra::Scene* scene, Umbra::ComputationParams::ParamName param, float& v);

// Static funtion implementations

bool IsTransparentOrCutoutRenderer (Renderer& renderer)
{
	const Renderer::MaterialArray& materials = renderer.GetMaterialArray();
	
	for (int i=0;i<materials.size();i++)
	{
		Material* material = materials[i];
		if (material && IsTransparentOrCutoutMaterial (*material))
			return true;
	}
	
	return false;
}


template<typename RendererT> static inline Mesh* GetMesh(RendererT* renderer);
template<> static inline Mesh* GetMesh(MeshRenderer* renderer)			{ return renderer->GetSharedMesh(); }
template<> static inline Mesh* GetMesh(SkinnedMeshRenderer* renderer)	{ return renderer->GetMesh(); }

template<typename RendererT> static inline void ExtractStaticRendererAndBoundsImpl( const std::vector<RendererT*>& renderers, dynamic_array< PPtr<Renderer> >* out, MinMaxAABB* sceneAABB )
{
	for (int i=0;i<renderers.size();i++)
	{
		RendererT* renderer = renderers[i];
		if (!renderer->IsActive() || !renderer->GetEnabled())
			continue;

		if( !GetMesh<RendererT>(renderer) )
			continue;

		if (renderer->GetGameObject().AreStaticEditorFlagsSet (kOccluderStatic | kOccludeeStatic))
		{
			if (out)
				out->push_back(renderer);

			if (sceneAABB)
			{
				AABB worldBounds;
				renderer->GetWorldAABB(worldBounds);
				sceneAABB->Encapsulate(worldBounds);
			}
		}
	}
}


void	ExtractStaticRendererAndBounds (dynamic_array<PPtr<Renderer> >* outputStaticRenderers, dynamic_array<TerrainBakeInfo>* outputStaticTerrains, MinMaxAABB* sceneAABB)
{
	if (sceneAABB != NULL)
		sceneAABB->Init();

	{
		vector<MeshRenderer*> renderers;
		std::vector<Object*>* casted = reinterpret_cast<std::vector<Object*>*> (&renderers);
		Object::FindObjectsOfType(MeshRenderer::GetClassIDStatic(), casted, true);
		ExtractStaticRendererAndBoundsImpl<MeshRenderer>(renderers, outputStaticRenderers, sceneAABB);
	}

	{
		vector<SkinnedMeshRenderer*> renderers;
		std::vector<Object*>* casted = reinterpret_cast<std::vector<Object*>*> (&renderers);
		Object::FindObjectsOfType(SkinnedMeshRenderer::GetClassIDStatic(), casted, true);
		ExtractStaticRendererAndBoundsImpl<SkinnedMeshRenderer>(renderers, outputStaticRenderers, sceneAABB);
	}

	dynamic_array<TerrainBakeInfo> tempStaticTerrains;
	ExtractStaticTerrains(kOccluderStatic | kOccludeeStatic, tempStaticTerrains);

	if (sceneAABB != NULL)
	{
		for (int i=0;i<tempStaticTerrains.size();i++)
			sceneAABB->Encapsulate(tempStaticTerrains[i].bounds);
	}

	if (outputStaticTerrains != NULL)
		*outputStaticTerrains = tempStaticTerrains;
}

void WarnTransparentRenderers (const dynamic_array<PPtr<Renderer> >& renderers)
{
	TEMP_STRING warning;
	warning.reserve (4096);
	for (int i=0;i<renderers.size();++i)
	{
		Renderer* renderer = renderers[i];
		
		if (renderer && IsTransparentOrCutoutRenderer(*renderer) && renderer->GetGameObject().AreStaticEditorFlagsSet (kOccluderStatic))
		{
			warning += renderer->GetName();
			warning += '\n';
		}
	}
	
	if (!warning.empty())
	{
		warning = "Some renderers use a transparent shader but are marked as Occluder Static.\nTransparent objects should not be Occluders, we recommend marking them as Occludee Static instead.\n" + warning;
		WarningStringWithoutStacktrace(warning.c_str());
	}
}


void	ExtractPortals (dynamic_array<PPtr<OcclusionPortal> >& outputPortals)
{
	vector<OcclusionPortal*> portals;
	Object::FindObjectsOfType(&portals);

	for (int i=0;i<portals.size();i++)
	{
		OcclusionPortal& portal = *portals[i];
		if (!portal.IsActive())
			continue;

		outputPortals.push_back(&portal);
	}
}

const Umbra::SceneModel* InsertCachedMesh (const Mesh& mesh, Umbra::Scene& scene, ModelCache& cache)
{
	if (cache.count(&mesh))
		return cache[&mesh];

	int vertexCount = mesh.GetVertexCount();
	Mesh::TemporaryIndexContainer triangles;
	mesh.GetTriangles(triangles);

	int triangleCount = triangles.size()/3;

	Vector3f* vertsTemp = NULL;
	ALLOC_TEMP(vertsTemp, Vector3f, vertexCount);
	mesh.ExtractVertexArray (vertsTemp);

    const Umbra::SceneModel* model = scene.insertModel((float*)vertsTemp, (Umbra::UINT32*)(&triangles[0]), vertexCount, triangleCount);
	cache[&mesh] = model;

	return model;
}


void AddTerrainPatches(Umbra::Scene& scene, TerrainData* terrainData, const Vector3f& terrainPosition)
{
	// get meshes and base material for current terrain
	TerrainRenderer tr(0, terrainData, terrainPosition, -1);
	std::vector<Mesh*> meshes = tr.GetMeshPatches();

	// add all terrain patches as Beast instances
	for (int j = 0; j < meshes.size(); j++)
	{
		Mesh* mesh = meshes[j];

		// create the instance
		Matrix4x4f mtx;
		mtx.SetTranslate(terrainPosition);

		int vertexCount = mesh->GetVertexCount();
		Mesh::TemporaryIndexContainer triangles;
		mesh->GetTriangles(triangles);

		int triangleCount = triangles.size()/3;

		Vector3f* vertsTemp = (Vector3f*)UNITY_MALLOC(kMemTempAlloc, sizeof(Vector3f) * vertexCount);
		mesh->ExtractVertexArray (vertsTemp);
		UNITY_FREE(kMemTempAlloc, vertsTemp);

		const Umbra::SceneModel* model = scene.insertModel((float*)vertsTemp, (Umbra::UINT32*)(&triangles[0]), vertexCount, triangleCount);
		scene.insertObject(model, (Umbra::Matrix4x4&)mtx, 0, Umbra::SceneObject::OCCLUDER);
	}
}

void AddTrees(Umbra::Scene& scene, TerrainData* terrainData, const Vector3f& terrainPosition, ModelCache& cache)
{
	const vector<TreeInstance>& treeInstances = terrainData->GetTreeDatabase().GetInstances();
	for (int i=0;i<treeInstances.size();i++)
	{
		const TreeInstance& instance = treeInstances[i];
		const Mesh* mesh = terrainData->GetTreeDatabase().GetPrototypes()[instance.index].mesh;

		if (mesh == NULL)
			continue;

        const Umbra::SceneModel* model = InsertCachedMesh(*mesh, scene, cache);

		Vector3f position = terrainPosition + Scale(instance.position, terrainData->GetHeightmap().GetSize());
		Matrix4x4f mtx;
		mtx.SetTRS(position, Quaternionf::identity(), Vector3f(instance.widthScale, instance.heightScale, instance.widthScale));

        scene.insertObject(model, (Umbra::Matrix4x4&)mtx, 0, Umbra::SceneObject::OCCLUDER);
	}
}

void GenerateUmbraScene (
	Umbra::Scene& scene, 
	const dynamic_array<PPtr<Renderer> >& staticRenderers, 
	const dynamic_array<PPtr<OcclusionPortal> >& portals, 
	const dynamic_array<TerrainBakeInfo>& staticTerrains, 
	const MinMaxAABB& sceneAABB)
{
	ModelCache cache;
	for (int i=0;i<staticRenderers.size();i++)
	{
		Mesh* mesh = NULL;
		Matrix4x4f worldM;
		bool oddNegativeScale = false;
		
		Renderer* 				renderer 			= staticRenderers[i];
		MeshRenderer* 			meshRenderer 		= dynamic_pptr_cast<MeshRenderer*>(renderer);
		SkinnedMeshRenderer* 	skinnedMeshRenderer = dynamic_pptr_cast<SkinnedMeshRenderer*>(renderer);

		int flags = 0;
		if (renderer->GetGameObject().AreStaticEditorFlagsSet (kOccludeeStatic))
			flags |= Umbra::SceneObject::TARGET;

		// Pull mesh and transform matrix from MeshRenderer
		if(meshRenderer)
		{
			if (renderer->GetGameObject().AreStaticEditorFlagsSet (kOccluderStatic))
				flags |= Umbra::SceneObject::OCCLUDER;

			mesh   = meshRenderer->GetSharedMesh();
			worldM = meshRenderer->GetLocalToWorldMatrix();
			oddNegativeScale = renderer->GetTransformInfo().transformType & kOddNegativeScaleTransform;
		}
		// SkinnedMeshRenderers can be marked as Occludee's but can not be Occluders.
		// We use the bounding volume of the skinned mesh in this case since it is adjusted to contain all poses.
		else if(skinnedMeshRenderer)
		{
			mesh = GetBuiltinResource<Mesh> ("Cube.fbx");

			AABB worldBounds;
			skinnedMeshRenderer->GetWorldAABB(worldBounds);

			worldM.SetTRS(worldBounds.GetCenter(), Quaternionf::identity(), worldBounds.GetExtent() * 2);
		}

		if( mesh == NULL )
			continue;
		
		const Umbra::SceneModel* model = InsertCachedMesh (*mesh, scene, cache);
		
		if (oddNegativeScale)
			scene.insertObject(model, (Umbra::Matrix4x4&)worldM, i, flags, Umbra::MF_COLUMN_MAJOR, Umbra::WINDING_CW);
		else
			scene.insertObject(model, (Umbra::Matrix4x4&)worldM, i, flags, Umbra::MF_COLUMN_MAJOR, Umbra::WINDING_CCW);
	}

	///@TODO: Manual portal mode
	// Add all portals
	for (int i=0;i<portals.size();i++)
	{
		OcclusionPortal& portal = *portals[i];
		const Umbra::SceneModel* model = InsertCachedMesh (*GetBuiltinResource<Mesh> ("Cube.fbx"), scene, cache);

		Matrix4x4f finalMtx;
		Matrix4x4f mtx = portal.GetComponent(Transform).GetLocalToWorldMatrix();
		Matrix4x4f localMatrix;
		localMatrix.SetTRS (portal.GetCenter(), Quaternionf::identity(), portal.GetSize());

		MultiplyMatrices4x4(&mtx, &localMatrix, &finalMtx);

		// Umbra userID's need to be unique. They are allocated to be after the static renderers
		int userID = i + staticRenderers.size();

		scene.insertObject(model, (Umbra::Matrix4x4&)finalMtx, userID, Umbra::SceneObject::GATE);
	}

	for (int i=0;i<staticTerrains.size();i++)
	{
		AddTerrainPatches(scene, staticTerrains[i].terrainData, staticTerrains[i].position);

		// HIDDEN OPTION FOR THE TIME BEING
		if (EditorPrefs::GetBool("SupportTreesAsOccluders", false))
			AddTrees(scene, staticTerrains[i].terrainData, staticTerrains[i].position, cache);
	}

    bool hasViewArea = false;

	// PVS volume is exposed explicitly
	vector<OcclusionArea*> volumes;
	Object::FindObjectsOfType(&volumes);
	if (!volumes.empty())
	{
		for (int i=0;i<volumes.size();i++)
		{
			OcclusionArea* volume = volumes[i];
			if (!volume->IsActive())
				continue;

			Vector3f c = volume->GetGlobalCenter();
			Vector3f e = volume->GetGlobalExtents();
			Vector3f mn = c-e;
			Vector3f mx = c+e;
			if (volume->GetViewVolume())
			{
				hasViewArea = true;
                scene.insertViewVolume((Umbra::Vector3&)mn, (Umbra::Vector3&)mx, 0);
			}
		}
	}

	// Use the bounding volume of the scene for target / view volume if no manually specified volumes were found
	if (!hasViewArea)
    {
		Vector3f mn = sceneAABB.GetMin();
		Vector3f mx = sceneAABB.GetMax();
        scene.insertViewVolume((Umbra::Vector3&)mn, (Umbra::Vector3&)mx, 0);
    }
}
#endif

OcclusionCullingTask* OcclusionCullingTask::CreateUmbraTask ()
{
#if ENABLE_OCCLUSION
	
	
	OcclusionBakeSettings bakeSettings = GetSceneSettings().GetOcclusionBakeSettings();
	
	s_lastTime = START_TIME;

	// Extract renderers & scene bounding volume
	dynamic_array<PPtr<Renderer> > staticRendererArray;
	dynamic_array<PPtr<OcclusionPortal> > portalArray;
	dynamic_array<TerrainBakeInfo> staticTerrains;
	MinMaxAABB sceneBounds;
	ExtractStaticRendererAndBounds(&staticRendererArray, &staticTerrains, &sceneBounds);
	ExtractPortals(portalArray);

	if (staticRendererArray.empty() && staticTerrains.empty())
	{
		ErrorString ("No Renderers that are marked static were found in the scene. Please mark all renderers that will never move as static in the inspector.");
		AnalyticsTrackPageView("/OcclusionCulling/NoStaticObjects");
		return NULL;
	}
	
	WarnTransparentRenderers (staticRendererArray);

	if (!CreateDirectory(kTempOcclusionFolder))
		return NULL;

	// Generate Umbra scene from renderers
    Umbra::Scene* scene = Umbra::Scene::create();
	GenerateUmbraScene (*scene, staticRendererArray, portalArray, staticTerrains, sceneBounds);
	
	// Create Occlusion culling task
	OcclusionCullingTask* task = new OcclusionCullingTask();
    task->m_Task = Umbra::Task::create(scene);

	///@TODO: Figure out if we should have parameters for object grouping exposed...
	
	
	///@TODO:	Is this needed for streaming? DATA_TOME_MATCH
	Umbra::UINT32 flags = Umbra::ComputationParams::DATA_VISUALIZATIONS;
	Umbra::ComputationParams params;
	params.setParam(Umbra::ComputationParams::OUTPUT_FLAGS, flags);
	params.setParam(Umbra::ComputationParams::TILE_SIZE, bakeSettings.smallestOccluder*8.f);
	params.setParam(Umbra::ComputationParams::SMALLEST_OCCLUDER, bakeSettings.smallestOccluder);    
	params.setParam(Umbra::ComputationParams::SMALLEST_HOLE, bakeSettings.smallestHole);
	params.setParam(Umbra::ComputationParams::BACKFACE_LIMIT, bakeSettings.backfaceThreshold);
	
	task->m_Task->setComputationParams(params);

	task->m_StaticRenderers = staticRendererArray;
	task->m_Portals = portalArray;
	
	// Incremental bake
	task->m_Task->setCacheSize(4*1024);

	// Start computation
	task->m_Analytics = new AnalyticsProcessTracker("OcclusionCulling", "BakeOcclusion");
	//task->m_Task->setRunAsProcess(AppendPathName(GetApplicationContentsPath(), "Tools").c_str());
	task->m_Task->start(kTempOcclusionFolder);

	scene->release();

	return task;
#else
	return NULL;
#endif
}

bool OcclusionCullingTask::DoesSceneHaveManualPortals ()
{
	dynamic_array<PPtr<OcclusionPortal> > portals;
	ExtractPortals (portals);
	return !portals.empty();
}


#if 0
void OcclusionCullingTask::ClampParams(float& smallestOccluder)
{
	if (!GetBuildSettings().hasAdvancedVersion)	// requires advanced license
		return;

#if ENABLE_OCCLUSION

	// Extract scene bounding volume

	MinMaxAABB sceneBounds;
	ExtractStaticRendererAndBounds(NULL, NULL, &sceneBounds);

    dynamic_array<PPtr<Renderer> > staticRendererArray; // empty to speed up visualization computation
	dynamic_array<TerrainBakeInfo> staticTerrains;      // empty to speed up visualization computation
	dynamic_array<PPtr<OcclusionPortal> > portals;      // empty to speed up visualization computation

	// Generate Umbra scene from renderers
    Umbra::Scene* scene = Umbra::Scene::create();
	GenerateUmbraScene (*scene, staticRendererArray, portals, staticTerrains, sceneBounds);

	if (sceneBounds.IsValid())
	{
	    clampUmbraParam(scene, Umbra::ComputationParams::SMALLEST_OCCLUDER, smallestOccluder);
		clampUmbraParam(scene, Umbra::ComputationParams::SAFE_OCCLUDER_DISTANCE, safeOccluderDistance);
	}

    scene->release();
#else

#endif
}
#endif


class UmbraTaskDebugRenderer : public Umbra::DebugRenderer
{
public:
    UmbraTaskDebugRenderer(dynamic_array<gizmos::ColorVertex>& lines)
	: m_Lines(lines)
    {
		m_Lines.resize_uninitialized(0);
    }
	
    virtual ~UmbraTaskDebugRenderer(void)
    {
    }
	
    virtual void addLine	(const Umbra::Vector3& start, const Umbra::Vector3& end, const Umbra::Vector4& color)
    {
		ColorRGBA32 c = (ColorRGBAf&)color;
		
        m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)start;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)end;
		m_Lines.back().color = c.GetUInt32();
    }
	
    virtual void addPoint	(const Umbra::Vector3& point, const Umbra::Vector4& color)
    {
		AssertString("TODO IMPLEMENT");
		//        m_Lines.push_back((Vector3f&)point);
		//        m_Lines.push_back((Vector3f&)point+Vector3f(0.1f,0.1f,0.1f));
    }

	virtual void addQuad    (const Umbra::Vector3& p0, const Umbra::Vector3& p1, const Umbra::Vector3& p2, const Umbra::Vector3& p3, const Umbra::Vector4& color)
	{
		AssertString("TODO IMPLEMENT");
		ColorRGBA32 c = (ColorRGBAf&)color;
		
        m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p0;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p1;
		m_Lines.back().color = c.GetUInt32();

		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p2;
		m_Lines.back().color = c.GetUInt32();
		
		m_Lines.push_back();
		m_Lines.back().vertex = (Vector3f&)p3;
		m_Lines.back().color = c.GetUInt32();
	}

    virtual void addBuffer	(Umbra::UINT8* , int , int )
    {
		AssertString("TODO IMPLEMENT");
    }
	
    virtual const Umbra::Tome* getTome   (void) const { return NULL; }
	
private:
	
    dynamic_array<gizmos::ColorVertex>& m_Lines;
};

void OcclusionCullingTask::UpdateOcclusionCullingProgressVisualization(Umbra::Task* Task)
{
	UmbraTaskDebugRenderer DebugRenderer(s_lines);
	Task->visualize(Umbra::Task::VISUALIZATION_PROGRESS, &DebugRenderer);
}

void OcclusionCullingTask::GetUmbraPreVisualization (dynamic_array<gizmos::ColorVertex>& lines)
{
	if (!GetBuildSettings().hasAdvancedVersion)	// requires advanced license
		return;

#if ENABLE_OCCLUSION
	ABSOLUTE_TIME ElapsedTime = ELAPSED_TIME(s_lastTime);
	float fElapsedTime = AbsoluteTimeToSeconds (ElapsedTime);

	// Update visualizations 5 times a second
	if (fElapsedTime >= 0.2f)
	{
	    s_lastTime = START_TIME;

		OcclusionCullingTask* RunningTask = (OcclusionCullingTask*) SceneBackgroundTask::GetBackgroundTask(kOcclusionBuildingTask);

		// Visualize the incremental bake progress
		if (RunningTask)
		{
			UpdateOcclusionCullingProgressVisualization(RunningTask->m_Task);
		}
		// Visualize the computation parameters
		else
		{
#if 0
			UmbraTaskDebugRenderer DebugRenderer(s_lines);
			// Extract scene bounding volume
			MinMaxAABB sceneBounds;
			ExtractStaticRendererAndBounds(NULL, NULL, &sceneBounds);

			dynamic_array<PPtr<Renderer> > staticRendererArray; // empty to speed up visualization computation
			dynamic_array<TerrainBakeInfo> staticTerrains;      // empty to speed up visualization computation
			dynamic_array<PPtr<OcclusionPortal> > portals;      // empty to speed up visualization computation

			// Generate Umbra scene from renderers
			Umbra::Scene* scene = Umbra::Scene::create();
			GenerateUmbraScene (*scene, staticRendererArray, portals, staticTerrains, sceneBounds);

			// Create Occlusion culling task
			Umbra::Task* task = Umbra::Task::create(scene);
			Umbra::ComputationParams params;
			params.setParam(Umbra::ComputationParams::SMALLEST_OCCLUDER, smallestOccluder);
			params.setParam(Umbra::ComputationParams::TILE_SIZE, smallestOccluder*8.f);
			task->setComputationParams(params);
			task->visualize(Umbra::Task::VISUALIZATION_PARAM, &DebugRenderer);

			scene->release();
			task->release();
#endif
		}
	}

	GUIView *view = GUIView::GetCurrent();
	if (view)
		view->RequestRepaint();

	lines = s_lines;
#endif
}

OcclusionCullingTask::~OcclusionCullingTask ()
{
	#if ENABLE_OCCLUSION
	if (!m_Task->isFinished())
		 m_Task->abort();

	UpdateOcclusionCullingProgressVisualization(m_Task);

    m_Task->release();
	delete m_Analytics;
	#endif
}

float OcclusionCullingTask::GetProgress ()
{
	#if ENABLE_OCCLUSION
	return m_Task->getProgress();
	#else
	return 0;
	#endif
}

bool OcclusionCullingTask::IsFinished ()
{
	return m_Task->isFinished();
}

bool OcclusionCullingTask::Complete ()
{
#if ENABLE_OCCLUSION
	m_Task->waitForFinish();
	if (m_Task->getError() != Umbra::Task::ERROR_OK)
	{
        ErrorString("Error occurred in occluder data computation:");
        ErrorString(m_Task->getErrorString());
		return false;
	}

    // Apply output to scene

    Umbra::UINT32 dataSize = m_Task->getTomeSize();

    if (dataSize == 0)
    {
		ErrorString("Failed reading occluder data output");
		return false;
    }

    dynamic_array<UInt8> buf;
    buf.resize_uninitialized(dataSize);
    m_Task->getTome(&buf[0], dataSize);

	RegisterUndo(&GetSceneSettings(), "Compute occluder data");
	GetSceneSettings().SetUmbraTome(m_StaticRenderers, m_Portals, &buf[0], dataSize);

	m_Analytics->Succeeded();
	delete m_Analytics;
	m_Analytics = NULL;

#endif
	return true;
}

bool OcclusionCullingTask::GenerateTome ()
{
	if (!GetBuildSettings().hasAdvancedVersion)	// requires advanced license to generate PVS
		return false;
	
	Cancel ();
	
	OcclusionCullingTask* task = CreateUmbraTask ();
	bool success = false;
	if (task)
		success = task->Complete();

	delete task;
	return success;
}

bool OcclusionCullingTask::GenerateTomeInBackground ()
{
	if (!GetBuildSettings().hasAdvancedVersion)	// requires advanced license to generate PVS
		return false;

	Cancel ();
	
	OcclusionCullingTask* task = CreateUmbraTask ();
	if (task != NULL)
		CreateTask (*task);

	return task != NULL;
}

void OcclusionCullingTask::ClearUmbraTome()
{
	RegisterUndo (&GetSceneSettings(), "Clear Umbra Tome");

    GetSceneSettings().SetUmbraTome(dynamic_array<PPtr<Renderer> >(), dynamic_array<PPtr<OcclusionPortal> >(), 0, 0);
	AnalyticsTrackPageView("/OcclusionCulling/Clear");
}

void OcclusionCullingTask::BackgroundStatusChanged ()
{
	CallStaticMonoMethod("OcclusionCullingWindow", "BackgroundTaskStatusChanged");
}
