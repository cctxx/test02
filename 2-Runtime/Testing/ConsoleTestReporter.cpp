#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "ConsoleTestReporter.h"
#include <algorithm>
#include <stdio.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

using namespace UnitTest;
using namespace std;

static ConsoleTestReporter* g_Instance;

static bool UnitTestLogEntryHandler (LogType type, const char* log, va_list args)
{
	Assert (g_Instance != NULL);
	
	// Expand message string.
	char buffer[4096];
	vsnprintf (buffer, sizeof (buffer), log, args);
	va_end (args);

	// If we're not currently running a test, just dump the message straight to
	// stdout.  This path is used for printing the test start/finish/fail messages.
	if (!g_Instance->IsCurrentlyRunningTest ())
	{
		fputs (buffer, stdout);
		fflush (stdout);
		
		#if UNITY_WIN
		OutputDebugString (buffer);
		#endif
	}
	else
	{
		// Otherwise, feed the message into the reporter.
		// Ignore debug log messages.
		if (type != LogType_Debug)
			g_Instance->ReportMessage (type, buffer);
	}

	// Disable all normal logging.
	return false;
}

ConsoleTestReporter::ConsoleTestReporter ()
	: m_ShowOnlySummary (false)
	, m_TestNameColumnIndex (100)
	, m_ResultColumnIndex (256)
	, m_IsCurrentlyRunningTest (false)
	, m_CurrentTestIsFailure (false)
{
	Assert (g_Instance == NULL);
	g_Instance = this;

	// Install our log override.
	SetLogEntryHandler (UnitTestLogEntryHandler);
}

ConsoleTestReporter::~ConsoleTestReporter ()
{
	SetLogEntryHandler (NULL);
	g_Instance = NULL;
}

ConsoleTestReporter* ConsoleTestReporter::GetInstance ()
{
	return g_Instance;
}

void ConsoleTestReporter::ExpectLogMessage (LogType type, const char* logFragment)
{
	Assert (type != LogType_Debug);

	if (!IsCurrentlyRunningTest ())
		return;

	m_ExpectedLogMessagesForCurrentTest.push_back (LogMessage (type, logFragment));
}

void ConsoleTestReporter::ReportTestStart (TestDetails const& test)
{
	if (!m_ShowOnlySummary)
	{
		Assert (m_ResultColumnIndex > m_TestNameColumnIndex);

		char buffer[1024];
		const int suiteNameLength = strlen (test.suiteName);
		Assert (suiteNameLength < sizeof(buffer) - 4);

		// Create '[TestSuite] ' string padded out with blanks up to the column
		// for the test name.
		memset (buffer, ' ', sizeof (buffer));
		buffer[0] = '[';
		memcpy (&buffer[1], test.suiteName, suiteNameLength);
		buffer[suiteNameLength + 1] = ']';
		buffer[std::min<int> (m_TestNameColumnIndex, sizeof (buffer))] = '\0';

		// Print '[TestSuite]    TestName'.
		printf_console ("%s%s", buffer, test.testName);

		// Print blanks to pad out to result column.
		const int numSpacesToPad = m_ResultColumnIndex - m_TestNameColumnIndex - strlen (test.testName);
		memset (buffer, ' ', sizeof (buffer));
		buffer[std::min<int> (numSpacesToPad, sizeof (buffer))] = '\0';
		printf_console (buffer);
	}

	m_CurrentTest = test;
	m_IsCurrentlyRunningTest = true;
	m_CurrentTestIsFailure = false;
}

void ConsoleTestReporter::ReportFailure (TestDetails const& test, char const* failure)
{
	// Memorize failure.
	Failure details;
	details.fileName = test.filename;
	details.lineNumber = test.lineNumber;
	details.text = failure;
	m_CurrentTestFailures.push_back (details);

	MarkCurrentTestAsFailure ();
}

void ConsoleTestReporter::ReportMessage (LogType type, string message)
{
	// Check whether we have expected this message to come in.
	for (size_t i = 0; i < m_ExpectedLogMessagesForCurrentTest.size (); ++i)
	{
		// Skip if type doesn't match.
		if (m_ExpectedLogMessagesForCurrentTest[i].first != type)
			continue;

		// Check whether the expected fragment is found in the current message.
		if (message.find (m_ExpectedLogMessagesForCurrentTest[i].second) != string::npos)
		{
			// Remove it.  We only accept one occurrence.
			m_ExpectedLogMessagesForCurrentTest.erase (m_ExpectedLogMessagesForCurrentTest.begin () + i);

			// Yes, so all ok.
			return;
		}
	}

	// Not an expected message.  Record.
	m_UnexpectedLogMessagesForCurrentTest.push_back (LogMessage (type, message));

	MarkCurrentTestAsFailure ();
}

void ConsoleTestReporter::ReportTestFinish (TestDetails const& test, float secondsElapsed)
{
	m_IsCurrentlyRunningTest = false;

	// If we are still expecting messages, fail the test.
	if (!m_ExpectedLogMessagesForCurrentTest.empty ())
		MarkCurrentTestAsFailure ();

	if (!m_ShowOnlySummary)
	{
		// Print status.
		if (m_CurrentTestIsFailure)
			printf_console ("FAIL!!!!\n");
		else
			printf_console ("PASS (%ims)\n", (int) (secondsElapsed * 1000.f));
		
		// Print failures.
		for (size_t i = 0; i < m_CurrentTestFailures.size (); ++i)
		{
			const Failure& failure = m_CurrentTestFailures[i];
			printf_console ("\tCHECK FAILURE: %s\n\t\t(%s:%i)\n",
				failure.text.c_str (),
				failure.fileName.c_str (),
				failure.lineNumber);
		}

		// Print unexpected messages.
		for (size_t i = 0; i < m_UnexpectedLogMessagesForCurrentTest.size (); ++i)
			printf_console ("\tUNEXPECTED %s: %s\n",
				LogTypeToString (m_UnexpectedLogMessagesForCurrentTest[i].first),
				m_UnexpectedLogMessagesForCurrentTest[i].second.c_str ());

		// Print expected messages that didn't show.
		for (size_t i = 0; i < m_ExpectedLogMessagesForCurrentTest.size (); ++i)
			printf_console ("\tEXPECTED %s: %s\n",
				LogTypeToString (m_ExpectedLogMessagesForCurrentTest[i].first),
				m_ExpectedLogMessagesForCurrentTest[i].second.c_str ());
	}

	// Clear state of current test.
	m_CurrentTestFailures.clear ();
	m_UnexpectedLogMessagesForCurrentTest.clear ();
	m_ExpectedLogMessagesForCurrentTest.clear ();
	m_CurrentTest = TestDetails ();
}

void ConsoleTestReporter::ReportSummary (int totalTestCount, int failedTestCount, int failureCount, float secondsElapsed)
{
	// Print counters (rely on our fail test counter since we also count unexpected messages
	// as failures).
	printf_console ("Ran %i tests with %i failures in %.2f seconds\n", totalTestCount, m_FailedTests.size (), secondsElapsed);

	// Print failures.
	for (int i = 0; i < m_FailedTests.size (); ++i)
	{
		const TestDetails& test = m_FailedTests[i];
		printf_console ("\tFAILED: %s [%s]\n", test.testName, test.suiteName);
	}
}

#endif
