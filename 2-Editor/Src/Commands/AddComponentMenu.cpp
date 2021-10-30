#include "UnityPrefix.h"
#include "Editor/Src/ComponentUtility.h"
#include "Runtime/Misc/ComponentRequirement.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

using namespace std;

struct AddComponentMenu;

void SetupComponentMenu (AddComponentMenu* menu);
void ScriptsDidChange ();
static bool PerformAllowComponentReplacementDialog (const vector_set<int>& allRequiredComponents, set<GameObject*>& selection);
static bool PerformAllowComponentConflictDialog (int classID, const vector_set<int>& conflictingComponents, set<GameObject*>& selection);
static void ExtractRequiredComponentsFromScriptRecursive (MonoClass* klass, vector_set<int>& allRequiredComponents);

struct AddComponentMenu : public MenuInterface
{
	bool needsRebuild;
	AddComponentMenu () { needsRebuild = true; }
	
	virtual bool Validate (const MenuItem &menuItem)
	{
		if (menuItem.m_Command.find ("ADD") == 0)
			return MonoObjectToBool (CallStaticMonoMethod ("AddComponentWindow", "ValidateAddComponentMenuItem"));
		
		/// \todo Validate the addability of the go directly.
		return !GetGameObjectSelection (kOnlyUserModifyableSelection).empty ();
	}

	virtual void Update ()
	{
		if (needsRebuild)
		{
			MenuController::RemoveMenuItem ("Component");
			SetupComponentMenu (this);
			RebuildMenu ();
			needsRebuild = false;
		}
	}
	
	virtual void Execute (const MenuItem &menuItem)
	{
		if (menuItem.m_Command.find ("ADD") == 0)
		{
			ScriptingInvocation("UnityEditor","AddComponentWindow","ExecuteAddComponentMenuItem").Invoke();
			return;
		}
		
		set<GameObject*> selection = GetGameObjectSelection (kOnlyUserModifyableSelection);
		AddComponentToGameObjects (menuItem, selection);
	}
	
	virtual void ExecuteOnGameObjects (const MenuItem &menuItem, vector<GameObject*> &selection)
	{
		set<GameObject*> goSet(selection.begin(), selection.end());
		AddComponentToGameObjects (menuItem, goSet);
	}
	
