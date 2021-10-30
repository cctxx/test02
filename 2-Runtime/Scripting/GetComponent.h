#ifndef GETCOMPONENT_H
#define GETCOMPONENT_H

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoIncludes.h"

#if ENABLE_SCRIPTING

ScriptingArrayPtr ScriptingGetComponentsOfType (Unity::GameObject& go, ScriptingObjectPtr reflectionTypeObject, bool useSearchTypeAsArrayReturnType, bool recursive, bool includeInactive);
ScriptingObjectPtr ScriptingGetComponentOfType (Unity::GameObject& go, ScriptingObjectPtr reflectionTypeObject, bool generateErrors = true);
ScriptingObjectPtr ScriptingGetComponentOfType (GameObject& go, ScriptingClassPtr systemTypeInstance);

#endif

#endif
