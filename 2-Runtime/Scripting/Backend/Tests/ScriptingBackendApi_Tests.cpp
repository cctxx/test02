#include "UnityPrefix.h"

#if DEDICATED_UNIT_TEST_BUILD
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/Backend/Tests/ScriptingBackendApi_Tests.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"

bool scripting_method_is_instance(ScriptingMethodPtr method)
{
	return method->instance;
}

int scripting_method_get_argument_count(ScriptingMethodPtr method)
{
	return method->args;
}

ScriptingTypePtr scripting_class_get_parent(ScriptingTypePtr type, ScriptingTypeRegistry& typeRegistry)
{
	return type->baseType;
}

void scripting_class_get_methods(ScriptingTypePtr type, ScriptingMethodRegistry& registry, std::vector<ScriptingMethodPtr>& result)
{
	result = type->methods;
}

ScriptingTypePtr scripting_class_from_systemtypeinstance(ScriptingObjectPtr type, ScriptingTypeRegistry& typeRegistry)
{
	FakeSystemTypeInstance* sti = (FakeSystemTypeInstance*) type;
	return typeRegistry.GetType(sti->nativeType);
}

#endif
