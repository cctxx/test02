#include "UnityPrefix.h"

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"

#include "Runtime/Profiler/Profiler.h"

#include "AvatarBuilder.h"
#include "Avatar.h"



namespace
{	
	int Find( AvatarBuilder::NamedTransforms const& transforms, Transform* parent )
	{
		for(int i=0;i<transforms.size();i++)
			if(transforms[i].transform == parent) return i;
		return -1;
	}

	template<typename TYPE>
	int GetIndexArray(HumanDescription const& humanDescription, AvatarBuilder::NamedTransforms const& namedTransform, std::vector<int> &indexArray)
	{
		int ret = 0;

		for(int i = 0 ; i < TYPE::GetBoneCount(); ++i)
		{	
			HumanBoneList::const_iterator it = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(TYPE::GetBoneName(i)) );
		
			if(it != humanDescription.m_Human.end())
			{
				for(int j = 0; j < namedTransform.size(); j++)
				{
					if((*it).m_BoneName == namedTransform[j].name)
					{
						indexArray[j] = i;
						ret++;
					}
				}
			}		
		}

		return ret;
	}


	mecanim::skeleton::Skeleton* BuildSkeleton(const AvatarBuilder::NamedTransforms & transform, TOSVector& tos, mecanim::memory::Allocator& alloc)
	{
		mecanim::skeleton::Skeleton * skel = mecanim::skeleton::CreateSkeleton(transform.size(), 0, alloc);	

		mecanim::uint32_t i;
		for(i=0;i<skel->m_Count;i++)
		{	
			// Find return -1 if transform is not found exactly like our data representation
			// when node doesn't have any parent
			skel->m_Node[i].m_ParentId = Find( transform, transform[i].transform->GetParent() );
			skel->m_Node[i].m_AxesId = -1;
			skel->m_ID[i] = ProccessString(tos, transform[i].path);
		}
		return skel;
	}

	void MarkBoneUp(mecanim::skeleton::Skeleton *avatarSkeleton, std::vector<bool> &isMark, int index, int stopIndex)
	{
		isMark[index] = true;

		if(index != stopIndex)
		{
			MarkBoneUp(avatarSkeleton,isMark,avatarSkeleton->m_Node[index].m_ParentId,stopIndex);
		}
	}

	mecanim::skeleton::Skeleton* BuildHumanSkeleton(mecanim::skeleton::Skeleton *avatarSkeleton, std::vector<int> humanBoneIndexArray, std::vector<int> leftHandIndexArray, std::vector<int> rightHandIndexArray, mecanim::memory::Allocator& alloc)
	{
		int humanCount = 0;
		int leftHandCount = 0;
		int rightHandCount = 0;
		int hipsIndex = -1;

		for(int i = 0; i < avatarSkeleton->m_Count; i++)
		{
			humanCount += humanBoneIndexArray[i] != -1 ? 1 : 0;
			leftHandCount += leftHandIndexArray[i] != -1 ? 1 : 0;
			rightHandCount += rightHandIndexArray[i] != -1 ? 1 : 0;
			hipsIndex = humanBoneIndexArray[i] == mecanim::human::kHips ? i : hipsIndex; 
		}

		std::vector<bool> isHumanBone(avatarSkeleton->m_Count,false);

		for(int i = 0; i < avatarSkeleton->m_Count; i++)
		{
			if(humanBoneIndexArray[i] != -1)
			{
				MarkBoneUp(avatarSkeleton,isHumanBone,i,hipsIndex);
			}

			if(leftHandIndexArray[i] != -1)
			{
				MarkBoneUp(avatarSkeleton,isHumanBone,i,hipsIndex);
			}

			if(rightHandIndexArray[i] != -1)
			{
				MarkBoneUp(avatarSkeleton,isHumanBone,i,hipsIndex);
			}
		}

		int boneCount = 0;

		for(int i = 0; i < isHumanBone.size(); i++)
		{
			boneCount += isHumanBone[i] ? 1 : 0;
		}

		mecanim::skeleton::Skeleton *skel = mecanim::skeleton::CreateSkeleton(1+boneCount,humanCount+leftHandCount+rightHandCount,alloc);	

		int nodeIndex = 0;
		int axesIndex = 0;

		skel->m_ID[nodeIndex] = avatarSkeleton->m_ID[avatarSkeleton->m_Node[hipsIndex].m_ParentId];
		skel->m_Node[nodeIndex].m_ParentId = -1;
		skel->m_Node[nodeIndex].m_AxesId = -1;

		nodeIndex++;

		for(int i = 0; i < avatarSkeleton->m_Count; i++)
		{
			if(isHumanBone[i])
			{
				skel->m_ID[nodeIndex] = avatarSkeleton->m_ID[i];
				skel->m_Node[nodeIndex].m_ParentId = mecanim::skeleton::SkeletonFindNode(skel,avatarSkeleton->m_ID[avatarSkeleton->m_Node[i].m_ParentId]); 
				skel->m_Node[nodeIndex].m_AxesId = humanBoneIndexArray[i] != -1 || leftHandIndexArray[i] != -1 || rightHandIndexArray[i] != -1 ? axesIndex++ : -1;

				nodeIndex++;
			}
		}

		return skel;
	}

	mecanim::skeleton::Skeleton* BuildRootMotionSkeleton(mecanim::skeleton::Skeleton *avatarSkeleton, int rootMotionIndex, mecanim::memory::Allocator& alloc)
	{

		std::vector<bool> isRootMotionBone(avatarSkeleton->m_Count,false);

		MarkBoneUp(avatarSkeleton,isRootMotionBone,rootMotionIndex,0);

		int rootMotionCount = 0;

		for(int i = 0; i < isRootMotionBone.size(); i++)
		{
			rootMotionCount += isRootMotionBone[i] ? 1 : 0;
		}

		mecanim::skeleton::Skeleton *skel = mecanim::skeleton::CreateSkeleton(rootMotionCount,0,alloc);	

		int nodeIndex = 0;

		for(int i = 0; i < avatarSkeleton->m_Count; i++)
		{
			if(isRootMotionBone[i])
			{
				skel->m_ID[nodeIndex] = avatarSkeleton->m_ID[i];
				skel->m_Node[nodeIndex].m_ParentId = nodeIndex-1;

				nodeIndex++;
			}
		}

		return skel;
	}

	void SetAxes(mecanim::human::Human* human, SkeletonBoneLimit const& skeletonBoneLimit, int id)
	{
		if(id != -1 && skeletonBoneLimit.m_Modified)
		{
			int axesId = human->m_Skeleton->m_Node[id].m_AxesId;
			if(axesId != -1)
			{
				math::Axes& axes = human->m_Skeleton->m_AxesArray[axesId];

				math::float4 minValue = math::radians(math::float4(skeletonBoneLimit.m_Min[0], skeletonBoneLimit.m_Min[1], skeletonBoneLimit.m_Min[2], 0.f));
				math::float4 maxValue = math::radians(math::float4(skeletonBoneLimit.m_Max[0], skeletonBoneLimit.m_Max[1], skeletonBoneLimit.m_Max[2], 0.f));
					axes.m_Limit.m_Min = minValue;
					axes.m_Limit.m_Max = maxValue;
				}
			}
		}

	class SetupAxesHelper
	{
	public:
		SetupAxesHelper(mecanim::human::Human* human, bool exist, mecanim::int32_t* index): mHuman(human),mExist(exist),mIndex(index){}

		void operator()(HumanBone const& humanBone, int i)
		{
			int index = mExist ? mIndex[i] : -1;
			SetAxes(mHuman, humanBone.m_Limit, index);
		}
	protected:
		mecanim::human::Human* mHuman;
		mecanim::skeleton::SkeletonPose* mPose;
		bool mExist;
		mecanim::int32_t* mIndex;
	};

	template<class Trait, class List, class Function> void for_each(List const& list, Function f)
	{
		for (int i = 0; i < Trait::GetBoneCount(); ++i ) 
		{
			typename List::const_iterator it = std::find_if(list.begin(), list.end(), FindHumanBone(Trait::GetBoneName(i)) );
			if(it != list.end())
			{
				f(*it, i);
			}
		}
	}

	void SetupAxes(mecanim::human::Human* human, HumanDescription const& humanDescription)
	{
		for_each<HumanTrait::Body>(humanDescription.m_Human, SetupAxesHelper(human, true, human->m_HumanBoneIndex));

		for_each<HumanTrait::LeftFinger>(humanDescription.m_Human, SetupAxesHelper(human, human->m_HasLeftHand, human->m_HasLeftHand ? human->m_LeftHand->m_HandBoneIndex : 0));

		for_each<HumanTrait::RightFinger>(humanDescription.m_Human, SetupAxesHelper(human, human->m_HasRightHand, human->m_HasRightHand ? human->m_RightHand->m_HandBoneIndex : 0));
	}

	struct IndexFromBoneName
	{
		UnityStr m_Predicate;
		IndexFromBoneName(const UnityStr& predicate):m_Predicate(predicate){}

		bool operator()(AvatarBuilder::NamedTransform const& namedTransform){return namedTransform.name == m_Predicate;}
	};

	static int GetIndexFromBoneName (AvatarBuilder::NamedTransforms::const_iterator start, AvatarBuilder::NamedTransforms::const_iterator stop, const UnityStr& boneName)
	{
		AvatarBuilder::NamedTransforms::const_iterator it = std::find_if(start, stop, IndexFromBoneName(boneName) );
		if(it!=stop)
			return it - start;

		return -1;
	}
	
	static void OverwriteTransforms(mecanim::skeleton::Skeleton* skeleton, mecanim::skeleton::SkeletonPose* pose, const HumanDescription& humanDescription, const AvatarBuilder::NamedTransforms& namedTransforms, bool force)
	{
		if( humanDescription.m_Skeleton.size() > 0)
		{
			const SkeletonBone& skeletonBone = humanDescription.m_Skeleton[0];
 			if(skeletonBone.m_TransformModified || force)
 			{
				if(namedTransforms[0].name == skeletonBone.m_Name)
 				{
 					pose->m_X[0] = xformFromUnity(skeletonBone.m_Position,skeletonBone.m_Rotation,skeletonBone.m_Scale);				
 				}
			}
			
			for (int i = 1; i < humanDescription.m_Skeleton.size(); ++i ) 
			{	
				SkeletonBone const& skeletonBone = humanDescription.m_Skeleton[i];
				if(skeletonBone.m_TransformModified || force)
				{
					int skeletonIndex = GetIndexFromBoneName (namedTransforms.begin()+1, namedTransforms.end(), skeletonBone.m_Name) + 1;
					if (skeletonIndex != 0)
					{
						pose->m_X[skeletonIndex] = xformFromUnity(skeletonBone.m_Position,skeletonBone.m_Rotation,skeletonBone.m_Scale);
					}
				}
			}
		}
	}
}

