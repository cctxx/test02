#ifndef SCRIPTINGUTILITY_H
#define SCRIPTINGUTILITY_H

#if ENABLE_SCRIPTING
#include "Runtime/Scripting/Backend/ScriptingTypes.h"

#include "Runtime/BaseClasses/BaseObject.h"

#include "Runtime/Scripting/Backend/ScriptingArguments.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/ICallString.h"

#if ENABLE_MONO_API_THREAD_CHECK && ENABLE_MONO
# include "Runtime/Threads/Thread.h"
# define SCRIPTINGAPI_CONSTRUCTOR_CHECK(NAME)	\
	if (GetMonoBehaviourInConstructor() == 0) ; else { \
		Scripting::RaiseArgumentException("You are not allowed to call " #NAME " when declaring a variable.\nMove it to the line after without a variable declaration.\nDon't use this function in the constructor or field initializers, instead move initialization code to the Awake or Start function."); \
	}

# define SCRIPTINGAPI_THREAD_CHECK(NAME) \
		if ( Thread::CurrentThreadIsMainThread() ) ; else \
		{\
			ErrorString(#NAME " can only be called from the main thread.\nConstructors and field initializers will be executed from the loading thread when loading a scene.\nDon't use this function in the constructor or field initializers, instead move initialization code to the Awake or Start function."); \
			Scripting::RaiseArgumentException(#NAME " can only be called from the main thread.\nConstructors and field initializers will be executed from the loading thread when loading a scene.\nDon't use this function in the constructor or field initializers, instead move initialization code to the Awake or Start function.");\
		}

#else
# define SCRIPTINGAPI_THREAD_CHECK(NAME)
# define SCRIPTINGAPI_CONSTRUCTOR_CHECK(NAME)
#endif

#if ENABLE_MONO
# include "Runtime/Mono/MonoIncludes.h"
# include "Runtime/Mono/MonoUtility.h"
#elif UNITY_WINRT
#elif UNITY_FLASH
# include "AS3Utility.h"
#endif

#include "Scripting.h"

//todo: put these back at the top when we've cleaned up the dependency mess
#include "Runtime/Scripting/ScriptingObjectOfType.h"
#include "Runtime/Scripting/ReadOnlyScriptingObjectOfType.h"

#endif //ENABLE_SCRIPTING

#endif
