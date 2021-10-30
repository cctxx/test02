#include "UnityPrefix.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Input/GetInput.h"
#include "Editor/Src/RemoteInput/AndroidRemote.h"
#include "Editor/Src/RemoteInput/GenericRemote.h"
#include "Editor/Src/RemoteInput/iPhoneRemoteImpl.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/RemoteInput/RemoteInput.h"

extern size_t iPhoneGetTouchCount();
extern size_t AndroidGetTouchCount();
#if ENABLE_NEW_EVENT_SYSTEM
extern Touch* AndroidGetTouch(unsigned int index);
extern Touch* iPhoneGetTouch(unsigned int index);
#else
extern bool AndroidGetTouch(unsigned index, Touch& touch);
extern bool iPhoneGetTouch(unsigned index, Touch& touch);
#endif
extern size_t AndroidGetActiveTouchCount();
extern size_t iPhoneGetActiveTouchCount();
extern size_t AndroidGetAccelerationCount();
extern size_t iPhoneGetAccelerationCount();
extern void AndroidGetAcceleration(size_t index, Acceleration& acceleration);
extern void iPhoneGetAcceleration(size_t index, Acceleration& acceleration);
extern unsigned AndroidGetOrientation();
extern unsigned iPhoneGetOrientation();
extern Vector3f AndroidGetAcceleration();
extern Vector3f iPhoneGetAcceleration();

Vector3f iPhoneGetGyroRotationRate(int idx);
bool iPhoneIsGyroAvailable();
Vector3f iPhoneGetGyroRotationRateUnbiased(int idx);
Vector3f iPhoneGetGravity(int idx);
Vector3f iPhoneGetUserAcceleration(int idx);
Quaternionf iPhoneGetAttitude(int idx);
bool iPhoneIsGyroEnabled(int idx);
void iPhoneSetGyroEnabled(int idx, bool enabled);
float iPhoneGetGyroUpdateInterval(int idx);
void iPhoneSetGyroUpdateInterval(int idx, float interval);
int iPhoneGetGyro();

Vector3f AndroidGetGyroRotationRate(int idx);
bool AndroidIsGyroAvailable();
Vector3f AndroidGetGyroRotationRateUnbiased(int idx);
Vector3f AndroidGetGravity(int idx);
Vector3f AndroidGetUserAcceleration(int idx);
Quaternionf AndroidGetAttitude(int idx);
bool AndroidIsGyroEnabled(int idx);
void AndroidSetGyroEnabled(int idx, bool enabled);
float AndroidGetGyroUpdateInterval(int idx);
void AndroidSetGyroUpdateInterval(int idx, float interval);
int AndroidGetGyro();

extern float iPhoneGetScreenWidth();
extern float iPhoneGetScreenHeight();
extern float AndroidGetScreenWidth();
extern float AndroidGetScreenHeight();

static size_t (* GetTouchCountProc)() = iPhoneGetTouchCount;
#if ENABLE_NEW_EVENT_SYSTEM
static Touch* (* GetTouchProc)(unsigned int index) = iPhoneGetTouch;
#else
static bool (* GetTouchProc)(unsigned index, Touch &touch) = iPhoneGetTouch;
#endif
static size_t (* GetActiveTouchCountProc)() = iPhoneGetActiveTouchCount;
static size_t (* GetAccelerationCountProc)() = iPhoneGetAccelerationCount;
static void (* GetAccelerationDataProc)(size_t index, Acceleration &acceleration) =
    iPhoneGetAcceleration;
static unsigned (* GetOrientationProc)() = iPhoneGetOrientation;
static Vector3f (* GetAccelerationProc)() = iPhoneGetAcceleration;
static float (* GetScreenWidthProc)() = iPhoneGetScreenWidth;
static float (* GetScreenHeightProc)() = iPhoneGetScreenHeight;

