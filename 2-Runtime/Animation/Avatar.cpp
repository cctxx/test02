#include "UnityPrefix.h"

#include "Avatar.h"


#include "Runtime/mecanim/human/hand.h"

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"

#include "MecanimUtility.h"


Avatar::Avatar(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode),
 m_Avatar(0),
 m_Allocator(1024*4),
 m_AvatarSize(0),
 m_ObjectUsers(this)
{

}

Avatar::~Avatar()
{
	NotifyObjectUsers( kDidModifyAvatar );
}

void Avatar::NotifyObjectUsers(const MessageIdentifier& msg)
{	
	m_ObjectUsers.SendMessage(msg);
}

void Avatar::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	NotifyObjectUsers( kDidModifyAvatar );
}


void Avatar::CheckConsistency () 
{
	Super::CheckConsistency();
	mecanim::animation::AvatarConstant* cst = GetAsset();
	if(cst != 0)
	{
		//@TODO: It looks like CheckConsistency is being abused to generate m_HumanSkeletonIndexArray??? Shouldn't the model importer do this or whatever generates the avatar constant
		
		// [case 522188] This is an old maya file with avatar already created. 
		// At this time m_HumanSkeletonIndexArray was not there and in this particular case the user did open
		// the file on a pc without maya so we couldn't reimport the file and create a valid avatar.
		if(cst->isHuman() && cst->m_HumanSkeletonIndexCount != cst->m_Human->m_Skeleton->m_Count)
		{
			cst->m_HumanSkeletonIndexCount = cst->m_Human->m_Skeleton->m_Count;

			// No need to deallocate memory here since we are using the ChainedAllocator. 
			// the allocator will release the memory on next reset().
			cst->m_HumanSkeletonIndexArray = m_Allocator.ConstructArray<mecanim::int32_t>(cst->m_HumanSkeletonIndexCount);

			mecanim::skeleton::SkeletonBuildIndexArray(cst->m_HumanSkeletonIndexArray.Get(), cst->m_Human->m_Skeleton.Get(), cst->m_AvatarSkeleton.Get());
		}
	}
}


IMPLEMENT_OBJECT_SERIALIZE (Avatar)
IMPLEMENT_CLASS (Avatar)

template<class TransferFunction>
void Avatar::Transfer (TransferFunction& transfer)
{
	SETPROFILERLABEL(AvatarConstant);

	Super::Transfer (transfer);

	TRANSFER(m_AvatarSize);
	
	if(m_Avatar == 0 )
		m_Allocator.Reserve(m_AvatarSize);

	transfer.SetUserData(&m_Allocator);
	TRANSFER_NULLABLE(m_Avatar, mecanim::animation::AvatarConstant);
	TRANSFER(m_TOS);
}

void Avatar::SetAsset (mecanim::animation::AvatarConstant* avatarConstant, TOSVector const& tos)
{
	// Free previously allocated memory
	m_Allocator.Reset();

	size_t size = m_AvatarSize;
	m_Avatar = CopyBlob(*avatarConstant, m_Allocator, size );
	m_AvatarSize = size;
	m_TOS = tos;

	NotifyObjectUsers( kDidModifyAvatar );
}

mecanim::animation::AvatarConstant*	Avatar::GetAsset()
{
	return m_Avatar;
}

const mecanim::animation::AvatarConstant*	Avatar::GetAsset() const
{
	return m_Avatar;
}

TOSVector const& Avatar::GetTOS() const
{
	return m_TOS;
}

bool	Avatar::IsValid()const
{
	return GetAsset() != 0;
}

bool Avatar::IsHuman() const
{
	return m_Avatar && m_Avatar->isHuman();
}

bool Avatar::HasRootMotion() const
{
	return m_Avatar && m_Avatar->m_RootMotionBoneIndex != -1;
}

float  Avatar::GetHumanScale() const
{
	return IsHuman() ? m_Avatar->m_Human->m_Scale : 1;
}

