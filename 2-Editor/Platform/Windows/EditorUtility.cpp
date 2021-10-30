#include "UnityPrefix.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Utility/CurlRequest.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/File.h"
#include "Configuration/UnityConfigure.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "resource.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Application.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include <ShellAPI.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <wbemcli.h>
#include <commdlg.h>
#include <CommCtrl.h>
#include <tlhelp32.h>
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Utility/SplashScreen.h"
#include "Utility/IFileDialogHeader.h"
#include "Runtime/Utilities/URLUtility.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Utility/BrowseForFolder.h"
#include "Utility/Windows7Taskbar.h"
#include <atlbase.h>
#include <atlcom.h>

#include "External/Audio/libogg/include/../../../../External/ProphecySDK/src/extlib/pnglib/png.h"

using namespace std;

HWND GetMainEditorWindow(); // WinEditorMain.cpp
BOOL GetGAppActive(); // WinEditorMain.cpp

//static bool LaunchTaskAuthorizedArray (const std::string& app, std::string* output, const std::string& prompt, const std::vector<std::string>& arguments, const std::string& currentDirectory = std::string ());

namespace 
{
	const int kProgressBarRange = 1000;

	struct ProgressBar
	{
	public: 
		ProgressBar() : window(NULL), hasCancel (false), wantsToCancel(false), buildInterval(Vector2f(-1.0f,-1.0f)) {}
		
		bool IsBuildInProgress () 
		{ 
			return (buildInterval.x >= 0.0f && buildInterval.x <= 1.0f);
		}
		void Clear () 
		{
			DestroyWindow( window );
			window = NULL;
			text = "";
			hasCancel = false;
			wantsToCancel = false;
			buildInterval = Vector2f(-1.0f,-1.0f);
		}

		HWND			window;
		std::string		text;
		bool			hasCancel;
		bool			wantsToCancel;
		Vector2f		buildInterval;
	};
	ProgressBar s_ProgressBar;
}


#define DEBUG_BACKGROUND_TASK 0

//----------------------------------------------------------------------------------------------------

std::string GetDefaultEditorPath ()
{
	return AppendPathName( GetApplicationContentsPath(), "../../MonoDevelop/bin/MonoDevelop.exe" );
}

static bool OpenWithBuiltinEditor (const std::string& path)
{
	std::string extension = ToLower(GetPathNameExtension(path));
	if (extension == "cs" || extension == "js" || extension == "boo" || extension == "txt" || extension == "shader" || extension == "log" || extension == "compute" || extension == "fnt")
	{
		return OpenFileWithApplication (path, GetDefaultEditorPath ());
	}

	return false;
}

bool OpenWithDefaultApp (const std::string& path)
{
	std::wstring widePath;
	ConvertUnityPathName( path, widePath );

	int res = (int)::ShellExecuteW( NULL, L"open", widePath.c_str(), NULL, NULL, SW_SHOWNORMAL );
	if( res <= 32 )
		res = (int)::ShellExecuteW( NULL, L"edit", widePath.c_str(), NULL, NULL, SW_SHOWNORMAL );
	if( res <= 32 )
		return OpenWithBuiltinEditor(path);

	return true;
}

bool OpenFileWithApplication (const std::string& path, const std::string& app)
{
	string sapp = PathToAbsolutePath (app);
	string spath = QuoteString(PathToAbsolutePath (path));

	wchar_t widePath[kDefaultPathBufferSize];
	wchar_t wideApp[kDefaultPathBufferSize];

	ConvertUnityPathName( sapp.c_str(), wideApp, kDefaultPathBufferSize );
	ConvertUnityPathName( spath.c_str(), widePath, kDefaultPathBufferSize );

	int res = (int)::ShellExecuteW( NULL, L"open", wideApp, widePath, NULL, SW_SHOWNORMAL );
	if( res <= 32 ) {
		res = (int)::ShellExecuteW( NULL, L"edit", wideApp, widePath, NULL, SW_SHOWNORMAL );
	}

	return res > 32;
}

bool LaunchApplication (const std::string& path)
{
	wchar_t widePath[kDefaultPathBufferSize];

	ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );

	int res = (int)::ShellExecuteW( NULL, L"open", widePath, NULL, NULL, SW_SHOWNORMAL );

	return res > 32;
}

bool PauseResumeProcess(DWORD dwOwnerPID, bool bResumeThread)
{
	// Based on: http://www.codeproject.com/KB/threads/pausep.aspx
	// Stopping programs this way might create deadlocks...
	HANDLE        hThreadSnap = NULL; 
	BOOL          bRet        = FALSE; 
	THREADENTRY32 te32; 

	ZeroMemory(&te32, sizeof(te32));

	// Take a snapshot of all threads currently in the system. 

	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
	if (hThreadSnap == INVALID_HANDLE_VALUE) 
		return (FALSE); 

	// Fill in the size of the structure before using it. 

	te32.dwSize = sizeof(THREADENTRY32); 

	// Walk the thread snapshot to find all threads of the process. 
	// If the thread belongs to the process, add its information 
	// to the display list.

	if (Thread32First(hThreadSnap, &te32)) 
	{ 
		do 
		{ 
			if (te32.th32OwnerProcessID == dwOwnerPID) 
			{
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
				if( hThread ) {
					if (bResumeThread)
						ResumeThread(hThread);
					else
						SuspendThread(hThread);
					CloseHandle(hThread);
				}
			} 
		}
		while (Thread32Next(hThreadSnap, &te32)); 
		bRet = TRUE; 
	} 
	else 
		bRet = FALSE;          // could not walk the list of threads 

	// Do not forget to clean up the snapshot object. 
	CloseHandle (hThreadSnap); 

	return (bRet); 
}

bool IsProcessRunning(DWORD pid)
{
	// Based on: http://www.codeproject.com/KB/threads/pausep.aspx
	HANDLE         hProcessSnap = NULL; 
	BOOL           bRet      = false; 
	PROCESSENTRY32 pe32; 

	ZeroMemory(&pe32, sizeof(pe32));

	//  Take a snapshot of all processes in the system. 
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 

	if (hProcessSnap == INVALID_HANDLE_VALUE) 
		return false; 

	//  Fill in the size of the structure before using it. 
	pe32.dwSize = sizeof(PROCESSENTRY32); 

	//  Walk the snapshot of the processes.
	if (Process32First(hProcessSnap, &pe32)) 
	{ 
		do 
		{ 
			if (pe32.th32ProcessID == pid)
			{
				bRet = true; 
				break;
			}
		} 
		while (Process32Next(hProcessSnap, &pe32)); 
	} 

	// Do not forget to clean up the snapshot object. 

	CloseHandle (hProcessSnap); 
	return bRet; 
}

bool LaunchTaskArrayOptions (const std::string& app, const std::vector<std::string>& arguments, UInt32 options,
								const std::string& currentDirectory, PROCESS_INFORMATION& processInfo)
{
	processInfo.dwProcessId = 0;

	// Search path if app is not absolute name
	wchar_t appWide[kDefaultPathBufferSize];
	ConvertUnityPathName( app.c_str(), appWide, kDefaultPathBufferSize );
	if( !IsAbsoluteFilePath(app) )
	{
		wchar_t pathWide[kDefaultPathBufferSize];
		memset(pathWide, 0, sizeof(pathWide));
		wchar_t* namePart;
		if( SearchPathW( NULL, appWide, NULL, kDefaultPathBufferSize, pathWide, &namePart ) > 0 )
		{
			memcpy( appWide, pathWide, sizeof(pathWide) );
		}
	}

	// Now create a child process
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFOW siStartInfo;
	ZeroMemory( &piProcInfo, sizeof(piProcInfo) );
	ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
	siStartInfo.cb = sizeof(siStartInfo);
	#if !DEBUG_BACKGROUND_TASK
	siStartInfo.dwFlags = STARTF_USESHOWWINDOW;
	siStartInfo.wShowWindow = SW_HIDE;
	#endif
	if( !(options & kLaunchBackground) )
		siStartInfo.wShowWindow = SW_SHOWNORMAL;

	// generate an argument list
	std::wstring argumentString = std::wstring(L"\"") + appWide + L'"';
	std::wstring argWide;
	for( int i = 0; i < arguments.size(); ++i ) {
		argumentString += L' ';

		std::string arg = arguments[ i ];
		if ( options & kLaunchQuoteArgs )
			arg = QuoteString( arg );

		ConvertUTF8ToWideString( arg, argWide );
		argumentString += argWide;
	}

	wchar_t* argumentBuffer = new wchar_t[argumentString.size()+1];
	memcpy( argumentBuffer, argumentString.c_str(), (argumentString.size()+1)*sizeof(wchar_t) );

	// launch process
	wchar_t directoryWide[kDefaultPathBufferSize];
	ConvertUnityPathName( currentDirectory.c_str(), directoryWide, kDefaultPathBufferSize );

	BOOL processResult = CreateProcessW(
		appWide,		// application
		argumentBuffer, // command line
		NULL,          // process security attributes
		NULL,          // primary thread security attributes
		TRUE,          // handles are inherited
		0,             // creation flags
		NULL,          // use parent's environment
		currentDirectory.empty() ? NULL : directoryWide, // directory
		&siStartInfo,  // STARTUPINFO pointer
		&piProcInfo ); // receives PROCESS_INFORMATION

	delete[] argumentBuffer;

	if( processResult == FALSE ) {
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
		return false;
	}

	memcpy(&processInfo, &piProcInfo, sizeof(PROCESS_INFORMATION));

	return true;
}

