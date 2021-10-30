#include "UnityPrefix.h"
#include "AssetPathUtilities.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/GUIDPersistentManager.h"

using namespace std;

string GetAssetOrScenePath (Object* object)
{
	if (object == NULL)
		return string();
	
	if ((object->GetHideFlags() & Object::kDontSave) != 0)
		return string();
	
	if (!object->IsPersistent())
		return GetApplication().GetCurrentScene();
	
	return GetAssetPathFromInstanceID(object->GetInstanceID());
}

std::string GetAssetPathFromObject (Object* obj)
{
	if (obj == NULL)
		return string();

	return GetAssetPathFromInstanceID(obj->GetInstanceID());
}

std::string GetAssetPathFromInstanceID (int instanceID)
{
	string path = GetPersistentManager().GetPathName(instanceID);
	return AssetPathNameFromAnySerializedPath(path);
}

std::string GetSceneBakedAssetsPath()
{
	string path = GetApplication().GetCurrentScene();
	if (!EndsWith(ToLower(path), ".unity"))
		return string();
	
	// cut 6 last chars (.unity)
	path = path.substr(0, path.length()-6);
	return path;
}