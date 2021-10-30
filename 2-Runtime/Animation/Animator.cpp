#include "UnityPrefix.h"

#include "Animator.h"

#include "Runtime/Animation/OptimizeTransformHierarchy.h"

#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/mecanim/animation/damp.h"
#include "Runtime/mecanim/generic/stringtable.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "AnimatorManager.h"
#include "Runtime/Threads/JobScheduler.h"
#include "AnimationClip.h"
#include "AnimationSetBinding.h"
#include "MecanimAnimation.h"

#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/GameCode/RootMotionData.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/BaseClasses/EventIDs.h"

#include "Runtime/Graphics/Transform.h"

#include "Runtime/Math/Vector4.h"

#include "AnimatorController.h"
#include "GenericAnimationBindingCache.h"
#include "Avatar.h"
#include "AnimatorOverrideController.h"

#include "Runtime/mecanim/generic/typetraits.h"

#include "Runtime/Serialize/SerializeTraits.h"
#include "AnimatorGenericBindings.h"

#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/AnimationUtility.h"

#define ENABLE_DETAILED_SINGLE_THREAD_PROFILER 0

#define ENABLE_MULTITHREADED_ANIMATION ENABLE_MULTITHREADED_CODE && (!ENABLE_DETAILED_SINGLE_THREAD_PROFILER)
using namespace UnityEngine::Animation;
Animator::Animator(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode),
m_BehaviourIndex(-1),
m_FixedBehaviourIndex(-1),
m_DeltaPosition(Vector3f::zero),
m_DeltaRotation(Quaternionf::identity()),
m_PivotPosition(0,0,0),
m_MatchPosition(Vector3f::zero),
m_MatchRotation(Quaternionf::identity()),
m_MatchStartTime(-1),
m_MatchStateID(-1),
m_MustCompleteMatch(false),
m_MatchTargetMask(Vector3f::one, 0),
m_ApplyRootMotion(true),
m_AnimatePhysics(false),
m_Visible(false),
m_CullingMode (kCullAlwaysAnimate),
m_Speed(1),
m_FireEvents(true),
m_LogWarnings(true),
m_AvatarPlayback(label),
m_RecorderMode(eNormal),
m_PlaybackDeltaTime(0),
m_PlaybackTime(0),
m_HasTransformHierarchy(true),
m_AnimatorAvatarNode(this),
m_AnimatorControllerNode(this),
m_SamplingDataSet(256*1024),
mAlloc(kMemAnimation)
{

}

Animator::~Animator()
{
	Assert(m_EvaluationDataSet.m_AvatarMemory == NULL && m_EvaluationDataSet.m_ControllerConstant == NULL && m_EvaluationDataSet.m_GenericBindingConstant == NULL && m_ContainedRenderers.size() == 0);
}

void Animator::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);	
	CreateObject();
	InitializeVisibilityCulling();		

	UpdateInManager();
}

void Animator::CheckConsistency()
{	
}

void Animator::Reset ()
{
	Super::Reset();

	m_CullingMode = kCullAlwaysAnimate;
	m_ApplyRootMotion = true;
	m_AnimatePhysics = false;
	m_HasTransformHierarchy = true;
}

IMPLEMENT_OBJECT_SERIALIZE (Animator)
IMPLEMENT_CLASS_HAS_INIT (Animator)

template<class TransferFunction>
void Animator::Transfer (TransferFunction& transfer)
{	
	transfer.SetVersion(2);

	Super::Transfer (transfer);

	transfer.Transfer (m_Avatar, "m_Avatar");	
	transfer.Transfer (m_Controller, "m_Controller");

	TRANSFER_ENUM (m_CullingMode);	
	transfer.Transfer (m_ApplyRootMotion, "m_ApplyRootMotion", kDontAnimate);	
	transfer.Transfer (m_AnimatePhysics, "m_AnimatePhysics", kDontAnimate);	
	transfer.Transfer (m_HasTransformHierarchy, "m_HasTransformHierarchy", kDontAnimate);
}

void Animator::AddToManager ()
{
	GetAnimatorManager().AddAnimator(*this);
}

void Animator::RemoveFromManager ()
{
	GetAnimatorManager().RemoveAnimator(*this);
}

void Animator::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate(operation);
	ClearObject();
	ClearContainedRenderers();
}


void Animator::TransformChanged (int change)
{
	// No need to initialize it
	if (!IsInitialize())
		return;
	
	// Teleport
	Transform& avatarTransform = GetComponent(Transform);
	if (change & Transform::kPositionChanged)
		SetAvatarPosition(avatarTransform.GetPosition());

	if (change & Transform::kRotationChanged)
		SetAvatarRotation(avatarTransform.GetRotation());

	if(change & Transform::kScaleChanged)
		SetAvatarScale(avatarTransform.GetWorldScaleLossy());
}

void Animator::OnAddComponent(Component* com)
{
	Renderer* renderer = dynamic_pptr_cast<Renderer*>(com);
	if(renderer)
		InitializeVisibilityCulling ();
}

void Animator::InitializeClass ()
{
	mecanim::memory::Profiler::StaticInitialize();

	REGISTER_MESSAGE (Animator, kTransformChanged, TransformChanged, int);
	REGISTER_MESSAGE_VOID(Animator, kDidModifyAnimatorController, ClearObject);
	REGISTER_MESSAGE_VOID(Animator, kDidModifyMotion, ClearObject); // an animationClip sends this when deleted
	REGISTER_MESSAGE_VOID(Animator, kDidModifyAvatar, ClearObject);

	///@TODO: This doesn't really cover any real world cases...
	/// adding new chidlren is not covered, adding component to childeren is not covered.
	REGISTER_MESSAGE_PTR (Animator, kDidAddComponent, OnAddComponent, Component);
	
	AnimatorManager::InitializeClass();
	MecanimAnimation::InitializeClass();
	mecanim::animation::ControllerConstant::InitializeClass();	
	mecanim::animation::AvatarConstant::InitializeClass();
	mecanim::statemachine::StateConstant::InitializeClass();
	
	mecanim::crc32::crc32_table_type::init_table();
	mecanim::animation::InitializeMuscleClipTables ();
	
	Assert(mecanim::animation::FindMuscleIndex(0) == -1);
	Assert(mecanim::animation::FindMuscleIndex(mecanim::processCRC32 ("MotionT.x")) == 0);
}

void Animator::CleanupClass ()
{
	AnimatorManager::CleanupClass();
	MecanimAnimation::CleanupClass();
	mecanim::memory::Profiler::StaticDestroy();
}

PROFILER_INFORMATION (gAnimatorUpdate, "Animator.Update",	kProfilerAnimation);
PROFILER_INFORMATION (gAnimatorInitialize, "Animator.Initialize",	kProfilerAnimation);

PROFILER_INFORMATION (gProfileApplyRootMotion, "Apply Root Motion",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileFKStep, "FK & Statemachine",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileRetarget, "Retarget", kProfilerAnimation);
PROFILER_INFORMATION (gRetargetAndPrepareIK, "Retarget", kProfilerAnimation);
PROFILER_INFORMATION (gProfileAvatarIK, "IK & Final Pose Computation", kProfilerAnimation);
PROFILER_INFORMATION (gProfileAvatarWrite, "Write", kProfilerAnimation);

PROFILER_INFORMATION (gAnimatorSendTransformChanged, "Animator.SendTransformChanged",	kProfilerAnimation);
PROFILER_INFORMATION (gAnimatorSetGenericProperties, "Animator.ApplyGenericAnimatedProperties",	kProfilerAnimation);
PROFILER_INFORMATION (gAnimatorSetTransformDirty, "Animator.EditorOnlySetDirty",	kProfilerAnimation);
PROFILER_INFORMATION (gAnimatorWriteSkeletonPose, "Animator.WriteSkeletonPose",	kProfilerAnimation);

PROFILER_INFORMATION (gProfileDetailSM, "EvaluateAvatarSM",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileDetailFK, "EvaluateAvatarFK",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileDetailAvatarIK, "EvaluateAvatarIK",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileDetailAvatarEnd, "EvaluateAvatarEnd",	kProfilerAnimation);
PROFILER_INFORMATION (gProfileSample, "Animator.Sample",	kProfilerAnimation);

PROFILER_INFORMATION (gProfileSetupDataSet, "Animator.SetupDataSet",	kProfilerAnimation);

#if ENABLE_DETAILED_SINGLE_THREAD_PROFILER
#define PROFILER_AUTO_DETAIL(x,o) PROFILER_AUTO(x, o)
#else
#define PROFILER_AUTO_DETAIL(x,o)
#endif


static bool DoesLayerHaveIKPass(int layerIndex, const mecanim::animation::ControllerConstant& controller)
{
	return layerIndex < controller.m_LayerCount && controller.m_LayerArray[layerIndex]->m_IKPass;
}

// lazy search to find the right evaluation context for the given clip
static bool FindClipInController(AnimationClip &clip, AnimationClipVector const& clips, AnimationSetBindings const& animationSetBindings, int &layerIndex, int &clipLayerIndex)
{
	layerIndex = -1;
	clipLayerIndex = -1;

	
	int clipIndex = -1;
	for(int clipIter = 0; clipIndex == -1 && clipIter < clips.size(); clipIter++)
	{
		AnimationClip* currentClip = clips[clipIter];
		if (&clip == currentClip)
		{
			clipIndex = clipIter;
			break;
		}
	}

	if (clipIndex == -1)
		return false;
	
	for(int layerIter = 0; layerIndex == -1 && layerIter < animationSetBindings.animationSet->m_LayerCount; layerIter++)
	{
		for(int clipLayerIter = 0; clipLayerIndex == -1 && clipLayerIter < animationSetBindings.animationSet->m_ClipPerLayer[layerIter]; clipLayerIter++)
		{
			const mecanim::animation::AnimationSet::Clip& clip = animationSetBindings.animationSet->m_ClipConstant[layerIter][clipLayerIter];
			if (clip.m_ClipIndex == clipIndex && clip.m_Clip != NULL)
			{
				layerIndex = layerIter;
				clipLayerIndex = clipLayerIter;
				return true;
			}
		}
	}

	return false;
}

namespace 
{
	mecanim::animation::ControllerConstant*	 BuildController(AnimationClip const& clip, mecanim::memory::Allocator& allocator)
	{
		mecanim::uint32_t id = mecanim::processCRC32(clip.GetName());	
		mecanim::animation::BlendTreeConstant* blendTreeCst = mecanim::animation::CreateBlendTreeConstant(id, allocator); 
		mecanim::statemachine::StateConstant* stateCst = mecanim::statemachine::CreateStateConstant(0, 0, 1, true,false, 0, &blendTreeCst, 1, id, id, 0, true, allocator);				
		mecanim::statemachine::StateMachineConstant* stateMachinCst = CreateStateMachineConstant(&stateCst,1 , 0, 0, 0, 1, allocator); 																						
		mecanim::animation::LayerConstant*	layer	= mecanim::animation::CreateLayerConstant(0, 0, allocator);			
		layer->m_BodyMask = mecanim::human::FullBodyMask();
		layer->m_SkeletonMask = 0;

		mecanim::ValueArrayConstant* values	= mecanim::CreateValueArrayConstant(0, 0, allocator);	
		mecanim::ValueArray* defaultValues =  mecanim::CreateValueArray(values, allocator);

		return mecanim::animation::CreateControllerConstant(1, &layer,1, &stateMachinCst, values, defaultValues, allocator);
	}
}

Animator::AutoMecanimDataSet::~AutoMecanimDataSet()
{
	Reset();
}

void Animator::AutoMecanimDataSet::Reset()
{
	if(m_MecanimDataSet.m_AvatarBindingConstant != NULL)
		UnregisterAvatarBindingObjects(m_MecanimDataSet.m_AvatarBindingConstant);
	if(m_MecanimDataSet.m_GenericBindingConstant != NULL)
		UnregisterGenericBindingObjects(m_MecanimDataSet.m_GenericBindingConstant);

	m_MecanimDataSet.Reset();
	m_Alloc.Reset();
}

