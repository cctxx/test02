#include "UnityPrefix.h"

#include "../ScriptingTypes.h"
#include "ScriptingBackendApi_Flash.h"
#include "../ScriptingTypeRegistry.h"
#include "../ScriptingMethodRegistry.h"
#include <string>
#include "../ScriptingArguments.h"
#include "PlatformDependent\FlashSupport\cpp\FlashUtils.h"

std::string scripting_cpp_string_for(ScriptingStringPtr str)
{
	return std::string((const char*)str);
}

//implemented in raw actionscript
extern "C" ScriptingClass* Ext_Flash_GetBaseClassFromScriptingClass(ScriptingClass* klass);
bool scripting_method_is_instance(ScriptingMethodPtr method)
{
	//not implemented on flash yet.
	return true;
}

ScriptingStringPtr scripting_string_new(const char* str)
{	
	__asm __volatile__("returnString = strFromPtr(%0);" : : "r" (str) );
	return (ScriptingStringPtr)str;
}

ScriptingStringPtr scripting_string_new(const std::string& str)
{
	return scripting_string_new(str.c_str());
}

extern "C" int Ext_Flash_ScriptingGetMethodParamCount(ScriptingObjectPtr object, AS3String as3String);
int scripting_method_get_argument_count(ScriptingMethodPtr method, ScriptingTypeRegistry& typeRegistry)
{
	ScriptingObjectPtr methodInfo = method->GetSystemReflectionMethodInfo();
	if (methodInfo==NULL)
		return 0;

	int result;
	FLASH_ASM_WITH_NEWSP("%0 = ScriptingMethodHelper.NumberOfArgumentsOf(marshallmap.getObjectWithId(%1));" : "=r"(result) : "r"(methodInfo));

	return result;
}

ScriptingTypePtr scripting_method_get_nth_argumenttype(ScriptingMethodPtr method, int index, ScriptingTypeRegistry& typeRegistry)
{
	ScriptingObjectPtr methodInfo = method->GetSystemReflectionMethodInfo();
	if (methodInfo==NULL)
		return NULL;

	ScriptingTypePtr result;
	FLASH_ASM_WITH_NEWSP("scratch_object = ScriptingMethodHelper.NthArgumentType(marshallmap.getObjectWithId(%0), %1)" : : "r"(methodInfo), "r"(index));
	FLASH_ASM_WITH_NEWSP("if (scratch_object!=null) %0 = marshallmap.getIdForObject(scratch_object.GetClass()); else %0 = 0;" : "=r"(result));
	return result;
}

const char* scripting_method_get_name(ScriptingMethodPtr method)
{
	return method->GetName();
}

bool scripting_method_has_attribute (ScriptingMethodPtr method, ScriptingClassPtr attribute)
{
	//not implemented on flash yet
	return false;
}

ScriptingTypePtr scripting_class_get_parent(ScriptingTypePtr type, ScriptingTypeRegistry& typeRegistry)
{
	return Ext_Flash_GetBaseClassFromScriptingClass(type);
}

bool scripting_class_is_enum(ScriptingTypePtr type)
{
	//todo: implement
	return false;
}

extern "C" const char* Ext_Flash_GetNameFromClass(ScriptingClassPtr klass);
const char* scripting_class_get_name(ScriptingClassPtr klass)
{
	//Horrible, we can only clean this up once we create a struct ScriptingClass for flash.
	std::string* s = new std::string(Ext_Flash_GetNameFromClass(klass));
	return s->c_str();
}

extern "C" const char* Ext_Flash_GetNameSpaceFromScriptingType(ScriptingType* scriptingType);
const char* scripting_class_get_namespace(ScriptingClassPtr klass)
{
	//Horrible, we can only clean this up once we create a struct ScriptingClass for flash.
	std::string* s = new std::string(Ext_Flash_GetNameSpaceFromScriptingType(klass));
	return s->c_str();
}

extern "C" bool Ext_Flash_ScriptingClassIsSubclassOf(ScriptingTypePtr t1, ScriptingTypePtr t2);
bool scripting_class_is_subclass_of(ScriptingTypePtr t1, ScriptingTypePtr t2)
{
	return Ext_Flash_ScriptingClassIsSubclassOf(t1,t2);
}

