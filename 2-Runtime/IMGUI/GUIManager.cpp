#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "GUIManager.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/IMGUI/GUIWindows.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Utilities/UserAuthorizationManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"

#if SUPPORT_REPRODUCE_LOG
#include <fstream>
#include "Runtime/Misc/ReproductionLog.h"
#endif

#if ENABLE_UNITYGUI
static GUIManager* s_GUIManager = NULL;

void InitGUIManager ()
{
	AssertIf(s_GUIManager != NULL);
	s_GUIManager = new GUIManager();
	
	InitGUIState ();
	
#if UNITY_HAS_DEVELOPER_CONSOLE
	InitializeDeveloperConsole ();
#endif // UNITY_HAS_DEVELOPER_CONSOLE
}

void CleanupGUIManager ()
{
#if UNITY_HAS_DEVELOPER_CONSOLE
	CleanupDeveloperConsole();
#endif // UNITY_HAS_DEVELOPER_CONSOLE
	CleanupGUIState ();

	AssertIf(s_GUIManager == NULL);
	delete s_GUIManager;
	s_GUIManager = NULL;
}

GUIManager &GetGUIManager () {
	AssertIf(s_GUIManager == NULL);
	return *s_GUIManager;
}

#if UNITY_EDITOR
void GUIManager::Reset ()
{
	m_MasterState.Reset ();	
}
#endif

GUIManager::GUIManager () :
	m_GUIPixelOffset (0.0f,0.0f)
{
	m_LastInputEventTime = 0.0f;
	m_DidGUIWindowsEatLastEvent = false;
	m_MouseUsed = false;
	m_mouseButtonsDown = 0;
}

void GUIManager::AddGUIScript (MonoBehaviourListNode& beh)
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

#if UNITY_EDITOR
void GUIManager::SetEditorGUIInfo (Vector2f guiPixelOffset) {
	m_GUIPixelOffset = guiPixelOffset;
	if (MONO_COMMON.setViewInfo) {
		ScriptingInvocation invocation(MONO_COMMON.setViewInfo);
		invocation.AddStruct(&m_GUIPixelOffset);
		invocation.Invoke();
	}
}

Vector2f GUIManager::GetEditorGUIInfo() const
{
	return m_GUIPixelOffset;
}
#endif

void GUIManager::SendQueuedEvents () 
{
	#if UNITY_EDITOR
	// Inside editor: make the Game GUI behave like it does at runtime.
	if (MONO_COMMON.setViewInfo) {
		Vector2f screenPosition (0,0);
		
		ScriptingInvocation invocation(MONO_COMMON.setViewInfo);
		invocation.AddStruct(&screenPosition);
		invocation.Invoke();
	}
	#endif
	
	while (!m_Events.empty())
	{
		DoGUIEvent(m_Events.front(), true);
		m_Events.pop_front();
	}
}

bool GUIManager::GetDidGUIWindowsEatLastEvent () {
	return GetGUIManager().m_DidGUIWindowsEatLastEvent;
}

void GUIManager::SetDidGUIWindowsEatLastEvent (bool value) {
	GetGUIManager().m_DidGUIWindowsEatLastEvent = value;
}

struct OldSortScript : std::binary_function<GUIManager::SortedScript&, GUIManager::SortedScript&, std::size_t>
{
	bool operator () (GUIManager::SortedScript& lhs, GUIManager::SortedScript& rhs) const { return lhs.depth < rhs.depth; }
};

struct NewSortScript : std::binary_function<GUIManager::SortedScript&, GUIManager::SortedScript&, std::size_t>
{
	bool operator () (GUIManager::SortedScript& lhs, GUIManager::SortedScript& rhs) const { return lhs.depth > rhs.depth; }
};



static bool MonoBehaviourDoGUI(void* beh, MonoBehaviour::GUILayoutType layoutType, int skin)
{
	Assert( NULL != beh );
	return reinterpret_cast<MonoBehaviour *>(beh)->DoGUI(layoutType, skin);
}

static ObjectGUIState& MonoBehaviourGetObjectGUIState(void* beh)
{
	Assert( NULL != beh );
	return reinterpret_cast<MonoBehaviour *>(beh)->GetObjectGUIState();
}

GUIManager::GUIObjectWrapper::GUIObjectWrapper(MonoBehaviour* beh)
	: wrapped_ptr(beh)
	, do_gui_func(&MonoBehaviourDoGUI)
	, get_gui_state_func(&MonoBehaviourGetObjectGUIState)
{}

#if UNITY_HAS_DEVELOPER_CONSOLE
static bool DeveloperConsoleDoGUI(void* dev, MonoBehaviour::GUILayoutType layoutType, int skin)
{
	Assert( NULL != dev );
	return reinterpret_cast<DeveloperConsole *>(dev)->DoGUI();
}

