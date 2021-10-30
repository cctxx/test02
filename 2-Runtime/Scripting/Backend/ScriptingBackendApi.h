#ifndef _SCRIPTINGBACKEND_API_H_
#define _SCRIPTINGBACKEND_API_H_

#include <string>

#include "ScriptingTypes.h"
class ScriptingMethodRegistry;
class ScriptingTypeRegistry;
struct ScriptingArguments;

struct StackTraceInfo
{
	std::string condition;
	std::string strippedStacktrace;
	std::string stacktrace;
	int errorNum;
	std::string file;
	int line;
};

bool				scripting_method_is_instance(ScriptingMethodPtr method);
const char*			scripting_method_get_name(ScriptingMethodPtr method);
int					scripting_method_get_argument_count(ScriptingMethodPtr method, ScriptingTypeRegistry& typeRegistry);
ScriptingTypePtr 	scripting_method_get_returntype(ScriptingMethodPtr method, ScriptingTypeRegistry& typeRegistry);
bool				scripting_method_has_attribute(ScriptingMethodPtr method, ScriptingClassPtr attribute);
ScriptingTypePtr	scripting_method_get_nth_argumenttype(ScriptingMethodPtr method, int index, ScriptingTypeRegistry& typeRegistry);
ScriptingObjectPtr	scripting_method_invoke(ScriptingMethodPtr method, ScriptingObjectPtr object, ScriptingArguments& arguments, ScriptingExceptionPtr* exception);

ScriptingTypePtr	scripting_class_get_parent(ScriptingTypePtr t, ScriptingTypeRegistry& typeRegistry);
void				scripting_class_get_methods(ScriptingTypePtr t, ScriptingMethodRegistry& registry, std::vector<ScriptingMethodPtr>& result);
const char*			scripting_class_get_name(ScriptingTypePtr t);
const char*			scripting_class_get_namespace(ScriptingTypePtr t);
bool				scripting_class_is_subclass_of(ScriptingTypePtr t1, ScriptingTypePtr t2);
bool				scripting_class_is_enum(ScriptingTypePtr t);
ScriptingObjectPtr	scripting_object_new(ScriptingTypePtr t);
void				scripting_object_invoke_default_constructor(ScriptingObjectPtr o, ScriptingExceptionPtr* exc);
ScriptingTypePtr	scripting_object_get_class(ScriptingObjectPtr t, ScriptingTypeRegistry& typeRegistry);
ScriptingMethodPtr	scripting_object_get_virtual_method(ScriptingObjectPtr o, ScriptingMethodPtr method, ScriptingMethodRegistry& methodRegistry);

ScriptingTypePtr	scripting_class_from_systemtypeinstance(ScriptingObjectPtr systemTypeInstance, ScriptingTypeRegistry& typeRegistry);

EXPORT_COREMODULE int scripting_gchandle_new(ScriptingObjectPtr o);
EXPORT_COREMODULE int scripting_gchandle_weak_new(ScriptingObjectPtr o);
EXPORT_COREMODULE void scripting_gchandle_free(int handle);
EXPORT_COREMODULE ScriptingObjectPtr scripting_gchandle_get_target(int handle);

int					scripting_gc_maxgeneration();
void				scripting_gc_collect(int maxGeneration);

ScriptingObjectPtr  scripting_class_get_object(ScriptingClassPtr klass);
ScriptingArrayPtr   scripting_cast_object_to_array(ScriptingObjectPtr o);

ScriptingStringPtr	scripting_string_new(const char* str);
ScriptingStringPtr	scripting_string_new(const std::string& str);
ScriptingStringPtr	scripting_string_new(const wchar_t* str);
ScriptingStringPtr	scripting_string_new(const char* str, unsigned int length);

void 				scripting_stack_trace_info_for(ScriptingExceptionPtr exception, StackTraceInfo& info);
EXPORT_COREMODULE std::string 		scripting_cpp_string_for(ScriptingStringPtr ptr);

void*				scripting_array_element_ptr(ScriptingArrayPtr array, int i, size_t element_size);

// TODO: temporary, these includes are going to disappear very soon

#if UNITY_FLASH
#include "Runtime/Scripting/Backend/Flash/ScriptingBackendApi_Flash.h"
#elif UNITY_WINRT
#elif ENABLE_MONO
#include "Runtime/Scripting/Backend/Mono/ScriptingBackendApi_Mono.h"
#endif

#endif
