#include "UnityPrefix.h"

#include "../ScriptingTypes.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "ScriptingBackendApi_Mono.h"
#include "../ScriptingMethodRegistry.h"
#include "../ScriptingTypeRegistry.h"
#include "Runtime/Scripting/ScriptingUtility.h" //required for ExtractMonoobjectData, todo: see if we can remove that.

std::string scripting_cpp_string_for(ScriptingStringPtr str)
{
	return MonoStringToCpp(str);
}

bool scripting_method_is_instance(ScriptingMethodPtr method)
{
	return method->isInstance;
}

const char* scripting_method_get_name(ScriptingMethodPtr method)
{
	return mono_method_get_name(method->monoMethod);
}

int scripting_method_get_argument_count(ScriptingMethodPtr method, ScriptingTypeRegistry& typeRegistry)
{
	MonoMethodSignature* sig = mono_method_signature(method->monoMethod);
	Assert(sig);
	return mono_signature_get_param_count(sig);
}

ScriptingTypePtr scripting_method_get_returntype(ScriptingMethodPtr method, ScriptingTypeRegistry& registry)
{
	MonoMethodSignature* sig = mono_method_signature (method->monoMethod);
	MonoType* returnType = mono_signature_get_return_type (sig);
	if (returnType == NULL)
		return NULL;

	return mono_class_from_mono_type (returnType);
}

ScriptingTypePtr scripting_method_get_nth_argumenttype(ScriptingMethodPtr method, int index, ScriptingTypeRegistry& typeRegistry)
{
	MonoMethodSignature* sig = mono_method_signature (method->monoMethod);
	void* iterator = NULL;
	MonoType* type = mono_signature_get_params (sig, &iterator);
	if (type == NULL)
		return NULL;
	MonoClass* methodClass = mono_class_from_mono_type (type);
	return typeRegistry.GetType(methodClass);
}

bool scripting_method_has_attribute(ScriptingMethodPtr method, ScriptingClassPtr attribute)
{
    bool hasAttribute = false;
    MonoCustomAttrInfo* attrInfo = mono_custom_attrs_from_method (method->monoMethod);
    if (attrInfo != NULL && mono_custom_attrs_has_attr (attrInfo, attribute))
        hasAttribute = true;

    if (attrInfo)
        mono_custom_attrs_free(attrInfo);

    return hasAttribute;
}

ScriptingTypePtr scripting_class_get_parent(ScriptingTypePtr t, ScriptingTypeRegistry& registry)
{
	return mono_class_get_parent(t);
}

void scripting_class_get_methods(ScriptingTypePtr t, ScriptingMethodRegistry& registry, std::vector<ScriptingMethodPtr>& result)
{
	void* iterator = NULL;
	ScriptingMethodPtr scriptingMethod = NULL;
	while (MonoMethod* method = mono_class_get_methods(t, &iterator))
		if ((scriptingMethod = registry.GetMethod (method)))
			result.push_back(scriptingMethod);
}

ScriptingTypePtr scripting_class_from_systemtypeinstance(ScriptingObjectPtr systemTypeInstance, ScriptingTypeRegistry& typeRegistry)
{
	if (!systemTypeInstance)
		return NULL;

	MonoClass* klass = mono_class_from_mono_type(ExtractMonoObjectData<MonoType*>(systemTypeInstance));
	return typeRegistry.GetType(klass);
}

const char* scripting_class_get_name(ScriptingClassPtr klass)
{
	return mono_class_get_name(klass);
}

const char* scripting_class_get_namespace(ScriptingClassPtr klass)
{
	return mono_class_get_namespace(klass);
}

bool scripting_class_is_subclass_of(ScriptingClassPtr c1, ScriptingClassPtr c2)
{
	return mono_class_is_subclass_of(c1,c2,true);
}

bool scripting_class_is_enum(ScriptingClassPtr klass)
{
	return mono_class_is_enum (klass);
}

ScriptingTypePtr scripting_object_get_class(ScriptingObjectPtr t, ScriptingTypeRegistry& registry)
{
	return mono_object_get_class(t);
}

void scripting_object_invoke_default_constructor(ScriptingObjectPtr t, ScriptingExceptionPtr* exc)
{
	mono_runtime_object_init_exception(t,exc);
}

ScriptingMethodPtr scripting_object_get_virtual_method(ScriptingObjectPtr o, ScriptingMethodPtr method, ScriptingMethodRegistry& methodRegistry)
{
	return methodRegistry.GetMethod(mono_object_get_virtual_method(o,method->monoMethod));
}

