#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "GuiManager.h"
#include "DeveloperConsole.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoUtility.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/GUI/GuiState.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/UserAuthorizationManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"

#if SUPPORT_REPRODUCE_LOG
#include <fstream>
#include "ReproductionLog.h"
#endif

#if ENABLE_UNITYGUI
static GUIManager* s_GUIManager = NULL;

void InitGUIManager ()
{
	AssertIf(s_GUIManager != NULL);
	s_GUIManager = new GUIManager();
}

void CleanupGUIManager ()
{
	AssertIf(s_GUIManager == NULL);
	delete s_GUIManager;
	s_GUIManager = NULL;
}

GUIManager &GetGUIManager () {
	AssertIf(s_GUIManager == NULL);
	return *s_GUIManager;
}


GUIManager::GUIManager () {
	m_CurrentDepth = 1;
	m_MouseUsed = false;
	m_RenderingUserGUI = false;
	m_HasKeyboardControl = false;
	m_KeyboardDirection = 0;
	m_CurrentKeyboardBehaviour = 0;
	m_CurrentBehaviour = NULL;
	m_LastInputEventTime = 0.0f;
	m_DidGUIWindowsEatLastEvent = false;
	#if UNITY_EDITOR
	m_HasKeyboardOverride = false;
	m_KeyboardControl = 0;
	#endif
	m_mouseButtonsDown = 0;
}

void GUIManager::AddGUIScript (ListNode_& beh)
{ 
	m_GUIScripts.push_back(beh);
}

PROFILER_INFORMATION(gGUIRepaintProfile, "GUI.Repaint", kProfilerGUI)
PROFILER_INFORMATION(gGUIEventProfile, "GUI.ProcessEvents", kProfilerGUI)

void GUIManager::Repaint () {
	GetInputManager().SetTextFieldInput(false);
	
	InputEvent ie;
	ie = m_LastEvent;
	ie.type = InputEvent::kRepaint;
	
	DoGUIEvent(ie, false);
}

bool GUIManager::AnyMouseButtonsDown()
{
	return GetGUIManager().m_mouseButtonsDown != 0;
}

void GUIManager::SetHasKeyboardControl (bool hasKeyboard) {
	GetGUIManager().m_HasKeyboardControl = true;
}

void GUIManager::SetKeyboardDirection (int direction) {
	GetGUIManager().m_KeyboardDirection = direction;
}
int GUIManager::GetKeyboardDirection () {
	return GetGUIManager().m_KeyboardDirection;
}

#if UNITY_EDITOR
// function EditorWindow can call to tell UnityGUI if this window has OS-level keyboard focus
void GUIManager::SetHasKeyboardOverride (int mode) {
	GetGUIManager().m_HasKeyboardOverride = mode;
}
#endif


bool GUIManager::CurrentScriptHasKeyboardFocus () {
	GUIManager &gm = GetGUIManager();
#if UNITY_EDITOR
	if (gm.m_HasKeyboardOverride == 1) 
		return true;
	else if (gm.m_HasKeyboardOverride == 2) 
		return false;
#endif
	
	return gm.m_CurrentBehaviour != NULL && gm.m_CurrentKeyboardBehaviour.GetInstanceID() == gm.m_CurrentBehaviour->GetInstanceID();
}
void GUIManager::SetKeyboardScriptInstanceID (int instanceID) {
	GetGUIManager().m_CurrentKeyboardBehaviour.SetInstanceID(instanceID);
}

// Small wrapper around DoGUI, that handles getting keyboard between the various controls.
bool GUIManager::CallGUI (GUIManager::SortedScripts::iterator i, InputEvent &e, bool doWindows) {
	MonoBehaviour *beh = i->beh;
	m_CurrentBehaviour = beh;
	bool retval = beh->DoGUI (e, true, true, doWindows, 0);
	return retval;
} 

void GUIManager::SetCurrentDepth (int depth) { GetGUIManager().m_CurrentDepth = depth; }
int GUIManager::GetCurrentDepth () { return GetGUIManager().m_CurrentDepth; }

