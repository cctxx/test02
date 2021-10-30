#ifndef ANIMATION_H
#define ANIMATION_H

#include "Runtime/GameCode/Behaviour.h"
#include "BoundCurveDeprecated.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Camera/UnityScene.h"

class AnimationState;
class Transform;

class AnimationClip;

struct QueuedAnimation
{
	int             mode;
	int             queue;
	float           fadeTime;
	
	AnimationState* state;
};

class Animation : public Behaviour
{
public:
	enum CullingType { kCulling_AlwaysAnimate, kCulling_BasedOnRenderers, kDeprecatedCulling_BasedOnClipBounds, kDeprecatedCulling_BasedOnUserBounds };

public:
	typedef UNITY_VECTOR(kMemAnimation, PPtr<AnimationClip>) Animations;
	typedef dynamic_array<BoundCurveDeprecated> BoundCurves;
	typedef UNITY_VECTOR(kMemAnimation, AnimationState*) AnimationStates;
	typedef AnimationStates::iterator iterator;
	typedef vector_set<int> SyncedLayers;
	typedef UNITY_VECTOR(kMemAnimation, Renderer*) ContainedRenderers;
	typedef UNITY_VECTOR(kMemAnimation, QueuedAnimation) QueuedAnimations;
	typedef UNITY_VECTOR(kMemAnimation, Transform*) AffectedRootTransforms;

	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }

private:
	int                          m_WrapMode;///< enum { Default = 0, Once = 1, Loop = 2, PingPong = 4, ClampForever = 8 }
	bool                         m_PlayAutomatically;
	bool                         m_AnimatePhysics;
	bool                         m_Visible;
	CullingType m_CullingType; ///< enum { Always Animate = 0, Based On Renderers = 1 }
	
	/// When we are animating transforms we cache the affect root transforms
	/// (The top most transform that covers all SendTransformChanged messages that need to sent)
	AffectedRootTransforms   m_CachedAffectedSendToRootTransform;
	int                          m_CachedTransformMessageMask;
	ContainedRenderers   		 m_ContainedRenderers;

	// new stuff
	BoundCurves                  m_BoundCurves;
	AnimationStates	             m_AnimationStates;
	AnimationState*			     m_ActiveAnimationStates[32];
	int							 m_ActiveAnimationStatesSize;

	UInt32                       m_DirtyMask;
	ListNode<Animation>    m_AnimationManagerNode;
	SyncedLayers                 m_SyncedLayers;	

	PPtr<AnimationClip>          m_Animation;
	Animations                   m_Animations;
	
	QueuedAnimations             m_Queued;

#if UNITY_EDITOR	
	typedef std::vector<std::pair<UnityStr, PPtr<AnimationClip> > > OldAnimations;
	OldAnimations                m_OldAnimations;
#endif

	void PlayClip (AnimationClip& animation, int mode);
	void RecomputeContainedRenderers ();
	void RecomputeContainedRenderersRecurse (Transform& transform);
	void ClearContainedRenderers ();
	void SampleInternal();
