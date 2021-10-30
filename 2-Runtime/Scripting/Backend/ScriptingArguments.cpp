#include "UnityPrefix.h"
#include "ScriptingTypes.h"

#if ENABLE_SCRIPTING

#include "ScriptingArguments.h"
#include "Runtime/Scripting/ScriptingUtility.h"

ScriptingArguments::ScriptingArguments() 
	: m_Count(0)
{
#if UNITY_WINRT
	m_StringArguments = ref new Platform::Array<Platform::String^>(MAXARGS);
#else
	memset(m_Arguments, 0, sizeof(m_Arguments));
#endif
	memset(&m_PrimitiveStorage, 0, sizeof(m_PrimitiveStorage));
	memset(m_ArgumentTypes, 0, sizeof(m_ArgumentTypes));
}

void ScriptingArguments::AddBoolean(bool value)
{
	m_PrimitiveStorage.ints[m_Count] = value ? 1 : 0;
#if UNITY_WINRT
	m_Arguments[m_Count] = m_PrimitiveStorage.ints[m_Count];
#else
	m_Arguments[m_Count] = &m_PrimitiveStorage.ints[m_Count];
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_BOOLEAN;
	m_Count++;
}

void ScriptingArguments::AddInt(int value)
{
	m_PrimitiveStorage.ints[m_Count] = value;
#if UNITY_WINRT
	m_Arguments[m_Count] = *(long long*)&value;
#else
	m_Arguments[m_Count] = &m_PrimitiveStorage.ints[m_Count];
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_INT;
	m_Count++;
}

void ScriptingArguments::AddFloat(float value)
{
	m_PrimitiveStorage.floats[m_Count] = value;
#if UNITY_WINRT
	m_Arguments[m_Count] = *(long long*)&value;
#else
	m_Arguments[m_Count] = &m_PrimitiveStorage.floats[m_Count];
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_FLOAT;
	m_Count++;
}

void ScriptingArguments::AddString(const char* str)
{
#if ENABLE_MONO
	m_Arguments[m_Count] = MonoStringNew(str);
#elif UNITY_FLASH
	m_Arguments[m_Count] = str;
#elif UNITY_WINRT
	m_StringArguments[m_Count] = ConvertUtf8ToString(str);
	m_Arguments[m_Count] = (long long)m_StringArguments[m_Count]->Data();
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_STRING;
	m_Count++;
}

void ScriptingArguments::AddString(std::string& str)
{
	AddString(str.c_str());
}

void ScriptingArguments::AddObject(ScriptingObjectPtr scriptingObject)
{
#if UNITY_WINRT
	m_Arguments[m_Count] = scriptingObject.GetHandle();
#else
	m_Arguments[m_Count] = scriptingObject;
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_OBJECT;
	m_Count++;
}

void ScriptingArguments::AddStruct(void* pointerToStruct)
{
#if UNITY_WINRT
	// We need to pass struct size, and ScriptingType here, to set the struct for metro
	FatalErrorMsg("ToDo");
#else
	m_Arguments[m_Count] = pointerToStruct;
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_STRUCT;
	m_Count++;
}

void ScriptingArguments::AddArray(ScriptingArrayPtr arr)
{
#if UNITY_WINRT
	m_Arguments[m_Count] = arr.GetHandle();
#elif UNITY_FLASH
	FatalErrorMsg("ToDo");
#else
	m_Arguments[m_Count] = arr;
#endif
	m_ArgumentTypes[m_Count] = ARGTYPE_ARRAY;
	m_Count++;
}

void ScriptingArguments::AddEnum(int value)
{
	AddInt(value);
	m_ArgumentTypes[m_Count-1] = ARGTYPE_ENUM;
}

bool ScriptingArguments::GetBooleanAt(int index)
{
	return m_PrimitiveStorage.ints[index] == 1;
}

int ScriptingArguments::GetIntAt(int index)
{
	return m_PrimitiveStorage.ints[index];
}

float ScriptingArguments::GetFloatAt(int index)
{
	return m_PrimitiveStorage.floats[index];
}

const void* ScriptingArguments::GetStringAt(int index)
{
#if UNITY_WINRT
	return m_StringArguments[index]->Data();
#else
	return m_Arguments[index];
#endif
}
	
ScriptingObjectPtr ScriptingArguments::GetObjectAt(int index)
{
#if UNITY_WINRT
	return ScriptingObjectPtr(safe_cast<long long>(m_Arguments[index]));
#else
	return (ScriptingObjectPtr) m_Arguments[index];
#endif
}

void** ScriptingArguments::InMonoFormat()
{
	return (void**) &m_Arguments[0];
}

int ScriptingArguments::GetTypeAt(int index)
{
	return m_ArgumentTypes[index];
}

int ScriptingArguments::GetCount()
{
	return m_Count;
}

void ScriptingArguments::AdjustArgumentsToMatchMethod(ScriptingMethodPtr method)
{	
#if ENABLE_MONO
	MonoMethodSignature* sig = mono_method_signature (method->monoMethod);
	int methodCount = mono_signature_get_param_count (sig);
	if (methodCount < m_Count)
		m_Count = methodCount;
#endif
}

bool ScriptingArguments::CheckArgumentsAgainstMethod(ScriptingMethodPtr method)
{
#if !ENABLE_MONO
	return true;
#else

	MonoMethodSignature* sig = mono_method_signature (method->monoMethod);
	int argCount = mono_signature_get_param_count (sig);
	if (argCount != GetCount())
		return false;

	void* iterator = NULL;
	int argIndex = -1;
	while(true)
	{
		argIndex++;
		MonoType* methodType = mono_signature_get_params (sig, &iterator);
		if (methodType == NULL)
			return true;

		if (GetTypeAt(argIndex) != ScriptingArguments::ARGTYPE_OBJECT)
			continue;

		MonoClass* invokingArgument  = mono_object_get_class(GetObjectAt(argIndex));
		MonoClass* receivingArgument = mono_class_from_mono_type (methodType);

		if (!mono_class_is_subclass_of (invokingArgument, receivingArgument, false))
			return false;
	}
	return true;
#endif
}


#if UNITY_WINRT
ScriptingParamsPtr ScriptingArguments::InMetroFormat()
{
	if (m_Count <= 0) return SCRIPTING_NULL;
	return &m_Arguments[0];
}
#endif

#endif
