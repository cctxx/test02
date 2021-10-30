#include "UnityPrefix.h"
#if ENABLE_AUDIO_FMOD
#include "AudioManager.h"
#include "Runtime/Audio/correct_fmod_includer.h"

FMOD_RESULT F_CALLBACK AudioManager::systemCallback(FMOD_SYSTEM* c_system, FMOD_SYSTEM_CALLBACKTYPE type, void* data1, void* data2)
{
	FMOD::System* system = (FMOD::System*)c_system;
	FMOD_RESULT result = FMOD_OK;

	switch (type)
	{
		case FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED:
			// Get available sound cards
			// If no device is found fall back on the NOSOUND driver
			// @TODO Enable user to choose driver
			int numDrivers;
			result = system->getNumDrivers(&numDrivers);

			if ((result == FMOD_OK) && (numDrivers != 0)) 
			{
				// set driver to the new default driver
				// and autodetect output
				result = system->setDriver(0);
				if (result != FMOD_OK) 
				{ 
					ErrorString(Format("Default audio device was changed, but the audio system failed to initialize it (%s). This may be because the audio device that Unity was started on and the device switched to have different sampling rates or speaker configurations. To get sound back you can either adjust the sampling rate on the new device (on Windows using the control panel, on Mac via the Audio MIDI Setup application), switch back to the old device or restart Unity.",FMOD_ErrorString(result)));
					return result;
				}
			}
		break;
		default: break;
	}

	return result;
}
#endif //ENABLE_AUDIO