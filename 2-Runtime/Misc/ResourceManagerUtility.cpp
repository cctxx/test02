#include "ResourceManagerUtility.h"

#if ENABLE_SCRIPTING

#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"

ScriptingObjectPtr GetScriptingBuiltinResourceFromManager(BuiltinResourceManager& resources, ScriptingObjectPtr type, const std::string& path)
{

#if ENABLE_MONO || UNITY_WINRT
	if(path.size() == 0) Scripting::RaiseArgumentException("Invalid path");
#endif

	ScriptingTypePtr requiredclass = GetScriptingTypeRegistry().GetType(type);
	int classID = Scripting::GetClassIDFromScriptingClass(GetScriptingTypeRegistry().GetType(type));

	Object* o = resources.GetResource (classID, path);

	ScriptingObjectPtr mono = Scripting::ScriptingWrapperFor(o);

#if ENABLE_MONO
	// The third parameter 'false' doesn't match in scripting_class_is_subclass_of, need to check
	if (mono != SCRIPTING_NULL && mono_class_is_subclass_of(mono_object_get_class(mono), requiredclass, false))
		return mono;
	else
		return SCRIPTING_NULL;
#else
	if (mono != SCRIPTING_NULL && scripting_class_is_subclass_of(scripting_object_get_class(mono, GetScriptingTypeRegistry()), requiredclass))
		return mono;
	else
		return SCRIPTING_NULL;
#endif
}

ScriptingObjectPtr GetScriptingBuiltinResource(ScriptingObjectPtr type, const std::string& path)
{
	return GetScriptingBuiltinResourceFromManager(GetBuiltinResourceManager(), type, path);
}

#endif


#if UNITY_EDITOR

ScriptingObjectPtr GetMonoBuiltinExtraResource(ScriptingObjectPtr type, ScriptingStringPtr path)
{
	return GetScriptingBuiltinResourceFromManager(GetBuiltinExtraResourceManager(), type, scripting_cpp_string_for(path));
}

#endif
