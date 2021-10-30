#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "InputManager.h"
#include "LocationService.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/BitSetSerialization.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Math/Quaternion.h"
#include "PlatformDependent/iPhonePlayer/APN.h"
#include "Runtime/Misc/BuildSettings.h"

#if ENABLE_NEW_EVENT_SYSTEM
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Dynamics/Rigidbody.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#endif

#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/GUIWindows.h"

#if SUPPORT_REPRODUCE_LOG
#include <fstream>
#include "Runtime/Misc/ReproductionLog.h"
#endif

IMPLEMENT_CLASS_HAS_INIT (InputManager)
IMPLEMENT_OBJECT_SERIALIZE (InputManager)
IMPLEMENT_CLUSTER_SERIALIZE (InputManager)
GET_MANAGER (InputManager)
GET_MANAGER_PTR (InputManager)

using namespace std;


InputManager::InputManager (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	MakeDefault ();
	m_CurrentKeyState.resize (kKeyAndJoyButtonCount, false);
	m_ThisFrameKeyDown.resize (kKeyAndJoyButtonCount, false);
	m_ThisFrameKeyUp.resize (kKeyAndJoyButtonCount, false);

#if ENABLE_NEW_EVENT_SYSTEM
	m_CurrentTouch = NULL;
	m_Selection = NULL;
#else
	m_MousePos = Vector2f (0.0F,0.0F);
	m_MouseDelta = Vector3f (0.0F,0.0F,0.0F);
#endif
	m_MousePresent = true;
	m_ShouldQuit = false;
	m_TextFieldInput = false;
	m_IMEIsSelected = false;
	m_SimulateMouseWithTouches = true;

	for(int i=0;i<kMaxJoySticks;i++)
	{
		std::vector<float> axes;
		for(int j=0;j<kMaxJoyStickAxis;j++)
			axes.push_back(0.0);
		m_JoystickPos.push_back(axes);
	}

	m_EatKeyPressOnTextFieldFocus = true;
	m_IMECompositionMode = kCompositionModeAuto;
}

InputManager::~InputManager () {
}

void InputManager::Reset ()
{
	Super::Reset();

	m_Axes.clear ();
	m_CompositionString.clear ();
	MakeDefault ();
}

void InputManager::MakeDefault () {
	m_Axes.push_back (InputAxis ("Horizontal"));
	m_Axes.push_back (InputAxis ("Vertical"));
	m_Axes[0].MakeAnalogKey (StringToKey ("right"), StringToKey ("left"), StringToKey ("d"), StringToKey ("a"));
	m_Axes[1].MakeAnalogKey (StringToKey ("up"), StringToKey ("down"), StringToKey ("w"), StringToKey ("s"));

	m_Axes.push_back (InputAxis ("Fire1"));
	m_Axes.push_back (InputAxis ("Fire2"));
	m_Axes.push_back (InputAxis ("Fire3"));
	m_Axes.push_back (InputAxis ("Jump"));
	m_Axes[2].MakeButton (StringToKey ("left ctrl"), StringToKey ("mouse 0"));
	m_Axes[3].MakeButton (StringToKey ("left alt"), StringToKey ("mouse 1"));
	m_Axes[4].MakeButton (StringToKey ("left cmd"), StringToKey ("mouse 2"));
	m_Axes[5].MakeButton (StringToKey ("space"), 0);

	m_Axes.push_back (InputAxis ("Mouse X"));
	m_Axes.push_back (InputAxis ("Mouse Y"));
	m_Axes.push_back (InputAxis ("Mouse ScrollWheel"));
	m_Axes[6].MakeMouse (0);
	m_Axes[7].MakeMouse (1);
	m_Axes[8].MakeMouse (2);

	m_Axes.push_back (InputAxis ("Horizontal"));
	m_Axes.push_back (InputAxis ("Vertical"));
	m_Axes[9].MakeJoystick (0);
	m_Axes[10].MakeJoystick (1);
	m_Axes[10].SetInvert(true);

	m_Axes.push_back (InputAxis ("Fire1"));
	m_Axes.push_back (InputAxis ("Fire2"));
	m_Axes.push_back (InputAxis ("Fire3"));
	m_Axes.push_back (InputAxis ("Jump"));
	m_Axes[11].MakeButton (StringToKey ("joystick button 0"), 0);
	m_Axes[12].MakeButton (StringToKey ("joystick button 1"), 0);
	m_Axes[13].MakeButton (StringToKey ("joystick button 2"), 0);
	m_Axes[14].MakeButton (StringToKey ("joystick button 3"), 0);
}

bool InputManager::GetButton (const string &name) {
	bool finalButton = false;
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++) {
		if (i->GetName() == name)
		{
			finalButton |= GetKey(i->GetPosKey ());
			finalButton |= GetKey(i->GetNegKey ());
			finalButton |= GetKey(i->GetAltPosKey ());
			finalButton |= GetKey(i->GetAltNegKey ());
		}
	}
	return finalButton;
}

bool InputManager::GetButtonDown (const string &name)
{
	bool finalButton = false;
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++)
	{
		if (i->GetName() == name && i->GetType () == kAxisButton)
		{
			finalButton |= m_ThisFrameKeyDown[i->GetPosKey ()];
			finalButton |= m_ThisFrameKeyDown[i->GetNegKey ()];
			finalButton |= m_ThisFrameKeyDown[i->GetAltPosKey ()];
			finalButton |= m_ThisFrameKeyDown[i->GetAltNegKey ()];
		}
	}
	return finalButton;
}

bool InputManager::GetButtonUp (const string &name)
{
	bool finalButton = false;
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++)
	{
		if (i->GetName() == name && i->GetType () == kAxisButton)
		{
			finalButton |= m_ThisFrameKeyUp[i->GetPosKey ()];
			finalButton |= m_ThisFrameKeyUp[i->GetNegKey ()];
			finalButton |= m_ThisFrameKeyUp[i->GetAltPosKey ()];
			finalButton |= m_ThisFrameKeyUp[i->GetAltNegKey ()];
		}
	}
	return finalButton;
}


float InputManager::GetAxis (const string &name)
{
	float finalValue = 0.0F;
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++) {
		if (i->GetName() == name && Abs (i->GetValue ()) > Abs (finalValue))
			finalValue = i->GetValue ();
	}
	return finalValue;
}

float InputManager::GetAxisRaw (const string &name) {
	float finalValue = 0.0F;
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++) {
		if (i->GetName() == name && Abs (i->GetValueRaw ()) > Abs (finalValue))
			finalValue = i->GetValueRaw ();
	}
	return finalValue;
}


bool InputManager::HasAxisOrButton (const string& name)
{
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end();i++) {
		if (i->GetName() == name)
			return true;
	}
	return false;
}

