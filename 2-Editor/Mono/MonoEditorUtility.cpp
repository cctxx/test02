#include "UnityPrefix.h"
#include "MonoEditorUtility.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/AssetPipeline/ImportMesh.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/GetComponent.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Scripting.h"

MonoObject* FindAssetWithKlass (const string& path, MonoObject* type)
{
	if (path.empty ())
		return NULL;

	UnityGUID guid = GetGUIDPersistentManager ().CreateAsset (AppendPathName("Assets", path));
	if (guid == UnityGUID())
		return NULL;

	if (!AssetDatabase::Get().IsAssetAvailable(guid))
		return NULL;

	if (type == NULL)
		Scripting::RaiseNullException("type is null");
	MonoClass* klass = GetScriptingTypeRegistry().GetType(type);

	const Asset& asset = AssetDatabase::Get().AssetFromGUID (guid);

	if (mono_class_is_subclass_of(klass, GetMonoManager().GetCommonClasses().component, true))
	{
		GameObject* go = dynamic_pptr_cast<GameObject*> (asset.mainRepresentation.object);
		if (go)
		{
			MonoObject* object = ScriptingGetComponentOfType(*go, type);
			if (object && !Scripting::CompareBaseObjects(object, NULL))
				return object;
		}

		for (int i=0;i<asset.representations.size();i++)
		{
			go  = dynamic_pptr_cast<GameObject*> (asset.representations[i].object);
			if (go)
			{
				MonoObject* object = ScriptingGetComponentOfType(*go, type);
				if (object && !Scripting::CompareBaseObjects(object, NULL))
					return object;
			}
		}
	}
	else
	{
		MonoObject* object = Scripting::ScriptingWrapperFor(asset.mainRepresentation.object);
		if (object && mono_class_is_subclass_of(mono_object_get_class(object), klass, true))
		{
			return object;
		}
		for (int i=0;i<asset.representations.size();i++)
		{
			object = Scripting::ScriptingWrapperFor(asset.representations[i].object);
			if (object && mono_class_is_subclass_of(mono_object_get_class(object), klass, true))
				return object;
		}
	}
	return NULL;
}



MonoObject* GetMonoFirstAssetAtPath (MonoObject* type, MonoString* path)
{
	MonoClass* requiredclass = GetScriptingTypeRegistry().GetType(type);
	int classID = Scripting::GetClassIDFromScriptingClass(requiredclass);
	Object* o = FindFirstAssetWithClassID(MonoStringToCpp(path), classID);
	MonoObject* mono = Scripting::ScriptingWrapperFor(o);
	if (mono && mono_class_is_subclass_of(mono_object_get_class(mono), requiredclass, true))
		return mono;
	else
		return NULL;
}

MonoArray* GetMonoAllAssets (const std::string& path)
{
	return CreateScriptingArrayFromUnityObjects(FindAllAssetsAtPath(path), ClassID(Object));
}

void GetPostProcessorVersions (dynamic_array<UInt32>& versions, string const& function)
{
	MonoArray* monoArray = reinterpret_cast<MonoArray*>(CallStaticMonoMethod("AssetPostprocessingInternal", function.c_str()));
	ScriptingArrayToDynamicArray<UInt32>(monoArray, versions);
}

bool MonoPostprocessMesh (GameObject& root, const std::string& pathName)
{
	ScriptingObjectOfType<GameObject> obj = Scripting::ScriptingWrapperFor(&root);

	ScriptingInvocation invocation("UnityEditor","AssetPostprocessingInternal", "PostprocessMesh");
	invocation.AddObject(obj.GetScriptingObject());
	invocation.AddString(pathName.c_str());
	invocation.AdjustArgumentsToMatchMethod();
	invocation.InvokeChecked();

	return obj.GetCachedPtr() != NULL;
}

