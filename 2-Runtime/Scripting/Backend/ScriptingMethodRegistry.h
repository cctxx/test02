#ifndef _SCRIPTINGMETHODREGISTRY_H_
#define _SCRIPTINGMETHODREGISTRY_H_

#if ENABLE_SCRIPTING
#include "ScriptingTypes.h"

#include <map>
#include <string>
#include <vector>

class IScriptingMethodFactory;
class ScriptingTypeRegistry;

class ScriptingMethodRegistry
{
public:
	ScriptingMethodRegistry(IScriptingMethodFactory* scriptingMethodFactory, ScriptingTypeRegistry* scriptingTypeRegistry);
	~ScriptingMethodRegistry() {InvalidateCache();}

	enum SearchFilter
	{
		kNone = 0,
		kInstanceOnly = 1,
		kStaticOnly = 2,
		kWithoutArguments = 4,
		kDontSearchBaseTypes = 8,
	};

	ScriptingMethodPtr GetMethod(const char* namespaze, const char* klassName, const char* methodName, int searchOptions=kNone);
	ScriptingMethodPtr GetMethod(ScriptingTypePtr klass, const char* methodName, int searchOptions=kNone);
	ScriptingMethodPtr GetMethod(BackendNativeMethod nativeMathod);

	void AllMethodsIn(ScriptingTypePtr klass, std::vector<ScriptingMethodPtr>& result, int searchOptions=kNone);

	void InvalidateCache();

private:
	typedef std::vector<ScriptingMethodPtr> VectorOfMethods;
	typedef std::map< std::string,VectorOfMethods > MethodsByName;
	typedef std::map<ScriptingTypePtr, MethodsByName> ClassToMethodsByName;
	typedef std::map<BackendNativeMethod, ScriptingMethodPtr> NativeMethodToScriptingMethod;
	typedef std::set<ScriptingMethodPtr> MethodsSet;

	ClassToMethodsByName m_Cache;

	
	NativeMethodToScriptingMethod m_NativeMethodToScriptingMethod;

	IScriptingMethodFactory* m_ScriptingMethodFactory;
	ScriptingTypeRegistry* m_ScriptingTypeRegistry;

	ScriptingMethodPtr FindInCache(ScriptingTypePtr klass, const char* name, int searchFilter, bool* out_found);
	void PlaceInCache(ScriptingTypePtr klass, const char* name, ScriptingMethodPtr method);
	MethodsByName* FindMethodsByNameFor(ScriptingTypePtr klass);
};

bool MethodDescriptionMatchesSearchFilter(int searchFilter, bool isInstance, int argCount);
#endif

#endif