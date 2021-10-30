#include "UnityPrefix.h"
#include "Editor/Platform/Interface/NativeMeshFormatConverter.h"
#import <Foundation/NSTask.h>
#import <Security/Authorization.h>
#include "Editor/Src/AssetPipeline/FBXImporter.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "PlatformDependent/OSX/NSStringConvert.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Platform/OSX/Utility/BundleUtilities.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Runtime/Input/TimeManager.h"
#include "../../../External/Cinema4DPlugin/builds/version.h"
#include <sstream>



static NSTask* gMayaTask = NULL;
static const char* kMayaCommandPipe = "Temp/Commandpipe";
static const char* kMayaSyncPipe = "Temp/Syncpipe";
static const char* kMayaSyncPipeKill = "Temp/SyncpipeKill";
#define POLL_NO_WARN
#include <poll.h>

#include <Cocoa/Cocoa.h>
#include <unistd.h>

/// To Convert a maya file into a fbx file we launch maya with a mel script that loops infinetely
/// Mel waits for a file kCommandPipe to contain the path to convert to an fbx file.
/// When it is complete it writes a file called kSyncPipe and kill the kCommandPipe file.
/// We continously check the kSyncPipe for existence and continue when it was created!

static const char* kExportedMayaFBXFile = "Temp/ExportedFBXFile.fbx";

static NSTask* LaunchMayaTask (const string& mayaPath, const string& mbFile);
//static NSTask* LaunchWatchdogTask (int pid);
static NSTask* AttachWatchDog (NSTask* watchdogTask, int pid);

//static string GetLaunchedC4DPath ();
static string GetHFSPath (const std::string& path);
static const char* kExportedC4DFBXFile = "Temp/ExportedC4DFBXFile.fbx";
const char* kC4DToFBXPlugin = "Contents/Tools/Unity-C4DToFBXConverter";
const char* kInstalledFBXPluginPath = "plugins/Unity-C4DToFBXConverter";
const char* kC4DCommandPipePath = "/private/tmp/%s-UnityC4DFBXcmd";
const char* kC4DSyncPipePath = "/private/tmp/%s-UnityC4DFBXout";

// This will kill the process pid if unity crashes or quits!
static NSTask* AttachWatchDog (NSTask* watchdogTask, int pid)
{
	if (watchdogTask)
	{
		if ([watchdogTask isRunning]) 
			[watchdogTask terminate];
		[watchdogTask release];
		watchdogTask = NULL;
	}
	watchdogTask = AttachWatchDog(pid);
	
	return watchdogTask;
}

inline bool PrepareAndCopyScript (const string& from, const string& dest, const string& mbFile, const string& mayaPath)
{
	// Generate a mel script		
	InputString melscript;
	ReadStringFromFile (&melscript, AppendPathName (GetApplicationPath (), from));
	if (melscript.empty ())
		return false;
	string scriptstring(melscript.c_str());
	replace_string (scriptstring, "!//UNITY_APP//!", GetApplicationPath ());
	replace_string (scriptstring, "!//UNITY_TEMP//!", PathToAbsolutePath ("Temp"));
	replace_string (scriptstring, "!//UNITY_MB_FILE//!", PathToAbsolutePath (mbFile));
	replace_string (scriptstring, "!//MAYA_PATH//!", mayaPath);
	CreateDirectory ("Temp");
	if (!WriteStringToFile (scriptstring, dest, kProjectTempFolder, kFileFlagTemporary|kFileFlagDontIndex))
		return false;
	
	return true;
}

