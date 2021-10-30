#include "UnityPrefix.h"

#if ENABLE_SCRIPTING

#include "ScriptingInvocationNoArgs.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Profiler/Profiler.h"

#if ENABLE_MONO
#include "Runtime/Mono/MonoIncludes.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoUtility.h"
#include "Runtime/Mono/MonoScript.h"
#endif

ScriptingInvocationNoArgs::ScriptingInvocationNoArgs()
{
	SetDefaults();
}

ScriptingInvocationNoArgs::ScriptingInvocationNoArgs(ScriptingMethodPtr in_method)
{
	SetDefaults();
	method = in_method;
}

void ScriptingInvocationNoArgs::SetDefaults()
{
	object = SCRIPTING_NULL;
	method = SCRIPTING_NULL;
	classContextForProfiler = NULL;
	logException = true;
	objectInstanceIDContextForException = 0;
	exception = SCRIPTING_NULL;
}

bool ScriptingInvocationNoArgs::Check()
{
#if !ENABLE_MONO
	return true;
#else

	// Check method
	if (method == NULL)
	{
		ErrorString("Failed to call function because it was null");
		return false;
	}

	bool methodIsInstance = mono_signature_is_instance (mono_method_signature(method->monoMethod));
	bool invokingInstance = object != NULL;

	if (methodIsInstance && !invokingInstance)
	{
		DebugStringToFile (Format("Failed to call instance function %s because the no object was provided", mono_method_get_name (method->monoMethod)), 0, __FILE_STRIPPED__, __LINE__, 
			kError, objectInstanceIDContextForException);
		return false;
	}
	
	if (!methodIsInstance && invokingInstance)
	{
		DebugStringToFile (Format("Failed to call static function %s because an object was provided", mono_method_get_name (method->monoMethod)), 0, __FILE_STRIPPED__, __LINE__, 
			kError, objectInstanceIDContextForException);
		return false;
	}

	return true;
	
#endif
}


ScriptingObjectPtr ScriptingInvocationNoArgs::Invoke()
{
	ScriptingExceptionPtr ex = NULL;
	return Invoke(&ex);
}

ScriptingObjectPtr ScriptingInvocationNoArgs::Invoke(ScriptingExceptionPtr* exception)
{
	ScriptingObjectPtr returnValue;
	
	*exception = NULL;

#if ENABLE_MONO || UNITY_FLASH || UNITY_WINRT
	ScriptingArguments arguments;
	MONO_PROFILER_BEGIN (method, classContextForProfiler, object)
#if UNITY_WINRT
	ScriptingObjectPtr metro_invoke_method(ScriptingMethodPtr method, ScriptingObjectPtr object, ScriptingArguments* arguments, ScriptingExceptionPtr* exception, bool convertArgs);
	returnValue = metro_invoke_method(method, object, NULL, exception, false);
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
		Scripting::LogException(*exception, objectInstanceIDContextForException);
#endif

	return SCRIPTING_NULL;
}

ScriptingObjectPtr ScriptingInvocationNoArgs::InvokeChecked()
{
	if (!Check())
		return SCRIPTING_NULL;

	return Invoke();
}

#endif
