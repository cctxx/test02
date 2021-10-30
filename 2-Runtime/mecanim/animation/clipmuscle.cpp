#include "UnityPrefix.h"
#include "Runtime/mecanim/animation/clipmuscle.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/mecanim/generic/stringtable.h"

namespace mecanim
{

namespace animation
{
	struct MuscleIndexId
	{
		mecanim::uint32_t index;
		mecanim::uint32_t id;
	};
	
	static mecanim::String s_ClipMuscleNameArray[s_ClipMuscleCurveCount];
	static MuscleIndexId s_ClipMuscleIndexIDArray[s_ClipMuscleCurveCount];

	static void ClipMuscleNameArrayInit()
	{		
		int i,j;

		i = 0;
		s_ClipMuscleNameArray[i++] = "MotionT.x";
		s_ClipMuscleNameArray[i++] = "MotionT.y";
		s_ClipMuscleNameArray[i++] = "MotionT.z";

		s_ClipMuscleNameArray[i++] = "MotionQ.x";
		s_ClipMuscleNameArray[i++] = "MotionQ.y";
		s_ClipMuscleNameArray[i++] = "MotionQ.z";
		s_ClipMuscleNameArray[i++] = "MotionQ.w";

		s_ClipMuscleNameArray[i++] = "RootT.x";
		s_ClipMuscleNameArray[i++] = "RootT.y";
		s_ClipMuscleNameArray[i++] = "RootT.z";

		s_ClipMuscleNameArray[i++] = "RootQ.x";
		s_ClipMuscleNameArray[i++] = "RootQ.y";
		s_ClipMuscleNameArray[i++] = "RootQ.z";
		s_ClipMuscleNameArray[i++] = "RootQ.w";

		for(j=0;j<mecanim::human::kLastGoal;j++)
		{
			mecanim::String nameT(mecanim::human::BoneName(mecanim::human::s_HumanGoalInfo[j].m_Index));
			nameT += "T";
			mecanim::String nameTx = nameT;
			nameTx += ".x";
			mecanim::String nameTy = nameT;
			nameTy += ".y";
			mecanim::String nameTz = nameT;
			nameTz += ".z";

			mecanim::String nameQ(mecanim::human::BoneName(mecanim::human::s_HumanGoalInfo[j].m_Index));
			nameQ += "Q";
			mecanim::String nameQx = nameQ;
			nameQx += ".x";
			mecanim::String nameQy = nameQ;
			nameQy += ".y";
			mecanim::String nameQz = nameQ;
			nameQz += ".z";
			mecanim::String nameQw = nameQ;
			nameQw += ".w";
		
			s_ClipMuscleNameArray[i++] = nameTx;
			s_ClipMuscleNameArray[i++] = nameTy;
			s_ClipMuscleNameArray[i++] = nameTz;
																					   
			s_ClipMuscleNameArray[i++] = nameQx;
			s_ClipMuscleNameArray[i++] = nameQy;
			s_ClipMuscleNameArray[i++] = nameQz;
			s_ClipMuscleNameArray[i++] = nameQw;
		}

		for(j=0;j<mecanim::human::kLastDoF;j++)
		{
			s_ClipMuscleNameArray[i++] = mecanim::human::MuscleName(j);
		}

		for(int f=0;f<mecanim::hand::kLastFinger;++f)
		{
			for(int d=0;d<mecanim::hand::kLastFingerDoF;++d)
			{
				mecanim::String name("LeftHand.");
				name += mecanim::hand::FingerName(f);
				name += ".";
				name += mecanim::hand::FingerDoFName(d);

				s_ClipMuscleNameArray[i++] = name;
			}
		}

		for(int f=0;f<mecanim::hand::kLastFinger;++f)
		{
			for(int d=0;d<mecanim::hand::kLastFingerDoF;++d)
			{
				mecanim::String name("RightHand.");
				name += mecanim::hand::FingerName(f);
				name += ".";
				name += mecanim::hand::FingerDoFName(d);

				s_ClipMuscleNameArray[i++] = name;
			}
		}	
	}

	static bool MuscleIndexIdSortFunction(MuscleIndexId i, MuscleIndexId j) 
	{ 
		return i.id < j.id;
	}
	
	static void ClipMuscleIDArrayInit()
	{		
		for(int i = 0; i < s_ClipMuscleCurveCount; i++)
		{
			s_ClipMuscleIndexIDArray[i].index = i;
			s_ClipMuscleIndexIDArray[i].id = mecanim::processCRC32(GetMuscleCurveName(i));
		}
		
		std::sort(s_ClipMuscleIndexIDArray, s_ClipMuscleIndexIDArray + s_ClipMuscleCurveCount, MuscleIndexIdSortFunction);
	}
	
	void InitializeMuscleClipTables ()
	{
		ClipMuscleNameArrayInit();
		ClipMuscleIDArrayInit();
	}
	
	
	mecanim::String const &GetMuscleCurveName(int32_t curveIndex)
	{
		Assert(s_ClipMuscleNameArray != NULL);
		return s_ClipMuscleNameArray[curveIndex];
	}

	class MuscleIndexIdFindfunction
	{ 
	public:
		
		uint32_t mId; 
		
		MuscleIndexIdFindfunction(uint32_t id) : mId(id) {}

		bool operator()(const MuscleIndexId &indexId)
		{	
			return indexId.id == mId;
		}
	};

	int32_t FindMuscleIndex(uint32_t id)
	{
		Assert(s_ClipMuscleIndexIDArray[0].id != 0);

		MuscleIndexId* end = s_ClipMuscleIndexIDArray + s_ClipMuscleCurveCount;
		
		const MuscleIndexId* found = std::find_if(s_ClipMuscleIndexIDArray, end, MuscleIndexIdFindfunction(id));
		if (found != end)
		{
			Assert(found->id == id);
			return found->index;
		}	
		else
			return -1;
	}

	int32_t GetMuscleCurveTQIndex(int32_t curveIndex) 
	{ 
		return (curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount) ? curveIndex%s_ClipMuscleCurveTQCount : -1; 
	};

	float GetXformCurveValue(math::xform const& x, int32_t index)
	{
		float ret = 0;

		if(index == 0)
		{
			ret = x.t.x().tofloat();
		}
		else if(index == 1)
		{
			ret = x.t.y().tofloat();
		}
		else if(index == 2)
		{
			ret = x.t.z().tofloat();
		}
		else if(index == 3)
		{
			ret = x.q.x().tofloat();
		}
		else if(index == 4)
		{
			ret = x.q.y().tofloat();
		}
		else if(index == 5)
		{
			ret = x.q.z().tofloat();
		}
		else if(index == 6)
		{
			ret = x.q.w().tofloat();
		}
	
		return ret;
	}

