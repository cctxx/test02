#include "UnityPrefix.h"
#include "MonoCompilationPipeline.h"
#include "LogicGraphCompilationPipeline.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Configuration/UnityConfigureOther.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "AssetDatabase.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Utility/BaseHierarchyProperty.h"
#include "Runtime/Serialize/BuildTargetVerification.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/Argv.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Src/ProjectWizardUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/EditorModules.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "Editor/Platform/Interface/AssetProgressbar.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Editor/Platform/Interface/RefreshAssets.h"
#include "Editor/Src/File/FileScanning.h"
#include "Editor/Src/VersionControl/VCProvider.h"
#include "Editor/Mono/MonoEditorUtility.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include <sstream>

using namespace std;

typedef vector<MonoIsland> MonoIslands;

static void CompilationFinishedAsync (bool success, const string& assemblyName, const MonoCompileErrors& errors);
static string GetAssetPathFromScript (Object& o);
static void LogCompileErrors (const MonoCompileErrors& errors, bool buildingPlayerErrors, int assemblyIndex);
static MonoIslands GetChangedMonoIslands (int options, bool buildingForEditor, BuildTargetPlatform platform);
static int GetErrorIdentifierForAssembly (int assemblyIndex);
static void InitializeAssemblyTargets ();
static string GetRuntimeExportFolder ();
static string GetEditorExportFolder ();
static set<int> GetDependentScriptAssemblies (int assemblyIndex);
void RebuildManagedEditorCode(int sync, bool recompileUserScripts);
static bool RecompileScriptsInternal (int options, bool buildingForEditor, BuildTargetPlatform platform);
static void SetPersistentIsCompilingFlag (bool isCompiling);
void SetMonoImporterAssemblyCount(int size);
static const char* GetProfileForBuild(BuildTargetPlatform platform, CompatibilityLevel compatibilityLevel);

const char* kEditorAssembliesPath = "Library/ScriptAssemblies";
const char* kCompilationCompletedFile = "Library/ScriptAssemblies/CompilationCompleted.txt";

const char* kGraphsDllName = "UnityEngine.Graphs.dll";
const char* kGraphsEditorDllName = "UnityEditor.Graphs.dll";

static const char* kCompilingScriptsMessage = "Compiling scripts...";

static string GetRuntimeExportFolder ()
{
	string unityFolder = GetBaseUnityDeveloperFolder();
	string monoExportFolder = AppendPathName (unityFolder, "Runtime/Export");
	return 	monoExportFolder;
}

static string GetEditorExportFolder ()
{
	string unityFolder = GetBaseUnityDeveloperFolder();
	string monoExportFolder = AppendPathName (unityFolder, "Editor/Mono");
	return 	monoExportFolder;
}


struct AssemblyTarget
{
	SupportedLanguage	language;
	string				assemblyName;
	vector<string>		requiredPath;
	vector<int>			dependencies;
};


typedef vector_set<string> SupportedExtensions;
static SupportedExtensions gSupportedExtensions;

typedef vector_map<string, int> AssemblyNameToIndex;
static AssemblyNameToIndex gAssemblyNameToIndex;

typedef vector<SupportedLanguage> SupportedLanguages;
static SupportedLanguages gSupportedLanguages;

typedef vector<AssemblyTarget> AssemblyTargets;
static AssemblyTargets gAssemblyTargets;

static AssemblyMask gDirtyAssemblyCompileMask ;
static AssemblyMask gAllUsedAssemblies ;
static AssemblyMask gIslandsFailed ;
static vector<int> gCompilerIdentifiers;
static vector<ScriptCompilationFinishedCallback> gScriptCompilationFinishedCallbacks;
static vector_map<string, bool> gEditorAssembliesCache;

static string GetScriptsTargetDllPathName (string assemblyName)
{
	CreateDirectory ("Temp");
	return AppendPathName("Temp", assemblyName);		
}

bool CanFindAssembly(string assembly, BuildTargetPlatform target)
{
	if (IsFileCreated(assembly)) return true;

	vector<string> dirs = GetDirectoriesGameCodeCanReferenceFromForBuildTarget(target);
	for (int i=0; i!=dirs.size(); i++)
	{
		if (IsFileCreated(AppendPathName(dirs[i],assembly))) 
			return true;
	}
	return false;
}

static set<int> sTargetSpecificExtensionDllAvailable;

bool IsTargetSpecificExtensionDllAvailable(BuildTargetPlatform platform)
{
	return (sTargetSpecificExtensionDllAvailable.find(platform) != sTargetSpecificExtensionDllAvailable.end());
}

ManagedMonoIsland MonoIsland::CreateManagedMonoIsland()
{
	vector<string> existing_dependencies;
	for (vector<std::string>::iterator k = dependencies.begin(); k !=  dependencies.end(); k++)
	{
		//Todo: decide if commenting out the IsFileCreated below is bad..
		if (CanFindAssembly(*k, targetPlatform))
			existing_dependencies.push_back(*k);
	}
	
	ManagedMonoIsland result;

	vector<string> files(paths.begin(),paths.end());
	result._defines = Scripting::StringVectorToMono(defines);
	result._files = Scripting::StringVectorToMono(files);
	result._classlibs_profile = MonoStringNew(classlibs_profile);
	result._references = Scripting::StringVectorToMono(existing_dependencies);
	result._output = MonoStringNew(assemblyPath);
	result._target = targetPlatform;	
	return result;
}

static string GetAssetPathFromScript (const Object& o)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	string path = pm.GetPathName (o.GetInstanceID ());
	path = pm.AssetPathNameFromAnySerializedPath (path);
	return path;
}

static set<int> GetDependentScriptAssemblies (int assemblyIndex)
{
	set<int> assemblies;
	for (int i=0;i<gAssemblyTargets.size();i++)
	{
		for (int d=0;d<gAssemblyTargets[i].dependencies.size();d++)
		{
			if (gAssemblyTargets[i].dependencies[d] == assemblyIndex)
				assemblies.insert(i);
		}
	}
	return assemblies;
}


void AddSupportedLanguage (const std::string& languageName, const std::string& extension)
{
	// Only allow one compiler per extension
	if( gSupportedExtensions.count(extension) )
		return;
	
	SupportedLanguage language;
	language.languageName = languageName;
	language.extension = ToLower(extension);
	
	gSupportedLanguages.push_back(language);
	gSupportedExtensions.insert (language.extension);
	
}

static void InitializeSupportedLanguages()
{
#if 1
	AddSupportedLanguage("CSharp", "cs");
	AddSupportedLanguage("Boo", "boo");
	AddSupportedLanguage("UnityScript", "js");
	
#else // Use CSharp to find all supported languages
	gSupportedLanguages.clear ();
	gSupportedExtensions.clear ();

	MonoMethod* method = FindStaticMonoMethod("UnityEditor.Scripting","ScriptCompilers","GetSupportedLanguageStructs");
	if (!method) return false;

	MonoArray* res = (MonoArray*) CallStaticMonoMethod(method,0);
	if (!res) return false;

	int langCount = mono_array_length_safe(res);
	
	for (int i = 0; i < langCount;i++)
	{
		MonoSupportedLanguage mlang = GetMonoArrayElement<MonoSupportedLanguage> (res, i);

		AddSupportedLanguage(MonoStringToCpp(mlang.languageName), MonoStringToCpp(mlang.extension));
	}
	return true;
#endif	
}

bool IsEditorOnlyScriptAssemblyIdentifier (const std::string& assemblyIdentifier)
{
	return assemblyIdentifier.find("Editor") != string::npos;
}

bool IsEditorOnlyScriptDllPath (const std::string& assemblyPath)
{
	return ToLower(assemblyPath).find("/editor/") != string::npos;
}

