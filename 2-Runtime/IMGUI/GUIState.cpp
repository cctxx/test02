#include "UnityPrefix.h"

#if ENABLE_UNITYGUI

#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/IMGUI/IDList.h"
#include "Runtime/IMGUI/NamedKeyControlList.h"
#include "Runtime/IMGUI/GUIWindows.h"
#include "Runtime/IMGUI/TextUtil.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/Backend/ScriptingArguments.h"

static EternalGUIState* gEternalGUIState = NULL;
static IDList* gEternalIDList = NULL;
static GUIState *gGUIState = NULL;

GUIState &GetGUIState ()
{
	AssertIf (!gGUIState);
	return *gGUIState;
}



EternalGUIState *GetEternalGUIState ()
{
	if (gEternalGUIState == NULL)
		gEternalGUIState = new EternalGUIState ();
	return gEternalGUIState;
}

static IDList* GetEternalIDList ()
{
	if (gEternalIDList == NULL)
		gEternalIDList = new IDList();
	return gEternalIDList;
}


MultiFrameGUIState::MultiFrameGUIState ()
{
	m_KeyboardControl = 0;
	m_NamedKeyControlList = NULL;
	m_Windows = NULL;
}

MultiFrameGUIState::~MultiFrameGUIState () 
{
	delete m_NamedKeyControlList;
	delete m_Windows;
}

void MultiFrameGUIState::Reset ()
{
	delete m_Windows;
	m_Windows = 0;

	delete m_NamedKeyControlList;
	m_NamedKeyControlList = 0;
}

IMGUI::NamedControl *MultiFrameGUIState::GetControlNamed (const std::string &name)
{
	if (m_NamedKeyControlList == NULL)
		return NULL;
	return m_NamedKeyControlList->GetControlNamed (name);
}

void MultiFrameGUIState::AddNamedControl (std::string &name, int id, IMGUI::GUIWindow* window)
{
	if (m_NamedKeyControlList == NULL)
		m_NamedKeyControlList = new IMGUI::NamedKeyControlList ();
	m_NamedKeyControlList->AddNamedControl(name, id, window ? window->m_ID : -1);
}

void MultiFrameGUIState::ClearNamedControls ()
{
	if (m_NamedKeyControlList)
		m_NamedKeyControlList->Clear ();
}


ObjectGUIState::ObjectGUIState ()
{
}

ObjectGUIState::~ObjectGUIState ()
{
}

void ObjectGUIState::BeginOnGUI ()
{
	m_IDList.BeginOnGUI ();
}

void OnGUIState::SetNameOfNextKeyboardControl (const std::string &nextName)
{
	delete m_NameOfNextKeyboardControl;
	m_NameOfNextKeyboardControl = new string (nextName);
}

void OnGUIState::ClearNameOfNextKeyboardControl ()
{
	delete m_NameOfNextKeyboardControl;
	m_NameOfNextKeyboardControl = NULL;
}

OnGUIState::OnGUIState ()
{
	m_NameOfNextKeyboardControl = NULL;
	m_MouseTooltip = m_KeyTooltip = NULL;
	m_CaptureBlock = NULL;
	m_Color = m_BackgroundColor = m_ContentColor = ColorRGBAf (1.0f,1.0f,1.0f,1.0f);
	m_Enabled = 1;
	m_Changed = 0;
	m_Depth = 0;
	m_ShowKeyboardControl = 1;
	
}

OnGUIState::~OnGUIState ()
{
	delete m_NameOfNextKeyboardControl;
	delete m_MouseTooltip;
	delete m_KeyTooltip;
}
	
void OnGUIState::BeginOnGUI ()
{
	m_Color = m_BackgroundColor = m_ContentColor = ColorRGBAf(1.0f,1.0f,1.0f,1.0f);
	m_Enabled = true;
	m_Changed = false;
	m_Depth = 1;
}

void OnGUIState::EndOnGUI ()
{
	delete m_NameOfNextKeyboardControl;
	m_NameOfNextKeyboardControl = NULL;
	delete m_MouseTooltip;
	m_MouseTooltip = NULL;
	delete m_KeyTooltip;
	m_KeyTooltip = NULL;
}


