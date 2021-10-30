#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "Runtime/BaseClasses/GameManager.h"
#include <map>
#include "Runtime/Input/InputAxis.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Configuration/UnityConfigure.h"
#include <iosfwd>

#if ENABLE_NEW_EVENT_SYSTEM
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Input/GetInput.h"
#endif

#include "Runtime/ClusterRenderer/ClusterRendererDefines.h"

enum {
	kMaxJoySticks = 12,
	kMaxJoyStickButtons = 20,
	kMaxJoyStickAxis = 20
};

enum {
	kKeyCount = 323,
	kMouseButtonCount = 7,
	kKeyAndMouseButtonCount = kKeyCount + kMouseButtonCount,
	kKeyAndJoyButtonCount = kKeyAndMouseButtonCount + kMaxJoySticks * kMaxJoyStickButtons
};

class InputManager : public GlobalGameManager
{
	public:
	
	InputManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~InputManager(); declared-by-macro
                	
	REGISTER_DERIVED_CLASS (InputManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (InputManager)
	// for cluster renderer
	DECLARE_CLUSTER_SERIALIZE (InputManager)
	
	static void InitializeClass ();
	static void CleanupClass ();

	/// Make up some sensible controls...
	void MakeDefault ();
	
	enum { kCompositionModeAuto, kCompositionModeOn, kCompositionModeOff };

	bool GetButton (const string &name);
	bool GetButtonDown (const string &name);
	bool GetButtonUp (const string &name);
	float GetAxis (const string &name);
	float GetAxisRaw (const string &name);
	bool HasAxisOrButton (const string& name);

	void ProcessInput();
	void SendInputEvents();
#if ENABLE_NEW_EVENT_SYSTEM
	void ProcessInput (const GameObject& go);

private:
	void ProcessMouse ();
	void ProcessTouches ();
	void ProcessTouch (GameObject* hover, bool isPressed, bool isReleased, bool isDragging, bool moveEvents);
#endif

public:
	void InputEndFrame ();
	
	// Is the button down or was down during this frame?
	bool GetMouseButton (int mouseBut) const { return GetKey (kKeyCount + mouseBut); }
	bool GetMouseButtonState (int mouseBut) const { return m_CurrentKeyState[kKeyCount + mouseBut]; }
	
	// Is the button down or was down during this frame?
	bool GetMouseButtonDown (int mouseBut) const { return GetKeyDown (kKeyCount + mouseBut); }
	bool GetMouseButtonUp (int mouseBut) const { return GetKeyUp (kKeyCount + mouseBut); }
	
	// Is the key down or was down during this frame?
	bool GetKey (int keyNum) const { return m_ThisFrameKeyDown[keyNum] | m_CurrentKeyState[keyNum]; }
	// Is the key down this frame but was up last frame?
	bool GetKeyDown (int keyNum) const { return m_ThisFrameKeyDown[keyNum]; }
	bool GetKeyUp (int keyNum) const { return m_ThisFrameKeyUp[keyNum]; }
	
#if ENABLE_NEW_EVENT_SYSTEM
	const Touch& GetMouse() { return m_Mouse[0]; }
	const Vector2f &GetMouseDelta () const { return m_Mouse[0].deltaPos; }
	const Vector2f &GetMouseScroll () const { return m_Mouse[0].deltaScroll; }
	const Vector2f &GetMousePosition () const { return m_Mouse[0].pos; }
	
	// Set prior to OnPress / OnRelease / OnDrag etc callbacks
	const Touch& GetCurrentTouch() const { return m_CurrentTouch != NULL ? *m_CurrentTouch : m_Mouse[0]; }
#else
	const Vector3f &GetMouseDelta () const { return m_MouseDelta; }
	const Vector2f &GetMousePosition () const { return m_MousePos; }
#endif
	
	float GetJoystickPosition (int joyNum, int axis) const;
	void SetJoystickPosition (int joyNum, int axis, float pos);
	
	void SetKeyState (int key, bool state);
#if ENABLE_NEW_EVENT_SYSTEM
	void SetMouseDelta (const Vector2f &delta) { m_Mouse[0].deltaPos = delta; }
	void SetMouseScroll (const Vector2f& delta) { m_Mouse[0].deltaScroll = delta; }
	void SetMousePosition (const Vector2f &pos) { m_Mouse[0].pos = pos; }
#else
	void SetMouseDelta (const Vector3f &pos) { m_MouseDelta = pos; }
	void SetMousePosition (const Vector2f &pos) { m_MousePos = pos; }
#endif
	void SetMouseButton (int button, bool enabled) { SetKeyState (kKeyCount + button, enabled); }

	void QuitApplication () { m_ShouldQuit = true; }
	void CancelQuitApplication () { m_ShouldQuit = false; }
	bool ShouldQuit () const { return m_ShouldQuit; }
		
	string& GetInputString () { return m_InputString; }
	string& GetCompositionString () { return m_CompositionString;}

	bool GetSimulateMouseWithTouches () const { return m_SimulateMouseWithTouches; }
	void SetSimulateMouseWithTouches (bool value) { m_SimulateMouseWithTouches = value; }
		
	void SetTextFieldInput(bool value) { m_TextFieldInput = value; }
	bool GetTextFieldInput();

	Vector2f &GetTextFieldCursorPos() { return m_TextFieldCursorPos; }
	void SetTextFieldCursorPos(Vector2f &value) { m_TextFieldCursorPos = value; }
	
	bool GetEatKeyPressOnTextFieldFocus ();
	void SetEatKeyPressOnTextFieldFocus (bool value) { m_EatKeyPressOnTextFieldFocus = value; }

	int GetIMECompositionMode () { return m_IMECompositionMode; } 
	void SetIMECompositionMode (int value) { m_IMECompositionMode = value; }

	bool GetIMEIsSelected() { return m_IMEIsSelected; }
	void SetIMEIsSelected(bool value) { m_IMEIsSelected = value; }

	bool GetEnableIMEComposition ();
	
	/// Needed here because m_Axes need to be preprocessed before use.
	virtual void CheckConsistency ();
	void ResetInputAxes ();

	bool GetAnyKey ();
	bool GetAnyKeyThisFrame ();
	
	virtual void Reset ();

	bool ConfigureButton(int *button);

	int NumAxes() {return m_Axes.size();}
	InputAxis *GetIndexedAxis(int i) {if(i<m_Axes.size())return &(m_Axes[i]); else return NULL;}
	
	#if SUPPORT_REPRODUCE_LOG
	void WriteLog (std::ofstream& out);
	void ReadLog (std::ifstream& in);
	#endif

	const std::vector< std::vector<float> >& GetJoystickPositions() const { return m_JoystickPos; }

  private:
  	std::vector<InputAxis> m_Axes;
	
	// There are shortclick keys because in one frame the player might hit a key and release it.
	// We want the key to be down for one frame in this case.
	dynamic_bitset m_CurrentKeyState;
	dynamic_bitset m_ThisFrameKeyDown;
	dynamic_bitset m_ThisFrameKeyUp;
	
#if ENABLE_NEW_EVENT_SYSTEM
	Touch m_Mouse[3];
	Touch* m_CurrentTouch;
	GameObject* m_Selection;
#else
	Vector3f m_MouseDelta; //x and y for delta, z for scrollwheel
	Vector2f m_MousePos;
#endif
	bool m_MousePresent;
	
	std::vector<std::vector<float> > m_JoystickPos;
	
	std::string m_InputString;
	std::string m_CompositionString;

	Vector2f m_TextFieldCursorPos;
	bool m_TextFieldInput;
	
	bool m_EatKeyPressOnTextFieldFocus;
	int m_IMECompositionMode; 
	bool m_IMEIsSelected;
	
	int m_LastJoyNum,m_LastJoyAxis;
	bool m_ShouldQuit;
	bool m_SimulateMouseWithTouches;

	friend bool GetConfigureJoyAxis(std::string name,int *joyNum,int *axis);
};

InputManager* GetInputManagerPtr ();
InputManager& GetInputManager ();

string KeyToString (int key);
int StringToKey (const string& name);

UInt16 NormalizeInputCharacter (UInt16 input);

enum {
	/* The keyboard syms have been cleverly chosen to map to ASCII */
	SDLK_UNKNOWN		= 0,
	SDLK_FIRST		= 0,
	SDLK_BACKSPACE		= 8,
	SDLK_TAB		= 9,
	SDLK_CLEAR		= 12,
	SDLK_RETURN		= 13,
	SDLK_PAUSE		= 19,
	SDLK_ESCAPE		= 27,
	SDLK_SPACE		= 32,
	SDLK_EXCLAIM		= 33,
	SDLK_QUOTEDBL		= 34,
	SDLK_HASH		= 35,
	SDLK_DOLLAR		= 36,
	SDLK_AMPERSAND		= 38,
	SDLK_QUOTE		= 39,
	SDLK_LEFTPAREN		= 40,
	SDLK_RIGHTPAREN		= 41,
	SDLK_ASTERISK		= 42,
	SDLK_PLUS		= 43,
	SDLK_COMMA		= 44,
	SDLK_MINUS		= 45,
	SDLK_PERIOD		= 46,
	SDLK_SLASH		= 47,
	SDLK_0			= 48,
	SDLK_1			= 49,
	SDLK_2			= 50,
	SDLK_3			= 51,
	SDLK_4			= 52,
	SDLK_5			= 53,
	SDLK_6			= 54,
	SDLK_7			= 55,
	SDLK_8			= 56,
	SDLK_9			= 57,
	SDLK_COLON		= 58,
	SDLK_SEMICOLON		= 59,
	SDLK_LESS		= 60,
	SDLK_EQUALS		= 61,
	SDLK_GREATER		= 62,
	SDLK_QUESTION		= 63,
	SDLK_AT			= 64,
	/* 
	   Skip uppercase letters
	 */
	SDLK_LEFTBRACKET	= 91,
	SDLK_BACKSLASH		= 92,
	SDLK_RIGHTBRACKET	= 93,
	SDLK_CARET		= 94,
	SDLK_UNDERSCORE		= 95,
	SDLK_BACKQUOTE		= 96,
	SDLK_a			= 97,
	SDLK_b			= 98,
	SDLK_c			= 99,
	SDLK_d			= 100,
	SDLK_e			= 101,
	SDLK_f			= 102,
	SDLK_g			= 103,
	SDLK_h			= 104,
	SDLK_i			= 105,
	SDLK_j			= 106,
	SDLK_k			= 107,
	SDLK_l			= 108,
	SDLK_m			= 109,
	SDLK_n			= 110,
	SDLK_o			= 111,
	SDLK_p			= 112,
	SDLK_q			= 113,
	SDLK_r			= 114,
	SDLK_s			= 115,
	SDLK_t			= 116,
	SDLK_u			= 117,
	SDLK_v			= 118,
	SDLK_w			= 119,
	SDLK_x			= 120,
	SDLK_y			= 121,
	SDLK_z			= 122,
	SDLK_DELETE		= 127,
	/* End of ASCII mapped keysyms */