void ShowInTaskbarIfNoMainWindow(HWND wnd)
{
	if(GetMainEditorWindow() == NULL ) 
	{
		SetWindowLong( wnd, GWL_EXSTYLE, GetWindowLong(wnd,GWL_EXSTYLE) | WS_EX_APPWINDOW );
		SetWindowTextW( wnd, L"Unity");
		SetForegroundWindow( wnd );
	}
}

/*
bool LaunchNewApplicationBackground (const std::string& path)
{
    LSLaunchURLSpec     lspec = { NULL, NULL, NULL, 0, NULL };
    BOOL                success = YES;
    OSStatus            status;
    CFURLRef            appurl = NULL;

	FSRef appRef;
	OSErr err;

	if ((err = FSPathMakeRef (reinterpret_cast<const unsigned char*> (ResolveSymlinks(PathToAbsolutePath(path)).c_str()), &appRef, NULL)) != noErr)
		return false;

    if (( appurl = CFURLCreateFromFSRef( kCFAllocatorDefault, &appRef )) == NULL ) {
        NSLog( @"CFURLCreateFromFSRef failed." );
        return( NO );
    }

    lspec.appURL = appurl;
    lspec.itemURLs = NULL;
    lspec.passThruParams = NULL;
    lspec.launchFlags = kLSLaunchAsync | kLSLaunchDontSwitch | kLSLaunchNoParams | kLSLaunchNewInstance;
    lspec.asyncRefCon = NULL;
    
    status = LSOpenFromURLSpec( &lspec, NULL );
    
    if ( status != noErr ) {
        NSLog( @"LSOpenFromRefSpec failed: error %d", status );
        success = NO;
    }
    
    if ( appurl != NULL ) {
        CFRelease( appurl );
    }

    return( success );
}
*/

bool LaunchTask (const string& app, string* output, ...)
{
	vector<string> arguments;
	va_list ap;
	va_start (ap, output);
	while (true)
	{
		char* cur = va_arg (ap, char*);
		if (cur == NULL) break;
		arguments.push_back (cur);	
	}
	va_end (ap);
	return LaunchTaskArray (app, output, arguments, true);
}



bool LaunchTaskArray( const string& app, string* output, const vector<string>& arguments, bool quoteArguments, const string& currentDirectory, UInt32* exitCode )
{
	// Most of this is from MSDN's "Creating a Child Process with Redirected Input and Output",
	// except for the parts where the code example is wrong :)
	//
	// See http://www.codeproject.com/KB/threads/redir.aspx for details on where it's wrong...

	if( output )
		output->clear();

	// Search path if app is not absolute name
	wchar_t appWide[kDefaultPathBufferSize];
	ConvertUnityPathName( app.c_str(), appWide, kDefaultPathBufferSize );
	if( !IsAbsoluteFilePath(app) )
	{
		wchar_t pathWide[kDefaultPathBufferSize];
		memset(pathWide, 0, sizeof(pathWide));
		wchar_t* namePart;
		if( SearchPathW( NULL, appWide, NULL, kDefaultPathBufferSize, pathWide, &namePart ) > 0 )
		{
			memcpy( appWide, pathWide, sizeof(pathWide) );
		}
	}

	// Setup redirect of stdout if needed

	HANDLE stdoutSaved = INVALID_HANDLE_VALUE;
	winutils::AutoHandle stdoutChildWrite, stdoutChildDup;
	if( output != NULL )
	{
		// The steps for redirecting child process's STDOUT:
		// 1. Save current STDOUT, to be restored later.
		// 2. Create anonymous pipe to be STDOUT for child process.
		// 3. Set STDOUT of the parent process to be write handle to
		//    the pipe, so it is inherited by the child process.
		// 4. Create a noninheritable duplicate of the read handle and
		//    close the inheritable read handle.

		// Set the inherit flag so pipe handles are inherited
		SECURITY_ATTRIBUTES saAttr;
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		// Save the handle to the current STDOUT.
		stdoutSaved = GetStdHandle(STD_OUTPUT_HANDLE);

		winutils::AutoHandle stdoutChildRead;
		if( !CreatePipe( &stdoutChildRead.handle, &stdoutChildWrite.handle, &saAttr, 0 ) )
		{
			if( output != NULL )
				*output = "Could not start task " + app + ": failed to create a pipe";
			return false;
		}

		// Set a write handle to the pipe to be STDOUT. 
		if( !SetStdHandle(STD_OUTPUT_HANDLE, stdoutChildWrite.handle) )
		{
			if( output != NULL )
				*output = "Could not start task " + app + ": failed to redirect stdout";
			return false;
		}

		// Create noninheritable read handle and close the inheritable read handle.
		bool success = DuplicateHandle( GetCurrentProcess(), stdoutChildRead.handle,
			GetCurrentProcess(), &stdoutChildDup.handle, 0,
			FALSE,
			DUPLICATE_SAME_ACCESS );
		if( !success )
		{
			if( output != NULL )
				*output = "Could not start task " + app + ": failed to duplicate handle";
			return false;
		}
		stdoutChildRead.Close();
	}


	//
	// Now create a child process

	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFOW siStartInfo;
	ZeroMemory( &piProcInfo, sizeof(piProcInfo) );
	ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
	siStartInfo.cb = sizeof(siStartInfo);
	siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	siStartInfo.hStdOutput = stdoutChildWrite.handle;
	siStartInfo.hStdError = stdoutChildWrite.handle;
	siStartInfo.wShowWindow = SW_HIDE;

	// generate an argument list
	std::wstring argumentString = std::wstring(L"\"") + appWide + L'"';
	std::wstring argWide;
	for( int i = 0; i < arguments.size(); ++i ) {
		argumentString += L' ';

		std::string arg = arguments[ i ];
		if ( quoteArguments )
			arg = QuoteString( arg );

		ConvertUTF8ToWideString( arg, argWide );
		argumentString += argWide;
	}

	wchar_t* argumentBuffer = new wchar_t[argumentString.size()+1];
	memcpy( argumentBuffer, argumentString.c_str(), (argumentString.size()+1)*sizeof(wchar_t) );

	// launch process
	wchar_t directoryWide[kDefaultPathBufferSize];
	ConvertUnityPathName( currentDirectory.c_str(), directoryWide, kDefaultPathBufferSize );

	BOOL processResult = CreateProcessW(
		appWide,		// application
		argumentBuffer, // command line
		NULL,          // process security attributes
		NULL,          // primary thread security attributes
		TRUE,          // handles are inherited
		0,             // creation flags
		NULL,          // use parent's environment
		currentDirectory.empty() ? NULL : directoryWide, // directory
		&siStartInfo,  // STARTUPINFO pointer
		&piProcInfo ); // receives PROCESS_INFORMATION

	delete[] argumentBuffer;

	if( processResult == FALSE )
	{
		if( output != NULL )
			*output = "Could not start task " + app + ": failed to launch process: " + winutils::ErrorCodeToMsg(GetLastError());
		return false;
	}

	winutils::AutoHandle processHandle( piProcInfo.hProcess );
	winutils::AutoHandle threadHandle( piProcInfo.hThread );


	// Read output of the child process
	bool readError = false;
	if( output != NULL )
	{
		// After process creation, restore the saved STDOUT
		if( !SetStdHandle(STD_OUTPUT_HANDLE, stdoutSaved) )
		{
			if( output != NULL )
				*output = "Could not start task " + app + ": failed to restore stdout";
			return false;
		}

		// Close the write end of the pipe before reading from the read end of the pipe
		stdoutChildWrite.Close();

		const DWORD kBufferSize = 1024;
		DWORD dwRead, dwWritten;
		char buffer[kBufferSize];

		for (;;)
		{
			DWORD availableBytes;
			if( !PeekNamedPipe( stdoutChildDup.handle, NULL, 0, NULL, &availableBytes, NULL ) )
			{
				// the child process might have ended
				break;
			}
			if( !availableBytes )
			{
				Sleep(1); // Sleep a bit to let the process run
				continue;
			}

			DWORD bytesRead;
			if( !ReadFile( stdoutChildDup.handle, buffer, std::min(availableBytes,kBufferSize), &bytesRead, NULL ) )
			{
				*output = "Could not read output from " + app + ": " + winutils::ErrorCodeToMsg(GetLastError());
				readError = true;
				break;
			}
			if( bytesRead == 0 )
				break;

			output->append( buffer, buffer+bytesRead );
		}
	}

	// wait for the process to exit
	WaitForSingleObject( processHandle.handle, INFINITE );

	DWORD code = 0;
	GetExitCodeProcess( processHandle.handle, &code );

	if (exitCode)
		*exitCode = code;

	return !readError && code == 0;
}


bool LaunchTaskArrayOptions (const string& app, const vector<string>& arguments, UInt32 options, const string& currentDirectory)
{
	PROCESS_INFORMATION procInfo;
	return LaunchTaskArrayOptions(app, arguments, options, currentDirectory, procInfo);
}

//----------------------------------------------------------------------------------------------------
static string GetTarGZToolPath()
{
	return AppendPathName (AppendPathName(GetApplicationContentsPath(), "Tools"), "7z.exe");
}

