#include "UnityPrefix.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Editor/Platform/Interface/NativeMeshFormatConverter.h"
#include "Editor/Src/AssetPipeline/FBXImporter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Runtime/Misc/SystemInfo.h"
#include "shlobj.h"
#include "Runtime/Input/TimeManager.h"
#include "../../../External/Cinema4DPlugin/builds/version.h"
#include <sstream>

#define DEBUG_MAX 0

using namespace std;

//std::string ConvertC4DToFBX (FBXImporter& importer, const std::string& sourceFile, std::string* file) { return ""; }


/// To Convert a Maya/3dsMax file into a fbx file we launch maya/max with a script that loops infinitely
/// Maya/3dsMax waits for a file kCommandPipe to contain the path to convert to an fbx file.
/// When it is complete it writes a file called kSyncPipe and kill the kCommandPipe file.
/// We continuously check the kSyncPipe for existence and continue when it was created!

static PROCESS_INFORMATION* g_MayaProcess = NULL;
static PROCESS_INFORMATION* g_MayaWatchDogProcess = NULL;
static const char* kMayaCommandPipe = "Temp/Commandpipe";
static const char* kMayaSyncPipe = "Temp/Syncpipe";
static const char* kMayaSyncPipeKill = "Temp/SyncpipeKill";
static const char* kMayaExportedFBXFile = "Temp/ExportedFBXFile.fbx";

static PROCESS_INFORMATION* g_MaxProcess = NULL;
static PROCESS_INFORMATION* g_MaxWatchDogProcess = NULL;
static const char* kMaxCommandPipe = "Temp/MaxCommandPipe";
static const char* kMaxSyncPipeKill = "Temp/MaxSyncPipeKill";
static const char* kMaxExportedFBXFile = "Temp/MaxExportedFBXFile.fbx";

static PROCESS_INFORMATION* g_C4DProcess = NULL;
static PROCESS_INFORMATION* g_C4DWatchDogProcess = NULL;
static const char* kExportedC4DFBXFile = "Temp/ExportedC4DFBXFile.fbx";
const char* kC4DToFBXPlugin = "Data/Tools/UNITY-C4DToFBXConverter";
const char* kInstalledFBXPluginPath = "plugins/Unity-C4DToFBXConverter";
const char* kC4DCommandPipePath = "%s%s-UnityC4DFBXcmd";
const char* kC4DSyncPipePath = "%s%s-UnityC4DFBXout";

// This will kill the process pid if unity crashes or quits!
static PROCESS_INFORMATION* AttachWatchDog( PROCESS_INFORMATION* procInfo, int pid )
{
	if ( procInfo != NULL)
	{
		// TODO : this doesn't look like good idea - if we still have old WatchDog running it means that 
		// the application (that it is watching) is still running (and it might be hanging), so
		// maybe we need to terminate it?
		if (IsProcessRunning( procInfo->dwProcessId ))
			TerminateProcess( procInfo->hProcess, 0);

		delete procInfo;
		procInfo = NULL;
	}

	procInfo = new PROCESS_INFORMATION();

	if (procInfo && !AttachWatchDog(*procInfo, pid))
	{
		delete procInfo;
		procInfo = NULL;
	}

	return procInfo;
}

