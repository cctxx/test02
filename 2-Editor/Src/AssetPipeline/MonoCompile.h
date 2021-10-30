#ifndef MONOCOMPILE_H
#define MONOCOMPILE_H

#include <vector>
#include <string>
#include <list>
class MonoScript;
struct MonoIsland;
struct MonoArray;
struct MonoString;

#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"

struct MonoCompileError
{
	enum { kError = 0, kWarning = 1 };
	std::string error;
	std::string file;
	int line;
	int column;
	int type;
};
typedef std::list<MonoCompileError> MonoCompileErrors;

typedef void CompileFinishedCallback (bool wasCompilationSuccessful, int options, bool buildingForEditor, const MonoIsland& island, const MonoCompileErrors& errors);

enum MonoCompileFlags
{
	kCompileFlagsNone = 0,
	kCompileSynchronous = 1 << 0,
	kDontLoadAssemblies = 1 << 1,
	kCompileDevelopmentBuild = 1 << 3
};

enum CompatibilityLevel
{
	kNET_2_0 = 1,
	kNET_Small = 2,
//	kNET_Micro = 2,
	kDefaultEditorCompatibilityLevel = kNET_Small
};

struct ManagedMonoIsland
{
	BuildTargetPlatform _target;
	MonoString* _classlibs_profile;
	MonoArray* _files;
	MonoArray* _references;
	MonoArray* _defines;
	MonoString* _output;
};

struct MonoSupportedLanguage
{
	MonoString* extension;
	MonoString* languageName;
};

struct SupportedLanguage
{
	std::string	extension;
	std::string	languageName;
};

struct MonoIsland
{
	int                     assemblyIndex;
	SupportedLanguage       language;
	std::string             assemblyPath;
	std::set<std::string>   paths;
	std::vector<std::string>  dependencies;
	std::vector<std::string>  defines;
	BuildTargetPlatform      targetPlatform;
	std::string				classlibs_profile;
	BuildTargetPlatform      simulationPlatform;
	
	ManagedMonoIsland CreateManagedMonoIsland();
};


// callback must be non-NULL:
// -  calls the callback after the compilation is finished
// - always returns true.
// Calls StopCompilation with island.assemblyIndex before starting the compile
void CompileFiles (CompileFinishedCallback* callback, bool buildingForEditor, const MonoIsland& island, int options = 0);
bool ConvertManagedCompileErrorsToNative(MonoArray* messages,MonoCompileErrors& errors);
void StopAllCompilation();
void StopCompilation (int assemblyIndex);
/// Updates compilation tasks.
/// Returns true if any compile tasks have been completed
bool UpdateMonoCompileTasks(bool waitForCompletion);
bool IsCompiling ();
bool IsCompiling (int assemblyIndex);
std::string GetMonoBinDirectory (BuildTargetPlatform targetPlatform);
std::vector<std::string> GetDirectoriesGameCodeCanReferenceFromForBuildTarget(BuildTargetPlatform target);
std::string GetScriptCompilerDirectory(BuildTargetPlatform target, CompatibilityLevel level);

#endif