bool Animator::Sample(AnimationClip& clip, float inTime)
{
	PROFILER_AUTO(gProfileSample, this);

	if(!clip.IsAnimatorMotion())
		return NULL;


	m_SamplingDataSet.Reset();

	SetupAvatarMecanimDataSet(m_Avatar.IsValid () ? m_Avatar->GetAsset() : NULL, m_SamplingDataSet.m_Alloc, *m_SamplingDataSet);

	// muscle clip can be NULL when there is not curve at all in the clip.
	mecanim::animation::ClipMuscleConstant* muscleConstant = clip.GetRuntimeAsset();
	if(muscleConstant == NULL)
	{
		// in this case we want to set a human rig into relax pose to start keyframing
		if(m_SamplingDataSet->m_AvatarConstant->isHuman())
		{
			mecanim::human::HumanPose pose;

			mecanim::human::Human const* human = m_SamplingDataSet->m_AvatarConstant->m_Human.Get();
			mecanim::human::RetargetTo(human, &pose, 0, math::xformIdentity(), m_SamplingDataSet->m_AvatarOutput->m_HumanPoseOutput, m_SamplingDataSet->m_AvatarWorkspace->m_BodySkeletonPoseWs, m_SamplingDataSet->m_AvatarWorkspace->m_BodySkeletonPoseWsA);
			mecanim::animation::EvaluateAvatarEnd(m_SamplingDataSet->m_AvatarConstant, m_SamplingDataSet->m_AvatarInput, m_SamplingDataSet->m_AvatarOutput, m_SamplingDataSet->m_AvatarMemory, m_SamplingDataSet->m_AvatarWorkspace, NULL);

			SetHumanTransformPropertyValues (*m_SamplingDataSet->m_AvatarBindingConstant, *m_SamplingDataSet->m_AvatarOutput->m_SkeletonPoseOutput);
			SetTransformPropertyApplyMainThread (GetComponent (Transform), *m_SamplingDataSet->m_AvatarBindingConstant, false); // when sampling we don't skip apply root
		}
		return false;
	}
	
	
	AnimationClipVector	clips;
	clips.push_back( PPtr<AnimationClip>(&clip) );

	mecanim::animation::ControllerConstant const*       controllerConstant = ::BuildController(clip, m_SamplingDataSet.m_Alloc);
	UnityEngine::Animation::AnimationSetBindings const* animationSetBindings = UnityEngine::Animation::CreateAnimationSetBindings(controllerConstant, clips, m_SamplingDataSet.m_Alloc);
	
	SetupControllerMecanimDataSet(controllerConstant, animationSetBindings, m_SamplingDataSet.m_Alloc, *m_SamplingDataSet);

	int layerIndex = 0;
	int clipLayerIndex = 0;

	// prepare the evaluation context for clip and evaluate in float array
	mecanim::ValueArray* valuesDefault = m_SamplingDataSet->m_GenericBindingConstant->controllerBindingConstant->m_DynamicValuesDefault;
	mecanim::ValueArrayConstant* valuesDefaultConstant = m_SamplingDataSet->m_GenericBindingConstant->controllerBindingConstant->m_DynamicValuesConstant;
	mecanim::ValueArrayMask& readMask = *m_SamplingDataSet->m_AvatarWorkspace->m_ControllerWorkspace->m_ReadMask;

	mecanim::animation::AnimationSet::Clip* setClip = &animationSetBindings->animationSet->m_ClipConstant[layerIndex][clipLayerIndex];
	mecanim::animation::ClipMemory* clipMemory = m_SamplingDataSet->m_AnimationSetMemory->m_ClipMemory[layerIndex][clipLayerIndex];
	mecanim::animation::ClipOutput* clipOutput = m_SamplingDataSet->m_AnimationSetMemory->m_ClipOutput;
				
	mecanim::animation::ClipInput in;
	in.m_Time = inTime;

	mecanim::animation::EvaluateClip (muscleConstant->m_Clip.Get(), &in, clipMemory, clipOutput);

	// load values and retarget
	mecanim::SetValueMask (&readMask, false);
	ValuesFromClip (*valuesDefault, *muscleConstant, *clipOutput, setClip->m_Bindings, animationSetBindings->animationSet->m_IntegerRemapStride, *m_SamplingDataSet->m_AvatarOutput->m_DynamicValuesOutput, readMask);	
	if (m_SamplingDataSet->m_AvatarConstant->isHuman()) 
	{
		mecanim::animation::GetHumanPose (*muscleConstant,clipOutput->m_Values,*m_SamplingDataSet->m_AvatarOutput->m_HumanPoseBaseOutput);
		mecanim::human::HumanPoseCopy (*m_SamplingDataSet->m_AvatarOutput->m_HumanPoseOutput,*m_SamplingDataSet->m_AvatarOutput->m_HumanPoseBaseOutput);
		mecanim::animation::EvaluateAvatarRetarget (m_SamplingDataSet->m_AvatarConstant, m_SamplingDataSet->m_AvatarInput, m_SamplingDataSet->m_AvatarOutput, m_SamplingDataSet->m_AvatarMemory, m_SamplingDataSet->m_AvatarWorkspace, m_SamplingDataSet->m_ControllerConstant);
		mecanim::animation::EvaluateAvatarEnd (m_SamplingDataSet->m_AvatarConstant, m_SamplingDataSet->m_AvatarInput, m_SamplingDataSet->m_AvatarOutput, m_SamplingDataSet->m_AvatarMemory, m_SamplingDataSet->m_AvatarWorkspace, m_SamplingDataSet->m_ControllerConstant);
	}
		
	// Controller animated values
	ValueArrayCopy (valuesDefaultConstant, m_SamplingDataSet->m_AvatarOutput->m_DynamicValuesOutput, m_SamplingDataSet->m_ControllerConstant->m_Values.Get(), m_SamplingDataSet->m_AvatarMemory->m_ControllerMemory->m_Values.Get(), animationSetBindings->animationSet->m_AdditionalIndexArray);

	// set transform values
	if (m_SamplingDataSet->m_AvatarConstant->isHuman())
		SetHumanTransformPropertyValues (*m_SamplingDataSet->m_AvatarBindingConstant, *m_SamplingDataSet->m_AvatarOutput->m_SkeletonPoseOutput);
	SetGenericTransformPropertyValues (*m_SamplingDataSet->m_GenericBindingConstant, *m_SamplingDataSet->m_AvatarOutput->m_DynamicValuesOutput,0); // when sampling set root transform if needed

	SetTransformPropertyApplyMainThread (GetComponent (Transform), *m_SamplingDataSet->m_GenericBindingConstant, *m_SamplingDataSet->m_AvatarBindingConstant, false); // when sampling we don't skip apply root
		
	// set generic values
	SetGenericFloatPropertyValues (*m_SamplingDataSet->m_GenericBindingConstant, *m_SamplingDataSet->m_AvatarOutput->m_DynamicValuesOutput);
	SetGenericPPtrPropertyValues (*m_SamplingDataSet->m_GenericBindingConstant, *m_SamplingDataSet->m_AvatarOutput->m_DynamicValuesOutput);
	
	return true;
}

void Animator::UpdateAvatars (Animator** inputAvatars, size_t inputSize, float deltaTime, bool doFKMove, bool doRetargetIKWrite)
{
	PROFILER_AUTO(gAnimatorUpdate, NULL)

	#if ENABLE_MULTITHREADED_ANIMATION
	
	JobScheduler& scheduler = GetJobScheduler();
	JobScheduler::JobGroupID jobGroup;
	
	#define	AVATAR_LOOP(x,list,profile) \
	{ \
		PROFILER_AUTO(profile, NULL);	\
		size_t avatarJobCount = list.size(); \
		jobGroup = scheduler.BeginGroup(avatarJobCount);	\
		for (size_t i = 0; i < avatarJobCount; ++i) \
			{ Animator& avatar = *list[i]; scheduler.SubmitJob (jobGroup, x, &avatar, NULL); } \
		scheduler.WaitForGroup (jobGroup); \
	}
	#else

	#define	AVATAR_LOOP(x,list, profile) \
	for (size_t i=0;i<list.size();i++) \
		{ Animator& avatar = *list[i]; PROFILER_AUTO(profile, &avatar);  x (&avatar); } 
	
	#endif	
	
	if(doFKMove)
	{
		// invisible & visible animators
		dynamic_array<Animator*> activeAvatars (kMemTempAlloc);
		activeAvatars.reserve(inputSize);
	
		for (size_t i=0;i<inputSize;i++)
		{
			Animator& avatar = *inputAvatars[i];
		
			if (avatar.Prepare())
			{
				activeAvatars.push_back(&avatar);
			}
		}

		// Init delta time
		for (size_t i=0;i<activeAvatars.size();i++)
		{
			Animator& avatar = *activeAvatars[i];
			avatar.InitStep(deltaTime);
		}
		
		
		// Multithreaded FK Step
		AVATAR_LOOP(FKStepStatic, activeAvatars, gProfileFKStep)

		// MainThread AnimationEvents and OnAnimatorMove script call
		for (size_t i=0;i<activeAvatars.size();i++)
		{
			Animator& avatar = *activeAvatars[i];
			avatar.FireAnimationEvents();
			avatar.ApplyOnAnimatorMove();
		}
	}

	if (doRetargetIKWrite)	
	{
		int layerCount = 0;

		// only visible animators
		dynamic_array<Animator*> visibleAvatars (kMemTempAlloc);
		visibleAvatars.reserve(inputSize);

		// human visible animators
		dynamic_array<Animator*> humanAvatars (kMemTempAlloc);
		humanAvatars.reserve(inputSize);

		for (size_t i=0;i<inputSize;i++)
		{
			Animator& avatar = *inputAvatars[i];
		
			if (avatar.Prepare() && avatar.m_Visible)
			{
				layerCount = avatar.GetLayerCount() > layerCount ? avatar.GetLayerCount() : layerCount;
				visibleAvatars.push_back(&avatar);

				if(avatar.IsHuman())
				{
					humanAvatars.push_back(&avatar);

					avatar.m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoIK = true;
				}

				avatar.m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoWrite = true;
			}
		}

		
		// Multithreaded Retarget & PrepareIK
		AVATAR_LOOP(RetargetStepStatic,       humanAvatars, gProfileRetarget)

		// Default IK Pass
		AVATAR_LOOP(AvatarIKAndEndStepStatic, humanAvatars, gProfileAvatarIK)
		AVATAR_LOOP(AvatarWriteStepStatic,    humanAvatars, gProfileAvatarWrite)
	
		// Layered IK Pass
		for(int layerIter = 0; layerIter < layerCount; layerIter++)
		{
			// MainThread OnApplyAvatarIK
			for (size_t i = 0; i < humanAvatars.size(); i++)
			{
				Animator& avatar = *humanAvatars[i];

				bool ikPass = DoesLayerHaveIKPass (layerIter, *avatar.m_EvaluationDataSet.m_ControllerConstant);
				if (ikPass)
				{
					avatar.ApplyOnAnimatorIK (layerIter);
				}

				avatar.m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoIK = ikPass;
				avatar.m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoWrite = ikPass;
			}

			AVATAR_LOOP(AvatarIKAndEndStepStatic,humanAvatars, gProfileAvatarIK)
			AVATAR_LOOP(AvatarWriteStepStatic,   humanAvatars, gProfileAvatarWrite)
		}
		
		// Write to those that were not written already
		AVATAR_LOOP(AvatarWriteStepStatic, visibleAvatars, gProfileAvatarWrite)

		// MainThread Apply skeleton to transform components
		for (size_t i=0;i<visibleAvatars.size();i++)
		{
			Animator& animator = *visibleAvatars[i];
			if(animator.IsActive()) // animator can be turned Inactive by a parent animator -> 566794
			{
				{
					if (animator.m_HasTransformHierarchy)
					{
						PROFILER_AUTO(gAnimatorSendTransformChanged, &animator)
							SetTransformPropertyApplyMainThread (animator.GetComponent(Transform), *animator.m_EvaluationDataSet.m_GenericBindingConstant, *animator.m_EvaluationDataSet.m_AvatarBindingConstant, animator.IsHuman() || animator.HasRootMotion()); // if it has root motion we skip root
					}
					else
					{
						const mecanim::skeleton::SkeletonPose* pose = animator.GetGlobalSpaceSkeletonPose();
						if (pose && animator.m_EvaluationDataSet.m_AvatarConstant)
							SetFlattenedSkeletonTransformsMainThread (*animator.m_EvaluationDataSet.m_AvatarBindingConstant, *pose, *animator.m_EvaluationDataSet.m_AvatarConstant);
					}
				}
			
				{
				
					PROFILER_AUTO(gAnimatorSetGenericProperties, &animator)
					SetGenericFloatPropertyValues (*animator.m_EvaluationDataSet.m_GenericBindingConstant, *animator.m_EvaluationDataSet.m_AvatarOutput->m_DynamicValuesOutput);
					SetGenericPPtrPropertyValues (*animator.m_EvaluationDataSet.m_GenericBindingConstant, *animator.m_EvaluationDataSet.m_AvatarOutput->m_DynamicValuesOutput);
				}

				animator.Record(deltaTime);

				UInt8 deltaTimeIs0 = (deltaTime==0) ? 1 : 0;
				animator.m_EvaluationDataSet.m_AvatarMemory->m_FirstEval &= deltaTimeIs0;
			}
		}
	}
}

