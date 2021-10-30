#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h" // include before anything, to get Prof_ENABLED if that is defined

#if ENABLE_MONO
#include "MonoManager.h"
#include "MonoScript.h"
#include "MonoBehaviour.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "MonoIncludes.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Export/MonoICallRegistration.h"
#include "Runtime/Utilities/Word.h"
#include <stdlib.h>
#include "Runtime/Misc/Plugins.h"
#include "Runtime/Threads/Mutex.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Profiler/ProfilerImpl.h"
#include "Runtime/Misc/PreloadManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Audio/AudioManager.h"
#include "Runtime/Utilities/Stacktrace.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/Misc/DeveloperConsole.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Scripting/Backend/Mono/ScriptingMethodFactory_Mono.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "Runtime/Scripting/Scripting.h"

#if UNITY_EDITOR
#include "Editor/Src/EditorModules.h"
#endif

#ifndef USE_ASSEMBLY_PREPROCESSOR
#define USE_ASSEMBLY_PREPROCESSOR 0
#endif

#if WEBPLUG
#include "PlatformDependent/CommonWebPlugin/CompressedFileStream.h"
#include "Runtime/Utilities/ErrorExit.h"
#define PLAYER_DATA_FOLDER ""
#endif
#if USE_ASSEMBLY_PREPROCESSOR
#include "AssemblyModifier.h"
#include "AssemblyModifierOnDisk.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/EditorSettings.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/BugReportingTools.h"
#include "Runtime/Utilities/Argv.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#define PLAYER_DATA_FOLDER ""
#endif
#if UNITY_OSX || UNITY_LINUX
#include <dlfcn.h>
#endif
#if UNITY_WIN
#include "Configuration/UnityConfigureOther.h"
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#include "PlatformDependent/Win/WinUtils.h"
#include <signal.h>
#endif
#if UNITY_WII
#include "PlatformDependent/wii/WiiUtility.h"
#endif
#if UNITY_OSX
#include <mach/task.h>
#include <sys/stat.h>
#endif
#ifdef _MSC_VER
#define va_copy(a,z) ((void)((a)=(z)))
#endif

#if ENABLE_PLAYERCONNECTION
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#endif

#ifndef PLAYER_DATA_FOLDER
#include "Runtime/Misc/Player.h"
#define PLAYER_DATA_FOLDER SelectDataFolder()
#endif

#define SUPPORT_MDB_FILES (UNITY_EDITOR || WEBPLUG)
#define USE_MONO_DOMAINS (UNITY_EDITOR || WEBPLUG) && !UNITY_PEPPER
// Turn off the extra domain for now. It doesn't buy us anything until
// we get some sharing in the runtime. Right now, it loads the assembly
// into the extra domain but the code will be jitted again in the child domain.
#define USE_TWO_MONO_DOMAINS 0

// Editor and development console players always load mdbs
#define ALWAYS_LOAD_MDBS (UNITY_EDITOR || ((UNITY_XENON || UNITY_WII || UNITY_PS3) && !MASTER_BUILD) || ENABLE_PROFILER)

#if UNITY_EDITOR
extern "C"
{
	void debug_mono_images_leak ();
}

static const char* kEditorAssembliesPath = "Library/ScriptAssemblies";

#endif // UNITY_EDITOR

#if ENABLE_MONO_MEMORY_PROFILER
void mono_profiler_startup ();
#endif

int* s_MonoDomainContainer = NULL;

UNITY_TLS_VALUE(bool) MonoManager::m_IsMonoBehaviourInConstructor;

#if UNITY_WIN && UNITY_WIN_ENABLE_CRASHHANDLER && !UNITY_WINRT

#define USE_WIN_CRASH_HANDLER 1
// We do cunning things with crash handler and mono - we disable
// it when mono is initialized (because mono installs its own handler),
// but setup a callback so that we can still launch bug reporter.
#include "../../Tools/BugReporterWin/lib/CrashHandler.h"
extern CrashHandler*	gUnityCrashHandler;

#else
#define USE_WIN_CRASH_HANDLER 0
#endif

static void UnloadDomain ();
static MonoDomain* CreateAndSetChildDomain();
static void UnhandledExceptionHandler (MonoObject* object);
double GetTimeSinceStartup ();

using namespace std;

static const char* kEngineAssemblyName = "UnityEngine.dll";
static const char* kEditorInternalNameSpace = "UnityEditorInternal";
#if MONO_2_12
const char* kMonoClasslibsProfile = "4.5";
#else
const char* kMonoClasslibsProfile = "2.0";
#endif

#if UNITY_STANDALONE || UNITY_EDITOR
static const char* gNativeLibsDir = NULL;
#endif

static MonoVTable** gClassIDToVTable = NULL;

ScriptsDidChangeCallback* gUnloadDomainCallback = NULL;

static bool gDebuggerEnabled = false;

#if UNITY_PEPPER
void *gCorLibMemory = NULL;
#endif

struct DomainReloadingData {
	std::vector<SInt32> m_SavedBehaviours;
	PPtr<MonoBehaviour> m_SavedScriptReloadProperties;
	ABSOLUTE_TIME reloadStart;
};

void RegisterUnloadDomainCallback (ScriptsDidChangeCallback* call)
{
	AssertIf(gUnloadDomainCallback != NULL);
	gUnloadDomainCallback = call;
}

static void ExtractMonoStacktrace (const std::string& condition, std::string& processedStackTrace, std::string& stackTrace, int errorNum, string& file, int* line, int type, int targetInstanceID);

#if UNITY_OSX
void HandleSignal (int i, __siginfo* info, void* p);
#define	UNITY_SA_DISABLE	0x0004	// disable taking signals on alternate stack
#endif
#if UNITY_WIN
int HandleSignal( EXCEPTION_POINTERS* ep );
void HandleAbort (int signal);
#endif

#if UNITY_EDITOR
static std::string GetBuildToolsEngineDllPathIfExists (BuildTargetPlatform target)
{
	string buildToolsDirectory = GetBuildToolsDirectory(target, false);
	if ( IsDirectoryCreated(buildToolsDirectory) )
	{
		string engineDLLPath = AppendPathName(buildToolsDirectory, kEngineAssemblyName);
		if (IsFileCreated(engineDLLPath))
			return engineDLLPath;
	}
	return "";
}
#endif // UNITY_EDITOR

#if UNITY_STANDALONE || UNITY_EDITOR
static void * mono_fallback_dlopen (const char* name, int flags, char **err, void *user_data)
{
	if (IsAbsoluteFilePath (name))
		return NULL;

	void* handle = NULL;
	string fullPath = AppendPathName (gNativeLibsDir, name);

	#if (UNITY_WIN && !UNITY_WINRT)
	std::wstring widePath;
	ConvertUnityPathName (fullPath, widePath);
	handle = (void *) LoadLibraryW (widePath.c_str ());

	#elif UNITY_OSX
	handle = dlopen (fullPath.c_str (), RTLD_NOW);

	#elif UNITY_XENON	
	handle = cached_module_load (fullPath.c_str (), MONO_DL_LAZY, 0);
	#endif

	if (!handle && err) {
		printf_console ("Fallback handler could not load library %s\n", fullPath.c_str());
	}
	return handle;
}

static void* mono_fallback_lookup_symbol (void *handle, const char *name, char **err, void *user_data)
{
	void* symbol = NULL;
	#if (UNITY_WIN && !UNITY_WINRT)
	symbol = GetProcAddress ((HMODULE)handle, name);
	
	#elif UNITY_OSX
	symbol = dlsym (handle, name);
	
	#elif UNITY_XENON
	char* ret = mono_dl_symbol ((MonoDl*)handle, name, &symbol);
	if (!symbol && err)
		*err = ret;
	#endif

	#if !UNITY_XENON
	if (!symbol && err) {
		printf_console ("Fallback handler could not load symbol %s\n", name);
	}
	#endif

	return symbol;
}

static void* mono_fallback_close (void *handle, void *user_data)
{
	#if (UNITY_WIN && !UNITY_METRO)
	FreeLibrary ((HMODULE) handle);

	#elif UNITY_OSX
	dlclose (handle);

	#endif

	return NULL;
}
#endif

MonoManager::MonoManager (MemLabelId label, ObjectCreationMode mode)
: ScriptingManager(label, mode, this, UNITY_NEW(ScriptingMethodFactory_Mono(), kMemManager))
{
	m_AssemblyReferencingDomain = NULL;

	#if UNITY_PLUGINS_AVAILABLE
	mono_set_find_plugin_callback ((gconstpointer)FindAndLoadUnityPlugin);
	#endif

	#if UNITY_EDITOR
	m_LogAssemblyReload = false;
	#endif

	#if UNITY_STANDALONE || UNITY_EDITOR
	gNativeLibsDir = strdup(GetApplicationNativeLibsPath().c_str ());
	mono_dl_fallback_register (mono_fallback_dlopen, mono_fallback_lookup_symbol, mono_fallback_close, NULL);
	#endif

	m_HasCompileErrors = false;

	CleanupClassIDMaps();
}

MonoManager::~MonoManager ()
{
	gClassIDToVTable = NULL;
	gClassIDToClass = NULL;
	RegisterLogPreprocessor (NULL);
}

AssemblyMask MonoManager::GetSystemAssemblyMask (bool load)
{
	AssemblyMask assemblies(kScriptAssemblies);
	assemblies[kEngineAssembly] = load;
	#if UNITY_EDITOR
	assemblies[kEditorAssembly] = load;
	assemblies[kLocatorAssembly] = load;
	#endif

	return assemblies;
}


