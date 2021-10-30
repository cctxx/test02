#ifndef _SCRIPTINGBACKENDAPI_FLASH_H_
#define _SCRIPTINGBACKENDAPI_FLASH_H_

#include <string>

#include "../ScriptingTypes.h"
#include "../ScriptingBackendApi.h"

//todo: remove
typedef ScriptingObject* (*FastMonoMethod) (void* thiz, ScriptingException** ex);
typedef int AS3String;

struct ScriptingMethod
{
	const char* m_Name;
	const char* m_Mappedname;
	const char* m_Signature;
	AS3String m_As3String;

	ScriptingClass* m_Class;
	ScriptingObject* m_SystemType;
	ScriptingObject* m_MethodInfo;

	ScriptingMethod(const char* name, ScriptingClass* klass);
	ScriptingMethod(const char* name, const char* mappedName, const char* sig, ScriptingClass* klass);
	void Init(const char* name, const char* mappedName, const char* sig, ScriptingClass* klass);	
	ScriptingClass* GetReturnType();
	const char* GetName() { return m_Name; }
	ScriptingObjectPtr GetSystemReflectionMethodInfo();
};

struct ScriptingField
{
	ScriptingField(const char* name,const char* type)
		: m_name(name)
		, m_type(type)
	{

	}

	std::string m_name;
	std::string m_type;
};

#endif
