#pragma once

#include "External/UnitTest++/src/TestReporter.h"
#include "External/UnitTest++/src/TestDetails.h"


/// Unit test reporter that logs to console output.
class ConsoleTestReporter : public UnitTest::TestReporter
{
public:

	ConsoleTestReporter ();
	virtual ~ConsoleTestReporter ();

	virtual void ReportTestStart (UnitTest::TestDetails const& test);
	virtual void ReportFailure (UnitTest::TestDetails const& test, char const* failure);
	virtual void ReportTestFinish (UnitTest::TestDetails const& test, float secondsElapsed);
	virtual void ReportSummary (int totalTestCount, int failedTestCount, int failureCount, float secondsElapsed);
	void ReportMessage (LogType type, std::string message);

	void ExpectLogMessage (LogType type, const char* logFragment);

	void SetShowOnlySummary (bool value) { m_ShowOnlySummary = value; }
	void SetTestNameColumnIndex (int value) { m_TestNameColumnIndex = value; }
	void SetResultColumnIndex (int value) { m_ResultColumnIndex = value; }

	bool IsCurrentlyRunningTest () const { return m_IsCurrentlyRunningTest; }

	static ConsoleTestReporter* GetInstance();

private:

	bool m_IsCurrentlyRunningTest;
	bool m_ShowOnlySummary;
	int m_TestNameColumnIndex;
	int m_ResultColumnIndex;

	struct Failure
	{
		std::string text;
		std::string fileName;
		int lineNumber;
	};

	typedef std::pair<LogType, std::string> LogMessage;

	std::vector<UnitTest::TestDetails> m_FailedTests;
	
	bool m_CurrentTestIsFailure;
	UnitTest::TestDetails m_CurrentTest;
	std::vector<Failure> m_CurrentTestFailures;
	std::vector<LogMessage> m_UnexpectedLogMessagesForCurrentTest;
	std::vector<LogMessage> m_ExpectedLogMessagesForCurrentTest;

	void MarkCurrentTestAsFailure ()
	{
		if (m_CurrentTestIsFailure)
			return;

		m_CurrentTestIsFailure = true;
		m_FailedTests.push_back (m_CurrentTest);
	}
};
