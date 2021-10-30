#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"

using namespace std;

struct MonoMenuItemBinding
{
	MonoString* menuItem;
	MonoString* execute;
	MonoString* validate;
	int         priority;
	int         index;
	MonoObject* type;
};

struct ScriptContextCommands : public MenuInterface {
	
	virtual void UpdateContextMenu (std::vector<Object*> &context, int userData)
	{
		if (!context.empty())
		{
			MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (context[0]);
			if (behaviour == NULL || behaviour->GetClass() == NULL)
				return;
			
			RebuildContext(behaviour->GetClass());
		}
	}

	void RebuildContext (MonoClass* klass);

	virtual bool Validate (const MenuItem &menuItem) {
		return true;
	}
	
	virtual void Execute (const MenuItem &menuItem) {
		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
		{
			MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (*i);
			if (behaviour == NULL)
				return;
			
			ScriptingInvocation invocation(behaviour->GetClass(), menuItem.m_Command.c_str());
			invocation.object = behaviour->GetInstance();
			invocation.objectInstanceIDContextForException = behaviour->GetInstanceID();
			invocation.InvokeChecked();

			behaviour->SetDirty();
		}
	}
};

void ScriptContextCommands::RebuildContext (MonoClass* klass)
{
	// Remove old menu items
	MenuItem* item = MenuController::FindItem("CONTEXT/MonoBehaviour");
	if (item && item->m_Submenu != NULL)
	{
		///@TODO: This should really be moved to MenuController
		Menu::iterator next;
		for (Menu::iterator i=item->m_Submenu->begin();i != item->m_Submenu->end();i=next)
		{
			next = i;
			next++;
			MenuItem& cur = *i;
			if (cur.m_Target == this)
				item->m_Submenu->erase(i);
		}
	}

	// Use C# to extract all menu commands in the script
	void* params[1] = { mono_type_get_object (mono_domain_get(), mono_class_get_type(klass)) };
	MonoArray* commandArray = (MonoArray*)CallStaticMonoMethod("AttributeHelper", "ExtractContextMenu", params);
	if (commandArray == NULL)
		return;
	
	// Add the menu commands to the commands array and register it with the menu system
	for (int i=0;i<mono_array_length(commandArray);i++)
	{
		MonoMenuItemBinding& binding = GetMonoArrayElement<MonoMenuItemBinding> (commandArray, i);
		
		string menuItem = MonoStringToCpp(binding.menuItem);
		string execute = MonoStringToCpp(binding.execute);
		
		if (execute.empty ())
			continue;
		
		MenuController::AddContextItem ("MonoBehaviour/" + menuItem, execute, this, 1000000);
	}
}

static ScriptContextCommands *gScriptContextCommands = NULL;

void ScriptContextCommandsRegisterMenu ();

void ScriptContextCommandsRegisterMenu () {
	gScriptContextCommands = new ScriptContextCommands ();
	MenuController::AddContextItem ("UNUSED_PROXY_ScriptContextCommands", "", gScriptContextCommands);
}

STARTUP (ScriptContextCommandsRegisterMenu)	// Call this on startup.