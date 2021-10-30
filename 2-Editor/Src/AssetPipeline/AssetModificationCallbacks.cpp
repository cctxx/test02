#include "UnityPrefix.h"
#include "AssetModificationCallbacks.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Mono/MonoEditorUtility.h"

//* Places that are not covered by AssetModificationCallbacks

//////@TODO: Uncovered places

// CRITICAL: 
// * void AssetDatabase::WriteSerializedAssets (const UnityGUID& guid) looks totally weird...
// * CreateSerializedAsset has a  mode where it can override existing assets. What callback should we call???



// Application.cpp 530:
// 	if (GetPersistentManager().TestNeedWriteFile(kProjectSettingsPath) == 1)
// 	GetPersistentManager().WriteFile(kProjectSettingsPath);

// AssetInterface.cpp 317
// if (GetPersistentManager().TestNeedWriteFile(kEditorSettingsPath) == 1)
// GetPersistentManager().WriteFile(kEditorSettingsPath);

// AssetInterface.cpp ProduceSingletonAsset



void AssetModificationCallbacks::WillCreateAssetFile (const std::string &path)
{
	void* args[] = { MonoStringNew (path) };
	CallStaticMonoMethod("AssetModificationProcessorInternal", "OnWillCreateAsset", args);
}

bool AssetModificationCallbacks::ShouldSaveSingleFile (const std::string &path)
{
	MonoArray *inputArray = mono_array_new (mono_domain_get (), mono_get_string_class(), 1);
	GetMonoArrayElement<MonoString*> (inputArray, 0) = MonoStringNew (path);

	int explicitlySaveScene = false;
	MonoArray *arr1, *arr2;
	void* args[] = { inputArray, &arr1, &arr2, &explicitlySaveScene };
	CallStaticMonoMethod("AssetModificationProcessorInternal", "OnWillSaveAssets", args);
	
	if (arr1 && mono_array_length_safe(arr1) == 1)
		return true;

	return false;
}

void MonoArrayToGUIDSet (MonoArray *arr, std::set<UnityGUID>& guids)
{
	if (arr)
	{
		guids.clear();
		for (int i = 0; i < mono_array_length_safe(arr); i++)
		{
			UnityGUID guid;
			std::string path = MonoStringToCpp (GetMonoArrayElement<MonoString*> (arr, i));
			GetGUIDPersistentManager().PathNameToGUID (path, &guid);
			guids.insert (guid);
		}
	}
}

bool AssetModificationCallbacks::CheckoutIfRequired(const std::set<UnityGUID>& assets, const std::string& promptIfCheckoutNeeded)
{
	MonoArray *inputArray = mono_array_new (mono_domain_get (), mono_get_string_class(), assets.size());
	int index = 0;
	for (std::set<UnityGUID>::const_iterator i = assets.begin(); i != assets.end() ;i++)
	{
		MonoString *obj = MonoStringNew (GetAssetPathFromGUID (*i));
		GetMonoArrayElement<MonoString*> (inputArray, index++) = obj;
	}
	MonoString* monoPromptIfCheckoutNeeded = MonoStringNew(promptIfCheckoutNeeded);
	void* args[] = { inputArray, &monoPromptIfCheckoutNeeded };
	
	MonoObject* monoResult = CallStaticMonoMethod("AssetModificationProcessorInternal", "CheckoutIfRequired", args);
	return ExtractMonoObjectData<bool>(monoResult);
}

void AssetModificationCallbacks::FileModeChanged(const std::set<UnityGUID>& assets, FileMode mode)
{
	MonoArray *inputArray = mono_array_new (mono_domain_get (), mono_get_string_class(), assets.size());
	int index = 0;
	for (std::set<UnityGUID>::const_iterator i = assets.begin(); i != assets.end() ;i++)
	{
		MonoString *obj = MonoStringNew (GetAssetPathFromGUID (*i));
		GetMonoArrayElement<MonoString*> (inputArray, index++) = obj;
	}

	void* args[] = { inputArray, &mode };
	CallStaticMonoMethod("AssetModificationProcessorInternal", "FileModeChanged", args);
}

void AssetModificationCallbacks::ShouldSaveAssets (const std::set<UnityGUID>& assets, std::set<UnityGUID>& outAssetsThatShouldBeSaved, std::set<UnityGUID>& outAssetsThatShouldBeReverted, bool explicitlySaveScene)
{
	MonoArray *inputArray = mono_array_new (mono_domain_get (), mono_get_string_class(), assets.size());
	int index = 0;
	for (std::set<UnityGUID>::const_iterator i = assets.begin(); i != assets.end() ;i++) 
	{
		MonoString *obj = MonoStringNew (GetAssetPathFromGUID (*i));
		GetMonoArrayElement<MonoString*> (inputArray, index++) = obj;
	}
	
	MonoArray *arr1, *arr2;
	int tempExplicitlySave = explicitlySaveScene;
	void* args[] = { inputArray, &arr1, &arr2, &tempExplicitlySave };
	CallStaticMonoMethod("AssetModificationProcessorInternal", "OnWillSaveAssets", args);
	
	MonoArrayToGUIDSet (arr1, outAssetsThatShouldBeSaved);
	MonoArrayToGUIDSet (arr2, outAssetsThatShouldBeReverted);
}	

int AssetModificationCallbacks::WillMoveAsset( std::string const& fromPath, std::string const& toPath)
{
	MonoString* monoFrom = MonoStringNew(fromPath);
	MonoString* monoTo = MonoStringNew(toPath);

	void* args[] = { monoFrom, monoTo };
	MonoObject* monoResult = CallStaticMonoMethod("AssetModificationProcessorInternal", "OnWillMoveAsset", args);
	return MonoEnumFlagsToInt(monoResult);
}

int AssetModificationCallbacks::WillDeleteAsset( std::string const& asset, AssetDatabase::RemoveAssetOptions options )
{
	MonoString* assetPath = MonoStringNew(asset);

	void* args[] = {assetPath, &options};
	MonoObject* monoResult = CallStaticMonoMethod("AssetModificationProcessorInternal", "OnWillDeleteAsset", args);
	return MonoEnumFlagsToInt(monoResult);
}

void AssetModificationCallbacks::OnStatusUpdated()
{
	CallStaticMonoMethod("AssetModificationProcessorInternal", "OnStatusUpdated");
}

