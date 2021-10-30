#ifndef REPRODUCTIONLOG_H
#define REPRODUCTIONLOG_H

#include "Configuration/UnityConfigure.h"

#if SUPPORT_REPRODUCE_LOG

#define REPRODUCE_VERSION 6

enum ReproduceMode { kPlaybackUninitialized = -1,
					 kNormalPlayback = 0,
					 kGenerateReproduceLog = 1,
					 kPlaybackReproduceLog = 2,
					 kGenerateReproduceLogAndRemapWWW = 3  };

class WWW;

ReproduceMode GetReproduceMode();
int GetReproduceVersion();
void CaptureScreenshotReproduction(bool manual);
bool HasNormalPlaybackSpeed();
void BatchInitializeReproductionLog();
void ReadWriteAbsoluteUrl(UnityStr& srcValue, UnityStr& absoluteUrl);
void WriteWebplayerSize(int width, int height);
void ReadWriteReproductionInput();
void ReadWriteReproductionTime();

void CreateWWWReproduce(WWW* www, const std::string& url, std::string& remappedUrl, int &postSize);
void CompleteWWWReproduce(WWW* www, const std::string& url, const UInt8* buffer, int size);

void ReproductionWriteExitMessage(int result);
void ReproductionExitPlayer (int result, bool writeExitMessage=true);
bool ShouldExitReproduction();

std::string GetReproductionDirectory();
void RepeatReproductionScreenshot();
void ReadWriteReproductionEnd();
bool RunningReproduction();
FILE* GetReproduceOutputLogFile();
void PlayerCleanupReproduction();
void ReproduceWriteMainDataFile(const UInt8* buffer, int size);

void WriteReproductionString (std::ostream& out, const std::string& value);
void ReadReproductionString (std::istream& in, std::string& value);

bool ShouldWaitForCompletedDownloads ();
void CleanupWWW (WWW* www);


std::ifstream* GetReproduceInStream ();
std::ofstream* GetReproduceOutStream ();

int GetReproduceVersion ();
bool CheckReproduceTag(const std::string& tag, std::ifstream& stream);
void CheckReproduceTagAndExit(const std::string& tag, std::ifstream& stream);

void FailReproduction (const std::string& err);

void WriteFloat (std::ostream& out, float& value);
void ReadFloat (std::istream& in, float& value);

void WriteFloat (std::ostream& out, double& value);
void ReadFloat (std::istream& in, double& value);

void WriteBigFloat (std::ostream& out, double& value);
void ReadBigFloat (std::istream& in, double& value);

#else

inline bool RunningReproduction() { return false; }

#endif

#if SUPPORT_REPRODUCE_LOG && SUPPORT_REPRODUCE_LOG_GFX_TRACE
void LogToScreenshotLog(std::string str);
#else
#define LogToScreenshotLog(x) 
#endif

#endif //REPRODUCTIONLOG_H
