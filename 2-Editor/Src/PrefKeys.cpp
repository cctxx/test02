#include "UnityPrefix.h"
#include "PrefKeys.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Utilities/PlayerPrefs.h"


using namespace std;

static vector<EditorPrefColor*> *gColorList = NULL;
static vector<EditorPrefKey*> *gKeyList = NULL;
static vector<EditorPrefFloat*> *gFloatList = NULL;

void ReadPrefKeysFromPreferences ()
{
	// read all colors.
	for (int i=0;i<EditorPrefColor::Count();i++)
	{
		EditorPrefColor::Get(i)->Load();
	}

	// read all keys.
	for (int i=0;i<EditorPrefKey::Count();i++)
	{
		EditorPrefKey::Get(i)->Load();
	}
	
	// read all floats.
	for (int i=0;i<EditorPrefFloat::Count();i++)
	{
		EditorPrefFloat::Get(i)->Load();
	}
}

EditorPrefColor::EditorPrefColor (const string &n, UInt32 rgba)
{
	m_Color.SetHex(rgba);
	m_DefaultColor = m_Color;

	m_Name = n;
	
	if (!gColorList)
		gColorList = new vector<EditorPrefColor*>;

	gColorList->push_back (this);
}

void EditorPrefColor::Load ()
{
	if (EditorPrefs::HasKey ("Color " + m_Name))
	{
		int hex = EditorPrefs::GetInt ("Color " + m_Name);
		m_Color.SetHex (hex);
	}
	else
	{
		m_Color = m_DefaultColor;
	}
}

void EditorPrefColor::Save ()
{
	if (m_DefaultColor.GetHex() != m_Color.GetHex())
	{
		EditorPrefs::SetInt ("Color " + m_Name, m_Color.GetHex());
	}
	else
	{
		EditorPrefs::DeleteKey ("Color " + m_Name);
	}
}
	
void EditorPrefColor::Revert ()
{
	m_Color = m_DefaultColor;
	EditorPrefs::DeleteKey("Color " + m_Name);
}

void EditorPrefColor::RevertAll ()
{
	// read all colors.
	for (int i=0;i<EditorPrefColor::Count();i++)
	{
		EditorPrefColor::Get(i)->Revert();
	}
}

EditorPrefColor* EditorPrefColor::Get(int i)
{
	if (i >= 0 && i < Count())
		return (*gColorList)[i];
	else
		return NULL;
}

int EditorPrefColor::Count()
{
	return gColorList->size();
}

struct KeyMnemonic
{
	const char *keyMnemonic;
	unsigned int code;
}; 

static KeyMnemonic s_keyNames[] =
{
	"LEFT", SDLK_LEFT, 
	"RIGHT", SDLK_RIGHT, 
	"UP", SDLK_UP, 
	"DOWN", SDLK_DOWN, 
	"F1", SDLK_F1, 
	"F2", SDLK_F2, 
	"F3", SDLK_F3, 
	"F4", SDLK_F4, 
	"F5", SDLK_F5, 
	"F6", SDLK_F6, 
	"F7", SDLK_F7, 
	"F8", SDLK_F8, 
	"F9", SDLK_F9, 
	"F10", SDLK_F10, 
	"F11", SDLK_F11, 
	"F12", SDLK_F12, 
	"HOME", SDLK_HOME, 
	"END", SDLK_END, 
	"PGUP", SDLK_PAGEUP, 
	"PGDN", SDLK_PAGEDOWN, 
	"KP0", SDLK_KP0,
	"KP1", SDLK_KP1,
	"KP2", SDLK_KP2,
	"KP3", SDLK_KP3,
	"KP4", SDLK_KP4,
	"KP5", SDLK_KP5,
	"KP6", SDLK_KP6,
	"KP7", SDLK_KP7,
	"KP8", SDLK_KP8,
	"KP9", SDLK_KP9,
	"KP.", SDLK_KP_PERIOD,
	"KP+", SDLK_KP_PLUS,
	"KP-", SDLK_KP_MINUS,
	"KP*", SDLK_KP_MULTIPLY,
	"KP/", SDLK_KP_DIVIDE,
	"KP=", SDLK_KP_EQUALS,
	NULL, 0
};

EditorPrefKey::EditorPrefKey (const string &n, const char *def)
{
	Set (def);
	m_DefaultKeyCode = m_KeyCode;
	m_DefaultModifierFlags = m_ModifierFlags;
	m_Name = n;
	if (!gKeyList)
		gKeyList = new vector<EditorPrefKey*>;
	gKeyList->push_back (this);
}

void EditorPrefKey::Load ()
{
	if (EditorPrefs::HasKey ("KeyCode " + m_Name) && EditorPrefs::HasKey ("KeyModifierFlags " + m_Name))
	{
		m_KeyCode = EditorPrefs::GetInt ("KeyCode " + m_Name);
		m_ModifierFlags = EditorPrefs::GetInt ("KeyModifierFlags " + m_Name);
	}
	else
	{
		m_KeyCode = m_DefaultKeyCode;
		m_ModifierFlags = m_DefaultModifierFlags;
	}
}
	
