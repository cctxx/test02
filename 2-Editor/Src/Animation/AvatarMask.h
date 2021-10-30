#ifndef AVATARMASK_H
#define AVATARMASK_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Serialize/SerializeTraits.h"

#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/mecanim/human/human.h"
#include "Runtime/Misc/UserList.h"

namespace mecanim
{
	namespace memory
	{
		class Allocator;
	};

	namespace skeleton
	{
		struct SkeletonMask;
		struct Skeleton;
	};
};


enum MaskBodyPart
{
    kRoot,
    kBody,
    kHead,
    kLeftLowerLeg,
    kRightLowerLeg,
    kLeftUpperArm,
    kRightUpperArm,
    kLeftFingers,
    kRightFingers,
    kLeftFootIK,
    kRightFootIK,
    kLeftHandIK,
    kRightHandIK,
    kLastMaskBodyPart
};

class AvatarMask;

mecanim::human::HumanPoseMask HumanPoseMaskFromBodyMask(std::vector<UInt32> const &m_Mask);
mecanim::skeleton::SkeletonMask* SkeletonMaskFromTransformMask(AvatarMask const &mask, mecanim::memory::Allocator& alloc);


struct TransformMaskElement
{

	TransformMaskElement() : m_Path(""), m_Weight(0)
	{	
	}

	TransformMaskElement(std::string transformPath, bool weight) : m_Path(transformPath), m_Weight(weight)
	{
	}

	UnityStr m_Path;
	float m_Weight;

	DEFINE_GET_TYPESTRING(TransformMaskElement);

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{		
		TRANSFER(m_Path);
		TRANSFER(m_Weight);
	}
};

class AvatarMask : public NamedObject
{
public:	

	REGISTER_DERIVED_CLASS (AvatarMask, NamedObject)
	DECLARE_OBJECT_SERIALIZE (AvatarMask)
	
	static void InitializeClass();
	static void CleanupClass() {};
	
	AvatarMask(MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Reset();

	int GetBodyPartCount() const;
	bool GetBodyPart(int index);
	void SetBodyPart(int index, bool value);
	mecanim::human::HumanPoseMask GetHumanPoseMask(UserList& dependencies);

	bool HasFeetIK();

	typedef std::vector<TransformMaskElement> ElementList;

	void SetTransformCount(int count);
	int GetTransformCount() const;

	string  GetTransformPath(int index);
	void   SetTransformPath(int index, string const& path);
	float GetTransformWeight(int index);
	void  SetTransformWeight(int index, float weight);

	mecanim::skeleton::SkeletonMask* GetSkeletonMask(UserList& dependencies, mecanim::memory::Allocator& alloc);

	ElementList m_Elements;

protected:

	bool ValidateBodyPartIndex(int index) const ;
	bool ValidateTransformIndex(int index) const ;

	std::vector<UInt32> m_Mask;
	mutable UserList m_UserList;
};

#endif
