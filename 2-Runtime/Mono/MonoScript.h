#ifndef MONOSCRIPT_H
#define MONOSCRIPT_H

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/TextAsset.h"
#include "MonoScriptType.h"

class MonoBehaviour;
class MonoManager;
struct CommonScriptingClasses;
struct MonoScriptCache;

/*
	The MonoScript wraps a MonoClass and caches eg. some commonly used methods for fast access.
*/
class MonoScript : public TextAsset
{
	public:
	REGISTER_DERIVED_CLASS (MonoScript, TextAsset)
	DECLARE_OBJECT_SERIALIZE (MonoScript)

	MonoScript (MemLabelId label, ObjectCreationMode mode);
	// virtual ~MonoScript (); declared-by-macro

	void Init (ScriptingClassPtr scriptType);
	void Init (const ScriptString& script, const std::string& className, const std::string& nameSpace, const std::string& assemblyIdentifier, bool isEditorScript);

	const MonoScriptCache* GetScriptCache () const { return m_ScriptCache; }
	
    // Returns the MonoClass with name GetScriptClassName ()
    // Returns NULL if the MonoClass can't be found.
	ScriptingClassPtr GetClass ();

	MonoScriptType GetScriptType() const;
	
	bool IsEditorScript () const                        { return m_IsEditorScript; }
	
	
	ScriptingMethodPtr FindMethod (const char* name);
	
	// Returns the className of the script
	virtual const UnityStr& GetScriptClassName () const { return m_ClassName; }
	UnityStr GetScriptFullClassName () const;

	std::string GetNameSpace ()const;

	// Rebuilds the cached information about the class
	// This function should only be called from MonoManager
	void Rebuild (ScriptingTypePtr klass);
	
	const UnityStr& GetAssemblyName () const            { return m_AssemblyName; }
	

	/// Calls Rebuild to initialize the cached information about the class
	void AwakeFromLoad(AwakeFromLoadMode mode);
	void AwakeFromLoadThreaded ();
	virtual bool ShouldIgnoreInGarbageDependencyTracking ();
	
	#if UNITY_EDITOR

	bool IsBuiltinScript() const;
	const std::map<UnityStr, PPtr<Object> > GetDefaultReferences ()                 { return m_DefaultReferences; }
	void SetDefaultReferences (const std::map<UnityStr, PPtr<Object> >& references) { m_DefaultReferences = references; }
	
	void SetIcon (PPtr<Object> icon);
	PPtr<Object> GetIcon () const;

	void SetEditorGraphData(Object* data);
	PPtr<Object> GetEditorGraphData();
	
	bool         GetScriptTypeWasJustCreatedFromComponentMenu ();
	void         SetScriptTypeWasJustCreatedFromComponentMenu ();
	
	#endif

	int GetExecutionOrder () const                                                   { return m_ExecutionOrder; }
	void SetExecutionOrder (SInt32 executionOrder);
	
	UnityStr GetScriptName();
	
	UInt32 GetPropertiesHash ();

private:
	
	template<class TransferFunction>
	void TransferPropertiesHash (TransferFunction& transfer);

	void CleanupScriptCache ();
	
	
	SInt32      m_ExecutionOrder;
	UInt32      m_PropertiesHash;
	
	const MonoScriptCache* m_ScriptCache;
	
	UnityStr    m_ClassName;
	UnityStr    m_Namespace;
	UnityStr    m_AssemblyName;
	bool        m_IsEditorScript;
	#if UNITY_EDITOR
	std::map<UnityStr, PPtr<Object> > m_DefaultReferences;
	PPtr<Object> m_Icon;
	PPtr<Object> m_EditorGraphData;
	#endif
	
	friend struct RemapAssemblyDuringBuild;
	
	void RebuildFromAwake();
};

typedef MonoScript* MonoScriptPtr;
typedef PPtr<MonoScript> MonoScriptPPtr;

MonoScript* CreateMonoScriptFromScriptingType(ScriptingClassPtr klass);
const char* GetNameOfMethodByIndex(int index);

#if UNITY_EDITOR
std::string BuildScriptClassId(const std::string& assembly, const std::string& ns, const std::string& klass);
void GetScriptClassIdComponents(const std::string& scriptClassId, std::string& assembly, std::string& ns, std::string& klass);
#endif

extern const char* kBuildLogicGraphDllFile;

#endif
