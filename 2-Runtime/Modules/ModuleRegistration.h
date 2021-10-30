#pragma once

/// Modules are not guaranteed to exist. You must always check if the module getter function returns non-null.

struct ClassRegistrationContext;


void RegisterAllAvailableModuleClasses (ClassRegistrationContext& context);
void RegisterAllAvailableModuleICalls ();
void RegisterAvailableModules ();

typedef void RegisterClassesCallback (ClassRegistrationContext& context);
typedef void RegisterIcallsCallback ();

struct ModuleRegistrationInfo
{
	RegisterClassesCallback* registerClassesCallback;
	RegisterIcallsCallback* registerIcallsCallback;
};

EXPORT_COREMODULE void RegisterModuleInfo(ModuleRegistrationInfo& info);