HumanBone::HumanBone():
m_HumanName(""),
m_BoneName("")
//m_ColliderPosition(Vector3f::zero),
//m_ColliderRotation(Quaternionf::identity()),
//m_ColliderScale(Vector3f::one)
{
}

HumanBone::HumanBone(std::string const& humanName):
m_HumanName(humanName),
m_BoneName("")
//m_ColliderPosition(Vector3f::zero),
//m_ColliderRotation(Quaternionf::identity()),
//m_ColliderScale(Vector3f::one)

{
}

class FindBone
{
protected:
	UnityStr  mName;
public:
	FindBone(const UnityStr& name):mName(name){}
	bool operator() (const AvatarBuilder::NamedTransform & bone){ return mName == bone.name; }
};

class FindBonePath
{
protected:
	UnityStr  mName;
public:
	FindBonePath(const UnityStr& name):mName(name){}
	
	bool operator() (const AvatarBuilder::NamedTransform & bone){ return mName == bone.path; }
};

PROFILER_INFORMATION (gAvatarBuilderBuildAvatar, "AvatarBuilder.BuildAvatar", kProfilerAnimation);

std::string AvatarBuilder::BuildAvatar(Avatar& avatar, const Unity::GameObject& go, bool doOptimizeTransformHierarchy, const HumanDescription& humanDescription, Options options)
{
	PROFILER_AUTO(gAvatarBuilderBuildAvatar, NULL)

	NamedTransforms namedTransform;

	std::string error = AvatarBuilder::GenerateAvatarMap(go, namedTransform, humanDescription, doOptimizeTransformHierarchy, options.avatarType, options.useMask);
	if(!error.empty())
		return Format("AvatarBuilder '%s': %s", go.GetName(), error.c_str());

	mecanim::memory::ChainedAllocator alloc(30*1024);	

	TOSVector tos;

	// build avatar skeleton
	mecanim::skeleton::Skeleton*		avatarSK = BuildSkeleton(namedTransform, tos, alloc);
	mecanim::skeleton::SkeletonPose*	avatarPose  = mecanim::skeleton::CreateSkeletonPose(avatarSK, alloc);
	mecanim::skeleton::SkeletonPose*	avatarGPose = mecanim::skeleton::CreateSkeletonPose(avatarSK, alloc);
	mecanim::uint32_t *					nameIDArray = alloc.ConstructArray<mecanim::uint32_t>(namedTransform.size());

	for(int i = 0; i < namedTransform.size(); i++)
	{	
		nameIDArray[i] = mecanim::processCRC32(GetLastPathNameComponent(namedTransform[i].path.c_str(), namedTransform[i].path.size()));
	}

	if(options.readTransform)
		ReadFromLocalTransformToSkeletonPose(avatarPose, namedTransform);
	
	// Overwrite transform that has been set by the user
	OverwriteTransforms(avatarSK, avatarPose, humanDescription, namedTransform, true);	

	mecanim::skeleton::SkeletonPoseComputeGlobal(avatarSK, avatarPose, avatarGPose);

	// Fill avatarDefaultPose. This pose will be used in optimized mode, when the character is not animated.
	// Please note that, although this value is only used in optimized mode, we initialize it here all the time.
	// Because, optimization/de-optimization should can be applied on the fly.
	// We don't want to trigger re-import (rebuild avatar).
	mecanim::skeleton::SkeletonPose*	avatarDefaultPose = mecanim::skeleton::CreateSkeletonPose(avatarSK, alloc);
	ReadFromLocalTransformToSkeletonPose(avatarDefaultPose, namedTransform);

	mecanim::human::Human*				human = 0;
	mecanim::skeleton::Skeleton*		humanSK    = 0;
	mecanim::skeleton::SkeletonPose*	humanPose  = 0;
	mecanim::skeleton::SkeletonPose*	humanGPose = 0;

	int rootMotionIndex	= -1;
	math::xform	rootMotionX;
	mecanim::skeleton::Skeleton *rootMotionSK = 0;

	if(options.avatarType == kHumanoid)
	{
		// build human skeleton
		std::vector<int> humanIndexArray(avatarSK->m_Count,-1);
		std::vector<int> leftHandIndexArray(avatarSK->m_Count,-1);
		std::vector<int> rightHandIndexArray(avatarSK->m_Count,-1);

		GetIndexArray<HumanTrait::Body>(humanDescription,namedTransform,humanIndexArray);
		bool leftHandValid = GetIndexArray<HumanTrait::LeftFinger>(humanDescription,namedTransform,leftHandIndexArray) > 0;
		bool rightHandValid = GetIndexArray<HumanTrait::RightFinger>(humanDescription,namedTransform,rightHandIndexArray) > 0;

		humanSK    = BuildHumanSkeleton(avatarSK, humanIndexArray, leftHandIndexArray, rightHandIndexArray,alloc);
		humanPose  = mecanim::skeleton::CreateSkeletonPose(humanSK, alloc);
		humanGPose = mecanim::skeleton::CreateSkeletonPose(humanSK, alloc);
	
		// build human. Setup human with 'pose', which will be initialized when HumanSetupAxes will be call
		
		//@TODO: SHould we remove support for handles in the runtime too?
		human = mecanim::human::CreateHuman(humanSK, humanPose, 0, humanSK->m_AxesCount, alloc);	
		mecanim::hand::Hand* leftHand = leftHandValid ? mecanim::hand::CreateHand(alloc) : 0;
		mecanim::hand::Hand* rightHand = rightHandValid ? mecanim::hand::CreateHand(alloc) : 0;
		human->m_LeftHand = leftHand;
		human->m_HasLeftHand = leftHandValid;
		human->m_RightHand = rightHand;
		human->m_HasRightHand = rightHandValid;

		for(int i = 0; i < avatarSK->m_Count; i++)
		{
			if(humanIndexArray[i] != -1)
				human->m_HumanBoneIndex[humanIndexArray[i]] = mecanim::skeleton::SkeletonFindNode(humanSK,avatarSK->m_ID[i]);
			
			if(leftHandValid && leftHandIndexArray[i] != -1)
				leftHand->m_HandBoneIndex[leftHandIndexArray[i]] = mecanim::skeleton::SkeletonFindNode(humanSK,avatarSK->m_ID[i]);
			
			if(rightHandValid && rightHandIndexArray[i] != -1)
				rightHand->m_HandBoneIndex[rightHandIndexArray[i]] = mecanim::skeleton::SkeletonFindNode(humanSK,avatarSK->m_ID[i]);
		}

		human->m_ArmTwist = humanDescription.m_ArmTwist;
		human->m_ForeArmTwist = humanDescription.m_ForeArmTwist;
		human->m_UpperLegTwist = humanDescription.m_UpperLegTwist;
		human->m_LegTwist = humanDescription.m_LegTwist;
		human->m_ArmStretch = humanDescription.m_ArmStretch;
		human->m_LegStretch = humanDescription.m_LegStretch;
		human->m_FeetSpacing = humanDescription.m_FeetSpacing;

		mecanim::skeleton::SkeletonPoseCopy(avatarSK,avatarGPose,humanSK,humanGPose);
		humanGPose->m_X[0] = math::xformIdentity();

		mecanim::human::HumanAdjustMass(human);
		mecanim::human::HumanSetupAxes(human, humanGPose);
		mecanim::human::HumanSetupCollider(human, humanGPose);

		if(leftHandValid) mecanim::hand::HandSetupAxes(leftHand, humanGPose, humanSK, true);
		if(rightHandValid) mecanim::hand::HandSetupAxes(rightHand, humanGPose, humanSK, false);

		SetupAxes(human, humanDescription);
	}
	else
	{
		rootMotionIndex = GetIndexFromBoneName (namedTransform.begin(), namedTransform.end(), humanDescription.m_RootMotionBoneName);

		if(rootMotionIndex != -1)
		{
			rootMotionX = avatarGPose->m_X[rootMotionIndex];
			rootMotionSK = BuildRootMotionSkeleton(avatarSK,rootMotionIndex,alloc);
		}
	}

	mecanim::animation::AvatarConstant* avatarConstant = mecanim::animation::CreateAvatarConstant(	avatarSK, avatarPose, avatarDefaultPose, human, rootMotionSK, rootMotionIndex, rootMotionX, alloc);
	avatarConstant->m_SkeletonNameIDCount = namedTransform.size();
	avatarConstant->m_SkeletonNameIDArray = nameIDArray;
	avatar.SetAsset(avatarConstant, tos);
	return std::string();
}

