#ifndef MONOCOMPILATIONPIPELINE_H
#define MONOCOMPILATIONPIPELINE_H

#include "MonoCompile.h"

struct MonoObject;
struct MonoClass;
struct MonoType;
struct MonoArray;
struct MonoString;
struct MonoDomain;

typedef void (*ScriptCompilationFinishedCallback) (bool success, const MonoCompileErrors& diagnostics);

bool IsExtensionSupportedByCompiler (const std::string& ext);
void ForceRecompileAllScriptsAndDlls (int assetImportFlags = 0);
void DirtyAllScriptCompilers ();
void DirtyScriptCompilerByAssemblyIdentifier(const std::string& assemblyIdentifier);
std::string GetCompilerAssemblyIdentifier (const std::string& pathName);
void SetupCustomDll (const std::string& dllName, const std::string& assemblyPath);
bool IsEditorOnlyAssembly (int index);
bool IsEditorOnlyAssembly (const std::string& path, bool checkAssemblyTargets = true);
bool IsEditorOnlyScriptAssemblyIdentifier (const std::string& assemblyIdentifier);
bool IsEditorOnlyScriptDllPath (const std::string& assemblyPath);
bool IsBlacklistedDllName (const std::string& assemblyPath);

void RecompileScripts (int options, bool buildingForEditor, BuildTargetPlatform platform, bool showProgress = true);
void PopupCompilerErrors();
bool ReloadAllUsedAssemblies ();
bool DoesProjectFolderHaveAnyScripts ();

void RegisterScriptCompilationFinishedCallback (ScriptCompilationFinishedCallback callback);

/// Returns if a mono reload assemblies should be performed.
bool UpdateMonoCompileTasks ();

MonoArray* GetAllManagedMonoIslands ();

void LoadMonoAssembliesOrRecompile ();
bool IsTargetSpecificExtensionDllAvailable(BuildTargetPlatform platform);

bool GenerateSPAConfig(std::string spaPath);

int GetBuildErrorIdentifier ();
void CleanupMonoCompilationPipeline();

extern const char* kEditorAssembliesPath;

#endif
