#include "UnityPrefix.h"
#include "Runtime/Misc/Player.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"

#include <ctype.h>

std::string GetAppDataPath()
{
#if UNITY_EDITOR
	return AppendPathName (File::GetCurrentDirectory (), "Assets");
#elif WEBPLUG || UNITY_PEPPER || UNITY_FLASH || UNITY_WEBGL
	std::string url = GetPlayerSettings().absoluteURL;
	size_t param = url.find('?');
	return DeleteLastPathNameComponent(url.substr(0, param));
#elif UNITY_WIN
	return SelectDataFolder ();
#elif UNITY_OSX
	return AppendPathName (GetApplicationPath (), "Contents");
#elif UNITY_WII
	return AppendPathName (GetApplicationFolder (), "/Data");
#elif UNITY_PS3
	return GetApplicationFolder();
#elif UNITY_XENON
	return "game:\\Media";
#elif UNITY_IPHONE
	return AppendPathName (GetApplicationFolder (), "Data");
#elif UNITY_ANDROID
	return GetApplicationPath();	// full path to the .apk
#elif UNITY_BB10
	return AppendPathName (GetApplicationFolder (), "app/native/Data");
#elif UNITY_TIZEN
	return AppendPathName (GetApplicationFolder (), "data");
#elif UNITY_LINUX
	return SelectDataFolder ();
#else
#error "Unknown platform"
#endif
}



#if SUPPORT_DIRECT_FILE_ACCESS

// We fucked this one up badly. In 3.4, this function would incorrectly strip 
// illegal characters from the file name, by changing the wrong indices.
// Now this a) does not solve the problem of illegal path names, and
// b) looks stupid. But, by fixing it, we'd lose all data cached by previous
// versions. So, what we do instead, is first check for the presence of
// a broken file name, and use that if it exists, and use the correct one otherwise.
// S.A. CachingManager.cpp.
void ConvertToLegalPathNameCorrectly(std::string& path)
{
	for (size_t i = path.size(); i > 0; --i)
	{
		char c = path[i-1];
		if (isalnum(c) || isspace(c))
			continue;
		path[i-1] = '_';
	}
}

void ConvertToLegalPathNameBroken(std::string& path)
{
	size_t size = path.size();
	for (size_t i = path.size(); i > 0; --i)
	{
		char c = path[i-1];
		if (isalnum(c) || isspace(c))
			continue;
		if (i < size)
			path[i] = '_';
	}
}

std::string GetApplicationSpecificDataPathAppendix(bool broken)
{
	if (UNITY_EMULATE_PERSISTENT_DATAPATH)
	{
		std::string companyName = GetPlayerSettings().companyName;
		std::string productName = GetPlayerSettings().productName;
		if (broken)
		{
			ConvertToLegalPathNameBroken(companyName);
			ConvertToLegalPathNameBroken(productName);
		}
		else
		{
#if UNITY_OSX && !UNITY_EDITOR
			// In the OS X standalone, return the bundle ID to match App Store requirements.
			return CFStringToString(CFBundleGetIdentifier(CFBundleGetMainBundle()));
#endif
			ConvertToLegalPathNameCorrectly(companyName);
			ConvertToLegalPathNameCorrectly(productName);
		}
		return AppendPathName(companyName, productName);
	}
	return "";
}

std::string GetPersistentDataPathApplicationSpecific()
{
	std::string dataPath = systeminfo::GetPersistentDataPath();
	#if !UNITY_WINRT
	if (dataPath.empty())
		return string();
	std::string brokenPath = AppendPathName(dataPath, GetApplicationSpecificDataPathAppendix(true)); 
	if (IsDirectoryCreated (brokenPath))
		dataPath = brokenPath;
	else
		dataPath = AppendPathName(dataPath, GetApplicationSpecificDataPathAppendix(false));
		
	if (!CreateDirectoryRecursive(dataPath))
		return string();
	#endif
	return dataPath;
}


std::string GetTemporaryCachePathApplicationSpecific()
{
	std::string cachePath = systeminfo::GetTemporaryCachePath();
	#if !UNITY_WINRT
	if (cachePath.empty())
		return string();
	std::string brokenPath = AppendPathName(cachePath, GetApplicationSpecificDataPathAppendix(true)); 
	if (IsDirectoryCreated (brokenPath))
		cachePath = brokenPath;
	else
		cachePath = AppendPathName(cachePath, GetApplicationSpecificDataPathAppendix(false));
	if (!CreateDirectoryRecursive(cachePath))
		return string();
	#endif
	return cachePath;
}

std::string GetStreamingAssetsPath()
{
#if (UNITY_EDITOR || UNITY_WIN || UNITY_WII || UNITY_LINUX) && !WEBPLUG
	return AppendPathName (GetAppDataPath(), "StreamingAssets");
#elif UNITY_OSX && !WEBPLUG
	return AppendPathName (SelectDataFolder(), "StreamingAssets");
#elif UNITY_IPHONE || UNITY_XENON || UNITY_PS3 || UNITY_BB10 || UNITY_TIZEN
	return AppendPathName (GetAppDataPath() , "Raw");
#elif UNITY_ANDROID
	return "jar:file://" + GetAppDataPath() + "!/assets";
#else
	ErrorString ("StreamingAssets is not available on this platform.");
	return "";
#endif
}

#else

std::string GetPersistentDataPathApplicationSpecific()
{
	return "";
}

std::string GetTemporaryCachePathApplicationSpecific()
{
	return "";
}

std::string GetStreamingAssetsPath()
{
	return "";
}
#endif
