#pragma once
#include <vector>
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/BaseClasses/RefCounted.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/AnimationCurve.h"

template<class T> class AnimationCurveTpl; typedef AnimationCurveTpl<float> AnimationCurve;
template<class T> class AnimationCurveTpl; typedef AnimationCurveTpl<float> AnimationCurveBase;
class AnimationClip;
class Transform;
class AABB;
namespace Unity { class Component; }

#define GET_SET_REF(a,b,c)	void Set##b (a& val) { c = val; }	a& Get##b () const {return c; }

#define kReallySmallWeight 0.0001F
#define kReallySmallFadeTime 0.001F

enum {
	kRebindDirtyMask   = 1 << 0,
	kLayersDirtyMask   = 1 << 1
};


class AnimationState : public TrackedReferenceBase
{
private:
	// Indicates state of m_AnimationEventIndex
	enum { 
		// m_AnimationEventIndex is valid
		kAnimationEventState_HasEvent = 0, 
		// m_AnimationEventIndex need to be researched
		kAnimationEventState_Search, 
		// m_AnimationEventIndex there are no more keys available, no need to search
		kAnimationEventState_NotFound, 
		// This is very special case:
		// m_AnimationEventIndex points to a valid index and AnimationState is paused, 
		// but this event has been triggered already, so when AnimationState state is unpaused
		// we have to continue triggering event on left or right side of this event 
		// (depending on the sign of AnimationState speed)
		kAnimationEventState_PausedOnEvent,

		// Used for assert
		kAnimationEventState__Count
	};

public:

	enum { kBlend = 0, kAdditive };
	
	AnimationState ();
	~AnimationState ();
	
	static void InitializeClass ();
	static void CleanupClass ();

	/// Makes the animation state reach a /target/ blend weight in length seconds.
	/// If the animation is already fading towards target and it would reach the target
	/// faster, then the weight speed will not be modified.
	/// if stopWhenFaded is enabled, the animation will stop when the target is reached.
	void SetWeightTarget (float target, float length, bool stopWhenFaded);

	void SetWeightTargetImmediate (float target, bool stopWhenFaded);

	void Stop ();
	
	// This automatically sets m_LastGlobalTime from globa
	void SetEnabled (bool enabled);

	bool GetEnabled () const                 { return m_Enabled; }
	
	void SetTime (float time);
	const float GetTime() const { return m_Time; }
	
	int GetWrapMode () const { return m_WrapMode; } 
	void SetWrapMode (int mode);
	
	GET_SET_REF(const UnityStr, Name, m_Name)
	GET_SET_REF(const UnityStr, ParentName, m_ParentName)
	
	void SetLayer (int layer)             { m_Layer = layer; m_DirtyMask |= kLayersDirtyMask; }
	int GetLayer () const                 { return m_Layer; }
	
	void SetWeight (float val)            { m_Weight = val; }
	float GetWeight () const              { return m_Weight; }

	void SetNormalizedSpeed (float speed) {	m_SyncedSpeed = m_Speed = speed * GetLength(); }
	float GetNormalizedSpeed () const     { return m_Speed / GetLength(); }

	void SetSpeed (float speed);
	float GetSpeed () const               { return m_Speed; }
	float GetSyncedSpeed () const         { return m_SyncedSpeed; }

	void SetNormalizedTime (float time)   { SetTime( time * GetLength()); }
	float GetNormalizedTime () const      { return m_Time / GetLength(); }

	float GetLength () const              { return m_CachedRange.second; }
	
	/// Sets the fadeout length and stop time based on the used wrapmode.
	void SetupFadeout (float length);
	
	int GetBlendMode() const              { return m_BlendMode; }
	void SetBlendMode(int mode)           { m_BlendMode = mode; }
	
	AnimationClip* GetClip()              { return m_Clip; }

	void ClearDirtyMask()                 { m_DirtyMask = 0; }
	UInt32 GetDirtyMask () const          { return m_DirtyMask; }

	// Returns true if the state is enabled and has a reasonably high weight
	inline bool ShouldUse() const;
	
	// Used for layer syncing
	void SetNormalizedSyncedSpeed (float speed)     {	m_SyncedSpeed = speed * GetLength(); }

	bool UpdateAnimationState (double globalTime, Unity::Component& animation);

	void Init(const UnityStr& name, AnimationClip* clip, double globalTime, int wrap, bool isClone = false);
	
	void CleanupCurves ();
	void SetAutoCleanup(){ m_AutoCleanup = 1; }
	void ForceAutoCleanup() { SetAutoCleanup(); m_ShouldCleanup = true; } 
	bool ShouldAutoCleanupNow () { return m_ShouldCleanup; }
	bool IsClone() { return m_IsClone; }
	
	typedef AnimationCurveBase** Curves;
	Curves GetCurves() { return m_Curves; }
	Curves const  GetCurves() const { return m_Curves; }

	void AllocateCurves(int count);
	void SetClonedCurves(AnimationState& state);

	bool ShouldMixTransform (Transform& transform);
	void AddMixingTransform(Transform& transform, bool recursive);
	void RemoveMixingTransform(Transform& transform);