Transform* AvatarBuilder::GetTransform(int id, HumanDescription const& humanDescription, NamedTransforms const& namedTransform, std::vector<string> const& boneName)
{
	HumanBoneList::const_iterator it1 = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(boneName[id]) );
	if(it1!=humanDescription.m_Human.end())
	{
		NamedTransforms::const_iterator it2 = std::find_if(namedTransform.begin(), namedTransform.end(), FindBone( (*it1).m_BoneName ) );
		if(it2 != namedTransform.end())
		{
			return it2->transform;
		}
	}
	return 0;
}

bool AvatarBuilder::IsValidHuman(HumanDescription const& humanDescription, NamedTransforms const& namedTransform, std::string& error)
{
	int i;
	for(i = 0 ; i < HumanTrait::Body::GetBoneCount(); ++i)
	{	
		if(HumanTrait::RequiredBone(i))
		{
			HumanBoneList::const_iterator it1 = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(HumanTrait::Body::GetBoneName(i)) );
			if(it1!=humanDescription.m_Human.end())
			{
				NamedTransforms::const_iterator it2 = std::find_if(namedTransform.begin(), namedTransform.end(), FindBone( (*it1).m_BoneName ) );
				if(it2 == namedTransform.end())
				{
					error = Format("Transform '%s' for human bone '%s' not found", (*it1).m_BoneName.c_str(), HumanTrait::Body::GetBoneName(i).c_str() );
					return false;
				}
			}
			else 
			{
				error = Format("Required human bone '%s' not found", HumanTrait::Body::GetBoneName(i).c_str() );
				return false;
			}
		}
	}

	// Look if all the bone hierarchy parenting is valid
	std::vector<string> boneName = HumanTrait::GetBoneName();	
	Transform* hips = GetTransform(0, humanDescription, namedTransform, boneName);
	if(hips && !hips->GetParent())
	{
		error = Format("Hips bone '%s' must have a parent", hips->GetName() );
		return false;
	}
	else if(hips && hips->GetParent())
	{
		if(std::find_if(namedTransform.begin(), namedTransform.end(), FindBone( hips->GetParent()->GetName() )) == namedTransform.end())
		{
			error = Format("Hips bone parent '%s' must be included in the HumanDescription Skeleton", hips->GetParent()->GetName());
			return false;
		}
	}

	for(i = 0 ; i < HumanTrait::BoneCount; ++i)
	{
		Transform* child = GetTransform(i, humanDescription, namedTransform, boneName);
		if(child)
		{
			// find out next required parent bone
			int parentId = HumanTrait::GetParent(i);
			while(parentId != -1 && !HumanTrait::RequiredBone(parentId))
				parentId = HumanTrait::GetParent(parentId);

			if(parentId != -1)
			{
				Transform* parent = GetTransform(parentId, humanDescription, namedTransform, boneName);
				if(!IsChildOrSameTransform(*child, *parent)) 
				{
					error = Format("Transform '%s' is not an ancestor of '%s'", parent->GetName(), child->GetName() );
					return false;
				}
			}
		}
	}
	return true;
}