void CompressPackageTarGZAndDeleteFolder (const std::string& folder, const std::string& compressedPath, bool runAsyncAndShowInFinder)
{
	vector<string> args;
	const std::string toolPath = GetTarGZToolPath();

	std::string tempExportPackage = AppendPathName(PathToAbsolutePath("Temp"), std::string(kTempExportPackage));
	ConvertSeparatorsToWindows( tempExportPackage );

	std::string resultFileWin = compressedPath;
	ConvertSeparatorsToWindows( resultFileWin );

	// clear the place for the temporary and final package
	DeleteFile( tempExportPackage );
	DeleteFile( resultFileWin );

	// TAR the folder first
	string tempFile = AppendPathName(PathToAbsolutePath("Temp"), "archtemp.tar");
	ConvertSeparatorsToWindows(tempFile);

	args.push_back("a");
	args.push_back("-r");
	args.push_back("-ttar");
	args.push_back("-y");
	args.push_back("-bd");
	args.push_back(tempFile);
	args.push_back(".");

	std::string toolOutput;
	bool success = LaunchTaskArray(toolPath, &toolOutput, args, true, folder);

	if( !success ) {
		ErrorString( Format("Error compressing TAR: %s\n", toolOutput.c_str ()) );
		return;
	}

	// Now compress the tar file with gzip
	args.clear();
	args.push_back("a");
	args.push_back("-tgzip");
	args.push_back("-bd");
	args.push_back("-y");
	args.push_back(tempExportPackage);
	args.push_back(tempFile);

	success = LaunchTaskArray(toolPath, &toolOutput, args, true, folder);

	if( !success ) {
		ErrorString( Format("Error compressing GZIP: %s\n", toolOutput.c_str ()) );
		return;
	}
	
	// move the package from the temp folder to the destination chosen by the user
	MoveReplaceFile( tempExportPackage, resultFileWin );

	// delete the tar
	DeleteFile( tempFile );

	// delete the temporary export package folder where the assets were gathered
	DeleteFileOrDirectory( folder );

	ClearProgressbar ();
	if ( runAsyncAndShowInFinder )
	{
		SelectFileWithFinder( compressedPath );
	}
}

bool EndsWithIgnoreCase (std::string const &fullString, std::string const &ending)
{
	if (fullString.length() > ending.length()) 
	{
		string strFull = ToLower(fullString);
		string strEnd = ToLower(ending);

		return (0 == strFull.compare (strFull.length() - strEnd.length(), strEnd.length(), strEnd));
	} 
	else 
	{
		return false;
	}
}

bool DecompressPackageTarGZ (const string& path, const string& destination)
{
	wchar_t winFileName[kDefaultPathBufferSize];

	const std::string kTempTarGZFolder = "Temp/TarGZ";
	DeleteFileOrDirectory(kTempTarGZFolder);
	if( !CreateDirectoryRecursive (kTempTarGZFolder) )
		return false;

	const string toolPath = GetTarGZToolPath();

	std::string shortPath;
	ConvertUnityPathNameToShort( path, shortPath );

	// Extract tar archive first
	vector<string> args;
	args.push_back("x");
	args.push_back(shortPath);
	args.push_back("-o" + kTempTarGZFolder);
	args.push_back("-r");
	args.push_back("-y");

	std::string toolOutput;
	bool success = LaunchTaskArray(toolPath, &toolOutput, args, true);

	if( !success ) {
		printf_console( "Error decompressing GZ: %s\n", toolOutput.c_str () );
		printf_console( "Details: path='%s' shortpath='%s' out='%s'\n", path.c_str(), shortPath.c_str(), kTempTarGZFolder.c_str() );
		return false;
	}

	// Now decompress the resulting tar file
	// Normally the TAR file should be named the same as tar.gz file, without the extension.
	std::string tarFileName = DeletePathNameExtension( GetLastPathNameComponent(path) );
	tarFileName = AppendPathName(kTempTarGZFolder, tarFileName);
	if( !IsFileCreated(tarFileName) ) 
	{
		// But if it is not, try finding it the hard way
		std::set<std::string> files;
		if( !GetFolderContentsAtPath( kTempTarGZFolder, files ) ) 
		{
			printf_console ("Error decompressing GZ: no files extracted into %s\n", kTempTarGZFolder.c_str());
			return false;
		}
		if( files.size() == 1 && EndsWithIgnoreCase(*files.begin(), ".tar")) 
		{
			tarFileName = *files.begin();
		}
		else
		{
			// seems like it was tar instead of tar.gz. Just move extracted files to destination
			DeleteFileOrDirectory(destination);

			if (!MoveFile(kTempTarGZFolder.c_str(), destination.c_str()))
			{
				printf_console ("Error decompressing GZ: could not rename directory %s to %s\n", kTempTarGZFolder.c_str(), destination.c_str());
				return false;
			}

			return true;
		}
	}

	ConvertUnityPathNameToShort( tarFileName, shortPath );

	args.clear();
	args.push_back("x");
	args.push_back(shortPath);
	args.push_back("-o" + destination);
	args.push_back("-r");
	args.push_back("-y");

	success = LaunchTaskArray(toolPath, &toolOutput, args, true);
	if( !success ) 
	{
		printf_console ("Error decompressing TAR: %s\n", toolOutput.c_str ());
		printf_console( "Details: path='%s' shortpath='%s' out='%s'\n", tarFileName.c_str(), shortPath, kTempTarGZFolder.c_str() );
		return false;
	}

	return true;
}


//----------------------------------------------------------------------------------------------------

static HANDLE UnityFindFirstFile(string searchPath, WIN32_FIND_DATAW &data)
{
	wchar_t widePath[kDefaultPathBufferSize+1];
	memset(widePath, 0, sizeof(widePath));
	ConvertUnityPathName( searchPath.c_str(), widePath, kDefaultPathBufferSize );
	return ::FindFirstFileW(widePath, &data);
}

static DWORD UnityGetFileAttributes(string filePath)
{
	wchar_t widePath[kDefaultPathBufferSize+1];
	memset(widePath, 0, sizeof(widePath));
	ConvertUnityPathName( filePath.c_str(), widePath, kDefaultPathBufferSize );
	return ::GetFileAttributesW(widePath);
}

static BOOL UnitySetFileAttributes(string filePath, DWORD attr)
{
	wchar_t widePath[kDefaultPathBufferSize+1];
	memset(widePath, 0, sizeof(widePath));
	ConvertUnityPathName( filePath.c_str(), widePath, kDefaultPathBufferSize );
	return ::SetFileAttributesW(widePath, attr);
}

static BOOL UnityFindNextFile(HANDLE hFindFile, WIN32_FIND_DATAW &data)
{
	return ::FindNextFileW(hFindFile, &data);
}

//----------------------------------------------------------------------------------------------------

