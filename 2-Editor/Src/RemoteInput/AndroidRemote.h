
#ifndef ANDROID_REMOTE_H
#define ANDROID_REMOTE_H

#include "Runtime/Math/Rect.h"

extern void AndroidRemoteUpdate();
extern void AndroidUpdateScreenShot(const Rectf &rect);
extern bool AndroidHasRemoteConnected();
extern void AndroidRemoteSetConnected(bool connected);

#endif
