#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Network/NetworkManager.h"

static const char* kNames[] = {
"Edit/Network Emulation/None",
"Edit/Network Emulation/Broadband",
"Edit/Network Emulation/DSL",
"Edit/Network Emulation/ISDN",
"Edit/Network Emulation/Dial-Up",
};

struct EditSimulationCaps : public MenuInterface {
	virtual bool Validate (const MenuItem &menuItem) {
		return true;
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		GetNetworkManager().SetSimulation((NetworkSimulation)idx);
		
		SetChecked("Edit/Network Emulation/" + menuItem.m_Name);
	}
	
	void SetChecked (const std::string& name)
	{
		for (int i=0;i<sizeof(kNames) / sizeof(char*);i++)
			MenuController::SetChecked(kNames[i], false);
		MenuController::SetChecked(name, true);	
	}
};

static EditSimulationCaps *gEditSimulationCaps;
void EditSimulationCapsRegisterMenu ();
void EditSimulationCapsRegisterMenu () {
	const int placement = 400;
	gEditSimulationCaps = new EditSimulationCaps;
	for (int i=0;i<sizeof(kNames) / sizeof(char*);i++)
		MenuController::AddMenuItem (kNames[i], IntToString(i), gEditSimulationCaps, placement);
	RebuildMenu ();
	MenuController::SetChecked(kNames[0], true);	
}

STARTUP (EditSimulationCapsRegisterMenu)