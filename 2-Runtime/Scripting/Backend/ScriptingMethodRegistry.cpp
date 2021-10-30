#include "UnityPrefix.h"
#if ENABLE_SCRIPTING
#include "ScriptingMethodRegistry.h"
#include "ScriptingMethodFactory.h"
#include "ScriptingTypeRegistry.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "ScriptingBackendApi.h"
#include "Runtime/Scripting/ScriptingManager.h"

using std::vector;


ScriptingMethodRegistry::ScriptingMethodRegistry(IScriptingMethodFactory* scriptingMethodFactory, ScriptingTypeRegistry* scriptingTypeRegistry) 
	: m_ScriptingMethodFactory(scriptingMethodFactory),
	  m_ScriptingTypeRegistry(scriptingTypeRegistry)
{
}

ScriptingMethodPtr ScriptingMethodRegistry::GetMethod(const char* namespaze, const char* className, const char* methodName, int searchOptions)
{
	ScriptingTypePtr klass = m_ScriptingTypeRegistry->GetType(namespaze,className);
	if (klass==SCRIPTING_NULL)
		return SCRIPTING_NULL;
	return GetMethod(klass,methodName,searchOptions);
}

ScriptingMethodPtr ScriptingMethodRegistry::GetMethod(BackendNativeMethod nativeMethod)
{
	NativeMethodToScriptingMethod::iterator i = m_NativeMethodToScriptingMethod.find(nativeMethod);
	if (i != m_NativeMethodToScriptingMethod.end())
		return i->second;

	ScriptingMethodPtr scriptingMethod = m_ScriptingMethodFactory->Produce(nativeMethod);
	m_NativeMethodToScriptingMethod[nativeMethod] = scriptingMethod;
	return scriptingMethod;
}

ScriptingMethodPtr ScriptingMethodRegistry::GetMethod(ScriptingTypePtr klass, const char* methodName, int searchOptions)
{
	bool found;
	ScriptingMethodPtr cached = FindInCache(klass,methodName,searchOptions,&found);
	if (found)
		return cached;

	ScriptingMethodPtr scriptingMethod = m_ScriptingMethodFactory->Produce(klass,methodName, searchOptions);
	
	if (!scriptingMethod && (!(searchOptions & kDontSearchBaseTypes)))
	{
		ScriptingTypePtr baseClass = scripting_class_get_parent (klass, *m_ScriptingTypeRegistry);
		if (baseClass)
			scriptingMethod = GetMethod(baseClass, methodName, searchOptions);
	}
	
	PlaceInCache(klass,methodName,scriptingMethod);
		
	return scriptingMethod;
}

bool MethodDescriptionMatchesSearchFilter(int searchFilter, bool isInstance, int argCount)
{
	if ((searchFilter & ScriptingMethodRegistry::kStaticOnly) && isInstance)
		return false;

	if ((searchFilter & ScriptingMethodRegistry::kInstanceOnly) && !isInstance)
		return false;

	if ((searchFilter & ScriptingMethodRegistry::kWithoutArguments) && argCount>0)
		return false;

	return true;
}

static bool MethodMatchesSearchFilter(ScriptingMethodPtr method, int searchFilter)
{
	return MethodDescriptionMatchesSearchFilter(searchFilter, scripting_method_is_instance(method), scripting_method_get_argument_count(method, GetScriptingTypeRegistry()));
}

void ScriptingMethodRegistry::AllMethodsIn(ScriptingTypePtr klass, vector<ScriptingMethodPtr>& result, int searchOptions)
{
	vector<ScriptingMethodPtr> methods; 
	scripting_class_get_methods(klass, *this, methods);
	for (vector<ScriptingMethodPtr>::iterator item = methods.begin();
		 item != methods.end();
		 item++)
	{
		if (!MethodMatchesSearchFilter(*item, searchOptions))
			continue;

		result.push_back(*item);
	}

	if (searchOptions & kDontSearchBaseTypes)
		return;

	ScriptingTypePtr baseClass = scripting_class_get_parent(klass, *m_ScriptingTypeRegistry);
	if (baseClass == SCRIPTING_NULL)
		return;

	return AllMethodsIn(baseClass, result, searchOptions);
}

void ScriptingMethodRegistry::InvalidateCache()
{
	//make sure to not delete a scriptingmethod twice, as it could be in the cache in multiple places. (in case of classes implementing interfaces & virtual methods)
	MethodsSet todelete;
	for (ClassToMethodsByName::iterator cci = m_Cache.begin(); cci != m_Cache.end(); cci++)
		for (MethodsByName::iterator mbni = cci->second.begin(); mbni != cci->second.end(); mbni++)
			for (VectorOfMethods::iterator smpi = mbni->second.begin(); smpi != mbni->second.end(); smpi++)
				todelete.insert(*smpi);

	for (NativeMethodToScriptingMethod::iterator i = m_NativeMethodToScriptingMethod.begin(); i != m_NativeMethodToScriptingMethod.end(); i++)
		todelete.insert(i->second);

	for (MethodsSet::iterator i = todelete.begin(); i != todelete.end(); i++)
		m_ScriptingMethodFactory->Release(*i);
	
	m_Cache.clear();
	m_NativeMethodToScriptingMethod.clear();
}

ScriptingMethodPtr ScriptingMethodRegistry::FindInCache(ScriptingTypePtr klass, const char* name, int searchFilter, bool* found)
{
	*found = false;
	MethodsByName* cc = FindMethodsByNameFor(klass);
	if (cc == NULL)
		return SCRIPTING_NULL;
		
	MethodsByName::iterator mbni = cc->find(name);
	if (mbni == cc->end())
		return SCRIPTING_NULL;

	for (VectorOfMethods::iterator i = mbni->second.begin(); i != mbni->second.end(); i++)
	{
		if (*i == NULL || MethodMatchesSearchFilter(*i, searchFilter))
		{
			*found = true;
			return *i;
		}
	}
	return SCRIPTING_NULL;
}

void ScriptingMethodRegistry::PlaceInCache(ScriptingTypePtr klass, const char* name, ScriptingMethodPtr method)
{
	MethodsByName* mbn = FindMethodsByNameFor(klass);
	if (mbn == NULL)
	{
		m_Cache.insert(std::make_pair(klass, MethodsByName()));
		mbn = FindMethodsByNameFor(klass);
	}
	
	VectorOfMethods vom;
	vom.push_back(method);
	mbn->insert(std::make_pair(name,vom));
}

ScriptingMethodRegistry::MethodsByName* ScriptingMethodRegistry::FindMethodsByNameFor(ScriptingTypePtr klass)
{
	ClassToMethodsByName::iterator cc = m_Cache.find(klass);
	if (cc == m_Cache.end())
		return NULL;
	return &cc->second;
}

#endif
