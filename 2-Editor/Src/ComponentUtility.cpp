#include "UnityPrefix.h"
#include "ComponentUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/ComponentRequirement.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/Undo/Undo.h"
#include "Editor/Src/CommandImplementation.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/src/Undo/Undo.h"

int ShowComponentReplacementDialog (GameObject& go, int existingClassID, const std::string& newClassName);



static bool MoveComponent (const std::vector<Object*>& context, bool up, bool validateOnly)
{
	bool anyValid = false;
	for (size_t i = 0; i < context.size(); ++i)
	{
		Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (context[i]);
		if (com == NULL || com->GetClassID() == ClassID(Transform) || !IsUserModifiable (*com) || com->GetGameObjectPtr() == NULL)
			continue;

		GameObject &go = com->GetGameObject();
		int idx = go.GetComponentIndex (com);
		if (up)
		{
			if (idx < 2)
				continue; // disallow moving up first two components (moving 2nd up would make it take place of Transform normally)
		}
		else
		{
			if (idx >= go.GetComponentCount()-1)
				continue;
		}

		if (!validateOnly)
		{
			if (!WarnPrefab (com))
				continue;

			int newIndex = up ? (idx-1) : (idx+1);
			com->GetGameObject().SwapComponents (idx, newIndex);
		}

		anyValid = true;
	}

	return anyValid;
}

bool MoveComponentUp (const std::vector<Object*>& context, bool validateOnly)
{
	return MoveComponent (context, true, validateOnly);
}

bool MoveComponentDown (const std::vector<Object*>& context, bool validateOnly)
{
	return MoveComponent (context, false, validateOnly);
}



bool CopyComponent (const std::vector<Object*>& context, bool validateOnly)
{
	if (context.size() != 1)
		return false; // can only copy when one object is selected

	Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (context[0]);
	if (com == NULL)
		return false; // can only copy components
	
	MonoBehaviour* monoBehavior = dynamic_pptr_cast<MonoBehaviour*> (context[0]);
	if(monoBehavior != NULL && monoBehavior->GetInstance() == NULL)
		return false;

	if (!validateOnly)
	{
		CopyComponentToPasteboard (com);
	}
	return true;
}



static bool HandleComponentPasteComponentReplacement (GameObject& go, int classID, const std::string& className)
{
	int res = ShowComponentReplacementDialog (go, classID, className);
	if (res == 2) // cancel
		return false;
	
	if (res == 0) // replace: remove existing one
	{
		Unity::Component* com = go.QueryComponentT<Unity::Component> (classID);
		if (com)
		{
			// Remove component
			string error;
			if (CanRemoveComponent(*com, &error))
				DestroyObjectUndoable (com);							
		}
	}
	
	return true;
}


bool PasteComponentAsNew (const std::vector<Object*>& context, bool validateOnly)
{
	int classID = 0;
	if (!HasComponentInPasteboard(classID))
		return false;
	std::string className = Object::ClassIDToString (classID);
	bool replacement = false;
	int replacementClassID = GetAllowComponentReplacementClass(classID);
	if (replacementClassID != -1)
	{
		replacement = true;
		classID = replacementClassID;
	}

	bool anyValid = false;

	for (size_t i = 0; i < context.size(); ++i)
	{
		Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (context[i]);
		if (com == NULL || !IsUserModifiable (*com))
			continue;
		GameObject* go = com->GetGameObjectPtr();
		if (!go)
			continue;

		if (!CanAddComponent (*go, classID))
		{
			if (replacement)
			{
				if (!validateOnly)
				{
					// For now only Collider can be replaced.
					// If the replacement is extended to MonoBehaviour, do check CanReplaceComponent.
					Assert(classID == ClassID(Collider));
					if (!HandleComponentPasteComponentReplacement (*go, classID, className))
						continue; // do not paste new component
				}
			}
			else
			{
				continue; // can't paste a new duplicate component
			}
		}

		if (!validateOnly)
		{
			PasteComponentFromPasteboard (go);
		}

		anyValid = true;
	}

	return anyValid;
}


bool PasteComponentValues (const std::vector<Object*>& context, bool validateOnly)
{
	bool anyValid = false;

	for (size_t i = 0; i < context.size(); ++i)
	{
		Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (context[i]);
		if (com == NULL || !IsUserModifiable (*com) || com->GetGameObjectPtr() == NULL)
			continue;

		if (!HasMatchingComponentInPasteboard(com))
			continue;

		if (!validateOnly)
		{
			PasteComponentValuesFromPasteboard (com);
		}

		anyValid = true;
	}

	return anyValid;
}

static void RemoveCoupledComponents (Unity::Component* com)
{
	const int coupledClassID = com->GetCoupledComponentClassID();
	if (coupledClassID < 0)
		return;

	GameObject* go = com->GetGameObjectPtr();
	if (!go)
		return;

	Unity::Component* coupled = go->QueryComponentT<Unity::Component>(coupledClassID);
	if (coupled)
	{
		string error;
		if (CanRemoveComponent (*coupled, &error))
			DestroyObjectUndoable (coupled);

		if (!error.empty ())
			LogString (Format("Can't remove component: %s", error.c_str()));
	}
}

bool RemoveComponentUndoable (Unity::Component& component, std::string* error)
{
	if (CanRemoveComponent (component, error))
	{
		// Remove any coupled components
		RemoveCoupledComponents (&component);

		DestroyObjectUndoable (&component);

		return true;
	}

	return false;
}