// Might consider doing this inside InputAxis somehow...
void InputManager::CheckConsistency () {
	Super::CheckConsistency ();
	ResetInputAxes ();
}

void InputManager::ResetInputAxes ()
{
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end(); i++)
		i->Reset();

	m_CurrentKeyState.reset ();
	m_ThisFrameKeyDown.reset ();
	m_ThisFrameKeyUp.reset();

	for (int i=0;i<m_JoystickPos.size ();i++)
	{
		for (int j=0;j<m_JoystickPos[i].size ();j++)
			m_JoystickPos[i][j] = 0.0F;
	}

#if ENABLE_NEW_EVENT_SYSTEM
	for (int i = 0; i < 3; ++i)
	{
		m_Mouse[i].deltaPos = Vector2f::zero;
		m_Mouse[i].deltaScroll = Vector2f::zero;
	}
#else
	m_MouseDelta = Vector3f (0,0,0);
#endif
}

bool InputManager::GetAnyKey ()
{
	#if SUPPORT_REPRODUCE_LOG
	if(GetKeyDown(286) || GetKeyDown(287))
		return false;
	#endif
	return m_CurrentKeyState.any () || m_ThisFrameKeyDown.any ();
}

bool InputManager::GetAnyKeyThisFrame ()
{
	#if SUPPORT_REPRODUCE_LOG
	if(GetKeyDown(286) || GetKeyDown(287))
		return false;
	#endif
	return m_ThisFrameKeyDown.any ();
}

template<class TransferFunc>
void InputManager::Transfer (TransferFunc& transfer) {
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_Axes);
}

#if ENABLE_CLUSTER_SYNC
template<class TransferFunc>
void InputManager::ClusterTransfer (TransferFunc& transfer) {
	
	TRANSFER(m_Axes);
	
	TRANSFER(m_CurrentKeyState);
	TRANSFER(m_ThisFrameKeyDown);
	TRANSFER(m_ThisFrameKeyUp);
	
	TRANSFER(m_MouseDelta.x);
	TRANSFER(m_MouseDelta.y);
	TRANSFER(m_MouseDelta.z);
	
	TRANSFER(m_MousePos.x);
	TRANSFER(m_MousePos.y);
}
#endif



float InputManager::GetJoystickPosition (int joyNum, int axis) const
{
	if (joyNum < m_JoystickPos.size () && axis < m_JoystickPos[joyNum].size ())
		return m_JoystickPos[joyNum][axis];
	else
		return 0;
}

void InputManager::SetJoystickPosition (int joyNum, int axis, float pos)
{
	if (joyNum < m_JoystickPos.size () && axis < m_JoystickPos[joyNum].size ())
	{
		m_JoystickPos[joyNum][axis]=pos;
	}
}


typedef std::map<int, std::string> KeyToName;
typedef std::map<std::string, int> NameToKey;
static KeyToName* g_KeyToName = NULL;
static NameToKey* g_NameToKey = NULL;

void SetupKeyNameMapping ();
void InputManager::InitializeClass()
{
	g_KeyToName = UNITY_NEW(KeyToName, kMemResource);
	g_NameToKey = UNITY_NEW(NameToKey, kMemResource);
	SetupKeyNameMapping();
}

void InputManager::CleanupClass()
{
	UNITY_DELETE(g_KeyToName, kMemResource);
	UNITY_DELETE(g_NameToKey, kMemResource);
}