bool StringEndsWith (string const &fullString, string const &ending)
{
	if (fullString.length() < ending.length())
		return false;
	return fullString.compare (fullString.length() - ending.length(), ending.length(), ending) == 0;
}

bool IsEditorOnlyAssembly (const std::string& path, bool checkAssemblyTargets)
{
	vector_map<string, bool>::iterator found = gEditorAssembliesCache.find (path);
	if (found != gEditorAssembliesCache.end ())
		return found->second;
	
	if (checkAssemblyTargets)
	{
		for (int i = 0; i < gAssemblyTargets.size(); ++i)
		{
			if (StringEndsWith(path, gAssemblyTargets[i].assemblyName))
			{
				gEditorAssembliesCache[path] = true;
				return true;
			}

		}
	}

	if (StringEndsWith(path, kGraphsDllName))
	{
		gEditorAssembliesCache[path] = false;
		return false;
	}
	if (StringEndsWith(path, kGraphsEditorDllName))
	{
		gEditorAssembliesCache[path] = true;
		return true;
	}

	int count = GetEditorModuleCount();
	for (int i = 0; i < count; i++)
	{
		if (StringEndsWith(path, GetEditorModule(i).dllName))
		{
			gEditorAssembliesCache[path] = true;
			return true;
		}
	}

	ScriptingInvocation invocation("UnityEditor","AssemblyHelper","IsInternalAssembly");
	invocation.AddString(const_cast<string&>(path));
	if (MonoObjectToBool(invocation.Invoke()))
	{
		gEditorAssembliesCache[path] = true;
		return true;
	}

	gEditorAssembliesCache[path] = ToLower(path).find("/editor/") != string::npos;

	return gEditorAssembliesCache[path];
}

bool IsEditorOnlyAssembly (int index)
{
	// Compiled script dll's
	if (index < gAssemblyTargets.size())
	{
		return IsEditorOnlyScriptAssemblyIdentifier(gAssemblyTargets[index].assemblyName);
	}
	// Custom dll locations
	else
	{
		string path = GetMonoManager().GetAssemblyPath(index);

		return IsEditorOnlyAssembly(path, false);
	}
}

int GetBuildErrorIdentifier ()
{
	return GetEditorUserBuildSettings().GetInstanceID();
}

void CleanupMonoCompilationPipeline()
{
	{ SupportedExtensions temp; temp.swap(gSupportedExtensions); }
	{ AssemblyNameToIndex temp; temp.swap(gAssemblyNameToIndex); }
	{ SupportedLanguages temp; temp.swap(gSupportedLanguages); }
	{ AssemblyTargets temp; temp.swap(gAssemblyTargets); }
	{ AssemblyMask temp; temp.swap(gDirtyAssemblyCompileMask); }
	{ AssemblyMask temp; temp.swap(gAllUsedAssemblies); }
	{ AssemblyMask temp; temp.swap(gIslandsFailed); }
	{ vector<int> temp; temp.swap(gCompilerIdentifiers); }
	{ vector_map<string, bool> temp; temp.swap(gEditorAssembliesCache); }
}

static void InitializeAssemblyTargets ()
{
	gAssemblyTargets.clear ();
		
	InitializeSupportedLanguages();
	
	set<int> firstRuntimePhaseAssemblies;
	set<int> secondRuntimePhaseAssemblies;
	set<int> firstEditorPhaseAssemblies;
	set<int> secondEditorPhaseAssemblies;
	
	MonoManager& mm=GetMonoManager();
	
	// The number of assemblies is kScriptAssemblies (3) plus the number of supported languages times 4, the number of custom compile phases
	int maxAssemblies = MonoManager::kScriptAssemblies + gSupportedLanguages.size() * 4 ;
	
	mm.ResizeAssemblyNames(maxAssemblies); 
	gCompilerIdentifiers.resize(maxAssemblies);
	SetMonoImporterAssemblyCount(maxAssemblies);	

	for (int i=0;i<maxAssemblies;i++)
	{
		Object* object = NEW_OBJECT(NamedObject);
		object->Reset();
		object->AwakeFromLoad(kDefaultAwakeFromLoad);
		gCompilerIdentifiers[i] = object->GetInstanceID ();
		DestroySingleObject(object);
	}
	
	
	SupportedLanguage cslang;
	cslang.extension = "cs";
	cslang.languageName = "CSharp";
	
	/// UnityEngine.dll
	gAssemblyTargets.push_back (AssemblyTarget ());
	gAssemblyTargets.back ().language = cslang;
	gAssemblyTargets.back ().assemblyName = "UnityEngine.dll";
	gAssemblyTargets.back ().requiredPath.push_back(GetRuntimeExportFolder ());
	AssertIf (IsEditorOnlyAssembly(gAssemblyTargets.size()-1));

	/// UnityEditor.dll
	gAssemblyTargets.push_back (AssemblyTarget ());
	gAssemblyTargets.back ().language = cslang;
	gAssemblyTargets.back ().assemblyName = "UnityEditor.dll";
	gAssemblyTargets.back ().dependencies.push_back(MonoManager::kEngineAssembly);
	gAssemblyTargets.back ().requiredPath.push_back(GetEditorExportFolder ());
	AssertIf (!IsEditorOnlyAssembly(gAssemblyTargets.size()-1));

	/// Unity.Locator.dll
	gAssemblyTargets.push_back (AssemblyTarget ());
	gAssemblyTargets.back ().language = cslang;
	gAssemblyTargets.back ().assemblyName = "Unity.Locator.dll";

	//
	// Init list of assembly targets 
	//
	
	for(int i = 0; i < gSupportedLanguages.size(); i++)
	{
		// First runtime phase
		gAssemblyTargets.push_back (AssemblyTarget ());
		gAssemblyTargets.back ().language = gSupportedLanguages[i];
		gAssemblyTargets.back ().assemblyName = "Assembly-" + gSupportedLanguages[i].languageName + "-firstpass.dll";
		gAssemblyTargets.back ().requiredPath.push_back("/assets/plugins/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/standard assets/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/pro standard assets/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/iphone standard assets/");
		firstRuntimePhaseAssemblies.insert( gAssemblyTargets.size()-1 );
		AssertIf (IsEditorOnlyAssembly(gAssemblyTargets.size()-1));
		
		// normal scripts
		gAssemblyTargets.push_back (AssemblyTarget ());
		gAssemblyTargets.back ().language = gSupportedLanguages[i];
		gAssemblyTargets.back ().assemblyName = "Assembly-" + gSupportedLanguages[i].languageName + ".dll";
		secondRuntimePhaseAssemblies.insert( gAssemblyTargets.size()-1 );
		AssertIf (IsEditorOnlyAssembly(gAssemblyTargets.size()-1));

		/// Editor folder - first phase
		gAssemblyTargets.push_back (AssemblyTarget ());
		gAssemblyTargets.back ().language = gSupportedLanguages[i];
		gAssemblyTargets.back ().assemblyName = "Assembly-" + gSupportedLanguages[i].languageName + "-Editor-firstpass.dll";
		gAssemblyTargets.back ().requiredPath.push_back("/assets/standard assets/editor/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/pro standard assets/editor/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/iphone standard assets/editor/");
		gAssemblyTargets.back ().requiredPath.push_back("/assets/plugins/editor/");
		firstEditorPhaseAssemblies.insert( gAssemblyTargets.size()-1 );
		AssertIf (!IsEditorOnlyAssembly(gAssemblyTargets.size()-1));
		
		/// Editor folder - second phase
		gAssemblyTargets.push_back (AssemblyTarget ());
		gAssemblyTargets.back ().language = gSupportedLanguages[i];
		gAssemblyTargets.back ().assemblyName = "Assembly-" + gSupportedLanguages[i].languageName + "-Editor.dll";
		gAssemblyTargets.back ().requiredPath.push_back("editor/");
		secondEditorPhaseAssemblies.insert( gAssemblyTargets.size()-1 );
	
		AssertIf (!IsEditorOnlyAssembly(gAssemblyTargets.size()-1));
	}

	//
	// Set up list of dependencies
	//
	
	// 1st phase runtime scripts only depend on UnityEngine.dll:
	for(set<int>::iterator i = firstRuntimePhaseAssemblies.begin(); i != firstRuntimePhaseAssemblies.end(); i++)
	{
		gAssemblyTargets[*i].dependencies.push_back(MonoManager::kEngineAssembly);
	}
	// 2nd phase runtime scripts  depend on UnityEngine.dll plus all 1st phase scripts:
	for(set<int>::iterator i = secondRuntimePhaseAssemblies.begin(); i != secondRuntimePhaseAssemblies.end(); i++)
	{
		vector<int>	& dependencies=gAssemblyTargets[*i].dependencies;
		dependencies.push_back(MonoManager::kEngineAssembly);
		dependencies.insert(dependencies.end(),firstRuntimePhaseAssemblies.begin(),firstRuntimePhaseAssemblies.end());
	}
	// 1st phase editor scripts  depend on UnityEngine.dll and UnityEditor.dll plus all 1st phase runtime assemblies:
	for(set<int>::iterator i = firstEditorPhaseAssemblies.begin(); i != firstEditorPhaseAssemblies.end(); i++)
	{
		vector<int>	& dependencies=gAssemblyTargets[*i].dependencies;
		dependencies.push_back(MonoManager::kEngineAssembly);
		dependencies.push_back(MonoManager::kEditorAssembly);
		dependencies.insert(dependencies.end(),firstRuntimePhaseAssemblies.begin(),firstRuntimePhaseAssemblies.end());
	}
	// 2nd phase runtime scripts  depend on UnityEngine.dll  and UnityEditor.dll plus all previous runtime and editor phases:
	for(set<int>::iterator i = secondEditorPhaseAssemblies.begin(); i != secondEditorPhaseAssemblies.end(); i++)
	{
		vector<int>	& dependencies=gAssemblyTargets[*i].dependencies;
		dependencies.push_back(MonoManager::kEngineAssembly);
		dependencies.push_back(MonoManager::kEditorAssembly);
		dependencies.insert(dependencies.end(),firstRuntimePhaseAssemblies.begin(),firstRuntimePhaseAssemblies.end());
		dependencies.insert(dependencies.end(),secondRuntimePhaseAssemblies.begin(),secondRuntimePhaseAssemblies.end());
		dependencies.insert(dependencies.end(),firstEditorPhaseAssemblies.begin(),firstEditorPhaseAssemblies.end());
	}

	// Initialize assembly names
	mm.SetAssemblyName (MonoManager::kEngineAssembly, "UnityEngine.dll");
	mm.SetAssemblyName (MonoManager::kEditorAssembly, "UnityEditor.dll");
	mm.SetAssemblyName (MonoManager::kLocatorAssembly, "Unity.Locator.dll");

	for (int i=MonoManager::kScriptAssemblies;i<gAssemblyTargets.size();i++)
	{
		string assemblyName = gAssemblyTargets[i].assemblyName;
		gAssemblyNameToIndex[assemblyName] = i;
		mm.SetAssemblyName (i, assemblyName);
	}
}

