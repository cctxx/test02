#include "UnityPrefix.h"
#include "CreatedObjectUndo.h"
#include "Undo.h"
#include "UndoManager.h"

CreatedObjectUndo::CreatedObjectUndo (PPtr<Object> object, bool isSceneUndo, const std::string& actionName)
{
	SetIsSceneUndo(isSceneUndo);
	m_Name = actionName;
	m_Object = object;
}

CreatedObjectUndo::~CreatedObjectUndo() { }

bool CreatedObjectUndo::Restore(bool registerRedo)
{
	Object* object = m_Object;
	if (object == NULL)
		return false;
	
	DestroyObjectUndoable(object);
	return true;
}

void RegisterCreatedObjectUndo (PPtr<Object> o, bool isSceneUndo, const std::string& actionName)
{
	CreatedObjectUndo* undo = UNITY_NEW(CreatedObjectUndo, kMemUndo) (o, isSceneUndo, actionName);
	GetUndoManager().RegisterUndo(undo);
}

void RegisterCreatedObjectUndo (Object* object, const std::string& actionName)
{
	if (object != NULL)
		RegisterCreatedObjectUndo (object, !object->IsPersistent(), actionName);
}