ScriptingObjectPtr scripting_object_new(ScriptingTypePtr t)
{
#if UNITY_EDITOR
	if (mono_unity_class_is_abstract (t)) {
		// Cannot instantiate abstract class
		return SCRIPTING_NULL;
	}
#endif
	return mono_object_new(mono_domain_get(), t);
}

int scripting_gchandle_new(ScriptingObjectPtr o)
{
	return mono_gchandle_new(o,1);
}

int	scripting_gchandle_weak_new(ScriptingObjectPtr o)
{
	return mono_gchandle_new_weakref(o, 1);
}

void scripting_gchandle_free(int handle)
{
	mono_gchandle_free(handle);
}

ScriptingObjectPtr scripting_gchandle_get_target(int handle)
{
	return mono_gchandle_get_target(handle);
}

int	scripting_gc_maxgeneration()
{
	return mono_gc_max_generation();
}

void scripting_gc_collect(int maxGeneration)
{
	mono_gc_collect(maxGeneration);
}

ScriptingObjectPtr scripting_method_invoke(ScriptingMethodPtr method, ScriptingObjectPtr object, ScriptingArguments& arguments, ScriptingExceptionPtr* exception)
{
#if UNITY_EDITOR
	bool IsStackLargeEnough ();
	if (!IsStackLargeEnough ())
	{
		*exception = mono_exception_from_name_msg (mono_get_corlib (), "System", "StackOverflowException", "");
		return NULL;
	}
#endif

	if (method->fastMonoMethod)
	{
		Assert(arguments.GetCount()==0);
		Assert(object);
		return method->fastMonoMethod(object,exception);
	}

	return mono_runtime_invoke(method->monoMethod, object, arguments.InMonoFormat(), exception);
}

ScriptingObjectPtr scripting_class_get_object(ScriptingClassPtr klass)
{
	return mono_class_get_object(klass);
}

ScriptingArrayPtr scripting_cast_object_to_array(ScriptingObjectPtr o)
{
	return (ScriptingArrayPtr)o;
}

ScriptingStringPtr scripting_string_new(const std::string& str)
{
	return scripting_string_new(str.c_str());
}

ScriptingStringPtr scripting_string_new(const UnityStr& str)
{
	return scripting_string_new(str.c_str());
}

ScriptingStringPtr scripting_string_new(const char* str)
{
	return MonoStringNew(str);
}

ScriptingStringPtr scripting_string_new(const wchar_t* str)
{
	return MonoStringNewUTF16(str);
}

ScriptingStringPtr scripting_string_new(const char* str, unsigned int length)
{
	return MonoStringNewLength(str, length);
}

void scripting_stack_trace_info_for(ScriptingExceptionPtr exception, StackTraceInfo& info)
{
	AssertIf (exception == NULL);
	
	MonoException* tempException = NULL;
	MonoString* monoStringMessage = NULL;
	MonoString* monoStringTrace = NULL;
	void* args[] = { exception, &monoStringMessage, &monoStringTrace };

	if (GetMonoManagerPtr () && GetMonoManager ().GetCommonClasses ().extractStringFromException)
	{
		// Call mono_runtime_invoke directly to avoid our stack size check in mono_runtime_invoke_profiled.
		// We *should* have enough stack here to make this call, and a stack trace would be useful for the user.
		mono_runtime_invoke (GetMonoManager ().GetCommonClasses ().extractStringFromException->monoMethod, (MonoObject*)exception, args, &tempException);
	}

	if (tempException)
	{
		char const* exceptionClassName = mono_class_get_name(mono_object_get_class((MonoObject*)tempException));		
		ErrorString ("Couldn't extract exception string from exception (another exception of class '"
						 + std::string (exceptionClassName) + "' was thrown while processing the stack trace)");
		return;
	}	

	// Log returned string
	string message;

	char* extractedMessage = NULL;
	if (monoStringMessage)
		message = extractedMessage = mono_string_to_utf8 (monoStringMessage);

	char* extractedTrace = NULL;
	if (monoStringTrace)
		extractedTrace = mono_string_to_utf8 (monoStringTrace);

	string processedStackTrace;
	int line = -1;
	string path;

	if (extractedTrace && *extractedTrace != 0)
	{
		PostprocessStacktrace(extractedTrace, processedStackTrace);
		ExceptionToLineAndPath (processedStackTrace, line, path);
	}

	info.condition = message;
	info.strippedStacktrace = processedStackTrace;
	info.stacktrace = extractedTrace;
	info.errorNum = 0;
	info.file = path;
	info.line = line;

	g_free (extractedMessage);
	g_free (extractedTrace);
}

void* scripting_array_element_ptr(ScriptingArrayPtr array, int i, size_t element_size)
{
	return kMonoArrayOffset + i * element_size + (char*)array;
}