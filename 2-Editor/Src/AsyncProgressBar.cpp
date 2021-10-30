#include "UnityPrefix.h"
#include "AsyncProgressBar.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Threads/Mutex.h"
#if UNITY_WIN
#include "Editor/Platform/Windows/Utility/Windows7Taskbar.h"
#endif

AsyncProgressBar* gAsyncProgressBar = NULL;

AsyncProgressBar::AsyncProgressBar ()
{
	m_IsDirty = false;
	m_IsShowing = false;
	m_Progress = 0;
	m_AsyncTickerFunction = NULL;
}

void AsyncProgressBar::Tick ()
{
	if (m_IsDirty)
	{
		ScriptingInvocation(MONO_COMMON.statusBarChanged).Invoke();
		m_IsDirty = false;
	}
}

float AsyncProgressBar::GetProgress ()
{ 
	return m_Progress;
}

std::string AsyncProgressBar::GetProgressInfo ()
{ 
	Mutex::AutoLock lock(m_Mutex);
	return m_ProgressInfo;
}

bool AsyncProgressBar::IsShowing ()
{
	return m_IsShowing;
}

void AsyncProgressBar::Display(const std::string& progressInfo, float progress)
{
#if UNITY_WIN
	UpdateWindows7ProgressBar(progress);
#endif
	
	m_Mutex.Lock();

	if (m_ProgressInfo != progressInfo)
	{
		m_ProgressInfo = progressInfo;
		m_IsDirty = true;
	}
	
	if (!m_IsShowing)
	{
		m_IsShowing = true;
		m_IsDirty = true;
	}
	
	if (RoundfToInt(m_Progress * 100.0F) != RoundfToInt(progress * 100.0F))
	{
		m_Progress = progress;
		m_IsDirty = true;
	}
	m_Mutex.Unlock();
}

void AsyncProgressBar::Clear()
{
#if UNITY_WIN
	ClearWindows7ProgressBar();
#endif
	
	m_Mutex.Lock();
	
	m_ProgressInfo = "";
	m_Progress = 0.0f;
	
	if (m_IsShowing)
	{
		m_IsShowing = false;
		m_IsDirty = true;
	}

	m_AsyncTickerFunction = NULL;
	m_Mutex.Unlock();
}

void AsyncProgressBar::SetTickerFunction(TickerFunction *fn)
{
	m_AsyncTickerFunction = fn;
}

void AsyncProgressBar::InvokeTickerFunctionIfActive()
{
	if (gAsyncProgressBar == NULL || gAsyncProgressBar->m_AsyncTickerFunction == NULL)
		return;

	gAsyncProgressBar->m_AsyncTickerFunction();
}

AsyncProgressBar& GetAsyncProgressBar()
{
	if (gAsyncProgressBar != NULL)
		return *gAsyncProgressBar;
	
	gAsyncProgressBar = new AsyncProgressBar();
	return *gAsyncProgressBar;
}
