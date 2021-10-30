#pragma once 

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Animation/AvatarPlayback.h"
#include "Runtime/Misc/UserList.h"
#include "Runtime/mecanim/statemachine/statemachine.h"

namespace math
{
	struct xform;
}
class Avatar;
class Renderer;
class Transform;
class AnimationClip;
class AnimatorController;
class RuntimeAnimatorController;
class AnimatorOverrideController;

namespace mecanim
{	
	namespace animation
	{
		struct AvatarConstant;
		struct AvatarInput;
		struct AvatarOutput;
		struct AvatarMemory;
		struct AvatarWorkspace;
		struct ControllerConstant;
		struct ControllerMemory;
		struct AnimatorOverrideController;
		struct AnimationSetMemory;
		struct BlendTreeNodeConstant;
		struct BlendTreeWorkspace;
	}
	
	namespace skeleton
	{
		struct SkeletonPose;
	}
	namespace human
	{
		struct HumanPose;
	}
}

namespace UnityEngine
{
	namespace Animation
	{
		struct AvatarBindingConstant;
		struct AnimatorGenericBindingConstant;
		struct AnimatorTransformBindingConstant;
		struct AnimationSetBindings;
	}
}



enum GetSetValueResult { kGetSetSuccess = 0, kParameterMismatchFailure = 1, kParameterDoesNotExist = 2, kAnimatorNotInitialized = 3, kParameterIsControlledByCurve = 4, kAnimatorInPlaybackMode = 5  };

struct AnimationInfo
{
	PPtr<AnimationClip>	clip;
	float				weight;

	AnimationInfo()
	{
		Clear();
	}
	
	void Clear()
	{	
		weight = 0;
	}
};

/// This struct must be kept in sync with the C# version in AnimatorBindings.txt
struct AnimatorTransitionInfo
{
	int		nameHash;
	int		userNameHash;
	float	normalizedTime;	

	AnimatorTransitionInfo() 
	{
		Clear();
	}
	
	void Clear()
	{
		nameHash = 0; 
		userNameHash = 0;
		normalizedTime = 0;	
	}
};

/// This struct must be kept in sync with the C# version in AnimatorBindings.txt
struct AnimatorStateInfo
{	
	int    nameHash;
	int    pathHash;
	float  normalizedTime;
	float  length;
	int    tagHash;
	int    loop;
	
	AnimatorStateInfo() 
	{
		Clear();
	}

	void Clear()
	{
		pathHash = 0;
		nameHash = 0; 
		normalizedTime = 0;
		length = 0;		
		tagHash = 0;
		loop = 0;
	}
};

/// This struct must be kept in sync with the C# version in AnimatorBindings.txt
struct MatchTargetWeightMask
{
	MatchTargetWeightMask(Vector3f positionXYZWeight, float rotationWeight)
		: m_PositionXYZWeight(positionXYZWeight), m_RotationWeight(rotationWeight) {}

	Vector3f m_PositionXYZWeight;
	float m_RotationWeight;
};

enum RecorderMode
{
	eNormal = 0,
	ePlayback= 1,
	eRecord =2	
};

typedef std::vector<PPtr<AnimationClip> > AnimationClipVector;

class Animator: public Behaviour
{
public:	
	
	enum CullingMode { kCullAlwaysAnimate = 0, kCullBasedOnRenderers = 1 };
	
	REGISTER_DERIVED_CLASS (Animator, Behaviour)
	DECLARE_OBJECT_SERIALIZE (Animator)
	
	static void InitializeClass ();
	static void CleanupClass ();
	
