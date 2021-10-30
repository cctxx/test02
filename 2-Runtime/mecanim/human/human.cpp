#include "UnityPrefix.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/human/human.h"

namespace mecanim
{

// anonymous namespace to hide data in local file scope
namespace
{
	using namespace human;

	static const int32_t Bone2DoF[kLastBone][3] =
	{
		{ -1, -1, -1 },																											// kHips
		{ kLeftLegDoFStart+kUpperLegFrontBack, kLeftLegDoFStart+kUpperLegInOut, kLeftLegDoFStart+kUpperLegRollInOut },			// kLeftUpperLeg
		{ kRightLegDoFStart+kUpperLegFrontBack, kRightLegDoFStart+kUpperLegInOut, kRightLegDoFStart+kUpperLegRollInOut },		// kRightUpperLeg
		{ kLeftLegDoFStart+kLegCloseOpen, -1, kLeftLegDoFStart+kLegRollInOut },													// kLeftLeg
		{ kRightLegDoFStart+kLegCloseOpen, -1, kRightLegDoFStart+kLegRollInOut },												// kRightLeg
		{ kLeftLegDoFStart+kFootCloseOpen, kLeftLegDoFStart+kFootInOut, -1 },													// kLeftFoot
		{ kRightLegDoFStart+kFootCloseOpen, kRightLegDoFStart+kFootInOut, -1 },													// kRightFoot
		{ kBodyDoFStart+kSpineFrontBack, kBodyDoFStart+kSpineLeftRight, kBodyDoFStart+kSpineRollLeftRight },					// kSpine
		{ kBodyDoFStart+kChestFrontBack, kBodyDoFStart+kChestLeftRight, kBodyDoFStart+kChestRollLeftRight },					// kChest
		{ kHeadDoFStart+kNeckFrontBack, kHeadDoFStart+kNeckLeftRight, kHeadDoFStart+kNeckRollLeftRight },						// kNeck
		{ kHeadDoFStart+kHeadFrontBack, kHeadDoFStart+kHeadLeftRight, kHeadDoFStart+kHeadRollLeftRight },						// kHead
		{ kLeftArmDoFStart+kShoulderDownUp, kLeftArmDoFStart+kShoulderFrontBack, -1 },											// kLeftShoulder
		{ kRightArmDoFStart+kShoulderDownUp, kRightArmDoFStart+kShoulderFrontBack, -1 },										// kRightShoulder
		{ kLeftArmDoFStart+kArmDownUp, kLeftArmDoFStart+kArmFrontBack, kLeftArmDoFStart+kArmRollInOut },						// kLeftArm
		{ kRightArmDoFStart+kArmDownUp, kRightArmDoFStart+kArmFrontBack, kRightArmDoFStart+kArmRollInOut },						// kRightArm
		{ kLeftArmDoFStart+kForeArmCloseOpen, -1, kLeftArmDoFStart+kForeArmRollInOut },											// kLeftForeArm
		{ kRightArmDoFStart+kForeArmCloseOpen, -1, kRightArmDoFStart+kForeArmRollInOut },										// kRightForeArm
		{ kLeftArmDoFStart+kHandDownUp,kLeftArmDoFStart+kHandInOut, -1 },														// kLeftHand
		{ kRightArmDoFStart+kHandDownUp,kRightArmDoFStart+kHandInOut, -1 },														// kRightHand
		{ kLeftLegDoFStart+kToesUpDown, -1, -1},																				// kLeftToes
		{ kRightLegDoFStart+kToesUpDown, -1, -1 },																				// kRightToes
		{ kHeadDoFStart+kLeftEyeDownUp, kHeadDoFStart+kLeftEyeLeftRight,-1 },													// LeftEye
		{ kHeadDoFStart+kRightEyeDownUp, kHeadDoFStart+kRightEyeLeftRight,-1 },													// RightEye
		{ kHeadDoFStart+kJawDownUp, kHeadDoFStart+kJawLeftRight,-1 }															// Jaw
	};

	static const float HumanBoneDefaultMass[kLastBone] = 
	{
		12.0f,		// kHips
		10.0f,		// kLeftUpperLeg
		10.0f,		// kRightUpperLeg
		4.0f,		// kLeftLowerLeg
		4.0f,		// kRightLowerLeg
		0.8f,		// kLeftFoot
		0.8f,		// kRightFoot
		2.5f,		// kSpine
		24.0f,		// kChest
		1.0f,		// kNeck
		4.0f,		// kHead
		0.5f,		// kLeftShoulder
		0.5f,	    // kRightShoulder
		2.0f,		// kLeftUpperArm
		2.0f,		// kRightUpperArm
		1.5f,		// kLeftLowerArm
		1.5f,		// kRightLowerArm
		0.5f,		// kLeftHand
		0.5f,		// kRightHand
		0.2f,		// kLeftToes
		0.2f,		// kRightToes
		0.0f,		// LeftEye
		0.0f,		// RightEye
		0.0f		// Jaw
	};	

	static const int32_t BoneMirror[kLastBone] =
	{
		kHips,				// kHips
		kRightUpperLeg,		// kLeftUpperLeg
		-kLeftUpperLeg,		// kRightUpperLeg
		kRightLowerLeg,			// kLeftLowerLeg
		-kLeftLowerLeg,			// kRightLowerLeg
		kRightFoot,			// kLeftFoot
		-kLeftFoot,			// kRightFoot
		kSpine,				// kSpine
		kChest,				// kChest
		kNeck,				// kNeck
		kHead,				// kHead
		kRightShoulder,		// kLeftShoulder
		-kLeftShoulder,	    // kRightShoulder
		kRightUpperArm,			// kLeftUpperArm
		-kLeftUpperArm,			// kRightUpperArm
		kRightLowerArm,		// kLeftLowerArm
		-kLeftLowerArm,		// kRightLowerArm
		kRightHand,			// kLeftHand
		-kLeftHand,			// kRightHand
		kRightToes,			// kLeftToes
		-kLeftToes,			// kRightToes
		kRightEye,			// kLeftEye
		-kLeftEye,			// kRightEye
		kJaw,				// kJaw
	};

	static const float BodyDoFMirror[kLastBodyDoF] =
	{
		+1.0f,	// kSpineFrontBack = 0,
		-1.0f,	// kSpineLeftRight,
		-1.0f,	// kSpineRollLeftRight,
		+1.0f,	// kChestFrontBack,
		-1.0f,	// kChestLeftRight,
		-1.0f	// kChestRollLeftRight,
	};

	static const float HeadDoFMirror[kLastHeadDoF] =
	{
		+1.0f,	// kNeckFrontBack = 0,
		-1.0f,	// kNeckLeftRight,
		-1.0f,	// kNeckRollLeftRight,
		+1.0f,	// kHeadFrontBack,
		-1.0f,	// kHeadLeftRight,
		-1.0f,	// kHeadRollLeftRight,
		+1.0f,	// kLeftEyeDownUp,
		-1.0f,	// kLeftEyeLeftRight,
		+1.0f,	// kRightEyeDownUp,
		-1.0f,	// kRightEyeLeftRight,
		+1.0f,	// kJawDownUp,
		-1.0f	// kJawLeftRight,
	};

	static const int32_t BoneChildren[kLastBone][4] =
	{
		{ 3,kLeftUpperLeg, kRightUpperLeg, kSpine },// kHips
		{ 1,kLeftLowerLeg },								// kLeftUpperLeg
		{ 1,kRightLowerLeg },							// kRightUpperLeg
		{ 1,kLeftFoot },							// kLeftLowerLeg
		{ 1,kRightFoot },							// kRightLowerLeg
		{ 1,kLeftToes },							// kLeftFoot
		{ 1,kRightToes },							// kRightFoot
		{ 1,kChest },								// kSpine
		{ 3,kNeck, kLeftShoulder, kRightShoulder },	// kChest
		{ 1,kHead },								// kNeck
		{ 3,kLeftEye,kRightEye,kJaw },				// kHead
		{ 1,kLeftUpperArm },								// kLeftShoulder
		{ 1,kRightUpperArm },	    					// kRightShoulder
		{ 1,kLeftLowerArm },							// kLeftUpperArm
		{ 1,kRightLowerArm },						// kRightUpperArm
		{ 1,kLeftHand },							// kLeftLowerArm
		{ 1,kRightHand },							// kRightLowerArm
		{ 0 },										// kLeftHand
		{ 0 },										// kRightHand
		{ 0 },										// kLeftToes
		{ 0 },										// kRightToes	
		{ 0 },										// kLeftEye
		{ 0 },										// kRightEye
		{ 0 }										// kJaw	
	};

	static const int32_t DoF2Bone[human::kLastDoF] = {
		kSpine,
		kSpine,
		kSpine,
		kChest,
		kChest,
		kChest,
		kNeck,
		kNeck,
		kNeck,
		kHead,
		kHead,
		kHead,
		kLeftEye,
		kLeftEye,
		kRightEye,
		kRightEye,
		kJaw,
		kJaw,
		kLeftUpperLeg,
		kLeftUpperLeg,
		kLeftUpperLeg,
		kLeftLowerLeg,
		kLeftLowerLeg,
		kLeftFoot,
		kLeftFoot,
		kLeftToes,
		kRightUpperLeg,
		kRightUpperLeg,
		kRightUpperLeg,
		kRightLowerLeg,
		kRightLowerLeg,
		kRightFoot,
		kRightFoot,
		kRightToes,
		kLeftShoulder,
		kLeftShoulder,
		kLeftUpperArm,
		kLeftUpperArm,
		kLeftUpperArm,
		kLeftLowerArm,
		kLeftLowerArm,
		kLeftHand,
		kLeftHand,
		kRightShoulder,
		kRightShoulder,
		kRightUpperArm,
		kRightUpperArm,
		kRightUpperArm,
		kRightLowerArm,
		kRightLowerArm,
		kRightHand,
		kRightHand
	};
    
	static const int32_t DoF2BoneDoFIndex[human::kLastDoF] = {
		2, // kSpine,
		1, // kSpine,
		0, // kSpine,
		2, // kChest,
		1, // kChest,
		0, // kChest,
		2, // kNeck,
		1, // kNeck,
		0, // kNeck,
		2, // kHead,
		1, // kHead,
		0, // kHead,
		2, // kLeftEye,
		1, // kLeftEye,
		2, // kRightEye,
		1, // kRightEye,
		2, // kJaw,
		1, // kJaw,
		2, // kLeftUpperLeg,
		1, // kLeftUpperLeg,
		0, // kLeftUpperLeg,
		2, // kLeftLowerLeg,
		0, // kLeftLowerLeg,
		2, // kLeftFoot,
		1, // kLeftFoot,
		2, // kLeftToes,
		2, // kRightUpperLeg,
		1, // kRightUpperLeg,
		0, // kRightUpperLeg,
		2, // kRightLowerLeg,
		0, // kRightLowerLeg,
		2, // kRightFoot,
		1, // kRightFoot,
		2, // kRightToes,
		2, // kLeftShoulder,
		1, // kLeftShoulder,
		2, // kLeftUpperArm,
		1, // kLeftUpperArm,
		0, // kLeftUpperArm,
		2, // kLeftLowerArm,
		0, // kLeftLowerArm,
		2, // kLeftHand,
		1, // kLeftHand,
		2, // kRightShoulder,
		1, // kRightShoulder,
		2, // kRightUpperArm,
		1, // kRightUpperArm,
		0, // kRightUpperArm,
		2, // kRightLowerArm,
		0, // kRightLowerArm,
		2, // kRightHand,
		1, // kRightHand
	};

	const static float ATTRIBUTE_ALIGN(ALIGN4F) goalOrientationOffsetArray[kLastGoal][4] = {{0.5f,-0.5f,0.5f,0.5f},{0.5f,-0.5f,0.5f,0.5f},{0.707107f,0,0.707107f,0},{0,0.707107f,0,0.707107f}};
}

namespace human
{	
	bool RequiredBone(uint32_t aBoneIndex)
	{
		static bool requiredBone[kLastBone] = {
			true, 	//kHips
			true, 	//kLeftUpperLeg
			true, 	//kRightUpperLeg
			true, 	//kLeftLowerLeg
			true, 	//kRightLowerLeg
			true, 	//kLeftFoot
			true, 	//kRightFoot
			true, 	//kSpine
			false, 	//kChest
			false, 	//kNeck
			true, 	//kHead
			false, 	//kLeftShoulder
			false, 	//kRightShoulder
			true, 	//kLeftUpperArm
			true, 	//kRightUpperArm
			true, 	//kLeftLowerArm
			true, 	//kRightLowerArm
			true, 	//kLeftHand
			true, 	//kRightHand
			false,	//kLeftToes
			false,	//kRightToes
			false,	//kLeftEye,
			false,	//kRightEye,
			false   //kJaw,
		};

		return requiredBone[aBoneIndex];
	}

