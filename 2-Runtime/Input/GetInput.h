#ifndef GETINPUT_H
#define GETINPUT_H

#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Color.h"
#include "PlatformDependent/iPhonePlayer/APN.h"
#include <string>

/* Mac os x input handling

--- Joystick input:
* Uses HID

--- Keyboard input:
* Uses HID for keyboard events
* Uses normal event passing for Input.inputString. The applications are responsible for passing the events on to GetInput.cpp

--- Mouse input

HID gives the nicest mouse delta, so we always try to use it.
BUT Some mice don't have mouse delta from HID and USB overdrive snatches it away.

So we must provide fallbacks. At startup hid mouse delta is disabled, as soon any hid mouse delta
is reported, we switch to using only hid mouse delta.

The fallback look different for all editor/player/webplayer

Some facts:
- In a cocoa application, Carbon events work only in fullscreen mode. In window mode they get snatched.
	(In Safari carbon events fail, in Firefox they work)
- In editor/player we can access all cocoa events directly. In the web player we can't.
- Cocoa/Carbon mouse delta is non-accelerated and supports screen boundary deltas
- getmouse based is accelerated and doesnt support screen boundaries

Which leads to the following workarounds:

Editor
-> NSApp sendEvents forwards all mouse moved NSEvents to InputProcessMouseMove
-> Mouse down is generated from GameView

Standalone
-> NSApp sendEvents forwards all mouse moved NSEvents to InputProcessMouseMove
-> Mouse down forwarded from NSApp

Web plugin windowed
-> getmouse based mouse delta (Fails at screen borders)
-> mouse down comes from the plugin events (No right or middle mouse buttons)

Web plugin fullscreen
-> carbon event based mouse delta
-> carbon event based mouse down
*/

void GetJoystickNames (std::vector<std::string> &names);
std::string GetJoystickAxisName(int joyNum,int axis);
std::string GetNiceKeyname(int key);

void InputShutdown ();

void InputReadMousePosition ();
void InputReadMouseState();
void InputReadKeyboardState();
void InputReadJoysticks();

void ClearInputEvents ();

// Clears all input axes, keydowns, mouse buttons and sets up how event handlers
void ResetInput ();
void ResetInputAfterPause ();

namespace Unity
{
	class GameObject;
};

// touchscreen
struct Touch
{
#if ENABLE_NEW_EVENT_SYSTEM
	SInt32 id;		// -1 = LMB, -2 = RMB, -3 = MMB, 0+ = actual touch IDs.
	Vector2f pos;
	Vector2f rawPos;
	Vector2f oldPos;
	Vector2f deltaPos;
	Vector2f deltaScroll;
	Vector3f worldPos;
	float deltaTime; // time between two events that came to us during this
	                 // touch/drag gesture. 1.0f = 1 second.
	UInt32 tapCount;
	UInt32 phase;

	Unity::GameObject* hover;	// Object the mouse is hovering over
	Unity::GameObject* press;	// Object that was pressed on via mouse or touch

	//bool eligibleForClick;

	enum TouchPhase
	{
		kTouchBegan = 0,
		kTouchMoved = 1,
		kTouchStationary = 2,
		kTouchEnded = 3,
		kTouchCanceled = 4
	};

	Touch () : hover(NULL), press(NULL) {}
#else
	UInt32 id;
	Vector2f pos;
	Vector2f rawPos;
	Vector2f deltaPos;
	float deltaTime;	// time between two events that came to us during this
						// touch/drag gesture. 1.0f = 1 second.
	UInt32 tapCount;
	UInt32 phase;
#endif
};

struct Acceleration
{
	Vector3f acc;
	float deltaTime;
};

Vector3f GetAcceleration ();
size_t GetAccelerationCount ();
void GetAcceleration (size_t index, Acceleration& acceleration);
size_t GetTouchCount ();
#if ENABLE_NEW_EVENT_SYSTEM
Touch* GetTouch (unsigned index);
#else
bool GetTouch (unsigned index, Touch& touch);
#endif
bool IsMultiTouchEnabled();
void SetMultiTouchEnabled(bool flag = true);

// Must match DeviceOrientation in UnityEngineInput.txt
enum DeviceOrientation
{
	// The orientation of the device cannot be determined.
	DEVICE_ORIENTATION_UNKNOWN = 0,
	// The device is in portrait mode, with the device held upright and the home button at the bottom.
	DEVICE_ORIENTATION_PORTRAIT = 1,
	// The device is in portrait mode but upside down, with the device held upright and the home button at the top.
	DEVICE_ORIENTATION_PORTRAITUPSIDEDOWN = 2,
	// The device is in landscape mode, with the device held upright and the home button on the right side.
	DEVICE_ORIENTATION_LANDSCAPELEFT = 3,
	// The device is in landscape mode, with the device held upright and the home button on the left side.
	DEVICE_ORIENTATION_LANDSCAPERIGHT = 4,
	// The device is held parallel to the ground with the screen facing upwards.
	DEVICE_ORIENTATION_FACEUP = 5,
	// The device is held parallel to the ground with the screen facing downwards.
	DEVICE_ORIENTATION_FACEDOWN = 6
};
unsigned int GetOrientation ();   // device orientation

