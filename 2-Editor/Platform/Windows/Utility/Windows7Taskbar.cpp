#include "UnityPrefix.h"
#include "Windows7Taskbar.h"
#include "../winsdk/shobjidl7.h"
#include "Runtime/Threads/Mutex.h"

HWND GetMainEditorWindow(); // WinEditorMain.cpp


static bool s_TaskBarList3Done = false;
static ITaskbarList3* s_TaskBarList3 = NULL;
static Mutex gWindows7TaskbarMutex;

static ITaskbarList3* GetWindows7TaskBar()
{
	if (s_TaskBarList3Done)
		return s_TaskBarList3;

	AssertIf(s_TaskBarList3);

	HRESULT hr = CoCreateInstance (
		CLSID_TaskbarList,
		NULL,
		CLSCTX_ALL,
		IID_ITaskbarList3, (void**)&s_TaskBarList3);
	s_TaskBarList3Done = true;
	return s_TaskBarList3;
}


void UpdateWindows7ProgressBar (float val)
{
	Mutex::AutoLock lock(gWindows7TaskbarMutex);

	ITaskbarList3* tb = GetWindows7TaskBar ();
	if (tb) {
		HWND wnd = GetMainEditorWindow ();
		tb->SetProgressState (wnd, TBPF_NORMAL);
		tb->SetProgressValue (wnd, val*100.0f, 100);
	}
}

void ClearWindows7ProgressBar ()
{
	Mutex::AutoLock lock(gWindows7TaskbarMutex);

	ITaskbarList3* tb = GetWindows7TaskBar ();
	if (tb) {
		HWND wnd = GetMainEditorWindow ();
		tb->SetProgressState (wnd, TBPF_NOPROGRESS);
	}
}