public:
	
	REGISTER_DERIVED_CLASS (Animation, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Animation)
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	Animation (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Animation (); declared-by-macro

	/// Are any animation states playing?
	/// (Returns true even if the animation state has a zero blend weight)
	/// (Unaffected by animation.enabled or animation.visible)
	bool IsPlaying ();

	/// Is the 
	/// (Returns true even if the animation state has a zero blend weight)
	/// (Unaffected by animation.enabled or animation.visible)
	bool IsPlaying (const string& name);

	/// Is the animation state playing?
	/// (Returns true even if the animation state has a zero blend weight)
	/// (Unaffected by animation.enabled or animation.visible)
	bool IsPlaying (const AnimationState& state);

	/// Is any animation in the layer playing?
	/// (Returns true even if the animation state has a zero blend weight)
	/// (Unaffected by animation.enabled or animation.visible)
	bool IsPlayingLayer (int layer);

	/// Stops all animations that were started from this component with play or play named!
	void Stop ();
	/// Stops all animations that were started from this component with name!
	void Stop (const string& name);
	void Stop (AnimationState& state);

	
	void Rewind ();
	void Rewind (const string& name);
	void Rewind (AnimationState& state);

	void SyncLayer (int layer) { m_SyncedLayers.insert(layer); }
	
	void SetWrapMode (int mode);
	int GetWrapMode () { return m_WrapMode; }
	
	PPtr<AnimationClip> GetClip () const { return m_Animation; }
	void SetClip (PPtr<AnimationClip> anim);
	
	const Animations& GetClips () const { return m_Animations; }
	void SetClips (const Animations& anims);
		
//	PPtr<AnimationClip> GetNamedClip (const string& name);
	
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void Deactivate (DeactivateOperation operation);
	
	bool GetPlayAutomatically () const { return m_PlayAutomatically; }
	void SetPlayAutomatically (bool b) { m_PlayAutomatically = b; SetDirty (); }
		
	void SetCullingType(CullingType type);
	CullingType GetCullingType() const { return m_CullingType; }
			
	void CheckRendererVisibleState ();

	// Exposed for Renderer
	void SetVisibleRenderers(bool visible);
	// Exposed for UnityScene
	void SetVisibleBounds(bool visible);
	
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	
	enum PlayMode { kStopSameLayer = 0, kPlayQueuedDeprecated = 1, kPlayMixedDeprecated = 2, kStopAll = 4  };
//	enum PlayMode { kStopAll = 0, kPlayQueued = 1, kPlayMixedDeprecated = 2 };
	
	bool Play(const std::string& name, int playMode);
	void Play(AnimationState& fadeIn, int playMode);
	bool Play(int playMode);

	/// 
	void Blend(const std::string& name, float targetWeight, float time);
	void Blend(AnimationState& fadeIn, float targetWeight, float time);
	
	void CrossFade(const std::string& name, float time, int mode);
	void CrossFade(AnimationState& fadeIn, float time, int mode, bool clearQueuedAnimations );
	
	enum QueueMode { CompleteOthers = 0, PlayNow = 2 };

	AnimationState* QueueCrossFade(const std::string& name, float time, int queue, int mode);
	AnimationState* QueueCrossFade(AnimationState& originalState, float time, int queue, int mode);

	AnimationState* QueuePlay(const std::string& name, int queue, int mode) { return QueueCrossFade(name, 0.0F, queue, mode); }
	AnimationState* QueuePlay(AnimationState& originalState, int queue, int mode) { return QueueCrossFade(originalState, 0.0F, queue, mode); }

	///	Adds an animation clip with name newName to the animation.
	/// - If newName is not the clip's name or the animation clip needs to be clipped a new instance of the clip will be created.
	void AddClip (AnimationClip& clip, const std::string& newName, int firstFrame, int lastFrame, bool loop);

	/// Adds an animation clip to the animation. If it already exists this will do nothing.
	void AddClip (AnimationClip& clip);
	
	/// Removes an named clip
	void RemoveClip (AnimationClip& clip);
	void RemoveClip (const std::string &clipName);
	
	/// Get the number of clips in the animation
	int GetClipCount () const;
	
	void SyncLayerTime (int layer);

	iterator begin () { return m_AnimationStates.begin(); }
	iterator end () { return m_AnimationStates.end(); }

	/// State management	
	AnimationState& GetAnimationStateAtIndex(int index) { BuildAnimationStates(); return *m_AnimationStates[index]; }
	int GetAnimationStateCount () { BuildAnimationStates(); return m_AnimationStates.size(); }

	AnimationState* GetState(const std::string& name);
	AnimationState* GetState(AnimationClip* clip);
	AnimationState* GetState(AnimationState* state);
	
	AnimationState* CloneAnimation (AnimationState* state);

	/// Returns the animation clip named name
	/// - This will onyl search the serialized animation array and not touch animation states at all!
	AnimationClip* GetClipWithNameSerialized (const std::string& name);
	
	void UpdateAnimation (double time);
	void SampleDefaultClip (double time);

	void RebuildStateForEverything();
	bool RebuildBoundStateMask();
	
	void Sample();
	
	void BlendOptimized();
	void BlendGeneric();
	void BlendAdditive();

	void SortAnimationStates();
	void ReleaseAnimationStates();

	void ApplyObjectSlow(Object* lastObject);

#if UNITY_EDITOR
	void LoadOldAnimations (); // Deprecated with 1.5
#endif	
	void SendTransformChangedToCachedTransform();
	void RemoveContainedRenderer (Renderer* renderer);
	
	void SetAnimatePhysics (bool anim);
	bool GetAnimatePhysics () { return m_AnimatePhysics; }
	
	void CleanupBoundCurves ();
	void ValidateBoundCurves ();
	
	void EnsureDefaultAnimationIsAdded ();

	AABB GetLocalAABB () const { return AABB::zero; }
	void SetLocalAABB (const AABB& aabb) {  }
	
	#if UNITY_EDITOR
	/// Forces the inspector auto refresh without setdirty being called but only in debug mode
	virtual bool HasDebugmodeAutoRefreshInspector () { return true; }
	#endif

private:
	
	AnimationClip* GetClipLegacyWarning (AnimationClip* clip);

	void CheckIsCullingBasedOnBoundsDeprecated() const { Assert(m_CullingType != kDeprecatedCulling_BasedOnClipBounds); Assert(m_CullingType != kDeprecatedCulling_BasedOnUserBounds); }
	void SetVisibleInternal(bool visible);
	void BuildAnimationStates();

	void UpdateAnimationInternal_Before32(double time);
	void UpdateAnimationInternal(double time);	

	void UpdateQueuedAnimations_Before34(bool& needsUpdate);
	void UpdateQueuedAnimations(bool& needsUpdate);
};

#endif
