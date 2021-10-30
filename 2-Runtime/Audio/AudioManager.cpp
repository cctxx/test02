#include "UnityPrefix.h"
#include "Runtime/Misc/ReproductionLog.h"
#if ENABLE_AUDIO
#include "AudioManager.h"
#include "AudioListener.h"
#include "AudioSource.h"
#include "AudioReverbZone.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Audio/correct_fmod_includer.h"
#include "Runtime/Video/MoviePlayback.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "WavReader.h"
#include "Runtime/Profiler/ProfilerStats.h"
#include "Runtime/Utilities/UserAuthorizationManager.h"
#include "Runtime/Misc/UTF8.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

#if UNITY_IPHONE
#include "External/Audio/FMOD/builds/iphone/include/fmodiphone.h"
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioServices.h>
#include "Runtime/Misc/PlayerSettings.h"
#endif

#if UNITY_EDITOR
#include "Runtime/BaseClasses/IsPlaying.h"
#if UNITY_WIN
#include "PlatformDependent/Win/WinUnicode.h"
#endif
#endif

#if UNITY_PS3
#include "External/Audio/FMOD/builds/ps3/include/fmodps3.h"
struct CellSpurs2;
extern CellSpurs2* g_pSpursInstance;
extern uint8_t g_aFmodPriorities[8];
#endif

#if UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/FMOD_FileIO.h"
#endif

#if UNITY_XENON
#include "fmodxbox360.h"
#include "PlatformDependent/Xbox360/Source/Services/Audio.h"
#endif

#if UNITY_METRO
#include "PlatformDependent/MetroPlayer/AppCallbacks.h"
#include "PlatformDependent/MetroPlayer/MetroCapabilities.h"
#endif

#if UNITY_WP8
#include "PlatformDependent/WP8Player/WP8Capabilities.h"
#endif

void*  F_CALLBACK FMODMemoryAlloc (unsigned int size, FMOD_MEMORY_TYPE type, const char *sourcestr)
{
#if UNITY_XENON
	if(type & FMOD_MEMORY_XBOX360_PHYSICAL)
    {
        return XPhysicalAlloc(size, MAXULONG_PTR, 0, PAGE_READWRITE);
    }
	else
#endif
	{
		SET_ALLOC_OWNER(GetAudioManagerPtr());
		return UNITY_MALLOC_ALIGNED(kMemFMOD, size, 16);
    }
}

void F_CALLBACK FMODMemoryFree(void *ptr, FMOD_MEMORY_TYPE type, const char *sourcestr);

void*  F_CALLBACK FMODMemoryRealloc (void *ptr, unsigned int size, FMOD_MEMORY_TYPE type, const char *sourcestr)
{
#if UNITY_XENON
    if (type & FMOD_MEMORY_XBOX360_PHYSICAL)
    {
        char *newdata = (char *)FMODMemoryAlloc(size, type, sourcestr);
        if (newdata && ptr)
        {
            int copylen;
            int curlen = XPhysicalSize(ptr);
            copylen = (size > curlen) ? curlen : size;
            memcpy(newdata, ptr, copylen);
        }
        if (ptr)
        {
            FMODMemoryFree(ptr, type, sourcestr);
        }
        return newdata;   
    }
    else
#endif
	{
        return UNITY_REALLOC_ALIGNED(kMemFMOD, ptr, size, 16);
    }
}

void  F_CALLBACK FMODMemoryFree (void *ptr, FMOD_MEMORY_TYPE type, const char *sourcestr)
{
#if UNITY_XENON
    if (type & FMOD_MEMORY_XBOX360_PHYSICAL)
    {
		XPhysicalFree(ptr);
    }
	else
#endif
	{
		UNITY_FREE (kMemFMOD, ptr);
	}
}


using namespace std;

extern double GetTimeSinceStartup();

// ---------------------------------------------------------------------------

static void _InitScriptBufferManager()
{
#if ENABLE_AUDIO_FMOD
    if (GetAudioManagerPtr())
        GetAudioManager().InitScriptBufferManager();
#endif
}

void AudioManager::InitializeClass ()
{
#if UNITY_EDITOR
	RegisterAllowNameConversion (AudioManager::GetClassStringStatic(), "iOS DSP Buffer Size", "m_DSPBufferSize");
#endif

	
	//@TODO: Refactor this. Its ugly...
	GlobalCallbacks::Get().managersWillBeReloadedHack.Register(_InitScriptBufferManager);
}

void AudioManager::CleanupClass ()
{
	
}

