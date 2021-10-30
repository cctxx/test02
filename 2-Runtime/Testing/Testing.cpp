#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

// Disclaimer: What we do here isn't real unit testing.  It is a reasonable compromise where we accept the
//	codebase mostly as is without major refactors and run reasonably small tests that sorta look like unit
//	tests but are really integration tests.  The key compromise here is that we create a global execution
//  environment that all the tests share and run within.  This environment dictates what tests are allowed
//  to do and what shared functionality they have access to.

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"
#include "External/UnitTest++/src/NunitTestReporter.h"
#include "External/UnitTest++/src/TestReporterStdout.h"
#include "External/UnitTest++/src/TestList.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Utilities/Argv.h"
#if !UNITY_EXTERNAL_TOOL
#include "Runtime/Serialize/PathNamePersistentManager.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Misc/SaveAndLoadHelper.h"
#include "Runtime/Misc/ResourceManager.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Camera/GraphicsSettings.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Input/GetInput.h"
#include "Runtime/BaseClasses/Tags.h"
#endif

#include "Testing.h"
#include "ConsoleTestReporter.h"

using namespace UnitTest;
using namespace std;

#endif

#include <iostream>
#include <fstream>
#include <algorithm>


#if ENABLE_UNIT_TESTS
static void GetLengthsOfLongestSuiteAndTestNames (int& longestSuiteNameLength, int& longestTestNameLength)
{
	longestSuiteNameLength = 0;
	longestTestNameLength = 0;

	TestList& allTests = Test::GetTestList ();
	for (Test* test = allTests.GetHead (); test != NULL; test = test->next)
	{
		longestSuiteNameLength = std::max<int> ((int) strlen (test->m_details.suiteName), longestSuiteNameLength);
		longestTestNameLength = std::max<int> ((int) strlen (test->m_details.testName), longestTestNameLength);
	}
}

static bool SwallowLogMessages (LogType type, const char* log, va_list args)
{
	// Ignore everything.
	return false;
}
#endif

template<typename Predicate>
static int RunUnitTests (const std::string& resultLog, bool summaryOnly, const Predicate& predicate)
{
#if !ENABLE_UNIT_TESTS
	return 0;
#else
	int failures;

	if (!resultLog.empty ())
	{
		std::ostringstream stringStream;
		UnitTest::NunitTestReporter reporter (stringStream);
		UnitTest::TestRunner runner (reporter);

		failures = runner.RunTestsIf (Test::GetTestList (), NULL, predicate, 0);

		std::ofstream fileStream (resultLog.c_str (), std::ios_base::out | std::ios_base::trunc);
		fileStream << stringStream.str();
	}
	else
	{
		int longestSuiteNameLength;
		int longestTestNameLength;
		GetLengthsOfLongestSuiteAndTestNames (longestSuiteNameLength, longestTestNameLength);

		ConsoleTestReporter reporter;
		reporter.SetShowOnlySummary (summaryOnly);
		reporter.SetTestNameColumnIndex (longestSuiteNameLength + 4);
		reporter.SetResultColumnIndex (longestSuiteNameLength + 4 + longestTestNameLength + 4);

		UnitTest::TestRunner runner (reporter);

		failures = runner.RunTestsIf (Test::GetTestList (), NULL, predicate, 0);
	}

	return failures;
#endif
}

template<typename Predicate>
static void PrintUnitTestList (const Predicate& filter)
{
#if ENABLE_UNIT_TESTS

	// Group tests by their files.
	
	int matchingTestCount = 0;
	vector<const char*> listedFileNames;
	for (Test* temp = Test::GetTestList ().GetHead(); temp != NULL; temp = temp->next)
	{
		if (!filter (temp))
			continue;

		const char* filename = temp->m_details.filename;

		// Find out whether we've already listed this file.
		bool alreadyListedThisFile = false;
		for (int i = 0; i < listedFileNames.size (); ++i)
			if (strcmp (listedFileNames[i], filename) == 0)
			{
				alreadyListedThisFile = true;
				break;
			}

		if (alreadyListedThisFile)
			continue;

		// Print filename.
		printf_console ("%s:\n", filename);
		listedFileNames.push_back (filename);

		// List matching tests in file.
		for (Test* test = Test::GetTestList ().GetHead(); test != NULL; test = test->next)
		{
			if (!filter (test))
				continue;

			if (strcmp (test->m_details.filename, filename) != 0)
				continue;

			printf_console ("\t[%s] %s\n", test->m_details.suiteName, test->m_details.testName);
			++ matchingTestCount;
		}
	}

	printf_console ("%i test%s\n", matchingTestCount, matchingTestCount == 1 ? "" : "s");

#endif
}

#if !UNITY_EXTERNAL_TOOL

#if ENABLE_UNIT_TESTS
struct TestFilter
{
	std::vector<std::string> m_MatchNames;
	TestFilter(const std::vector<std::string>& matchNames)
		: m_MatchNames (matchNames)
	{
		// Lowercase everything.
		for (int i = 0; i < m_MatchNames.size(); ++i)
			m_MatchNames[i] = ToLower (m_MatchNames[i]);
	}