bool AvatarBuilder::IsValidHumanDescription(HumanDescription const& humanDescription, std::string& error)
{	
	int i ;
	for(i = 0 ; i < HumanTrait::Body::GetBoneCount(); ++i)
	{	
		if(HumanTrait::RequiredBone(i))
		{
			HumanBoneList::const_iterator it1 = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(HumanTrait::Body::GetBoneName(i)) );
			if(it1 == humanDescription.m_Human.end())
			{
				error = Format("Required human bone '%s' not found", HumanTrait::Body::GetBoneName(i).c_str() );
				return false;
			}
		}
	}

	for(i = 0 ; i < humanDescription.m_Human.size() ; i++)
	{
		if(!humanDescription.m_Human[i].m_BoneName.empty())
		{
			HumanBoneList::const_iterator foundDuplicated = std::find_if(humanDescription.m_Human.begin() + i +1, humanDescription.m_Human.end(), FindHumanBone(humanDescription.m_Human[i].m_HumanName) );
			if(foundDuplicated != humanDescription.m_Human.end())
			{
				error = Format("Found duplicate human bone '%s' with transform '%s' and '%s'", humanDescription.m_Human[i].m_HumanName.c_str(), foundDuplicated->m_BoneName.c_str(),  humanDescription.m_Human[i].m_BoneName.c_str() );
				return false;
			}
		}
	}


	for(i = 0 ; i < humanDescription.m_Human.size() ; i++)
	{
		if(!humanDescription.m_Human[i].m_BoneName.empty())
		{
			HumanBoneList::const_iterator foundDuplicated = std::find_if(humanDescription.m_Human.begin() + i +1, humanDescription.m_Human.end(), FindBoneName(humanDescription.m_Human[i].m_BoneName) );
			if(foundDuplicated != humanDescription.m_Human.end())
			{
				error = Format("Found duplicate transform '%s' for human bone '%s' and '%s'", humanDescription.m_Human[i].m_BoneName.c_str(), foundDuplicated->m_HumanName.c_str(),  humanDescription.m_Human[i].m_HumanName.c_str() );
				return false;
			}
		}
	}

	return true;	
}