void SetupKeyNameMapping ()
{
	static bool isInitialized = false;
	if (isInitialized)
		return;
	isInitialized = true;

	SET_ALLOC_OWNER(NULL);

	(*g_KeyToName)[(int)SDLK_BACKSPACE] = "backspace";
	(*g_KeyToName)[(int)SDLK_TAB] = "tab";
	(*g_KeyToName)[(int)SDLK_CLEAR] = "clear";
	(*g_KeyToName)[(int)SDLK_RETURN] = "return";
	(*g_KeyToName)[(int)SDLK_PAUSE] = "pause";
	(*g_KeyToName)[(int)SDLK_ESCAPE] = "escape";
	(*g_KeyToName)[(int)SDLK_SPACE] = "space";
	(*g_KeyToName)[(int)SDLK_EXCLAIM]  = "!";
	(*g_KeyToName)[(int)SDLK_QUOTEDBL]  = "\"";
	(*g_KeyToName)[(int)SDLK_HASH]  = "#";
	(*g_KeyToName)[(int)SDLK_DOLLAR]  = "$";
	(*g_KeyToName)[(int)SDLK_AMPERSAND]  = "&";
	(*g_KeyToName)[(int)SDLK_QUOTE] = "'";
	(*g_KeyToName)[(int)SDLK_LEFTPAREN] = "(";
	(*g_KeyToName)[(int)SDLK_RIGHTPAREN] = ")";
	(*g_KeyToName)[(int)SDLK_ASTERISK] = "*";
	(*g_KeyToName)[(int)SDLK_PLUS] = "+";
	(*g_KeyToName)[(int)SDLK_COMMA] = ",";
	(*g_KeyToName)[(int)SDLK_MINUS] = "-";
	(*g_KeyToName)[(int)SDLK_PERIOD] = ".";
	(*g_KeyToName)[(int)SDLK_SLASH] = "/";
	(*g_KeyToName)[(int)SDLK_0] = "0";
	(*g_KeyToName)[(int)SDLK_1] = "1";
	(*g_KeyToName)[(int)SDLK_2] = "2";
	(*g_KeyToName)[(int)SDLK_3] = "3";
	(*g_KeyToName)[(int)SDLK_4] = "4";
	(*g_KeyToName)[(int)SDLK_5] = "5";
	(*g_KeyToName)[(int)SDLK_6] = "6";
	(*g_KeyToName)[(int)SDLK_7] = "7";
	(*g_KeyToName)[(int)SDLK_8] = "8";
	(*g_KeyToName)[(int)SDLK_9] = "9";
	(*g_KeyToName)[(int)SDLK_COLON] = ":";
	(*g_KeyToName)[(int)SDLK_SEMICOLON] = ";";
	(*g_KeyToName)[(int)SDLK_LESS] = "<";
	(*g_KeyToName)[(int)SDLK_EQUALS] = "=";
	(*g_KeyToName)[(int)SDLK_GREATER] = ">";
	(*g_KeyToName)[(int)SDLK_QUESTION] = "?";
	(*g_KeyToName)[(int)SDLK_AT] = "@";
	(*g_KeyToName)[(int)SDLK_LEFTBRACKET] = "[";
	(*g_KeyToName)[(int)SDLK_BACKSLASH] = "\\";
	(*g_KeyToName)[(int)SDLK_RIGHTBRACKET] = "]";
	(*g_KeyToName)[(int)SDLK_CARET] = "^";
	(*g_KeyToName)[(int)SDLK_UNDERSCORE] = "_";
	(*g_KeyToName)[(int)SDLK_BACKQUOTE] = "`";
	(*g_KeyToName)[(int)SDLK_a] = "a";
	(*g_KeyToName)[(int)SDLK_b] = "b";
	(*g_KeyToName)[(int)SDLK_c] = "c";
	(*g_KeyToName)[(int)SDLK_d] = "d";
	(*g_KeyToName)[(int)SDLK_e] = "e";
	(*g_KeyToName)[(int)SDLK_f] = "f";
	(*g_KeyToName)[(int)SDLK_g] = "g";
	(*g_KeyToName)[(int)SDLK_h] = "h";
	(*g_KeyToName)[(int)SDLK_i] = "i";
	(*g_KeyToName)[(int)SDLK_j] = "j";
	(*g_KeyToName)[(int)SDLK_k] = "k";
	(*g_KeyToName)[(int)SDLK_l] = "l";
	(*g_KeyToName)[(int)SDLK_m] = "m";
	(*g_KeyToName)[(int)SDLK_n] = "n";
	(*g_KeyToName)[(int)SDLK_o] = "o";
	(*g_KeyToName)[(int)SDLK_p] = "p";
	(*g_KeyToName)[(int)SDLK_q] = "q";
	(*g_KeyToName)[(int)SDLK_r] = "r";
	(*g_KeyToName)[(int)SDLK_s] = "s";
	(*g_KeyToName)[(int)SDLK_t] = "t";
	(*g_KeyToName)[(int)SDLK_u] = "u";
	(*g_KeyToName)[(int)SDLK_v] = "v";
	(*g_KeyToName)[(int)SDLK_w] = "w";
	(*g_KeyToName)[(int)SDLK_x] = "x";
	(*g_KeyToName)[(int)SDLK_y] = "y";
	(*g_KeyToName)[(int)SDLK_z] = "z";
	(*g_KeyToName)[(int)SDLK_DELETE] = "delete";

	(*g_KeyToName)[(int)SDLK_WORLD_0] = "world 0";
	(*g_KeyToName)[(int)SDLK_WORLD_1] = "world 1";
	(*g_KeyToName)[(int)SDLK_WORLD_2] = "world 2";
	(*g_KeyToName)[(int)SDLK_WORLD_3] = "world 3";
	(*g_KeyToName)[(int)SDLK_WORLD_4] = "world 4";
	(*g_KeyToName)[(int)SDLK_WORLD_5] = "world 5";
	(*g_KeyToName)[(int)SDLK_WORLD_6] = "world 6";
	(*g_KeyToName)[(int)SDLK_WORLD_7] = "world 7";
	(*g_KeyToName)[(int)SDLK_WORLD_8] = "world 8";
	(*g_KeyToName)[(int)SDLK_WORLD_9] = "world 9";
	(*g_KeyToName)[(int)SDLK_WORLD_10] = "world 10";
	(*g_KeyToName)[(int)SDLK_WORLD_11] = "world 11";
	(*g_KeyToName)[(int)SDLK_WORLD_12] = "world 12";
	(*g_KeyToName)[(int)SDLK_WORLD_13] = "world 13";
	(*g_KeyToName)[(int)SDLK_WORLD_14] = "world 14";
	(*g_KeyToName)[(int)SDLK_WORLD_15] = "world 15";
	(*g_KeyToName)[(int)SDLK_WORLD_16] = "world 16";
	(*g_KeyToName)[(int)SDLK_WORLD_17] = "world 17";
	(*g_KeyToName)[(int)SDLK_WORLD_18] = "world 18";
	(*g_KeyToName)[(int)SDLK_WORLD_19] = "world 19";
	(*g_KeyToName)[(int)SDLK_WORLD_20] = "world 20";
	(*g_KeyToName)[(int)SDLK_WORLD_21] = "world 21";
	(*g_KeyToName)[(int)SDLK_WORLD_22] = "world 22";
	(*g_KeyToName)[(int)SDLK_WORLD_23] = "world 23";
	(*g_KeyToName)[(int)SDLK_WORLD_24] = "world 24";
	(*g_KeyToName)[(int)SDLK_WORLD_25] = "world 25";
	(*g_KeyToName)[(int)SDLK_WORLD_26] = "world 26";
	(*g_KeyToName)[(int)SDLK_WORLD_27] = "world 27";
	(*g_KeyToName)[(int)SDLK_WORLD_28] = "world 28";
	(*g_KeyToName)[(int)SDLK_WORLD_29] = "world 29";
	(*g_KeyToName)[(int)SDLK_WORLD_30] = "world 30";
	(*g_KeyToName)[(int)SDLK_WORLD_31] = "world 31";
	(*g_KeyToName)[(int)SDLK_WORLD_32] = "world 32";
	(*g_KeyToName)[(int)SDLK_WORLD_33] = "world 33";
	(*g_KeyToName)[(int)SDLK_WORLD_34] = "world 34";
	(*g_KeyToName)[(int)SDLK_WORLD_35] = "world 35";
	(*g_KeyToName)[(int)SDLK_WORLD_36] = "world 36";
	(*g_KeyToName)[(int)SDLK_WORLD_37] = "world 37";
	(*g_KeyToName)[(int)SDLK_WORLD_38] = "world 38";
	(*g_KeyToName)[(int)SDLK_WORLD_39] = "world 39";
	(*g_KeyToName)[(int)SDLK_WORLD_40] = "world 40";
	(*g_KeyToName)[(int)SDLK_WORLD_41] = "world 41";
	(*g_KeyToName)[(int)SDLK_WORLD_42] = "world 42";
	(*g_KeyToName)[(int)SDLK_WORLD_43] = "world 43";
	(*g_KeyToName)[(int)SDLK_WORLD_44] = "world 44";
	(*g_KeyToName)[(int)SDLK_WORLD_45] = "world 45";
	(*g_KeyToName)[(int)SDLK_WORLD_46] = "world 46";
	(*g_KeyToName)[(int)SDLK_WORLD_47] = "world 47";
	(*g_KeyToName)[(int)SDLK_WORLD_48] = "world 48";
	(*g_KeyToName)[(int)SDLK_WORLD_49] = "world 49";
	(*g_KeyToName)[(int)SDLK_WORLD_50] = "world 50";
	(*g_KeyToName)[(int)SDLK_WORLD_51] = "world 51";
	(*g_KeyToName)[(int)SDLK_WORLD_52] = "world 52";
	(*g_KeyToName)[(int)SDLK_WORLD_53] = "world 53";
	(*g_KeyToName)[(int)SDLK_WORLD_54] = "world 54";
	(*g_KeyToName)[(int)SDLK_WORLD_55] = "world 55";
	(*g_KeyToName)[(int)SDLK_WORLD_56] = "world 56";
	(*g_KeyToName)[(int)SDLK_WORLD_57] = "world 57";
	(*g_KeyToName)[(int)SDLK_WORLD_58] = "world 58";
	(*g_KeyToName)[(int)SDLK_WORLD_59] = "world 59";
	(*g_KeyToName)[(int)SDLK_WORLD_60] = "world 60";
	(*g_KeyToName)[(int)SDLK_WORLD_61] = "world 61";
	(*g_KeyToName)[(int)SDLK_WORLD_62] = "world 62";
	(*g_KeyToName)[(int)SDLK_WORLD_63] = "world 63";
	(*g_KeyToName)[(int)SDLK_WORLD_64] = "world 64";
	(*g_KeyToName)[(int)SDLK_WORLD_65] = "world 65";
	(*g_KeyToName)[(int)SDLK_WORLD_66] = "world 66";
	(*g_KeyToName)[(int)SDLK_WORLD_67] = "world 67";
	(*g_KeyToName)[(int)SDLK_WORLD_68] = "world 68";
	(*g_KeyToName)[(int)SDLK_WORLD_69] = "world 69";
	(*g_KeyToName)[(int)SDLK_WORLD_70] = "world 70";
	(*g_KeyToName)[(int)SDLK_WORLD_71] = "world 71";
	(*g_KeyToName)[(int)SDLK_WORLD_72] = "world 72";
	(*g_KeyToName)[(int)SDLK_WORLD_73] = "world 73";
	(*g_KeyToName)[(int)SDLK_WORLD_74] = "world 74";
	(*g_KeyToName)[(int)SDLK_WORLD_75] = "world 75";
	(*g_KeyToName)[(int)SDLK_WORLD_76] = "world 76";
	(*g_KeyToName)[(int)SDLK_WORLD_77] = "world 77";
	(*g_KeyToName)[(int)SDLK_WORLD_78] = "world 78";
	(*g_KeyToName)[(int)SDLK_WORLD_79] = "world 79";
	(*g_KeyToName)[(int)SDLK_WORLD_80] = "world 80";
	(*g_KeyToName)[(int)SDLK_WORLD_81] = "world 81";
	(*g_KeyToName)[(int)SDLK_WORLD_82] = "world 82";
	(*g_KeyToName)[(int)SDLK_WORLD_83] = "world 83";
	(*g_KeyToName)[(int)SDLK_WORLD_84] = "world 84";
	(*g_KeyToName)[(int)SDLK_WORLD_85] = "world 85";
	(*g_KeyToName)[(int)SDLK_WORLD_86] = "world 86";
	(*g_KeyToName)[(int)SDLK_WORLD_87] = "world 87";
	(*g_KeyToName)[(int)SDLK_WORLD_88] = "world 88";
	(*g_KeyToName)[(int)SDLK_WORLD_89] = "world 89";
	(*g_KeyToName)[(int)SDLK_WORLD_90] = "world 90";
	(*g_KeyToName)[(int)SDLK_WORLD_91] = "world 91";
	(*g_KeyToName)[(int)SDLK_WORLD_92] = "world 92";
	(*g_KeyToName)[(int)SDLK_WORLD_93] = "world 93";
	(*g_KeyToName)[(int)SDLK_WORLD_94] = "world 94";
	(*g_KeyToName)[(int)SDLK_WORLD_95] = "world 95";

	(*g_KeyToName)[(int)SDLK_KP0] = "[0]";
	(*g_KeyToName)[(int)SDLK_KP1] = "[1]";
	(*g_KeyToName)[(int)SDLK_KP2] = "[2]";
	(*g_KeyToName)[(int)SDLK_KP3] = "[3]";
	(*g_KeyToName)[(int)SDLK_KP4] = "[4]";
	(*g_KeyToName)[(int)SDLK_KP5] = "[5]";
	(*g_KeyToName)[(int)SDLK_KP6] = "[6]";
	(*g_KeyToName)[(int)SDLK_KP7] = "[7]";
	(*g_KeyToName)[(int)SDLK_KP8] = "[8]";
	(*g_KeyToName)[(int)SDLK_KP9] = "[9]";
	(*g_KeyToName)[(int)SDLK_KP_PERIOD] = "[.]";
	(*g_KeyToName)[(int)SDLK_KP_DIVIDE] = "[/]";
	(*g_KeyToName)[(int)SDLK_KP_MULTIPLY] = "[*]";
	(*g_KeyToName)[(int)SDLK_KP_MINUS] = "[-]";
	(*g_KeyToName)[(int)SDLK_KP_PLUS] = "[+]";
	(*g_KeyToName)[(int)SDLK_KP_ENTER] = "enter";
	(*g_KeyToName)[(int)SDLK_KP_EQUALS] = "equals";

	(*g_KeyToName)[(int)SDLK_UP] = "up";
	(*g_KeyToName)[(int)SDLK_DOWN] = "down";
	(*g_KeyToName)[(int)SDLK_RIGHT] = "right";
	(*g_KeyToName)[(int)SDLK_LEFT] = "left";
	(*g_KeyToName)[(int)SDLK_DOWN] = "down";
	(*g_KeyToName)[(int)SDLK_INSERT] = "insert";
	(*g_KeyToName)[(int)SDLK_HOME] = "home";
	(*g_KeyToName)[(int)SDLK_END] = "end";
	(*g_KeyToName)[(int)SDLK_PAGEUP] = "page up";
	(*g_KeyToName)[(int)SDLK_PAGEDOWN] = "page down";

	(*g_KeyToName)[(int)SDLK_F1] = "f1";
	(*g_KeyToName)[(int)SDLK_F2] = "f2";
	(*g_KeyToName)[(int)SDLK_F3] = "f3";
	(*g_KeyToName)[(int)SDLK_F4] = "f4";
	(*g_KeyToName)[(int)SDLK_F5] = "f5";
	(*g_KeyToName)[(int)SDLK_F6] = "f6";
	(*g_KeyToName)[(int)SDLK_F7] = "f7";
	(*g_KeyToName)[(int)SDLK_F8] = "f8";
	(*g_KeyToName)[(int)SDLK_F9] = "f9";
	(*g_KeyToName)[(int)SDLK_F10] = "f10";
	(*g_KeyToName)[(int)SDLK_F11] = "f11";
	(*g_KeyToName)[(int)SDLK_F12] = "f12";
	(*g_KeyToName)[(int)SDLK_F13] = "f13";
	(*g_KeyToName)[(int)SDLK_F14] = "f14";
	(*g_KeyToName)[(int)SDLK_F15] = "f15";

	(*g_KeyToName)[(int)SDLK_NUMLOCK] = "numlock";
	(*g_KeyToName)[(int)SDLK_CAPSLOCK] = "caps lock";
	(*g_KeyToName)[(int)SDLK_SCROLLOCK] = "scroll lock";
	(*g_KeyToName)[(int)SDLK_RSHIFT] = "right shift";
	(*g_KeyToName)[(int)SDLK_LSHIFT] = "left shift";
	(*g_KeyToName)[(int)SDLK_RCTRL] = "right ctrl";
	(*g_KeyToName)[(int)SDLK_LCTRL] = "left ctrl";
	(*g_KeyToName)[(int)SDLK_RALT] = "right alt";
	(*g_KeyToName)[(int)SDLK_LALT] = "left alt";
	(*g_KeyToName)[(int)SDLK_RMETA] = "right cmd";
	(*g_KeyToName)[(int)SDLK_LMETA] = "left cmd";
	(*g_KeyToName)[(int)SDLK_LSUPER] = "left super";	/* "Windows" keys */
	(*g_KeyToName)[(int)SDLK_RSUPER] = "right super";
	(*g_KeyToName)[(int)SDLK_MODE] = "alt gr";
	(*g_KeyToName)[(int)SDLK_COMPOSE] = "compose";

	(*g_KeyToName)[(int)SDLK_HELP] = "help";
	(*g_KeyToName)[(int)SDLK_PRINT] = "print screen";
	(*g_KeyToName)[(int)SDLK_SYSREQ] = "sys req";
	(*g_KeyToName)[(int)SDLK_BREAK] = "break";
	(*g_KeyToName)[(int)SDLK_MENU] = "menu";
	(*g_KeyToName)[(int)SDLK_POWER] = "power";
	(*g_KeyToName)[(int)SDLK_EURO] = "euro";
	(*g_KeyToName)[(int)SDLK_UNDO] = "undo";

	(*g_KeyToName)[(int)kKeyCount + 0] = "mouse 0";
	(*g_KeyToName)[(int)kKeyCount + 1] = "mouse 1";
	(*g_KeyToName)[(int)kKeyCount + 2] = "mouse 2";
	(*g_KeyToName)[(int)kKeyCount + 3] = "mouse 3";
	(*g_KeyToName)[(int)kKeyCount + 4] = "mouse 4";
	(*g_KeyToName)[(int)kKeyCount + 5] = "mouse 5";
	(*g_KeyToName)[(int)kKeyCount + 6] = "mouse 6";

	for (int joystick=0;joystick<kMaxJoySticks;joystick++) {
		for (int button=0;button<kMaxJoyStickButtons;button++) {
			char buffy[100];
			if(joystick!=0)
				sprintf (buffy, "joystick %d button %d", joystick, button);
			else
				sprintf (buffy, "joystick button %d", button);
			(*g_KeyToName)[kKeyAndMouseButtonCount + joystick * kMaxJoyStickButtons + button] = buffy;
		}
	}

	AssertIf ((int)kKeyCount != (int)SDLK_LAST);

	g_NameToKey->clear ();
	for (KeyToName::iterator i=g_KeyToName->begin ();i != g_KeyToName->end ();i++)
		(*g_NameToKey)[i->second] = i->first;
}