bool Animator::Prepare()
{
	if (!IsInitialize())
		CreateObject();

	return IsInitialize();
}

void Animator::InitStep(float deltaTime)
{
	if(IsAutoPlayingBack())
	{
		SetPlaybackTimeInternal(m_AvatarPlayback.CursorTime() + (deltaTime * GetSpeed()));
	}
	else
	{
		m_EvaluationDataSet.m_AvatarInput->m_DeltaTime = deltaTime * GetSpeed();
	}
	
	if(IsPlayingBack())
	{
		m_EvaluationDataSet.m_AvatarInput->m_DeltaTime = m_PlaybackDeltaTime;
		m_PlaybackDeltaTime = 0;
	}	
}

void Animator::EvaluateSM()
{
	if(Prepare())
	{
	mecanim::animation::EvaluateAvatarSM(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_ControllerConstant);
}
}

void Animator::FKStep()
{		
	m_DeltaPosition = Vector3f::zero;
	m_DeltaRotation = Quaternionf::identity();

	{
		PROFILER_AUTO_DETAIL (gProfileDetailSM, this)
		mecanim::animation::EvaluateAvatarSM(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_ControllerConstant);
	}

	{
		PROFILER_AUTO_DETAIL(gProfileDetailFK, this);

		mecanim::animation::EvaluateAvatarLayers(m_EvaluationDataSet.m_GenericBindingConstant->controllerBindingConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_AnimationSetMemory);
		
		if(m_EvaluationDataSet.m_AvatarConstant->isHuman())
		{
			// Prepare for OnAnimatorMove
			m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_TargetX.t *= math::float1(m_EvaluationDataSet.m_AvatarConstant->m_Human->m_Scale);
		
			TargetMatch(m_MustCompleteMatch);		
			m_MustCompleteMatch = false;
		
			m_DeltaPosition = RotateVectorByQuat(GetAvatarRotation(), float4ToVector3f(m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX.t*m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.s*math::float1(m_EvaluationDataSet.m_AvatarConstant->m_Human->m_Scale)));	
			m_DeltaRotation = float4ToQuaternionf(m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX.q);
		}
		else if(m_EvaluationDataSet.m_AvatarConstant->m_RootMotionBoneIndex != -1)
		{
			m_DeltaPosition = RotateVectorByQuat(GetAvatarRotation(), float4ToVector3f(m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX.t*m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.s));	
			m_DeltaRotation = float4ToQuaternionf(m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX.q);
		}

		if(m_EvaluationDataSet.m_AvatarConstant->isHuman() || m_EvaluationDataSet.m_AvatarConstant->m_RootMotionBoneIndex != -1)
		{
			mecanim::animation::EvaluateAvatarX(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace);
		}

		if(m_EvaluationDataSet.m_AvatarConstant->isHuman())
		{
			m_TargetPosition = float4ToVector3f(math::xformMulVec(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX, m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_TargetX.t));
			m_TargetRotation = float4ToQuaternionf(math::normalize(math::quatMul(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.q, m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_TargetX.q)));
			m_PivotPosition = float4ToVector3f(math::xformMulVec(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX, m_EvaluationDataSet.m_AvatarMemory->m_Pivot));	
		}
	}
}

void Animator::RetargetStep()
{
	mecanim::animation::EvaluateAvatarRetarget(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_ControllerConstant);
}

void Animator::AvatarIKAndEndStep()
{
	if (!m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoIK)
		return;

	{
		PROFILER_AUTO_DETAIL(gProfileDetailAvatarIK, this)
		mecanim::animation::EvaluateAvatarIK(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_ControllerConstant);					
	}

	{
		PROFILER_AUTO_DETAIL(gProfileDetailAvatarEnd, this)

		mecanim::animation::EvaluateAvatarEnd(m_EvaluationDataSet.m_AvatarConstant, m_EvaluationDataSet.m_AvatarInput, m_EvaluationDataSet.m_AvatarOutput, m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarWorkspace, m_EvaluationDataSet.m_ControllerConstant);
	}
}

void Animator::AvatarWriteStep()
{
	if (!m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoWrite)
		return;
	
	m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_DoWrite = false;

	const bool hasRootMotion = m_EvaluationDataSet.m_AvatarConstant->isHuman() || (m_EvaluationDataSet.m_AvatarConstant->m_RootMotionBoneIndex != -1) ;
	
	if (m_HasTransformHierarchy)
	{
		// Write Transforms from humanoid skeleton
		if (m_EvaluationDataSet.m_AvatarConstant->isHuman())
			/// review this function to just write what it needs
			SetHumanTransformPropertyValues (*m_EvaluationDataSet.m_AvatarBindingConstant, *m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput);

		// Write Transforms for generic binding
		SetGenericTransformPropertyValues (*m_EvaluationDataSet.m_GenericBindingConstant, *m_EvaluationDataSet.m_AvatarOutput->m_DynamicValuesOutput, hasRootMotion ? &GetComponent(Transform) : 0);
	}
	else if (m_EvaluationDataSet.m_AvatarConstant->m_AvatarSkeleton->m_Count > 0)
	{
		mecanim::animation::SkeletonPoseFromValue(	*m_EvaluationDataSet.m_AvatarConstant->m_AvatarSkeleton.Get(), 
													*m_EvaluationDataSet.m_AvatarConstant->m_AvatarSkeletonPose.Get(), 
													*m_EvaluationDataSet.m_AvatarOutput->m_DynamicValuesOutput, 
													m_EvaluationDataSet.m_GenericBindingConstant->controllerBindingConstant->m_SkeletonTQSMap, 
													*m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput,
													m_EvaluationDataSet.m_AvatarConstant->isHuman() ? m_EvaluationDataSet.m_AvatarConstant->m_HumanSkeletonReverseIndexArray.Get() : 0,
													hasRootMotion );
		
		if (!m_EvaluationDataSet.m_AvatarConstant->isHuman())
		{
			m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput->m_X[0] = m_EvaluationDataSet.m_AvatarMemory->m_AvatarX;
		}
				
		mecanim::skeleton::SkeletonPoseComputeGlobal (m_EvaluationDataSet.m_AvatarConstant->m_AvatarSkeleton.Get(), m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput, m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput);

		m_EvaluationDataSet.m_AvatarMemory->m_SkeletonPoseOutputReady = true;
	}
}

const mecanim::skeleton::SkeletonPose* Animator::GetGlobalSpaceSkeletonPose() const
{
	if (m_EvaluationDataSet.m_AvatarConstant && m_EvaluationDataSet.m_AvatarMemory->m_SkeletonPoseOutputReady)
		return m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput;
	return NULL;
}

void Animator::ApplyOnAnimatorMove()
{
	if(!IsPlayingBack())
	{
		if (SupportsOnAnimatorMove ())
		{
			SendMessage (kAnimatorMove);
		}
		else
		{
			if (m_ApplyRootMotion)
			{
				ApplyBuiltinRootMotion();
			}	
		}

		// force feedback of transform in avatarX
		// avatarX is always modified by deltaTransform in FK step 
		// if transform is not modified then avatarX won't be sync with it
		// case 498101
		// TODO: we could skip this most of the time with a simple dirty check
		Transform& avatarTransform = GetComponent(Transform);
		SetAvatarPosition(avatarTransform.GetPosition());
		SetAvatarRotation(avatarTransform.GetRotation());
	}
	else
	{
		Transform& avatarTransform = GetComponent(Transform);
		avatarTransform.SetPositionAndRotation(GetAvatarPosition(), GetAvatarRotation());
	}
}

void Animator::ApplyBuiltinRootMotion()
{
	PROFILER_AUTO(gProfileApplyRootMotion, this);
	
	// Builtin move (For example rigidbodies & Character Controllers)
	RootMotionData motionData;
	motionData.deltaPosition = GetDeltaPosition();
	motionData.targetRotation = GetAvatarRotation();
	motionData.gravityWeight = GetGravityWeight();
	motionData.didApply = false;
	SendMessage (kAnimatorMoveBuiltin, &motionData, ClassID(RootMotionData));

	// Fallback to just moving the transform
	if (!motionData.didApply)
	{
		Transform& avatarTransform = GetComponent(Transform);
		avatarTransform.SetPositionAndRotation(GetAvatarPosition(), motionData.targetRotation);
	}
}

void Animator::FireAnimationEvents()
{
	if(!m_FireEvents) return; // we dont want to fire events in previewers

	AnimationClipVector const allClips = GetAnimationClips();
	for(int i = 0; i < m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_BlendingClipCount; i++)
	{
		mecanim::animation::BlendingClip &blendingClip = m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_BlendingClipArray[i];

		AnimationClip *clip = allClips[blendingClip.m_ClipIndex];

		if (clip->HasAnimationEvents())
		{
			float prevTime = blendingClip.m_Reverse ? blendingClip.m_Time : blendingClip.m_PrevTime;
			float time = blendingClip.m_Reverse ? blendingClip.m_PrevTime : blendingClip.m_Time;
			
			clip->FireAnimationEvents (prevTime, time, *this);
		}
	}
}

bool Animator::SupportsOnAnimatorMove ()
{
	return GetGameObject().GetSupportedMessages() & kHasOnAnimatorMove;
}

void Animator::ApplyOnAnimatorIK(int layerIndex)
{
	if (GetGameObject().GetSupportedMessages() & kHasOnAnimatorIK)
		SendMessage (kAnimatorIK, layerIndex, ClassID(int));	
}

void Animator::Record(float deltaTime)
{
	if(m_RecorderMode == eRecord && GetSpeed() >= 0)
	{
		m_AvatarPlayback.RecordFrame(deltaTime*GetSpeed(), m_EvaluationDataSet.m_AvatarMemory);		
	}	
}

void* Animator::FKStepStatic (void* userData)
{
	PROFILER_AUTO(gProfileFKStep,NULL);
	static_cast<Animator*> (userData)->FKStep();
	return NULL;
}

void* Animator::RetargetStepStatic (void* userData)
{
	PROFILER_AUTO(gProfileRetarget,NULL);
	static_cast<Animator*> (userData)->RetargetStep();
	return NULL;
}

void* Animator::AvatarIKAndEndStepStatic (void* userData)
{
	PROFILER_AUTO(gProfileAvatarIK,NULL);
	static_cast<Animator*> (userData)->AvatarIKAndEndStep();
	return NULL;
}

void* Animator::AvatarWriteStepStatic (void* userData)
{
	PROFILER_AUTO(gProfileAvatarWrite,NULL);
	static_cast<Animator*> (userData)->AvatarWriteStep();
	return NULL;
}

void Animator::Update(float deltaTime)
{
	Animator* avatar = this;
	UpdateAvatars (&avatar, 1, deltaTime, true, true);
}

bool Animator::IsAvatarInitialize() const
{
	return m_EvaluationDataSet.m_AvatarMemory;
}

bool Animator::IsInitialize() const
{	
	return IsAvatarInitialize() && m_EvaluationDataSet.m_ControllerMemory;
}

void Animator::SetLayersAffectMassCenter(bool value)
{
	if(!IsAvatarInitialize())
		return;
	
	m_EvaluationDataSet.m_AvatarInput->m_LayersAffectMassCenter = value;
}

bool Animator::GetLayersAffectMassCenter() const 
{
	if(!IsAvatarInitialize())
		return false;

	return m_EvaluationDataSet.m_AvatarInput->m_LayersAffectMassCenter ;
}

bool Animator::IsInManagerList() const
{
	return m_BehaviourIndex != -1 || m_FixedBehaviourIndex != -1;
}

bool Animator::IsValid() const
{
	return m_Controller.GetInstanceID() != 0;
}


RuntimeAnimatorController* Animator::GetRuntimeAnimatorController() const
{
	return m_Controller;
}

void Animator::SetRuntimeAnimatorController(RuntimeAnimatorController* controller)
{
	if(m_Controller != PPtr<RuntimeAnimatorController>(controller))
	{		
		m_Controller = controller;		
		
		UpdateInManager();		
	
		CreateObject();
		SetDirty();		
	}	
}


AnimatorController* Animator::GetAnimatorController()const
{
	return dynamic_pptr_cast<AnimatorController*>(m_Controller);
}


AnimatorOverrideController* Animator::GetAnimatorOverrideController()const
{
	return dynamic_pptr_cast<AnimatorOverrideController*>(m_Controller);
}