ScriptingTypePtr scripting_method_get_returntype(ScriptingMethodPtr method, ScriptingTypeRegistry& typeRegistry)
{
	return method->GetReturnType();
}

extern "C" ScriptingObjectPtr Ext_Flash_InvokeMethodOnObject(void* object, AS3String as3String, ScriptingException** exception);
ScriptingObjectPtr scripting_method_invoke(ScriptingMethodPtr method, ScriptingObjectPtr object, ScriptingArguments& arguments, ScriptingExceptionPtr* exception)
{
	void* objectToInvokeOn = object;
	*exception = NULL;
	if (object==NULL)
	{
		objectToInvokeOn = method->m_Class;
		if (objectToInvokeOn == NULL)
			ErrorString("flash_invoke_method called with NULL object, and scriptmethod->m_Class is NULL too.");
	}

	__asm __volatile__("invocation_arguments.length = 0;\n");

	for (int i=0; i!=arguments.GetCount(); i++)
	{
		switch (arguments.GetTypeAt(i))
		{
		case ScriptingArguments::ARGTYPE_BOOLEAN:
			__asm __volatile__("invocation_arguments.push(%0 ? true : false);" : : "r"(arguments.GetBooleanAt(i)));
			break;
		case ScriptingArguments::ARGTYPE_INT:
			__asm __volatile__("invocation_arguments.push(%0);" : : "r"(arguments.GetIntAt(i)));
			break;
		case ScriptingArguments::ARGTYPE_FLOAT:
			__asm __volatile__("invocation_arguments.push(%0);" : : "f"(arguments.GetFloatAt(i)));
			break;
		case ScriptingArguments::ARGTYPE_STRING:
			__asm __volatile__("invocation_arguments.push(strFromPtr(%0));" : : "r"(arguments.GetStringAt(i)));
			break;
		case ScriptingArguments::ARGTYPE_OBJECT:
			__asm __volatile__("invocation_arguments.push(marshallmap.getObjectWithId(%0));" : : "r"(arguments.GetObjectAt(i)));
			break;
		default:
			ErrorString(Format("Flash does not support calling managed methods with this type of argument: %d",arguments.GetTypeAt(i)));
			break;
		}
	}
	return Ext_Flash_InvokeMethodOnObject(objectToInvokeOn, method->m_As3String,exception);
}

ScriptingTypePtr scripting_class_from_systemtypeinstance(ScriptingObjectPtr systemTypeInstance, ScriptingTypeRegistry& typeRegistry)
{
	//todo: think about if this is actually correct.
	return (ScriptingClassPtr)systemTypeInstance;
}

extern "C" ScriptingObjectPtr Ext_Flash_CreateInstance(ScriptingClass* klass);
ScriptingObjectPtr scripting_object_new(ScriptingTypePtr t)
{
	return Ext_Flash_CreateInstance(t);
}

extern "C" ScriptingClassPtr Ext_Flash_GetScriptingTypeOfScriptingObject(ScriptingObjectPtr);
ScriptingTypePtr scripting_object_get_class(ScriptingObjectPtr o, ScriptingTypeRegistry& typeRegistry)
{
	return Ext_Flash_GetScriptingTypeOfScriptingObject(o);
}

ScriptingMethodPtr scripting_object_get_virtual_method(ScriptingObjectPtr o, ScriptingMethodPtr method, ScriptingMethodRegistry& methodRegistry)
{
	return method;
}

void scripting_object_invoke_default_constructor(ScriptingObjectPtr o, ScriptingExceptionPtr* exc)
{
	*exc = NULL;
	//todo: properly deal with exception.
	FLASH_ASM_WITH_NEWSP("marshallmap.getObjectWithId(%0).cil2as_DefaultConstructor()" : : "r"(o));
}

int scripting_gchandle_new(ScriptingObjectPtr o)
{
	__asm __volatile__("marshallmap.gcHandle(%0);" : : "r" (o));
	return (int)o;
}

