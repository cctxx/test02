#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/Mono/MonoScript.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Utility/ObjectNames.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Editor/Src/AssetPipeline/TextureImporter.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/ComponentUtility.h"
#include "Editor/Src/EditorHelper.h" 
#include "Editor/Src/SceneInspector.h"
#include "Runtime/Graphics/ParticleSystem/ParticleSystem.h"


struct MiscContextMenus : public MenuInterface {

	virtual bool Validate (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) {
		case 0:
			for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			{
				if (!IsUserModifiable(**i))
					return false;
			}
			return true;
		case 1:
			return true;
		case 2:
			return true;
		case 3:
			{
				EditorExtension* extendable = dynamic_pptr_cast<EditorExtension*> (menuItem.context[0]);	
				return extendable != NULL && extendable->GetPrefabParentObject ().IsValid();
			}
		case 4:
			for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			{
				Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (*i);
				if (com == NULL || com->GetClassName() == "Transform" || !IsUserModifiable(**i) || com->GetGameObjectPtr() == NULL)
					return false;
			}
			return true;
				
		case 5: // move up
			return MoveComponentUp (menuItem.context, true);
				
		case 6: // move down
			return MoveComponentDown (menuItem.context, true);

		case 7: // copy
			return CopyComponent (menuItem.context, true);

		case 8: // paste as new component
			return PasteComponentAsNew (menuItem.context, true);

		case 9: // paste component values
			return PasteComponentValues (menuItem.context, true);
		}
		return false;
	}

	virtual void UpdateContextMenu (std::vector<Object*> &context, int userData) { 

		// Cleanup context menu
		MenuController::RemoveMenuItem("CONTEXT/Object/Reset");
		MenuController::RemoveMenuItem("CONTEXT/EditorExtension/Revert to Prefab");
		MenuController::RemoveMenuItem("CONTEXT/MonoBehaviour/Edit Script");
		MenuController::RemoveMenuItem("CONTEXT/Component/Remove Component");
		MenuController::RemoveMenuItem("CONTEXT/GameObject/Select Prefab");
		
		MenuController::RemoveMenuItem("CONTEXT/Component/Move Up");
		MenuController::RemoveMenuItem("CONTEXT/Component/Move Down");
		MenuController::RemoveMenuItem("CONTEXT/Component/Copy Component");
		MenuController::RemoveMenuItem("CONTEXT/Component/Paste Component As New");
		MenuController::RemoveMenuItem("CONTEXT/Component/Paste Component Values");

		// Add context menu items again
		MenuController::AddContextItem ("Object/Reset", "0", this);
		for (std::vector<Object*>::iterator i = context.begin(); i!=context.end(); i++)
		{
			EditorExtension* extendable = dynamic_pptr_cast<EditorExtension*> (*i);
			if (extendable && IsPrefabInstanceWithValidParent(extendable))
			{
				MenuController::AddContextItem ("EditorExtension/Revert to Prefab", "1", this);
				break;
			}
		}

		MenuController::AddContextItem ("MonoBehaviour/Edit Script", "2", this, 600);
		MenuController::AddContextItem ("GameObject/Select Prefab", "3", this, 600);
		MenuController::AddContextItem ("Component/Remove Component", "4", this, 500);
		MenuController::AddContextItem ("Component/Move Up", "5", this, 500);
		MenuController::AddContextItem ("Component/Move Down", "6", this, 500);
		MenuController::AddContextItem ("Component/Copy Component", "7", this, 500);
		MenuController::AddContextItem ("Component/Paste Component As New", "8", this, 500);
		MenuController::AddContextItem ("Component/Paste Component Values", "9", this, 500);
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx)
		{
		case 0:
		{
			for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			{
				Object* object = *i;

				std::string undoName = Append("Reset ", object->GetName());
				Unity::Component* component = dynamic_pptr_cast<Unity::Component*> (object);
				if (component)
					undoName = Append(undoName, " ").append(GetNiceObjectType(component));
				RegisterUndo(object, undoName);
				SmartResetObject(*object);
			}
		}
		break;
		case 1:
		{
			for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			{
				EditorExtension* object = dynamic_pptr_cast<EditorExtension*> (*i);
				if (object)
				{
					SmartResetToInstantiatedPrefabState (*object);
					GetSceneTracker().ForceReloadInspector();
				}
			}
		}
		break;
		case 2:
		{
			// Regardless of whether we're editing multiple objects, we only want to open the script once.
			MonoBehaviour* mono = dynamic_pptr_cast<MonoBehaviour*> (menuItem.context[0]);
			if (mono)
				OpenAsset (mono->GetScript ().GetInstanceID ());
		}
		break;
		case 3:
		{
			EditorExtension* extendable = dynamic_pptr_cast<EditorExtension*> (menuItem.context[0]);
			if (extendable)
				SetActiveObject (extendable->GetPrefabParentObject ());
		}
		break;
		case 4:
		{	
			for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
			{
				Unity::Component* com = dynamic_pptr_cast<Unity::Component*> (*i);
				if (com == NULL || com->GetClassName() == "Transform" || !IsUserModifiable (*com) || com->GetGameObjectPtr() == NULL)
					return;
								
				string error;
				RemoveComponentUndoable (*com, &error);
				
				if (!error.empty ())
					DisplayDialog ("Can't remove component", error, "Ok");
			}
		}
		break;
		case 5: // move up
			MoveComponentUp (menuItem.context, false);
			break;
		case 6: // move down
			MoveComponentDown (menuItem.context, false);
			break;
		case 7: // copy
			CopyComponent (menuItem.context, false);
			break;
		case 8: // paste as new component
			PasteComponentAsNew (menuItem.context, false);
			break;
		case 9: // paste component values
			PasteComponentValues (menuItem.context, false);
			break;				
		}
		GetSceneTracker ().ForceReloadInspector ();
	}
};

static MiscContextMenus *gMiscContextMenus;
void SetupMiscContextMenus ();
void SetupMiscContextMenus () {
	gMiscContextMenus = new MiscContextMenus ();
	// This is just a proxy. They get rebuilt in UpdateContextMenu
	MenuController::AddContextItem ("UNUSED_PROXY_SetupMisc", "", gMiscContextMenus);
}

STARTUP (SetupMiscContextMenus)	// Call this on startup.