inline bool PrepareAndCopyScript (const string& from, const string& dest, const string& modelFile, const string& appPath)
{
	// Generate a mel/max script
	InputString readscript;
	string path = GetApplicationPath();

	path = path.substr( 0, path.find_last_of( kPathNameSeparator ) );
	ReadStringFromFile( &readscript, AppendPathName( path, from ) );
	if ( readscript.empty() )
		return false;
	string script = string(readscript.c_str(), readscript.size());
	replace_string( script, "!//UNITY_APP//!", ToShortPathName(GetApplicationPath()) );
	replace_string( script, "!//UNITY_TEMP//!", ToShortPathName(PathToAbsolutePath( "Temp" )) );
	replace_string( script, "!//UNITY_MB_FILE//!", ToShortPathName(PathToAbsolutePath( modelFile )) ); // deprecated
	replace_string( script, "!//UNITY_MODEL_FILE//!", ToShortPathName(PathToAbsolutePath( modelFile )) );
	replace_string( script, "!//MAYA_PATH//!", ToShortPathName(PathToAbsolutePath( appPath.substr(0, appPath.find_last_of( kPathNameSeparator ) ) )) ); // deprecated
 	replace_string( script, "!//DCC_APP_PATH//!", ToShortPathName(PathToAbsolutePath( appPath.substr(0, appPath.find_last_of( kPathNameSeparator ) )) ) );

	// This one is special. It needs to be passed to windows command line, and that one does not understand mac PathNameSeparator
	string str = ToShortPathName(PathToAbsolutePath ("Temp"));

	ConvertSeparatorsToWindows(str);
	replace_string(str, "\\", "\\\\");// escape string 

	replace_string (script, "!//UNITY_TEMP_WIN//!", str);
	
	CreateDirectory ("Temp");
	if (!WriteStringToFile (script, dest, kProjectTempFolder, kFileFlagDontIndex | kFileFlagTemporary))
		return false;

	return true;
}

static int MajorVersionCompare(string v1, string v2)
{
	int v1i = atoi(v1.substr(0, v1.find_first_of('.')).c_str());
	int v2i = atoi(v2.substr(0, v2.find_first_of('.')).c_str());

	if (v1i > v2i)
		return 1;
	if (v1i < v2i)
		return -1;

	return 0;
}

