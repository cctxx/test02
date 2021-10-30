#ifndef EDITOR_MODULES_H
#define EDITOR_MODULES_H

#include <string>
#include "Runtime/Serialize/SerializationMetaFlags.h"

class IPlatformSupportModule;

struct EditorModuleEntry
{
	std::string         dllName;
	BuildTargetPlatform targetPlatform;
};

int GetEditorModuleCount();
const EditorModuleEntry& GetEditorModule(int index);
const EditorModuleEntry* GetEditorModuleByDllName(const std::string& dllName);
void InitializeManagedModuleManager();
void ShutdownManagedModuleManager();
void RegisterPlatformSupportModulesInManaged();

void UnloadCurrentPlatformSupportModule();
void LoadPlatformSupportModule(BuildTargetPlatform target);
IPlatformSupportModule* GetPlatformSupportModule();

#endif // EDITOR_MODULES_H