string KeyToString (int key) {
	KeyToName::iterator found = g_KeyToName->find (key);
	if (found == g_KeyToName->end ())
		return string ();
	else
		return found->second;
}

int StringToKey (const string& name) {
	NameToKey::iterator found = g_NameToKey->find (name);
	if (found == g_NameToKey->end ())
		return 0;
	else
		return found->second;
}

void InputManager::InputEndFrame ()
{
	m_ThisFrameKeyDown.reset ();
	m_ThisFrameKeyUp.reset ();
#if ENABLE_NEW_EVENT_SYSTEM
	m_Mouse->deltaPos = Vector2f::zero;
	m_Mouse->deltaScroll = Vector2f::zero;
#else
	m_MouseDelta = Vector3f::zero;
#endif
	m_InputString.clear ();
}

void InputManager::ProcessInput()
{
	// Update Joystick 0

	if (m_JoystickPos.size() > 0)
	{
		vector<float>& joy0 = m_JoystickPos[0];

		for (vector<float>::iterator axis = joy0.begin(); axis != joy0.end(); ++axis)
		{
			*axis = 0.0f;
		}

		for (vector<vector<float> >::const_iterator joy = (m_JoystickPos.begin() + 1); joy != m_JoystickPos.end(); ++joy)
		{
			size_t const size = min(joy0.size(), joy->size());

			for (size_t i = 0; i < size; ++i)
			{
				if (abs((*joy)[i]) > abs(joy0[i]))
				{
					joy0[i] = (*joy)[i];
				}
			}
		}
	}

	//Update Axes
	for (vector<InputAxis>::iterator i = m_Axes.begin(); i != m_Axes.end(); i++)
		i->Update();
}

