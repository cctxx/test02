#include "UnityPrefix.h"
#include "SceneSettings.h"
#include "UnityScene.h"
#include "Runtime/Camera/OcclusionPortal.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Misc/BuildSettings.h"
#include "UmbraBackwardsCompatibility.h"

SceneSettings::SceneSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

SceneSettings::~SceneSettings ()
{
	if (GetScene().GetUmbraTome () == m_UmbraTome)
		InvalidatePVSOnScene();

	Cleanup();
}

void SceneSettings::InitializeClass ()
{
	Scene::InitializeClass();
}

void SceneSettings::CleanupClass ()
{
	Scene::CleanupClass();
}

void SceneSettings::Cleanup ()
{
	CleanupUmbraTomeData(m_UmbraTome);
}

void SceneSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	
	InvalidatePVSOnScene();
}

int SceneSettings::GetUmbraTotalDataSize() const
{
	return m_UmbraTome.tome ? m_UmbraTome.tome->getStatistic(Umbra::Tome::STAT_TOTAL_DATA_SIZE) : 0;
}

int SceneSettings::GetPortalDataSize() const
{
	return m_UmbraTome.tome ? m_UmbraTome.tome->getStatistic(Umbra::Tome::STAT_PORTAL_DATA_SIZE) : 0;
}

void SceneSettings::InvalidatePVSOnScene ()
{
	GetScene().CleanupPVSAndRequestRebuild();
}

template<class TransferFunction> inline
void SceneSettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	dynamic_array<UInt8> tempPVSData;
	if (transfer.IsWriting () && m_UmbraTome.tome != NULL)
	{
		UInt8* tomeData = (UInt8*)(m_UmbraTome.tome);
        tempPVSData.assign(tomeData, tomeData + UMBRA_TOME_METHOD(m_UmbraTome, getSize()));
		
		// Strip tomeData when building player data
//		if (transfer.IsWritingGameReleaseData())
//		{
//			Umbra::Tome* tempTomeData = (Umbra::Tome*)tempPVSData.begin();
//			Umbra::Tome::stripDebugInfo(tempTomeData);
//			tempPVSData.resize_uninitialized(tempTomeData->getSize());
//		}
	}
	
	///@TODO: make a fast path for loading tome data when we know that the data version matches. (Just alllocate&read tome data directly...)
	transfer.Transfer(tempPVSData, "m_PVSData");
	
	if (transfer.DidReadLastProperty ())
	{
		Cleanup();
		
		// In free editor don't load occlusion data since there is no way to clear or rebake it.
		if (GetBuildSettings().hasPROVersion && !tempPVSData.empty())
            m_UmbraTome = LoadUmbraTome(tempPVSData.begin(), tempPVSData.size());
	}
	
	TRANSFER(m_PVSObjectsArray);
	TRANSFER(m_PVSPortalsArray);
	
	TRANSFER_EDITOR_ONLY(m_OcclusionBakeSettings);
}

template<class TransferFunction> inline
void OcclusionBakeSettings::Transfer (TransferFunction& transfer)
{
	TRANSFER(smallestOccluder);
	TRANSFER(smallestHole);
	TRANSFER(backfaceThreshold);
}


/*
 CheckConsistency...
 
void ComputationParameterGUIChange()
{
	m_SmallestOccluder = Mathf.Max(m_SmallestOccluder, 0.1F);
	StaticOcclusionCullingVisualization.smallestOccluder = m_SmallestOccluder;
	
	m_SmallestHole = Mathf.Max(m_SmallestHole, 0.001F);
	m_SmallestHole = Mathf.Min(m_SmallestHole, m_SmallestOccluder);
	StaticOcclusionCullingVisualization.smallestHole = m_SmallestHole;
*/	

void SceneSettings::SetUmbraTome(const dynamic_array<PPtr<Renderer> >& pvsObjectsArray, const dynamic_array<PPtr<OcclusionPortal> >& portalArray, const UInt8* visibilitybuffer, int size)
{
	InvalidatePVSOnScene();
	Cleanup();

	m_PVSObjectsArray = pvsObjectsArray;
	m_PVSPortalsArray = portalArray;
	if (size != 0)
		m_UmbraTome.tome = Umbra::TomeLoader::loadFromBuffer(visibilitybuffer, size);
	else
		m_UmbraTome = UmbraTomeData();

	SetDirty();
}


GET_MANAGER(SceneSettings)

IMPLEMENT_CLASS_HAS_INIT (SceneSettings)
IMPLEMENT_OBJECT_SERIALIZE (SceneSettings)