#if UNITY_EDITOR
void GUIManager::SetEditorGUIInfo (bool hasKeyboardFocus, Vector2f guiPixelOffset) {
	if (MONO_COMMON.setViewInfo) {
		void* params[] = { &hasKeyboardFocus, &guiPixelOffset };
		CallStaticMonoMethod (MONO_COMMON.setViewInfo, params);		
	}
}
#endif

void GUIManager::SendQueuedEvents () 
{
	#if UNITY_EDITOR
	if (MONO_COMMON.setViewInfo) {
		bool hasKeyboardFocus = false;
		Vector2f screenPosition (0,0);
		void* params[] = { &hasKeyboardFocus, &screenPosition };
		CallStaticMonoMethod (MONO_COMMON.setViewInfo, params);		
	}
	#endif
	
	while (!m_Events.empty())
	{
		DoGUIEvent(m_Events.front(), true);
		m_Events.pop_front();
	}
}

//implemented in monobehaviour.cpp
void CallGuiUtilityBeginGUI(ScriptingObjectPtr monoEvent,int skin,int instanceID,bool allowGUILayout,ScriptingObjectPtr idList);

bool GUIManager::BeginWindows (InputEvent &event, int skin, int editorWindowID) 
{

	ScriptingObjectPtr monoEvent = CreateMonoInputEvent(event);
	m_DidGUIWindowsEatLastEvent = false;

	ScriptingObjectPtr idList = ScriptingGetGCHandleTarget(GetGlobalGUIState().m_State.m_IDListHandle);

	CallGuiUtilityBeginGUI(monoEvent, skin, 0, true, idList);
#if ENABLE_MONO
	void* params[] = { &skin, idList, &editorWindowID };
	CallStaticMonoMethod(MONO_COMMON.beginGuiWindows, params);
#elif UNITY_FLASH
	FLASH_ASM_WITH_NEWSP("GUI.BeginWindows(%0, marshallmap.getObjectWithId(%1) as UnityEngine.IDList, %2);" : : "r"(skin), "r"(idList), "r"(editorWindowID));
#endif

	InputEvent ie;
	MarshallManagedStructIntoNative(monoEvent,&ie);
	return ie.type == InputEvent::kUsed;
}

bool GUIManager::GetDidGUIWindowsEatLastEvent () {
	return GetGUIManager().m_DidGUIWindowsEatLastEvent;
}

void GUIManager::SetDidGUIWindowsEatLastEvent (bool value) {
	GetGUIManager().m_DidGUIWindowsEatLastEvent = value;
}

void GUIManager::EndWindows () {
	ScriptingObjectPtr idList = ScriptingGetGCHandleTarget(GetGlobalGUIState().m_State.m_IDListHandle);
#if ENABLE_MONO	
	void* params[] = { idList };
	CallStaticMonoMethod(MONO_COMMON.endGuiWindows, params);
#elif UNITY_FLASH
	FLASH_ASM_WITH_NEWSP("GUI.EndWindows(marshallmap.getObjectWithId(%0));" : : "r"(idList));
#endif
}

// A note on how the popup windows work:
// It's a bit of a hack: The gui system maintains a list of all popup windows that gets rebuilt every frame. Then we have some static functions we need to call to process that list.
// It's a LOT simpler if we do the EndWindows call from inside EndGUI, hence we have to call it DURING executing a script (even if its static).

// Here's how it goes:
// We call BeginWindows with a layout event before anything else. This makes them init.
// after the last layout event, we call EndWindows. Now the GUI system has an up-to-date list of windows.

// During event processing, we call beginWindows just before the first script that lies on a depth > 0, so it can eat mouse clicks, etc....
//			EndWindows can be called at any time here, so we do it on the same script
// During repaint, we can call BeginWindows at any point, but EndWindows on the LAST script that is in layer > 0. We call them at the same time.

// I _think_ this can be cleaned up a bit, but I'm not yet ready to do so before I have it working in scene views, etc...

struct OldSortScript : std::binary_function<GUIManager::SortedScript&, GUIManager::SortedScript&, std::size_t>
{
	bool operator () (GUIManager::SortedScript& lhs, GUIManager::SortedScript& rhs) const { return lhs.depth < rhs.depth; }
};

