#pragma once


#include "Runtime/Animation/RuntimeAnimatorController.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/Misc/UserList.h"
#include <vector>


class AnimationClip;

typedef std::vector<PPtr<AnimationClip> > AnimationClipVector;

namespace UnityEngine{namespace Animation{struct AnimationSetBindings;}}

struct AnimationClipOverride
{
	DEFINE_GET_TYPESTRING(AnimationClipOverride)

	PPtr<AnimationClip>	m_OriginalClip;
	PPtr<AnimationClip>	m_OverrideClip;

	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{
		TRANSFER(m_OriginalClip);
		TRANSFER(m_OverrideClip);
	}

	bool operator== (AnimationClipOverride const& other){return m_OriginalClip == other.m_OriginalClip && m_OverrideClip == other.m_OverrideClip; }

	PPtr<AnimationClip> GetEffectiveClip() const {return !m_OverrideClip.IsNull() ? m_OverrideClip : m_OriginalClip; }
};

class AnimatorOverrideController : public RuntimeAnimatorController
{
public:
	REGISTER_DERIVED_CLASS (AnimatorOverrideController, RuntimeAnimatorController)
	DECLARE_OBJECT_SERIALIZE (AnimatorOverrideController)
	
	static void InitializeClass ();
	static void CleanupClass () {}
	
	AnimatorOverrideController (MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	virtual mecanim::animation::ControllerConstant*	GetAsset();			
	virtual void BuildAsset();
	virtual void ClearAsset	();	
	
	virtual UnityEngine::Animation::AnimationSetBindings* GetAnimationSetBindings();
	virtual AnimationClipVector GetAnimationClips()const;
	
	virtual std::string	StringFromID(unsigned int ID) const ;	

	PPtr<RuntimeAnimatorController> GetAnimatorController()const;
	void SetAnimatorController(PPtr<RuntimeAnimatorController> controller);
			
	AnimationClipVector GetOriginalClips()const;
	AnimationClipVector GetOverrideClips()const;

	PPtr<AnimationClip> GetClip(std::string const& name, bool returnEffectiveClip)const;
	void				SetClip(std::string const& name, PPtr<AnimationClip> clip);
	PPtr<AnimationClip> GetClip(PPtr<AnimationClip> originalClip, bool returnEffectiveClip)const;
	void				SetClip(PPtr<AnimationClip> originalClip, PPtr<AnimationClip> overrideClip);

	void				PerformOverrideClipListCleanup();
protected:

	typedef dynamic_array<AnimationClipOverride> AnimationClipOverrideVector;

	PPtr<RuntimeAnimatorController>						m_Controller;

	// This list is a map between m_Controller clips and override clips.
	// We should never rely on this list to return m_Controller clip list because this list may become
	// offsync when an user edit the controller's clip list(either adding or removing a state).
	//
	// The map should only be updated by PerformOverrideClipListCleanup() when user edit the 
	// AnimatorOverrideController in the inspector.
	AnimationClipOverrideVector							m_Clips;

	UnityEngine::Animation::AnimationSetBindings*		m_AnimationSetBindings;		
	mecanim::memory::MecanimAllocator					m_Allocator;
	UserListNode										m_AnimationSetNode;	

private:
	
	virtual AnimationClipVector GetAnimationClipsToRegister() const;

	PPtr<AnimationClip>	GetOriginalClip(std::string const& name)const;

	template<class Functor> PPtr<AnimationClip> FindAnimationClipInMap(PPtr<AnimationClip> const& clip, Functor functor, PPtr<AnimationClip> const& defaultClip = PPtr<AnimationClip>() )const;
};