static Vector3f (* GetGyroRotationRateProc)(int idx) = iPhoneGetGyroRotationRate;
static bool (* IsGyroAvailableProc)() = iPhoneIsGyroAvailable;
static Vector3f (* GetGyroRotationRateUnbiasedProc)(int idx) = iPhoneGetGyroRotationRateUnbiased;
static Vector3f (* GetGravityProc)(int idx) = iPhoneGetGravity;
static Vector3f (* GetUserAccelerationProc)(int idx) = iPhoneGetUserAcceleration;
static Quaternionf (* GetAttitudeProc)(int idx) = iPhoneGetAttitude;
static bool (* IsGyroEnabledProc)(int idx) = iPhoneIsGyroEnabled;
static void (* SetGyroEnabledProc)(int idx, bool enabled) = iPhoneSetGyroEnabled;
static float (* GetGyroUpdateIntervalProc)(int idx) = iPhoneGetGyroUpdateInterval;
static void (* SetGyroUpdateIntervalProc)(int idx, float interval) = iPhoneSetGyroUpdateInterval;
static int (* GetGyroProc)() = iPhoneGetGyro;


void RemoteInputUpdate()
{
	if (iPhoneHasRemoteConnected())
	{
		AndroidRemoteSetConnected(false);
		RemoteSetConnected(false);
		GetTouchCountProc = iPhoneGetTouchCount;
		GetTouchProc = iPhoneGetTouch;
		GetActiveTouchCountProc = iPhoneGetActiveTouchCount;
		GetAccelerationCountProc = iPhoneGetAccelerationCount;
		GetAccelerationDataProc = iPhoneGetAcceleration;
		GetOrientationProc = iPhoneGetOrientation;
		GetAccelerationProc = iPhoneGetAcceleration;
		GetGyroRotationRateProc = iPhoneGetGyroRotationRate;
		IsGyroAvailableProc = iPhoneIsGyroAvailable;
		GetGyroRotationRateUnbiasedProc = iPhoneGetGyroRotationRateUnbiased;
		GetGravityProc = iPhoneGetGravity;
		GetUserAccelerationProc = iPhoneGetUserAcceleration;
		GetAttitudeProc = iPhoneGetAttitude;
		IsGyroEnabledProc = iPhoneIsGyroEnabled;
		SetGyroEnabledProc = iPhoneSetGyroEnabled;
		GetGyroUpdateIntervalProc = iPhoneGetGyroUpdateInterval;
		SetGyroUpdateIntervalProc = iPhoneSetGyroUpdateInterval;
		GetGyroProc = iPhoneGetGyro;
		GetScreenWidthProc = iPhoneGetScreenWidth;
		GetScreenHeightProc = iPhoneGetScreenHeight;
	}
	else if (AndroidHasRemoteConnected())
	{
		iPhoneRemoteSetConnected(false);
		RemoteSetConnected(false);
		GetTouchCountProc = AndroidGetTouchCount;
		GetTouchProc = AndroidGetTouch;
		GetActiveTouchCountProc = AndroidGetActiveTouchCount;
		GetAccelerationCountProc = AndroidGetAccelerationCount;
		GetAccelerationDataProc = AndroidGetAcceleration;
		GetOrientationProc = AndroidGetOrientation;
		GetAccelerationProc = AndroidGetAcceleration;
		GetGyroRotationRateProc = AndroidGetGyroRotationRate;
		IsGyroAvailableProc = AndroidIsGyroAvailable;
		GetGyroRotationRateUnbiasedProc = AndroidGetGyroRotationRateUnbiased;
		GetGravityProc = AndroidGetGravity;
		GetUserAccelerationProc = AndroidGetUserAcceleration;
		GetAttitudeProc = AndroidGetAttitude;
		IsGyroEnabledProc = AndroidIsGyroEnabled;
		SetGyroEnabledProc = AndroidSetGyroEnabled;
		GetGyroUpdateIntervalProc = AndroidGetGyroUpdateInterval;
		SetGyroUpdateIntervalProc = AndroidSetGyroUpdateInterval;
		GetGyroProc = AndroidGetGyro;
		GetScreenWidthProc = AndroidGetScreenWidth;
		GetScreenHeightProc = AndroidGetScreenHeight;
	}
	else if (RemoteIsConnected())
	{
		iPhoneRemoteSetConnected(false);
		AndroidRemoteSetConnected(false);
		GetTouchCountProc = RemoteGetTouchCount;
		GetTouchProc = RemoteGetTouch;
		GetActiveTouchCountProc = RemoteGetActiveTouchCount;
		GetAccelerationCountProc = RemoteGetAccelerationCount;
		GetAccelerationDataProc = RemoteGetAcceleration;
		GetOrientationProc = RemoteGetDeviceOrientation;
		GetAccelerationProc = RemoteGetAcceleration;
		GetGyroRotationRateProc = RemoteGetGyroRotationRate;
		IsGyroAvailableProc = RemoteIsGyroAvailable;
		GetGyroRotationRateUnbiasedProc = RemoteGetGyroRotationRateUnbiased;
		GetGravityProc = RemoteGetGravity;
		GetUserAccelerationProc = RemoteGetUserAcceleration;
		GetAttitudeProc = RemoteGetAttitude;
		IsGyroEnabledProc = RemoteIsGyroEnabled;
		SetGyroEnabledProc = RemoteSetGyroEnabled;
		GetGyroUpdateIntervalProc = RemoteGetGyroUpdateInterval;
		SetGyroUpdateIntervalProc = RemoteSetGyroUpdateInterval;
		GetGyroProc = RemoteGetGyro;
		GetScreenWidthProc = RemoteGetScreenWidth;
		GetScreenHeightProc = RemoteGetScreenHeight;
	}

	AndroidRemoteUpdate();
	iPhoneRemoteUpdate();

	RemoteUpdate();	// Generic remote
}


