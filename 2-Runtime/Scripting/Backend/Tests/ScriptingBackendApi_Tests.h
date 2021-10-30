#ifndef _SCRIPTINGBACKENDAPI_TESTS_H_
#define _SCRIPTINGBACKENDAPI_TESTS_H_

#if DEDICATED_UNIT_TEST_BUILD
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include <vector>

struct ScriptingType
{
	ScriptingTypePtr baseType;
	ScriptingType() : baseType(NULL) {}

	std::vector<ScriptingMethodPtr> methods;
};

struct ScriptingMethod
{
	bool instance;
	int args;
	
	ScriptingMethod() : instance(false), args(0) {}
};

struct FakeSystemTypeInstance
{
	void* nativeType;
};

#endif
#endif
