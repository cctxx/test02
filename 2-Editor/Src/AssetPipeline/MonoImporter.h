#ifndef MONOIMPORTER_H
#define MONOIMPORTER_H

#include "AssetImporter.h"
#include "Runtime/Utilities/GUID.h"

class MonoScript;

class MonoImporter : public AssetImporter
{
	public:
	
	REGISTER_DERIVED_CLASS (MonoImporter, AssetImporter)
	DECLARE_OBJECT_SERIALIZE (MonoImporter)
	
	MonoImporter (MemLabelId label, ObjectCreationMode mode);
	// MonoImporter (); declared-by-macro
	
	virtual void Reset ();
	
	virtual void GenerateAssetData ();
	
	static void InitializeClass ();
	static void CleanupClass () {}
	static void Postprocess (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, std::string>& moved);
	static int CanLoadPathName (const string& pathName, int* queue);
	
	void SetExecutionOrder (SInt32 executionOrder);
	void SetIcon(PPtr<Texture2D> icon);
	void SetDefaultReferences (const std::vector<std::string>& names, const std::vector<Object*>& targets);
	PPtr<Object> GetDefaultReference (const std::string& name);

	static void PerlPreprocess (const string& engineSourceFolder);

	virtual void UnloadObjectsAfterImport (UnityGUID guid);
	
	private:

	void TransferReadDefaultReferences (YAMLNode *node);

	std::vector<pair<UnityStr, PPtr<Object> > > m_DefaultReferences;
	SInt16 m_ExecutionOrder;
	PPtr<Texture2D> m_Icon;
};

void StartCompilation (int importFlags);

#endif
