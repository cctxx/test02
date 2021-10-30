#include "UnityPrefix.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Misc/BuildSettings.h"
#include "PlatformDependent/Win/MonoLoader.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/GfxDevice/GfxDeviceSetup.h"
#include "Editor/Src/Application.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Configuration/UnityConfigureRevision.h"
#include "Configuration/UnityConfigureOther.h"
#include "PlatformDependent/AndroidPlayer/../../Tools/BugReporterWin/lib/CrashHandler.h"
#include "resource.h"
#include "DragAndDropImpl.h"
#include "QuicktimeUtils.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/Audio/AudioManager.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Utility/SplashScreen.h"
#include "PlatformDependent/Win/SingleAppInstance.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Configuration/UnityConfigure.h"
#include "PlatformDependent/Win/WinGraphicsSelection.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/GfxDevice/d3d/D3D9Utils.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "PlatformDependent/Win/wow64.h"
#include "Runtime/Graphics/SubstanceArchive.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Platform/Windows/LicenseWebViewWindow.h"

#if SUPPORT_IPHONE_REMOTE
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#endif 

#if ENABLE_UNIT_TESTS && !DEPLOY_OPTIMIZED
#include "Runtime/Testing/Testing.h"
#endif

#include <shlobj.h>

#if MONO_2_10 || MONO_2_12
static string kEditorMonoRuntimeVersion = "MonoBleedingEdge";
#else
static string kEditorMonoRuntimeVersion = "Mono";
#endif

bool IsInputInitialized(void);

// MenuControllerWin.cpp
void UpdateMainMenu();
bool ExecuteMenuItemWithID( int id );
HACCEL GetMainMenuAccelerators();
void ShutdownMainMenu();

// LogAssert.cpp
FILE* OpenConsole();
void CloseConsoleFile();

void DisplayBleedingEdgeWarningIfRequired();

static HWND gMainWindow;
bool gAppActive = true;
bool gInitialized = false;
bool gAlreadyClosing = false;

bool g_BatchMode = false;

CrashHandler* gUnityCrashHandler = NULL;

static SingleAppInstance g_SingleAppInstance("UnityEditor");

extern char* gReporter = "UnityBugReporter.exe"; // BugReportingTools.cpp

DWORD g_TaskbarButtonCreatedMessage;


HWND GetMainEditorWindow() { return gMainWindow; }
void SetMainEditorWindow( HWND window ) {
	gMainWindow = window;
}
BOOL GetGAppActive() { return gAppActive; };

static bool gMainDocumentEdited = false;
static std::string gCurrentMainWindowTitle;
static std::string gCurrentMainWindowFile;

void SetMainWindowFileName( const std::string& title, const std::string& file )
{
	if (g_BatchMode)
		return;

	gCurrentMainWindowTitle = title;
	gCurrentMainWindowFile = file;

	std::wstring wideTitle;
	#if UNITY_64
	std::string windowTitle = "Unity (64bit) - " + title;
	#else
	std::string windowTitle = "Unity - " + title;
	#endif
	if( gMainDocumentEdited )
		windowTitle += '*';
	

	switch(GetGfxDevice().GetRenderer())
	{
	case kGfxRendererD3D9: break; // nothing
	case kGfxRendererD3D11:
		if (gGraphicsCaps.shaderCaps >= kShaderLevel5)
			windowTitle += " <DX11>";
		else if (gGraphicsCaps.shaderCaps >= kShaderLevel4)
			windowTitle += " <DX11 on DX10 GPU>";
		else
			windowTitle += " <DX11 on DX9 GPU>";
		break;
	case kGfxRendererOpenGL: windowTitle += " <OpenGL>"; break;
	case kGfxRendererOpenGLES20Mobile: windowTitle += " <OpenGL ES>"; break;
	default: AssertString("Unknown renderer!"); break;
	}

	ConvertUTF8ToWideString( windowTitle, wideTitle );

	SetWindowTextW( gMainWindow, wideTitle.c_str() );

	// TODO: file name
}

void SetMainWindowDocumentEdited (bool edited)
{
	if( edited == gMainDocumentEdited )
		return;
	if (g_BatchMode)
		return;

	gMainDocumentEdited = edited;
	SetMainWindowFileName( gCurrentMainWindowTitle, gCurrentMainWindowFile );
}

