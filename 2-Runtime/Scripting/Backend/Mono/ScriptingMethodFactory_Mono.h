#ifndef _SCRIPTINGMETHODFACTORY_MONO_
#define _SCRIPTINGMETHODFACTORY_MONO_

#include "../ScriptingMethodFactory.h"
#include "Runtime/Mono/MonoIncludes.h"
#include "../ScriptingMethodRegistry.h"
#if ENABLE_MONO


// Flag defined in mono, when AOT libraries are built with -ficall option
// But that is not available in mono/consoles
extern "C" int mono_ficall_flag;

FastMonoMethod FastMonoMethodPtrFor(MonoMethod* method)
{
#if USE_MONO_AOT && !(UNITY_XENON || UNITY_PS3)
	return mono_ficall_flag && method ?  (FastMonoMethod) mono_aot_get_method(mono_domain_get(), method) : NULL;	
#else
	return NULL;
#endif
}

static bool MethodMatchesSearchFilter(MonoMethod* method, int searchFilter)
{
	MonoMethodSignature* sig = mono_method_signature(method);
	return MethodDescriptionMatchesSearchFilter(searchFilter, mono_signature_is_instance(sig), mono_signature_get_param_count(sig));
}

class ScriptingMethodFactory_Mono : public IScriptingMethodFactory
{
public:
	virtual ScriptingMethodPtr Produce(ScriptingTypePtr klass, const char* name, int searchFilter)
	{
		void* iterator = NULL;
		while (MonoMethod* method = mono_class_get_methods(klass, &iterator))
		{
			if (!method)
				return NULL;

			if (strcmp(mono_method_get_name(method), name)!=0)
				continue;

			if (MethodMatchesSearchFilter(method,searchFilter))
				return Produce(method);
		}
		return NULL;
	}

	virtual ScriptingMethodPtr Produce(BackendNativeMethod nativeMethod)
	{
		ScriptingMethodPtr result = new ScriptingMethod();
		result->monoMethod = nativeMethod;
		MonoMethodSignature* sig = mono_method_signature(nativeMethod);
#if UNITY_EDITOR
		if (!sig) {
			// Loader error - usually missing reference
			Scripting::LogException(mono_loader_error_prepare_exception (mono_loader_get_last_error ()), 0);
			return NULL;
		}
#endif
		result->isInstance = mono_signature_is_instance(sig);
		result->fastMonoMethod = IsSignatureSupportedForFastAotCalls(sig) ? FastMonoMethodPtrFor(nativeMethod) : NULL;
		return result;
	}

	virtual void Release(ScriptingMethodPtr method)
	{
		delete method;
	}

private:
	bool IsSignatureSupportedForFastAotCalls(MonoMethodSignature* sig)
	{
		if (!mono_signature_is_instance(sig))
			return false;

		return mono_signature_get_param_count(sig) == 0;
	}
};

#endif

#endif