	bool operator () (const Test* test) const
	{
		if (m_MatchNames.empty ())
			return true;

		for (int i = 0; i < m_MatchNames.size(); ++i)
		{
			if (ToLower (std::string (test->m_details.suiteName)).find (m_MatchNames[i]) != std::string::npos ||
				ToLower (std::string (test->m_details.testName)).find (m_MatchNames[i]) != std::string::npos)
				return true;
		}

		return false;
	}
};

inline void CreateAndInstallGameManager (ManagerContext::Managers id)
{
	GameManager* instance = CreateGameManager (GetManagerContext ().m_ManagerClassIDs[id]);
	SetManagerPtrInContext (id, instance);
}

static void InitializeScriptMapper ()
{
	// Create manager.
	CreateAndInstallGameManager (ManagerContext::kScriptMapper);

	#if UNITY_EDITOR
	// Populate with builtin shaders.  Only available in editor.
	GetBuiltinExtraResourceManager ().RegisterShadersWithRegistry (GetScriptMapperPtr ());
	#endif
}

static void InitializeGraphicsSettings ()
{
	CreateAndInstallGameManager (ManagerContext::kGraphicsSettings);
	GetGraphicsSettings ().SetDefaultAlwaysIncludedShaders ();
}

static void InitializePhysicsManager ()
{
	#if ENABLE_PHYSICS
	CreateAndInstallGameManager (ManagerContext::kPhysicsManager);
	#endif
}

static void InitializeTagManager ()
{
	CreateAndInstallGameManager (ManagerContext::kTagManager);
	RegisterDefaultTagsAndLayerMasks ();
}

static void InitializeBuildSettings ()
{
	CreateAndInstallGameManager (ManagerContext::kBuildSettings);
}
#endif

void RunUnitTestsIfRequiredAndExit ()
{
#if !ENABLE_UNIT_TESTS
	return;
#else

	const bool listUnitTests = HasARGV ("listUnitTests");
	const bool runUnitTests = HasARGV ("runUnitTests");

	// Check if we are supposed to run or list unit tests.
	if (!listUnitTests && !runUnitTests)
		return;

	// Yes, so initialize the systems we need.
	
	// We log to stdout so get that in place on Windows.
#if UNITY_WIN
	OpenConsole ();
#endif

	// For unit testing, we're not interested in log output from engine initialization.
	// Temporarily disable logging.
	SetLogEntryHandler (SwallowLogMessages);

	// We want the test runner to behave like batchmode and not pop up
	// any dialogs, so force running in batchmode.
	SetIsBatchmode (true);

	// Start with persistent manager.  Required by InitializeEngineNoGraphics().
	UNITY_NEW_AS_ROOT (PathNamePersistentManager (0), kMemManager, "PersistentManager", "");

	if (!InitializeEngineNoGraphics ())
	{
		fprintf (stderr, "Failed to initialize engine (no graphics)!\n");
		exit (-1);
	}

	if (!InitializeEngineGraphics ())
	{
		fprintf (stderr, "Failed to initialize engine!\n");
		exit (-1);
	}

	// Initialize the global game managers we want to have available.
	InitializeScriptMapper ();
	InitializeGraphicsSettings ();
	InitializePhysicsManager ();
	InitializeTagManager ();
	InitializeBuildSettings ();

	////TODO: this path is broken; the EXPECT stuff doesn't work for it
	std::string log;
	#if 0
	if (HasARGV ("unitTestsLog"))
		log = GetFirstValueForARGV ("unitTestsLog");
	#endif

	const bool showOnlySummary = HasARGV ("unitTestsSummaryOnly");

	std::vector<std::string> matchNames;
	if (runUnitTests)
		matchNames = GetValuesForARGV ("runUnitTests");
	else if (listUnitTests)
		matchNames = GetValuesForARGV ("listUnitTests");

	TestFilter filter (matchNames);

	// Run or list tests.
	int numFailures = 0;
	if (runUnitTests)
	{
		numFailures = RunUnitTests (log, showOnlySummary, filter);
	}
	else if (listUnitTests)
	{
		SetLogEntryHandler (NULL);
		PrintUnitTestList (filter);
	}
	
	// Shut down.
	CleanupEngine ();
	InputShutdown ();

	// Done.
	exit (numFailures != 0 ? 1 : 0);

#endif // ENABLE_UNIT_TESTS
}

#endif // UNITY_EXTERNAL_TOOL

int RunUnitTests (const std::string& resultLog)
{
#if !ENABLE_UNIT_TESTS
	return 0;
#else
	return RunUnitTests (resultLog, false, UnitTest::True ());
#endif
}

void ExpectLogMessageTriggeredByTest (LogType type, const char* logFragment)
{
#if ENABLE_UNIT_TESTS
	ConsoleTestReporter::GetInstance ()->ExpectLogMessage (type, logFragment);
#endif
}

#if ENABLE_PERFORMANCE_TESTS

	void TestMultiplyMatrices();
	void RUN_PERFORMANCE_TESTS ()
	{
		TestMultiplyMatrices();
	}

#endif