	Animator (MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Deactivate (DeactivateOperation operation);
	virtual void Reset ();
	virtual void CheckConsistency();

	virtual void AddToManager ();
	virtual void RemoveFromManager ();

	void Update (float deltaTime);
	bool Sample (AnimationClip& clip, float inTime);

	virtual void TransformChanged (int changeMask);
	void OnAddComponent(Component* com);

	bool IsValid() const;
	bool IsInManagerList() const;	

	RuntimeAnimatorController* GetRuntimeAnimatorController() const;
	void SetRuntimeAnimatorController(RuntimeAnimatorController* animation) ;

	AnimatorController* GetAnimatorController() const;		
	AnimatorOverrideController* GetAnimatorOverrideController() const;
	
	Avatar* GetAvatar();
	void SetAvatar(Avatar* avatar);

	const mecanim::animation::AvatarConstant* GetAvatarConstant();
 
	bool IsOptimizable() const;
	bool IsHuman() const;
	bool HasRootMotion() const;
	float GetHumanScale() const;

	GetSetValueResult GetFloat(int id, float& value);
	GetSetValueResult SetFloat(int id, float value);
	GetSetValueResult SetFloatDamp(int id, float value, float dampTime, float deltaTime);
	
	GetSetValueResult GetInteger(int id, int& output);
	GetSetValueResult SetInteger(int id, int integer);

	GetSetValueResult GetBool(int id, bool& output);
	GetSetValueResult SetBool(int id, bool value);

	GetSetValueResult ResetTrigger(int id);
	GetSetValueResult SetTrigger(int id);

	bool		HasParameter(int id);

	bool		GetMuscleValue(int id, float *value);

	GetSetValueResult ParameterControlledByCurve(int id);

	Vector3f    GetAvatarPosition();
	Quaternionf GetAvatarRotation();
	Vector3f    GetAvatarScale();

	void		SetAvatarPosition(const Vector3f& rootPosition);
	void        SetAvatarRotation(const Quaternionf& rootRotation);
	void		SetAvatarScale(const Vector3f&rootScale);

	Vector3f	GetDeltaPosition();
	Quaternionf	GetDeltaRotation();

	Vector3f	GetBodyPosition();
	Quaternionf	GetBodyRotation();

	void		SetBodyPosition(const Vector3f& rootPosition);
	void        SetBodyRotation(const Quaternionf& rootRotation);

	float		GetPivotWeight();
	Vector3f	GetPivotPosition();
	
	bool        GetApplyRootMotion () const            { return m_ApplyRootMotion; }
	void        SetApplyRootMotion (bool rootMotion);
	
	bool        GetAnimatePhysics () const            { return m_AnimatePhysics; }
	void        SetAnimatePhysics (bool animatePhysics);

	float		GetGravityWeight();

	bool		SupportsOnAnimatorMove();
	
	void		MatchTarget(Vector3f const& matchPosition, Quaternionf const& matchRotation, int targetIndex, const MatchTargetWeightMask& mask,  float startNormalizedTime, float targetNormalizedTime);
	void		InterruptMatchTarget(bool completeMatch = true);
	bool		IsMatchingTarget()const;
	
	void		SetSpeed(float speed);
	float		GetSpeed() const ;	

	void		GotoState(int layer, int stateId, float normalizedTime, float transitionDuration, float transitionTime = 0.0F);

	void		SetTarget(int targetIndex, float targetNormalizedTime);
	Vector3f	GetTargetPosition();
	Quaternionf	GetTargetRotation();

	bool		IsBoneTransform(Transform *transform);
	Transform*	GetBoneTransform(int humanBoneId);	

	Vector3f	GetGoalPosition(int index);
	void		SetGoalPosition(int index, Vector3f const& pos);

	Quaternionf	GetGoalRotation(int index);
	void		SetGoalRotation(int index, Quaternionf const& rot);

	void		SetGoalWeightPosition(int index, float value);
	void		SetGoalWeightRotation(int index, float value);

	float		GetGoalWeightPosition(int index);
	float		GetGoalWeightRotation(int index);

	void		SetLookAtPosition(Vector3f lookAtPosition);
	void		SetLookAtClampWeight(float weight);
	void		SetLookAtBodyWeight(float weight);
	void		SetLookAtHeadWeight(float weight);
	void		SetLookAtEyesWeight(float weight);

	int			GetLayerCount()const;
	std::string	GetLayerName(int layerIndex);
	float		GetLayerWeight(int layerIndex);
	void		SetLayerWeight(int layerIndex, float w);
	
	void        SetCullingMode (CullingMode mode);
	CullingMode GetCullingMode () const                                         { return m_CullingMode; }
	
	bool		IsInTransition(int layerIndex)const;

	bool		ShouldInterruptMatchTarget()const;
    
	bool		GetAnimatorStateInfo (int layerIndex, bool currentState, AnimatorStateInfo& output);	
	bool		GetAnimatorTransitionInfo (int layerIndex, AnimatorTransitionInfo& output);

	string		GetAnimatorStateName (int layerIndex, bool currentState);	

	bool		GetAnimationClipState(int layerIndex, bool currentState, dynamic_array<AnimationInfo>& output);

	void		GetRootBlendTreeConstantAndWorkspace (int layerIndex, int stateHash, mecanim::animation::BlendTreeNodeConstant const* & constant, mecanim::animation::BlendTreeWorkspace*& workspace);
	
	float		GetFeetPivotActive();
	void		SetFeetPivotActive(float value);

	bool		GetStabilizeFeet();
	void		SetStabilizeFeet(bool value);

	float		GetLeftFeetBottomHeight();
	float		GetRightFeetBottomHeight();
		
	void		WriteHumanPose(mecanim::human::HumanPose &pose);
	void		WriteDefaultPose();
	
	const		mecanim::skeleton::SkeletonPose* GetGlobalSpaceSkeletonPose () const;
	
	void StartPlayback();
	void SetPlaybackTime(float time); 
	float GetPlaybackTime();
	void StopPlayback();
	
	void PrepareForPlayback();
	void StartRecording(int frameCount = 0);
	void StopRecording();
	
	float GetRecorderStartTime();
	float GetRecorderStopTime();
	
	int  GetBehaviourIndex ()          { return m_BehaviourIndex; }
	void SetBehaviourIndex (int index) { m_BehaviourIndex = index; }

	int  GetFixedBehaviourIndex ()          { return m_FixedBehaviourIndex; }
	void SetFixedBehaviourIndex (int index) { m_FixedBehaviourIndex = index; }

	static void UpdateAvatars (Animator** inputAvatars, size_t inputSize, float deltaTime, bool doFKMove, bool doRetargetIKWrite);

	bool IsAvatarInitialize() const;
	bool IsInitialize() const;

	void ValidateParameterString (GetSetValueResult result, const std::string& parameterName);
	void ValidateParameterID (GetSetValueResult result, int identifier);


	void SetLayersAffectMassCenter(bool value);
	bool GetLayersAffectMassCenter() const ;

	void SetHasTransformHierarchy (bool value);
	bool GetHasTransformHierarchy () const { return m_HasTransformHierarchy; }
	
	void EvaluateSM();
	
	GET_SET(bool, LogWarnings, m_LogWarnings);
	GET_SET(bool, FireEvents, m_FireEvents);
	
	std::string GetPerformanceHints();

	AnimationClipVector								GetAnimationClips()const;
	UnityEngine::Animation::AnimationSetBindings*	GetAnimationSetBindings()const;
	

	Transform*	GetAvatarRoot();
protected:	
	
	struct MecanimDataSet
	{
		MecanimDataSet():
			m_AvatarConstant(0),
			m_AvatarInput(0),
			m_AvatarOutput(0),
			m_AvatarMemory(0),
			m_AvatarWorkspace(0),
			m_ControllerConstant(0),
			m_ControllerMemory(0),
			m_AnimationSetMemory(0),
			m_GenericBindingConstant(0),
			m_AvatarBindingConstant(0),
			m_AvatarMemorySize(0),
			m_OwnsAvatar(false)
		{
		}

		mecanim::animation::AvatarConstant const*		m_AvatarConstant;
		mecanim::animation::AvatarInput*				m_AvatarInput;
		mecanim::animation::AvatarOutput*				m_AvatarOutput;
		mecanim::animation::AvatarMemory*				m_AvatarMemory;
		mecanim::animation::AvatarWorkspace*			m_AvatarWorkspace;
		mecanim::animation::ControllerConstant const*	m_ControllerConstant;
		mecanim::animation::ControllerMemory*			m_ControllerMemory;
		mecanim::animation::AnimationSetMemory*			m_AnimationSetMemory;
		
		UnityEngine::Animation::AnimatorGenericBindingConstant*		m_GenericBindingConstant;
		UnityEngine::Animation::AvatarBindingConstant*				m_AvatarBindingConstant;
		
		size_t														m_AvatarMemorySize;
		
		bool											m_OwnsAvatar;	

		void Reset()
		{
			m_AvatarConstant=0;
			m_AvatarInput=0;
			m_AvatarOutput=0;
			m_AvatarMemory=0;
			m_AvatarWorkspace=0;
			m_ControllerConstant=0;
			m_ControllerMemory=0;
			m_AnimationSetMemory=0;
			m_GenericBindingConstant=0;
			m_AvatarBindingConstant=0;
			m_AvatarMemorySize=0;
			m_OwnsAvatar = false;
		}
	};

	// Used by Animator::Sample to auto unregister bindings.
	struct AutoMecanimDataSet
	{
		mecanim::memory::ChainedAllocator	m_Alloc;
		MecanimDataSet						m_MecanimDataSet;

		AutoMecanimDataSet(size_t size):m_Alloc(size){}
		~AutoMecanimDataSet();

		Animator::MecanimDataSet const* operator ->() const{ return &m_MecanimDataSet; }
		Animator::MecanimDataSet * operator ->(){ return &m_MecanimDataSet; }
		Animator::MecanimDataSet const& operator *()const{ return m_MecanimDataSet; }
		Animator::MecanimDataSet& operator *(){ return m_MecanimDataSet; }

		void Reset();
	};


	void ClearObject();
	void CreateObject();

	void SetupAvatarMecanimDataSet(mecanim::animation::AvatarConstant const* avatarConstant, mecanim::memory::Allocator& allocator, Animator::MecanimDataSet& outMecanimDataSet);
	void SetupControllerMecanimDataSet(mecanim::animation::ControllerConstant const* controllerConstant, UnityEngine::Animation::AnimationSetBindings const* animationSetBindings, mecanim::memory::Allocator& allocator, Animator::MecanimDataSet& outMecanimDataSet);
	
	void WriteSkeletonPose(mecanim::skeleton::SkeletonPose& pose);

	bool IsInTransitionInternal(int layerIndex)const;
		
	bool Prepare();
	void InitStep(float deltaTime);
	void FKStep();
	void RetargetStep();
	void AvatarIKAndEndStep();
	void AvatarWriteStep();
	void ApplyOnAnimatorIK(int layerIndex);
	void ApplyOnAnimatorMove();
	void ApplyBuiltinRootMotion();
	void FireAnimationEvents();

	void Record(float deltaTime);
	
	
	// Visibility culling
	void ClearContainedRenderers ();
	void RecomputeContainedRenderersRecurse (Transform& transform);
	void InitializeVisibilityCulling ();
	void SetVisibleRenderers(bool visible);
	bool IsAnyRendererVisible () const;
	void RemoveContainedRenderer (void* renderer);

	static void AnimatorVisibilityCallback (void* userData, void* sender, int visibilityEvent);
		
	static void* FKStepStatic (void* userData);
	static void* RetargetStepStatic (void* userData);
	static void* AvatarIKAndEndStepStatic (void* userData);
	static void* AvatarWriteStepStatic (void* userData);
	
	int                             m_BehaviourIndex;
	int                             m_FixedBehaviourIndex;
	bool                            m_Visible;
	CullingMode                     m_CullingMode; ///< enum { Always Animate = 0, Based On Renderers = 1 }
	PPtr<Avatar>					m_Avatar;	
	PPtr<RuntimeAnimatorController>			m_Controller;   	
	
	mecanim::memory::MecanimAllocator			mAlloc;

	MecanimDataSet								m_EvaluationDataSet;

	AutoMecanimDataSet							m_SamplingDataSet;
	
	Vector3f									m_DeltaPosition;
	Quaternionf									m_DeltaRotation;
	
	Vector3f									m_PivotPosition;

	Vector3f									m_TargetPosition;
	Quaternionf									m_TargetRotation;
	
	float										m_MatchStartTime;
	int											m_MatchStateID;
	Vector3f									m_MatchPosition;
	Quaternionf									m_MatchRotation;	
	MatchTargetWeightMask						m_MatchTargetMask;	
	bool										m_MustCompleteMatch;

	bool                                        m_ApplyRootMotion;
	bool										m_AnimatePhysics;

	float										m_Speed;	

	bool										m_LogWarnings;
	bool										m_FireEvents;


	typedef dynamic_array<Renderer*>            ContainedRenderers;
	ContainedRenderers                          m_ContainedRenderers;

	UserListNode	m_AnimatorAvatarNode;
	UserListNode	m_AnimatorControllerNode;

	AvatarPlayback	m_AvatarPlayback;
	RecorderMode	m_RecorderMode;
	float			m_PlaybackDeltaTime;
	float			m_PlaybackTime; // for query back
	bool			IsPlayingBack();
	bool			IsAutoPlayingBack();

	bool			m_HasTransformHierarchy;	

	void SetPlaybackTimeInternal(float time);

	void UpdateInManager();

	int GetLayerClipOffset(int layerIndex);

	void TargetMatch(bool matchCurrentFrame = false);	

	bool ValidateGoalIndex(int index);
	bool ValidateTargetIndex(int index);
	bool ValidateLayerIndex(int index)const;
	bool ValidateSubLayerIndex(int index, int subLayerIndex);
	
	template<typename T>
	GetSetValueResult SetValue(mecanim::uint32_t id, T const& value);
	
	template<typename T>
	GetSetValueResult GetValue(mecanim::uint32_t id, T& value);

	void WarningStringIfLoggingActive(const char* warning) const;
	void WarningStringIfLoggingActive(const std::string& warning) const;
};

