#include "UnityPrefix.h"

#include "Runtime/Utilities/PlayerPrefs.h"

#if UNITY_PLUGIN
#if UNITY_OSX
#include "OSXWebPluginUtility.h"
#endif // #if UNITY_OSX
#include "PlatformDependent/CommonWebPlugin/WebPluginUtility.h"
#endif // #if UNITY_PLUGIN

#include "GlobalPreferences.h"


#if UNITY_WIN
#include "PlatformDependent/Win/Registry.h"
#if UNITY_EDITOR
const char* kRegistryPath = "Software\\Unity\\UnityEditor";
#elif UNITY_STANDALONE
const char* kRegistryPath = "Software\\Unity\\UnityStandalone";
#else
const char* kRegistryPath = "Software\\Unity\\WebPlayer";
#endif
#endif // #if UNITY_WIN

#if UNITY_OSX
#if UNITY_EDITOR
const char* kPrefsAppID = "com.unity3d.UnityEditor";
#elif UNITY_STANDALONE
const char* kPrefsAppID = "com.unity3d.UnityStandalone";
#else
const char* kPrefsAppID = "com.unity3d.UnityWebPlayer";
#endif
#endif // #if UNITY_OSX

#if UNITY_LINUX
#include "PlatformDependent/Linux/XmlOptions.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"

const char* kPrefsFileName = "global.prefs";
#endif // #if UNITY_LINUX


std::string GetGlobalPreference(const char *key)
{
	std::string result;

	#if UNITY_WIN && !UNITY_WINRT
	return registry::getString( kRegistryPath, key, "" );

	#elif UNITY_OSX
	CFStringRef cfPrefsAppID = CFStringCreateWithCString (NULL, kPrefsAppID, kCFStringEncodingASCII);
	CFStringRef cfKey = CFStringCreateWithCString (NULL, key, kCFStringEncodingASCII);
	CFStringRef val = (CFStringRef)CFPreferencesCopyAppValue (cfKey, cfPrefsAppID);
	if (val)
	{
		result = CFStringToString(val);
		CFRelease (val);
	}
	CFRelease (cfPrefsAppID);
	CFRelease (cfKey);

	#elif UNITY_LINUX
	std::string path = AppendPathName (GetUserConfigFolder (), kPrefsFileName);
	XmlOptions options;
	if (options.Load (path))
	{
		result = options.GetString (key, "");
	}

	#elif GAMERELEASE
	return PlayerPrefs::GetString (key);

	#else
	// TODO
	#endif
	return result;
}

bool GetGlobalBoolPreference(const char *key, bool defaultValue)
{
	std::string const pref = GetGlobalPreference (key);
	if (pref == "yes")
		return true;
	if (pref == "no")
		return false;
	return defaultValue;
}

void SetGlobalPreference(const char *key, std::string value)
{
	#if UNITY_WIN && !UNITY_WINRT
	registry::setString( kRegistryPath, key, value.c_str() );
	
	#elif UNITY_OSX
	CFStringRef cfPrefsAppID = CFStringCreateWithCString (NULL, kPrefsAppID, kCFStringEncodingASCII);
	CFStringRef cfKey = CFStringCreateWithCString (NULL, key, kCFStringEncodingASCII);
	CFStringRef cfValue = CFStringCreateWithCString (NULL, value.c_str(), kCFStringEncodingASCII);
	
	CFPreferencesSetAppValue( cfKey, cfValue, cfPrefsAppID );
	CFPreferencesAppSynchronize( cfPrefsAppID );

	#elif UNITY_LINUX
	std::string path = AppendPathName (GetUserConfigFolder (), kPrefsFileName);
	XmlOptions options;
	options.Load (path);
	options.SetString (key, value);
	options.Save (path);

	#elif GAMERELEASE
	PlayerPrefs::SetString (key, value);

	#else
	// TODO
	#endif
}

void SetGlobalBoolPreference(const char *key, bool value)
{
	SetGlobalPreference (key, value?"yes":"no");
}


#if WEBPLUG && !UNITY_PLUGIN
#include "Runtime/Misc/PlayerSettings.h"

std::string GetStrippedPlayerDomain ()
{
	std::string currentDomain = GetPlayerSettings().absoluteURL;
	if (currentDomain.find("http://") == 0 || currentDomain.find("https://") == 0)
	{
		//remove http://
		if (currentDomain.find("http://") == 0)
			currentDomain.erase(0, 7);
		else if (currentDomain.find("https://") == 0)
			currentDomain.erase(0, 8);
		
		//remove path
		std::string::size_type pos = currentDomain.find("/", 0);
		if (pos != std::string::npos)
			currentDomain.erase(currentDomain.begin() + pos, currentDomain.end());
		
		//remove port if present
		pos = currentDomain.find(":", 0);
		if (pos != std::string::npos)
			currentDomain.erase(currentDomain.begin() + pos, currentDomain.end());
	}
	return currentDomain;
}
#endif // #if WEBPLUG && !UNITY_PLUGIN
