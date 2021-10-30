#pragma once

#include "Runtime/Threads/Mutex.h"

class AsyncProgressBar
{
	bool        m_IsDirty;
	bool        m_IsShowing;
	float       m_Progress;
	std::string m_ProgressInfo;
	Mutex       m_Mutex;
	
	typedef void TickerFunction();

	TickerFunction* m_AsyncTickerFunction;

	public:
	AsyncProgressBar ();
	
	void Tick ();
	
	float GetProgress ();
	
	std::string GetProgressInfo ();
	bool IsShowing ();

	
	void Display(const std::string& progressInfo, float progress);
	void Clear();

	void SetTickerFunction(TickerFunction *fn);
	static void InvokeTickerFunctionIfActive();
};

AsyncProgressBar& GetAsyncProgressBar();
