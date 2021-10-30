#include "UnityPrefix.h"
#include "MonoImporter.h"
#include "MonoCompilationPipeline.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/TypeUtilities.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "AssetInterface.h"
#include "AssetDatabase.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Graphics/Texture2D.h"

// should be possible to be refactored away
#include "Editor/Src/EditorUserBuildSettings.h"

enum { kMonoImporterVersion = 3 };

static void ExtractDefaultReferences (MonoBehaviour& behaviour, vector<pair<UnityStr, PPtr<Object> > >& output);


void MonoImporter::GenerateAssetData ()
{
	string pathName = GetAssetPathName ();
	
	// read script file contents
	InputString input;
	if (!ReadStringFromFile (&input, pathName))
	{
		LogImportError ("File couldn't be read!");
		return;
	}
	
	bool encodingDetected;
	string contents = ConvertToUTF8(input.c_str(), input.size(), encodingDetected);
	
	// if (!encodingDetected) ...
	
	
	string assemblyIdentifier = GetCompilerAssemblyIdentifier(pathName);
	
	if (assemblyIdentifier.empty ())
	{
		ErrorString ("Internal compiler error!");
		return;
	}
	
	// Create script, set className to be the name of the file, set script content.
	MonoScript& script = RecycleExistingAssetObject<MonoScript> ("script");
	script.SetHideFlags(0);
	
	if (WarnInconsistentLineEndings(contents))
	{
		AddImportError (Format("There are inconsistent line endings in the '%s' script. Some are Mac OS X (UNIX) and some are Windows.\nThis might lead to incorrect line numbers in stacktraces and compiler errors. Many text editors can fix this using Convert Line Endings menu commands.", pathName.c_str()), pathName.c_str(), -1, kLog | kAssetImportWarning, &script);
	}
	
	// Read Default references from Monobehaviour for backwards compatibility
	// We used to store this in a MonoBehaviour named Default References.
	// Now it is just a map stored in MonoScript
	MonoBehaviour* defaultReferencesBehaviour = GetFirstDerivedObjectAtPath<MonoBehaviour>(GetAssetPathName());
	if (defaultReferencesBehaviour)
	{
		ExtractDefaultReferences(*defaultReferencesBehaviour, m_DefaultReferences);
	}
	
	// Assign default references to script
	map<UnityStr, PPtr<Object> > defaultReferences;
	for (int i=0;i<m_DefaultReferences.size();i++)
		defaultReferences[m_DefaultReferences[i].first] = m_DefaultReferences[i].second;
	script.SetDefaultReferences(defaultReferences);
	
	script.SetExecutionOrder(m_ExecutionOrder);

	script.SetIcon (PPtr<Object> (m_Icon.GetInstanceID()));
	
	// Moving to a different compiler. Need to recompile everything
	if (!script.GetAssemblyName().empty() && script.GetAssemblyName() != assemblyIdentifier)
		DirtyAllScriptCompilers();
	
	ScriptingInvocation invocation("UnityEditor.Scripting", "ScriptCompilers", "GetNamespace");
	std::string assetPathName = GetAssetPathName();
	invocation.AddString(assetPathName);
	MonoObject* mo = invocation.Invoke();

	std::string namespaze = MonoStringToCppChecked (mo);

	string className = GetLastPathNameComponent (GetAssetPathName ());
	className = DeletePathNameExtension (className);
	script.Init (contents, className, namespaze, assemblyIdentifier, IsEditorOnlyScriptAssemblyIdentifier(assemblyIdentifier));
	
#if !UNITY_RELEASE
	// Mono assemblies will be build async w/o linkning back to scripts, so we can't simply delay check
	// that's why we skip checking 
	//   actually AwakeFromLoad will be called, and if smth goes wrong - we'll see  that from the first test that uses prefabs
	script.HackSetAwakeWasCalled();
#endif
	
	DirtyScriptCompilerByAssemblyIdentifier(assemblyIdentifier);
	
	if (! IsEditorOnlyScriptAssemblyIdentifier(assemblyIdentifier) )
		GetMonoScriptManager().RegisterRuntimeScript (script);
	else
		GetMonoScriptManager().RegisterEditorScript (script);
	
	// check if script class name duplicates any of built-in components
	int classID = Object::StringToClassID (className);
	if (classID != -1 && Object::IsDerivedFromClassID(classID, ClassID(Component)))
	{
		AddImportError (Format("Script '%s' has the same name as built-in Unity component.\nAddComponent and GetComponent will not work with this script.", className.c_str()), pathName.c_str(), -1, kLog | kAssetImportWarning, &script);
	}
	
	bool startCompilation = !(GetImportFlags() & kNextImportHasSamePriority);
	if (startCompilation)
	{
		StartCompilation(GetImportFlags());
	}
}