void Animator::UpdateInManager()
{
	bool isValid = IsValid() && IsAddedToManager() ;
	
	bool isEffectivelyAddedToManager = IsInManagerList();

	if(!isValid && isEffectivelyAddedToManager)
	{
		RemoveFromManager();
	}
	else if(isValid && !isEffectivelyAddedToManager)
	{
		AddToManager();
	}
}




Avatar* Animator::GetAvatar()
{
	return m_Avatar;
}

void Animator::SetAvatar(Avatar* avatar)
{
	if(m_Avatar != PPtr<Avatar>(avatar))
	{				
		m_Avatar = avatar;

		UpdateInManager();		
	
		CreateObject();
		SetDirty();
	}
}

const mecanim::animation::AvatarConstant* Animator::GetAvatarConstant()
{
	if (!IsAvatarInitialize())
		CreateObject();
	return m_EvaluationDataSet.m_AvatarConstant;
}


void Animator::SetCullingMode (CullingMode mode)
{
	if (m_CullingMode == mode)
		return;
	
	m_CullingMode = mode;

	InitializeVisibilityCulling ();
}

void Animator::SetVisibleRenderers(bool visible)
{
	Assert(m_CullingMode == kCullBasedOnRenderers);

	bool becameVisible = visible && !m_Visible;
	m_Visible = visible;

	if (!IsWorldPlaying())
		return;
	
	// Perform Retarget & IK step during culling to ensure the animator
	// will be rendered correctly in the same frame.
	if (becameVisible )
	{
		float deltaTime = GetDeltaTime();

		if(!IsInManagerList() || deltaTime == 0)
			return;

		if(Prepare())
		{
			Animator* animator[1] = { this };

			bool firstEval = m_EvaluationDataSet.m_AvatarMemory->m_FirstEval;
			m_EvaluationDataSet.m_AvatarMemory->m_FirstEval = 1;
			m_EvaluationDataSet.m_AvatarMemory->m_SkeletonPoseOutputReady = 0;
			UpdateAvatars (animator, 1, deltaTime, firstEval, true);
		}
	}
}

void Animator::AnimatorVisibilityCallback (void* userData, void* renderer, int visibilityEvent)
{
	Animator& animator = *reinterpret_cast<Animator*> (userData);
	
	if (visibilityEvent == kBecameVisibleEvent)
		animator.SetVisibleRenderers(true);
	else if (visibilityEvent == kBecameInvisibleEvent)
	{
		animator.SetVisibleRenderers(animator.IsAnyRendererVisible ());
	}
	else if (visibilityEvent == kWillDestroyEvent)
	{
		animator.RemoveContainedRenderer(renderer);
		
		// Check if we are visible
		animator.SetVisibleRenderers(animator.IsAnyRendererVisible ());
	}
}

void Animator::RemoveContainedRenderer (void* renderer)
{
	for (int i=0;i<m_ContainedRenderers.size();i++)
	{
		if (m_ContainedRenderers[i] == renderer)
		{
			m_ContainedRenderers[i] = m_ContainedRenderers.back();
			m_ContainedRenderers.pop_back();
			return;
		}
	}
}

bool Animator::IsAnyRendererVisible () const
{
	Assert(m_CullingMode == kCullBasedOnRenderers);
	
	ContainedRenderers::const_iterator end = m_ContainedRenderers.end();	
	for (ContainedRenderers::const_iterator i = m_ContainedRenderers.begin();i != end;++i)
	{
		const Renderer* renderer = *i;
		Assert(renderer->HasEvent(AnimatorVisibilityCallback, this));
		
		if (renderer->IsVisibleInScene())
			return true;
	}
	
	return false;
}

void Animator::ClearContainedRenderers ()
{
	ContainedRenderers::iterator end = m_ContainedRenderers.end();
	for (ContainedRenderers::iterator i = m_ContainedRenderers.begin();i != end;++i)
	{
		Renderer* renderer = *i;
		renderer->RemoveEvent(AnimatorVisibilityCallback, this);
	}
	m_ContainedRenderers.clear();
}

void Animator::RecomputeContainedRenderersRecurse (Transform& transform)
{

	Renderer* renderer = transform.QueryComponent(Renderer);
	if (renderer)
	{
		m_ContainedRenderers.push_back(renderer);
		renderer->AddEvent(AnimatorVisibilityCallback, this);
	}
	Transform::iterator end = transform.end();
	for (Transform::iterator i = transform.begin();i != end;++i)
	{
		RecomputeContainedRenderersRecurse(**i);
	}
}

void Animator::InitializeVisibilityCulling ()
{
	if(!IsActive())
		return;
	
	ClearContainedRenderers ();
	
	if (m_CullingMode == kCullBasedOnRenderers)
	{
		Transform& transform = GetComponent (Transform);
		RecomputeContainedRenderersRecurse(transform);
		
		if (m_ContainedRenderers.empty())
			m_Visible = IsAnyRendererVisible();
	}
	else
	{
		m_Visible = true;
	}
}


void Animator::ClearObject()
{
	InvokeEvent(kAnimatorClearEvent);

	mecanim::animation::DestroyAnimationSetMemory(m_EvaluationDataSet.m_AnimationSetMemory, mAlloc);
	mecanim::animation::DestroyControllerMemory(m_EvaluationDataSet.m_ControllerMemory, mAlloc);
	DestroyAnimatorGenericBindings (m_EvaluationDataSet.m_GenericBindingConstant, mAlloc);
	DestroyAvatarBindingConstant (m_EvaluationDataSet.m_AvatarBindingConstant, mAlloc);
	if(m_EvaluationDataSet.m_AvatarWorkspace)
		mecanim::animation::DestroyControllerWorkspace(m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace, mAlloc);
	mecanim::animation::DestroyAvatarOutput(m_EvaluationDataSet.m_AvatarOutput, mAlloc);
	mecanim::animation::DestroyAvatarInput(m_EvaluationDataSet.m_AvatarInput, mAlloc);
	mecanim::animation::DestroyAvatarMemory(m_EvaluationDataSet.m_AvatarMemory, mAlloc);

	mecanim::animation::DestroyAvatarWorkspace(m_EvaluationDataSet.m_AvatarWorkspace, mAlloc);					
		
	if(m_EvaluationDataSet.m_OwnsAvatar)
	{
		mecanim::animation::DestroyAvatarConstant( const_cast<mecanim::animation::AvatarConstant*>(m_EvaluationDataSet.m_AvatarConstant), mAlloc);
		m_EvaluationDataSet.m_OwnsAvatar = false;
	}	
	
	m_EvaluationDataSet.m_AvatarConstant = 0;
	m_EvaluationDataSet.m_AvatarMemory = 0;
	// It is very important to reset the memory size to zero because we use this member to determine if the data is blobified and ready to be copy by recorder
	m_EvaluationDataSet.m_AvatarMemorySize = 0;
	m_EvaluationDataSet.m_AvatarInput = 0;
	m_EvaluationDataSet.m_AvatarOutput = 0;
	m_EvaluationDataSet.m_AvatarWorkspace = 0;
	m_EvaluationDataSet.m_ControllerConstant = 0;

	m_EvaluationDataSet.m_GenericBindingConstant = 0;
	m_EvaluationDataSet.m_AvatarBindingConstant = 0;

	m_EvaluationDataSet.m_ControllerMemory = 0;	
	m_EvaluationDataSet.m_AnimationSetMemory = 0;	
	
	m_AnimatorControllerNode.Clear();
	m_AnimatorAvatarNode.Clear();

	m_SamplingDataSet.Reset();
}

void Animator::SetupAvatarMecanimDataSet(mecanim::animation::AvatarConstant const* avatarConstant, mecanim::memory::Allocator& allocator, Animator::MecanimDataSet& outMecanimDataSet)
{
	PROFILER_AUTO(gProfileSetupDataSet, this);
	
	outMecanimDataSet.m_AvatarConstant = avatarConstant;

	if (outMecanimDataSet.m_AvatarConstant == NULL)
	{
		outMecanimDataSet.m_OwnsAvatar = true;
		outMecanimDataSet.m_AvatarConstant = mecanim::animation::CreateAvatarConstant(0,0,0,0,0,-1,math::xformIdentity(), allocator);
	}

	// It is very important to reset the memory size to zero because we use this member to determine if the data is blobified and ready to be copy by recorder
	outMecanimDataSet.m_AvatarMemorySize = 0;
	outMecanimDataSet.m_AvatarMemory = mecanim::animation::CreateAvatarMemory(outMecanimDataSet.m_AvatarConstant, allocator);
	outMecanimDataSet.m_AvatarInput = mecanim::animation::CreateAvatarInput(outMecanimDataSet.m_AvatarConstant, allocator);
	outMecanimDataSet.m_AvatarWorkspace = mecanim::animation::CreateAvatarWorkspace(outMecanimDataSet.m_AvatarConstant, allocator);
	outMecanimDataSet.m_AvatarOutput = mecanim::animation::CreateAvatarOutput(outMecanimDataSet.m_AvatarConstant, m_HasTransformHierarchy, allocator);

	Transform *effectiveRoot = GetAvatarRoot();
	if (m_HasTransformHierarchy)
		outMecanimDataSet.m_AvatarBindingConstant = CreateAvatarBindingConstant (*effectiveRoot, outMecanimDataSet.m_AvatarConstant, allocator);	
	else
		outMecanimDataSet.m_AvatarBindingConstant = CreateAvatarBindingConstantOpt (*effectiveRoot, outMecanimDataSet.m_AvatarConstant, allocator);

	// Setup AvatarX based on current transform
	Transform& transform = GetComponent(Transform);
	SetAvatarPosition(transform.GetPosition());
	SetAvatarRotation(transform.GetRotation());
	SetAvatarScale(transform.GetWorldScaleLossy());
}	

void Animator::SetupControllerMecanimDataSet(mecanim::animation::ControllerConstant const* controllerConstant, UnityEngine::Animation::AnimationSetBindings const* animationSetBindings, mecanim::memory::Allocator& allocator, Animator::MecanimDataSet& outMecanimDataSet)
{
	outMecanimDataSet.m_ControllerConstant = controllerConstant;

	Transform *effectiveRoot = GetAvatarRoot();
	if (m_HasTransformHierarchy)	
		outMecanimDataSet.m_GenericBindingConstant = CreateAnimatorGenericBindings (*animationSetBindings, *effectiveRoot, outMecanimDataSet.m_AvatarConstant, outMecanimDataSet.m_ControllerConstant, allocator);	
	else
		outMecanimDataSet.m_GenericBindingConstant = CreateAnimatorGenericBindingsOpt (*animationSetBindings, *effectiveRoot, outMecanimDataSet.m_AvatarConstant, outMecanimDataSet.m_ControllerConstant, allocator);

	const mecanim::ValueArrayConstant* dynamicValuesConstant = outMecanimDataSet.m_GenericBindingConstant->controllerBindingConstant->m_DynamicValuesConstant;
	
	outMecanimDataSet.m_ControllerMemory = mecanim::animation::CreateControllerMemory(outMecanimDataSet.m_ControllerConstant, outMecanimDataSet.m_AvatarConstant, animationSetBindings->animationSet, dynamicValuesConstant, allocator);
	outMecanimDataSet.m_AvatarMemory->m_ControllerMemory = outMecanimDataSet.m_ControllerMemory;

	outMecanimDataSet.m_AnimationSetMemory = mecanim::animation::CreateAnimationSetMemory(animationSetBindings->animationSet, outMecanimDataSet.m_GenericBindingConstant->allowConstantClipSamplingOptimization, allocator);
	
	outMecanimDataSet.m_AvatarWorkspace->m_ControllerWorkspace = mecanim::animation::CreateControllerWorkspace(outMecanimDataSet.m_ControllerConstant, outMecanimDataSet.m_AvatarConstant, animationSetBindings->animationSet, dynamicValuesConstant, allocator);
	outMecanimDataSet.m_AvatarOutput->m_DynamicValuesOutput = mecanim::CreateValueArray(dynamicValuesConstant, allocator);	
	outMecanimDataSet.m_AvatarInput->m_GotoStateInfos = allocator.ConstructArray<mecanim::statemachine::GotoStateInfo>(outMecanimDataSet.m_ControllerConstant->m_LayerCount);

	UpdateLeafNodeDuration(*outMecanimDataSet.m_ControllerConstant,*animationSetBindings->animationSet,*outMecanimDataSet.m_ControllerMemory);
}

