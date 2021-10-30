#pragma once
#if ENABLE_SELECTIONHISTORY

void RegisterSelectionChange();
void GotoPreviousSelection();
void GotoNextSelection();
bool HasPreviousSelection();
bool HasNextSelection();
#endif
