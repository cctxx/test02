#include "UnityPrefix.h"

#include "Runtime/BaseClasses/EditorExtension.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "AssetReferenceFilter.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferFunctions/RemapPPtrTransfer.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/Serialize/CacheWrap.h"
#include "Editor/Src/GUIDPersistentManager.h"

class TestReferencingFunctor : public GenerateIDFunctor
{
public:
	int m_StopId;
	std::set<int> m_Visited;
	
	TestReferencingFunctor (int stop) : m_StopId(stop) {}
	
	virtual SInt32 GenerateInstanceID (SInt32 instanceID, TransferMetaFlags metaFlag = kNoTransferFlags)
	{
		if (Object* targetObject = dynamic_instanceID_cast<Object*> (instanceID))
		{			
			if (m_Visited.count (instanceID) == 0)
			{
				m_Visited.insert (instanceID);
				
				// do not follow Transform as that might have a reference to another Transform (parent)
				// and then the crawler will pull in other GameObjects and almost everything else.
				if (instanceID != m_StopId && !targetObject->IsDerivedFrom (ClassID(Transform)))
				{
					RemapPPtrTransfer transferFunction (kSerializeGameRelease, false);
					transferFunction.SetGenerateIDFunctor (this);
					targetObject->VirtualRedirectTransfer (transferFunction);
				}
			}
		}
		
		return instanceID;
	}
};

bool IsReferencing (int parentInstanceId, int childInstanceId)
{
	if (childInstanceId == 0)
		return false;

	TestReferencingFunctor search (parentInstanceId);
	
	if (GameObject* parentO = dynamic_instanceID_cast<GameObject*>(parentInstanceId))
	{
		// Check if referencing a prefab
		if (parentO->m_PrefabParentObject.GetInstanceID () == childInstanceId)
			return true;

		for (int i=0; i < parentO->GetComponentCount (); ++i)
		{
			Unity::Component& component = parentO->GetComponentAtIndex (i);
			
			if (!component.IsDerivedFrom (ClassID(Transform)))
			{
				RemapPPtrTransfer transferFunction (kSerializeGameRelease, false);
				transferFunction.SetGenerateIDFunctor (&search);
				component.VirtualRedirectTransfer (transferFunction);
			}
		}
		
		if (search.m_Visited.count (childInstanceId))
			return true;
	}
	
	return false;
}

bool HasReferenceInExternalRefs (std::string const& localPathName, UnityGUID const& child)
{
	std::string const& absolutePathName = GetPersistentManager().RemapToAbsolutePath (localPathName);
	
	SerializedFile* file = UNITY_NEW(SerializedFile,kMemTempAlloc);
	
	ResourceImageGroup group;
	bool found = false;
	if (file->InitializeRead(absolutePathName, group, 512, 4, 0))
	{
		dynamic_block_vector<FileIdentifier> const& files = file->GetExternalRefs();
		for (int i=0;i<files.size();i++)
		{
			UnityGUID const& guid = files[i].guid;
			if (guid == child)
			{
				found = true;
				break;
			}
		}
	}
	UNITY_DELETE(file,kMemTempAlloc);
	return found;
}

bool IsReferencingAsset (UnityGUID const& parent, UnityGUID const& child)
{
	if (AssetDatabase::Get ().IsAssetAvailable (parent))
	{
		if (AssetDatabase::Get ().AssetFromGUID (parent).type == kFolderAsset)
			return false;
	} else
		return false;
	
	std::string metaPath = GetMetaDataPathFromGUID (parent);
	std::string assetPath = GetAssetPathFromGUID (parent);
	
	if (HasReferenceInExternalRefs (metaPath, child))
		return true;
	else if (HasReferenceInExternalRefs (assetPath, child))
		return true;
	else
		return false;
}