static bool LaunchMayaTask (const string& mayaPath, const string& mbFile)
{
	if (mayaPath.empty ())
		return false;

	// Generate mel scripts
	if (!PrepareAndCopyScript ("Data/Tools/FBXMayaExport5.mel", "Temp/FBXMayaExport5.mel", mbFile, mayaPath) ||
		!PrepareAndCopyScript ("Data/Tools/FBXMayaExport.mel", "Temp/FBXMayaExport.mel", mbFile, mayaPath) ||
		!PrepareAndCopyScript ("Data/Tools/FBXMayaMain.mel", "Temp/FBXMayaMain.mel", mbFile, mayaPath))
	{
		printf_console("Generating mel scripts failed\n");
		return false;
	}

	// Setup maya location correctly for maya 6.0 and higher
	if (MajorVersionCompare(GetBundleVersionForApplication(mayaPath), "6.0") >= 0)
	{
		string env = "MAYA_LOCATION=" + mayaPath;
		ConvertSeparatorsToWindows(env);
		_putenv (env.c_str());
	}
	// maya 5.0 and lower has trouble with setting the maya location so unset it!
	//else
	//	unsetenv ("MAYA_LOCATION"); // FIXME: why unset something that was never set? (couldn't quickly find function for this on windows)


	vector<string> args;

	string scriptpath = PathToAbsolutePath ("Temp/FBXMayaMain.mel");
	ConvertSeparatorsToWindows(scriptpath);

	args.push_back("-batch");
	args.push_back("-nosplash");
	args.push_back("-script");
	args.push_back(scriptpath);
	args.push_back("-prompt");

	//args.push_back("-log");
	//args.push_back("c:\\maya.log");

	static PROCESS_INFORMATION smayaproc;

	if (!LaunchTaskArrayOptions(mayaPath, args, kLaunchQuoteArgs|kLaunchBackground, mayaPath.substr(0, mayaPath.find_last_of(kPathNameSeparator)), smayaproc))
	{
		g_MayaProcess = NULL;
		return false;
	}else
	{
		g_MayaProcess = &smayaproc;
		return true;
	}
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
	DeleteFile (kMayaExportedFBXFile);

	// AE 04/03/2009: Maya's 'file' command supports short path names (e.g. C:/NEWFOL~1) for directories, but not for 
	// filenames in Maya 2008 (fixed in Maya 2009). The command supports long filenames, but no unicode/UTF8 (e.g. arabic) 
	// if the region is set to English (United States). If the region is set to English (US) and extended characters (^2)
	// are use in the filename, then it requires extended characters to be in ANSI1252. 
	// 
	// Potential fix for English (US) locale: mbFile = ConvertUnityToDefaultAnsi( mbFile );
	
	string instructions = ToShortPathName( DeleteLastPathNameComponent( mbFile ) ) + "/" + GetLastPathNameComponent( mbFile ) + "\n";
	//string instructions = mbFile + "\n";
	if (importer.GetBakeIK())
		instructions += "bake\n";

	if (attemptNumber == 0)
	{
		if (importer.GetMeshSettings().normalImportMode == kTangentSpaceOptionsImport)
			instructions += "exportNormals\n";
		if (importer.GetMeshSettings().tangentImportMode == kTangentSpaceOptionsImport)
			instructions += "exportTangents\n";
	}

	if (!WriteStringToFile (instructions, kMayaCommandPipe, kProjectTempFolder, kFileFlagDontIndex | kFileFlagTemporary))
	{
		printf_console("Failed writing command pipe\n");
		return kMayaConversionFailed;
	}

	if (g_MayaProcess && !IsProcessRunning(g_MayaProcess->dwProcessId))
	{
		g_MayaProcess = NULL;
	}

	if (g_MayaProcess != NULL)
	{
		PauseResumeProcess(g_MayaProcess->dwProcessId, true);
	}
	else
	{
		string mayaPath = GetDefaultApplicationForFile (mbFile);

		if (mayaPath.empty ())
			return "Maya could not be found.\nMake sure that maya is installed and the maya file has maya as its 'Open with' application!";

		if (!LaunchMayaTask (mayaPath, mbFile))
			return "Maya could not be launched.\nMake sure that maya is installed correctly and the maya file has maya as its 'Open with' application!";

		g_MayaWatchDogProcess = AttachWatchDog ( g_MayaWatchDogProcess, g_MayaProcess->dwProcessId );
	}

	const double launchTime = GetTimeSinceStartup();
	const int kTimeOutMin = 3;
	bool timeOut = false;

	while (true)
	{
		Sleep(100);

		if (IsFileCreated (kMayaSyncPipeKill) || IsFileCreated (kMayaSyncPipe))
			break;

		if (!IsProcessRunning(g_MayaProcess->dwProcessId))
		{
			if (IsFileCreated (kMayaSyncPipeKill) || IsFileCreated (kMayaSyncPipe))
				break;

			printf_console("Maya quit prematurely\n");
			g_MayaProcess = NULL;
			return kMayaConversionFailed;
		}

		if (GetTimeSinceStartup() - launchTime > kTimeOutMin * 60)
		{
			timeOut = true;
			break;
		}
	}

	if (IsFileCreated (kMayaSyncPipeKill))
	{
		TerminateProcess(g_MayaProcess->hProcess, 0);
		g_MayaProcess = NULL;
	}
	else
		PauseResumeProcess(g_MayaProcess->dwProcessId, false);

	if (IsFileCreated (kMayaExportedFBXFile))
	{
		*file = kMayaExportedFBXFile;
		return "";
	}
	else
	{
		std::ostringstream str;
		str << "Maya couldn't convert the mb file to an fbx file! ";
		if (timeOut)
			str << "It reached timeout of " << kTimeOutMin << " minutes.";
		return std::string(str.str().c_str());
	}
}

