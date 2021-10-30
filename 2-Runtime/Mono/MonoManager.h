#ifndef MONOMANAGER_H
#define MONOMANAGER_H

#include "MonoIncludes.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Modules/ExportModules.h"

#if UNITY_FLASH
# include "Runtime/Scripting/MonoManager_Flash.h"
#elif UNITY_WINRT
# include "Runtime/Scripting/MonoManager_WinRT.h"
#endif

#if !ENABLE_MONO
struct MonoAssembly;
struct MonoVTable;
struct MonoException;
struct MonoClassField;
struct MonoDomain;
#endif

#if ENABLE_MONO

#include "Runtime/BaseClasses/GameManager.h"
#include <set>
#include <list>
#include "Runtime/Utilities/DateTime.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Mono/MonoScriptManager.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Scripting/Backend/IScriptingTypeProvider.h"

namespace Unity { class Component; class GameObject; }
extern const char* kUnityEngine;
extern const char* kMonoClasslibsProfile;
struct GUIState;
struct DomainReloadingData;

typedef dynamic_bitset AssemblyMask;

class MonoScript;
class MonoBehaviour;


/*
	The Mono Scripting runtime System consists of 3 classes.
	
	MonoManager
	MonoScript
	MonoBehaviour
	
	The MonoManager contains all information about the Assemblys it uses.
	(An assembly is an DLL or Exe containing the classes, metadata, and IL assembly code)
		
	When a MonoScript is loaded or rebuilt because the script has changed,
	the MonoManager is asked to lookup the MonoClass given by the script name.
	
	The MonoManager also keeps a lookup of ClassIDToMonoClass which is a precalculated list
	of classID's to their MonoClass* (The lookup respects inheritance so that when the C++ class is not availible as a MonoClass its parent Class is used instead)
		
	---------- continue.....
		
	MonoExport.cpp is used to wrap all the C++ objects eg. Transform
*/


typedef void ScriptsDidChangeCallback ();

// TODO: remove
MonoObject* MonoInstantiateScriptingWrapperForClassID(int classID);