AssemblyMask MonoManager::GetAvailableDllAssemblyMask ()
{
	AssemblyMask assemblies(GetAssemblyCount());
	assemblies[kEngineAssembly]=true;
	#if UNITY_EDITOR
	assemblies[kEditorAssembly]=true;
	assemblies[kLocatorAssembly]=true;
	#endif

	for (int i=kScriptAssemblies;i<GetAssemblyCount();i++)
	{
		string path = GetAssemblyPath (i);
		#if UNITY_XENON
		path += ".mono";
		#endif
		#if !UNITY_PEPPER
		if (IsFileCreated (path))
			assemblies[i] = true;
		#endif

		#if WEBPLUG
		if (CompressedFileStream::Get().Find(GetLastPathNameComponent(path)))
			assemblies[i] = true;
		#if UNITY_PEPPER
		if (CompressedFileStream::GetResources().Find(GetLastPathNameComponent(path)))
			assemblies[i] = true;
		#endif
		#endif
	}

	return assemblies;
}

void MonoManager::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);

	// Load assemblies in player
	// In editor MonoImporter Reloads the assemblies
	#if GAMERELEASE
	ReloadAssembly (GetAvailableDllAssemblyMask());
	#endif

#if ENABLE_SERIALIZATION_BY_CODEGENERATION
		ScriptingInvocation initManagedAnalysis(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","ManagedLivenessAnalysis","Init"));
		initManagedAnalysis.Invoke();

		ScriptingInvocation initWriter(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","SerializedStateWriter","Init"));
		initWriter.Invoke();

		ScriptingInvocation initReader(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","SerializedStateReader","Init"));
		initReader.Invoke();

		ScriptingInvocation initRemapper(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","PPtrRemapper","Init"));
		initRemapper.Invoke();
#endif

	// Disable Mono stacktrace in non-development (release) player builds
	#if UNITY_EDITOR || UNITY_DEVELOPER_BUILD
	RegisterLogPreprocessor (ExtractMonoStacktrace);	
	#endif
}

#if !UNITY_RELEASE
void MonoManager::AssertInvalidAssembly (MonoClass* klass)
{
	if (klass == NULL)
		return;

	MonoImage* image = mono_class_get_image(klass);
	if (image == mono_get_corlib())
		return;

	for (int i=0;i<m_ScriptImages.size();i++)
	{
//		if (m_ScriptImages[i])
//			printf_console("compare against image file %s\n", mono_image_get_filename(m_ScriptImages[i]));

		if (m_ScriptImages[i] == image)
			return;
	}
	printf_console("with error class %p \n", klass);
	printf_console("with name %s \n", mono_class_get_name(klass));
	printf_console("with image %p \n", image);
	printf_console("will image file name %s\n", mono_image_get_filename(image));
	printf_console("Mono class %s is in an invalid assembly %s ! BUG", mono_class_get_name(klass), mono_image_get_filename(image));
	#if UNITY_EDITOR
	ErrorString("Invalid assembly loaded");
	#endif
	printf_console("\n\n\n\n");
}
#endif

#if UNITY_EDITOR

void MonoManager::ResizeAssemblyNames(int max)
{
	m_AssemblyNames.clear();
	m_AssemblyNames.resize(max);
}

int MonoManager::InsertAssemblyName(const std::string& assemblyName)
{
	int index = GetAssemblyIndexFromAssemblyName(assemblyName);
	if (index != -1)
		return index;

	index = GetAssemblyCount();
	m_AssemblyNames.push_back(assemblyName);
	return index;
}

void MonoManager::SetAssemblyName (unsigned assemblyIndex, const string& name)
{
	Assert(assemblyIndex <= m_AssemblyNames.size ());
	m_AssemblyNames[assemblyIndex] = name;
}


void MonoManager::SetCustomDllPathLocation (const std::string& name, const std::string& path)
{
	if (m_CustomDllLocation[name] != path)
	{
		////@TODO: Create warnings for duplicate names
		m_CustomDllLocation[name] = path;
	}
}

/*
std::string MonoManager::AddCustomDll (const std::string& path)
{
	if (!DetectDotNetDll(path))
		return "";

	m_CustomDlls.insert(path);

	set<std::string> names;

	// Warn if a .dll name exists twice
	for (set<std::string>::iterator i=m_CustomDlls.begin();i != m_CustomDlls.end();i++)
	{
		const std::string& dllpath = *i;
		if (!IsFileCreated (dllpath))
			continue;

		if(!DetectDotNetDll(dllpath))
			continue;

		string name = GetLastPathNameComponent(dllpath);
		if (!names.insert (name).second)
			return Format("A .dll with name %s exists two times. The dll '%s' will be ignored.", name.c_str(), dllpath.c_str());
	}

	return "";
}
*/

void MonoManager::SetHasCompileErrors (bool compileErrors)
{
	m_HasCompileErrors = compileErrors;
}

#endif

#if UNITY_EDITOR
static int gMonoPathsIndexOfManagedFolder = 0;
//#else
//static int gMonoPathsIndexOfManagedFolder = 1;
#endif


namespace MonoPathContainer
{
	std::vector<std::string>* g_MonoPaths = NULL;
	void StaticInitialize(){ g_MonoPaths = UNITY_NEW(std::vector<std::string>,kMemMono);}
	void StaticDestroy(){UNITY_DELETE(g_MonoPaths,kMemMono);}
	std::vector<std::string>& GetMonoPaths() {return *g_MonoPaths;}
	void SetMonoPaths(const std::vector<std::string>& paths){*g_MonoPaths = paths;}
	void AppendMonoPath (const string& path)
	{
#if UNITY_WIN && !UNITY_WINRT
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName(path.c_str(), widePath, kDefaultPathBufferSize);
		
		wchar_t fullPath[kDefaultPathBufferSize];
		GetFullPathNameW(widePath, kDefaultPathBufferSize, fullPath, NULL);
		
		string unityPath;
		ConvertWindowsPathName(fullPath, unityPath);
#else
		string unityPath(path);
#endif
		MonoPathContainer::GetMonoPaths().push_back(unityPath);
	}
};

string MonoManager::GetAssemblyPath (int index)
{
	AssertIf (index < 0);

#if UNITY_EDITOR
	if (index == kEngineAssembly)
	{
		// Check if target specific assembly exists
		std::string engineDLLPath = GetBuildToolsEngineDllPathIfExists (GetEditorUserBuildSettings().GetActiveBuildTarget());
		if (!engineDLLPath.empty ())
		{
			return engineDLLPath;
		}
		else
		{
			return AppendPathName(MonoPathContainer::GetMonoPaths()[gMonoPathsIndexOfManagedFolder], m_AssemblyNames[index]);
		}
	}
	else if (index < kScriptAssemblies)
	{
		return AppendPathName(MonoPathContainer::GetMonoPaths()[gMonoPathsIndexOfManagedFolder], m_AssemblyNames[index]);
	}
	else if (index < m_AssemblyNames.size ())
	{
		CustomDllLocation::iterator found = m_CustomDllLocation.find(m_AssemblyNames[index]);
		if (found != m_CustomDllLocation.end())
			return found->second;
		else
			return AppendPathName (kEditorAssembliesPath, m_AssemblyNames[index]);
	}
	else
		return "";
#elif WEBPLUG
#if !UNITY_PEPPER
	if (index < kScriptAssemblies)
		return AppendPathName(MonoPathContainer::GetMonoPaths()[0], m_AssemblyNames[index]);
	else
#endif
	if (index < m_AssemblyNames.size ())
		return m_AssemblyNames[index];
	else
		return "";
#else
	return AppendPathName("Managed", m_AssemblyNames[index]);
#endif
}

MonoAssembly* MonoManager::GetAssembly (int index)
{
	AssertIf (index < 0);
	if (index < m_ScriptImages.size ())
	{
		MonoImage* image = m_ScriptImages[index];
		if (image)
			return mono_image_get_assembly (image);
	}
	return NULL;
}

BackendNativeType MonoManager::NativeTypeFor(const char* namespaze, const char* className)
{
	return GetMonoClass(className,namespaze);
}

ScriptingTypePtr MonoManager::Provide(BackendNativeType nativePtr)
{
	return (ScriptingTypePtr) nativePtr;
}

ScriptingTypePtr Provide(void* nativeType);

void MonoManager::Release(ScriptingTypePtr klass)
{
}

// If namespace is NULL then search is in any namespace
MonoClass* MonoManager::GetMonoClassCaseInsensitive (const char* className, const char* theNameSpace /*=NULL*/)
{
	for (ScriptImages::iterator i=m_ScriptImages.begin ();i != m_ScriptImages.end ();i++)
	{
		MonoImage* curImage = *i;
		if (!curImage)
			continue;
		
		MonoClass* klass = mono_class_from_name_case (curImage, theNameSpace, className);
		if (klass)
			return klass;
	}
	
	return NULL;
}

// If namespace is NULL then search is in any namespace
MonoClass* MonoManager::GetMonoClass (const char* className, const char* theNameSpace /*=NULL*/)
{
	MonoClass* klass = NULL;
	MonoImage* curImage;

	klass = mono_class_from_name(mono_get_corlib(), theNameSpace, className);

	///@todo: give compile error when classes with the same name are defined in different dll's
	for (ScriptImages::iterator i=m_ScriptImages.begin ();i != m_ScriptImages.end () && klass == NULL;i++)
	{
		curImage = *i;
		if (!curImage)
			continue;
		
		klass = mono_class_from_name (curImage, theNameSpace, className);
	}

	return klass;
}

int MonoManager::GetAssemblyIndexFromAssemblyName (const string& name)
{
	vector<UnityStr>::iterator found = find (m_AssemblyNames.begin (), m_AssemblyNames.end (), name);
	if (found == m_AssemblyNames.end ())
		return -1;

	return distance (m_AssemblyNames.begin (), found);
}

MonoClass* MonoManager::GetMonoClassWithAssemblyName (const std::string& className, const string& nameSpace, const string& assemblyName)
{
	int index = GetAssemblyIndexFromAssemblyName (assemblyName);
	MonoImage* image = 0;
	if (index == -1)
	{
		MonoAssemblyName aname;
#if !UNITY_PEPPER || UNITY_NACL_WEBPLAYER
		if (!mono_assembly_name_parse (assemblyName.c_str(),&aname))
			return NULL;
#endif
		MonoAssembly* assembly = mono_assembly_loaded (&aname);
		if (!assembly) return NULL;
		image = mono_assembly_get_image(assembly);
	}
	else
	{
		if (index >= m_ScriptImages.size ()) return NULL;
		image = m_ScriptImages[index];
	}

	if (!image) return NULL;

	return mono_class_from_name (image, nameSpace.c_str(), className.c_str ());
}

string MonoManager::GetAssemblyIdentifierFromImage(MonoImage* image)
{
	for (int i=0;i<m_ScriptImages.size ();i++)
	{
		if (m_ScriptImages[i] == image)
			return m_AssemblyNames[i];
	}
	return "";
}

int MonoManager::GetAssemblyIndexFromImage(MonoImage* image)
{
	for (int i=0;i<m_ScriptImages.size ();i++)
	{
		if (m_ScriptImages[i] == image)
			return i;
	}
	return -1;
}

#if UNITY_OSX
bool SameFileSystemEntry(const struct stat& s1, const struct stat& s2)
{
	return (s1.st_ino == s2.st_ino) && (s1.st_dev == s2.st_dev);
}
#endif

bool IsPlatformPath(const std::string& path)
{
	std::vector<string>& monoPaths = MonoPathContainer::GetMonoPaths();
#if UNITY_OSX
	// On OSX we want to take symlinks into account
	struct stat pathStat;
	if (stat(path.c_str(), &pathStat) != 0)
		return false;

	for (std::vector<string>::iterator i = monoPaths.begin(); i != monoPaths.end(); ++i)
	{
		struct stat platformStat;
		if (stat((*i).c_str(), &platformStat) == 0 && SameFileSystemEntry(pathStat, platformStat))
			return true;
	}
	return false;
#elif UNITY_WIN
	#if !UNITY_WINRT
	string unityPath = "";

	if (!path.empty())
	{
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName(path.c_str(), widePath, kDefaultPathBufferSize);

		wchar_t fullPath[kDefaultPathBufferSize];
		if (!GetFullPathNameW(widePath, kDefaultPathBufferSize, fullPath, NULL))
			fullPath[0] = L'\0';

		ConvertWindowsPathName(fullPath, unityPath);
	}

	std::vector<string>::iterator pos = std::find(monoPaths.begin(), monoPaths.end(), unityPath);
	return pos != monoPaths.end();
	#else
	#pragma message("todo: implement")	// ?!-
	return false;	// ?!-
	#endif
#else
	std::vector<string>::iterator pos = std::find(monoPaths.begin(), monoPaths.end(), path);
	return pos != monoPaths.end();
#endif
}

bool isPlatformCodeCallback(const char* image_name)
{
	std::string name(image_name);
	ConvertSeparatorsToUnity(name);

	bool result = IsPlatformPath(DeleteLastPathNameComponent(name));
	printf_console(result ? "Platform assembly: %s (this message is harmless)\n" : "Non platform assembly: %s (this message is harmless)\n", image_name);
	return result;
}

static MonoAssembly* LoadAssemblyAndSymbolsWrapper(const string& absolutePath)
{
	MonoAssembly* assembly = mono_domain_assembly_open (mono_domain_get (), absolutePath.c_str());
	if (assembly)
	{
		#if USE_MONO_DEBUGGER
		#if !ALWAYS_LOAD_MDBS
		if (gDebuggerEnabled)
		#endif
		{
			mono_debug_open_image_from_memory(mono_assembly_get_image(assembly), 0, 0);
		}
		#endif
	}
	return assembly;
}

static MonoAssembly* LoadAssemblyWrapper (const void* data, size_t size, const char* name)
{
	#if (UNITY_PS3 || UNITY_XENON)
	Assert(data == NULL);
	string absolutePath = PathToAbsolutePath (name);
	ConvertSeparatorsToPlatform (absolutePath);
	
	#if UNITY_XENON
		// Try loading the .DLL.MONO from UPDATE: drive first.
		// If it succeeds, mono will also load dependencies from the same path.
		Assert(absolutePath.substr(0, 5) == "game:");
		string absoluteUpdatePath = "update" + absolutePath.substr(4);
		MonoAssembly* updateAssembly = LoadAssemblyAndSymbolsWrapper(absoluteUpdatePath);
		if (updateAssembly)
			return updateAssembly;
	#endif
	
	MonoAssembly* assembly = LoadAssemblyAndSymbolsWrapper(absolutePath);
	return assembly;
	
	#else


	// We can't use mono_image_open_file because we continously replace the file when compiling new dll's.
	// But mono will keep the file locked, so we need to load it into memory.
	InputString dataString;

	string absolutePath = PathToAbsolutePath(name);
	ConvertSeparatorsToPlatform(absolutePath);
	
	if (data == NULL)
	{
		#if WEBPLUG
		return mono_domain_assembly_open (mono_domain_get (), absolutePath.c_str());
		#endif

		if (!ReadStringFromFile (&dataString, absolutePath))
			return NULL;

		data = &dataString[0];
		size = dataString.size ();
	}

	int status = 0;
	MonoImage* image = mono_image_open_from_data_with_name ((char*)data, size, /*Copy data*/true, &status, false /* ref only*/, absolutePath.c_str());
	if (status != 0 || image == NULL)
	{
		printf_console("Failed loading assembly %s\n", name);
		return NULL;
	}

	#if USE_MONO_DEBUGGER
	#if !ALWAYS_LOAD_MDBS
	if (gDebuggerEnabled)
	#endif
	{
		string absolutePathMdb = PathToAbsolutePath(MdbFile(name));
		ConvertSeparatorsToPlatform(absolutePathMdb);

		#if !UNITY_PEPPER
		if (ReadStringFromFile(&dataString, absolutePathMdb))
		{
			mono_debug_open_image_from_memory(image, &dataString[0], dataString.size ());
		}
		#endif
	}
	#endif

	/// try out true as last argument (loads reflection only should be faster)
	printf_console ("Loading %s into Unity Child Domain\n", absolutePath.c_str ());
	MonoAssembly* assembly = mono_assembly_load_from_full (image, absolutePath.c_str (), &status, false/* ref only*/);
	if (status != 0 || assembly == NULL)
	{
		// clear reference added by mono_image_open_from_data_with_name
		mono_image_close (image);

		printf_console("Failed loading assembly '%s'\n", name);
		return NULL;
	}

	// We are loading a new assembly and it is already loaded????
	if (mono_assembly_get_image(assembly) != image)
	{
		#if !UNITY_RELEASE
		LogString(Format("Already loaded assembly '%s'", mono_image_get_name(mono_assembly_get_image(assembly))));
		#endif
	}

	// clear reference added by mono_image_open_from_data_with_name
	mono_image_close (image);

	return assembly;
	#endif

}

void SetSecurityMode()
{
	bool enableCoreClr = false;
	bool enableVerifier = false;
	bool enableSocketSecurity = false;

#if WEBPLUG && !UNITY_PEPPER
	enableSocketSecurity = true;
	enableCoreClr = true;
	enableVerifier = true;
#endif

#if UNITY_EDITOR
	if (DoesBuildTargetUseSecuritySandbox(GetEditorUserBuildSettings().GetActiveBuildTarget ()))
	{
		enableSocketSecurity = true;
		enableVerifier = true;
	}
#endif

#if MONO_2_12
	mono_security_core_clr_set_options ((MonoSecurityCoreCLROptions)(MONO_SECURITY_CORE_CLR_OPTIONS_RELAX_REFLECTION | MONO_SECURITY_CORE_CLR_OPTIONS_RELAX_DELEGATE));
#endif
	mono_security_set_core_clr_platform_callback (&isPlatformCodeCallback);

#if !UNITY_PEPPER
	if (enableCoreClr)
	{
		mono_security_set_mode (MONO_SECURITY_MODE_CORE_CLR);
	} else {
		mono_security_set_mode (MONO_SECURITY_MODE_NONE);
	}

	if (enableVerifier)
		mono_verifier_set_mode (MONO_VERIFIER_MODE_VERIFIABLE);
	else
		mono_verifier_set_mode (MONO_VERIFIER_MODE_OFF);
#endif
	
	#if !UNITY_XENON && !UNITY_PS3 && !UNITY_PEPPER && !UNITY_WII
	mono_unity_socket_security_enabled_set (enableSocketSecurity);
	#endif
}


#define LogAssemblyError(x) DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kAssetImportError, 0, GetInstanceID ());
#if WEBPLUG



MonoAssembly *LoadAssemblyWebPlug (string path)
{
	MonoAssembly *assembly;
	CompressedFileStream::Data* data = CompressedFileStream::Get().Find(GetLastPathNameComponent(path));
#if UNITY_PEPPER
	if (!data)
		data = CompressedFileStream::GetResources().Find(GetLastPathNameComponent(path));
#endif
	if (data)
	{
		MemoryCacherReadBlocks cacher (data->blocks, data->end, kCacheBlockSize);
		UInt8* dataCpy;
		ALLOC_TEMP(dataCpy, UInt8, data->GetSize());
		cacher.DirectRead(dataCpy, data->offset, data->GetSize());
#if USE_ASSEMBLY_PREPROCESSOR
		bool modify = true;
		if (modify)
		{
			AssemblyModifier am;
			InputString output;
			bool success = am.PreProcessAssembly(dataCpy, data->GetSize(), output);
			if (success)
				assembly = LoadAssemblyWrapper (&output[0], output.size(), path.c_str ());
		}
		if (assembly==NULL)
#endif
		assembly = LoadAssemblyWrapper (dataCpy, data->GetSize(), path.c_str ());
		if (!assembly) return NULL;
#if !ALWAYS_LOAD_MDBS
		if (gDebuggerEnabled)
#endif
		{
			//see if there is an mdb too
			CompressedFileStream::Data* mdbdata = CompressedFileStream::Get().Find(GetLastPathNameComponent(MdbFile(path)));
		#if UNITY_PEPPER
			if (!mdbdata)
				mdbdata = CompressedFileStream::GetResources().Find(GetLastPathNameComponent(MdbFile(path)));
		#endif
			if (mdbdata)
			{
				MemoryCacherReadBlocks mdbcacher (mdbdata->blocks, mdbdata->end, kCacheBlockSize);
				UInt8* mdbdataCpy;
				ALLOC_TEMP(mdbdataCpy, UInt8, mdbdata->GetSize());
				mdbcacher.DirectRead(mdbdataCpy, mdbdata->offset, mdbdata->GetSize());

				MonoImage* image = mono_assembly_get_image(assembly);
				mono_debug_open_image_from_memory(image, (const char*)mdbdataCpy, mdbdata->GetSize());
			}
		}
	}
	else
		assembly = LoadAssemblyWrapper (NULL, 0, path.c_str ());
	return assembly;
}

MonoAssembly *AssemblyLoadHook (char **name, void* user_data)
{
	return LoadAssemblyWebPlug(string(*name)+".dll");
}
#endif

bool MonoManager::IsThisFileAnAssemblyThatCouldChange(std::string& path)
{
	std::string abspath = PathToAbsolutePath(path);
	ConvertSeparatorsToUnity(abspath);
	int startingScriptIndex = kScriptAssemblies;

#if UNITY_EDITOR
	if (IsDeveloperBuild())
		startingScriptIndex = kEngineAssembly;
#endif

	for (int i=startingScriptIndex; i<GetAssemblyCount(); i++)
	{
		std::string assemblypath = PathToAbsolutePath(GetAssemblyPath (i));
		ConvertSeparatorsToUnity(assemblypath);

		if (abspath == assemblypath)
			return true;
	}
	return false;
}



bool MonoManager::LoadAssemblies (AssemblyMask allAssembliesMask)
{
	bool firstLoad = false;
	bool failedLoadingSomeAssemblies = false;

	// Load assemblies
	for (int i = 0; i < GetAssemblyCount() && i < allAssembliesMask.size (); i++)
	{

		// Does the assembly have to be reloaded?
		if (allAssembliesMask[i])
		{
			if (m_ScriptImages.empty () || m_ScriptImages.size () <= i)
				m_ScriptImages.resize (max<int> (i + 1, m_ScriptImages.size ()));
			
			if (i >= kScriptAssemblies || m_ScriptImages[i] == NULL)
			{
				if (i < kScriptAssemblies)
					firstLoad = true;

				string path = GetAssemblyPath (i);
				m_ScriptImages[i] = NULL;

				// Load the script assembly, the script assembly might not be there
				// if we dont have scripts this is valid
				MonoAssembly* assembly = NULL;
				#if WEBPLUG
				assembly = LoadAssemblyWebPlug (path);
				#elif UNITY_EDITOR
				if ( i < kScriptAssemblies && !IsDeveloperBuild())
				{
					string s = PathToAbsolutePath (path);
					assembly = mono_domain_assembly_open (mono_domain_get (), s.c_str());
				}
				else
					assembly = LoadAssemblyWrapper (NULL, 0, path.c_str ());
				#else
					assembly = LoadAssemblyWrapper (NULL, 0, path.c_str ());
				#endif

				MonoImage* newImage = assembly ? mono_assembly_get_image (assembly) : NULL;

				m_ScriptImages[i] = newImage;
				if (newImage == NULL)
				{
					failedLoadingSomeAssemblies = true;
					LogAssemblyError ("Loading script assembly \"" + path + "\" failed!");
				}
			}
		}
		// Should we unload the assembly?
		else if (i < m_ScriptImages.size ())
		{
			m_ScriptImages[i] = NULL;
		}
	}

	if (firstLoad)
	{

#if !UNITY_PS3 && !UNITY_XENON && !UNITY_IPHONE
		for (int i = 0; i < m_ScriptImages.size (); i++)
		{
			if (m_ScriptImages[i])
			{
#if USE_ANCIENT_MONO
				if (!mono_assembly_preload_references (m_ScriptImages[i]))
				{
					printf_console ("Failed loading references %s\n", GetAssemblyPath (i).c_str ());
					m_ScriptImages[i] = NULL;
				}
#endif
			}
		}
		ScriptingInvocation ("UnityEngine","ClassLibraryInitializer","Init").Invoke ();
#endif
#if UNITY_IOS
		CallStaticMonoMethod ("UnhandledExceptionHandler", "RegisterUECatcher");
#endif

	}

	return failedLoadingSomeAssemblies;
}

#if UNITY_EDITOR
void MonoManager::SetupLoadedEditorAssemblies ()
{
	// Setup editor assemblies
	MonoClass* editorAssemblies = GetMonoClass("EditorAssemblies", "UnityEditor");
	if (editorAssemblies)
	{
		MonoClass* assemblyClass = mono_class_from_name (mono_get_corlib (), "System.Reflection", "Assembly");
		
		vector<MonoAssembly*> assemblies;
		assemblies.reserve(64);
		
		for (int i=0;i<GetAssemblyCount();i++)
		{
			if (GetAssembly(i) && i != kEngineAssembly)
				assemblies.push_back(GetAssembly(i));
		}
		
		MonoArray* array = mono_array_new (mono_domain_get (), assemblyClass, assemblies.size());
		for (int i=0;i<assemblies.size();i++)
		{
			Scripting::SetScriptingArrayElement(array, i, mono_assembly_get_object(mono_domain_get(), assemblies[i]));
		}
		
		ScriptingInvocation invocation(editorAssemblies,"SetLoadedEditorAssemblies");
		invocation.AddArray(array);
		invocation.Invoke();
	}
}
#endif


MonoManager::AssemblyLoadFailure MonoManager::ReloadAssembly (AssemblyMask allAssembliesMask)
{
	DomainReloadingData savedData;
	MonoManager::AssemblyLoadFailure initialLoad = BeginReloadAssembly (savedData);
	if (initialLoad == kFailedLoadingEngineOrEditorAssemblies)
		return initialLoad;

#if UNITY_EDITOR
	GlobalCallbacks::Get().initialDomainReloadingComplete.Invoke();
	allAssembliesMask = GetAvailableDllAssemblyMask();
#endif

	return EndReloadAssembly (savedData, allAssembliesMask);
}

MonoManager::AssemblyLoadFailure MonoManager::BeginReloadAssembly (DomainReloadingData& savedData)
{
	printf_console ("Begin MonoManager ReloadAssembly\n");
	
	// Make sure there are no preload operations queued up before we reload the domain.
	GetPreloadManager().WaitForAllAsyncOperationsToComplete();
	StopPreloadManager();

#if UNITY_EDITOR
	// if there's no script images on the list, no point in trying to stop, nothing has been loaded yet
	if (!m_ScriptImages.empty())
		ShutdownManagedModuleManager ();
#endif

	savedData.reloadStart = START_TIME;

	RemoveErrorWithIdentifierFromConsole (GetInstanceID ());

	#if GAMERELEASE
	if (!m_ScriptImages.empty())
		ErrorString("Reload Assembly may not be called multiple times in the player");
	#endif

	#if DEBUGMODE
	///@TODO: Also assert when the preload manager is running, write a test for it
	if (mono_method_get_last_managed ())
	{
		ErrorString("Reload Assembly called from managed code directly. This will cause a crash. You should never refresh assets in synchronous mode or enter playmode synchronously from script code.");
		return kFailedLoadingEngineOrEditorAssemblies;
	}
	#endif

	// Find and backup all Monobehaviours to the EditorExtensionImpl
	// If we are in play mode we use kForceSerializeAllProperties which will serialize private and public properties.
	// This makes it possible to reload script while in playmode
	// Otherwise we use the normal backup mechanism which doesnt persist private properties.

	Object::FindAllDerivedObjects (ClassID(MonoBehaviour), &savedData.m_SavedBehaviours, false);
	// sort all behaviours by script execution order (if set) and then instance ID, just like the persistent manager does
	std::sort (savedData.m_SavedBehaviours.begin (), savedData.m_SavedBehaviours.end (), AwakeFromLoadQueue::SortBehaviourByExecutionOrderAndInstanceID);
	MonoBehaviour* behaviour;

	#if UNITY_EDITOR

	// Allow static editor classes to store their state
	if (GetAssembly(kEditorAssembly) != NULL)
		CallStaticMonoMethod ("AssemblyReloadEvents", "OnBeforeAssemblyReload");
	// Store script reload properties (For example text editing state.)
	// Only store them if there are any other monobehaviours around, because when loading scripts for the first
	// time we don't want this because the resource manager has not yet registered the ScriptReloadProperties
	if (GetAssembly(kEditorAssembly) != NULL && !savedData.m_SavedBehaviours.empty ())
	{
		savedData.m_SavedScriptReloadProperties = ScriptingObjectToObject<MonoBehaviour> (CallStaticMonoMethod ("ScriptReloadProperties", "Store"));
		if (savedData.m_SavedScriptReloadProperties.IsValid ())
			savedData.m_SavedBehaviours.push_back(savedData.m_SavedScriptReloadProperties.GetInstanceID ());
	}

	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour> (savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;

		// Take backup just so that if we can load after reloading the assembly: Then we load the backup
		// If we can't load after reloading the assembly, We just keep the backup on the behviour..
		if (behaviour->GetInstance ())
		{
			Assert(behaviour->GetBackup() == NULL);
			
			int flags = kSerializeDebugProperties | kSerializeMonoReload;

			BackupState* backup = new BackupState();
			// Due to serialization to YAML not being super-fast, we backup all MonoBehaviours in binary mode
			// and we convert (when saving) the backup to YAML only when reload of a script fails.
			MonoBehaviour::ExtractBackupFromInstance (behaviour->GetInstance (), behaviour->GetClass (), *backup, flags);
			behaviour->SetBackup(backup);
		}

		// Release and unload.
		// Calling RemoveFromManager here makes sure
		if (behaviour->IsAddedToManager ())
			behaviour->RemoveFromManager ();
		else
			behaviour->WillUnloadScriptableObject();
	}

	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour> (savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;

		behaviour->StopAllCoroutines();
	}

	#endif //#if UNITY_EDITOR

	// Release all mono behaviours
	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour> (savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;

//		#if !UNITY_EDITOR
//		ErrorString(Format("MonoBehaviour not unloaded in web player unload %s instance: %d", behaviour->GetName().c_str(), behaviour->GetInstance()));
//		#endif

		// Take backup just so that if we can load after reloading the assembly: Then we load the backup
		behaviour->ReleaseMonoInstance ();
	}

	// Release all Monohandles
	vector<Object*> allObjects;
	Object::FindObjectsOfType (ClassID(Object), &allObjects);
	for (int i=0;i<allObjects.size();i++)
	{
		if (allObjects[i]->GetCachedScriptingObject() != NULL)
			allObjects[i]->SetCachedScriptingObject(NULL);
	}

	// Clear common scripting classes
	ClearCommonScriptingClasses (m_CommonScriptingClasses);
	m_ScriptingMethodRegistry->InvalidateCache();
	m_ScriptingTypeRegistry->InvalidateCache();

	#if ENABLE_PROFILER
	UnityProfiler::Get().CleanupMonoMethodCaches();
	#endif

#if !USE_TWO_MONO_DOMAINS
	AssemblyMask unloadMask = GetSystemAssemblyMask (false);
	LoadAssemblies (unloadMask);
#endif

	#if USE_MONO_DOMAINS

	PopulateAssemblyReferencingDomain();

	if (CreateAndSetChildDomain() == NULL)
	{
		return kFailedLoadingEngineOrEditorAssemblies;
	}

#if USE_TWO_MONO_DOMAINS
	if (m_AssemblyReferencingDomain == NULL)
		m_AssemblyReferencingDomain = mono_domain_create_appdomain("AssemblyReferencingDomain",NULL);
#endif

#endif
	
	SetSecurityMode();

	#if ENABLE_MONO_MEMORY_PROFILER
	UnityProfiler::Get().SetupProfilerEvents ();
	#endif

	// debug_mono_images_leak();

	AssemblyMask loadMask = GetSystemAssemblyMask (true);
	bool failedLoadingSomeAssemblies = LoadAssemblies(loadMask);

	mono_gc_collect (mono_gc_max_generation ());

	if (failedLoadingSomeAssemblies)
	{
		for (int i=0;i<m_ScriptImages.size();i++)
			m_ScriptImages[i] = NULL;
		CleanupClassIDMaps();
	}

	return failedLoadingSomeAssemblies ? kFailedLoadingEngineOrEditorAssemblies : kEverythingLoaded;
}

MonoManager::AssemblyLoadFailure MonoManager::EndReloadAssembly (const DomainReloadingData& savedData, AssemblyMask allAssembliesMask)
{
	MonoBehaviour* behaviour;

	bool failedLoadingSomeAssemblies = LoadAssemblies(allAssembliesMask);

	// Rebuild the m_ClassIDToMonoClass lookup table
	RebuildClassIDToScriptingClass ();
	RebuildCommonMonoClasses ();
	
#if UNITY_EDITOR

	// Rebuild all MonoScript classes cached info
	vector<MonoScript*> scripts;
	Object::FindObjectsOfType (&scripts);
	for (int i=0;i<scripts.size ();i++)
	{
		MonoScript&	script = *scripts[i];
		MonoClass* klass = GetMonoClassWithAssemblyName (script.GetScriptClassName (), script.GetNameSpace(), script.GetAssemblyName ());
		#if !UNITY_RELEASE
		AssertInvalidAssembly(klass);
		#endif

		script.Rebuild (klass);
	}

	// Rebuild all mono instances!
	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour>(savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;
		behaviour->RebuildMonoInstance (SCRIPTING_NULL);

		if (behaviour->GetGameObjectPtr())
			behaviour->GetGameObjectPtr ()->SetSupportedMessagesDirty ();
	}

	// Reload all MonoBehaviours from the Backup
	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour>(savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;

		BackupState* backup = behaviour->GetBackup ();

		// We now have an instance
		// So read the state from the behaviours own backup
		if (backup != NULL && behaviour->GetInstance ())
		{
			int flags = 0;
			if (!backup->loadedFromDisk)
				flags = kSerializeDebugProperties | kSerializeMonoReload;
			
			behaviour->RestoreInstanceStateFromBackup (*backup, flags);
			behaviour->SetBackup (NULL);
		}

		AssertIf (behaviour->GetInstance () != NULL && behaviour->GetBackup () != NULL);
	}
	

	// Add to manager and Awake from load comes last because we want everything to have the correct backup already loaded and ready.
	for (int i = 0; i < savedData.m_SavedBehaviours.size (); i++)
	{
		behaviour = PPtr<MonoBehaviour> (savedData.m_SavedBehaviours[i]);
		if (behaviour == NULL)
			continue;

		behaviour->CheckConsistency ();
		behaviour->AwakeFromLoad (kDefaultAwakeFromLoad);

		behaviour->DidReloadDomain();
	}
	
	/////////@TODO: Switch this to didReloadMonoDomain
	// Notify AudioScriptBufferManager to reload buffers
	if (GetAudioManager().GetScriptBufferManagerPtr())
		GetAudioManager().GetScriptBufferManager().DidReloadDomain();

	SetupLoadedEditorAssemblies ();

	GlobalCallbacks::Get().didReloadMonoDomain.Invoke();

	if (m_LogAssemblyReload)
	{
		LogString("Mono: successfully reloaded assembly\n");
	}
	else
	{
		printf_console ("Mono: successfully reloaded assembly\n");
	}
	#else
	ErrorIf(Object::FindAllDerivedObjects (ClassID(MonoBehaviour), NULL) != 0);
	ErrorIf(Object::FindAllDerivedObjects (ClassID(MonoScript), NULL) != 0);
	#endif

	#if UNITY_EDITOR

	// Allow static editor classes to reconstruct their state
	if (GetAssembly(kEditorAssembly) != NULL)
		CallStaticMonoMethod ("AssemblyReloadEvents", "OnAfterAssemblyReload");

	if (savedData.m_SavedScriptReloadProperties.IsValid ())
	{
		void* arg[] = { Scripting::ScriptingWrapperFor(savedData.m_SavedScriptReloadProperties) };
		CallStaticMonoMethod("ScriptReloadProperties", "Load", arg);

		DestroySingleObject (savedData.m_SavedScriptReloadProperties);
	}
	#endif

	printf_console("- Completed reload, in %6.3f seconds\n", GetElapsedTimeInSeconds(savedData.reloadStart));

	return failedLoadingSomeAssemblies ? kFailedLoadingScriptAssemblies : kEverythingLoaded;
}

void MonoManager::RebuildClassIDToScriptingClass ()
{
	ScriptingManager::RebuildClassIDToScriptingClass();
	int size = m_ClassIDToMonoClass.size();
	m_ClassIDToVTable.clear();
	m_ClassIDToVTable.resize (size,NULL);
	gClassIDToVTable = &m_ClassIDToVTable[0];

	for(int i=0; i!=size; i++)
	{
		if (m_ClassIDToMonoClass[i]==NULL) continue;
		m_ClassIDToVTable[i] = mono_class_vtable(mono_domain_get(), m_ClassIDToMonoClass[i]);
		
		AssertIf ((m_ClassIDToMonoClass[i] == NULL) != (m_ClassIDToVTable[i] == NULL));
	}
}

void MonoManager::CleanupClassIDMaps()
{
	m_ClassIDToMonoClass.clear ();
	m_ClassIDToVTable.clear ();
	gClassIDToVTable = &m_ClassIDToVTable[0];
	gClassIDToClass = SCRIPTING_NULL;

	memset(&m_CommonScriptingClasses, 0, sizeof(m_CommonScriptingClasses) );
}

MonoClass* MonoManager::GetBuiltinMonoClass (const char* name, bool optional)
{
	MonoClass* klass = NULL;
	if (m_ScriptImages[kEngineAssembly])
		klass = mono_class_from_name (m_ScriptImages[kEngineAssembly], "UnityEngine", name);

	if (klass)
		return klass;
	else if (!optional)
	{
		ErrorString (Format ("Mono Class %s couldn't be found! This might lead to random crashes later on!", name));
		return NULL;
	}
	else
	{
		return NULL;
	}
}


#if UNITY_EDITOR
MonoClass* MonoManager::GetBuiltinEditorMonoClass (const char* name)
{
	MonoClass* klass = NULL;
	if (m_ScriptImages[kEditorAssembly])
		klass = mono_class_from_name (m_ScriptImages[kEditorAssembly], "UnityEditor", name);

	if (klass)
		return klass;
	else
	{
		ErrorString (Format ("Mono Class %s couldn't be found! This might lead to random crashes later on!", name));
		return NULL;
	}
}
#endif

void MonoManager::RebuildCommonMonoClasses ()
{
	FillCommonScriptingClasses(m_CommonScriptingClasses);

	ScriptingMethodPtr setProject = m_CommonScriptingClasses.stackTraceUtilitySetProjectFolder;
	if (setProject)
	{
		MonoException* exception;
		std::string projectFolder = File::GetCurrentDirectory ();
		if( !projectFolder.empty() )
			projectFolder += kPathNameSeparator;
		ConvertSeparatorsToPlatform( projectFolder );
		
		void* values[1] = { MonoStringNew (projectFolder) };
		mono_runtime_invoke_profiled (setProject->monoMethod, NULL, values, &exception);
		if (exception)
			Scripting::LogException (exception, 0);
	}
}


MonoObject* MonoInstantiateScriptingWrapperForClassID(int classID)
{
	MonoVTable* vtable = gClassIDToVTable[classID];
	if (!vtable)
		return NULL;

	return mono_object_new_alloc_specific (vtable);
}
DOES_NOT_RETURN void RaiseDotNetExceptionImpl (const char* ns, const char* type, const char* format, va_list list)
{
	va_list ap;
	va_copy (ap, list);
	
	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, ap);
	va_end (ap);

	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), ns, type, buffer);
	mono_raise_exception (exception);

}