size_t GetTouchCount()
{
    return GetTouchCountProc();
}

#if ENABLE_NEW_EVENT_SYSTEM
Touch* GetTouch (unsigned int index)
{
    return GetTouchProc(index);
}
#else
bool GetTouch(unsigned index, Touch& touch)
{
	return GetTouchProc(index, touch);
}
#endif


size_t GetActiveTouchCount()
{
    return GetActiveTouchCountProc();
}


size_t GetAccelerationCount()
{
    return GetAccelerationCountProc();
}


void GetAcceleration(size_t index, Acceleration& acceleration)
{
    GetAccelerationDataProc(index, acceleration);
}


unsigned GetOrientation()
{
    return GetOrientationProc();
}


Vector3f GetAcceleration()
{
    return GetAccelerationProc();
}

// Gyroscope
Vector3f GetGyroRotationRate(int idx) { return GetGyroRotationRateProc(idx); }
bool IsGyroAvailable() { return IsGyroAvailableProc(); }
Vector3f GetGyroRotationRateUnbiased(int idx) { return GetGyroRotationRateUnbiasedProc(idx); }
Vector3f GetGravity(int idx) { return GetGravityProc(idx); }
Vector3f GetUserAcceleration(int idx) { return GetUserAccelerationProc(idx); }
Quaternionf GetAttitude(int idx) { return GetAttitudeProc(idx); }
bool IsGyroEnabled(int idx) { return IsGyroEnabledProc(idx); }
void SetGyroEnabled(int idx, bool enabled) { SetGyroEnabledProc(idx, enabled); }
float GetGyroUpdateInterval(int idx) { return GetGyroUpdateIntervalProc(idx); }
void SetGyroUpdateInterval(int idx, float interval) { SetGyroUpdateIntervalProc(idx, interval); }
int GetGyro() { return GetGyroProc(); }

float RemoteScreenWidth()
{
	return GetScreenWidthProc();
}

float RemoteScreenHeight()
{
	return GetScreenHeightProc();
}
