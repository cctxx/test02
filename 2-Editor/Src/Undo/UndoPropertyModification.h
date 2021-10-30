#pragma once
#include "Editor/Src/Prefabs/PropertyModification.h"

struct UndoPropertyModification
{
	PropertyModification	modification;
	bool					keepPrefabOverride;
};
typedef std::vector<UndoPropertyModification> UndoPropertyModifications;