	static void AddComponentToGameObjects (const MenuItem &menuItem, set<GameObject*> &selection)
	{
		int classID = atoi (menuItem.m_Command.c_str());
		int scriptInstanceID = 0;
		if (menuItem.m_Command.find ("SCRIPT") == 0)
			scriptInstanceID = atoi (menuItem.m_Command.c_str() + strlen ("SCRIPT"));
		// Add script
		if (scriptInstanceID != 0)
		{
			MonoScript* script = dynamic_instanceID_cast<MonoScript*> (scriptInstanceID);
			string error;
			if (script != NULL)
			{
				// Extract component requirements
				if (script->GetClass ())
				{
					vector_set<int> allRequiredComponentsForReplacement;
					ExtractRequiredComponentsFromScriptRecursive (script->GetClass (), allRequiredComponentsForReplacement);
					if (!PerformAllowComponentReplacementDialog(allRequiredComponentsForReplacement, selection))
						return;
				}
				
				for (set<GameObject*>::iterator i=selection.begin ();i != selection.end ();i++)
				{
					string actionName = "Add " + (std::string)script->GetScriptClassName();
					AddComponentUndoable (**i, ClassID (MonoBehaviour), script, &error);
				}
			}
			else
			{
				error = Format("Script %s doesn't exist", menuItem.m_Name.c_str ());
			}

			if (!error.empty ())
			{
				DisplayDialog ("Can't add script", error, "Ok");
			}
		}
		// Add component
		else
		{
			string className = Object::ClassIDToString (classID);

			// Can't add a component if we are already inserted and the component doesn't allow multiple insertion!
			for (set<GameObject*>::iterator i=selection.begin ();i != selection.end ();i++)
			{
				GameObject& go = **i;

				// We can't add a component if it has conflicting components.  This is included when checking if we can add a component but we want to give a specific reason here i.e. a component conflict.
				Unity::Component* conflictingComponent = go.FindConflictingComponentPtr (classID);
				if (conflictingComponent)
				{
					string warning = Format ("Can't add component '%s' to %s because it conflicts with the existing '%s' derived component!",
						Object::ClassIDToString (classID).c_str (), go.GetName (), Object::ClassIDToString (conflictingComponent->GetClassID ()).c_str ());
					DisplayDialog ("Can't add due to a conflicting component!", warning, "Cancel");
					return;
				}

				if (!CanAddComponent (go, classID))
				{
					string warning = Format ("The component %s can't be added because %s already contains the same component.", className.c_str (), go.GetName ());
					DisplayDialog ("Can't add the same component multiple times!", warning, "Cancel");
					return;
				}
			}

			if (!IsClassSupportedOnSelectedBuildTarget(classID))
			{
				string warning = Format ("The component %s can't be added because it is not supported by selected platform.", className.c_str());
				DisplayDialog ("Can't add component!", warning, "Cancel");
				return;
			}

			vector_set<int> allRequiredComponents;
			FindAllRequiredComponentsRecursive(classID, allRequiredComponents);
			if (!PerformAllowComponentReplacementDialog(allRequiredComponents, selection))
				return;

			const vector_set<int>& conflictingComponents = FindConflictingComponents(classID);
			if (!PerformAllowComponentConflictDialog(classID, conflictingComponents, selection))
				return;
			
			for (set<GameObject*>::iterator i=selection.begin ();i != selection.end ();i++)
			{
				string error;
				AddComponentUndoable (**i, classID, NULL, &error);
				if (!error.empty ())
					DisplayDialog ("Can't add component", error, "Ok");
			}
		}
	}
};

static void ExtractRequiredComponentsFromScriptRecursive (MonoClass* klass, vector_set<MonoClass*>& allRequiredComponents)
{
	if (!allRequiredComponents.insert(klass).second)
		return;
	
	MonoArray* array = RequiredComponentsOf(klass);
	if (array)
	{
		for (int j=0;j<mono_array_length(array);j++)
		{
			MonoObject* requiredClassObject = GetMonoArrayElement<MonoObject*>(array, j);
			if (requiredClassObject == NULL)
				continue;
			
			MonoClass* requiredClass = GetScriptingTypeRegistry().GetType (requiredClassObject);
			ExtractRequiredComponentsFromScriptRecursive(requiredClass, allRequiredComponents);
		}
	}		
}

// Extract all required C++ components for the klass using [ComponentRequirement attribute]
static void ExtractRequiredComponentsFromScriptRecursive (MonoClass* klass, vector_set<int>& allRequiredComponents)
{
	vector_set<MonoClass*> allRequiredClasses;
	ExtractRequiredComponentsFromScriptRecursive(klass, allRequiredClasses);
	vector_set<int> allRequiredComponentsForReplacement;

	for (int i=0;i<allRequiredClasses.size();i++)
	{
		int requiredClassID = Scripting::GetClassIDFromScriptingClass(allRequiredClasses[i]);
		if (requiredClassID != -1 && requiredClassID != ClassID(MonoBehaviour))
			FindAllRequiredComponentsRecursive(requiredClassID, allRequiredComponents);
	}
}


int ShowComponentReplacementDialog (GameObject& go, int existingClassID, const std::string& newClassName)
{
	Unity::Component* com = go.QueryComponentT<Unity::Component> (existingClassID);
	DebugAssert(com);
	string existingClassName = com->GetClassName ();
	
	string warning = Format ("A %s is already added, do you want to replace it with a %s?", existingClassName.c_str(), newClassName.c_str());
	int result = DisplayDialogComplex ("Replace existing component?", warning, "Replace", "Add", "Cancel");
	return result;
}


