#ifndef INPUTINTERFACE_H
#define INPUTINTERFACE_H

#include "Runtime/Math/Vector2.h"
#include "Runtime/Input/GetInput.h"
#if UNITY_LINUX
#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#endif
#include <iostream>

#ifdef __OBJC__
	@class NSEvent;
	@class NSView;
	@class NSDraggingInfo;
#endif

/// An input event
struct InputEvent {
	InputEvent () { commandString = NULL; }
#if UNITY_OSX && defined(__OBJC__)
	InputEvent (NSEvent *src);
	InputEvent (NSEvent *src, NSView *view, bool cullOffscreenMouseDownEvents = true);
	InputEvent (id<NSDraggingInfo> draggingInfo, int inType, NSView* view);

	static InputEvent RepaintEvent (NSView *source);
#endif
#if UNITY_WIN
	InputEvent( UINT message, WPARAM wParam, LPARAM lParam, HWND window );
	#if UNITY_EDITOR
	InputEvent( int x, int y, DWORD keyFlags, int typ, HWND window );
	static InputEvent RepaintEvent (HWND window);
	static InputEvent CommandStringEvent (const std::string& editorCommand, bool execute, HWND window);
	static InputEvent SimulateKeyPressEvent(HWND window, int keyCode, int modifiers);
	#endif
#endif
#if UNITY_LINUX
	static InputEvent RepaintEvent (NativeWindow window);
	InputEvent ( int type, Vector2f location, int code, int state, unsigned long timeStamp );
	InputEvent ( int type, unsigned int key, unsigned int keycode, unsigned state, Vector2f location );
#elif UNITY_FLASH || UNITY_WEBGL
	InputEvent( int type );
	InputEvent(int eventType, int key, int code, int state);
#endif

	~InputEvent(  );

	InputEvent( const InputEvent& evt );
	void operator = (const InputEvent& evt);

	static InputEvent CommandStringEvent (const std::string& editorCommand, bool execute);

	void Debug () const;

	enum Modifiers {
		kShift = 1 << 0,
		kControl = 1 << 1,
		kAlt = 1 << 2,
		kCommand = 1 << 3,
		kNumeric = 1 << 4,
		kCapsLock = 1 << 5,
		kFunctionKey = 1 << 6
	};
	enum TypeEnum {
		kMouseDown = 0, kMouseUp = 1, kMouseMove = 2, kMouseDrag = 3, kKeyDown=4, kKeyUp=5,
		kScrollWheel=6, kRepaint=7, kLayout=8, kDragUpdated=9, kDragPerform=10,kDragExited=15, kIgnore=11,kUsed=12,kValidateCommand=13,kExecuteCommand=14,kContextClick=16,
		kMagnifyGesture=1000, kSwipeGesture=1001, kRotateGesture=1002
	};
	enum MouseButton {
		kLeftButton = 0, kRightButton = 1, kMiddleButton = 2
	};
#if UNITY_METRO
	enum TouchType {
		kMouseTouch = 0, kFingerTouch = 1
	};
#endif

#if UNITY_WINRT
	// Putted ctor here so I can use TypeEnum...
	InputEvent (TypeEnum inEventType, Vector2f mouseLocation, int modifiers);
	InputEvent (TypeEnum inEventType, UInt32 inKey, UInt32 inCharacter, UInt32 state, Vector2f mouseLocation);
#endif
	// needs to be size_t, as mono enums are 32-bit or 64-bit depending on architecture
	typedef size_t	Type;
	Type 			type;				///< Which type of event.
#if ENABLE_NEW_EVENT_SYSTEM
	Touch			touch;				///< Touch containing position and delta information
#else
	Vector2f 		mousePosition;		///< Position of mouse events.
	Vector2f 		delta;				///< Delta of mouse events.
	#if UNITY_METRO
	TouchType		touchType;
	#endif
#endif
	int 			button;				///< mouse button number. (bitfield of MouseButton enum)
	int 			modifiers;			///< keyboard modifier flags. (bitfield of Modifiers enum)
	float 			pressure;			///< Stylus pressure.
	int				clickCount;
	UInt16			character;			///< unicode keyboard character (with modifiers).
	UInt16			keycode;			///< The keyboard scancode of the event.
	char*			commandString;

	// Initialize to ignore event with all values cleared.
	void Init();

	void Use ()	 { type = kUsed; }

	#ifdef __OBJC__
	static InputEvent CommandStringEvent (const std::string& editorCommand, bool execute, NSView* view);
	#endif

private:
	#if UNITY_OSX && defined (__OBJC__)
	void GetImmediateMousePosition (NSEvent *event, NSView *view);
	void Init (NSEvent *src, NSView *view, bool cullOffscreenMouseDownEvents);
	#endif
	#if UNITY_WIN && UNITY_EDITOR
	void DoMouseJumpingTroughScreenEdges(HWND window, Vector2f mousePos, Vector2f& lastMousePos);
	#endif
};

/// Semi-Abstract superclass for all things input-related.
/// Override these functions and return true if you ate the event.
/// The default implementations just return false.
class InputInterface {
  public:
	virtual ~InputInterface ();

	virtual bool OnInputEvent (InputEvent &event) = 0;

	virtual bool PerformDrag (InputEvent &event);
	virtual int  UpdateDrag (InputEvent &event);
};

std::string InputEventToString (InputEvent& event);
void StringToInputEvent (const std::string& inputEvt, InputEvent& evt);

#if UNITY_OSX && GAMERELEASE
typedef struct _NPCocoaEvent NPCocoaEvent;

void InitWebplayerInputEvent (InputEvent& event, EventRecord* rec);
void InitEventFromEventRef ( InputEvent& event, EventRef eventRef );
void InitEventFromEventRecord( InputEvent& event, EventRecord* evt );
void InitWebplayerInputEventCocoa (InputEvent& event, NPCocoaEvent* cocoaEvent, bool textInputHandledByBrowser);
#endif

#endif