	float GetMuscleCurveValue(human::HumanPose const& pose, math::xform const &motionX, int32_t curveIndex)
	{
		float ret = 0;

		if(curveIndex < s_ClipMuscleCurveTQCount)
		{
			ret = GetXformCurveValue(motionX,curveIndex);
		}
		else if(curveIndex < 2*s_ClipMuscleCurveTQCount)
		{
			ret = GetXformCurveValue(pose.m_RootX,curveIndex-s_ClipMuscleCurveTQCount);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount)
		{
			int index = curveIndex - (2*s_ClipMuscleCurveTQCount); 
			int goalIndex = index / s_ClipMuscleCurveTQCount;
			int xformIndex = index % s_ClipMuscleCurveTQCount;
			
			ret = GetXformCurveValue(pose.m_GoalArray[goalIndex].m_X,xformIndex);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF)
		{
			int dofIndex = curveIndex - (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount;
			ret = pose.m_DoFArray[dofIndex];
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF + hand::s_DoFCount)
		{
			int dofIndex = curveIndex - ((2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF);
			ret = pose.m_LeftHandPose.m_DoFArray[dofIndex];
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF + 2 * hand::s_DoFCount)
		{
			int dofIndex = curveIndex - ((2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF + hand::s_DoFCount);
			ret = pose.m_RightHandPose.m_DoFArray[dofIndex];
		}

		return ret;
	}

	bool GetMuscleCurveInMask(human::HumanPoseMask const& mask, int32_t curveIndex)
	{
		bool ret = false;
		
		if(curveIndex < s_ClipMuscleCurveTQCount) // root motion
		{
			ret = true;
		}
		else if(curveIndex < 2*s_ClipMuscleCurveTQCount) // root xform
		{
			ret = mask.test(human::kMaskRootIndex);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount)
		{
			int index = curveIndex - (2*s_ClipMuscleCurveTQCount); 
			int goalIndex = index / s_ClipMuscleCurveTQCount;
			
			ret = mask.test(human::kMaskGoalStartIndex+goalIndex);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount+ human::kLastDoF)
		{
			int dofIndex = curveIndex - (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount;
			ret = mask.test(human::kMaskDoFStartIndex+dofIndex);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF + hand::s_DoFCount)
		{
			ret = mask.test(human::kMaskLeftHand);
		}
		else if(curveIndex < (2 + human::kLastGoal) * s_ClipMuscleCurveTQCount + human::kLastDoF + 2 * hand::s_DoFCount)
		{
			ret = mask.test(human::kMaskRightHand);
		}

		return ret;
	}

	ClipMuscleConstant* CreateClipMuscleConstant(Clip * clip, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ClipMuscleConstant);

		ClipMuscleConstant *clipMuscle = alloc.Construct<ClipMuscleConstant>();

		clipMuscle->m_Clip = clip;
		clipMuscle->m_Mirror = false;

		clipMuscle->m_StartTime = 0;
		clipMuscle->m_StopTime = 1;

		clipMuscle->m_Level = 0;
	
		clipMuscle->m_CycleOffset = 0;
		clipMuscle->m_LoopTime = false;
		clipMuscle->m_LoopBlend = false;
		clipMuscle->m_LoopBlendOrientation = false;
		clipMuscle->m_LoopBlendPositionY = false;
		clipMuscle->m_LoopBlendPositionXZ = false;
		clipMuscle->m_KeepOriginalOrientation = false;
		clipMuscle->m_KeepOriginalPositionY = true;
		clipMuscle->m_KeepOriginalPositionXZ = false;
		clipMuscle->m_HeightFromFeet = false;

		clipMuscle->m_ValueArrayCount = GetClipCurveCount(*clip);
		clipMuscle->m_ValueArrayDelta = alloc.ConstructArray<ValueDelta>(clipMuscle->m_ValueArrayCount);

		return clipMuscle;
	}

	void DestroyClipMuscleConstant(ClipMuscleConstant * constant, memory::Allocator& alloc)
	{
		if(constant)
		{
			alloc.Deallocate(constant->m_ValueArrayDelta);
			alloc.Deallocate(constant);
		}
	}
	
	void MotionOutputClear(MotionOutput *output)
	{
		output->m_DX.t = math::float4::zero(); 
		output->m_DX.q = math::quatIdentity();
		output->m_DX.s = math::float4::one(); 

		output->m_MotionX.t = math::float4::zero(); 
		output->m_MotionX.q = math::quatIdentity();
		output->m_MotionX.s = math::float4::one(); 

		output->m_MotionStartX.t = math::float4::zero(); 
		output->m_MotionStartX.q = math::quatIdentity();
		output->m_MotionStartX.s = math::float4::one(); 

		output->m_MotionStopX.t = math::float4::zero(); 
		output->m_MotionStopX.q = math::quatIdentity();
		output->m_MotionStopX.s = math::float4::one(); 

		output->m_PrevRootX.t = math::float4::zero(); 
		output->m_PrevRootX.q = math::quatIdentity();
		output->m_PrevRootX.s = math::float4::one(); 

		output->m_PrevLeftFootX.t = math::float4::zero(); 
		output->m_PrevLeftFootX.q = math::quatIdentity();
		output->m_PrevLeftFootX.s = math::float4::one(); 

		output->m_PrevRightFootX.t = math::float4::zero(); 
		output->m_PrevRightFootX.q = math::quatIdentity();
		output->m_PrevRightFootX.s = math::float4::one(); 

		output->m_TargetX.t = math::float4::zero(); 
		output->m_TargetX.q = math::quatIdentity();
		output->m_TargetX.s = math::float4::one(); 

		output->m_GravityWeight = 0;
	}

	void MotionOutputCopy(MotionOutput *output, MotionOutput const *motion, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask)
	{
		if(hasRootMotion || (isHuman && poseMask.test(human::kMaskRootIndex)))
		{
			output->m_DX = motion->m_DX;
			output->m_GravityWeight = motion->m_GravityWeight;
		}

		if(hasRootMotion)
		{
			output->m_MotionX = motion->m_MotionX;				
			output->m_MotionStartX = motion->m_MotionStartX;				
			output->m_MotionStopX = motion->m_MotionStopX;				
		}

		if(isHuman)
		{
			if(human::MaskHasLegs(poseMask))
			{
				output->m_PrevRootX = motion->m_PrevRootX;				
				output->m_PrevLeftFootX = motion->m_PrevLeftFootX;				
				output->m_PrevRightFootX = motion->m_PrevRightFootX;				
			}

			if(poseMask.test(human::kMaskRootIndex))
			{
				output->m_TargetX = motion->m_TargetX;				
			}
		}
	}

	void MotionOutputBlend(MotionOutput *output, MotionOutput **outputArray, float *weight, uint32_t count, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask)
	{
		output->m_DX.t = math::float4::zero(); 
		output->m_DX.q = math::float4::zero(); 
		output->m_DX.s = math::float4::one(); 

		output->m_MotionX.t = math::float4::zero(); 
		output->m_MotionX.q = math::float4::zero(); 
		output->m_MotionX.s = math::float4::one(); 

		output->m_MotionStartX.t = math::float4::zero(); 
		output->m_MotionStartX.q = math::float4::zero(); 
		output->m_MotionStartX.s = math::float4::one(); 

		output->m_MotionStopX.t = math::float4::zero(); 
		output->m_MotionStopX.q = math::float4::zero(); 
		output->m_MotionStopX.s = math::float4::one(); 

		output->m_PrevRootX.t = math::float4::zero(); 
		output->m_PrevRootX.q = math::float4::zero(); 
		output->m_PrevRootX.s = math::float4::one(); 

		output->m_PrevLeftFootX.t = math::float4::zero(); 
		output->m_PrevLeftFootX.q = math::float4::zero(); 
		output->m_PrevLeftFootX.s = math::float4::one(); 

		output->m_PrevRightFootX.t = math::float4::zero(); 
		output->m_PrevRightFootX.q = math::float4::zero(); 
		output->m_PrevRightFootX.s = math::float4::one(); 

		output->m_TargetX.t = math::float4::zero(); 
		output->m_TargetX.q = math::float4::zero(); 
		output->m_TargetX.s = math::float4::one(); 

		output->m_GravityWeight = 0;

		float sumW = 0;

		for(int iter = 0; iter < count; iter++)
		{
			float w = weight[iter]; 

			math::float1 w1(w);

			sumW += w;

			if(hasRootMotion || (isHuman && poseMask.test(human::kMaskRootIndex)))
			{
				output->m_DX.t += outputArray[iter]->m_DX.t*w1; 
				output->m_DX.q += math::cond(math::dot(output->m_DX.q,outputArray[iter]->m_DX.q) < math::float1::zero(),outputArray[iter]->m_DX.q * -w1,outputArray[iter]->m_DX.q * w1); 

				output->m_GravityWeight += outputArray[iter]->m_GravityWeight*w;
			}

			if(hasRootMotion)
			{
				output->m_MotionX.t += outputArray[iter]->m_MotionX.t*w1; 
				output->m_MotionX.q += math::cond(math::dot(output->m_MotionX.q,outputArray[iter]->m_MotionX.q) < math::float1::zero(),outputArray[iter]->m_MotionX.q * -w1,outputArray[iter]->m_MotionX.q * w1); 

				output->m_MotionStartX.t += outputArray[iter]->m_MotionStartX.t*w1; 
				output->m_MotionStartX.q += math::cond(math::dot(output->m_MotionStartX.q,outputArray[iter]->m_MotionStartX.q) < math::float1::zero(),outputArray[iter]->m_MotionStartX.q * -w1,outputArray[iter]->m_MotionStartX.q * w1); 

				output->m_MotionStopX.t += outputArray[iter]->m_MotionStopX.t*w1; 
				output->m_MotionStopX.q += math::cond(math::dot(output->m_MotionStopX.q,outputArray[iter]->m_MotionStopX.q) < math::float1::zero(),outputArray[iter]->m_MotionStopX.q * -w1,outputArray[iter]->m_MotionStopX.q * w1); 
			}

			if(isHuman)
			{
				if(human::MaskHasLegs(poseMask))
				{
					output->m_PrevRootX.t += outputArray[iter]->m_PrevRootX.t*w1; 
					output->m_PrevRootX.q += math::cond(math::dot(output->m_PrevRootX.q,outputArray[iter]->m_PrevRootX.q) < math::float1::zero(),outputArray[iter]->m_PrevRootX.q * -w1,outputArray[iter]->m_PrevRootX.q * w1); 

					output->m_PrevLeftFootX.t += outputArray[iter]->m_PrevLeftFootX.t*w1; 
					output->m_PrevLeftFootX.q += math::cond(math::dot(output->m_PrevLeftFootX.q,outputArray[iter]->m_PrevLeftFootX.q) < math::float1::zero(),outputArray[iter]->m_PrevLeftFootX.q * -w1,outputArray[iter]->m_PrevLeftFootX.q * w1); 

					output->m_PrevRightFootX.t += outputArray[iter]->m_PrevRightFootX.t*w1; 
					output->m_PrevRightFootX.q += math::cond(math::dot(output->m_PrevRightFootX.q,outputArray[iter]->m_PrevRightFootX.q) < math::float1::zero(),outputArray[iter]->m_PrevRightFootX.q * -w1,outputArray[iter]->m_PrevRightFootX.q * w1); 
				}

				if(poseMask.test(human::kMaskRootIndex))
				{
					output->m_TargetX.t += outputArray[iter]->m_TargetX.t*w1; 
					output->m_TargetX.q += math::cond(math::dot(output->m_TargetX.q,outputArray[iter]->m_TargetX.q) < math::float1::zero(),outputArray[iter]->m_TargetX.q * -w1,outputArray[iter]->m_TargetX.q * w1); 
				}
			}
		}

		math::float4 q(0,0,0,math::saturate(1.0f-sumW));
		
		if(hasRootMotion || (isHuman && poseMask.test(human::kMaskRootIndex)))
		{
			output->m_DX.q = math::normalize(output->m_DX.q+q);
			if(sumW > 0) output->m_GravityWeight /= sumW;
		}

		if(hasRootMotion)
		{
			output->m_MotionX.q = math::normalize(output->m_MotionX.q+q);
			output->m_MotionStartX.q = math::normalize(output->m_MotionStartX.q+q);
			output->m_MotionStopX.q = math::normalize(output->m_MotionStopX.q+q);
		}

		if(isHuman)
		{
			if(human::MaskHasLegs(poseMask))
			{
				output->m_PrevRootX.q = math::normalize(output->m_PrevRootX.q+q);
				output->m_PrevLeftFootX.q = math::normalize(output->m_PrevLeftFootX.q+q);
				output->m_PrevRightFootX.q = math::normalize(output->m_PrevRightFootX.q+q);
			}
			
			if(poseMask.test(human::kMaskRootIndex))
			{
				output->m_TargetX.q = math::normalize(output->m_TargetX.q+q);
			}
		}
	}

	void MotionAddAdditiveLayer(MotionOutput *output, MotionOutput const *motion, float weight, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask)
	{
		math::float1 w(weight);

		if(hasRootMotion || (isHuman && poseMask.test(human::kMaskRootIndex)))
		{
			output->m_DX = math::xformMul(output->m_DX, math::xformWeight(motion->m_DX,w));
			output->m_GravityWeight = output->m_GravityWeight + motion->m_GravityWeight * weight;
		}

		if(hasRootMotion)
		{
			output->m_MotionX = math::xformMul(output->m_MotionX, math::xformWeight(motion->m_MotionX,w));				
			output->m_MotionStartX = math::xformMul(output->m_MotionStartX, math::xformWeight(motion->m_MotionStartX,w));				
			output->m_MotionStopX = math::xformMul(output->m_MotionStopX, math::xformWeight(motion->m_MotionStopX,w));				
		}

		if(isHuman)
		{
			if(human::MaskHasLegs(poseMask))
			{
				output->m_PrevRootX = math::xformMul(output->m_PrevRootX, math::xformWeight(motion->m_PrevRootX,w));				
				output->m_PrevLeftFootX = math::xformMul(output->m_PrevLeftFootX, math::xformWeight(motion->m_PrevLeftFootX,w));				
				output->m_PrevRightFootX = math::xformMul(output->m_PrevRightFootX, math::xformWeight(motion->m_PrevRightFootX,w));				
			}

			if(poseMask.test(human::kMaskRootIndex))
			{
				output->m_TargetX = math::xformMul(output->m_TargetX, math::xformWeight(motion->m_TargetX,w));				
			}
		}
	}

	void MotionAddOverrideLayer(MotionOutput *output, MotionOutput const *motion, float weight, bool hasRootMotion, bool isHuman, human::HumanPoseMask const &poseMask)
	{
		if(weight < 1.0f)
		{
			math::float1 w(weight);

			if(hasRootMotion || (isHuman && poseMask.test(human::kMaskRootIndex)))
			{
				output->m_DX = math::xformBlend(output->m_DX,motion->m_DX,w);
				output->m_GravityWeight = math::lerp(output->m_GravityWeight,motion->m_GravityWeight,weight);
			}

			if(hasRootMotion)
			{
				output->m_MotionX = math::xformBlend(output->m_MotionX,motion->m_MotionX,w);
				output->m_MotionStartX = math::xformBlend(output->m_MotionStartX,motion->m_MotionStartX,w);
				output->m_MotionStopX = math::xformBlend(output->m_MotionStopX,motion->m_MotionStopX,w);
			}

			if(isHuman)
			{
				if(human::MaskHasLegs(poseMask))
				{
					output->m_PrevRootX = math::xformBlend(output->m_PrevRootX,motion->m_PrevRootX,w);
					output->m_PrevLeftFootX = math::xformBlend(output->m_PrevLeftFootX,motion->m_PrevLeftFootX,w);
					output->m_PrevRightFootX = math::xformBlend(output->m_PrevRightFootX,motion->m_PrevRightFootX,w);
				}

				if(poseMask.test(human::kMaskRootIndex))
				{
					output->m_TargetX = math::xformBlend(output->m_TargetX,motion->m_TargetX,w);
				}
			}
		}
		else
		{
			MotionOutputCopy(output,motion,hasRootMotion,isHuman,poseMask);
		}
	}

	ClipMuscleInput* CreateClipMuscleInput(ClipMuscleConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ClipMuscleInput);

		ClipMuscleInput *in = alloc.Construct<ClipMuscleInput>();
		return in;
	}

	void DestroyClipMuscleInput(ClipMuscleInput * input, memory::Allocator& alloc)
	{
		if(input)
		{
			alloc.Deallocate(input);
		}
	}

	float ComputeClipTime(float normalizedTimeIn, float startTime, float stopTime, float cycleOffset, bool loop, bool reverse, float &normalizedTimeOut, float &timeInt)
	{
		float timeFrac = math::cond(loop,math::modf(normalizedTimeIn+cycleOffset,timeInt),math::saturate(normalizedTimeIn));
		normalizedTimeOut  = math::cond(!reverse, timeFrac, 1.f - timeFrac);
		return startTime+normalizedTimeOut*(stopTime-startTime); 
	}

	math::xform EvaluateMotion(ClipMuscleConstant const& constant, ClipMemory &memory, float time)
	{
		math::xform ret;

		float ATTRIBUTE_ALIGN(ALIGN4F) value[4];
				
		ClipInput clipIn;
		clipIn.m_Time = time;

		value[0] = constant.m_IndexArray[0] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[0]) : 0;
		value[1] = constant.m_IndexArray[1] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[1]) : 0;
		value[2] = constant.m_IndexArray[2] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[2]) : 0;
		value[3] = 0.f;
		
