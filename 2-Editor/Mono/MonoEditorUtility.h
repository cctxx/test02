#pragma once

#include <set>
#include <map>
#include "Runtime/Utilities/dynamic_array.h"
struct MonoObject;
struct MonoClass;
struct MonoArray;
struct MonoString;
namespace Unity { class GameObject; class Material; }
class Object;
class Renderer;
struct UnityGUID;
struct ImportNodeUserData;
class MonoBehaviour;
template<class T>
class PPtr;
class Object;
class Texture2D;

MonoObject* FindAssetWithKlass (const std::string& path, MonoObject* klass);
MonoObject* GetMonoFirstAssetAtPath (MonoObject* type, MonoString* path);
MonoObject* GetMonoBuiltinResource (MonoObject* type, MonoString* path);

bool MonoProcessMeshHasAssignMaterial ();
void MonoPreprocessMesh (const std::string& pathName);
Unity::Material* MonoProcessAssignMaterial (Renderer& renderer, Unity::Material& material);
// returns false if the root is no longer valid after post processing
bool MonoPostprocessMesh (Unity::GameObject& root, const std::string& pathName);
void MonoPostprocessGameObjectWithUserProperties (Unity::GameObject& root, const std::vector<ImportNodeUserData>& userDefinedProperties);

void MonoPreprocessTexture (const std::string& pathName);
void MonoPostprocessTexture (Object& tex, const std::string& pathName);
void MonoPostprocessAudio (Object& tex, const std::string& pathName);
void MonoPreprocessAudio (const std::string& pathName);

MonoArray* GetMonoAllAssets (const std::string& path);

void MonoPostprocessAllAssets (const std::set<UnityGUID>& imported, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);

MonoObject* LoadAssetAtPath (const std::string& assetPath, MonoObject* type);

void InitPostprocessors (const std::string& path);
void CleanupPostprocessors ();

bool IsEditingTextField ();

// Given a MonoArray of MonoStrings representing asset path names, returns a set of GUIDs
// If the path name does not exist, the element will be skipped.
std::set<UnityGUID> MonoPathsToGUIDs (MonoArray* array);

// Given a set of GUIDs returns a MonoArray of MonoStrings representing path names.
MonoArray* GUIDsToMonoPaths (const std::set<UnityGUID>& guids);

int MonoEnumFlagsToInt(MonoObject* obj);
void GetPostProcessorVersions(dynamic_array<UInt32>& versions, std::string const& function);
bool DetectDotNetDll (const std::string& path);