struct NewSortScript : std::binary_function<GUIManager::SortedScript&, GUIManager::SortedScript&, std::size_t>
{
	bool operator () (GUIManager::SortedScript& lhs, GUIManager::SortedScript& rhs) const { return lhs.depth > rhs.depth; }
};

void GUIManager::DoGUIEvent (InputEvent &e, bool frontToBack) 
{
	#if ENABLE_PROFILER
	ProfilerInformation* information = &gGUIEventProfile;
	if (e.type == InputEvent::kRepaint)
		information = &gGUIRepaintProfile;

	PROFILER_AUTO(*information, NULL)
	#endif
	
#if UNITY_EDITOR
	if (MONO_COMMON.setKeyboardControl)
	{
		void* args[] =  { &m_KeyboardControl };
		CallStaticMonoMethod(MONO_COMMON.setKeyboardControl, args);
	}
#endif
	
	MonoBehaviour* authorizationDialog = GetUserAuthorizationManager().GetAuthorizationDialog();
	MonoBehaviour* developerConsole = DeveloperConsole::Get();

	
	// Update the lists of which sripts we _actually_ want to execute.
	if (m_GUIScripts.empty() && authorizationDialog == NULL && !DeveloperConsole::IsVisible())
	{
		m_MouseUsed = false;
		return;
	}

	// Move the event mouse position away if the screen is locked. We don't want the cursor to interact
	// with the GUI when it is in an arbitrary, fixed position.
	if (GetScreenManager().GetLockCursor())
		e.mousePos = Vector2f (-10000, -10000);

	// ok - first we send them the layout event and find out the layering
	InputEvent::Type originalType = e.type;

	int handleTab = 0;
	if (e.type == InputEvent::kKeyDown && (e.character == '\t' || e.character == 25))
	{
		handleTab = ((e.modifiers & InputEvent::kShift) == 0) ? 1 : -1;
#if ENABLE_MONO
		if (MONO_COMMON.beginTabControlSearch)
			CallStaticMonoMethod (MONO_COMMON.beginTabControlSearch, NULL);
#endif
	}
	
	

	std::vector<PPtr<MonoBehaviour> > layoutedScripts;
	if (authorizationDialog)
		layoutedScripts.push_back (authorizationDialog);
	else
	{
		layoutedScripts.reserve(m_GUIScripts.size_slow());
		SafeListIterator<MonoBehaviour*> guiScriptIterator (m_GUIScripts);
		while (guiScriptIterator.Next())
		{
			MonoBehaviour& beh = **guiScriptIterator;
			
			if (beh.GetUseGUILayout())
				layoutedScripts.push_back(&beh);
			else
			{	
				m_CurrentDepth = 1;
				m_HasKeyboardControl = false;
				beh.DoGUI (e, false, false, false, 0);
			}
		}
	}
	if (developerConsole)
		layoutedScripts.push_back (developerConsole);
	if (layoutedScripts.empty())
		return;

	e.type = InputEvent::kLayout;
	BeginWindows (e, 0, 0);

	m_SortedScripts.clear ();
	int current = 1;
	for (std::vector<PPtr<MonoBehaviour> >::iterator i = layoutedScripts.begin (); i != layoutedScripts.end ();i ++)
	{
		MonoBehaviour *beh = *i;
		if (beh)
		{
			m_CurrentDepth = 1;
			m_HasKeyboardControl = false;
			bool doWindows = current == layoutedScripts.size();
			beh->DoGUI (e, true, true, doWindows, 0);
			m_SortedScripts.push_back (SortedScript (m_CurrentDepth, beh, m_HasKeyboardControl));
			current++;
		}
	}
	
//  @TODO: Fix sort order for 3.0 by introducing new property
//	if( IsUnity2_6OrHigher() )
//	{
//		NewSortScript sort;
//		// Next, we sort by depth
//		m_SortedScripts.sort (sort);
//	}
	OldSortScript sort;
	// Next, we sort by depth
	m_SortedScripts.sort (sort);
	
	// If we don't have a keyboard-focused script (or the script has been disabled), we set it to be the topmost script;
	MonoBehaviour* currentKey = m_CurrentKeyboardBehaviour;
	bool found = false;
	if (currentKey != NULL) {
		for (SortedScripts::iterator i = m_SortedScripts.begin(); i != m_SortedScripts.end(); i++) {
			if (i->beh == currentKey) {
				found = true;
				break;
			}
		}
	} 
	if (!found)
		m_CurrentKeyboardBehaviour = m_SortedScripts.begin()->beh;
	
	e.type = originalType;

	bool hasSentEndWindows = false;
	current = 1;	// reset the count so we can send the DoWindows.
	bool eventUsed = false;
	if (frontToBack) 
	{
		for (SortedScripts::iterator i = m_SortedScripts.begin(); i != m_SortedScripts.end(); i++) 
		{
			// If this is the first script in layer 0,
			bool endWindows = false;
			if ((current == m_SortedScripts.size() || i->depth  > 0) && !hasSentEndWindows) 
			{
				eventUsed = BeginWindows (e, 0, 0);
				if (eventUsed)
					break;
				hasSentEndWindows = true;
				endWindows = true;
			}
			current++;
		
			eventUsed = CallGUI (i, e, endWindows); // Tell the last one to do windows as well

			// If this script used the event, we terminate the loop
			if (eventUsed)
				break;
		}

		// Remove keyboard focus when clicking on empty GUI area 
		// (so text fields don't take away game view input).
		if (originalType == InputEvent::kMouseDown && !eventUsed)
		{
			int noKeyboardControl = 0;
			#if ENABLE_MONO
			void* args[] =  { &noKeyboardControl };
			CallStaticMonoMethod(MONO_COMMON.setKeyboardControl, args);
			#endif
		}
			
		// Handle mouse focus: We want the new GUI system to eat any mouse events. This means that we need to check this during repaint or mouseDown/Up 
		// and set a global variable accordingly.
		if (originalType == InputEvent::kMouseDown || originalType == InputEvent::kMouseUp)
			m_MouseUsed |= eventUsed;
	}
	else
	{	
		// Flag the mouse as beign unused. During repainting, we will detect if any controls are draw under the mouse and mark 
		// it as used.
		m_MouseUsed = false;
		m_RenderingUserGUI = true;

		// It's on purpose I'm iterating backwrds here - used by repainting
		// this time, you can't bail out of the event processing
		BeginWindows (e,0,0);
		for (SortedScripts::iterator i = m_SortedScripts.end(); i != m_SortedScripts.begin();) 
		{
			i--;
			bool endWindows = false;


			// If this window is the last, OR the next one is above 0 depth, we should do repaint all popup windows NOW
			if (!hasSentEndWindows)	// If we haven't already sent it
			{
				if (current == m_SortedScripts.size()) 	// If this is the last script, we must call it now
				{
					hasSentEndWindows = true;
					endWindows = true;
				} 
				else 
				{
					// If this is the last script with a depth > 0, we must call it now.
					SortedScripts::iterator j = i;
					j--;
					if (j->depth <= 0)
					{
						hasSentEndWindows = true;
						endWindows = true;
					}
				}
			}
			CallGUI (i, e, endWindows);
			current++;	
		}	

		m_RenderingUserGUI = false;
	}

	if (handleTab != 0 && !eventUsed)
	{
#if ENABLE_MONO
		if (MONO_COMMON.endTabControlSearch)
		{
			void* params[] = { &handleTab };
			CallStaticMonoMethod (MONO_COMMON.endTabControlSearch, params);		
			
		}
#endif
	}
	
#if UNITY_EDITOR
	if (MONO_COMMON.getKeyboardControl)
		m_KeyboardControl = ExtractMonoObjectData<int> (CallStaticMonoMethod(MONO_COMMON.getKeyboardControl, NULL));
#endif
}