void DoQuitEditor()
{
	AssertIf(gAlreadyClosing);

	gAlreadyClosing = true;
	DestroyWindow( gMainWindow );
	ShutdownMainMenu();
	gMainWindow = NULL;
	ShutdownEditorWindowClasses();

	// Important to close any handles to the log file here (our own, stdout, mono stdout).
	// Otherwise next Unity instance won't be able to move old file away.
	CloseConsoleFile();
	//unity_mono_close_output();

	gInitialized = false;
	PostQuitMessage(0);
}


bool IsD3D9DeviceLost(); // GfxDeviceD3D9.cpp


static void Shutdown()
{
	#if SUPPORT_IPHONE_REMOTE
		iPhoneRemoteInputShutdown();
	#endif 
	ReleaseGfxWindowOnAllGUIViews();
	DestroyGfxDevice();

	gInitialized =  false;
}

bool ProcessMainWindowMessages( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result )
{
	LRESULT sim = g_SingleAppInstance.CheckMessage( message, wParam, lParam );
	if( sim ) {
		result = sim;
		return true;
	}

	//if(message!=WM_TIMER && message!=WM_SETCURSOR)
	//	printf_console("msg: %s\n", winutils::GetWindowsMessageInfo(message,wParam,lParam).c_str());

	switch (message) 
	{

	// Ok, this is fun: when we're using swap chains and no "regular window", often we don't get device loss events, even though it
	// looks like the device is put into "lost" state. So whenever anything important changes (resolution, user logs in, etc.),
	// try to manually reset the device and repaint everything. Yay!
	case WM_SETTINGCHANGE:
		// when switching to screen saver, it seems that the only way to detect that is by handling working area change message.
		if( wParam == SPI_SETWORKAREA ) {
			ResetGfxDeviceAndRepaint();
		}
		break;
	case WM_DISPLAYCHANGE:
	case WM_DEVMODECHANGE:
	case WM_USERCHANGED:
		ResetGfxDeviceAndRepaint();
		break;

	case WM_COPYDATA:
		{
			const COPYDATASTRUCT* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
			if( cds ) {
				const char* text = static_cast<const char*>(cds->lpData);
				if( text && strlen(text) > 0 && GetApplicationPtr() )
				{
					std::string fileName = text;
					if ( IsAssetStoreURL( fileName ) )
					{
						GetApplication().OpenAssetStoreURL( fileName );
					}
					else
					{
						ConvertSeparatorsToUnity( fileName );
						GetApplication().OpenFileGeneric( fileName );
					}
					::FlashWindow( gMainWindow, FALSE );
					return TRUE;
				}
			}
		}
		break;
	case WM_INITMENU:
		if( !gInitialized )
			break;
		UpdateMainMenu();
		result = 0;
		return true;
	case WM_COMMAND:
		if( !gInitialized )
			break;
		GetUndoManager().IncrementCurrentGroup();
		if( ExecuteMenuItemWithID(LOWORD(wParam)) ) {
			result = 0;
			return true;
		}
		break;

	case WM_ACTIVATEAPP:
		if( !gInitialized )
			break;

		gAppActive = wParam == TRUE;
		break;

	case WM_DESTROY:
		gAlreadyClosing = true;
		gInitialized = false;
		PostQuitMessage(0);
		break;
	}

	// Have to do dummy process of "win7 taskbar button" message
	if (message == g_TaskbarButtonCreatedMessage)
	{
		result = 0;
		return true;
	}

	return false;
}