float	Avatar::GetLeftFeetBottomHeight() const
{
	if(!IsHuman())
		return 0.f;

	return HumanGetFootHeight(m_Avatar->m_Human.Get(), true);
}

float	Avatar::GetRightFeetBottomHeight() const
{
	if(!IsHuman())
		return 0.f;
	
	return HumanGetFootHeight(m_Avatar->m_Human.Get(), false);
}


void Avatar::SetParameter(int parameterId, float value)
{
	mecanim::animation::AvatarConstant* avatar = GetAsset();
	if(avatar)
	{
		switch(parameterId)
		{
		case UpperArmTwist: avatar->m_Human->m_ArmTwist = value; break;
		case LowerArmTwist: avatar->m_Human->m_ForeArmTwist = value; break;
		case UpperLegTwist: avatar->m_Human->m_UpperLegTwist = value; break;
		case LowerLegTwsit: avatar->m_Human->m_LegTwist = value; break;
		case ArmStretch: avatar->m_Human->m_ArmStretch = value; break;
		case LegStretch: avatar->m_Human->m_LegStretch = value; break;
		case FeetSpacing: avatar->m_Human->m_FeetSpacing = value; break;
		default: break;
		}
	}
}

void	Avatar::SetMuscleMinMax(int muscleId, float min, float max)
{
	int humanId = HumanTrait::BoneFromMuscle(muscleId);

	mecanim::animation::AvatarConstant* avatar = GetAsset();

	int boneId = HumanTrait::GetBoneId(*this, humanId);
	if(boneId != -1)
	{
		int axesId = avatar->m_Human->m_Skeleton->m_Node[boneId].m_AxesId;
		if(axesId != -1)
		{
			math::float4 maxv = avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_Limit.m_Max;
			math::float4 minv = avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_Limit.m_Min;

			int musclex = HumanTrait::MuscleFromBone(humanId, 0);
			int muscley = HumanTrait::MuscleFromBone(humanId, 1);
			int musclez = HumanTrait::MuscleFromBone(humanId, 2);

			if(musclex == muscleId)
			{
				minv.x() = math::radians(min);
				maxv.x() = math::radians(max);
			}
			else if(muscley == muscleId)
			{
				minv.y() = math::radians(min);
				maxv.y() = math::radians(max);
			}
			else if(musclez == muscleId)
			{
				minv.z() = math::radians(min);
				maxv.z() = math::radians(max);
			}

			avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_Limit.m_Max = maxv;
			avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_Limit.m_Min = minv;
		}
	}
}

float		Avatar::GetAxisLength(int humanId)const
{
	float ret = 0.0f;

	mecanim::animation::AvatarConstant const* avatar = GetAsset();

	int boneId =  HumanTrait::GetBoneId(*this, humanId);
	if(boneId != -1)
	{
		int axesId = avatar->m_Human->m_Skeleton->m_Node[boneId].m_AxesId;
		if(axesId != -1)
		{
			ret = avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_Length;
		}
	}

	return ret;
}

Quaternionf Avatar::GetPreRotation(int humanId)const
{
	math::float4 ret = math::quatIdentity();

	mecanim::animation::AvatarConstant const* avatar = GetAsset();

	int boneId =  HumanTrait::GetBoneId(*this, humanId);
	if(boneId != -1)
	{
		int axesId = avatar->m_Human->m_Skeleton->m_Node[boneId].m_AxesId;
		if(axesId != -1)
		{
			ret = avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_PreQ;
		}
	}

	return float4ToQuaternionf(ret);
}
Quaternionf Avatar::GetPostRotation(int humanId)const
{
	math::float4 ret = math::quatIdentity();

	mecanim::animation::AvatarConstant const* avatar = GetAsset();

	int boneId =  HumanTrait::GetBoneId(*this, humanId);
	if(boneId != -1)
	{
		int axesId = avatar->m_Human->m_Skeleton->m_Node[boneId].m_AxesId;
		if(axesId != -1)
		{
			ret = avatar->m_Human->m_Skeleton->m_AxesArray[axesId].m_PostQ;
		}
	}

	return float4ToQuaternionf(ret);
}

