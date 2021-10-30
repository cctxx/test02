#include "UnityPrefix.h"
#include "BrowseForFolder.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Runtime/Misc/SystemInfo.h"
#include <ShlObj.h>
#include <shlwapi.h>

static ValidateFolderFunc* s_ValidateFunc;

static wchar_t* GetPathFromIDList (LPITEMIDLIST idlist)
{
	if (idlist == NULL)
		return L"";

	static wchar_t path[kDefaultPathBufferSize];
	memset( path, 0, sizeof(path) );
	SHGetPathFromIDListW( idlist, path );
	return path;
}

static bool IsDirectoryEmptyWide(LPCWSTR path)
{
	if (0 == ::lstrlenW(path))
	{
		return true;
	}

	WCHAR buffer[MAX_PATH];
	::PathCombineW(buffer, path, L"*");

	WIN32_FIND_DATAW data;
	HANDLE hFindFile = ::FindFirstFileW(buffer, &data);

	if (INVALID_HANDLE_VALUE == hFindFile)
	{
		return true;
	}

	while (true)
	{
		if ((0 != ::lstrcmpW(data.cFileName, L".")) && (0 != ::lstrcmpW(data.cFileName, L"..")))
		{
			::FindClose(hFindFile);
			return false;
		}

		if (!::FindNextFileW(hFindFile, &data))
		{
			::FindClose(hFindFile);
			return true;
		}
	}
}

static bool ConfirmFolderOverwrite(HWND hWnd)
{
	return (IDYES == ::MessageBoxW(hWnd, L"Do you want to replace selected folder?\r\nReplacing it will overwrite its current contents.", L"Selected folder is not empty", (MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION)));
}

// --------------------------------------------------------------------------
// SHBrowseForFolder path


static int CALLBACK BrowseForFolderCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
		// Looks like if we disable OK button from inside of BFFM_SETSELECTION processing, it does not get disabled.
		// So disable it for real here.
		const wchar_t *winpath = reinterpret_cast<const wchar_t *>(lpData);
		bool okFolder = true;
		if( s_ValidateFunc ) {
			std::wstring infoMessage;
			okFolder = s_ValidateFunc(winpath, infoMessage);
		}
		SendMessage(hwnd, BFFM_ENABLEOK, 0, okFolder);
	}
	else if (uMsg == BFFM_SELCHANGED)
	{
		const wchar_t *winpath = GetPathFromIDList((ITEMIDLIST*)lParam);
		bool okFolder = true;
		if( s_ValidateFunc ) {
			std::wstring infoMessage;
			okFolder = s_ValidateFunc(winpath, infoMessage);
		}
		SendMessage( hwnd, BFFM_ENABLEOK, 0, okFolder );
	}

	return 0;
}

static std::wstring BrowseForFolderShellImpl( HWND parent, const wchar_t* title, const wchar_t* initialFolder, const wchar_t* defaultFolder, bool existingFolder )
{
	wchar_t path[kDefaultPathBufferSize];
	PathCombineW(path, initialFolder, defaultFolder);

	BROWSEINFOW bi;
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = parent;
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS | BIF_VALIDATE | BIF_NEWDIALOGSTYLE;
	if (existingFolder)
		bi.ulFlags |= BIF_NONEWFOLDERBUTTON;
	else
		bi.ulFlags |= BIF_DONTGOBELOWDOMAIN;
	bi.lpfn = BrowseForFolderCallbackProc;
	bi.lParam = (LPARAM)path;

	std::wstring result;

	while (true)
	{
		LPITEMIDLIST idlist = SHBrowseForFolderW(&bi);
		if (idlist == NULL)
			return L"";

		result = GetPathFromIDList(idlist);
		CoTaskMemFree(idlist);

		if (!existingFolder && !::IsDirectoryEmptyWide(result.c_str()))
		{
			if (!::ConfirmFolderOverwrite(parent))
			{
				bi.lParam = (LPARAM)result.c_str();
				continue;
			}
		}

		return result;
	}
}


// --------------------------------------------------------------------------
// IFileDialog in folders mode path (Vista and later)

#include "PlatformDependent/Win/SmartComPointer.h"
#include "IFileDialogHeader.h"

const int kHintTextID = 1001;

