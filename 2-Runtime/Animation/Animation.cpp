#include "UnityPrefix.h"
#include "Animation.h"
#include "AnimationManager.h"
#include "AnimationClip.h"
#include "AnimationClipUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/Utility.h"
#include "AnimationBinder.h"
#include "NewAnimationTrack.h"
#include "AnimationState.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Shaders/Material.h"
#include "AnimationCurveUtility.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/Animation/AnimationUtility.h"
#include "Runtime/BaseClasses/EventIDs.h"

using std::make_pair;
using std::max;

PROFILER_INFORMATION (gBuildAnimationState, "Animation.RebuildInternalState", kProfilerAnimation);
PROFILER_INFORMATION (gUpdateAnimation, "Animation.Update", kProfilerAnimation);
PROFILER_INFORMATION (gSampleAnimation, "Animation.Sample", kProfilerAnimation);
PROFILER_WARNING (gDidDestroyObjectNotification, "Animation.DestroyAnimationClip [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_WARNING (gAddClip, "Animation.AddClip [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_WARNING (gRemoveClip, "Animation.RemoveClip [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_WARNING (gCloneAnimationState, "Animation.Clone [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_WARNING (gAnimationDeactivate, "Animation.Deactivate [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_INFORMATION (gValidate, "ValidateBoundCurves", kProfilerAnimation);

inline int CombineWrapMode (int clipWrapMode, int animationWrapMode)
{
	if (clipWrapMode != 0)
		return clipWrapMode;
	else
		return animationWrapMode;
}

Animation::Animation (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_AnimationManagerNode(this)
,	m_ActiveAnimationStatesSize(0)
,	m_CullingType(kCulling_AlwaysAnimate)
,   m_BoundCurves (label)
{
	m_WrapMode = 0;
	
	m_PlayAutomatically = true;
	m_AnimatePhysics = false;
	m_DirtyMask = 0;
	m_Visible = false;
	memset (m_ActiveAnimationStates, 0, sizeof(m_ActiveAnimationStates));
}

Animation::~Animation () 
{
	ClearContainedRenderers ();
	ReleaseAnimationStates ();
	CleanupBoundCurves();
}


void Animation::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	
#if UNITY_EDITOR
	LoadOldAnimations();
#endif
	
	// We don't know what kind of culling we used before, so we clear all culling related data
	ClearContainedRenderers();

	CheckIsCullingBasedOnBoundsDeprecated();

	if (m_CullingType == kCulling_BasedOnRenderers && !m_AnimationStates.empty())
		RecomputeContainedRenderers ();
	
	if (m_PlayAutomatically && (awakeMode & (kDidLoadFromDisk | kInstantiateOrCreateFromCodeAwakeFromLoad | kActivateAwakeFromLoad)) && IsActive () && IsWorldPlaying())
		Play (kStopAll);	
}

void Animation::Deactivate (DeactivateOperation operation)
{
	if (operation != kDeprecatedDeactivateToggleForLevelLoad)
	{
		PROFILER_AUTO(gAnimationDeactivate, this)
		//ReleaseAnimationStates ();
		Stop();
		CleanupBoundCurves();
	}
}

AnimationClip* Animation::GetClipWithNameSerialized (const std::string& name)
{
	for (Animations::iterator i=m_Animations.begin();i != m_Animations.end();i++)
	{
		AnimationClip* clip = *i;
		if (clip && clip->GetName() == name)
			return clip;
	}
	return NULL;
}

void Animation::SetClip (PPtr<AnimationClip> anim)
{
	m_Animation = anim;

	SetDirty ();
}

void Animation::SetClips (const Animations& anims)
{
	m_Animations = anims;
	
	CheckIsCullingBasedOnBoundsDeprecated();

	SetDirty ();
}

bool Animation::IsPlayingLayer (int layer)
{
	if (m_AnimationStates.empty ())
		return false;
	
	for (iterator i=begin();i != end();i++)
	{
		AnimationState& state = **i;
		if (state.GetLayer() == layer && state.GetEnabled())
			return true;
	}
	
	return false;
}


bool Animation::IsPlaying ()
{
	if (m_AnimationStates.empty ())
		return false;
	
	for (iterator i=begin();i != end();i++)
	{
		AnimationState& state = **i;
		if (state.GetEnabled())
			return true;
	}
	
	return false;
}

bool Animation::IsPlaying (const string& clip)
{
	AnimationState* state = GetState(clip);
	
	if ( state && state->GetEnabled() )
		return true;
	else // check for clones
	{
		for ( int s = 0; s < m_AnimationStates.size(); s++ )
		{
			AnimationState* st = m_AnimationStates[s];
			
			if (   st->IsClone()
				&& st->GetParentName() == clip
				&& st->GetEnabled() )
				return true;
		}
	}
	
	return false;
}



void Animation::Stop ()
{
	for (iterator i=begin();i != end();i++)
	{
		(**i).Stop();
	}
	
	// Remove all queued animations.
	m_Queued.clear();
}

void Animation::Stop( const string& name )
{
	for ( int s = 0; s < m_AnimationStates.size(); s++ )
	{
		AnimationState* state = m_AnimationStates[s];
		
		bool stopState = false;
		if ( state->IsClone() && state->GetParentName() == name )
			stopState = true;
		else if ( !state->IsClone() && state->GetName() == name )
			stopState = true;
		
		if ( stopState )
			Stop( *state );
	}
	
	// Remove any queued animations that were cloned from the named state.
	QueuedAnimations::iterator q, qnext;
	for (q=m_Queued.begin();q != m_Queued.end();q=qnext)
	{
		qnext = q;

		if ( q->state->GetParentName() != name )
			qnext++;			
		else
			m_Queued.erase( q );
	}
	
}

void Animation::Stop (AnimationState& state)
{
	state.Stop();
}

void Animation::Rewind ()
{
	for (iterator i=begin();i != end();i++)
	{
		Rewind(**i);
	}
}

void Animation::Rewind (const string& name)
{
	if (!m_AnimationStates.empty())
	{
		AnimationState* state = GetState(name);
		if (state)
			Rewind(*state);
	}
}

void Animation::Rewind (AnimationState& state)
{
	state.SetTime(0.0F);
}

void Animation::SetWrapMode (int mode)
{
	m_WrapMode = mode;
	for (iterator i=begin ();i != end ();i++)
	{
		AnimationState& state = **i;
		state.SetWrapMode(mode);
	}
	SetDirty();
}

#if UNITY_EDITOR
/// Deprecated with 1.5
void Animation::LoadOldAnimations ()
{
	// Load old animations into new animation array
	for (OldAnimations::iterator i=m_OldAnimations.begin ();i != m_OldAnimations.end ();i++)
	{
		AnimationClip* clip = i->second;
		if (clip)
		{
			AddClip (*clip, i->first, INT_MIN, INT_MAX, false);
		}
	}
	m_OldAnimations.clear();
	
	
	// Make sure we have the default animation in the animations list
	AnimationClip* defaultAnim = m_Animation;
	if (defaultAnim)
	{
		for (Animations::iterator i=m_Animations.begin();i!=m_Animations.end();i++)
		{
			if (*i == m_Animation)
				return;
		}

		AddClip (*defaultAnim, defaultAnim->GetName(), INT_MIN, INT_MAX, false);
	}
}
#endif

IMPLEMENT_CLASS_HAS_INIT (Animation)
IMPLEMENT_OBJECT_SERIALIZE (Animation)
///////////////////***************************

void Animation::AddToManager ()
{
	// This method is named AddToManager, but it is used to remove from manager too...
	m_AnimationManagerNode.RemoveFromList();
	if (IsWorldPlaying())
	{
		if (GetEnabled() && (m_Visible || m_CullingType == kCulling_AlwaysAnimate) && IsActive() && !m_AnimationStates.empty())
		{
			if (!m_AnimatePhysics)
				GetAnimationManager().AddDynamic(m_AnimationManagerNode);
			else
				GetAnimationManager().AddFixed(m_AnimationManagerNode);
		}
	}
	else
	{
		// Insert all in edit mode - for sampling animation with the timeline
		if (IsActive())
			GetAnimationManager().AddDynamic(m_AnimationManagerNode);
	}
}

void Animation::RemoveFromManager ()
{
	m_AnimationManagerNode.RemoveFromList();
}

void Animation::SetAnimatePhysics (bool anim)
{
	m_AnimatePhysics = anim;
	SetDirty();
	if (m_AnimationManagerNode.IsInList())
	{
		m_AnimationManagerNode.RemoveFromList();
		if (!m_AnimatePhysics)
			GetAnimationManager().AddDynamic(m_AnimationManagerNode);
		else
			GetAnimationManager().AddFixed(m_AnimationManagerNode);
	}
}

#define ScriptErrorStringObject ErrorStringObject

const char* kAnimationNotFoundError =
"The animation state %s could not be played because it couldn't be found!\n"
"Please attach an animation clip with the name '%s' or call this function only for existing animations.";
#define CANT_PLAY_ERROR { ScriptErrorStringObject(Format(kAnimationNotFoundError, name.c_str(), name.c_str()), this); }


const char* kWrongStateError =
"The animation state %s could not be played because it is not attached to the animation component!\n"
"You have to provide an animation state that is attached to the same animation component.";
#define WRONG_STATE_ERROR(x) { ScriptErrorStringObject(Format(kWrongStateError, x.GetName().c_str()), this); }


bool Animation::Play(const std::string& name, int playMode)
{
	// Deprecated support for play with queueing
	if (playMode == kPlayQueuedDeprecated)
	{
		QueueCrossFade(name, 0.0F, CompleteOthers, kStopSameLayer);
		return true;
	}

	CrossFade(name, 0.0F, playMode);
	return true;
}

void Animation::Play(AnimationState& fadeIn, int playMode)
{
	// Deprecated support for play with queueing
	if (playMode == kPlayQueuedDeprecated)
	{
		QueueCrossFade(fadeIn, 0.0F, CompleteOthers, kStopSameLayer);
		return;
	}
		
	CrossFade(fadeIn, 0.0F, playMode, true);
	return;
}


bool Animation::Play (int mode)
{
	AnimationClip* clip = m_Animation;
	if (clip)
	{
		AnimationState* state = GetState(clip);
		if (state)
		{
			Play(*state, mode);
			return true;
		}
		else
		{
			LogStringObject("Default clip could not be found in attached animations list.", this);
			return false;
		}
	}
	else
		return false;
}

void Animation::Blend(const std::string& name, float targetWeight, float time) {
	AnimationState* state = GetState(name);
	if (state)
		Blend(*state, targetWeight, time);
	else
		CANT_PLAY_ERROR
}

void Animation::CrossFade(const std::string& name, float time, int mode) {
	AnimationState* state = GetState(name);
	if (state)
		CrossFade(*state, time, mode, true);
	else
		CANT_PLAY_ERROR
}

AnimationState* Animation::QueueCrossFade(const std::string& name, float time, int queue, int mode) {
	AnimationState* state = GetState(name);
	if (state)
		return QueueCrossFade(*state, time, queue, mode);
	else
	{
		CANT_PLAY_ERROR
		return NULL;
	}
}

/// - Should Blend and CrossFade Rewind animations before fading in?
/// - Should CrossFade Rewind animations after they are faded out?

void Animation::Blend(AnimationState& playState, float targetWeight, float time)
{
	bool found = false;
	for (iterator i=begin();i!=end();i++)
	{
		AnimationState& state = **i;
		
		if (&playState == &state)
		{
			state.SetEnabled(true);
			state.SetWeightTarget(targetWeight, time, false);
			state.SetupFadeout(time);
			found = true;
		}
	}
	if (!found)
		WRONG_STATE_ERROR(playState)
}

void Animation::CrossFade(AnimationState& playState, float time, int mode, bool clearQueuedAnimations )
{
	bool found = false;
	for (iterator i=begin();i!=end();i++)
	{
		AnimationState& state = **i;
		
		// Don't touch animations in other layers!
		if ((mode & kStopAll) == 0 && state.GetLayer() != playState.GetLayer())
			continue;
			
		if (&playState == &state)
		{
			state.SetEnabled(true);
			if (time > kReallySmallFadeTime)
				state.SetWeightTarget(1.0F, time, false);
			else
			{
				state.SetWeightTargetImmediate(1.0F, false);
			}
			
			state.SetupFadeout(time);
			found = true;
		}
		else
		{
			if (time > kReallySmallFadeTime)
				state.SetWeightTarget(0.0F, time, true);
			else
			{
				state.Stop();
				state.SetWeight(0.0F);
			}
		}
	}
	
	if ( clearQueuedAnimations )
	{
		// Clear out queued animations on the same channel as the state
		// we are cross fading to or all queued animations if we are
		// stopping all.

		// Fixed bug: https://fogbugz.unity3d.com/default.asp?470484, on metro std vector are somehow implemented differently
		// If you remove an element from the list, you have to reinitialize all the iterators, to fix the problem, I removed iterators at all
		for (int i = 0; i < m_Queued.size();)
		{
			if ( !(mode & kStopAll) && m_Queued[i].state->GetLayer() != playState.GetLayer() )
			{		
				i++;
				continue;
			}
			
			m_Queued[i].state->Stop();
			m_Queued[i].state->ForceAutoCleanup();
			m_Queued.erase(m_Queued.begin() + i);
		}
	}

	if ( !found )
		WRONG_STATE_ERROR(playState)
}

AnimationState* Animation::QueueCrossFade(AnimationState& originalState, float time, int queueMode, int mode)
{
	AnimationState* cloned = CloneAnimation(&originalState);
	if (!cloned)
	{
		WRONG_STATE_ERROR(originalState)
		return NULL;
	}

	AnimationState& playState = *cloned;
	playState.SetAutoCleanup();
	
	// Queue the animation for real!
	if (queueMode == CompleteOthers)
	{
		QueuedAnimation queue;
		queue.mode = mode;
		queue.queue = queueMode;
		queue.fadeTime = time;
		queue.state = &playState;
		m_Queued.push_back(queue);
		return &playState;
	}
	else
	{
		CrossFade(playState, time, mode, true);
		return &playState;
	}
}

/// * All animation states have an array with pointers to the curves used when sampling. (AnimationState.m_Curves)
///   The index in the AnimationState.m_Curves is the same as the BoundCurves index.
///   If a curve is not available or excluded because of mixing the pointer is NULL.

void InsertAnimationClipCurveIDs(AnimationBinder::CurveIDLookup& curveIDLookup, AnimationClip& clip)
{
	AnimationClip::QuaternionCurves& rot = clip.GetRotationCurves();
	AnimationClip::Vector3Curves& pos = clip.GetPositionCurves();
	AnimationClip::Vector3Curves& scale = clip.GetScaleCurves();
	AnimationClip::FloatCurves& floats = clip.GetFloatCurves();
	CurveID curveID;
	
	for (AnimationClip::QuaternionCurves::iterator i=rot.begin();i != rot.end();i++)
	{
		AnimationClip::QuaternionCurve& element = *i;

		curveID = CurveID (element.path.c_str(), ClassID(Transform), NULL, "m_LocalRotation", element.hash);
		if (element.hash == 0)
		{
			curveID.CalculateHash();
			element.hash = curveID.hash;
		}
		
		AnimationBinder::InsertCurveIDIntoLookup(curveIDLookup, curveID);
	}

	for (AnimationClip::Vector3Curves::iterator i=pos.begin();i != pos.end();i++)
	{
		AnimationClip::Vector3Curve& element = *i;

		curveID = CurveID (element.path.c_str(), ClassID(Transform), NULL, "m_LocalPosition", element.hash);
		if (element.hash == 0)
		{
			curveID.CalculateHash();
			element.hash = curveID.hash;
		}
		AnimationBinder::InsertCurveIDIntoLookup(curveIDLookup, curveID);
	}

	for (AnimationClip::Vector3Curves::iterator i=scale.begin();i != scale.end();i++)
	{
		AnimationClip::Vector3Curve& element = *i;

		curveID = CurveID (element.path.c_str(), ClassID(Transform), NULL, "m_LocalScale", element.hash);
		if (element.hash == 0)
		{
			curveID.CalculateHash();
			element.hash = curveID.hash;
	}

		
		AnimationBinder::InsertCurveIDIntoLookup(curveIDLookup, curveID);
	}

	for (AnimationClip::FloatCurves::iterator i=floats.begin();i != floats.end();i++)
	{
		AnimationClip::FloatCurve& element = *i;
		curveID = CurveID (element.path.c_str(), element.classID, element.script, element.attribute.c_str(), element.hash);
		if (element.hash == 0)
		{
			curveID.CalculateHash();
			element.hash = curveID.hash;
		}
		
		AnimationBinder::InsertCurveIDIntoLookup(curveIDLookup, curveID);
	}
}

static bool IsMixedIn (const Animation::BoundCurves& boundCurves, int index, AnimationState& state)
{
	// We only mix transform animation
	if (boundCurves[index].targetType == kBindTransformRotation || boundCurves[index].targetType == kBindTransformPosition || boundCurves[index].targetType == kBindTransformScale)
	{
		DebugAssertIf(dynamic_pptr_cast<Transform*> (boundCurves[index].targetObject) == NULL);
		Transform* transform = static_cast<Transform*> (boundCurves[index].targetObject);
		
		return state.ShouldMixTransform(*transform);
	}
	return true;
}


static void AssignBoundCurve (const AnimationBinder::CurveIDLookup& curveIDLookup, const CurveID& curveID, AnimationCurveBase* curve, const Animation::BoundCurves& boundCurves, AnimationState& state)
{
	AnimationBinder::CurveIDLookup::const_iterator found;
	found = curveIDLookup.find(curveID);
	if (found == curveIDLookup.end())
		return;

	if (!IsMixedIn (boundCurves, found->second, state))
		return;
	
	AnimationState::Curves curves = state.GetCurves();
	curves[found->second] = curve;
}

static void CalculateAnimationClipCurves (const AnimationBinder::CurveIDLookup& curveIDLookup, AnimationClip& clip, const Animation::BoundCurves& boundCurves, AnimationState& state)
{
	AnimationClip::QuaternionCurves& rot = clip.GetRotationCurves();
	AnimationClip::Vector3Curves& pos = clip.GetPositionCurves();
	AnimationClip::Vector3Curves& scale = clip.GetScaleCurves();
	AnimationClip::FloatCurves& floats = clip.GetFloatCurves();

	CurveID curveID;
	
	for (AnimationClip::QuaternionCurves::iterator i=rot.begin();i != rot.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;
		AssertIf(i->hash == 0);
		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalRotation", i->hash);
		AssignBoundCurve(curveIDLookup, curveID, (AnimationCurveBase*)&i->curve, boundCurves, state);
	}

	for (AnimationClip::Vector3Curves::iterator i=pos.begin();i != pos.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;
		
		AssertIf(i->hash == 0);
		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalPosition", i->hash);
		AssignBoundCurve(curveIDLookup, curveID, (AnimationCurveBase*)&i->curve, boundCurves, state);
	}

	for (AnimationClip::Vector3Curves::iterator i=scale.begin();i != scale.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;
		
		AssertIf(i->hash == 0);
		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalScale", i->hash);
		AssignBoundCurve(curveIDLookup, curveID, (AnimationCurveBase*)&i->curve, boundCurves, state);
	}

	for (AnimationClip::FloatCurves::iterator i=floats.begin();i != floats.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;
		
		AssertIf(i->hash == 0);
		curveID = CurveID (i->path.c_str(), i->classID, i->script, i->attribute.c_str(), i->hash);
		AssignBoundCurve(curveIDLookup, curveID, (AnimationCurveBase*)&i->curve, boundCurves, state);
	}
}

void Animation::ValidateBoundCurves ()
{
	PROFILER_AUTO(gValidate, this);
	BoundCurves::const_iterator end = m_BoundCurves.end();
	for (BoundCurves::const_iterator i=m_BoundCurves.begin(); i != end; ++i)
	{
		const BoundCurveDeprecated& bound = *i;
		
#if 0
		// We are accessing potentially deleted memory and validating it if the instanceID matches.
		// This is unsafe, but will work in almost all cases. ValidateBoundCurves does not trigger in normal circumstances.
		// Yet we have to pay a large cost for the safety provided... This seems not fair.
		if (bound.targetObject->GetInstanceID () != bound.targetInstanceID)
#else		
		if (Object::IDToPointer(bound.targetInstanceID) != bound.targetObject)
#endif			
		{
			PROFILER_AUTO(gDidDestroyObjectNotification, this);
			CleanupBoundCurves ();
			return;
		}
	}
}

void Animation::CleanupBoundCurves ()
{
	if (m_BoundCurves.empty ())
	{
		return;
	}
	
	m_BoundCurves.clear();
	m_DirtyMask |= kRebindDirtyMask;
}


void Animation::RebuildStateForEverything ()
{
	PROFILER_AUTO(gBuildAnimationState, this)
	
	//@TODO: we should make sure that the curves are aligned for better cache utilization
	// -> Simply modify curveIDLookup::iterator->second to be in the right cache order, after building the curveIDLookup!
	//    Possibly caching is already pretty good if the first curve contains all curves!

	AnimationBinder::CurveIDLookup curveIDLookup;
	AnimationBinder::InitCurveIDLookup(curveIDLookup);
	
	/// Build curveIDLookup
	/// * walk through all animation states and insert the curve into curveIDLookup
	/// * curveCount gets increased when we encounter a new curve
	CurveID curveID ("", 0, NULL, "", 0);
	AnimationState* state;
	AnimationBinder::CurveIDLookup::iterator found;
	
	string attribute;
	
	Transform* transform = QueryComponent (Transform);
		
	if (transform)
	{
		for (int s=0;s<m_AnimationStates.size();s++)
		{
			state = m_AnimationStates[s];
			AnimationClip* clip = state->m_Clip;
			if (clip == NULL)
				continue;
			
			InsertAnimationClipCurveIDs(curveIDLookup, *clip);
		}

		// Bind the global curves.
		GetAnimationBinder().BindCurves(curveIDLookup, *transform, m_BoundCurves, m_CachedAffectedSendToRootTransform, m_CachedTransformMessageMask);

		// This will compact any curves that couldn't be bound
		// - Faster at runtime (Can remove some ifs)
		// - Slower at load time
		AnimationBinder::RemoveUnboundCurves(curveIDLookup, m_BoundCurves);
		
		
		/// * Build state.m_Curves arrays
		///   - We go through all curves in the every state/clip look it up in the curve map
		for (int s=0;s<m_AnimationStates.size();s++)
		{
			state = m_AnimationStates[s];
			// Initialize all states to have no curves assigned
			// Those that aren't used or excluded for mixing will remain null after this loop
			state->CleanupCurves();
			state->AllocateCurves(curveIDLookup.size());
			
			AnimationClip* clip = state->m_Clip;
			if (clip == NULL)
				continue;
			
			CalculateAnimationClipCurves(curveIDLookup, *clip, m_BoundCurves, *state);
		}
	}
	
	// Force Bound curves mask recalculation
	m_ActiveAnimationStatesSize = 0;
	
	m_DirtyMask &= ~kRebindDirtyMask;
}
	
inline float InverseWeight(float weight)
{
	return weight > kReallySmallWeight ? (1.0F / weight) : 0.0F;
}

/***  This calculates the blend weights for one curve.

Weights are distributed so that the top layer gets everything.
If it doesn't use the full weight then the next layer gets to distribute the remaining
weights and so on. Once all weights are used by the top layers,
no weights will be available for lower layers anymore
We use fair weighting, which means if a lower layer wants 80% and 50% have already been used up, the layer will NOT use up all weights.
instead it will take up 80% of the 50%.

Example:
a upper body which is affected by wave, walk and idle
a lower body which is affected by only walk and idle

weight  name  layer lower    upper
 20%    wave   2     0%     20% 
 50%    walk   1     50%    40% 
 100%   idle   0     50%    40% 
 
- Blend weights can change per animated value because of mixing.
  Even without mixing, sometimes a curve is just not defined. Still you want the blend weights to add up to 1.
  Most of the time weights are similar between curves. So there is a lot of caching one can do.
*/

// We use fair weighting, which means if a lower layer wants 80% and 50% have already been used up, the layer will NOT use up all weights.
// Instead it will take up 80% of the 50%.
#define FAIR_DISTRIBUTION 1

template<bool optimize32States>
void CalculateWeights( AnimationState** states, int stateCount, int curveIndex, OUTPUT_OPTIONAL float* outWeights, int mask )
{
	Assert( outWeights != NULL );

	// state index -> layer index
	int*   layerIndices;
	ALLOC_TEMP(layerIndices, int, stateCount);
	// summed weights for each layer - we'll never have more layers than affectors
	float* layerSummedWeights;
	ALLOC_TEMP(layerSummedWeights, float, stateCount);

	const AnimationState* state = states[0];
	int prevLayer = state->GetLayer();
	int layerIndex = 0;

	// Clear summed layer weights
	for( int i = 0; i < stateCount; ++i )
		layerSummedWeights[i] = 0.0F;

	// sum weights for each layer
	UInt32 stateBit = 1;
	for( int i = 0; i < stateCount; ++i )
	{
		if (optimize32States)
		{
			if (mask & stateBit)
			{
				state = states[i];
				
				AssertIf( prevLayer < state->GetLayer()); // Algorithm requires sorted states by layer
				
				if( prevLayer != state->GetLayer() )
					++layerIndex;
				
				layerSummedWeights[layerIndex] += state->GetWeight();
				layerIndices[i] = layerIndex;
				outWeights[i] = state->GetWeight();
			}
			else
			{
				outWeights[i] = 0.0F;
				layerIndices[i] = 0;
			}
		}
		else
		{
			state = states[i];
			if (state->ShouldUse() && state->GetCurves()[curveIndex] != NULL && state->GetBlendMode() == AnimationState::kBlend)
			{
				AssertIf( prevLayer < state->GetLayer()); // Algorithm requires sorted states by layer
				
				if( prevLayer != state->GetLayer() )
					++layerIndex;
				
				layerSummedWeights[layerIndex] += state->GetWeight();
				layerIndices[i] = layerIndex;
				outWeights[i] = state->GetWeight();
			}
			else
			{
				outWeights[i] = 0.0F;
				layerIndices[i] = 0;
			}
		}
		stateBit <<= 1;
		prevLayer = states[i]->GetLayer();
	}
	int layerCount = layerIndex + 1;

	// Distribute weights so that the top layers get everything up to 1.
	// If they use less, the remainder goes to the lower layers.
	float* layerInvSummedWeights;
	ALLOC_TEMP(layerInvSummedWeights, float, stateCount);
	float remainderWeight = 1.0F;
	for( int i = 0; i < layerCount; ++i )
	{
		float layerWeight = max(1.0F, layerSummedWeights[i]);

		layerInvSummedWeights[i] = InverseWeight(layerWeight) * remainderWeight;

		#if FAIR_DISTRIBUTION
		remainderWeight -= layerSummedWeights[i] * remainderWeight;
		#else
		remainderWeight -= layerSummedWeights[i];
		#endif
		
		remainderWeight = max(0.0F, remainderWeight);
	}

	// - Apply the layer inverse weights
	// - Normalize the weights once again, just in case

	// @TODO: Renormalization is only necessary if the remainderWeight is larger than zero as far as i can see
	float summedWeight = 0.0F;
	for( int i = 0; i < stateCount; ++i )
	{
		outWeights[i] *= layerInvSummedWeights[layerIndices[i]];
		summedWeight += outWeights[i];
	}

	summedWeight = InverseWeight(summedWeight);
	for( int i = 0; i < stateCount; ++i )
		outWeights[i] *= summedWeight;
}

void Animation::SampleDefaultClip (double time)
{
	AnimationClip* clip = m_Animation;
	if (!clip)
		return;
	
	SampleAnimation(GetGameObject(), *clip, time, CombineWrapMode(clip->GetWrapMode(), m_WrapMode));
}

// Calculates a bitmask for every bound curve, containing which curve state affects it.
// - The recalculation is only necessary if a state got enabled or disabled (Either through manually disabling it or through a too low blend weight)
bool Animation::RebuildBoundStateMask()
{
	int activeStateCount = 0;
	bool requireRebuild = false;
	int s;
	for (s=0;s<m_AnimationStates.size() && activeStateCount < 32;s++)
	{
		AnimationState& state = *m_AnimationStates[s];
		if (state.ShouldUse() && state.GetBlendMode() == AnimationState::kBlend)
		{
			requireRebuild |= m_ActiveAnimationStates[activeStateCount] != &state;
			m_ActiveAnimationStates[activeStateCount] = &state;
			DebugAssertIf(activeStateCount != 0 && m_ActiveAnimationStates[activeStateCount-1]->GetLayer() < m_ActiveAnimationStates[activeStateCount]->GetLayer());
			activeStateCount++;
		}
	}
	
	// Too many active animation states
	if (s != m_AnimationStates.size())
		return false;
	
	requireRebuild |= activeStateCount != m_ActiveAnimationStatesSize;
	// early out if nothing in which animations are currently playing has changed.
	if (!requireRebuild)
		return true;
	
	m_ActiveAnimationStatesSize = activeStateCount;

	for (int i=0;i<m_BoundCurves.size();i++)
	{
		m_BoundCurves[i].affectedStateMask = 0;
		for (int s=0;s<m_ActiveAnimationStatesSize;s++)
		{
			AnimationState& state = *m_ActiveAnimationStates[s];
			if (state.m_Curves[i])
				m_BoundCurves[i].affectedStateMask |= 1 << s;
		}
	}
	
	return true;
}

inline void AwakeAndDirty (Object* o)
{
	if (o)
	{
		o->AwakeFromLoad(kDefaultAwakeFromLoad);
		o->SetDirty();
	}
}

static void UpdateLastNonTransformObject(Object*& lastNonTransformObject, Object* const targetObject)
{
	if (lastNonTransformObject != targetObject)
	{
		AwakeAndDirty(lastNonTransformObject);
		lastNonTransformObject = targetObject;
	}
}

// Optimized for 32 animation states
void Animation::BlendOptimized()
{
	AssertIf(m_BoundCurves.empty());
	
	///@TODO: Keep a cache for all curves and pass it into animationcurve.Evaluate.
	// So that many characters dont kill the cache of shared animation curves.
	
	int curveCount = m_BoundCurves.size();

	AnimationState** activeStates = m_ActiveAnimationStates;
	int stateSize = m_ActiveAnimationStatesSize;
	float* weights;
	ALLOC_TEMP(weights, float, stateSize);
	
	float weight;
	const AnimationState* state;
	
	Object* lastNonTransformObject = NULL;
	UInt32 lastAffectedStateMask = m_BoundCurves[0].affectedStateMask;
	CalculateWeights<true>(activeStates, stateSize, 0, weights, lastAffectedStateMask);
	BoundCurveDeprecated* boundCurves = &m_BoundCurves[0];
	
	for( int c = 0; c < curveCount; ++c )
	{
		BoundCurveDeprecated& bind = boundCurves[c];
	
		// Only recalculate weights if the state mask changes!
		// This happens very rarely. (If you dont use mixing it never happens)
		if (lastAffectedStateMask != bind.affectedStateMask)
		{
			lastAffectedStateMask = bind.affectedStateMask;
			CalculateWeights<true>(activeStates, stateSize, c, weights, lastAffectedStateMask);
		}
		
		if (lastAffectedStateMask == 0)
			continue;
		
		const UInt32 targetType = bind.targetType;
		
		// Sample quaternion
		if (targetType == kBindTransformRotation)
		{
			Prefetch(bind.targetPtr);
			Quaternionf& result = *(Quaternionf*)bind.targetPtr;
			result.Set(0,0,0,0);
			UInt32 stateBit = 1;
			for (int i=0;i<stateSize;i++)
			{
				if (lastAffectedStateMask & stateBit)
				{
					state = activeStates[i];
					const AnimationCurveQuat* quatCurve = reinterpret_cast<AnimationCurveQuat*>(state->GetCurves()[c]);
	
					Quaternionf sample = quatCurve->EvaluateClamp(state->m_WrappedTime);
					DebugAssertIf(!IsFinite(sample));

					/// Not necessary because we make sure when sampling that our curves are sampled and almost normalized
					/// @todo: If people animate inside unity this might be a problem.
					/// Hopefully no one does that in combination with blending.
					// sample = NormalizeFastEpsilonZero(sample);
					result += Sign(Dot (sample, result)) * sample * weights[i];
				}
				stateBit <<= 1;
			}
			
			result = NormalizeSafe(result);
			DebugAssertIf(!IsFinite(result));
		}
		// Sample vector 3
		else if (targetType == kBindTransformPosition)
		{
			Vector3f result = Vector3f(0.0F, 0.0F, 0.0F);
			UInt32 stateBit = 1;
			for (int i=0;i<stateSize;i++)
			{
				if (lastAffectedStateMask & stateBit)
				{
					state = activeStates[i];
					const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);
		
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
				}
				stateBit <<= 1;
			}
			
			DebugAssertIf(!IsFinite(result));
			*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
		}
		else if (targetType == kBindTransformScale)
		{
			Vector3f result = Vector3f(0.0F, 0.0F, 0.0F);
			UInt32 stateBit = 1;
			for (int i=0;i<stateSize;i++)
			{
				if (lastAffectedStateMask & stateBit)
				{
					state = activeStates[i];
					const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);
		
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
				}
				stateBit <<= 1;
			}
			
			DebugAssertIf(!IsFinite(result));
			*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
			Transform* targetTransform = reinterpret_cast<Transform*> (bind.targetObject);
			targetTransform->RecalculateTransformType ();
		}
		// Sample float
		else if (targetType > kUnbound)
		{
			float result = 0.0F;
			UInt32 stateBit = 1;
			for (int i=0;i<stateSize;i++)
			{
				if (lastAffectedStateMask & stateBit)
				{
					state = activeStates[i];
					const AnimationCurve* floatCurve = reinterpret_cast<AnimationCurve*>(state->GetCurves()[c]);
	
					float sample = floatCurve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
				}
				stateBit <<= 1;
			}

			DebugAssertIf(!IsFinite(result));
			
			AnimationBinder::SetFloatValue(bind, result);
			
			if (AnimationBinder::ShouldAwakeGeneric (bind))
				UpdateLastNonTransformObject(lastNonTransformObject, bind.targetObject);

		}
		else
		{
			#if COMPACT_UNBOUND_CURVES
			AssertString("Unbound curves should be compacted!");
			#endif
			continue;
		}

	}
	
	AwakeAndDirty(lastNonTransformObject);
}


// Unlimited amount of animation states
void Animation::BlendGeneric()
{
	AssertIf(m_BoundCurves.empty());
	
	///@TODO: Keep a cache for all curves and pass it into animationcurve.Evaluate.
	// So that many characters dont kill the cache of shared animation curves.
	
	int curveCount = m_BoundCurves.size();

	int stateSize = m_AnimationStates.size();
	float* weights;
	ALLOC_TEMP(weights, float, stateSize);
		
	float weight;
	const AnimationState* state;
	
	Object* lastNonTransformObject = NULL;
	for( int c = 0; c < curveCount; ++c )
	{
		BoundCurveDeprecated& bind = m_BoundCurves[c];
	
		CalculateWeights<false>(&m_AnimationStates[0], stateSize, c, weights, 0);
		
		bool didSample = false;
		
		int targetType = bind.targetType;
		
		// Sample quaternion
		if (targetType == kBindTransformRotation)
		{
			Quaternionf result = Quaternionf(0.0F, 0.0F, 0.0F, 0.0F);
			
			for (int i=0;i<stateSize;i++)
			{
				state = m_AnimationStates[i];
				const AnimationCurveQuat* quatCurve = reinterpret_cast<AnimationCurveQuat*>(state->GetCurves()[c]);
				if (quatCurve && weights[i] > kReallySmallWeight)
				{
					Quaternionf sample = quatCurve->EvaluateClamp(state->m_WrappedTime);

					/// Not necessary because we make sure when sampling that our curves are sampled and almost normalized
					/// @todo: If people animate inside unity this might be a problem.
					/// Hopefully no one does that in combination with blending.

					// sample = NormalizeFastEpsilonZero(sample);
					weight = weights[i];
					if (Dot (sample, result) < 0.0F)
						weight = -weight;
					result += sample * weight;
					didSample = true;
				}
			}
			
			result = NormalizeSafe(result);
			DebugAssertIf(!IsFinite(result));
			if (didSample)
				*reinterpret_cast<Quaternionf*>(bind.targetPtr) = result;
		}
		// Sample vector 3
		else if (targetType == kBindTransformPosition)
		{
			Vector3f result = Vector3f(0.0F, 0.0F, 0.0F);
			for (int i=0;i<stateSize;i++)
			{
				state = m_AnimationStates[i];
				const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);
				if (vec3Curve && weights[i] > kReallySmallWeight)
				{
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
					didSample = true;
				}
			}
			
			DebugAssertIf(!IsFinite(result));
			if (didSample)
			{
				*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
			}
		}
		// Sample vector 3
		else if (targetType == kBindTransformScale)
		{
			Vector3f result = Vector3f(0.0F, 0.0F, 0.0F);
			for (int i=0;i<stateSize;i++)
			{
				state = m_AnimationStates[i];
				const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);
				if (vec3Curve && weights[i] > kReallySmallWeight)
				{
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
					didSample = true;
				}
			}
			
			DebugAssertIf(!IsFinite(result));

			if (didSample)
			{
				*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
				Transform* targetTransform = reinterpret_cast<Transform*> (bind.targetObject);
				targetTransform->RecalculateTransformType ();
			}
		}
		// Sample float
		else if (targetType > kUnbound)
		{
			float result = 0.0F;
			for (int i=0;i<stateSize;i++)
			{
				state = m_AnimationStates[i];
				const AnimationCurve* floatCurve = reinterpret_cast<AnimationCurve*>(state->GetCurves()[c]);
				if (floatCurve && weights[i] > kReallySmallWeight)
				{
					float sample = floatCurve->EvaluateClamp(state->m_WrappedTime);
					weight = weights[i];
					result += sample * weight;
					didSample = true;
				}
			}

			DebugAssertIf(!IsFinite(result));

			if (didSample)
			{
				AnimationBinder::SetFloatValue(bind, result);

				if (AnimationBinder::ShouldAwakeGeneric (bind))
					UpdateLastNonTransformObject(lastNonTransformObject, bind.targetObject);
			}
		}	
		else
		{
			#if COMPACT_UNBOUND_CURVES
			AssertString("Unbound curves should be compacted!");
			#endif
			continue;
		}

	}
	
	AwakeAndDirty(lastNonTransformObject);
}

void Animation::BlendAdditive()
{
	AssertIf(m_BoundCurves.empty());
	int stateSize = m_AnimationStates.size();
	AnimationState* state;

	AnimationState** activeStates;
	ALLOC_TEMP(activeStates, AnimationState*, stateSize);
	int activeStateSize = 0;

	///@TODO: We can also compare if the current time matches the first frame, then the animation has no effect!
	for (int i=0;i<stateSize;i++)
	{
		state = m_AnimationStates[i];
		if (state->GetBlendMode() == AnimationState::kAdditive && state->ShouldUse ())
		{
			activeStates[activeStateSize] = state;
			activeStateSize++;
		}
	}
	// early out if we have no additive animations playing
	if (activeStateSize == 0)
		return;
	
	int curveCount = m_BoundCurves.size();
	BoundCurveDeprecated* boundCurves = &m_BoundCurves[0];
//	float* weights;
//	ALLOC_TEMP(weights, float, stateSize, kMemAnimation);
//	float* time;
//	ALLOC_TEMP(time, float, stateSize, kMemAnimation);
		
	float weight;
	
	Object* lastNonTransformObject = NULL;
	for( int c = 0; c < curveCount; ++c )
	{
		BoundCurveDeprecated& bind = boundCurves[c];
		
		bool didSample = false;
		
		int targetType = bind.targetType;
		
		// Sample quaternion
		if (targetType == kBindTransformRotation)
		{
			Quaternionf result = *reinterpret_cast<Quaternionf*>(bind.targetPtr);
			
			for (int i=0;i<activeStateSize;i++)
			{
				state = activeStates[i];
				weight = clamp01 (state->GetWeight());
				const AnimationCurveQuat* quatCurve = reinterpret_cast<AnimationCurveQuat*>(state->GetCurves()[c]);
				if (quatCurve)
				{
					Quaternionf sample = Inverse(quatCurve->GetKey(0).value) * quatCurve->EvaluateClamp(state->m_WrappedTime);
					sample = Lerp(Quaternionf::identity(), sample, weight);
					result *= sample;
					didSample = true;
				}
			}
			
			result = NormalizeSafe(result);
			DebugAssertIf(!IsFinite(result));
			if (didSample)
				*reinterpret_cast<Quaternionf*>(bind.targetPtr) = result;
		}
		// Sample vector 3
		else if (targetType == kBindTransformPosition)
		{
			Vector3f result = *reinterpret_cast<Vector3f*>(bind.targetPtr);

			for (int i=0;i<activeStateSize;i++)
			{
				state = activeStates[i];
				weight = clamp01 (state->GetWeight());
				const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);

				if (vec3Curve)
				{
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime) - vec3Curve->GetKey(0).value;
					result += sample * weight;
					didSample = true;
				}
			}
			DebugAssertIf(!IsFinite(result));
			if (didSample)
				*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
		}
		// Sample vector 3
		else if (targetType == kBindTransformScale)
		{
			Vector3f result = *reinterpret_cast<Vector3f*>(bind.targetPtr);

			for (int i=0;i<activeStateSize;i++)
			{
				state = activeStates[i];
				weight = clamp01 (state->GetWeight());
				const AnimationCurveVec3* vec3Curve = reinterpret_cast<AnimationCurveVec3*>(state->GetCurves()[c]);

				if (vec3Curve)
				{
					Vector3f sample = vec3Curve->EvaluateClamp(state->m_WrappedTime) - vec3Curve->GetKey(0).value;
					result += sample * weight;
					didSample = true;
				}
			}
			DebugAssertIf(!IsFinite(result));
			if (didSample)
			{
				*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
				Transform* targetTransform = reinterpret_cast<Transform*> (bind.targetObject);
				targetTransform->RecalculateTransformType ();
			}
		}
	}
	
	AwakeAndDirty(lastNonTransformObject);
}

