#ifndef _SCRIPTINGINVOCATION_H_
#define _SCRIPTINGINVOCATION_H_

#if ENABLE_SCRIPTING

#include "ScriptingInvocationNoArgs.h"
#include "ScriptingArguments.h"


/// ScriptingInvocation invocation (scriptingMethod); // Slow alternative (Don't use this in runtime code) ScriptingInvocation invocation ("MyNameSpace", "MyClassName", "MyMethod");
/// invocation.AddInt(5);
/// invocation.Invoke (); // < you are certain that the bound method accepts those exact parameters
/// invocation.InvokeChecked(); // < you cant guarantee that the bound method has the passed parameters

/// By default logs the exception (Can be turned off via invocation.logException = false;)
/// Parameters are setup AddInt, AddFloat, AddStruct functions below
class ScriptingInvocation : public ScriptingInvocationNoArgs
{
public:
	ScriptingInvocation();
	ScriptingInvocation(ScriptingMethodPtr in_method);
#if ENABLE_MONO || UNITY_WINRT
	ScriptingInvocation(const char* namespaze, const char* klass, const char* name);
	ScriptingInvocation(ScriptingClassPtr klass, const char* name);
	ScriptingInvocation(BackendNativeMethod method);
#endif

	//convenience forwarders to arguments
	void AddBoolean(bool value) { arguments.AddBoolean(value); }
	void AddInt(int value) { arguments.AddInt(value); }
	void AddFloat(float value) { arguments.AddFloat(value); }
	void AddString(const char* str) { arguments.AddString(str); }
	void AddString(std::string& str) { arguments.AddString(str); }
	void AddObject(ScriptingObjectPtr scriptingObject) { arguments.AddObject(scriptingObject); }
	void AddStruct(void* pointerToStruct) { arguments.AddStruct(pointerToStruct); }
	void AddEnum(int value) { arguments.AddEnum(value); }
	void AddArray(ScriptingArrayPtr arr) { arguments.AddArray(arr); }

	void AdjustArgumentsToMatchMethod();
	
	template<class T> T Invoke();
	template<class T> T Invoke(ScriptingException**);
	ScriptingObjectPtr Invoke();
	ScriptingObjectPtr Invoke(ScriptingException**);
	ScriptingObjectPtr Invoke(ScriptingExceptionPtr*, bool);

	ScriptingArguments& Arguments() { return arguments; }
protected:
	ScriptingArguments arguments;
	virtual bool Check();
};


#if ENABLE_MONO

struct MonoObject;
struct MonoException;
class Object;
struct MonoMethod;
struct MonoClass;
class MonoScript;

MonoObject* CallStaticMonoMethod (const char* className, const char* methodName, void** parameters = NULL);
MonoObject* CallStaticMonoMethod (const char* className, const char* methodName, void** parameters, MonoException** exception);
MonoObject* CallStaticMonoMethod (const char* className, const char* nameSpace, const char* methodName, void** paramenters = NULL);
MonoObject* CallStaticMonoMethod (const char* className, const char* nameSpace, const char* methodName, void** paramenters, MonoException** exception);
MonoObject* CallStaticMonoMethod (MonoClass* klass , const char* methodName, void** parameters = NULL);
MonoObject* CallStaticMonoMethod (MonoClass* klass , const char* methodName, void** parameters, MonoException** exception);
#endif

#endif

#endif