std::string AvatarBuilder::GenerateAvatarMap(GameObject const& go, NamedTransforms& namedTransform, const HumanDescription& humanDescription, bool doOptimizeTransformHierarchy, AvatarType avatarType, bool useMask)
{
	Assert(avatarType == kHumanoid || avatarType == kGeneric);

	std::string error;
	if (avatarType == kHumanoid)
	{
		if (!AvatarBuilder::IsValidHumanDescription(humanDescription, error) )
			return error;
	}

	Transform& rootTransform = go.GetComponent(Transform);	
	
	// Get all the transform below root transform
	NamedTransforms namedAllTransform;
	GetAllChildren(rootTransform, namedAllTransform);
	
	Transform* root = &rootTransform;
	if(avatarType == kHumanoid)
	{
		root = GetHipsNode(humanDescription, namedAllTransform);
		if(root == 0)
		{
			HumanBoneList::const_iterator it = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(HumanTrait::Body::GetBoneName(mecanim::human::kHips)) );
			
			return Format("Transform '%s' for human bone '%s' not found", it->m_BoneName.c_str(), HumanTrait::Body::GetBoneName(mecanim::human::kHips).c_str() );
		}
	}
	else if(avatarType == kGeneric && !humanDescription.m_RootMotionBoneName.empty())
	{
		root = GetRootMotionNode(humanDescription, namedAllTransform);
		if(root == 0)
			return Format("Cannot find root motion transform '%s'", humanDescription.m_RootMotionBoneName.c_str() );
	}
	
	
	// Mask is mainly used for API call, when user want to select only a sub part of a hierarchy
	std::vector<UnityStr> mask;
	if(useMask)
	{
		SkeletonBoneList::const_iterator it;
		for(it = humanDescription.m_Skeleton.begin(); it != humanDescription.m_Skeleton.end(); ++it)
			mask.push_back(UnityStr(it->m_Name.c_str()));
	}

	GetAllChildren(rootTransform, namedTransform, mask);

	if(avatarType == kHumanoid)
	{
		if (!AvatarBuilder::IsValidHuman(humanDescription, namedTransform, error))
			return error;
	}

	return std::string();
}