inline Animation::CullingType RemapDeprecatedCullingType (Animation::CullingType type)
{
	if (type == Animation::kDeprecatedCulling_BasedOnClipBounds || type == Animation::kDeprecatedCulling_BasedOnUserBounds)
		return Animation::kCulling_BasedOnRenderers;
	else
		return type;
}

void Animation::SetCullingType(CullingType type)
{
	type = RemapDeprecatedCullingType(type);

	CheckIsCullingBasedOnBoundsDeprecated ();
	
	// Clearing culling related data
	if (m_CullingType == kCulling_BasedOnRenderers)
	{
		ClearContainedRenderers();
	}
	else if (m_CullingType == kCulling_AlwaysAnimate)
		RemoveFromManager();

	m_CullingType = type;

	// Building new culling data
	if (m_CullingType == kCulling_BasedOnRenderers && !m_AnimationStates.empty())
		RecomputeContainedRenderers ();
	else if (m_CullingType == kCulling_AlwaysAnimate && !m_AnimationManagerNode.IsInList())
		AddToManager();

	SetDirty();
}

void Animation::RemoveContainedRenderer (Renderer* renderer)
{
	ContainedRenderers::iterator end = m_ContainedRenderers.end();
	for (ContainedRenderers::iterator i = m_ContainedRenderers.begin();i != end;++i)
	{
		Renderer* cur = *i;
		if (cur == renderer)
		{
			*i = m_ContainedRenderers.back();
			m_ContainedRenderers.resize(m_ContainedRenderers.size() - 1);
			return;
		}
	}
}