	/* International keyboard syms */
	SDLK_WORLD_0		= 160,		/* 0xA0 */
	SDLK_WORLD_1		= 161,
	SDLK_WORLD_2		= 162,
	SDLK_WORLD_3		= 163,
	SDLK_WORLD_4		= 164,
	SDLK_WORLD_5		= 165,
	SDLK_WORLD_6		= 166,
	SDLK_WORLD_7		= 167,
	SDLK_WORLD_8		= 168,
	SDLK_WORLD_9		= 169,
	SDLK_WORLD_10		= 170,
	SDLK_WORLD_11		= 171,
	SDLK_WORLD_12		= 172,
	SDLK_WORLD_13		= 173,
	SDLK_WORLD_14		= 174,
	SDLK_WORLD_15		= 175,
	SDLK_WORLD_16		= 176,
	SDLK_WORLD_17		= 177,
	SDLK_WORLD_18		= 178,
	SDLK_WORLD_19		= 179,
	SDLK_WORLD_20		= 180,
	SDLK_WORLD_21		= 181,
	SDLK_WORLD_22		= 182,
	SDLK_WORLD_23		= 183,
	SDLK_WORLD_24		= 184,
	SDLK_WORLD_25		= 185,
	SDLK_WORLD_26		= 186,
	SDLK_WORLD_27		= 187,
	SDLK_WORLD_28		= 188,
	SDLK_WORLD_29		= 189,
	SDLK_WORLD_30		= 190,
	SDLK_WORLD_31		= 191,
	SDLK_WORLD_32		= 192,
	SDLK_WORLD_33		= 193,
	SDLK_WORLD_34		= 194,
	SDLK_WORLD_35		= 195,
	SDLK_WORLD_36		= 196,
	SDLK_WORLD_37		= 197,
	SDLK_WORLD_38		= 198,
	SDLK_WORLD_39		= 199,
	SDLK_WORLD_40		= 200,
	SDLK_WORLD_41		= 201,
	SDLK_WORLD_42		= 202,
	SDLK_WORLD_43		= 203,
	SDLK_WORLD_44		= 204,
	SDLK_WORLD_45		= 205,
	SDLK_WORLD_46		= 206,
	SDLK_WORLD_47		= 207,
	SDLK_WORLD_48		= 208,
	SDLK_WORLD_49		= 209,
	SDLK_WORLD_50		= 210,
	SDLK_WORLD_51		= 211,
	SDLK_WORLD_52		= 212,
	SDLK_WORLD_53		= 213,
	SDLK_WORLD_54		= 214,
	SDLK_WORLD_55		= 215,
	SDLK_WORLD_56		= 216,
	SDLK_WORLD_57		= 217,
	SDLK_WORLD_58		= 218,
	SDLK_WORLD_59		= 219,
	SDLK_WORLD_60		= 220,
	SDLK_WORLD_61		= 221,
	SDLK_WORLD_62		= 222,
	SDLK_WORLD_63		= 223,
	SDLK_WORLD_64		= 224,
	SDLK_WORLD_65		= 225,
	SDLK_WORLD_66		= 226,
	SDLK_WORLD_67		= 227,
	SDLK_WORLD_68		= 228,
	SDLK_WORLD_69		= 229,
	SDLK_WORLD_70		= 230,
	SDLK_WORLD_71		= 231,
	SDLK_WORLD_72		= 232,
	SDLK_WORLD_73		= 233,
	SDLK_WORLD_74		= 234,
	SDLK_WORLD_75		= 235,
	SDLK_WORLD_76		= 236,
	SDLK_WORLD_77		= 237,
	SDLK_WORLD_78		= 238,
	SDLK_WORLD_79		= 239,
	SDLK_WORLD_80		= 240,
	SDLK_WORLD_81		= 241,
	SDLK_WORLD_82		= 242,
	SDLK_WORLD_83		= 243,
	SDLK_WORLD_84		= 244,
	SDLK_WORLD_85		= 245,
	SDLK_WORLD_86		= 246,
	SDLK_WORLD_87		= 247,
	SDLK_WORLD_88		= 248,
	SDLK_WORLD_89		= 249,
	SDLK_WORLD_90		= 250,
	SDLK_WORLD_91		= 251,
	SDLK_WORLD_92		= 252,
	SDLK_WORLD_93		= 253,
	SDLK_WORLD_94		= 254,
	SDLK_WORLD_95		= 255,		/* 0xFF */