Quaternionf Avatar::GetZYPostQ(int index, Quaternionf const& parentQ, Quaternionf const& q)const
{
	mecanim::animation::AvatarConstant const* cst = GetAsset();

	math::float4 qzypost = math::quatIdentity();

	int id = HumanTrait::GetBoneId(*this, index);
	if( id != -1)
	{
		int axesId = cst->m_Human->m_Skeleton->m_Node[id].m_AxesId;
		if(axesId != -1)
		{
			math::float4 mpq = QuaternionfTofloat4(parentQ);
			math::float4 mq = QuaternionfTofloat4(q);

			math::Axes const& axes = cst->m_Human->m_Skeleton->m_AxesArray[axesId];

			math::float4 lq = normalize(quatMul(quatConj(mpq),mq));
			math::float4 dofzy = ToAxes(axes,lq);
			dofzy.x() = 0;
			qzypost = normalize(quatMul(mpq,quatMul( FromAxes(axes,dofzy),axes.m_PostQ)));
		}
	}
	return float4ToQuaternionf(qzypost);
}

Quaternionf Avatar::GetZYRoll(int index, Vector3f const& v)const
{
	mecanim::animation::AvatarConstant const* cst = GetAsset();

	math::float4 qzyroll = math::quatIdentity();

	int id = HumanTrait::GetBoneId(*this, index);

	if( id != -1)
	{
		int axesId = cst->m_Human->m_Skeleton->m_Node[id].m_AxesId;
		if(axesId != -1)
		{
			ATTRIBUTE_ALIGN(ALIGN4F) float buf[4] = {v.x, v.y, v.z, 0};

			math::Axes const& axes = cst->m_Human->m_Skeleton->m_AxesArray[axesId];
			math::float4 uvw = math::load(buf);
			qzyroll = ZYRoll2Quat(halfTan(LimitUnproject(axes.m_Limit,uvw))*sgn(axes.m_Sgn));
		}
	}
	return float4ToQuaternionf(qzyroll);
}

Vector3f Avatar::GetLimitSign(int index)const
{
	mecanim::animation::AvatarConstant const* cst = GetAsset();	
	int id = HumanTrait::GetBoneId(*this, index);

	Vector3f sign = Vector3f::one;
	if( id != -1)
	{
		int axesId = cst->m_Human->m_Skeleton->m_Node[id].m_AxesId;
		if(axesId != -1)
		{
			sign.x = cst->m_Human->m_Skeleton->m_AxesArray[axesId].m_Sgn.x().tofloat();
			sign.y = cst->m_Human->m_Skeleton->m_AxesArray[axesId].m_Sgn.y().tofloat();
			sign.z = cst->m_Human->m_Skeleton->m_AxesArray[axesId].m_Sgn.z().tofloat();
		}
	}
	return sign;
}

std::string HumanTrait::GetFingerMuscleName(int index, bool left)
{
	std::string fingerName = left ? "Left " : "Right ";
	if(0 <= index && index < mecanim::hand::s_DoFCount)
	{
		int fingerIndex = index / mecanim::hand::kLastFingerDoF;
		int dofIndex = index % mecanim::hand::kLastFingerDoF;

		fingerName += mecanim::hand::FingerName(fingerIndex);
		fingerName += " "; 
		fingerName += mecanim::hand::FingerDoFName(dofIndex);
	}
	return fingerName;	
}

std::string HumanTrait::GetFingerName(int index, bool left)
{
	std::string fingerName = left ? "Left " : "Right ";
	if(0 <= index && index < mecanim::hand::s_BoneCount)
	{
		int fingerIndex = index /  mecanim::hand::kLastPhalange;
		int phalangesIndex = index %  mecanim::hand::kLastPhalange;

		fingerName += mecanim::hand::FingerName(fingerIndex);
		fingerName += " "; 
		fingerName += mecanim::hand::PhalangeName(phalangesIndex);
	}
	return fingerName;	
}

