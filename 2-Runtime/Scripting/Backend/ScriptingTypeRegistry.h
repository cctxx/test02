#ifndef _SCRIPTINGTYPEREGISTRY_Y_
#define _SCRIPTINGTYPEREGISTRY_Y_

#if ENABLE_SCRIPTING

#include <map>
#include <string>
#include "ScriptingTypes.h"
#include "IScriptingTypeProvider.h"

class ScriptingTypeRegistry
{
public:
	ScriptingTypeRegistry(IScriptingTypeProvider* scriptingTypeProvider);
	ScriptingTypePtr GetType(const char* namespaze, const char* name);
	ScriptingTypePtr GetType(BackendNativeType nativeType);
	ScriptingTypePtr GetType(ScriptingObjectPtr systemTypeInstance);
	void InvalidateCache();

private:
	typedef std::pair<std::string,std::string> NameSpaceAndNamePair;
	typedef std::map<NameSpaceAndNamePair, ScriptingTypePtr> Cache;
	Cache m_Cache;

	typedef std::map<void*, ScriptingTypePtr> NativeTypeCache;
	NativeTypeCache m_NativeTypeCache;

	IScriptingTypeProvider* m_ScriptingTypeProvider;
};

#endif

#endif