AudioManager::AudioManager (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
	  ,m_DefaultVolume(1.0f)
	  ,m_Volume(1.0f)
	  ,m_IsPaused(false)
	  ,m_Rolloffscale(1.0f)
	  ,m_DisableAudio(false)
#if ENABLE_AUDIO_FMOD
	  ,m_SpeedOfSound(347.0f)
	  ,m_DopplerFactor(1.0f)
	  ,m_speakerMode(FMOD_SPEAKERMODE_STEREO)
	  ,m_activeSpeakerMode(FMOD_SPEAKERMODE_STEREO)
	  ,m_speakerModeCaps(FMOD_SPEAKERMODE_STEREO)
	  ,m_driverCaps(0)
	  ,m_FMODSystem(NULL)
	  ,m_ChannelGroup_FMODMaster(NULL)
	  ,m_ChannelGroup_FX_IgnoreVolume(NULL)
	  ,m_ChannelGroup_FX_UseVolume(NULL)
	  ,m_ChannelGroup_NoFX_IgnoreVolume(NULL)
	  ,m_ChannelGroup_NoFX_UseVolume(NULL)
	  ,m_DSPBufferSize(0)
	  ,m_ScriptBufferManager(NULL)
	  ,m_accPausedTicks(0)
	  ,m_pauseStartTicks(0)
#endif
{}

AudioManager::~AudioManager ()
{	
#if ENABLE_AUDIO_FMOD
	CloseFMOD();
	m_FMODSystem->release();
#endif
}

#if ENABLE_AUDIO_FMOD
// ----------------------------------------------------------------------
//  FMOD

bool AudioManager::ValidateFMODResult(FMOD_RESULT result, const char* errmsg)
{
	if (result != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(result);
		m_LastFMODErrorResult = result;
		ErrorString(std::string(errmsg) + m_LastErrorString);
		return false;
	}
	return true;
}


#if UNITY_METRO
namespace FMOD {

extern Windows::UI::Core::CoreDispatcher^ (*GetAppcallbackCoreDispatcher)(); // Defined in fmod_output_wasapi.cpp

} // namespace FMOD
#endif // UNITY_METRO

bool AudioManager::InitFMOD()
{
	FMOD_RESULT result;

	string fmoddebuglevel = GetFirstValueForARGV("fmoddebuglevel");
	if(!fmoddebuglevel.empty())
	{
		int level = atoi(fmoddebuglevel.c_str());
		FMOD::Debug_SetLevel(level);
	}
	else
	{
#if DEBUGMODE && !UNITY_WINRT
	// FMOD in verbose mode .. remember to link w. L versions of FMOD
	FMOD::Debug_SetLevel( FMOD_DEBUG_ALL );
#else
	FMOD::Debug_SetLevel(FMOD_DEBUG_LEVEL_NONE);
#endif	
	}

	if (!m_FMODSystem) // not loaded yet
	{		
		#if UNITY_METRO
			// This serves as a getter for the dispatcher from AppCallbacks.cpp
			FMOD::GetAppcallbackCoreDispatcher = &UnityPlayer::AppCallbacks::GetCoreDispatcher;
		#endif

		FMOD::Memory_Initialize(NULL, 0, FMODMemoryAlloc, FMODMemoryRealloc, FMODMemoryFree);
		result = FMOD::System_Create(&m_FMODSystem);          // Create the main system object. 
		if(!ValidateFMODResult(result, "FMOD failed to initialize ... ")) return false;
	#if UNITY_ANDROID
		m_FMODSystem->setFileSystem(FMOD_FileOpen, FMOD_FileClose, FMOD_FileRead, FMOD_FileSeek, 0,0,-1);
	#elif UNITY_WII
	#	if WIIWARE
		m_FMODSystem->setFileSystem(wii::FMOD_FileOpen, wii::FMOD_FileClose, wii::FMOD_FileRead, wii::FMOD_FileSeek, NULL, NULL, -1);
	#	endif
	#endif
	}
	
#if DEBUGMODE
	if (m_FMODSystem)
	{
		FMOD_ADVANCEDSETTINGS settings;
		memset(&settings, 0, sizeof(settings));
		settings.profileport = 9264;
	}
#endif

#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
	{
		if (!InitReproduction())
			return false;
	}
	else
	{
		if (!InitNormal())
			return false;
	}
#else
	if (!InitNormal())
		return false;
#endif // SUPPORT_REPRODUCE_LOG


	// 64k for streaming buffer sizes - we're streaming from memory so this should be sufficient
	// when we're streaming from a www/movie class, buffer sizes are set independently
	result = m_FMODSystem->setStreamBufferSize(64000, FMOD_TIMEUNIT_RAWBYTES);
	if(!ValidateFMODResult(result, "FMOD failed to initialize ... ")) return false;

	// Setup system callbacks
	result = m_FMODSystem->setCallback(AudioManager::systemCallback);
	if(!ValidateFMODResult(result, "FMOD failed to setup system callbacks ... ")) return false;

	#if UNITY_EDITOR
	m_EditorChannel = NULL;
	#endif
	
	// Setup channel callbacks
	result = m_FMODSystem->set3DRolloffCallback(AudioSource::rolloffCallback);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel callbacks ... ")) return false;
		
	// setup channel groups

	Assert(m_ChannelGroup_FMODMaster == NULL);
	result = m_FMODSystem->getMasterChannelGroup(&m_ChannelGroup_FMODMaster);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;

	Assert(m_ChannelGroup_FX_IgnoreVolume == NULL);
	result = m_FMODSystem->createChannelGroup("FX_IgnoreVol", &m_ChannelGroup_FX_IgnoreVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;

	Assert(m_ChannelGroup_FX_UseVolume == NULL);
	result = m_FMODSystem->createChannelGroup("FX_UseVol", &m_ChannelGroup_FX_UseVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;

	Assert(m_ChannelGroup_NoFX_IgnoreVolume == NULL);
    result = m_FMODSystem->createChannelGroup("NoFX_IgnoreVol", &m_ChannelGroup_NoFX_IgnoreVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;
        
	Assert(m_ChannelGroup_NoFX_UseVolume == NULL);
	result = m_FMODSystem->createChannelGroup("NoFX_UseVol", &m_ChannelGroup_NoFX_UseVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;
	
	result = m_ChannelGroup_FMODMaster->addGroup(m_ChannelGroup_FX_IgnoreVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;
	
	result = m_ChannelGroup_FX_IgnoreVolume->addGroup(m_ChannelGroup_FX_UseVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;

	result = m_ChannelGroup_FMODMaster->addGroup(m_ChannelGroup_NoFX_IgnoreVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;

	result = m_ChannelGroup_NoFX_IgnoreVolume->addGroup(m_ChannelGroup_NoFX_UseVolume);
	if(!ValidateFMODResult(result, "FMOD failed to setup channel groups ... ")) return false;
	
	m_activeSpeakerMode = m_speakerMode;
	
	return true;
}
void AudioManager::InitScriptBufferManager()
{
	if (m_ScriptBufferManager == 0)
	{
		unsigned DSPBufferSize;
		int maxOutputChannels;
		int maxInputChannels;
		m_FMODSystem->getDSPBufferSize(&DSPBufferSize, NULL);
		m_FMODSystem->getSoftwareFormat(NULL, 
															 NULL, 
															 &maxOutputChannels, 
															 &maxInputChannels, 
															 NULL, 
															 NULL);
		m_ScriptBufferManager = new AudioScriptBufferManager(16384 / 4, DSPBufferSize * std::max(maxOutputChannels, maxInputChannels)); 
	}
}
void AudioManager::ReloadFMODSounds()
{
	CloseFMOD();
	InitFMOD();
	InitScriptBufferManager();

	// reload any loaded audio clips
	std::vector<AudioClip*> audioClips;
	Object::FindObjectsOfType(&audioClips);
	for(std::vector<AudioClip*>::iterator it = audioClips.begin(); it != audioClips.end(); ++it)
		(*it)->Reload();
	
	#if ENABLE_SCRIPTING
	// Recreate Filters on Monobehaviours
	std::vector<MonoBehaviour*> monoBehaviours;
	Object::FindObjectsOfType(&monoBehaviours);
	for(std::vector<MonoBehaviour*>::iterator it = monoBehaviours.begin(); it != monoBehaviours.end(); ++it)
	{
		MonoBehaviour* behaviour = *it;
		FMOD::DSP* dsp = behaviour->GetOrCreateDSP();
		if (dsp)
			dsp->setBypass(!behaviour->GetEnabled());
	}
	#endif

	// Awake sources
	std::vector<AudioSource*> audioSources;
	Object::FindObjectsOfType(&audioSources);
	for(std::vector<AudioSource*>::iterator it = audioSources.begin(); it != audioSources.end(); ++it)
		(*it)->AwakeFromLoad(kDefaultAwakeFromLoad);
	
	// reload listener filters (if any)
	TAudioListenersIterator i = m_Listeners.begin();
	for (;i!=m_Listeners.end();++i)
	{
		AudioListener& curListener = **i;
		curListener.ApplyFilters();
	}	
	
	// reload reverb zones (if any)
	TAudioReverbZonesIterator j = m_ReverbZones.begin();
	for(;j!=m_ReverbZones.end();++j)
	{
		AudioReverbZone& curReverbZone = **j;
		curReverbZone.Init();
	}		
}


void AudioManager::SetSpeakerMode(FMOD_SPEAKERMODE speakerMode)
{
	m_speakerMode = speakerMode;
	if (m_activeSpeakerMode != m_speakerMode)
		ReloadFMODSounds();
} 
#endif  //ENABLE_AUDIO_FMOD

double AudioManager::GetDSPTime() const
{
#if ENABLE_AUDIO_FMOD
	int sampleRate;
	m_FMODSystem->getSoftwareFormat(&sampleRate, NULL, NULL, NULL, NULL, NULL);
	if(m_IsPaused)
		return (double)(m_pauseStartTicks - m_accPausedTicks) / (double)sampleRate;
	unsigned clockLo, clockHi;
	m_FMODSystem->getDSPClock(&clockHi, &clockLo);
	return (double)(((UInt64)clockHi << 32) + clockLo - m_accPausedTicks) / (double)sampleRate;
#else
	return GetTimeSinceStartup();
#endif
}

#if SUPPORT_REPRODUCE_LOG && ENABLE_AUDIO_FMOD
bool AudioManager::InitReproduction( )
{
	// set a non-realtime wav writer up as output
	FMOD_RESULT result = m_FMODSystem->setOutput(FMOD_OUTPUTTYPE_WAVWRITER_NRT);
	if(!ValidateFMODResult(result, "[Reproduction]FMOD failed to initialize WAV writer output... ")) return false;
	
	// set lowest possible mixing quality (stereo,11025hz, 8bit)
	result = m_FMODSystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
	if(!ValidateFMODResult(result, "[Reproduction]FMOD failed to set speaker mode... ")) return false;
	
	result = m_FMODSystem->setSoftwareFormat(22050, FMOD_SOUND_FORMAT_PCM8, 0, 6, FMOD_DSP_RESAMPLER_LINEAR);
	if(!ValidateFMODResult(result, "[Reproduction]FMOD failed to set software format... ")) return false;

	// init (with filename)
	string reproductionPathTemp = AppendPathName( GetReproductionDirectory(), "/Audio/" );
	string out = AppendPathName(reproductionPathTemp, "audio_output.wav");
	result = m_FMODSystem->init(100, FMOD_INIT_STREAM_FROM_UPDATE | FMOD_INIT_SYNCMIXERWITHUPDATE, (void*)out.c_str());        // Initialize FMOD. 	
	if(!ValidateFMODResult(result, "[Reproduction]FMOD failed to initialize ... ")) return false;

	return true;
}
#endif //SUPPORT_REPRODUCE_LOG && ENABLE_AUDIO_FMODD

#if UNITY_LINUX
static void LogDriverDetails (FMOD::System *system, int driverID)
{
	char driverName[BUFSIZ];
	FMOD_GUID guid;
	FMOD_OUTPUTTYPE output;
	char *outputName;

	system->getOutput (&output);
	system->getDriverInfo (driverID, driverName, BUFSIZ, &guid);

	switch (output) {
	case FMOD_OUTPUTTYPE_PULSEAUDIO:
		outputName = "PulseAudio";
		break;
	case FMOD_OUTPUTTYPE_ALSA:
		outputName = "ALSA";
		break;
	case FMOD_OUTPUTTYPE_OSS:
		outputName = "OSS";
		break;
	case FMOD_OUTPUTTYPE_ESD:
		outputName = "ESD";
		break;
	default:
		outputName = "Unknown";
		break;
	}
	printf_console ("AudioManager: Using %s: %s\n", outputName, driverName);
}
#endif


#if ENABLE_AUDIO_FMOD
bool AudioManager::InitNormal( )
{
	// Get available sound cards
	// If no device is found fall back on the NOSOUND driver
	// @TODO Enable user to choose driver
	int numDrivers;
	FMOD_RESULT result = m_FMODSystem->getNumDrivers(&numDrivers);
	if(!ValidateFMODResult(result, "FMOD failed to get number of drivers ... ")) return false;

	if (numDrivers == 0
#if !UNITY_EDITOR
		|| m_DisableAudio
#endif
	) // no suitable audio devices/drivers
	{
		result = m_FMODSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND);
		if(!ValidateFMODResult(result, "FMOD failed to initialize nosound device ... ")) return false;
	}
	
	// get driver id
	int driverID;
	m_FMODSystem->getDriver(&driverID);
	
	// setup speakermode
	// check if current hw is capable of the current speakerMode
	result = m_FMODSystem->getDriverCaps(driverID,&m_driverCaps,0,&m_speakerModeCaps);
	if(!ValidateFMODResult(result, "FMOD failed to get driver capabilities ... ")) return false;
	
	m_activeSpeakerMode = m_speakerMode;

	if (m_speakerModeCaps < m_activeSpeakerMode)
	{
		if (m_activeSpeakerMode != FMOD_SPEAKERMODE_SRS5_1_MATRIX)
			// hardware is not capable of the current speakerMode
			m_activeSpeakerMode = m_speakerModeCaps;
	}
		
#if UNITY_EDITOR

	int samplerate;
	FMOD_SOUND_FORMAT format;
	FMOD_DSP_RESAMPLER resampler;

	result = m_FMODSystem->getSoftwareFormat(&samplerate, &format, NULL, NULL, &resampler, NULL);
	if(!ValidateFMODResult(result, "FMOD failed to get driver capabilities ... ")) return false;

	result = m_FMODSystem->setSoftwareFormat(samplerate, format, 0, 8, resampler);
	if(!ValidateFMODResult(result, "FMOD failed to get driver capabilities ... ")) return false;

#endif

	result = m_FMODSystem->setSpeakerMode((FMOD_SPEAKERMODE)m_activeSpeakerMode);
	if(result != FMOD_OK)
	{
		ErrorStringMsg("FMOD could not set speaker mode to the one specified in the project settings. Falling back to stereo.");
		result = m_FMODSystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
	}
	if(!ValidateFMODResult(result, "FMOD failed to set speaker mode ... ")) return false;
	
	FMOD_INITFLAGS initFlags = FMOD_INIT_NORMAL;

	if(HasARGV("fmodprofiler"))
		initFlags |= FMOD_INIT_ENABLE_PROFILE;
	
	// Initialize FMOD.
#if UNITY_IPHONE	
	FMOD_IPHONE_EXTRADRIVERDATA extradriverdata;
	memset(&extradriverdata, 0, sizeof(FMOD_IPHONE_EXTRADRIVERDATA));

	if (GetPlayerSettings().prepareIOSForRecording)
		extradriverdata.sessionCategory = FMOD_IPHONE_SESSIONCATEGORY_PLAYANDRECORD;
	else if (GetPlayerSettings().overrideIPodMusic)
		extradriverdata.sessionCategory = FMOD_IPHONE_SESSIONCATEGORY_SOLOAMBIENTSOUND;
	else
		extradriverdata.sessionCategory = FMOD_IPHONE_SESSIONCATEGORY_AMBIENTSOUND;
	
	extradriverdata.forceMixWithOthers = !GetPlayerSettings().overrideIPodMusic;
	
	if (m_DSPBufferSize != 0)
	{
		result = m_FMODSystem->setDSPBufferSize(m_DSPBufferSize, 4);		
		if(!ValidateFMODResult(result, "FMOD failed to set DSP Buffer size ... ")) return false;
	}
	
	result = m_FMODSystem->init(100, initFlags, &extradriverdata); 
#elif UNITY_PS3
	if (!IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1) && m_activeSpeakerMode == FMOD_SPEAKERMODE_STEREO)
		initFlags |= FMOD_INIT_PS3_FORCE2CHLPCM;
	
	FMOD_PS3_EXTRADRIVERDATA extradriverdata;
	memset(&extradriverdata, 0, sizeof(FMOD_PS3_EXTRADRIVERDATA));
	extradriverdata.spurs = g_pSpursInstance;
	extradriverdata.spursmode = FMOD_PS3_SPURSMODE_CREATECONTEXT;
	extradriverdata.spurs_taskset_priorities = &g_aFmodPriorities[0];
	result = m_FMODSystem->setSoftwareChannels(64);
	result = m_FMODSystem->setOutput((!UNITY_EDITOR && m_DisableAudio) ? FMOD_OUTPUTTYPE_NOSOUND : FMOD_OUTPUTTYPE_PS3);
	result = m_FMODSystem->setSpeakerMode(FMOD_SPEAKERMODE_7POINT1);
	result = m_FMODSystem->init(100, initFlags | FMOD_INIT_VOL0_BECOMES_VIRTUAL, &extradriverdata); 

#elif UNITY_XENON
    FMOD_360_EXTRADRIVERDATA extraDriverData;
    ZeroMemory(&extraDriverData, sizeof(extraDriverData));
    extraDriverData.xaudio2instance = xenon::Audio::GetXAudio();
    result = m_FMODSystem->init(100, initFlags, &extraDriverData);
#elif UNITY_ANDROID
	result = m_FMODSystem->setOutput((!UNITY_EDITOR && m_DisableAudio) ? FMOD_OUTPUTTYPE_NOSOUND : FMOD_OUTPUTTYPE_AUDIOTRACK);
	if(!ValidateFMODResult(result, "FMOD failed to force Java Audio Track output ... ")) return false;
	if (m_DSPBufferSize != 0)
	{
		result = m_FMODSystem->setDSPBufferSize(m_DSPBufferSize, 4);		
		if(!ValidateFMODResult(result, "FMOD failed to set DSP Buffer size ... ")) return false;
	}
	result = m_FMODSystem->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
	result = m_FMODSystem->init(100, initFlags, NULL); 
#else
	result = m_FMODSystem->init(100, initFlags, NULL); 
#endif
	if(!ValidateFMODResult(result, "FMOD failed to initialize ... ")) return false;

#if UNITY_LINUX
	LogDriverDetails (m_FMODSystem, driverID);
#endif

	return true;
}

void AudioManager::CloseFMOD()
{
	if (m_FMODSystem)
	{
		// Cleanup sources
		std::vector<AudioSource*> audioSources;
		Object::FindObjectsOfType(&audioSources);
		for(std::vector<AudioSource*>::iterator it = audioSources.begin(); it != audioSources.end(); ++it)
			(*it)->Cleanup();		
		
		// Cleanup listener(s)
		std::vector<AudioListener*> audioListeners;
		Object::FindObjectsOfType(&audioListeners);
		for(std::vector<AudioListener*>::iterator it = audioListeners.begin(); it != audioListeners.end(); ++it)
			(*it)->Cleanup();
		
		// Cleanup reverb zone(s)
		std::vector<AudioReverbZone*> audioReverbZones;
		Object::FindObjectsOfType(&audioReverbZones);
		for(std::vector<AudioReverbZone*>::iterator it = audioReverbZones.begin(); it != audioReverbZones.end(); ++it)
			(*it)->Cleanup();
        
		if (m_ChannelGroup_FX_IgnoreVolume)
		{		
			m_ChannelGroup_FX_IgnoreVolume->release();
			m_ChannelGroup_FX_IgnoreVolume = NULL;
		}

		if (m_ChannelGroup_NoFX_IgnoreVolume)
		{		
			m_ChannelGroup_NoFX_IgnoreVolume->release();
			m_ChannelGroup_NoFX_IgnoreVolume = NULL;
		}
	
		if (m_ChannelGroup_FX_UseVolume)
		{		
			m_ChannelGroup_FX_UseVolume->release();
			m_ChannelGroup_FX_UseVolume = NULL;
		}

		if (m_ChannelGroup_NoFX_UseVolume)
		{
			m_ChannelGroup_NoFX_UseVolume->release();
			m_ChannelGroup_NoFX_UseVolume = NULL;
		}

		// m_ChannelGroup_FMODMaster  is the FMOD master group so we should not call release on it
		m_ChannelGroup_FMODMaster = NULL;

		// cleanup any loaded audio clips
		std::vector<AudioClip*> audioClips;
		Object::FindObjectsOfType(&audioClips);
		for(std::vector<AudioClip*>::iterator it = audioClips.begin(); it != audioClips.end(); ++it)
			(*it)->Cleanup();
		
		m_FMODSystem->close();
	}
	
	delete m_ScriptBufferManager;
	m_ScriptBufferManager = NULL;
}



int AudioManager::GetMemoryAllocated() const
{
	int a = 0;
	FMOD::Memory_GetStats(&a, NULL);
	return a;
}

float AudioManager::GetCPUUsage() const
{
	float c = 0.0f;
	if (m_FMODSystem)
		m_FMODSystem->getCPUUsage(NULL, NULL, NULL,NULL, &c);
	return c;
}

#if ENABLE_PROFILER
void AudioManager::GetProfilerData( AudioStats& audioStats )
{
	if (m_FMODSystem)
	{
		FMOD::Memory_GetStats(&audioStats.audioMemUsage, &audioStats.audioMaxMemUsage);
		float cpuUsage;
		m_FMODSystem->getCPUUsage(NULL, NULL, NULL, NULL, &cpuUsage);
		audioStats.audioCPUusage = RoundfToInt(cpuUsage * 10.0F);
		m_FMODSystem->getChannelsPlaying(&audioStats.audioVoices);
		audioStats.pausedSources = m_PausedSources.size_slow();
		audioStats.playingSources = m_Sources.size_slow();
		audioStats.audioClipCount = AudioClip::s_AudioClipCount;
		audioStats.audioSourceCount = AudioSource::s_AudioSourceCount;
		FMOD_MEMORY_USAGE_DETAILS details;
		m_FMODSystem->getMemoryInfo(FMOD_EVENT_MEMBITS_ALL, 0, &audioStats.audioMemDetailsUsage, &details);
		audioStats.audioMemDetails.other = details.other;                          /* [out] Memory not accounted for by other types */
		audioStats.audioMemDetails.string = details.string;                         /* [out] String data */
		audioStats.audioMemDetails.system = details.system;                         /* [out] System object and various internals */
		audioStats.audioMemDetails.plugins = details.plugins;                        /* [out] Plugin objects and internals */
		audioStats.audioMemDetails.output = details.output;                         /* [out] Output module object and internals */
		audioStats.audioMemDetails.channel = details.channel;                        /* [out] Channel related memory */
		audioStats.audioMemDetails.channelgroup = details.channelgroup;                   /* [out] ChannelGroup objects and internals */
		audioStats.audioMemDetails.codec = details.codec;                          /* [out] Codecs allocated for streaming */
		audioStats.audioMemDetails.file = details.file;                           /* [out] File buffers and structures */
		audioStats.audioMemDetails.sound = details.sound;                          /* [out] Sound objects and internals */
		audioStats.audioMemDetails.secondaryram = details.secondaryram;                   /* [out] Sound data stored in secondary RAM */
		audioStats.audioMemDetails.soundgroup = details.soundgroup;                     /* [out] SoundGroup objects and internals */
		audioStats.audioMemDetails.streambuffer = details.streambuffer;                   /* [out] Stream buffer memory */
		audioStats.audioMemDetails.dspconnection = details.dspconnection;                  /* [out] DSPConnection objects and internals */
		audioStats.audioMemDetails.dsp = details.dsp;                            /* [out] DSP implementation objects */
		audioStats.audioMemDetails.dspcodec = details.dspcodec;                       /* [out] Realtime file format decoding DSP objects */
		audioStats.audioMemDetails.profile = details.profile;                        /* [out] Profiler memory footprint. */
		audioStats.audioMemDetails.recordbuffer = details.recordbuffer;                   /* [out] Buffer used to store recorded data from microphone */
		audioStats.audioMemDetails.reverb = details.reverb;                         /* [out] Reverb implementation objects */
		audioStats.audioMemDetails.reverbchannelprops = details.reverbchannelprops;             /* [out] Reverb channel properties structs */
		audioStats.audioMemDetails.geometry = details.geometry;                       /* [out] Geometry objects and internals */
		audioStats.audioMemDetails.syncpoint = details.syncpoint;                      /* [out] Sync point memory. */
		audioStats.audioMemDetails.eventsystem = details.eventsystem;                    /* [out] EventSystem and various internals */
		audioStats.audioMemDetails.musicsystem = details.musicsystem;                    /* [out] MusicSystem and various internals */
		audioStats.audioMemDetails.fev = details.fev;                            /* [out] Definition of objects contained in all loaded projects e.g. events, groups, categories */
		audioStats.audioMemDetails.memoryfsb = details.memoryfsb;                      /* [out] Data loaded with preloadFSB */
		audioStats.audioMemDetails.eventproject = details.eventproject;                   /* [out] EventProject objects and internals */
		audioStats.audioMemDetails.eventgroupi = details.eventgroupi;                    /* [out] EventGroup objects and internals */
		audioStats.audioMemDetails.soundbankclass = details.soundbankclass;                 /* [out] Objects used to manage wave banks */
		audioStats.audioMemDetails.soundbanklist = details.soundbanklist;                  /* [out] Data used to manage lists of wave bank usage */
		audioStats.audioMemDetails.streaminstance = details.streaminstance;                 /* [out] Stream objects and internals */
		audioStats.audioMemDetails.sounddefclass = details.sounddefclass;                  /* [out] Sound definition objects */
		audioStats.audioMemDetails.sounddefdefclass = details.sounddefdefclass;               /* [out] Sound definition static data objects */
		audioStats.audioMemDetails.sounddefpool = details.sounddefpool;                   /* [out] Sound definition pool data */
		audioStats.audioMemDetails.reverbdef = details.reverbdef;                      /* [out] Reverb definition objects */
		audioStats.audioMemDetails.eventreverb = details.eventreverb;                    /* [out] Reverb objects */
		audioStats.audioMemDetails.userproperty = details.userproperty;                   /* [out] User property objects */
		audioStats.audioMemDetails.eventinstance = details.eventinstance;                  /* [out] Event instance base objects */
		audioStats.audioMemDetails.eventinstance_complex = details.eventinstance_complex;          /* [out] Complex event instance objects */
		audioStats.audioMemDetails.eventinstance_simple = details.eventinstance_simple;           /* [out] Simple event instance objects */
		audioStats.audioMemDetails.eventinstance_layer = details.eventinstance_layer;            /* [out] Event layer instance objects */
		audioStats.audioMemDetails.eventinstance_sound = details.eventinstance_sound;            /* [out] Event sound instance objects */
		audioStats.audioMemDetails.eventenvelope = details.eventenvelope;                  /* [out] Event envelope objects */
		audioStats.audioMemDetails.eventenvelopedef = details.eventenvelopedef;               /* [out] Event envelope definition objects */
		audioStats.audioMemDetails.eventparameter = details.eventparameter;                 /* [out] Event parameter objects */
		audioStats.audioMemDetails.eventcategory = details.eventcategory;                  /* [out] Event category objects */
		audioStats.audioMemDetails.eventenvelopepoint = details.eventenvelopepoint;             /* [out] Event envelope point objects */
		audioStats.audioMemDetails.eventinstancepool = details.eventinstancepool;              /* [out] Event instance pool memory */
	}
}
#endif
#endif // ENABLE_AUDIO_FMOD

#if ENABLE_WWW && ENABLE_AUDIO_FMOD
FMOD::Sound* AudioManager::CreateFMODSoundFromWWW(WWW* webStream,
									 bool threeD,
									 FMOD_SOUND_TYPE suggestedtype,
									 FMOD_SOUND_FORMAT format,
									 unsigned freq,
									 unsigned channels,
									 bool stream)
{
	if (!m_FMODSystem)
		return NULL;

	FMOD::Sound* sound = NULL;
	FMOD_CREATESOUNDEXINFO exInfo;
	memset(&exInfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
	exInfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
	exInfo.decodebuffersize = 16384;
	exInfo.suggestedsoundtype = suggestedtype; 
	exInfo.format = format;
	exInfo.defaultfrequency = freq;
	exInfo.numchannels = channels;
	exInfo.useropen = AudioClip::WWWOpen;
	exInfo.userclose = AudioClip::WWWClose;
	exInfo.userread = AudioClip::WWWRead;
	exInfo.userseek = AudioClip::WWWSeek;
	exInfo.userdata = (void*)webStream;
	
	
    FMOD_MODE mode = FMOD_SOFTWARE | (threeD?FMOD_3D:FMOD_2D) | (stream?FMOD_CREATESTREAM:FMOD_CREATESAMPLE) | (suggestedtype==FMOD_SOUND_TYPE_MPEG?FMOD_MPEGSEARCH:FMOD_IGNORETAGS) | FMOD_LOOP_NORMAL | FMOD_3D_CUSTOMROLLOFF ;
	
	if (suggestedtype==FMOD_SOUND_TYPE_RAW)
		mode |= FMOD_OPENRAW;
	
	FMOD_RESULT err = m_FMODSystem->createSound(
												(const char*)webStream,
												mode,
												&exInfo,
												&sound );
	
	if (err != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(err);
		m_LastFMODErrorResult = err;
		return NULL;
	}
	else
	{
		// FMOD_LOOP_NORMAL is set on createSound() - this prepares the sound for looping
		// now turn it off (and let the user set it later)
		sound->setMode( FMOD_LOOP_OFF );
		return sound;	
	}
	
}
#endif // ENABLE_WWW && ENABLE_AUDIO_FMOD

#if ENABLE_AUDIO_FMOD
FMOD::Sound* AudioManager::CreateFMODSoundFromMovie(AudioClip* clip, bool threeD)
{
	if (!m_FMODSystem)
		return NULL;

	Assert ( clip->GetMovie() );
	
	FMOD::Sound* sound = NULL;
	FMOD_CREATESOUNDEXINFO exInfo;
	memset(&exInfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
	exInfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);	
	// @FIX FMODs position is increased even though the pcm reader thread is starving (FMOD just repeats the last buffer).
	// @FIX This can lead to out-of-synch problems and that the audio is stopped before the movie is done.
	// @FIX fix: double the length of the clip as a headroom (and stop the audio when video frames are done)
	// @FIX duration: Theora format is really stupid for streams, so if we´re not lucky that the duration is in the meta-tags we set it to infinity
	if (clip->GetMovie()->GetMovieTotalDuration() < 0)
		exInfo.length = 0xffffffff;
	else		
		exInfo.length = (unsigned int)(clip->GetMovie()->GetMovieTotalDuration() * clip->GetFrequency() * clip->GetChannelCount() * ( clip->GetBitsPerSample() / 8 ) * 2);
	exInfo.decodebuffersize = 4096;
	exInfo.format = FMOD_SOUND_FORMAT_PCM16;
	exInfo.defaultfrequency = clip->GetFrequency();
	exInfo.numchannels = clip->GetChannelCount();	
	exInfo.pcmreadcallback = AudioClip::pcmread;
	exInfo.userdata = (void*)clip;
	
	
    FMOD_MODE mode = FMOD_SOFTWARE | (threeD?FMOD_3D:FMOD_2D) | FMOD_CREATESTREAM | FMOD_OPENUSER | FMOD_IGNORETAGS | FMOD_LOOP_NORMAL | FMOD_3D_CUSTOMROLLOFF ;
		
	FMOD_RESULT err = m_FMODSystem->createSound(
												0,
												mode,
												&exInfo,
												&sound );
	
	if (err != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(err);
		m_LastFMODErrorResult = err;
		return NULL;
	}
	else
	{
		// FMOD_LOOP_NORMAL is set on createSound() - this prepares the sound for looping
		// now turn it off (and let the user set it later)
		sound->setMode( FMOD_LOOP_OFF );
		return sound;	
	}	
}


FMOD::Sound* AudioManager::CreateFMODSound(const void* buffer, FMOD_CREATESOUNDEXINFO exInfo, FMOD_MODE mode, bool useHardwareDecoder)
{
	Assert(exInfo.cbsize == sizeof(FMOD_CREATESOUNDEXINFO));

#if !(UNITY_ANDROID || UNITY_XENON || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN)
	// On Android the file paths have been converted to ascii and fmod doesn't seem to accept unicode strings
	dynamic_array<UnicodeChar> unicodeBuf (kMemTempAlloc);
	if((mode & (FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT | FMOD_OPENUSER)) == 0 && (mode & FMOD_UNICODE) == 0)
	{
		ConvertUTF8toUTF16((const char*)buffer, unicodeBuf);
		unicodeBuf.push_back(0);
		unicodeBuf.push_back(0); // FMOD reads unicode strings as shorts, so it expects 2 bytes
		buffer = (const char*)&unicodeBuf[0];
		mode |= FMOD_UNICODE;
	}
#endif
	
#if UNITY_IPHONE	
	if (mode & FMOD_CREATESTREAM)
	{
		exInfo.suggestedsoundtype = exInfo.suggestedsoundtype==FMOD_SOUND_TYPE_MPEG?FMOD_SOUND_TYPE_AUDIOQUEUE:exInfo.suggestedsoundtype;
		FMOD_AUDIOQUEUE_CODECPOLICY policy = FMOD_AUDIOQUEUE_CODECPOLICY_SOFTWAREONLY;
		if (useHardwareDecoder)
			policy = FMOD_AUDIOQUEUE_CODECPOLICY_DEFAULT; // try hardware, if it fails then try software
		exInfo.audioqueuepolicy = policy;
	}
#endif	
	FMOD::Sound* sound = NULL;	
	FMOD_RESULT result = m_FMODSystem->createSound(
												(const char*)buffer,
												mode,
												&exInfo,
												&sound );	
#if UNITY_IPHONE
	// if the Apple's AudioQueue codec(sf/hw) fails completely (this can happen with some obscure mp3s that FMOD software MP3 codec can read/import, but Apple's decoder can't)
	// then use FMOD's SOFTWARE codec instead
	// @TODO Wait for FMOD to handle this internally
	if (FMOD_ERR_FORMAT == result)
	{
		exInfo.suggestedsoundtype = FMOD_SOUND_TYPE_MPEG;
		result = m_FMODSystem->createSound(
										(const char*)buffer,
										mode,
										&exInfo,
										&sound );		
	}
#endif	

	if (result != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(result);
		m_LastFMODErrorResult = result;
#if UNITY_WII
		ErrorStringMsg ("ERROR: %s\n", m_LastErrorString.c_str());
#endif
		return NULL;
	}
	else
	{
		// FMOD_LOOP_NORMAL is set on createSound() - this prepares the sound for looping
		// now turn it off (and let the user set it later)
		sound->setMode( FMOD_LOOP_OFF );
		return sound;	
	}
}
#endif //ENABLE_AUDIO_FMOD					
									
#if UNITY_EDITOR && ENABLE_AUDIO_FMOD					
FMOD::Sound* AudioManager::CreateFMODSound(const std::string& path, bool threeD, bool hardware, bool openOnly /* = false */)
{
	FMOD::Sound* sound = NULL;
	
	if (!m_FMODSystem)
		return NULL;

	Assert( m_FMODSystem );
	
	const char* strPtr = path.c_str();
	FMOD_MODE mode = (openOnly?FMOD_OPENONLY:0x0) | FMOD_SOFTWARE | (threeD?FMOD_3D:FMOD_2D) | FMOD_LOOP_OFF | FMOD_MPEGSEARCH;
#if !UNITY_ANDROID
	// On Android the file paths have been converted to ascii and fmod doesn't seem to accept unicode strings
	dynamic_array<UnicodeChar> unicodeBuf (kMemTempAlloc);
	if((mode & (FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT | FMOD_OPENUSER)) == 0 && (mode & FMOD_UNICODE) == 0)
	{
		ConvertUTF8toUTF16(strPtr, unicodeBuf);
		unicodeBuf.push_back(0);
		unicodeBuf.push_back(0); // FMOD reads unicode strings as shorts, so it expects 2 bytes
		strPtr = (const char*)&unicodeBuf[0];
		mode |= FMOD_UNICODE;
	}
#endif
	
	FMOD_RESULT err = m_FMODSystem->createSound(strPtr, mode, 0, &sound);
	if (err != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(err);
		m_LastFMODErrorResult = err;
		return NULL;
	}
	else
		return sound;	
}

#endif // UNITY_EDITOR && ENABLE_AUDIO_FMOD					


#if ENABLE_AUDIO_FMOD
FMOD::Channel* AudioManager::GetFreeFMODChannel(FMOD::Sound* sound, bool paused /*=true*/)
{
	if (!m_FMODSystem)
		return NULL;
	
	FMOD::Channel* channel;
	FMOD_RESULT err = m_FMODSystem->playSound(FMOD_CHANNEL_FREE, sound, paused || (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && m_IsPaused), &channel);
	if (err != FMOD_OK)
	{
		m_LastErrorString = FMOD_ErrorString(err);
		m_LastFMODErrorResult = err;
		return NULL;
	}
	else
		return channel;
	
}
#endif //ENABLE_AUDIO_FMOD


void AudioManager::UpdateListener (
								  const Vector3f& position, 
								  const Vector3f& velocity, 
								  const Vector3f& up,
								  const Vector3f& forward)
{
#if ENABLE_AUDIO_FMOD
	if (!m_FMODSystem)
		return;
	
	m_FMODSystem->set3DListenerAttributes(
								   0, 
								   reinterpret_cast<const FMOD_VECTOR*>( &position ), 
								   reinterpret_cast<const FMOD_VECTOR*>( &velocity ),								   
								   reinterpret_cast<const FMOD_VECTOR*>( &forward ),
								   reinterpret_cast<const FMOD_VECTOR*>( &up )  
								   );	
#endif //ENABLE_AUDIO_FMOD
	
}

int AudioManager::GetAutomaticUpdateMode(GameObject *go)
{
	Rigidbody* body = go->QueryComponent (Rigidbody);
	if (body)
		return kVelocityUpdateModeFixed;

	Transform* parent = go->GetComponent (Transform).GetParent ();
	while (parent)
	{
		go = parent->GetGameObjectPtr ();
		if (go)
			body = go->QueryComponent (Rigidbody);
		else
			body = NULL;
		if (body)
			return kVelocityUpdateModeFixed;
		
		parent = parent->GetParent ();
	}
	return kVelocityUpdateModeDynamic;
}

void AudioManager::SetPause (bool pause)
{
	if(m_IsPaused == pause)
		return;
	m_IsPaused = pause;
#if ENABLE_AUDIO_FMOD
	unsigned clockLo, clockHi;
	m_FMODSystem->getDSPClock(&clockHi, &clockLo);
	UInt64 dspTicks = ((UInt64)clockHi << 32) + clockLo;
#else
	UInt64 dspTicks = 0;
#endif
	if (m_IsPaused)
	{
		m_pauseStartTicks = dspTicks;
		// Pause all audio sources and put them in the m_PausedSources, when resuming we start playing them again
		for (TAudioSourcesIterator i=m_Sources.begin();i != m_Sources.end();)
		{			
			AudioSource& source = **(i);
			i++;
			
			if (source.IsPlaying())
			{
				source.Pause();
				m_PausedSources.push_back(source.m_Node);
			}
			source.PauseOneShots();
		}		
	}
	else
	{
		UInt64 pauseDuration = dspTicks - m_pauseStartTicks;
		m_accPausedTicks += pauseDuration;
		// Resume all paused audio sources
		for (TAudioSourcesIterator i=m_PausedSources.begin();i != m_PausedSources.end();)
		{
			AudioSource& source = **(i);
			++i;
			
			// Play is pushing the source to the m_Sources list
			// don't play if editor is pause
			source.Play();
			if(source.HasScheduledTime())
				source.CorrectScheduledTimeAfterUnpause(pauseDuration);
		}

		for (TAudioSourcesIterator i=m_Sources.begin();i != m_Sources.end();)
		{
			AudioSource& source = **(i);
			++i;
			source.ResumeOneShots();
		}
		Assert( m_PausedSources.empty() );
	}
}

void AudioManager::SetVolume (float volume)
{
#if ENABLE_AUDIO_FMOD
	if (!m_FMODSystem)
		return;
	m_ChannelGroup_FX_UseVolume->setVolume(volume);
	m_ChannelGroup_NoFX_UseVolume->setVolume(volume);
#endif
#if UNITY_FLASH
	__asm __volatile__("SoundMixer.soundTransform = new SoundTransform(Math.min(Math.max(0.0, %0), 1.0));"::"f"(volume));
#endif
	m_Volume = volume;
}

float AudioManager::GetVolume () const
{
	return m_Volume;
}


template<class TransferFunction>
void AudioManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	transfer.Transfer (m_DefaultVolume, "m_Volume", kSimpleEditorMask);
	transfer.Transfer (m_Rolloffscale, "Rolloff Scale");
	
#if ENABLE_AUDIO_FMOD
	if (!(transfer.IsWritingGameReleaseData () && transfer.GetBuildingTarget().platform==kBuildFlash || transfer.GetBuildingTarget().platform==kBuildWebGL)){
		transfer.Transfer (m_SpeedOfSound, "m_SpeedOfSound");
		transfer.Transfer (m_DopplerFactor, "Doppler Factor");
		
		transfer.Transfer ((SInt32&)m_speakerMode, "Default Speaker Mode"); // Remember to specify the TransferName in the doxygen comment in the .h file

		TRANSFER (m_DSPBufferSize);
	}
#endif	

	TRANSFER (m_DisableAudio);
}


void AudioManager::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
#if ENABLE_AUDIO_FMOD	
	if (!m_FMODSystem)
	{
		InitFMOD();
		m_IsPaused = false;
	}
	
	if (!m_FMODSystem)
		return;
	
	// has the speakermode changed?
	if (m_activeSpeakerMode != m_speakerMode)
	{
		// bootstrap FMOD
		ReloadFMODSounds();
		if (!m_FMODSystem)
			return;
	}
	m_Volume = m_DefaultVolume;	
	m_ChannelGroup_FX_UseVolume->setVolume(m_Volume);
	m_ChannelGroup_NoFX_UseVolume->setVolume(m_Volume);
	m_FMODSystem->set3DSettings(m_DopplerFactor, 1, m_Rolloffscale);
#endif
}

PROFILER_INFORMATION(gAudioUpdateProfile, "AudioManager.Update", kProfilerAudio);
PROFILER_INFORMATION(gAudioFixedUpdateProfile, "AudioManager.FixedUpdate", kProfilerAudio);

#if UNITY_EDITOR
void AudioManager::ListenerCheck()
{
	if (!IsWorldPlaying())
		return;
	
	int listenerCount = m_Listeners.size_slow();
	// @TODO enable multiple listeners
	if (listenerCount == 0 && !m_Sources.empty())
	{
		LogString("There are no audio listeners in the scene. Please ensure there is always one audio listener in the scene");		
		m_ChannelGroup_FX_UseVolume->setVolume(0.0f);
		m_ChannelGroup_NoFX_UseVolume->setVolume(0.0f);
	}
	else
	{
		if (listenerCount > 1)
			LogString(Format("There are %d audio listeners in the scene. Please ensure there is always exactly one audio listener in the scene.", listenerCount));
		m_ChannelGroup_FX_UseVolume->setVolume(m_Volume);
		m_ChannelGroup_NoFX_UseVolume->setVolume(m_Volume);
	}	
}
#endif


#define Unity_HiWord(x) ((UInt32)((UInt64)(x) >> 32))
#define Unity_LoWord(x) ((UInt32)(x))
#if UNITY_WII
void AudioManager::UpdateOnDiskEject()
{
	for (TAudioSourcesIterator i = m_Sources.begin(); i != m_Sources.end(); i++)
	{
		AudioSource& curSource = **i;
		curSource.Update();
	}	
	m_FMODSystem->update();
}
#endif
void AudioManager::ProcessScheduledSources()
{
#if ENABLE_AUDIO_FMOD
	// start scheduled sources
	// Get mixer clock
	unsigned hiclock, loclock;
	m_FMODSystem->getDSPClock(&hiclock, &loclock);
#endif	
	for (TScheduledSourcesIterator s = m_ScheduledSources.begin(); s != m_ScheduledSources.end(); s++)
	{
		const AudioScheduledSource& p = *s;
		AudioSource* curSource = p.source;
		
		Assert(curSource != NULL);
		Assert(curSource->m_Channel != NULL);
		

#if ENABLE_AUDIO_FMOD
		if (p.time != 0.0)
		{
			int sampleRate;
			m_FMODSystem->getSoftwareFormat(&sampleRate, NULL, NULL, NULL, NULL, NULL);
			if(p.time > 0.0)
			{
				// exact scheduled
				UInt64 sample = (UInt64)(p.time * sampleRate) + m_accPausedTicks;
				curSource->m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_START, Unity_HiWord(sample), Unity_LoWord(sample)); 
			}
			else
			{
				UInt64 sample = ((UInt64)hiclock << 32) + loclock + (UInt64)(-p.time * sampleRate);
				curSource->m_Channel->setDelay(FMOD_DELAYTYPE_DSPCLOCK_START, Unity_HiWord(sample), Unity_LoWord(sample)); 
			}
			curSource->m_HasScheduledStartDelay = true;
		}
#endif
		// play (TODO: if paused in the same frame - pause here)
		bool paused = curSource->m_pause || (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a3) && m_IsPaused && !curSource->m_AudioParameters.ignoreListenerPause);
		curSource->m_Channel->setPaused(paused);
		AddAudioSource(curSource, paused); // to make sure source is put into active or paused sources lists
	}
	// clean queue
	m_ScheduledSources.clear();
}

void AudioManager::Update()
{
	PROFILER_AUTO (gAudioUpdateProfile, NULL);
	
#if ENABLE_AUDIO_FMOD
	if (!m_FMODSystem)
		return;	
#endif
	
	ProcessScheduledSources();
	
#if UNITY_EDITOR
	ListenerCheck();			
#endif
		
	for (TAudioListenersIterator l = m_Listeners.begin(); l != m_Listeners.end(); l++)
	{
		AudioListener& curListener = **l;
		curListener.Update();
	}	
	
	for (TAudioSourcesIterator i = m_Sources.begin(); i != m_Sources.end(); i++)
	{
		AudioSource& curSource = **i;
#if UNITY_EDITOR
		if (!IsWorldPlaying() && curSource.m_Channel)
		{
			if (curSource.GetGameObject().IsMarkedVisible())
				curSource.m_Channel->setMute(curSource.GetMute());
			else
				curSource.m_Channel->setMute(true);
		}
#endif
		curSource.Update();
	}
#if ENABLE_AUDIO_FMOD	
	// update reverb zones position
	for (TAudioReverbZonesIterator r = m_ReverbZones.begin(); r != m_ReverbZones.end(); ++r)
	{
		AudioReverbZone& curReverbZone = **r;
		curReverbZone.Update();
	}
			 
	m_FMODSystem->update();
#endif
}


void AudioManager::FixedUpdate()
{
#if ENABLE_AUDIO_FMOD
	if (!m_FMODSystem)
		return;
#endif
	PROFILER_AUTO (gAudioFixedUpdateProfile, NULL);

	#if UNITY_EDITOR
	ListenerCheck();	
	#endif
	
	for (TAudioListenersIterator l = m_Listeners.begin(); l != m_Listeners.end(); l++)
	{
		AudioListener& curListener = **l;
		curListener.FixedUpdate();
	}	
	
	TAudioSourcesIterator i;	
	for (i = m_Sources.begin(); i != m_Sources.end(); i++)
	{
		AudioSource& curSource = **i;
		curSource.FixedUpdate();
	}	
}

void AudioManager::AddAudioSource( AudioSource* s, bool paused )
{
	Assert(s);
	if(paused)
		m_PausedSources.push_back(s->m_Node);
	else
		m_Sources.push_back(s->m_Node);
}	

void AudioManager::RemoveAudioSource( AudioSource* s )
{
	Assert(s);
	UnScheduleSource(s);
	s->m_Node.RemoveFromList(); // note: removes either from m_Sources or m_PausedSources
}

void AudioManager::StopSources()
{ 
	TAudioSourcesIterator i = m_Sources.begin();
	while (!m_Sources.empty())
	{
		AudioSource& curSource = **i;
		++i;
		curSource.Stop(true);
	}
	i = m_PausedSources.begin();
	while (!m_PausedSources.empty())
	{
		AudioSource& curSource = **i;
		++i;
		curSource.Stop(true);
	}
}

void AudioManager::AddAudioListener (AudioListener* s)
{
	Assert(s);
	m_Listeners.push_back( s->GetNode() );
}

void AudioManager::RemoveAudioListener (AudioListener* s)
{
	Assert(s);
	s->GetNode().RemoveFromList();
}

AudioListener* AudioManager::GetAudioListener() const
{
	if (!m_Listeners.empty())
		return m_Listeners.back().GetData();
	else
		return NULL;
}

/// Schedule source to be played in sync. In this frame if time==0, delayed by -time if time<0 or scheduled at time
void AudioManager::ScheduleSource(AudioSource* s, double time)
{
	s->m_ScheduledSource.RemoveFromList();
	s->m_ScheduledSource.time = time;
	m_ScheduledSources.push_back(s->m_ScheduledSource);
}

void AudioManager::UnScheduleSource(AudioSource* s)
{
	s->m_ScheduledSource.RemoveFromList();
}

#if ENABLE_AUDIO_FMOD
void AudioManager::AddAudioReverbZone(AudioReverbZone* zone)
{
	Assert(zone);
	m_ReverbZones.push_back(zone->m_Node);	
}

void AudioManager::RemoveAudioReverbZone(AudioReverbZone* zone)
{
	Assert(zone);
	zone->m_Node.RemoveFromList();	
}



#endif

#if ENABLE_MICROPHONE
// Microphone(s)
bool HasMicrophoneAuthorization ()
{
	#if UNITY_WINRT
		#if UNITY_METRO
		namespace Capabilities = metro::Capabilities;
		#elif UNITY_WP8
		namespace Capabilities = WP8::Capabilities;
		#else
		#error Unknown WinRT flavour (did you implement capability detection for the OS?)
		#endif

		Capabilities::IsSupported(Capabilities::kMicrophone, "because you're using Microphone functionality");
	#endif // UNITY_WINRT

	return GetUserAuthorizationManager().HasUserAuthorization(UserAuthorizationManager::kMicrophone);
}

const std::vector<std::string> AudioManager::GetRecordDevices() const
{
	std::vector<std::string> devices;
	m_MicrophoneNameToIDMap.clear();
	
	if (!m_FMODSystem)
		return devices;	
	
	if (!HasMicrophoneAuthorization())
		return devices;
	
	int numDevices;
	FMOD_RESULT result = m_FMODSystem->getRecordNumDrivers(&numDevices);
	
	if (result != FMOD_OK)
		return devices;
	
	if (numDevices > 0)
	{
		for (int i=0; i < numDevices; ++i)
		{
			char name[255];
			
			m_FMODSystem->getRecordDriverInfo(i, name, 255, NULL);
			
			std::string strName = (char*)name;
			// update map with a unique name
			std::string origName = strName;
			int no = 0;
			while (m_MicrophoneNameToIDMap.find(strName) != m_MicrophoneNameToIDMap.end())
			{
				char post[3];
				sprintf(post, " %i", ++no);
				strName = origName + post;
			}
			devices.push_back(strName);
			m_MicrophoneNameToIDMap[strName] = i;						
		}
	}
	
	return devices;
}

int AudioManager::GetMicrophoneDeviceIDFromName(const std::string& name) const
{
	if ( m_MicrophoneNameToIDMap.empty() )
		GetRecordDevices();

	// Double lookup on return is totally unnecessary, because we can cache the iterator
	std::map<std::string, int>::const_iterator devit = m_MicrophoneNameToIDMap.find( name );
	if ( devit != m_MicrophoneNameToIDMap.end() )
		return devit->second;
	else
		return 0; // the default device is always 0.
}


void ReportError (char const* msg, FMOD_RESULT result)
{
	ErrorString (Format ("%s. result=%d (%s)", msg, result, FMOD_ErrorString (result)));
}

void CapsToSoundFormat (FMOD_CAPS caps, FMOD_SOUND_FORMAT *soundFormat, int *sampleSizeInBytes)
{
	*soundFormat = FMOD_SOUND_FORMAT_PCM16;
	*sampleSizeInBytes = 2;
	
	if ((caps & FMOD_CAPS_OUTPUT_FORMAT_PCM16) == FMOD_CAPS_OUTPUT_FORMAT_PCM16)
	{
		*soundFormat = FMOD_SOUND_FORMAT_PCM16;
		*sampleSizeInBytes = 2;
	}
	else
	if ((caps & FMOD_CAPS_OUTPUT_FORMAT_PCM8) == FMOD_CAPS_OUTPUT_FORMAT_PCM8)
	{
		*soundFormat = FMOD_SOUND_FORMAT_PCM8;
		*sampleSizeInBytes = 1;
	}
	else 
	if ((caps & FMOD_CAPS_OUTPUT_FORMAT_PCM24) == FMOD_CAPS_OUTPUT_FORMAT_PCM24)
	{
		*soundFormat = FMOD_SOUND_FORMAT_PCM24;
		*sampleSizeInBytes = 3;
	}
	else if ((caps & FMOD_CAPS_OUTPUT_FORMAT_PCM32) == FMOD_CAPS_OUTPUT_FORMAT_PCM32)
	{
		*soundFormat = FMOD_SOUND_FORMAT_PCM32;
		*sampleSizeInBytes = 4;
	}
	else if ((caps & FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT) == FMOD_CAPS_OUTPUT_FORMAT_PCMFLOAT)
	{
		*soundFormat = FMOD_SOUND_FORMAT_PCMFLOAT;
		*sampleSizeInBytes = sizeof (float);
	}
}

void AudioManager::GetDeviceCaps(int deviceID, int *minFreq, int *maxFreq) const
{
	FMOD_CAPS caps = 0;
	FMOD_RESULT result = m_FMODSystem->getRecordDriverCaps (deviceID, &caps, minFreq, maxFreq);
	
	if (result != FMOD_OK)
	{
		ReportError ("Failed to get record driver caps", result);
	}
}

FMOD::Sound* AudioManager::CreateSound (int deviceID, int lengthSec, int frequency)
{
	FMOD::Sound* sound;
	FMOD_CAPS caps = 0;
	FMOD_RESULT result = m_FMODSystem->getRecordDriverCaps (deviceID, &caps, NULL, NULL);

	if (result != FMOD_OK)
	{
		ReportError ("Failed to get record driver caps", result);
		return NULL;
	}

	FMOD_SOUND_FORMAT soundFormat = FMOD_SOUND_FORMAT_NONE;
	int sampleSizeInBytes = 1;
	CapsToSoundFormat (caps, &soundFormat, &sampleSizeInBytes);

	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));

	exinfo.cbsize           = sizeof(FMOD_CREATESOUNDEXINFO);
	exinfo.numchannels      = 1;
	exinfo.format           = soundFormat;
	exinfo.defaultfrequency = frequency;
	exinfo.length = exinfo.defaultfrequency * sampleSizeInBytes * exinfo.numchannels * lengthSec;

	FMOD_MODE mode = FMOD_2D | FMOD_SOFTWARE | FMOD_OPENUSER;
	result = m_FMODSystem->createSound (0, mode, &exinfo, &sound);

	if (result != FMOD_OK)
	{
		ReportError ("Failed to create sound clip for recording", result);
		return NULL;
	}

	return sound;
}

PPtr<AudioClip> AudioManager::StartRecord(int deviceID, bool loop, int lengthSec, int frequency)
{
	if (!m_FMODSystem)
		return NULL;

	if (!HasMicrophoneAuthorization())
		return NULL;
	
	if (lengthSec <= 0)
	{
		ErrorString("Length of the recording must be greater than zero (0)");
		return NULL;
	}
	
	if (frequency <= 0)
	{
		ErrorString("Frequency must be greater than zero (0)");
		return NULL;	
	}

	FMOD::Sound* sound = CreateSound (deviceID, lengthSec, frequency);

	if (sound == NULL)
		return NULL;

	FMOD_RESULT result = m_FMODSystem->recordStart (deviceID, sound, loop);

	if (result == FMOD_OK)
	{
		PPtr<AudioClip> audioClip = NEW_OBJECT(AudioClip);
		audioClip->Reset();
		audioClip->HackSetAwakeWasCalled();

		audioClip->InitWSound(sound);
		audioClip->SetName("Microphone");

		return audioClip;
	}
	else
	{
		ReportError ("Starting Microphone failed", result);
		return NULL;
	}
}

bool AudioManager::EndRecord(int deviceID)
{
	if (!m_FMODSystem)
		return false;
	
	m_FMODSystem->recordStop(deviceID);
	
	return true;
}

bool AudioManager::IsRecording(int deviceID) const
{
	if (!m_FMODSystem)
		return false;
	
	bool isRecording;
	m_FMODSystem->isRecording(deviceID, &isRecording);
	return isRecording;
}

unsigned AudioManager::GetRecordPosition(int deviceID) const
{
	if (!m_FMODSystem)
		return 0;
	
	unsigned pos;
	m_FMODSystem->getRecordPosition(deviceID, &pos);
	return pos;
}

#endif // ENABLE_MICROPHONE

#if ENABLE_AUDIO_FMOD
AudioScriptBufferManager& AudioManager::GetScriptBufferManager()
{
	return *GetScriptBufferManagerPtr();
}

AudioScriptBufferManager* AudioManager::GetScriptBufferManagerPtr()
{
	if (m_ScriptBufferManager == 0)
	{
		InitScriptBufferManager ();
	}

	return m_ScriptBufferManager;
}
#endif

#if UNITY_EDITOR
void AudioManager::PlayClip(AudioClip& clip, int startSample, bool loop, bool twoD)
{
	if (!m_FMODSystem)
		return;

	// update FMOD to get any device changes
	m_FMODSystem->update();
	
	if (m_EditorChannel)
		m_EditorChannel->stop();

	m_EditorChannel = clip.CreateChannel();
	
	if(m_EditorChannel != NULL)
	{
		if (twoD && clip.Is3D())
		{
			m_EditorChannel->setMode(FMOD_2D);
		}

		m_EditorChannel->setChannelGroup(m_ChannelGroup_NoFX_IgnoreVolume);

		FMOD_REVERB_CHANNELPROPERTIES rev;
		memset(&rev, 0, sizeof(rev));
		rev.Room = -10000;
		m_EditorChannel->setReverbProperties(&rev);

		if (loop)
			m_EditorChannel->setMode(FMOD_LOOP_NORMAL);

		// movie audio
		if (clip.GetMovie())
			clip.GetMovie()->SetAudioChannel(m_EditorChannel);

		m_EditorChannel->setPaused(false);
	}
}


void AudioManager::LoopClip(const AudioClip& clip, bool loop)
{
	if (m_EditorChannel)
		m_EditorChannel->setMode(loop?FMOD_LOOP_NORMAL:FMOD_LOOP_OFF);
}


void AudioManager::StopClip(const AudioClip& clip)
{
	if (m_EditorChannel)
		m_EditorChannel->stop();

	if (clip.GetMovie())
		clip.GetMovie()->SetAudioChannel(NULL);
}

void AudioManager::StopAllClips()
{
	if (m_EditorChannel)
		m_EditorChannel->stop();	
}
void AudioManager::PauseClip(const AudioClip& clip)
{
	if (m_EditorChannel)
		m_EditorChannel->setPaused(true);

	if (clip.GetMovie())
		clip.GetMovie()->SetAudioChannel(NULL);
}
void AudioManager::ResumeClip(const AudioClip& clip)
{
	if (m_EditorChannel)
		m_EditorChannel->setPaused(false);}

bool AudioManager::IsClipPlaying(const AudioClip& clip)
{
	bool isPlaying = false;
	if (m_EditorChannel)
		 m_EditorChannel->isPlaying(&isPlaying);
	return isPlaying;
}

float AudioManager::GetClipPosition(const AudioClip& clip)
{
	unsigned int pos = 0;
	if (m_EditorChannel)
		m_EditorChannel->getPosition(&pos, FMOD_TIMEUNIT_MS);
	return (float)pos / 1000.f;
}

unsigned int AudioManager::GetClipSamplePosition(const AudioClip& clip)
{
	unsigned int pos = 0;
	if (m_EditorChannel)
		m_EditorChannel->getPosition(&pos, FMOD_TIMEUNIT_PCM);
	return pos;
}

void AudioManager::SetClipSamplePosition(const AudioClip& clip, unsigned int iSamplePosition)
{
	if (m_EditorChannel)
		m_EditorChannel->setPosition(iSamplePosition, FMOD_TIMEUNIT_PCM);
}

#endif // UNITY_EDITOR



IMPLEMENT_CLASS_HAS_INIT (AudioManager)
IMPLEMENT_OBJECT_SERIALIZE (AudioManager)
GET_MANAGER (AudioManager)
GET_MANAGER_PTR (AudioManager)

#endif //ENABLE_AUDIO