class CFileDialogEventHandler : public IFileDialogEvents {
public:
	CFileDialogEventHandler( ValidateFolderFunc validateFunction, IFileDialogCustomize* customize );
	~CFileDialogEventHandler();

	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, void ** ppvObject);
	virtual ULONG   STDMETHODCALLTYPE AddRef (void);
	virtual ULONG   STDMETHODCALLTYPE Release (void);

	// IFileDialogEvents
	virtual HRESULT STDMETHODCALLTYPE OnFileOk( IFileDialog *pfd );
	virtual HRESULT STDMETHODCALLTYPE OnFolderChanging( IFileDialog *pfd, IShellItem *psiFolder );
	virtual HRESULT STDMETHODCALLTYPE OnFolderChange( IFileDialog *pfd );
	virtual HRESULT STDMETHODCALLTYPE OnSelectionChange( IFileDialog *pfd );
	virtual HRESULT STDMETHODCALLTYPE OnShareViolation( IFileDialog *pfd, IShellItem *psi, FDE_SHAREVIOLATION_RESPONSE *pResponse );
	virtual HRESULT STDMETHODCALLTYPE OnTypeChange( IFileDialog *pfd );
	virtual HRESULT STDMETHODCALLTYPE OnOverwrite( IFileDialog *pfd, IShellItem *psi, FDE_OVERWRITE_RESPONSE *pResponse );

private:
	void DoValidation( IFileDialog* pfd, IShellItem* item );

private:
	LONG m_RefCount;
	ValidateFolderFunc* m_ValidateFunction;
	IFileDialogCustomize* m_Customize;
};

CFileDialogEventHandler::CFileDialogEventHandler( ValidateFolderFunc validateFunction, IFileDialogCustomize* customize )
:	m_RefCount(1)
,	m_ValidateFunction(validateFunction)
,	m_Customize(customize)
{
}

CFileDialogEventHandler::~CFileDialogEventHandler()
{
}

ULONG __stdcall CFileDialogEventHandler::AddRef(void)
{
	return InterlockedIncrement(&m_RefCount);
}
ULONG __stdcall CFileDialogEventHandler::Release(void)
{
	LONG count = InterlockedDecrement(&m_RefCount);
	if(count == 0)
	{
		delete this;
		return 0;
	}
	else
	{
		return count;
	}
}

