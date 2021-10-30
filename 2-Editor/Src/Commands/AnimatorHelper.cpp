#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Animation/OptimizeTransformHierarchy.h"
#include "Editor/Src/Application.h"

struct AnimatorHelper : public MenuInterface 
{
	Animator *GetActiveAnimator (Object *context) 
	{
		Animator *activeanimator = dynamic_pptr_cast<Animator*> (context);
		return activeanimator;
	}
	virtual bool Validate (const MenuItem &menuItem) 
	{
		Assert (!menuItem.context.empty());
		Animator *activeAnimator = GetActiveAnimator (menuItem.context[0]);
		if (!activeAnimator)
			return false;

		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) 
		{
			case 0:
				return activeAnimator->GetHasTransformHierarchy ();
			case 1:
				return !activeAnimator->GetHasTransformHierarchy ();
		}
		return false;
	}

	virtual void Execute (const MenuItem &menuItem) 
	{
		Assert (!menuItem.context.empty());
		Animator *activeAnimator = GetActiveAnimator (menuItem.context[0]);
		AssertIf (!activeAnimator);
		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) 
		{
		case 0:
			OptimizeTransformHierarchy (activeAnimator->GetGameObject());
			GetApplication().RequestRepaintAllViews ();
			break;
		case 1:
			DeoptimizeTransformHierarchy (activeAnimator->GetGameObject());
			GetApplication().RequestRepaintAllViews ();
			break;
		}
	}

	virtual void UpdateContextMenu (std::vector<Object*> &context, int userData)
	{
		if (context.empty())
			return;

		Animator *activeAnimator = GetActiveAnimator (context[0]);		
		if (!activeAnimator)
			return;

		// Cleanup old menuitem
		MenuController::RemoveMenuItem("CONTEXT/Animator/Optimize Transform Hierarchy");
		MenuController::RemoveMenuItem("CONTEXT/Animator/Deoptimize Transform Hierarchy");

		// Add the correct option based on animator optimization state
		if (activeAnimator->GetHasTransformHierarchy ())
			MenuController::AddContextItem ("Animator/Optimize Transform Hierarchy", "0", this);
		else
			MenuController::AddContextItem ("Animator/Deoptimize Transform Hierarchy", "1", this);
	}
};

static AnimatorHelper *gAnimatorHelper;
void AnimatorHelperRegisterMenu ();
void AnimatorHelperRegisterMenu () 
{
	gAnimatorHelper = new AnimatorHelper;

	MenuController::AddContextItem ("Animator/Optimize Transform Hierarchy", "0", gAnimatorHelper);
}

STARTUP (AnimatorHelperRegisterMenu)	// Call this on startup.
