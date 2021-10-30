#ifndef _SCRIPTINGINVOCATIONNOARGS_H_
#define _SCRIPTINGINVOCATIONNOARGS_H_

#if ENABLE_SCRIPTING

#include "ScriptingTypes.h"

class ScriptingInvocationNoArgs
{
public:
	ScriptingInvocationNoArgs();
	ScriptingInvocationNoArgs(ScriptingMethodPtr in_method);

	ScriptingMethodPtr method;
	ScriptingObjectPtr object;
	int objectInstanceIDContextForException;
	ScriptingTypePtr classContextForProfiler;
	bool logException;
	ScriptingExceptionPtr exception;
	
	ScriptingObjectPtr Invoke();
	virtual ScriptingObjectPtr Invoke(ScriptingException**);
	ScriptingObjectPtr InvokeChecked();
protected:
	void SetDefaults();
	virtual bool Check();
};

#endif

#endif
