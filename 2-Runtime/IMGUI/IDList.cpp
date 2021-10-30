#include "UnityPrefix.h"
#include "Runtime/IMGUI/IDList.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Misc/InputEvent.h"

IDList::IDList ()
{
	m_Idx = 0;
}

int IDList::CalculateNextFromHintList (GUIState &state, int hint, bool isKeyboard)
{
	int retval = 0;
	// Ok - we're searching: start at idx and search to end.
	for (int searchIdx = m_Idx; searchIdx < m_IDs.size(); searchIdx++)
	{
		if (m_IDs[searchIdx].hint == hint)
		{
			m_Idx = searchIdx + 1;
			retval = m_IDs[searchIdx].value; 
			break;
		}
	}
	
	// We still couldn't find it, so we just add to end...
	if (retval == 0)
	{
		retval = state.m_EternalGUIState->GetNextUniqueID();
		m_IDs.push_back (ID (hint, retval, isKeyboard));
		m_Idx = m_IDs.size();
	}
	
	return retval;
}

void IDList::BeginOnGUI () 
{
	m_Idx = 0;
	m_FirstKeyControl = -1;
	m_LastKeyControl = -1;
	m_PreviousKeyControl = -1;	
	m_NextKeyControl = -1;
	m_HasKeyboardControl = false;
	m_TabControlSearchStatus = kLookingForPrevious;
}

int IDList::GetNext (GUIState &state, int hint, FocusType focusType, const Rectf &rect)
{
	int retval = GetNext (state, hint, focusType);
	if (state.m_CurrentEvent->type != InputEvent::kLayout && state.m_CurrentEvent->type != InputEvent::kUsed && ShouldBeKeyboardControl(focusType))
	{
		Assert (m_Idx > 0);
		m_IDs[m_Idx-1].rect = rect;
	}
	return retval;
}

int IDList::GetNext (GUIState &state, int hint, FocusType focusType)
{
	InputEvent::Type type = state.m_CurrentEvent->type;

	bool isKeyboard = ShouldBeKeyboardControl(focusType);
	int retval = 0;
	if (type != InputEvent::kUsed)
		retval = CalculateNextFromHintList(state, hint, isKeyboard);
	else
	{
		return -1;
	}
	
	if (type != InputEvent::kLayout) 
	{
		if (type == InputEvent::kKeyDown && state.m_OnGUIState.m_Enabled == (int)true)
		{
			if (isKeyboard)
			{
				switch (m_TabControlSearchStatus)
				{
					case kNotActive:
						break;
					case kLookingForPrevious:
						if (m_FirstKeyControl == -1)
							m_FirstKeyControl = retval;
						if (retval != state.m_MultiFrameGUIState.m_KeyboardControl)
							m_PreviousKeyControl = retval;
						else
						{
							m_TabControlSearchStatus = kLookingForNext;
							m_HasKeyboardControl = true;
						}
						break;
					case kLookingForNext:
						m_NextKeyControl = retval;
						m_TabControlSearchStatus = kFound;
						break;
					default:
						break;
				}
				m_LastKeyControl = retval;				
			}
		}			
	}
	return retval;
}

bool IDList::GetRectOfControl (int id, Rectf &out) const
{
	for (dynamic_array<ID>::const_iterator i = m_IDs.begin(); i != m_IDs.end(); i++)
	{
		if (i->value == id && i->rect.width != -1.0f)
		{
			out = i->rect;
			return true;
		}
	}
	return false;
}

bool IDList::CanHaveKeyboardFocus (int id) const
{
	for (dynamic_array<ID>::const_iterator i = m_IDs.begin(); i != m_IDs.end(); i++)
	{
		if (i->value == id)
		{
			return i->isKeyboard;
		}
	}
	return false;
}

bool IDList::ShouldBeKeyboardControl (FocusType focus) {
	switch (focus) {
		case kPassive:
			return false;
		case kKeyboard:
			return true;
		case kNative:
			return false;
			// TODO: Move this back in during 2.x when we accept keyboard input on the various GUI controls.
			/*			PlatformSelection platform = GUI.skin.settings.keyboardFocus;
			 if (platform == PlatformSelection.Native) {
			 if (Application.platform == RuntimePlatform.WindowsPlayer || Application.platform == RuntimePlatform.WindowsWebPlayer || Application.platform == RuntimePlatform.WindowsEditor)
			 platform = PlatformSelection.Windows;
			 else
			 platform = PlatformSelection.Mac;
			 }
			 return platform == PlatformSelection.Windows;
			 */		}
	return true;
}

void IDList::SetSearchIndex (int index)
{
	if (index >= 0 && index < m_IDs.size())
		m_Idx = index;
	else
		AssertString (Format("Invalid index %d (size is %zd)", index, m_IDs.size()));
}

int IDList::GetSearchIndex () const
{
	return m_Idx;
}