void Animator::CreateObject()
{
	if(!IsActive())
		return;

	SET_ALLOC_OWNER(this);
	PROFILER_AUTO(gAnimatorInitialize, this);
	
	ClearObject();

	SETPROFILERLABEL(Animator);

	// ///////////////////////////////////////////////////
	// Setup is split in two part: Avatar and controller
	// both part are independant 

	// ///////////////////////////////////////////////////
	// 1. Avatar setup and binding
	mecanim::animation::AvatarConstant*	avatarConstant = NULL;
	if(m_Avatar.IsValid())
	{
		avatarConstant = m_Avatar->GetAsset();
		m_Avatar->AddObjectUser(m_AnimatorAvatarNode);
	}
	SetupAvatarMecanimDataSet(avatarConstant, mAlloc, m_EvaluationDataSet);
		
	// ///////////////////////////////////////////////////
	// 2. Controller setup and binding
	if(m_Controller.IsNull())
		return;

	m_Controller->AddObjectUser(m_AnimatorControllerNode);

	mecanim::animation::ControllerConstant*	controllerConstant = m_Controller->GetAsset();

	UnityEngine::Animation::AnimationSetBindings* animationSetBindings = m_Controller->GetAnimationSetBindings();
	if (animationSetBindings == NULL)
		return;

	SetupControllerMecanimDataSet(controllerConstant, animationSetBindings, mAlloc, m_EvaluationDataSet);
}

std::string Animator::GetPerformanceHints()
{
	if (m_EvaluationDataSet.m_GenericBindingConstant == NULL)
		return "Not initialized";
	
	std::string info;
	///@TODO: more accuaracy...
	
//	if (!m_EvaluationDataSet.m_GenericBindingConstant->allowConstantClipSamplingOptimization)
//		info += "Constant curve optimization disabled (Instance default values differ from constant curve values)\n";
//	else
//		info += Format ("Constant curve optimization enabled. %d of %d were eliminated.\n", m_EvaluationDataSet.m_GenericBindingConstant->controllerBindingConstant->m_AnimationSet->m_DynamicReducedValuesConstant->m_Count, GetAnimationSet()->m_DynamicFullValuesConstant->m_Count);
	
	//@TODO: Bound curves overview like animationclip stats...
	
	info += "Instance memory: " + FormatBytes (GetRuntimeMemorySize());
	
	return info;
}

void Animator::PrepareForPlayback()
{	
	if(m_EvaluationDataSet.m_AvatarMemorySize == 0)
	{	
		/// Blobify memory to be able to use inplace allocator during playback
		mecanim::animation::AvatarMemory *mem = m_EvaluationDataSet.m_AvatarMemory;
		m_EvaluationDataSet.m_AvatarMemory = CopyBlob(*mem,mAlloc,m_EvaluationDataSet.m_AvatarMemorySize); 
		mecanim::animation::DestroyAvatarMemory(mem, mAlloc);
	}
}

void Animator::StartRecording(int frameCount)
{
	PrepareForPlayback();

	if(m_RecorderMode == ePlayback)
	{
		WarningStringIfLoggingActive("Can't call StartRecording while in playback mode. You must call StopPlayback.");			
		return;
	}

	m_AvatarPlayback.Init(frameCount);
	m_RecorderMode = eRecord;
}
void Animator::StopRecording()
{
	m_RecorderMode = eNormal;
}

float Animator::GetRecorderStartTime() 
{
	return m_AvatarPlayback.StartTime();
}

float Animator::GetRecorderStopTime() 
{
	return m_AvatarPlayback.StopTime();
}

void Animator::StartPlayback()
{
	if(m_RecorderMode == eRecord)
	{
		WarningStringIfLoggingActive("Can't call StartPlayback while in record mode. You must call StopRecording.");					
		return;
	}
		
	m_RecorderMode = ePlayback;
}


void Animator::SetPlaybackTimeInternal(float time)
{
	float effectiveTime = 0;
	mecanim::animation::AvatarMemory*  memory = m_AvatarPlayback.PlayFrame(time, effectiveTime);
	
	if(memory != 0 )
	{
		if(effectiveTime > time )
		{
			if(IsAutoPlayingBack())
				WarningStringIfLoggingActive ("Cannot rewind Animator Recorder anymore. No more recorded data");
			else
				WarningStringIfLoggingActive("Animator Recorder does not have recorded data at given time");
		}
		else if(time > m_AvatarPlayback.StopTime())
		{
			WarningStringIfLoggingActive("Animator Recorder does not have recorded data at given time, Animator will update based on current AnimatorParameters");
		}

		PrepareForPlayback();

		m_PlaybackTime = time;
		mecanim::memory::InPlaceAllocator inPlaceAlloc(m_EvaluationDataSet.m_AvatarMemory, m_EvaluationDataSet.m_AvatarMemorySize);
		mecanim::animation::AvatarMemory* memoryCopy = CopyBlob(*memory, inPlaceAlloc, m_EvaluationDataSet.m_AvatarMemorySize);
		if(memoryCopy != 0)
			m_EvaluationDataSet.m_AvatarMemory = memoryCopy;
		else
		{
			// We don't have enough memory to fulfill the request with the current m_EvaluationDataSet.m_AvatarMemory memory block
			// Let's try another time with a memory block big enough.
			mecanim::animation::DestroyAvatarMemory(m_EvaluationDataSet.m_AvatarMemory, mAlloc	);

			UInt8* ptr = reinterpret_cast<UInt8*>(mAlloc.Allocate(m_EvaluationDataSet.m_AvatarMemorySize, ALIGN_OF(mecanim::animation::AvatarMemory)));
			mecanim::memory::InPlaceAllocator inPlaceAlloc(ptr, m_EvaluationDataSet.m_AvatarMemorySize);
			m_EvaluationDataSet.m_AvatarMemory = CopyBlob(*memory, inPlaceAlloc, m_EvaluationDataSet.m_AvatarMemorySize);		
			if(m_EvaluationDataSet.m_AvatarMemory == 0 )
			{
				WarningStringIfLoggingActive ("Can't playback from recorder, cannot allocate memory for recorded data.");					
				m_PlaybackDeltaTime = 0;
				m_PlaybackTime = 0;
				return;
			}
		}
		m_PlaybackDeltaTime = time-effectiveTime;
	}
	else
	{
		WarningStringIfLoggingActive ("Can't playback from recorder, no recorded data found");					
		m_PlaybackDeltaTime = 0;
		m_PlaybackTime = 0;
	}
}

float Animator::GetPlaybackTime()
{

	if(m_RecorderMode != ePlayback)
	{
		WarningStringIfLoggingActive("Can't call GetPlaybackTime while not in playback mode. You must call StartPlayback before.");	
		return -1;
	}	

	return m_PlaybackTime;
}

void Animator::SetPlaybackTime(float time)
{
	if(m_RecorderMode != ePlayback)
	{
		WarningStringIfLoggingActive("Can't call SetPlaybackTime while not in playback mode. You must call StartPlayback before.");					
		return ;
	}	
	
	SetPlaybackTimeInternal(time);
}

void Animator::StopPlayback()
{
	m_RecorderMode = eNormal;		
}
	
bool Animator::IsOptimizable() const
{
	return m_Avatar.IsValid();
}

bool Animator::IsHuman() const
{
	return m_Avatar.IsValid() && m_Avatar->IsHuman();
}

bool Animator::HasRootMotion() const
{
	return m_Avatar.IsValid() && m_Avatar->HasRootMotion();
}

float  Animator::GetHumanScale() const
{
	return m_Avatar.IsValid() ? m_Avatar->GetHumanScale() : 1.f;
}

void  Animator::SetApplyRootMotion (bool rootMotion)
{
	if (m_ApplyRootMotion != rootMotion)
	{
		m_ApplyRootMotion = rootMotion;
		SetDirty();
	}
}

void   Animator::SetAnimatePhysics (bool animatePhysics)
{
	if (m_AnimatePhysics != animatePhysics)
	{
		m_AnimatePhysics = animatePhysics;
		RemoveFromManager();
		AddToManager();
		SetDirty();
	}
}

void Animator::SetHasTransformHierarchy (bool value)
{
	if (m_HasTransformHierarchy != value)
	{
		m_HasTransformHierarchy = value;
	
		// I need to validate this
		CreateObject();
		InitializeVisibilityCulling();		

		SetDirty();
	}
}

GetSetValueResult Animator::SetFloatDamp(int id, float value, float dampTime, float deltaTime)
{
	if (dampTime > 0)
	{
		mecanim::dynamics::ScalDamp damper;
		GetValue(id, damper.m_Value);
		damper.m_DampTime = dampTime;		
		damper.Evaluate(value, deltaTime);

		return SetValue(id, damper.m_Value);		
	}
	else
	{
		return SetValue(id, value);
	}
}

GetSetValueResult Animator::SetFloat(int id, float value)
{
	return SetValue(id, value);
}

GetSetValueResult Animator::SetBool(int id, bool value)
{
	return SetValue(id, value);
}

GetSetValueResult Animator::SetInteger(int id, int value)
{
	return SetValue(id, (mecanim::int32_t)value);
}

GetSetValueResult Animator::GetFloat(int id, float& output)
{
	return GetValue(id, output);
}

GetSetValueResult Animator::GetBool(int id, bool& output)
{
	return GetValue(id, output);
}

GetSetValueResult Animator::GetInteger(int id, int& output)
{
	mecanim::int32_t temp;
	GetSetValueResult res = GetValue(id, temp);
	output = temp;
	return res;
}

GetSetValueResult Animator::ResetTrigger(int id)
{
	return SetValue(id, false);
}

GetSetValueResult Animator::SetTrigger(int id)
{	
	return SetValue(id, true);
}

bool Animator::HasParameter(int id)
{	
	if(!IsInitialize())
		return false;

	mecanim::int32_t i = mecanim::FindValueIndex(m_EvaluationDataSet.m_ControllerConstant->m_Values.Get(), id);
	return i != -1;
}

bool  Animator::GetMuscleValue(int id, float *value)
{
	*value = 0;
	if (this->m_Avatar.IsNull() || !this->m_Avatar->IsHuman()) 
		return false;

	// it is the desire behavior to return true if the Animator is not initialized correctly because the AnimationWindows is using this function to
	// find what is the newly added curve type.
	// Also has a side effect adding any kind of curve like MotionT trigger a ClearObject() on the animator because we are adding new curve to a clip
	// used by the animator.
	mecanim::int32_t muscleIndex = mecanim::animation::FindMuscleIndex(id);
	bool ret = muscleIndex != -1;

	if(m_SamplingDataSet->m_GenericBindingConstant == NULL)
		return ret;

	if (ret)
		*value = mecanim::animation::GetMuscleCurveValue(*m_SamplingDataSet->m_AvatarOutput->m_HumanPoseOutput, m_SamplingDataSet->m_AvatarOutput->m_MotionOutput->m_MotionX,muscleIndex);

	return ret;
}

GetSetValueResult Animator::ParameterControlledByCurve(int id)
{
	if (!IsInitialize())
		return kAnimatorNotInitialized;
	
	mecanim::int32_t index = mecanim::FindValueIndex(m_EvaluationDataSet.m_ControllerConstant->m_Values.Get(), id);
	if (index == -1)
		return kParameterDoesNotExist;
	
	if (m_EvaluationDataSet.m_GenericBindingConstant->controllerBindingConstant->m_AnimationSet->m_AdditionalIndexArray[index] != -1)
		return kParameterIsControlledByCurve;
	else
		return kGetSetSuccess;
}

Vector3f    Animator::GetAvatarPosition()
{	
	if(!IsAvatarInitialize()) 
		return Vector3f(0,0,0);
		
	return float4ToVector3f(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.t);
}

void Animator::SetAvatarPosition(const Vector3f& pos)
{	
	///@TODO: Give error messages...
	if (!IsAvatarInitialize()) 
		return ;
		
	m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.t = Vector3fTofloat4(pos);
}

Quaternionf Animator::GetAvatarRotation()
{
	if(!IsAvatarInitialize()) 
		return Quaternionf(0,0,0,1);
	
	
	return float4ToQuaternionf(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.q);
}

void Animator::SetAvatarRotation(const Quaternionf& q)
{
	if(!IsAvatarInitialize())
		return;
	
	m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.q = QuaternionfTofloat4(q);
}

Vector3f    Animator::GetAvatarScale()
{	
	if(!IsAvatarInitialize()) 
		return Vector3f(1,1,1);
		
	return float4ToVector3f(m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.s);
}

