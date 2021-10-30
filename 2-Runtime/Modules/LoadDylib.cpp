#include "UnityPrefix.h"
#include "LoadDylib.h"
#include <map>

#if UNITY_OSX || UNITY_LINUX
#include "dlfcn.h"
#endif

#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/MetroUtils.h"
#endif

using namespace std;

map<string, void*> gLoaded;


std::string GetPathWithPlatformSpecificDllExtension(const std::string& path)
{
	string extended = path;

	#if UNITY_OSX
		extended += ".dylib";
	#elif UNITY_WIN
		extended += ".dll";
	#elif UNITY_LINUX
		extended += ".so";
	#else
		ErrorString ("Dynamic module loading not implemented");
	#endif

	return extended;
}


void* LoadDynamicLibrary (const std::string& absolutePath)
{
	void* library = NULL;

	// load library if needed
	if (gLoaded.find(absolutePath) != gLoaded.end())
	{
		library = gLoaded[absolutePath];
	}
	else
	{
		#if UNITY_OSX || UNITY_LINUX
			/// RTLD_LOCAL is supposedly default and also completely fails on 10.3.9
			library = dlopen (absolutePath.c_str (), RTLD_NOW);
			if ( !library )
				ErrorStringMsg( "ERROR: LoadDynamicLibrary(): %s\n", dlerror() );

		#elif UNITY_WIN

			#if UNITY_WINRT
				HMODULE module = LoadPackagedLibrary (ConvertToWindowsPath(absolutePath.c_str())->Data(), 0);
			#else
				std::wstring widePath;
				ConvertUnityPathName( absolutePath, widePath );
				HINSTANCE module = LoadLibraryW( widePath.c_str() );
			#endif
			library = module; 

		#endif

			if (library)
			{
				gLoaded[absolutePath] = library;
			}
	}

	return library;
}


void* LoadAndLookupSymbol (const std::string& absolutePath, const std::string& name)
{
	void* library = LoadDynamicLibrary(absolutePath);
	void* symbol = LookupSymbol(library, name);
	return symbol;
}


void UnloadDynamicLibrary (void* libraryReference)
{
	map<string, void*>::iterator it;
	for (it = gLoaded.begin(); it != gLoaded.end(); ++it)
	{
		if (it->second == libraryReference)
		{
			#if UNITY_OSX || UNITY_LINUX
			dlclose (it->second);
			#elif UNITY_WIN
			FreeLibrary( (HMODULE)it->second );
			#endif

			gLoaded.erase(it);
			break;
		}
	}
}


void UnloadDynamicLibrary (const std::string& absolutePath)
{
	if (gLoaded.count (absolutePath) && gLoaded[absolutePath])
	{
		#if UNITY_OSX || UNITY_LINUX
		dlclose (gLoaded[absolutePath]);
		#elif UNITY_WIN
		FreeLibrary( (HMODULE)gLoaded[absolutePath] );
		#endif
	}
	gLoaded.erase (absolutePath);
}


bool LoadAndLookupSymbols (const char* path, ...)
{
	va_list ap;
	va_start (ap, path);
	while (true)
	{
		const char* symbolName = va_arg (ap, const char*);
		if (symbolName == NULL)
			return true;
		
		void** functionHandle = va_arg (ap, void**);
		AssertIf(functionHandle == NULL);
		
		*functionHandle = LoadAndLookupSymbol (path, symbolName);
		if (*functionHandle == NULL)
			return false;
	}

	return false;
}

void* LookupSymbol(void* libraryReference, const std::string& symbolName)
{
#if UNITY_OSX || UNITY_LINUX

	void *sym = dlsym (libraryReference, symbolName.c_str ());
	if (!sym)
		ErrorStringMsg("Could not load symbol %s : %s\n", symbolName.c_str(), dlerror());
	return sym;
	
#elif UNITY_WIN

	if( !libraryReference )
		return NULL;
	return GetProcAddress( (HMODULE)libraryReference, symbolName.c_str() );

#else
	ErrorStringMsg("LoadAndLookupSymbol is not supported on this platform.\n");
	return NULL;
#endif
}

/*

#include <Carbon/Carbon.h>
#import <mach-o/dyld.h>
#import <stdlib.h>
#import <string.h>
#include "FileUtilities.h"

using namespace std;

CFBundleRef LoadDynamicLibrary (const string& absolutePath)
{
	CFStringRef cfstring = CFStringCreateWithCString (NULL, ResolveSymlinks(absolutePath).c_str (), kCFStringEncodingUTF8);

	CFURLRef url = CFURLCreateWithFileSystemPath(
                kCFAllocatorDefault, 
                cfstring,
                kCFURLPOSIXPathStyle,
                false);
//	CFURLRef url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8*)absolutePath.c_str (), absolutePath.size (), false);

    CFBundleRef bundle = CFBundleCreate (NULL, url);
    CFRelease (url);
    CFRelease (cfstring);
    if (!bundle)
    	return false;
	
	// Check if already loaded    
    if (CFBundleIsExecutableLoaded (bundle))
    	return bundle;
	
    // Load the bundle
    if (!CFBundleLoadExecutable (bundle))
        return NULL;
	
	return bundle;
}

void UnloadDynamicLibrary (const string& absolutePath)
{
//	CFURLRef url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8*)absolutePath.c_str (), absolutePath.size (), false);
	CFStringRef cfstring = CFStringCreateWithCString (kCFAllocatorDefault, absolutePath.c_str (), kCFStringEncodingUTF8);

	CFURLRef url = CFURLCreateWithFileSystemPath(
                kCFAllocatorDefault, 
                cfstring,
                kCFURLPOSIXPathStyle,
                true);

    CFBundleRef bundle = CFBundleCreate (kCFAllocatorDefault, url);
    CFRelease (url);
    CFRelease (cfstring);
    if (!bundle)
    	return;
    
    CFBundleUnloadExecutable (bundle);
}

void* LoadAndLookupSymbol (const std::string& absolutePath, const std::string& name)
{
	CFBundleRef bundle = LoadDynamicLibrary (absolutePath);
	CFStringRef cfstring = CFStringCreateWithCString (kCFAllocatorDefault, name.c_str (), kCFStringEncodingUTF8);
	if (bundle == NULL)
		return NULL;
		
	return CFBundleGetFunctionPointerForName(bundle, cfstring);
}*/
