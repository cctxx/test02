#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

// Disabled GUITests for lack of acceptable framework for native tests hitting scripting invocations (GetControlID). Rene is on the case!
#if 0
//#if ENABLE_UNIT_TESTS

#include "External/UnitTest++/src/UnitTest++.h"

#include "Runtime/IMGUI/GUIState.h"
#include "Runtime/Misc/InputEvent.h"
#include "Runtime/IMGUI/IDList.h"

static ObjectGUIState* gObjectGUIState;

// Set up a clean GUIState for testing purposes. Delete it with DeleteTestGUIState
static GUIState &MakeTestGUIState () 
{
	GUIState *state = new GUIState ();
	state->m_EternalGUIState = new EternalGUIState ();
	state->m_CurrentEvent = new InputEvent();
	state->m_CurrentEvent->Init();
	gObjectGUIState = new ObjectGUIState();
	return *state;
}

static void DeleteTestGUIState (GUIState &state)
{
	delete gObjectGUIState;
	delete &state;
}

static void BeginOnGUI (GUIState &state, InputEvent evt) 
{
	*state.m_CurrentEvent = evt;
	state.BeginOnGUI (*gObjectGUIState);
}

static void EndOnGUI (GUIState &state) 
{
	state.EndOnGUI ();
}

static InputEvent MakeLayoutEvent () 
{
	InputEvent e;
	e.type = InputEvent::kLayout;
	return e;
}

static InputEvent MakeRepaintEvent () 
{
	InputEvent e;
	e.type = InputEvent::kRepaint;
	return e;
}

static InputEvent MakeKeyDownEvent (char c) 
{
	InputEvent e;
	e.type = InputEvent::kKeyDown;
	e.character = c;
	return e;
}

SUITE ( GUITests )
{
TEST (GUITests_IDListGeneration)
{
	GUIState &state = MakeTestGUIState ();
	state.BeginFrame ();
	BeginOnGUI (state, MakeLayoutEvent ());
	int v1 = state.GetControlID (1, kPassive);
	int v2 = state.GetControlID (1, kPassive);
	int v3 = state.GetControlID (2, kPassive);	
	EndOnGUI (state);

	// Check we get the same IDs next event
	BeginOnGUI (state, MakeRepaintEvent ());
	CHECK_EQUAL (v1, state.GetControlID (1, kPassive));
	CHECK_EQUAL (v2, state.GetControlID (1, kPassive));
	CHECK_EQUAL (v3, state.GetControlID (2, kPassive));
	EndOnGUI (state);
	
	//check we correctly handle something going away.
	BeginOnGUI (state, MakeRepaintEvent ());
	CHECK_EQUAL (v1, state.GetControlID (1, kPassive));
	CHECK_EQUAL (v3, state.GetControlID (2, kPassive));
	EndOnGUI (state);

	state.EndFrame ();
	DeleteTestGUIState(state);
}

TEST (GUITests_IDListTabFinding)
{
	GUIState &state = MakeTestGUIState ();
	state.BeginFrame ();

	// Init & set up keycontrol
	BeginOnGUI (state, MakeLayoutEvent ());
	int v1 = state.GetControlID (1, kKeyboard);
	state.GetControlID (1, kPassive);
	int v2 = state.GetControlID (1, kKeyboard);
	int v3 = state.GetControlID (2, kKeyboard);	
	EndOnGUI (state);
	state.m_MultiFrameGUIState.m_KeyboardControl = v2;
	
	// Run again to make sure we have the values - they are only recorded on keydown events
	BeginOnGUI (state, MakeKeyDownEvent ('\t'));
	state.GetControlID (1, kKeyboard);
	state.GetControlID (1, kPassive);
	state.GetControlID (1, kKeyboard);
	state.GetControlID (2, kKeyboard);	

	CHECK (state.m_ObjectGUIState->m_IDList.HasKeyboardControl());
	CHECK_EQUAL (v1, state.m_ObjectGUIState->m_IDList.GetPreviousKeyboardControlID());
	CHECK_EQUAL (v3, state.m_ObjectGUIState->m_IDList.GetNextKeyboardControlID());
	CHECK_EQUAL (v1, state.m_ObjectGUIState->m_IDList.GetFirstKeyboardControlID());
	CHECK_EQUAL (v3, state.m_ObjectGUIState->m_IDList.GetLastKeyboardControlID());

	EndOnGUI (state);
	state.EndFrame ();
	DeleteTestGUIState(state);	
}

TEST (GUITests_IDListNamedKeyControls)
{
	GUIState &state = MakeTestGUIState ();
	state.BeginFrame ();

	BeginOnGUI (state, MakeLayoutEvent ());
	state.GetControlID (1, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v1");
	int v1 = state.GetControlID (1, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v2");
	state.GetControlID (1, kPassive);
	int v2 = state.GetControlID (1, kKeyboard);
	state.GetControlID (2, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v3fake");
	EndOnGUI (state);

	// Run event chain so we can move to past events
	BeginOnGUI (state, MakeRepaintEvent ());
	state.GetControlID (1, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v1");
	state.GetControlID (1, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v2");
	state.GetControlID (1, kPassive);
	state.GetControlID (1, kKeyboard);
	state.GetControlID (2, kKeyboard);
	state.SetNameOfNextKeyboardControl ("v3fake");
	
	CHECK_EQUAL (v1, state.GetIDOfNamedControl ("v1"));
	CHECK_EQUAL (v2, state.GetIDOfNamedControl ("v2"));
	EndOnGUI (state);
	
	state.EndFrame ();
	DeleteTestGUIState(state);	
}

TEST (GUITests_IDListNativeGetsKBControl)
{
	GUIState &state = MakeTestGUIState ();
	state.BeginFrame ();

	// First pass: Layout... get all the control ID's
	BeginOnGUI (state, MakeLayoutEvent ());
	
	// padding control at start
	int id1 = state.GetControlID (1, kNative);
	
	// first named control
	state.SetNameOfNextKeyboardControl ("named1");
	int id2 = state.GetControlID (1, kNative);
	
	// second named control... passive should be ignored!
	state.SetNameOfNextKeyboardControl ("named2");
	int id3 = state.GetControlID (1, kPassive);
	int id4 = state.GetControlID (1, kNative);
	
	// extra trailing controls
	int id5 = state.GetControlID (1, kNative);

	// check the named ID's
	CHECK_EQUAL (id2, state.GetIDOfNamedControl ("named1"));
	CHECK_EQUAL (id4, state.GetIDOfNamedControl ("named2"));
	EndOnGUI (state);

	// Now simulate a repaint... the control ID's should be the same as before!
	BeginOnGUI (state, MakeRepaintEvent ());
	CHECK_EQUAL (id1, state.GetControlID (1, kNative));
	state.SetNameOfNextKeyboardControl ("v1");
	CHECK_EQUAL (id2, state.GetControlID (1, kNative));
	state.SetNameOfNextKeyboardControl ("v2");
	CHECK_EQUAL (id3, state.GetControlID (1, kPassive));
	CHECK_EQUAL (id4, state.GetControlID (1, kNative));
	CHECK_EQUAL (id5, state.GetControlID (1, kNative));
	
	// also check that the named events STILL have the same ID's
	CHECK_EQUAL (id2, state.GetIDOfNamedControl ("named1"));
	CHECK_EQUAL (id4, state.GetIDOfNamedControl ("named2"));
	EndOnGUI (state);
	
	state.EndFrame ();
	DeleteTestGUIState(state);	
}
}
#endif