static void AnimationVisibilityCallback (void* userData, void* senderUserData, int visibilityEvent)
{
	Animation& animation = *reinterpret_cast<Animation*> (userData);
	
	if (visibilityEvent == kBecameVisibleEvent)
		animation.SetVisibleRenderers(true);
	else if (visibilityEvent == kBecameInvisibleEvent)
		animation.CheckRendererVisibleState ();
	else if (visibilityEvent == kWillDestroyEvent)
	{
		animation.RemoveContainedRenderer(reinterpret_cast<Renderer*> (senderUserData));
		
		animation.CheckRendererVisibleState ();
	}
}

void Animation::ClearContainedRenderers ()
{
	ContainedRenderers::iterator end = m_ContainedRenderers.end();
	for (ContainedRenderers::iterator i = m_ContainedRenderers.begin();i != end;++i)
	{
		Renderer* renderer = *i;
		renderer->RemoveEvent(AnimationVisibilityCallback, this);
	}
	m_ContainedRenderers.clear();
}

void Animation::RecomputeContainedRenderers ()
{
	Assert(m_CullingType == kCulling_BasedOnRenderers);

	ClearContainedRenderers ();
		
	Transform& transform = GetComponent (Transform);
	RecomputeContainedRenderersRecurse(transform);

	Assert(m_CullingType == kCulling_BasedOnRenderers);

	CheckRendererVisibleState ();
}

