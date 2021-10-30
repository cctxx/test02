#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

using namespace std;

struct MonoMenuItem
{
	string	menuItem;
	string	execute;
	string	validate;
	int		position;
	MonoClass* klass;
};

struct MonoMenuItemBinding
{
	MonoString* menuItem;
	MonoString* execute;
	MonoString* validate;
	int			priority;
	int         index;
	MonoObject* type;
};

struct MenuCommandBinding
{
	MonoObject* context;
	int         userData;
};

struct ScriptCommands : public MenuInterface
{
	typedef list<MonoMenuItem> MenuItems;
	MenuItems m_MenuItems;
	bool m_NeedsRebuild;

	ScriptCommands ()
	{
		m_NeedsRebuild = true;
	}
	
	virtual void UpdateContextMenu (std::vector<Object*> &context, int userData)
	{
		Update();
	}
	
	virtual void Update ()
	{
		if (m_NeedsRebuild)
		{
			Rebuild ();
			RebuildMenu ();
			m_NeedsRebuild = false;
		}
	}

	virtual bool Validate (const MenuItem &menuItem)
	{
		if (m_NeedsRebuild)
			return false;
			
		int index = StringToInt (menuItem.m_Command);
		MenuItems::iterator cmd = m_MenuItems.begin();
		advance(cmd, index);
		
		if (cmd->validate.empty())
			return true;
	
		if (menuItem.context.empty ())
		{
			MonoObject* returnValue = InvokeMenuItemWithContext(cmd, NULL, menuItem.contextUserData, cmd->validate.c_str());
			return MonoObjectToBool(returnValue);
		}

		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
		{
			MonoObject* returnValue = InvokeMenuItemWithContext(cmd, Scripting::ScriptingWrapperFor(*i), menuItem.contextUserData, cmd->validate.c_str());
			if (!MonoObjectToBool(returnValue))
				return false;
		}
		
		return true;
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		if (m_NeedsRebuild)
			return;
			
		int index = StringToInt (menuItem.m_Command);
		MenuItems::iterator cmd = m_MenuItems.begin();
		advance(cmd, index);
		
		if (menuItem.context.empty())
		{
			InvokeMenuItemWithContext(cmd,NULL,menuItem.contextUserData,cmd->execute.c_str());
			return;
		}
		
		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			InvokeMenuItemWithContext(cmd, Scripting::ScriptingWrapperFor(*i),menuItem.contextUserData, cmd->execute.c_str());
	}
			
	ScriptingObjectPtr InvokeMenuItemWithContext(MenuItems::iterator& cmd, ScriptingObject* context, int userData, const char* methodname)
	{
		MonoObject* pass = scripting_object_new(GetScriptingTypeRegistry().GetType("UnityEditor","MenuCommand"));
		ExtractMonoObjectData<MenuCommandBinding>(pass).context = context;
		ExtractMonoObjectData<MenuCommandBinding>(pass).userData = userData;
		
		ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(cmd->klass,methodname,ScriptingMethodRegistry::kStaticOnly); 
		ScriptingInvocation invocation(method);
		invocation.AddObject(pass);
		invocation.AdjustArgumentsToMatchMethod();
		return invocation.InvokeChecked();
	}

	void Rebuild ();
	void RebuildAssembly (MonoAssembly* ass);
};

void ScriptCommands::RebuildAssembly (MonoAssembly* ass)
{
	if (ass == NULL)
		return;
		
	// Use C# to extract all menu commands in the script
	void* params[1] = { mono_assembly_get_object (mono_domain_get(), ass) };
	MonoArray* commandArray = (MonoArray*)CallStaticMonoMethod("AttributeHelper", "ExtractMenuCommands", params);
	if (commandArray == NULL)
		return;
	
	// Add the menu commands to the commands array and register it with the menu system
	for (int i=0;i<mono_array_length(commandArray);i++)
	{
		MonoMenuItemBinding& binding = GetMonoArrayElement<MonoMenuItemBinding> (commandArray, i);
		
		MonoMenuItem item;
		item.menuItem = MonoStringToCpp(binding.menuItem);
		item.execute = MonoStringToCpp(binding.execute);
		item.validate = MonoStringToCpp(binding.validate);
		item.position = binding.priority;
		item.klass = GetScriptingTypeRegistry().GetType (binding.type);
		
		if (item.execute.empty ())
			continue;
		
		MenuController::AddMenuItem (item.menuItem, IntToString(m_MenuItems.size()), this, item.position);
		m_MenuItems.push_back(item);
	}
}

void ScriptCommands::Rebuild ()
{
	// Clear old commands
	for (MenuItems::iterator i=m_MenuItems.begin();i != m_MenuItems.end();i++)
		MenuController::RemoveMenuItem (i->menuItem);
	m_MenuItems.clear();

	MenuController::RemoveEmptyParentMenus ();
	
	for (int i=MonoManager::kEditorAssembly;i<GetMonoManager().GetAssemblyCount();i++)
	{
		RebuildAssembly (GetMonoManager().GetAssembly(i));
	}
		
}

static ScriptCommands *gScriptCommands = NULL;

static void EditorScriptsDidChange ()
{
	if (gScriptCommands)
		gScriptCommands->m_NeedsRebuild = true;
}

static void ScriptCommandsRegisterMenu ()
{
	gScriptCommands = new ScriptCommands ();
	MenuController::AddContextItem ("UNUSED_PROXY_ScriptCommands", "", gScriptCommands);

	GlobalCallbacks::Get().didReloadMonoDomain.Register(EditorScriptsDidChange);
}

STARTUP (ScriptCommandsRegisterMenu)	// Call this on startup.
