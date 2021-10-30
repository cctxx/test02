#include "UnityPrefix.h"
#include "ErrorExit.h"
#include "Runtime/Misc/ReproductionLog.h"
#if SUPPORT_ERROR_EXIT
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Stacktrace.h"

#if UNITY_WIN

#include "Configuration/UnityConfigureOther.h"
#include "Runtime/Mono/MonoIncludes.h"

#include "PlatformDependent/Win/WinUtils.h"

#if !UNITY_WIN_ENABLE_CRASHHANDLER
#error "UNITY_WIN_ENABLE_CRASHHANDLER must be defined"
#endif

#include "../../Tools/BugReporterWin/lib/CrashHandler.h"
extern CrashHandler *gUnityCrashHandler;

#endif

ExitErrorCode gExitErrorCode = kErrorNone;
bool gExitErrorDidCleanup = false;
#if !UNITY_WIN
static UNITY_TLS_VALUE(jmp_buf*) gJumpBuffer;
#endif

ExitErrorCode GetExitErrorCode()
{
	return gExitErrorCode;
}

#if UNITY_PEPPER
#define STACKTRACE() ""
#else
#define STACKTRACE() GetStacktrace().c_str()
#endif

void ExitWithErrorCode(ExitErrorCode error)
{
	printf_console("Exit with error code: %d\n", error);
	printf_console("\n========== OUTPUTING STACK TRACE ==================\n\n");
	printf_console("%s\n", STACKTRACE());
	printf_console("\n========== END OF STACKTRACE ===========\n\n");
	
	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
	{
		ErrorString("ExitWithErrorCode invoked, exiting application because we are in reproduction mode.");
		ReproductionExitPlayer(1);
	}
	#endif

	#if !UNITY_RELEASE
	printf_console("\n*** DEBUGMODE -> ExitWithError code calling abort for debugging ease *** ");
	abort();
	#endif
	
	gExitErrorCode = error;
	
	#if !UNITY_WIN

	jmp_buf *buf = gJumpBuffer;	
	
	if (buf)
	{
		printf_console("Got error %d. Bailing out.\n", error);
		longjmp (*buf, 1);
	}
	else
	{
		printf_console("Got error %d. Cannot exit at this point.\n", error);
	}

	#endif
}

#if !UNITY_WIN

void InsertThreadExitPoint(jmp_buf *buf)
{
	gJumpBuffer = buf;
}

AutoRemoveEntryPoint::AutoRemoveEntryPoint (jmp_buf *buf)
{
	if (gJumpBuffer)
		m_ShouldRemove = false;
	else
	{
		m_ShouldRemove = true;
		gJumpBuffer = buf;
	}
}

AutoRemoveEntryPoint::~AutoRemoveEntryPoint ()
{
	if (m_ShouldRemove)
		gJumpBuffer = NULL;
}

#endif

const char* GetExitErrorString(ExitErrorCode err)
{
	switch(err)
	{
		case kErrorSecurityBreach:
			return "The content was stopped because it\nattempted an insecure operation.";
		case kErrorFatalException:
			return "The content was stopped because a fatal\ncontent error has been detected.";
		case kErrorNoSSE2Architecture:
			return "Unity Web Player requires an SSE2 capable CPU.";
		case kErrorIncompatibleRuntimeVersion:
			return "The Unity Web Player content which you are trying to load was built with an older version of the Unity Editor, and is incompatible with this runtime.";
		case kErrorUnsupportedGPU:
			return "Unity Web Player requires DX9 level graphics card.\nMake sure you have graphics card drivers installed.";
		default:
			return NULL;
	}
}

#if UNITY_WIN

extern LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilter;
int HandleSignal( EXCEPTION_POINTERS* ep );

DWORD OnExcept(DWORD code, LPEXCEPTION_POINTERS exception)
{
	__try
	{
		DWORD result;
		if( NULL != exceptionFilter )
		{
			#if WEBPLUG
			winutils::ProcessInternalCrash(exception, false);
			#endif

			result = exceptionFilter(exception);
		}
		else
		{
			result = mono_unity_seh_handler(exception);
		}

		if (EXCEPTION_CONTINUE_EXECUTION != result)
		{
			gExitErrorCode = kErrorFatalException;
			result = EXCEPTION_EXECUTE_HANDLER;
		}

		return result;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		gExitErrorCode = kErrorFatalException;
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

#endif
#endif