int	scripting_gchandle_weak_new(ScriptingObjectPtr o)
{
	FatalErrorMsg("ToDo");
	return 0;
}

void scripting_gchandle_free(int handle)
{
	__asm __volatile__("marshallmap.gcFree(%0);" : : "r" (handle));
}

ScriptingObjectPtr scripting_gchandle_get_target(int handle)
{
	return (ScriptingObjectPtr) handle;
}

int	scripting_gc_maxgeneration()
{
	return 0;
}

void scripting_gc_collect(int maxGeneration)
{
	FatalErrorMsg("ToDo");
}

void ScriptingMethod::Init(const char* name, const char* mappedName, const char* sig, ScriptingClass* klass)
{
	m_Name = strcpy(new char[strlen(name) + 1],name);
	m_Mappedname = strcpy(new char[strlen(mappedName) + 1],mappedName);
	__asm __volatile__("%0 = getAS3StringForPtr(%1);\n" : "=r" (m_As3String) : "r" (m_Mappedname));

	m_Signature = strcpy(new char[strlen(sig) + 1],sig);
	m_Class = klass;
	m_MethodInfo = NULL;
	FLASH_ASM_WITH_NEWSP("%0 = marshallmap.getIdForObject(System.Type.ForClass(marshallmap.getObjectWithId(%1)));\n" : "=r" (m_SystemType) : "r"(m_Class) );
	scripting_gchandle_new(m_SystemType);
}

extern "C" const char* Ext_GetMappedMethodName(const char* name, ScriptingType* klass);
ScriptingMethod::ScriptingMethod(const char* name, ScriptingClass* klass)
{
	const char* mappedName = Ext_GetMappedMethodName(name,klass);
	Init(name,mappedName,"",klass);
}

ScriptingMethod::ScriptingMethod(const char* name, const char* mappedName, const char* sig, ScriptingClass* klass)
{
	Init(name,mappedName,sig,klass);
}

extern "C" ScriptingClass* Ext_Flash_GetMethodReturnType(const char* methodname, ScriptingClass* klass);
ScriptingClass* ScriptingMethod::GetReturnType()
{
	return Ext_Flash_GetMethodReturnType(m_Mappedname, m_Class);
}

ScriptingObjectPtr ScriptingMethod::GetSystemReflectionMethodInfo()
{
	if (m_MethodInfo)
		return m_MethodInfo;

	FLASH_ASM_WITH_NEWSP("%0 = marshallmap.getIdForObject(ScriptingMethodHelper.GetMethodInfo(marshallmap.getObjectWithId(%1), strFromPtr(%2)));" : "=r"(m_MethodInfo) : "r"(m_SystemType),"r"(m_Name) );

	scripting_gchandle_new(m_MethodInfo);
	return m_MethodInfo;
}

ScriptingObjectPtr scripting_class_get_object(ScriptingClassPtr klass)
{
	FatalErrorMsg("ToDo");
	return SCRIPTING_NULL;
}

ScriptingArrayPtr scripting_cast_object_to_array(ScriptingObjectPtr o)
{
	FatalErrorMsg("ToDo");
	return SCRIPTING_NULL;
}

void scripting_stack_trace_info_for(ScriptingExceptionPtr exception, StackTraceInfo& info)
{
	AssertIf (exception == NULL);
	
	__asm __volatile__("var errorStr:String = marshallmap.getObjectWithId(%0) as String;"::"r"(exception));
	int length;
	__asm __volatile__("%0=getStringMarshallingLength(errorStr);":"=r"(length));
	char* errorStrPtr = (char*)alloca(length);
	__asm __volatile__("var ptr:int = placeStringAtPtr(errorStr,%0);"::"r"(errorStrPtr));

	info.condition = errorStrPtr;
	info.strippedStacktrace = "";
	info.stacktrace = "";
	info.errorNum = 0;
	info.file = "";
	info.line = 0;
}

void* scripting_array_element_ptr(ScriptingArrayPtr array, int i, size_t element_size)
{
	return (UInt8*)array + sizeof(int) + i * element_size;
}