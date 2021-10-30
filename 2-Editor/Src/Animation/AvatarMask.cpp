#include "UnityPrefix.h"

#include "AvatarMask.h"

#include "Runtime/mecanim/human/human.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/generic/crc32.h"

#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"


#define DIRTY_AND_INVALIDATE() m_UserList.SendMessage(kDidModifyMotion); SetDirty();  

mecanim::human::HumanPoseMask HumanPoseMaskFromBodyMask(std::vector<UInt32> const &bodyMask)
{
	mecanim::human::HumanPoseMask poseMask;

	poseMask.set(mecanim::human::kMaskRootIndex,bodyMask[kRoot]!=0);

	for(int goalIter = 0; goalIter < mecanim::human::kLastGoal; goalIter++)
	{
		poseMask.set(mecanim::human::kMaskGoalStartIndex+goalIter,bodyMask[kLeftFootIK+goalIter]!=0);
	}

	for(int dofIter = 0; dofIter < mecanim::human::kLastBodyDoF; dofIter++)
	{
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kBodyDoFStart+dofIter,bodyMask[kBody]!=0);
	}

	for(int dofIter = 0; dofIter < mecanim::human::kLastHeadDoF; dofIter++)
	{
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kHeadDoFStart+dofIter,bodyMask[kHead]!=0);
	}

	for(int dofIter = 0; dofIter < mecanim::human::kLastLegDoF; dofIter++)
	{
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kLeftLegDoFStart+dofIter,bodyMask[kLeftLowerLeg]!=0);
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kRightLegDoFStart+dofIter,bodyMask[kRightLowerLeg]!=0);
	}
		
	for(int dofIter = 0; dofIter < mecanim::human::kLastArmDoF; dofIter++)
	{
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kLeftArmDoFStart+dofIter,bodyMask[kLeftUpperArm]!=0);
		poseMask.set(mecanim::human::kMaskDoFStartIndex+mecanim::human::kRightArmDoFStart+dofIter,bodyMask[kRightUpperArm]!=0);
	}

	poseMask.set(mecanim::human::kMaskLeftHand,bodyMask[kLeftFingers]!=0);
	poseMask.set(mecanim::human::kMaskRightHand,bodyMask[kRightFingers]!=0);

	return poseMask;

}

AvatarMask::AvatarMask(MemLabelId label, ObjectCreationMode mode) : Super(label, mode), m_UserList(this)
{
	for(int maskIter = 0; maskIter < kLastMaskBodyPart; maskIter++)
	{
		m_Mask.push_back(1);
	}
}

AvatarMask::~AvatarMask()
{
}

void AvatarMask::InitializeClass()
{
	RegisterAllowNameConversion("AvatarMask", "elements", "m_Elements");

	RegisterAllowNameConversion("TransformMaskElement", "path", "m_Path");
	RegisterAllowNameConversion("TransformMaskElement", "weight", "m_Weight");
}

void AvatarMask::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	m_UserList.SendMessage(kDidModifyMotion);
}

void AvatarMask::Reset()
{
	Super::Reset();
}

int  AvatarMask::GetBodyPartCount() const
{
	return kLastMaskBodyPart;
}

bool  AvatarMask::GetBodyPart(int index)
{
	if(!ValidateBodyPartIndex(index))
		return false;

	return m_Mask[index] != 0;
}

void  AvatarMask::SetBodyPart(int index, bool value)
{
	if(!ValidateBodyPartIndex(index))
		return ;

	UInt32 ui32Value = value ? 1 : 0;
	if(m_Mask[index] != ui32Value)
	{
		m_Mask[index] = ui32Value;
		DIRTY_AND_INVALIDATE();
	}
}

mecanim::human::HumanPoseMask AvatarMask::GetHumanPoseMask(UserList& dependencies)
{
	dependencies.AddUser(m_UserList);
	return HumanPoseMaskFromBodyMask(m_Mask);
}

IMPLEMENT_OBJECT_SERIALIZE (AvatarMask)
IMPLEMENT_CLASS_HAS_INIT (AvatarMask)

template<class TransferFunction>
void AvatarMask::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER(m_Mask);
	TRANSFER(m_Elements);
}

bool AvatarMask::HasFeetIK()
{
	return GetBodyPart(kLeftFootIK) && GetBodyPart(kRightFootIK);		
}

void AvatarMask::SetTransformCount(int count)
{
	if(count != m_Elements.size())
	{
		m_Elements.resize(count);
		DIRTY_AND_INVALIDATE();
	}		
}

int AvatarMask::GetTransformCount() const
{
	return m_Elements.size();
}


string AvatarMask::GetTransformPath(int index)
{
	if(!ValidateTransformIndex(index))
		return "";

	return m_Elements[index].m_Path;
}

void   AvatarMask::SetTransformPath(int index, string const& path)
{
	if(!ValidateTransformIndex(index))
		return ;

	if(m_Elements[index].m_Path != path)
	{
		m_Elements[index].m_Path = path;
		DIRTY_AND_INVALIDATE();
	}
}

float AvatarMask:: GetTransformWeight(int index)
{
	if(!ValidateTransformIndex(index))
		return 0;

	return m_Elements[index].m_Weight;

}

void  AvatarMask::SetTransformWeight(int index, float weight)
{
	if(!ValidateTransformIndex(index))
		return ;

	if(m_Elements[index].m_Weight != weight)
	{
		m_Elements[index].m_Weight = weight;
		DIRTY_AND_INVALIDATE();

	}	
}

mecanim::skeleton::SkeletonMask* AvatarMask::GetSkeletonMask(UserList& dependencies, mecanim::memory::Allocator& alloc)
{
	dependencies.AddUser(m_UserList);
	return SkeletonMaskFromTransformMask(*this, alloc);
}

bool AvatarMask::ValidateBodyPartIndex(int index) const 
{
	if(index >= 0 && index < GetBodyPartCount())
	{
		return true;	
	}

	ErrorString("Invalid BodyPart Index");
	return false;	
}

bool AvatarMask::ValidateTransformIndex(int index) const 
{
	if(index >= 0 && index < GetTransformCount())
	{
		return true;	
	}

	ErrorString("Invalid Transform Index");
	return false;	
}

mecanim::skeleton::SkeletonMask* SkeletonMaskFromTransformMask(AvatarMask const &mask, mecanim::memory::Allocator& alloc)
{
	std::vector<mecanim::skeleton::SkeletonMaskElement> list;

	AvatarMask::ElementList::const_iterator it;
	for(it=mask.m_Elements.begin();it!=mask.m_Elements.end();++it)
	{
		if(!it->m_Path.empty()) // root path == ""
		{
			mecanim::uint32_t pathHash = mecanim::processCRC32(it->m_Path.c_str());

			mecanim::skeleton::SkeletonMaskElement element;			
			element.m_PathHash = pathHash;
			element.m_Weight = it->m_Weight;

			list.push_back(element);
		}
	}

	return list.size() > 0 ? mecanim::skeleton::CreateSkeletonMask(list.size(), &list.front(), alloc) : 0;
}

#undef DIRTY_AND_INVALIDATE