static bool DoSetPermissionsReadWrite(const string &path)
{
	string searchPath = ::AppendPathName(path, "*");

	WIN32_FIND_DATAW data;
	HANDLE hFindFile;

	if (INVALID_HANDLE_VALUE == (hFindFile = UnityFindFirstFile(searchPath, data)))
	{
		::printf_console("Error: DoSetPermissionsReadWrite: FindFirstFile: %s", WIN_LAST_ERROR_TEXT);
		return false;
	}

	while (true)
	{
		if ((0 != ::_wcsicmp(data.cFileName, L".")) && (0 != ::_wcsicmp(data.cFileName, L"..")))
		{
			string fileName;
			ConvertWindowsPathName(data.cFileName, fileName);
			string filePath = ::AppendPathName(path, fileName);

			// According to MSDN file attribute information returned by FindNextFile cannot be trusted.

			DWORD attributes = UnityGetFileAttributes(filePath);

			if (INVALID_FILE_ATTRIBUTES != attributes)
			{
				if (FILE_ATTRIBUTE_READONLY == (attributes & FILE_ATTRIBUTE_READONLY))
				{
					if (!UnitySetFileAttributes(filePath, (attributes & ~FILE_ATTRIBUTE_READONLY)))
					{
						::printf_console("Warning: DoSetPermissionsReadWrite: SetFileAttributes: %s", WIN_LAST_ERROR_TEXT);
					}
				}

				if (FILE_ATTRIBUTE_DIRECTORY == (attributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					if (!::DoSetPermissionsReadWrite(filePath))
					{
						::FindClose(hFindFile);
						return false;
					}
				}
			}
			else
			{
				::printf_console("Warning: DoSetPermissionsReadWrite: GetFileAttributes: %s", WIN_LAST_ERROR_TEXT);
			}
		}

		if (!UnityFindNextFile(hFindFile, data))
		{
			if (ERROR_NO_MORE_FILES != ::GetLastError())
			{
				::printf_console("Error: DoSetPermissionsReadWrite: FindNextFile: %s", WIN_LAST_ERROR_TEXT);
				::FindClose(hFindFile);
				return false;
			}

			break;
		}
	}

	::FindClose(hFindFile);

	return true;
}

bool SetPermissionsForProjectFolder (const std::string& projectPath)
{
	AssertIf (projectPath.empty());
		
	std::string libraryPath = AppendPathName(projectPath, "Library");
	if (IsDirectoryCreated(libraryPath))
	{
		SetFileFlags(libraryPath, kAllFileFlags, kFileFlagDontIndex ); // mark Library folder as non-indexable
	}
	std::string tempPath = AppendPathName(projectPath, "Temp");
	if (IsDirectoryCreated(tempPath))
	{
		SetFileFlags(tempPath, kAllFileFlags, kFileFlagDontIndex ); // mark Temp folder as non-indexable
	}
		
	return true;
}

bool SetPermissionsReadWrite (const string& path)
{
	AssertIf (path.empty());
	return DoSetPermissionsReadWrite(path);
}

//----------------------------------------------------------------------------------------------------
static wchar_t s_DialogTitle[1000];
static wchar_t s_DialogText[8000];
static wchar_t s_DialogButton1[100];
static wchar_t s_DialogButton2[100];
static wchar_t s_DialogButton3[100];
static HFONT s_BoldFont;

static void SetDialogTextsAndSize(HWND hDlg)
{
	POINT offset = {0,0};
	ClientToScreen( hDlg, &offset );

	// set texts
	SetWindowTextW( hDlg, NULL ); // title bar
	SetDlgItemTextW( hDlg, IDC_ST_TITLE, s_DialogTitle );
	SetDlgItemTextW( hDlg, IDC_ST_CONTENT, s_DialogText );

	// Message text might be quite long. So we calculate its area and push the buttons up or down accordingly.
	// title height
	HWND textWnd = GetDlgItem(hDlg, IDC_ST_TITLE);
	HDC hdc = GetDC(textWnd);
	//HGDIOBJ oldObj = SelectObject(hdc, (HGDIOBJ)SendMessage(textWnd, WM_GETFONT, 0, 0));
	HGDIOBJ oldObj = SelectObject(hdc, s_BoldFont );
	RECT textRC;
	GetClientRect( textWnd, &textRC );
	int oldHeight = textRC.bottom;
	int oldWidth = textRC.right;
	DrawTextW( hdc, s_DialogTitle, -1, &textRC, DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK | DT_CALCRECT );
	SelectObject(hdc, oldObj);
	ReleaseDC(textWnd, hdc);

	int delta = textRC.bottom - oldHeight;
	int widthDelta = textRC.right - oldWidth;

	RECT rc;

	// content height
	textWnd = GetDlgItem(hDlg, IDC_ST_CONTENT);
	hdc = GetDC(textWnd);
	oldObj = SelectObject(hdc, (HGDIOBJ)SendMessage(textWnd, WM_GETFONT, 0, 0));
	GetWindowRect(textWnd, &rc);
	SetWindowPos(textWnd, 0, rc.left-offset.x, rc.top-offset.y + delta, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOREPOSITION);
	GetClientRect( textWnd, &textRC );
	oldHeight = textRC.bottom;
	oldWidth = textRC.right;
	textRC.top = textRC.left = textRC.bottom = 0;
	DrawTextW( hdc, s_DialogText, -1, &textRC, DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK | DT_CALCRECT );
	SelectObject(hdc, oldObj);
	ReleaseDC(textWnd, hdc);

	delta += textRC.bottom - oldHeight;
	if (textRC.right - oldWidth > widthDelta)
		widthDelta = textRC.right - oldWidth;

	if (widthDelta < -150)
		widthDelta = -150;

	// make buttons go below the icon
	if (delta < 30)
		delta = 30;

	// adjust dialog dimensions
	GetWindowRect(hDlg, &rc);
	SetWindowPos(hDlg, 0, rc.left, rc.top, rc.right-rc.left+widthDelta, rc.bottom-rc.top+delta, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOREPOSITION);

	// adjust text window
	GetWindowRect(textWnd, &rc);
	SetWindowPos(textWnd, 0, rc.left-offset.x, rc.top-offset.y, rc.right-rc.left + widthDelta, rc.bottom-rc.top+delta, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOREPOSITION);

	// adjust title label
	textWnd = GetDlgItem(hDlg, IDC_ST_TITLE);
	GetWindowRect(textWnd, &rc);
	SetWindowPos(textWnd, 0, rc.left-offset.x, rc.top-offset.y, rc.right-rc.left + widthDelta, rc.bottom-rc.top+delta, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOREPOSITION);

	// Set texts for the buttons.
	// They might be too long though. Resize them to fit.
	static DWORD kButtons[] = { IDOK, IDABORT, IDCANCEL };
	const wchar_t* kTexts[] = { s_DialogButton1, s_DialogButton3, s_DialogButton2 };
	int buttonSizes[ARRAY_SIZE(kButtons)];
	int totalButtonSize = 0;
	const int kSpaceBetweenButtons = 8;
	for( int i = 0; i < ARRAY_SIZE(kButtons); ++i ) 
	{
		HWND w = GetDlgItem(hDlg, kButtons[i]);
		if( w == NULL )
			continue;

		// set text
		SetWindowTextW( w, kTexts[i] );

		// get it's normal size
		RECT rc;
		GetWindowRect(w, &rc);
		int size = rc.right-rc.left;

		// calculate size of text
		HDC hdc = GetDC(w);
		RECT textRC = {0,0,0,0};
		DrawTextW( hdc, kTexts[i], -1, &textRC, DT_LEFT | DT_EXPANDTABS | DT_CALCRECT );
		int newSize = textRC.right - textRC.left;
		if( newSize > size )
			size = newSize;

		buttonSizes[i] = size;

		totalButtonSize += size + kSpaceBetweenButtons;
	}
	totalButtonSize -= kSpaceBetweenButtons;

	// Now position the buttons
	RECT dialogRC;
	GetClientRect( hDlg, &dialogRC );

	const int dialogWidth = dialogRC.right;
	int buttonX = dialogWidth - totalButtonSize - kSpaceBetweenButtons * 2; //(dialogWidth - totalButtonSize) / 2;
	for( int i = 0; i < ARRAY_SIZE(kButtons); ++i ) 
	{
		HWND w = GetDlgItem(hDlg, kButtons[i]);
		if( w == NULL )
			continue;

		RECT rc;
		GetWindowRect(w, &rc);
		SetWindowPos(w, 0, buttonX, rc.top-offset.y, buttonSizes[i], rc.bottom-rc.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
		buttonX += buttonSizes[i] + kSpaceBetweenButtons;
	}

	// adjust buttons
	for( int i = 0; i < ARRAY_SIZE(kButtons); ++i ) 
	{
		HWND w = GetDlgItem(hDlg, kButtons[i]);
		if( w != NULL ) 
		{
			GetWindowRect(w, &rc);
			SetWindowPos(w, 0, rc.left-offset.x, rc.top-offset.y + delta, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOSIZE);
		}
	}
}

static INT_PTR EditorDisplayDialogProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );

	switch( msg ) {
	case WM_INITDIALOG:
		{
			// note: ShowInTaskbarIfNoMainWindow needs to be called earlier than the rest
			// if WM_INITDIALOG processing
			ShowInTaskbarIfNoMainWindow(hDlg);
			SetDialogTextsAndSize(hDlg);

			winutils::CenterWindowOnParent(hDlg);

			LOGFONT font;
			GetObject ( (HANDLE)SendDlgItemMessage(hDlg, IDC_ST_TITLE, WM_GETFONT, 0, 0), sizeof(LOGFONT), &font );
			font.lfWeight = FW_BOLD;
			font.lfHeight -= 1;

			s_BoldFont = CreateFontIndirect(&font);

			SendDlgItemMessage(hDlg, IDC_ST_TITLE, WM_SETFONT, (WPARAM)s_BoldFont, 0);

			// create a memory DC holding the splash bitmap
			HDC hdcScreen = GetDC(NULL);
			HDC hdcMem = CreateCompatibleDC(hdcScreen);

			// load the bitmap
			int splashWidth, splashHeight;
			HBITMAP hbmpSplash = LoadPNGFromResource( hdcMem, IDR_UNITY_ICON, splashWidth, splashHeight );

			SendDlgItemMessage(hDlg, IDC_PICTURE, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(HANDLE)hbmpSplash);
		}

		return TRUE;

	case WM_COMMAND:
		switch( LOWORD(wParam) )
		{
		case IDOK:
			EndDialog( hDlg, IDOK );
			return TRUE;
		case IDCANCEL:
			EndDialog( hDlg, IDCANCEL );
			return TRUE;
		case IDABORT:
			EndDialog( hDlg, IDABORT );
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DeleteObject(s_BoldFont);
		break;
	}

	return FALSE;
}

static void RestoreMainWindowFromMinimize()
{
	HWND window = GetMainEditorWindow();
	if( !window )
		return;
	if( !IsIconic(window) )
		return;
	ShowWindow(window, SW_RESTORE);
}


bool DisplayDialog (const std::string& title, const std::string& content, const std::string& firstButton, const std::string& secondButton)
{
	if( IsBatchmode() ) {
		printf_console("Cancelling DisplayDialog: %s %s\n", title.c_str(), content.c_str());
		return false; // cancel
	}

	HideSplashScreen();
	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();
	RestoreMainWindowFromMinimize();
	EnableAllContainerWindows(false);

	UTF8ToWide( title.c_str(), s_DialogTitle, 1000 );
	UTF8ToWide( content.c_str(), s_DialogText, 8000 );
	UTF8ToWide( firstButton.c_str(), s_DialogButton1, 100 );
	UTF8ToWide( secondButton.c_str(), s_DialogButton2, 100 );
	int res = DialogBoxW( winutils::GetInstanceHandle(), MAKEINTRESOURCEW( secondButton.empty() ? IDD_DIALOGONE : IDD_DIALOGTWO ), GetMainEditorWindow(), (DLGPROC)EditorDisplayDialogProc );
	if( res == -1 ) {
		AssertString( "Error displaying dialog: " + winutils::ErrorCodeToMsg(GetLastError()) );
	}
	EnableAllContainerWindows(true);
	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);
	return res == IDOK ? true : false;
}