	/* Numeric keypad */
	SDLK_KP0		= 256,
	SDLK_KP1		= 257,
	SDLK_KP2		= 258,
	SDLK_KP3		= 259,
	SDLK_KP4		= 260,
	SDLK_KP5		= 261,
	SDLK_KP6		= 262,
	SDLK_KP7		= 263,
	SDLK_KP8		= 264,
	SDLK_KP9		= 265,
	SDLK_KP_PERIOD		= 266,
	SDLK_KP_DIVIDE		= 267,
	SDLK_KP_MULTIPLY	= 268,
	SDLK_KP_MINUS		= 269,
	SDLK_KP_PLUS		= 270,
	SDLK_KP_ENTER		= 271,
	SDLK_KP_EQUALS		= 272,

	/* Arrows + Home/End pad */
	SDLK_UP			= 273,
	SDLK_DOWN		= 274,
	SDLK_RIGHT		= 275,
	SDLK_LEFT		= 276,
	SDLK_INSERT		= 277,
	SDLK_HOME		= 278,
	SDLK_END		= 279,
	SDLK_PAGEUP		= 280,
	SDLK_PAGEDOWN		= 281,

	/* Function keys */
	SDLK_F1			= 282,
	SDLK_F2			= 283,
	SDLK_F3			= 284,
	SDLK_F4			= 285,
	SDLK_F5			= 286,
	SDLK_F6			= 287,
	SDLK_F7			= 288,
	SDLK_F8			= 289,
	SDLK_F9			= 290,
	SDLK_F10		= 291,
	SDLK_F11		= 292,
	SDLK_F12		= 293,
	SDLK_F13		= 294,
	SDLK_F14		= 295,
	SDLK_F15		= 296,