void MonoPostprocessGameObjectWithUserProperties(GameObject& go, const std::vector<ImportNodeUserData>& userData)
{
	std::vector<std::string> propnames;

	MonoArray *arr = mono_array_new (mono_domain_get (), mono_get_object_class(), userData.size());
	for (int i = 0; i < userData.size();i++) {
		const ImportNodeUserData& ud = userData[i];

		bool supportedprop = true;
		MonoObject* obj;

		switch(ud.data_type_indicator)
		{
		case kUserDataBool:
			{
				MonoClass* klass = mono_class_from_name (mono_get_corlib (), "System", "Boolean");
				obj = mono_object_new(mono_domain_get(), klass);
				bool& b = ExtractMonoObjectData<bool>(obj);
				b = ud.boolData;
			}
			break;
		case kUserDataFloat:
			{
				obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().floatSingle);
				float& f = ExtractMonoObjectData<float>(obj);
				f = ud.floatData;
			}
			break;
		case kUserDataColor:
			{
				obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().color);
				ColorRGBAf& c = ExtractMonoObjectData<ColorRGBAf>(obj);
				c.a = ud.colorData.a / 255.;
				c.r = ud.colorData.r / 255.;
				c.g = ud.colorData.g / 255.;
				c.b = ud.colorData.b / 255.;
			}
			break;
		case kUserDataInt:
			{
				obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().int_32);
				int& i = ExtractMonoObjectData<int>(obj);
				i = ud.intData;
			}
			break;
		case kUserDataVector:
			{
				obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().vector4);
				Vector4f& v = ExtractMonoObjectData<Vector4f>(obj);
				v = ud.vectorData;
			}
			break;
		case kUserDataString:
			{
				obj = (MonoObject*) scripting_string_new(ud.stringData.c_str());
			}
			break;
		default:
			supportedprop = false;
		}
		if (supportedprop)
		{
			propnames.push_back(ud.name);
			GetMonoArrayElement<MonoObject*> (arr,i) = obj;
		}

	}

	void* params[] = { Scripting::ScriptingWrapperFor(&go), Scripting::StringVectorToMono(propnames), arr };

	CallStaticMonoMethod ("AssetPostprocessingInternal","PostprocessGameObjectWithUserProperties", params);
}

void MonoPreprocessMesh (const std::string& pathName)
{
	void* params[] = { scripting_string_new(pathName) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "PreprocessMesh", params);
}

bool MonoProcessMeshHasAssignMaterial ()
{
	return MonoObjectToBool (CallStaticMonoMethod ("AssetPostprocessingInternal", "ProcessMeshHasAssignMaterial"));
}

Material* MonoProcessAssignMaterial (Renderer& renderer, Material& material)
{
	void* params[] = { Scripting::ScriptingWrapperFor(&renderer), Scripting::ScriptingWrapperFor(&material) };
	return ScriptingObjectToObject<Material> (CallStaticMonoMethod ("AssetPostprocessingInternal", "ProcessMeshAssignMaterial", params));
}

void MonoPreprocessTexture (const std::string& pathName)
{
	void* params[] = { scripting_string_new(pathName) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "PreprocessTexture", params);
}

void MonoPostprocessTexture (Object& tex, const std::string& pathName)
{
	void* params[] = { Scripting::ScriptingWrapperFor(&tex), scripting_string_new(pathName) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "PostprocessTexture", params);
}

void MonoPreprocessAudio (const std::string& pathName)
{
	void* params[] = { scripting_string_new(pathName) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "PreprocessAudio", params);
}

void MonoPostprocessAudio (Object& tex, const std::string& pathName)
{
	void* params[] = { Scripting::ScriptingWrapperFor(&tex), scripting_string_new(pathName) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "PostprocessAudio", params);
}

void InitPostprocessors (const std::string& path)
{
	void* params[] = { scripting_string_new(path) };
	CallStaticMonoMethod ("AssetPostprocessingInternal", "InitPostprocessors", params);
}

void CleanupPostprocessors ()
{
	CallStaticMonoMethod ("AssetPostprocessingInternal", "CleanupPostprocessors");
}

bool IsEditingTextField ()
{
	if (GetMonoManagerPtr() == NULL)
		return false;

	return MonoObjectToBool(CallStaticMonoMethod ("EditorGUI", "IsEditingTextField"));
}

void MonoPostprocessAllAssets (const std::set<UnityGUID>& imported, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved)
{
	vector<string> importedArray;
	for (set<UnityGUID>::const_iterator i=imported.begin();i!=imported.end();i++)
		importedArray.push_back(GetAssetPathFromGUID(*i));

	vector<string> addedArray;
	for (set<UnityGUID>::const_iterator i=added.begin();i!=added.end();i++)
		addedArray.push_back(GetAssetPathFromGUID(*i));

	vector<string> removedArray;
	for (set<UnityGUID>::const_iterator i=removed.begin();i!=removed.end();i++)
		removedArray.push_back(GetAssetPathFromGUID(*i));

	vector<string> movedArray;
	vector<string> movedFromArray;
	for (map<UnityGUID, string>::const_iterator i=moved.begin();i!=moved.end();i++)
	{
		movedArray.push_back(GetAssetPathFromGUID(i->first));
		movedFromArray.push_back(i->second);
	}

	void* params[] = { Scripting::StringVectorToMono(importedArray), Scripting::StringVectorToMono(addedArray), Scripting::StringVectorToMono(removedArray), Scripting::StringVectorToMono(movedArray), Scripting::StringVectorToMono(movedFromArray) };

	CallStaticMonoMethod ("AssetPostprocessingInternal", "PostprocessAllAssets", params);
}

