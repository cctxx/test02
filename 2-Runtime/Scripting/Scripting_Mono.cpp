#include "UnityPrefix.h"

#include <stdarg.h>
#include "Scripting.h"
#include "Runtime/Mono/MonoManager.h"

#ifdef _MSC_VER
#define va_copy(a,z) ((void)((a)=(z)))
#endif

static UnityEngineObjectMemoryLayout* GetUnityEngineObjectMemoryLayout(ScriptingObjectPtr object) 
{
	return reinterpret_cast<UnityEngineObjectMemoryLayout*>(((char*)object) + kMonoObjectOffset);
}

namespace Scripting
{

void* GetCachedPtrFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return NULL;

	return GetUnityEngineObjectMemoryLayout(object)->cachedPtr;
}

void SetCachedPtrOnScriptingWrapper(ScriptingObjectPtr object, void* cachedPtr)
{
	GetUnityEngineObjectMemoryLayout(object)->cachedPtr = cachedPtr;
}

int GetInstanceIDFromScriptingWrapper(ScriptingObjectPtr object)
{
	if(object == SCRIPTING_NULL)
		return 0;

	return GetUnityEngineObjectMemoryLayout(object)->instanceID;
}

void SetInstanceIDOnScriptingWrapper(ScriptingObjectPtr object, int instanceID)
{
	GetUnityEngineObjectMemoryLayout(object)->instanceID = instanceID;
}

void SetErrorOnScriptingWrapper(ScriptingObjectPtr object, ScriptingStringPtr error)
{
#if MONO_QUALITY_ERRORS
	GetUnityEngineObjectMemoryLayout(object)->error = error;
#endif
}

ScriptingObjectPtr InstantiateScriptingWrapperForClassID(int classID)
{
	return (ScriptingObjectPtr)MonoInstantiateScriptingWrapperForClassID(classID);
}

ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass)
{ 
	return MonoObjectNULL(klass); 
}

ScriptingObjectPtr ScriptingObjectNULL(ScriptingClassPtr klass, ScriptingStringPtr error)
{
#if MONO_QUALITY_ERRORS
	return MonoObjectNULL(klass,error);
#else
	return MonoObjectNULL(klass);
#endif
}


DOES_NOT_RETURN void RaiseDotNetExceptionImpl (const char* ns, const char* type, const char* format, va_list list)
{
	va_list ap;
	va_copy (ap, list);
	
	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, ap);
	va_end (ap);

	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), ns, type, buffer);
	mono_raise_exception (exception);

}

DOES_NOT_RETURN void RaiseSystemExceptionImpl (const char* type, const char* format, va_list list)
{
	va_list ap;
	va_copy (ap, list);
	RaiseDotNetExceptionImpl("System",type,format,ap);
}

void RaiseManagedException (const char *ns, const char *type, const char *format, ...)
{
	va_list va;
	va_start (va, format);
	RaiseDotNetExceptionImpl (ns, type, format, va);
}

void RaiseMonoException (const char* format, ...)
{
	va_list va;
	va_start( va, format );

	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, va);

	MonoException* exception = mono_exception_from_name_msg (GetMonoManager ().GetEngineImage (), kEngineNameSpace, "UnityException", buffer);
	mono_raise_exception (exception);
}

void RaiseOutOfRangeException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("IndexOutOfRangeException", format, va);
}

void RaiseNullException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("NullReferenceException", format, va);
}

void RaiseArgumentException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("ArgumentException", format, va);
}

void RaiseInvalidOperationException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("InvalidOperationException", format, va);
}

void RaiseSecurityException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseDotNetExceptionImpl("System.Security","SecurityException", format, va);
}

void RaiseNullExceptionObject (MonoObject* object)
{
	#if MONO_QUALITY_ERRORS
	if (object)
	{
		MonoClass* klass = mono_object_get_class(object);
		if (mono_class_is_subclass_of (mono_object_get_class(object), GetMonoManager().ClassIDToScriptingClass(ClassID(Object)), false))
		{
			UnityEngineObjectMemoryLayout& data = ExtractMonoObjectData<UnityEngineObjectMemoryLayout>(object);
			string error = MonoStringToCpp(data.error);

			// The object was destroyed underneath us - but if we hit a MissingReferenceException then we show that instead!
			if (data.instanceID != 0 && error.find ("MissingReferenceException:") != 0)
			{
				error = Format("The object of type '%s' has been destroyed but you are still trying to access it.\n"
							"Your script should either check if it is null or you should not destroy the object.", mono_class_get_name(klass));

				MonoException* exception = mono_exception_from_name_msg (GetMonoManager().GetEngineImage(), "UnityEngine", "MissingReferenceException", error.c_str());
				mono_raise_exception (exception);
			}

			// We got an error message, parse it and throw it
			if (data.error)
			{
				error = MonoStringToCpp(data.error);
				string::size_type exceptionTypePos = error.find(':');
				if (exceptionTypePos != string::npos)
				{
					string type = string(error.begin(), error.begin() + exceptionTypePos);
					error.erase(error.begin(), error.begin()+exceptionTypePos + 1);

					MonoException* exception = mono_exception_from_name_msg (GetMonoManager().GetEngineImage(), "UnityEngine", type.c_str(), error.c_str ());
					mono_raise_exception (exception);
				}
			}
		}
	}

	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), "System", "NullReferenceException", "");
	mono_raise_exception (exception);
	#else
	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), "System", "NullReferenceException", "");
	mono_raise_exception (exception);
	#endif
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