AssemblyMask GetPrecompiledAssemblyMask ()
{
	AssemblyMask mask;
	int count = GetMonoManager().GetAssemblyCount();
	mask.resize(count, false);
	
	// Enable Editor +  engine .dll
	for (int i=0;i<MonoManager::kScriptAssemblies;i++)
		mask[i] = true;
	
	// Enable all custom dll's in project folder
	for (int i=gAssemblyTargets.size();i<count;i++)
	{
		if (IsFileCreated(GetMonoManager().GetAssemblyPath(i)))
		{
			mask[i] = true;
		}
	}
	
	return mask;
}

void SetMonoImporterAssemblyCount(int size)
{
	gDirtyAssemblyCompileMask.resize(size);
	gAllUsedAssemblies.resize(size);
	gIslandsFailed.resize(size);
}

inline bool IsEmptyIsland (const MonoIsland& island) { return island.paths.empty (); }

string GetDependencyAssemblyPathForBuildTarget (int index, bool buildingForEditor, BuildTargetPlatform platform )
{
	// Retrieve the right UnityEngine.dll to compile against
	if (index == MonoManager::kEngineAssembly && !buildingForEditor)
	{
		return GetUnityEngineDllForBuildTarget(platform);
	}
	else
		return GetMonoManager().GetAssemblyPath(index);
}

const char* GetProfileForCompatibilityLevel(int compatibilityLevel)
{
	switch (compatibilityLevel)
	{
		case kNET_Small:
			return "unity";
		case kNET_2_0:
			return "2.0";
		default:
			ErrorString("Unexpected compatilibitylevel");
			return GetProfileForCompatibilityLevel(kDefaultEditorCompatibilityLevel);
	}
}


static const char* GetProfileForBuild(BuildTargetPlatform platform, CompatibilityLevel compatibilityLevel)
{
	if (IsWebPlayerTargetPlatform(platform))
		return "unity_web";
	else
		return GetProfileForCompatibilityLevel(compatibilityLevel);
}


vector<string> GetDirectoriesGameCodeCanReferenceFromForBuildTarget(BuildTargetPlatform target)
{
	vector<string> result;
	CompatibilityLevel level = (CompatibilityLevel)GetPlayerSettings().GetAPICompatibilityLevel();
	
	result.push_back(GetMonoLibDirectory(target, GetProfileForBuild(target,level)));
	
	return result;
}


void DumpIslands(const char* msg, MonoIslands islands)
{
	printf_console("-------------------------\n");
	printf_console("%s", msg);
	printf_console("\n");
	for (MonoIslands::iterator i=islands.begin(); i != islands.end(); i++) {
		printf_console("Index: %i, editoronly: %i,Name: %s, pathsize: %u\n",i->assemblyIndex, static_cast<int> (IsEditorOnlyAssembly(i->assemblyIndex)), i->assemblyPath.c_str(),(int)i->paths.size());
	}
	printf_console("------------------------------\n");
}

// Initialize a set of mono islands based on assembly targets and output profile
static void InitializeMonoIslands (MonoIslands &islands, int options, bool buildingForEditor, BuildTargetPlatform platform)
{
	MonoManager& mm = GetMonoManager ();
	CompatibilityLevel compatibilityLevel = (CompatibilityLevel)GetPlayerSettings().GetAPICompatibilityLevel();

	islands.resize (gAssemblyTargets.size ());
	
	// Initialize the basic island setup
	for (int i=0;i<gAssemblyTargets.size ();i++)
	{
		islands[i].language = gAssemblyTargets[i].language;
		islands[i].assemblyIndex = i;

		islands[i].assemblyPath = GetScriptsTargetDllPathName(
									GetLastPathNameComponent(
										mm.GetAssemblyPath (i)));

		islands[i].targetPlatform = platform;

		const char* profile;
		if (IsEditorOnlyAssembly(i))
			profile = kMonoClasslibsProfile;
		else 
			profile = GetProfileForBuild(platform, compatibilityLevel);
		islands[i].classlibs_profile = profile;
		
		bool isDevelopmentBuild = (options & kCompileDevelopmentBuild) != 0;
		GetScriptCompilationDefines(isDevelopmentBuild, buildingForEditor, IsEditorOnlyAssembly(i), platform, islands[i].defines);
	}	
}

