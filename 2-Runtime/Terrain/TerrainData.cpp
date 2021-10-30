#include "UnityPrefix.h"
#include "TerrainData.h"

#if ENABLE_TERRAIN

#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Profiler/Profiler.h"
#include "TerrainManager.h"
#if UNITY_EDITOR
#include "Runtime/Serialize/PersistentManager.h"
#endif

IMPLEMENT_CLASS_HAS_INIT (TerrainData)
IMPLEMENT_OBJECT_SERIALIZE(TerrainData)

PROFILER_INFORMATION(gAwakeFromLoadTerrain, "TerrainData.AwakeFromLoad", kProfilerLoading)

TerrainData::TerrainData(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode), m_Heightmap (this), m_TreeDatabase(*this), m_DetailDatabase(this, &m_TreeDatabase), m_SplatDatabase(this)
{
}

TerrainData::~TerrainData ()
{
	UpdateUsers(kWillBeDestroyed);
}

void TerrainData::InitializeClass ()
{
#if UNITY_EDITOR
	GetPersistentManager().AddNonTextSerializedClass (ClassID (TerrainData));
#endif
	TerrainManager::InitializeClass();
}

void TerrainData::CleanupClass ()
{
	TerrainManager::CleanupClass();
}

SplatDatabase &TerrainData::GetSplatDatabase () 
{ 
	return m_SplatDatabase; 
}

DetailDatabase &TerrainData::GetDetailDatabase () 
{ 
	return m_DetailDatabase; 
}

void TerrainData::ExtractPreloadShaders (vector<PPtr<Object> >& shaders)
{
	ScriptMapper& sm = GetScriptMapper ();
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/BillboardTree"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Details/BillboardWavingDoublePass"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Details/Vertexlit"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Details/WavingDoublePass"));
	shaders.push_back(sm.FindShader("Hidden/Nature/Tree Soft Occlusion Leaves Rendertex"));
	shaders.push_back(sm.FindShader("Hidden/Nature/Tree Soft Occlusion Bark Rendertex"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Details/Vertexlit"));

	shaders.push_back(sm.FindShader("Diffuse"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Splatmap/Lightmap-FirstPass"));
	shaders.push_back(sm.FindShader("Hidden/TerrainEngine/Splatmap/Lightmap-AddPass"));

	for (int i=0;i<shaders.size();i++)
	{
		if (!shaders[i].IsValid())
		{
			ErrorString("Terrain preloaded shaders could not be found");
		}
	}
}

void TerrainData::AwakeFromLoadThreaded ()
{
	Super::AwakeFromLoadThreaded();
	m_SplatDatabase.RecalculateBasemap(false);
	m_DetailDatabase.SetDetailPrototypesDirty();
	m_DetailDatabase.GenerateTextureAtlasThreaded();
}

void TerrainData::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	PROFILER_AUTO(gAwakeFromLoadTerrain, this)

	Super::AwakeFromLoad(awakeMode);
	m_SplatDatabase.AwakeFromLoad(awakeMode);
	m_DetailDatabase.SetDetailPrototypesDirty();

	if (awakeMode & kDidLoadThreaded)
		m_DetailDatabase.UpdateDetailPrototypesIfDirty();
	
	m_TreeDatabase.RefreshPrototypes();
	
	UpdateUsers(kFlushEverythingImmediately);
	
	m_Heightmap.AwakeFromLoad();

	// Just upload image - threaded basemap calculation already done
	if (awakeMode & kDidLoadThreaded)
	{
		m_SplatDatabase.UploadBasemap();
	}
	// Do full recalculation
	else
	{
		m_SplatDatabase.RecalculateBasemapIfDirty();
	}
}

template<class TransferFunc>
void TerrainData::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);

	transfer.Transfer (m_SplatDatabase, "m_SplatDatabase", kHideInEditorMask);
	transfer.Transfer (m_DetailDatabase, "m_DetailDatabase", kHideInEditorMask);
	transfer.Transfer (m_Heightmap, "m_Heightmap", kHideInEditorMask);

#if UNITY_EDITOR
	// Are we collecting all references for preloading?
	if ((transfer.GetFlags () & kBuildPlayerOnlySerializeBuildProperties) && transfer.IsRemapPPtrTransfer())
	{
		vector<PPtr<Object> > preloadShader;
		ExtractPreloadShaders(preloadShader);
		TRANSFER(preloadShader);
	}
#endif
}

bool TerrainData::HasUser (GameObject *user) const
{
	return m_Users.find(user) != m_Users.end();
}

void TerrainData::AddUser (GameObject *user) 
{
	m_Users.insert (user);
}

void TerrainData::RemoveUser (GameObject *user) 
{
	m_Users.erase (user);
}

void TerrainData::UpdateUsers (ChangedFlags changedFlag) 
{
	for (std::set<PPtr<GameObject> >::iterator i = m_Users.begin(); i != m_Users.end(); i++) 
	{
		GameObject *go = *i;
		if (go)
			go->SendMessage(kTerrainChanged, (int)changedFlag, ClassID (int));
	}
}

void TerrainData::SetLightmapIndexOnUsers(int lightmapIndex)
{
	for (std::set<PPtr<GameObject> >::iterator i = m_Users.begin(); i != m_Users.end(); i++) 
	{
		GameObject *go = *i;
		if (go)
			go->SendMessage(kSetLightmapIndex, (int)lightmapIndex, ClassID (int));
	}
}

#endif // ENABLE_TERRAIN

