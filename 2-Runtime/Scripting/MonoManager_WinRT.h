#ifndef MONOMANAGER_WINRT_H
#define MONOMANAGER_WINRT_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Mono/MonoScriptManager.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingManager.h"

class MonoManager : public ScriptingManager
{
public:
	REGISTER_DERIVED_CLASS (MonoManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (MonoManager)

	MonoManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~MonoManager (); declared-by-macro

	ScriptingObjectPtr CreateInstance(ScriptingClassPtr klass);
	
private:

};

MonoManager& GetMonoManager ();
MonoManager* GetMonoManagerPtr ();

#endif