// Mark dirty assemblies and populate their script paths for compilation
static void MarkDirtyScriptAssemblies (MonoIslands &islands)
{
	MonoScriptManager& msm = GetMonoScriptManager();

	/// Build islands for project based scripts!
	MonoScriptManager::AllScripts scripts = msm.GetAllRuntimeAndEditorScripts ();
	for (MonoScriptManager::AllScripts::const_iterator i=scripts.begin ();i != scripts.end ();i++)
	{
		const MonoScript& script = **i;
		
		// Find the scripts assembly
		AssemblyNameToIndex::iterator found = gAssemblyNameToIndex.find (script.GetAssemblyName ());
		if (found == gAssemblyNameToIndex.end ())
		{
			// Warn that the used compiler is no longer available
			if (BeginsWith(script.GetAssemblyName (), "Assembly-"))
			{
				ErrorStringObject ("The compiler this script was imported with is not available anymore.", *i);
			}
			continue;
		}
		
		int assemblyIndex = found->second;
		
		// Compiler dll couldn't be loaded?
		if (assemblyIndex >= gAssemblyTargets.size())
			continue;
		
		// Compile the script if any scripts for the same compiler are dirty.
		if (gDirtyAssemblyCompileMask[assemblyIndex])
		{
			string path = GetAssetPathFromScript (script);
			if (!path.empty ())
				islands[assemblyIndex].paths.insert (path);
		}
		gAllUsedAssemblies[ assemblyIndex ]=true;
	}
	//DumpIslands("after setting paths", islands);

	gIslandsFailed &= gAllUsedAssemblies;
	
}

// Don't compile dll's that
// * have dependencies on a dll that we are going to compile right now.
// * that have compile errors right now
static void UnmarkInvalidScriptAssemblies (MonoIslands &islands, dynamic_bitset &assembliesDisabledDuringDependencyTracking)
{
	assembliesDisabledDuringDependencyTracking.resize(gAssemblyTargets.size(), false);
	
	for (int c=0;c<gAssemblyTargets.size ();c++)
	{
		// Early out since here since this doesn't need to compile anyway
		if (islands[c].paths.empty())
			continue;
		
		for (int d=0;d<gAssemblyTargets[c].dependencies.size();d++)
		{
			int dependency = gAssemblyTargets[c].dependencies[d];
			
			// Verify that we don't have any recursive dependencies setup
			// For example if first script compilation pass dependends on unityengine.dll, second script compilation pass will include both unityengine.dll and second script compilation pass
			// as opposed to actually leaving out unityengine.dll
			for (int rd=0;rd<gAssemblyTargets[dependency].dependencies.size();rd++)
			{
				Assert(count(gAssemblyTargets[c].dependencies.begin(), gAssemblyTargets[c].dependencies.end(), gAssemblyTargets[dependency].dependencies[rd]) == 1);
			}
			
			// Clear dependency if either of these is happening:
			// * Dependency has scripts it wants to compile
			// * Dependency has failed compilation
			// * Dependency is current compiling
			if (!islands[dependency].paths.empty() || gIslandsFailed[dependency] || IsCompiling (dependency))
			{
				// printf_console ("+++ Ignoring %s because it depends on %s\n", gAssemblyTargets[c].assemblyName.c_str(), gAssemblyTargets[dependency].assemblyName.c_str());
				assembliesDisabledDuringDependencyTracking[c] = true;
				break;
			}
		}
	}
	
	
	//DumpIslands("after removing islands that cannot be built yet",islands);
}

// Delete all .dll's that aren't used anymore
static void DeleteUnusedAssemblies ()
{
	MonoManager& mm = GetMonoManager ();
	
	set<string> deleteFiles;
	GetFolderContentsAtPath (kEditorAssembliesPath, deleteFiles);
	deleteFiles.erase(kCompilationCompletedFile);
	
	for (int i=0;i<mm.GetAssemblyCount();i++)
	{
		if (gAllUsedAssemblies[i])
		{
			string path = mm.GetAssemblyPath (i);
			deleteFiles.erase (path);
			deleteFiles.erase (MdbFile (path));
			deleteFiles.erase (PdbFile (path));
		}
	}
	for (set<string>::iterator i=deleteFiles.begin ();i != deleteFiles.end ();i++)
	{
		string deleteFileName = *i;
		DeleteFile (deleteFileName);
	}
}

// Populate dependencies for mono islands
static void AddDependencies (MonoIslands &islands, bool buildingForEditor, BuildTargetPlatform platform)
{
	// Initialize the dependencies & custom dll's
	for (int i=0;i<gAssemblyTargets.size ();i++)
	{
		islands[i].dependencies.reserve(gAssemblyTargets[i].dependencies.size());
		
		// Add implicit dependencies
		for (int d=0;d<gAssemblyTargets[i].dependencies.size();d++)
		{
			string path = GetDependencyAssemblyPathForBuildTarget (gAssemblyTargets[i].dependencies[d], buildingForEditor, platform);
			islands[i].dependencies.push_back(path);
		}
		
		// Add custom dll dependencies to all compiled assemblies
		if (i >= MonoManager::kScriptAssemblies)
		{
			for (int d=gAssemblyTargets.size ();d<gAllUsedAssemblies.size();d++)
			{
				// Don't add editor-script dll's as dependencies to runtime-script dll's
				if (!IsEditorOnlyAssembly(i) && IsEditorOnlyAssembly(d))
					continue;

				string path = GetDependencyAssemblyPathForBuildTarget (d, buildingForEditor, platform);
				islands[i].dependencies.push_back(path);
			}
		}
		
		// Add UnityEditor.dll to engine scripts but only when building inside of editor
		if (!IsEditorOnlyAssembly(i) && buildingForEditor && i >= MonoManager::kScriptAssemblies)
		{
			string path = GetMonoManager().GetAssemblyPath (MonoManager::kEditorAssembly);
			islands[i].dependencies.push_back(path);
		}
	}
}

static void ClearCompilingAndEmptyAssemblies (MonoIslands &islands, bool buildingForEditor, dynamic_bitset &assembliesDisabledDuringDependencyTracking)
{
	// Clear compiling editor assemblies, when building player
	if (!buildingForEditor)
	{
		for (int i=0;i<islands.size ();i++)
		{
			if (IsEditorOnlyAssembly(islands[i].assemblyIndex))
				islands[i].paths.clear();
		}
	}
	
	// Clear assemblies that have dependencies that have not completed compilation yet
	for (int i=0;i<islands.size ();i++)
	{
		if (assembliesDisabledDuringDependencyTracking[i])
			islands[i].paths.clear();
	}
	
	//DumpIslands("After clearing editorassemblies",islands);
	
	// Remove islands with nothing to compile
	erase_if (islands, IsEmptyIsland);	
}

// Update and return the set of dirty mono islands
///@TODO: This is currently changing state, and deleting files. We should turn this into two functions. One responsible for updating state.
/// The other one just returning a set of values like this function name implies!
static MonoIslands GetChangedMonoIslands (int options, bool buildingForEditor, BuildTargetPlatform platform)
{
	// printf_console ("Calculating changed assemblies\n");

	gAllUsedAssemblies = GetPrecompiledAssemblyMask();
	dynamic_bitset assembliesDisabledDuringDependencyTracking;

	MonoIslands islands;

	InitializeMonoIslands (islands, options, buildingForEditor, platform);
	AddDependencies (islands, buildingForEditor, platform);
	MarkDirtyScriptAssemblies (islands);
	UnmarkInvalidScriptAssemblies (islands, assembliesDisabledDuringDependencyTracking);
	DeleteUnusedAssemblies ();
	
	ClearCompilingAndEmptyAssemblies (islands, buildingForEditor, assembliesDisabledDuringDependencyTracking);
	
	// printf_console ("End Calculating changed assemblies\n");
	
	return islands;
}

