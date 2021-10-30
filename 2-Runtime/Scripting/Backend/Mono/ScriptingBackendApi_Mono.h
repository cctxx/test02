#ifndef _SCRIPTINGBACKENDAPI_MONO_H_
#define _SCRIPTINGBACKENDAPI_MONO_H_

#include "../ScriptingTypes.h"

typedef MonoObject* (*FastMonoMethod) (void* thiz, MonoException** ex);

struct ScriptingMethod
{
	MonoMethod* monoMethod;
	FastMonoMethod fastMonoMethod;
	bool isInstance;
};

#endif
