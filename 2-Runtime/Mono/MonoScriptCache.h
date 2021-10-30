#pragma once

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Threads/AtomicRefCounter.h"
#include "Runtime/Utilities/CStringHash.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "MonoScriptType.h"

struct CommonScriptingClasses;


// A refcounted constant method cache for the MonoBehaviour.
// When creating the MonoScript it precomputes all available methods etc.
// The data is shared in this class.
// It is refcounted, in case the MonoSript is deleted before the MonoBehaviour is destroyed.

struct MonoScriptCache
{
	typedef std::map<const char*, ScriptingMethodPtr, compare_cstring> MethodCache;
	
	enum { kUpdate = 0, kLateUpdate, kFixedUpdate, kAwake, kStart, kMain, kRenderObject, kAddToManager, kRemoveFromManager, kRemoveFromManagerInternal, kCoroutineStart, kCoroutineMain, kRenderImageFilter, kDrawGizmos, kGUI, kValidateProperties, kSerializeNetView, kNetworkInstantiate, kOnDestroy, kAudioFilterRead, kMethodCount };
	
	AtomicRefCounter                  refCount;
	ScriptingClassPtr                 klass;
	dynamic_array<ScriptingMethodPtr> methods;	
	MethodCache                       methodCache;
	MonoScriptType                        scriptType;
	const char*                       className;

	#if UNITY_EDITOR
	bool                              scriptTypeWasJustCreatedFromComponentMenu;
	bool                              runInEditMode;
	#endif

	
	MonoScriptCache ();
	~MonoScriptCache ();	
	
	void Release () const;
	void Retain () const;
};

ScriptingMethodPtr FindMethod (const MonoScriptCache& cache, const char* name);

typedef void RegisterMonoRPCCallback (const char* name);
void RegisterMonoRPC (RegisterMonoRPCCallback* callback);
MonoScriptCache* CreateMonoScriptCache (ScriptingTypePtr klass, bool isEditorScript, Object* errorContext);

bool IsValidScriptType(MonoScriptType type);
std::string FormatScriptTypeError(MonoScriptType type, const std::string& fileName);