// Populate the script paths of set of mono islands, regardless of dirtiness
static void PopulateAllScriptPaths (MonoIslands &islands)
{
	MonoScriptManager& msm = GetMonoScriptManager();
	
	MonoScriptManager::AllScripts scripts = msm.GetAllRuntimeAndEditorScripts ();
	for (MonoScriptManager::AllScripts::const_iterator i=scripts.begin ();i != scripts.end ();i++)
	{
		const MonoScript& script = **i;
		
		// Find the scripts assembly
		AssemblyNameToIndex::iterator found = gAssemblyNameToIndex.find (script.GetAssemblyName ());
		if (found == gAssemblyNameToIndex.end ())
		{
			// Warn that the used compiler is no longer available
			if (BeginsWith(script.GetAssemblyName (), "Assembly-"))
			{
				ErrorStringObject ("The compiler this script was imported with is not available anymore.", *i);
			}
			continue;
		}
		
		int assemblyIndex = found->second;
		
		// Compiler dll couldn't be loaded?
		if (assemblyIndex >= gAssemblyTargets.size())
			continue;
		
		string path = GetAssetPathFromScript (script);
		if (!path.empty ())
			islands[assemblyIndex].paths.insert (path);
	}
}

// Collect all mono islands and return as a managed MonoIsland[]
MonoArray* GetAllManagedMonoIslands ()
{
	MonoIslands islands;
	BuildTargetPlatform platform = GetEditorUserBuildSettings ().GetActiveBuildTarget ();
	InitializeMonoIslands (islands, 0, false, platform);
	PopulateAllScriptPaths (islands);
	AddDependencies (islands, false, platform);
	
	MonoManager &mm = GetMonoManager ();
	MonoClass *klass = mm.GetMonoClass ("MonoIsland", "UnityEditor.Scripting");
	MonoArray *arr = mono_array_new (mono_domain_get (), klass, islands.size());
	
	for (int i=0; i<islands.size (); ++i) {
		GetMonoArrayElement<ManagedMonoIsland> (arr, i) = islands[i].CreateManagedMonoIsland ();
	}
	
	return arr;
}

static vector<int> CalculateAllDependents (int assemblyIndex)
{
	vector<int> dependents;
	for (int i=0;i<gAssemblyTargets.size();i++)
	{
		for (int d=0;d<gAssemblyTargets[i].dependencies.size();d++)
		{
			if (gAssemblyTargets[i].dependencies[d] == assemblyIndex)
				dependents.push_back(i);
		}
	}

	return dependents;
}

static void CompilationFinishedAsync (bool success, int options, bool buildingForEditor, const MonoIsland& island, const MonoCompileErrors& errors)
{
	MonoManager& mm = GetMonoManager ();
	printf_console ("- Finished compile %s\n", mm.GetAssemblyPath (island.assemblyIndex).c_str());

	gIslandsFailed &= gAllUsedAssemblies;

	LogCompileErrors (errors, !buildingForEditor, island.assemblyIndex);
	
	// When a assembly successfully compiled we immediately replace it.
	// But we don't reload it until all scripts have successfully been compiled!
	if (success)
	{	
		string targetPath = mm.GetAssemblyPath (island.assemblyIndex);
		
		// UnityEngine.dll will only be built from cspreprocess.pl
		if (MoveReplaceFile (island.assemblyPath, targetPath))
		{
			std::string sourceMdb = MdbFile (island.assemblyPath);
			std::string targetMdb = MdbFile (targetPath);

			if (IsFileCreated (sourceMdb))
				MoveReplaceFile (MdbFile (island.assemblyPath), MdbFile (targetPath));
			else if (IsFileCreated (targetMdb))
				DeleteFile (targetMdb);

			std::string sourcePdb = PdbFile (island.assemblyPath);
			std::string targetPdb = PdbFile (targetPath);

			if (IsFileCreated (sourcePdb))
				MoveReplaceFile (sourcePdb, targetPdb);
			else if (IsFileCreated (targetPdb))
				DeleteFile (targetPdb);

			gIslandsFailed[island.assemblyIndex]=false;
		}
		else
			ErrorString ("Script compilation error: Couldn't replace " + targetPath);
		
		set<int> needRecompile = GetDependentScriptAssemblies(island.assemblyIndex);
		if (!needRecompile.empty())
		{
			for (set<int>::iterator d=needRecompile.begin();d != needRecompile.end();d++)
			{
				int dependentAssembly = *d;
				gDirtyAssemblyCompileMask [ dependentAssembly ] = true;
			}
			RecompileScriptsInternal (options, buildingForEditor, island.targetPlatform);
		}		
	}
	else
	{
		gIslandsFailed[island.assemblyIndex] = true;
	}
	
	// We are done compiling and have no compile errors left, mark monomanager accordingly
	if (!IsCompiling ())
		mm.SetHasCompileErrors (gIslandsFailed.any());

	for (int i = 0; i < gScriptCompilationFinishedCallbacks.size (); ++i)
		gScriptCompilationFinishedCallbacks[i] (success, errors);
}

void RegisterScriptCompilationFinishedCallback (ScriptCompilationFinishedCallback callback)
{
	gScriptCompilationFinishedCallbacks.push_back (callback);
}

bool ReloadAllUsedAssemblies ()
{
	if (IsCompiling ())
		return false;

	MonoManager& mm = GetMonoManager ();

	if (mm.HasCompileErrors ())
		return false;

#if UNITY_LOGIC_GRAPH
	ClearGraphCompilationErrors();
#endif

	BaseHierarchyProperty::ClearNameToScriptingClassMap ();

	if (mm.ReloadAssembly (gAllUsedAssemblies) == MonoManager::kEverythingLoaded)
	{
		SetPersistentIsCompilingFlag(false);
		return true;
	}
	else
		return false;
}

bool DoesProjectFolderHaveAnyScripts ()
{
	BuildTargetPlatform simulationPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
	MonoIslands islands = GetChangedMonoIslands (0, true, simulationPlatform);
	for (MonoIslands::iterator i=islands.begin ();i != islands.end ();i++)
	{
		// Extract the path name of all scripts in the island!
		MonoIsland& island = *i;
		
		// Early out when we dont need to compile anything
		if (!island.paths.empty())
			return true;
	}	
	
	return false;
}

bool UpdateMonoCompileTasks ()
{
	bool completedAnyTasks = UpdateMonoCompileTasks(false);

	if (completedAnyTasks && gIslandsFailed.none() && !IsCompiling())
		return true;
	else
		return false;
}

void RecompileScripts (int options, bool buildingForEditor, BuildTargetPlatform platform, bool showProgress /*= true*/)
{
	bool compiledAny = false;
	if (options & kCompileSynchronous)
	{
		if (showProgress)
			UpdateAssetProgressbar( 0.5F, "Hold on", kCompilingScriptsMessage );
		
		compiledAny |= RecompileScriptsInternal(options, buildingForEditor, platform);

		if (compiledAny)
			SetPersistentIsCompilingFlag (true);

		while (IsCompiling())
			UpdateMonoCompileTasks(true);
		
		if (showProgress)
			ClearAssetProgressbar();
	}
	else
	{
		compiledAny |= RecompileScriptsInternal(options, buildingForEditor, platform);
		if (compiledAny)
			SetPersistentIsCompilingFlag (true);
	}
	
	// No assemblies failed compilation, reload - synchronous codepath
	if (compiledAny && (options & kCompileSynchronous) && (options & kDontLoadAssemblies) == 0)
	{
		Assert(!IsCompiling());
		
		if (gIslandsFailed.none())
		{
			printf_console ("Reloading assemblies after successful script compilation.\n");
			ReloadAllUsedAssemblies ();
		}
	}
}

static void SetPersistentIsCompilingFlag (bool isCompiling)
{
	if (isCompiling)
		DeleteFile(kCompilationCompletedFile);
	else
		WriteStringToFile("Completed\n", kCompilationCompletedFile, kNotAtomic, kFileFlagDontIndex);
}