void OnGUIState::SetMouseTooltip (const UTF16String &tooltip)
{
	delete m_MouseTooltip;	
	m_MouseTooltip = new UTF16String (tooltip);
}

void OnGUIState::SetKeyTooltip (const UTF16String &tooltip)
{
	delete m_KeyTooltip;	
	m_KeyTooltip = new UTF16String (tooltip);
}

GUIState::GUIState ()
{
	m_CurrentEvent = NULL;
	m_ObjectGUIState = NULL;
	m_OnGUIDepth = 0;
}

GUIState::~GUIState() 
{
}


void GUIState::BeginFrame ()
{
	
}

void GUIState::EndFrame ()
{
	
}

void GUIState::BeginOnGUI (ObjectGUIState &objectGUIState) 
{
	m_ObjectGUIState = &objectGUIState;

	m_OnGUIState.BeginOnGUI ();
	m_ObjectGUIState->BeginOnGUI ();
	m_OnGUIDepth++;
}

void GUIState::EndOnGUI ()
{
	m_OnGUIState.EndOnGUI();
	m_ObjectGUIState = NULL;
	
	AssertIf (m_OnGUIDepth < 1);
	m_OnGUIDepth--;
}

static int GetControlID (GUIState& state, int hint, FocusType focusType, const Rectf& rect, bool useRect)
{	
	int id;
	
	if (state.m_ObjectGUIState == NULL)
	{
		id = state.m_EternalGUIState->GetNextUniqueID ();
		
#if (UNITY_EDITOR)
		// Editor GUI needs some additional things to happen when calling GetControlID.
		// While this will also be called for runtime code running in Play mode in the Editor,
		// it won't have any effect. EditorGUIUtility.s_LastControlID will be set to the id,
		// but this is only used inside the handling of a single control.
		ScriptingInvocation invocation(MONO_COMMON.handleControlID);
		invocation.AddInt(id);
		invocation.Invoke();
#endif
		
		return id;
	}
	
	if (useRect)
		id = state.m_ObjectGUIState->m_IDList.GetNext (state, hint, focusType, rect);
	else
		id = state.m_ObjectGUIState->m_IDList.GetNext (state, hint, focusType);

	// maybe in future this should check for focusType == keyboard
	// the issue currently is that there is no way to focus buttons via the keyboard
	// this means that if you have a game with no mouse input you can not 'tab' to the 
	// button using a joystic or keys.
	//
	// because of this we need to allow custom named controls of all types (apart from passive)
	// to be selectable.
	if (focusType != kPassive && state.m_OnGUIState.GetNameOfNextKeyboardControl () != NULL)
	{
		IMGUI::GUIWindow* currentWindow = NULL;
		if (state.m_MultiFrameGUIState.m_Windows)
			currentWindow = state.m_MultiFrameGUIState.m_Windows->m_CurrentWindow;
		state.m_MultiFrameGUIState.AddNamedControl (*state.m_OnGUIState.GetNameOfNextKeyboardControl (), id, currentWindow);
		state.m_OnGUIState.ClearNameOfNextKeyboardControl ();
	}
	
#if (UNITY_EDITOR)
	// Same as above; see comment there.
	ScriptingInvocation invocation(MONO_COMMON.handleControlID);
	invocation.AddInt(id);
	invocation.Invoke();
#endif
	
	return id;
}

int GUIState::GetControlID (int hint, FocusType focusType)
{
	return ::GetControlID (*this, hint, focusType, Rectf(), false);
}

int GUIState::GetControlID (int hint, FocusType focusType, const Rectf& rect)
{
	return ::GetControlID (*this, hint, focusType, rect, true);
}

int GUIState::GetIDOfNamedControl (const std::string &name)
{
	IMGUI::NamedControl *ctrl = m_MultiFrameGUIState.GetControlNamed (name);
	if (ctrl)
		return ctrl->ID;
	return 0;
}