DOES_NOT_RETURN void RaiseSystemExceptionImpl (const char* type, const char* format, va_list list)
{
	va_list ap;
	va_copy (ap, list);
	RaiseDotNetExceptionImpl("System",type,format,ap);
}

void RaiseManagedException (const char *ns, const char *type, const char *format, ...)
{
	va_list va;
	va_start (va, format);
	RaiseDotNetExceptionImpl (ns, type, format, va);
}

void RaiseMonoException (const char* format, ...)
{
	va_list va;
	va_start( va, format );

	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, va);

	MonoException* exception = mono_exception_from_name_msg (GetMonoManager ().GetEngineImage (), kEngineNameSpace, "UnityException", buffer);
	mono_raise_exception (exception);
}

void RaiseOutOfRangeException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("IndexOutOfRangeException", format, va);
}

void RaiseNullException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("NullReferenceException", format, va);
}

void RaiseArgumentException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("ArgumentException", format, va);
}

void RaiseInvalidOperationException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseSystemExceptionImpl("InvalidOperationException", format, va);
}

void RaiseSecurityException (const char* format, ...)
{
	va_list va;
	va_start( va, format );
	RaiseDotNetExceptionImpl("System.Security","SecurityException", format, va);
}

void RaiseNullExceptionObject (MonoObject* object)
{
	#if MONO_QUALITY_ERRORS
	if (object)
	{
		MonoClass* klass = mono_object_get_class(object);
		if (mono_class_is_subclass_of (mono_object_get_class(object), GetMonoManager().ClassIDToScriptingClass(ClassID(Object)), false))
		{
			UnityEngineObjectMemoryLayout& data = ExtractMonoObjectData<UnityEngineObjectMemoryLayout>(object);
			string error = MonoStringToCpp(data.error);

			// The object was destroyed underneath us - but if we hit a MissingReferenceException then we show that instead!
			if (data.instanceID != 0 && error.find ("MissingReferenceException:") != 0)
			{
				error = Format("The object of type '%s' has been destroyed but you are still trying to access it.\n"
							"Your script should either check if it is null or you should not destroy the object.", mono_class_get_name(klass));

				MonoException* exception = mono_exception_from_name_msg (GetMonoManager().GetEngineImage(), "UnityEngine", "MissingReferenceException", error.c_str());
				mono_raise_exception (exception);
			}

			// We got an error message, parse it and throw it
			if (data.error)
			{
				error = MonoStringToCpp(data.error);
				string::size_type exceptionTypePos = error.find(':');
				if (exceptionTypePos != string::npos)
				{
					string type = string(error.begin(), error.begin() + exceptionTypePos);
					error.erase(error.begin(), error.begin()+exceptionTypePos + 1);

					MonoException* exception = mono_exception_from_name_msg (GetMonoManager().GetEngineImage(), "UnityEngine", type.c_str(), error.c_str ());
					mono_raise_exception (exception);
				}
			}
		}
	}

	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), "System", "NullReferenceException", "");
	mono_raise_exception (exception);
	#else
	MonoException* exception = mono_exception_from_name_msg (mono_get_corlib (), "System", "NullReferenceException", "");
	mono_raise_exception (exception);
	#endif
}

