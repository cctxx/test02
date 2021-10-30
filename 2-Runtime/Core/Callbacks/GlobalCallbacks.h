#pragma once

#include "CallbackArray.h"
#include "Runtime/Modules/ExportModules.h"

struct EXPORT_COREMODULE GlobalCallbacks
{
	public:
	
	// Called after loading the level.
	// Resets all Random number seeds in the engine after loading a level.
	CallbackArray resetRandomAfterLevelLoad;
	
	CallbackArray didReloadMonoDomain;
	CallbackArray didUnloadScene;

	CallbackArray registerGizmos;

	//only used by hacky audiocode, should be killed.
	CallbackArray managersWillBeReloadedHack;

	CallbackArray willSaveScene;
	CallbackArray initializedEngineGraphics;

	CallbackArray initialDomainReloadingComplete;

	static GlobalCallbacks& Get();
};

#define REGISTER_GLOBAL_CALLBACK(eventName,body) struct eventName { static void Forward () { body; } };  GlobalCallbacks::Get().eventName.Register(eventName::Forward);
