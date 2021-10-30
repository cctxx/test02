#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"	// Get STARTUP
#include "Editor/Src/Selection.h"
#include "Editor/Src/SelectionHistory.h"

struct SaveSelection : public MenuInterface {
	std::set<int> m_Selections[10];
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		const char cmd = menuItem.m_Command[0];
#if ENABLE_SELECTIONHISTORY		
		if (cmd == 'p')
		{
			return HasPreviousSelection();
		}
		else if (cmd == 'n')
		{
			return HasNextSelection();
		}
		else
#endif
		if (cmd == 's')
		{
			// Saving: can execute if we have a selection
			return !Selection::GetSelectionID().empty();
		}
		else
		{
			// Loading: need to have a selection saved
			return !m_Selections[atoi (&menuItem.m_Command[1])].empty();
		}
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		const char cmd = menuItem.m_Command[0];
#if ENABLE_SELECTIONHISTORY		
		if (cmd == 'p')
		{
			GotoPreviousSelection();
			return;
		}
		if (cmd == 'n')
		{
			GotoNextSelection();
			return;
		}
#endif
		
		int idx = atoi (&menuItem.m_Command[1]);
		if (menuItem.m_Command.c_str()[0] == 's')
		{
			// Save
			m_Selections[idx] = Selection::GetSelectionID();
		}
		else
		{
			// Load
			Selection::SetSelectionID(m_Selections[idx]);
		}				
	}

};
static SaveSelection *gSelCommands;
void SaveSelectionRegisterMenu ();
void SaveSelectionRegisterMenu ()
{
	gSelCommands = new SaveSelection;
	const int kCommandIdx = 200;

#if ENABLE_SELECTIONHISTORY
	// Use Mac browser shortcuts for selection history navigation;  Cmd+[ and Cmd+].
	// On Windows the browsers use Alt+Left and Alt+Right, but that's quite likely to clash
	// with game controls so let's not use them.
	MenuController::AddMenuItem ("Edit/Selection/Previous Selection %[", "p", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Next Selection %]", "n", gSelCommands, kCommandIdx);
#endif

	MenuController::AddMenuItem ("Edit/Selection/Load Selection 1 %#1", "l1", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 2 %#2", "l2", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 3 %#3", "l3", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 4 %#4", "l4", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 5 %#5", "l5", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 6 %#6", "l6", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 7 %#7", "l7", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 8 %#8", "l8", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 9 %#9", "l9", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Load Selection 0 %#0", "l0", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 1 %&1", "s1", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 2 %&2", "s2", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 3 %&3", "s3", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 4 %&4", "s4", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 5 %&5", "s5", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 6 %&6", "s6", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 7 %&7", "s7", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 8 %&8", "s8", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 9 %&9", "s9", gSelCommands, kCommandIdx);
	MenuController::AddMenuItem ("Edit/Selection/Save Selection 0 %&0", "s0", gSelCommands, kCommandIdx);
}

STARTUP (SaveSelectionRegisterMenu)	// Call this on startup.