MonoImporter::MonoImporter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	
}

void MonoImporter::Reset ()
{
	Super::Reset();
	m_ExecutionOrder = 0;
	m_Icon = PPtr<Texture2D>();
	m_DefaultReferences.clear();
}

MonoImporter::~MonoImporter ()
{}

int MonoImporter::CanLoadPathName (const string& pathName, int* queue)
{
	/// The Monoimporter uses the queue to determine if it is the last script in the refresh queue.
	/// Thus this queue should be unique
	/// We compile scripts first so that when downloading from maint we don't lose properties because when loading a prefab
	/// the script variables might not be available in the script yet!
	*queue = -999998;
	
	string ext = GetPathNameExtension (pathName);
	for (string::iterator i=ext.begin ();i!=ext.end ();i++)
		*i = ToLower (*i);
	
	if (IsExtensionSupportedByCompiler (ext))
		return kCopyAsset;
	else
		return 0;
}
void MonoImporter::InitializeClass ()
{ 
	RegisterCanLoadPathNameCallback (CanLoadPathName, ClassID (MonoImporter), kMonoImporterVersion);
	AssetDatabase::RegisterPostprocessCallback (Postprocess);
}

void MonoImporter::Postprocess (const std::set<UnityGUID>& refreshed, const std::set<UnityGUID>& added, const std::set<UnityGUID>& removed, const std::map<UnityGUID, string>& moved)
{
	// Recompile all if a script was removed!
	// -> compile errors are removed when a script is deleted
	bool needRecompile = false;
	for (set<UnityGUID>::const_iterator i=removed.begin ();i != removed.end ();i++)
	{
		string extension = ToLower(GetPathNameExtension (GetAssetPathFromGUID (*i)));
		// Check if we are dealing with a dll or script
		// (Can't use CanLoadPathName since the dll check requires the file to exist!)
		if (!StrICmp(extension, "dll") || IsExtensionSupportedByCompiler (extension))
			needRecompile = true;
	}
	
	for (std::map<UnityGUID, string>::const_iterator i=moved.begin ();i != moved.end ();i++)
	{
		string assetPath = GetAssetPathFromGUID (i->first);
		string extension = ToLower(GetPathNameExtension (assetPath));
		// Check if we are dealing with a dll or script
		// (Can't use CanLoadPathName since the dll check requires the file to exist!)
		if (!StrICmp(extension, "dll") || IsExtensionSupportedByCompiler (extension))
		{
			needRecompile = true;
			
			// kNextImportHasSamePriority -> prevent automatic recompiling.
			// All script assemblies will be recompiled anyway
			AssetInterface::Get().ImportAtPathImmediate(assetPath, kNextImportHasSamePriority);
		}
	}
	
	if (needRecompile)
	{
		ForceRecompileAllScriptsAndDlls (AssetInterface::Get().GetGlobalPostprocessFlags());
	}
}


struct ExtractDefaultReferencesFunctor
{
	vector<pair<UnityStr, PPtr<Object> > > output;
	
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if (IsTypeTreePPtr(typeTree) && typeTree.m_Name != "m_Script" && typeTree.m_Name != "m_GameObject" && typeTree.m_Name != "m_ExtensionPtr")
		{
			int instanceID = ExtractPPtrInstanceID(data, bytePosition);
			if (instanceID != 0)
			{
				output.push_back(make_pair(typeTree.m_Name, PPtr<Object> (instanceID)));
			}
		}
		