static NSTask* LaunchMayaTask (const string& mayaPath, const string& mbFile)
{
	if (mayaPath.empty ())
		return NULL;
	
	// Generate mel scripts
	if (!PrepareAndCopyScript ("Contents/Tools/FBXMayaExport5.mel", "Temp/FBXMayaExport5.mel", mbFile, mayaPath) ||
	    !PrepareAndCopyScript ("Contents/Tools/FBXMayaExport.mel", "Temp/FBXMayaExport.mel", mbFile, mayaPath) ||
	    !PrepareAndCopyScript ("Contents/Tools/FBXMayaMain.mel", "Temp/FBXMayaMain.mel", mbFile, mayaPath))
	{
		printf_console("Generating mel scripts failed\n");
		return NULL;
	}
		
	// Setup maya location correctly for maya 6.0 and higher
	if (GetBundleVersion (mayaPath) >= VersionStringToNumeric ("6.0"))
		setenv ("MAYA_LOCATION", AppendPathName(mayaPath, "Contents").c_str(), 1);
	// maya 5.0 and lower has trouble with setting the maya location so unset it!
	else
		unsetenv ("MAYA_LOCATION");

 	// set up the decompression task using shell
	NSTask* launchMayaTask = [[NSTask alloc]init];
	[launchMayaTask setLaunchPath: MakeNSString (ResolveSymlinks (AppendPathName (mayaPath, "Contents/bin/maya")))];
	NSArray* args = [NSArray arrayWithObjects:  @"-batch", @"-nosplash", @"-script", MakeNSString (PathToAbsolutePath ("Temp/FBXMayaMain.mel")), nil];
	[launchMayaTask setArguments: args];
	[launchMayaTask setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
	[launchMayaTask setStandardOutput: [NSFileHandle fileHandleWithStandardOutput]];

	// launch synchrously
	NS_DURING
	[launchMayaTask launch];
	NS_HANDLER
	NS_VALUERETURN (NULL, NSTask*);
	NS_ENDHANDLER

	return launchMayaTask;
}


static const char* kMayaConversionFailed =
"Maya couldn't convert the mb file to an fbx file\n"
"Try exporting the mb file to an fbx file manually.\n"
"FBX plugins can be found here: http://www.autodesk.com/fbx\n";

string ConvertMayaToFBX (FBXImporter& importer, const string& mbFile, string* file, int attemptNumber)
{
	CreateDirectory ("Temp");
	DeleteFile (kMayaCommandPipe);
	DeleteFile (kMayaSyncPipe);
	DeleteFile (kMayaSyncPipeKill);
	DeleteFile (kExportedMayaFBXFile);
	
	string instructions = mbFile + "\n";
	if (importer.GetBakeIK())
		instructions += "bake\n";
	
	if (attemptNumber == 0)
	{
		if (importer.GetMeshSettings().normalImportMode == kTangentSpaceOptionsImport)
			instructions += "exportNormals\n";
		if (importer.GetMeshSettings().tangentImportMode == kTangentSpaceOptionsImport)
			instructions += "exportTangents\n";
	}
	
	if (!WriteStringToFile (instructions, kMayaCommandPipe, kProjectTempFolder, kFileFlagTemporary|kFileFlagDontIndex))
	{
		printf_console("Failed writing command pipe\n");
		return kMayaConversionFailed;
	}
	
	if (gMayaTask && ![gMayaTask isRunning])
	{
		[gMayaTask release];
		gMayaTask = NULL;
	}

	if (gMayaTask)
	{
 		[gMayaTask resume];
 	}
 	else
 	{
		string mayaPath = GetDefaultApplicationForFile (mbFile);
		/*
		if (!IsFileCreated(ResolveSymlinks (AppendPathName (mayaPath, "Contents/bin/maya"))))
		{
			
		}
		
		if (mayaPath.empty ())
			return "Maya could not be found.\nMake sure that maya is installed and the maya file has maya as its 'Open with' application!";
		
		if (mayaPath.empty())
		string mayaPath = GetDefaultApplicationForFile (mbFile);
		*/
		if (mayaPath.empty ())
			return "Maya could not be found.\nMake sure that maya is installed and the maya file has maya as its 'Open with' application!";

		static NSTask* gMayaWatchdogTask = NULL;	

		gMayaTask = LaunchMayaTask (mayaPath, mbFile);
		if (gMayaTask)
			gMayaWatchdogTask = AttachWatchDog (gMayaWatchdogTask, [gMayaTask processIdentifier]);
	}
	
	if (gMayaTask == NULL)
		return "Maya could not be launched.\nMake sure that maya is installed correctly and the maya file has maya as its 'Open with' application!";
		
	const double launchTime = GetTimeSinceStartup();
	const int kTimeOutMin = 3;
	bool timeOut = false;
	
 	while (true)
 	{
 		pollfd p;
		poll (&p, 0, 10);

 		if (IsFileCreated (kMayaSyncPipeKill) || IsFileCreated (kMayaSyncPipe))
 			break;
 		
 		if (![gMayaTask isRunning])
 		{
	 		if (IsFileCreated (kMayaSyncPipeKill) || IsFileCreated (kMayaSyncPipe))
 				break;

	
			printf_console("Maya quit prematurely\n");
 			[gMayaTask release];
 			gMayaTask = NULL;
 			return kMayaConversionFailed;
 		}
 		
 		if (GetTimeSinceStartup() - launchTime > kTimeOutMin * 60)
		{
			timeOut = true;
			break;
		}
 	}
 	
 	if (IsFileCreated (kMayaSyncPipeKill))
 		[gMayaTask terminate];
 	else
	 	[gMayaTask suspend];
 	
 	
	if (IsFileCreated (kExportedMayaFBXFile))
	{
		*file = kExportedMayaFBXFile;
		return string ();
	}
	else
	{
		std::ostringstream str;
		str << "Maya couldn't convert the mb file to an fbx file! ";
		if (timeOut)
			str << "It reached timeout of " << kTimeOutMin << " minutes.";
		return str.str();
	}
}

inline bool IsApplicationLaunched(string path)
{
	path = PathToAbsolutePath(path);
	
	NSArray* launchedApps = [[NSWorkspace sharedWorkspace]launchedApplications];
	for (int i=0;i<[launchedApps count];i++)
	{
		string curPath = MakeString([[launchedApps objectAtIndex: i]objectForKey:@"NSApplicationPath"]);
		if (curPath == path)
			return true;
	}
	return false;
}

static string GetHFSPath (const std::string& path)
{
	CFStringRef cfpath = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), CFStringGetSystemEncoding ());
	CFURLRef pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfpath, kCFURLPOSIXPathStyle, false);
	CFStringRef cfposixpath = CFURLCopyFileSystemPath(pathURL, kCFURLHFSPathStyle);
	
	char buffer[4096];
	CFStringGetCString(cfposixpath, buffer, 4096, kCFStringEncodingUTF8);
	CFRelease (cfpath);
	CFRelease (cfposixpath);
	CFRelease(pathURL);
	
	return buffer;
}

