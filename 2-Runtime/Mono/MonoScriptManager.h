#ifndef _MONOSCRIPTMANAGER_H_
#define _MONOSCRIPTMANAGER_H_ 

class MonoScript;
class MonoManager;

class MonoScriptManager
{
public:
	void RegisterRuntimeScript (MonoScript& script);
	void RegisterEditorScript (MonoScript& script);
	
	typedef UNITY_SET(kMemScriptManager, MonoScript*) AllScripts;
	typedef UNITY_SET(kMemScriptManager, PPtr<MonoScript>) Scripts;
	
	MonoScript* FindRuntimeScript (const std::string& className);
	MonoScript* FindRuntimeScript (const std::string& className, std::string const& nameSpace, std::string const& assembly);
	MonoScript* FindRuntimeScript (ScriptingClassPtr klass);
	AllScripts GetAllRuntimeScripts ();

#if UNITY_EDITOR
	AllScripts GetAllRuntimeAndEditorScripts ();
	MonoScript* FindEditorScript (MonoClass* klass);
	MonoScript* FindEditorScript (const std::string& className);
#else
	MonoScript* FindEditorScript (ScriptingClassPtr klass) { return NULL; }
#endif


private:

	Scripts                      m_RuntimeScripts;
#if UNITY_EDITOR
	Scripts                      m_EditorScripts;
#endif
	
	friend class MonoManager;
};

#endif
