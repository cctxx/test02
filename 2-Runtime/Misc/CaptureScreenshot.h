#ifndef _CAPTURESCREENSHOT_H
#define _CAPTURESCREENSHOT_H

#include "Configuration/UnityConfigure.h"

#undef CAPTURE_SCREENSHOT_AVAILABLE

// In web player we never capture screenshots. Don't even compile the code in (saves whole pnglib!)
#if (WEBPLUG && !SUPPORT_REPRODUCE_LOG && !UNITY_PEPPER) 
#define CAPTURE_SCREENSHOT_AVAILABLE 0
#else
#define CAPTURE_SCREENSHOT_AVAILABLE 1
// Player connection might be used to transfer screenshots and async is a pain there
#define CAPTURE_SCREENSHOT_THREAD (SUPPORT_THREADS && !UNITY_WII && !ENABLE_PLAYERCONNECTION)
#endif


#if CAPTURE_SCREENSHOT_AVAILABLE

#include <string>

void QueueScreenshot (const std::string& path, int superSize);
bool IsScreenshotQueued ();
void UpdateCaptureScreenshot ();
void FinishAllCaptureScreenshot ();

#endif


#endif