#if ENABLE_NEW_EVENT_SYSTEM
Rigidbody* FindRigidbodyInParents (GameObject& root)
{
	Rigidbody* rb = root.QueryComponentT<Rigidbody>(ClassID(Rigidbody));

	if (rb != NULL)
		return rb;

	Transform* parent = root.GetComponentT<Transform>(ClassID(Transform)).GetParent();

	if (parent == NULL)
		return NULL;

	return FindRigidbodyInParents(parent->GetGameObject());
}

void InputManager::SendInputEvents()
{
	// Prior to Unity 4.1 events were sent out via C# in MouseEvents.cs, and only for mouse events
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_1_a1))
	{
		ProcessMouse();
		ProcessTouches();
		m_CurrentTouch = NULL;
	}
}

void InputManager::ProcessMouse ()
{
	const RenderManager::CameraContainer& cameras = GetRenderManager().GetOnscreenCameras();

	// Mouse events are handled as separate touches for each button
	for (int b = 0; b < 3; ++b)
	{
		// Touch IDs: -1 for LMB, -2 for RMB, -3 for MMB.
		// Touch IDs for actual touch events are 0+.
		m_CurrentTouch = &m_Mouse[b];
		m_CurrentTouch->id = -1 - b;

		bool isPressed	= GetMouseButtonDown(b);
		bool isReleased = GetMouseButtonUp(b);
		bool isDragging = (m_CurrentTouch->press != NULL);

		if		(isReleased)	m_CurrentTouch->phase = Touch::kTouchEnded;
		else if (isPressed)		m_CurrentTouch->phase = Touch::kTouchBegan;
		else					m_CurrentTouch->phase = Touch::kTouchMoved;

		// TODO: What order are these in? We need to go from highest depth to lowest depth for events
		for (RenderManager::CameraContainer::const_iterator i = cameras.begin(); i != cameras.end();  ++i)
		{
			RaycastHit hit;
			const Camera& cam = (**i);
			Ray ray = cam.ScreenPointToRay(m_Mouse[0].pos);
			UInt32 mask = cam.GetEventMask() & cam.GetCullingMask();

			GameObject* go = NULL;

			if (GetPhysicsManager().Raycast(ray, 1000.0f, hit, mask))
			{
				m_CurrentTouch->worldPos = hit.point;
				go = &hit.collider->GetGameObject();

				// Raycast hitting a collider should target its rigidbody instead
				Rigidbody* rb = FindRigidbodyInParents(*go);
				if (rb != NULL)
					go = &rb->GetGameObject();
			}

			if (isPressed)
			{
				// Newly pressed -- clear all values
				m_CurrentTouch->press = NULL;
				m_CurrentTouch->oldPos = m_CurrentTouch->pos;
				//m_CurrentTouch->eligibleForClick = true;
				m_CurrentTouch->deltaPos = Vector2f::zero;
				m_CurrentTouch->deltaScroll = Vector2f::zero;
			}
			else if (b == 0)
			{
				// Left mouse button should calculate delta
				m_CurrentTouch->deltaPos = m_CurrentTouch->pos - m_CurrentTouch->oldPos;
				m_CurrentTouch->oldPos = m_CurrentTouch->pos;
			}
			else
			{
				// Other mouse buttons should simply copy the first button's data
				m_CurrentTouch->pos = m_Mouse[0].pos;
				m_CurrentTouch->deltaPos = m_Mouse[0].deltaPos;
				m_CurrentTouch->deltaScroll = m_Mouse[0].deltaScroll;
				m_CurrentTouch->deltaTime = m_Mouse[0].deltaTime;
			}

			// Process this mouse event as a touch
			ProcessTouch(go, isPressed, isReleased, isDragging, b == 0);
			if (go != NULL) break;
		}
	}
}

