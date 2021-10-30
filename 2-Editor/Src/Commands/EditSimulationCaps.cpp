#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Editor/Src/Application.h"
#include "Runtime/Utilities/ArrayUtility.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Misc/PlayerSettings.h"

static const char* kNames[GraphicsCaps::kEmulCount] = {
"Edit/Graphics Emulation/No Emulation",
"Edit/Graphics Emulation/Shader Model 3",
"Edit/Graphics Emulation/Shader Model 2",
"Edit/Graphics Emulation/OpenGL ES 2.0",
"Edit/Graphics Emulation/Xbox 360",
"Edit/Graphics Emulation/Playstation 3",
"Edit/Graphics Emulation/DirectX 11 9.1 (Shader Model 2) No Fixed Function",
"Edit/Graphics Emulation/DirectX 11 9.3 (Shader Model 3)"
};

void ReloadAllMaterialInspectors(); // VariousStubs.cpp


struct EditNetSimulationCaps : public MenuInterface {
	virtual bool Validate (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		return gGraphicsCaps.CheckEmulationSupported((GraphicsCaps::Emulation)idx);
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		Execute((GraphicsCaps::Emulation)idx);
		SetChecked("Edit/Graphics Emulation/" + menuItem.m_Name);
	}
	
	void Execute(GraphicsCaps::Emulation emulation)
	{
		gGraphicsCaps.SetEmulation(emulation);
		ReloadAllMaterialInspectors();
		GetApplication().SetSceneRepaintDirty();		
		GetApplication().UpdateMainWindowTitle(); // title might need to change, e.g. "DX11 on DX9 GPU"
	}
	
	void SetChecked (const std::string& name)
	{
		for (int i=0;i<sizeof(kNames) / sizeof(char*);i++)
		{
			if (strlen(kNames[i]) > 0 && MenuController::FindItem(kNames[i]))
				MenuController::SetChecked(kNames[i], false);
		}
		MenuController::SetChecked(name, true);	
	}

	void ClearSimulationMenu()
	{
		for (int i=0;i<sizeof(kNames) / sizeof(char*);i++)
			if (strlen(kNames[i]) > 0)
				MenuController::RemoveMenuItem (kNames[i]);
	}

	#define ADDMENU(itemNumber) if (strlen(kNames[itemNumber]) > 0) { MenuController::AddMenuItem (kNames[itemNumber], IntToString(itemNumber), this, placement); }

	void BuildSimulationMenu(GraphicsCaps::Emulation activeEmulation)
	{
		const int placement = 400;

		ADDMENU(GraphicsCaps::kEmulNone);

		switch (GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget()))
		{
		case kPlatformStandalone:
		case kPlatformWebPlayer:
			ADDMENU(GraphicsCaps::kEmulSM3);
			ADDMENU(GraphicsCaps::kEmulSM2);
			break;
		case kPlatform_iPhone:
		case kPlatformAndroid:
			ADDMENU(GraphicsCaps::kEmulGLES20);
			break;
		case kPlatformBB10:
		case kPlatformTizen:
			ADDMENU(GraphicsCaps::kEmulGLES20);
			break;
		case kPlatformXBOX360:
			ADDMENU(GraphicsCaps::kEmulXBox360);
			break;
		case kPlatformPS3:
			ADDMENU(GraphicsCaps::kEmulPS3);
			break;
		case kPlatformMetro:
			ADDMENU(GraphicsCaps::kEmulDX11_9_1);
			ADDMENU(GraphicsCaps::kEmulDX11_9_3);
			break;
		}

		RebuildMenu ();

		MenuController::SetChecked(kNames[activeEmulation], true);
	}
};

static EditNetSimulationCaps *gEditSimulationCaps;

GraphicsCaps::Emulation SetDefaultGraphicsEmulationForPlatform(BuildTargetPlatformGroup targetPlatformGroup)
{
	GraphicsCaps::Emulation emulation;
	
	switch (targetPlatformGroup)
	{
	case kPlatform_iPhone:
	case kPlatformAndroid:
		case kPlatformBB10:
        case kPlatformTizen:
		emulation = GraphicsCaps::kEmulGLES20;
		break;
/*
	case kPlatformXBOX360:
		emulation = GraphicsCaps::kEmulXBox360;
		break;
	case kPlatformPS3:
		emulation = GraphicsCaps::kEmulPS3;
		break;
*/
	default:
		emulation = GraphicsCaps::kEmulNone;
		break;
	}

	if (emulation != GraphicsCaps::kEmulNone && !gGraphicsCaps.CheckEmulationSupported(emulation))
		emulation = GraphicsCaps::kEmulNone;

	gGraphicsCaps.SetEmulation(emulation);
	return emulation;
}

void EditNetSimulationCapsRegisterMenuAndSetDefaultEmulation ()
{
	gEditSimulationCaps = new EditNetSimulationCaps;
	BuildTargetPlatformGroup activeTarget = GetBuildTargetGroup(GetEditorUserBuildSettings().GetActiveBuildTarget());
	gEditSimulationCaps->BuildSimulationMenu(SetDefaultGraphicsEmulationForPlatform(activeTarget));
}

void RebuildSimulationsMenu(GraphicsCaps::Emulation activeEmulation)
{
	gEditSimulationCaps->ClearSimulationMenu();
	gEditSimulationCaps->BuildSimulationMenu(activeEmulation);
}

void SwitchGraphicsEmulationBuildTarget(BuildTargetPlatformGroup targetPlatformGroup)
{
	RebuildSimulationsMenu(SetDefaultGraphicsEmulationForPlatform(targetPlatformGroup));
}

STARTUP (EditNetSimulationCapsRegisterMenuAndSetDefaultEmulation)