	/// When an animation is stopped and it is the only animation playing.
	/// Then you usually want the last frame to display before the animation stops.
	/// For example an elevator moving up. It should always make sure the last frame gets sampled.
	/// -> Now unfortunately we stop time during UpdateAnimationState which means the time gets reset
	/// -> Which wrapped time and weight will be set to zero when actually sampling the animation.
	/// -> So we just fix the very specific, single animation playing and stopping case,
	///     by storing the wrap mode prior to animation stop and then afterwards reverting it again.
	/// *** I guess the right way to solve this would be to make animation stopping happen after sampling or something...
	void SetupUnstoppedState ();
	void CleanupUnstoppedState ();

	bool FireEvents (const float deltaTime, float newWrappedTime, bool reverse, Unity::Component& animation, const float beginTime, const float offsetTime, const bool reverseOffsetTime);

	// We made a bunch of fixes in Unity 3.2, but we couldn't use them since it breaks backwards compatibility,
	// so we enable the fixes only for 3.2 content
	// TODO : get rid of this function and all related backwards compatible functions as soon as we are allowed to break backwards compatibility
	static bool UseUnity32AnimationFixes();

	// Same story (see above) with animation fixes in Unity 3.4
	static bool UseUnity34AnimationFixes();

	// Same story (see above) with animation fixes in Unity 3.5
	static bool UseUnity35AnimationFixes();

private:
	bool UseStopTime() const { return m_WrapMode == kClamp || m_WrapMode == kDefaultWrapMode; }
	void SetupStopTime();

	typedef List< ListNode<AnimationState> > AnimationStateList;
	static void DidModifyAnimationClip (AnimationClip* clip, AnimationStateList& states);

	bool UpdateFading_Before32(float deltaTime);
	bool UpdateFading(float deltaTime);

	bool UpdateBlendingWeight(const float deltaTime, const bool instantBlend);

private:
	Curves             m_Curves;
//	int                m_CurvesCount;

	float              m_Weight;
	float              m_WrappedTime;  // Always keep in animation clip length range

	double             m_Time; // Keeps on increasing forever -> higher precision
	double             m_LastGlobalTime; // Keeps on increasing forever -> higher precision

	int                m_Layer;
	float              m_Speed;
	float              m_SyncedSpeed;
	
	float              m_StopTime;
	
	float              m_FadeOutLength;
	float              m_WeightTarget;
	
	UInt32             m_FadeBlend : 1;
	UInt32             m_Enabled : 1;
	UInt32             m_StopWhenFadedOut : 1;
	UInt32             m_AutoCleanup : 1;
	UInt32             m_OwnsCurves : 1;
	UInt32             m_IsFadingOut : 1;
	UInt32             m_ShouldCleanup : 1;
	UInt32             m_HasAnimationEvent : 1;
	UInt32			   m_IsClone : 1;
	// make sure that there are enough bits to store kAnimationEventStates
	UInt32			   m_AnimationEventState : 2;
	int                m_AnimationEventIndex;
	
	UInt32             m_DirtyMask;

	int                m_WrapMode; ///< enum { Default = 0, Once = 1, Loop = 2, PingPong = 4, ClampForever = 8 }
	int                m_BlendMode; ///< enum { Blend = 0, Additive = 1 }

	float              m_WeightDelta;
	
	///@TODO: FIXME HACKED Time stopping
	float              m_UnstoppedLastWrappedTime;
	float              m_UnstoppedLastWeight;
	
	std::pair<float, float> m_CachedRange;
	
	AnimationClip*     m_Clip;
	ListNode<AnimationState> m_AnimationClipNode;

	UnityStr           m_Name;
	UnityStr           m_ParentName;
	
	typedef std::map<PPtr<Transform>, bool> MixingTransforms;
	MixingTransforms   m_MixingTransforms;

	friend class Animation;
};

float WrapTime (float time, const std::pair<float, float>& range, int m_WrapMode);

inline bool AnimationState::ShouldUse() const
{
	return m_Clip && m_Enabled && m_Weight > kReallySmallWeight; 
}

// For debugging purposes display some of the animation state information!
#if UNITY_EDITOR
#include "Runtime/Serialize/SerializeTraits.h"
template<>
class SerializeTraits<AnimationState*> : public SerializeTraitsBase<AnimationState*>
{
	public:

	typedef AnimationState*	value_type;
	inline static const char* GetTypeString (void*)	{ return "AnimationState"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return true; }
	inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		TRANSFER_PROPERTY_DEBUG(UnityStr, m_Name, data->GetName)
		TRANSFER_PROPERTY_DEBUG(bool, m_Enabled, data->GetEnabled)
		transfer.Align();
		TRANSFER_PROPERTY_DEBUG(float, m_Weight, data->GetWeight)
		TRANSFER_PROPERTY_DEBUG(float, m_Time, data->GetTime)
		TRANSFER_PROPERTY_DEBUG(float, m_Speed, data->GetSpeed)
		TRANSFER_PROPERTY_DEBUG(float, m_SyncedSpeed, data->GetSyncedSpeed)
		TRANSFER_PROPERTY_DEBUG(int, m_WrapMode, data->GetWrapMode)
		TRANSFER_PROPERTY_DEBUG(int, m_BlendMode, data->GetBlendMode)
		TRANSFER_PROPERTY_DEBUG(PPtr<AnimationClip>, m_Clip, data->GetClip)
	}
};

#endif