template <class InputIterator1> bool Include(InputIterator1 first1, InputIterator1 last1, UnityStr const& e)
{
	while(first1!=last1)
	{
		if( (*first1) == e) return true;
		first1++;
	}
	return false;
}

void AvatarBuilder::GetAllChildren (Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask)
{
	UnityStr path = CalculateTransformPath (node, &node.GetRoot());
	GetAllChildren (node, path, transforms, mask);
}

void AvatarBuilder::GetAllChildren (Transform& node, UnityStr& path, NamedTransforms& transforms, std::vector<UnityStr> const& mask)
{
	// @TODO: Does it make sense that you can exclude a node but it's child can still be included?
	//        That seems like it can only break stuff...
	bool isIncluded = mask.size() == 0 || Include(mask.begin(), mask.end(), UnityStr(node.GetName()));
	if(isIncluded)
	{
		transforms.push_back(NamedTransform());
		transforms.back().transform = &node;
		transforms.back().path = path;
		transforms.back().name = node.GetName();
	}
	
	for (int i=0;i<node.GetChildrenCount();i++)
	{
		Transform& child = node.GetChild(i);
		size_t pathLength = path.size();
		AppendTransformPath (path, child.GetName());
		
		GetAllChildren(child, path, transforms, mask);
		
		path.resize(pathLength);
	}
}