static bool RemoveComponentIfPresent (GameObject& go, int classID)
{
	Unity::Component* com = go.QueryComponentT<Unity::Component> (classID);
	if (!com)
		return true;

	string error;
	RemoveComponentUndoable (*com, &error);

	if (!error.empty())
	{
		DisplayDialog ("Can't remove component", error, "Ok");
		return false;
	}
	return true;
}


static int FindFirstReplaceableComponent(GameObject& go, int newComponentID, int replaceComponent)
{
	if (replaceComponent == -1)
		return -1;

	bool allowMultipleInclusion = DoesComponentAllowMultipleInclusion (newComponentID);

	GameObject::Container& goComponents = go.GetComponentContainerInternal();
	GameObject::Container::const_iterator i;
	for (i = goComponents.begin (); i != goComponents.end (); ++i)
	{
		//current component is derived from a replaceable component.
		if(Object::IsDerivedFromClassID (i->first, replaceComponent))
		{
			//If the component does not allow multiple inclusion then its replaceable.
			if (!allowMultipleInclusion)
				return i->first;

			//it should be a different component that we are trying to add
			if (i->first != newComponentID)
				return i->first;

			//reaching here, means the Component supports multiple inclusions, and we are trying to add exactly the same component twice.
			//then we just add the new component, since it does not much sense to replace the same component.
		}
	}
	return -1;
}


static bool PerformAllowComponentReplacementDialog (const vector_set<int>& allRequiredComponents, set<GameObject*>& selection)
{
	for (int i=0;i<allRequiredComponents.size();i++)
	{
		string className = Object::ClassIDToString (allRequiredComponents[i]);
		
		int requiredComponent = allRequiredComponents[i];
		int replaceComponent = GetAllowComponentReplacementClass (requiredComponent);
		if (replaceComponent == -1)
			continue;

		bool shouldReplace = false;
		for (set<GameObject*>::iterator i=selection.begin ();i != selection.end ();i++)
		{
			GameObject& go = **i;

			int repleceableComponentID = FindFirstReplaceableComponent(go, requiredComponent, replaceComponent);
			if(repleceableComponentID > -1)
			{
				// Can't add a component if we are already inserted and the component doesn't allow multiple insertion!
				int result = ShowComponentReplacementDialog (go, repleceableComponentID, className);
				if (result == 2)
					return false;
				if (result == 0)
					shouldReplace = true;

				break;
			}
		}
		if (!shouldReplace)
			continue;
			
		for (set<GameObject*>::iterator i=selection.begin ();i != selection.end ();i++)
		{
			GameObject& go = **i;

			// Got the required component already
			if (go.CountDerivedComponents (requiredComponent) != 0)
				continue;

			if (!RemoveComponentIfPresent (go, replaceComponent))
				return false;
		}
	}
	return true;
}


static bool PerformAllowComponentConflictDialog (int classID, const vector_set<int>& conflictingComponents, set<GameObject*>& selection)
{
	vector_set<int> foundConflicting;
	std::string names;

	// Find conflicting components in the selection
	for (size_t i=0;i<conflictingComponents.size();i++)
	{
		int conflictID = conflictingComponents[i];

		for (set<GameObject*>::iterator isel = selection.begin (); isel != selection.end (); ++isel)
		{
			GameObject& go = **isel;
			if (go.CountDerivedComponents (conflictID) != 0)
			{
				if (foundConflicting.insert(conflictID).second)
				{
					if (!names.empty())
						names += ", ";
					names += Object::ClassIDToString (conflictID);
				}
			}
		}
	}
	if (foundConflicting.empty())
		return true; // Nothing conflicts!

	string msg = Format("Adding a %s requires removing %s", Object::ClassIDToString (classID).c_str(), names.c_str());
	bool shouldRemove = DisplayDialog ("Remove conflicting components?", msg, "Remove", "Cancel");
	if (!shouldRemove)
		return false;

	// Remove conflicting components from selection
	for (set<GameObject*>::iterator isel = selection.begin (); isel != selection.end (); ++isel)
	{
		GameObject& go = **isel;
		for (vector_set<int>::const_iterator i = foundConflicting.begin(); i != foundConflicting.end(); ++i)
			if (!RemoveComponentIfPresent (go, *i))
				return false;
	}
	return true;
}