HRESULT __stdcall CFileDialogEventHandler::QueryInterface(REFIID iid, void **ppvObject)
{
	if( iid == IID_IFileDialogEvents || iid == IID_IUnknown )
	{
		AddRef();
		*ppvObject = this;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnFileOk( IFileDialog *pfd )
{
	HRESULT hr;

	DWORD options;

	if (FAILED(hr = pfd->GetOptions(&options)))
	{
		return hr;
	}

	if (FOS_FILEMUSTEXIST != (options & FOS_FILEMUSTEXIST))
	{
		SmartComPointer<IShellItem> item;

		if (FAILED(hr = pfd->GetResult(&item)))
		{
			return hr;
		}

		LPWSTR path;

		if (FAILED(hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
		{
			return hr;
		}

		bool empty = ::IsDirectoryEmptyWide(path);

		::CoTaskMemFree(path);

		if (!empty)
		{
			SmartComPointer<IOleWindow> window;

			if (FAILED(hr = pfd->QueryInterface(IID_IOleWindow, reinterpret_cast<void **>(&window))))
			{
				return hr;
			}

			HWND hWnd;

			if (FAILED(hr = window->GetWindow(&hWnd)))
			{
				return hr;
			}

			if (!::ConfirmFolderOverwrite(hWnd))
			{
				return S_FALSE;
			}
		}
	}

	return S_OK;
}

void CFileDialogEventHandler::DoValidation( IFileDialog* pfd, IShellItem* item )
{
	HRESULT hr;

	SmartComPointer<IOleWindow> window;
	hr = pfd->QueryInterface( IID_IOleWindow, (void**)&window );
	if( FAILED(hr) )
		return;

	HWND hwndDialog;
	hr = window->GetWindow( &hwndDialog );
	if( FAILED(hr) )
		return;

	std::wstring infoMessage;

	wchar_t* path = NULL;
	hr = item->GetDisplayName( SIGDN_FILESYSPATH, &path );
	if( FAILED(hr) ) {
		// not a file system object selected, disable OK button
		EnableWindow( GetDlgItem(hwndDialog, IDOK), FALSE );
		if( m_Customize ) {
			m_ValidateFunction( path, infoMessage );
			m_Customize->SetControlLabel( kHintTextID, infoMessage.c_str() );
		}
		return;
	}

	bool pathValid = m_ValidateFunction( path, infoMessage );
	EnableWindow( GetDlgItem(hwndDialog, IDOK), pathValid );
	if( m_Customize ) {
		m_Customize->SetControlLabel( kHintTextID, infoMessage.c_str() );
	}
}


HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnFolderChanging( IFileDialog *pfd, IShellItem *psiFolder )
{
	DoValidation( pfd, psiFolder );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnFolderChange( IFileDialog *pfd )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnSelectionChange( IFileDialog *pfd )
{
	HRESULT hr;

	SmartComPointer<IShellItem> item;
	hr = pfd->GetCurrentSelection(&item);
	if( FAILED(hr) )
		return S_OK;

	DoValidation( pfd, item );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnShareViolation( IFileDialog *pfd, IShellItem *psi, FDE_SHAREVIOLATION_RESPONSE *pResponse )
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnTypeChange( IFileDialog *pfd )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CFileDialogEventHandler::OnOverwrite( IFileDialog *pfd, IShellItem *psi, FDE_OVERWRITE_RESPONSE *pResponse )
{
	return E_NOTIMPL;
}


static std::wstring BrowseForFolderVistaImpl( HWND parent, const wchar_t* title, const wchar_t* initialFolder, const wchar_t* defaultFolder, bool existingFolder )
{
	HRESULT hr;

	SmartComPointer<IFileOpenDialog> dialog;
	hr = CoCreateInstance( CLSID_FileOpenDialog, 0, CLSCTX_INPROC, IID_IFileOpenDialog, (void**)&dialog );
	if( FAILED(hr) )
		return L"";

	if( initialFolder != NULL )
	{
		// SHCreateItemFromParsingName is in shell32.dll, but only starting with Vista. So we have to
		// dynamically load it.
		HMODULE shell32 = LoadLibraryW(L"shell32.dll");
		if( shell32 ) {
			typedef HRESULT (STDAPICALLTYPE* SHCreateItemFromParsingNameProc)(__in PCWSTR pszPath, __in_opt IBindCtx *pbc, __in REFIID riid, __deref_out void **ppv);
			SHCreateItemFromParsingNameProc createItem = (SHCreateItemFromParsingNameProc)GetProcAddress(shell32, "SHCreateItemFromParsingName");
			if( createItem ) {
				SmartComPointer<IShellItem> initialFolderItem;
				hr = createItem( initialFolder, NULL, IID_PPV_ARGS(&initialFolderItem) );
				if( SUCCEEDED(hr) )
					hr = dialog->SetFolder(initialFolderItem);
				if (NULL != defaultFolder)
					hr = dialog->SetFileName(defaultFolder);
			}
			FreeLibrary(shell32);
		}
	}

	SmartComPointer<IFileDialogCustomize> dialogCustomize;
	hr = dialog->QueryInterface( IID_IFileDialogCustomize, (void**)&dialogCustomize );
	if( SUCCEEDED(hr) ) {
		dialogCustomize->AddText( kHintTextID, L" " );
	}

	CFileDialogEventHandler eventHandler( s_ValidateFunc, dialogCustomize );
	DWORD cookie = 0;
	dialog->Advise( &eventHandler, &cookie );

	dialog->SetTitle( title );
	DWORD options = 0;
	dialog->GetOptions(&options);
	if (!existingFolder) options &= ~FOS_FILEMUSTEXIST;
	options |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOREADONLYRETURN | FOS_HIDEMRUPLACES | FOS_DONTADDTORECENT;
	dialog->SetOptions(options);
	hr = dialog->Show( parent );

	if( cookie )
		dialog->Unadvise( cookie );

	if( FAILED(hr) )
		return L"";

	SmartComPointer<IShellItem> item;
	hr = dialog->GetResult( &item );
	if( FAILED(hr) )
		return L"";

	wchar_t* path = NULL;
	hr = item->GetDisplayName( SIGDN_FILESYSPATH, &path );
	if( FAILED(hr) )
		return L"";

	std::wstring result = path;
	CoTaskMemFree(path);
	return result;
}



// --------------------------------------------------------------------------

std::string BrowseForFolderDialog( HWND parent, const wchar_t* title, const wchar_t* initialFolder, const wchar_t* defaultFolder, bool existingFolder, ValidateFolderFunc validateFunc )
{
	s_ValidateFunc = validateFunc;
	std::wstring widePath;

	bool vistaOrLater = systeminfo::GetOperatingSystemNumeric() >= 600;

	if( vistaOrLater )
		widePath = BrowseForFolderVistaImpl( parent, title, initialFolder, defaultFolder, existingFolder );
	else
		widePath = BrowseForFolderShellImpl( parent, title, initialFolder, defaultFolder, existingFolder );

	std::string path;
	ConvertWindowsPathName( widePath, path );
	return path;
}