void EditorPrefKey::Save ()
{
	if (m_KeyCode != m_DefaultKeyCode || m_ModifierFlags != m_DefaultModifierFlags)
	{
		EditorPrefs::SetInt("KeyCode " + m_Name, m_KeyCode);
		EditorPrefs::SetInt("KeyModifierFlags " + m_Name, m_ModifierFlags);
	}
	else
	{
		EditorPrefs::DeleteKey("KeyCode " + m_Name);
		EditorPrefs::DeleteKey("KeyModifierFlags " + m_Name);
	}
}

void EditorPrefKey::Revert ()
{
	m_KeyCode = m_DefaultKeyCode;
	m_ModifierFlags = m_DefaultModifierFlags;
	EditorPrefs::DeleteKey("KeyCode " + m_Name);
	EditorPrefs::DeleteKey("KeyModifierFlags " + m_Name);
}

void EditorPrefKey::RevertAll ()
{
	// read all colors.
	for (int i=0;i<EditorPrefKey::Count();i++)
	{
		EditorPrefKey::Get(i)->Revert();
	}
}

void EditorPrefKey::ParseUserInputKeycode (string keyCode)
{
	if (keyCode.size() == 1)
	{
		m_KeyCode = keyCode[0];
	}
	else
	{
		KeyMnemonic *k = s_keyNames;
		while (k->keyMnemonic)
		{
			if (keyCode == k->keyMnemonic)
			{
				m_KeyCode = k->code;
				break;
			}
			k++;
		}
	}
}


void EditorPrefKey::Set (const char *def)
{
 	m_ModifierFlags = 0;
	m_KeyCode = 0;
	while (*def)
	{
		switch (*def)
		{
		case '&': // Alt
			m_ModifierFlags |= InputEvent::kAlt;
			break;
		case '^': // Ctrl
			m_ModifierFlags |= InputEvent::kControl;
			break;
		case '%':
			m_ModifierFlags |= InputEvent::kCommand;
			break;
		case '#':
			m_ModifierFlags |= InputEvent::kShift;
			break;
		default:
			if (*def == ToLower (*def)) 
				m_KeyCode = *def;
			else
			{
				KeyMnemonic *k = s_keyNames;
				while (k->keyMnemonic)
				{
					if (strcmp (def, k->keyMnemonic) == 0)
					{
						m_KeyCode = k->code;
						break;
					}
					k++;
				}
			}
		}
		def++;
	}
}

bool EditorPrefKey::Matches (InputEvent& event) const
{
	if (event.type != InputEvent::kKeyDown && event.type != InputEvent::kKeyUp)
		return false;
		
	if (event.modifiers != m_ModifierFlags)
		return false;

	return event.keycode == m_KeyCode;
}

string EditorPrefKey::GetString () {
	string hmm = "";
	if (m_ModifierFlags & InputEvent::kShift)
		hmm = "SHIFT-";
	if (m_ModifierFlags & InputEvent::kControl)
		hmm = "CTRL-";
	if (m_ModifierFlags & InputEvent::kAlt)
		hmm = "ALT-";
	if (m_ModifierFlags & InputEvent::kCommand)
		hmm = "CMD-";

	hmm += GetStringIgnoringModifiers();
	return hmm;
}

string EditorPrefKey::GetStringIgnoringModifiers () {
	KeyMnemonic *k = s_keyNames;
	while (k->keyMnemonic)
	{
		if (m_KeyCode == k->code)
		{
			return k->keyMnemonic;
		}
		k++;
	}
	
	if (m_KeyCode >= ' ' && m_KeyCode <= 'z')
		return string() + (char)m_KeyCode;
	else
		return string();
}

EditorPrefKey* EditorPrefKey::Get(int i)
{
	if (i >= 0 && i < Count())
		return (*gKeyList)[i];
	else
		return NULL;
}

int EditorPrefKey::Count()
{
	return gKeyList->size();
}

EditorPrefFloat::EditorPrefFloat (const string &n, float defaultValue)
{
	m_DefaultValue = defaultValue;
	m_Value = defaultValue;
	m_Name = n;
	if (!gFloatList)
		gFloatList = new vector<EditorPrefFloat*>;
	gFloatList->push_back (this);
}


void EditorPrefFloat::Load ()
{
	if (EditorPrefs::HasKey ("Float " + m_Name))
	{
		m_Value = EditorPrefs::GetFloat ("Float " + m_Name);
	}
	else
	{
		m_Value = m_DefaultValue;
	}
}

void EditorPrefFloat::Save ()
{
	if (m_DefaultValue != m_Value)
	{
		EditorPrefs::SetFloat ("Float " + m_Name, m_Value);
	}
	else
	{
		EditorPrefs::DeleteKey ("Float " + m_Name);
	}
}
	
void EditorPrefFloat::Revert ()
{
	m_Value = m_DefaultValue;
	EditorPrefs::DeleteKey("Float " + m_Name);
}

EditorPrefFloat* EditorPrefFloat::Get(int i)
{
	if (i >= 0 && i < Count())
		return (*gFloatList)[i];
	else
		return NULL;
}

int EditorPrefFloat::Count()
{
	return gFloatList->size();
}