void InputManager::ProcessTouches()
{
	const RenderManager::CameraContainer& cameras = GetRenderManager().GetOnscreenCameras();

	size_t count = GetTouchCount();

	for (size_t i = 0; i < count; ++i)
	{
		m_CurrentTouch = GetTouch(count);

		bool isPressed  = (m_CurrentTouch->phase == Touch::kTouchBegan);
		bool isReleased = (m_CurrentTouch->phase == Touch::kTouchEnded) || (m_CurrentTouch->phase == Touch::kTouchCanceled);

		for (RenderManager::CameraContainer::const_iterator i = cameras.begin(); i != cameras.end();  ++i)
		{
			RaycastHit hit;
			const Camera& cam = (**i);
			Ray ray = cam.ScreenPointToRay(m_Mouse[0].pos);
			UInt32 mask = cam.GetEventMask() & cam.GetCullingMask();
			bool isHit = GetPhysicsManager().Raycast(ray, 1000.0f, hit, mask);

			GameObject* go = NULL;

			if (GetPhysicsManager().Raycast(ray, 1000.0f, hit, mask))
			{
				m_CurrentTouch->worldPos = hit.point;
				go = &hit.collider->GetGameObject();

				// Raycast hitting a collider should target its rigidbody instead
				Rigidbody* rb = FindRigidbodyInParents(*go);
				if (rb != NULL)
					go = &rb->GetGameObject();
			}

			if (m_CurrentTouch->phase == Touch::kTouchBegan)
			{
				m_CurrentTouch->press = NULL;
				m_CurrentTouch->oldPos = m_CurrentTouch->pos;
				//m_CurrentTouch->eligibleForClick = true;
				m_CurrentTouch->deltaPos = Vector2f::zero;
				m_CurrentTouch->deltaScroll = Vector2f::zero;
			}

			ProcessTouch(go, isPressed, isReleased, true, true);
			if (go != NULL) break;
		}
	}
}

void InputManager::ProcessTouch (GameObject* hover, bool isPressed, bool isReleased, bool isDragging, bool moveEvents)
{
	GUIState& state = GetGUIState();
	state.BeginUsingEvents();

	// Save the previous game objects prior to updates
	GameObject* previousHover = m_CurrentTouch->hover;
	GameObject* previousPress = m_CurrentTouch->press;

	// Update the current hovered so that Input.current.hover is proper in the callbacks below
	m_CurrentTouch->hover = hover;

	// Update the pressed object if it has changed
	if (isPressed) m_CurrentTouch->press = hover;

	MessageData data;
	InputEvent ev;
	ev.touch = *m_CurrentTouch;
	ev.button = -m_CurrentTouch->id;

	// Set the event so that C# can access it via Event.current.touch
	GetGUIState().SetEvent(ev);

	// Move and Drag notifications
	if (Magnitude(m_CurrentTouch->deltaPos) > 0.001f)
	{
		if (previousPress != NULL)
		{
			previousPress->SendMessageAny(kOnDragEvent, data);
		}
		else if (previousHover != NULL)
		{
			if (moveEvents)
				previousHover->SendMessageAny(kOnMouseMoveEvent, data);
		}
	}

	// Hovering over a different object
	if (previousHover != hover)
	{
		if (moveEvents && hover != NULL)
			hover->SendMessageAny(isDragging ? kOnDragEnterEvent : kOnMouseEnterEvent, data);

		if (moveEvents && previousHover != NULL)
			previousHover->SendMessageAny(isDragging ? kOnDragExitEvent : kOnMouseExitEvent, data);
	}

	// Pressed the mouse button on something
	if (isPressed)
	{
		// Change the selection
		if (m_Selection != hover)
		{
			GetGUIState().SetEvent(ev);

			if (hover != NULL)
				hover->SendMessageAny(kOnSelectEvent, data);

			// Only send the OnDeselect if the Event.current was marked as 'used'
			if (GetGUIState().GetIsEventUsed())
			{
				if (m_Selection != NULL)
					m_Selection->SendMessageAny(kOnDeselectEvent, data);

				m_Selection = hover;
			}
		}

		// Pressed on the hovered item
		if (hover != NULL)
			hover->SendMessageAny(kOnPressEvent, data);
	}

	// Released the mouse button
	if (isReleased)
	{
		if (hover != NULL)
		{
			if (previousPress == hover)
				hover->SendMessageAny(kOnClickEvent, data);
			else if (hover != NULL)
				hover->SendMessageAny(kOnDropEvent, data);
		}

		if (previousPress != NULL)
			previousPress->SendMessageAny(kOnReleaseEvent, data);
	}

	// Now that we are done with events we can clear the pressed object.
	// Clearing it earlier makes the property pointless for scripts: Null is not very informative.
	if (isReleased) m_CurrentTouch->press = NULL;
	state.EndUsingEvents();
}
#endif

void InputManager::SetKeyState (int key, bool state)
{
	// This ignores keyRepeats (multiple keydown without a keyup event between)
	if (state && !m_CurrentKeyState[key])
		m_ThisFrameKeyDown[key] = true;
	if (!state && m_CurrentKeyState[key])
		m_ThisFrameKeyUp[key] = true;

	m_CurrentKeyState[key] = state;
}

