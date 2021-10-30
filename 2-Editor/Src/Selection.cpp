#include "UnityPrefix.h"
#include "Selection.h"
#include "SceneInspector.h"

void Selection::SetActive (Object* anObject) { GetSceneTracker().SetActive(anObject); }

void Selection::SetActiveID (int anObject) { GetSceneTracker().SetActiveID(anObject); }

Object* Selection::GetActive () { return GetSceneTracker().GetActive(); }
int Selection::GetActiveID ()  { return GetSceneTracker().GetActiveID(); }

GameObject* Selection::GetActiveGO () { return GetSceneTracker().GetActiveGO(); }

void Selection::GetSelection (TempSelectionSet& selection)   { GetSceneTracker().GetSelection(selection); }
std::set<PPtr<Object> > Selection::GetSelectionPPtr () { return GetSceneTracker().GetSelectionPPtr(); }
std::set<int> Selection::GetSelectionID () { return GetSceneTracker().GetSelectionID(); }

void Selection::SetSelectionID (const std::set<int>& sel) { GetSceneTracker().SetSelectionID(sel); }
template<typename objectcontainer>
void Selection::SetSelection (const objectcontainer& sel) { GetSceneTracker().SetSelection(sel); }
void Selection::SetSelectionPPtr (const std::set<PPtr<Object> >& sel) { GetSceneTracker().SetSelectionPPtr(sel); }

template void Selection::SetSelection < TempSelectionSet  > (const TempSelectionSet& sel);
template void Selection::SetSelection < std::set<Object*> > (const std::set<Object*>& sel);