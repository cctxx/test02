#include "UnityPrefix.h"

#if ENABLE_AUDIO
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"
#include "Runtime/Interfaces/IAudio.h"

static void RegisterAudioClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (AudioManager)
	REGISTER_CLASS (AudioListener)
	REGISTER_CLASS (AudioSource)
	REGISTER_CLASS (AudioClip)
	REGISTER_CLASS (AudioBehaviour)

#if ENABLE_AUDIO_FMOD
	REGISTER_CLASS (AudioReverbFilter)
	REGISTER_CLASS (AudioHighPassFilter)
	REGISTER_CLASS (AudioChorusFilter)
	REGISTER_CLASS (AudioReverbZone)
	REGISTER_CLASS (AudioEchoFilter)
	REGISTER_CLASS (AudioLowPassFilter)
	REGISTER_CLASS (AudioDistortionFilter)
	REGISTER_CLASS (AudioFilter)
#endif

#if ENABLE_MOVIES
	REGISTER_CLASS (MovieTexture)
#endif

#if ENABLE_WEBCAM
	REGISTER_CLASS (WebCamTexture)
#endif

}

void ExportAudioBindings ();
void ExportUnityEngineWebCamTexture ();
void ExportMovieTextureBindings();

static void RegisterAudioICallModule ()
{
#if !INTERNAL_CALL_STRIPPING
	ExportAudioBindings ();
	#if ENABLE_WEBCAM
	ExportUnityEngineWebCamTexture ();
	#endif
	#if ENABLE_MOVIES
	ExportMovieTextureBindings();
	#endif
#endif
}

extern "C" EXPORT_MODULE void RegisterModule_Audio ()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterAudioClasses;
#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterAudioICallModule;
#endif
	RegisterModuleInfo (info);

	SetIAudio(CreateAudioModule());
}
#endif