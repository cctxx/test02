#ifndef TESTING_H
#define TESTING_H

#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

/// Use this to deal with asserts and other things that get logged.
/// If your test explicitly *expects* a certain piece of code to detect
/// and log an error, set up an expected message.
///
/// The given log fragment can simply be a substring of the actual log
/// message.
///
/// @example
/// EXPECT (Error, "Cannot find");
/// MethodThatTriggersError ();
/// @endexample
///
/// @param type Log type (i.e. enum value from LogType without the "LogType_" prefix").
/// @param logFragment Substring of log message that is expected.
///
/// @note Debug messages are ignored entirely and cannot be expected.
/// @note Any log messages that are not expected will lead to a failure of the
///		running unit test.
/// @note The order of log messages is not checked but a single EXPECT will only
///     cause the acceptance of a single occurrence of the message.
/// @note Expecting messages that do not arrive will cause tests to fail.
#define EXPECT(type, logFragment) \
	ExpectLogMessageTriggeredByTest (LogType_##type, logFragment)

/// Expect a log message to be triggered by the currently
/// executing unit test.
///
/// @see EXPECT
void ExpectLogMessageTriggeredByTest (LogType type, const char* logFragment);

#endif // ENABLE_UNIT_TESTS


/// If the "-runUnitTests" command-line argument is present, run unit tests
/// and exit the process with a status code indicating success or failure.
void RunUnitTestsIfRequiredAndExit ();

int RunUnitTests (const std::string& resultLog);


////TODO: needs to be cleaned up
#define ENABLE_PERFORMANCE_TESTS 0
#if ENABLE_PERFORMANCE_TESTS
	void RUN_PERFORMANCE_TESTS ();
#else
	#define RUN_PERFORMANCE_TESTS() 
#endif

#endif
