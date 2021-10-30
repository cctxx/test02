#ifndef AVATARBUILDER_H
#define AVATARBUILDER_H

#include "Runtime/BaseClasses/NamedObject.h"

#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/mecanim/types.h"

#include "Runtime/Animation/MecanimUtility.h"

#include "Runtime/Serialize/SerializeTraits.h"

#include <vector>

namespace Unity { class GameObject; }
class Avatar;
class Transform;

namespace mecanim
{
	namespace skeleton
	{
		struct SkeletonPose;
		struct Skeleton;
	}

	namespace animation
	{
		struct AvatarConstant;
	}
}

struct SkeletonBone
{
	DEFINE_GET_TYPESTRING(SkeletonBone)

	SkeletonBone()
		:m_Position(Vector3f::zero), 
		m_Rotation(Quaternionf::identity()),
		m_Scale(Vector3f::one),
		m_TransformModified(false)
	{
	}

	UnityStr	m_Name;
	Vector3f	m_Position;
	Quaternionf m_Rotation;
	Vector3f	m_Scale;
	bool		m_TransformModified;

	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{
		TRANSFER(m_Name);		
		TRANSFER(m_Position);
		TRANSFER(m_Rotation);
		TRANSFER(m_Scale);
		TRANSFER(m_TransformModified);
		transfer.Align();
	}
};

struct SkeletonBoneLimit
{
	DEFINE_GET_TYPESTRING(SkeletonBoneLimit)

	SkeletonBoneLimit()
		:m_Min(Vector3f::zero), 
		m_Max(Vector3f::zero),
		m_Value(Vector3f::zero),
		m_Length(0.f),
		m_Modified(false)
	{
	}

	Vector3f	m_Min;
	Vector3f	m_Max;
	Vector3f	m_Value;
	float		m_Length;
	bool		m_Modified;
	
	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{
		TRANSFER(m_Min);		
		TRANSFER(m_Max);
		TRANSFER(m_Value);
		TRANSFER(m_Length);
		TRANSFER(m_Modified);
		transfer.Align();
	}
};


struct HumanBone
{
	DEFINE_GET_TYPESTRING(HumanBone)

	HumanBone();
	HumanBone(std::string const& humanName);

	UnityStr			m_BoneName;
	UnityStr			m_HumanName;
	SkeletonBoneLimit   m_Limit;
	//Vector3f			m_ColliderPosition;
	//Quaternionf			m_ColliderRotation;
	//Vector3f			m_ColliderScale;
	
	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{
		TRANSFER(m_BoneName);
		TRANSFER(m_HumanName);
		TRANSFER(m_Limit);

		//TRANSFER(m_ColliderPosition);
		//TRANSFER(m_ColliderRotation);
		//TRANSFER(m_ColliderScale);
	}
};

class FindHumanBone
{
protected:
	UnityStr  mName;
public:
	FindHumanBone(UnityStr const& name):mName(name){}
	bool operator()(HumanBone const& bone){ return mName == bone.m_HumanName;}
};

class FindBoneName
{
protected:
	UnityStr  mName;
public:
	FindBoneName(UnityStr const& name):mName(name){}
	bool operator()(HumanBone const& bone){ return mName == bone.m_BoneName;}
};

typedef std::vector<HumanBone> HumanBoneList;
typedef std::vector<SkeletonBone> SkeletonBoneList;

struct HumanDescription
{
	HumanBoneList m_Human;
	SkeletonBoneList m_Skeleton;

	float	m_ArmTwist;
	float	m_ForeArmTwist;
	float	m_UpperLegTwist;
	float	m_LegTwist;

	float	m_ArmStretch;
	float	m_LegStretch;

	float	m_FeetSpacing;

	UnityStr	m_RootMotionBoneName;

	void Reset()
	{
		m_Human.clear();
		m_Skeleton.clear();

		m_ArmTwist = 0.5f;
		m_ForeArmTwist = 0.5f;
		m_UpperLegTwist = 0.5f;
		m_LegTwist = 0.5f;
		m_ArmStretch = 0.05f;
		m_LegStretch = 0.05f;

		m_FeetSpacing = 0.0f;

		m_RootMotionBoneName = "";
	}

	DEFINE_GET_TYPESTRING(HumanDescription)
	
	template<class TransferFunction>
	inline void Transfer (TransferFunction& transfer)
	{
		TRANSFER(m_Human);
		TRANSFER(m_Skeleton);

		TRANSFER(m_ArmTwist);
		TRANSFER(m_ForeArmTwist);
		TRANSFER(m_UpperLegTwist);
		TRANSFER(m_LegTwist);
		TRANSFER(m_ArmStretch);
		TRANSFER(m_LegStretch);
		TRANSFER(m_FeetSpacing);
		TRANSFER(m_RootMotionBoneName);
	}
};

// The type of Avatar to create
enum AvatarType
{
	kGeneric  = 2,
	kHumanoid = 3
};

class AvatarBuilder
{
public:
	struct Options
	{
		Options():avatarType(kGeneric), readTransform(false), useMask(false){}

		AvatarType avatarType;
		bool readTransform;
		bool useMask;
	};

	struct NamedTransform
	{
		UnityStr    name;
		UnityStr    path;
		Transform*  transform;
	};

	typedef std::vector<NamedTransform>	NamedTransforms;

	// readTransform is mainly used by importer. It does read the Default pose from the first frame pose found in the file.
	// useMask is mainly used for API call, when suer want to select only a sub part of a hierarchy.
	static std::string	BuildAvatar(Avatar& avatar, const Unity::GameObject& go, bool doOptimizeTransformHierarchy, const HumanDescription& humanDescription, Options options = Options() );	

	static bool			IsValidHuman(HumanDescription const& humanDescription, NamedTransforms const& namedTransform, std::string& error);
	static bool			IsValidHumanDescription(HumanDescription const& humanDescription, std::string& error);

	static std::string	GenerateAvatarMap(Unity::GameObject const& go, NamedTransforms& namedTransform, const HumanDescription& humanDescription, bool doOptimizeTransformHierarchy, AvatarType avatarType, bool useMask = false);

	static void			GetAllChildren(Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask = std::vector<UnityStr>() );
	static void			GetAllParent(Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask = std::vector<UnityStr>(), bool includeSelf = false );

	static void			ReadFromLocalTransformToSkeletonPose(mecanim::skeleton::SkeletonPose* pose, NamedTransforms const& namedTransform);
	
	static bool			TPoseMatch(mecanim::animation::AvatarConstant const& avatar, NamedTransforms const& namedTransform, std::string& warning);
protected:		

	static void GetAllChildren (Transform& node, UnityStr& path, NamedTransforms& transforms, std::vector<UnityStr> const& mask);
	static void	GetAllParent(Transform& root, Transform& node, NamedTransforms& transforms, std::vector<UnityStr> const& mask = std::vector<UnityStr>(), bool includeSelf = false );

	static Transform* GetTransform(int id, HumanDescription const& humanDescription, NamedTransforms const& namedTransform, std::vector<string> const& boneName);	

	
	static Transform*	GetHipsNode(const HumanDescription& humanDescription, NamedTransforms const& transforms);
	static Transform*	GetRootMotionNode(const HumanDescription& humanDescription, NamedTransforms const& transforms);
	static bool			RemoveAllNoneHumanLeaf(NamedTransforms& namedTransform, HumanDescription const& humanDescription);
};




#endif
