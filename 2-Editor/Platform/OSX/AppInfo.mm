#include "UnityPrefix.h"
#ifdef UNITY_OSX // for mac
#include "Editor/Platform/Interface/AppInfo.h"
#include <Cocoa/Cocoa.h>


std::vector<std::string> AppInfo::GetDefaultApps(const std::string& fileType)
{
	std::vector<std::string> paths;
	
	/*std::string path = fileType + "\\OpenWithList";
	paths = registry::getKeyNames( path );*/
	
	return paths;
}


NSString*  keys[] = { @"CFBundleDisplayName", @"CFBundleName", NULL };
std::string AppInfo::GetAppFriendlyName(const std::string& App)
{
	NSString* fullPath = [NSString stringWithUTF8String: App.c_str ()];	
	
	NSBundle *bundle = [NSBundle bundleWithPath: fullPath];
	NSDictionary *info = [bundle infoDictionary]; 
	
	/*NSEnumerator* enumerator = [info keyEnumerator];
	NSString *next = (NSString*) [enumerator nextObject];
	
	while (next)		
	{
		char mad[255];
		sprintf(mad,"%s", [next UTF8String]);
		//printf_console(mad);
		next = (NSString*) [enumerator nextObject];
	}*/
	
	
	NSString* entry = NULL;
	int i = 0;
	while (!entry)
	{
		NSString* key = keys[i];
		
		if (!key)
			return App;
		
		entry = [ info objectForKey: key ];
		i++;
	}
	
	return ::std::string([entry UTF8String]);	
	
	
	
}


std::string AppInfo::GetDefaultAppPath(const std::string fileType)
{
	std::string s = ""; 
	
	/*registry::getString("SOFTWARE\\Classes\\" + fileType, "", fileType + "file");
	s = registry::getString("SOFTWARE\\Classes\\" + s + "\\shell\\open\\command","",s);
	// clean the string
	s = s.substr(0, s.find_first_of(".") + 4);*/
	
	return s;
}


#endif // UNITY_OSX