std::string GUIState::GetNameOfFocusedControl ()
{
	if (m_MultiFrameGUIState.m_NamedKeyControlList)
		return m_MultiFrameGUIState.m_NamedKeyControlList->GetNameOfControl (m_MultiFrameGUIState.m_KeyboardControl);
	return "";
}

void GUIState::FocusKeyboardControl (const std::string &name)
{
	IMGUI::NamedControl *ctrl = m_MultiFrameGUIState.GetControlNamed (name);
	if (ctrl)
	{
		m_MultiFrameGUIState.m_KeyboardControl = ctrl->ID;
		IMGUI::FocusWindow (*this, ctrl->windowID);
	} 
	else
	{
		m_MultiFrameGUIState.m_KeyboardControl = 0;
		IMGUI::FocusWindow (*this, -1);
	}
}

void GUIState::SetEvent( const InputEvent& event )
{
	ScriptingInvocationNoArgs invocation;
	invocation.method = MONO_COMMON.makeMasterEventCurrent;
	invocation.Invoke();
	*m_CurrentEvent = event;
}

void GUIState::Internal_SetManagedEvent (void *event)
{
	m_CurrentEvent = (InputEvent*)event;
}

void GUIState::MoveAllDataTo (GUIState &dest, bool saving)
{
	dest.m_MultiFrameGUIState = m_MultiFrameGUIState;
	m_MultiFrameGUIState.m_NamedKeyControlList = NULL;
	m_MultiFrameGUIState.m_Windows = NULL;
	
	dest.m_OnGUIState = m_OnGUIState;
	m_OnGUIState.m_CaptureBlock = NULL;
	m_OnGUIState.m_NameOfNextKeyboardControl = NULL;
	m_OnGUIState.m_MouseTooltip = NULL;
	m_OnGUIState.m_KeyTooltip = NULL;
	
	dest.m_ObjectGUIState = m_ObjectGUIState;
	
	// Move over the clipping info.
	dest.m_CanvasGUIState = m_CanvasGUIState;
	
	// This one is truly eternal, so we don't want to clear it from the current.
	dest.m_EternalGUIState = m_EternalGUIState;
	
	// Move over the event. Since the event is shared in weird ways between Mono and C++, we'll copy it.
	if (saving)
	{
		dest.m_BackupEvent = *m_CurrentEvent;
		dest.m_CurrentEvent = m_CurrentEvent;
	}
	else
	{
		
		*(dest.m_CurrentEvent) = m_BackupEvent;
	}
}

GUIState* GUIState::GetPushState ()
{
	// Set up state
	GUIState *pushState = new GUIState ();
	GetGUIState().MoveAllDataTo (*pushState, true);
	return pushState;
}

void GUIState::PopAndDelete (GUIState *pushState)
{
	pushState->MoveAllDataTo (GetGUIState(), false);
	delete pushState;
}

static IDList* FindWhichIDListHasKeyboardControl (std::vector<IDList *> &idListsToSearch)
{
	// Find which IDList has the keyboard Control.
	for (std::vector<IDList *>::iterator i = idListsToSearch.begin(); i != idListsToSearch.end(); i++)
	{
		if ((*i)->HasKeyboardControl ())
		{
			return *i;
		}
	}
	return NULL;
}

void GUIState::CycleKeyboardFocus (std::vector<IDList *> &idListsToSearch, bool searchForward)
{
	m_MultiFrameGUIState.m_KeyboardControl = GetNextKeyboardControlID(idListsToSearch, searchForward);
}