// This function will generate Assets/_SPAConfig.cs containing an enum with values from the specified spa.h
static bool ConvertSPAConfig(const string& srcFile, const string& dstFile)
{
	InputString src;
	if (!ReadStringFromFile(&src, srcFile))
		return false;

	File csFile;
	if (!csFile.Open(dstFile, File::kWritePermission))
		return false;

	std::map<std::string, std::string> valueMap;
	istringstream srcStream(src.c_str(), istringstream::in);
	string srcLine;
	while (std::getline(srcStream, srcLine))
	{
		istringstream parseLine(srcLine.c_str(), istringstream::in);
		string word0, word1, word2;
		parseLine >> word0;
		parseLine >> word1;
		parseLine >> word2;
		if (word0 == "#define" && !word1.empty() && !word2.empty())
		{
			valueMap[word1] = word2; // Overwrite previous values
		}
	}

	string content = "// Auto-generated by Unity\n";
	content += "// From " + srcFile + "\n\n";
	if (valueMap.size() > 0)
	{
		content += "public enum SPAConfig : uint\n";
		content += "{\n";
		for (std::map<std::string, std::string>::const_iterator it = valueMap.begin(); it != valueMap.end(); ++it)
		{
			content += "    " + it->first + " = " + it->second + ",\n";
		}
		content += "}\n";
	}
	else
	{
		content += "// Nothing to generate.\n";
	}
	content += "\n\n";

	csFile.Write(content.c_str(), content.length());
	csFile.Close();

	return true;
}

bool GenerateSPAConfig(string spaPath)
{
	spaPath += ".h"; // Use the .h file as source
	if (!IsFileCreated(spaPath.c_str()))
	{
		ErrorString("Could not generate _SPAConfig.cs: File does not exist: " + spaPath);
		return false;
	}
		
	string csPath = AppendPathName(GetProjectPath(), "Assets/_SPAConfig.cs");
	if (IsFileCreated(csPath.c_str()))
	{
		// Do not generate if source file is older
		DateTime spaDate = GetContentModificationDate(spaPath);
		DateTime csDate = GetContentModificationDate(csPath);
		if (spaDate < csDate)
		{
			return true;
		}
	}

	if (!ConvertSPAConfig(spaPath, csPath))
	{
		ErrorString("Could not generate " + csPath);
		return false;
	}

	return true;
}

static bool RegenerateUtilityScripts(int options, BuildTargetPlatform platform)
{
	if (platform == kBuildXBOX360)
	{
		// Generate _SPAConfig.cs
		const string& spaPath = GetPlayerSettings().GetEditorOnly().XboxSpaPath;
		if (spaPath.empty() || !GetPlayerSettings().GetEditorOnly().XboxGenerateSpa)
		{
			return true;
		}
		else
		{
			return GenerateSPAConfig(spaPath);
		}
	}
	else if (platform == kBuildPS3)
	{
		// TODO: handle the trophy file
	}

	return true;
}

static bool RecompileScriptsInternal (int options, bool buildingForEditor, BuildTargetPlatform platform)
{
	RegenerateUtilityScripts(options, platform);

	// Generate a vector of pathnames to all scripts that will be recompiled
	MonoIslands islands = GetChangedMonoIslands (options, buildingForEditor, platform);
	gDirtyAssemblyCompileMask.reset();

	bool compiledAny = false;
	// Recompile those that need recompilation
	for (MonoIslands::iterator i=islands.begin ();i != islands.end ();i++)
	{
		// Extract the path name of all scripts in the island!
		MonoIsland& island = *i;

		// Early out when we dont need to compile anything
		if (island.paths.empty())
			continue;
					
		printf_console ("- starting compile %s, for buildtarget %i\n", GetMonoManager ().GetAssemblyPath (island.assemblyIndex).c_str(), island.targetPlatform);
		int compileOptions = options;
		if (options & kCompileSynchronous)
			compileOptions |= kDontLoadAssemblies;
		
		// Compile files. This might re-enter RecompileScriptsInternal
		CompileFiles (&CompilationFinishedAsync, buildingForEditor, island, compileOptions);

		compiledAny = true;
	}
	
	GetMonoManager ().SetHasCompileErrors (gIslandsFailed.any());

	return compiledAny;
}

static inline MonoScript* GetScriptAtPath (const string& path)
{
	// The guid persistent manager allows only relative pathnames
	if (path.empty () || path[0] == '/')
		return NULL;

	///@TODO: The mono script will always be generated at the same file ID. no need for a loop here...
	set<SInt32> objects;
	GetGUIDPersistentManager ().GetInstanceIDsAtPath (GetMetaDataPathFromAssetPath (path), &objects);
	for (set<SInt32>::iterator i=objects.begin ();i != objects.end ();i++)
	{
		MonoScript* script = dynamic_instanceID_cast<MonoScript*> (*i);
		if (script)
			return script;
	}
	return NULL;
}

static int GetErrorIdentifierForAssembly (int assemblyIndex)
{
	AssertIf (assemblyIndex >= gCompilerIdentifiers.size());
	return gCompilerIdentifiers[assemblyIndex];
}

static void LogCompileErrors (const MonoCompileErrors& errors, bool buildingPlayerCompilation, int assemblyIndex)
{
	// Clear dependency errors
	int errorIdentifier;
	vector<int> errorDependencies = CalculateAllDependents(assemblyIndex);
	for (int i=0;i<errorDependencies.size();i++)
	{
		errorIdentifier = GetErrorIdentifierForAssembly (errorDependencies[i]);
		RemoveErrorWithIdentifierFromConsole (errorIdentifier);
	}
		
	// Clear old errors	
	errorIdentifier = GetErrorIdentifierForAssembly (assemblyIndex);
	RemoveErrorWithIdentifierFromConsole (errorIdentifier);
	
	// In the status bar we want to show the first compiler error not the last
	bool firstError = false;
	for (MonoCompileErrors::const_iterator i=errors.begin ();i != errors.end ();i++)
	{
		int mask = kLog | kDontExtractStacktrace;
		if (firstError)
			mask |= kDisplayPreviousErrorInStatusBar;
		if (i->type == MonoCompileError::kError)
		{
			mask |= kScriptCompileError;
			if (!buildingPlayerCompilation)
				mask |= kStickyError;
			firstError = true;
		}
		else
			mask |= kScriptCompileWarning;
		
		if (buildingPlayerCompilation)
			errorIdentifier = GetBuildErrorIdentifier ();
		
		MonoScript* script = GetScriptAtPath (i->file);
		
		DebugStringToFile (i->error, 0, i->file.c_str (), i->line, mask, script ? script->GetInstanceID() : 0, errorIdentifier);
	}
}
bool IsBlacklistedDllName (const string& assemblyPath)
{
	const string fileName = ToLower (GetLastPathNameComponent (assemblyPath));
	
	if (fileName == "unityengine.dll" ||
	    fileName == "mscorlib.dll")
		return true;

	return false;
}

void RegisterOneCustomDll(const string& path)
{
	if (StrICmp(GetPathNameExtension(path), "dll") == 0)
	{
		if (DetectDotNetDll(path))
			SetupCustomDll(GetLastPathNameComponent(path), path);
	}
}

string GetGraphsDllName()
{
	return kGraphsDllName;
}

void RegisterGraphsCustomDlls()
{
	string path;

#if UNITY_LOGIC_GRAPH
	path = AppendPathName(GetApplicationManagedPath(), kGraphsDllName);
	SetupCustomDll(GetLastPathNameComponent(path), path);
#endif

	path = AppendPathName(GetApplicationManagedPath(), kGraphsEditorDllName);
	SetupCustomDll(GetLastPathNameComponent(path), path);

#if UNITY_LOGIC_GRAPH
	ClearLogicGraphCompilationDirectory();
#endif
}

