#pragma once
#include "Runtime/BaseClasses/GameObject.h"

class Selection
{
	public:
	
	/// Select an object.
	static void SetActive (Object* anObject);
	static void SetActiveID (int anObject);

	/// Get the currently active object.
	static Object* GetActive ();
	static int GetActiveID ();

	/// Get the currently active Game Object.
	static GameObject* GetActiveGO ();

	/// Get the array of all selected objects (including active)
	static void GetSelection (TempSelectionSet& selection);
	static std::set<PPtr<Object> > GetSelectionPPtr ();
	static std::set<int> GetSelectionID ();

	/// Set selection as an set of instanceIDs
	static void SetSelectionID (const std::set<int>& sel);
	template<typename objectcontainer>
	static void SetSelection (const objectcontainer& sel);
	static void SetSelectionPPtr (const std::set<PPtr<Object> >& sel);
};