#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include "Runtime/Modules/ExportModules.h"
#include <string>

enum RuntimePlatform
{
	// NEVER change constants of existing platforms!
	OSXEditor = 0,
	OSXPlayer = 1,
	WindowsPlayer = 2,
	OSXWebPlayer = 3,
	OSXDashboardPlayer = 4,
	WindowsWebPlayer = 5,
	WiiPlayer = 6,
	WindowsEditor = 7,
	iPhonePlayer = 8,
	PS3Player = 9,
	XenonPlayer = 10,
	AndroidPlayer = 11,
	NaClWebPlayer = 12,
	LinuxPlayer = 13,
	LinuxWebPlayer = 14,
	FlashPlayer = 15,
	LinuxEditor = 16,
	WebGLPlayer = 17,
	MetroPlayerX86 = 18,
	MetroPlayerX64 = 19,
	MetroPlayerARM = 20,
	WP8Player = 21,
	BB10Player = 22,
	TizenPlayer = 23,

	RuntimePlatformCount // keep this last
};

enum DeviceType
{
	kDeviceTypeUnknown = 0,
	kDeviceTypeHandheld,
	kDeviceTypeConsole,
	kDeviceTypeDesktop,
	kDeviceTypeCount // keep this last
};

enum SystemLanguage {
	SystemLanguageAfrikaans,
	SystemLanguageArabic,
	SystemLanguageBasque,
	SystemLanguageBelarusian,
	SystemLanguageBulgarian,
	SystemLanguageCatalan,
	SystemLanguageChinese,
	SystemLanguageCzech,
	SystemLanguageDanish,
	SystemLanguageDutch,
	SystemLanguageEnglish,
	SystemLanguageEstonian,
	SystemLanguageFaroese,
	SystemLanguageFinnish,
	SystemLanguageFrench,
	SystemLanguageGerman,
	SystemLanguageGreek,
	SystemLanguageHebrew,
	SystemLanguageHugarian,
	SystemLanguageIcelandic,
	SystemLanguageIndonesian,
	SystemLanguageItalian,
	SystemLanguageJapanese,
	SystemLanguageKorean,
	SystemLanguageLatvian,
	SystemLanguageLithuanian,
	SystemLanguageNorwegian,
	SystemLanguagePolish,
	SystemLanguagePortuguese,
	SystemLanguageRomanian,
	SystemLanguageRussian,
	SystemLanguageSerboCroatian,
	SystemLanguageSlovak,
	SystemLanguageSlovenian,
	SystemLanguageSpanish,
	SystemLanguageSwedish,
	SystemLanguageThai,
	SystemLanguageTurkish,
	SystemLanguageUkrainian,
	SystemLanguageVietnamese,
	SystemLanguageUnknown
};

namespace systeminfo {