// Loads PlaybackEngine/EditorExtensions/Unity.PLATFORM_NAME.Extensions.dll
bool RegisterPlatformSupportModule(BuildTargetPlatform platform, const string& dllName)
{
	const std::string buildTools = GetPlaybackEngineDirectory(platform, 0, false);
	if (buildTools.empty())
		return false;

	std::string dllPath = AppendPathName(buildTools, "EditorExtensions");
	dllPath = AppendPathName(dllPath, dllName);
	
	if (IsFileCreated(dllPath))
	{
		printf_console("Register platform support module: %s\n", dllPath.c_str());
		sTargetSpecificExtensionDllAvailable.insert(platform);
		SetupCustomDll(GetLastPathNameComponent(dllPath), dllPath);
		return true;
	}
	else
	{
		printf_console("Platform support module is not available: %s\n", dllPath.c_str());
		return false;
	}
}

void RegisterEditorModuleDlls()
{
	sTargetSpecificExtensionDllAvailable.clear();
	BuildTargetSelection target = GetEditorUserBuildSettings().GetActiveBuildTargetSelection();

	int i;
	int count = GetEditorModuleCount();
	for (i = 0; i < count; ++i)
	{
		const EditorModuleEntry& module = GetEditorModule(i);
		if (IsBuildTargetSupported(module.targetPlatform))
		{
			RegisterPlatformSupportModule(module.targetPlatform, module.dllName);
		}
	}
}

void RegisterCustomDlls ()
{
	printf_console("Registering custom dll's ...\n");
	ABSOLUTE_TIME time = START_TIME;
	
	// We make sure that custom dll's are filled with all dll's.
	// This way users can place editor extensions that are active during startup.

	vector<string> allDlls;
	RecursiveFindFilesWithExtension("Assets", "dll", allDlls);
	
	for (int i=0;i<allDlls.size();i++)
	{
		const string& path = allDlls[i];

		// Skip if it's a DLL we use internally.
		if (IsBlacklistedDllName (path))
			continue;

		RegisterOneCustomDll(path);
	}
	
	RegisterGraphsCustomDlls();
	RegisterEditorModuleDlls();

	printf_console("Registered in %f seconds.\n", GetElapsedTimeInSeconds(time));
}

void SetupCustomDll (const std::string& dllName, const std::string& assemblyPath)
{
	MonoManager& mm=GetMonoManager();
	int index = mm.InsertAssemblyName(dllName);
	SetMonoImporterAssemblyCount(mm.GetAssemblyCount());
	gAllUsedAssemblies[index] = true;
	mm.SetCustomDllPathLocation(dllName, assemblyPath);
	AssertIf(!IsFileCreated(assemblyPath));
}

void InitializeRegisteredMonoScripts ()
{
	Assets::const_iterator end, i;
	i = AssetDatabase::Get().begin();
	end = AssetDatabase::Get().end();
	for (;i != end;i++)
	{
		const Asset& asset = i->second;
		if (asset.mainRepresentation.classID == ClassID(MonoScript))
		{	
			MonoScript* mainScript = dynamic_pptr_cast<MonoScript*> (asset.mainRepresentation.object);
			if (mainScript && mainScript->IsEditorScript())
				GetMonoScriptManager().RegisterEditorScript(*mainScript);
			else if (mainScript)
				GetMonoScriptManager().RegisterRuntimeScript(*mainScript);
		}
		for (int j=0;j<asset.representations.size();j++)
		{
			if (asset.representations[j].classID == ClassID(MonoScript))
			{
				MonoScript* repScript = dynamic_pptr_cast<MonoScript*> (asset.representations[j].object);
				if (repScript && repScript->IsEditorScript())
					GetMonoScriptManager().RegisterEditorScript(*repScript);
				else if (repScript)
					GetMonoScriptManager().RegisterRuntimeScript(*repScript);
			}
		}
	}
}

bool HasValidCachedScriptAssemblies ()
{
	// If Unity crashed in the middle of a compilation this will not exist.
	if (!IsFileCreated(kCompilationCompletedFile))
		return false;
	
	string engineDLLPath = GetMonoManager().GetAssemblyPath(MonoManager::kEngineAssembly);
	string editorDLLPath = GetMonoManager().GetAssemblyPath(MonoManager::kEditorAssembly);

	//  @TODO: Write the modification date of UnityEngine / UnityEditor into the text file
	
	// last compilation of scripts must be newer than engine / editor modification date
	DateTime compilationCompletedModDate = GetContentModificationDate (kCompilationCompletedFile);
	DateTime editorModDate = GetContentModificationDate (editorDLLPath);
	DateTime engineModDate = GetContentModificationDate (engineDLLPath);
	if (compilationCompletedModDate < engineModDate || compilationCompletedModDate < editorModDate)
		return false;
		

	// @TODO: Verify if all dll's are available by checking against all MonoScripts
	
	// Recompile all assemblies when script assemblies are gone (This is not a 100% robust solution, see todo)
	bool hasAnyScriptDlls = false;
	set<string> files;
	GetFolderContentsAtPath (kEditorAssembliesPath, files);
	for (set<string>::iterator i=files.begin ();i != files.end();i++)
	{
		if (StrICmp (GetPathNameExtension (*i), "dll") == 0)
			hasAnyScriptDlls = true;
	}
	
	return hasAnyScriptDlls;
}
		
bool IsExtensionSupportedByCompiler (const std::string& ext)
{
	return gSupportedExtensions.count (ext);
}

MonoManager::AssemblyLoadFailure LoadDomainAndAssemblies (AssemblyMask loadedAssemblyMask)
{
	MonoManager& mm = GetMonoManager();

	GlobalCallbacks::Get().initialDomainReloadingComplete.Register (InitializeManagedModuleManager);

	// Load all dlls that are cached in the script assemblies folder and any custom dlls in the project folder
	MonoManager::AssemblyLoadFailure failedLoading = mm.ReloadAssembly (loadedAssemblyMask);

	if (!CreateDirectoryRecursive (kEditorAssembliesPath))
		FatalErrorStringDontReport ("Couldn't create compiled assemblies folder! Out of disk space?");

	// Can't do anything about the engine assembly missing!
	if ( failedLoading == MonoManager::kFailedLoadingEngineOrEditorAssemblies )
		FatalErrorStringDontReport ("Failed loading engine script runtime!");

	return failedLoading;
}

void LoadMonoAssembliesOrRecompile ()
{
	// Find all MonoScripts in the project and inject them into the MonoManager
	InitializeRegisteredMonoScripts ();

	// Initialize assembly targets
	InitializeAssemblyTargets();
	RegisterCustomDlls();

	// Clear script dll cache based on modification date / last compilation having succeeded without compile errors.
	bool hasValidCachedScriptAssemblies = HasValidCachedScriptAssemblies ();
	if (!hasValidCachedScriptAssemblies)
		DeleteFileOrDirectory (kEditorAssembliesPath);

	AssemblyMask loadedAssemblyMask = GetMonoManager ().GetAvailableDllAssemblyMask();
	MonoManager::AssemblyLoadFailure failedLoading = LoadDomainAndAssemblies (loadedAssemblyMask);
	loadedAssemblyMask = GetMonoManager ().GetAvailableDllAssemblyMask();

	// - Script assembly couldn't load -> Recompile
	// - Cached script assemblies are out of sync
	if (failedLoading == MonoManager::kFailedLoadingScriptAssemblies || !hasValidCachedScriptAssemblies)
		gDirtyAssemblyCompileMask = ~GetPrecompiledAssemblyMask();
	
	///@TODO: Commit message: "Fix crash when hitting play without touching any scripts before doing it" revision: 36449
	// Try removing it. I can't see why we should need this.
	// Check that all dll's which are necessary have actually been loaded
	BuildTargetPlatform simulationPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
	GetChangedMonoIslands(0, true, simulationPlatform);
	if (gAllUsedAssemblies != loadedAssemblyMask)
		gDirtyAssemblyCompileMask |= ~GetPrecompiledAssemblyMask();			

	
	if (gDirtyAssemblyCompileMask.any())
	{
		///@TODO: WHat is the purpose of this?????
		GetChangedMonoIslands (0, true, simulationPlatform);
		RemoveErrorWithIdentifierFromConsole (GetMonoManager ().GetInstanceID ());
		RecompileScripts (kCompileSynchronous, true, simulationPlatform);
	}
}