		ret.t = math::load(value);

		value[0] = constant.m_IndexArray[3] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[3]) : 0;
		value[1] = constant.m_IndexArray[4] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[4]) : 0;
		value[2] = constant.m_IndexArray[5] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[5]) : 0;
		value[3] = constant.m_IndexArray[6] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[6]) : 1;
		
		ret.q = math::normalize(math::load(value));

		return ret;
	}

	math::xform EvaluateRoot(ClipMuscleConstant const& constant, ClipMemory &memory, float time)
	{
		math::xform ret;

		float ATTRIBUTE_ALIGN(ALIGN4F) value[4];
				
		ClipInput clipIn;
		clipIn.m_Time = time;

		value[0] = constant.m_IndexArray[7] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[7]) : 0;
		value[1] = constant.m_IndexArray[8] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[8]) : 0;
		value[2] = constant.m_IndexArray[9] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[9]) : 0;
		value[3] = 0.f;
		
		ret.t = math::load(value);

		value[0] = constant.m_IndexArray[10] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[10]) : 0;
		value[1] = constant.m_IndexArray[11] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[11]) : 0;
		value[2] = constant.m_IndexArray[12] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[12]) : 0;
		value[3] = constant.m_IndexArray[13] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory, constant.m_IndexArray[13]) : 1;
		
		ret.q = math::normalize(math::load(value));

		return ret;
	}

	math::xform EvaluateGoal(ClipMuscleConstant const& constant, ClipMemory &memory, float time, int32_t goalIndex)
	{
		math::xform ret;

		float ATTRIBUTE_ALIGN(ALIGN4F) value[4];

		const int32_t index = (2 + goalIndex) * s_ClipMuscleCurveTQCount;

		ClipInput clipIn;
		clipIn.m_Time = time;

		value[0] = constant.m_IndexArray[index+0] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+0]) : 0;
		value[1] = constant.m_IndexArray[index+1] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+1]) : 0;
		value[2] = constant.m_IndexArray[index+2] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+2]) : 0;
		value[3] = 0.f;
	
		ret.t = math::load(value);

		value[0] = constant.m_IndexArray[index+3] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+3]) : 0;
		value[1] = constant.m_IndexArray[index+4] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+4]) : 0;
		value[2] = constant.m_IndexArray[index+5] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+5]) : 0;
		value[3] = constant.m_IndexArray[index+6] != -1 ? EvaluateClipAtIndex(constant.m_Clip.Get(), &clipIn, &memory,constant.m_IndexArray[index+6]) : 1;
		
		ret.q = math::normalize(math::load(value));

		return ret;
	}

	math::xform GetMotionX(const ClipMuscleConstant& constant, const ClipOutput &output)
	{
		math::xform ret;

		float ATTRIBUTE_ALIGN(ALIGN4F) value[4];

		uint32_t currentCurveIndex = 0;

		if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
		value[3] = 0;

		ret.t = math::load(value);

		if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[3] = output.m_Values[constant.m_IndexArray[currentCurveIndex]]; else value[3] = 1.0f; currentCurveIndex++;

		ret.q = math::normalize(math::load(value));

		ret.s = math::float4::one();

		return ret;
	}

	template<typename TYPE>
	struct ValueAccessor
	{
		ValueAccessor(const TYPE* t) : m_Values(t)
		{}

		const TYPE* m_Values;

		float operator[](int index)
		{
			return m_Values[index];
		}
	};

	template<> 
	struct ValueAccessor<ValueDelta>
	{
		ValueAccessor(const ValueDelta* t) : m_Values(t)
		{}

		const ValueDelta* m_Values;

		float operator[](int index)
		{
			return m_Values[index].m_Start;
		}
	};
	
	template<typename TYPE>
	void GetHumanPose(const ClipMuscleConstant& constant, const TYPE* values, human::HumanPose &humanPose)
	{
		float ATTRIBUTE_ALIGN(ALIGN4F) value[4];
		
		ValueAccessor<TYPE> accessor(values);

		uint32_t i,currentCurveIndex = s_ClipMotionCurveCount;

		if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
		value[3] = 0;

		humanPose.m_RootX.t = math::load(value);

		if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
		if(constant.m_IndexArray[currentCurveIndex] != -1) value[3] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[3] = 1.0f; currentCurveIndex++;

		humanPose.m_RootX.q = math::normalize(math::load(value));
		humanPose.m_RootX.s = math::float4::one();

		for(i = 0; i < human::kLastGoal; i++)
		{	
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++; 
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
			value[3] = 0;
		
			humanPose.m_GoalArray[i].m_X.t = math::load(value);

			if(constant.m_IndexArray[currentCurveIndex] != -1) value[0] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[0] = 0; currentCurveIndex++;
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[1] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[1] = 0; currentCurveIndex++;
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[2] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[2] = 0; currentCurveIndex++;
			if(constant.m_IndexArray[currentCurveIndex] != -1) value[3] = accessor[constant.m_IndexArray[currentCurveIndex]]; else value[3] = 1.0f; currentCurveIndex++;

		
			humanPose.m_GoalArray[i].m_X.q = math::load(value);
			humanPose.m_GoalArray[i].m_X.q = math::normalize(humanPose.m_GoalArray[i].m_X.q);
		}

		for(i = 0 ; i < human::kLastDoF; i++)
		{
			if(constant.m_IndexArray[currentCurveIndex] != -1) humanPose.m_DoFArray[i] = accessor[constant.m_IndexArray[currentCurveIndex]]; 
			else humanPose.m_DoFArray[i] = 0; 
			currentCurveIndex++;
		}

		for(i = 0 ; i < hand::s_DoFCount; i++)
		{
			if(constant.m_IndexArray[currentCurveIndex] != -1) humanPose.m_LeftHandPose.m_DoFArray[i] = accessor[constant.m_IndexArray[currentCurveIndex]]; 
			else humanPose.m_LeftHandPose.m_DoFArray[i] = 0; 
			currentCurveIndex++;
		}

		for(i = 0 ; i < hand::s_DoFCount; i++)
		{
			if(constant.m_IndexArray[currentCurveIndex] != -1) humanPose.m_RightHandPose.m_DoFArray[i] = accessor[constant.m_IndexArray[currentCurveIndex]]; 
			else humanPose.m_RightHandPose.m_DoFArray[i] = 0; 
			currentCurveIndex++;
		}
	}	

	void GetHumanPose(const ClipMuscleConstant& constant, const float* values, human::HumanPose &humanPose)
	{
		GetHumanPose<float>(constant, values, humanPose);
	}

	void GetHumanPose(const ClipMuscleConstant& constant, const ValueDelta* values, human::HumanPose &humanPose)
	{
		GetHumanPose<ValueDelta>(constant, values, humanPose);
	}


	// root motion extraction WIP
	void EvaluateClipRootMotionDeltaX(const ClipMuscleConstant& constant, const ClipMuscleInput &input, MotionOutput &output, ClipMemory &memory)
	{
		output.m_DX = math::xformIdentity();
		output.m_MotionX = math::xformIdentity();

		if(constant.m_IndexArray[0] != -1)
		{
			float cycleOffset = constant.m_CycleOffset + input.m_CycleOffset;

			float prevTime;
			float prevTimeFrac = math::cond(constant.m_LoopTime,math::modf(input.m_PreviousTime+cycleOffset,prevTime),math::saturate(input.m_PreviousTime));
			prevTimeFrac  = math::cond(!input.m_Reverse, prevTimeFrac, 1.f - prevTimeFrac);
			prevTime = constant.m_StartTime+prevTimeFrac*(constant.m_StopTime-constant.m_StartTime); 

			float currentTime;
			float lTimeFrac = math::cond(constant.m_LoopTime,math::modf(input.m_Time+cycleOffset,currentTime),math::saturate(input.m_Time));
			lTimeFrac  = math::cond(!input.m_Reverse, lTimeFrac, 1.f - lTimeFrac);
			currentTime = constant.m_StartTime+lTimeFrac*(constant.m_StopTime-constant.m_StartTime); 

			math::xform refStartX(constant.m_MotionStartX);
			math::xform refStopX(constant.m_MotionStopX);
			math::xform prevRefX(EvaluateMotion(constant,memory,prevTime));
			math::xform refX(EvaluateMotion(constant,memory,currentTime));

			refStartX.q = math::quatProjOnYPlane(refStartX.q);
			refStopX.q = math::quatProjOnYPlane(refStopX.q);
			prevRefX.q = math::quatProjOnYPlane(prevRefX.q);
			refX.q = math::quatProjOnYPlane(refX.q);

			math::float4 refOffsetT = math::float4(0,constant.m_Level,0,0);
			math::float4 refOffsetQ = math::qtan2Quat(math::float4(0,math::halfTan(math::radians(constant.m_OrientationOffsetY)),0,1));

			if(constant.m_KeepOriginalPositionY) 
			{
				refOffsetT.y() -= refStartX.t.y();
			}

			if(constant.m_KeepOriginalPositionXZ) 
			{
				refOffsetT.x() -= refStartX.t.x();
				refOffsetT.z() -= refStartX.t.z();
			}

			if(constant.m_KeepOriginalOrientation) 
			{
				refOffsetQ = math::normalize(math::quatMul(refOffsetQ,math::quatConj(refStartX.q)));
			}

			refStartX.t = refStartX.t + refOffsetT;
			refStopX.t = refStopX.t + refOffsetT;
			prevRefX.t = prevRefX.t + refOffsetT;
			refX.t = refX.t + refOffsetT;

			refStartX.q = math::normalize(math::quatMul(refOffsetQ,refStartX.q));
			refStopX.q = math::normalize(math::quatMul(refOffsetQ,refStopX.q));
			prevRefX.q = math::normalize(math::quatMul(refOffsetQ,prevRefX.q));
			refX.q = math::normalize(math::quatMul(refOffsetQ,refX.q));

			if(constant.m_LoopBlendOrientation)
			{
				refStopX.q = refStartX.q;
				prevRefX.q = refStartX.q;
				refX.q = refStartX.q;		
			}

			if(constant.m_LoopBlendPositionY) 
			{ 
				refStopX.t.y() = refStartX.t.y(); 
				prevRefX.t.y() = refStartX.t.y();
				refX.t.y() = refStartX.t.y();
			}

			if(constant.m_LoopBlendPositionXZ) 
			{ 
				refStartX.t.x() = refStartX.t.x(); 
				refStopX.t.x() = refStartX.t.x();; 
				prevRefX.t.x() = refStartX.t.x();;
				refX.t.x() = refStartX.t.x();;

				refStartX.t.z() = refStartX.t.z(); 
				refStopX.t.z() = refStartX.t.z();; 
				prevRefX.t.z() = refStartX.t.z();;
				refX.t.z() = refStartX.t.z();;
			}

			output.m_MotionX = refX;
			output.m_MotionStartX = refStartX;
			output.m_MotionStopX = refStopX;

			if(constant.m_LoopTime)
			{
				float deltaTime = math::abs(lTimeFrac - prevTimeFrac);

				if(deltaTime > 0.5f)
				{
					if(lTimeFrac < prevTimeFrac)
					{
						output.m_DX = math::xformInvMulNS(prevRefX,math::xformMul(refStopX,math::xformInvMulNS(refStartX,refX)));
					}
					else if(lTimeFrac > prevTimeFrac)
					{
						output.m_DX = math::xformInvMulNS(prevRefX,math::xformMul(refStartX,math::xformInvMulNS(refStopX,refX)));
					}
				}
				else if(lTimeFrac != prevTimeFrac)
				{
					output.m_DX = math::xformInvMulNS(prevRefX,refX);
				}
			}
			else
			{
				if(input.m_PreviousTime != input.m_Time)
				{
					output.m_DX = math::xformInvMulNS(prevRefX,refX);
				}
			}

			output.m_DX.q = math::quat2Qtan(output.m_DX.q);

			output.m_DX.q.x() = 0;
			output.m_DX.q.z() = 0;

			if(constant.m_LoopBlendPositionY) 
			{ 
				output.m_DX.t.y() = 0;
			}

			if(constant.m_LoopBlendPositionXZ) 
			{
				output.m_DX.t.x() = 0;
				output.m_DX.t.z() = 0;
			}
		
			if(constant.m_LoopBlendOrientation) 
			{
				output.m_DX.q.y() = 0;
			}

			output.m_DX.q = math::qtan2Quat(output.m_DX.q);
		}
	}

	void  EvaluateClipMusclePrevTime(const ClipMuscleConstant& constant, const ClipMuscleInput &input, MotionOutput &output, ClipMemory &memory)
	{
		float prevTimeInt;
		float prevTimeFrac;
		float prevTime = ComputeClipTime(input.m_PreviousTime,constant.m_StartTime,constant.m_StopTime,constant.m_CycleOffset+input.m_CycleOffset,constant.m_LoopTime,input.m_Reverse,prevTimeFrac,prevTimeInt);

		output.m_PrevRootX  = EvaluateRoot(constant,memory,prevTime);
		output.m_PrevLeftFootX = EvaluateGoal(constant,memory,prevTime,human::kLeftFootGoal);
		output.m_PrevRightFootX = EvaluateGoal(constant,memory,prevTime,human::kRightFootGoal);
	}

	void  EvaluateClipMuscle(const ClipMuscleConstant& constant, const ClipMuscleInput &input, const float *valuesOutput, MotionOutput &motionOutput, human::HumanPose &humanPose, ClipMemory &memory)
	{
		float cycleOffset = constant.m_CycleOffset+input.m_CycleOffset;
		float prevTimeInt;
		float prevTimeFrac;
		ComputeClipTime(input.m_PreviousTime,constant.m_StartTime,constant.m_StopTime,cycleOffset,constant.m_LoopTime,input.m_Reverse,prevTimeFrac,prevTimeInt);

		ClipInput in;
		float timeFrac;
		float currentTimeInt;
		in.m_Time = ComputeClipTime(input.m_Time,constant.m_StartTime,constant.m_StopTime,cycleOffset,constant.m_LoopTime,input.m_Reverse,timeFrac,currentTimeInt);

		bool mirror = (constant.m_Mirror || input.m_Mirror) && !(constant.m_Mirror && input.m_Mirror);

		math::xform stopX = math::xformMulInv(constant.m_StartX,constant.m_DeltaPose.m_RootX);

		math::xform prevRootX = motionOutput.m_PrevRootX ;
		math::xform prevLeftFootX = motionOutput.m_PrevLeftFootX;
		math::xform prevRightFootX = motionOutput.m_PrevRightFootX;

		GetHumanPose(constant,valuesOutput,humanPose);
		math::xform rootX = humanPose.m_RootX;
		
		math::xform refStartX(constant.m_StartX.t,math::quatProjOnYPlane(constant.m_StartX.q),constant.m_StartX.s);
		math::xform refStopX(stopX.t,math::quatProjOnYPlane(stopX.q),stopX.s);
		math::xform prevRefX(prevRootX.t,math::quatProjOnYPlane(prevRootX.q),prevRootX.s);
		math::xform refX(rootX.t,math::quatProjOnYPlane(rootX.q),rootX.s);
		
		// todo: target stuff most likely breaks curve cache
		float targetTime = constant.m_StopTime;
		float targetTimeFrac = 1;
		math::xform targetRootX = stopX;
		math::xform targetRefX = refStopX;
		math::xform targetGoalX = math::xformIdentity();
		
		float targetTimeInt = 0;
		if(input.m_TargetTime != 1)
		{
			targetTime = ComputeClipTime(input.m_TargetTime,constant.m_StartTime,constant.m_StopTime,cycleOffset,constant.m_LoopTime,input.m_Reverse,targetTimeFrac,targetTimeInt);

			targetRootX = EvaluateRoot(constant,memory,targetTime);				
			targetRefX = math::xform(targetRootX.t,math::quatProjOnYPlane(targetRootX.q),targetRootX.s);
		}

		int32_t targetGoalIndex = input.m_TargetIndex - kTargetLeftFoot;

		if(targetGoalIndex >= 0) 
		{
			targetGoalIndex = mirror ? ((targetGoalIndex % 2) ? targetGoalIndex-1 : targetGoalIndex+1) : targetGoalIndex; 
			targetGoalX = EvaluateGoal(constant,memory,targetTime,targetGoalIndex);
		}
		//////////////////////////////////////////

		if(constant.m_HeightFromFeet)
		{
			math::xform refLeftFootStartX = math::xformMul(constant.m_StartX,constant.m_LeftFootStartX);
			math::xform refRightFootStartX = math::xformMul(constant.m_StartX,constant.m_RightFootStartX);

			math::xform refLeftFootStopX = math::xformMul(stopX,math::xformMulInv(constant.m_LeftFootStartX,constant.m_DeltaPose.m_GoalArray[human::kLeftFootGoal].m_X));
			math::xform refRightFootStopX = math::xformMul(stopX,math::xformMulInv(constant.m_RightFootStartX,constant.m_DeltaPose.m_GoalArray[human::kRightFootGoal].m_X));
		
			math::xform refLeftFootPrevX = math::xformMul(prevRootX,prevLeftFootX);
			math::xform refRightFootPrevX = math::xformMul(prevRootX,prevRightFootX);

			math::xform refLeftFootX = math::xformMul(rootX,humanPose.m_GoalArray[human::kLeftFootGoal].m_X);
			math::xform refRightFootX = math::xformMul(rootX,humanPose.m_GoalArray[human::kRightFootGoal].m_X);

			math::xform refTargetLeftFootX = math::xformMul(targetRootX,EvaluateGoal(constant,memory,targetTime,human::kLeftFootGoal));
			math::xform refTargetRightFootX = math::xformMul(targetRootX,EvaluateGoal(constant,memory,targetTime,human::kRightFootGoal));

			refStartX.t.y() = math::minimum(refStartX.t,math::minimum(refLeftFootStartX.t,refRightFootStartX.t)).y();
			refStopX.t.y() = math::minimum(refStopX.t,math::minimum(refLeftFootStopX.t,refRightFootStopX.t)).y();
			prevRefX.t.y() = math::minimum(prevRefX.t,math::minimum(refLeftFootPrevX.t,refRightFootPrevX.t)).y();
			refX.t.y() = math::minimum(refX.t,math::minimum(refLeftFootX.t,refRightFootX.t)).y();
			targetRefX.t.y() = math::minimum(targetRefX.t,math::minimum(refTargetLeftFootX.t,refTargetRightFootX.t)).y();
		}

		math::float4 refOffsetT = math::float4(0,constant.m_Level,0,0);
		math::float4 refOffsetQ = math::qtan2Quat(math::float4(0,math::halfTan(math::radians(constant.m_OrientationOffsetY)),0,1));

		if(constant.m_KeepOriginalPositionY) 
		{
			refOffsetT.y() -= refStartX.t.y();
		}

		if(constant.m_KeepOriginalPositionXZ) 
		{
			refOffsetT.x() -= refStartX.t.x();
			refOffsetT.z() -= refStartX.t.z();
		}

		if(constant.m_KeepOriginalOrientation) 
		{
			refOffsetQ = math::normalize(math::quatMul(refOffsetQ,math::quatConj(refStartX.q)));
		}

		refStartX.t = refStartX.t + refOffsetT;
		refStopX.t = refStopX.t + refOffsetT;
		prevRefX.t = prevRefX.t + refOffsetT;
		refX.t = refX.t + refOffsetT;
		targetRefX.t = targetRefX.t + refOffsetT;

		refStartX.q = math::normalize(math::quatMul(refOffsetQ,refStartX.q));
		refStopX.q = math::normalize(math::quatMul(refOffsetQ,refStopX.q));
		prevRefX.q = math::normalize(math::quatMul(refOffsetQ,prevRefX.q));
		refX.q = math::normalize(math::quatMul(refOffsetQ,refX.q));
		targetRefX.q = math::normalize(math::quatMul(refOffsetQ,targetRefX.q));

		if(constant.m_LoopBlendOrientation)
		{
			refStopX.q = refStartX.q;
			prevRefX.q = refStartX.q;
			refX.q = refStartX.q;		
			targetRefX.q = refStartX.q;		
		}

		if(constant.m_LoopBlendPositionY) 
		{ 
			refStopX.t.y() = refStartX.t.y(); 
			prevRefX.t.y() = refStartX.t.y();
			refX.t.y() = refStartX.t.y();
			targetRefX.t.y() = refStartX.t.y();
		}

		if(constant.m_LoopBlendPositionXZ) 
		{ 
			refStartX.t.x() = refStartX.t.x(); 
			refStopX.t.x() = refStartX.t.x();; 
			prevRefX.t.x() = refStartX.t.x();;
			refX.t.x() = refStartX.t.x();;
			targetRefX.t.x() = refStartX.t.x();;

			refStartX.t.z() = refStartX.t.z(); 
			refStopX.t.z() = refStartX.t.z();; 
			prevRefX.t.z() = refStartX.t.z();;
			refX.t.z() = refStartX.t.z();;
			targetRefX.t.z() = refStartX.t.z();;
		}

		prevRootX = math::xformInvMulNS(prevRefX,prevRootX);
		rootX = math::xformInvMulNS(refX,rootX);
		targetRootX = math::xformInvMulNS(targetRefX,targetRootX);
					
		if(constant.m_LoopTime && constant.m_LoopBlend)
		{
			human::HumanPose deltaPose;
			human::HumanPoseWeight(deltaPose,constant.m_DeltaPose,timeFrac);
			human::HumanPoseAdd(humanPose,humanPose,deltaPose);

			prevLeftFootX = math::xformMul(prevLeftFootX,math::xformWeight(constant.m_DeltaPose.m_GoalArray[human::kLeftFootGoal].m_X,math::float1(prevTimeFrac)));
			prevRightFootX = math::xformMul(prevRightFootX,math::xformWeight(constant.m_DeltaPose.m_GoalArray[human::kRightFootGoal].m_X,math::float1(prevTimeFrac)));

			math::xform rootDeltaX = math::xformInvMulNS(math::xformInvMulNS(refStopX,stopX),math::xformInvMulNS(refStartX,constant.m_StartX));
			math::xform prevRootDeltaX = math::xformWeight(rootDeltaX,math::float1(prevTimeFrac));
			math::xform targetRootDeltaX = math::xformWeight(rootDeltaX,math::float1(targetTimeFrac));
			rootDeltaX = math::xformWeight(rootDeltaX,math::float1(timeFrac));
			prevRootX = math::xformMul(prevRootX,prevRootDeltaX);
			rootX = math::xformMul(rootX,rootDeltaX);
			targetRootX = math::xformMul(targetRootX,targetRootDeltaX);
		
			math::xform cycleDelta;
			for(int i = 0 ; i < targetTimeInt - currentTimeInt; i++)
			{
				if(i == 0) cycleDelta = math::xformInvMulNS(refStartX,refStopX);
				targetRefX = math::xformMul(targetRefX,cycleDelta);
			}			
			if(targetGoalIndex >= 0) targetGoalX = math::xformMul(targetGoalX,math::xformWeight(constant.m_DeltaPose.m_GoalArray[targetGoalIndex].m_X,math::float1(targetTimeFrac)));
		}

		humanPose.m_RootX = rootX;

		for(int i = 0; i < human::kLastGoal; i++)
		{
			humanPose.m_GoalArray[i].m_X = math::xformMul(rootX,humanPose.m_GoalArray[i].m_X);
		}

		motionOutput.m_PrevLeftFootX = math::xformMul(prevRootX,prevLeftFootX);
		motionOutput.m_PrevRightFootX = math::xformMul(prevRootX,prevRightFootX);

		motionOutput.m_DX = math::xformIdentity();
		motionOutput.m_MotionX = math::xformIdentity();

		if(constant.m_LoopTime)
		{
			float deltaTime = math::abs(timeFrac - prevTimeFrac);

			if(deltaTime > 0.5f)
			{
				if(timeFrac < prevTimeFrac)
				{
					motionOutput.m_DX = math::xformInvMulNS(prevRefX,math::xformMul(refStopX,math::xformInvMulNS(refStartX,refX)));
				}
				else if(timeFrac > prevTimeFrac)
				{
					motionOutput.m_DX = math::xformInvMulNS(prevRefX,math::xformMul(refStartX,math::xformInvMulNS(refStopX,refX)));
				}
			}
			else if(timeFrac != prevTimeFrac)
			{
				motionOutput.m_DX = math::xformInvMulNS(prevRefX,refX);
			}
		}
		else
		{
			if(input.m_PreviousTime != input.m_Time)
			{
				motionOutput.m_DX = math::xformInvMulNS(prevRefX,refX);
			}
		}

		motionOutput.m_DX.q = math::quat2Qtan(motionOutput.m_DX.q);

		motionOutput.m_DX.q.x() = 0;
		motionOutput.m_DX.q.z() = 0;

		if(constant.m_LoopBlendPositionY) 
		{ 
			motionOutput.m_DX.t.y() = 0;
		}

		if(constant.m_LoopBlendPositionXZ) 
		{
			motionOutput.m_DX.t.x() = 0;
			motionOutput.m_DX.t.z() = 0;
		}
		
		if(constant.m_LoopBlendOrientation) 
		{
			motionOutput.m_DX.q.y() = 0;
		}

		motionOutput.m_DX.q = math::qtan2Quat(motionOutput.m_DX.q);
		motionOutput.m_DX.s = math::float4::one();
		motionOutput.m_TargetX = math::xformInvMulNS(refX,targetRefX);

		if(mirror)
		{
			human::HumanPoseMirror(humanPose,humanPose);

			motionOutput.m_DX = math::mirror(motionOutput.m_DX);

			math::xform x = math::mirror(motionOutput.m_PrevLeftFootX);
			motionOutput.m_PrevLeftFootX = math::mirror(motionOutput.m_PrevRightFootX);
			motionOutput.m_PrevRightFootX = x;
		
			constant_float4(offsetQY,0,1,0,0);
			constant_float4(offsetQZ,0,0,1,0);

			motionOutput.m_PrevLeftFootX.q = math::normalize(math::quatMul(motionOutput.m_PrevLeftFootX.q,offsetQY));
			motionOutput.m_PrevRightFootX.q = math::normalize(math::quatMul(motionOutput.m_PrevRightFootX.q,offsetQY));

			motionOutput.m_TargetX = math::mirror(motionOutput.m_TargetX);

			if(input.m_TargetIndex > kTargetReference)
			{
				targetRootX = math::mirror(targetRootX);

				if(input.m_TargetIndex > kTargetRoot)
				{
					targetGoalX = math::mirror(targetGoalX);
					targetGoalX.q = math::normalize(math::quatMul(targetGoalX.q,math::cond(math::bool4(input.m_TargetIndex > kTargetRightFoot),offsetQZ,offsetQY)));
				}
			}
		}

		if(input.m_TargetIndex > kTargetReference)
		{
			motionOutput.m_TargetX = math::xformMul(motionOutput.m_TargetX,targetRootX);
			
			if(input.m_TargetIndex > kTargetRoot)
			{
				motionOutput.m_TargetX = math::xformMul(motionOutput.m_TargetX,targetGoalX);
			}
		}		

		motionOutput.m_MotionX = refX;
	}	

	void ComputeClipMuscleDeltaPose(ClipMuscleConstant const& constant, float startTime, float stopTime, human::HumanPose &deltaPose, math::xform &startX, math::xform &leftFootStartX, math::xform &rightFootStartX, memory::Allocator& alloc)
	{
		ClipInput in;
		human::HumanPose poseStart;
		human::HumanPose poseStop;

		ClipOutput *clipOutStart = CreateClipOutput(constant.m_Clip.Get(),alloc);
		ClipOutput *clipOutStop = CreateClipOutput(constant.m_Clip.Get(),alloc);
		ClipMemory *mem = CreateClipMemory(constant.m_Clip.Get(), alloc);

		in.m_Time = startTime;
		EvaluateClip(constant.m_Clip.Get(),&in,mem,clipOutStart);
		GetHumanPose(constant,clipOutStart->m_Values,poseStart);

		in.m_Time = stopTime;
		EvaluateClip(constant.m_Clip.Get(),&in,mem,clipOutStop);
		GetHumanPose(constant,clipOutStop->m_Values,poseStop);

		human::HumanPoseSub(deltaPose,poseStart,poseStop);

		startX = poseStart.m_RootX;
		leftFootStartX = poseStart.m_GoalArray[human::kLeftFootGoal].m_X;
		rightFootStartX = poseStart.m_GoalArray[human::kRightFootGoal].m_X;

		DestroyClipOutput(clipOutStart,alloc);
		DestroyClipOutput(clipOutStop,alloc);
		DestroyClipMemory(mem,alloc);
	}

	void InitClipMuscleDeltaValues(ClipMuscleConstant& constant)
	{
		mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);
		
		ClipInput in;
		ClipOutput *outStart = CreateClipOutput(constant.m_Clip.Get(),alloc);
		ClipOutput *outStop = CreateClipOutput(constant.m_Clip.Get(),alloc);
		ClipMemory *mem = CreateClipMemory(constant.m_Clip.Get(), alloc);

		in.m_Time = constant.m_StartTime;
		EvaluateClip(constant.m_Clip.Get(),&in,mem,outStart);

		in.m_Time = constant.m_StopTime;
		EvaluateClip(constant.m_Clip.Get(),&in,mem,outStop);

		constant.m_MotionStartX = GetMotionX(constant,*outStart);
		constant.m_MotionStopX = GetMotionX(constant,*outStop);

		for(int valueIter = 0; valueIter < constant.m_ValueArrayCount; valueIter++)
		{
			constant.m_ValueArrayDelta[valueIter].m_Start = outStart->m_Values[valueIter];
			constant.m_ValueArrayDelta[valueIter].m_Stop = outStop->m_Values[valueIter];
		}

		DestroyClipOutput(outStart,alloc);
		DestroyClipOutput(outStop,alloc);
		DestroyClipMemory(mem,alloc);
	}

	void InitClipMuscleDeltaPose(ClipMuscleConstant& constant)
	{
		mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);
		ComputeClipMuscleDeltaPose(constant,constant.m_StartTime,constant.m_StopTime,constant.m_DeltaPose,constant.m_StartX,constant.m_LeftFootStartX,constant.m_RightFootStartX,alloc);
	}

	void InitClipMuscleAverageSpeed(ClipMuscleConstant& constant, int steps)
	{
		mecanim::memory::MecanimAllocator alloc(kMemTempAlloc);
		
		ClipMemory *mem = CreateClipMemory(constant.m_Clip.Get(), alloc);

		float period = (constant.m_StopTime - constant.m_StartTime) / float(steps);
		float time = constant.m_StartTime;

		math::xform prevRootX;

		math::float4 speed = math::float4::zero();
		float angularSpeed = 0;

		for(int i = 0; i <= steps; i++)
		{
			math::xform rootX = EvaluateRoot(constant,*mem,time);

			math::float4 qYOffset = math::qtan2Quat(math::float4(0,math::halfTan(math::radians(constant.m_OrientationOffsetY)),0,1));		

			if(constant.m_KeepOriginalOrientation) 
			{
				qYOffset = math::normalize(math::quatMul(qYOffset,math::quatConj(constant.m_StartX.q)));
			}

			rootX.q = math::normalize(math::quatMul(qYOffset,math::quatProjOnYPlane(math::cond(math::bool4(constant.m_LoopBlendOrientation),constant.m_StartX.q,rootX.q))));
		
			if(i > 0)
			{
				math::xform dx = math::xformInvMul(prevRootX,rootX);
				if(constant.m_Mirror) dx = math::mirror(dx);
				math::float4 dxdof = math::doubleAtan(math::quat2Qtan(dx.q));
				angularSpeed +=  dxdof.y().tofloat() / period;
				speed = speed + dx.t / math::float1(period);
			}	

			prevRootX = rootX;			
			time += period;
		}

		constant.m_AverageAngularSpeed = angularSpeed / float(steps);

		constant.m_AverageSpeed = speed / math::float1(steps);
		constant.m_AverageSpeed = math::cond(math::bool4(constant.m_LoopBlendPositionXZ,constant.m_LoopBlendPositionY,constant.m_LoopBlendPositionXZ,true),math::float4::zero(),constant.m_AverageSpeed);

		DestroyClipMemory(mem,alloc);
	}
	
	size_t GetClipCurveCount(const ClipMuscleConstant& constant)
	{
		return GetClipCurveCount(*constant.m_Clip);
	}
}

}