	const char* BoneName(uint32_t aBoneIndex)
	{
		static const char* boneName[kLastBone] = {
			"Hips",
			"LeftUpperLeg",
			"RightUpperLeg",
			"LeftLowerLeg",
			"RightLowerLeg",
			"LeftFoot",
			"RightFoot",
			"Spine",
			"Chest",
			"Neck",
			"Head",
			"LeftShoulder",
			"RightShoulder",
			"LeftUpperArm",
			"RightUpperArm",
			"LeftLowerArm",
			"RightLowerArm",
			"LeftHand",
			"RightHand",
			"LeftToes",
			"RightToes",
			"LeftEye",
			"RightEye",
			"Jaw"
		};

		return boneName[aBoneIndex];
	}

	const char* MuscleName(uint32_t aBoneIndex)
	{
		static const char* muscleName[human::kLastDoF] = {
			
			"Spine Front-Back",
			"Spine Left-Right",
			"Spine Twist Left-Right",
			"Chest Front-Back",
			"Chest Left-Right",
			"Chest Twist Left-Right",
			
			"Neck Nod Down-Up",
			"Neck Tilt Left-Right",
			"Neck Turn Left-Right",
			"Head Nod Down-Up",
			"Head Tilt Left-Right",
			"Head Turn Left-Right",
			
			"Left Eye Down-Up",
			"Left Eye In-Out",
			"Right Eye Down-Up",
			"Right Eye In-Out",
			
			"Jaw Close",
			"Jaw Left-Right",

			"Left Upper Leg Front-Back",
			"Left Upper Leg In-Out",
			"Left Upper Leg Twist In-Out",
			"Left Lower Leg Stretch",
			"Left Lower Leg Twist In-Out",
			"Left Foot Up-Down",
			"Left Foot Twist In-Out",
			"Left Toes Up-Down",

			"Right Upper Leg Front-Back",
			"Right Upper Leg In-Out",
			"Right Upper Leg Twist In-Out",
			"Right Lower Leg Stretch",
			"Right Lower Leg Twist In-Out",
			"Right Foot Up-Down",
			"Right Foot Twist In-Out",
			"Right Toes Up-Down",

			"Left Shoulder Down-Up",
			"Left Shoulder Front-Back",
			"Left Arm Down-Up",
			"Left Arm Front-Back",
			"Left Arm Twist In-Out",
			"Left Forearm Stretch",
			"Left Forearm Twist In-Out",
			"Left Hand Down-Up",
			"Left Hand In-Out",

			"Right Shoulder Down-Up",
			"Right Shoulder Front-Back",
			"Right Arm Down-Up",
			"Right Arm Front-Back",
			"Right Arm Twist In-Out",
			"Right Forearm Stretch",
			"Right Forearm Twist In-Out",
			"Right Hand Down-Up",
			"Right Hand In-Out"
		};

		return muscleName[aBoneIndex];
	}

	bool MaskHasLegs(const HumanPoseMask& mask)
	{
		for(int dofIter = 0; dofIter < mecanim::human::kLastLegDoF; dofIter++)
		{
			if(!mask.test(mecanim::human::kMaskDoFStartIndex+mecanim::human::kLeftLegDoFStart+dofIter))
				return false;
			if(!mask.test(mecanim::human::kMaskDoFStartIndex+mecanim::human::kRightLegDoFStart+dofIter))
				return false;
		}

		return true;
	}

	int32_t MuscleFromBone(int32_t aBoneIndex, int32_t aDoFIndex)
	{
		return Bone2DoF[aBoneIndex][2-aDoFIndex];
	}

	int32_t BoneFromMuscle(int32_t aDoFIndex)
	{
		return DoF2Bone[aDoFIndex];
	}

	HumanPoseMask FullBodyMask()
	{
		return HumanPoseMask(~HumanPoseMask::type(0));
	}	

	skeleton::SetupAxesInfo const& GetAxeInfo(uint32_t index)
	{
		const static skeleton::SetupAxesInfo setupAxesInfoArray[kLastBone] =
		{
			{{0,0,0,1},{-1,0,0,0},{-40,-40,-40},{40,40,40},{1,1,1,1},math::kZYRoll,0},						// kHips,
			{{-0.268f,0,0,1},{1,0,0,0},{-60,-60,-90},{60,60,50},{1,1,1,1},math::kZYRoll,0},					// kLeftUpperLeg,
			{{-0.268f,0,0,1},{1,0,0,0},{-60,-60,-90},{60,60,50},{-1,-1,1,1},math::kZYRoll,0},				// kRightUpperLeg,
			{{0.839f,0,0,1},{1,0,0,0},{-90,0,-80},{90,0,80},{1,1,-1,1},math::kZYRoll,0},					// kLeftLeg,
			{{0.839f,0,0,1},{1,0,0,0},{-90,0,-80},{90,0,80},{-1,1,-1,1},math::kZYRoll,0},					// kRightLeg,
			{{0,0,0,1},{1,0,0,0},{0,-30,-50},{0,30,50},{1,1,1,1},math::kZYRoll,-2},							// kLeftFoot,
			{{0,0,0,1},{1,0,0,0},{0,-30,-50},{0,30,50},{1,-1,1,1},math::kZYRoll,-2},						// kRightFoot,
			{{0,0,0,1},{-1,0,0,0},{-40,-40,-40},{40,40,40},{1,1,1,1},math::kZYRoll,0},						// kSpine,
			{{0,0,0,1},{-1,0,0,0},{-40,-40,-40},{40,40,40},{1,1,1,1},math::kZYRoll,0},						// kChest,
			{{0,0,0,1},{-1,0,0,0},{-40,-40,-40},{40,40,40},{1,1,1,1},math::kZYRoll,0},						// kNeck,
			{{0,0,0,1},{-1,0,0,0},{-40,-40,-40},{40,40,40},{1,1,1,1},math::kZYRoll,2},						// kHead,
			{{0,0,0,1},{0,0,1,0},{0,-15,-15},{0,15,30},{1,1,-1,1},math::kZYRoll,0},							// kLeftShoulder,
			{{0,0,0,1},{0,0,1,0},{0,-15,-15},{0,15,30},{1,1,1,1},math::kZYRoll,0},							// kRightShoulder,
			{{0,0.268f,0.364f,1},{0,0,1,0},{-90,-100,-60},{90,100,100},{1,1,-1,1},math::kZYRoll,0},			// kLeftArm,
			{{0,-0.268f,-0.364f,1},{0,0,1,0},{-90,-100,-60},{90,100,100},{-1,1,1,1},math::kZYRoll,0},		// kRightArm,
			{{0,0.839f,0,1},{0,1,0,0},{-90,0,-80},{90,0,80},{1,1,-1,1},math::kZYRoll,0},					// kLeftForeArm,
			{{0,-0.839f,0,1},{0,1,0,0},{-90,0,-80},{90,0,80},{-1,1,1,1},math::kZYRoll,0},					// kRightForeArm,
			{{0,0,0,1},{0,0,1,0},{0,-40,-80},{0,40,80},{1,1,-1,1},math::kZYRoll,0},							// kLeftHand,
			{{0,0,0,1},{0,0,1,0},{0,-40,-80},{0,40,80},{1,1,1,1},math::kZYRoll,0},							// kRightHand,
			{{0,0,0,1},{1,0,0,0},{0,0,-50},{0,0,50},{1,1,1,1},math::kZYRoll,3},								// kLeftToes,
			{{0,0,0,1},{1,0,0,0},{0,0,-50},{0,0,50},{1,1,1,1},math::kZYRoll,3},								// kRightToes,
			{{0,0,0,1},{1,0,0,0},{0,-20,-10},{0,20,15},{1,1,-1,1},math::kZYRoll,3},							// kLeftEye,
			{{0,0,0,1},{1,0,0,0},{0,-20,-10},{0,20,15},{1,-1,-1,1},math::kZYRoll,3},						// kRightEye,
			{{0.09f,0,0,1},{1,0,0,0},{0,-10,-10},{0,10,10},{1,1,-1,1},math::kZYRoll,3}						// kJaw,
		};

		return setupAxesInfoArray[index];

	}

    
	Human::Human() :	m_HandlesCount(0),
						m_HasLeftHand(false), 
						m_HasRightHand(false), 
						m_ColliderCount(0),
						m_Scale(1),
						m_RootX(math::xformIdentity()),
						m_ArmTwist(0.5f),
						m_ForeArmTwist(0.5f),
						m_UpperLegTwist(0.5f),
						m_LegTwist(0.5f),
						m_ArmStretch(0.05f),
						m_LegStretch(0.05f),
						m_FeetSpacing(0.0f)
	{ 
		int32_t i;
		
		float mass = 0;

		for(i = 0; i < kLastBone; i++) 
		{
			m_HumanBoneIndex[i] = -1;
			m_HumanBoneMass[i] = HumanBoneDefaultMass[i];
			mass += m_HumanBoneMass[i];
			m_ColliderIndex[i] = -1;
		}

		for(i = 0; i < kLastBone; i++) 
		{	
			m_HumanBoneMass[i] /= mass;
		}
	}

	Human* CreateHuman(skeleton::Skeleton *apSkeleton, skeleton::SkeletonPose *apSkeletonPose, uint32_t aHandlesCount, uint32_t aColliderCount, memory::Allocator& arAlloc)
	{
		Human* human = arAlloc.Construct<Human>();

		human->m_Skeleton = apSkeleton;
		human->m_SkeletonPose = apSkeletonPose;
		human->m_Handles = arAlloc.ConstructArray<Handle>(aHandlesCount);
		human->m_HandlesCount = aHandlesCount;
		human->m_ColliderArray = arAlloc.ConstructArray<math::Collider>(aColliderCount);
		human->m_ColliderCount = aColliderCount;

		memset(human->m_HumanBoneIndex, -1, sizeof(int32_t)*kLastBone);
		memset(human->m_ColliderIndex, -1, sizeof(int32_t)*kLastBone);

		human->m_HasLeftHand = false;
		human->m_HasRightHand = false;

		human->m_Scale = 1;

		return human;
	}

	void DestroyHuman(Human *apHuman, memory::Allocator& arAlloc)
	{
		if(apHuman)
		{
			arAlloc.Deallocate(apHuman->m_Handles);
			arAlloc.Deallocate(apHuman->m_ColliderArray);

			arAlloc.Deallocate(apHuman);
		}
	}

	HumanPose::HumanPose()
	{
		int32_t i;

		for(i = 0; i < kLastDoF; i++)
		{
			m_DoFArray[i] = 0;
		}

		m_LookAtPosition = math::float4::zero();
		m_LookAtWeight = math::float4::zero();
	}

