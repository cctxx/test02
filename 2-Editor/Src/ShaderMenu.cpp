#include "UnityPrefix.h"
#include "ShaderMenu.h"
#include "MenuController.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "EditorHelper.h"
#include "Runtime/Scripting/Scripting.h"

const char* kBasePath = "CONTEXT/ShaderPopup/";

class SetupShaderPopupMenuController : public MenuInterface
{
	public: 
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		return true;
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
		{
			MonoBehaviour* target = dynamic_pptr_cast<MonoBehaviour*> (*i);
			if (target && target->GetInstance())
			{
				ScriptingMethodPtr method = target->FindMethod("OnSelectedShaderPopup");
				
				int instanceID = StringToInt(menuItem.m_Command);
				Shader* shader = dynamic_instanceID_cast<Shader*> (instanceID);
				
				void* args[] = { MonoStringNew(menuItem.m_Command), Scripting::ScriptingWrapperFor(shader) };
				MonoException* exc;
				MonoObject* instance = target->GetInstance();
				mono_runtime_invoke_profiled (method->monoMethod, target->GetInstance(), args, &exc);	
				if (exc)
				{
					Scripting::LogException(exc, Scripting::GetInstanceIDFromScriptingWrapper(instance));
				}
			}
		}
	}
};
static SetupShaderPopupMenuController *gShaderMenu = NULL;


static bool IsLegacyShader (const std::string& name)
{
	return BeginsWith (name, "Legacy Shaders/");
}


static void AddShaderToMenu (const std::string& name, int id, int selectedID, bool* addSeparator)
{
	if (BeginsWith (name, "Deprecated") || BeginsWith (name, "Hidden"))
		return;

	if (addSeparator && !(*addSeparator))
	{
		MenuController::AddSeparator (kBasePath);
		*addSeparator = true;
	}

	MenuController::AddMenuItem(kBasePath + name, IntToString(id), gShaderMenu);
	MenuController::SetChecked(kBasePath + name, id == selectedID);
}


struct ShaderNameInfo
{
	int			id;
	bool		hasErrors;
};

typedef std::multimap<std::string,ShaderNameInfo> ShaderNameMap;

namespace ShaderNameManager
{
	static ShaderNameMap* s_BuiltinShaderNames = NULL;

	void StaticInitialize()
	{
		if(!s_BuiltinShaderNames)
			s_BuiltinShaderNames = UNITY_NEW(ShaderNameMap,kMemManager);
	}
	void StaticDestroy()
	{
		UNITY_DELETE(s_BuiltinShaderNames,kMemManager);
	}
}

void AddBuiltinShaderInfoForMenu (const std::string& name, SInt32 id)
{
	ShaderNameManager::StaticInitialize();
	ShaderNameInfo info;
	info.id = id;
	info.hasErrors = false;
	ShaderNameManager::s_BuiltinShaderNames->insert (std::make_pair(name, info));
}


static ShaderNameMap GetAllShaderNames()
{
	ShaderNameManager::StaticInitialize();
	ShaderNameMap res = *ShaderNameManager::s_BuiltinShaderNames;
	AssetDatabase& db = AssetDatabase::Get();
	for (Assets::const_iterator it = db.begin(), itEnd = db.end(); it != itEnd; ++it)
	{
		if (it->second.importerClassId != ClassID(ShaderImporter))
			continue;
		if (it->second.mainRepresentation.classID != ClassID(Shader))
			continue;
		const std::string& name = it->second.mainRepresentation.scriptClassName;
		if (name.empty())
			continue;
		ShaderNameInfo info;
		info.id = it->second.mainRepresentation.object.GetInstanceID();
		info.hasErrors = it->second.mainRepresentation.flags & LibraryRepresentation::kAssetHasErrors;
		res.insert (std::make_pair(name, info));
	}
	
	// ShaderGraph generated shaders
	for (Assets::const_iterator it = db.begin(), itEnd = db.end(); it != itEnd; ++it)
	{
		if (it->second.importerClassId != ClassID(NativeFormatImporter))
			continue;
		if (it->second.mainRepresentation.classID != ClassID(MonoBehaviour))
			continue;
		const std::string& name = it->second.mainRepresentation.scriptClassName;
		if (name != "MaterialGraph")
			continue;

		printf_console ("MaterialGraph: %s \n", it->second.mainRepresentation.name.c_str());
		
		const std::string& assetFileName = GetPersistentManager().GetPathName (it->second.mainRepresentation.object.GetInstanceID());
		// NOTE: (ClassID(Shader) * kMaxObjectsPerClassID) gives a FileID for the 1st Shader asset inside MaterialGraph
		// according to Joe at least :)
		const LocalIdentifierInFileType fileID = ClassID(Shader) * kMaxObjectsPerClassID;
		SInt32 shaderInstanceID = GetPersistentManager().GetInstanceIDFromPathAndFileID (assetFileName, fileID);
		ShaderNameInfo info;
		info.id = shaderInstanceID;
		info.hasErrors = it->second.mainRepresentation.flags & LibraryRepresentation::kAssetHasErrors;
		const std::string& ShaderName = it->second.mainRepresentation.name;
		res.insert (std::make_pair( std::string("Graphs/") + name, info));
	}

	return res;
}


void SetupShaderPopupMenu (Material& material)
{
	if (gShaderMenu == NULL)
		gShaderMenu = new SetupShaderPopupMenuController();
	
	MenuController::RemoveMenuItem ("CONTEXT/ShaderPopup");

	ShaderNameMap shaderNames = GetAllShaderNames();
	
	int selectedID = material.GetShaderPPtr().GetInstanceID();
	if (selectedID == 0) // invalid ID
		selectedID = 1;

	
	// TODO:
	// The built-in shaders could be sorted much better. Put them before anything
	// else, separate from the rest with a separator, and don't sort by name, instead like:
	// VertexLit, Diffuse, Specular, Bumped Diffuse, Bumped Specular, ...

	// First add non-submenu shaders
	for (ShaderNameMap::const_iterator i = shaderNames.begin(); i != shaderNames.end(); i++)
	{
		// Ignore shaders with errors
		if (i->second.hasErrors)
			continue;
		// Skip submenu shaders here
		if (i->first.find('/') != string::npos)
			continue;

		AddShaderToMenu (i->first, i->second.id, selectedID, NULL);
	}
	
	// Then add submenu shaders
	for (ShaderNameMap::const_iterator i = shaderNames.begin(); i != shaderNames.end(); i++)
	{
		// Ignore shaders with errors
		if (i->second.hasErrors)
			continue;
		// Skip non-submenu shaders here
		if (i->first.find('/') == string::npos)
			continue;
		// Skip legacy shaders
		if (IsLegacyShader (i->first))
			continue;

		AddShaderToMenu (i->first, i->second.id, selectedID, NULL);
	}

	bool addedSeparator;

	// Then add legacy shaders
	addedSeparator = false;
	for (ShaderNameMap::const_iterator i = shaderNames.begin(); i != shaderNames.end(); i++)
	{
		// Ignore shaders with errors
		if (i->second.hasErrors)
			continue;
		// Only include legacy shaders
		if (!IsLegacyShader (i->first))
			continue;

		AddShaderToMenu (i->first, i->second.id, selectedID, &addedSeparator);
	}
	
	// Add failed-compilation shaders last
	addedSeparator = false;
	for (ShaderNameMap::const_iterator i = shaderNames.begin(); i != shaderNames.end(); i++)
	{
		// Ignore all correct shaders!
		if (!i->second.hasErrors)
			continue;

		AddShaderToMenu (i->first, i->second.id, selectedID, &addedSeparator);
	}
}