static void TranslateAndDispatch (MSG& msg)
{
	if( g_BatchMode || !TranslateAccelerator( gMainWindow, GetMainMenuAccelerators(), &msg ) )
	{
		ResetGfxDeviceIfNeeded();
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
}


static int MainMessageLoop()
{
	BOOL returnValue;
	MSG  msg, lastMsg;
	msg.message = WM_NULL;
	std::vector<MSG> messages;
	PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE );
	bool isQuitSignaled = msg.message == WM_QUIT;

	while( !isQuitSignaled ) 
	{
		ABSOLUTE_TIME startTime = GetStartTime ();
		
		// Process message queue before calling GetApplication().TickTimer() (to update world and repaint views).
		messages.clear();
		while (PeekMessage (&msg, NULL,  0, 0, PM_REMOVE)) 
		{
			if (msg.message == WM_INPUT || msg.message == WM_PAINT) 
			{
				// Process Input and Paint messages first then less important messages afterwards
				// WM_INPUT And WM_PAINT needs to be dispatched immediately to be removed from the queue
				TranslateAndDispatch (msg);
				continue;
			}

			if (msg.message == WM_QUIT)
				isQuitSignaled = true;

			messages.push_back (msg);
			
			// If messages piles up because of low framerate break out to ensure we update/repaint now and then.
			if (messages.size() > 100)
				break;
		}

		for (unsigned i=0; i<messages.size (); ++i)
		{
			msg = messages[i];
			TranslateAndDispatch (msg);
		}

		GetApplication().TickTimer();

		bool active = IsApplicationActive() || GetPlayerRunInBackground () || GetPlayerPause() != kPlayerPaused;
		if (!IsWorldPlaying() || GetApplication().IsPaused() || !active)
		{
			// Sleep to prevent using 100% CPU power if not needed
			float elapsedTime = GetElapsedTimeInSeconds (startTime) * 1000.f; 
			const float minTime = 4.f; // Max 250 FPS (1000/250);
			float sleepTime = minTime - elapsedTime;
			if (sleepTime > 0)
			{
				::Sleep ((DWORD)sleepTime);
			}
		}
	}

	return (INT)msg.wParam;
}