MonoObject* InstantiateScriptingWrapperForClassID(int classID)
{
	MonoVTable* vtable = gClassIDToVTable[classID];
	if (!vtable)
		return NULL;

	return mono_object_new_alloc_specific (vtable);
}

#if UNITY_EDITOR
bool AmIBeingDebugged();
#endif


#if UNITY_EDITOR
extern "C" {
	void *mono_trace_parse_options         (char *options);
	extern void *mono_jit_trace_calls;
}
#endif

#if USE_MONO_DOMAINS

void MonoManager::UnloadAllAssembliesOnNextDomainReload()
{
#if USE_TWO_MONO_DOMAINS
	if (m_AssemblyReferencingDomain != NULL)
		mono_domain_unload(m_AssemblyReferencingDomain);
	m_AssemblyReferencingDomain = NULL;
#endif
}

void MonoManager::PopulateAssemblyReferencingDomain()
{
#if USE_TWO_MONO_DOMAINS
	//if unityeditor isn't loaded yet (first time), return, since we depend on some managed code to do our work.
	if (m_ScriptImages.empty())
		return;

	if (m_AssemblyReferencingDomain == NULL)
	{
		printf_console("Not loading any assemblies in assemblyreferencingdomain, since the domain doesn't exist\n");
		return;
	}

	MonoException* exc = NULL;
	printf_console("- GetNamesOfAssembliesLoadedInCurrentDomain\n");
	ScriptingArrayPtr result = (ScriptingArrayPtr) CallStaticMonoMethod("AssemblyHelper","GetNamesOfAssembliesLoadedInCurrentDomain",NULL,&exc);
	
	if (exc!=NULL)
	{
		ErrorString("Failed calling GetNamesOfAssembliesLoadedInCurrentDomain");
		return;
	}

	for (int i=0; i!= GetScriptingArraySize(result); i++)
	{
		string filename(ScriptingStringToCpp(Scripting::GetScriptingArrayElement<MonoString*>(result,i)));

		if (IsThisFileAnAssemblyThatCouldChange(filename))
			continue;

		printf_console("- loading assembly into m_AssemblyReferencingDomain: %s\n",filename.c_str());
		MonoAssembly* a = mono_domain_assembly_open(m_AssemblyReferencingDomain,filename.c_str());
		if (a == NULL)
			ErrorString(Format("Failed to load assembly: %s into temporary domain\n",filename.c_str()));
	}
#endif
}

