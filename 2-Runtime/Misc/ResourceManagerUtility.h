#ifndef RESOURCEMANAGERUTILITY_H
#define RESOURCEMANAGERUTILITY_H

#include "UnityPrefix.h"

#include <string>

#include "Runtime/Scripting/Backend/ScriptingTypes.h"

class BuiltinResourceManager;

ScriptingObjectPtr GetScriptingBuiltinResourceFromManager(BuiltinResourceManager& resources, ScriptingObjectPtr type, const std::string& path);
ScriptingObjectPtr GetScriptingBuiltinResource(ScriptingObjectPtr type, const std::string& path);

#if UNITY_EDITOR

ScriptingObjectPtr GetMonoBuiltinExtraResource(ScriptingObjectPtr type, ScriptingStringPtr path);

#endif

#endif
