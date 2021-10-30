#include "UnityPrefix.h"
#include "AnimationState.h"
#include "Runtime/Math/AnimationCurve.h"
#include "AnimationClip.h"
#include "AnimationEvent.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Misc/BuildSettings.h"

PROFILER_INFORMATION (gAddMixingTransform, "Animation.AddMixingTransform [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_INFORMATION (gRemoveMixingTransform, "Animation.RemoveMixingTransform [Triggers RebuildInternalState]", kProfilerAnimation);
PROFILER_INFORMATION (gModifyAnimationClip, "Animation.ModifyAnimationClip [Triggers RebuildInternalState]", kProfilerAnimation);

void AnimationState::InitializeClass ()
{
	AnimationClip::SetDidModifyClipCallback(DidModifyAnimationClip);
}

void AnimationState::CleanupClass ()
{
	AnimationClip::SetDidModifyClipCallback(NULL);
}

AnimationState::AnimationState ()
: m_AnimationClipNode (this)
{
	// We use only 2 bits for m_AnimationEventState
	// This assert guards against kAnimationEventStates running out of these bits
	Assert(kAnimationEventState__Count <= 4);

	m_Clip = NULL;
	m_Curves = NULL;
	m_IsClone = false;
}

AnimationState::~AnimationState ()
{
	m_Clip = NULL;
	m_MixingTransforms.clear();
	m_Name.clear();
	CleanupCurves ();
	m_AnimationClipNode.RemoveFromList();
}

void AnimationState::AllocateCurves(int count)
{
	AssertIf(m_Curves);
	m_OwnsCurves = 1;

	m_Curves = new AnimationCurveBase*[count];
	for (int i=0;i<count;i++)
		m_Curves[i] = NULL;
}


void AnimationState::SetClonedCurves(AnimationState& state)
{
	AssertIf(m_Curves);
	m_OwnsCurves = 0;

	m_Curves = state.m_Curves;
//	m_CurvesCount = state.m_CurvesCount;
}


void AnimationState::CleanupCurves ()
{
	if (!m_OwnsCurves || m_Curves == NULL)
	{
		m_Curves = NULL;
		return;
	}

	delete[] m_Curves;
	m_Curves = NULL;
}

void AnimationState::SetWeightTarget (float target, float length, bool stopWhenFaded)
{
	// TODO : this m_WeightDelta approach doesn't work very well when length is 0
	// Current approach might lead to precision problems.
	// The blend should happen instantly, but with very small deltaTimes it might need several frames.

	AssertFiniteParameter(target);

	float newWeightDelta;
	if (length > 0.001)
		newWeightDelta = (target - m_Weight) / length;
	else
		newWeightDelta = (target - m_Weight) * 100000.0F;

	// If the current weight delta is going to reach the target faster than the new one, don't update it!
	bool ignoreWeightDelta = m_FadeBlend && CompareApproximately(m_WeightTarget, target, kReallySmallWeight) && Abs(m_WeightDelta) > Abs(newWeightDelta);
	if (!ignoreWeightDelta)
		m_WeightDelta = newWeightDelta;

	// We need to make sure that the weight delta is never zero, otherwise the stop condition in UpdateAnimationState is never reached!
	if (CompareApproximately(m_WeightDelta, 0.0F, kReallySmallWeight))
		m_WeightDelta = 100000.0F;

	m_WeightTarget = target;

	m_FadeBlend = true;
	m_StopWhenFadedOut = stopWhenFaded;
	m_IsFadingOut = false;
}

void AnimationState::SetWeightTargetImmediate (float target, bool stopWhenFaded)
{
	AssertFiniteParameter(target);

	m_Weight = target;
	m_StopWhenFadedOut = stopWhenFaded;
	m_FadeBlend = false;
	m_IsFadingOut = false;
}


///@TODO: Doesn't using the stop time conflict with using stop time for something else, like when queueing?
void AnimationState::SetupFadeout (float length)
{
	m_FadeOutLength = length;
}

/*
void AnimationState::ApplyWeightDeltaFraction ()
{

}
*/
/*
void AnimationState::Delay (float time)
{

}
*/

void AnimationState::SetTime (float time)
{
	AssertFiniteParameter(time);
	m_Time = time;
	m_WrappedTime = WrapTime(time, m_CachedRange, m_WrapMode);
	DebugAssertIf(!IsFinite(m_Time) || !IsFinite(m_WrappedTime));
	m_AnimationEventState = kAnimationEventState_Search; // Re-search for animation event index
}

float WrapTime (float curveT, const std::pair<float, float>& range, int m_WrapMode)
{
	float begTime = range.first;
	float endTime = range.second;

	if (curveT >= endTime)
	{
		if (m_WrapMode == kRepeat)
			curveT = Repeat (curveT, begTime, endTime);
		else if (m_WrapMode == kClamp || m_WrapMode == kClampForever)
		{
			curveT = endTime;
		}
		else if (m_WrapMode == kPingPong)
			curveT = PingPong (curveT, begTime, endTime);
	}
	else if (curveT < begTime)
	{
		if (m_WrapMode == kRepeat)
			curveT = Repeat (curveT, begTime, endTime);
		if (m_WrapMode == kClamp || m_WrapMode == kClampForever)
			curveT = begTime;
		else if (m_WrapMode == kPingPong)
			curveT = PingPong (curveT, begTime, endTime);
	}
	return curveT;
}

namespace
{
	// returns -1, 0, 1 depending on sign/value of v
	int GetDirection(float v)
	{
		if (v == 0) return 0;
		else return v > 0 ? 1 : -1;
	}
}

void AnimationState::SetSpeed (float speed)
{
	// When reversing speed. Recalculate animation event index
	if (m_AnimationEventState == kAnimationEventState_PausedOnEvent)
	{
		// Handling special case when speed=0 and m_AnimationEventIndex is valid,
		// bug the event that m_AnimationEventIndex is pointing to has been triggered before pause,
		// so wee need to trigger event on the left or on the right
		Assert(GetDirection(m_Speed) == 0);

		int newSpeedDirection = GetDirection(speed);
		if (newSpeedDirection != 0)
		{
			// In PinPong mode event are sometimes executed in the oposite direction of speed
			// so we need to reverse newSpeedDirection based on that
			if (m_WrapMode == kPingPong)
			{
				const float begTime = m_CachedRange.first;
				const float endTime = m_CachedRange.second;

				// Get index of how many times we are playing this back and forth
				const int newWrapIndex = FloorfToInt((m_Time - begTime) / (endTime - begTime));

				// Switch going forward based on the PingPong direction
				if (newWrapIndex % 2 != 0)
					newSpeedDirection = -newSpeedDirection;
			}

			m_AnimationEventState = kAnimationEventState_HasEvent;
			m_AnimationEventIndex += newSpeedDirection > 0 ? 1 : -1;
		}
	}
	else
	{
		const int oldSpeedDirection = GetDirection(m_Speed);
		const int newSpeedDirection = GetDirection(speed);

		// TODO : if we want a minor optimization, we could not trigger search
		// if state is paused and then resumed in same direction, but triggering
		// search should work just fine as long as we're not on event
		if (oldSpeedDirection != newSpeedDirection)
			m_AnimationEventState = kAnimationEventState_Search;
	}

	m_SyncedSpeed = m_Speed = speed;
	AssertFiniteParameter(speed);

	if (UseUnity32AnimationFixes())
		SetupStopTime();
}

// offsetTime - time from which newWrappedTime starts (from which newWrappedTime is wrapped)
// it is used for recalculating m_Time when event modifies playback direction
bool AnimationState::FireEvents (const float deltaTime, float newWrappedTime, bool forward, Unity::Component& animation, const float beginTime, const float offsetTime, const bool reverseOffsetTime)
{
	AnimationClip::Events& events = m_Clip->GetEvents();

	// Find initial event
	if (m_AnimationEventState == kAnimationEventState_Search)
	{
		const float oldWrappedTime = m_WrappedTime;

		if (forward)
		{
			AssertIf(oldWrappedTime > newWrappedTime);

			for (int i=0;i<events.size();i++)
			{
				if (events[i].time >= oldWrappedTime)
				{
					m_AnimationEventIndex = i;
					m_AnimationEventState = kAnimationEventState_HasEvent;
					break;
				}
			}
		}
		else
		{
			AssertIf(oldWrappedTime < newWrappedTime);

			for (int i=events.size()-1;i>=0;i--)
			{
				if (events[i].time <= oldWrappedTime)
				{
					m_AnimationEventIndex = i;
					m_AnimationEventState = kAnimationEventState_HasEvent;
					break;
				}
			}
		}

		if (m_AnimationEventState == kAnimationEventState_Search)
			m_AnimationEventState = kAnimationEventState_NotFound;
	}

	const float oldSyncedSpeed = m_SyncedSpeed;
	const float oldWrappedTime = m_WrappedTime;

	while (true)
	{
		if (m_AnimationEventIndex < 0 || m_AnimationEventIndex >= events.size())
			break;

		float eventTime = events[m_AnimationEventIndex].time;

		if (forward && eventTime > newWrappedTime)
			break;
		if (!forward && eventTime < newWrappedTime)
			break;

		const int currentAnimationEventIndex = m_AnimationEventIndex;

		FireEvent (events[m_AnimationEventIndex], this, animation);
		DebugAssertIf(!IsFinite(m_Time));
		DebugAssertIf(!IsFinite(m_WrappedTime));

		if (m_AnimationEventState == kAnimationEventState_Search)
		{
			const int oldSpeedDirection = GetDirection(oldSyncedSpeed);
			const int newSpeedDirection = GetDirection(m_SyncedSpeed);

			// handling special case when speed direction was changed
			// we want to continue time from eventTime and events from next event (we do not want to trigger this event immediately again)
			if (oldSpeedDirection != newSpeedDirection)
			{
				// do not modify m_WrappedTime if it has been modified inside of event
				if (m_WrappedTime == oldWrappedTime)
				{
					const float timeDelta = eventTime - beginTime;
					float newTime = offsetTime + (reverseOffsetTime ? -timeDelta : timeDelta);
					AssertMsg(Abs(newTime - m_Time) <= deltaTime + std::numeric_limits<float>::epsilon(), "Abs(%f - %f) <= %f\n%f <= %f\n%e <= 0", newTime, m_Time, deltaTime, Abs(newTime - m_Time), deltaTime, Abs(newTime - m_Time) - deltaTime);
					m_Time = newTime;

					m_WrappedTime = eventTime;

					if (newSpeedDirection == 0)
					{
						// if time is paused we need to continue from "right" or "left" event
						// after time is unpaused, but we will know next event only when time is unpaused,
						// so we set AnimationEventState to PausedOnEvent.
						// We can just set state to Searh, because it would find and trigger same event
						m_AnimationEventIndex = currentAnimationEventIndex;
						m_AnimationEventState = kAnimationEventState_PausedOnEvent;
					}
					else
					{
						// if time is reversed we need to continue from "right" or "left" event
						m_AnimationEventIndex = currentAnimationEventIndex + (!forward ? 1 : -1);
						m_AnimationEventState = kAnimationEventState_HasEvent;
					}
				}
			}

			DebugAssertIf(!IsFinite(m_Time) || !IsFinite(m_WrappedTime));
			return false;
		}

		if (forward)
			m_AnimationEventIndex++;
		else
			m_AnimationEventIndex--;
	}

	return true;
}


bool AnimationState::UpdateAnimationState (double globalTime, Unity::Component& animationComponent)
{
	DebugAssertIf(!IsFinite(m_Time) || !IsFinite(m_WrappedTime));

	// Update time
	const float deltaTime = globalTime - m_LastGlobalTime;
	m_LastGlobalTime = globalTime;

	//deltaTime = 0.166667f;

	//LogString(Format("globalTime: %f; deltaTime: %f; m_Time: %f", globalTime, deltaTime, m_Time));

	float syncedSpeedDeltaTime = deltaTime * m_SyncedSpeed;
	// Do not trigger events if time is stopped
	if (syncedSpeedDeltaTime != 0)
	{
		double lastTime = m_Time;
		m_Time += syncedSpeedDeltaTime;
		// if syncedSpeedDeltaTime is big it might cross whole loop,
		// so we would have to fire all events in that loop, but we ignore cases like this for now...
		float newWrappedTime = m_WrappedTime + syncedSpeedDeltaTime;
		const float oldWrappedTime = m_WrappedTime;


		/// Wrap time and Fire Animation Events
		float begTime = m_CachedRange.first;
		float endTime = m_CachedRange.second;

		const bool forward = m_SyncedSpeed >= 0.0F;

		///@TODO: How should repeat behave when the first key is not at zero???
		/// - Animations should always start playback at zero and also go back to zero, regardless of where the first frame is. (Rune)
		///@TODO: How do we stop the animation when we play backwards?
		/// - The animation should stop when it has reached zero. See functional test AnimationOnceSamplesAtEndAndResetsTimeAndGetsDisabled (Rune)
		///@TODO: Repeat currently doesnt start at the first key frame it might enter anywhere depending on begin / end Time (Repeat (m_Time, begTime, endTime);)
		/// - The animation should not go back to the exact start of the range when looping, but rather subtract the range length. (Rune)
		/// - However, the range should always start at zero, not at the first key. (Rune)

		// Repeat
		if (m_WrapMode == kRepeat)
		{
			// Reached end of time range - wrap around and fire animation events
			if (newWrappedTime >= endTime)
			{
				newWrappedTime = RepeatD (m_Time, begTime, endTime);
				if (m_HasAnimationEvent)
				{
					float offsetTime = m_Time - (newWrappedTime - begTime) - (endTime - begTime);
					if (FireEvents (deltaTime, endTime, forward, animationComponent, begTime, offsetTime, false))
					{
						offsetTime += (endTime - begTime);
						m_AnimationEventIndex = 0;
						m_AnimationEventState = kAnimationEventState_HasEvent;
						FireEvents (deltaTime, newWrappedTime, forward, animationComponent, begTime, offsetTime, false);
					}
				}
			}
			else if (newWrappedTime < begTime)
			{
				newWrappedTime = RepeatD (m_Time, begTime, endTime);
				if (m_HasAnimationEvent)
				{
					float offsetTime = m_Time + (endTime - newWrappedTime);
					if (FireEvents (deltaTime, begTime, forward, animationComponent, begTime, offsetTime, false))
					{
						offsetTime -= (endTime - begTime);
						m_AnimationEventIndex = m_Clip->GetEvents().size() - 1;
						m_AnimationEventState = kAnimationEventState_HasEvent;
						FireEvents (deltaTime, newWrappedTime, forward, animationComponent, begTime, offsetTime, false);
					}
				}
			}
			// Inside of begin / end time range ->  Fire animation event only
			else if (m_HasAnimationEvent)
			{
				const float offsetTime = m_Time - (newWrappedTime - begTime);
				FireEvents (deltaTime, newWrappedTime, forward, animationComponent, begTime, offsetTime, false);
			}


			// It's important to used RepeatD, because we get an assert otherwise (for exmaple: 1.9999999 double is rounded to 2.0f, which results in 0 when wrapped)
			DebugAssertIf(m_WrappedTime == oldWrappedTime && !CompareApproximately(newWrappedTime, RepeatD (m_Time, begTime, endTime), 0.01F));
		}
		// Clamp
		else if (m_WrapMode == kClamp || m_WrapMode == kClampForever)
		{
			if (m_Time < begTime)
				newWrappedTime = begTime;
			else if (m_Time > endTime)
				newWrappedTime = endTime;
			else
				newWrappedTime = m_Time;

			if (m_HasAnimationEvent)
			{
				FireEvents (deltaTime, m_Time, forward, animationComponent, begTime, begTime, false);
			}
		}
		// Default
		else if (m_WrapMode == kDefaultWrapMode)
		{
			if (m_HasAnimationEvent)
			{
				FireEvents (deltaTime, newWrappedTime, forward, animationComponent, begTime, begTime, false);
			}
		}
		// Ping Pong
		else if (m_WrapMode == kPingPong)
		{
			newWrappedTime = PingPong(m_Time, begTime, endTime);

			if (m_HasAnimationEvent)
			{
				const AnimationClip::Events& events = m_Clip->GetEvents();

				AssertIf(Abs(endTime - begTime) < std::numeric_limits<float>::epsilon());

				// Get index of how many times we are playing this back and forth
				int wrapIndex = FloorfToInt((lastTime - begTime) / (endTime - begTime));
				int newWrapIndex = FloorfToInt((m_Time - begTime) / (endTime - begTime));

				bool forwardPlayback = m_SyncedSpeed >= 0.0F;

				// Switch going forward based on the pingpong direction
				if (newWrapIndex % 2 != 0)
					forwardPlayback = !forwardPlayback;

				// Inside of begin / end boundary
				if (wrapIndex == newWrapIndex)
				{
					const float offsetTime = m_Time - (newWrappedTime - begTime);
					FireEvents (deltaTime, newWrappedTime, forwardPlayback, animationComponent, begTime, offsetTime, false);
				}
				// Crossing boundary
				else
				{
					if (forwardPlayback)
					{
						float offsetTime = forward ?
							m_Time - (newWrappedTime - begTime) :
							m_Time + (newWrappedTime - begTime);
						if (FireEvents (deltaTime, begTime, false, animationComponent, begTime, offsetTime, forward))
						{
							// if event is right at the begin time - we want to play it once, otherwise twice
							// we might get m_AnimationEventIndex=events.size() in this statement, but that's acceptable situation
							// it indicates that there are no events until we reverse the time
							m_AnimationEventIndex = (events.front().time == begTime ? 1 : 0);
							m_AnimationEventState = kAnimationEventState_HasEvent;
							// Events should never be searched in next call of FireEvents
							FireEvents (deltaTime, newWrappedTime, true, animationComponent, endTime, offsetTime, !forward);
						}
					}
					else
					{
						float offsetTime = forward ?
							m_Time - (endTime - newWrappedTime) :
							m_Time + (endTime - newWrappedTime);
						if (FireEvents (deltaTime, endTime, true, animationComponent, endTime, offsetTime, !forward))
						{
							// if event is right at the end time - we want to play it once, otherwise twice
							// we might get m_AnimationEventIndex=-1 in this statement, but that's acceptable situation
							// it indicates that there are no events until we reverse the time
							m_AnimationEventIndex = events.size() - (events.back().time == endTime ? 2 : 1);
							m_AnimationEventState = kAnimationEventState_HasEvent;
							// Events should never be searched in next call of FireEvents
							FireEvents (deltaTime, newWrappedTime, false, animationComponent, begTime, offsetTime, forward);
						}
					}
				}
			}
		}
		else
			ErrorString("Unknown wrapMode");

		DebugAssertIf(!IsFinite(newWrappedTime));

		// do not set m_WrappedTime if it was altered in one of events
		if (m_WrappedTime == oldWrappedTime)
			m_WrappedTime = newWrappedTime;
		DebugAssertIf(!IsFinite(m_Time) || !IsFinite(m_WrappedTime));
	}

	return UseUnity32AnimationFixes() ? UpdateFading(deltaTime) : UpdateFading_Before32(deltaTime);
}

// This is for backwards compatibility. If you need to make changes,
// then make them in UpdateFading which is used with Unity 3.2 and later content
bool AnimationState::UpdateFading_Before32(float deltaTime)
{
	// We are now fading out!
	if (m_Time > m_StopTime - m_FadeOutLength && !m_IsFadingOut)
	{
		SetWeightTarget(0.0F, m_FadeOutLength, true);
		m_IsFadingOut = true;

		///@TODO: Apply fractional delta based on current time!
		//		A fadeout should have been started at m_StopTime - m_FadeOutLength but we exceeded it by some time
		//		We should apply the fraction by what we exceeded it to the weight!
		//		ApplyWeightDeltaFraction(m_Time - m_FadeOutLength);
	}

	bool didStopAtEnd = false;
	// Update blend target
	if (m_FadeBlend)
		didStopAtEnd = UpdateBlendingWeight(deltaTime, false);

	return didStopAtEnd;
}

bool AnimationState::UpdateFading(float deltaTime)
{
	bool didStopAtEnd = false;

	// We are now fading out!
	if (!m_IsFadingOut && UseStopTime())
	{
		Assert(m_FadeOutLength >= 0);

		const bool forward = m_Speed >= 0;
		const float dt = forward ? m_Time - (m_StopTime - m_FadeOutLength) : (m_StopTime + m_FadeOutLength) - m_Time;
		if (dt > 0)
		{
			SetWeightTarget(0.0F, m_FadeOutLength, true);
			m_IsFadingOut = true;

			if (UseUnity35AnimationFixes())
			{
				// Applying fractional delta based on current time!
				// A fadeout has been started at m_StopTime - m_FadeOutLength but we exceeded it by some time,
				// so we need to apply delta on m_Weight
				didStopAtEnd = UpdateBlendingWeight(dt, m_FadeOutLength == 0);
			}
		}
	}

	// Update blend target
	if (m_FadeBlend)
		didStopAtEnd = UpdateBlendingWeight(deltaTime, false);

	return didStopAtEnd;
}

bool AnimationState::UpdateBlendingWeight(const float deltaTime, const bool instantBlend)
{
	bool didStopAtEnd = false;

	m_Weight += deltaTime * m_WeightDelta;

	// Stop blending and clamp when we reach the target
	if (instantBlend ||
		(m_WeightDelta > 0.0F && m_Weight > m_WeightTarget) ||
		(m_WeightDelta <= 0.0F && m_Weight < m_WeightTarget))
	{
		m_Weight = m_WeightTarget;
		m_FadeBlend = 0;
		m_IsFadingOut = 0;
		if (m_StopWhenFadedOut)
		{
			m_UnstoppedLastWrappedTime = m_WrappedTime;
			Stop();
			didStopAtEnd = true;
		}
	}
	else
	{
		#if !UNITY_RELEASE
		AssertMsg((m_WrapMode != kDefaultWrapMode && m_WrapMode != kClamp) || m_Time < GetLength() + 0.05F, "Time is out of range: %f < %f", m_Time, GetLength());
		#endif
	}

	return didStopAtEnd;
}

void AnimationState::DidModifyAnimationClip (AnimationClip* clip, AnimationStateList& states)
{
	AnimationStateList::iterator i;
	for (i=states.begin();i!=states.end();i++)
	{
		AnimationState& state = **i;
		if (clip == NULL)
		{
			state.m_Clip = NULL;
			state.m_HasAnimationEvent = 0;
		}
		else
		{
			AssertIf (state.m_Clip != clip);
			state.m_CachedRange = state.m_Clip->GetRange();
			AssertIf(!IsFinite(state.m_CachedRange.first) || !IsFinite(state.m_CachedRange.second));
			state.m_HasAnimationEvent = !state.m_Clip->GetEvents().empty();
		}

		PROFILER_AUTO(gModifyAnimationClip, NULL)

		state.m_DirtyMask |= kRebindDirtyMask;
	}

	if (clip == NULL)
		states.clear();
}

///@TODO: Import pipeline should allow reimporting clips while in playmode. For this we just need to make them reuse the animation clip asset
///       and call DidChangeClip

void AnimationState::Init(const UnityStr& name, AnimationClip* clip, double globalTime, int wrap, bool isClone )
{
	AssertIf (m_Clip);
	AssertIf (m_Curves);
	AssertIf (m_AnimationClipNode.IsInList());

	m_IsClone = isClone;
	m_Clip = clip;
	m_HasAnimationEvent = 0;
	if (m_Clip)
	{
		m_CachedRange = m_Clip->GetRange();
		AssertIf(!IsFinite(m_CachedRange.first) || !IsFinite(m_CachedRange.second));
		m_Clip->AddAnimationState(m_AnimationClipNode);
		m_Name = name;
		m_HasAnimationEvent = !m_Clip->GetEvents().empty();
	}

	m_BlendMode = kBlend;
	m_Weight = 0.0F;
	m_FadeBlend = 0;
	m_StopWhenFadedOut = 0;
	m_IsFadingOut = 0;
	m_AutoCleanup = 0;
	m_ShouldCleanup = 0;
	m_FadeOutLength = 0.0F;
	m_WrappedTime = 0.0F;
	m_AnimationEventIndex = -1;
	m_AnimationEventState = kAnimationEventState_Search;
	m_Time = 0;
//	m_WeightDelta = 0.0F;
//	m_UnstoppedLastWeight = 0.0F;
//	m_UnstoppedLastWrappedTime = 0.0F;
//	m_WeightTarget = 1.0F;
//	m_WeightDelta = 0.0F;
	m_LastGlobalTime = globalTime;

	m_Layer = 0;
	m_SyncedSpeed = m_Speed = 1.0F;
	m_Enabled = false;

	SetWrapMode(wrap);
	SetTime(0.0F);

//	m_GlobalStopTime = std::numeric_limits<float>::infinity();

	m_DirtyMask = kRebindDirtyMask | kLayersDirtyMask;
}

void AnimationState::SetupStopTime()
{
	bool forward = UseUnity32AnimationFixes() ? m_Speed >= 0 : true;

	m_StopTime = UseStopTime() ?
		(forward ? m_CachedRange.second : m_CachedRange.first) :
		(forward ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity());
}

void AnimationState::SetWrapMode (int wrap)
{
	m_WrapMode = wrap;

	// we need to update m_WrappedTime, because we can set time first and then wrap mode
	m_WrappedTime = WrapTime(m_Time, m_CachedRange, m_WrapMode);
	DebugAssertIf(!IsFinite(m_Time) || !IsFinite(m_WrappedTime));

	SetupStopTime();
}

void AnimationState::SetEnabled (bool enabled)
{
	if (enabled && !m_Enabled)
		m_LastGlobalTime = GetCurTime();
	m_Enabled = enabled;
}

void AnimationState::AddMixingTransform(Transform& transform, bool recursive)
{
	m_MixingTransforms.insert(std::make_pair(PPtr<Transform> (&transform), recursive));
	m_DirtyMask |= kRebindDirtyMask;

	PROFILER_AUTO(gAddMixingTransform, NULL)
}

void AnimationState::RemoveMixingTransform(Transform& transform)
{
	MixingTransforms::iterator it = m_MixingTransforms.find(PPtr<Transform>(&transform));
	if (it != m_MixingTransforms.end())
		m_MixingTransforms.erase(it);
	else
	{
		ErrorStringMsg("RemoveMixingTransform couldn't find transform '%s' in a list of mixing transforms. "
			"You can only remove transforms that have been added through AddMixingTransform", transform.GetName());
	}
	m_DirtyMask |= kRebindDirtyMask;

	PROFILER_AUTO(gRemoveMixingTransform, NULL)
}


// TODO : this looks a bit expensive: it's iterating hierarchy every time
bool AnimationState::ShouldMixTransform (Transform& transform)
{
	if (m_MixingTransforms.empty())
		return true;

	for (MixingTransforms::iterator i=m_MixingTransforms.begin();i != m_MixingTransforms.end();i++)
	{
		if (i->second)
		{
			Transform* root = i->first;
			if (root && IsChildOrSameTransform(transform, *root))
				return true;
		}
		else
		{
			if (i->first == PPtr<Transform> (&transform))
				return true;
		}
	}

	return false;
}

void AnimationState::Stop ()
{
	if ( m_Enabled && m_AutoCleanup )
		m_ShouldCleanup = true;

	m_Enabled = false;
	SetTime(0.0F);
	m_FadeBlend = 0;
	m_StopWhenFadedOut = 0;
}

void AnimationState::SetupUnstoppedState()
{
	std::swap(m_WrappedTime, m_UnstoppedLastWrappedTime);
	m_UnstoppedLastWeight = m_Weight;
	// HACK: VERY ugly way of making sure the last frame of an animation gets accounted into the final position. was 1.0f but this
	// gave problems when crossfading since the state that should no longer contribute, suddently had a weight of 1 for one frame
	// We need to redo some of the logic in here at some point
	m_Weight = kReallySmallWeight*1.001f;
	m_Enabled = true;
}

void AnimationState::CleanupUnstoppedState()
{
	std::swap(m_WrappedTime, m_UnstoppedLastWrappedTime);
	m_Weight = m_UnstoppedLastWeight;
	m_Enabled = false;
}

bool AnimationState::UseUnity32AnimationFixes()
{
	return IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1);
}

bool AnimationState::UseUnity34AnimationFixes()
{
	return IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_4_a1);
}

bool AnimationState::UseUnity35AnimationFixes()
{
	return IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_5_a1);
}