void GUIManager::QueueEvent  (InputEvent &ie) 
{
	QueueEventImmediate(ie);
}


void GUIManager::QueueEventImmediate  (InputEvent &ie)
{
	// MouseMove events are not sent.
	// The same info can be obtained from repaint events.
	if (ie.type == InputEvent::kMouseMove)
	{
		// We still use them as last event to update the cursor position.
		m_LastEvent = ie;
		return;
	}
	if (ie.type == InputEvent::kIgnore)
		return;
	
	if ( ie.type == InputEvent::kMouseDown )
		m_mouseButtonsDown |= (1<<ie.button);
	else if ( ie.type == InputEvent::kMouseUp )
		m_mouseButtonsDown &= ~(1<<ie.button);
	
	switch (ie.type) {
		case InputEvent::kMouseDown:
		case InputEvent::kMouseUp:
		case InputEvent::kKeyDown:
			ResetCursorFlash ();
			break;
	}

	m_LastEvent = ie;
	m_Events.push_back(ie);
}

void GUIManager::ResetCursorFlash ()
{
	#if SUPPORT_REPRODUCE_LOG
	if (RunningReproduction())
		return;
	#endif

	GetGUIManager().m_LastInputEventTime = GetTimeManager().GetRealtime ();
}

float GUIManager::GetCursorFlashTime ()
{
	return GetGUIManager().m_LastInputEventTime;
}

