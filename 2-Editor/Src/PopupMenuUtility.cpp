#include "UnityPrefix.h"
#include "PopupMenuUtility.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "MenuController.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "EditorHelper.h"

using namespace std;

const char* kName = "CONTEXT/TEMPORARY-OBJECT-DISPLAY";

static void AddContextMenu (string klassName) 
{
	// Add any context menus we may have...
	MenuItem *item = MenuController::FindItem ("CONTEXT/" + klassName);

	MenuItem *newMenu = MenuController::FindItem (kName);
	AssertIf(newMenu == NULL);
	if (item && item->m_Submenu)
	{
		MenuController::DeepCopyReplaceSame(*item, *newMenu);
	}
}

static void RecursiveAddContextMenu (int classID) 
{
	if (classID != ClassID (Object))
		RecursiveAddContextMenu (Object::GetSuperClassID (classID));
	
	AddContextMenu (Object::ClassIDToString(classID));
}

static void RecursiveAddContextMenu (MonoClass* klass) 
{
	MonoClass* parent = mono_class_get_parent (klass);
	if (parent && parent != MONO_COMMON.monoBehaviour)
		RecursiveAddContextMenu(parent);
	
	AddContextMenu (mono_class_get_name(klass));
}

void PrepareObjectContextPopupMenu (std::vector<Object*> &context, int contextData)
{
	MenuController::UpdateContextMenu (context, contextData);
	
	MenuController::RemoveMenuItem (kName);
	MenuController::AddMenuItem (kName, "", NULL);
	MenuItem* newMenu = MenuController::FindItem (kName);
	newMenu->m_Submenu = new Menu();
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*>(context[0]);
	if (behaviour && behaviour->IsScriptableObject ())
	{
		MonoScript* script = dynamic_pptr_cast<MonoScript*>(behaviour->GetScript());
		if (script && IsAssetAvailable (script->GetInstanceID ())) // Only add the "Edit Script" menu item if a script file is available (e.g. not the case for a GUISkin or Terrain)
		{
			AddContextMenu (behaviour->GetClassName ());
		}
	} 
	else // We avoid the recursive add for ScriptableObject because it doesn't actually work as a Component and thus we don't want to add menu items for this
	{
		RecursiveAddContextMenu (context[0]->GetClassID());
	}
	
	if (behaviour && behaviour->GetClass ())
		RecursiveAddContextMenu(behaviour->GetClass ());
}

void DisplayObjectContextPopupMenu (const Rectf &pos, std::vector<Object*> &context, int contextData)
{
	if (context[0] == NULL)
	{
		ErrorString ("Context is null.");
		return;
	}

	PrepareObjectContextPopupMenu(context, contextData);
	MenuController::DisplayPopupMenu (pos, kName, context, contextData);
}


class CustomPopupMenuInterface : public MenuInterface
{
public:
	
	CustomPopupMenuInterface()
		: monoUserData(0), monoDelegate(0)
	{ }
	
	int monoUserData;
	int monoDelegate;
	vector<string> enums;
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		return true;
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		MonoObject* delegate = mono_gchandle_get_target(monoDelegate);
		MonoObject* userData = mono_gchandle_get_target(monoUserData);
		if (delegate)
		{
			int selected = StringToInt (menuItem.m_Command);
			void* params[] = { userData, Scripting::StringVectorToMono(enums), &selected };
			MonoException* exc;
			mono_runtime_delegate_invoke(delegate, params, &exc);
			if (exc)
				Scripting::LogException(exc, 0);
		}

		if (monoUserData)
			mono_gchandle_free(monoUserData);
		monoUserData = 0;

		if (monoDelegate)
			mono_gchandle_free(monoDelegate);
		monoDelegate = 0;
	}
};

CustomPopupMenuInterface* gCustomPopupInterface = NULL;


void CleanupCustomContextMenuHandles ()
{
	if (gCustomPopupInterface)
	{
		if (gCustomPopupInterface->monoUserData)
			mono_gchandle_free(gCustomPopupInterface->monoUserData);
		gCustomPopupInterface->monoUserData = 0;
		if (gCustomPopupInterface->monoDelegate)
			mono_gchandle_free(gCustomPopupInterface->monoDelegate);
		gCustomPopupInterface->monoDelegate = 0;
	}
}

void DisplayCustomContextPopupMenu (const Rectf &pos, const vector<string>& enums, const vector<bool> &enabled, const vector<int>& selected, MonoObject* monoDelegate, MonoObject* monoUserData)
{
	if (gCustomPopupInterface == NULL )
	{
		gCustomPopupInterface = new CustomPopupMenuInterface();
		RegisterUnloadDomainCallback(CleanupCustomContextMenuHandles);
	}

	if (gCustomPopupInterface->monoUserData)
	{
		ErrorIf(!mono_gchandle_is_in_domain(gCustomPopupInterface->monoUserData, mono_domain_get()));
		mono_gchandle_free(gCustomPopupInterface->monoUserData);
		gCustomPopupInterface->monoUserData = 0;
	}
	if (gCustomPopupInterface->monoDelegate)
	{
		ErrorIf(!mono_gchandle_is_in_domain(gCustomPopupInterface->monoDelegate, mono_domain_get()));
		mono_gchandle_free(gCustomPopupInterface->monoDelegate);
		gCustomPopupInterface->monoDelegate = 0;
	}

	if (enums.empty())
		return;

	gCustomPopupInterface->enums = enums;
	
	if (monoUserData)
		gCustomPopupInterface->monoUserData = mono_gchandle_new (monoUserData, 0);
		
	if (monoDelegate)
		gCustomPopupInterface->monoDelegate = mono_gchandle_new (monoDelegate, 0);
	
	
	MenuController::RemoveMenuItem (kName);
	for (int i=0;i<enums.size();i++)
	{
		MenuController::AddMenuItem (string(kName) + "/" + enums[i], IntToString(i), gCustomPopupInterface);
		MenuController::SetEnabled (string(kName) + "/" + enums[i], enabled[i]);
	}
	
	for (int i=0;i<selected.size();i++)
	{
		if (selected[i] >= 0 && selected[i] < enums.size())
			MenuController::SetChecked (string(kName) + "/" + enums[selected[i]], true);
	}
	
	std::vector<Object*> context;
	MenuController::DisplayPopupMenu (pos, kName, context, 0);
}