void Animation::CheckRendererVisibleState ()
{
	Assert(m_CullingType == kCulling_BasedOnRenderers);

	ContainedRenderers::iterator end = m_ContainedRenderers.end();	
	for (ContainedRenderers::iterator i = m_ContainedRenderers.begin();i != end;++i)
	{
		Renderer* renderer = *i;
		Assert(renderer->HasEvent(AnimationVisibilityCallback, this));
		if (renderer->IsVisibleInScene())
		{
			SetVisibleRenderers(true);
			return;
		}
	}

	SetVisibleRenderers(false);
}

void Animation::SetVisibleInternal(bool visible)
{
	Assert(m_CullingType != kCulling_AlwaysAnimate);
	m_Visible = visible;

	if (IsWorldPlaying())
	{
		const bool wasAttached = m_AnimationManagerNode.IsInList();
		// Method is called AddToManager, but it does removal too...
		AddToManager();

		// Culling is after AnimationManager Update,
		// so when we pop into visibility we should make sure we are rendering the right frame!
		if (m_AnimationManagerNode.IsInList() && wasAttached == false)
			UpdateAnimation(GetCurTime());
	}
}


void Animation::SetVisibleRenderers(bool visible)
{
	//LogString(Format("SetVisibleRenderers %d %s", visible ? 1 : 0, GetGameObject().GetName()));

	Assert(m_CullingType == kCulling_BasedOnRenderers);
	SetVisibleInternal(visible);
}

