#include "UnityPrefix.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoScriptManager.h"
#include "Runtime/Threads/Thread.h"

#if ENABLE_SCRIPTING
template<typename T> MonoScript* FindScript(MonoScriptManager::Scripts& scripts, T& filter)
{
	MonoScriptManager::Scripts::iterator i, next;
	for (i=	scripts.begin ();i != scripts.end ();i=next)
	{
		next = i; next++;
		MonoScript* script = *i;
		if (script == NULL)
		{
			scripts.erase (i);
		}
		else if (filter.Match(script))
			return script;
	}
	return NULL;	
}

struct MatchScriptByNameFilter
{
	const char* name;
	bool Match(MonoScript* ms) { return ms->GetScriptClassName() == name; }
};

struct MatchScriptByClassFilter
{
	ScriptingClassPtr klass;
	bool Match(MonoScript* ms) { return ms->GetClass() == klass; }
};

struct MatchScriptByClassNamespaceAssemblyFilter
{
	// hold by const ref to minimize copying
	std::string const& scriptClassName, nameSpace, assemblyName;
	
	MatchScriptByClassNamespaceAssemblyFilter (std::string const& scriptName, std::string const& ns, std::string const& assembly)
	:	scriptClassName (scriptName), nameSpace (ns), assemblyName (assembly)
	{}
	
	bool Match(MonoScript* ms)
	{
		return ms->GetScriptClassName () == scriptClassName
			&& ms->GetNameSpace () == nameSpace
			&& ms->GetAssemblyName () == assemblyName;
	}
};

static MonoScript* FindScript(ScriptingClassPtr klass, MonoScriptManager::Scripts& scripts)
{
	MatchScriptByClassFilter filter;
	filter.klass = klass;
	return FindScript(scripts,filter);
}

static void AddScriptsToList(MonoScriptManager::Scripts& scripts, MonoScriptManager::AllScripts& addtothis)
{
	MonoScriptManager::Scripts::iterator i, next;
	
	for (i=	scripts.begin ();i != scripts.end ();i=next)
	{
		next = i; next++;
		MonoScript* script = *i;
		if (script)
			addtothis.insert (script);
		else
		{
			scripts.erase (i);
		}
	}	
}

MonoScriptManager::AllScripts MonoScriptManager::GetAllRuntimeScripts ()
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	
	AllScripts scripts;
	AddScriptsToList(m_RuntimeScripts,scripts);
	return scripts;
}

MonoScript* MonoScriptManager::FindRuntimeScript (const string& className)
{
	ASSERT_RUNNING_ON_MAIN_THREAD

	MatchScriptByNameFilter filter;
	filter.name = className.c_str();
	return FindScript(m_RuntimeScripts,filter);
}

MonoScript* MonoScriptManager::FindRuntimeScript (const string& className, string const& nameSpace, string const& assembly)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	
	MatchScriptByClassNamespaceAssemblyFilter filter (className, nameSpace, assembly);
	return FindScript (m_RuntimeScripts, filter);
}

MonoScript* MonoScriptManager::FindRuntimeScript (ScriptingClassPtr klass)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	return FindScript(klass,m_RuntimeScripts);
}

void MonoScriptManager::RegisterRuntimeScript (MonoScript& script)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	m_RuntimeScripts.insert (&script);
}

#if UNITY_EDITOR
MonoScript* MonoScriptManager::FindEditorScript (MonoClass* klass)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	return FindScript(klass,m_EditorScripts);
}

MonoScript* MonoScriptManager::FindEditorScript (const string& className)
{
	ASSERT_RUNNING_ON_MAIN_THREAD

	MatchScriptByNameFilter filter;
	filter.name = className.c_str();
	return FindScript(m_EditorScripts,filter);
}

void MonoScriptManager::RegisterEditorScript (MonoScript& script)
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	m_EditorScripts.insert (&script);
}

MonoScriptManager::AllScripts MonoScriptManager::GetAllRuntimeAndEditorScripts ()
{
	ASSERT_RUNNING_ON_MAIN_THREAD
	
	AllScripts scripts;

	AddScriptsToList(m_RuntimeScripts,scripts);
	AddScriptsToList(m_EditorScripts,scripts);
	
	return scripts;
}

#endif
#endif
