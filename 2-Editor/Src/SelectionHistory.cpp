#if ENABLE_SELECTIONHISTORY
#include "UnityPrefix.h"
#include "SelectionHistory.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/BaseClasses/IsPlaying.h"


const int kMaxSelectionEntries = 20;

struct SelectionEntry
{
	SelectionEntry() : m_IsValid(false) { }
	
	void Invalidate()
	{
		m_Selection.clear();
		m_Active = NULL;
		m_IsValid = false;
	}
	
	std::set< PPtr<Object> > m_Selection;
	PPtr<Object> m_Active;
	bool m_IsValid;
};

static SelectionEntry s_Entries[kMaxSelectionEntries];
static int s_CurEntryIndex = -1;
static bool s_IgnoreSelectionChange = false;


void RegisterSelectionChange()
{
	if (IsWorldPlaying() || s_IgnoreSelectionChange)
		return;
	
	// get current selection
	std::set<PPtr<Object> > curSelection = GetObjectSelectionPPtr();
	PPtr<Object> curActive = GetActiveObject();
	
	// if current selection same as new one: don't do anything
	if (s_CurEntryIndex >= 0)
	{
		const SelectionEntry& sel = s_Entries[s_CurEntryIndex];
		if (sel.m_IsValid && sel.m_Active == curActive && sel.m_Selection == curSelection)
			return;
	}
	
	// got a new selection: clear all "next selection" entries
	for (int i = s_CurEntryIndex + 1; i < kMaxSelectionEntries; ++i)
		s_Entries[i].Invalidate();

	// if we reached max. capacity: move all previous entries
	if (s_CurEntryIndex == kMaxSelectionEntries-1)
	{
		for (int i = 1; i <= s_CurEntryIndex; ++i)
		{
			SelectionEntry& sa = s_Entries[i-1];
			SelectionEntry& sb = s_Entries[i];
			sa.m_Selection.swap (sb.m_Selection);
			sa.m_Active = sb.m_Active;
			sa.m_IsValid = sb.m_IsValid;
		}
	}
	else
	{
		++s_CurEntryIndex;
	}
	
	// put new entry info
	Assert(s_CurEntryIndex >= 0 && s_CurEntryIndex < kMaxSelectionEntries);
	SelectionEntry& newsel = s_Entries[s_CurEntryIndex];
	newsel.m_Selection = curSelection;
	newsel.m_Active = curActive;
	newsel.m_IsValid = true;
}


void GotoPreviousSelection()
{
	if (!HasPreviousSelection())
		return;
	--s_CurEntryIndex;
	
	Assert(s_CurEntryIndex >= 0 && s_CurEntryIndex < kMaxSelectionEntries);
	SelectionEntry& sel = s_Entries[s_CurEntryIndex];
	
	s_IgnoreSelectionChange = true;
	SetActiveObject(sel.m_Active);
	SetObjectSelection(sel.m_Selection);
	s_IgnoreSelectionChange = false;
}


void GotoNextSelection()
{
	if (!HasNextSelection())
		return;
	
	++s_CurEntryIndex;
	Assert(s_CurEntryIndex >= 0 && s_CurEntryIndex < kMaxSelectionEntries);
	SelectionEntry& sel = s_Entries[s_CurEntryIndex];
	Assert(sel.m_IsValid);
	
	s_IgnoreSelectionChange = true;
	SetActiveObject(sel.m_Active);
	SetObjectSelection(sel.m_Selection);
	s_IgnoreSelectionChange = false;
}


bool HasPreviousSelection()
{
	return s_CurEntryIndex > 0;
}


bool HasNextSelection()
{
	int nextIndex = s_CurEntryIndex + 1;
	if (nextIndex < 0 || nextIndex >= kMaxSelectionEntries)
		return false;
	if (!s_Entries[nextIndex].m_IsValid)
		return false;
	return true;
}
#endif
