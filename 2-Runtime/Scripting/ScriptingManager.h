#ifndef _SCRIPTINGMANAGER_H_
#define _SCRIPTINGMANAGER_H_

#if ENABLE_SCRIPTING

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Mono/MonoScriptManager.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Modules/ExportModules.h"

class IScriptingTypeProvider;
class ScriptingMethodRegistry;
class ScriptingTypeRegistry;
class IScriptingMethodFactory;

extern const char* kEngineNameSpace;
extern const char* kEditorNameSpace;
extern ScriptingClassPtr gClassIDToClass;

class EXPORT_COREMODULE ScriptingManager : public GlobalGameManager
{
public:
	MonoScriptManager& GetMonoScriptManager() { return m_MonoScriptManager; }
	ScriptingMethodRegistry& GetScriptingMethodRegistry() { return *m_ScriptingMethodRegistry; }
	ScriptingTypeRegistry& GetScriptingTypeRegistry() { return *m_ScriptingTypeRegistry; }
	const CommonScriptingClasses& GetCommonClasses () { return m_CommonScriptingClasses; }
	ScriptingClassPtr ClassIDToScriptingClass (int classID);
	int ClassIDForScriptingClass(ScriptingClassPtr klass);
	virtual void RebuildClassIDToScriptingClass ();
	ScriptingObjectPtr CreateInstance(ScriptingClassPtr klass);
	virtual bool IsTrustedToken (const std::string& /*publicKeyToken*/){ return false; }

	~ScriptingManager();
protected:
	ScriptingManager (MemLabelId label, ObjectCreationMode mode, IScriptingTypeProvider* scriptingTypeProvider, IScriptingMethodFactory* scriptingMethodFactory);

	MonoScriptManager			 m_MonoScriptManager;
	CommonScriptingClasses       m_CommonScriptingClasses;
	typedef std::vector<ScriptingTypePtr> ClassIDToMonoClass;
	ClassIDToMonoClass			 m_ClassIDToMonoClass;

	typedef vector_map<ScriptingTypePtr, int> ScriptingClassMap;
	ScriptingClassMap m_ScriptingClassToClassID;

	ScriptingMethodRegistry* m_ScriptingMethodRegistry;
	ScriptingTypeRegistry* m_ScriptingTypeRegistry;
	IScriptingMethodFactory* m_ScriptingMethodFactory;
};

EXPORT_COREMODULE ScriptingManager& GetScriptingManager ();

inline MonoScriptManager& GetMonoScriptManager() { return GetScriptingManager().GetMonoScriptManager(); }

inline ScriptingMethodRegistry& GetScriptingMethodRegistry() { return GetScriptingManager().GetScriptingMethodRegistry(); }
inline ScriptingTypeRegistry& GetScriptingTypeRegistry() { return GetScriptingManager().GetScriptingTypeRegistry(); }

#define ScriptingClassFor(x) GetScriptingManager().ClassIDToScriptingClass(CLASS_##x)

#endif

#endif