int GUIState::GetNextKeyboardControlID (std::vector<IDList *> &idListsToSearch, bool searchForward)
{
	IDList *start = FindWhichIDListHasKeyboardControl (idListsToSearch);

	if (searchForward)
	{
		if (start && start->GetNextKeyboardControlID() != -1)
		{
			return start->GetNextKeyboardControlID();
		}
		
		// Which script did we start the keyboard search FROM.
		int startIdx = -1;
		// Which script are we scanning for keyboard controls.
		int listIdx = -1;
		if (start != NULL)
		{
			for (int i = 0; i < idListsToSearch.size(); i++)
			{
				if (idListsToSearch[i] == start)
				{
					startIdx = listIdx = (i + 1) % idListsToSearch.size();
					break;
				}
			}
		} else {
			startIdx = 0;//idListsToSearch.size();
			listIdx = 0;
		}
		
		do {
			int firstInList = idListsToSearch[listIdx]->GetFirstKeyboardControlID ();
			if (firstInList != -1)
			{
				return firstInList;
			}
			listIdx++;
			listIdx = listIdx % idListsToSearch.size();
		} while (listIdx != startIdx);
		return 0;
	}
	else
	{
		if (start && start->GetPreviousKeyboardControlID() != -1)
		{
			return start->GetPreviousKeyboardControlID();
		}
		
		int startIdx = 0, listIdx = idListsToSearch.size();
		if (start != NULL)
		{
			for (int i = 0; i < idListsToSearch.size(); i++)
			{
				if (idListsToSearch[i] == start)
				{
					startIdx = listIdx = i;
					break;
				}
			}
		}
		
		do {
			listIdx = listIdx - 1;
			if (listIdx == -1)
				listIdx = idListsToSearch.size() - 1;
			
			int firstInList = idListsToSearch[listIdx]->GetLastKeyboardControlID ();
			if (firstInList != -1)
			{
				return firstInList;
			}
		} while  (listIdx != startIdx);
		return 0;
	}
}

GUIKeyboardState::GUIKeyboardState ()
{
	m_KeyboardControl = 0;
	m_FocusedGUIWindow = -1;
	m_ShowKeyboardControl = true;
	m_Windows = NULL;
	m_NamedKeyControlList = NULL;
}

GUIKeyboardState::~GUIKeyboardState ()
{
	delete m_Windows;
	delete m_NamedKeyControlList;
}

void GUIKeyboardState::LoadIntoGUIState (GUIState &dest)
{
	dest.m_MultiFrameGUIState.m_KeyboardControl = m_KeyboardControl;

	Assert (!dest.m_MultiFrameGUIState.m_NamedKeyControlList);
	dest.m_MultiFrameGUIState.m_NamedKeyControlList = m_NamedKeyControlList;

	Assert (!dest.m_MultiFrameGUIState.m_Windows);
	dest.m_MultiFrameGUIState.m_Windows = m_Windows;

	dest.m_OnGUIState.m_ShowKeyboardControl = m_ShowKeyboardControl;
	m_Windows = NULL;
}
void GUIKeyboardState::SaveFromGUIState (GUIState &src)
{
	m_KeyboardControl = src.m_MultiFrameGUIState.m_KeyboardControl;
	m_NamedKeyControlList = src.m_MultiFrameGUIState.m_NamedKeyControlList;
	src.m_MultiFrameGUIState.m_NamedKeyControlList = NULL;
	m_Windows = src.m_MultiFrameGUIState.m_Windows;
	m_ShowKeyboardControl = src.m_OnGUIState.m_ShowKeyboardControl;
	src.m_MultiFrameGUIState.m_Windows = NULL;
}

void GUIKeyboardState::Reset ()
{
	m_KeyboardControl = m_FocusedGUIWindow = 0;
	delete m_Windows;
	m_Windows = NULL;
	delete m_NamedKeyControlList;
	m_NamedKeyControlList = NULL;
}

void GUIKeyboardState::EndFrame ()
{
	if (m_Windows)
		m_Windows->ReleaseScriptingObjects ();
}

void InitGUIState ()
{
	Assert(gGUIState == NULL);
	gGUIState = new GUIState ();
	gGUIState->m_EternalGUIState = GetEternalGUIState();
	gGUIState->m_CurrentEvent = new InputEvent ();
	gGUIState->m_CurrentEvent->Init();
}


void CleanupGUIState ()
{
	Assert(gGUIState != NULL);
	delete gGUIState;
	gGUIState = NULL;
}

#endif
