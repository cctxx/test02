
#ifndef GENERIC_REMOTE_H
#define GENERIC_REMOTE_H

#include "Runtime/Math/Rect.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Input/GetInput.h"

extern void RemoteUpdate();
extern void RemoteUpdateScreenShot(const Rectf &rect);
extern bool RemoteIsConnected();
extern void RemoteSetConnected(bool connected);

extern size_t RemoteGetTouchCount();
#if ENABLE_NEW_EVENT_SYSTEM
extern Touch* RemoteGetTouch(unsigned index);
#else
extern bool RemoteGetTouch(unsigned index, Touch& touch);
#endif
extern size_t RemoteGetActiveTouchCount();
extern size_t RemoteGetAccelerationCount();
extern void RemoteGetAcceleration(size_t index, Acceleration& acceleration);
extern unsigned RemoteGetDeviceOrientation();
extern Vector3f RemoteGetAcceleration();

Vector3f RemoteGetGyroRotationRate(int idx);
bool RemoteIsGyroAvailable();
Vector3f RemoteGetGyroRotationRateUnbiased(int idx);
Vector3f RemoteGetGravity(int idx);
Vector3f RemoteGetUserAcceleration(int idx);
Quaternionf RemoteGetAttitude(int idx);
bool RemoteIsGyroEnabled(int idx);
void RemoteSetGyroEnabled(int idx, bool enabled);
float RemoteGetGyroUpdateInterval(int idx);
void RemoteSetGyroUpdateInterval(int idx, float interval);
int RemoteGetGyro();

extern float RemoteGetScreenWidth();
extern float RemoteGetScreenHeight();

#endif