int DisplayDialogComplex (const std::string& title, const std::string& content, const std::string& okButton, const std::string& secondary, const std::string& third)
{
	if( IsBatchmode() ) {
		printf_console("Cancelling DisplayDialogComplex: %s %s\n", title.c_str(), content.c_str());
		return 1; // cancel
	}

	HideSplashScreen();
	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();
	RestoreMainWindowFromMinimize();
	EnableAllContainerWindows(false);

	UTF8ToWide( title.c_str(), s_DialogTitle, 1000 );
	UTF8ToWide( content.c_str(), s_DialogText, 8000 );
	UTF8ToWide( okButton.c_str(), s_DialogButton1, 100 );
	UTF8ToWide( third.c_str(), s_DialogButton2, 100 );
	UTF8ToWide( secondary.c_str(), s_DialogButton3, 100 );
	int res = DialogBoxW( winutils::GetInstanceHandle(), MAKEINTRESOURCEW( IDD_DIALOGTHREE ), GetMainEditorWindow(), (DLGPROC)EditorDisplayDialogProc );
	if( res == -1 ) {
		AssertString( "Error displaying dialog: " + winutils::ErrorCodeToMsg(GetLastError()) );
	}
	
	EnableAllContainerWindows(true);
	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);
	
	switch( res ) {
	case IDOK: return 0;
	case IDABORT: return 1;
	default: return 2;	// if you just close dialog in windows this means "cancel my action" not "no", Abort means "no"
	}
}

void RelaunchWithArguments (vector<string>& args)
{
	char path[kDefaultPathBufferSize];
	GetModuleFileName(winutils::GetInstanceHandle(), path, kDefaultPathBufferSize);

	string cmdline = QuoteString(path);

	for( int i = 0; i < args.size(); i++ ) {
		cmdline += ' ';
		cmdline += QuoteString(args[i]);
	}
	
	//SetRelaunchApplicationArguments (args);
	SendMessage(GetMainEditorWindow(), WM_CLOSE, NULL, NULL);
	// Don't relaunch if used cancelled the quit (e.g. cancel in "do you want to save changes")
	if( GetMainEditorWindow() == NULL )
		WinExec(cmdline.c_str(), SW_SHOW);
}

string GetDefaultApplicationForFile (const string& path)
{
	string appPath;

	wchar_t wfile[kDefaultPathBufferSize];
	wchar_t wapp[kDefaultPathBufferSize];

	ConvertUnityPathName(path.c_str(), wfile + 1, kDefaultPathBufferSize - 1);

	wfile[0] = L'"';
	wfile[wcslen(wfile) + 1] = 0;
	wfile[wcslen(wfile)] = L'"';

	if ((int)FindExecutableW(wfile, NULL, wapp) <= 32)
		return string();

	ConvertWindowsPathName(wapp, appPath);

	return appPath;
}

//----------------------------------------------------------------------------------------------------
wchar_t *g_openSaveDialogOpenButtonText;
RunComplexOpenPanelDelegate *g_isValidFileNameDelegate;
RunComplexOpenPanelDelegate *g_shouldShowFileName;
void *gOpenSaveDialogUserData = NULL;
bool g_isOpenDialog;

UINT_PTR CALLBACK OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	OFNOTIFY *notify;

	if (uiMsg == WM_NOTIFY)
	{
		notify = (OFNOTIFY*)lParam;

		switch (notify->hdr.code)
		{
		case CDN_INITDONE:
			if (g_openSaveDialogOpenButtonText != NULL)
				SetDlgItemTextW(notify->hdr.hwndFrom, IDOK, g_openSaveDialogOpenButtonText);
			return 0;
		case CDN_SELCHANGE: 
			if (g_isOpenDialog)	// for save dialog this works totally wrong
			{
				if (g_isValidFileNameDelegate != NULL)
				{
					string path;
					wchar_t buf[kDefaultPathBufferSize];

					SendMessageW(notify->hdr.hwndFrom, CDM_GETFILEPATH, kDefaultPathBufferSize, (LPARAM)buf);
					ConvertWindowsPathName(buf, path);
			
					EnableWindow(GetDlgItem(notify->hdr.hwndFrom, IDOK), g_isValidFileNameDelegate(gOpenSaveDialogUserData, path));
				}
			}
			return 0;
		case CDN_FILEOK:
			if (g_isValidFileNameDelegate != NULL)
			{
				string path;
				
				ConvertWindowsPathName((wchar_t*)notify->lpOFN->lpstrFile, path);
				if (!g_isValidFileNameDelegate(gOpenSaveDialogUserData, path))
				{
					MessageBoxW(hdlg, L"Selected file name is not valid.", (LPWSTR)notify->lpOFN->lpstrTitle, MB_OK | MB_ICONINFORMATION);
					SetWindowLong(hdlg, DWLP_MSGRESULT, IDCANCEL);
					return 1;
				}
			}
			return 0;
		}
	}

	return 1;
}


static HRESULT CheckShellItem(IShellItem* psi, RunComplexOpenPanelDelegate checkFunc)
{
	HRESULT returnCode = S_OK;

	WCHAR* pszPath;
	HRESULT hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
	if(SUCCEEDED(hr))
	{
		std::string path;
		ConvertWindowsPathName(pszPath, path);

		if(!checkFunc(gOpenSaveDialogUserData, path))
			returnCode = S_FALSE;

		CoTaskMemFree(pszPath);
	}

	return returnCode;
}


// IFileDialogEvents implementation for accepting or rejecting the selection in the Vista file dialog
class CFileDialogEvents :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CFileDialogEvents>,
	public IFileDialogEvents
{
public:
	CFileDialogEvents() {}
	~CFileDialogEvents() {}

	BEGIN_COM_MAP(CFileDialogEvents)
		COM_INTERFACE_ENTRY(IFileDialogEvents)
	END_COM_MAP()
 
	STDMETHODIMP OnFileOk(IFileDialog* pfd)
	{
		HRESULT hr;
		IShellItem* psi;

		hr = pfd->GetResult(&psi);
		if(SUCCEEDED(hr))
		{
			HRESULT hr = CheckShellItem(psi, g_isValidFileNameDelegate);
			psi->Release();
			return hr;
		}

		return hr;
	}

	STDMETHODIMP OnFolderChanging(IFileDialog* pfd, IShellItem* psiFolder) { return E_NOTIMPL; }
	STDMETHODIMP OnFolderChange(IFileDialog* pfd) { return E_NOTIMPL; }
	STDMETHODIMP OnSelectionChange(IFileDialog* pfd) { return E_NOTIMPL; }
	STDMETHODIMP OnShareViolation(IFileDialog* pfd, IShellItem* psi, FDE_SHAREVIOLATION_RESPONSE* pResponse) { return E_NOTIMPL; }
	STDMETHODIMP OnTypeChange(IFileDialog* pfd) { return E_NOTIMPL; }
	STDMETHODIMP OnOverwrite(IFileDialog* pfd, IShellItem* psi, FDE_OVERWRITE_RESPONSE* pResponse) { return E_NOTIMPL; }
};

// IShellItemFilter implementation for filtering out files in the Vista file dialog
class CShellItemFilter :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CShellItemFilter>,
    public IShellItemFilter
{
public:
	CShellItemFilter() {}
	~CShellItemFilter() {}
 
	BEGIN_COM_MAP(CShellItemFilter)
		COM_INTERFACE_ENTRY(IShellItemFilter)
	END_COM_MAP()
 
	STDMETHODIMP IncludeItem(IShellItem *psi)
	{
		SFGAOF attribs;
		if(SUCCEEDED(psi->GetAttributes(SFGAO_FOLDER, &attribs)))
		{
			if((attribs & SFGAO_FOLDER) != 0)
				return S_OK;
		}

		if(g_shouldShowFileName != NULL)
			return CheckShellItem(psi, g_shouldShowFileName);

		return S_OK;
	}

	STDMETHODIMP GetEnumFlagsForItem(IShellItem *psi, SHCONTF *pgrfFlags)
	{
		return E_NOTIMPL;
	}
};