class EXPORT_COREMODULE MonoManager : public ScriptingManager, public IScriptingTypeProvider
{
	public:
	REGISTER_DERIVED_CLASS (MonoManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (MonoManager)

	MonoManager (MemLabelId label, ObjectCreationMode mode);
	// virtual ~MonoManager (); declared-by-macro
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	// If 'theNameSpace' is NULL then search is in any namespace
	MonoClass* GetMonoClassCaseInsensitive (const char* className, const char* theNameSpace = NULL);
	// If 'theNameSpace' is NULL then search is in any namespace
	MonoClass* GetMonoClass (const char* className, const char* theNameSpace = NULL);
	
	// Returns the class with className in the assembly defined by identifier (normally the pathname extension of the script)
	MonoClass* GetMonoClassWithAssemblyName (const std::string& className, const string& nameSpace, const string& identifier);

	/// Reloads all assemblies with assemblymask. If the assembly can be loaded, the bit for that assembly is cleared.
	/// Returns which assemblies could be loaded

#if UNITY_EDITOR
	enum { kEngineAssembly = 0, kEditorAssembly = 1, kLocatorAssembly = 2, kScriptAssemblies = 3 };
#else
	enum { kEngineAssembly = 0, kEditorAssembly = 1, kScriptAssemblies = 2 };
#endif
	enum AssemblyLoadFailure { kEverythingLoaded = 0, kFailedLoadingScriptAssemblies = 1, kFailedLoadingEngineOrEditorAssemblies = 2 };
	AssemblyLoadFailure ReloadAssembly (AssemblyMask allAssembliesMask);

	AssemblyMask GetSystemAssemblyMask (bool load);
	AssemblyMask GetAvailableDllAssemblyMask ();

	MonoImage* GetEngineImage () { return m_ScriptImages[kEngineAssembly]; }
	
	std::string GetAssemblyPath (int index);
	MonoAssembly* GetAssembly (int index);
	
	std::string GetAssemblyIdentifierFromImage(MonoImage* image);
	int GetAssemblyIndexFromImage(MonoImage* image);

	void AssertInvalidAssembly (MonoClass* klass);
	
	MonoClass* GetBuiltinMonoClass (const char* name, bool optional = false);
	
	int GetAssemblyCount() const {return m_AssemblyNames.size(); }
	int GetAssemblyIndexFromAssemblyName (const string& identifier);

	void UnloadAllAssembliesOnNextDomainReload();

	#if UNITY_EDITOR

	void ResizeAssemblyNames(int max);

	// The assembly name should never be changed when mono dll's have already been loaded.	
	void SetAssemblyName (unsigned index, const string& name);

	void RegisterScriptsChanged (ScriptsDidChangeCallback* func);

	// Called when the domain and UnityEngine/UnityEditor have been reloaded but before anything else is,
	// to give a chance for other dlls to be loaded before anything else (i.e., package manager)
	void SetLogAssemblyReload (bool reload) { m_LogAssemblyReload = reload; }

	MonoClass* GetBuiltinEditorMonoClass (const char* name);

	int InsertAssemblyName(const std::string& assemblyName);
	void SetCustomDllPathLocation (const std::string& name, const std::string& path);
	
	bool HasCompileErrors () { return m_HasCompileErrors; }
	void SetHasCompileErrors (bool compileErrors);
	
	std::vector<UnityStr>& GetRawAssemblyNames () { return m_AssemblyNames; }
	
	#else
	bool HasCompileErrors () { return false; }
	#endif
	
	
	//implementation of IScriptingTypeProvider
	BackendNativeType NativeTypeFor(const char* namespaze, const char* className);
	ScriptingTypePtr Provide(BackendNativeType nativeType);
	void Release(ScriptingTypePtr klass);

	private:

	void SetupLoadedEditorAssemblies ();
	AssemblyLoadFailure BeginReloadAssembly (DomainReloadingData& savedData);
	AssemblyLoadFailure EndReloadAssembly (const DomainReloadingData& savedData, AssemblyMask allAssembliesMask);

	// Rebuilds the m_ClassIDToMonoClass lookup table.
	// m_ClassIDToMonoClass maps from the classID to the MonoClass it is best represented by.
	// If a MonoClass can't be found for the exact type the next parent class is used instead
	void RebuildClassIDToScriptingClass ();
	void CleanupClassIDMaps();
	// Initializes in the CommonMonoTypes struct
	virtual void RebuildCommonMonoClasses ();
	
	bool LoadAssemblies (AssemblyMask allAssembliesMask);
	void PopulateAssemblyReferencingDomain();
	
	
	typedef std::vector<MonoImage*> ScriptImages;

	ScriptImages                 m_ScriptImages;

	std::vector<UnityStr>        m_AssemblyNames;
		
	std::vector<MonoVTable*>     m_ClassIDToVTable;
	
	bool                         m_HasCompileErrors;
	static UNITY_TLS_VALUE(bool) m_IsMonoBehaviourInConstructor;

	MonoDomain*                  m_AssemblyReferencingDomain;
	bool IsThisFileAnAssemblyThatCouldChange(std::string& path);

	#if UNITY_EDITOR
	DateTime                     m_EngineDllModDate;

	typedef std::map<std::string, std::string> CustomDllLocation;
	CustomDllLocation            m_CustomDllLocation;
	bool                         m_LogAssemblyReload;
	string                       m_ManagedEditorAssembliesBasePath;
	#endif
};

EXPORT_COREMODULE MonoManager& GetMonoManager ();
MonoManager* GetMonoManagerPtr ();


namespace MonoPathContainer
{
	std::vector<std::string>& GetMonoPaths();
	void AppendMonoPath (const string& path);
};

MonoMethod* FindStaticMonoMethod (MonoImage* image, const char* className, const char* nameSpace, const char* methodName);
MonoMethod* FindStaticMonoMethod (const char* className, const char* methodName);
MonoMethod* FindStaticMonoMethod (const char* nameSpace, const char* className, const char* methodName);

/// This has to be called from main to initialize mono
bool InitializeMonoFromMain (const std::vector<string>& monoPaths, string monoConfigPath, int argc, const char** argv, bool enableDebugger=false);
void CleanupMono ();
bool CleanupMonoReloadable ();

#if UNITY_RELEASE
#define AssertInvalidClass(x)
#else
#define AssertInvalidClass(x) GetMonoManager().AssertInvalidAssembly(x);
#endif

std::string MdbFile (const string& path);
std::string PdbFile (const string& path);

void DisableLoadSeperateDomain ();

MonoDomain* CreateDomain ();

void RegisterUnloadDomainCallback (ScriptsDidChangeCallback* call);

void PostprocessStacktrace(const char* stackTrace, std::string& processedStackTrace);

void ClearLogCallback ();

#endif //ENABLE_SCRIPTING
#endif
