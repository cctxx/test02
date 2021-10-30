#include "UnityPrefix.h"
#include "AnimatorOverrideController.h"
#include "AnimationSetBinding.h"
#include "RuntimeAnimatorController.h"
#include "AnimationClipBindings.h"
#include "GenericAnimationBindingCache.h"
#include "AnimationClip.h"

#if UNITY_EDITOR
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#endif

#define DIRTY_AND_INVALIDATE() ClearAsset(); SetDirty(); NotifyObjectUsers( kDidModifyAnimatorController )

struct FindClip
{
	char const* m_ClipName;
	FindClip(char const* clipName):m_ClipName(clipName){}

	bool operator()(PPtr<AnimationClip> const& clip){ return strcmp(clip->GetName(), m_ClipName) == 0 ; }
};

struct FindOriginalClipByName
{
	char const* m_ClipName;
	FindOriginalClipByName(char const* clipName):m_ClipName(clipName){}

	bool operator()(AnimationClipOverride const& overrideClip){ return strcmp(overrideClip.m_OriginalClip->GetName(), m_ClipName) == 0 ; }
};

struct FindOriginalClip
{
	PPtr<AnimationClip> const& m_Clip;
	FindOriginalClip(PPtr<AnimationClip> const& clip):m_Clip(clip){}

	bool operator()(AnimationClipOverride const& overrideClip){ return overrideClip.m_OriginalClip == m_Clip ; }
};

PPtr<AnimationClip> return_original(AnimationClipOverride const& overrideClip){ return overrideClip.m_OriginalClip; }
PPtr<AnimationClip> return_override(AnimationClipOverride const& overrideClip){ return overrideClip.m_OverrideClip; }
PPtr<AnimationClip> return_effective(AnimationClipOverride const& overrideClip){ return overrideClip.GetEffectiveClip(); }

IMPLEMENT_OBJECT_SERIALIZE (AnimatorOverrideController)
IMPLEMENT_CLASS_HAS_INIT (AnimatorOverrideController)

INSTANTIATE_TEMPLATE_TRANSFER(AnimatorOverrideController)

AnimatorOverrideController::AnimatorOverrideController(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode),
m_AnimationSetBindings(0),
m_AnimationSetNode(this), 
m_Allocator(label)
{
}

AnimatorOverrideController::~AnimatorOverrideController()
{	
}

void AnimatorOverrideController::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);

	if(!m_Controller.IsNull())
		m_Controller->AddObjectUser(m_AnimationSetNode);

	NotifyObjectUsers( kDidModifyAnimatorController );
}

void AnimatorOverrideController::InitializeClass ()
{
	REGISTER_MESSAGE_VOID(AnimatorOverrideController, kDidModifyAnimatorController, ClearAsset);
	REGISTER_MESSAGE_VOID(AnimatorOverrideController, kDidModifyMotion, ClearAsset);
}

template<class Functor> PPtr<AnimationClip> AnimatorOverrideController::FindAnimationClipInMap(PPtr<AnimationClip> const& clip, Functor functor, PPtr<AnimationClip> const& defaultClip)const
{
	AnimationClipOverrideVector::const_iterator it = std::find_if(m_Clips.begin(), m_Clips.end(), FindOriginalClip(clip) );
	return it != m_Clips.end() ? functor(*it) : defaultClip;
}

// Return the clip list from controller
AnimationClipVector AnimatorOverrideController::GetOriginalClips()const
{
	AnimationClipVector clips;
	if(m_Controller.IsNull())
		return clips;

	return m_Controller->GetAnimationClips();
}

// Return the merged clip list that should be used to drive the animator
// Always start from original clip list and search for each clip if there is a match in m_Clips
AnimationClipVector AnimatorOverrideController::GetAnimationClips() const
{
	AnimationClipVector controllerClips = GetOriginalClips();
	AnimationClipVector clips;
	clips.reserve(controllerClips.size());

	for(AnimationClipVector::const_iterator clipIt = controllerClips.begin(); clipIt != controllerClips.end() ; ++clipIt)
		clips.push_back( FindAnimationClipInMap(*clipIt, return_effective, *clipIt) );
	
	return clips;
}

AnimationClipVector AnimatorOverrideController::GetOverrideClips()const
{
	AnimationClipVector controllerClips = GetOriginalClips();
	AnimationClipVector clips;
	clips.reserve(controllerClips.size());

	for(AnimationClipVector::const_iterator clipIt = controllerClips.begin(); clipIt != controllerClips.end() ; ++clipIt)
		clips.push_back( FindAnimationClipInMap(*clipIt, return_override, *clipIt) );

	return clips;
}

mecanim::animation::ControllerConstant*	AnimatorOverrideController::GetAsset()
{
	if(m_Controller.IsNull())
		return 0;
	
	return m_Controller->GetAsset();
}

void AnimatorOverrideController::BuildAsset()
{
	ClearAsset();

	if(m_Controller.IsNull())
	{
		m_Clips.clear();
		return;
	}

	mecanim::animation::ControllerConstant* controller = m_Controller->GetAsset();
	if(controller == 0)
	{
		m_Clips.clear();
		return;	
	}
	
	RegisterAnimationClips();
	AnimationClipVector clips = GetAnimationClips();	
	m_AnimationSetBindings = UnityEngine::Animation::CreateAnimationSetBindings(controller, clips, m_Allocator);
}