	/* Key state modifier keys */
	SDLK_NUMLOCK		= 300,
	SDLK_CAPSLOCK		= 301,
	SDLK_SCROLLOCK		= 302,
	SDLK_RSHIFT		= 303,
	SDLK_LSHIFT		= 304,
	SDLK_RCTRL		= 305,
	SDLK_LCTRL		= 306,
	SDLK_RALT		= 307,
	SDLK_LALT		= 308,
	SDLK_RMETA		= 309,
	SDLK_LMETA		= 310,
	SDLK_RGUI		= 309,
	SDLK_LGUI		= 310,
	SDLK_LSUPER		= 311,		/* Left "Windows" key */
	SDLK_RSUPER		= 312,		/* Right "Windows" key */
	SDLK_MODE		= 313,		/* "Alt Gr" key */
	SDLK_COMPOSE		= 314,		/* Multi-key compose key */

	/* Miscellaneous function keys */
	SDLK_HELP		= 315,
	SDLK_PRINT		= 316,
	SDLK_SYSREQ		= 317,
	SDLK_BREAK		= 318,
	SDLK_MENU		= 319,
	SDLK_POWER		= 320,		/* Power Macintosh power key */
	SDLK_EURO		= 321,		/* Some european keyboards */
	SDLK_UNDO		= 322,		/* Atari keyboard has Undo */

	/* Add any other keys here */

	SDLK_LAST
};


#endif