void AddComponentRegisterMenu ();

inline bool ScriptSort (MonoScript* lhs, MonoScript* rhs)
{
	int namespaces = StrICmp(lhs->GetNameSpace(), rhs->GetNameSpace());

	if(namespaces == 0) 
		return StrICmp (lhs->GetName(), rhs->GetName()) < 0;
	else 
	{
		// Alphabetical order would make scripts without namespace come first, but we want the other way
		if(lhs->GetNameSpace().empty() || rhs->GetNameSpace().empty()) 
			return namespaces > 0;
		else
			return namespaces < 0;
	}
}

void SetupComponentMenu (AddComponentMenu* menu)
{
	// Add Add... menu item that opens Add Component dropdown
	MenuController::AddMenuItem ("Component/Add... %#a", "ADD", menu, 20);
	
	// Add normal components to the menu
	const int kMenuPos = 20;
	const ComponentsHierarchy &hier = GetComponentsHierarchy();
	for (ComponentsHierarchy::const_iterator i =hier.begin(); i != hier.end(); i++)
	{
		const string menuName = string ("Component/") + i->first + "/";
		const string sepName = string ("Component/") + i->first + "/";
		for (int j=0;j<i->second.size ();j++)
		{
			int classID = i->second[j].classID;
			const string &className = i->second[j].name;
			if (classID == 0)
				MenuController::AddSeparator (sepName, kMenuPos);
			else
			{
				string command;
				command = Format ("%d", classID);
				MenuController::AddMenuItem (menuName + MangleVariableName(className.c_str()), command, menu, kMenuPos);
			}
		}
	}

	// Add scripts to the menu
	const int kScriptMenuPos = 20;
	MonoScriptManager::AllScripts temp = GetMonoScriptManager ().GetAllRuntimeScripts ();
	vector<MonoScript*> allScripts (temp.begin(), temp.end());
	sort (allScripts.begin (), allScripts.end (), ScriptSort);

	MonoMethod* getComponentMenuNameMethod = FindStaticMonoMethod ("AttributeHelper", "GetComponentMenuName");
	for (vector<MonoScript*>::iterator i=allScripts.begin ();i != allScripts.end ();i++)
	{
		MonoScript* script = *i;
		string path = Format("Scripts/%s", MangleVariableName(script->GetName ()));
		
		if (script && script->GetScriptType () != kScriptTypeMonoBehaviourDerived)
			continue;
		
		if (script && script->GetClass())
		{
			ScriptingInvocation invocation(getComponentMenuNameMethod);
			invocation.AddObject(mono_type_get_object(mono_domain_get(), mono_class_get_type(script->GetClass())));
			MonoString* monoMenuPath = (MonoString*) invocation.Invoke();
			if (monoMenuPath)
			{
				path = MonoStringToCpp(monoMenuPath);
				if (path.empty())
					continue;
			} 
			else 
			{
				if (!script->GetNameSpace().empty())
					path = Format("Scripts/%s/%s", script->GetNameSpace().c_str(), MangleVariableName(script->GetName ()));
			}
		}
		
		string command;
		command = Format ("SCRIPT%d", script->GetInstanceID());
		
		MenuController::AddMenuItem ("Component/" + path, command, menu, kScriptMenuPos);
	}
}

AddComponentMenu* gComponentMenu = NULL;

void ScriptsDidChange ()
{
	if (gComponentMenu)
		gComponentMenu->needsRebuild = true;
}

void AddComponentRegisterMenu ()
{
	gComponentMenu = new AddComponentMenu ();
	MenuController::AddContextItem ("UNUSED_PROXY_AddComponent", "", gComponentMenu);

	GlobalCallbacks::Get().didReloadMonoDomain.Register(ScriptsDidChange);
}



STARTUP (AddComponentRegisterMenu)	// Call this on startup.Component* AddComponentShowAlert (GameObject &go, int classID)