MonoDomain* CreateAndSetChildDomain()
{
	MonoDomain* old_domain = mono_domain_get();

	// Unload the old domain
	UnloadDomain ();

	//there should only ever be one child domain, so if we're making a new one,
	//we better be in the rootdomain.
	AssertIf(mono_get_root_domain() != mono_domain_get());
	
	MonoDomain* newDomain = mono_domain_create_appdomain("Unity Child Domain", NULL);

	AssertIf(mono_get_root_domain() != mono_domain_get());

	// Activate the domain!
	if (newDomain)
	{
		mono_thread_push_appdomain_ref(newDomain);

		if (!mono_domain_set (newDomain, false))
		{
			printf_console("Exception setting domain\n");
			return NULL;
		}
	}
	else
	{
		printf_console("Failed to create domain\n");
		return NULL;
	}

	
	AssertIf(mono_domain_get () == mono_get_root_domain());
	AssertIf(mono_domain_get () == NULL);
	AssertIf(mono_domain_get () == old_domain);
	UNUSED(old_domain);

	MonoDomainIncrementUniqueId();

	return mono_domain_get ();
}


static void UnloadDomain ()
{
	if (gUnloadDomainCallback)
		gUnloadDomainCallback();

	ClearLogCallback ();
	
	//make sure to be in the domain you want to unload.
	MonoDomain* domainToUnload = mono_domain_get();

	//never unload the rootdomain
	if (domainToUnload && domainToUnload != mono_get_root_domain())
	{
		// you can only unload a domain when it's not the active domain, so we're going to switch to the rootdomain,
		// so we can kill the childdomain.
		if (!mono_domain_set(mono_get_root_domain(), false))
		{
			printf_console("Exception setting domain\n");
		}
		mono_thread_pop_appdomain_ref();
		mono_domain_unload(domainToUnload);
	}

	//unloading a domain is also a nice point in time to have the GC run.
	mono_gc_collect(mono_gc_max_generation());

#if UNITY_PLUGINS_AVAILABLE
	//@TODO: Have Mono release its DLL handles (see case 373275).  Until that is the case,
	//	this is disabled as it results in a change of behavior (plugin functions can get called multiple
	//  times in the editor which as long as we don't actually release the DLLs doesn't really make
	//  sense).
	//UnloadAllPlugins ();
#endif
}
#endif

