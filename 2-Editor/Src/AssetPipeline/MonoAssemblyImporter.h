#ifndef MONO_ASSEMBLY_IMPORTER_H
#define MONO_ASSEMBLY_IMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Utilities/GUID.h"

struct MonoClass;
class MonoScript;

class MonoAssemblyImporter : public AssetImporter
{
public:
	
	REGISTER_DERIVED_CLASS (MonoAssemblyImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (MonoAssemblyImporter)
		
	static void InitializeClass ();
	static void CleanupClass () { }
	static int CanLoadPathName (const string& pathName, int* queue);
	
	MonoAssemblyImporter (MemLabelId label, ObjectCreationMode mode);
	// MonoImporter (); declared-by-macro
	
	virtual void GenerateAssetData ();
	
	virtual void Reset ();
	
	void SetExecutionOrder (const std::string& className, int executionOrder);
	void SetIcon (const std::string& className, PPtr<Texture2D> icon);
	
	virtual void UnloadObjectsAfterImport (UnityGUID guid);
	
	private:

	void CreateMonoScriptForClass(const std::string& assemblyName, const std::string& className, const std::string& nameSpace, bool isEditorDll);
	
	typedef std::map<UnityStr, int> ExecutionOrder;
	ExecutionOrder m_ExecutionOrder;

	typedef std::map<UnityStr, PPtr<Texture2D> > IconMap;	
	IconMap m_IconMap;
};

void CopyMonoScriptIconToImporters (MonoScript& monoScript);
void SetMonoScriptExecutionOrder (MonoScript& monoScript, int executionOrder);

#endif