int HumanTrait::Body::GetBoneCount()
{
	return mecanim::human::kLastBone;
}

std::string HumanTrait::Body::GetBoneName(int index)
{
	return std::string( mecanim::human::BoneName(index) );
}

int HumanTrait::Body::GetMuscleCount()
{
	return mecanim::human::kLastDoF;
}

std::string HumanTrait::Body::GetMuscleName(int index)
{
	return std::string(mecanim::human::MuscleName(index));
}

int HumanTrait::LeftFinger::GetBoneCount()
{
	return mecanim::hand::s_BoneCount;
}

std::string HumanTrait::LeftFinger::GetBoneName(int index)
{
	return HumanTrait::GetFingerName(index, IsLeftHand());
}

int HumanTrait::LeftFinger::GetMuscleCount()
{
	return mecanim::hand::s_DoFCount;
}

std::string HumanTrait::LeftFinger::GetMuscleName(int index)
{
	return HumanTrait::GetFingerMuscleName(index, IsLeftHand());
}

bool HumanTrait::LeftFinger::IsLeftHand()
{
	return true;
}


int HumanTrait::RightFinger::GetBoneCount()
{
	return mecanim::hand::s_BoneCount;
}

std::string HumanTrait::RightFinger::GetBoneName(int index)
{
	return HumanTrait::GetFingerName(index, IsLeftHand());
}

int HumanTrait::RightFinger::GetMuscleCount()
{
	return mecanim::hand::s_DoFCount;
}

std::string HumanTrait::RightFinger::GetMuscleName(int index)
{
	return HumanTrait::GetFingerMuscleName(index, IsLeftHand());
}

bool HumanTrait::RightFinger::IsLeftHand()
{
	return false;
}

std::vector<string> HumanTrait::GetMuscleName()
{
	static std::vector<string> muscles = InternalGetMuscleName();
	return muscles;
}

std::vector<string> HumanTrait::GetBoneName()
{	
	static std::vector<string> bones = InternalGetBoneName();
	return bones;
}

int HumanTrait::MuscleFromBone(int i, int dofIndex)
{
	if(i < LastBone)
		return  mecanim::human::MuscleFromBone(i, dofIndex);	
	else if(i < LastLeftFingerBone)
	{
		int muscle = mecanim::hand::MuscleFromBone(i-LastBone, dofIndex);
		return muscle != -1 ? LastDoF + muscle : -1;
	}
	else if(i < LastRightFingerBone)
	{
		int muscle = mecanim::hand::MuscleFromBone(i-LastLeftFingerBone, dofIndex);
		return muscle != -1 ? LastLeftFingerDoF + muscle : -1;
	}
	return -1;
}

int HumanTrait::BoneFromMuscle(int i)
{
	if(i < LastDoF)
		return mecanim::human::BoneFromMuscle(i);
	else if(i < LastLeftFingerDoF)
	{
		int bone = mecanim::hand::BoneFromMuscle(i-LastDoF);
		return bone != -1 ? LastBone + bone : -1;
	}
	else if(i < LastRightFingerDoF)
	{
		int bone = mecanim::hand::BoneFromMuscle(i-LastLeftFingerDoF);
		return bone != -1 ? LastLeftFingerBone + bone : -1;
	}
	return -1;
}

int HumanTrait::GetBoneId(Avatar const& avatar, int humanId)
{
	mecanim::animation::AvatarConstant const* cst = avatar.GetAsset();

	int index = -1;
	if(humanId < LastBone && cst->isHuman())
		index = cst->m_Human->m_HumanBoneIndex[humanId];
	else if(humanId < LastLeftFingerBone && cst->isHuman() && !cst->m_Human->m_LeftHand.IsNull())
		index = cst->m_Human->m_LeftHand->m_HandBoneIndex[humanId - LastBone];
	else if(humanId < LastRightFingerBone && cst->isHuman() && !cst->m_Human->m_RightHand.IsNull())
		index = cst->m_Human->m_RightHand->m_HandBoneIndex[humanId - LastLeftFingerBone];	

	return index;
}