void AvatarBuilder::GetAllParent (Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask, bool includeSelf)
{
	GetAllParent (node.GetRoot(), node, transforms, mask, includeSelf);
}

void AvatarBuilder::GetAllParent (Transform& root, Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask, bool includeSelf)
{
	if(node.GetParent() != NULL)
	{
		Transform& parent = *node.GetParent();

		// Insertion order matter, top most node must be inserted first	
		GetAllParent(root, parent, transforms, mask);

		bool isIncluded = mask.size() == 0 || Include(mask.begin(), mask.end(), UnityStr(parent.GetName()));
		if(isIncluded)
		{
			transforms.push_back(NamedTransform());
			transforms.back().transform = &parent;
			transforms.back().path = CalculateTransformPath(parent, &root);
			transforms.back().name = parent.GetName();
		}
	}

	if(includeSelf)
	{
		bool isIncluded = mask.size() == 0 || Include(mask.begin(), mask.end(), UnityStr(node.GetName()));
		if(isIncluded)
		{
			transforms.push_back(NamedTransform());
			transforms.back().transform = &node;
			transforms.back().path = CalculateTransformPath(node, &root);
			transforms.back().name = node.GetName();
		}
	}
}

Transform* AvatarBuilder::GetHipsNode(const HumanDescription& humanDescription, NamedTransforms const& transforms)
{
	HumanBoneList::const_iterator it = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindHumanBone(HumanTrait::Body::GetBoneName(mecanim::human::kHips)) );
	if(it != humanDescription.m_Human.end())
	{
		NamedTransforms::const_iterator it2 = std::find_if(transforms.begin(), transforms.end(), FindBone( it->m_BoneName ) );
		if(it2 != transforms.end())
			return it2->transform;
	}
	return 0;
}

Transform*  AvatarBuilder::GetRootMotionNode(const HumanDescription& humanDescription, NamedTransforms const& transforms)
{
	NamedTransforms::const_iterator it = std::find_if(transforms.begin(), transforms.end(), FindBone( humanDescription.m_RootMotionBoneName ) );	
	return it != transforms.end() ? it->transform : 0;
}

