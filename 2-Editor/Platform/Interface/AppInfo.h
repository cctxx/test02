/**
* Application info
* Get default application info, paths etc
*
**/
#ifndef __UNITY_APP_INFO_H__
#define __UNITY_APP_INFO_H__

#include <string>

class AppInfo
{
public:
	static std::vector<std::string> GetDefaultApps(const std::string& fileType); 
	static std::string GetAppFriendlyName(const std::string& app);
	static std::string GetAppPath(const std::string& appPath);
	static std::string GetDefaultAppPath(const std::string fileType);
};

#endif // __UNITY_APP_INFO_H__