bool HumanTrait::RequiredBone(int humanId)
{
	if(humanId < LastBone)
		return mecanim::human::RequiredBone(humanId);
	return false;
}

int HumanTrait::RequiredBoneCount()
{
	int count = 0;
	for(int i=0;i<LastBone;i++)
	{
		count = RequiredBone(i) ? count + 1 : count;
	}
	return count;
}

bool HumanTrait::HasCollider(Avatar& avatar, int humanId)
{
	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();

	bool ret = false;
	if(humanId < LastBone && cst->isHuman())
		ret = cst->m_Human->m_ColliderIndex[humanId] != -1;
	return ret;
}

int HumanTrait::GetColliderId(Avatar& avatar, int humanId)
{
	mecanim::animation::AvatarConstant* cst = avatar.GetAsset();

	int ret = -1;
	if(humanId < LastBone && cst->isHuman())
		ret = cst->m_Human->m_ColliderIndex[humanId];
	return ret;
}

int HumanTrait::GetParent(int humanId)
{
	#define FINGER_INDEX(finger, phalanges) (mecanim::hand::finger * mecanim::hand::kLastPhalange) + mecanim::hand::phalanges

	const int LeftFingerStart = LastBone;
	const int RightFingerStart = LastLeftFingerBone;

	static int humanParent[] = { 
		-1, 
		mecanim::human::kHips, 
		mecanim::human::kHips, 
		mecanim::human::kLeftUpperLeg,
		mecanim::human::kRightUpperLeg,
		mecanim::human::kLeftLowerLeg,
		mecanim::human::kRightLowerLeg,
		mecanim::human::kHips,
		mecanim::human::kSpine,
		mecanim::human::kChest,
		mecanim::human::kNeck,
		mecanim::human::kChest,
		mecanim::human::kChest,
		mecanim::human::kLeftShoulder,
		mecanim::human::kRightShoulder,
		mecanim::human::kLeftUpperArm,
		mecanim::human::kRightUpperArm,
		mecanim::human::kLeftLowerArm,
		mecanim::human::kRightLowerArm,
		mecanim::human::kLeftFoot,
		mecanim::human::kRightFoot,
		mecanim::human::kHead,
		mecanim::human::kHead,
		mecanim::human::kHead,
		mecanim::human::kLeftHand, LeftFingerStart + FINGER_INDEX(kThumb, kProximal),  LeftFingerStart + FINGER_INDEX(kThumb, kIntermediate),
		mecanim::human::kLeftHand, LeftFingerStart + FINGER_INDEX(kIndex, kProximal),  LeftFingerStart + FINGER_INDEX(kIndex, kIntermediate),
		mecanim::human::kLeftHand, LeftFingerStart + FINGER_INDEX(kMiddle, kProximal), LeftFingerStart + FINGER_INDEX(kMiddle, kIntermediate), 
		mecanim::human::kLeftHand, LeftFingerStart + FINGER_INDEX(kRing, kProximal),   LeftFingerStart + FINGER_INDEX(kRing, kIntermediate),
		mecanim::human::kLeftHand, LeftFingerStart + FINGER_INDEX(kLittle, kProximal), LeftFingerStart + FINGER_INDEX(kLittle, kIntermediate),
		mecanim::human::kRightHand, RightFingerStart + FINGER_INDEX(kThumb, kProximal),  RightFingerStart + FINGER_INDEX(kThumb, kIntermediate),
		mecanim::human::kRightHand, RightFingerStart + FINGER_INDEX(kIndex, kProximal),  RightFingerStart + FINGER_INDEX(kIndex, kIntermediate),
		mecanim::human::kRightHand, RightFingerStart + FINGER_INDEX(kMiddle, kProximal), RightFingerStart + FINGER_INDEX(kMiddle, kIntermediate), 
		mecanim::human::kRightHand, RightFingerStart + FINGER_INDEX(kRing, kProximal),   RightFingerStart + FINGER_INDEX(kRing, kIntermediate),
		mecanim::human::kRightHand, RightFingerStart + FINGER_INDEX(kLittle, kProximal), RightFingerStart + FINGER_INDEX(kLittle, kIntermediate),
	};

	Assert(0 <= humanId && humanId < BoneCount);
	return humanParent[humanId];
}

