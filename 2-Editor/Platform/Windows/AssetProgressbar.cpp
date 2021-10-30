#include "UnityPrefix.h"
#include "Editor/Platform/Interface/AssetProgressbar.h"
#include "Editor/Platform/Windows/resource.h"
#include "Editor/Platform/Windows/Utility/Windows7Taskbar.h"
#include "PlatformDependent/Win/WinUtils.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "Runtime/Utilities/Argv.h"
#include <CommCtrl.h>

// WinEditorMain.cpp
HWND GetMainEditorWindow();
void ProcessDialogMessages(HWND dialog);


using namespace std;
using namespace winutils;


// For now the progress is not threaded or anything. Just shows a modeless dialog and redraws it as updates come in.
// This means while long operation is in progress, Unity windows can't be moved for example.


namespace
{
	struct ProgressBar
	{
		HWND dialog;
		bool wantsToCancel;

		inline ProgressBar() : dialog(NULL), wantsToCancel(false) {}
	} s_ProgressBar;


	INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_INITDIALOG:
			{
				// make progress dialog show in the taskbar if we don't have main window yet
				if (!GetMainEditorWindow())
				{
					LONG exStyle = GetWindowLongW(dialog, GWL_EXSTYLE);
					exStyle |= WS_EX_APPWINDOW;
					SetWindowLongW(dialog, GWL_EXSTYLE, exStyle);
					SetWindowTextW(dialog, L"Unity - Importing Assets");
				}

				SendDlgItemMessageW(dialog, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
				CenterWindowOnParent(dialog);
				SetForegroundWindow(dialog);
			}
			return TRUE;

		case WM_COMMAND:
			{
				if (wParam == IDCANCEL)
				{
					HWND const cancelButton = GetDlgItem(dialog, IDCANCEL);
					Assert(cancelButton);

					BOOL const wasDisabled = EnableWindow(cancelButton, FALSE);
					Assert(!wasDisabled);

					s_ProgressBar.wantsToCancel = true;
					return TRUE;
				}
			}
			break;
		}

		return FALSE;
	}
}


void UpdateAssetProgressbar(float value, string const& title, string const& text, bool canCancel)
{
	if (IsBatchmode())
		return;

	HWND const dialog = s_ProgressBar.dialog;

	if (!dialog)
	{
		HWND const parent = GetMainEditorWindow();
		BOOL const parentWasDisabled = EnableWindow(parent, FALSE);
		Assert(!parentWasDisabled);

		s_ProgressBar.dialog = CreateDialogW(GetInstanceHandle(), canCancel ? MAKEINTRESOURCEW(IDD_PROGRESS_WITHCANCEL) : MAKEINTRESOURCEW(IDD_PROGRESS), parent, DialogProc);
		Assert(s_ProgressBar.dialog);
	}

	// update progress bar and text

	SendDlgItemMessageW(s_ProgressBar.dialog, IDC_PROGRESS, PBM_SETPOS, (int)(value * 1000.0f), 0);

	wstring textWide;

	ConvertUnityPathName(text, textWide);
	SetDlgItemTextW(s_ProgressBar.dialog, IDC_ST_PATH, textWide.c_str());

	ConvertUnityPathName(title.c_str(), textWide);
	SetWindowTextW(s_ProgressBar.dialog, textWide.c_str());

	// show dialog box last so we don't see how window is being repainted
	if (!dialog)
		ShowWindow(s_ProgressBar.dialog, SW_SHOWDEFAULT);

	UpdateWindows7ProgressBar(value);

	ProcessDialogMessages(s_ProgressBar.dialog);
}

void ClearAssetProgressbar()
{
	if (!s_ProgressBar.dialog)
		return;

	// parent must be enabled before destroying dialog box. it will not receive focus otherwise

	HWND const parent = GetMainEditorWindow();

	if (parent)
	{
		BOOL const parentWasDisabled = EnableWindow(parent, TRUE);
		Assert(parentWasDisabled);
	}

	BOOL const destroyDialogResult = DestroyWindow(s_ProgressBar.dialog);
	Assert(destroyDialogResult);

	s_ProgressBar.dialog = NULL;
	s_ProgressBar.wantsToCancel = false;

	ClearWindows7ProgressBar();
}

bool IsAssetProgressBarCancelPressed()
{
	return s_ProgressBar.wantsToCancel;
}