// Reusable C4D process used for exporting in non-background mode
static NSTask* sReusableC4DTask = NULL;
static NSTask* sLastC4DBackgroundTask = NULL;

static NSTask* LaunchCinema4DTask (const string& ciname4DPath, bool useBackgroundMode)
{
	if (ciname4DPath.empty ())
		return NULL;
	
	if (sLastC4DBackgroundTask != NULL)
	{
		[sLastC4DBackgroundTask terminate];
		[sLastC4DBackgroundTask release];
		sLastC4DBackgroundTask = NULL;
	}
	
	if (!useBackgroundMode && sReusableC4DTask)
	{
		if ([sReusableC4DTask isRunning])
			return sReusableC4DTask;
		else
		{
			[sReusableC4DTask release];
			sReusableC4DTask = NULL;
		}
	}
	
	NSTask* task = [[NSTask alloc]init];
	[task setLaunchPath: MakeNSString (ResolveSymlinks (AppendPathName (ciname4DPath, "Contents/MacOS/Cinema 4D")))];
	if (useBackgroundMode)
	{
		NSArray* args = [NSArray arrayWithObjects:  @"-nogui", nil];
		[task setArguments: args];
	}
	[task setStandardInput: [NSFileHandle fileHandleWithStandardInput]];
	[task setStandardOutput: [NSFileHandle fileHandleWithStandardOutput]];
	
	NS_DURING
	[task launch];
	NS_HANDLER
	NS_VALUERETURN (NULL, NSTask*);
	NS_ENDHANDLER
	
	static NSTask* gC4DWatchdogTask = NULL;	
	gC4DWatchdogTask = AttachWatchDog (gC4DWatchdogTask, [task processIdentifier]);
	
	if (!useBackgroundMode)
		sReusableC4DTask = task;
	
	return task;
}

static string GetLaunchedC4DPath ()
{
	// Find in open applications list
	string cinema4DPath;
	NSArray* launchedApps = [[NSWorkspace sharedWorkspace]launchedApplications];
	for (int i=0;i<[launchedApps count];i++)
	{
		string appName = MakeString([[launchedApps objectAtIndex: i]objectForKey: @"NSApplicationName"]);
		if (appName.find("CINEMA 4D") == 0)
		{
			cinema4DPath = MakeString([[launchedApps objectAtIndex: i]objectForKey:@"NSApplicationPath"]);
			break;
		}
	}
	return cinema4DPath;
}