void Animator::SetAvatarScale(const Vector3f& scale)
{	
	if (!IsAvatarInitialize()) 
		return ;
		
	m_EvaluationDataSet.m_AvatarMemory->m_AvatarX.s = Vector3fTofloat4(scale,1);
}
	
Vector3f	Animator::GetDeltaPosition()
{	
	return m_DeltaPosition;	
}

Quaternionf	Animator::GetDeltaRotation()
{
	return m_DeltaRotation;
}

Vector3f	Animator::GetBodyPosition()
{	
	if(!IsAvatarInitialize())
		return Vector3f();
	
	mecanim::animation::AvatarConstant const* avatar = m_EvaluationDataSet.m_AvatarConstant;

	if(avatar->isHuman())
	{
		return float4ToVector3f(m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.t);		
	}

	return Vector3f();
}

Quaternionf	Animator::GetBodyRotation()
{
	if(!IsAvatarInitialize())
		return Quaternionf();
	
	mecanim::animation::AvatarConstant const * avatar = m_EvaluationDataSet.m_AvatarConstant;

	if(avatar->isHuman())
	{
		return float4ToQuaternionf(m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.q);		
	}

	return Quaternionf();
}

void Animator::SetBodyPosition(Vector3f const& bodyPosition)
{
	if(!IsAvatarInitialize() || !m_EvaluationDataSet.m_AvatarConstant->isHuman())
		return;
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.t = Vector3fTofloat4(bodyPosition);		
}

void Animator::SetBodyRotation(Quaternionf const& bodyRotation)
{
	if(!IsAvatarInitialize() || !m_EvaluationDataSet.m_AvatarConstant->isHuman())
		return;
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.q = QuaternionfTofloat4(bodyRotation);		
}

Vector3f Animator::GetPivotPosition()
{
	return m_PivotPosition;
}

Vector3f  Animator::GetTargetPosition()
{
	return m_TargetPosition;
}

Quaternionf  Animator::GetTargetRotation()
{
	if (m_EvaluationDataSet.m_AvatarInput->m_TargetIndex >= mecanim::animation::kTargetLeftFoot && m_EvaluationDataSet.m_AvatarInput->m_TargetIndex <= mecanim::animation::kTargetRightHand)			
	{
		return m_TargetRotation * float4ToQuaternionf(mecanim::human::HumanGetGoalOrientationOffset(mecanim::human::Goal(m_EvaluationDataSet.m_AvatarInput->m_TargetIndex - mecanim::animation::kTargetLeftFoot)));	
	}
	
	return m_TargetRotation;
}

float Animator::GetGravityWeight()
{
	if(m_EvaluationDataSet.m_AvatarOutput && m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput)
	{
		return m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_GravityWeight;
	}
	else
	{
		return 0;
	}
}

bool Animator::IsBoneTransform(Transform *transform)
{
	if(!IsAvatarInitialize())
		return false;
		
	bool ret = false;

	if (m_HasTransformHierarchy)
	{
	for(int boneIter = 0; !ret && boneIter < m_EvaluationDataSet.m_AvatarBindingConstant->skeletonBindingsCount; boneIter++)
		{
		ret = m_EvaluationDataSet.m_AvatarBindingConstant->skeletonBindings[boneIter] == transform;
		}
	}
	else
	{
		for (int exposedIndex = 0; !ret && exposedIndex < m_EvaluationDataSet.m_AvatarBindingConstant->exposedTransformCount; exposedIndex++)
		{
			ret = (m_EvaluationDataSet.m_AvatarBindingConstant->exposedTransforms[exposedIndex].transform == transform) &&
				(m_EvaluationDataSet.m_AvatarBindingConstant->exposedTransforms[exposedIndex].skeletonIndex != -1);
		}
	}

	return ret;
}

Transform* Animator::GetBoneTransform(int humanId)
{
	if(!IsAvatarInitialize())
		return 0;

	Transform *ret = 0;

	mecanim::animation::AvatarConstant* cst = GetAvatar()->GetAsset();

	if(cst && cst->isHuman())
	{
		int humanBoneId = HumanTrait::GetBoneId(*GetAvatar(),humanId);

		if( humanBoneId == -1)
			return NULL;

		if (m_HasTransformHierarchy)
		{
			ret = m_EvaluationDataSet.m_AvatarBindingConstant->skeletonBindings[cst->m_HumanSkeletonIndexArray[humanBoneId]];
		}
		else
		{
			int skeletonIndex = cst->m_HumanSkeletonIndexArray[humanBoneId];
			for (int exposedIndex = 0; exposedIndex < m_EvaluationDataSet.m_AvatarBindingConstant->exposedTransformCount; exposedIndex++)
			{
				const ExposedTransform& exposedTransform = m_EvaluationDataSet.m_AvatarBindingConstant->exposedTransforms[exposedIndex];
				if (exposedTransform.skeletonIndex == skeletonIndex)
				{
					ret = exposedTransform.transform;
					break;
				}
			}
		}
	}

	return ret;
}

void Animator::MatchTarget(Vector3f const& matchPosition, Quaternionf const& matchRotation, int targetIndex, const MatchTargetWeightMask & mask, float startNormalizedTime,  float targetNormalizedTime)
{
	// It's only possible to match a target on the first layer
	const int layerIndex = 0;

	if(!ValidateTargetIndex(targetIndex) || IsMatchingTarget() || !IsInitialize())
		return;

	if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1) && IsInTransitionInternal(layerIndex))
	{
		WarningStringIfLoggingActive("Calling Animator.MatchTarget while in transition does not have any effect");		
		return;
	}
	
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;		
	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();

	float internalTime = apStateMachineMem->m_StateMemoryArray[apStateMachineMem->m_CurrentStateIndex]->m_PreviousTime;

	float intTime = 0 ;
	float currentStateTime = math::modf(internalTime, intTime);
		
	float effectiveStartTime = 0;
	float effectiveTargetTime = targetNormalizedTime + intTime;

	if(currentStateTime <= startNormalizedTime) 
		effectiveStartTime = startNormalizedTime + intTime;
	else if( currentStateTime > startNormalizedTime && currentStateTime < targetNormalizedTime)
		effectiveStartTime = currentStateTime + intTime;
	else
	{
		// Passed target time, wait for next loop
		effectiveStartTime = startNormalizedTime + intTime + 1.0f;
		effectiveTargetTime += 1.0f;
	}

	AnimatorStateInfo animatorInfo;
	GetAnimatorStateInfo(layerIndex, true, animatorInfo);
	if (!animatorInfo.loop != 0 && targetNormalizedTime < effectiveStartTime )
		return;
			 	
	m_MatchTargetMask = mask;

	m_MatchStartTime = effectiveStartTime;
	m_MatchStateID = animatorInfo.nameHash;
	m_MatchPosition = matchPosition;
	m_MatchRotation = SqrMagnitude(matchRotation) > 0 ? matchRotation : Quaternionf::identity();
	m_EvaluationDataSet.m_AvatarInput->m_TargetIndex = targetIndex;
	m_EvaluationDataSet.m_AvatarInput->m_TargetTime = effectiveTargetTime < effectiveStartTime ? effectiveTargetTime + 1.f : effectiveTargetTime;	
}

void Animator::InterruptMatchTarget(bool completeMatch)
{
	if(completeMatch)
		m_MustCompleteMatch = true;
	else
	{
		m_MatchStartTime  = -1;
		m_MatchStateID = -1;
	}
}

bool Animator::IsMatchingTarget()const
{
	if(!IsInitialize())
		return false;

	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[0]->m_StateMachineIndex;
	mecanim::statemachine::StateMachineConstant const* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();
	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();
	mecanim::statemachine::StateConstant const* state = apStateMachineConst->m_StateConstantArray[apStateMachineMem->m_CurrentStateIndex].Get();
	
	return m_MatchStartTime >= 0 && CompareStateID (state, m_MatchStateID);
}

void Animator::SetTarget(int targetIndex, float targetNormalizedTime)
{	
	if(!ValidateTargetIndex(targetIndex) || !IsInitialize())
		return;

	if(IsMatchingTarget())
	{
		ErrorString("Calling Animator::SetTarget while already Matching Target does not have any effect");
	}
	
	m_EvaluationDataSet.m_AvatarInput->m_TargetIndex = targetIndex;
	m_EvaluationDataSet.m_AvatarInput->m_TargetTime = targetNormalizedTime;	
}

void Animator::SetSpeed(float speed)
{

	m_Speed = speed;
}

float Animator::GetSpeed() const
{
	return m_Speed;
}

bool GetLayerAndStateIndex(mecanim::animation::ControllerConstant const* controllerConstant, mecanim::uint32_t id, int* outLayer, int* outStateIndex)
{
	for (int i=0;i < controllerConstant->m_LayerCount;i++)
	{
		int index = controllerConstant->m_LayerArray[i]->m_StateMachineIndex;

		// Ignore synced layers
		if (controllerConstant->m_LayerArray[i]->m_StateMachineMotionSetIndex != 0)
			continue;

		mecanim::int32_t stateIndex = mecanim::statemachine::GetStateIndex(controllerConstant->m_StateMachineArray[index].Get(), id);
		if (stateIndex != -1)
		{
			*outStateIndex = stateIndex;
			*outLayer = i;
			return true;
		}
	}
	return false;
}

void Animator::GotoState(int layerIndex, int stateId, float normalizedTime, float transitionDuration, float transitionTime)
{	
	if(!IsInitialize() )
		return ;

	// Automatically find a good layer index
	if (layerIndex == -1)
	{
		int stateIndex;
		// StateId = 0 means active state
		if (stateId == 0)
			layerIndex = 0;
		else if (!GetLayerAndStateIndex(m_EvaluationDataSet.m_ControllerConstant, stateId, &layerIndex, &stateIndex))
		{
			ErrorString("Animator.GotoState: State could not be found");
		}	
	}

	if(!ValidateLayerIndex(layerIndex))
		return;	

	const mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
	
	if(stateMachineIndex == -1)
		return;

	if(stateMachineIndex >= m_EvaluationDataSet.m_ControllerConstant->m_StateMachineCount)
	{
		ErrorString("Animator.GotoState: Cannot find statemachine");
		return;
	}

	const mecanim::uint32_t motionSetIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineMotionSetIndex;
	if(motionSetIndex != 0)
	{
		ErrorString("Calling Animator.GotoState on Synchronize layer");
		return;
	}

	#if DEBUGMODE
	if (stateId != 0 && mecanim::statemachine::GetStateIndex(m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get(), stateId) == -1)
	{
		ErrorString("Animator.GotoState: State could not be found");
	}	
	#endif

	// When no explicit normalizedTime is specified we will simply start the clip at the start if it is not already playing.
	if (normalizedTime == -std::numeric_limits<float>::infinity())
	{
		AnimatorStateInfo info;
		GetAnimatorStateInfo(layerIndex, true, info);

		// If the state is not currently playing -> Start Playing it
		bool nameMatches = info.pathHash == stateId || info.nameHash == stateId;
		if (nameMatches)
			return;
		normalizedTime = 0.0F;
	}

	m_EvaluationDataSet.m_ControllerMemory->m_StateMachineMemory[stateMachineIndex]->m_ActiveGotoState = true;

	m_EvaluationDataSet.m_AvatarInput->m_GotoStateInfos[layerIndex].m_StateID = stateId;
	m_EvaluationDataSet.m_AvatarInput->m_GotoStateInfos[layerIndex].m_NormalizedTime = normalizedTime;
	m_EvaluationDataSet.m_AvatarInput->m_GotoStateInfos[layerIndex].m_TransitionDuration = transitionDuration;
	m_EvaluationDataSet.m_AvatarInput->m_GotoStateInfos[layerIndex].m_TransitionTime = transitionTime;
}

#define VALIDATE_IK_GOAL(x) \
if (!GetBuildSettings().hasAdvancedVersion) \
	return x; \
if(!ValidateGoalIndex(index) || !IsAvatarInitialize()) \
	return x; \

#define VALIDATE_IK_GOAL_VOID() \
if (!GetBuildSettings().hasAdvancedVersion)\
	return; \
if(!ValidateGoalIndex(index) || !IsAvatarInitialize()) \
	return; \


Vector3f Animator::GetGoalPosition(int index)
{	
	VALIDATE_IK_GOAL(Vector3f::zero)

	return float4ToVector3f(m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_X.t);
}

void Animator::SetGoalPosition(int index, Vector3f const& pos)
{
	VALIDATE_IK_GOAL_VOID()
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_X.t = Vector3fTofloat4(pos);		
}

