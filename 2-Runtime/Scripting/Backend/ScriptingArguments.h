#ifndef _SCRIPTINGARGUMENTS_H_
#define _SCRIPTINGARGUMENTS_H_

#if ENABLE_SCRIPTING
#include "ScriptingTypes.h"
#include <string>
#include "Runtime/Modules/ExportModules.h"


struct EXPORT_COREMODULE ScriptingArguments
{
	enum Constants
	{
		MAXARGS=10
	};

	enum ArgType
	{
		ARGTYPE_BOOLEAN,
		ARGTYPE_INT,
		ARGTYPE_FLOAT,
		ARGTYPE_STRING,
		ARGTYPE_OBJECT,
		ARGTYPE_STRUCT,
		ARGTYPE_ARRAY,
		ARGTYPE_ENUM
	};

	//this setup is kind of weird. some types of arguments just need to be stuffed in m_Arguments,
	//however for ints and floats, instead of stuffing them in, mono actually excepts a pointer to one.
	//to make it happy, we store the actual int in a field, and store a pointer to it in the ScriptingParam.
	union
	{
		int ints[MAXARGS];
		float floats[MAXARGS];
	} m_PrimitiveStorage;

#if UNITY_WINRT
	Platform::Array<Platform::String^>^ m_StringArguments;
	ScriptingParams m_Arguments[MAXARGS];
#else
	const void* m_Arguments[MAXARGS];
#endif
	int m_ArgumentTypes[MAXARGS];
	int m_Count;

	ScriptingArguments();

	void AddBoolean(bool value);
	void AddInt(int value);
	void AddFloat(float value);
	void AddString(const char* str);
	void AddString(std::string& str);
	void AddObject(ScriptingObjectPtr scriptingObject);
	void AddStruct(void* pointerToStruct);
	void AddEnum(int value);
	void AddArray(ScriptingArrayPtr arr);

	bool GetBooleanAt(int index);
	int GetIntAt(int index);
	float GetFloatAt(int index);
	const void* GetStringAt(int index);
	ScriptingObjectPtr GetObjectAt(int index);

	void** InMonoFormat();
	
	void AdjustArgumentsToMatchMethod(ScriptingMethodPtr method);
	bool CheckArgumentsAgainstMethod(ScriptingMethodPtr method);
#if UNITY_WINRT
	ScriptingParamsPtr InMetroFormat();
#endif

	int GetTypeAt(int index);
	int GetCount();
};

#endif

#endif
