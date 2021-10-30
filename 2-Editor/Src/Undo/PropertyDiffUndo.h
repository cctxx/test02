#pragma once
#include "UndoBase.h"
#include "UndoPropertyModification.h"
#include <vector>

class PropertyDiffUndo : public UndoBase
{
public:
	PropertyDiffUndo(const UndoPropertyModifications& modifications);

	virtual bool Restore(bool registerRedo);
	void RemoveDuplicates( UndoPropertyModifications& modifications );

private:
	UndoPropertyModifications m_UndoPropertyModifications;
};

void RegisterPropertyModificationUndo (UndoPropertyModifications& modifications, const UnityStr& actionName);
