#ifndef AVATAR_H
#define AVATAR_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Misc/UserList.h"
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/mecanim/animation/avatar.h"

#include "Runtime/Animation/MecanimUtility.h"

namespace mecanim {  namespace animation { struct AvatarConstant; } }

enum HumanParameter
{
	UpperArmTwist = 0,
	LowerArmTwist,
	UpperLegTwist,
	LowerLegTwsit,
	ArmStretch,
	LegStretch,
	FeetSpacing
};

class Avatar : public NamedObject
{
public:	
	REGISTER_DERIVED_CLASS (Avatar, NamedObject)
	DECLARE_OBJECT_SERIALIZE (Avatar)
	
	static void InitializeClass (){};
	static void CleanupClass () {}
	
	Avatar (MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency ();		

	void SetAsset (mecanim::animation::AvatarConstant* avatarConstant, TOSVector const& tos);

	mecanim::animation::AvatarConstant*	GetAsset();
	const mecanim::animation::AvatarConstant*	GetAsset() const;
	TOSVector const&					GetTOS() const;

	bool	IsValid()const;


	void	SetMuscleMinMax(int muscleId, float min, float max);
	void	SetParameter(int parameterId, float value);	

	void	NotifyObjectUsers(const MessageIdentifier& msg);
	void	AddObjectUser( UserListNode& node ) { m_ObjectUsers.AddUser(node); }
	
	bool	IsHuman() const;
	bool	HasRootMotion() const;
	float	GetHumanScale() const;
	float	GetLeftFeetBottomHeight() const;
	float	GetRightFeetBottomHeight() const;

	float		GetAxisLength(int humanId)const;
	Quaternionf GetPreRotation(int humanId)const;
	Quaternionf GetPostRotation(int humanId)const;
	Quaternionf GetZYPostQ(int index, Quaternionf const& parentQ, Quaternionf const& q)const;
	Quaternionf GetZYRoll(int index, Vector3f const& v)const;
	Vector3f    GetLimitSign(int index)const;

protected:
	
	mecanim::memory::ChainedAllocator   m_Allocator;
	mecanim::animation::AvatarConstant* m_Avatar;
	TOSVector							m_TOS;

	UInt32							m_AvatarSize;
	
	UserList                                m_ObjectUsers; 
};

class HumanTrait
{
public:
	enum {
		LastDoF = mecanim::human::kLastDoF,
		LastLeftFingerDoF = LastDoF + mecanim::hand::s_DoFCount,
		LastRightFingerDoF = LastLeftFingerDoF + mecanim::hand::s_DoFCount,
		MuscleCount = LastRightFingerDoF
	};

	enum {
		LastBone = mecanim::human::kLastBone,
		LastLeftFingerBone = LastBone + mecanim::hand::s_BoneCount,
		LastRightFingerBone = LastLeftFingerBone + mecanim::hand::s_BoneCount,
		BoneCount = LastRightFingerBone
	};

	static std::string GetFingerMuscleName(int index, bool left);
	static std::string GetFingerName(int index, bool left);

	class Body
	{
	public:
		static int GetBoneCount();
		static std::string GetBoneName(int index);
		static int GetMuscleCount();
		static std::string GetMuscleName(int index);		
	};

	class LeftFinger
	{
	public:
		static int GetBoneCount();
		static std::string GetBoneName(int index);
		static int GetMuscleCount();
		static std::string GetMuscleName(int index);		
		static bool IsLeftHand();
	};

	class RightFinger
	{
	public:
		static int GetBoneCount();
		static std::string GetBoneName(int index);
		static int GetMuscleCount();
		static std::string GetMuscleName(int index);	
		static bool IsLeftHand();
	};	

	static std::vector<string> GetMuscleName();
	static std::vector<string> GetBoneName();
	static int MuscleFromBone(int i, int dofIndex);
	static int BoneFromMuscle(int i);
	static int GetBoneId(Avatar const& avatar, int humanId);
	static bool RequiredBone(int humanId);
	static int RequiredBoneCount();
	static bool HasCollider(Avatar& avatar, int humanId);
	static int GetColliderId(Avatar& avatar, int humanId);

	static int GetParent(int humanId);

	static float GetMuscleDefaultMin(int i);
	static float GetMuscleDefaultMax(int i);
	
protected:
	static std::vector<string> InternalGetMuscleName();
	static std::vector<string> InternalGetBoneName();	
};


#endif

