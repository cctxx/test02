#ifndef MONOATTRIBUTEHELPERS_H
#define MONOATTRIBUTEHELPERS_H

#include "MonoIncludes.h"
#include "Configuration/UnityConfigure.h"
#if UNITY_EDITOR && ENABLE_MONO
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include <vector>

struct ScriptingArguments;

//void GetMethodsWithAttribute (ScriptingClass* attributeClass, void** requiredParameters, MonoMethod* comparedParams, std::vector<MonoMethod*>& resultList);
void CallMethodsWithAttribute (ScriptingClass* attributeClass, ScriptingArguments& arguments, MonoMethod* comparedParams);
bool CallMethodsWithAttributeAndReturnTrueIfUsed (ScriptingClass* attributeClass, ScriptingArguments& arguments, MonoMethod* comparedParams);

#endif //UNITY_EDITOR && ENABLE_MONO

#endif //MONOATTRIBUTEHELPERS_H
