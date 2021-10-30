#include "UnityPrefix.h"

#if ENABLE_SCRIPTING

#include "ScriptingInvocation.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "ScriptingArguments.h"
#include "ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Scripting/ScriptingManager.h"

#if ENABLE_MONO
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoUtility.h"
#include "Runtime/Mono/MonoScript.h"
#endif

ScriptingInvocation::ScriptingInvocation()
{
}

ScriptingInvocation::ScriptingInvocation(ScriptingMethodPtr in_method)
	: ScriptingInvocationNoArgs(in_method)
{
}

#if ENABLE_MONO || UNITY_WINRT
ScriptingInvocation::ScriptingInvocation(const char* namespaze, const char* klassName, const char* methodName)
{
	method = GetScriptingMethodRegistry().GetMethod(namespaze, klassName, methodName);
}

ScriptingInvocation::ScriptingInvocation(ScriptingClassPtr klass, const char* methodName)
{
	method = GetScriptingMethodRegistry().GetMethod(klass, methodName);
}

ScriptingInvocation::ScriptingInvocation(BackendNativeMethod monoMethod)
{
	method = GetScriptingMethodRegistry().GetMethod(monoMethod);
}
#endif

bool ScriptingInvocation::Check()
{
#if !ENABLE_MONO
	return true;
#else
	return ScriptingInvocationNoArgs::Check() && arguments.CheckArgumentsAgainstMethod(method);
#endif
}

template<class T>
T ScriptingInvocation::Invoke()
{
	ScriptingExceptionPtr ex = NULL;
	return Invoke<T>(&ex);
}

template<>
ScriptingObjectPtr ScriptingInvocation::Invoke<ScriptingObjectPtr>()
{
	return Invoke();
}

template<>
bool ScriptingInvocation::Invoke<bool>(ScriptingExceptionPtr* exception)
{
	ScriptingObjectPtr o = Invoke(exception);
	if (*exception != NULL)
		return false;
	
	#if ENABLE_MONO
		if (method->fastMonoMethod)
			return (bool)o;
		else
			return ExtractMonoObjectData<char>(o);
	#elif UNITY_FLASH
		bool boolResult;
		__asm __volatile__("%0 = marshallmap.getObjectWithId(%1);" : "=r"(boolResult) : "r"(o));
		return boolResult;
	#elif UNITY_WINRT
		return o != SCRIPTING_NULL ? o.ToBool() : false;
	#endif
}

ScriptingObjectPtr ScriptingInvocation::Invoke()
{
	ScriptingExceptionPtr ex = NULL;
	return Invoke(&ex);
}

ScriptingObjectPtr ScriptingInvocation::Invoke(ScriptingExceptionPtr* exception)
{
	return Invoke(exception, false);
}

ScriptingObjectPtr ScriptingInvocation::Invoke(ScriptingExceptionPtr* exception, bool convertArguments)
{
	ScriptingObjectPtr returnValue;
	
	*exception = NULL;

#if ENABLE_MONO || UNITY_FLASH || UNITY_WINRT
	MONO_PROFILER_BEGIN (method, classContextForProfiler, object)
#if UNITY_WINRT
	ScriptingObjectPtr metro_invoke_method(ScriptingMethodPtr method, ScriptingObjectPtr object, ScriptingArguments* arguments, ScriptingExceptionPtr* exception, bool convertArgs);
	returnValue = metro_invoke_method(method, object, &arguments, exception, convertArguments);
#else
	returnValue = scripting_method_invoke(method, object, arguments, exception);
#endif
	MONO_PROFILER_END
#elif !UNITY_EXTERNAL_TOOL
	ErrorString("Invoke() not implemented on this platform");
#else	
	return NULL;
#endif

	if (! *exception) return returnValue;

	this->exception = *exception;
#if !UNITY_EXTERNAL_TOOL
	if (logException)
		Scripting::LogException(*exception, objectInstanceIDContextForException );
#endif

	return SCRIPTING_NULL;
}

void ScriptingInvocation::AdjustArgumentsToMatchMethod()
{
	arguments.AdjustArgumentsToMatchMethod(method);
}


#if ENABLE_MONO

MonoObject* CallStaticMonoMethod (MonoClass* klass , const char* methodName, void** parameters)
{
	MonoException* exception = NULL;
	return CallStaticMonoMethod(klass, methodName, parameters, &exception);
}


static MonoObject* CallStaticMonoMethod (MonoMethod* method, void** parameters, MonoException** exception)
{
	MonoObject* returnValue = mono_runtime_invoke_profiled (method, NULL, parameters, exception);
	if (! *exception) return returnValue;

	Scripting::LogException(*exception, 0);
	return NULL;
}


static MonoObject* CallStaticMonoMethod (MonoMethod* method, void** parameters)
{
	MonoException* exception = NULL;
	return CallStaticMonoMethod(method, parameters, &exception);
}

MonoObject* CallStaticMonoMethod (const char* className, const char* methodName, void** parameters)
{
	MonoException* exception = NULL;
	return CallStaticMonoMethod(className, methodName, parameters, &exception);
}

MonoObject* CallStaticMonoMethod (MonoClass* klass , const char* methodName, void** parameters, MonoException** exception)
{
	MonoMethod* method = mono_class_get_method_from_name (klass, methodName, -1);

	if (!method)
	{
		ErrorString (Format ("Couldn't call method %s in class %s because it wasn't found.", methodName, mono_class_get_name(klass)));
		return NULL;
	}

	return CallStaticMonoMethod(method, parameters, exception);
}

MonoObject* CallStaticMonoMethod (const char* className, const char* methodName, void** parameters, MonoException** exception)
{
	MonoMethod* m = FindStaticMonoMethod(className, methodName);
	if (!m)
	{
		ErrorString (Format ("Couldn't call method %s because the class %s couldn't be found.", methodName, className));
		return NULL;
	}

	return CallStaticMonoMethod(m, parameters, exception);
}

MonoObject* CallStaticMonoMethod (const char* className, const char* nameSpace, const char* methodName, void** parameters)
{
	MonoException* exception = NULL;
	MonoMethod* m = FindStaticMonoMethod(nameSpace, className, methodName);
	if (!m)
	{
		ErrorString (Format ("Couldn't call method %s because the class %s couldn't be found.", methodName, className));
		return NULL;
	}

	return CallStaticMonoMethod(m, parameters, &exception);
}

MonoObject* CallStaticMonoMethod (const char* className, const char* nameSpace, const char* methodName, void** parameters, MonoException** exception)
{
	MonoMethod* m = FindStaticMonoMethod(nameSpace, className, methodName);
	if (!m)
	{
		ErrorString (Format ("Couldn't call method %s because the class %s couldn't be found.", methodName, className));
		return NULL;
	}

	return CallStaticMonoMethod(m, parameters, exception);
}


#endif

#endif
