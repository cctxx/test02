#include "UnityPrefix.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"

struct PlayMenu : public MenuInterface
{
	virtual bool Validate (const MenuItem &menuItem)
	{
		return true;
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx)
		{
			case 0:
				CallStaticMonoMethod ("Toolbar", "TogglePlaying");
				break;
			case 1:
				GetApplication().SetPaused(!GetApplication().IsPaused());
				break;
			case 2:
				GetApplication().Step();
				break;
		}
	}
};

static PlayMenu *gPlayMenu = NULL;

void RunPlaymodeMenu ()
{
	gPlayMenu = new PlayMenu;

	MenuController::AddMenuItem ("Edit/Play %p", "0", gPlayMenu, 150);
	MenuController::AddMenuItem ("Edit/Pause %#p", "1", gPlayMenu, 150);
	MenuController::AddMenuItem ("Edit/Step %&p", "2", gPlayMenu, 150);
}

STARTUP (RunPlaymodeMenu)	// Call this on startup.