static ObjectGUIState& DeveloperConsoleGetObjectGUIState(void* dev)
{
	Assert( NULL != dev );
	return reinterpret_cast<DeveloperConsole *>(dev)->GetObjectGUIState();
}

GUIManager::GUIObjectWrapper::GUIObjectWrapper(DeveloperConsole* dev)
	: wrapped_ptr(dev)
	, do_gui_func(&DeveloperConsoleDoGUI)
	, get_gui_state_func(&DeveloperConsoleGetObjectGUIState)
{}
#endif // UNITY_HAS_DEVELOPER_CONSOLE


void GUIManager::DoGUIEvent (InputEvent &eventToSend, bool frontToBack) 
{
	#if ENABLE_PROFILER
	ProfilerInformation* information = &gGUIEventProfile;
	if (eventToSend.type == InputEvent::kRepaint)
		information = &gGUIRepaintProfile;

	PROFILER_AUTO(*information, NULL)
	#endif
	
	GUIState &state = GetGUIState();
	state.SetEvent (eventToSend);
	InputEvent &e = *state.m_CurrentEvent;
	
	m_MasterState.LoadIntoGUIState (state);
	state.BeginFrame ();
	MonoBehaviour* authorizationDialog = GetUserAuthorizationManager().GetAuthorizationDialog();
	
	// Update the lists of which scripts we _actually_ want to execute.
#if UNITY_HAS_DEVELOPER_CONSOLE
	if (m_GUIScripts.empty() && authorizationDialog == NULL && !GetDeveloperConsole().IsVisible())
#else
	if (m_GUIScripts.empty() && authorizationDialog == NULL)
#endif // UNITY_HAS_DEVELOPER_CONSOLE
	{
		m_MouseUsed = false;
		return;
	}

	// Move the event mouse position away if the screen is locked. We don't want the cursor to interact
	// with the GUI when it is in an arbitrary, fixed position.
	if (GetScreenManager().GetLockCursor()
#if UNITY_METRO
		&& e.touchType == InputEvent::kMouseTouch
#endif
		)
	{
#if ENABLE_NEW_EVENT_SYSTEM
		e.touch.pos = Vector2f (-10000, -10000);
#else
		e.mousePosition = Vector2f (-10000, -10000);
#endif
	}

	// ok - first we send them the layout event and find out the layering
	InputEvent::Type originalType = e.type;

	int handleTab = 0;
	if (e.type == InputEvent::kKeyDown && (e.character == '\t' || e.character == 25))
		handleTab = ((e.modifiers & InputEvent::kShift) == 0) ? 1 : -1;

	// Execute all the no-layout scripts
	bool eventUsed = false;
	std::vector<GUIObjectWrapper> layoutedScripts;
	if (authorizationDialog)
	{
		layoutedScripts.push_back (GUIObjectWrapper(authorizationDialog));
	}
	else
	{
		layoutedScripts.reserve(m_GUIScripts.size_slow());
		SafeIterator<MonoBehaviourList> guiScriptIterator (m_GUIScripts);
		while (guiScriptIterator.Next())
		{
			MonoBehaviour& beh = **guiScriptIterator;
			
			if (beh.GetUseGUILayout())
				layoutedScripts.push_back(GUIObjectWrapper(&beh));
			else
			{	
				if (!eventUsed)
					eventUsed |= beh.DoGUI (MonoBehaviour::kNoLayout, 0);
			}
		}
	}
	// TODO post 3.5: We can't bail here - nolayout scripts need tab handling as well
#if UNITY_HAS_DEVELOPER_CONSOLE
	layoutedScripts.push_back(GUIObjectWrapper(&GetDeveloperConsole()));
#endif // UNITY_HAS_DEVELOPER_CONSOLE

	int current = 1;
	bool hasModalWindow = state.m_MultiFrameGUIState.m_Windows != NULL && state.m_MultiFrameGUIState.m_Windows->m_ModalWindow != NULL;
	m_SortedScripts.clear ();
	if (!layoutedScripts.empty ())
	{
		// Send the layout event to all scripts that have that.
		e.type = InputEvent::kLayout;

		IMGUI::BeginWindows (state, true, !hasModalWindow);	// We need to enable clipping as otherwise it hasn't been set up.

		for (std::vector<GUIObjectWrapper>::iterator i = layoutedScripts.begin (); i != layoutedScripts.end (); ++i)
		{
			GUIObjectWrapper beh = *i;
			if (beh)
			{
				beh.DoGUI (MonoBehaviour::kGameLayout, 0);
				m_SortedScripts.push_back (SortedScript (GetGUIState().m_OnGUIState.m_Depth, beh));
				current++;
			}
		}
		// Remove any unused windows
		state.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (*state.m_CurrentEvent);
		IMGUI::EndWindows (state, !hasModalWindow);
		state.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*state.m_CurrentEvent);
	
		OldSortScript sort;
		// Next, we sort by depth
		m_SortedScripts.sort (sort);
		e.type = originalType;
	}

	bool hasSentEndWindows = false;

	current = 1;	// reset the count so we can send the DoWindows.
	if (frontToBack) 
	{
		// This gets cleared during REPAINT event. For normal event processing, we'll just re-read that.
		state.m_CanvasGUIState.m_IsMouseUsed = m_MouseUsed;
		
		for (SortedScripts::iterator i = m_SortedScripts.begin(); i != m_SortedScripts.end(); i++) 
		{
			// If this script used the event, we terminate the loop
			if (eventUsed)
				break;

			// If this is the first script in layer 0, Put BeginWindows / EndWindows around the OnGUI call
			bool endWindows = false;
			if ((hasModalWindow || (current == m_SortedScripts.size() || i->depth  > 0)) && !hasSentEndWindows)
			{
				IMGUI::BeginWindows (state, true, !hasModalWindow);
				if (eventUsed)
					break;
				hasSentEndWindows = true;
				endWindows = true;
			}
			current++;
			eventUsed = i->beh.DoGUI (MonoBehaviour::kGameLayout, 0);
			if (endWindows)
				IMGUI::EndWindows (state, !hasModalWindow);
		}

		// Remove keyboard focus when clicking on empty GUI area 
		// (so text fields don't take away game view input).
		if (originalType == InputEvent::kMouseDown && !eventUsed)
		{
			GUIState &state = GetGUIState();
			state.m_MultiFrameGUIState.m_KeyboardControl = 0;
		}
			
		// Handle mouse focus: We want the new GUI system to eat any mouse events. This means that we need to check this during repaint or mouseDown/Up 
		// and set a global variable accordingly.
		if (originalType == InputEvent::kMouseDown || originalType == InputEvent::kMouseUp)
			state.m_CanvasGUIState.m_IsMouseUsed = (bool)state.m_CanvasGUIState.m_IsMouseUsed | eventUsed;
	}
	else
	{	
		// We clear mouse used at repaint time, then let it run through any event processing for next frame, before reading it back
		m_MouseUsed = state.m_CanvasGUIState.m_IsMouseUsed = false;

		// I'm iterating backwards here - used by repainting
		// this time, users can't bail out of the event processing 
		IMGUI::BeginWindows (state, true, !hasModalWindow);
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
			i->beh.DoGUI (MonoBehaviour::kGameLayout, 0);
			if (endWindows)
			{
				state.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (*state.m_CurrentEvent);
				IMGUI::EndWindows (state, !hasModalWindow); // don't ignore modal windows if we have them
				state.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*state.m_CurrentEvent);
			}

			current++;	
		}
		
		if(hasModalWindow)
		{
			// Ensure modal windows are always on top by painting them last.
			state.m_CanvasGUIState.m_GUIClipState.BeginOnGUI (*state.m_CurrentEvent);
			IMGUI::RepaintModalWindow (state);
			state.m_CanvasGUIState.m_GUIClipState.EndOnGUI (*state.m_CurrentEvent);
		}
	}

	if (handleTab != 0 && !eventUsed && m_SortedScripts.size() != 0)
	{
		// Build the list of IDLists to cycle through
		std::vector<IDList*> keyIDLists;

		IMGUI::GUIWindow* focusedWindow = IMGUI::GetFocusedWindow (state);
		if (focusedWindow)
			keyIDLists.push_back (&focusedWindow->m_ObjectGUIState.m_IDList);
		else
		{
			keyIDLists.reserve (m_SortedScripts.size());
			for (SortedScripts::iterator i = m_SortedScripts.begin(); i != m_SortedScripts.end(); i++) 
			{
				keyIDLists.push_back (&i->beh.GetObjectGUIState().m_IDList);
			}
		} 
			
		state.CycleKeyboardFocus(keyIDLists, handleTab == 1);
	}
	
	m_MasterState.SaveFromGUIState (state);	
	m_MasterState.EndFrame ();
	m_MouseUsed = state.m_CanvasGUIState.m_IsMouseUsed;
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


GUIKeyboardState &GUIManager::GetMasterGUIState ()
{
	return GetGUIManager().m_MasterState;
}

#if SUPPORT_REPRODUCE_LOG
void WriteInputEvent (InputEvent& event, std::ostream& out)
{
	out << (int&)event.type << ' ';
	WriteFloat(out, event.mousePosition.x); out << ' ';
	WriteFloat(out, event.mousePosition.y);  out << ' ';
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
	ReadFloat(in, event.mousePosition.x);
	ReadFloat(in, event.mousePosition.y);
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
