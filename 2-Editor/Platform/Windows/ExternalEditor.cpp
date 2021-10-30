#include "UnityPrefix.h"
#include "Editor/Platform/Interface/ExternalEditor.h"
#include "Runtime/Utilities/File.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include <ShellAPI.h>
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "Editor/Src/Panels/HelpPanel.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/EditorHelper.h"

//-------------------------------------------------------------------------------------------------
// VSOpenFile() code is based on public domain code from "BenBuck" @ http://benbuck.com/archives/13
//-------------------------------------------------------------------------------------------------

#include <atlbase.h>
#pragma warning(disable : 4278)
#pragma warning(disable : 4146)
// import EnvDTE
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" version("8.0") lcid("0") raw_interfaces_only named_guids
#pragma warning(default : 4146)
#pragma warning(default : 4278)

int ShellExecuteW_Helper(HWND hwnd, LPCWSTR lpOperation, std::string app, std::string args, LPCWSTR lpDirectory, INT nShowCmd)
	{
		const int argBufferSize = kDefaultPathBufferSize * 3; // Make room for a second path for the solution and additional args
		wchar_t wideApp[kDefaultPathBufferSize];
		wchar_t wideArgs[ argBufferSize ]; 
		ConvertUnityPathName( app.c_str(), wideApp, kDefaultPathBufferSize );
		ConvertUnityPathName( args.c_str(), wideArgs, argBufferSize );
		return (int)::ShellExecuteW( hwnd, lpOperation, wideApp, wideArgs, NULL, nShowCmd );
	}

