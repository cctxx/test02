#ifndef _SCRIPTINGMETHODFACTORY_H_
#define _SCRIPTINGMETHODFACTORY_H_

#include "ScriptingTypes.h"


// !!TODO: ScriptingMethodPtr Produce(ScriptingTypePtr klass, const char* name, int searchFilter) & ScriptingMethodPtr Produce(BackendNativeMethod nativeMethod) must return the same
// method if internally method is the same !!!
class IScriptingMethodFactory
{
public:
	virtual ~IScriptingMethodFactory() {}
	virtual	ScriptingMethodPtr Produce(ScriptingTypePtr klass, const char* name, int searchFilter) = 0;
	virtual	ScriptingMethodPtr Produce(BackendNativeMethod nativeMethod) = 0;
	virtual	void Release(ScriptingMethodPtr) = 0;
};

#endif
