#include "UnityPrefix.h"

#include "AnimationUtility.h"
#include "AnimationClip.h"
#include "BoundCurveDeprecated.h"
#include "Animator.h"
#include "AnimationState.h"
#include "AnimationBinder.h"

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Graphics/Transform.h"

#include "Runtime/BaseClasses/IsPlaying.h"

#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/mecanim/skeleton/skeleton.h"

#include "Runtime/Misc/GameObjectUtility.h"

PROFILER_INFORMATION (gSampleAnimationClip, "GameObject.SampleAnimation", kProfilerAnimation);

void SampleEulerHint (Transform& transform, AnimationClip& clip, float time)
{
	// Sample euler hint curves, only in edit mode
	// This is necessary for euler angles to look sensible when sampling animations
#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		AnimationClip::FloatCurves& eulerCurves = clip.GetEulerEditorCurves();
		for (AnimationClip::FloatCurves::iterator i=eulerCurves.begin();i != eulerCurves.end();i++)
		{
			if (!i->curve.IsValid ())
				continue;
			
			// Lookup without path
			Transform* child = &transform;
			if (!i->path.empty())
			{
				child = FindRelativeTransformWithPath(*child, i->path.c_str());
				if (child == NULL)
					continue;
			}
			
			if (!i->attribute.empty())
			{
				int c = i->attribute[i->attribute.size()-1] - 'x';
				c = clamp(c, 0, 2);
				float value = i->curve.EvaluateClamp(time);
				child->m_LocalEulerAnglesHint[c] = value;
			}		
		}
	}
#endif
}


void SampleAnimation (Unity::GameObject& go, AnimationClip& clip, float inTime, int wrapMode)
{
	PROFILER_AUTO(gSampleAnimationClip, &go)

	AnimationClip::QuaternionCurves& rot = clip.GetRotationCurves();
	AnimationClip::Vector3Curves& pos = clip.GetPositionCurves();
	AnimationClip::Vector3Curves& scale = clip.GetScaleCurves();
	AnimationClip::FloatCurves& floats = clip.GetFloatCurves();

	CurveID curveID;
	BoundCurveDeprecated bind;

	float time = WrapTime(inTime, clip.GetRange(), wrapMode);

	AnimationBinder& binder = GetAnimationBinder();
	Transform& transform = go.GetComponent(Transform);

	Animator* animator = go.QueryComponent(Animator);
	if (animator)
	{
		if (animator->Sample (clip, inTime))
		{	
			SampleEulerHint (transform, clip, time);
			return;
		}
	}

	// Sample rotations
	for (AnimationClip::QuaternionCurves::iterator i=rot.begin();i != rot.end();i++)
	{
		AnimationCurveQuat& curve = i->curve;
		if (!curve.IsValid ())
			continue;

		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalRotation", 0);
		if (binder.BindCurve(curveID, bind, transform))
		{
			Quaternionf result = NormalizeSafe(curve.EvaluateClamp(time));
			DebugAssertIf(!IsFinite(result));

			*reinterpret_cast<Quaternionf*>(bind.targetPtr) = result;
			bind.targetObject->AwakeFromLoad(kDefaultAwakeFromLoad);
			bind.targetObject->SetDirty();
		}
	}

	// Sample positions
	for (AnimationClip::Vector3Curves::iterator i=pos.begin();i != pos.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;

		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalPosition", 0);
		if (binder.BindCurve(curveID, bind, transform))
		{
			Vector3f result = i->curve.EvaluateClamp(time);
			DebugAssertIf(!IsFinite(result));

			*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
			bind.targetObject->AwakeFromLoad(kDefaultAwakeFromLoad);
			bind.targetObject->SetDirty();
		}
	}


	// Sample scale
	for (AnimationClip::Vector3Curves::iterator i=scale.begin();i != scale.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;

		curveID = CurveID (i->path.c_str(), ClassID(Transform), NULL, "m_LocalScale", 0);
		if (binder.BindCurve(curveID, bind, transform))
		{
			Vector3f result = i->curve.EvaluateClamp(time);
			DebugAssertIf(!IsFinite(result));

			*reinterpret_cast<Vector3f*>(bind.targetPtr) = result;
			bind.targetObject->AwakeFromLoad(kDefaultAwakeFromLoad);
			bind.targetObject->SetDirty();
		}
	}

	// Sample arbitrary floats
	for (AnimationClip::FloatCurves::iterator i=floats.begin();i != floats.end();i++)
	{
		if (!i->curve.IsValid ())
			continue;

		curveID = CurveID (i->path.c_str(), i->classID, i->script, i->attribute.c_str(), 0);
		if (binder.BindCurve(curveID, bind, transform))
		{
			float result = i->curve.EvaluateClamp(time);
			DebugAssertIf(!IsFinite(result));

			AnimationBinder::SetFloatValue(bind, result);
			AnimationBinder::SetValueAwakeGeneric(bind);
		}
	}
		
	SampleEulerHint (transform, clip, time);
}


