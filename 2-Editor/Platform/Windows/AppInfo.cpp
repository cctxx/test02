#include "UnityPrefix.h"
#ifdef UNITY_WIN
#include "Editor/Platform/Interface/AppInfo.h"
#include "PlatformDependent/Win/Registry.h"
#include "PlatformDependent/Win/MiniVersion.h"
#include "Runtime/Utilities/PathNameUtility.h"

using std::string;

static std::string GetApplicationPathFromCommand( const std::string& command )
{
	std::string s = ToLower(command);
	if( s.empty() )
		return s;

	// if command starts with a quote, get contents up to next quote
	if( s[0] == '"' ) {
		size_t otherQuote = s.find('"', 1);
		if( otherQuote == string::npos )
			return ""; // something wrong with the command, return empty instead
		s = s.substr( 1, otherQuote-1 );
	}

	// try to strip everything after last ".exe", ".com", ".bat" or ".cmd"
	size_t pos;
	pos = s.rfind(".exe"); if( pos != string::npos ) s = s.substr( 0, pos+4 );
	pos = s.rfind(".com"); if( pos != string::npos ) s = s.substr( 0, pos+4 );
	pos = s.rfind(".bat"); if( pos != string::npos ) s = s.substr( 0, pos+4 );
	pos = s.rfind(".cmd"); if( pos != string::npos ) s = s.substr( 0, pos+4 );

	// now check if it actually exists
	if( GetFileAttributesA(s.c_str()) == INVALID_FILE_ATTRIBUTES )
		return "";

	return s;
}

std::vector<std::string> AppInfo::GetDefaultApps(const std::string& fileType)
{
	std::vector<std::string> keys;
	std::vector<std::string> values;
	std::vector<std::string> paths;

	// "Open With" list seems to be stored in:
	// HKCU\Software\Windows\CurrentVersion\Explorer\FileExts\.ext\OpenWithList and
	// HKCU\Software\Windows\CurrentVersion\Explorer\FileExts\.ext\OpenWithProgids

	// Get OpenWithList
	registry::getKeyValues( "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + fileType + "\\OpenWithList", keys, values );
	paths.reserve(keys.size());
	for( size_t i = 0; i < keys.size(); ++i ) {
		if( ToLower(keys[i]) == "mrulist" )
			continue;
		std::string cmd = registry::getString("Applications\\" + values[i] + "\\shell\\open\\command", "", "");
		std::string appPath = GetApplicationPathFromCommand(cmd);
		if( !appPath.empty() )
			paths.push_back(appPath);
	}

	// Get OpenWithProgids
	registry::getKeyValues( "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + fileType + "\\OpenWithProgids", keys, values );
	paths.reserve(paths.size() + keys.size());
	for( size_t i = 0; i < keys.size(); ++i ) {
		std::string place = registry::getString("Software\\Classes\\" + keys[i] + "\\shell", "", "open");
		std::string cmd = registry::getString("Software\\Classes\\" + keys[i] + "\\shell\\" + place + "\\command", "", "");
		std::string appPath = GetApplicationPathFromCommand(cmd);
		if( !appPath.empty() )
			paths.push_back(appPath);
	}
	
	return paths;
}

std::string AppInfo::GetAppFriendlyName(const std::string& appPath)
{
	std::wstring widePath;
	ConvertUTF8ToWideString(appPath, widePath);

	CMiniVersion version( widePath.c_str() );
	char description[200];
	bool ok = version.GetFileDescription( description, 200 );
	version.Release();
	if (ok)
		return description;
	
	std::string unityPath = appPath;
	ConvertSeparatorsToUnity(unityPath);
	return GetLastPathNameComponent(unityPath);
	//std::string str = registry::getString("SOFTWARE\\Classes\\AppID\\" + app, "", app);;
	//str = ToLower(str);
	//return str;
}


std::string AppInfo::GetDefaultAppPath(const std::string fileType)
{
	// The default application is stored in myriad of places:
	// HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.ext\Application
	std::string app = registry::getString( "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + fileType, "Application", "" );
	if( !app.empty() ) {
		std::string cmd = registry::getString("Applications\\" + app + "\\shell\\open\\command", "", "");
		std::string path = GetApplicationPathFromCommand(cmd);
		if( !path.empty() )
			return path;

	}
	// or HKCU\Software\Classes\.ext as filetype, then Software\Classes\filetype\shell\open\command
	std::string s = registry::getString("SOFTWARE\\Classes\\" + fileType, "", fileType + "file");
	s = registry::getString( "SOFTWARE\\Classes\\" + s + "\\shell\\open\\command", "", "" );
	return GetApplicationPathFromCommand(s);
}


#endif // UNITY_WIN
