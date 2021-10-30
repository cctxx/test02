#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Editor/Src/Undo/Undo.h"

struct GOParent : public MenuInterface
{
	std::set<Transform *> GetValidGameObjects (bool requiresParent)
	{
		std::set<Transform *> sel = GetTransformSelection ();
		if (!requiresParent)
			return sel;
		else
		{
			std::set<Transform *> result;
			for (std::set<Transform *>::iterator i = sel.begin(); i != sel.end(); i++)
			{
				if ((**i).GetParent () != NULL)
					result.insert (*i);
			}
			return result;
		}
	}

	virtual bool Validate (const MenuItem &menuItem)
	{
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx)
		{
		case 0:
			return GetValidGameObjects (false).size () > 1 && 
				   GetActiveTransform () != NULL && !GetActiveTransform()->IsPrefabParent ();
		case 1:
			return !GetValidGameObjects (true).empty();
		}
		return false;
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx)
		{
		case 0:
		{
			std::set<Transform *> sel = GetValidGameObjects (false);
			
			Transform *activeTC = GetActiveTransform ();
			// We should never have been called if this fails
			AssertIf (!(sel.size() > 1 && !activeTC->IsPrefabParent ()));
			
			// Only the old parents and the new parent need to have their losing prefab changed
			std::set<EditorExtension*> prefabChanges;
			prefabChanges.insert (activeTC);
			for (std::set<Transform *>::iterator i = sel.begin(); i != sel.end(); i++)
			{
				Transform* transform = *i;
				if (transform != activeTC && !IsPrefabTransformParentChangeAllowed(*transform, activeTC))
					prefabChanges.insert (transform->GetParent ()); 
			}
			
			if (WarnPrefab (prefabChanges))
			{
				for (std::set<Transform *>::iterator i = sel.begin(); i != sel.end(); i++)
				{
					if (*i != activeTC)
					{
						Transform& transform = **i;
						SetTransformParentUndo (transform, activeTC, "Make Parent");
					}
				}
			}

		}
		break;
				
		case 1:
		{
			std::set<Transform *> sel = GetValidGameObjects (true);
			
			// Only the old parents need to have their losing prefab changed
			std::set<EditorExtension*> prefabChanges;
			for (std::set<Transform *>::iterator i = sel.begin(); i != sel.end(); i++)
			{
				Transform& transform = **i;
				if (!IsPrefabTransformParentChangeAllowed (transform, NULL))
					prefabChanges.insert (&transform);
			}
			
			if (WarnPrefab (prefabChanges))
			{
				for (std::set<Transform *>::iterator i = sel.begin(); i != sel.end(); i++)
				{
					Transform& transform = **i;
					SetTransformParentUndo (transform, NULL, "Clear Parent");
				}
			}

		}
		break;
		
		}
	}
};

void GOParentRegisterMenu ()
{
	GOParent* menu = new GOParent;

	MenuController::AddMenuItem ("GameObject/Make Parent", "0", menu,100);
	MenuController::AddMenuItem ("GameObject/Clear Parent", "1", menu,100);
}

STARTUP (GOParentRegisterMenu)	// Call this on startup.
