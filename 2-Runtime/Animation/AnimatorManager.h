#pragma once

#include "Runtime/Utilities/dynamic_array.h"
class Animator;

class AnimatorManager
{
	public:
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	void AddAnimator(Animator& animator);
	void RemoveAnimator(Animator& animator);
	
	virtual void FixedUpdateFKMove();
	virtual void FixedUpdateRetargetIKWrite();

	virtual void UpdateFKMove();
	virtual void UpdateRetargetIKWrite();
	
private:
	
	dynamic_array<Animator*> m_UpdateAvatars;
	dynamic_array<Animator*> m_FixedUpdateAvatars;
};

AnimatorManager& GetAnimatorManager ();