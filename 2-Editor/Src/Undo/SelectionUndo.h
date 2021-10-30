#pragma once

#include "UndoBase.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include <set>

class SelectionUndo : public UndoBase
{
public:

	PPtr<Object>            m_Active;
	std::set<PPtr<Object> >  m_Selection;

	virtual bool Restore (bool registerRedo);
	virtual bool IgnoreInUndoLevel () { return true; }
};

void RegisterSelectionUndo ();