namespace
{
	std::string InstallationError(const std::string& details, OSStatus status)
	{
		std::string res = std::string() + "Cinema 4D converter plugin installation failed - " + details;
		if (errAuthorizationSuccess != status)
			res += " (error code: " + IntToString(status) + ")";
		printf_console((res + "\n").c_str());
		return res;
	}
}
	
std::string ParseOutputLine(const std::string& resultString, const std::string& findLine)
{
	std::string res;

	std::string::size_type pos = resultString.find(findLine);
	if (pos != string::npos)
	{
		pos += findLine.size();

		std::string::size_type pos2 = resultString.find("\r\n", pos);
		if (pos2 == std::string::npos)
		{
			pos2 = resultString.find("\r", pos);
			if (pos2 == std::string::npos)
				pos2 = resultString.size();
		}

		res = resultString.substr(pos, pos2 - pos);
	}

	return res;
}

string ConvertC4DToFBX (FBXImporter& importer, const string& sourceFile, string* file)
{
	// Cleanup export folder and command files
	const char* uid = getenv("USER");
	string commandPipePath = Format(kC4DCommandPipePath, uid);
	string syncPipePath = Format(kC4DSyncPipePath, uid);

	CreateDirectory ("Temp");
	DeleteFile (kExportedC4DFBXFile);
	DeleteFile (commandPipePath);
	DeleteFile (syncPipePath);

	// Find in open applications list
	string cinema4DPath = GetLaunchedC4DPath();

	// Find through default application for file
	if (cinema4DPath.empty())
	{
		// Check if Cinema 4D is installed!
		cinema4DPath = GetDefaultApplicationForFile (sourceFile);
		if (cinema4DPath.empty ())
			return "Cinema 4D could not be found.\nMake sure that Cinema 4D is installed and the .C4D file has Cinema 4D as its 'Open with' application!";
		
		// Protect against using the net render / server version	
		if (GetLastPathNameComponent(cinema4DPath) == "C4D Client" || GetLastPathNameComponent(cinema4DPath) == "C4D Server")
		{
			cinema4DPath = AppendPathName(DeleteLastPathNameComponent (cinema4DPath), "CINEMA 4D");
			if (!IsPathCreated(cinema4DPath))
				return "Cinema 4D could not be found.\nMake sure that Cinema 4D is installed and the .C4D file has Cinema 4D as its 'Open with' application!";
		}
	}
	
	bool useBackgroundMode = false;
	{
		// Detecting if it should use background mode - only plugin for R11 is designed to work in background mode
		// This is a bit hackish way to check that - get the number after "R" in cinema4DPath
		const std::string searchString = "cinema 4d r";
		std::string::size_type pos = ToLower(cinema4DPath).find(searchString);
		if (std::string::npos != pos)
		{
			std::string versionStr;
			pos += searchString.size();
			for (; pos < cinema4DPath.size(); ++pos)
			{
				const char c = cinema4DPath[pos];
				if (c < '0' || c > '9')
					break;
				
				versionStr += c;
			}
			
			if (versionStr.size() > 0 && StringToInt(versionStr) >= 11)
				useBackgroundMode = true;
		}
	}
	

	// Write command pipe file
	string commands;
	commands =  "src=" + GetHFSPath(PathToAbsolutePath(sourceFile)) + "\n";
	commands += "dst=" + GetHFSPath(PathToAbsolutePath(kExportedC4DFBXFile)) + "\n";
	commands += "textureSearchPath=" + GetHFSPath(PathToAbsolutePath("Assets")) + "\n";
	commands += "src10=" + PathToAbsolutePath(sourceFile) + "\n";
	commands += "dst10=" + PathToAbsolutePath(kExportedC4DFBXFile) + "\n";
	commands += "textureSearchPath10=" + PathToAbsolutePath("Assets") + "\n";
	
	if (importer.GetBakeIK())
	{
		commands += Format("ikfrom=0\n");
		commands += Format("ikto=30\n");
	}
	
	if (!WriteStringToFile(commands, commandPipePath, kSystemTempFolder, kFileFlagTemporary|kFileFlagDontIndex))
	{
		return "Couldn't convert .C4D file to fbx file because you don't have write access to the tmp folder..\n"
	           "Try exporting the .C4D file to an fbx file manually.\n";
	}
		
	// Auto-install C4D plugin
	bool requiresInstall = false;
	string installedPluginPath = AppendPathName(DeleteLastPathNameComponent(cinema4DPath), kInstalledFBXPluginPath);

	// TODO : share code with windows
		
	// Make sure the installed plugin is the same version as plugin carried by this Unity
	const char* const kVersionFileName = "Version";
	const char* const kExpectedVersion = "Plugin version: " C4DTOFBX_PLUGIN_VERSION ";";
	
	InputString version;
	ReadStringFromFile(&version, AppendPathName(installedPluginPath, kVersionFileName));
	requiresInstall = !IsDirectoryCreated(installedPluginPath) || version != kExpectedVersion;
	
	// Quit cinema 4D before installing!
	if (requiresInstall)
	{
		// Cinema 4d is already launched
		if (IsApplicationLaunched(cinema4DPath))
		{
			// Make sure it is quit
			while (IsApplicationLaunched(cinema4DPath))
			{
				bool ok = DisplayDialog ("Please quit Cinema 4D", "Unity needs to install the Cinema 4D -> Unity converter plugin. When you have quit Cinema 4D, hit Ok.", "Ok", "Cancel");
				if (!ok)
					return "Cinema 4D import was cancelled. Please make sure to relaunch Cinema 4D.";
			}
		}
	}

	// Install plugin
	if (requiresInstall)
	{
		printf_console("\nInstalling Unity Cinema 4D Converter Plugin\n");
		DeleteFileOrDirectory(installedPluginPath);
		std::string sourcePluginPath = AppendPathName(GetApplicationPath(), kC4DToFBXPlugin);
		if (CopyFileOrDirectory (sourcePluginPath, installedPluginPath))
		{
			/*
			// Cinema 4D 10 has some issues if we keep older libraries in the plugins folder.
			if (BeginsWith(GetBundleVersionForApplication(cinema4DPath), "10."))
			{
				printf_console("Cinema 4D 10 detected. Removing C4D 9.x plugins.\n");
				DeleteFile(AppendPathName(installedPluginPath, "Unity-C4DToFBXConverter_8.5.xdl"));
				DeleteFile(AppendPathName(installedPluginPath, "Unity-C4DToFBXConverter_9.5UB.dylib"));
				DeleteFile(AppendPathName(installedPluginPath, "Unity-C4DToFBXConverter_9.x.xdl"));
			}
			*/
			
			WriteStringToFile(kExpectedVersion, AppendPathName(installedPluginPath, kVersionFileName), kNotAtomic, 0);
		}
		else
		{
			// if it fails to copy, we assume that it is authorization problems,
			// so we try to do all operations with authorization
			AuthorizationRef authorization;
			// TODO : free authorization on return
			
			// TODO : there is some code for creating authorizations already - maybe use that?
			OSStatus status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorization);
			if (errAuthorizationSuccess != status)
			{
				return InstallationError("failed to create authorization", status);
			}
			
			if (IsDirectoryCreated(installedPluginPath))
			{
				const char *args[] = { "-fdr", installedPluginPath.c_str(), NULL };
				status = AuthorizationExecuteWithPrivileges(authorization, "/bin/rm", kAuthorizationFlagDefaults, const_cast<char**> (args), NULL);
				if (errAuthorizationSuccess != status)
				{
					return InstallationError("failed to delete plugin directory", status);
				}
			}

			const char *args[] = { "-pr", sourcePluginPath.c_str(), installedPluginPath.c_str(), NULL };
			status = AuthorizationExecuteWithPrivileges(authorization, "/bin/cp", kAuthorizationFlagDefaults, const_cast<char**> (args), NULL);
			if (errAuthorizationSuccess != status)
			{
				return InstallationError("failed to copy plugin", status);
			}
			
			// we create version file localy and then move to Cinema4D path
			std::string unityVersionTempFile = AppendPathName(GetApplicationFolder(), kVersionFileName);
			if (!WriteStringToFile(kExpectedVersion, unityVersionTempFile, kNotAtomic, 0))
			{
				return InstallationError("failed to create version file", errAuthorizationSuccess);
			}
				
			std::string unityVersionFile = AppendPathName(installedPluginPath, kVersionFileName);
			char *argsMv[] = { (char*)unityVersionTempFile.c_str(), (char*)unityVersionFile.c_str(), NULL };
				
			status = AuthorizationExecuteWithPrivileges(authorization, "/bin/mv", kAuthorizationFlagDefaults, argsMv, NULL);
			if (errAuthorizationSuccess != status)
			{
				return InstallationError("failed to copy version file", status);
			}
			
			status = AuthorizationFree(authorization, kAuthorizationFlagDefaults);
		}
			
		printf_console("Installation succeed\n");
	}
	
	// Launch Cinema 4D
	NSTask* C4DTask = LaunchCinema4DTask (cinema4DPath, useBackgroundMode);
	if (!C4DTask)
		return "Cinema 4D could not be launched and perform the conversion.\nMake sure that Cinema 4D is installed and the .C4D file has Cinema 4D as its 'Open with' application!";

	double launchTime = GetTimeSinceStartup();
	bool pluginNotInstalled = false;
	bool conversionFailed = false;
	
 	while (true)
 	{
 		pollfd p;
		poll (&p, 0, 10);
		
		if (IsFileCreated (syncPipePath))
			break;
		
		if (GetTimeSinceStartup() - launchTime > 90)
		{
			if (IsFileCreated (commandPipePath))
			{
				pluginNotInstalled = true;
				break;
			}
		}

		if (GetTimeSinceStartup() - launchTime > 90)
		{
			if (!IsFileCreated (syncPipePath))
			{
				conversionFailed = true;
				break;
			}
		}
		
		if (![C4DTask isRunning])
		{
			break;
		}
 	}
	
	if (useBackgroundMode)
	{
		if ((pluginNotInstalled || conversionFailed) && [C4DTask isRunning])
		{
			// if conversion failed and task is still running we choose to terminate it
			// because it's probably hanging due to some reason - for example failed to connect to license server
			[C4DTask terminate];
			[C4DTask release];
		}
		else 
		{
			Assert(sLastC4DBackgroundTask == NULL);
			// Storing C4DTask - we will make sure it's terminated next time
			// We could actually kill it instantly, but we want C4D to terminate properly (if it can until we need to lounch C4D again)
			sLastC4DBackgroundTask = C4DTask;
		}
	}

	// No conversion took place	
	if (!IsFileCreated (syncPipePath))
	{
		if (IsFileCreated (kExportedC4DFBXFile))
		{
			printf_console ("FBX FILE exists but sync pipe doesn't!!!\n");	
		}
		
		InputString tempRES;
		ReadStringFromFile(&tempRES, syncPipePath);
		
		printf_console ("SYNC PIPE NOT FOUND EXTRACT: '%s'\n", tempRES.c_str());
		printf_console ("SYNC PIPE PATH: '%s'\n", PathToAbsolutePath(syncPipePath).c_str());
	
		return "Cinema 4D couldn't convert the .C4D file to an fbx file.\n"
	       "Try exporting the .C4D file to an fbx file manually.\n"
	       "You need at last Cinema 4D 8.5 to import .C4D files directly.\n";
	}

	// The file should be written atomically
	// - it is not so lets have multiple tries!
	InputString resultString;
	for (int i=0;i<100;i++)
	{
		pollfd p;
		poll (&p, 0, 100);
		
		ReadStringFromFile(&resultString, syncPipePath);
		if (!resultString.empty())
			break;
	}
	
	// An error occurred!
	if (!BeginsWith(resultString.c_str(), "SUCCESS"))
	{
		printf_console ("Got fbx converter error: '%s'", resultString.c_str());
	
		return "Cinema 4D couldn't convert the .C4D file to an fbx file.\n"
	       "Try exporting the .C4D file to an fbx file manually.\n" +
		std::string(resultString.c_str());
	}
	
	{
		std::string framerate = ParseOutputLine(resultString.c_str(), "framerate=");
		if (!framerate.empty())
			importer.SetOverrideSampleRate(StringToInt(framerate));
	}

	/*std::string cinema4DVersion = ParseOutputLine(resultString, "version=");
	if (!cinema4DVersion.empty())
	{
		// Cinema 4D version is written in 5 digits (for example 13012), so
		// we reformat it into xx.x
		cinema4DVersion = std::string("R") + cinema4DVersion.substr(0, cinema4DVersion.size() - 3) + "." + cinema4DVersion.substr(2, 1);
	}*/

	*file = kExportedC4DFBXFile;
	return "";
}