	std::string GetOperatingSystem();
	std::string GetProcessorType();
	int EXPORT_COREMODULE GetProcessorCount();
	int GetNumberOfCores();
	int GetPhysicalMemoryMB();
	int GetUsedVirtualMemoryMB();
	int GetExecutableSizeMB();
	int GetSystemLanguage();
	std::string GetSystemLanguageISO();

#if UNITY_WIN
	ULONG_PTR GetCoreAffinityMask(DWORD core);
	std::string GetBIOSIdentifier();
#endif

#if UNITY_XENON
	void SetExecutableSizeMB(UInt32 sizeMB);
#endif

#if UNITY_WIN || UNITY_OSX || UNITY_LINUX
// Windows: 500=2000, 510=XP, 520=2003, 600=Vista
// Mac: 1006=Snow Leopard
// Linux: kernel version 206 = 2.6
int GetOperatingSystemNumeric();

int GetProcessorSpeed();
std::string GetMacAddress();
#endif

#if UNITY_EDITOR && (UNITY_WIN || UNITY_OSX)
unsigned char* GetMacAddressForBeast ();
#endif

#if WEBPLUG && UNITY_OSX
bool IsRunningInDashboardWidget();
#endif

#if UNITY_WP8
int GetCommitedMemoryLimitMB();
int GetCommitedMemoryMB();
#endif

inline RuntimePlatform GetRuntimePlatform()
{

	#if UNITY_EDITOR
		#if UNITY_OSX
		return OSXEditor;
		#elif UNITY_WIN
		return WindowsEditor;
		#elif UNITY_LINUX
		return LinuxEditor;
		#else
		#error Unknown platform
		#endif
	#else
		#if UNITY_OSX
			#if WEBPLUG
			#if !UNITY_PEPPER
			if (systeminfo::IsRunningInDashboardWidget ())
				return OSXDashboardPlayer;
			else
			#endif
				return OSXWebPlayer;
			#else
			return OSXPlayer;
			#endif
		#elif UNITY_WIN && !UNITY_WINRT
			#if WEBPLUG
			return WindowsWebPlayer;
			#else
			return WindowsPlayer;
			#endif
		#elif UNITY_WP8
			return WP8Player;
		#elif UNITY_METRO
			#if __arm__
				return MetroPlayerARM;
			#else
				return MetroPlayerX86;
			#endif
		#elif UNITY_WII
			return WiiPlayer;
		#elif UNITY_XENON
			return XenonPlayer;
		#elif UNITY_PS3
			return PS3Player;
		#elif UNITY_IPHONE
			return iPhonePlayer;
		#elif UNITY_ANDROID
			return AndroidPlayer;
		#elif UNITY_BB10
			return BB10Player;
		#elif UNITY_TIZEN
			return TizenPlayer;
		#elif UNITY_PEPPER
			#if UNITY_NACL_WEBPLAYER
				// Since we want to be fully compatible with existing content
				// we need to pretend we are a normal web player.
				return WindowsWebPlayer;
			#else
				return NaClWebPlayer;
			#endif
		#elif UNITY_LINUX
			#if WEBPLUG
				return LinuxWebPlayer;
			#else
				return LinuxPlayer;
			#endif
		#elif UNITY_FLASH
			return FlashPlayer;
		#elif UNITY_WEBGL
			return WebGLPlayer;
		#else
			#error Unknown platform
		#endif
	#endif
}

inline bool IsPlatformStandalone( RuntimePlatform p ) {
	return p==WindowsPlayer || p==OSXPlayer || p == LinuxPlayer;
}
inline bool IsPlatformWebPlayer( RuntimePlatform p ) {
	return p==WindowsWebPlayer || p==OSXWebPlayer || p==OSXDashboardPlayer;
}

inline std::string GetRuntimePlatformString(RuntimePlatform p)
{
	switch (p)
	{
	case OSXEditor:          return "OSXEditor";
	case OSXPlayer:          return "OSXPlayer";
	case WindowsPlayer:      return "WindowsPlayer";
	case OSXWebPlayer:       return "OSXWebPlayer";
	case OSXDashboardPlayer: return "OSXDashboardPlayer";
	case WindowsWebPlayer:   return "WindowsWebPlayer";
	case WiiPlayer:          return "WiiPlayer";
	case WindowsEditor:      return "WindowsEditor";
	case iPhonePlayer:       return "iPhonePlayer";
	case PS3Player:          return "PS3Player";
	case XenonPlayer:        return "XenonPlayer";
	case AndroidPlayer:      return "AndroidPlayer";
	case NaClWebPlayer:      return "NaClWebPlayer";
	case LinuxPlayer:        return "LinuxPlayer";
	case LinuxWebPlayer:     return "LinuxWebPlayer";
	case FlashPlayer:        return "FlashPlayer";
	case LinuxEditor:        return "LinuxEditor";
	case WebGLPlayer:        return "WebGL";
	case MetroPlayerX86:     return "MetroPlayerX86";
	case MetroPlayerX64:     return "MetroPlayerX64";
	case MetroPlayerARM:     return "MetroPlayerARM";
	case WP8Player:          return "WP8Player";
	case BB10Player:         return "BB10Player";
	case TizenPlayer:        return "TizenPlayer";
	default:
		#if !UNITY_EXTERNAL_TOOL
		AssertString("Unknown platform.");
		#endif
		return "Unknown";
	}
}

inline std::string GetRuntimePlatformString()
{
	return GetRuntimePlatformString(GetRuntimePlatform());
}

	std::string GetPersistentDataPath();		/// A path for data that can be considered "long-lived", possibly to time of un-installation.
	std::string GetTemporaryCachePath();		/// A path for "short-term" data, that may out-live the running session.
#if !UNITY_FLASH
	char const* GetDeviceUniqueIdentifier ();
	char const* GetDeviceName ();
	char const* GetDeviceModel ();
	char const* GetDeviceSystemName ();
	char const* GetDeviceSystemVersion ();
#endif

	inline bool IsHandheldPlatform ()
	{
#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_WP8 || UNITY_TIZEN
		return true;
#else
		return false;
#endif
	}

#if UNITY_WINRT
	bool SupportsAccelerometer ();
#else
	inline bool SupportsAccelerometer ()
	{
		return IsHandheldPlatform ();
	}
#endif

	inline bool SupportsLocationService ()
	{
		return IsHandheldPlatform ();
	}


#if UNITY_ANDROID || UNITY_IPHONE
	bool SupportsVibration ();
#else
	inline bool SupportsVibration ()
	{
		return IsHandheldPlatform ();
	}
#endif


	inline DeviceType DeviceType ()
	{
#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_WP8 || UNITY_TIZEN
		return kDeviceTypeHandheld;
#elif UNITY_EDITOR || UNITY_PEPPER || UNITY_FLASH \
      || UNITY_OSX || (UNITY_WIN && !UNITY_WP8) || UNITY_LINUX || UNITY_WEBGL
		return kDeviceTypeDesktop;
#elif UNITY_WII || UNITY_XENON || UNITY_PS3
		return kDeviceTypeConsole;
#else
		#error Should never get here. Add your platform.
		return kDeviceTypeUknown;
#endif
	}
} // namespace

#endif
