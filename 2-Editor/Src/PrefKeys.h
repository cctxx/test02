#pragma once

#include "Runtime/Math/Color.h"
using  std::string;
struct InputEvent;

/// Class for KeyBindings. 
/// def: 	lowercase is keyboard key. Uppercase is one of:
///			LEFT,RIGHT,UP,DOWN,F1..F12,HOME,END,PGUP, PGDN
///			KP0..KP9, KP*, KP=, KP/, KP+, KP-, KP.
///		prefix with: &=Alt  ^=Ctrl  %=Cmd  #=shift
#define PREFKEY(name, def) static EditorPrefKey name (#name, def)
class EditorPrefKey
{
 	public:

	EditorPrefKey () {}
	EditorPrefKey (const std::string &n, const char *def);
	
	std::string GetString ();
	std::string GetStringIgnoringModifiers ();
	void ParseUserInputKeycode (string keyCode);
	
	static EditorPrefKey* Get(int i);
	static int Count();

	static void RevertAll ();

	bool Matches (InputEvent& event) const;

	void Load ();
	void Save ();
	void Revert ();
		
//	bool Matches (InputEvent& event);
//	#if __OBJC__
//	bool operator () (NSEvent *e) const;
//	bool Matches (NSEvent *e) const;
//	#endif

	void Set (const char *def);

	string GetName() { return m_Name; }

  public:
	string         m_Name;

  	int            m_KeyCode;
	UInt32         m_ModifierFlags;

  	int            m_DefaultKeyCode;
	UInt32         m_DefaultModifierFlags;

};


#define PREFCOLOR(name,def) EditorPrefColor name(#name, def);

class EditorPrefColor {
	
	public:

	EditorPrefColor (const std::string &n, UInt32 rgba);

	string GetName() { return m_Name; }

	void Load ();
	void Save ();
	void Revert ();

	operator ColorRGBAf () const { return m_Color; }
	ColorRGBAf GetColor () { return m_Color; }

	static EditorPrefColor* Get(int i);
	static int Count();

	static void RevertAll ();
	
	public:

	string     m_Name;
	ColorRGBAf m_DefaultColor;
	ColorRGBAf m_Color;
};


#define PREFFLOAT(name, def) static EditorPrefFloat name (#name, def)
class EditorPrefFloat
{
	public:
	
	EditorPrefFloat (const std::string &n, float defaultValue);
  
	operator float ()	{ return m_Value; }

	static EditorPrefFloat* Get(int i);
	static int Count();

	string GetName() { return m_Name; }

	void Load ();
	void Save ();
	void Revert ();


	string         m_Name;
	float          m_Value;
	float          m_DefaultValue;
};

void ReadPrefKeysFromPreferences ();