// Find the assembly identifier
// Every compiler has required paths in which the script needs to reside.
// Since we have overlapping folders we need to find the one with the longest path name (the deepest folder)
string GetCompilerAssemblyIdentifier (const std::string& pathName)
{
	string extension = ToLower (GetPathNameExtension (pathName));
	string assemblyName;
	int highestPathDepth = -1;
	for (int i=MonoManager::kScriptAssemblies;i<gAssemblyTargets.size ();i++)
	{
		// extensions match
		if (extension != gAssemblyTargets[i].language.extension)
			continue;
		
		int pathDepth = -1;
		if (gAssemblyTargets[i].requiredPath.empty())
			pathDepth = 0;
		
		// search path matches
		for (int j=0;j<gAssemblyTargets[i].requiredPath.size();j++)
		{
			// Absolute
			if (gAssemblyTargets[i].requiredPath[j][0] == '/')
			{
				if (ToLower("/" + pathName).find(gAssemblyTargets[i].requiredPath[j]) == 0)
				{
					pathDepth = gAssemblyTargets[i].requiredPath[j].size();
				}
			}
			else
			{
				if (ToLower("/" + pathName).find("/" + gAssemblyTargets[i].requiredPath[j]) != string::npos)
					pathDepth = gAssemblyTargets[i].requiredPath[j].size();
			}
		}
		
		if (pathDepth > highestPathDepth)
		{
			assemblyName = gAssemblyTargets[i].assemblyName;
			highestPathDepth = pathDepth;
		}
	}
	return assemblyName;
}

void DirtyScriptCompilerByAssemblyIdentifier(const std::string& assemblyIdentifier)
{
	AssemblyNameToIndex::iterator found = gAssemblyNameToIndex.find(assemblyIdentifier);
	Assert(found != gAssemblyNameToIndex.end());
	gDirtyAssemblyCompileMask[found->second] = true;
}

static void ClearAllCompilerErrors()
{
	for (int i=0;i<gCompilerIdentifiers.size();i++)
		RemoveErrorWithIdentifierFromConsole (GetErrorIdentifierForAssembly (i));	
	
	gIslandsFailed.reset();
	GetMonoManager ().SetHasCompileErrors (false);
}

void ForceRecompileAllScriptsAndDlls (int assetImportFlags)
{
	// Needed to clear all state prior to recompile (For example when deleting the last script)
	ClearAllCompilerErrors ();

	// Recompile scripts
	DirtyAllScriptCompilers();

	MonoCompileFlags compileOptions = kCompileFlagsNone;
	if (assetImportFlags & kForceSynchronousImport)
		compileOptions = kCompileSynchronous;

	if (DoesProjectFolderHaveAnyScripts())
	{
		BuildTargetPlatform simulationPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
		RecompileScripts (compileOptions, true, simulationPlatform);
	}
	// Just reload assemblies since no scripts are in the project
	else
	{
		if (compileOptions == kCompileSynchronous)
		{
			printf_console ("Reloading assemblies after forced synchronous recompile.\n");
			ReloadAllUsedAssemblies();
		}
		else
			GetApplication().RequestScriptReload();
	}
}

void StartCompilation (int importFlags)
{
	// Force synchronous import?
	// Synchronous mode is used when importing assets on startup to prevent prefabs from being imported before all script's have been compiled.
	// Because we otherwise might lose serialized properties
	MonoCompileFlags options = kCompileFlagsNone;
	if (importFlags & kForceSynchronousImport)
		options = kCompileSynchronous;
	
	BuildTargetPlatform simulationPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
	RecompileScripts (options, true, simulationPlatform);
}


void DirtyAllScriptCompilers()
{
	gDirtyAssemblyCompileMask |= ~GetPrecompiledAssemblyMask();
}

void PopupCompilerErrors()
{
	if (gIslandsFailed.any())
		ShowErrorWithMode (kScriptCompileError);
}

void RebuildManagedEditorCode(int sync, bool recompileUserScripts, bool recompileEditorModules)
{
	UpdateAssetProgressbar(0.5f, "Hold on", "Recompiling Managed Editorcode");

	printf_console( "Recompiling Managed Editorcode\n" );

	GetMonoManager().UnloadAllAssembliesOnNextDomainReload();

	MonoArray* messages;
	void* args[] = { &messages, &recompileEditorModules };
	bool success = MonoObjectToBool(CallStaticMonoMethod("ManagedEditorCodeRebuilder", "Run", args));
		
	if (!messages)
	{
		ClearAssetProgressbar();
		ErrorString("Failed recompiling editorcode");
		return;
	}

	// Parse error messages
	MonoCompileErrors errors;
	ConvertManagedCompileErrorsToNative(messages,errors);
	LogCompileErrors(errors, false, 0);
	
	// Recompile user scripts
	if (recompileUserScripts && success)
	{
		DirtyAllScriptCompilers();
		BuildTargetPlatform simulationPlatform = GetEditorUserBuildSettings().GetActiveBuildTarget ();
		RecompileScripts (sync, true, simulationPlatform);
		
		// There are no scripts, thus a script reload will have no effect. Do it explicitly
		if (!DoesProjectFolderHaveAnyScripts())
			GetApplication().RequestScriptReload();
	}

	// Remove progress bar	
	ClearAssetProgressbar();
}


struct RecompileEngineMenu : public MenuInterface
{
	virtual bool Validate (const MenuItem &menuItem) { return true; }
	virtual void Execute (const MenuItem &menuItem)
	{ 
		if (menuItem.m_Command == "1")
			RebuildManagedEditorCode(0, true, false);
		else if (menuItem.m_Command == "2")
			CallStaticMonoMethod("AssembleEditorSkin", "Init");
		else if (menuItem.m_Command == "3")
			CallStaticMonoMethod("AssembleEditorSkin", "RegenerateAllIconsWithMipLevels");
		else if (menuItem.m_Command == "4")
			CallStaticMonoMethod("AssembleEditorSkin", "RegenerateSelectedIconsWithMipLevels");
		else if (menuItem.m_Command == "5")
			RebuildManagedEditorCode(0, true, true);
	}
};

void RecompileEngineRegisterMenu ();
void RecompileEngineRegisterMenu ()
{
	string unityFolder = GetBaseUnityDeveloperFolder();
	string monoExportFolder = AppendPathName (unityFolder, "Runtime/Export");
	if (IsDirectoryCreated (monoExportFolder))
	{
		RecompileEngineMenu* menu = new RecompileEngineMenu;
		MenuController::AddMenuItem ("Assets/Recompile Editor Code, Editor, TXT %k", "1", menu, 1000);

		string projectPath = AppendPathName (GetProjectPath(), "Assets/Editor Default Resources");
		printf_console ("%s\n", projectPath.c_str());
		if (IsDirectoryCreated(projectPath))
		{
			MenuController::AddMenuItem ("Tools/Regenerate Editor Skins...", "2", menu, 2000);
			MenuController::AddMenuItem ("Tools/Regenerate All Icons with Mips", "3", menu, 3000);
			MenuController::AddMenuItem ("Tools/Regenerate Selected Icons with Mips", "4", menu, 3001);
		}
		
		MenuController::AddMenuItem ("Assets/Recompile Editor Code and Modules %l", "5", menu, 1000);
	}	
}

STARTUP (RecompileEngineRegisterMenu)
