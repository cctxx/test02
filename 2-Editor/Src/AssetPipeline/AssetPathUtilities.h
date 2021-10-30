#pragma once

class Object;

std::string GetAssetOrScenePath(Object* object);
std::string GetAssetPathFromObject (Object* obj);
std::string GetAssetPathFromInstanceID (int instanceID);
std::string GetSceneBakedAssetsPath();