Quaternionf Animator::GetGoalRotation(int index)
{	
	VALIDATE_IK_GOAL(Quaternionf::identity())
		
	return float4ToQuaternionf(math::normalize(math::quatMul(m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_X.q,mecanim::human::HumanGetGoalOrientationOffset(mecanim::human::Goal(index)))));
}

void Animator::SetGoalRotation(int index, Quaternionf const& rot)
{	
	VALIDATE_IK_GOAL_VOID()
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_X.q =  math::normalize(math::quatMul(QuaternionfTofloat4(rot),math::quatConj(mecanim::human::HumanGetGoalOrientationOffset(mecanim::human::Goal(index)))));
}

void Animator::SetGoalWeightPosition(int index, float value)
{
	VALIDATE_IK_GOAL_VOID()
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_WeightT = value;
}

void Animator::SetGoalWeightRotation(int index, float value)
{
	VALIDATE_IK_GOAL_VOID()

	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_WeightR = value;
}

float Animator::GetGoalWeightPosition(int index)
{	
	VALIDATE_IK_GOAL(0.0F)

	return m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_WeightT;
}

float Animator::GetGoalWeightRotation(int index)
{	
	VALIDATE_IK_GOAL(0.0F)

	return m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[index].m_WeightR;
}

#define VALIDATE_LOOKAT if (!GetBuildSettings().hasAdvancedVersion || !IsAvatarInitialize()) return;


void Animator::SetLookAtPosition(Vector3f lookAtPosition)
{
	VALIDATE_LOOKAT
	
	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_LookAtPosition = Vector3fTofloat4(lookAtPosition);
}

void Animator::SetLookAtClampWeight(float weight)
{
	VALIDATE_LOOKAT

	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_LookAtWeight.x() = weight;
}

void Animator::SetLookAtBodyWeight(float weight)
{
	VALIDATE_LOOKAT

	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_LookAtWeight.y() = weight;
}

void Animator::SetLookAtHeadWeight(float weight)
{
	VALIDATE_LOOKAT

	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_LookAtWeight.z() = weight;
}

void Animator::SetLookAtEyesWeight(float weight)
{
	VALIDATE_LOOKAT

	m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_LookAtWeight.w() = weight;
}

int	Animator::GetLayerCount()const
{	
	if(!IsInitialize())
		return 0;

	return m_EvaluationDataSet.m_ControllerConstant->m_LayerCount;
}

std::string	Animator::GetLayerName(int layerIndex)
{	
	if(!ValidateLayerIndex(layerIndex))
		return "";

	return m_Controller->StringFromID(m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_Binding);
}

float	Animator::GetLayerWeight(int layerIndex)
{	
	if(!ValidateLayerIndex(layerIndex))
		return layerIndex == 0 ? 1 : 0 ;
	
	return m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_LayerWeights[layerIndex];
}

void Animator::SetLayerWeight(int layerIndex, float w)
{
	if(!ValidateLayerIndex(layerIndex))
		return;
	
	m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_LayerWeights[layerIndex] = w;	
}

float Animator::GetPivotWeight()
{	
	if(!IsAvatarInitialize())
		return 0;

	return m_EvaluationDataSet.m_AvatarMemory->m_PivotWeight;		
}

bool Animator::IsInTransitionInternal(int layerIndex)const
{
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;

	if(stateMachineIndex == mecanim::DISABLED_SYNCED_LAYER_IN_NON_PRO) 
		return false;

	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();
	return apStateMachineMem->m_InTransition;
}

bool Animator::IsInTransition(int layerIndex)const
{
	if(!ValidateLayerIndex(layerIndex) )
		return false;
	
	return IsInTransitionInternal(layerIndex);
}

AnimationClipVector Animator::GetAnimationClips()const
{		
	return m_Controller->GetAnimationClips();
}

UnityEngine::Animation::AnimationSetBindings*	Animator::GetAnimationSetBindings()const
{	
	return m_Controller->GetAnimationSetBindings();
}

bool Animator::GetAnimationClipState (int layerIndex, bool currentState, dynamic_array<AnimationInfo>& output)
{
	if (!ValidateLayerIndex(layerIndex))
		return false;
	
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;		

	mecanim::uint32_t motionSetIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineMotionSetIndex;
	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();

	float blendFactor = currentState ? 1 - m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex]->m_BlendFactor : m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex]->m_BlendFactor;

	mecanim::statemachine::BlendNodeLayer *blendNodeLayer = currentState ? &m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex]->m_Left.m_BlendNodeLayer[motionSetIndex] : 
		apStateMachineMem->m_InTransition ? &m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_StateMachineOutput[stateMachineIndex]->m_Right.m_BlendNodeLayer[motionSetIndex]  : 0;
	
	int clipCount = blendNodeLayer ?  blendNodeLayer->m_OutputCount : 0 ;	

	AnimationClipVector const allClips = GetAnimationClips();

	int layerClipOffset = GetLayerClipOffset(layerIndex);		

	output.resize_uninitialized(clipCount);
	for(int i = 0 ; i < clipCount ; i++)
	{
		int index = blendNodeLayer->m_OutputIndexArray[i];

		AnimationInfo& animInfo = output[i];
		animInfo.clip = allClips[index+layerClipOffset];
		animInfo.weight = blendNodeLayer->m_OutputBlendArray[i] * blendFactor;			
	}				

	return true;
}


bool Animator::GetAnimatorStateInfo (int layerIndex, bool currentState, AnimatorStateInfo& output)
{
	if (!ValidateLayerIndex(layerIndex))
		return false;
	
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
	const mecanim::statemachine::StateMachineConstant* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();
	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();
		
	mecanim::uint32_t stateIndex = currentState ? apStateMachineMem->m_CurrentStateIndex : apStateMachineMem->m_InTransition ? apStateMachineMem->m_NextStateIndex : apStateMachineMem->m_StateMemoryCount;
	if (stateIndex < apStateMachineMem->m_StateMemoryCount)
	{
		output.nameHash = apStateMachineConst->m_StateConstantArray[stateIndex]->m_NameID;	
		output.pathHash = apStateMachineConst->m_StateConstantArray[stateIndex]->m_PathID;	
		output.normalizedTime = apStateMachineMem->m_StateMemoryArray[stateIndex]->m_PreviousTime;	
		output.length = apStateMachineMem->m_StateMemoryArray[stateIndex]->m_Duration;
		output.tagHash = apStateMachineConst->m_StateConstantArray[stateIndex]->m_TagID;	
		output.loop = apStateMachineConst->m_StateConstantArray[stateIndex]->m_Loop ? 1 : 0;	

		return true;

	}	
	else
		return false;
}


bool Animator::GetAnimatorTransitionInfo(int layerIndex, AnimatorTransitionInfo& info)
{
	if(!ValidateLayerIndex(layerIndex))
		return false;
			
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;	
	const mecanim::statemachine::StateMachineConstant* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();

	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();	

	if (apStateMachineMem->m_InTransition)
	{
		mecanim::statemachine::TransitionConstant const* transition  = mecanim::statemachine::GetTransitionConstant(apStateMachineConst, apStateMachineConst->m_StateConstantArray[apStateMachineMem->m_CurrentStateIndex].Get(), apStateMachineMem->m_TransitionId);
		if(transition)
		{
			info.nameHash = transition->m_ID;
			info.userNameHash = transition->m_UserID;
		}
		// Dynamic transition doesn't exist, they are created on demand by user.
		else
		{
			info.nameHash = 0;
			info.userNameHash = 0;
		}
		info.normalizedTime = apStateMachineMem->m_TransitionTime;
		return true;
	}
	else
	{
		return false;
	}
}

string Animator::GetAnimatorStateName (int layerIndex, bool currentState)
{
	if (!ValidateLayerIndex(layerIndex))
		return "";

	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
	const mecanim::statemachine::StateMachineConstant* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();
	mecanim::statemachine::StateMachineMemory const* apStateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();

	mecanim::uint32_t stateIndex = currentState ? apStateMachineMem->m_CurrentStateIndex : apStateMachineMem->m_InTransition ? apStateMachineMem->m_NextStateIndex : apStateMachineMem->m_StateMemoryCount;
	if (stateIndex < apStateMachineMem->m_StateMemoryCount)
	{
		return m_Controller->StringFromID(apStateMachineConst->m_StateConstantArray[stateIndex]->m_PathID);	
	}
	
	return "";
}

void Animator::GetRootBlendTreeConstantAndWorkspace (int layerIndex, int stateHash, mecanim::animation::BlendTreeNodeConstant const* & constant, mecanim::animation::BlendTreeWorkspace*& workspace)
{
	if(!ValidateLayerIndex(layerIndex))
		return;
	
	mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineIndex;
	mecanim::uint32_t motionSetIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[layerIndex]->m_StateMachineMotionSetIndex;
	
	mecanim::statemachine::StateMachineConstant const* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();
	mecanim::statemachine::StateMachineWorkspace* apStateMachineWorkspace = m_EvaluationDataSet.m_AvatarWorkspace->m_ControllerWorkspace->m_StateMachineWorkspace[stateMachineIndex];
	
	for (int i=0; i<apStateMachineConst->m_StateConstantCount; i++)
	{
		if (CompareStateID (apStateMachineConst->m_StateConstantArray[i].Get(), stateHash))
		{
			constant = apStateMachineConst->m_StateConstantArray[i]->m_BlendTreeConstantArray[motionSetIndex]->m_NodeArray[0].Get();
			workspace = apStateMachineWorkspace->m_StateWorkspaceArray[i]->m_BlendTreeWorkspaceArray[motionSetIndex];
		}
	}
}

float Animator::GetFeetPivotActive()
{
	if(!IsAvatarInitialize())
		return false;

	return m_EvaluationDataSet.m_AvatarInput->m_FeetPivotActive;
}
void Animator::SetFeetPivotActive(float value)
{
	if(!IsAvatarInitialize())
		return ;

	m_EvaluationDataSet.m_AvatarInput->m_FeetPivotActive = value;
}

bool Animator::GetStabilizeFeet()
{
	if(!IsAvatarInitialize())
		return false;

	return m_EvaluationDataSet.m_AvatarInput->m_StabilizeFeet;

}
void Animator::SetStabilizeFeet(bool value)
{
	if(!IsAvatarInitialize())
		return ;

	m_EvaluationDataSet.m_AvatarInput->m_StabilizeFeet = value;
}

float	Animator::GetLeftFeetBottomHeight()
{
	if(!m_Avatar.IsValid())
		return 0.f;
	
	return m_Avatar->GetLeftFeetBottomHeight();
}

float	Animator::GetRightFeetBottomHeight()
{
	if(!m_Avatar.IsValid())
		return 0.f;
	
	return m_Avatar->GetRightFeetBottomHeight();
}

#if UNITY_EDITOR

void Animator::WriteSkeletonPose(mecanim::skeleton::SkeletonPose& pose)
{
	PROFILER_AUTO(gAnimatorWriteSkeletonPose, this)
	
	if (m_EvaluationDataSet.m_GenericBindingConstant != NULL)
	{
		SetHumanTransformPropertyValues (*m_EvaluationDataSet.m_AvatarBindingConstant, pose);
		SetTransformPropertyApplyMainThread (GetComponent(Transform), *m_EvaluationDataSet.m_GenericBindingConstant, *m_EvaluationDataSet.m_AvatarBindingConstant,false); // we don't skip root for ui update pose stuff
	}
	else
	{
		ErrorString("Failed to write skeleton pose");
	}
}

void  Animator::WriteHumanPose(mecanim::human::HumanPose &pose)
{
	// this function is only called from the editor.
	// always fetch the avatar constant from the Avatar asset in case it been updated.
	// Looking if the pptr is valid
	if(!m_Avatar.IsValid())
		return;

	// Looking if the mecanim avatar constant is valid
	if(!m_Avatar->IsValid())
		return;

	if(!Prepare())
		return;

	mecanim::animation::AvatarConstant *avatar = m_Avatar->GetAsset();

	if(avatar && avatar->isHuman())
	{
		mecanim::human::Human *human = avatar->m_Human.Get();
		mecanim::human::RetargetTo(human,&pose,0,math::xformIdentity(),m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput,m_EvaluationDataSet.m_AvatarWorkspace->m_BodySkeletonPoseWs,m_EvaluationDataSet.m_AvatarWorkspace->m_BodySkeletonPoseWsA);
		mecanim::animation::EvaluateAvatarEnd(avatar,m_EvaluationDataSet.m_AvatarInput,m_EvaluationDataSet.m_AvatarOutput,m_EvaluationDataSet.m_AvatarMemory,m_EvaluationDataSet.m_AvatarWorkspace,m_EvaluationDataSet.m_ControllerConstant);
			
		WriteSkeletonPose(*m_EvaluationDataSet.m_AvatarOutput->m_SkeletonPoseOutput);			
	}
	else
	{
		ErrorString("Failed to write human pose");
	}
}