std::string	AnimatorOverrideController::StringFromID(unsigned int ID) const 
{
	if(m_Controller.IsNull())
		return "";
	
	return m_Controller->StringFromID(ID);
}

void AnimatorOverrideController::ClearAsset()
{
	DestroyAnimationSetBindings(m_AnimationSetBindings, m_Allocator);	
	m_AnimationSetBindings = 0;

#if UNITY_EDITOR
	// This is needed to update AnimatorOverrideController's inspector clip list.
	// If a user modify the controller by adding or removing a clip we cannot dirty AnimatorOverrideController asset because the user didn't modify it but still we need to update the UI
	// which is based on the controller clip list.
	ScriptingInvocation invocation("UnityEngine", "AnimatorOverrideController", "OnInvalidateOverrideController");
	invocation.AddObject(Scripting::ScriptingWrapperFor(this));
	invocation.Invoke();
#endif
}

template<class TransferFunction>
void AnimatorOverrideController::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER(m_Controller);
	TRANSFER(m_Clips);
}


PPtr<RuntimeAnimatorController> AnimatorOverrideController::GetAnimatorController()const
{
	return m_Controller;
}

void AnimatorOverrideController::SetAnimatorController(PPtr<RuntimeAnimatorController> controller)
{
	if(m_Controller != controller)
	{
		m_AnimationSetNode.Clear();

		m_Controller = controller;

		if(!m_Controller.IsNull())
			m_Controller->AddObjectUser(m_AnimationSetNode);

		DIRTY_AND_INVALIDATE();
	}
}

PPtr<AnimationClip> AnimatorOverrideController::GetClip(std::string const& name, bool returnEffectiveClip)const
{
	// if clip 'name' is not an original clip bailout
	PPtr<AnimationClip> clip = GetOriginalClip(name);
	if(clip.IsNull())
		return NULL;

	return returnEffectiveClip ? FindAnimationClipInMap(clip, return_effective) : FindAnimationClipInMap(clip, return_override);
}

PPtr<AnimationClip> AnimatorOverrideController::GetClip(PPtr<AnimationClip> originalClip, bool returnEffectiveClip)const
{
	if(originalClip.IsNull())
		return NULL;

	return GetClip( originalClip->GetName(), returnEffectiveClip );
}

void AnimatorOverrideController::SetClip(PPtr<AnimationClip> originalClip, PPtr<AnimationClip> overrideClip)
{
	if(originalClip.IsNull())
		return;

	SetClip(originalClip->GetName(), overrideClip);
}

void AnimatorOverrideController::SetClip(std::string const& name, PPtr<AnimationClip> clip)
{
	// if clip 'name' is not an original clip bailout
	PPtr<AnimationClip> originalClip = GetOriginalClip(name);
	if(originalClip.IsNull())
		return;

	AnimationClipOverrideVector::iterator it = std::find_if(m_Clips.begin(), m_Clips.end(), FindOriginalClip(originalClip) );
	if(it != m_Clips.end())
	{
		it->m_OverrideClip = clip;
		DIRTY_AND_INVALIDATE();
	}
	else
	{
		AnimationClipOverride clipOverride;
		clipOverride.m_OriginalClip = originalClip;
		clipOverride.m_OverrideClip = clip;

		m_Clips.push_back(clipOverride);
		DIRTY_AND_INVALIDATE();
	}
}

PPtr<AnimationClip>	 AnimatorOverrideController::GetOriginalClip(std::string const& name)const
{
	AnimationClipVector controllerClips = GetOriginalClips();
	AnimationClipVector::const_iterator it = std::find_if(controllerClips.begin(), controllerClips.end(), FindClip( name.c_str() ) );
	return it != controllerClips.end() ? *it : NULL;
}

UnityEngine::Animation::AnimationSetBindings* AnimatorOverrideController::GetAnimationSetBindings()
{
	if(m_AnimationSetBindings == 0)
		BuildAsset();

	return m_AnimationSetBindings;
}

AnimationClipVector AnimatorOverrideController::GetAnimationClipsToRegister() const
{
	return GetOverrideClips();
}

void AnimatorOverrideController::PerformOverrideClipListCleanup()
{
	AnimationClipVector clips = GetOriginalClips();

	AnimationClipOverrideVector clipsToRemove;
	AnimationClipOverrideVector::iterator it;
	for(it = m_Clips.begin(); it != m_Clips.end(); ++it)
	{
		if(it->m_OriginalClip.IsNull() || it->m_OverrideClip.IsNull())
			clipsToRemove.push_back(*it);
		else
		{
			AnimationClipVector::const_iterator it2 = std::find_if(clips.begin(), clips.end(), FindClip( it->m_OriginalClip->GetName() ) );
			if(it2 == clips.end())
				clipsToRemove.push_back(*it);
		}
	}

	for(it = clipsToRemove.begin(); it != clipsToRemove.end(); ++it)
	{
		AnimationClipOverrideVector::iterator it2 = std::find_if(m_Clips.begin(), m_Clips.end(), FindOriginalClip( it->m_OriginalClip ) );
		if(it2 != m_Clips.end()) 
			m_Clips.erase(it2);
	}

	if(clipsToRemove.size() > 0 )
		DIRTY_AND_INVALIDATE();
}


#undef DIRTY_AND_INVALIDATE