void ModalWinMainMessageLoop(ContainerWindow *win)
{
	MSG  msg;
	do
	{
		if( GetMessage( &msg, NULL, 0U, 0U ) != 0 )
		{
			ResetGfxDeviceIfNeeded();
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		GUIView::RepaintAll(true);
	}
	while (IsWindow(win->GetWindowHandle()));
}

void ProcessDialogMessages(HWND dialog)
{
	MSG msg;

	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (!IsDialogMessageW(dialog, &msg))
		{
			ResetGfxDeviceIfNeeded();
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
}

#if UNITY_WIN_ENABLE_CRASHHANDLER
void CrashCallback( const char* crashFilesPath )
{
	if (GetEditorUserBuildSettingsPtr() != NULL)
	{
		gUnityCrashHandler->SetEditorMode((GetBuildTargetName(GetEditorUserBuildSettings().GetActiveBuildTarget())).c_str());
	}
	std::string projectFolder = EditorPrefs::GetString( "kProjectBasePath" );
	ConvertSeparatorsToWindows( projectFolder );
	gUnityCrashHandler->SetProjectFolder( projectFolder.c_str() );
}
#endif




/*
#include "Editor/Src/Utility/DiffTool.h"
#include "Runtime/Utilities/PlayerPrefs.h"
static void TestDiffTools()
{
	std::vector<std::string> diffTools = GetAvailableDiffTools();
	for( int i = 0; i < diffTools.size(); ++i ) {
		EditorPrefs::SetString("kDiffsDefaultApp", diffTools[i]);
		InvokeDiffTool(
			"The Left File", "F:/unitycode/unity-nextgen/testleft.txt",
			"The Right File", "F:/unitycode/unity-nextgen/testright.txt",
			"", "" );
	}
}
*/

static void DisableWindowGhosting(void)
{
	HMODULE hLibrary = ::LoadLibraryW(L"user32.dll");

	if (NULL != hLibrary)
	{
		typedef void (WINAPI *DisableProcessWindowsGhostingFunc)(void);
		DisableProcessWindowsGhostingFunc DisableProcessWindowsGhosting = reinterpret_cast<DisableProcessWindowsGhostingFunc>(::GetProcAddress(hLibrary, "DisableProcessWindowsGhosting"));

		if (NULL != DisableProcessWindowsGhosting)
		{
			DisableProcessWindowsGhosting();
		}

		::FreeLibrary(hLibrary);
	}
}

extern bool ShouldGiveDebuggerChanceToAttach();

bool ShouldStartBugReporterOnCrash()
{
	
#if SUPPORT_ENVIRONMENT_VARIABLES
	char* var = getenv("UNITY_DONOTSTARTBUGREPORTER");
	if (var)
	{
		if (*var == '1') return false;
		if (*var == '0') return true;
	}
#endif
	
	return IsHumanControllingUs();
}

string FindMonoBinaryToUse()
{
	string normalLocation(AppendPathName(GetApplicationContentsPath(),AppendPathName(kEditorMonoRuntimeVersion + "/EmbedRuntime","mono.dll")));
	if (!IsDeveloperBuild()) return normalLocation;

	string unityFolder = GetBaseUnityDeveloperFolder();
#if _WIN64
	string monoBuiltFromSource = AppendPathName (unityFolder, "External/" + kEditorMonoRuntimeVersion + "/builds/embedruntimes/win64/mono.dll");
#else
	string monoBuiltFromSource = AppendPathName (unityFolder, "External/" + kEditorMonoRuntimeVersion + "/builds/embedruntimes/win32/mono.dll");
#endif
	if (!IsFileCreated(monoBuiltFromSource)) return normalLocation;

	return monoBuiltFromSource;
}

void InitializeCurrentDirectory()
{
	char path[kDefaultPathBufferSize];
	GetCurrentDirectory(kDefaultPathBufferSize, path);
	File::SetCurrentDirectory(path);
}


/*
 * ReserveBottomMemory - from http://randomascii.wordpress.com/2012/02/14/64-bit-made-easy/
 */
void ReserveBottomMemory()
{
#if defined(_WIN64) && defined(_DEBUG)
	static bool s_initialized = false;
	if ( s_initialized )
		return;
	s_initialized = true;

	// Start by reserving large blocks of address space, and then
	// gradually reduce the size in order to capture all of the
	// fragments. Technically we should continue down to 64 KB but
	// stopping at 1 MB is sufficient to keep most allocators out.

	const size_t LOW_MEM_LINE = 0x100000000LL;
	size_t totalReservation = 0;
	size_t numVAllocs = 0;
	size_t numHeapAllocs = 0;
	size_t oneMB = 1024 * 1024;
	for (size_t size = 256 * oneMB; size >= oneMB; size /= 2)
	{
		for (;;)
		{
			void* p = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
			if (!p)
				break;

			if ((size_t)p >= LOW_MEM_LINE)
			{
				// We don't need this memory, so release it completely.
				VirtualFree(p, 0, MEM_RELEASE);
				break;
			}

			totalReservation += size;
			++numVAllocs;
		}
	}

	// Now repeat the same process but making heap allocations, to use up
	// the already reserved heap blocks that are below the 4 GB line.
	HANDLE heap = GetProcessHeap();
	for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
	{
		for (;;)
		{
			void* p = HeapAlloc(heap, 0, blockSize);
			if (!p)
				break;

			if ((size_t)p >= LOW_MEM_LINE)
			{
				// We don't need this memory, so release it completely.
				HeapFree(heap, 0, p);
				break;
			}

			totalReservation += blockSize;
			++numHeapAllocs;
		}
	}

	// Perversely enough the CRT doesn't use the process heap. Suck up
	// the memory the CRT heap has already reserved.
	for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
	{
		for (;;)
		{
			void* p = malloc(blockSize);
			if (!p)
				break;

			if ((size_t)p >= LOW_MEM_LINE)
			{
				// We don't need this memory, so release it completely.
				free(p);
				break;
			}

			totalReservation += blockSize;
			++numHeapAllocs;
		}
	}

	// Print diagnostics showing how many allocations we had to make in
	// order to reserve all of low memory, typically less than 200.
	char buffer[1000];
	sprintf_s(buffer, "Reserved %1.3f MB (%d vallocs,"
						"%d heap allocs) of low-memory.\n",
						totalReservation / (1024 * 1024.0),
						(int)numVAllocs, (int)numHeapAllocs);
	OutputDebugStringA(buffer);
#endif
}



int WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int nCmdShow )
{
	// Make sure all pointers use the upper word.
	ReserveBottomMemory();

	CoInitialize(0);
	winutils::SetInstanceHandle( hInst );

	AutoInitializeAndCleanupRuntime autoInit;
	
	if (!CPUInfo::HasSSE2Support ())
	{
		DisplayDialog ("SSE2 Required", "UnityEditor requires a SSE2 capable CPU to run.", "OK");
		exit(0);
	}

	SetupArgv(__argc, (const char**)__argv);

	// Set log file path

	string logFilePath;

	if (!HasARGV("nolog"))
	{
		if (HasARGV("logfile"))
		{
			logFilePath = GetFirstValueForARGV("logfile");
		}

		if (logFilePath.empty())
		{
			WCHAR widePath[MAX_PATH];

			if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, widePath)))
			{
				string path;
				ConvertWindowsPathName(widePath, path);

				path = AppendPathName(path, "Unity");
				CreateDirectory(path);

				path = AppendPathName(path, "Editor");
				CreateDirectory(path);

				logFilePath = AppendPathName(path, "Editor.log");

				// Backup previous log file. Ignore failure

				WCHAR wideFrom[kDefaultPathBufferSize];
				ConvertUnityPathName(logFilePath.c_str(), wideFrom, ARRAYSIZE(wideFrom));

				WCHAR wideTo[kDefaultPathBufferSize];
				ConvertUnityPathName(AppendPathName(path, "Editor-prev.log").c_str(), wideTo, ARRAYSIZE(wideTo));

				MoveFileExW(wideFrom, wideTo, (MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED));
			}
		}
	}

	logFilePath = SetLogFilePath(logFilePath);

	// Delete old log file so log entries are stored in new empty file. Ignore failure

	if (!logFilePath.empty())
	{
		WCHAR widePath[kDefaultPathBufferSize];
		ConvertUnityPathName(logFilePath.c_str(), widePath, ARRAYSIZE(widePath));
			
		DeleteFileW(widePath);
	}

	// Initialize cleaned log file

	if (HasARGV ("cleanedLogFile"))
	{
		string logFile = GetFirstValueForARGV ("cleanedLogFile");
		InitializeCleanedLogFile(fopen(logFile.c_str(), "w"));
	}

	// Run unit tests, if instructed to do so.
	RunUnitTestsIfRequiredAndExit ();

	// Check if we have other instances running
	if( g_SingleAppInstance.CheckOtherInstance() ) {
		// If we have "-openfile" parameter, pass that to another instance instead, and exit this one
		if( HasARGV("openfile") ) {
			if( g_SingleAppInstance.FindOtherInstance() ) {
				g_SingleAppInstance.SendCommand( GetFirstValueForARGV("openfile") );
				return 0;
			}
		}
		// Same applies to "-openurl"
		if( HasARGV("openurl") ) {
			if( g_SingleAppInstance.FindOtherInstance() ) {
				g_SingleAppInstance.SendCommand( GetFirstValueForARGV("openurl") );
				return 0;
			}
		}
	}

	g_BatchMode = IsBatchmode();
	
	if( !g_BatchMode )
		ShowSplashScreen();

	// WebKit requires this to be enabled
	InitializeDragAndDrop();

	if (!LicenseInfo::Activated())
	{
		if (g_BatchMode)
		{
			ErrorString("Unity has not been activated with a valid License. You must activate Unity with a valid license before running batchmode.");
			exit (1);
		}

		TurnOffSplashScreenWhenActivationWindowAppears();

		if (!LicenseMessagePump())
			return 0;
	}


	BeginBusyCursor();

	DisplayBleedingEdgeWarningIfRequired();

	InitializeCurrentDirectory();

	// Select data folder
	string appFolder = GetApplicationFolder();
	const string dataFolder = GetApplicationContentsPath();
	if( dataFolder.empty() )
	{
		winutils::AddErrorMessage( "There should be 'Data' folder next to the executable" );
		winutils::DisplayErrorMessagesAndQuit( "Data folder not found" );
	}

	// Setup crash handler
	#if UNITY_WIN_ENABLE_CRASHHANDLER
	bool startBugReporterOnCrash = ShouldStartBugReporterOnCrash();
	printf_console("Built from '%s' repo; Version is '%s revision %d'; Using compiler version '%d'\n", UNITY_VERSION_BRANCH_URL, UNITY_VERSION_FULL_NICE, UNITY_VERSION_BLD, _MSC_FULL_VER);
	printf_console("BatchMode: %d, IsHumanControllingUs: %d, StartBugReporterOnCrash: %d, shouldGiveDebuggerChanceToAttach: %d\n", g_BatchMode, IsHumanControllingUs(), startBugReporterOnCrash, ShouldGiveDebuggerChanceToAttach());
	gUnityCrashHandler = new CrashHandler( startBugReporterOnCrash ? gReporter : NULL, "Unity Editor", "Unity " UNITY_VERSION_FULL, appFolder.c_str());
	gUnityCrashHandler->SetCrashCallback( CrashCallback, true );
	gUnityCrashHandler->Install();
	if (!logFilePath.empty())
		gUnityCrashHandler->AddFile( logFilePath.c_str(), "Output log file" );
	gUnityCrashHandler->AddFile( GetMonoDevelopLogPath().c_str(), "MonoDevelop log file" );

	std::string projectFolder = EditorPrefs::GetString( "kProjectBasePath" );
	ConvertSeparatorsToWindows( projectFolder );
	gUnityCrashHandler->SetProjectFolder( projectFolder.c_str() );
	winutils::SetupInternalCrashHandler();
	#endif	

	EnsureBusyCursor();

	::DisableWindowGhosting();
	// Have to do dummy process of "win7 taskbar button" message
	g_TaskbarButtonCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");

	// init mono
	std::vector<string> monoPaths;

	std::string classlibs = kEditorMonoRuntimeVersion + "/lib/mono/";
	classlibs += kMonoClasslibsProfile;

	SetupDefaultPreferences();

	monoPaths.push_back(AppendPathName(dataFolder,"Managed"));
	monoPaths.push_back(AppendPathName(dataFolder,classlibs));
	monoPaths.push_back(AppendPathName(dataFolder,"UnityScript"));
	string monoBinaryToUse = FindMonoBinaryToUse();
	bool monook = LoadMono(	monoPaths, 
							AppendPathName(dataFolder, kEditorMonoRuntimeVersion + "/etc"),
							monoBinaryToUse, 
							logFilePath.empty() ? NULL : logFilePath.c_str() );
	
	if( !monook )
		winutils::DisplayErrorMessagesAndQuit( "Failed to load mono" );
	EnsureBusyCursor();

	if (HasARGV("nographics"))
	{
		if (!g_BatchMode)
		{
			winutils::AddErrorMessage( "-nographics requires -batchmode" );
			winutils::DisplayErrorMessagesAndQuit( "Bad Editor arguments" );
		}
		g_ForcedGfxRenderer = kGfxRendererNull;
	}
	else
	{
		ParseGfxDeviceArgs ();
	}

	EnsureBusyCursor();

	// Initialize editor window stuff
	InitializeEditorWindowClasses();

	// initialize editor application
	new Application();
	GetApplication().InitializeProject();

	if (!IsGfxDevice())
	{
		if (!IsHumanControllingUs())
		{
			winutils::AddErrorMessage ("\r\nMake sure you aren't running Unity as a service. Services\r\ncan't do any rendering; use -nographics -batchmode\r\nto turn off all rendering.");
		}
		winutils::DisplayErrorMessagesAndQuit( "Failed to initialize 3D graphics" );
	}

	EnsureBusyCursor();

	// Block user if we launched without license 
	//if( ! GetApplication().CheckLicense() )
	//{
	//THJ
	//g_BatchMode = true;
	//GetApplication().m_Licensed = false;