static bool LaunchMaxTask (const string& maxPath, const string& maxFile)
{
	if (maxPath.empty ())
		return false;

	// Generate max script(s)
	if ( !PrepareAndCopyScript ("Data/Tools/FBXMaxExport.ms", "Temp/FBXMaxExport.ms", maxFile, maxPath) )
	{
		printf_console("Generating max script(s) failed\n");
		return false;
	}

	vector<string> args;

	string scriptPath = PathToAbsolutePath ("Temp/FBXMaxExport.ms");
	ConvertSeparatorsToWindows( scriptPath );

	//args.push_back( "-q -silent -mip -U MAXScript \"" + scriptPath + "\"" );
#if !DEBUG_MAX
	args.push_back( "-q" );
	args.push_back( "-silent" );
	args.push_back( "-mip" );
#endif
	args.push_back( "-U MAXScript \"" + scriptPath + "\"" );
	
	static PROCESS_INFORMATION sMaxProc;

	if ( !LaunchTaskArrayOptions(maxPath, args, kLaunchBackground, maxPath.substr(0, maxPath.find_last_of(kPathNameSeparator)), sMaxProc ) )
	{
		g_MaxProcess = NULL;
		return false;
	}
	else
	{
		g_MaxProcess = &sMaxProc;
		return true;
	}

	return false;
}

BOOL CALLBACK EnumWindowsProc( HWND hwnd, LPARAM lParam )
{
	DWORD dwProcessId;
	GetWindowThreadProcessId( hwnd, &dwProcessId );

	if ( (DWORD)lParam == dwProcessId )
	{
		ShowWindow( hwnd, SW_HIDE );
		//return FALSE;
	}

	return TRUE;
}

void HideMainWindow( PROCESS_INFORMATION* proc )
{
	EnumWindows( EnumWindowsProc, proc->dwProcessId );
}

static const char* kMaxConversionFailed =
"3ds Max couldn't convert the max file to an fbx file\n"
"Try exporting the max file to an fbx file manually.\n"
"FBX plugins can be found here: http://www.autodesk.com/fbx\n";