void Animation::SetVisibleBounds(bool visible)
{
	//LogString(Format("SetVisibleBounds %d %s", visible ? 1 : 0, GetGameObject().GetName()));

	CheckIsCullingBasedOnBoundsDeprecated ();
	SetVisibleInternal(visible);
}

///@TODO: We must ensure that there are not two animation components in a hierarchy!!!. Otherwise SetAnimationPtr will break!

void Animation::RecomputeContainedRenderersRecurse (Transform& transform)
{
	Renderer* renderer = transform.QueryComponent(Renderer);
	if (renderer)
	{
		m_ContainedRenderers.push_back(renderer);
		renderer->AddEvent(AnimationVisibilityCallback, this);
	}
	Transform::iterator end = transform.end();
	for (Transform::iterator i = transform.begin();i != end;++i)
	{
		RecomputeContainedRenderersRecurse(**i);
	}
}

void Animation::SendTransformChangedToCachedTransform()
{
	int physicsMask = m_AnimatePhysics ? Transform::kAnimatePhysics : 0;
	int size = m_CachedAffectedSendToRootTransform.size();
	for (int i=0;i<size;i++)
	{
		m_CachedAffectedSendToRootTransform[i]->SendTransformChanged(m_CachedTransformMessageMask | physicsMask);
	}
}

