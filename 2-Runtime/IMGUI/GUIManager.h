#ifndef GUIMANAGER_H
#define GUIMANAGER_H

#include "Configuration/UnityConfigure.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Misc/DeveloperConsole.h"

#include <deque>

struct InputEvent;
class GUIManager {
  public:
	GUIManager ();

	// Issue a repaint event - used by players... 
	// Mouse position & modifiers are read from the input manager.
	// TODO: implement Modifier keys
	void Repaint ();

	// Add/remove  - called from MonoBehaviour
	void AddGUIScript (MonoBehaviourListNode& beh);
	
	// Send an event to all in-game GUI scripts
	// e -				the event to send.
	// mask -			GOLayer mask to match.
	// frontToBack		Send event to the frontmost GUI script first (true for normal event processing, false for repaint)
	void DoGUIEvent (InputEvent &e, bool frontToBack);
	
	// Send an input event (keydown, mousemove, mouseup, etc) to the GUI scripts.
	void QueueEvent (InputEvent &ie);
	void QueueEventImmediate (InputEvent &ie);
	
	void SendQueuedEvents ();

	InputEvent GetLastInputEvent() { return m_LastEvent;}
	
	static bool AnyMouseButtonsDown();	

	// Did GUI code inside BeginWindows make it irrelevant for OnGUI code to be called at all? (e.g. mouseClick inside a GUI.window)
	// We used to do this by calling event.Use (), but that causes repaints, so instead this function is called that sets a var to track it
	static void SetDidGUIWindowsEatLastEvent (bool value);
	static bool GetDidGUIWindowsEatLastEvent ();
	
	#if UNITY_EDITOR
	// Clear all setting for entering / exiting playmode
	void Reset ();
	#endif
		
	static void ResetCursorFlash ();
	static float GetCursorFlashTime ();	

	#if UNITY_EDITOR
	void SetEditorGUIInfo (Vector2f guiPixelOffset);
	Vector2f GetGUIPixelOffset () { return m_GUIPixelOffset; }
	Vector2f GetEditorGUIInfo() const;
	#endif

	#if SUPPORT_REPRODUCE_LOG
	void WriteLog (std::ofstream& out);
	void ReadLog (std::ifstream& in);
	#endif
	
	struct GUIObjectWrapper
	{
	public:
		explicit GUIObjectWrapper(MonoBehaviour* beh);
	#if UNITY_HAS_DEVELOPER_CONSOLE
	
		explicit GUIObjectWrapper(DeveloperConsole* dev);

	#endif // UNITY_HAS_DEVELOPER_CONSOLE

		// Wrapped functions for GUI objects
		bool DoGUI(MonoBehaviour::GUILayoutType layoutType, int skin) const  {
			return do_gui_func(wrapped_ptr, layoutType, skin);
		}

		ObjectGUIState& GetObjectGUIState() const {
			return get_gui_state_func(wrapped_ptr);
		}

		// This avoids a plethora of pitfalls in situations,
		// where implicit conversions may happen
		typedef void * GUIObjectWrapper::*unspecified_bool_type;
		operator unspecified_bool_type() const  { // never throws
			return wrapped_ptr == 0? 0: &GUIObjectWrapper::wrapped_ptr;
		}

	private:
		typedef bool (*dogui_function_type)(void*, MonoBehaviour::GUILayoutType, int);
		typedef ObjectGUIState& (*get_gui_state_function_type)(void*);

		void* wrapped_ptr;

		dogui_function_type do_gui_func;
		get_gui_state_function_type get_gui_state_func;
	};

	struct SortedScript 
	{
		int depth;
		GUIObjectWrapper beh;
		SortedScript (int dep, GUIObjectWrapper b) : depth(dep), beh(b) {}
	};

	inline std::deque<InputEvent>& GetQueuedEvents() {return m_Events;}
	bool GetMouseUsed () const { return m_MouseUsed; }

	static GUIKeyboardState &GetMasterGUIState ();

private:
	typedef List<MonoBehaviourListNode> MonoBehaviourList;
	MonoBehaviourList m_GUIScripts;
	std::deque<InputEvent> m_Events;

	bool m_MouseUsed, m_DidGUIWindowsEatLastEvent;
	int m_mouseButtonsDown;
	Vector2f m_GUIPixelOffset;
	typedef std::list<SortedScript, memory_pool<SortedScript> > SortedScripts;
	SortedScripts m_SortedScripts;

	float m_LastInputEventTime;
	InputEvent m_LastEvent;
	GUIKeyboardState m_MasterState;
};

GUIManager &GetGUIManager ();

void InitGUIManager ();
void CleanupGUIManager ();

#endif
