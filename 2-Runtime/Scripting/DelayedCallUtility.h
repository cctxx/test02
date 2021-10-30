#ifndef _DELAYEDCALLUTILITYH_
#define _DELAYEDCALLUTILITYH_

#if ENABLE_SCRIPTING
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/ICallString.h"

class MonoBehaviour;

void InvokeDelayed (MonoBehaviour& behaviour, ICallString& monoMethodName, float time, float repeatRate);
void CancelInvoke (MonoBehaviour& behaviour, ICallString& monoMethodName);
void CancelInvoke (MonoBehaviour& behaviour);
bool IsInvoking (MonoBehaviour& behaviour, ICallString& monoMethodName);
bool IsInvoking (MonoBehaviour& behaviour);
#endif
#endif
