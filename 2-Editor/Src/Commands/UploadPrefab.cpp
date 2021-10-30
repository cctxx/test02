#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Selection.h"

struct UploadPrefab : public MenuInterface
{
	virtual bool Validate (const MenuItem &menuItem)
	{
		if (menuItem.m_Command == "10")
		{
			TempSelectionSet selection;
			Selection::GetSelection(selection);
			
			AddParentWithSamePrefab(selection);
			
			GameObject* root;
			Prefab* prefab;
			return FindValidUploadPrefab (selection, &prefab, &root);
		}
		else if (menuItem.m_Command == "11")
		{
			TempSelectionSet selection;
			Selection::GetSelection(selection);

			for (TempSelectionSet::iterator i=selection.begin();i!=selection.end();++i)
			{
				if (IsPrefabInstanceOrDisconnectedInstance(*i))
					return true;
			}
			return false;
		}
		else
		{
			return false;
		}

	}

	virtual void Execute (const MenuItem &menuItem)
	{
		if (menuItem.m_Command == "10")
		{
			TempSelectionSet selection;
			Selection::GetSelection(selection);
			
			AddParentWithSamePrefab(selection);
			
			GameObject* root;
			Prefab* prefab;
			if (!FindValidUploadPrefab(selection, &prefab, &root))
				return UnityBeep();
			
			ReplacePrefab(*root, prefab, kAutoConnectPrefab);
		}
		else if (menuItem.m_Command == "11")
		{
			TempSelectionSet selection;
			Selection::GetSelection(selection);

			for (TempSelectionSet::iterator i=selection.begin();i!=selection.end();++i)
			{
				if (IsPrefabInstanceOrDisconnectedInstance(*i))
					DisconnectPrefabInstance(*i);
			}
		}
	}
};

static UploadPrefab *gUploadPrefab;
void UploadPrefabRegisterMenu ();
void UploadPrefabRegisterMenu () {
	gUploadPrefab = new UploadPrefab;

	MenuController::AddMenuItem ("GameObject/Apply Changes To Prefab", "10", gUploadPrefab);
	MenuController::AddMenuItem ("GameObject/Break Prefab Instance", "11", gUploadPrefab);
}

STARTUP (UploadPrefabRegisterMenu)	// Call this on startup.
