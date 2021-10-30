#ifndef ERROREXIT_H
#define ERROREXIT_H

#include "Configuration/UnityConfigure.h"

#if SUPPORT_ERROR_EXIT

enum ExitErrorCode
{
	kErrorNone = 0,
	kErrorSecurityBreach = 1,
	kErrorFatalException = 2,
	kErrorNoSSE2Architecture = 3,
	kErrorIncompatibleRuntimeVersion = 4,
	kErrorUnsupportedGPU = 5,
};

extern ExitErrorCode gExitErrorCode;

#if !UNITY_WIN
#include <setjmp.h>
void InsertThreadExitPoint(jmp_buf *buf);
class AutoRemoveEntryPoint {
	bool m_ShouldRemove;
public:
	AutoRemoveEntryPoint (jmp_buf *buf);
	~AutoRemoveEntryPoint ();
};
#endif
void ExitWithErrorCode(ExitErrorCode error);
ExitErrorCode GetExitErrorCode();
const char *GetExitErrorString(ExitErrorCode err);

#if UNITY_WIN

DWORD OnExcept(DWORD code, LPEXCEPTION_POINTERS exception);

#define UNITY_ENTRY_POINT(x) if(GetExitErrorCode()) return x; __try {
#define UNITY_ENTRY_POINT_NO_RETURN_VALUE() if(GetExitErrorCode()) return; __try {
#define UNITY_ENTRY_POINT_SKIP() if(!GetExitErrorCode()) { __try {
#define UNITY_EXIT_POINT(x) } __except (OnExcept(GetExceptionCode(), GetExceptionInformation())) { return x; }
#define UNITY_EXIT_POINT_NO_RETURN_VALUE() } __except (OnExcept(GetExceptionCode(), GetExceptionInformation())) { return; }
#define UNITY_EXIT_POINT_SKIP() } __except (OnExcept(GetExceptionCode(), GetExceptionInformation())) { } }

#define ERROR_EXIT_THREAD_ENTRY()			\
	__try									\
	{
#define ERROR_EXIT_THREAD_EXIT()			\
	}										\
	__except (OnExcept(GetExceptionCode(),	\
	          GetExceptionInformation()))	\
	{										\
		/* do nothing */					\
	}

#else
#define UNITY_ENTRY_POINT(x) if(GetExitErrorCode()) return x; jmp_buf buf; if (setjmp (buf)) return x; AutoRemoveEntryPoint entry(&buf); 
#define UNITY_ENTRY_POINT_NO_RETURN_VALUE() if(GetExitErrorCode()) return; jmp_buf buf; if (setjmp (buf)) return; AutoRemoveEntryPoint entry(&buf);
#define UNITY_EXIT_POINT(x)
#define UNITY_EXIT_POINT_NO_RETURN_VALUE()

#define ERROR_EXIT_THREAD_ENTRY()			\
	jmp_buf buf;							\
	if (!setjmp (buf))						\
	{										\
		InsertThreadExitPoint (&buf);
#define ERROR_EXIT_THREAD_EXIT()			\
	}

#endif

#else
#define ExitWithErrorCode(x) 
#define GetExitErrorCode() kErrorNone
#endif

#endif
