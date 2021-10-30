#include "UnityPrefix.h"

#include "Scripting.h"
#include "ScriptingUtility.h"
#include "Runtime/BaseClasses/BaseObject.h"

namespace Scripting
{

void RaiseAS3NativeException(const char* type, const char* format, va_list list)
{
	va_list ap = list;
	va_copy (ap, list);
	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, ap);
	string typeString;
	typeString += type;
	typeString += ":";
	typeString += buffer;
	//ErrorString(typeString);
	Ext_Flash_ThrowError(typeString.c_str());
	va_end (ap);
}

void RaiseArgumentException(const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseAS3NativeException("RaiseArgumentException", format, va);
}

void RaiseSecurityException(const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseAS3NativeException("RaiseSecurityException", format, va);
}

void RaiseNullException(const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseAS3NativeException("NullReferenceException", format, va);
}

void RaiseMonoException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseAS3NativeException("RaiseMonoException", format, va);
}

void RaiseNullExceptionObject(ScriptingObjectPtr object)
{
	ErrorString("RaiseNullExceptionObject not implemented");
	Ext_Flash_ThrowError("RaiseNullExceptionObject");
}

void RaiseOutOfRangeException(const char* format, ...)
{
	ErrorString("RaiseOutOfRangeException not implemented");
	Ext_Flash_ThrowError("RaiseOutOfRangeException");
}


void* GetCachedPtrFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return NULL;
	
	void* result;
	__asm __volatile__("%0 = (marshallmap.getObjectWithId(%1) as UnityEngine._Object).m_CachedPtr;" : "=r"(result) : "r"(object));
	return result;
}

void SetCachedPtrOnScriptingWrapper(ScriptingObjectPtr object, void* cachedPtr)
{
	__asm __volatile__("(marshallmap.getObjectWithId(%0) as UnityEngine._Object).m_CachedPtr = %1;" : : "r"(object), "r"(cachedPtr));
}

int GetInstanceIDFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return 0;

	int result;
	__asm __volatile__("%0 = (marshallmap.getObjectWithId(%1) as UnityEngine._Object).m_InstanceID;" : "=r"(result) : "r"(object));
	return result;
}

void SetInstanceIDOnScriptingWrapper(ScriptingObjectPtr object, int instanceID)
{
	__asm __volatile__("(marshallmap.getObjectWithId(%0) as UnityEngine._Object).m_InstanceID = %1;" : : "r"(object), "r"(instanceID));
}

void SetErrorOnScriptingWrapper(ScriptingObjectPtr object, ScriptingStringPtr error)
{

}

ScriptingObjectPtr InstantiateScriptingWrapperForClassID(int classID)
{
	const string& name = Object::ClassIDToString(classID);
	const char* cname = name.c_str();

	ScriptingObjectPtr result = Ext_Scripting_InstantiateScriptingWrapperForClassWithName(cname);
	if(result != NULL)	
		return result;

	int superClassID = Object::GetSuperClassID(classID);
	if(superClassID != ClassID(Object))
		return InstantiateScriptingWrapperForClassID(superClassID);

	return NULL;
}

ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass)
{ 
	return SCRIPTING_NULL; 
}

void RaiseIfNull(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		RaiseNullException("(null)");
}

void RaiseIfNull(void* object)
{
	if(object == NULL)
		RaiseNullException("(null)");
}

void SetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i, ScriptingObjectPtr value)
{
	void* raw = scripting_array_element_ptr(a, i, sizeof(ScriptingObjectPtr));
	*(ScriptingObjectPtr*)raw = value;
}

void SetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i, ScriptingStringPtr value)
{
	void* raw = scripting_array_element_ptr(a, i, sizeof(ScriptingStringPtr));
	*(ScriptingStringPtr*)raw = value;
}

ScriptingStringPtr* GetScriptingArrayStringElementImpl(ScriptingArrayPtr a, int i)
{
	return (ScriptingStringPtr*)scripting_array_element_ptr(a, i, sizeof(ScriptingStringPtr));
}

ScriptingObjectPtr* GetScriptingArrayObjectElementImpl(ScriptingArrayPtr a, int i)
{
	return (ScriptingObjectPtr*)scripting_array_element_ptr(a, i, sizeof(ScriptingObjectPtr));
}

ScriptingStringPtr* GetScriptingArrayStringStartImpl(ScriptingArrayPtr a)
{
	return (ScriptingStringPtr*)scripting_array_element_ptr(a, 0, sizeof(ScriptingStringPtr));
}

ScriptingObjectPtr* GetScriptingArrayObjectStartImpl(ScriptingArrayPtr a)
{
	return (ScriptingObjectPtr*)scripting_array_element_ptr(a, 0, sizeof(ScriptingObjectPtr));
}

ScriptingStringPtr GetScriptingArrayStringElementNoRefImpl(ScriptingArrayPtr a, int i)
{
	return *((ScriptingStringPtr*)scripting_array_element_ptr(a, i, sizeof(ScriptingStringPtr)));
}

ScriptingObjectPtr GetScriptingArrayObjectElementNoRefImpl(ScriptingArrayPtr a, int i)
{
	return *((ScriptingObjectPtr*)scripting_array_element_ptr(a, i, sizeof(ScriptingObjectPtr)));
}

}//namespace Scripting