#if UNITY_OSX && !UNITY_IPHONE && !UNITY_PEPPER
void SetupSignalHandler(int signal)
{
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = HandleSignal;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(signal, &action, NULL) < 0)
		printf_console("Error setting signal handler for signal %d\n",signal);
	}
#endif

void SetupSignalHandlers()
{

#if UNITY_OSX
#	if	WEBPLUG
	//Remove possible existing mach exception handlers which interfere with our exception handling (ie Firefox)
	mach_port_t current_task = mach_task_self();
	exception_mask_t exceptionMask = EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC | EXC_MASK_BREAKPOINT;
	task_set_exception_ports(current_task, exceptionMask, 0, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
#	endif

#	if	!UNITY_IPHONE && !UNITY_PEPPER
	SetupSignalHandler(SIGSEGV);
	SetupSignalHandler(SIGBUS);
	SetupSignalHandler(SIGFPE);
	SetupSignalHandler(SIGILL);
#endif
#endif

// Make sure abort() calls get passed to the crash handler
#if UNITY_WIN
	_set_abort_behavior (0, _WRITE_ABORT_MSG);
	signal (SIGABRT, HandleAbort);
#endif

#if UNITY_WIN || UNITY_OSX || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
	//this causes mono to forward the signal to us, if the signal is not mono related. (i.e. not a breakpoint, or a managed nullref exception, etc)
	mono_set_signal_chaining(1);
#endif
}

bool ShouldGiveDebuggerChanceToAttach()
{
#if SUPPORT_ENVIRONMENT_VARIABLES
	if (getenv("UNITY_GIVE_CHANCE_TO_ATTACH_DEBUGGER"))
		return true;
#endif
	return false;
}

void GiveDebuggerChanceToAttachIfRequired()
{
	if (ShouldGiveDebuggerChanceToAttach())
	{
#if UNITY_WIN && !UNITY_XENON && !UNITY_WINRT
		MessageBoxW(0,L"You can attach a debugger now if you want",L"Debug",MB_OK);
#endif
#if UNITY_OSX
		CFStringRef msg = CFSTR ("You can attach a debugger now if you want");
		CFOptionFlags responseFlags;
		CFUserNotificationDisplayAlert (0.0, kCFUserNotificationPlainAlertLevel, NULL, NULL, NULL, msg, NULL, NULL, NULL, NULL, &responseFlags);
#endif
	}
}

void StoreGlobalMonoPaths (const std::vector<string>& monoPaths)
{
#if UNITY_WIN && !UNITY_WINRT

	MonoPathContainer::GetMonoPaths().reserve(monoPaths.size());

	for (vector<string>::const_iterator it = monoPaths.begin(); it != monoPaths.end(); ++it)
	{
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName(it->c_str(), widePath, kDefaultPathBufferSize);

		wchar_t fullPath[kDefaultPathBufferSize];
		GetFullPathNameW(widePath, kDefaultPathBufferSize, fullPath, NULL);

		string unityPath;
		ConvertWindowsPathName(fullPath, unityPath);

		MonoPathContainer::GetMonoPaths().push_back(unityPath);
	}

#else
	MonoPathContainer::SetMonoPaths(monoPaths);
#endif
}

static std::string BuildMonoPath (const std::vector<string>& monoPaths)
{
#if UNITY_WIN
	const char* separator = ";";
#else
	const char* separator = ":";
#endif
	
	string monoPath;
	for (int i=0;i!=monoPaths.size();i++)
	{
		if (i!=0) monoPath.append(separator);
		monoPath.append(monoPaths[i]);
	}

	return monoPath;
}

static void SetupMonoPaths (const std::vector<string>& monoPaths, const std::string& monoConfigPath)
{
	// Mono will not look for the loaded Boo assembly if the version is different.
	// It will look in the mono path and next to the loading dll.
	// In web player Boo.Lang.dll is in different location than the rest of mono,
	// hence we setup multiple assembly paths.
	for (int i = 0; i < monoPaths.size(); i++)
		printf_console("Mono path[%d] = '%s'\n", i, monoPaths[i].c_str());
	printf_console ("Mono config path = '%s'\n", monoConfigPath.c_str ());
	mono_set_dirs (monoPaths[0].c_str(), monoConfigPath.c_str());	

#if !UNITY_PEPPER
	mono_set_assemblies_path(BuildMonoPath (monoPaths).c_str ());
#endif

	StoreGlobalMonoPaths (monoPaths);
}

static void InitializeMonoDebugger (bool forceEnable=false)
{
	// Set up debugger
	string monoOptions;

#if MONO_2_12
	const string defaultDebuggerOptions = "--debugger-agent=transport=dt_socket,embedding=1,server=y,suspend=n";
#else
	const string defaultDebuggerOptions = "--debugger-agent=transport=dt_socket,embedding=1,defer=y";
#endif

#if UNITY_EDITOR
	if (EditorPrefs::GetBool ("AllowAttachedDebuggingOfEditor", true))
	{
	// Editor always listens on default port
		monoOptions = defaultDebuggerOptions;
	}
#endif

#if SUPPORT_ENVIRONMENT_VARIABLES
	// MONO_ARGUMENTS overrides everything
	char* args = getenv("MONO_ARGUMENTS");
	if (args!=0)
	{
		monoOptions = args;
	}
#endif
#if ENABLE_PLAYERCONNECTION && !UNITY_FLASH
	// Allow attaching using player connection discovery
	if (monoOptions.empty ())
	{
		PlayerConnection::Initialize (PLAYER_DATA_FOLDER, forceEnable);
		if (PlayerConnection::Get ().AllowDebugging ()) {
			// Must be synchronized with Unity SDB addin
			unsigned debuggerPort = 56000 + (PlayerConnection::Get ().GetLocalGuid () % 1000);
			monoOptions = defaultDebuggerOptions + Format (",address=0.0.0.0:%u", debuggerPort);
		}
	}
#endif


	if (!monoOptions.empty ())
	{
		char *optionArray[] = { (char*)monoOptions.c_str () };
		printf_console ("Using monoOptions %s\n", monoOptions.c_str ());
		mono_jit_parse_options(1, optionArray);
		gDebuggerEnabled = true;
	}

#if !ALWAYS_LOAD_MDBS
	if (gDebuggerEnabled)
#endif
	{
		//this is not so much about debugging (although also required for debugging),  but it's about being able to load .mdb files, which you need for nice stacktraces, lineinfo, etc.
		mono_debug_init (1);
	}
}

void MonoPrintfRedirect(const char* msg, va_list list)
{
	va_list args;
	va_copy (args, list);
	printf_consolev(LogType_Debug,msg,args);
	va_end (args);
}

#if UNITY_NACL_WEBPLAYER
extern "C" {
	void mono_set_pthread_suspend(int (*_pthread_suspend)(pthread_t thread, int sig));
	int pthreadsuspend_initialize(void);
	int pthread_suspend(pthread_t thread, int sig);
}
#endif

bool InitializeMonoFromMain (const std::vector<string>& monoPaths, string monoConfigPath, int argc, const char** argv, bool enableDebugger)
{
	s_MonoDomainContainer = UNITY_NEW_AS_ROOT(int,kMemMono, "MonoDomain", "");
	SET_ALLOC_OWNER(s_MonoDomainContainer);

	MonoPathContainer::StaticInitialize();

	const char *emptyarg = "";
	GiveDebuggerChanceToAttachIfRequired();
	

	#if DEBUGMODE
	printf_console( "Initialize mono\n" );
	#endif	

// Debug helpers
#if 0
	mono_unity_set_vprintf_func(MonoPrintfRedirect);

	//setenvvar ("MONO_DEBUG", "abort-on-sigsegv");
	//setenvvar ("MONO_LOG_MASK", "all");
	//setenvvar ("MONO_LOG_LEVEL", "debug");
	//setenvvar("MONO_TRACE","Console.Out:++++ ");

	// Tracing mask 
	#if UNITY_EDITOR
	mono_trace_set_mask_string("all"); // "asm", "type", "dll", "gc", "cfg", "aot", "all",
	mono_trace_set_level_string("debug");// "error", "critical", "warning", "message", "info", "debug"
	//mono_jit_trace_calls = mono_trace_parse_options ("Console.Out:++++ ");
	#endif
#endif

	SetupMonoPaths (monoPaths, monoConfigPath);

	mono_config_parse (NULL);

	SetupSignalHandlers();

	mono_set_defaults (0, mono_parse_default_optimizations (NULL));

#if USE_MONO_DEBUGGER
	InitializeMonoDebugger (enableDebugger);
#endif

	if (argv==NULL) argv = &emptyarg;
#if !UNITY_XENON && ! UNITY_PS3 && !UNITY_IPHONE  && !UNITY_PEPPER && !UNITY_WII
	mono_set_commandline_arguments(argc,argv, NULL);
#endif

#if UNITY_NACL
#if UNITY_NACL_WEBPLAYER
	pthreadsuspend_initialize ();
	
	mono_set_pthread_suspend (pthread_suspend);
#endif
	
	CompressedFileStream::Data* data = CompressedFileStream::GetResources().Find("mscorlib.dll");
	if (data == NULL)
	{
		ErrorString ("Could not find mscorlib in web stream!");
		return false;
	}

	MemoryCacherReadBlocks cacher (data->blocks, data->end, kCacheBlockSize);
	gCorLibMemory = (UInt8*)UNITY_ALLOC(kMemMono, data->GetSize(), 32);
	cacher.DirectRead(gCorLibMemory, data->offset, data->GetSize());
	mono_set_corlib_data (gCorLibMemory, data->GetSize());

	mono_install_assembly_postload_search_hook (AssemblyLoadHook, NULL);

	mono_jit_parse_options(argc, (char**)argv);
#endif


//	mono_runtime_set_no_exec (true);
#if MONO_2_12
	MonoDomain* domain = mono_jit_init_version ("Unity Root Domain", "v4.0.30319");
#else
	MonoDomain* domain = mono_jit_init_version ("Unity Root Domain", "v2.0.50727");
#endif
	if (domain == NULL)
		return false;

#if WEBPLUG && (!UNITY_PEPPER || UNITY_NACL_WEBPLAYER)
	mono_set_ignore_version_and_key_when_finding_assemblies_already_loaded(true);
#endif

	// The soft debugger needs this
	mono_thread_set_main (mono_thread_current ());

#if ENABLE_MONO_MEMORY_PROFILER
	//mono_gc_base_init ();  ask joachim why we need this.

	mono_profiler_startup ();
#endif

#if !UNITY_XENON && !UNITY_PS3 && !UNITY_IPHONE && !UNITY_PEPPER && !UNITY_WII
	mono_unity_set_embeddinghostname("Unity");
#endif

#if !UNITY_PEPPER
	mono_runtime_unhandled_exception_policy_set (MONO_UNHANDLED_POLICY_LEGACY);
#endif
	
	RegisterAllInternalCalls();
	
	return true;
}

#if !UNITY_WII && !UNITY_PS3 && !UNITY_XENON && !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_PEPPER && !UNITY_LINUX && !UNITY_BB10 && !UNITY_TIZEN

#if UNITY_OSX

#include <dlfcn.h>
#include <cxxabi.h>

void PrintStackTraceOSX (void* context)
{
	void *array[256];
	size_t size;
	char **strings;
	size_t i;

	size = mono_backtrace_from_context (context, array, 256);

	printf_console ("Obtained %zu stack frames.\n", size);

	for (i = 0; i < size; i++)
	{
		const char* symbolName = mono_pmip(array[i]);
		if (!symbolName)
		{
			Dl_info dlinfo;
			dladdr(array[i], &dlinfo);
			symbolName = dlinfo.dli_sname;
			if (symbolName)
			{
				int status = 0;
				char* cname = abi::__cxa_demangle(symbolName, 0, 0, &status);
				if (status == 0 && cname)
					symbolName = cname;
			}
		}

		printf_console ("#%-3d%016p in %s\n", i, array[i], symbolName);
	}

	free (strings);
}


void HandleSignal (int i, __siginfo* info, void* p)
#endif
#if UNITY_WIN
int __cdecl HandleSignal( EXCEPTION_POINTERS* ep )
#endif
{
	printf_console("Receiving unhandled NULL exception\n");

	#if UNITY_EDITOR

		// ---- editor

		#if UNITY_OSX
		printf_console("Launching bug reporter\n");
		PrintStackTraceOSX(p);
		LaunchBugReporter (kCrashbug);

		#elif UNITY_WIN

		winutils::ProcessInternalCrash(ep, false);

		if( gUnityCrashHandler )
		{
			printf_console( "unity: Launch crash handler\n" );
			return gUnityCrashHandler->ProcessCrash( ep );
		}
		return EXCEPTION_EXECUTE_HANDLER;

		#else
		#error "Unknown platform"
		#endif

	#else
	// ---- player

	// We are hitting this from inside a mono method, so lets just keep going!
	if (mono_thread_current() != NULL && mono_method_get_last_managed() != NULL)
	{
		#if WEBPLUG
		ExitWithErrorCode(kErrorFatalException);
		#endif

		#if UNITY_OSX
		abort();
		#endif

		#if UNITY_WIN
		return EXCEPTION_EXECUTE_HANDLER;
		#endif

	}
	else
	{
		// we have a serious exception - launch crash reporter
		#if UNITY_WIN && USE_WIN_CRASH_HANDLER

		winutils::ProcessInternalCrash(ep, false);

		if( gUnityCrashHandler ) {
			printf_console( "unity: Launch crash handler\n" );
			return gUnityCrashHandler->ProcessCrash( ep );
		}
		#endif

		#if WEBPLUG
		ExitWithErrorCode(kErrorFatalException);
		#endif

		#if UNITY_OSX
		abort();
		#endif

		#if UNITY_WIN
		return EXCEPTION_EXECUTE_HANDLER;
		#endif
	}
	#endif
}

#endif // !UNITY_WII && !UNITY_PS3 && !UNITY_XENON && !UNITY_IPHONE && !UNITY_ANDROID && !UNITY_PEPPER && !UNITY_LINUX

#if UNITY_WIN
void __cdecl HandleAbort (int signal)
{
	printf_console ("Received abort signal from the operating system\n");
	#if UNITY_EDITOR
	LaunchBugReporter (kFatalError);
	#endif
}
#endif

void UnhandledExceptionHandler (MonoObject* object)
{
	printf_console("Receiving unhandled exception\n");

	#if UNITY_EDITOR
	LaunchBugReporter (kFatalError);
	#elif WEBPLUG

	#else
	#endif
}

#if USE_MONO_DOMAINS
bool CleanupMonoReloadable ()
{
	
	UnloadDomain();
	return true;
}
#endif

void CleanupMono ()
{
	#if DEBUGMODE
	printf_console( "Cleanup mono\n" );
	#endif

	RegisterLogPreprocessor (NULL);

#if USE_MONO_DOMAINS
	UnloadDomain();
#endif

	mono_runtime_set_shutting_down ();
#if !UNITY_PEPPER
	mono_threads_set_shutting_down ();
	mono_thread_pool_cleanup ();
	mono_thread_suspend_all_other_threads ();
#endif
	mono_jit_cleanup(mono_get_root_domain());

	#if UNITY_PEPPER
	UNITY_FREE (kMemMono, gCorLibMemory);
	#endif

	#if UNITY_OSX
	struct sigaction sa;
	sa.sa_sigaction = NULL;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = UNITY_SA_DISABLE;
	sigaction (SIGSEGV, &sa, NULL);

	sa.sa_sigaction = NULL;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = UNITY_SA_DISABLE;
	sigaction (SIGABRT, &sa, NULL);
	#endif

	MonoPathContainer::StaticDestroy();

	UNITY_DELETE(s_MonoDomainContainer,kMemMono);
}

void PostprocessStacktrace(const char* stackTrace, std::string& processedStackTrace)
{
	if (GetMonoManagerPtr () && GetMonoManager ().GetCommonClasses ().postprocessStacktrace)
	{
		MonoException* exception = NULL;
		int stripEngineInternalInformation = UNITY_RELEASE;
		void* arg[] = { MonoStringNew(stackTrace), &stripEngineInternalInformation };


		MonoString* msTrace = (MonoString*)mono_runtime_invoke_profiled (GetMonoManager ().GetCommonClasses ().postprocessStacktrace->monoMethod, NULL, arg, &exception);
		if (exception)
		{
			printf_console ("Failed to postprocess stacktrace\n");
			return;
		}
		processedStackTrace = MonoStringToCpp (msTrace);
	}
}

static void ExtractMonoStacktrace (const std::string& condition, std::string& processedStackTrace, std::string& stackTrace, int errorNum, string& file, int* line, int type, int targetInstanceID)
{
#if UNITY_WII
	// When debugger is running on Wii, mono stack extraction fails for unknown reasons
	// But if running without debugger it works fine
	if (wii::IsDebuggerPresent()) return;
#endif

#if SUPPORT_THREADS
	const bool isMainThread = Thread::EqualsCurrentThreadID(Thread::mainThreadId);
#else
	const bool isMainThread = true;
#endif
	if (isMainThread && mono_thread_current() != NULL && mono_method_get_last_managed () != NULL && (type & kDontExtractStacktrace) == 0)
	{
		MonoClass* stackTraceUtil = GetMonoManager ().GetMonoClass ("StackTraceUtility", kEngineNameSpace);
		if (stackTraceUtil)
		{
			void* iter = NULL;
			MonoMethod* method;
			while ((method = mono_class_get_methods (stackTraceUtil, &iter))) {
				const char* curName = mono_method_get_name (method);
				if (strcmp ("ExtractStackTrace", curName) == 0)
					break;
			}

			if (method)
			{
				MonoException* exception = NULL;
				MonoString* msTrace = (MonoString*)mono_runtime_invoke_profiled (method, NULL, NULL, &exception);
				if (exception)
				{
					printf_console ("Failed to extract mono stacktrace from Log message\n");
					return;
				}

				stackTrace = MonoStringToCpp (msTrace);
				if (!stackTrace.empty ())
				{
					int oldLine = *line;
					string oldFile = file;
					ExceptionToLineAndPath (stackTrace, *line, file);
					if (!(type & kMayIgnoreLineNumber))
						stackTrace = Format ("%s\n[%s line %d]", stackTrace.c_str (), oldFile.c_str(), oldLine);

					PostprocessStacktrace(stackTrace.c_str(), processedStackTrace);
				}
			}
		}
	}
}

MonoMethod* FindStaticMonoMethod (MonoImage* image, const char* className, const char* nameSpace, const char* methodName)
{
	MonoClass* klass = mono_class_from_name(image,nameSpace,className);
	if (!klass)	return NULL;

	MonoMethod* method = mono_class_get_method_from_name(klass,methodName,-1);
	if (!method) return NULL;

	return method;
}

MonoMethod* FindStaticMonoMethod (const char* nameSpace, const char* className, const char* methodName)
{
	MonoClass* klass = GetMonoManager ().GetMonoClass (className, nameSpace);
	if (!klass)
		return NULL;
	return mono_class_get_method_from_name (klass, methodName, -1);
}

MonoMethod* FindStaticMonoMethod (const char* className, const char* methodName)
{
	MonoClass* klass = GetMonoManager ().GetMonoClass (className, kEngineNameSpace);
	if (!klass)
		klass = GetMonoManager ().GetMonoClass (className, kEditorNameSpace);
	if (!klass)
		klass = GetMonoManager ().GetMonoClass (className, kEditorInternalNameSpace);
	if (!klass)
		return NULL;

	return mono_class_get_method_from_name (klass, methodName, -1);
}

std::string MdbFile (const std::string& path)
{
	return AppendPathNameExtension (path, "mdb");
}
std::string PdbFile (const std::string& path)
{
	int f = path.find(".dll");
	return AppendPathNameExtension (f != -1 ? path.substr(0, f) : path, "pdb");
}	

void ClearLogCallback ()
{
#if UNITY_HAS_DEVELOPER_CONSOLE
	// By default redirect log output to the developer console
	RegisterLogCallback(DeveloperConsole_HandleLogFunction, true);
#else
	RegisterLogCallback(NULL, false);
#endif // UNITY_HAS_DEVELOPER_CONSOLE
}

template<class TransferFunction>
void MonoManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion(2);

	#if UNITY_EDITOR
	if (transfer.IsSerializingForGameRelease ())
	{
		transfer.Transfer (m_MonoScriptManager.m_RuntimeScripts, "m_Scripts");
		
		//flash exports monomanager in a special way, it only requires m_Scripts
		if (transfer.IsWritingGameReleaseData () && transfer.GetBuildingTarget().platform==kBuildFlash)
			return;
		
		transfer.Transfer (m_AssemblyNames, "m_AssemblyNames");
	}
	else if (transfer.GetFlags() & kPerformUnloadDependencyTracking)
	{
		transfer.Transfer (m_MonoScriptManager.m_RuntimeScripts, "m_Scripts");
	}

	#else
	transfer.Transfer (m_MonoScriptManager.m_RuntimeScripts, "m_Scripts");
	transfer.Transfer (m_AssemblyNames, "m_AssemblyNames");

	if (transfer.IsOldVersion (1))
	{
		if (m_AssemblyNames.size() >= kScriptAssemblies)
		{
			m_AssemblyNames[0] = kEngineAssemblyName;
			for (int i = 1; i < kScriptAssemblies; i++)
				m_AssemblyNames[i] = "";
		}

		set<UnityStr> m_CustomDlls;
		TRANSFER(m_CustomDlls);
		for (std::set<UnityStr>::iterator i=m_CustomDlls.begin();i!=m_CustomDlls.end();i++)
		{
			if (find(m_AssemblyNames.begin(), m_AssemblyNames.end(), *i) == m_AssemblyNames.end())
				m_AssemblyNames.push_back(*i);
		}
	}
	#endif
}


IMPLEMENT_OBJECT_SERIALIZE (MonoManager)
IMPLEMENT_CLASS (MonoManager)
GET_MANAGER (MonoManager)
GET_MANAGER_PTR (MonoManager)

#endif //ENABLE_MONO
