#ifndef _ISCRIPTINGTYPEPROVIDER_H_
#define _ISCRIPTINGTYPEPROVIDER_H_

#include "ScriptingTypes.h"
#include "Runtime/Modules/ExportModules.h"

class EXPORT_COREMODULE IScriptingTypeProvider
{
public:
	virtual ~IScriptingTypeProvider() {}
	virtual BackendNativeType NativeTypeFor(const char* namespaze, const char* name) = 0;
	virtual ScriptingTypePtr Provide(BackendNativeType nativeType) = 0;
	virtual void Release(ScriptingTypePtr t) = 0;
};

#endif