#define RETURN_ON_FAIL( expression ) \
	result = ( expression );	\
	if ( FAILED( result ) )		\
	{							\
		printf_console(#expression);	\
		ClearProgressbar();		\
		return false;			\
	}							\
	else // To prevent danging else condition


bool HaveRunningVSProOpenFile(CComPtr<EnvDTE::_DTE> dte, const std::string& _filename, int line)
{
	HRESULT result;
	
	wchar_t filename[kDefaultPathBufferSize];
	ConvertUnityPathName(_filename.c_str(), filename, kDefaultPathBufferSize);

	CComBSTR bstrFileName(filename);
    //CComBSTR bstrKind(EnvDTE::vsViewKindTextView);
	CComBSTR bstrKind(EnvDTE::vsViewKindPrimary);
    CComPtr<EnvDTE::Window> window = NULL;

	CComPtr<EnvDTE::ItemOperations> item_ops;
    RETURN_ON_FAIL( dte->get_ItemOperations(&item_ops) );


	RETURN_ON_FAIL( item_ops->OpenFile(bstrFileName, bstrKind, &window) );

	if ( line > 0 )
	{
		CComPtr<EnvDTE::Document> doc;
		if ( SUCCEEDED( dte->get_ActiveDocument(&doc) ) )
		{
			CComPtr<IDispatch> selection_dispatch;
			if ( SUCCEEDED( doc->get_Selection(&selection_dispatch) ) )
			{
				CComPtr<EnvDTE::TextSelection> selection;
				if ( SUCCEEDED( selection_dispatch->QueryInterface(&selection) ) )
				{
					selection->GotoLine( line, TRUE );
				}
			}
		}
	}

	window = NULL;
	if ( SUCCEEDED( dte->get_MainWindow( &window ) ) )
	{
		// Activate() set the window to visible and active (blinks in taskbar)
		window->Activate();

		// DTE doesn't seem to be able to make the main window the foreground
		// window, so I just set that manually.
		HWND hWnd;
		window->get_HWnd( (LONG*)&hWnd );
		SetForegroundWindow( hWnd );
	}

	// Put the app under user control, so it doesn't close along with Unity
	VARIANT_BOOL userControl = FALSE;
	
	ClearProgressbar();

	
    return true;
}


bool GuessCLSIDForVSPath(const std::string& vspath, CLSID& clsid)
{
	HRESULT result;
	if (vspath.find("Microsoft Visual Studio 9.0") != std::string::npos)
	{
		result = ::CLSIDFromProgID(L"VisualStudio.DTE.9.0", &clsid);
		if (!FAILED(result)) return true;
	}
	if (vspath.find("Microsoft Visual Studio 10.0") != std::string::npos)
	{
		result = ::CLSIDFromProgID(L"VisualStudio.DTE.10.0", &clsid);
		if (!FAILED(result)) return true;
	}

	result = ::CLSIDFromProgID(L"VisualStudio.DTE", &clsid);
	return !FAILED(result);
}

CComPtr<EnvDTE::_DTE> FindRunningVSProWithOurSolution(const std::string& vspath, const std::string& _solutionPath)
{
	HRESULT result;
    CLSID clsid;
    CComPtr<IUnknown> punk = NULL;
	CComPtr<EnvDTE::_DTE> dte = NULL;

	bool success = GuessCLSIDForVSPath(vspath, clsid);
	if (!success) return false;

	wchar_t solutionPath[kDefaultPathBufferSize];
	ConvertUnityPathName(_solutionPath.c_str(), solutionPath, kDefaultPathBufferSize);

	// Search through the Running Object Table for an instance of Visual Studio
	// to use that either has the correct solution already open or does not have 
	// any solution open.
	CComPtr<IRunningObjectTable> ROT;
	RETURN_ON_FAIL( GetRunningObjectTable( 0, &ROT ) );

	CComPtr<IBindCtx> bindCtx;
	RETURN_ON_FAIL( CreateBindCtx( 0, &bindCtx ) );

	CComPtr<IEnumMoniker> enumMoniker;
	RETURN_ON_FAIL( ROT->EnumRunning( &enumMoniker ) );

	CComPtr<IMoniker> dteMoniker;
	RETURN_ON_FAIL( CreateClassMoniker( clsid, &dteMoniker ) );

	CComBSTR bstrSolution( solutionPath );
	CComPtr<IMoniker> moniker;
	ULONG monikersFetched = 0;
	while ( enumMoniker->Next( 1, &moniker, &monikersFetched ) == S_OK)
	{
		if ( moniker->IsEqual( dteMoniker ) )
		{
			punk = NULL;		
			result = ROT->GetObject( moniker, &punk );
			moniker = NULL;
			if ( result != S_OK )
				continue;

			dte = punk;

			if ( !dte )
				continue;
			//okay, so we found an actual visualstudio.
			
			//ask for its current solution
			CComPtr<EnvDTE::_Solution> solution;
			result = dte->get_Solution( &solution );
			if ( FAILED(result) )
				continue;

			//and the name of that solution.
			CComBSTR fullName;
			result = solution->get_FullName( &fullName );
			if (FAILED(result))
				continue;

			//if the name matches the solution we want to open, we're happy. 
			if ( fullName == bstrSolution )
				return dte;
		}
	}
	return NULL;
}


bool VSPro_OpenFile_COM(const std::string& vspath, const std::string& _filename, int line, const std::string& _solutionPath )
{
    HRESULT result;
	CComPtr<EnvDTE::_DTE> dte = NULL;
	
	dte = FindRunningVSProWithOurSolution(vspath, _solutionPath);

	if ( !dte )
	{
		CComPtr<IUnknown> punk = NULL;
		CLSID clsid;
		bool success = GuessCLSIDForVSPath(vspath, clsid);
		if (!success) return false;

		DisplayProgressbar("Opening Visual Studio", "Starting up Visual Studio, this might take a few seconds.", .3f);
		RETURN_ON_FAIL( ::CoCreateInstance( clsid, NULL, CLSCTX_LOCAL_SERVER, EnvDTE::IID__DTE, (LPVOID*)&punk ) );
		
		dte = punk;

		if ( !dte )
			return false;

		dte->put_UserControl( TRUE );
		
		DisplayProgressbar("Opening Visual Studio", "Opening the Visual Studio solution.", .5f);

		CComPtr<EnvDTE::_Solution> solution;
		RETURN_ON_FAIL( dte->get_Solution( &solution ) );

		wchar_t solutionPath[kDefaultPathBufferSize];
		ConvertUnityPathName(_solutionPath.c_str(), solutionPath, kDefaultPathBufferSize);

		CComBSTR bstrSolution( solutionPath );
		RETURN_ON_FAIL( solution->Open( bstrSolution ) );
		
		// Open script reference in VS's browser window
		string path = FindHelpNamed("file:///unity/ScriptReference/index.html");
		if (!path.empty())
		{
			CComBSTR bstrCommand( "View.URL" );
			CComBSTR bstrPath( path.c_str() );		
			dte->ExecuteCommand( bstrCommand, bstrPath );
		}


		//Sometimes VS is slow opening. It can happen that we call HaveRunningVSProOpenFile before the new VS
		//instance is ready to handle our request, resulting in it remaining hidden, and the default editor opening instead.
		int time = 0;
		while(time<5000)
		{
			EnvDTE::Window* window = NULL;
			if ( SUCCEEDED( dte->get_MainWindow( &window ) ) )
				break;
			printf_console("retrying grabbing VisualStudios mainwindow.\n");
			Sleep(50);
			time += 50;
		}

	}
 
	bool res = HaveRunningVSProOpenFile(dte, _filename, line);
    
	ClearProgressbar();
	return res;
}

bool VSPro_OpenFile_CmdLine(std::string& vspath)
{
	// printf_console( "Using fallback to launch VS" );
	// Fallback if COM doesn't work. We could potentially use this for launching
	// VS via Parallels on OSX, too.
	/*
	args = QuoteString(PathToAbsolutePath (vspath));

	if ( line >= 0 || !winutils::IsApplicationRunning( kVisualStudioProExe ) )
	{
		args = QuoteString( solutionPath ) + args;
		if ( line >= 0 )
			args += " -command \"Edit.GoTo " + IntToString( line ) + "\"";
	}
	else
		args += " -edit";

	int res = ShellExecuteW_Helper( NULL, L"open", app, args, NULL, SW_SHOWNORMAL );
	if( res <= 32 )
		res = ShellExecuteW_Helper( NULL, L"edit", app, args, NULL, SW_SHOWNORMAL );

	return res > 32;*/

	return false;
}

bool VSExpress_OpenFile(const std::string& vsexpress, const std::string& filename, int line, const std::string& solutionPath)
{
	string vsecontroller = AppendPathName(GetApplicationFolder(), "Data/Tools/VSExpressController/VSExpressController.exe");
	std::string app = PathToAbsolutePath (vsecontroller);
	std::string args = QuoteString(vsexpress) + " " + QuoteString(solutionPath) + " " + QuoteString(PathToAbsolutePath (filename)) + " " + IntToString(line);

	int res = ShellExecuteW_Helper( NULL, L"open", app, args, NULL, SW_HIDE );
	return res > 32;
}

bool MonoDevelop_OpenFile (const std::string& monodevelop, const std::string& filename, int line, const std::string& solutionPath)
{
	std::string args = "--nologo " + QuoteString (solutionPath) + " " + QuoteString (filename + ";" + IntToString (line));
	int res = ShellExecuteW_Helper( NULL, L"open", monodevelop, args, NULL, SW_HIDE );
	return res > 32;
}


bool OpenFileAtLine (const std::string& inPath, int line, const std::string& inAppPath, const std::string& inAppArgs, OpenFileAtLineMode openMode)
{
	std::string appPath = inAppPath;
	if (appPath.empty ())
		appPath = GetDefaultEditorPath ();
	appPath = PathToAbsolutePath(appPath);
	std::string args = QuoteString(inPath);

	const char* kVisualStudioProExe = "devenv.exe";
	const char* kVisualStudioExpressExe = "vcsexpress.exe";
	const char* kMonoDevelopExe = "monodevelop";

	bool useMonoDevelop = ToLower ( inAppPath ).rfind( kMonoDevelopExe ) != std::string::npos;
	bool usePro = ToLower( inAppPath ).rfind( kVisualStudioProExe ) != std::string::npos;
	bool useExpress = ToLower( inAppPath ).rfind( kVisualStudioExpressExe ) != std::string::npos;
	
	//turning off express support for 2.6, because I can't get it to work reliably yet.
	//fallback to builtin.
	if (useExpress) return false;

	if ( usePro || useExpress || useMonoDevelop )
	{
		CallStaticMonoMethod("SyncVS","CreateIfDoesntExist");

		string solutionPath;
		string filePath;
		GetSolutionFileToOpenForSourceFile (inPath, useMonoDevelop, &solutionPath, &filePath);
		
		if (usePro)
			return VSPro_OpenFile_COM( appPath, filePath, line, solutionPath );

		if (useExpress)
			return VSExpress_OpenFile( appPath, filePath, line, solutionPath );

		if (useMonoDevelop)
			return MonoDevelop_OpenFile ( appPath, filePath, line, solutionPath );
	}
	else if (!inAppArgs.empty())
		args = inAppArgs;
	
	int res = ShellExecuteW_Helper( NULL, L"open", appPath, args, NULL, SW_SHOWNORMAL );
	if( res <= 32 )
		res = ShellExecuteW_Helper( NULL, L"edit", appPath, args, NULL, SW_SHOWNORMAL );

	return res > 32;
}