void Animation::SyncLayerTime (int layer)
{
	float normalizedSpeed = 0.0F;
	float normalizedTime = 0.0F;
	float summedLayerWeight = 0.0F;
	
	for (AnimationStates::iterator i=m_AnimationStates.begin();i!=m_AnimationStates.end();i++)
	{
		AnimationState& state = **i;
		if (state.GetLayer() != layer || !state.GetEnabled())
			continue;
		
		float weight = max(state.GetWeight(), 0.0F);
		normalizedSpeed += state.GetNormalizedSpeed() * weight;
		normalizedTime += state.GetNormalizedTime() * weight;
		summedLayerWeight += weight;
	}
	
	if (summedLayerWeight > kReallySmallWeight)
	{
		normalizedSpeed = normalizedSpeed / summedLayerWeight;
		normalizedTime = normalizedTime / summedLayerWeight;
		
		for (AnimationStates::iterator i=m_AnimationStates.begin();i!=m_AnimationStates.end();i++)
		{
			AnimationState& state = **i;
			if (state.GetLayer() != layer || !state.GetEnabled())
				continue;
			
			state.SetNormalizedSyncedSpeed(normalizedSpeed);
			state.SetNormalizedTime(normalizedTime);
		}
	}
}

void Animation::Sample ()
{
	bool needsUpdate = false;

	// Update animation state
	for (int i=0;i<m_AnimationStates.size();i++)
	{
		AnimationState& state = *m_AnimationStates[i];
		
		// Do we actually use any of the animation states?
		if (state.ShouldUse())
			needsUpdate = true;

		m_DirtyMask |= state.GetDirtyMask();
		state.ClearDirtyMask();
	}
	
	if (needsUpdate)
		SampleInternal();
}

void Animation::SampleInternal()
{
	PROFILER_AUTO(gSampleAnimation, this)

	ValidateBoundCurves ();

	if (m_DirtyMask != 0)
	{
		if (m_DirtyMask & kRebindDirtyMask)
			RebuildStateForEverything();
		
		if (m_DirtyMask & kLayersDirtyMask)
			SortAnimationStates();
	}
	
	AssertIf(m_DirtyMask != 0);

	if (!m_BoundCurves.empty())
	{
		if (RebuildBoundStateMask())
		{
			if (m_ActiveAnimationStatesSize != 0)
				BlendOptimized();
		}
		else
		{
			/// More than 32 animation states active at the same time!
			/// @TODO: 3.0 take this out. Possibly sort animation states by maximum weight. 
			/// Or maybe supporting more than 32 states at once is really pointless
			BlendGeneric();
		}
			
		BlendAdditive();
		
		SendTransformChangedToCachedTransform();
	}
}

void Animation::UpdateAnimation (double time)
{
	if (AnimationState::UseUnity32AnimationFixes())
		UpdateAnimationInternal(time);
	else
		UpdateAnimationInternal_Before32(time);
}

// Calculates remaining play-times for all animations and for specified layer
static void GetQueueTimes(const Animation::AnimationStates& states, const int targetLayer, float& allQueueTime, float& layerQueueTime)
{
	allQueueTime = 0;
	layerQueueTime = 0;

	for (Animation::AnimationStates::const_iterator it = states.begin(), end = states.end(); it != end; ++it)
	{
		const AnimationState& state = **it;

		if (state.GetEnabled())
		{
			const int layer = state.GetLayer();

			const int wrapMode = state.GetWrapMode();
			if (wrapMode != kDefaultWrapMode && wrapMode != kClamp)
			{
				// for "infinite" animations (Loop, Repeat, ClampForever) we mark layer as "occupied"
				allQueueTime = std::numeric_limits<float>::infinity();
				if (layer == targetLayer)
					layerQueueTime = std::numeric_limits<float>::infinity();
			}
			else
			{
				const float dt = state.GetLength() - state.GetTime();
				Assert(dt >= 0);

				allQueueTime = std::max(allQueueTime, dt);
				if (layer == targetLayer)
					layerQueueTime = std::max(layerQueueTime, dt);
			}
		}
	}
}

// This function starts Queued animations (if it's already time to start).
// We always blend animations based on QueuedAnimation::fadeTime, i.e. it doesn't
// matter if currently playing animation(s) will finish in shorter time we will blend 
// in the new one in QueuedAnimation::fadeTime. We leave up to a user to specify 
// sufficient blend times.
void Animation::UpdateQueuedAnimations(bool& needsUpdate)
{
	int lastLayer = -1;
	float allQueueTime, lastLayerQueueTime;
	allQueueTime = lastLayerQueueTime = -1;

	// Update queued animations
	for (QueuedAnimations::iterator q = m_Queued.begin(); q != m_Queued.end(); )
	{
		const QueuedAnimation& qa = *q;		
		const float fadeTime = qa.fadeTime;

		const int layer = qa.state->GetLayer();

		bool startNow = false;
		if (qa.mode == kStopAll)
		{
			// queuing after all animations

			if (allQueueTime < 0)
			{
				// allQueueTime must to be recalculated
				GetQueueTimes(m_AnimationStates, layer, allQueueTime, lastLayerQueueTime);
				lastLayer = layer;
			}

			startNow = fadeTime >= allQueueTime;
		}
		else
		{
			// queuing after animations in specific layer

			if (lastLayer != layer || lastLayerQueueTime < 0)
			{
				// lastLayerQueueTime must to be recalculated
				GetQueueTimes(m_AnimationStates, layer, allQueueTime, lastLayerQueueTime);
				lastLayer = layer;
			}

			startNow = fadeTime >= lastLayerQueueTime;
		}

		if (startNow)
		{
			// This crossfade logic is framerate specific. We know when this animation had
			// to be started, so in theory we should advance time and blending value (and execute events), 
			// but we don't do that because it would be an over-complication.
			AnimationState& state = *qa.state;
			
			CrossFade(state, fadeTime, qa.mode, false);
			q = m_Queued.erase(q);
			needsUpdate = true;

			Assert(state.GetEnabled());
			// we need to recalculate queue times, because we just started an animation			
			allQueueTime = lastLayerQueueTime = -1;
		}
		else
			++q;
	}
}

