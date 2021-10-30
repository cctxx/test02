#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#include "Runtime/Utilities/LinkedList.h"

class Animation;

class AnimationManager
{
	public:
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	/// Animates all registered objects according to the registered parameters
	/// Removes Animations if the animated object or the track is gone or if the time 
	/// of the track end time of the animation is reached
	void Update ();
	
	public:

	void AddDynamic(ListNode<Animation>& node) { m_Animations.push_back(node); } 
	void AddFixed (ListNode<Animation>& node)  { m_FixedAnimations.push_back(node); }
	
#if ENABLE_PROFILER	
	int GetUpdatedAnimationCount () { return m_Animations.size_slow() + m_FixedAnimations.size_slow(); }
#endif
	
	private:
	
	typedef List< ListNode<Animation> > AnimationList;

	AnimationList	m_Animations;
	AnimationList	m_FixedAnimations;
};

AnimationManager& GetAnimationManager ();

#endif