void Animator::WriteDefaultPose()
{
	// Looking if the pptr is valid
	if(!m_Avatar.IsValid())
		return;

	// Looking if the mecanim avatar constant is valid
	if(!m_Avatar->IsValid())
		return;

	if(!Prepare())
		return;

	mecanim::animation::AvatarConstant *avatar = m_Avatar->GetAsset();

	WriteSkeletonPose(*avatar->m_AvatarSkeletonPose);	
}
#endif

bool Animator::IsAutoPlayingBack()
{
	return m_RecorderMode == eRecord && GetSpeed() < 0;
}

bool Animator::IsPlayingBack()
{
	return m_RecorderMode == ePlayback || IsAutoPlayingBack();
} 

int Animator::GetLayerClipOffset(int layerIndex)
{
	int layerClipOffset = 0;
	for(int i = 0; i < layerIndex ; i++)
	{
		mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[i]->m_StateMachineIndex;
		mecanim::uint32_t motionSetIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[i]->m_StateMachineMotionSetIndex;

		if(stateMachineIndex != mecanim::DISABLED_SYNCED_LAYER_IN_NON_PRO)
		{
			const mecanim::statemachine::StateMachineConstant* apStateMachineConst = m_EvaluationDataSet.m_ControllerConstant->m_StateMachineArray[stateMachineIndex].Get();
			int j;	
			for( j = 0 ; j < apStateMachineConst->m_StateConstantCount; j++)
			{
				layerClipOffset += apStateMachineConst->m_StateConstantArray[j]->m_LeafInfoArray[motionSetIndex].m_Count ;
			}
		}
	}

	return layerClipOffset;
}

bool Animator::ShouldInterruptMatchTarget()const
{
	if(IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))
		return false;

	// case 516805: Match target must be interrupted if we begin a transition.
	if(IsInTransitionInternal(0))
		return true;

	// case 542102: when the frame rate drop we miss the end of the match target and the last match target parameter stick.
	// detect such case in TargetMatch by watching the initial state id
	if(!IsMatchingTarget())
		return true;

	return false;
}

void Animator::TargetMatch(bool matchCurrentFrame)
{		
	if(ShouldInterruptMatchTarget())
		InterruptMatchTarget(false);

	if(IsInitialize() && IsMatchingTarget())
	{		
		math::xform avatarX = m_EvaluationDataSet.m_AvatarMemory->m_AvatarX;		

		math::float1 scale(m_EvaluationDataSet.m_AvatarConstant->m_Human->m_Scale);												
		math::xform dx = m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX;
		dx.t *= scale;
		avatarX = math::xformMul(avatarX, dx);

		float stateTime = 0 ;
		float stateDuration = 1 ;
		if( m_EvaluationDataSet.m_ControllerConstant->m_LayerCount > 0)
		{
			mecanim::uint32_t stateMachineIndex = m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[0]->m_StateMachineIndex;					
			mecanim::statemachine::StateMachineMemory const* stateMachineMem = m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_StateMachineMemory[stateMachineIndex].Get();
			if(stateMachineMem->m_StateMemoryCount > 0 )
			{
				stateTime = stateMachineMem->m_StateMemoryArray[stateMachineMem->m_CurrentStateIndex]->m_PreviousTime; 
				stateDuration = stateMachineMem->m_StateMemoryArray[stateMachineMem->m_CurrentStateIndex]->m_Duration; 
			}
		}		

		if(stateTime >= m_MatchStartTime)
		{						
			float endTime = m_EvaluationDataSet.m_AvatarInput->m_TargetTime;						
																
			float normalizeDeltaTime = m_EvaluationDataSet.m_AvatarInput->m_DeltaTime / stateDuration;
			float remainingTime = max(0.0f, endTime - stateTime );
			float w = remainingTime != 0 ? normalizeDeltaTime / remainingTime : 1;				
			w = math::saturate(w);
			
			math::float4 targetT = m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_TargetX.t;
			math::float4 targetQ = m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_TargetX.q;

			mecanim::uint32_t targetIndex = m_EvaluationDataSet.m_AvatarInput->m_TargetIndex;			

			if(matchCurrentFrame)
			{					
				w = 1;							
				if(targetIndex  == mecanim::animation::kTargetReference) 
				{
					targetT = math::float4::zero();
					targetQ = math::quatIdentity();
				}
				else if(targetIndex  == mecanim::animation::kTargetRoot) 
				{
					targetT = m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.t;	
					targetQ = m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_RootX.q;	
				}
				else if (targetIndex >= mecanim::animation::kTargetLeftFoot && targetIndex <= mecanim::animation::kTargetRightHand)
				{
					targetT = m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[targetIndex-2].m_X.t;	
					targetQ = m_EvaluationDataSet.m_AvatarOutput->m_HumanPoseOutput->m_GoalArray[targetIndex-2].m_X.q;
				}			
			}


			if (targetIndex >= mecanim::animation::kTargetLeftFoot && targetIndex <= mecanim::animation::kTargetRightHand)			
				targetQ = math::normalize(math::quatMul(targetQ,mecanim::human::HumanGetGoalOrientationOffset(mecanim::human::Goal(targetIndex - mecanim::animation::kTargetLeftFoot))));	
								
			
			// from muscleclips, local to avatarX
			math::xform targetXLocal(targetT,targetQ, math::scaleIdentity());

			// from user, where we need to be, in world space
			math::xform matchX(Vector3fTofloat4(m_MatchPosition), QuaternionfTofloat4(m_MatchRotation), math::scaleIdentity());			

			// make match local to avatar X
			math::xform matchXLocal = math::xformInvMul(avatarX, matchX); // @Sonny: any problem here for match target bug on IOS

			// m_EvaluationDataSet.m_AvatarOutput->m_DX is in normalized space, so dx musy be computed in normalized space too.
			math::xform dx;
			dx.t = matchXLocal.t - targetXLocal.t;  // dt not influenced by rotation
			dx.t *= rcp(scale);						
			dx.q = quatMul(matchXLocal.q, quatConj(targetXLocal.q));			 			
			dx.t *= Vector3fTofloat4(w*m_MatchTargetMask.m_PositionXYZWeight);					
			dx.q = math::quatLerp( math::quatIdentity(), dx.q, math::float1(w*m_MatchTargetMask.m_RotationWeight));

			m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX = math::xformMul(m_EvaluationDataSet.m_AvatarOutput->m_MotionOutput->m_DX,dx);
							
			if(w >= 1)
			{
				InterruptMatchTarget(false);
			}			
		}			
	}
}

bool Animator::ValidateGoalIndex(int index)
{
	if(index < mecanim::human::kLeftFootGoal || index > mecanim::human::kRightHandGoal )
	{
		ErrorString("Invalid Goal Index");
		return false;
	}

	return true;
}

bool Animator::ValidateTargetIndex(int index)
{
	if(index < mecanim::animation::kTargetReference || index > mecanim::animation::kTargetRightHand )
	{
		ErrorString("Invalid Target Index");
		return false;
	}

	return true;
}

bool Animator::ValidateLayerIndex(int index)const
{
	if(!IsInitialize() )
		return false;

	if(index < 0 || index >= GetLayerCount())
	{
		ErrorString("Invalid Layer Index");
		return false;
	}

	if(m_EvaluationDataSet.m_ControllerConstant->m_LayerArray[index]->m_StateMachineIndex == mecanim::DISABLED_SYNCED_LAYER_IN_NON_PRO)
	{
		ErrorString("Sync Layer is only supported in Unity Pro");
		return false;
	}

	return true;
}

void Animator::ValidateParameterID (GetSetValueResult result, int identifier)
{
	ValidateParameterString (result, Format("Hash %d", identifier));
}

void Animator::ValidateParameterString (GetSetValueResult result, const std::string& name)
{
	
	if (result == kParameterMismatchFailure)
	{
		WarningStringIfLoggingActive(Format ("Parameter type '%s' does not match.", name.c_str()));
	}
	else if (result == kParameterDoesNotExist)
	{
		WarningStringIfLoggingActive(Format ("Parameter '%s' does not exist.", name.c_str()));
	}
	else if (result == kAnimatorNotInitialized)
	{
		WarningStringIfLoggingActive("Animator has not been initialized.");
	}
	else if (result == kParameterIsControlledByCurve)
	{
		WarningStringIfLoggingActive(Format("Parameter '%s' is controlled by a curve.", name.c_str()));
	}
}

template<typename T> inline bool IsTypeMatching(mecanim::ValueConstant const& valueConstant, T const& value)
{
	return	valueConstant.m_Type == mecanim::traits<T>::type() || 
			(valueConstant.m_Type == mecanim::kTriggerType && mecanim::traits<T>::type() == mecanim::kBoolType);
}

template<typename T> inline
GetSetValueResult Animator::SetValue(mecanim::uint32_t id, T const& value)
{
	if(IsPlayingBack())
		return kAnimatorInPlaybackMode;

	if (!IsInitialize())
		return kAnimatorNotInitialized;
	
	mecanim::int32_t i = mecanim::FindValueIndex(m_EvaluationDataSet.m_ControllerConstant->m_Values.Get(), id);
	if( i == -1)
		return kParameterDoesNotExist;
		
	bool isCurve = m_EvaluationDataSet.m_GenericBindingConstant->controllerBindingConstant->m_AnimationSet->m_AdditionalIndexArray[i] != -1;
	if (isCurve)
		return kParameterIsControlledByCurve;
	
	if (!IsTypeMatching(m_EvaluationDataSet.m_ControllerConstant->m_Values->m_ValueArray[i], value))
		return kParameterMismatchFailure;
	
	m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_Values->WriteData(value, m_EvaluationDataSet.m_ControllerConstant->m_Values->m_ValueArray[i].m_Index);
	return kGetSetSuccess;
}

template<typename T> inline
GetSetValueResult Animator::GetValue(mecanim::uint32_t id, T& value)
{
	if (!IsInitialize())
	{
		value = T();
		return kAnimatorNotInitialized;
	}
	
	mecanim::int32_t i = mecanim::FindValueIndex(m_EvaluationDataSet.m_ControllerConstant->m_Values.Get(), id);
	if ( i == -1)
	{
		value = T();
		return kParameterDoesNotExist;
	}
		
	if (! IsTypeMatching(m_EvaluationDataSet.m_ControllerConstant->m_Values->m_ValueArray[i], value) )
	{
		value = T();
		return kParameterMismatchFailure;
	}
	
	
	m_EvaluationDataSet.m_AvatarMemory->m_ControllerMemory->m_Values->ReadData(value, m_EvaluationDataSet.m_ControllerConstant->m_Values->m_ValueArray[i].m_Index);

	return kGetSetSuccess;
}


void Animator::WarningStringIfLoggingActive(const std::string& warning) const
{
	WarningStringIfLoggingActive(warning.c_str());	
}

/// add or modify curve for a read only imported clip ... put Animation Window modification to curve in .meta

void Animator::WarningStringIfLoggingActive(const char* warning) const
{
	if(m_LogWarnings)
	{
		WarningStringObject(warning ,this);
	}
}

Transform* Animator::GetAvatarRoot()
{	
	Transform *root = &GetComponent(Transform); 	
	if(IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_3_a1))
	{
		if(m_Avatar.IsValid())
		{
			Transform* effectiveRoot =  0;		
			if(m_Avatar->GetAsset()&& m_Avatar->GetAsset()->m_AvatarSkeleton.Get())
			{
				effectiveRoot = FindAvatarRoot(m_Avatar->GetAsset()->m_AvatarSkeleton.Get(), m_Avatar->GetAsset()->m_SkeletonNameIDArray.Get(), *root, m_HasTransformHierarchy) ;
			}									
			if(effectiveRoot) root = effectiveRoot;
		}
	}

	return root;
}


///@TODO:

/// * How does a user setup the animation range in the animation window?
/// * Figure out how we want to move loop blend feel intuitive.

/// * LivePreview pretends that non-looping animations loop. At least that looks like what it is visualizing...
/// * Blending of dynamic values does not treat default values correctly
/// muscle clip info for animation window created clips
///	visible avatar for some objects like lights
/// * Figure out how we will handle startTime / stopTime from AnimationWindow.

///@TODO: Write Test
/// Create cube with light attached
/// * Create Animation clip with transform changes -> Automatically applies root motion
/// * Animated child object is animated as normal position movement
/// * Blending between one clip that has a property and one that doesn't