bool InputManager::ConfigureButton(int *button)
{
	for(int key=0;key<kKeyAndMouseButtonCount;key++)
		if(m_ThisFrameKeyDown[key])
		{
			*button=key;
			return true;
		}
	//skip virtual joystick 0
	for(int key=kKeyAndMouseButtonCount+kMaxJoyStickButtons;key<kKeyAndJoyButtonCount;key++)
		if(m_ThisFrameKeyDown[key])
		{
			*button=key;
			return true;
		}
	return false;
}

#if SUPPORT_REPRODUCE_LOG

template<class T>
void WriteFloat (std::ofstream& out, T& value)
{
	int intValue = RoundfToInt(value * 1000.0);
	out << intValue;
	value = intValue / 1000.0;
}

template<class T>
void ReadFloat (std::ifstream& in, T& value)
{
	int intValue = 0;
	in >> intValue;
	value = intValue / 1000.0;
}

void WriteBitSet (std::ofstream& out, dynamic_bitset& value)
{
	out << value.count() << ' ';
	for (int i=0;i<value.size();i++)
	{
		if (value[i])
			out << i << ' ';
	}
	out << std::endl;
}

void ReadBitSet (std::ifstream& in, dynamic_bitset& value)
{
	int size = 0;
	in >> size;
	value.reset();
	for (int i=0;i<size;i++)
	{
		int index = 0;
		in >> index;
		value[index] = true;
	}
}

void InputManager::WriteLog (std::ofstream& out)
{
	out << "Input" << std::endl;

	for (int i=0;i<m_Axes.size();i++)
	{
		WriteFloat(out, m_Axes[i].GetValueRef()); out << ' ';
		WriteFloat(out, m_Axes[i].GetValueRawRef());  out << ' ';
	}

	out << std::endl;

	WriteBitSet(out, m_CurrentKeyState);
	WriteBitSet(out, m_ThisFrameKeyDown);
	WriteBitSet(out, m_ThisFrameKeyUp);

//	to_string(m_CurrentKeyState, keystate); out << keystate << std::endl;
//	to_string(m_ThisFrameKeyDown, keystate); out << keystate << std::endl;
//	to_string(m_ThisFrameKeyUp, keystate); out << keystate << std::endl;



	WriteFloat(out, m_MouseDelta.x); out << ' ';
	WriteFloat(out, m_MouseDelta.y);  out << ' ';
	WriteFloat(out, m_MousePos.x); out << ' ';
	WriteFloat(out, m_MousePos.y);  out << ' ';
	float obsoleteWindowDelta = 0;
	WriteFloat(out, obsoleteWindowDelta); out << ' ';
	WriteFloat(out, obsoleteWindowDelta);  out << std::endl;
	WriteReproductionString(out, m_InputString);

	out << GetScreenManager().GetAllowCursorLock() << ' ';
}


void InputManager::ReadLog (std::ifstream& in)
{
	CheckReproduceTagAndExit("Input", in);

	for (int i=0;i<m_Axes.size();i++)
	{
		ReadFloat(in, m_Axes[i].GetValueRef());
		ReadFloat(in, m_Axes[i].GetValueRawRef());
	}

	ReadBitSet(in, m_CurrentKeyState);
	ReadBitSet(in, m_ThisFrameKeyDown);
	ReadBitSet(in, m_ThisFrameKeyUp);

	ReadFloat(in, m_MouseDelta.x);
	ReadFloat(in, m_MouseDelta.y);

	ReadFloat(in, m_MousePos.x);
	ReadFloat(in, m_MousePos.y);

	float obsoleteWindowDelta = 0;
	ReadFloat(in, obsoleteWindowDelta);
	ReadFloat(in, obsoleteWindowDelta);

	if (GetReproduceVersion () == 1 || GetReproduceVersion () == 2)
	{
		in.ignore(1);
		getline(in, m_InputString);
		if (in.peek() != 'E')
			in.ignore(1);
	}
	else
	{
		ReadReproductionString(in, m_InputString);
	}

	if (GetReproduceVersion () >= 5)
	{
		bool cursorLock;
		in >> cursorLock;
		GetScreenManager().SetAllowCursorLock(cursorLock);
	}
}
#endif

UInt16 NormalizeInputCharacter (UInt16 input)
{
	// standardize input characters as different platforms report different
	// ascii codes for the same input.
	if (input == 3)
		return '\n';
	if (input == '\r')
		return '\n';
	if (input == 8 || input == 127) //backspace
		return '\b';

	return input;
}

bool InputManager::GetTextFieldInput()
{
	return m_TextFieldInput;
}

bool InputManager::GetEnableIMEComposition()
{
	if (m_IMECompositionMode != InputManager::kCompositionModeAuto)
		return m_IMECompositionMode == InputManager::kCompositionModeOn;
	else
		return m_TextFieldInput;
}

bool InputManager::GetEatKeyPressOnTextFieldFocus()
{
	// The "or" predicate ensures that we force input-eating when modal windows are open
	if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_4_a1))
		return m_EatKeyPressOnTextFieldFocus || (GetGUIState().m_MultiFrameGUIState.m_Windows != NULL && GetGUIState().m_MultiFrameGUIState.m_Windows->m_ModalWindow != NULL);
#if UNITY_WIN
	return false;
#else
	return true;
#endif
}


// These stubs are for all the other platforms that do not (yet?) support this.
#if !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_EDITOR && !UNITY_WINRT && !UNITY_BB10 && !UNITY_TIZEN
unsigned GetOrientation () { return 0; }
size_t GetAccelerationCount () { return 0; }
void GetAcceleration (size_t, struct Acceleration&) {}
#endif

#if !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_BB10 && !UNITY_EDITOR && !UNITY_TIZEN && !UNITY_METRO && !UNITY_WP8
bool IsApplicationGenuine () { return true; }
bool IsApplicationGenuineAvailable () { return false; }
LocationInfo LocationService::GetLastLocation () { return LocationInfo (); }
LocationServiceStatus LocationService::GetLocationStatus ()
{ return kLocationServiceStopped; }
bool LocationService::IsServiceEnabledByUser () { return true; }
void LocationService::StartUpdatingLocation () {}
void LocationService::StopUpdatingLocation () {}
void LocationService::SetDesiredAccuracy (float) {}
void LocationService::SetDistanceFilter (float) {}
#endif

#if !UNITY_IPHONE && !UNITY_EDITOR && !UNITY_ANDROID && !UNITY_METRO && !UNITY_WP8
void LocationService::SetHeadingUpdatesEnabled (bool enabled) { }
bool LocationService::IsHeadingUpdatesEnabled() { return false; }
LocationServiceStatus LocationService::GetHeadingStatus ()
{ return  kLocationServiceStopped; }
const HeadingInfo &LocationService::GetLastHeading ()
{
	static HeadingInfo dummy = { 0, 0, Vector3f::zero, 0 };
	return dummy;
}
bool LocationService::IsHeadingAvailable () { return false; }
#endif