static string DoOpenOrSavePanel(const std::string& title, const std::string& directory, const std::string& defaultName, const std::string& extension, bool open)
{
	wchar_t widePathName[kDefaultPathBufferSize];
	wchar_t szFileTitle[256];
	wchar_t initialDir[kDefaultPathBufferSize];
	wchar_t titleWide[256];

	g_isOpenDialog = open;

	UTF8ToWide( defaultName.c_str(), widePathName, kDefaultPathBufferSize );

	UTF8ToWide(title.c_str(), titleWide, 256);
	ConvertUnityPathName (PathToAbsolutePath(directory).c_str(), initialDir, kDefaultPathBufferSize);

	std::string result;

	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();
	EnableAllContainerWindows(false);

	if(winutils::GetWindowsVersion() >= winutils::kWindowsVista)
	{
		IFileDialog* pfd;

		HRESULT hr = CoCreateInstance(open ? CLSID_FileOpenDialog : CLSID_FileSaveDialog, 
									  NULL, 
									  CLSCTX_INPROC_SERVER, 
									  IID_PPV_ARGS(&pfd));

		if(SUCCEEDED(hr))
		{
			if(g_openSaveDialogOpenButtonText != NULL)
				pfd->SetOkButtonLabel(g_openSaveDialogOpenButtonText);

			pfd->SetTitle(titleWide);

			ITEMIDLIST* pidl;
			ULONG attr;
			hr = SHParseDisplayName(initialDir, NULL, &pidl, 0, &attr);
			if(SUCCEEDED(hr))
			{
				IShellItem* psi;
				hr = SHCreateShellItem(NULL, NULL, pidl, &psi);
				if(SUCCEEDED(hr))
				{
					pfd->SetFolder(psi);
					psi->Release();
				}
				CoTaskMemFree(pidl);
			}

			pfd->SetFileName(widePathName);

			if(!extension.empty())
			{
				wchar_t extensionWide[MAX_PATH];
				wchar_t szMyFilter[256];

				UTF8ToWide(extension.c_str(), extensionWide, MAX_PATH);
				wsprintfW(szMyFilter, L"*.%s", extensionWide);

				COMDLG_FILTERSPEC filters[2] =
				{
					{ extensionWide, szMyFilter },
					{ L"All files", L"*.*" }
				};
				pfd->SetFileTypes(2, filters);
				pfd->SetDefaultExtension(extensionWide);
			}
			else
			{
				COMDLG_FILTERSPEC filter = { L"All files", L"*.*" };
				pfd->SetFileTypes(1, &filter);
			}

			CComObjectStackEx<CShellItemFilter> shellItemFilter;
			if(g_shouldShowFileName != NULL)
				pfd->SetFilter(&shellItemFilter);

			bool advised = false;
			DWORD adviseCookie;
			CComObjectStackEx<CFileDialogEvents> eventHandler;
			if(g_isValidFileNameDelegate != NULL)
			{
				HRESULT hr = pfd->Advise(&eventHandler, &adviseCookie);
				advised = SUCCEEDED(hr);
			}

			hr = pfd->Show(GetMainEditorWindow());

			if(advised)
				pfd->Unadvise(adviseCookie);

			if(SUCCEEDED(hr))
			{
				IShellItem *psiResult;
				hr = pfd->GetResult(&psiResult);
		        
				if(SUCCEEDED(hr))
				{
					WCHAR *pszPath;
		            
					hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
					if(SUCCEEDED(hr))
					{
						ConvertWindowsPathName(pszPath, result);
						CoTaskMemFree(pszPath);
					}

					psiResult->Release();
				}
			}
			pfd->Release();
		}
	}
	else
	{
		wchar_t extensionWide[MAX_PATH];
		wchar_t szMyFilter[256];
		if (!extension.empty())
		{
			UTF8ToWide(extension.c_str(), extensionWide, MAX_PATH);
			wsprintfW(szMyFilter, L"%s?*.%s?All files?*.*??", extensionWide, extensionWide);
		}else
		{
			extensionWide[0] = 0;
			wcscpy(szMyFilter, L"All files?*.*??");
		}

		int stlen = wcslen(szMyFilter);
		for (int i = 0; i < stlen; i++)
			if (szMyFilter[i] == L'?')
				szMyFilter[i] = 0;

		OPENFILENAMEW ofn;

		ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
		szFileTitle[0] = 0;

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GetMainEditorWindow();
		ofn.hInstance = ::GetModuleHandle(0);
		ofn.lpstrTitle = titleWide;
		ofn.lpstrFilter = szMyFilter;
		ofn.lpstrFile = widePathName;
		ofn.nMaxFile = kDefaultPathBufferSize;
		ofn.lpstrFileTitle = szFileTitle;
		ofn.nMaxFileTitle = 256;
		ofn.lpstrInitialDir = initialDir;
		ofn.lpstrDefExt = extensionWide;

		if ((g_isValidFileNameDelegate != NULL) || (g_openSaveDialogOpenButtonText != NULL))
		{
			ofn.Flags = OFN_ENABLEHOOK;
			ofn.lpfnHook = OFNHookProc;
		}

		if (open)
		{
			ofn.Flags |= OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_HIDEREADONLY;
			if (::GetOpenFileNameW(&ofn) != 0)
				ConvertWindowsPathName(ofn.lpstrFile, result);
		}
		else
		{
			ofn.Flags |= OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
			if (::GetSaveFileNameW(&ofn) != 0)
				ConvertWindowsPathName(ofn.lpstrFile, result);
		}
	}

	EnableAllContainerWindows(true);
	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);

	return result;
}

std::string RunOpenPanel (const std::string& title, const std::string& directory, const std::string& extension)
{
	g_openSaveDialogOpenButtonText = NULL;
	g_isValidFileNameDelegate = NULL;
	g_shouldShowFileName = NULL;
	gOpenSaveDialogUserData = NULL;
	return DoOpenOrSavePanel(title, directory, std::string(), extension, true);
}

std::string RunSavePanel (const std::string& title, const std::string& directory, const std::string& extension, const std::string& defaultName)
{
	g_openSaveDialogOpenButtonText = NULL;
	g_isValidFileNameDelegate = NULL;
	g_shouldShowFileName = NULL;
	gOpenSaveDialogUserData = NULL;
	return DoOpenOrSavePanel(title, directory, defaultName, extension, false);
}

static bool ValidateOpenFolder( const wchar_t* path, std::wstring& outInfoMessage )
{
	if ((NULL == path) || (0 == path[0]))
	{
		outInfoMessage = L"Select folder.";
		return false;
	}

	if (!PathIsDirectoryW(path))
	{
		outInfoMessage = L"Select folder.";
		return false;
	}

	return true;
}

static bool ValidateSaveFolder( const wchar_t* path, std::wstring& outInfoMessage )
{
	if ((NULL == path) || (0 == path[0]))
	{
		outInfoMessage = L"Select folder.";
		return false;
	}

	if (!PathFileExistsW(path))
	{
		return true;
	}

	if (!PathIsDirectoryW(path))
	{
		outInfoMessage = L"Select folder.";
		return false;
	}

	return true;
}

string RunOpenFolderPanel (const string& title, const string& folder, const string& defaultName)
{
	return RunSaveFolderPanel (title, folder, defaultName, false);
}

std::string RunSaveFolderPanel (const std::string& title,
                                const std::string& directory,
                                const std::string& defaultName,
                                bool canCreateFolder)
{
	std::wstring titleWide, folderWide, defaultNameWide;

	ConvertUTF8ToWideString(title, titleWide);
	ConvertUnityPathName(PathToAbsolutePath(directory), folderWide);
	ConvertUTF8ToWideString(defaultName, defaultNameWide);

	if( GetApplicationPtr() )
		GetApplication().DisableUpdating();
	EnableAllContainerWindows(false);

	std::string result = BrowseForFolderDialog (GetMainEditorWindow (),
												titleWide.c_str(),
	                                            folderWide.c_str(),
												defaultNameWide.c_str(),
	                                            !canCreateFolder,
	                                            ValidateSaveFolder);

	EnableAllContainerWindows(true);
	if( GetApplicationPtr() )
		GetApplication().EnableUpdating(false);

	return result;
}

std::string RunComplexOpenPanel (const std::string& title, const std::string message, const string& openButton, const std::string& directory, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData)
{
	// shouldShowFileName is only used on Vista or better. Standard win dialog can't hide specific files.

	if (openButton.empty())
	{
		g_openSaveDialogOpenButtonText = NULL;
	}else
	{
		static wchar_t buf[50];
		UTF8ToWide(openButton.c_str(), buf, 50);
		g_openSaveDialogOpenButtonText = buf;
	}

	g_shouldShowFileName = shouldShowFileName;
	g_isValidFileNameDelegate = isValidFilename;
	gOpenSaveDialogUserData = userData;

	return DoOpenOrSavePanel(title, directory, string(), string(), true);
	
}

std::string RunComplexSavePanel (const std::string& title, const std::string message, const std::string& openButton, const std::string& directory, const std::string& defaultName, const std::string& extension, RunComplexOpenPanelDelegate* isValidFilename, RunComplexOpenPanelDelegate* shouldShowFileName, void* userData)
{
	// shouldShowFileName is only used on Vista or better. Standard win dialog can't hide specific files.

	g_openSaveDialogOpenButtonText = NULL; // does not work with save panel (and no need for that)
	g_isValidFileNameDelegate = isValidFilename;
	g_shouldShowFileName = shouldShowFileName;
	gOpenSaveDialogUserData = userData;

	return DoOpenOrSavePanel(title, directory, defaultName, extension, false);
}

//----------------------------------------------------------------------------------------------------

void OpenURLInWebbrowser (const string& url)
{
	OpenURL(url);
}

void OpenPathInWebbrowser (const string& path)
{
	OpenURL("file://"+path);
}

bool SelectFileWithFinder (const std::string& path)
{
	RevealInFinder(path);
	return true;
}

bool IsSamePathInternal(string sourcePath1, BSTR sourcePath2)
{
	wchar_t* urlPrefix = L"file:///";
	wchar_t path1[1000], convertedPath[1000], path2[1000];
	LPWSTR fileName;

	if (wcsncmp(urlPrefix, sourcePath2, wcslen(urlPrefix)) != 0)
		return false;

	ConvertUnityPathName(sourcePath1.c_str(), convertedPath, 1000);

	if (!GetFullPathNameW(sourcePath2 + wcslen(urlPrefix), 1000, path1, NULL))
		return false;

	if (!GetFullPathNameW(convertedPath, 1000, path2, &fileName))
		return false;

	if (fileName != NULL)
		fileName[-1] = 0;
	
	return wcscmp(path1, path2) == 0;
}

