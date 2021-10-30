#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Editor/Src/LicenseInfo.h"
#include "Runtime/Mono/MonoManager.h"

struct EditProjectSettings : public MenuInterface 
{
	virtual bool Validate (const MenuItem &menuItem) 
	{
		return true;
	}

	void ShowObjectInInspector(Object *asset)
	{
		CallStaticMonoMethod("InspectorWindow", "ShowWindow");
		SetActiveObject(asset);
	}

	void ShowObjectInInspector(string assetName)
	{
		ShowObjectInInspector (AssetInterface::Get ().GetSingletonAsset (assetName));
	}

	virtual void Execute (const MenuItem &menuItem) 
	{
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) 
		{
		case 0:
			ShowObjectInInspector ("InputManager");
			break;
		case 1:
			ShowObjectInInspector ("TagManager");
			break;
		case 2:
			ShowObjectInInspector ("AudioManager");
			break;
		case 3:
			ShowObjectInInspector ("TimeManager");
			break;
		case 4:
			ShowObjectInInspector ("PlayerSettings");
			break;
		case 5:
			ShowObjectInInspector ("PhysicsManager");
			break;
		case 6:
			ShowObjectInInspector (GetManagerPtrFromContext (ManagerContext::kRenderSettings));
			break;
		case 7:
			ShowObjectInInspector ("NetworkManager");
			break;

		case 13:
			ShowObjectInInspector ("QualitySettings");
			break;
		case 14:
			ShowObjectInInspector ("EditorSettings");
			break;
		case 15:
			ShowObjectInInspector ("MonoManager");
			break;
		#if ENABLE_2D_PHYSICS
		case 16:
			ShowObjectInInspector ("Physics2DSettings");
			break;
		#endif
		case 17:
			ShowObjectInInspector ("GraphicsSettings");
			break;
		}
	}
};

static EditProjectSettings *gEditProjectSettings;
void EditProjectSettingsRegisterMenu ();
void EditProjectSettingsRegisterMenu () 
{
	const int placement = 300;
	gEditProjectSettings = new EditProjectSettings;

	MenuController::AddMenuItem ("Edit/Project Settings/Input", "0", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Tags and Layers", "1", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Audio", "2", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Time", "3", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Player", "4", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Physics", "5", gEditProjectSettings, placement);
	#if ENABLE_2D_PHYSICS
	MenuController::AddMenuItem ("Edit/Project Settings/Physics 2D", "16", gEditProjectSettings, placement);
	#endif
	MenuController::AddMenuItem ("Edit/Project Settings/Quality", "13", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Graphics", "17", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Network", "7", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Editor", "14", gEditProjectSettings, placement);
	MenuController::AddMenuItem ("Edit/Project Settings/Script Execution Order", "15", gEditProjectSettings, placement);

	MenuController::AddMenuItem ("Edit/Render Settings", "6", gEditProjectSettings, placement);
}

STARTUP (EditProjectSettingsRegisterMenu)	// Call this on startup.