//	Application::
	
	//}

	GetApplication().FinishLoadingProject();
	EnsureBusyCursor();

	if( g_BatchMode || LicenseInfo::Get()->GetRawFlags() == 0  )
	{
		AssertIf( gMainWindow );
		gMainWindow = CreateMainWindowForBatchMode();
		gInitialized = true;
	}
	else
	{
		if( !gMainWindow ) {
			winutils::AddErrorMessage("Failed to read window layout: there's no main window in it!");
			winutils::DisplayErrorMessagesAndQuit("Corrupt window layout. Try deleting Library/WindowPreferences folder from your project for now");
		}
		AssertIf(!gMainWindow);
		SetSplashScreenParent(gMainWindow);
		bool ok = GetScreenManager().SetWindow( gMainWindow );
		if(	!ok )
			winutils::DisplayErrorMessagesAndQuit( "Failed to initialize OpenGL" );
	
		// Input may be already initialized by screen selector
		if (!IsInputInitialized())
			InputInit( gMainWindow );

		#if SUPPORT_IPHONE_REMOTE
			iPhoneRemoteInputInit(0);		// dummy init until we have proper remote support on windows
		#endif 

		UpdateMainMenu();

		HideSplashScreen();

		gInitialized = true;
		SetActiveWindow(gMainWindow);
	}

	EndBusyCursor();

	//
	GetApplication().AfterEverythingLoaded();
	CheckWow64 ();

	MainMessageLoop();

	// exit, always unlock cursor and show it!
	::ClipCursor( NULL );
	::ShowCursor( TRUE );
	Shutdown();
	UnloadMono();
	
	ShutdownDragAndDrop();
#if !UNITY_64
	ShutdownQuicktime();
#endif

	RegisterLogPreprocessor (NULL);

	delete gUnityCrashHandler;

	return 0;
}