MonoObject* LoadAssetAtPath (const string& assetPath, MonoObject* type)
{
	MonoClass* klass = GetScriptingTypeRegistry().GetType(type);
	if (klass == NULL)
		return NULL;

	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	UnityGUID guid;
	if (!pm.PathNameToGUID(assetPath, &guid))
		return NULL;

	const Asset* asset = AssetDatabase::Get().AssetPtrFromGUID(guid);
	if (asset == NULL)
		return NULL;

	// Try casting main representation to the requested type
	MonoObject* o = Scripting::ScriptingWrapperFor(asset->mainRepresentation.object);
	if (o && mono_class_is_subclass_of(mono_object_get_class(o), klass, true))
		return o;

	// Try get component if we are dealing with a prefab
	GameObject* go = dynamic_pptr_cast<GameObject*> (asset->mainRepresentation.object);
	if (go != NULL)
	{
		o = ScriptingGetComponentOfType(*go, type, false);
		if (o != NULL)
			return o;
	}

	// Go through all main representations of that asset
	for (int i=0;i<asset->representations.size();i++)
	{
		// We only want the root game object in the resources. Not all child game objects!
		if (go != NULL && asset->representations[i].classID == ClassID(GameObject))
			continue;

		o = Scripting::ScriptingWrapperFor(asset->representations[i].object);
		if (o && mono_class_is_subclass_of(mono_object_get_class(o), klass, true))
			return o;
	}
	return NULL;
}

set<UnityGUID> MonoPathsToGUIDs (MonoArray* array)
{
	set<UnityGUID> cguids;
	GUIDPersistentManager& pm = GetGUIDPersistentManager();

	for (int i = 0; i < mono_array_length_safe(array); i++)
	{
		UnityGUID guid;
		string path = MonoStringToCpp(GetMonoArrayElement<MonoString*> (array, i));
		ConvertSeparatorsToUnity(path);

		if (pm.PathNameToGUID(path, &guid ))
			cguids.insert(guid);
	}
	return cguids;
}

MonoArray* GUIDsToMonoPaths (const set<UnityGUID>& guids)
{
	vector<string> cstrings;
	cstrings.reserve(guids.size());
	GUIDPersistentManager& pm = GetGUIDPersistentManager();

	for (set<UnityGUID>::const_iterator i = guids.begin(); i != guids.end(); i++)
	{
		cstrings.push_back(pm.AssetPathNameFromGUID(*i));
	}

	return Scripting::StringVectorToMono (cstrings);
}

int MonoEnumFlagsToInt (MonoObject* obj)
{
	if (obj != NULL)
	{
		MonoClass* cs = mono_object_get_class(obj);
		MonoType* enumMonoType = mono_class_enum_basetype (cs);
		if (enumMonoType)
		{
			int enumType = mono_type_get_type (enumMonoType);
			switch (enumType)
			{
			case MONO_TYPE_I4:
				return ExtractMonoObjectData<int>(obj);
				break;
			case MONO_TYPE_U1:
				return static_cast<int>(ExtractMonoObjectData<UInt8>(obj));
				break;
			default:
				AssertMsg(false, "Enum not of correct type");
			}
		}
		else
		{
			AssertMsg(false, "MonoObject is not an enum type");
		}
	}
	else
	{
		AssertMsg(false, "Invalid MonoObject passed");
	}

	return 0;
}

inline bool IgnoredPluginPath(const string& path)
{
	return (ToLower(path).find("assets/plugins/metro/") != -1)		// Those are windows platforms, and on windows file system is case insensitive.
#if INCLUDE_WP8SUPPORT
		|| (ToLower(path).find("assets/plugins/wp8/") != -1)
#endif
		;
}

bool DetectDotNetDll (const std::string& path)
{
	if (IgnoredPluginPath(path))
	{
		printf_console("\n...Ignoring platform specific dll: %s\n", path.c_str());
		return false;
	}

	InputString data;
	if (ReadStringFromFile(&data, path))
	{
		int status;
		MonoImage* image = mono_image_open_from_data_full (data.c_str(), data.size(), /*Copy data*/true, &status, false /* ref only*/);
		if (image == NULL)
			return false;
		else
		{
			mono_image_close(image);
			return true;
		}
	}
	return false;
}