mecanim::crc32 AppendPathToHash(const mecanim::crc32& nameHash, const char* path)
{
	mecanim::crc32 childNameHash = nameHash;
	if (childNameHash.checksum() != 0)
		childNameHash.process_bytes("/", 1);
	childNameHash.process_bytes(path, strlen(path));
	return childNameHash;
}

Transform* FindChildWithID( Transform* transform,  const mecanim::crc32& nameHash, mecanim::uint32_t id, mecanim::crc32& outFoundNameHash, int childStartHint)
{
	int childCount = transform->GetChildrenCount();	
	for (int i = 0 ; i < childCount ; ++i)
	{
		mecanim::crc32 currentHash = AppendPathToHash(nameHash, transform->GetChild((i+childStartHint)%childCount).GetName());
		if(id == currentHash.checksum())
		{
			outFoundNameHash = currentHash;
			return &transform->GetChild((i+childStartHint)%childCount); 
		}
	}

	return 0;
}



int HiearchyMatches(Transform* transform, const mecanim::skeleton::Skeleton* skeleton, int skeletonIndex, const mecanim::crc32& nameHash)
{	
	int currentMatchCount = 0 ;				
	int childStartHint = 0 ;

	for(int i = skeletonIndex ; i < skeleton->m_Count ; i++)
	{	
		if(skeleton->m_Node[i].m_ParentId == skeletonIndex)
		{		
			mecanim::crc32 childHash;
			Transform* childTransform = FindChildWithID(transform, nameHash, skeleton->m_ID[i], childHash, childStartHint);
		
			if(childTransform)
			{	
				currentMatchCount++;						
				currentMatchCount += HiearchyMatches(childTransform, skeleton, i, childHash);
				childStartHint++;
			}		
			
		}
	}		

	return currentMatchCount;
}

int HiearchyMatchesOpt(Transform* transform, const mecanim::skeleton::Skeleton* skeleton, const mecanim::uint32_t* nameArray)
{
	int matchCount = 0;

	for (Transform::iterator transformIt = transform->begin(); transformIt != transform->end(); ++transformIt)
	{
		for(mecanim::int32_t i = 0 ; i < skeleton->m_Count ; ++i)
		{
			if(nameArray[i] == mecanim::processCRC32((*transformIt)->GetName()))
			{
				matchCount++;
				continue;
			}
		}
	}

	return matchCount;
}


void BuildTransformList(Transform& root, dynamic_array<Transform*>& outTransforms)
{
	outTransforms.push_back(&root);	
	int childCount = root.GetChildrenCount();	
	for (int i = 0 ; i < childCount ; ++i)	
		BuildTransformList(root.GetChild(i), outTransforms);	
}
	
Transform* FindAvatarRoot(const mecanim::skeleton::Skeleton* skeleton, const mecanim::uint32_t* nameArray, Transform& root, bool hasTransformHierarchy)
{		
	int bestMatchCount = 0; 
	Transform* animationRoot = 0;

	dynamic_array<Transform*> allTransforms (kMemTempAlloc);
	allTransforms.reserve(skeleton->m_Count*2);
	BuildTransformList(root, allTransforms);	
				
	for(int i = 0 ; i < allTransforms.size() ; ++i)
	{
		int currentMatchCount = 0 ;		
		
		if(hasTransformHierarchy)			
			currentMatchCount = HiearchyMatches(allTransforms[i], skeleton, 0, mecanim::crc32()); // find the transform that matches best the Avatar Hierarchy		
		else
			currentMatchCount = HiearchyMatchesOpt(allTransforms[i], skeleton, nameArray); // find the Transform that has the most direct childs in the Avatar Skeleton
						
		if(currentMatchCount > bestMatchCount)
		{
			bestMatchCount = currentMatchCount;
			animationRoot = allTransforms[i];
		}

		if( bestMatchCount >= (allTransforms.size() - 1 - i) ) // early out, since we cant have a better score than the remaning transform count.
			return animationRoot;
	}
	
	return animationRoot;	
}