bool GUIManager::GetMouseUsed () 
{ 
	return GetGUIManager().m_MouseUsed; 
}

void GUIManager::SetMouseUsed (bool used) 
{ 
	// We only allow changing m_MouseUsed during rendering of User GUI (fix for case 387913)
	if (GetGUIManager().m_RenderingUserGUI)
	{
		GetGUIManager().m_MouseUsed = used; 
	}
}

#if SUPPORT_REPRODUCE_LOG

void WriteInputEvent (InputEvent& event, std::ostream& out)
{
	out << (int&)event.type << ' ';
	WriteFloat(out, event.mousePos.x); out << ' ';
	WriteFloat(out, event.mousePos.y);  out << ' ';
	WriteFloat(out, event.delta.x); out << ' ';
	WriteFloat(out, event.delta.y); out << ' ';
	
	out << event.button << ' ' << event.modifiers << ' '; 
	WriteFloat(out, event.pressure); out << ' ';
	out << event.clickCount << ' ' << event.character << ' ' << event.keycode << ' ';
	
	//	if (event.commandString)
	//		WriteReproductionString(out, event.commandString);
	//	else
	//		WriteReproductionString(out, "");
}	

void ReadInputEvent (InputEvent& event, std::istream& in, int version)
{
	event.Init();
	
	in >> (int&)event.type;
	ReadFloat(in, event.mousePos.x);
	ReadFloat(in, event.mousePos.y);
	ReadFloat(in, event.delta.x);
	ReadFloat(in, event.delta.y);
	in >> event.button >> event.modifiers;
	ReadFloat(in, event.pressure);
	in >> event.clickCount >> event.character >> event.keycode;
	
	//if (version >= 6)
	//{
	//	std::string commandString;
	//	ReadReproductionString(in, commandString);
	//	event.commandString = new char[commandString.size() + 1];
	//	memcpy(event.commandString, commandString.c_str(), commandString.size() + 1);
	//}
}

void GUIManager::WriteLog (std::ofstream& out)
{
	out << "Events" << std::endl;

	out << m_Events.size() << std::endl;
	
	for (int i=0;i<m_Events.size();i++)
	{
		InputEvent& event = m_Events[i];
		WriteInputEvent(event, out);
	}
	
	// Hover events seem to require the last event as well!
    InputEvent& event = m_LastEvent;
	WriteInputEvent (event, out);
	
	if (GetReproduceVersion () >= 6)
	{
		WriteReproductionString(out, GetInputManager().GetCompositionString());
		out << (int)(GetInputManager().GetTextFieldInput()) << ' ';
	}

	out << std::endl;
}

void GUIManager::ReadLog (std::ifstream& in)
{
	CheckReproduceTagAndExit("Events", in);
	
	int size;
	in >> size;
	m_Events.clear();
	for (int i=0;i<size;i++)
	{
		InputEvent event;
		ReadInputEvent (event, in, GetReproduceVersion());
		m_Events.push_back(event);
	}
	
	// Hover events seem to require the last event as well!
	ReadInputEvent (m_LastEvent, in, GetReproduceVersion());

	if (GetReproduceVersion () >= 6)
	{
		ReadReproductionString(in, GetInputManager().GetCompositionString());
		int textFieldInput;
		in >> textFieldInput;
		GetInputManager().SetTextFieldInput(textFieldInput);
	}
}
#endif
#endif