bool AvatarBuilder::RemoveAllNoneHumanLeaf(NamedTransforms& namedTransform, HumanDescription const & humanDescription)
{
	bool didRemove = false;

	for (int n = 0; n < namedTransform.size(); n++) 
	{
		Transform& transfom = *namedTransform[n].transform;
		bool hasAnyChildMapped = false;
		for(int i=0;i < transfom.GetChildrenCount() && !hasAnyChildMapped; i++)
		{
			NamedTransforms::const_iterator it2 = std::find_if(namedTransform.begin(), namedTransform.end(), FindBone( transfom.GetChild(i).GetName() ) );
			hasAnyChildMapped = it2 != namedTransform.end();
		}

		if( !hasAnyChildMapped)
		{
			HumanBoneList::const_iterator it2 = std::find_if(humanDescription.m_Human.begin(), humanDescription.m_Human.end(), FindBoneName( transfom.GetName() ));
			if(it2==humanDescription.m_Human.end())
			{
				namedTransform.erase(namedTransform.begin() + n);
				n--;
				didRemove = true;
			}
		}
	}

	return didRemove;
}

void AvatarBuilder::ReadFromLocalTransformToSkeletonPose(mecanim::skeleton::SkeletonPose* pose, NamedTransforms const& namedTransform)
{
	int j;
	for(j=0;j<namedTransform.size();j++)
	{
		Transform& transform = *namedTransform[j].transform;
		pose->m_X[j] = xformFromUnity(transform.GetLocalPosition(), transform.GetLocalRotation(), transform.GetLocalScale());
	}
}

bool AvatarBuilder::TPoseMatch(mecanim::animation::AvatarConstant const& avatar, NamedTransforms const& namedTransform, std::string& warning)
{
	bool ret = true;
	if(avatar.isHuman())
	{
		mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);	

		mecanim::skeleton::SkeletonPose*  avatarSKPose = mecanim::skeleton::CreateSkeletonPose(avatar.m_AvatarSkeleton.Get(), alloc);
		mecanim::skeleton::SkeletonPose*  avatarHumanPose = mecanim::skeleton::CreateSkeletonPose(avatar.m_Human->m_Skeleton.Get(), alloc);		
		mecanim::skeleton::SkeletonPose*  avatarGPose1 = mecanim::skeleton::CreateSkeletonPose(avatar.m_Human->m_Skeleton.Get(), alloc);
		mecanim::skeleton::SkeletonPose*  avatarGPose2 = mecanim::skeleton::CreateSkeletonPose(avatar.m_Human->m_Skeleton.Get(), alloc);
	
		ReadFromLocalTransformToSkeletonPose(avatarSKPose, namedTransform);
		mecanim::skeleton::SkeletonPoseCopy(avatar.m_AvatarSkeleton.Get(),avatarSKPose,avatar.m_Human->m_Skeleton.Get(), avatarHumanPose);

		mecanim::skeleton::SkeletonPoseComputeGlobal(avatar.m_Human->m_Skeleton.Get(), avatarHumanPose, avatarGPose1);
		mecanim::skeleton::SkeletonPoseComputeGlobal(avatar.m_Human->m_Skeleton.Get(), avatar.m_Human->m_SkeletonPose.Get(), avatarGPose2);

		// Do not check for human hips bone lenght, because an animation is not guarantee to start at origin.
		mecanim::uint32_t i = avatar.m_Human->m_HumanBoneIndex[mecanim::human::kHips + 1];
		for(;i<avatar.m_Human->m_Skeleton->m_Count;++i)
		{
			mecanim::int32_t parentId = avatar.m_Human->m_Skeleton->m_Node[i].m_ParentId;
			if(parentId != -1)
			{
				float len1 = math::length(avatarGPose1->m_X[i].t - avatarGPose1->m_X[parentId].t).tofloat();
				float len2 = math::length(avatarGPose2->m_X[i].t - avatarGPose2->m_X[parentId].t).tofloat();

				if( math::abs(len1-len2) > M_EPSF && math::abs(len2) > M_EPSF)
				{				
					float ratio = len1/len2;

					if( math::abs(1.f-ratio) > 0.30f)
					{
						warning += Format("'%s' : avatar = %.2f, animation = %.2f\n", namedTransform[avatar.m_HumanSkeletonIndexArray[i]].transform->GetName(), len2, len1);		
						ret = false;
					}
				}
			}
		}	

		mecanim::skeleton::DestroySkeletonPose(avatarSKPose, alloc);
		mecanim::skeleton::DestroySkeletonPose(avatarHumanPose, alloc);
		mecanim::skeleton::DestroySkeletonPose(avatarGPose1, alloc);
		mecanim::skeleton::DestroySkeletonPose(avatarGPose2, alloc);
	}
	
	return ret;
}
