#ifndef IDLIST_H
#define IDLIST_H

struct GUIState;
#include "Runtime/Math/Rect.h"
#include "Runtime/Utilities/dynamic_array.h"

/// Used by GUIUtility.GetcontrolID to inform the UnityGUI system if a given control can get keyboard focus.
/// MUST MATCH FocusType in GUIUtility.txt
enum FocusType {
	/// This control can get keyboard focus on Windows, but not on Mac. Used for buttons, checkboxes and other "pressable" things.
	kNative = 0,
	/// This is a proper keyboard control. It can have input focus on all platforms. Used for TextField and TextArea controls
	kKeyboard = 1,
	/// This control can never recieve keyboard focus.
	kPassive = 2,
};

/// Manages the list of IDs that are returned from GUIUtility.GetControlID ();
class IDList
{
public:
	IDList ();
	int GetNext (GUIState &state, int hint, FocusType focusType);
	int GetNext (GUIState &state, int hint, FocusType focusType, const Rectf &rect);
	void BeginOnGUI ();
	bool HasKeyboardControl () { return m_HasKeyboardControl; }

	// Get the ID of the keyboard control BEFORE the one that has keyboard focus (used to implement shift-tab) -1 if not found
	int GetPreviousKeyboardControlID () { return m_PreviousKeyControl; }
	// Get the ID of the keyboard control AFTER the one that has keyboard focus (used to implement tab)			-1 if not found
	int GetNextKeyboardControlID () { return m_NextKeyControl; }
	// Get the ID of the first keyboard control - used when tabbing into this script							-1 if not found
	int GetFirstKeyboardControlID () { return m_FirstKeyControl; }
	// Get the ID of the last keyboard control - used when tabbing out of this script							-1 if not found
	int GetLastKeyboardControlID () { return m_LastKeyControl; }

	bool GetRectOfControl (int id, Rectf &out) const;
	bool CanHaveKeyboardFocus (int id) const;
	void SetSearchIndex (int index);
	int GetSearchIndex () const;
private:
	// Enum and vars for handling searching for next and previous fields when tabbing through controls.
	enum TabControlSearchStatus 
	{
		kNotActive = 0, kLookingForPrevious = 1, kLookingForNext = 2, kFound = 3
	};
	TabControlSearchStatus m_TabControlSearchStatus;
	int m_FirstKeyControl, m_LastKeyControl, m_PreviousKeyControl, m_NextKeyControl;
	bool m_HasKeyboardControl;
	/// Determine if a given focusType should result in keyboard control
	static bool ShouldBeKeyboardControl (FocusType focus);

	int CalculateNextFromHintList (GUIState &state, int hint, bool isKeyboard);
	struct ID
	{
		int hint, value;
		bool isKeyboard;
		Rectf rect;
		ID (int _hint, int _value, bool _isKeyboard) : hint (_hint), value (_value), isKeyboard (_isKeyboard), rect (-1.0f, -1.0f, -1.0f, -1.0f) {}
		ID (int _hint, int _value, bool _isKeyboard, Rectf _rect) : hint (_hint), value (_value), isKeyboard (_isKeyboard), rect (_rect) {}
	};	
	
	dynamic_array<ID> m_IDs;
	int m_Idx;
};


#endif