		// Only iterate top level children
		return typeTree.m_Father == NULL;
	}
};

static void ExtractDefaultReferences (MonoBehaviour& behaviour, vector<pair<UnityStr, PPtr<Object> > >& output)
{
	TypeTree typeTree;
	dynamic_array<UInt8> data(kMemTempAlloc);
	
	// Create typetree and data
	GenerateTypeTree(behaviour, &typeTree);
	WriteObjectToVector(behaviour, &data);
	
	// Modify data 
	ExtractDefaultReferencesFunctor functor;
	IterateTypeTree (typeTree, data, functor);
	output = functor.output;
}

void MonoImporter::SetExecutionOrder (SInt32 executionOrder)
{
	m_ExecutionOrder = executionOrder;
	SetDirty();
}

void MonoImporter::SetIcon (PPtr<Texture2D> icon)
{
	m_Icon = icon;
	SetDirty();
}

void MonoImporter::SetDefaultReferences (const std::vector<std::string>& name, const std::vector<Object* >&  target)
{
	m_DefaultReferences.resize(name.size());
	for (int i=0;i<m_DefaultReferences.size();i++)
	{
		m_DefaultReferences[i].first = name[i];
		m_DefaultReferences[i].second = target[i];
	}
	SetDirty();
}

PPtr<Object> MonoImporter::GetDefaultReference (const std::string& name)
{
	for (int i=0;i<m_DefaultReferences.size();i++)
	{
		if (m_DefaultReferences[i].first  == name)
			return m_DefaultReferences[i].second;
	}
	
	return NULL;
}

void MonoImporter::TransferReadDefaultReferences (YAMLNode *node)
{
	YAMLMapping *settings = dynamic_cast<YAMLMapping*>(node);

	m_DefaultReferences.clear();
	if(settings) 
	{
		YAMLMapping* defaultReferences = dynamic_cast<YAMLMapping*>(settings->Get("defaultReferences"));
		if (defaultReferences) 
		{
			for ( YAMLMapping::const_iterator i = defaultReferences->begin(); i != defaultReferences->end(); i++  )
			{
				string name = (string) *(i->first);
				YAMLMapping* value = dynamic_cast<YAMLMapping*> (i->second);
				
				if ( value )
					m_DefaultReferences.push_back(make_pair(name,value->GetPPtr()));
			}
		}
		
		YAMLScalar* executionOrder = dynamic_cast<YAMLScalar*>( settings->Get("executionOrder") );
		if (executionOrder != NULL)
			m_ExecutionOrder = int(*executionOrder);

		
		YAMLMapping* icon = dynamic_cast<YAMLMapping*>( settings->Get("icon") );
		if ( icon )
		{
			PPtr<Object> obj = icon->GetPPtr();
			m_Icon = PPtr<Texture2D>(obj.GetInstanceID());
		}
	}
}

template<class T>
void MonoImporter::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	transfer.SetVersion (2);
  	
	if (transfer.AssetMetaDataOnly() && transfer.IsOldVersion(1))
  	{
		// Use custom code to transfer default references to meta files.
		// This is needed for backwards compatibility, as the mapping used here would not work in the generic code
		Assert ((IsSameType<YAMLRead, T>::result));
		YAMLNode *node = reinterpret_cast<YAMLRead&> (transfer).GetCurrentNode();
		TransferReadDefaultReferences (node);
		delete node;
  	}
	else
		TRANSFER(m_DefaultReferences);
	transfer.Transfer(m_ExecutionOrder, "executionOrder");
	transfer.Align();
	transfer.Transfer (m_Icon, "icon");

	PostTransfer (transfer);
}

void MonoImporter::UnloadObjectsAfterImport (UnityGUID guid)
{
	// Intentially left blank.
	// MonoScripts are never unloaded and always kept in memory.
}

IMPLEMENT_CLASS_HAS_INIT (MonoImporter)
IMPLEMENT_OBJECT_SERIALIZE (MonoImporter)
