#ifndef BUGREPORTINGTOOLS_H
#define BUGREPORTINGTOOLS_H

enum BugReportMode { kManualOpen = 0, kCrashbug = 1, kFatalError = 2, kCocoaExceptionOrAssertion = 3, kManualSimple = 4 };

void LaunchBugReporter (BugReportMode mode);

/// Calls exit but makes sure that it won't launch the bug reporter when starting up next time.
void ExitDontLaunchBugReporter (int exitValue = 0);

#endif
