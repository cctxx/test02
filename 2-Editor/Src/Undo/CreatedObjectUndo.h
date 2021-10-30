#pragma once
#include "Editor/Src/Undo/UndoBase.h"
#include "Runtime/BaseClasses/BaseObject.h"

class CreatedObjectUndo : public UndoBase
{
private:
	PPtr<Object> m_Object;

public:

	CreatedObjectUndo (PPtr<Object> object, bool isSceneUndo, const std::string& actionName);
	virtual ~CreatedObjectUndo ();

	virtual bool Restore (bool registerRedo);
};

void RegisterCreatedObjectUndo (PPtr<Object> o, bool isSceneUndo, const std::string& actionName);
void RegisterCreatedObjectUndo (Object* object, const std::string& actionName);