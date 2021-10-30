#include "UnityPrefix.h"
#include "AnimationEvent.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingArguments.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingObjectWithIntPtrField.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#if ENABLE_MONO
#endif


INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED (AnimationEvent)

template<class TransferFunction>
void AnimationEvent::Transfer (TransferFunction& transfer)
{
	TRANSFER (time);
	TRANSFER (functionName);
	transfer.Transfer (stringParameter, "data");
	transfer.Transfer (objectReferenceParameter, "objectReferenceParameter");
	transfer.Transfer (floatParameter, "floatParameter");
	transfer.Transfer (intParameter, "intParameter");

	TRANSFER (messageOptions);
}

#if ENABLE_SCRIPTING


static ScriptingObjectPtr s_ManagedAnimationEvent;

static bool SetupInvokeArgument(ScriptingMethodPtr method, AnimationEvent& event, ScriptingArguments& parameters)
{
	int argCount = scripting_method_get_argument_count(method, GetScriptingTypeRegistry());

	// Fast path - method takes no arguments
	if (argCount == 0)
		return true;
	
	if (argCount > 1)
		return false;

	ScriptingTypePtr typeOfFirstArgument = scripting_method_get_nth_argumenttype(method,0,GetScriptingTypeRegistry());

	const CommonScriptingClasses& cc = GetScriptingManager().GetCommonClasses();

	if (typeOfFirstArgument == cc.floatSingle)
	{
		parameters.AddFloat(event.floatParameter);
		return true;
	}
		
	if (typeOfFirstArgument == cc.int_32)
	{
		parameters.AddInt(event.intParameter);
		return true;
	}
		
	if (typeOfFirstArgument == cc.string)
	{
		parameters.AddString(event.stringParameter.c_str());
		return true;
	}
		
	if (typeOfFirstArgument == cc.animationEvent)
	{
		ScriptingObjectWithIntPtrField<AnimationEvent> scriptingAnimationEvent = scripting_object_new(GetScriptingManager().GetCommonClasses().animationEvent);
		scriptingAnimationEvent.SetPtr(&event);
		
		s_ManagedAnimationEvent = scriptingAnimationEvent.object;
		parameters.AddObject(scriptingAnimationEvent.object);
		return true;
	}
		
	if (scripting_class_is_subclass_of(typeOfFirstArgument,cc.unityEngineObject))
	{
		parameters.AddObject(Scripting::ScriptingWrapperFor(event.objectReferenceParameter));
		return true;
	}	

	if (scripting_class_is_enum(typeOfFirstArgument))
	{
		parameters.AddInt(event.intParameter);
		return true;
	}
	
	return false;
}

static void CleanupManagedAnimationEventIfRequired()
{
	if (s_ManagedAnimationEvent == SCRIPTING_NULL)
		return;

	AnimationEvent* nativeAnimationEvent = NULL;
	MarshallNativeStructIntoManaged(nativeAnimationEvent, s_ManagedAnimationEvent);
	s_ManagedAnimationEvent = SCRIPTING_NULL;
}

static bool FireEventTo(MonoBehaviour& behaviour, AnimationEvent& event, AnimationState* state)
{
	ScriptingObjectPtr instance = behaviour.GetInstance ();
	if (instance == SCRIPTING_NULL)
		return false;

	ScriptingMethodPtr method = behaviour.FindMethod (event.functionName.c_str());
	if (method == SCRIPTING_NULL)
		return false;

	
	ScriptingInvocation invocation(method);
	
	if (!SetupInvokeArgument(method, event, invocation.Arguments()))
	{
		ErrorStringObject (Format ("Failed to call AnimationEvent %s of class %s.\nThe function must have either 0 or 1 parameters and the parameter can only be: string, float, int, enum, Object and AnimationEvent.", scripting_method_get_name (method), behaviour.GetScriptClassName ().c_str ()), &behaviour);
		return true;
	}

    // Suppress immediate destruction during the event callback to disallow
    // the object killing itself directly or indirectly in there (would do bad
    // things to the still updating animation state).
    const bool disableImmediateDestruction = IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1);
    bool oldDisableImmediateDestruction = false;
    if (disableImmediateDestruction)
    {
        oldDisableImmediateDestruction = GetDisableImmediateDestruction ();
        SetDisableImmediateDestruction (true);
    }
	
	event.stateSender = state;

	ScriptingExceptionPtr exception = NULL;
	invocation.object = instance;
	invocation.logException = true;
	invocation.objectInstanceIDContextForException = behaviour.GetInstanceID();
	ScriptingObjectPtr returnValue = invocation.Invoke();
    
    if (disableImmediateDestruction)
        SetDisableImmediateDestruction (oldDisableImmediateDestruction);

	if (returnValue && exception == NULL)
		behaviour.HandleCoroutineReturnValue (method, returnValue);

	event.stateSender = NULL;
	CleanupManagedAnimationEventIfRequired();
		
	return true;
}

static bool EventRequiresReceiver(const AnimationEvent& event)
{
	return event.messageOptions == 0;
}

#endif

bool FireEvent (AnimationEvent& event, AnimationState* state, Unity::Component& animation)
{
	#if ENABLE_SCRIPTING
	GameObject& go = animation.GetGameObject();
	if (!go.IsActive ())
		return false;
	
	bool sent = false;
	
	for (int i=0;i<go.GetComponentCount ();i++)
	{
		if (go.GetComponentClassIDAtIndex (i) != ClassID (MonoBehaviour))
			continue;

		MonoBehaviour& behaviour = static_cast<MonoBehaviour&> (go.GetComponentAtIndex (i));
		if (FireEventTo(behaviour,event,state))
			sent = true;
	}
	
	if (DEPLOY_OPTIMIZED)
		return true;

	if (sent)
		return true;
	
	if (!EventRequiresReceiver(event))
		return true;

	std::string warning = event.functionName.empty() 
		? Format ("'%s' AnimationEvent has no function name specified!", go.GetName())
		: Format ("'%s' AnimationEvent '%s' has no receiver! Are you missing a component?", go.GetName(), event.functionName.c_str());
		
	ErrorStringObject (warning.c_str(), animation.GetGameObjectPtr());
	return true;
	#else
	return false;
	#endif
}