float HumanTrait::GetMuscleDefaultMin(int i)
{
	int bone = HumanTrait::BoneFromMuscle(i);
	int dx = HumanTrait::MuscleFromBone (bone, 0);
	int dy = HumanTrait::MuscleFromBone (bone, 1);
	int dz = HumanTrait::MuscleFromBone (bone, 2);

	if(i < LastDoF)
	{		
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::human::GetAxeInfo(bone);
		if(i == dx) return axeInfo.m_Min[0];
		else if(i == dy) return axeInfo.m_Min[1];
		else if(i == dz) return axeInfo.m_Min[2];		
	}
	else if(i < LastLeftFingerDoF)
	{
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::hand::GetAxeInfo(bone-LastBone);
		if(i == dx) return axeInfo.m_Min[0];
		else if(i == dy) return axeInfo.m_Min[1];
		else if(i == dz) return axeInfo.m_Min[2];
	}
	else if(i < LastRightFingerDoF)
	{
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::hand::GetAxeInfo(bone-LastLeftFingerBone);
		if(i == dx) return axeInfo.m_Min[0];
		else if(i == dy) return axeInfo.m_Min[1];
		else if(i == dz) return axeInfo.m_Min[2];
	}
	return 0.f;
}

float HumanTrait::GetMuscleDefaultMax(int i)
{
	int bone = HumanTrait::BoneFromMuscle(i);
	int dx = HumanTrait::MuscleFromBone (bone, 0);
	int dy = HumanTrait::MuscleFromBone (bone, 1);
	int dz = HumanTrait::MuscleFromBone (bone, 2);

	if(i < LastDoF)
	{		
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::human::GetAxeInfo(bone);
		if(i == dx) return axeInfo.m_Max[0];
		else if(i == dy) return axeInfo.m_Max[1];
		else if(i == dz) return axeInfo.m_Max[2];		
	}
	else if(i < LastLeftFingerDoF)
	{
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::hand::GetAxeInfo(bone-LastBone);
		if(i == dx) return axeInfo.m_Max[0];
		else if(i == dy) return axeInfo.m_Max[1];
		else if(i == dz) return axeInfo.m_Max[2];
	}
	else if(i < LastRightFingerDoF)
	{
		mecanim::skeleton::SetupAxesInfo const& axeInfo = mecanim::hand::GetAxeInfo(bone-LastLeftFingerBone);
		if(i == dx) return axeInfo.m_Max[0];
		else if(i == dy) return axeInfo.m_Max[1];
		else if(i == dz) return axeInfo.m_Max[2];
	}
	return 0.f;
}

std::vector<string> HumanTrait::InternalGetMuscleName()
{
	std::vector<string> muscles;

	muscles.reserve(MuscleCount);
	for (int i = 0; i < MuscleCount; i++)
	{
		if(i < LastDoF)
			muscles.push_back( Body::GetMuscleName(i) );
		else if(i < LastLeftFingerDoF)
			muscles.push_back( LeftFinger::GetMuscleName(i-LastDoF) );				
		else if(i < LastRightFingerDoF)
			muscles.push_back( RightFinger::GetMuscleName(i-LastLeftFingerDoF) );
	}	
	return muscles;
}

std::vector<string> HumanTrait::InternalGetBoneName()
{	
	std::vector<string> bones;

	bones.reserve(BoneCount);
	for (int i = 0; i < BoneCount; i++)
	{
		if(i < LastBone)
			bones.push_back( Body::GetBoneName(i) );
		else if(i < LastLeftFingerBone)
			bones.push_back( LeftFinger::GetBoneName(i-LastBone) );
		else if(i < LastRightFingerBone)
			bones.push_back( RightFinger::GetBoneName(i-LastLeftFingerBone) );
	}

	return bones;
}