bool BringExplorerWindowToFrontIfExists(string requestedPath)
{
	CComPtr<IShellWindows> psw;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, NULL,  
		CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&psw))))
	{
		VARIANT v = { VT_I4 };
		if (SUCCEEDED(psw->get_Count(&v.lVal)))
		{
			while (--v.lVal >= 0)
			{
				CComPtr<IDispatch> pdisp;
				if (S_OK == psw->Item(v, &pdisp))
				{
					CComPtr<IWebBrowser2> pwb;
					if (SUCCEEDED(pdisp->QueryInterface(IID_PPV_ARGS(&pwb))))
					{
						CComBSTR locationURL;
						pwb->get_LocationURL(&locationURL);
						
						//Ugly bstr -> string conversion
						std::wstring locationWSTR(locationURL);
						std::string str(locationWSTR.begin(),locationWSTR.end());
						str.assign(locationWSTR.begin(),locationWSTR.end());
						
						//The URL that is found can have URL encoding... decode it first!
						CComBSTR decodedURLBSTR((CurlUrlDecode (str)).c_str ());

						if (IsSamePathInternal(requestedPath, decodedURLBSTR))
						{
							HWND hwnd;
							pwb->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&hwnd));
							ShowWindow(hwnd,SW_SHOWNORMAL);
							SetForegroundWindow(hwnd);
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

void RevealInFinder (const string& path)
{
	// Always take the absolute path. Explorer doesn't always obey CWD when opening up relative paths. Case 491381
	string absolutePath = PathToAbsolutePath (path);

	if (BringExplorerWindowToFrontIfExists(absolutePath))
		return;

	wchar_t buffer[kDefaultPathBufferSize + 10];
	buffer[0] = NULL;

	wcscat(buffer, L"/select, \"");
	ConvertUnityPathName(absolutePath.c_str(), buffer + wcslen(buffer), kDefaultPathBufferSize);
	wcscat(buffer, L"\"");

	ShellExecuteW(NULL, L"open", L"explorer.exe", buffer, NULL, SW_SHOWNORMAL);
}


void UnityBeep()
{
	MessageBeep(MB_OK);
}

#if !UNITY_RELEASE
bool AmIBeingDebugged() 
{ 
	return IsDebuggerPresent(); 
}
#else
bool AmIBeingDebugged()
{ 
	return false;
}
#endif

std::string GetBundleVersionForApplication (const std::string& path)
{
	VS_FIXEDFILEINFO fInfo;

	LPVOID	lpInfo;
	UINT	len;
	DWORD	dwDummyHandle; // will always be set to zero
	wchar_t	wpath[kDefaultPathBufferSize];
	BYTE	*versionInfo;
	char	ver[64];

	ConvertUnityPathName(path.c_str(), wpath, kDefaultPathBufferSize);

	len = GetFileVersionInfoSizeW(wpath, &dwDummyHandle);
	if (len <= 0)
		return string();

	versionInfo = new BYTE[len]; // allocate version info
	if (!GetFileVersionInfoW(wpath, 0, len, versionInfo))
		return string();

	if (!VerQueryValueW(versionInfo, L"\\", &lpInfo, &len))
		return string();

	memcpy(&fInfo, lpInfo, sizeof(VS_FIXEDFILEINFO));
	delete[] versionInfo;

	if (fInfo.dwSignature != VS_FFI_SIGNATURE)
		return string();
	
	sprintf(ver, "%d.%d.%d.%d", HIWORD(fInfo.dwFileVersionMS), LOWORD(fInfo.dwFileVersionMS), HIWORD(fInfo.dwFileVersionLS), LOWORD(fInfo.dwFileVersionLS));

	return ver;
}

static INT_PTR CALLBACK GeneralProgressDialogProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
	case WM_INITDIALOG:
		winutils::CenterWindowOnParent(hDlg);
		SetForegroundWindow( hDlg );
		SendDlgItemMessage( hDlg, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, kProgressBarRange) );
		ShowInTaskbarIfNoMainWindow(hDlg);
		return TRUE;
	case WM_COMMAND:
		if (wParam == IDCANCEL)
		{
			s_ProgressBar.wantsToCancel = TRUE;
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

static void ProcessMessages(HWND hDlg)
{
	MSG  msg;
	while (PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE))
	{
		if (WM_QUIT != msg.message)
		{
			GetMessage( &msg, NULL, 0U, 0U );

			// later on escape keys would be eaten for some... WinAPI reasons
			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
				s_ProgressBar.wantsToCancel = TRUE;

			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
			return;
	}
	if (s_ProgressBar.wantsToCancel)
	{
		HWND hButton = GetDlgItem(hDlg, IDCANCEL);
		if (IsWindow(hButton))
		{
			EnableWindow(hButton, FALSE);
			UpdateWindow(hButton);
		}
	}
}

ProgressBarState DisplayBuildProgressbar (const std::string& title, const std::string& text, Vector2f buildInterval, bool canCancel /*=false*/)
{
	s_ProgressBar.buildInterval = buildInterval;
	if (s_ProgressBar.IsBuildInProgress())
	{
		return DisplayProgressbar (title, text, 0.0f, canCancel);
	}
	return kPBSNone;
}

ProgressBarState DisplayProgressbar (const std::string& title, const std::string& text, float value, bool canCancel /*=false*/)
{
	if( IsBatchmode() )
	{
		static std::string prevTitle;
		if (title != prevTitle) {
			printf_console("DisplayProgressbar: %s\n", title.c_str());
			prevTitle = title;
		}
		return kPBSNone;
	}

	if (canCancel != s_ProgressBar.hasCancel)
		ClearProgressbar();


	wchar_t textWide[kDefaultPathBufferSize];
	if( !IsWindow(s_ProgressBar.window) )
	{
		HWND handle = NULL;

		GUIView *view = GetKeyGUIView();

		if (view)
			handle = view->GetWindowHandle();
		else
			handle = GetMainEditorWindow();

		if (canCancel)
			EnableAllViews(false);

		s_ProgressBar.window = CreateDialogW( winutils::GetInstanceHandle(), 
			MAKEINTRESOURCEW(canCancel ? IDD_PROGRESS_WITHCANCEL : IDD_PROGRESS), handle, GeneralProgressDialogProc  );
		s_ProgressBar.hasCancel = canCancel;
		s_ProgressBar.wantsToCancel = FALSE;

		// Always set title when creating a window
		ConvertUnityPathName( title.c_str(), textWide, kDefaultPathBufferSize );
		SetWindowTextW( s_ProgressBar.window, textWide);

		ShowWindow(s_ProgressBar.window, SW_NORMAL);
	}

	if (canCancel)
	{
		GetApplication().DisableUpdating();
		ProcessMessages(s_ProgressBar.window);
		GetApplication().EnableUpdating(false);
	}

	// If we building to another platform we override progress with build progress
	float progress = value;
	if (s_ProgressBar.IsBuildInProgress())
	{
		// Any progress value here is considered relative to the current build progress interval.
		// This lets child processes have their progress displayed as part of the overall build progress
		Vector2f interval = s_ProgressBar.buildInterval;
		progress = (interval.y - interval.x)*value + interval.x;
	}

	// Update progress bar and text
	
	// Progress bars in Windows are animated which does not fit with our usage of them since they will not show what progress we throw at it. 
	// To fix this we need the following dance below. See others with same issue here: http://stackoverflow.com/questions/2217688/windows-7-aero-theme-progress-bar-bug
	// Fixes case 536410: "ProgressBar displays incorrect progress". 
	int step = (int)(progress * kProgressBarRange);
	if (step >= kProgressBarRange)
	{
		// 100%
		SendDlgItemMessage( s_ProgressBar.window, IDC_PROGRESS, PBM_SETPOS, step, 0 );
		SendDlgItemMessage( s_ProgressBar.window, IDC_PROGRESS, PBM_SETPOS, step-1, 0 );
		SendDlgItemMessage( s_ProgressBar.window, IDC_PROGRESS, PBM_SETPOS, step, 0 );
	}
	else
	{
		// < 100%
		SendDlgItemMessage( s_ProgressBar.window, IDC_PROGRESS, PBM_SETPOS, step+1, 0 );
		SendDlgItemMessage( s_ProgressBar.window, IDC_PROGRESS, PBM_SETPOS, step, 0 );
	}
	
	ConvertUnityPathName( text.c_str(), textWide, kDefaultPathBufferSize );
	SetDlgItemTextW( s_ProgressBar.window, IDC_ST_PATH, textWide);

	ConvertUnityPathName( title.c_str(), textWide, kDefaultPathBufferSize );
	SetWindowTextW( s_ProgressBar.window, textWide);

	UpdateWindows7ProgressBar (progress);

	// Repaint
	RedrawWindow( s_ProgressBar.window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW );


	return s_ProgressBar.wantsToCancel ? kPBSWantsToCancel : kPBSNone;
}

void ClearProgressbar ()
{
	if( IsBatchmode() )
		return;

	if( !IsWindow(s_ProgressBar.window) )
		return;

	// During build we do not allow to clear the progress bar, this
	// prevents spawning OSX user attention requests during the build process.
	// The build process clears the progress bar on its cleanup.
	if (!s_ProgressBar.IsBuildInProgress())
	{
		ClearWindows7ProgressBar ();

		// re-enable views
		if (s_ProgressBar.hasCancel)
			EnableAllViews(true);

		s_ProgressBar.Clear ();
	}
}

void SetCocoaCurrentDirectory( std::string path )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );
	::SetCurrentDirectoryW( widePath );
}

string GetCocoaCurrentDirectory()
{
	wchar_t widePath[kDefaultPathBufferSize];
	widePath[0] = 0;
	GetCurrentDirectoryW( kDefaultPathBufferSize, widePath );
	std::string path;
	ConvertWindowsPathName( widePath, path );
	return path;
}

bool IsApplicationActiveOSImpl ()
{
	return GetGAppActive();
}

void ActivateApplication ()
{
	SetForegroundWindow(GetMainEditorWindow());
}

bool IsOptionKeyDown()
{
	return GetKeyState(VK_MENU) < 0;
}

bool CopyFileOrDirectoryFollowSymlinks (const string& from, const string& to)
{
	return CopyFileOrDirectory(from, to);
}

// -----------------------------------------------------------------------------

// Based on:
/*
Twilight Prophecy SDK
A multi-platform development system for virtual reality and multimedia.
Copyright (C) 1997-2003 Twilight 3D Finland Oy Ltd.

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

static const unsigned char* s_pngInputPtr;
static size_t s_pngInputSizeLeft;

static void png_user_read_func( png_structp png_ptr, png_bytep data, png_size_t len )
{
	// check for overflow
	if( len > s_pngInputSizeLeft )
		len = s_pngInputSizeLeft;
	memcpy( data, s_pngInputPtr, len );
	s_pngInputPtr += len;
	s_pngInputSizeLeft -= len;
}


static void png_user_warning_fn( png_struct* png_ptr, png_const_charp warning_msg )
{
}


static bool png_decode( const void* data, size_t size, unsigned char*& outData, int& outWidth, int& outHeight )
{
	s_pngInputPtr = reinterpret_cast<const unsigned char*>(data);
	s_pngInputSizeLeft = size;

	if( !s_pngInputPtr )
		return false;

	// check png header
	if( s_pngInputSizeLeft < 8 || !png_check_sig( const_cast<unsigned char*>(s_pngInputPtr), 8 ) )
		return false;

	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;

	double image_gamma = 0.45;
	int number_passes = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,&png_user_warning_fn);
	if( png_ptr == NULL )
		return false;

	info_ptr = png_create_info_struct(png_ptr);
	if( info_ptr == NULL )
	{
		png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL);
		return false;
	}

	if( setjmp(png_ptr->jmpbuf) )
	{
		png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL);
		return false;
	}

	png_set_read_fn( png_ptr,(void*)NULL,&png_user_read_func );
	png_read_info( png_ptr, info_ptr );

	png_get_IHDR( png_ptr,info_ptr,&width,&height,&bit_depth,&color_type, &interlace_type, NULL, NULL );

	png_set_strip_16(png_ptr); // strip 16 bit channels to 8 bit
	png_set_packing(png_ptr);  // separate palettized channels

	// figure out format
	int bpp = 4;
	outWidth = width;
	outHeight = height;

	// palette -> rgb
	if( color_type == PNG_COLOR_TYPE_PALETTE ) png_set_expand(png_ptr);

	// grayscale -> 8 bits
	if( !(color_type & PNG_COLOR_MASK_COLOR) && bit_depth < 8 ) png_set_expand(png_ptr);

	// if exists, expand tRNS to alpha channel
	if ( png_get_valid(png_ptr,info_ptr,PNG_INFO_tRNS) ) png_set_expand(png_ptr);

	// expand gray to RGB
	if( color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb(png_ptr);

	double screen_gamma = 2.0f;

	image_gamma = 0.0;
	if ( png_get_gAMA(png_ptr,info_ptr,&image_gamma) )
	{
		png_set_gamma(png_ptr,screen_gamma,image_gamma);
	}
	else
	{
		png_set_gamma(png_ptr,screen_gamma,0.45);
	}

	png_set_bgr(png_ptr);         // flip RGBA to BGRA

	if (color_type == PNG_COLOR_TYPE_RGB)
		png_set_filler( png_ptr, 0xFF, PNG_FILLER_AFTER ); // force alpha byte

	number_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr,info_ptr); // update gamma, etc.

	// allocate image
	int numbytes = width * height * bpp;
	outData = new unsigned char[numbytes];
	if( !outData )
	{
		png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp)NULL);
		return false;
	}

	memset( outData, 0, numbytes );

	png_bytep* row_pointers = new png_bytep[height];
	for( png_uint_32 row=0; row<height; ++row )
	{
		row_pointers[row] = outData + row*width*bpp;
	}

	for( int pass=0; pass<number_passes; pass++ )
	{
		png_read_rows(png_ptr,row_pointers,NULL,height);
	}

	// cleanup
	png_read_end( png_ptr,info_ptr );
	png_destroy_read_struct( &png_ptr,&info_ptr,(png_infopp)NULL );
	delete[] row_pointers;

	// premultiply alpha!
	unsigned int index = 0;
	for( unsigned int i = 0; i < width * height; ++i )
	{
		int alpha = outData[index+3];
		outData[index+0] = unsigned int(outData[index+0]) * alpha / 255;
		outData[index+1] = unsigned int(outData[index+1]) * alpha / 255;
		outData[index+2] = unsigned int(outData[index+2]) * alpha / 255;
		index += 4;
	}

	return true;
}

HBITMAP LoadPNGFromResource( HDC dc, unsigned int resourceID, int& width, int& height )
{
	HINSTANCE instance = winutils::GetInstanceHandle();
	HBITMAP bitmap = NULL;
	width = 0;
	height = 0;
	HRSRC resource = FindResource( instance, MAKEINTRESOURCEA(resourceID), "BINARY" );
	if( resource )
	{
		DWORD size = SizeofResource( instance, resource );
		HGLOBAL glob = LoadResource( instance, resource );
		if( glob )
		{
			void* data = LockResource( glob );
			if( data )
			{
				unsigned char* bgra;
				if( !png_decode( data, size, bgra, width, height ) )
					return bitmap;

				BITMAPINFO bi;
				memset( &bi, 0, sizeof(bi) );
				bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
				bi.bmiHeader.biWidth = width;
				bi.bmiHeader.biHeight = -height;
				bi.bmiHeader.biBitCount = 32;
				bi.bmiHeader.biPlanes = 1;
				bi.bmiHeader.biCompression = BI_RGB;
				unsigned char* bitmapData = NULL;
				bitmap = CreateDIBSection( dc, &bi, DIB_RGB_COLORS, (void**)&bitmapData, NULL, 0 );
				if( bitmap ) {
					memcpy(bitmapData, bgra, width*height*4);
				}
				
				delete [] bgra;
			}
		}
	}
	return bitmap;
}

// ----------------------------------------------------------------------

static HCURSOR s_BusyCursor = NULL;
static HCURSOR s_RestoreBusyCursor = NULL;
static int s_BusyCursorCounter = 0;

static void ProcessBusyCursor( int delta )
{
	// 0 => ensure it's shown, 1 => begin, -1 => end
	AssertIf( delta < -1 || delta > 1 );

	if( s_BusyCursor == NULL ) {
		s_BusyCursor = ::LoadCursor(NULL, IDC_WAIT);
		if( s_BusyCursor == NULL )
			return;
	}

	s_BusyCursorCounter += delta;
	if( s_BusyCursorCounter > 0 )
	{
		HCURSOR prev = ::SetCursor(s_BusyCursor);
		if( delta > 0 && s_BusyCursorCounter == 1 )
			s_RestoreBusyCursor = prev;
	}
	else
	{
		s_BusyCursorCounter = 0; // prevent underflow
		::SetCursor( s_RestoreBusyCursor );
	}
}
void BeginBusyCursor() { ProcessBusyCursor(1); }
void EndBusyCursor() { ProcessBusyCursor(-1); }
void EnsureBusyCursor() { ProcessBusyCursor(0); }

bool CheckIPhoneXCodeInstalled ()
{
	return false;
}

bool CheckIPhoneXCode4Installed ()
{
	return false;
}

void LaunchIPhoneXCode4 ()
{
}

void TerminateIPhoneXCode4 ()
{
}


ExternalTask::ExternalTask() 
{
	task.hProcess = NULL;
	task.dwProcessId = 0;
}

std::auto_ptr<ExternalTask> ExternalTask::LauchTask(const std::string& taskPath, const std::vector<std::string>& arguments)
{
	std::auto_ptr<ExternalTask> task(new ExternalTask());

	// " symbol has to be escaped, because args are quoted in LaunchTaskArrayOptions
	std::vector<std::string> args = arguments;
	for (size_t i = 0; i < args.size(); ++i)
		replace_string(args[i], "\"", "\\\"");

	if (!LaunchTaskArrayOptions( taskPath, args, kLaunchQuoteArgs|kLaunchBackground, taskPath.substr(0, taskPath.find_last_of(kPathNameSeparator)), task->task))
		task.reset();

	return task;
}

ExternalTask::~ExternalTask() 
{
	if (task.hProcess)
		Terminate();
}

bool ExternalTask::IsRunning() const
{
	Assert(task.hProcess && task.dwProcessId);
	return IsProcessRunning(task.dwProcessId);
}

void ExternalTask::Terminate()
{
	Assert(task.hProcess && task.dwProcessId);
	TerminateProcess(task.hProcess, 0);
	task.hProcess = NULL;
}

std::auto_ptr<ExternalTask> ExternalTask::AttachWatchDog() const
{
	std::auto_ptr<ExternalTask> watchDogTask(new ExternalTask());

	if (!::AttachWatchDog(watchDogTask->task, task.dwProcessId))
		watchDogTask.reset();

	return watchDogTask;
}

void ExternalTask::Sleep(int miliseconds)
{
	::Sleep(miliseconds);
}


bool AttachWatchDog(PROCESS_INFORMATION& procInfo, int pid)
{
	vector<string> args;
	char buf[8];

	string app = AppendPathName(GetApplicationFolder(), "Data/Tools/auto_quitter.exe");

	args.push_back(_itoa(GetCurrentProcessId(), buf, 10));
	args.push_back(_itoa(pid, buf, 10));

	return LaunchTaskArrayOptions(app, args, kLaunchQuoteArgs|kLaunchBackground, app.substr(0, app.find_last_of(kPathNameSeparator)), procInfo);
}

