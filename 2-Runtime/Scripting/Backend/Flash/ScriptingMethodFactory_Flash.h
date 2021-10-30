#ifndef _SCRIPTINGMETHODFACTORY_FLASH_
#define _SCRIPTINGMETHODFACTORY_FLASH_

#include "../ScriptingMethodFactory.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "ScriptingBackendApi_Flash.h"

#if UNITY_FLASH

extern "C" const char* Ext_GetMappedMethodName(const char* name, ScriptingType* klass);

class ScriptingMethodFactory_Flash : public IScriptingMethodFactory
{
public:
	virtual ScriptingMethodPtr Produce(ScriptingTypePtr klass, const char* name, int searchFilter)
	{
		//todo: respect the searchfilter
		std::string mappedName(Ext_GetMappedMethodName(name,klass));
		
		//remove this hack when interfaces contain mapping information
		if (mappedName.size()==0 && strcmp(name,"MoveNext")==0)
			mappedName.assign("IEnumerator_MoveNext");
		return new ScriptingMethod(name, mappedName.c_str(), "",klass);
	}

	virtual ScriptingMethodPtr Produce(void* nativeMethod)
	{
		//not implemented for flash.
		return NULL;
	}

	virtual void Release(ScriptingMethodPtr method)
	{
		delete method;
	}
};

#endif

#endif
