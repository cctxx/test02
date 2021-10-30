#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"

static void RegisterAnimationClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (Animation)
	REGISTER_CLASS (AnimationClip)
	REGISTER_CLASS (RuntimeAnimatorController)
	REGISTER_CLASS (AnimatorController)	
	REGISTER_CLASS (Animator)
	REGISTER_CLASS (Avatar)	
	REGISTER_CLASS (Motion)
#if !UNITY_WEBGL
	REGISTER_CLASS (AnimatorOverrideController)
#endif
#if UNITY_EDITOR
	///@TODO: Lets remove those. It's been deprecated since Unity 1.6
	REGISTER_CLASS (BaseAnimationTrack)
	REGISTER_CLASS (NewAnimationTrack)
#endif	
}

#if ENABLE_MONO || UNITY_WINRT
void ExportRuntimeAnimatorControllerBindings();
void ExportAnimatorBindings();
void ExportAnimations();
void ExportAvatarBuilderBindings ();
void ExportAvatar ();
void ExportAnimatorOverrideControllerBindings();

static void RegisterAnimationICallModule ()
{
#if !INTERNAL_CALL_STRIPPING
	ExportRuntimeAnimatorControllerBindings ();
	ExportAnimatorBindings();
	ExportAnimations ();
	ExportAvatarBuilderBindings ();
	ExportAvatar ();
	ExportAnimatorOverrideControllerBindings ();	
#endif
}
#endif

extern "C" EXPORT_MODULE void RegisterModule_Animation ()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterAnimationClasses;
#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterAnimationICallModule;
#endif
	RegisterModuleInfo (info);
}
