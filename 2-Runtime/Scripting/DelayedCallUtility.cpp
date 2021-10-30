#include "UnityPrefix.h"

#if ENABLE_SCRIPTING

#include "Runtime/Scripting/DelayedCallUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/GameCode/CallDelayed.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"

static void ForwardInvokeDelayed (Object* o, void* userData)
{
	const char* methodName = (const char*)userData;
	MonoBehaviour* behaviour = static_cast<MonoBehaviour*> (o);
	if (behaviour->GetInstance ())
	{
		bool didCall = behaviour->CallMethodInactive (methodName);
		#if !DEPLOY_OPTIMIZED
		if (!didCall)
			LogStringObject ("Trying to Invoke method: " + behaviour->GetScript ()->GetScriptClassName () + "." + methodName + " couldn't be called.", o);
		#else
			UNUSED(didCall);
		#endif
		
	}
}

static void ForwardInvokeDelayedCleanup (void* userData)
{
	ScriptingStringToAllocatedChars_Free ((char*)userData);
}

void InvokeDelayed (MonoBehaviour& behaviour, ICallString& monoMethodName, float time, float repeatRate)
{
	char* methodName = ScriptingStringToAllocatedChars (monoMethodName);
	if (repeatRate > 0.00001F || repeatRate == 0.0F)
		CallDelayed (&ForwardInvokeDelayed, &behaviour, time, methodName, repeatRate, &ForwardInvokeDelayedCleanup);
	else
		Scripting::RaiseMonoException ("Invoke repeat rate has to be larger than 0.00001F)");
}


static bool ShouldCancelInvoke (void* lhs, void* rhs)
{
	return strcmp ((char*)lhs, (char*)rhs) == 0;
}

void CancelInvoke (MonoBehaviour& behaviour, ICallString& monoMethodName)
{
	char* methodName = ScriptingStringToAllocatedChars (monoMethodName);
	GetDelayedCallManager ().CancelCallDelayed (&behaviour, &ForwardInvokeDelayed, &ShouldCancelInvoke, methodName);
	ScriptingStringToAllocatedChars_Free (methodName);
}

void CancelInvoke (MonoBehaviour& behaviour)
{
	GetDelayedCallManager ().CancelCallDelayed (&behaviour, &ForwardInvokeDelayed, NULL, NULL);
}

bool IsInvoking (MonoBehaviour& behaviour)
{
	return GetDelayedCallManager ().HasDelayedCall (&behaviour, &ForwardInvokeDelayed, NULL, NULL);
}

bool IsInvoking (MonoBehaviour& behaviour, ICallString& monoMethodName)
{
	char* methodName = ScriptingStringToAllocatedChars (monoMethodName);
	bool result = GetDelayedCallManager ().HasDelayedCall (&behaviour, &ForwardInvokeDelayed, &ShouldCancelInvoke, methodName);
	ScriptingStringToAllocatedChars_Free (methodName);
	return result;
}

#endif
