#if ENABLE_AUDIO_FMOD
	#if UNITY_WIN
		#include "External/Audio/FMOD/builds/win32/include/fmod.hpp"
		#include "External/Audio/FMOD/builds/win32/include/fmod_errors.h"
		#include "External/Audio/FMOD/builds/win32/include/fmod_types.h"
	#elif UNITY_LINUX
		#if defined(__LP64__) || defined(_LP64)
			#include "External/Audio/FMOD/builds/linux64/include/fmod.hpp"
			#include "External/Audio/FMOD/builds/linux64/include/fmod_errors.h"
			#include "External/Audio/FMOD/builds/linux64/include/fmod_types.h"
		#else
			#include "External/Audio/FMOD/builds/linux32/include/fmod.hpp"
			#include "External/Audio/FMOD/builds/linux32/include/fmod_errors.h"
			#include "External/Audio/FMOD/builds/linux32/include/fmod_types.h"
		#endif
	#elif UNITY_IPHONE
		#include "External/Audio/FMOD/builds/iphone/include/fmod.hpp"
		#include "External/Audio/FMOD/builds/iphone/include/fmod_errors.h"
		#include "External/Audio/FMOD/builds/iphone/include/fmod_types.h"
	#elif UNITY_PS3
		#include "External/Audio/FMOD/builds/ps3/include/fmod.hpp"
		#include "External/Audio/FMOD/builds/ps3/include/fmod_errors.h"
		#include "External/Audio/FMOD/builds/ps3/include/fmod_types.h"
	#elif UNITY_BB10
		#include "External/Audio/FMOD/builds/bb10/include/fmod.hpp"
		#include "External/Audio/FMOD/builds/bb10/include/fmod_errors.h"
		#include "External/Audio/FMOD/builds/bb10/include/fmod_types.h"
	#elif UNITY_TIZEN
		#include "External/Audio/FMOD/builds/tizen/include/fmod.hpp"
		#include "External/Audio/FMOD/builds/tizen/include/fmod_errors.h"
		#include "External/Audio/FMOD/builds/tizen/include/fmod_types.h"
	#else
		#include <fmod.hpp>
		#include <fmod_errors.h>
		#include <fmod_types.h>
	#endif
#elif UNITY_FLASH || UNITY_WEBGL
	#include "External/Audio/FMOD/builds/win32/include/fmod.h"
#endif