string ConvertMaxToFBX (FBXImporter& importer, const string& maxFile, string* file, int attemptNumber, bool useFileUnits)
{
	CreateDirectory ("Temp");
	DeleteFile (kMaxCommandPipe);
	DeleteFile (kMaxSyncPipeKill);
	DeleteFile (kMaxExportedFBXFile);

	string instructions = ToShortPathName( maxFile ) + "\n";
	if (importer.GetBakeIK())
			instructions += "bake\n";

	if (attemptNumber == 0)
	{
		if (importer.GetMeshSettings().normalImportMode == kTangentSpaceOptionsImport)
			instructions += "exportNormals\n";
		if (importer.GetMeshSettings().tangentImportMode == kTangentSpaceOptionsImport)
			instructions += "exportTangents\n";
		if (useFileUnits)
			instructions += "useFileUnits\n";
	}

	if (!WriteStringToFile (instructions, kMaxCommandPipe, kProjectTempFolder, kFileFlagDontIndex | kFileFlagTemporary))
	{
		printf_console("Failed writing command pipe\n");
		return kMaxConversionFailed;
	}

	if (g_MaxProcess && !IsProcessRunning(g_MaxProcess->dwProcessId))
	{
		g_MaxProcess = NULL;
	}

	if (g_MaxProcess != NULL)
	{
		PauseResumeProcess(g_MaxProcess->dwProcessId, true);
	}
	else
	{
		string maxPath = GetDefaultApplicationForFile( maxFile );
#if DEBUG_MAX
		//maxPath = "C:/Program Files (x86)/Autodesk/3dsMax8/3dsmax.exe"; // V64: Works correctly.
		//maxPath = "C:/Program Files (x86)/Autodesk/3ds Max 9/3dsmax.exe"; // V64: Works correctly.
		//maxPath = "C:/Program Files (x86)/Autodesk/3ds Max 2008/3dsmax.exe"; // V64: Launches minimized, allows activation.
		//maxPath = "C:/Program Files (x86)/Autodesk/3ds Max 2009/3dsmax.exe"; // V64: Launches maximized.
		//maxPath = "D:/Program Files/Autodesk/3dsMax8/3dsmax.exe"; // XP32: Works correctly.
		//maxPath = "C:/Program Files/Autodesk/3ds Max 9/3dsmax.exe"; // V64: Launches minimized, can't activate. XP32: Works correctly.
		//maxPath = "C:/Program Files/Autodesk/3ds Max 2008/3dsmax.exe"; // V64: Launches minimized, allows activation. XP32: Works correctly.
		//maxPath = "C:/Program Files/Autodesk/3ds Max 2009/3dsmax.exe"; // V64/XP32: Launches maximized.
		maxPath = "C:/Program Files/Autodesk/3ds Max 2010/3dsmax.exe"; // V64/XP32: Launches maximized.
#endif
		
		if ( maxPath.empty() )
			return "3ds Max could not be found.\nMake sure that 3ds Max is installed and the max file has 3ds Max as its 'Open with' application!";

		if ( !LaunchMaxTask( maxPath, maxFile ) )
			return "3ds Max could not be launched.\nMake sure that 3ds Max is installed correctly and the max file has 3ds Max as its 'Open with' application!";

		g_MaxWatchDogProcess = AttachWatchDog ( g_MaxWatchDogProcess, g_MaxProcess->dwProcessId);
	}

	const double launchTime = GetTimeSinceStartup();
	const int kTimeOutMin = 3;
	bool timeOut = false;

	while (true)
	{
		if (IsFileCreated (kMaxSyncPipeKill) || !IsFileCreated (kMaxCommandPipe))
			break;

		if (!IsProcessRunning(g_MaxProcess->dwProcessId))
		{
			if (IsFileCreated (kMaxSyncPipeKill) || !IsFileCreated (kMaxCommandPipe))
				break;

			printf_console("Max quit prematurely\n");
			g_MaxProcess = NULL;
			return kMaxConversionFailed;
		}
#if !DEBUG_MAX
		else
		{
			HideMainWindow( g_MaxProcess );
		}
#endif

		if (GetTimeSinceStartup() - launchTime > kTimeOutMin * 60)
		{
			timeOut = true;
			break;
		}

		Sleep(100);
	}

	if (IsFileCreated (kMaxSyncPipeKill))
	{
		TerminateProcess(g_MaxProcess->hProcess, 0);
		g_MaxProcess = NULL;
	}
	else
		PauseResumeProcess(g_MaxProcess->dwProcessId, false);

	if (IsFileCreated (kMaxExportedFBXFile))
	{
		*file = kMaxExportedFBXFile;
		return "";
	}
	else
	{
		std::ostringstream str;
		str << "Max couldn't convert the max file to an fbx file! ";
		if (timeOut)
			str << "It reached timeout of " << kTimeOutMin << " minutes.";
		return std::string(str.str().c_str());
	}

	return "";
}