	void HumanAdjustMass(Human *apHuman)
	{
		if(apHuman->m_HumanBoneIndex[kNeck] < 0) 
		{
			apHuman->m_HumanBoneMass[kChest] += apHuman->m_HumanBoneMass[kNeck];
			apHuman->m_HumanBoneMass[kNeck] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kLeftShoulder] < 0) 
		{
			apHuman->m_HumanBoneMass[kChest] += apHuman->m_HumanBoneMass[kLeftShoulder];
			apHuman->m_HumanBoneMass[kLeftShoulder] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kRightShoulder] < 0) 
		{
			apHuman->m_HumanBoneMass[kChest] += apHuman->m_HumanBoneMass[kRightShoulder];
			apHuman->m_HumanBoneMass[kRightShoulder] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kChest] < 0) 
		{
			apHuman->m_HumanBoneMass[kSpine] += apHuman->m_HumanBoneMass[kChest];
			apHuman->m_HumanBoneMass[kChest] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kLeftToes] < 0) 
		{
			apHuman->m_HumanBoneMass[kLeftFoot] += apHuman->m_HumanBoneMass[kLeftToes];
			apHuman->m_HumanBoneMass[kLeftToes] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kRightToes] < 0) 
		{
			apHuman->m_HumanBoneMass[kRightFoot] += apHuman->m_HumanBoneMass[kRightToes];
			apHuman->m_HumanBoneMass[kRightToes] = 0;
		}
	}

	void HumanSetupAxes(Human *apHuman, skeleton::SkeletonPose const *apSkeletonPoseGlobal)
	{
		apHuman->m_RootX = math::xformIdentity();
		apHuman->m_RootX = HumanComputeRootXform(apHuman,apSkeletonPoseGlobal);
		apHuman->m_Scale = apHuman->m_RootX.t.y().tofloat(); 

		skeleton::SkeletonPoseComputeLocal(apHuman->m_Skeleton.Get(), apSkeletonPoseGlobal, apHuman->m_SkeletonPose.Get());	

		int32_t i;

		for(i = 0; i < kLastBone; i++)
		{
			int32_t skBoneIndex = apHuman->m_HumanBoneIndex[i];

			int32_t skAxisBoneId = -1;
			float len = 1.0f;

			switch(i)
			{
				case kLeftEye:
				case kRightEye:
				case kJaw:
					len = 0.1f;
				break;

				case kHead:
					if(apHuman->m_HumanBoneIndex[kNeck] >= 0)
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kNeck];
						len = -1.0f;
					}
					else if(apHuman->m_HumanBoneIndex[kChest] >= 0)
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kChest];
						len = -0.5f;
					}
					else
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kSpine];
						len = -0.25f;
					}
					break;

				case kLeftFoot:
					len = -apSkeletonPoseGlobal->m_X[skBoneIndex].t.y().tofloat();
					break;

				case kRightFoot:
					len = -apSkeletonPoseGlobal->m_X[skBoneIndex].t.y().tofloat();
					break;

				case kLeftHand:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftLowerArm];
					len = -0.5f;
					break;

				case kRightHand:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightLowerArm];
					len = -0.5f;
					break;

				case kLeftToes:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftFoot];
					len = 0.5f;
					break;
				
				case kRightToes:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightFoot];
					len = 0.5f;
					break;
				
				case kHips:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kSpine];
				break;

				case kLeftUpperLeg:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftLowerLeg];
				break;

				case kRightUpperLeg:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightLowerLeg];
				break;

				case kLeftLowerLeg:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftFoot];
				break;

				case kRightLowerLeg:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightFoot];
				break;

				case kSpine:
					if(apHuman->m_HumanBoneIndex[kChest] >= 0)
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kChest];
					}
					else if(apHuman->m_HumanBoneIndex[kNeck] >= 0)
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kNeck];
					}
					else
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kHead];
					}
				break;

				case kChest:
					if(apHuman->m_HumanBoneIndex[kNeck] >= 0)
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kNeck];
					}
					else
					{
						skAxisBoneId = apHuman->m_HumanBoneIndex[kHead];
					}
				break;

				case kNeck:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kHead];
				break;

				case kLeftShoulder:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftUpperArm];
				break;

				case kRightShoulder:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightUpperArm];
				break;

				case kLeftUpperArm:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftLowerArm];
				break;

				case kRightUpperArm:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightLowerArm];
				break;

				case kLeftLowerArm:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kLeftHand];
				break;

				case kRightLowerArm:
					skAxisBoneId = apHuman->m_HumanBoneIndex[kRightHand];
				break;
			};
			
			if(skBoneIndex >= 0)
			{
				skeleton::SetupAxes(apHuman->m_Skeleton.Get(), apSkeletonPoseGlobal, GetAxeInfo(i), skBoneIndex,skAxisBoneId,true,len);
			}
		}
	}

	void HumanSetupCollider(Human *apHuman, skeleton::SkeletonPose const *apSkeletonPoseGlobal)
	{
		//float refLen = apSkeletonPoseGlobal->m_X[apHuman->m_HumanBoneIndex[kHead]].t.y();

		float hipsWidth = math::length(apSkeletonPoseGlobal->m_X[apHuman->m_HumanBoneIndex[kLeftUpperLeg]].t - apSkeletonPoseGlobal->m_X[apHuman->m_HumanBoneIndex[kRightUpperLeg]].t).tofloat();
		float shouldersWidth  = math::length(apSkeletonPoseGlobal->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t - apSkeletonPoseGlobal->m_X[apHuman->m_HumanBoneIndex[kRightUpperArm]].t).tofloat();

		int32_t colliderIndex = 0;
		int32_t boneIndex;
		
		for(boneIndex = 0; boneIndex < kLastBone; boneIndex++)
		{
			int32_t skIndex = apHuman->m_HumanBoneIndex[boneIndex];
		
			if(skIndex >= 0)
			{
				apHuman->m_ColliderIndex[boneIndex] = colliderIndex;
	
				math::Axes axes = GetAxes(apHuman,boneIndex);
				apHuman->m_ColliderArray[colliderIndex].m_X.s.x() = axes.m_Length;
				apHuman->m_ColliderArray[colliderIndex].m_X.t.x() = math::float1(0.5f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();

				if(boneIndex == kHips)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCube;
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(3.0f);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.x() = math::float1::zero();
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(1.5f * hipsWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(1.0f * hipsWidth);

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kIgnored;
				}
				else if(boneIndex == kSpine)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kSphere;
					
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(1.5f);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.x() = math::float1(0.5f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(1.2f * hipsWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.8f * hipsWidth);

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -7.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 7.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 7.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 7.0f;
				}
				else if(boneIndex == kChest)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCube;
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(1.0f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.6f * shouldersWidth);

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -11.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 11.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 11.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 11.0f;
				}
				else if(boneIndex == kNeck)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kSphere;
				
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(1.5f);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.y() = math::float1(0.1f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.33f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.33f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -10.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 10.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 20.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 5.0f;
				}
				else if(boneIndex == kHead)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kSphere;
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.x() = math::float1(0.6f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.y() = math::float1(0.2f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();	
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.4f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.45f * shouldersWidth);

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -5.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 8.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 20.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 10.0f;
				}
				else if(boneIndex == kLeftShoulder || boneIndex == kRightShoulder)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kNone;
				
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(1.2f);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.x() = math::float1(0.5f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.1f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.2f * shouldersWidth);

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kIgnored;
				}
				else if(boneIndex == kLeftHand || boneIndex == kRightHand)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCube;
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.5f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x(); 
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.2f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x(); 

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -30.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 30.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 40.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 25.0f;
				}
				else if(boneIndex == kLeftFoot || boneIndex == kRightFoot)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCube;
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.4f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.85f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.y() = math::float1(-0.25f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.y();

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
					apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -45.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 20.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 30.0f;
					apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 50.0f;
				}
				else if(boneIndex == kLeftToes || boneIndex == kRightToes)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kNone;
				
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.4f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.2f * shouldersWidth);
					apHuman->m_ColliderArray[colliderIndex].m_X.t.y() = math::float1(-0.5f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.y();

					apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kIgnored;
					apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kIgnored;
				}
				else if(	boneIndex == kLeftUpperArm || 
							boneIndex == kLeftLowerArm || 
							boneIndex == kRightUpperArm || 
							boneIndex == kRightLowerArm)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCapsule;
				
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(1.2f);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.25f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.25f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();

					if(boneIndex == kLeftUpperArm || boneIndex == kRightUpperArm)
					{
						apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -100.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 100.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 20.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 45.0f;
					}
					else if (boneIndex == kLeftLowerArm || boneIndex == kRightLowerArm)
					{
						apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLocked;
						apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
						if (boneIndex == kLeftLowerArm)
						{
							apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -130.0f;
							apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 0.0f;
						}
						else if (boneIndex == kRightLowerArm)
						{
							apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = 0.0f;
							apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 130.0f;
						}
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 5.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 20.0f;
					}
				}
				else if(	boneIndex == kLeftLowerLeg ||
							boneIndex == kLeftUpperLeg ||
							boneIndex == kRightLowerLeg ||
							boneIndex == kRightUpperLeg)
				{
					apHuman->m_ColliderArray[colliderIndex].m_Type = math::kCapsule;
				
					//apHuman->m_ColliderArray[colliderIndex].m_X.s.x() *= math::float1(1.2f);
					apHuman->m_ColliderArray[colliderIndex].m_X.s.z() = math::float1(0.175f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();
					apHuman->m_ColliderArray[colliderIndex].m_X.s.y() = math::float1(0.175f) * apHuman->m_ColliderArray[colliderIndex].m_X.s.x();

					if (boneIndex == kLeftLowerLeg || boneIndex == kRightLowerLeg)
					{
						apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLocked;
						apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = 0.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 130.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 0.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 10.0f;
					}
					else if(boneIndex == kLeftUpperLeg || boneIndex == kRightUpperLeg)
					{
						apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kLimited;
						apHuman->m_ColliderArray[colliderIndex].m_MinLimitX = -70.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitX = 10.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitY = 45.0f;
						apHuman->m_ColliderArray[colliderIndex].m_MaxLimitZ = 60.0f;
					}
				}
			else
			{
				apHuman->m_ColliderArray[colliderIndex].m_Type = math::kNone;
				apHuman->m_ColliderArray[colliderIndex].m_XMotionType = math::kIgnored;
				apHuman->m_ColliderArray[colliderIndex].m_YMotionType = math::kIgnored;
				apHuman->m_ColliderArray[colliderIndex].m_ZMotionType = math::kIgnored;
			}

				colliderIndex++;
			}
		}
	}

	void HumanCopyAxes(Human const *apSrcHuman, Human *apHuman)
	{
		int32_t i;

		for(i = 0; i < kLastBone; i++)
		{	
			skeleton::Node const * srcNode = apSrcHuman->m_HumanBoneIndex[i] >= 0 ? &apSrcHuman->m_Skeleton->m_Node[apSrcHuman->m_HumanBoneIndex[i]] : 0;
			skeleton::Node const * node = apHuman->m_HumanBoneIndex[i] >= 0 ? &apHuman->m_Skeleton->m_Node[apHuman->m_HumanBoneIndex[i]] : 0;

			if(srcNode != 0 && node != 0 && srcNode->m_AxesId != -1 && node->m_AxesId != -1)
			{
				apHuman->m_Skeleton->m_AxesArray[node->m_AxesId] = apSrcHuman->m_Skeleton->m_AxesArray[srcNode->m_AxesId];
			}
		}
	}

	math::Axes GetAxes(Human const *apHuman, int32_t aBoneIndex)
	{
		math::Axes ret;

		int32_t skIndex = apHuman->m_HumanBoneIndex[aBoneIndex];

		if(skIndex >= 0)
		{
			int32_t axesIndex = apHuman->m_Skeleton->m_Node[skIndex].m_AxesId;
		
			if(axesIndex >= 0)
			{
				ret = apHuman->m_Skeleton->m_AxesArray[axesIndex];			
			}
		}

		return ret;
	}

	void GetMuscleRange(Human const *apHuman, int32_t aDoFIndex, float &aMin, float &aMax)
	{
		math::Axes axes = GetAxes(apHuman,DoF2Bone[aDoFIndex]);

		switch(DoF2BoneDoFIndex[aDoFIndex])
		{
			case 0: aMin = axes.m_Limit.m_Min.x().tofloat(); aMax = axes.m_Limit.m_Max.x().tofloat(); break;
			case 1: aMin = axes.m_Limit.m_Min.y().tofloat(); aMax = axes.m_Limit.m_Max.y().tofloat(); break;
			case 2: aMin = axes.m_Limit.m_Min.z().tofloat(); aMax = axes.m_Limit.m_Max.z().tofloat(); break;
		}
	}

	math::float4 AddAxis(Human const *apHuman, int32_t aIndex, math::float4 const &arQ)
	{
		math::Axes cAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[aIndex].m_AxesId];			
		return math::normalize(math::quatMul(arQ,cAxes.m_PostQ));
	}

	math::float4 RemoveAxis(Human const *apHuman, int32_t aIndex, const math::float4 &arQ)
	{
		math::Axes cAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[aIndex].m_AxesId];			
		return math::normalize(math::quatMul(arQ,math::quatConj(cAxes.m_PostQ)));
	}

	math::xform NormalizedHandleX(Human const *apHuman, int32_t aHandleIndex)
	{
		int32_t pIndex = apHuman->m_HumanBoneIndex[apHuman->m_Handles[aHandleIndex].m_ParentHumanIndex];

		math::xform px = apHuman->m_SkeletonPose->m_X[pIndex];
		math::xform hx = math::xformMul(px,apHuman->m_Handles[aHandleIndex].m_X);
			
		px.q = AddAxis(apHuman,pIndex,px.q);
		px.s = math::float4::one();

		return math::xformInvMul(px,hx);
	}

	void HumanPoseAdjustForMissingBones(Human const *apHuman, HumanPose *apHumanPose)
	{
		if(apHuman->m_HumanBoneIndex[kNeck] < 0)
		{
			apHumanPose->m_DoFArray[kHeadDoFStart+kHeadFrontBack] += apHumanPose->m_DoFArray[kHeadDoFStart+kNeckFrontBack];
			apHumanPose->m_DoFArray[kHeadDoFStart+kNeckFrontBack] = 0;
			
			apHumanPose->m_DoFArray[kHeadDoFStart+kHeadLeftRight] += apHumanPose->m_DoFArray[kHeadDoFStart+kNeckLeftRight];
			apHumanPose->m_DoFArray[kHeadDoFStart+kNeckLeftRight] = 0;

			apHumanPose->m_DoFArray[kHeadDoFStart+kHeadRollLeftRight] += apHumanPose->m_DoFArray[kHeadDoFStart+kNeckRollLeftRight];
			apHumanPose->m_DoFArray[kHeadDoFStart+kNeckRollLeftRight] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kChest] < 0)
		{
			apHumanPose->m_DoFArray[kBodyDoFStart+kSpineFrontBack] += apHumanPose->m_DoFArray[kBodyDoFStart+kChestFrontBack];
			apHumanPose->m_DoFArray[kBodyDoFStart+kChestFrontBack] = 0;
			
			apHumanPose->m_DoFArray[kBodyDoFStart+kSpineLeftRight] += apHumanPose->m_DoFArray[kBodyDoFStart+kChestLeftRight];
			apHumanPose->m_DoFArray[kBodyDoFStart+kChestLeftRight] = 0;

			apHumanPose->m_DoFArray[kBodyDoFStart+kSpineRollLeftRight] += apHumanPose->m_DoFArray[kBodyDoFStart+kChestRollLeftRight];
			apHumanPose->m_DoFArray[kBodyDoFStart+kChestRollLeftRight] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kLeftShoulder] < 0)
		{
			apHumanPose->m_DoFArray[kLeftArmDoFStart+kArmDownUp] += (30.0f/200.0f) * apHumanPose->m_DoFArray[kLeftArmDoFStart+kShoulderDownUp];
			apHumanPose->m_DoFArray[kLeftArmDoFStart+kShoulderDownUp] = 0;

			apHumanPose->m_DoFArray[kLeftArmDoFStart+kArmFrontBack] += (45.0f/160.0f) * apHumanPose->m_DoFArray[kLeftArmDoFStart+kShoulderFrontBack];
			apHumanPose->m_DoFArray[kLeftArmDoFStart+kShoulderFrontBack] = 0;
		}

		if(apHuman->m_HumanBoneIndex[kRightShoulder] < 0)
		{
			apHumanPose->m_DoFArray[kRightArmDoFStart+kArmDownUp] += (30.0f/200.0f) * apHumanPose->m_DoFArray[kRightArmDoFStart+kShoulderDownUp];
			apHumanPose->m_DoFArray[kRightArmDoFStart+kShoulderDownUp] = 0;

			apHumanPose->m_DoFArray[kRightArmDoFStart+kArmFrontBack] += (45.0f/160.0f) * apHumanPose->m_DoFArray[kRightArmDoFStart+kShoulderFrontBack];
			apHumanPose->m_DoFArray[kRightArmDoFStart+kShoulderFrontBack] = 0;
		}
	}

	void Human2SkeletonPose(Human const *apHuman, HumanPose const *apHumanPose, skeleton::SkeletonPose *apSkeletonPose, int32_t i)
	{
		if(apHuman->m_HumanBoneIndex[i] != -1)
		{
			math::float4 xyz = math::cond(	math::bool4(Bone2DoF[i][2] != -1,Bone2DoF[i][1] != -1,Bone2DoF[i][0] != -1,false),
											math::float4(apHumanPose->m_DoFArray[Bone2DoF[i][2]],apHumanPose->m_DoFArray[Bone2DoF[i][1]],apHumanPose->m_DoFArray[Bone2DoF[i][0]],0),
											math::float4::zero());
			
			skeleton::SkeletonSetDoF(apHuman->m_Skeleton.Get(),apSkeletonPose,xyz,apHuman->m_HumanBoneIndex[i]);
		}
	}

	void Human2SkeletonPose(Human const *apHuman, HumanPose const *apHumanPose, skeleton::SkeletonPose *apSkeletonPose)
	{
		int32_t i;
		for(i = 1; i < kLastBone; i++)
		{
			Human2SkeletonPose(apHuman,apHumanPose,apSkeletonPose,i);
		}

		if(apHuman->m_HasLeftHand)
		{
			hand::Hand2SkeletonPose(apHuman->m_LeftHand.Get(),apHuman->m_Skeleton.Get(),&apHumanPose->m_LeftHandPose,apSkeletonPose);
		}

		if(apHuman->m_HasRightHand)
		{
			hand::Hand2SkeletonPose(apHuman->m_RightHand.Get(),apHuman->m_Skeleton.Get(),&apHumanPose->m_RightHandPose,apSkeletonPose);
		}
	}

	void Skeleton2HumanPose(Human const *apHuman, skeleton::SkeletonPose const *apSkeletonPose, HumanPose *apHumanPose, int32_t i)
	{
		if(apHuman->m_HumanBoneIndex[i] != -1)
		{
			const math::float4 xyz = skeleton::SkeletonGetDoF(apHuman->m_Skeleton.Get(),apSkeletonPose,apHuman->m_HumanBoneIndex[i]);

			if(Bone2DoF[i][2] != -1) apHumanPose->m_DoFArray[Bone2DoF[i][2]] = xyz.x().tofloat();
			if(Bone2DoF[i][1] != -1) apHumanPose->m_DoFArray[Bone2DoF[i][1]] = xyz.y().tofloat();
			if(Bone2DoF[i][0] != -1) apHumanPose->m_DoFArray[Bone2DoF[i][0]] = xyz.z().tofloat();
		}
	}

	void Skeleton2HumanPose(Human const *apHuman, skeleton::SkeletonPose const *apSkeletonPose, HumanPose *apHumanPose)
	{
		int32_t i;

		for(i = 1; i < kLastBone; i++)
		{
			Skeleton2HumanPose(apHuman,apSkeletonPose,apHumanPose,i);
		}

		if(apHuman->m_HasLeftHand)
		{
			hand::Skeleton2HandPose(apHuman->m_LeftHand.Get(),apHuman->m_Skeleton.Get(),apSkeletonPose,&apHumanPose->m_LeftHandPose);
		}

		if(apHuman->m_HasRightHand)
		{
			hand::Skeleton2HandPose(apHuman->m_RightHand.Get(),apHuman->m_Skeleton.Get(),apSkeletonPose,&apHumanPose->m_RightHandPose);
		}
	}

	math::float4 HumanComputeBoneMassCenter(Human const *apHuman, skeleton::SkeletonPose const *apSkeletonPose,int32_t aBoneIndex)
	{
		math::float4 ret( math::float4::zero() );

		switch(aBoneIndex)
		{
		case kHips:
			ret = math::float1(1.0f/3.0f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kSpine]].t);	
			break;

		case kSpine:
			if(apHuman->m_HumanBoneIndex[kChest] >= 0)
			{
				ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kSpine]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kChest]].t);
			}
			else
			{
				ret = math::float1(0.1f) * apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kSpine]].t + math::float1(0.9f * 0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperArm]].t);
			}
			break;

		case kChest:
			if(apHuman->m_HumanBoneIndex[kNeck] >= 0 && apHuman->m_HumanBoneIndex[kLeftShoulder] >= 0 && apHuman->m_HumanBoneIndex[kRightShoulder] >= 0)
			{
				ret = math::float1(0.25f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kChest]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kNeck]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftShoulder]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightShoulder]].t);
			}
			else
			{
				ret = math::float1(1.0f/3.0f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kChest]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperArm]].t);
			}
			break;

		case kNeck:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kNeck]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kHead]].t);
			break;

		case kLeftUpperLeg:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftLowerLeg]].t);
			break;

		case kLeftLowerLeg:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftLowerLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftFoot]].t);
			break;

		case kLeftShoulder:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftShoulder]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t);
			break;

		case kLeftUpperArm:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftLowerArm]].t);
			break;

		case kLeftLowerArm:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftUpperArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kLeftHand]].t);
			break;

		case kRightUpperLeg:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightLowerLeg]].t);
			break;

		case kRightLowerLeg:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightLowerLeg]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightFoot]].t);
			break;

		case kRightShoulder:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightShoulder]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperArm]].t);
			break;

		case kRightUpperArm:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightUpperArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightLowerArm]].t);
			break;

		case kRightLowerArm:
			ret = math::float1(0.5f) * (apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightLowerArm]].t + apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[kRightHand]].t);
			break;

		default:
			ret = apSkeletonPose->m_X[apHuman->m_HumanBoneIndex[aBoneIndex]].t;
			break;	
		};

		return ret;
	}

	math::float4 HumanComputeMassCenter(Human const *apHuman, skeleton::SkeletonPose const *apSkeletonPoseGlobal)
	{
		math::float4 ret(math::float4::zero());

		int32_t i;

		float mass = 0;

		for(i = 0; i < kLastBone; i++)
		{
			int32_t index = apHuman->m_HumanBoneIndex[i];

			if(index >=0)
			{
				float boneMass = apHuman->m_HumanBoneMass[i];
				ret += HumanComputeBoneMassCenter(apHuman,apSkeletonPoseGlobal,i) * math::float1(boneMass);
				mass += boneMass;
			}
		}

		return ret / math::float1(mass);
	}

	float HumanComputeMomentumOfInertia(Human const *apHuman, skeleton::SkeletonPose const *apSkeletonPoseGlobal)
	{
		float ret = 0;

		math::float4 mc = HumanComputeMassCenter(apHuman,apSkeletonPoseGlobal);

		int32_t i;

		for(i = 0; i < kLastBone; i++)
		{
			int32_t index = apHuman->m_HumanBoneIndex[i];
			
			if(index >= 0)
			{
				float r = math::length(HumanComputeBoneMassCenter(apHuman,apSkeletonPoseGlobal,index) - mc).tofloat();
				ret += apHuman->m_HumanBoneMass[i] * r * r;			
			}
		}

		return ret;

	}

	math::float4 HumanComputeOrientation(Human const* apHuman,skeleton::SkeletonPose const* apPoseGlobal)
	{
		int32_t llIndex = apHuman->m_HumanBoneIndex[kLeftUpperLeg];
		int32_t rlIndex = apHuman->m_HumanBoneIndex[kRightUpperLeg];

		int32_t laIndex = apHuman->m_HumanBoneIndex[kLeftUpperArm];
		int32_t raIndex = apHuman->m_HumanBoneIndex[kRightUpperArm];

		math::float4 legMC = math::float1(0.5f) * (apPoseGlobal->m_X[llIndex].t + apPoseGlobal->m_X[rlIndex].t);
		math::float4 armMC = math::float1(0.5f) * (apPoseGlobal->m_X[laIndex].t + apPoseGlobal->m_X[raIndex].t);

		math::float4 upV = math::normalize(armMC-legMC);

		math::float4 legV = apPoseGlobal->m_X[rlIndex].t - apPoseGlobal->m_X[llIndex].t;
		math::float4 armV = apPoseGlobal->m_X[raIndex].t - apPoseGlobal->m_X[laIndex].t;

		math::float4 rightV = math::normalize(legV+armV);
		math::float4 frontV = math::cross(rightV, upV);
		
		rightV = math::cross(upV,frontV);

		return math::normalize(math::quatMul(math::quatMatrixToQuat(rightV,upV,frontV),math::quatConj(apHuman->m_RootX.q)));
	}

	math::xform HumanComputeRootXform(Human const* apHuman,skeleton::SkeletonPose const* apPoseGlobal)
	{
		return math::xform(HumanComputeMassCenter(apHuman,apPoseGlobal),HumanComputeOrientation(apHuman,apPoseGlobal),math::float4(1.0f));
	}

	float HumanGetFootHeight(Human const* apHuman, bool aLeft)
	{
		return apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[apHuman->m_HumanBoneIndex[aLeft ? kLeftFoot : kRightFoot]].m_AxesId].m_Length;
	}

	math::float4 HumanGetFootBottom(Human const* apHuman, bool aLeft)
	{
		return math::float4(HumanGetFootHeight(apHuman,aLeft),0,0,0);
	}

	math::xform HumanGetColliderXform(Human const* apHuman, math::xform const& x, int32_t aBoneIndex)
	{
		math::xform ret;

		int32_t skIndex = apHuman->m_HumanBoneIndex[aBoneIndex];

		if(skIndex >= 0)
		{
			int32_t axesIndex = apHuman->m_Skeleton->m_Node[skIndex].m_AxesId;
			int32_t colliderIndex = apHuman->m_ColliderIndex[aBoneIndex];
			
			if(axesIndex >= 0 && colliderIndex >= 0)
			{				
				ret = x;

				ret.q = math::normalize(math::quatMul(ret.q,apHuman->m_Skeleton->m_AxesArray[axesIndex].m_PostQ));

				ret = math::xformMul(ret,apHuman->m_ColliderArray[colliderIndex].m_X);

				//ret.q = math::normalize(math::quatMul(ret.q,math::float4(0,1,0,1))); // to math physX axis setup
			}
		}

		return ret;
	}

	math::xform HumanSubColliderXform(Human const* apHuman, math::xform const& x, int32_t aBoneIndex)
	{
		math::xform ret;

		int32_t skIndex = apHuman->m_HumanBoneIndex[aBoneIndex];

		if(skIndex >= 0)
		{
			int32_t axesIndex = apHuman->m_Skeleton->m_Node[skIndex].m_AxesId;
			int32_t colliderIndex = apHuman->m_ColliderIndex[aBoneIndex];
			
			if(axesIndex >= 0 && colliderIndex >= 0)
			{				
				ret = x;

				ret = math::xformMulInv(ret,apHuman->m_ColliderArray[colliderIndex].m_X);

				ret.q = math::normalize(math::quatMul(ret.q, math::quatConj(apHuman->m_Skeleton->m_AxesArray[axesIndex].m_PostQ)));
			}
		}

		return ret;
	}
    
    math::float4 HumanGetGoalOrientationOffset(Goal goalIndex)
    {
        return math::load(goalOrientationOffsetArray[goalIndex]);
    }

	void HumanPoseClear(HumanPose& arPose)
	{
		uint32_t i;

		arPose.m_RootX = math::xformIdentity();

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X = math::xformIdentity();
		}

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = 0; 
		}

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			arPose.m_LeftHandPose.m_DoFArray[i] = 0; 
			arPose.m_RightHandPose.m_DoFArray[i] = 0; 
		}
	}

	void HumanPoseCopy(HumanPose &arPose,HumanPose const &arPoseA, bool aDoFOnly)
	{
		uint32_t i;

		if(!aDoFOnly)
		{
			arPose.m_RootX = arPoseA.m_RootX;

			for(i = 0; i < kLastGoal; i++)
			{
				arPose.m_GoalArray[i].m_X = arPoseA.m_GoalArray[i].m_X;
			}
		}

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = arPoseA.m_DoFArray[i]; 
		}
	
		hand::HandPoseCopy(&arPoseA.m_LeftHandPose,&arPose.m_LeftHandPose);
		hand::HandPoseCopy(&arPoseA.m_RightHandPose,&arPose.m_RightHandPose);
	}

	void HumanPoseCopy(HumanPose &arPose,HumanPose const &arPoseA, HumanPoseMask const &arHumanPoseMask)
	{
		if( arHumanPoseMask == FullBodyMask())
		{
			HumanPoseCopy(arPose,arPoseA);
		}
		else
		{		
			int32_t i;
			for(i = 0; i < kLastDoF; i++)
			{
				if(arHumanPoseMask.test(kMaskDoFStartIndex+i))
				{
					arPose.m_DoFArray[i] = arPoseA.m_DoFArray[i];
				}
				else
				{
					arPose.m_DoFArray[i] = 0;
				}
			}

			if(arHumanPoseMask.test(kMaskLeftHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPose.m_LeftHandPose.m_DoFArray[i] = arPoseA.m_LeftHandPose.m_DoFArray[i];  
				}
			}
			else
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPose.m_LeftHandPose.m_DoFArray[i] = 0;  
				}
			}

			if(arHumanPoseMask.test(kMaskRightHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPose.m_RightHandPose.m_DoFArray[i] = arPoseA.m_RightHandPose.m_DoFArray[i];  
				}
			}
			else
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPose.m_RightHandPose.m_DoFArray[i] = 0;  
				}
			}

			for(i = 0; i < kLastGoal; i++)
			{
				if(arHumanPoseMask.test(kMaskGoalStartIndex+i))
				{
					arPose.m_GoalArray[i].m_X = arPoseA.m_GoalArray[i].m_X;
				}
				else
				{
					arPose.m_GoalArray[i].m_X = math::xformIdentity();
				}
			}

			if(arHumanPoseMask.test(0))
			{
				arPose.m_RootX = arPoseA.m_RootX;
			}
			else
			{
				arPose.m_RootX = math::xformIdentity();
			}
			
		}
	}

	void HumanPoseAdd(HumanPose &arPose,HumanPose const &arPoseA,HumanPose const &arPoseB)
	{
		uint32_t i;

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X = math::xformMul(arPoseA.m_GoalArray[i].m_X,arPoseB.m_GoalArray[i].m_X); 
		}

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = arPoseA.m_DoFArray[i] + arPoseB.m_DoFArray[i]; 
		}

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			arPose.m_LeftHandPose.m_DoFArray[i] = arPoseA.m_LeftHandPose.m_DoFArray[i] + arPoseB.m_LeftHandPose.m_DoFArray[i]; 
			arPose.m_RightHandPose.m_DoFArray[i] = arPoseA.m_RightHandPose.m_DoFArray[i] + arPoseB.m_RightHandPose.m_DoFArray[i]; 
		}

		arPose.m_RootX = math::xformMul(arPoseA.m_RootX,arPoseB.m_RootX);
	}

	void HumanPoseSub(HumanPose &arPose,HumanPose const &arPoseA,HumanPose const &arPoseB)
	{
		uint32_t i;

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X = math::xformInvMulNS(arPoseB.m_GoalArray[i].m_X,arPoseA.m_GoalArray[i].m_X);
		}

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = arPoseA.m_DoFArray[i] - arPoseB.m_DoFArray[i]; 
		}

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			arPose.m_LeftHandPose.m_DoFArray[i] = arPoseA.m_LeftHandPose.m_DoFArray[i] - arPoseB.m_LeftHandPose.m_DoFArray[i]; 
			arPose.m_RightHandPose.m_DoFArray[i] = arPoseA.m_RightHandPose.m_DoFArray[i] - arPoseB.m_RightHandPose.m_DoFArray[i]; 
		}

		arPose.m_RootX = math::xformInvMulNS(arPoseB.m_RootX,arPoseA.m_RootX);
	}
	
	void HumanPoseWeight(HumanPose &arPose,HumanPose const &arPoseA, float aWeight)
	{
		uint32_t i;

		math::float1 w(aWeight);

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X = math::xformWeight(arPoseA.m_GoalArray[i].m_X,w); 
		}

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = arPoseA.m_DoFArray[i] * aWeight; 
		}

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			arPose.m_LeftHandPose.m_DoFArray[i] = arPoseA.m_LeftHandPose.m_DoFArray[i] * aWeight;
			arPose.m_RightHandPose.m_DoFArray[i] = arPoseA.m_RightHandPose.m_DoFArray[i] * aWeight;
		}

		arPose.m_RootX = math::xformWeight(arPoseA.m_RootX,w);
	}

	void HumanPoseMirror(HumanPose &arPose,HumanPose const &arPoseA)
	{
		uint32_t i;

		for(i = 0; i < kLastBodyDoF; i++)
		{
			arPose.m_DoFArray[kBodyDoFStart + i] *= BodyDoFMirror[i];
		}

		for(i = 0; i < kLastHeadDoF; i++)
		{
			arPose.m_DoFArray[kHeadDoFStart + i] *= HeadDoFMirror[i];
			// bobtodo
		}

		for(i = 0; i < kLastArmDoF; i++)
		{
			float dof = arPose.m_DoFArray[kLeftArmDoFStart + i];
			arPose.m_DoFArray[kLeftArmDoFStart + i] = arPose.m_DoFArray[kRightArmDoFStart + i];
			arPose.m_DoFArray[kRightArmDoFStart + i] = dof;		
		}

		for(i = 0; i < kLastLegDoF; i++)
		{
			float dof = arPose.m_DoFArray[kLeftLegDoFStart + i];
			arPose.m_DoFArray[kLeftLegDoFStart + i] = arPose.m_DoFArray[kRightLegDoFStart + i];
			arPose.m_DoFArray[kRightLegDoFStart + i] = dof;		
		}

		math::xform x = arPose.m_GoalArray[kLeftFootGoal].m_X;
		arPose.m_GoalArray[kLeftFootGoal].m_X = arPose.m_GoalArray[kRightFootGoal].m_X; 
		arPose.m_GoalArray[kRightFootGoal].m_X = x;  

		x = arPose.m_GoalArray[kLeftHandGoal].m_X;
		arPose.m_GoalArray[kLeftHandGoal].m_X = arPose.m_GoalArray[kRightHandGoal].m_X; 
		arPose.m_GoalArray[kRightHandGoal].m_X = x;  

		constant_float4(offsetQY,0,1,0,0);
		constant_float4(offsetQZ,0,0,1,0);

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X = math::mirror(arPose.m_GoalArray[i].m_X);
		}
		
		arPose.m_GoalArray[kLeftFootGoal].m_X.q = math::normalize(math::quatMul(arPose.m_GoalArray[kLeftFootGoal].m_X.q,offsetQY));
		arPose.m_GoalArray[kRightFootGoal].m_X.q = math::normalize(math::quatMul(arPose.m_GoalArray[kRightFootGoal].m_X.q,offsetQY));
		arPose.m_GoalArray[kLeftHandGoal].m_X.q = math::normalize(math::quatMul(arPose.m_GoalArray[kLeftHandGoal].m_X.q,offsetQZ));
		arPose.m_GoalArray[kRightHandGoal].m_X.q = math::normalize(math::quatMul(arPose.m_GoalArray[kRightHandGoal].m_X.q,offsetQZ));

		arPose.m_RootX = math::mirror(arPose.m_RootX);

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			float leftdof = arPose.m_LeftHandPose.m_DoFArray[i];
			arPose.m_LeftHandPose.m_DoFArray[i] = arPose.m_RightHandPose.m_DoFArray[i];
			arPose.m_RightHandPose.m_DoFArray[i] = leftdof;
		}
	}

	void HumanPoseBlend(HumanPose &arPose,HumanPose **apPoseArray, float *apWeightArray, uint32_t aCount)
	{
		uint32_t poseIter,i;

		for(i = 0; i < kLastDoF; i++)
		{
			arPose.m_DoFArray[i] = 0; 
		}

		for(i = 0; i < hand::s_DoFCount; i++)
		{
			arPose.m_LeftHandPose.m_DoFArray[i] = 0;
			arPose.m_RightHandPose.m_DoFArray[i] = 0;
		}

		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X.t = math::float4::zero(); 
			arPose.m_GoalArray[i].m_X.q = math::float4::zero(); 
			arPose.m_GoalArray[i].m_X.s = math::float4::one(); 
		}

		arPose.m_RootX.t = math::float4::zero(); 
		arPose.m_RootX.q = math::float4::zero(); 
		arPose.m_RootX.s = math::float4::one(); 

		float sumW = 0;

		for(poseIter = 0; poseIter < aCount; poseIter++)
		{
			float w = apWeightArray[poseIter]; 
			math::float1 w1(w);

			sumW += w;

			for(i = 0; i < kLastDoF; i++)
			{
				arPose.m_DoFArray[i] += apPoseArray[poseIter]->m_DoFArray[i]*w; 
			}

			for(i = 0; i < hand::s_DoFCount; i++)
			{
				arPose.m_LeftHandPose.m_DoFArray[i] += apPoseArray[poseIter]->m_LeftHandPose.m_DoFArray[i] * w;
				arPose.m_RightHandPose.m_DoFArray[i] += apPoseArray[poseIter]->m_RightHandPose.m_DoFArray[i] * w;
			}

			for(i = 0; i < kLastGoal; i++)
			{
				arPose.m_GoalArray[i].m_X.t += apPoseArray[poseIter]->m_GoalArray[i].m_X.t*w1; 
				arPose.m_GoalArray[i].m_X.q += math::cond(math::dot(arPose.m_GoalArray[i].m_X.q,apPoseArray[poseIter]->m_GoalArray[i].m_X.q) < math::float1::zero(),apPoseArray[poseIter]->m_GoalArray[i].m_X.q * -w1,apPoseArray[poseIter]->m_GoalArray[i].m_X.q * w1); 
				arPose.m_GoalArray[i].m_X.s *= scaleWeight(apPoseArray[poseIter]->m_GoalArray[i].m_X.s,w1); 
			}

			arPose.m_RootX.t += apPoseArray[poseIter]->m_RootX.t*w1; 
			arPose.m_RootX.q += math::cond(math::dot(arPose.m_RootX.q,apPoseArray[poseIter]->m_RootX.q) < math::float1::zero(),apPoseArray[poseIter]->m_RootX.q * -w1,apPoseArray[poseIter]->m_RootX.q * w1); 
			arPose.m_RootX.s *= scaleWeight(apPoseArray[poseIter]->m_RootX.s,w1); 
		}

		math::float4 q(0,0,0,math::saturate(1.0f-sumW));
		
		for(i = 0; i < kLastGoal; i++)
		{
			arPose.m_GoalArray[i].m_X.q = math::normalize(arPose.m_GoalArray[i].m_X.q+q);
		}

		arPose.m_RootX.q = math::normalize(arPose.m_RootX.q+q);

	}

	void HumanPoseAddOverrideLayer(HumanPose &arPoseBase,HumanPose const &arPose, float aWeight, HumanPoseMask const &arHumanPoseMask)
	{
		if(aWeight > 0.0f)
		{
			float weightInv = 1.0f - aWeight;
			math::float1 w(aWeight);

			int32_t i;
			for(i = 0; i < kLastDoF; i++)
			{
				if(arHumanPoseMask.test(kMaskDoFStartIndex+i))
				{
					if(aWeight < 1.0f)
					{
						arPoseBase.m_DoFArray[i] = weightInv * arPoseBase.m_DoFArray[i] + aWeight * arPose.m_DoFArray[i];  
					}
					else
					{
						arPoseBase.m_DoFArray[i] = arPose.m_DoFArray[i];
					}
				}
			}

			if(arHumanPoseMask.test(kMaskLeftHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					if(aWeight < 1.0f)
					{
						arPoseBase.m_LeftHandPose.m_DoFArray[i] = weightInv * arPoseBase.m_LeftHandPose.m_DoFArray[i] + aWeight * arPose.m_LeftHandPose.m_DoFArray[i];  
					}
					else
					{
						arPoseBase.m_LeftHandPose.m_DoFArray[i] = arPose.m_LeftHandPose.m_DoFArray[i];  
					}
				}
			}

			if(arHumanPoseMask.test(kMaskRightHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					if(aWeight < 1.0f)
					{
						arPoseBase.m_RightHandPose.m_DoFArray[i] = weightInv * arPoseBase.m_RightHandPose.m_DoFArray[i] + aWeight * arPose.m_RightHandPose.m_DoFArray[i];  
					}
					else
					{
						arPoseBase.m_RightHandPose.m_DoFArray[i] = arPose.m_RightHandPose.m_DoFArray[i];  
					}
				}
			}

			for(i = 0; i < kLastGoal; i++)
			{
				if(arHumanPoseMask.test(kMaskGoalStartIndex+i))
				{
					if(aWeight < 1.0f)
					{
						arPoseBase.m_GoalArray[i].m_X = math::xformBlend(arPoseBase.m_GoalArray[i].m_X,arPose.m_GoalArray[i].m_X,w);
					}
					else
					{
						arPoseBase.m_GoalArray[i].m_X = arPose.m_GoalArray[i].m_X;
					}
				}
			}

			if(arHumanPoseMask.test(0))
			{
				if(aWeight < 1.0f)
				{
					arPoseBase.m_RootX = math::xformBlend(arPoseBase.m_RootX,arPose.m_RootX,w);
				}
				else
				{
					arPoseBase.m_RootX = arPose.m_RootX;
				}
			}
		}
	}

	void HumanPoseAddAdditiveLayer(HumanPose &arPoseBase,HumanPose const &arPose, float aWeight, HumanPoseMask const &arHumanPoseMask)
	{
		if(aWeight > 0.0f)
		{		
			math::float1 w(aWeight);

			int32_t i;
			for(i = 0; i < kLastDoF; i++)
			{
				if(arHumanPoseMask.test(kMaskDoFStartIndex+i))
				{					
					arPoseBase.m_DoFArray[i] += aWeight * arPose.m_DoFArray[i];  					
				}
			}

			if(arHumanPoseMask.test(kMaskLeftHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPoseBase.m_LeftHandPose.m_DoFArray[i] += aWeight * arPose.m_LeftHandPose.m_DoFArray[i]; 					
				}
			}

			if(arHumanPoseMask.test(kMaskRightHand))
			{
				for(i = 0; i < hand::s_DoFCount; i++)
				{
					arPoseBase.m_RightHandPose.m_DoFArray[i] += aWeight * arPose.m_RightHandPose.m_DoFArray[i];  					
				}
			}

			for(i = 0; i < kLastGoal; i++)
			{
				if(arHumanPoseMask.test(kMaskGoalStartIndex+i))
				{
					arPoseBase.m_GoalArray[i].m_X = math::xformMul(arPoseBase.m_GoalArray[i].m_X, math::xformWeight(arPose.m_GoalArray[i].m_X, w));					
				}
			}

			if(arHumanPoseMask.test(0))
			{
				arPoseBase.m_RootX = math::xformMul(arPoseBase.m_RootX, math::xformWeight(arPose.m_RootX,w));				
			}
		}
	}

	void HumanFixMidDoF(Human const *apHuman, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *apSkeletonPoseWs, int32_t aPIndex, int32_t aCIndex)
	{
		int32_t pNodeIndex = apHuman->m_HumanBoneIndex[aPIndex];
		int32_t cNodeIndex = apHuman->m_HumanBoneIndex[aCIndex];
		int32_t aNodeIndex = apHuman->m_Skeleton->m_Node[pNodeIndex].m_ParentId;
	
		math::Axes pAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[pNodeIndex].m_AxesId];
		math::Axes cAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[cNodeIndex].m_AxesId];

		apSkeletonPoseWs->m_X[aNodeIndex].q = math::quatIdentity();
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		math::float4 pq = apSkeletonPose->m_X[pNodeIndex].q;		
		math::float4 cqg = apSkeletonPoseWs->m_X[cNodeIndex].q;
		
		math::float4 cql = AxesProject(cAxes,apSkeletonPose->m_X[cNodeIndex].q);

		math::float4 xyz = math::quat2ZYRoll(cql);
		xyz.y() = math::float1::zero();
		cql = math::ZYRoll2Quat(xyz);

		cql = math::AxesUnproject(cAxes,cql);

		apSkeletonPose->m_X[cNodeIndex].q = cql;
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,cNodeIndex);

		math::float4 qdiff = math::quatMul(cqg,math::quatConj(apSkeletonPoseWs->m_X[cNodeIndex].q));
		
		apSkeletonPose->m_X[pNodeIndex].q = math::normalize(math::quatMul(qdiff,apSkeletonPose->m_X[pNodeIndex].q));

		skeleton::SkeletonAlign(apHuman->m_Skeleton.Get(),pq,apSkeletonPose->m_X[pNodeIndex].q,pNodeIndex);

		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		apSkeletonPoseWs->m_X[cNodeIndex].q = cqg;

		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWs,apSkeletonPose,cNodeIndex,cNodeIndex);
	}

	void HumanFixEndDoF(Human const *apHuman, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *apSkeletonPoseWs, int32_t aPIndex, int32_t aCIndex)
	{
		int32_t pNodeIndex = apHuman->m_HumanBoneIndex[aPIndex];
		int32_t cNodeIndex = apHuman->m_HumanBoneIndex[aCIndex];
		int32_t aNodeIndex = apHuman->m_Skeleton->m_Node[pNodeIndex].m_ParentId;

		math::Axes pAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[pNodeIndex].m_AxesId];
		math::Axes cAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[cNodeIndex].m_AxesId];

		apSkeletonPoseWs->m_X[aNodeIndex].q = math::quatIdentity();
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		math::float4 pq = apSkeletonPose->m_X[pNodeIndex].q;		
		math::float4 cqg = apSkeletonPoseWs->m_X[cNodeIndex].q;

		math::float4 pq0 = FromAxes(pAxes,math::float4::zero());
		math::float4 cql0 = FromAxes(cAxes,math::float4::zero());

		apSkeletonPose->m_X[pNodeIndex].q = pq0;
		apSkeletonPose->m_X[cNodeIndex].q = cql0;
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		math::float4 qdiff = math::quatMul(cqg,math::quatConj(apSkeletonPoseWs->m_X[cNodeIndex].q));

		apSkeletonPose->m_X[pNodeIndex].q = math::normalize(math::quatMul(qdiff,pq0));

		skeleton::SkeletonAlign(apHuman->m_Skeleton.Get(),pq,apSkeletonPose->m_X[pNodeIndex].q,pNodeIndex);

		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		apSkeletonPoseWs->m_X[cNodeIndex].q = cqg;

		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWs,apSkeletonPose,cNodeIndex,cNodeIndex);
	}

	void HumanFixTwist(Human const *apHuman, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *apSkeletonPoseWs, int32_t aPIndex, int32_t aCIndex, const math::float1& aTwist)
	{
		int32_t pNodeIndex = apHuman->m_HumanBoneIndex[aPIndex];
		int32_t cNodeIndex = apHuman->m_HumanBoneIndex[aCIndex];
		int32_t aNodeIndex = apHuman->m_Skeleton->m_Node[pNodeIndex].m_ParentId;
	
		math::Axes pAxes = apHuman->m_Skeleton->m_AxesArray[apHuman->m_Skeleton->m_Node[pNodeIndex].m_AxesId];

		apSkeletonPoseWs->m_X[aNodeIndex].q = math::quatIdentity();
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		math::float4 pq = apSkeletonPose->m_X[pNodeIndex].q;
		math::float4 cqg = apSkeletonPoseWs->m_X[cNodeIndex].q;

		math::float4 pxyz = math::ToAxes(pAxes,apSkeletonPose->m_X[pNodeIndex].q);
		pxyz.x() *= aTwist;
		
		apSkeletonPose->m_X[pNodeIndex].q = math::FromAxes(pAxes,pxyz);
		
		skeleton::SkeletonAlign(apHuman->m_Skeleton.Get(),pq,apSkeletonPose->m_X[pNodeIndex].q,pNodeIndex);

		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWs,cNodeIndex,pNodeIndex);

		apSkeletonPoseWs->m_X[cNodeIndex].q = cqg;

		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWs,apSkeletonPose,cNodeIndex,cNodeIndex);
	}

	void ReachGoalRotation(Human const *apHuman,math::float4 const &arEndQ, int32_t aGoalIndex, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *apSkeletonPoseGbl, skeleton::SkeletonPose *apSkeletonPoseWorkspace)
	{
		int32_t index = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[aGoalIndex].m_Index]; 
		int32_t parentIndex = apHuman->m_Skeleton->m_Node[index].m_ParentId;
		apSkeletonPose->m_X[index].q = math::normalize(math::quatMul(math::quatConj(apSkeletonPoseGbl->m_X[parentIndex].q),arEndQ));

		HumanFixEndDoF(apHuman,apSkeletonPose,apSkeletonPoseWorkspace,s_HumanGoalInfo[aGoalIndex].m_MidIndex,s_HumanGoalInfo[aGoalIndex].m_EndIndex);
	}

	void HumanFixEndPointsSkeletonPose(Human const *apHuman, skeleton::SkeletonPose const*apSkeletonPoseRef, HumanPose *apHumanPose, skeleton::SkeletonPose *apSkeletonPoseGbl, skeleton::SkeletonPose *apSkeletonPoseLcl, skeleton::SkeletonPose *apSkeletonPoseWs,int32_t cIndex, int32_t pIndex)
	{		
		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseLcl,apSkeletonPoseGbl,apHuman->m_HumanBoneIndex[cIndex],apHuman->m_HumanBoneIndex[pIndex]);
		apSkeletonPoseGbl->m_X[apHuman->m_HumanBoneIndex[cIndex]].q = apSkeletonPoseRef->m_X[apHuman->m_HumanBoneIndex[cIndex]].q; 
		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseGbl,apSkeletonPoseLcl,apHuman->m_HumanBoneIndex[cIndex],apHuman->m_HumanBoneIndex[pIndex]);		

		HumanFixEndDoF(apHuman,apSkeletonPoseLcl,apSkeletonPoseWs,pIndex,cIndex);
	}

	void HumanAlignSkeletonPose(Human const *apHuman, skeleton::SkeletonPose const*apSkeletonPoseRef, HumanPose *apHumanPose, skeleton::SkeletonPose *apSkeletonPoseGbl, skeleton::SkeletonPose *apSkeletonPoseLcl,int32_t cIndex, int32_t pIndex)
	{
		Skeleton2HumanPose(apHuman,apSkeletonPoseLcl,apHumanPose,cIndex);
		Human2SkeletonPose(apHuman,apHumanPose,apSkeletonPoseLcl,cIndex);

		skeleton::SkeletonPoseComputeGlobalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseLcl,apSkeletonPoseGbl,apHuman->m_HumanBoneIndex[cIndex],apHuman->m_HumanBoneIndex[pIndex]);
		skeleton::SkeletonAlign(apHuman->m_Skeleton.Get(),apSkeletonPoseRef,apSkeletonPoseGbl,apHuman->m_HumanBoneIndex[cIndex]);
		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseGbl,apSkeletonPoseLcl,apHuman->m_HumanBoneIndex[cIndex],apHuman->m_HumanBoneIndex[pIndex]);
	}

	void RetargetFrom(	Human const *apHuman, 
						skeleton::SkeletonPose const *apSkeletonPose,
						HumanPose *apHumanPose, 
						skeleton::SkeletonPose *apSkeletonPoseRef,
						skeleton::SkeletonPose *apSkeletonPoseGbl,
						skeleton::SkeletonPose *apSkeletonPoseLcl,
						skeleton::SkeletonPose *apSkeletonPoseWs,
						mecanim::int32_t maxFixIter)

	{
		const int32_t hipsIndex = apHuman->m_HumanBoneIndex[human::kHips];
		const math::float1 scale(apHuman->m_Scale);

		skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseRef);
		skeleton::SkeletonPoseCopy(apSkeletonPoseRef,apSkeletonPoseGbl);

		// force dummy bones to their default rotation
		int32_t nodeIter;
		for(nodeIter = 1; nodeIter < apHuman->m_Skeleton->m_Count; nodeIter++)
		{
			if(apHuman->m_Skeleton->m_Node[nodeIter].m_AxesId == -1)
			{
				apSkeletonPoseGbl->m_X[nodeIter].q = math::quatMul(apSkeletonPoseGbl->m_X[apHuman->m_Skeleton->m_Node[nodeIter].m_ParentId].q,apHuman->m_SkeletonPose->m_X[nodeIter].q);
			}
		}

		skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseGbl,apSkeletonPoseLcl);
		
		int32_t fixIter;

		// align shoulders
		for(fixIter = 0; fixIter < maxFixIter; fixIter++)
		{
			if(apHuman->m_HumanBoneIndex[kLeftShoulder] != -1)
			{
				HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kLeftShoulder, kLeftShoulder);
			}

			if(apHuman->m_HumanBoneIndex[kRightShoulder] != -1)
			{
				HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kRightShoulder, kRightShoulder);
			}
		}

		// align upper limbs
		for(fixIter = 0; fixIter < maxFixIter; fixIter++)
		{
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kLeftUpperArm, apHuman->m_HumanBoneIndex[kLeftShoulder] != -1 ? kLeftShoulder : kLeftUpperArm);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kRightUpperArm, apHuman->m_HumanBoneIndex[kRightShoulder] != -1 ? kRightShoulder : kRightUpperArm);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kLeftUpperLeg, kLeftUpperLeg);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kRightUpperLeg, kRightUpperLeg);
		}

		// align & fix lower limbs
		for(fixIter = 0; fixIter < maxFixIter; fixIter++)
		{
			HumanFixMidDoF(apHuman,apSkeletonPoseLcl,apSkeletonPoseWs,kLeftUpperArm,kLeftLowerArm);
			HumanFixMidDoF(apHuman,apSkeletonPoseLcl,apSkeletonPoseWs,kRightUpperArm,kRightLowerArm);
			HumanFixMidDoF(apHuman,apSkeletonPoseLcl,apSkeletonPoseWs,kLeftUpperLeg,kLeftLowerLeg);
			HumanFixMidDoF(apHuman,apSkeletonPoseLcl,apSkeletonPoseWs,kRightUpperLeg,kRightLowerLeg);

			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kLeftLowerArm, kLeftUpperArm);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kRightLowerArm, kRightUpperArm);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kLeftLowerLeg, kLeftUpperLeg);
			HumanAlignSkeletonPose(apHuman,apSkeletonPoseRef,apHumanPose,apSkeletonPoseGbl,apSkeletonPoseLcl,kRightLowerLeg, kRightUpperLeg);
		}

		HumanFixEndPointsSkeletonPose(apHuman,apSkeletonPoseRef, apHumanPose, apSkeletonPoseGbl, apSkeletonPoseLcl, apSkeletonPoseWs, kLeftHand, kLeftLowerArm);
		HumanFixEndPointsSkeletonPose(apHuman,apSkeletonPoseRef, apHumanPose, apSkeletonPoseGbl, apSkeletonPoseLcl, apSkeletonPoseWs, kRightHand, kRightLowerArm);
		HumanFixEndPointsSkeletonPose(apHuman,apSkeletonPoseRef, apHumanPose, apSkeletonPoseGbl, apSkeletonPoseLcl, apSkeletonPoseWs, kLeftFoot, kLeftLowerLeg);
		HumanFixEndPointsSkeletonPose(apHuman,apSkeletonPoseRef, apHumanPose, apSkeletonPoseGbl, apSkeletonPoseLcl, apSkeletonPoseWs, kRightFoot, kRightLowerLeg);
		
		Skeleton2HumanPose(apHuman,apSkeletonPoseLcl,apHumanPose);
		skeleton::SkeletonPoseCopy(apHuman->m_SkeletonPose.Get(), apSkeletonPoseLcl);
		Human2SkeletonPose(apHuman, apHumanPose, apSkeletonPoseLcl); 
		skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(), apSkeletonPoseLcl,apSkeletonPoseGbl);

		apHumanPose->m_RootX = HumanComputeRootXform(apHuman,apSkeletonPoseGbl);
		apHumanPose->m_RootX = math::xformInvMul(apSkeletonPoseGbl->m_X[hipsIndex],apHumanPose->m_RootX);
		apHumanPose->m_RootX = math::xformMul(apSkeletonPoseRef->m_X[hipsIndex],apHumanPose->m_RootX);
		apHumanPose->m_RootX.s = math::float4::one();

		int32_t goalIter;
		for(goalIter = 0; goalIter < kLastGoal; goalIter++)
		{
			int32_t index = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_Index];
			apHumanPose->m_GoalArray[goalIter].m_X.t = apSkeletonPoseRef->m_X[index].t;
			apHumanPose->m_GoalArray[goalIter].m_X.q = AddAxis(apHuman,index,apSkeletonPoseRef->m_X[index].q);
			apHumanPose->m_GoalArray[goalIter].m_X.s = math::float4::one();

			if(goalIter < 2) apHumanPose->m_GoalArray[goalIter].m_X.t = math::xformMulVec(apHumanPose->m_GoalArray[goalIter].m_X,human::HumanGetFootBottom(apHuman,goalIter==0));
			apHumanPose->m_GoalArray[goalIter].m_X = math::xformInvMulNS(apHumanPose->m_RootX,apHumanPose->m_GoalArray[goalIter].m_X);
			apHumanPose->m_GoalArray[goalIter].m_X.t /= scale;
		}

		apHumanPose->m_RootX.t /= scale;
	}

	void RetargetTo(	Human const *apHuman, 
						HumanPose const *apHumanPoseBase, 
						HumanPose const *apHumanPose, 
						const math::xform &arX,
						HumanPose *apHumanPoseOut, 
						skeleton::SkeletonPose *apSkeletonPose, 
						skeleton::SkeletonPose *apSkeletonPoseWs)
	{
		const int32_t rootIndex = 0;
		const int32_t hipsIndex = apHuman->m_HumanBoneIndex[human::kHips];
		const math::float1 scale(apHuman->m_Scale);
				
		human::HumanPoseCopy(*apHumanPoseOut,*apHumanPoseBase);
	
		apHumanPoseOut->m_RootX.t *= scale;
		apHumanPoseOut->m_RootX = math::xformMul(arX,apHumanPoseOut->m_RootX);

		int32_t goalIter;
		for(goalIter = 0; goalIter < kLastGoal; goalIter++)
		{
			apHumanPoseOut->m_GoalArray[goalIter].m_X = apHumanPose ? apHumanPose->m_GoalArray[goalIter].m_X : apHumanPoseBase->m_GoalArray[goalIter].m_X;
			apHumanPoseOut->m_GoalArray[goalIter].m_X.t *= scale;
			apHumanPoseOut->m_GoalArray[goalIter].m_X = math::xformMul(arX,apHumanPoseOut->m_GoalArray[goalIter].m_X);

			if(goalIter < 2) apHumanPoseOut->m_GoalArray[goalIter].m_X.t = math::xformMulVec(apHumanPoseOut->m_GoalArray[goalIter].m_X,-human::HumanGetFootBottom(apHuman,goalIter==0));
		}

		//////////////////////////////////////////////////
		//
		// transfer muscle space for base pose
		//
		skeleton::SkeletonPoseCopy(apHuman->m_SkeletonPose.Get(), apSkeletonPose);
		HumanPoseAdjustForMissingBones(apHuman,apHumanPoseOut);
		Human2SkeletonPose(apHuman, apHumanPoseOut, apSkeletonPose); 
		skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(), apSkeletonPose,apSkeletonPoseWs);
		
		///////////////////////////////////////////////////////
		//
		// adjust hips local
		//
		math::xform rootX = HumanComputeRootXform(apHuman,apSkeletonPoseWs);
		apSkeletonPose->m_X[hipsIndex] = math::xformInvMulNS(rootX,apSkeletonPoseWs->m_X[hipsIndex]);
        apSkeletonPose->m_X[hipsIndex].s = apSkeletonPoseWs->m_X[hipsIndex].s;

		////////////////////////////////////////////////////////
		//
		// transfer muscle space
		//
		if(apHumanPose)
		{
			human::HumanPoseCopy(*apHumanPoseOut,*apHumanPose,true);
			HumanPoseAdjustForMissingBones(apHuman,apHumanPoseOut);
			Human2SkeletonPose(apHuman, apHumanPoseOut, apSkeletonPose); 
		}

		//////////////////////////////////////////////////
		//
		// root
		//
		apSkeletonPose->m_X[rootIndex] = apHumanPoseOut->m_RootX;
	}

	math::float4 GetLookAtDeltaQ(math::float4 const &pivot,math::float4 const &eyesT, math::float4 const &eyesQ, math::float4 const &eyesDir, math::float4 const &target, math::float1 const &weight)
	{
		math::float1 len = math::length(target - eyesT);
		math::float4 dstV = target - pivot;
		math::float4 srcV = eyesT - math::quatMulVec(eyesQ,eyesDir*math::float1(len)) - pivot;

		return math::quatWeight(math::normalize(math::quatArcRotate(srcV,dstV)),weight);
	}

	void FullBodySolve(Human const *apHuman, HumanPose const *apHumanPose, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *apSkeletonPoseWorkspaceA, skeleton::SkeletonPose *apSkeletonPoseWorkspaceB)
	{
		const int32_t hipsIndex = apHuman->m_HumanBoneIndex[kHips]; 
		const int32_t chestIndex = apHuman->m_HumanBoneIndex[kChest]; 
		const int32_t spineIndex = apHuman->m_HumanBoneIndex[kSpine]; 
		const int32_t neckIndex = apHuman->m_HumanBoneIndex[kNeck]; 
		const int32_t headIndex = apHuman->m_HumanBoneIndex[kHead]; 
		const int32_t leftEyeIndex = apHuman->m_HumanBoneIndex[kLeftEye]; 
		const int32_t rightEyeIndex = apHuman->m_HumanBoneIndex[kRightEye]; 

		math::float1 lcw = math::saturate(math::float1(apHumanPose->m_LookAtWeight.x()));
		math::float1 lbw = math::saturate(math::float1(apHumanPose->m_LookAtWeight.y()));
		math::float1 lhw = math::saturate(math::float1(apHumanPose->m_LookAtWeight.z()));
		math::float1 lew = math::saturate(math::float1(apHumanPose->m_LookAtWeight.w()));

		math::float4 headGoalT = apHumanPose->m_LookAtPosition;

		if(lcw > math::float1::zero())
		{
			math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[headIndex].t;
			math::float4 eyesRot = AddAxis(apHuman,headIndex,apSkeletonPoseWorkspaceA->m_X[headIndex].q);
			if(leftEyeIndex != -1 && rightEyeIndex != -1) eyesPos = math::xformMulVec(apSkeletonPoseWorkspaceA->m_X[headIndex],(apHuman->m_SkeletonPose->m_X[leftEyeIndex].t + apHuman->m_SkeletonPose->m_X[rightEyeIndex].t) * math::float1(0.5f));
			
			math::float4 dstV = headGoalT - eyesPos;
			math::float4 v = math::float4::zero();
			v.y() = -math::length(dstV);
			math::float4 srcV = math::quatMulVec(eyesRot,v);

			math::float4 deltaQ = math::quatClamp(math::normalize(math::quatArcRotate(srcV,dstV)),math::radians(180.f*(1.f - lcw.tofloat())));

			headGoalT = eyesPos + math::quatMulVec(deltaQ,srcV);
		}

		if(lbw > math::float1::zero())
		{
			math::float1 lsw = chestIndex != -1 ? math::float1(0.5) * lbw : lbw;

			math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[headIndex].t;
			math::float4 eyesRot = AddAxis(apHuman,headIndex,apSkeletonPoseWorkspaceA->m_X[headIndex].q);

			math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[spineIndex].t,eyesPos,eyesRot,math::float4(0,1,0,0),headGoalT,lsw);

			apSkeletonPoseWorkspaceA->m_X[spineIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[spineIndex].q));				
			skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,spineIndex,spineIndex);

			if(chestIndex != -1)
			{
				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,headIndex,spineIndex);

				math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[headIndex].t;
				math::float4 eyesRot = AddAxis(apHuman,headIndex,apSkeletonPoseWorkspaceA->m_X[headIndex].q);

				math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[chestIndex].t,eyesPos,eyesRot,math::float4(0,1,0,0),headGoalT,lbw);
				
				apSkeletonPoseWorkspaceA->m_X[chestIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[chestIndex].q));				
				skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,chestIndex,chestIndex);
			}
		}	

		if(lhw > math::float1::zero())
		{
			skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,headIndex,spineIndex);

			if(neckIndex != -1)
			{
				math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[headIndex].t;
				math::float4 eyesRot = AddAxis(apHuman,headIndex,apSkeletonPoseWorkspaceA->m_X[headIndex].q);

				if(leftEyeIndex != -1 && rightEyeIndex != -1) eyesPos = math::xformMulVec(apSkeletonPoseWorkspaceA->m_X[headIndex],(apHuman->m_SkeletonPose->m_X[leftEyeIndex].t + apHuman->m_SkeletonPose->m_X[rightEyeIndex].t) * math::float1(0.5f));
				
				math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[neckIndex].t,eyesPos,eyesRot,math::float4(0,1,0,0),headGoalT,lhw * math::float1(0.5));

				apSkeletonPoseWorkspaceA->m_X[neckIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[neckIndex].q));				
				skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,neckIndex,neckIndex);
			}
			
			int32_t iter;
			for(iter = 0; iter < 3; iter++)
			{
				math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[headIndex].t;
				math::float4 eyesRot = AddAxis(apHuman,headIndex,apSkeletonPoseWorkspaceA->m_X[headIndex].q);

				if(leftEyeIndex != -1 && rightEyeIndex != -1) eyesPos = math::xformMulVec(apSkeletonPoseWorkspaceA->m_X[headIndex],(apHuman->m_SkeletonPose->m_X[leftEyeIndex].t + apHuman->m_SkeletonPose->m_X[rightEyeIndex].t) * math::float1(0.5f));
			
				math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[headIndex].t,eyesPos,eyesRot,math::float4(0,1,0,0),headGoalT,lhw*lhw);

				apSkeletonPoseWorkspaceA->m_X[headIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[headIndex].q));				
				skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,headIndex,headIndex);
				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,headIndex,headIndex);
			}
		}	

		if(lew > math::float1::zero())
		{
			if(leftEyeIndex != -1)
			{
				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,leftEyeIndex,spineIndex);
				
				math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[leftEyeIndex].t;
				math::float4 eyesRot = AddAxis(apHuman,leftEyeIndex,apSkeletonPoseWorkspaceA->m_X[leftEyeIndex].q);
				
				math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[leftEyeIndex].t,eyesPos,eyesRot,math::float4(-1,0,0,0),headGoalT,lew);

				apSkeletonPoseWorkspaceA->m_X[leftEyeIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[leftEyeIndex].q));				
				skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,leftEyeIndex,leftEyeIndex);
			}

			if(rightEyeIndex != -1)
			{
				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,rightEyeIndex,spineIndex);

				math::float4 eyesPos = apSkeletonPoseWorkspaceA->m_X[rightEyeIndex].t;
				math::float4 eyesRot = AddAxis(apHuman,rightEyeIndex,apSkeletonPoseWorkspaceA->m_X[rightEyeIndex].q);

				math::float4 deltaQ = GetLookAtDeltaQ(apSkeletonPoseWorkspaceA->m_X[rightEyeIndex].t,eyesPos,eyesRot,math::float4(-1,0,0,0),headGoalT,lew);
				
				apSkeletonPoseWorkspaceA->m_X[rightEyeIndex].q = math::normalize(math::quatMul(deltaQ,apSkeletonPoseWorkspaceA->m_X[rightEyeIndex].q));				
				skeleton::SkeletonPoseComputeLocalQ(apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspaceA,apSkeletonPose,rightEyeIndex,rightEyeIndex);
			}
		}	
			
		int32_t goalIter;
		for(goalIter = kLeftFootGoal; goalIter <= kRightHandGoal; goalIter++)
		{ 
			int32_t topIndex = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_TopIndex];
			int32_t midIndex = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_MidIndex];
			int32_t endIndex = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_EndIndex];

			if(apHumanPose->m_GoalArray[goalIter].m_WeightT > 0.f)
			{
				float weightT = math::saturate(apHumanPose->m_GoalArray[goalIter].m_WeightT);

				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,endIndex,hipsIndex);

				// adjust len
				math::float4 t = math::lerp(apSkeletonPoseWorkspaceA->m_X[endIndex].t,apHumanPose->m_GoalArray[goalIter].m_X.t, math::float1(weightT));
				skeleton::Skeleton2BoneAdjustLength(apHuman->m_Skeleton.Get(),topIndex,midIndex,endIndex,t, math::float1( (goalIter < kLeftHandGoal ? apHuman->m_LegStretch : apHuman->m_ArmStretch) * weightT),apSkeletonPose,apSkeletonPoseWorkspaceA);

				// 2 bone ik
				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,endIndex,topIndex);
				skeleton::Skeleton2BoneIK(apHuman->m_Skeleton.Get(),topIndex,midIndex,endIndex,apHumanPose->m_GoalArray[goalIter].m_X.t,weightT,apSkeletonPose,apSkeletonPoseWorkspaceA);
			}
		}

		// end rotation
		for(goalIter = kLeftFootGoal; goalIter <= kRightHandGoal; goalIter++)
		{
			int32_t endIndex = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_EndIndex];

			if(apHumanPose->m_GoalArray[goalIter].m_WeightR > 0)
			{
				float weightR = math::saturate(apHumanPose->m_GoalArray[goalIter].m_WeightR);

				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspaceA,endIndex,hipsIndex);

				int32_t index = apHuman->m_HumanBoneIndex[s_HumanGoalInfo[goalIter].m_Index];

				math::float4 q = AddAxis(apHuman,index,apSkeletonPoseWorkspaceA->m_X[index].q);

				q = math::quatLerp(q,apHumanPose->m_GoalArray[goalIter].m_X.q,math::float1(weightR));
					
				q = RemoveAxis(apHuman,index,q);

				ReachGoalRotation(apHuman,q,goalIter,apSkeletonPose,apSkeletonPoseWorkspaceA,apSkeletonPoseWorkspaceB);
			}
		}

		/* no finger ik for 4.0
		if(apHuman->m_HasLeftHand)
		{
			if(apHumanPose->m_LeftHandPose.m_Grab > 0)
			{
				hand::HandPose pose;
				hand::HandPoseSolve(&apHumanPose->m_LeftHandPose,&pose);
				hand::Hand2SkeletonPose(apHuman->m_LeftHand,apHuman->m_Skeleton.Get(),&pose,apSkeletonPose);

				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspace);
				
				hand::Hand *hand = apHuman->m_LeftHand; 
				
				float wArray[hand::kLastFinger];
				math::float4 posArray[hand::kLastFinger];

				hand::FingerTipsFromPose(hand,apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspace,posArray);

				for(int i = 0; i < hand::kLastFinger; i++)
				{
					posArray[i] = SphereCollide(apHumanPose->m_LeftHandPose.m_GrabX,posArray[i]);
					wArray[i] = apHumanPose->m_LeftHandPose.m_Grab;
				}
				
				hand::FingersIKSolve(hand,apHuman->m_Skeleton.Get(),posArray,wArray,apSkeletonPose,apSkeletonPoseWorkspace);
			}
		}

		if(apHuman->m_HasRightHand)
		{
			if(apHumanPose->m_RightHandPose.m_Grab > 0)
			{
				hand::HandPose pose;
				hand::HandPoseSolve(&apHumanPose->m_RightHandPose,&pose);
				hand::Hand2SkeletonPose(apHuman->m_RightHand,apHuman->m_Skeleton.Get(),&pose,apSkeletonPose);

				skeleton::SkeletonPoseComputeGlobal(apHuman->m_Skeleton.Get(),apSkeletonPose,apSkeletonPoseWorkspace);
				
				hand::Hand *hand = apHuman->m_RightHand; 
				
				float wArray[hand::kLastFinger];
				math::float4 posArray[hand::kLastFinger];

				hand::FingerTipsFromPose(hand,apHuman->m_Skeleton.Get(),apSkeletonPoseWorkspace,posArray);

				for(int i = 0; i < hand::kLastFinger; i++)
				{
					posArray[i] = SphereCollide(apHumanPose->m_RightHandPose.m_GrabX,posArray[i]);
					wArray[i] = apHumanPose->m_RightHandPose.m_Grab;
				}
				
				hand::FingersIKSolve(hand,apHuman->m_Skeleton.Get(),posArray,wArray,apSkeletonPose,apSkeletonPoseWorkspace);
			}
		}
		*/
	}

	void TwistSolve(Human const *apHuman, skeleton::SkeletonPose *apSkeletonPose, skeleton::SkeletonPose *skeletonPoseWorkspace)
	{
		const math::float1 foreArmTwist(apHuman->m_ForeArmTwist);
		const math::float1 armTwist(apHuman->m_ArmTwist);		
		const math::float1 legTwist(apHuman->m_LegTwist);
		const math::float1 upperLegTwist(apHuman->m_UpperLegTwist);

		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kLeftLowerArm,kLeftHand,foreArmTwist);
		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kLeftUpperArm,kLeftLowerArm,armTwist);
	
		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kRightLowerArm,kRightHand,foreArmTwist);
		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kRightUpperArm,kRightLowerArm,armTwist);

		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kLeftLowerLeg,kLeftFoot,legTwist);
		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kLeftUpperLeg,kLeftLowerLeg,upperLegTwist);

		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kRightLowerLeg,kRightFoot,legTwist);
		HumanFixTwist(apHuman,apSkeletonPose,skeletonPoseWorkspace,kRightUpperLeg,kRightLowerLeg,upperLegTwist);
	}

	float ComputeHierarchicMass(int32_t aBoneIndex,float *apMassArray)
	{
		apMassArray[aBoneIndex] = HumanBoneDefaultMass[aBoneIndex];

		for(int childIter = 0; childIter < BoneChildren[aBoneIndex][0]; childIter++)
		{
			apMassArray[aBoneIndex] += ComputeHierarchicMass(BoneChildren[aBoneIndex][1+childIter],apMassArray);
		}

		return apMassArray[aBoneIndex];
	}

	float DeltaPoseQuality(HumanPose &arDeltaPose, float aTol)
	{
		float massArray[kLastBone];
		ComputeHierarchicMass(0,massArray);

		float q = 0;
		float sumW = 0;

		for(int dofIter = 0; dofIter < kLastDoF; dofIter++)
		{
			int32_t boneIndex = DoF2Bone[dofIter];
		
			float v = math::saturate((aTol - math::abs(arDeltaPose.m_DoFArray[dofIter]))/aTol); 

			q += v * massArray[boneIndex];  
		
			sumW += massArray[boneIndex];
		}

		return q / sumW;		
	}

} // namespace human

}