#if !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_WINRT
bool IsCompensatingSensors() { return false; }
void SetCompensatingSensors(bool val) { }
#endif
#if !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_EDITOR && !UNITY_METRO && !UNITY_WP8 && !UNITY_BB10 && !UNITY_TIZEN
Vector3f GetGyroRotationRate(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
bool IsGyroAvailable() { return false; }
Vector3f GetGyroRotationRateUnbiased(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f GetGravity(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Vector3f GetUserAcceleration(int idx) { return Vector3f(0.0f, 0.0f, 0.0f); }
Quaternionf GetAttitude(int idx) { return Quaternionf(0.0f, 0.0f, 0.0f, 1.0f); }
bool IsGyroEnabled(int idx) { return false; }
void SetGyroEnabled(int idx, bool enabled) {}
float GetGyroUpdateInterval(int idx) { return 0.0f; }
void SetGyroUpdateInterval(int idx, float interval) {}
int GetGyro() { return 0; }
#endif

#if !UNITY_PS3
Vector3f GetAcceleration (int controllerID) { return Vector3f(0,0,0); }
Quaternionf GetRotation(int controllerID) { return Quaternionf(0,0,0,0); }
Vector3f GetPosition(int controllerID) { return Vector3f(0,0,0); }
void SetActuator(int controllerID, UInt32 mode) {}
#endif

#if !UNITY_IPHONE
// --- iPhoneLocalNotification ---
iPhoneLocalNotification::iPhoneLocalNotification()
 :	m_Notification(0)
{ }

iPhoneLocalNotification::iPhoneLocalNotification(UILocalNotification *notification)
 :	m_Notification(0)
{ }

iPhoneLocalNotification::iPhoneLocalNotification(const iPhoneLocalNotification &other)
 :	m_Notification(0)
{ }

iPhoneLocalNotification::~iPhoneLocalNotification()
{ }

iPhoneLocalNotification &iPhoneLocalNotification::operator=(const iPhoneLocalNotification &other)
{ return *this; }

double iPhoneLocalNotification::GetFireDate() const
{
	return 0;
}

void iPhoneLocalNotification::SetFireDate(const double fireDate)
{ }

const char *iPhoneLocalNotification::GetTimeZone() const
{ return 0; }

void iPhoneLocalNotification::SetTimeZone(const char *timeZone)
{ }

unsigned int iPhoneLocalNotification::GetRepeatInterval() const
{ return 0; }

void iPhoneLocalNotification::SetRepeatInterval(unsigned int repeatInterval)
{ }

CalendarIdentifier iPhoneLocalNotification::GetRepeatCalendar() const
{ return kGregorianCalendar; }

void iPhoneLocalNotification::SetRepeatCalendar(CalendarIdentifier calendar)
{ }

const char *iPhoneLocalNotification::GetAlertBody() const
{ return 0; }

void iPhoneLocalNotification::SetAlertBody(const char *message)
{ }

const char *iPhoneLocalNotification::GetAlertAction() const
{ return 0; }

void iPhoneLocalNotification::SetAlertAction(const char *action)
{ }

bool iPhoneLocalNotification::HasAction() const
{ return false; }

void iPhoneLocalNotification::HasAction(bool yes)
{ }

const char *iPhoneLocalNotification::GetAlertLaunchImage() const
{ return 0; }

void iPhoneLocalNotification::SetAlertLaunchImage(const char *imagePath)
{ }

int iPhoneLocalNotification::GetApplicationIconBadgeNumber() const
{ return 0; }

void iPhoneLocalNotification::SetApplicationIconBadgeNumber(int number)
{ }

const char *iPhoneLocalNotification::GetSoundName() const
{ return 0; }

void iPhoneLocalNotification::SetSoundName(const char *soundName)
{ }

const char *iPhoneLocalNotification::GetDefaultSoundName()
{ return 0; }

MonoObject *iPhoneLocalNotification::GetUserInfo() const
{ return 0; }

void iPhoneLocalNotification::SetUserInfo(MonoObject *userInfo)
{ }

void iPhoneLocalNotification::Schedule()
{ }

void iPhoneLocalNotification::PresentNow()
{ }

void iPhoneLocalNotification::Cancel()
{ }

void iPhoneLocalNotification::CancelAll()
{ }

std::vector<iPhoneLocalNotification*> iPhoneLocalNotification::GetScheduled()
{ return std::vector<iPhoneLocalNotification*>(); }

// ---

size_t GetLocalNotificationCount()
{ return 0; }

iPhoneLocalNotification *CopyLocalNotification(unsigned index)
{ return 0; }

void ClearLocalNotifications()
{ }


// --- iPhoneRemoteNotification ---

iPhoneRemoteNotification::iPhoneRemoteNotification(NSDictionary *notification)
 :	m_HasAction(false),
	m_Badge(0),
	m_UserInfo(0)
{ }

iPhoneRemoteNotification::iPhoneRemoteNotification(const iPhoneRemoteNotification &other)
:	m_HasAction(false),
	m_Badge(0),
	m_UserInfo(0)
{ }

iPhoneRemoteNotification::~iPhoneRemoteNotification()
{ }

iPhoneRemoteNotification &iPhoneRemoteNotification::operator=(const iPhoneRemoteNotification &other)
{ return *this; }

const char *iPhoneRemoteNotification::GetAlertBody() const
{ return 0; }

bool iPhoneRemoteNotification::HasAction() const
{ return false; }

int iPhoneRemoteNotification::GetApplicationIconBadgeNumber() const
{ return 0; }

const char *iPhoneRemoteNotification::GetSoundName() const
{ return 0; }

MonoObject *iPhoneRemoteNotification::GetUserInfo() const
{ return 0; }

void iPhoneRemoteNotification::Register(int notifTypes)
{ }

void iPhoneRemoteNotification::Unregister()
{ }

int iPhoneRemoteNotification::GetEnabledTypes()
{ return 0; }

int iPhoneRemoteNotification::GetDeviceTokenLength()
{ return 0; }

const char *iPhoneRemoteNotification::GetDeviceToken()
{ return 0; }

void iPhoneRemoteNotification::SetDeviceToken(NSData *deviceToken)
{ }

//void iPhoneRemoteNotification::SendProviderDeviceToken(NSData *deviceToken)
//{ }

void iPhoneRemoteNotification::SetError(NSError *error)
{ }

const char *iPhoneRemoteNotification::GetError()
{ return 0; }

// ---

size_t GetRemoteNotificationCount()
{ return 0; }

iPhoneRemoteNotification *CopyRemoteNotification(unsigned index)
{ return 0; }

void ClearRemoteNotifications()
{ }

#endif