static bool LaunchC4DTask (const string& cinema4DPath)
{
	if (cinema4DPath.empty ())
		return false;

	if (g_C4DProcess != NULL && IsProcessRunning(g_C4DProcess->dwProcessId))
	{
		// Terminate last process if it's still running
		TerminateProcess( g_C4DProcess->hProcess, 0);
	}

	vector<string> args;
	// starts Cinema4D in background mode (but it quits immediately too, 
	// so all work has to be done right after it started)
	args.push_back("-nogui");

	static PROCESS_INFORMATION sC4DProc;

	if (!LaunchTaskArrayOptions( cinema4DPath, args, kLaunchQuoteArgs|kLaunchBackground, cinema4DPath.substr(0, cinema4DPath.find_last_of(kPathNameSeparator)), sC4DProc))
	{
		g_C4DProcess = NULL;
		return false;
	}else
	{
		g_C4DProcess = &sC4DProc;
		return true;
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
	// TODO : should share most of this code with OSX! now it's duplicated....

	// Cleanup export folder and command files
	const char* uid = getenv("USERNAME");
	char tempPath[ kDefaultPathBufferSize ];
	if ( !GetTempPath( kDefaultPathBufferSize, tempPath ) )
		return "Unity could not retrieve the Windows temporary directory.\nTry exporting the .C4D file to an fbx file manually.";

	string commandPipePath = Format(kC4DCommandPipePath, tempPath, uid);
	string syncPipePath = Format(kC4DSyncPipePath, tempPath, uid);

	CreateDirectory ("Temp");
	DeleteFile (kExportedC4DFBXFile);
	DeleteFile (commandPipePath);
	DeleteFile (syncPipePath);

	// Find in open applications list
	string cinema4DPath; // = GetLaunchedC4DPath();

	// Find through default application for file
	if (cinema4DPath.empty())
	{
		// Check if Cinema 4D is installed!
		cinema4DPath = GetDefaultApplicationForFile (sourceFile);
		if (cinema4DPath.empty ())
			return "Cinema 4D could not be found.\nMake sure that Cinema 4D is installed and the .C4D file has Cinema 4D as its 'Open with' application!";

		// Protect against using the net render / server version	
		if (GetLastPathNameComponent(cinema4DPath) == "C4D Client.exe" || GetLastPathNameComponent(cinema4DPath) == "C4D Server.exe")
		{
			cinema4DPath = AppendPathName(DeleteLastPathNameComponent (cinema4DPath), "CINEMA 4D");
			if (!IsFileCreated(cinema4DPath) && !IsDirectoryCreated(cinema4DPath))
				return "Cinema 4D could not be found.\nMake sure that Cinema 4D is installed and the .C4D file has Cinema 4D as its 'Open with' application!";
		}
	}

	string exportedFilePath = PathToAbsolutePath( kExportedC4DFBXFile );
	ConvertSeparatorsToWindows( exportedFilePath );

	// Write command pipe file
	string commands;
	commands =  "src=" + PathToAbsolutePath(sourceFile) + "\n";
	commands += "dst=" + exportedFilePath + "\n";
	commands += "textureSearchPath=" + PathToAbsolutePath("Assets") + "\n";
	commands += "src10=" + PathToAbsolutePath(sourceFile) + "\n";
	commands += "dst10=" + exportedFilePath + "\n";
	commands += "textureSearchPath10=" + PathToAbsolutePath("Assets") + "\n";

	if (importer.GetBakeIK())
	{
		commands += Format("ikfrom=0\n");
		commands += Format("ikto=30\n");
	}

	//if (!WriteStringToFile(commands, commandPipePath, kSystemTempFolder))
	if (!WriteStringToFile(commands, commandPipePath, kProjectTempFolder, kFileFlagDontIndex | kFileFlagTemporary))
	{
		return "Couldn't convert .C4D file to fbx file because you don't have write access to the tmp folder..\n"
			"Try exporting the .C4D file to an fbx file manually.\n";
	}

	// Auto-install C4D plugin
	bool requiresInstall = false;
	string installedPluginPath = AppendPathName(DeleteLastPathNameComponent(cinema4DPath), kInstalledFBXPluginPath);

	// TODO : share code with OSX

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
		if (winutils::IsApplicationRunning(cinema4DPath))
		{
			// Make sure it is quit
			while (winutils::IsApplicationRunning(cinema4DPath))
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
		bool vistaOrLater = systeminfo::GetOperatingSystemNumeric() >= 600;
		if ( vistaOrLater )
		{
			if ( !IsUserAnAdmin() )
				return "Cinema 4D converter plugin installation failed. Please run Unity as an Administrator to have the plugin installed.";
		}

		printf_console("\nInstalling Unity Cinema 4D Converter Plugin\n");
		DeleteFileOrDirectory(installedPluginPath);
		bool accessDenied = false;
		if (CopyFileOrDirectoryCheckPermissions (AppendPathName(GetApplicationFolder(), kC4DToFBXPlugin), installedPluginPath, accessDenied))
		{
			WriteStringToFile(kExpectedVersion, AppendPathName(installedPluginPath, kVersionFileName), kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary);
			printf_console("Installation succeed\n");
		}
		else
		{
			if (accessDenied)
			{
				if (DisplayDialog("Permission Error", "Failed to install Cinema 4D plugin because access was denied to copy files to " + installedPluginPath + "\n"+
					"Please run Unity with administrator rights at least once so that the plugin can be installed.", "OK", "Force Quit") == 0)
				{
					exit(1);
				}
			}

			printf_console("Installation failed\n");
			return "Cinema 4D converter plugin installation failed.";
		}
	}

	// Launch Cinema 4D
	// We lounch Cinema 4D every time, because we export only single file with it
	// At the moment there is no other way to louch it in background mode
	if ( !LaunchC4DTask( cinema4DPath ))
		return "Cinema 4D could not be launched.\nMake sure that Cinema 4D is installed correctly and the C4D file has Cinema 4D as its 'Open with' application!";

	g_C4DWatchDogProcess = AttachWatchDog( g_C4DWatchDogProcess, g_C4DProcess->dwProcessId );

	double launchTime = GetTimeSinceStartup();
	bool pluginNotInstalled = false;
	bool conversionFailed = false;
	bool processExited = false;

	///@TODO: RELAUCH cinema 4d to make sure that we have the plugins installed
	while (true)
	{
		Sleep( 100 );

		if ( IsFileCreated( syncPipePath ) )
			break;

		if ( GetTimeSinceStartup() - launchTime > 90 )
		{
			if (IsFileCreated( commandPipePath ) )
			{
				pluginNotInstalled = true;
				break;
			}
		}

		if ( GetTimeSinceStartup() - launchTime > 90 )
		{
			if ( !IsFileCreated( syncPipePath ) )
			{
				conversionFailed = true;
				break;
			}
		}

		if (!IsProcessRunning( g_C4DProcess->dwProcessId ))
		{
			processExited = true;
			break;
		}
	}

	if ((pluginNotInstalled || conversionFailed) && IsProcessRunning( g_C4DProcess->dwProcessId ))
	{
		// if conversion failed and task is still running we choose to terminate it
		// because it's probably hanging due to some reason - for example failed to connect to license server
		TerminateProcess( g_C4DProcess->hProcess, 0);
	}

	// No conversion took place	
	if ( !IsFileCreated( syncPipePath ) )
	{
		if (IsFileCreated( kExportedC4DFBXFile ))
		{
			printf_console ("FBX FILE exists but sync pipe doesn't!!!\n");	
		}

		InputString tempRES;
		ReadStringFromFile(&tempRES, syncPipePath);

		printf_console ("SYNC PIPE NOT FOUND EXTRACT: '%s'\n", tempRES.c_str());
		printf_console ("SYNC PIPE PATH: '%s'\n", PathToAbsolutePath( syncPipePath ).c_str());
		printf_console ("LOOP TERMINATION: pluginNotInstalled: %d, conversionFailed: %d, processExited: %d\n", pluginNotInstalled, conversionFailed, processExited );

		// TODO : add a note that demo version can't be used
		return "Cinema 4D couldn't convert the .C4D file to an fbx file.\n"
			"Try exporting the .C4D file to an fbx file manually.\n"
			"You need at least Cinema 4D R11 to import .C4D files directly.\n";
	}

	// The file should be written atomically
	// - it is not so lets have multiple tries!
	InputString resultString;
	for (int i=0;i<100;i++)
	{
		Sleep( 100 );
		ReadStringFromFile( &resultString, syncPipePath );
		if (!resultString.empty())
			break;
	}

	// An error occurred!
	if (!BeginsWith(resultString.c_str(), "SUCCESS"))
	{
		printf_console ("Got fbx converter error: '%s'", resultString.c_str());

		return std::string("Cinema 4D couldn't convert the .C4D file to an fbx file.\n"
			"Try exporting the .C4D file to an fbx file manually.\n") +
			resultString.c_str();
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