void Animation::UpdateQueuedAnimations_Before34(bool& needsUpdate)
{
	// Update queued animations
	for (QueuedAnimations::iterator q = m_Queued.begin(); q != m_Queued.end(); )
	{
		const QueuedAnimation& qa = *q;
		if ((qa.mode == kStopAll && !IsPlaying()) || (qa.mode != kStopAll && !IsPlayingLayer(qa.state->GetLayer())))
		{
			CrossFade(*qa.state, qa.fadeTime, qa.mode, false);
			q = m_Queued.erase(q);
			needsUpdate = true;
		}
		else
			++q;
	}
}

void Animation::UpdateAnimationInternal(double time)
{
	PROFILER_AUTO(gUpdateAnimation, this)
	
	bool needsUpdate = false;
		
	// Sync animations
	for (SyncedLayers::iterator sync=m_SyncedLayers.begin();sync != m_SyncedLayers.end();sync++)
		SyncLayerTime(*sync);

	int stoppedAnimationCount = 0;
	AnimationState** stoppedAnimations = NULL;
	ALLOC_TEMP(stoppedAnimations, AnimationState*, m_AnimationStates.size());

	// Update animation state
	for (int i=0;i<m_AnimationStates.size();)
	{
		AnimationState& state = *m_AnimationStates[i];
			
		// Update state
		if (state.GetEnabled())
		{
			if (state.UpdateAnimationState(time, *this))
			{
				if (!state.ShouldAutoCleanupNow())
					stoppedAnimations[stoppedAnimationCount++] = &state;				
			}
		}
		
		// Do we actually use any of the animation states?
		if (state.ShouldUse())
			needsUpdate = true;
	
		m_DirtyMask |= state.GetDirtyMask();
		state.ClearDirtyMask();
		
		// Cleanup queued animations that have finished playing
		if (state.ShouldAutoCleanupNow())
		{
			delete &state;
			m_DirtyMask |= kLayersDirtyMask;
			m_AnimationStates.erase(m_AnimationStates.begin() + i);
			m_ActiveAnimationStatesSize = 0;
		}
		else
		{
			i++;
		}
	}

	if (AnimationState::UseUnity34AnimationFixes())
		UpdateQueuedAnimations(needsUpdate);
	else
		UpdateQueuedAnimations_Before34(needsUpdate);

	if (stoppedAnimationCount > 0)
	{
		for (int i = 0; i < stoppedAnimationCount; ++i)
			stoppedAnimations[i]->SetupUnstoppedState();

		needsUpdate = true;
	}

	// Only do blending if it is really necessary
	if (needsUpdate)
	{
		SampleInternal();
	}

	for (int i = 0; i < stoppedAnimationCount; ++i)
		stoppedAnimations[i]->CleanupUnstoppedState();
}

// This is for backwards compatibility. If you need to make changes, 
// then make them in UpdateAnimationInternal which is used with Unity 3.2 and later content
void Animation::UpdateAnimationInternal_Before32(double time)
{
	PROFILER_AUTO(gUpdateAnimation, this)

		bool needsUpdate = false;
	int activeLastStateCount = 0;

	// Sync animations
	for (SyncedLayers::iterator sync=m_SyncedLayers.begin();sync != m_SyncedLayers.end();sync++)
		SyncLayerTime(*sync);

	AnimationState* stoppedAnimation = NULL;

	// Update animation state
	for (int i=0;i<m_AnimationStates.size();)
	{
		AnimationState& state = *m_AnimationStates[i];

		if (state.ShouldUse())
			activeLastStateCount++;

		// Update state
		if (state.GetEnabled())
		{
			if (state.UpdateAnimationState(time, *this))
			{
				if (!state.ShouldAutoCleanupNow())
					stoppedAnimation = &state;
			}
		}

		// Do we actually use any of the animation states?
		if (state.ShouldUse())
			needsUpdate = true;

		m_DirtyMask |= state.GetDirtyMask();
		state.ClearDirtyMask();

		// Cleanup queued animations that have finished playing
		if (state.ShouldAutoCleanupNow())
		{
			delete &state;
			m_DirtyMask |= kLayersDirtyMask;
			m_AnimationStates.erase(m_AnimationStates.begin() + i);
			m_ActiveAnimationStatesSize = 0;
		}
		else
		{
			i++;
		}
	}

	UpdateQueuedAnimations_Before34(needsUpdate);	

	bool revertWrappedTimeToBeforeStop = false;
	if (activeLastStateCount == 1 && needsUpdate == false && stoppedAnimation)
	{
		stoppedAnimation->SetupUnstoppedState();
		revertWrappedTimeToBeforeStop = true;
	}

	// Only do blending if it is really necessary
	if (needsUpdate)
	{
		SampleInternal();
	}

	if (revertWrappedTimeToBeforeStop)
		stoppedAnimation->CleanupUnstoppedState();
}


struct GreaterLayer : std::binary_function<AnimationState*, AnimationState*, std::size_t>
{
	bool operator () (AnimationState* lhs, AnimationState* rhs) const
	{
		if (lhs->GetLayer() != rhs->GetLayer())
			return lhs->GetLayer() > rhs->GetLayer();
		else
			return lhs->GetName() > rhs->GetName();
	}
};

void Animation::SortAnimationStates ()
{
	sort(m_AnimationStates.begin(), m_AnimationStates.end(), GreaterLayer());
	m_DirtyMask &= ~kLayersDirtyMask;
	m_ActiveAnimationStatesSize = 0;
}

void Animation::ReleaseAnimationStates ()
{
	for (AnimationStates::iterator i=m_AnimationStates.begin();i!=m_AnimationStates.end();i++)
	{
		delete *i;
	}
	m_AnimationStates.clear();
}

AnimationClip* Animation::GetClipLegacyWarning (AnimationClip* clip)
{
	if (clip == NULL)
		return NULL;
	else
	{
		if (clip->GetAnimationType () == AnimationClip::kLegacy)
			return clip;
		else	
		{
			WarningStringObject(Format("The AnimationClip '%s' used by the Animation component '%s' must be marked as Legacy.", clip->GetName(), GetName()), clip);
			return NULL;
		}
	}
}


void Animation::BuildAnimationStates()
{
	if (!m_AnimationStates.empty())
		return;
	if (m_Animations.empty ())
		return;
	
	PROFILER_AUTO(gBuildAnimationState, this)
	
	ReleaseAnimationStates();
	
	m_AnimationStates.reserve(m_Animations.size());
	
	double time = GetCurTime();
	for (int i=0;i<m_Animations.size();i++)
	{
		AnimationClip* clip = GetClipLegacyWarning(m_Animations[i]);
		if (clip != NULL)
		{
			m_AnimationStates.push_back(new AnimationState());
			m_AnimationStates.back()->Init(clip->GetName(), clip, time, CombineWrapMode(clip->GetWrapMode(), m_WrapMode));
		}
	}

	if (m_CullingType == kCulling_BasedOnRenderers)
		RecomputeContainedRenderers();
	
	m_DirtyMask |= kRebindDirtyMask;
		
	AddToManager();
}

AnimationState* Animation::CloneAnimation (AnimationState* state)
{
	// The animation state needs to be attached to this animation
	if (GetState(state) == NULL)
		return NULL;

	PROFILER_AUTO(gCloneAnimationState, this)

	// Clone the state and reference all it's bound curves
	AnimationState* clone = new AnimationState();
	clone->Init( state->GetName() + " - Queued Clone", state->GetClip(), GetCurTime(), state->GetWrapMode(), true );
	clone->SetParentName( state->GetName() );
	clone->SetLayer(state->GetLayer());
	clone->SetClonedCurves(*state);
	clone->ClearDirtyMask();
	m_AnimationStates.push_back(clone);
	
	m_DirtyMask |= kLayersDirtyMask;
	return clone;
}