void Vibrate ();
bool IsApplicationGenuine ();
bool IsApplicationGenuineAvailable ();
void PlayMovie (std::string const& path, ColorRGBA32 const& backgroundColor,
                UInt32 controlMode, UInt32 scalingMode, bool pathIsUrl);

Vector3f GetAcceleration (int controllerID);
Quaternionf GetRotation(int controllerID);
Vector3f GetPosition(int controllerID);
bool IsCompensatingSensors();
void SetCompensatingSensors(bool val);
Vector3f GetGyroRotationRate(int idx);
bool IsGyroAvailable();
Vector3f GetGyroRotationRateUnbiased(int idx);
Vector3f GetGravity(int idx);
Vector3f GetUserAcceleration(int idx);
Quaternionf GetAttitude(int idx);
bool IsGyroEnabled(int idx);
void SetGyroEnabled(int idx, bool enabled);
float GetGyroUpdateInterval(int idx);
void SetGyroUpdateInterval(int idx, float interval);
int GetGyro();
size_t GetLocalNotificationCount();
iPhoneLocalNotification* CopyLocalNotification(unsigned index);
size_t GetRemoteNotificationCount();
iPhoneRemoteNotification* CopyRemoteNotification(unsigned index);
void ClearLocalNotifications();
void ClearRemoteNotifications();

#if UNITY_OSX
	//scaling value for mouse deltas
	#define kHIDMouseDeltaScale 0.5
	#define kCocoaMouseDeltaScale 0.25

	struct InputEvent;

	void InputInit();
	void CleanupMouseInputHandlers();
	int MapCocoaScancodeToSDL (int inKey);
	void InputProcessMacOSEvent(EventRecord *evt);
	void InputProcessMouseMove (float x, float y);
	Vector2f MacOSGlobalPointToLocal (Point p);
	void ResetUnicodeInput();
	int ConvertCocoaModifierFlags (UInt32 modFlags);
	void SanitizeKeyEvent( InputEvent& event);
	void ProcessCocoaKeyInput(InputEvent &ie, CFStringRef characters, UInt32 keyCode);
	void GetKeyStateFromFlagsChangedEvent (UInt32 mod);
	void InputGetKeyboardIMEMode();

	#if WEBPLUG
	typedef struct _NPCocoaEvent NPCocoaEvent;

	void InputProcessMacOSEventHIDWorkaround(EventRecord *evt);
	void InputProcessCocoaEvent(NPCocoaEvent *event, bool letBrowserHandleTextComposition);
	#endif
#elif UNITY_IPHONE || UNITY_ANDROID
	void InputInit();
	// Process any input - call once per main loop
	void InputProcess();
#elif UNITY_BB10
	void InputInit();
	// Process any input - call once per main loop
	void InputProcess();
	void AddKeyEvent(int key, int flag);
	void HandleEvents();
	int GetFrameCount();
#elif UNITY_TIZEN
	void InputInit();
	// Process any input - call once per main loop
	void InputProcess();
#elif UNITY_LINUX
	#define kX11MouseDeltaScale 0.25
	void InputInit();
	// Process any input - call once per main loop
	void InputProcess();
	void processKey( void* event, bool down );
	void processMouseButton( void* event, bool down );
	void processMouseMovedEvent( void* event );
	void processMouseWarp( void* event );
#elif UNITY_WII
	#include <revolution/kpad.h>

	void InputInit();
	// Process any input - call once per main loop
	void InputProcess();

#elif UNITY_WIN

	// Initialize input with application's main window
	void InputInit( HWND window );

	void InputInitWindow (HWND window);
	void InputShutdownWindow ();

	// Process any input - call once per main loop
	void InputProcess ();

	// Activate input - call when application goes to foreground
	void InputActivate();
	// Passivate input - call when application goes to background
	void InputPassivate();
	// Window/fullscreen has changed
	void InputSetWindow(HWND window, bool fullscreen);

	LRESULT ProcessInputMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, BOOL &handled);

	int InputKeycodeFromVKEY( int vkey );
	bool GetMousePresent();
	void SetMousePresent(const bool mousePresent);

	#if UNITY_EDITOR
	void SetEditorMouseOffset( int x, int y, HWND window );
	void GetEditorMouseOffset(int *x, int *y, HWND *window = NULL);
	#endif
#elif UNITY_XENON

	void InputInit();
	void InputProcess ();

#elif UNITY_PS3
	int InputInit();
	int InputProcess ();
#elif UNITY_PEPPER
	void InputInit ();
#elif UNITY_FLASH || UNITY_WEBGL
#else
#error "Unknown platform"
#endif

#endif