void Animation::AddClip (AnimationClip& clip, const std::string& newName, int firstFrame, int lastFrame, bool loop)
{
	PROFILER_AUTO (gAddClip, this)

	if (GetClipLegacyWarning (&clip) == NULL)
		return;
	
	AnimationClip* newClip = &clip;
	// Do we really need to create a duplicate clip?
	if (loop || firstFrame != INT_MIN || lastFrame != INT_MAX || newName != clip.GetName())
	{
		newClip = NEW_OBJECT (AnimationClip);

		CopySerialized(clip, *newClip);
		newClip->SetName(newName.c_str());
		
		if (loop || firstFrame != INT_MIN || lastFrame != INT_MAX)
		{
			// [case 504486] need to clear curve because function ClipAnimation() will add all these clipped curves from source clip.
			newClip->ClearCurves();
			ClipAnimation(clip, *newClip, FrameToTime(firstFrame, clip.GetSampleRate()), FrameToTime(lastFrame, clip.GetSampleRate()), loop);
		}
	}
	
	
	// Replace clips with duplicate names
	Animations::iterator i;
	for (i=m_Animations.begin();i != m_Animations.end();i++)
	{
		AnimationClip* cur = *i;
		if (cur && cur->GetName() == newName)
			break;
	}
	if (i == m_Animations.end())
		m_Animations.push_back(newClip);
	else
		*i = newClip;

	if (!m_AnimationStates.empty())
	{
		m_DirtyMask |= kRebindDirtyMask;

		// Remove states with duplicate names
		for (AnimationStates::iterator s=begin();s != end();s++)
		{
			if ((**s).GetName() == newName)
			{
				delete *s;
				m_AnimationStates.erase(s);
				break;			
			}
		}

		m_AnimationStates.push_back(new AnimationState());
		m_AnimationStates.back()->Init(newName, newClip, GetCurTime(), CombineWrapMode(newClip->GetWrapMode(), m_WrapMode));		
	}

	CheckIsCullingBasedOnBoundsDeprecated();
	
	SetDirty();
}

void Animation::AddClip (AnimationClip& clip)
{
	AddClip(clip, clip.GetName(), std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false);
}

void Animation::RemoveClip (AnimationClip& clip)
{
	PROFILER_AUTO(gRemoveClip, this)
	
	// Find the clip to remove
	// We might have the same clip multiple times in the animation list.
	// We are removing elements, so we iterate over it backwards.
	{
		bool found = false;
		int i = m_Animations.size();
		while (i--)
		{
			AnimationClip* cur = m_Animations[i];
			if (cur && cur == &clip) 
			{
				found = true;
				Animations::iterator j = m_Animations.begin() + i;
				m_Animations.erase(j);
			}
		}
		if (!found) 
		{
			AssertStringObject (Format ("Unable to remove Animation Clip '%s' - clip not found in animation list", clip.GetName()), this);
			return;
		}
	}
	
	{
		// Find the animation state(s) to remove
		// We are removing elements, so we iterate over it backwards.
		int i = m_AnimationStates.size();
		while (i--)
		{
			if (m_AnimationStates[i]->m_Clip == &clip) 
			{
				delete m_AnimationStates[i];
				iterator j = m_AnimationStates.begin() + i;
				m_AnimationStates.erase(j);
			}
		}
	}
	
	CheckIsCullingBasedOnBoundsDeprecated ();

	m_DirtyMask |= kRebindDirtyMask;
}

void Animation::RemoveClip (const std::string &clipName)
{
	PROFILER_AUTO(gRemoveClip, this)

	// Find the clip to remove
	// We might have the same clip multiple times in the animation list.
	// We are removing elements, so we iterate over it backwards.
	{
		bool found = false;
		int i = m_Animations.size();
		while (i--)
		{
			AnimationClip* cur = m_Animations[i];
			if (cur && cur->GetName() == clipName) 
			{
				found = true;
				Animations::iterator j = m_Animations.begin() + i;
				m_Animations.erase(j);
			}
		}
		if (!found) 
		{
			AssertStringObject (Format ("Unable to remove Animation Clip '%s' - clip not found in animation list", clipName.c_str()), this);
			return;
		}
	}
	
	{
		// Find the animation state(s) to remove
		// We are removing elements, so we iterate over it backwards.
		int i = m_AnimationStates.size();
		while (i--)
		{
			AnimationState *cur = m_AnimationStates[i];
			if (cur && cur->GetName() == clipName) 
			{
				delete cur;
				iterator j = m_AnimationStates.begin() + i;
				m_AnimationStates.erase(j);
			}
		}
	}	

	CheckIsCullingBasedOnBoundsDeprecated();

	m_DirtyMask |= kRebindDirtyMask;
}

int Animation::GetClipCount () const {
	return m_Animations.size ();
}

AnimationState* Animation::GetState(const string& name)
{
	BuildAnimationStates();

	for (AnimationStates::iterator i=m_AnimationStates.begin();i!=m_AnimationStates.end();i++)
	{
		AnimationState& state = **i;
		if (state.m_Name == name)
			return &state;
	}
	return NULL;
}

AnimationState* Animation::GetState(AnimationClip* clip)
{
	BuildAnimationStates();
	for (iterator i=begin();i != end();i++)
	{
		
		if ((**i).GetClip() == clip)
			return *i;
	}
	return NULL;
}

AnimationState* Animation::GetState(AnimationState* state)
{
	BuildAnimationStates();
	for (iterator i=begin();i != end();i++)
	{
		if (*i == state)
			return state;
	}
	return NULL;
}

void Animation::InitializeClass ()
{
	AnimationState::InitializeClass();
	AnimationManager::InitializeClass();
	
	RegisterAllowNameConversion("Animation", "m_PlayFixedFrameRate", "m_AnimatePhysics");
	RegisterAllowNameConversion("Animation", "m_AnimateIfVisible", "m_AnimateOnlyIfVisible");
}

void Animation::CleanupClass ()
{
	AnimationState::CleanupClass();
	AnimationManager::CleanupClass();
}


template<class TransferFunction>
void Animation::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	
	transfer.SetVersion (3);
	
#if UNITY_EDITOR
	if (transfer.IsOldVersion(1))
	{
		TRANSFER_SIMPLE (m_Animation);
		transfer.Transfer (m_OldAnimations, "m_Animations");
		TRANSFER_SIMPLE (m_WrapMode);
		TRANSFER_SIMPLE (m_PlayAutomatically);
		transfer.Transfer (m_AnimatePhysics, "m_PlayFixedFrameRate");
		return;
	}
#endif
	
	
	TRANSFER_SIMPLE (m_Animation);
	TRANSFER_SIMPLE (m_Animations);
	
	// Hide the wrapmode in the inspector if the user has not changed it already... We are in the process of deprecating it
#if UNITY_EDITOR
	bool hide = (transfer.GetFlags () & kSerializeForInspector) != 0 && (transfer.GetFlags () & kSerializeDebugProperties) == 0 && m_WrapMode == 0;
	transfer.Transfer (m_WrapMode, "m_WrapMode", hide ? kHideInEditorMask : kNoTransferFlags);
#else
	transfer.Transfer (m_WrapMode, "m_WrapMode", kNoTransferFlags);
#endif	
	
	// In Unity 3.4 we switched to the m_CullingType enum. Previously we serialized animateOnlyIfVisible.
	if (transfer.IsOldVersion(2))
	{
		bool animateOnlyIfVisible = false;
		transfer.Transfer(animateOnlyIfVisible, "m_AnimateOnlyIfVisible");
		m_CullingType = animateOnlyIfVisible ? kCulling_BasedOnRenderers : kCulling_AlwaysAnimate;
	}
	
	TRANSFER_SIMPLE (m_PlayAutomatically);
	TRANSFER (m_AnimatePhysics);

	transfer.Align();
	
	// We hide these two fields here, because they are displayed by custom inspector (AnimationEditor)
	TRANSFER_ENUM(m_CullingType);
	if (transfer.IsReading())
		m_CullingType = RemapDeprecatedCullingType(m_CullingType);
	
	
	TRANSFER_DEBUG(m_AnimationStates);
}

/*

 TODO:
 
 * Allow for reimport animations while in playmode!
 
- Make a list of all mesh users and invalidate them when the mesh changes!
- implement all missing functions
- Implement mixing properly (Based on which curves have changed)
- support delay cross fade (Starts time advancing only when the animation has faded in completely )
- Don't always add animation component to all game objects! But what if people want to animate a prefab???

- Make the animation system store amount of loops and not one huge float for time?
- Keep a cache for all curves and pass it into animationcurve.Evaluate.
   So that many characters dont kill the cache of shared animation curves.

 - automatically Reduce curves that dont affect anything!
	1. If all animation clips change that curve to the same value and all curves dont actually modify the value!
	2. The current state when loaded is the same as in all curves
    3. Only do it for animation clips that got imported from a fbx file
      - Unity made animations are more likely to be modified by scripting as well 
         so it should not play any tricks on the user
    4. Have an option to turn the optimization off. eg someone wants to do IK etc.

---
- Handle bake simulation better when using clipped animations!
- auto cleanup of queued animations	
- search for !IsWorldPlaying () those are hacks to get the timeline working
- timeline doesnt work anymore!

 - Store optimized animation curves in animation clip instead of animation state.
 - CRASHBUG: Check when animated objects are removed -> rebuild 
 - CRASHBUG: Check when animations change -> rebuild

- Remove normalize fast before blending, not necessary for sampled animations!
  But what about unity made animations?


///@TODO: EXPOSE AnimationState too!(cspreprocess fucks up at the moment)
- implement manual clipping of animations from scripts
- Handle stopping and fading out of animations!
- Add named clip will fail when called at runtime

*/

