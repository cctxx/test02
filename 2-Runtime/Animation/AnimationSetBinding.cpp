#include "UnityPrefix.h"
#include "AnimationSetBinding.h"
#include "RuntimeAnimatorController.h"
#include "AnimationClipBindings.h"
#include "GenericAnimationBindingCache.h"
#include "AnimationClip.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/mecanim/animation/curvedata.h"

namespace UnityEngine
{
namespace Animation
{
static mecanim::ValueArrayConstant* CreateBindingValueArrayConstant (const GenericBinding* genericBindings, size_t genericBindingsSize, const GenericBinding* genericPPtrBindings, size_t genericPPtrBindingsSize, const TransformBinding* transformBindings, size_t transformBindingsSize, mecanim::memory::Allocator& alloc);
static void CombineUniqueGeneric (AnimationClipBindingConstant** constants, mecanim::animation::AnimationSet::Clip** animationSetClips, size_t clipCount, dynamic_array<TransformBinding>& outputCombinedTransformBinding, int& outputNonConstantTransformCount, dynamic_array<GenericBinding>& outputCombinedGenericBinding, dynamic_array<GenericBinding>& outputCombinedGenericPPtrBinding, dynamic_array<float>& constantValues, mecanim::memory::Allocator& alloc);
static void BindAdditionalCurves(mecanim::ValueArrayConstant const &valueConstant, dynamic_array<GenericBinding> const& genericBindings, size_t bindingOffset, mecanim::int32_t *additionalIndexArray);
static void BindSkeletonMask(mecanim::ValueArrayConstant const &valueConstant, mecanim::skeleton::SkeletonMask const &skeletonMask, mecanim::ValueArrayMask &mask);


bool IsPPtrAnimation (const GenericBinding& binding)
{
	return binding.isPPtrCurve;
}	
	
void DestroyAnimationSetBindings (AnimationSetBindings* bindings, mecanim::memory::Allocator& allocator)
{
	if (bindings != NULL)
	{
		allocator.Deallocate (bindings->genericBindings);
		allocator.Deallocate (bindings->genericPPtrBindings);
		allocator.Deallocate (bindings->transformBindings);
		DestroyAnimationSet  (bindings->animationSet, allocator);
		allocator.Deallocate (bindings);
	}
}

static void BindGravityWeight (mecanim::animation::AnimationSet* animationSet, const mecanim::ValueArrayConstant* dynamicValuesConstant)
{
	animationSet->m_GravityWeightIndex = mecanim::FindValueIndex(dynamicValuesConstant, CRCKey(mecanim::eGravityWeight));
	if (animationSet->m_GravityWeightIndex != -1)
		animationSet->m_GravityWeightIndex = dynamicValuesConstant->m_ValueArray[animationSet->m_GravityWeightIndex].m_Index;
}
	
AnimationSetBindings* CreateAnimationSetBindings (mecanim::animation::ControllerConstant const* controller, AnimationClipVector const& clips, mecanim::memory::Allocator& allocator)
{
	if (controller == NULL)
		return NULL;

	SETPROFILERLABEL(AnimationSetBindings);
	

	size_t clipCount = clips.size();
	
	mecanim::animation::AnimationSet::Clip** animationSetClips;
	ALLOC_TEMP(animationSetClips, mecanim::animation::AnimationSet::Clip*, clipCount);
	
	AnimationClipBindingConstant**   animationClipBindings;
	ALLOC_TEMP(animationClipBindings, AnimationClipBindingConstant*, clipCount);
	
	mecanim::animation::ClipMuscleConstant** clipConstants;
	ALLOC_TEMP(clipConstants, mecanim::animation::ClipMuscleConstant*, clipCount);


	// Create Animation Set
	mecanim::animation::AnimationSet* animationSet = CreateAnimationSet(controller, allocator);
	animationSet->m_IntegerRemapStride = sizeof(PPtr<Object>);


	size_t clipArrayIndex = 0;
	for(int layerIter=0; layerIter<animationSet->m_LayerCount; layerIter++)
	{	
		for(int clipIter=0; clipIter < animationSet->m_ClipPerLayer[layerIter]; clipIter++)
		{
			AnimationClip* animationClip = clips[clipArrayIndex];
			mecanim::animation::ClipMuscleConstant* clipConstant = animationClip != NULL ? animationClip->GetRuntimeAsset() : 0;
			
			mecanim::animation::AnimationSet::Clip& setClip = animationSet->m_ClipConstant[layerIter][clipIter];	
			setClip.m_Clip = clipConstant;
			setClip.m_ClipIndex = clipArrayIndex;

			if (clipConstant != NULL)
			{
				animationSetClips[clipArrayIndex] = &setClip;
				animationClipBindings[clipArrayIndex] = animationClip->GetBindingConstant();

			// Setup pptrCurve mapping (mecanim accesses it through m_IntegerRemap & m_IntegerRemapStride)
				setClip.m_Bindings.m_IntegerRemap = reinterpret_cast<mecanim::int32_t*> (animationClipBindings[clipArrayIndex]->pptrCurveMapping.begin());
			}
			else
			{
				animationSetClips[clipArrayIndex] = NULL;
				animationClipBindings[clipArrayIndex] = NULL;
				setClip.m_Bindings.m_IntegerRemap = NULL;
			}

			clipArrayIndex++;					
		}
	}

	assert(clipCount == clipArrayIndex);
	

	// Bind to unique curves
	dynamic_array<GenericBinding> genericBindings (kMemTempAlloc);
	dynamic_array<GenericBinding> genericPPtrBindings (kMemTempAlloc);
	dynamic_array<TransformBinding> transformBindings (kMemTempAlloc);
	dynamic_array<float> constantValues (kMemTempAlloc);
	int transformBindingsNonConstants;
	
	CombineUniqueGeneric (animationClipBindings, animationSetClips, clipCount, transformBindings, transformBindingsNonConstants, genericBindings, genericPPtrBindings, constantValues, allocator);

	animationSet->m_DynamicFullValuesConstant = CreateBindingValueArrayConstant (genericBindings.begin(), genericBindings.size(), genericPPtrBindings.begin(), genericPPtrBindings.size(), transformBindings.begin(), transformBindings.size(), allocator);
	
	for(int layerIter=0; layerIter<animationSet->m_LayerCount; layerIter++)
	{	
		animationSet->m_DynamicValuesMaskArray[layerIter] = mecanim::CreateValueArrayMask(animationSet->m_DynamicFullValuesConstant,allocator);
		BindSkeletonMask(*animationSet->m_DynamicFullValuesConstant, *controller->m_LayerArray[layerIter]->m_SkeletonMask, *animationSet->m_DynamicValuesMaskArray[layerIter]);
	}

	BindAdditionalCurves(*controller->m_Values.Get(), genericBindings, 0, animationSet->m_AdditionalIndexArray);

	BindGravityWeight (animationSet, animationSet->m_DynamicFullValuesConstant);

	// Generate output bindings
	AnimationSetBindings* outputBindings = allocator.Construct<AnimationSetBindings> ();
	outputBindings->animationSet = animationSet;

	outputBindings->transformBindingsNonConstantSize = transformBindingsNonConstants;
	outputBindings->transformBindingsSize = transformBindings.size();
	outputBindings->transformBindings = allocator.ConstructArray<TransformBinding> (transformBindings.begin(), transformBindings.size());
	
	outputBindings->genericBindingsSize = genericBindings.size();
	outputBindings->genericBindings = allocator.ConstructArray<GenericBinding> (genericBindings.begin(), genericBindings.size());

	outputBindings->genericPPtrBindingsSize = genericPPtrBindings.size();
	outputBindings->genericPPtrBindings = allocator.ConstructArray<GenericBinding> (genericPPtrBindings.begin(), genericPPtrBindings.size());

	outputBindings->constantCurveValueCount = constantValues.size();
	outputBindings->constantCurveValues = allocator.ConstructArray<float> (constantValues.begin(), constantValues.size());
	
	return outputBindings;
}	

size_t GetCurveCountForBindingType (UInt32 targetType)
{
	if (targetType == kBindTransformRotation)
		return 4;
	else if (targetType == kBindTransformPosition || targetType == kBindTransformScale)
		return 3;
	else 	
		return 1;
}
	
static size_t GetCurveCountForBinding (const GenericBinding& binding)
{
	if (binding.classID == ClassID(Transform))
		return GetCurveCountForBindingType(binding.attribute);
	else
		return 1;
}
	
static mecanim::ValueArrayConstant* CreateBindingValueArrayConstant (const GenericBinding* genericBindings, size_t genericBindingsSize, const GenericBinding* genericPPtrBindings, size_t genericPPtrBindingsSize,  const TransformBinding* transformBindings, size_t transformBindingsSize, mecanim::memory::Allocator& alloc)
{
	size_t valueCount = genericBindingsSize + transformBindingsSize + genericPPtrBindingsSize;
	mecanim::ValueArrayConstant* constant = CreateValueArrayConstant (mecanim::kFloatType, valueCount, alloc);
	
	mecanim::ValueConstant* valueArray = constant->m_ValueArray.Get();
	
	mecanim::uint32_t valueIndex = 0;
	mecanim::uint32_t positionCount = 0;
	mecanim::uint32_t rotationCount = 0;
	mecanim::uint32_t scaleCount = 0;
	
	for(int i=0;i<genericBindingsSize;i++,valueIndex++)
	{
		valueArray[valueIndex].m_ID = genericBindings[i].attribute;
		valueArray[valueIndex].m_Index = i;
		valueArray[valueIndex].m_Type = mecanim::kFloatType;
	}
	
	for(int i=0;i<genericPPtrBindingsSize;i++,valueIndex++)
	{
		valueArray[valueIndex].m_ID = genericPPtrBindings[i].attribute;
		valueArray[valueIndex].m_Index = i;
		valueArray[valueIndex].m_Type = mecanim::kInt32Type;
	}
	
	// Currently transform bindings are the only ones with constant curve optimization.
	// They have to be placed at the end of the ValueArrayConstant so we can reduce it.
	for(int i=0;i<transformBindingsSize;i++,valueIndex++)
	{
		valueArray[valueIndex].m_ID = transformBindings[i].path;

		switch(	transformBindings[i].bindType)
		{
			case kBindTransformPosition: 
				valueArray[valueIndex].m_Index = positionCount++;
				valueArray[valueIndex].m_Type = mecanim::kPositionType;
				break;	
				
			case kBindTransformRotation: 
				valueArray[valueIndex].m_Index = rotationCount++;
				valueArray[valueIndex].m_Type = mecanim::kQuaternionType;
				break;	
				
			case kBindTransformScale: 
				valueArray[valueIndex].m_Index = scaleCount++;
				valueArray[valueIndex].m_Type = mecanim::kScaleType;
				break;	
				
			default:
				AssertString("Unsupported");
		}
	}
	
	return constant;
}

struct BoundIndex
{
	enum { kNotInitialized = 0, kIsConstant = 2, kIsNotConstant = 3 };
	
	mecanim::ValueType type;
	int                index;
	int                curveCount;
	
	int					constantType;
	float				constantValue[4];
	
	BoundIndex ()
	{
		constantType = kNotInitialized;
		type = mecanim::kLastType;
		index = -1;
		curveCount = -1;
	}
};
	
// Currently constant curve optimization is only supported for Transforms	
bool DoesBindingSupportConstantCurveOptimization (const GenericBinding& binding)
{
	return binding.classID == ClassID(Transform);
}
	
typedef UNITY_MAP(kMemTempAlloc, GenericBinding, BoundIndex) BindingMap;

static void GenerateUniqueBindingMap (AnimationClipBindingConstant** constants, mecanim::animation::AnimationSet::Clip** animationSetClips, size_t clipCount, BindingMap& bindingMap)
{
	for (int c=0;c<clipCount;c++)
	{
		if (constants[c] == NULL)
			continue;
		
		const dynamic_array<GenericBinding>& genericBindingArray = constants[c]->genericBindings;

		const mecanim::animation::Clip& clipData = *animationSetClips[c]->m_Clip->m_Clip;
		size_t curveIndex = 0;
		size_t firstConstantIndex = GetClipCurveCount(clipData) - clipData.m_ConstantClip.curveCount;
		for (int i=0;i<genericBindingArray.size();i++)
		{
			const GenericBinding& binding = genericBindingArray[i];
		
			size_t curveCountForBinding = GetCurveCountForBinding(binding);
			BoundIndex& boundIndex = bindingMap.insert(std::make_pair (binding, BoundIndex ())).first->second;
			
			// Detect constant curve values
			if (curveIndex >= firstConstantIndex && DoesBindingSupportConstantCurveOptimization (binding))
			{
				const float* constantCurveValue = &clipData.m_ConstantClip.data[curveIndex - firstConstantIndex];
				if (boundIndex.constantType == BoundIndex::kNotInitialized)
				{
					boundIndex.constantType = BoundIndex::kIsConstant;
					
					memcpy(boundIndex.constantValue, constantCurveValue, curveCountForBinding * sizeof(float));
				}
				else if (boundIndex.constantType == BoundIndex::kIsConstant)
				{
					//@TODO: Use approximately??? also in the instance check?
					
					if (memcmp(boundIndex.constantValue, constantCurveValue, curveCountForBinding * sizeof(float)) != 0)
						boundIndex.constantType = BoundIndex::kIsNotConstant;	
				}
			}
			else
			{
				boundIndex.constantType = BoundIndex::kIsNotConstant;
			}
			
			curveIndex += curveCountForBinding;
		}
	}
}

static void CombineUniqueGeneric (AnimationClipBindingConstant** constants, mecanim::animation::AnimationSet::Clip** animationSetClips, size_t clipCount, dynamic_array<TransformBinding>& outputCombinedTransformBinding, int& outputNonConstantTransformCount, dynamic_array<GenericBinding>& outputCombinedGenericBinding, dynamic_array<GenericBinding>& outputCombinedGenericPPtrBinding, dynamic_array<float>& constantValues, mecanim::memory::Allocator& alloc)
{
	BindingMap bindingMap;

	// Generate unique set of all properties in all clips
	GenerateUniqueBindingMap (constants, animationSetClips, clipCount, bindingMap);
	
	// Sorting binding array, normal curves first, constant curves at the end
	typedef pair<GenericBinding, BoundIndex*> BindingType;
	dynamic_array<BindingType> bindingsSorted;
	bindingsSorted.reserve(bindingMap.size());
	
	for (BindingMap::iterator i=bindingMap.begin();i != bindingMap.end();i++)
	{
		if (i->second.constantType == BoundIndex::kIsNotConstant)
			bindingsSorted.push_back(std::make_pair(i->first, &i->second));
	}
	for (BindingMap::iterator i=bindingMap.begin();i != bindingMap.end();i++)
	{
		if (i->second.constantType == BoundIndex::kIsConstant)
			bindingsSorted.push_back(std::make_pair(i->first, &i->second));
	}
	
	// Fill in BoundIndex in the bindingMap
	int positionCount = 0;
	int rotationCount = 0;
	int scaleCount = 0;
	int genericCount = 0;
	int genericPPtrCount = 0;
	for (int i=0;i<bindingsSorted.size();i++)
	{
		BindingType& binding = bindingsSorted[i];
		
		if (binding.first.classID == ClassID (Transform))
		{
			switch(	binding.first.attribute)
			{
				case kBindTransformPosition: 
					binding.second->type = mecanim::kPositionType;
					binding.second->index = positionCount++;
					binding.second->curveCount = 3;
					break;	
					
				case kBindTransformRotation: 
					binding.second->type = mecanim::kQuaternionType;
					binding.second->index = rotationCount++;
					binding.second->curveCount = 4;
					break;	
					
				case kBindTransformScale: 
					binding.second->type = mecanim::kScaleType;
					binding.second->index = scaleCount++;
					binding.second->curveCount = 3;
					break;

				default:
					Assert("Unsupported");
			}
		}
		else if (IsPPtrAnimation(binding.first))
		{
			binding.second->type = mecanim::kInt32Type;
			binding.second->index = genericPPtrCount++;
			binding.second->curveCount = 1;
		}
		else if (!IsMuscleBinding(binding.first))
		{
			binding.second->type = mecanim::kFloatType;
			binding.second->index = genericCount++;
			binding.second->curveCount = 1;
		}
		else
		{
			binding.second->type = mecanim::kLastType;
			binding.second->index = -1;
			binding.second->curveCount = 1;
		}
	}

	// Convert to outputCombinedArray
	outputCombinedGenericBinding.reserve(genericCount);
	outputCombinedGenericPPtrBinding.reserve(genericPPtrCount);
	outputCombinedTransformBinding.reserve(positionCount + rotationCount + scaleCount);

	outputNonConstantTransformCount = 0;
		
	for (int i=0;i<bindingsSorted.size();i++)
	{
		const BindingType& binding = bindingsSorted[i];

		if (binding.first.classID == ClassID (Transform))
		{
			TransformBinding& transformBinding = outputCombinedTransformBinding.push_back();
			transformBinding.path = binding.first.path;
			transformBinding.bindType = binding.first.attribute;
			
			if (binding.second->constantType == BoundIndex::kIsNotConstant)
					outputNonConstantTransformCount = outputCombinedTransformBinding.size();
			else
			{
					for (int k=0;k<binding.second->curveCount;k++)
						constantValues.push_back(binding.second->constantValue[k]);
			}
		}
		else if (IsPPtrAnimation(binding.first))
		{
			Assert(binding.second->constantType == BoundIndex::kIsNotConstant);
			outputCombinedGenericPPtrBinding.push_back(binding.first);
		}
		else if (!IsMuscleBinding(binding.first))
		{
			Assert(binding.second->constantType == BoundIndex::kIsNotConstant);
			outputCombinedGenericBinding.push_back(binding.first);
		}
	}
	
	
	// Generate remap from ValueArray to curveIndex
	for (int c=0;c<clipCount;c++)
	{
		if (animationSetClips[c] == NULL)
			continue;
		
		const dynamic_array<GenericBinding>& genericBindingArray = constants[c]->genericBindings;
	
		mecanim::animation::AnimationSet::Clip* clip = animationSetClips[c];
		mecanim::animation::ClipBindings& clipBindings = clip->m_Bindings;
		
		clipBindings.m_PositionIndex = alloc.ConstructArray<mecanim::int16_t>(positionCount);
		clipBindings.m_QuaternionIndex = alloc.ConstructArray<mecanim::int16_t>(rotationCount);
		clipBindings.m_ScaleIndex = alloc.ConstructArray<mecanim::int16_t>(scaleCount);
		clipBindings.m_FloatIndex = alloc.ConstructArray<mecanim::int16_t>(genericCount);
		clipBindings.m_IntIndex = alloc.ConstructArray<mecanim::int16_t>(genericPPtrCount);

		for (int i=0;i<positionCount;i++)
			clipBindings.m_PositionIndex[i] = -1;
		for (int i=0;i<rotationCount;i++)
			clipBindings.m_QuaternionIndex[i] = -1;
		for (int i=0;i<scaleCount;i++)
			clipBindings.m_ScaleIndex[i] = -1;
		for (int i=0;i<genericCount;i++)
			clipBindings.m_FloatIndex[i] = -1;
		for (int i=0;i<genericPPtrCount;i++)
			clipBindings.m_IntIndex[i] = -1;
		
		int curveIndex = 0;
		int totalUsedOptimizedCurveCount = 0;
		for (int i=0;i<genericBindingArray.size();i++)
		{
			const GenericBinding& genericBinding = genericBindingArray[i];
			
			BindingMap::iterator found = bindingMap.find(genericBinding);
			Assert(found != bindingMap.end());
			
			switch (found->second.type)
			{
				case mecanim::kPositionType:
					clipBindings.m_PositionIndex[found->second.index] = curveIndex;
					break;	
					
				case mecanim::kQuaternionType:
					clipBindings.m_QuaternionIndex[found->second.index] = curveIndex;
					break;	
					
				case mecanim::kScaleType:
					clipBindings.m_ScaleIndex[found->second.index] = curveIndex;
					break;	

				case mecanim::kFloatType:
					clipBindings.m_FloatIndex[found->second.index] = curveIndex;
					break;
				
				case mecanim::kInt32Type:
					clipBindings.m_IntIndex[found->second.index] = curveIndex;
					break;
				
				default:
					Assert("Unsupported");
					break;
			}
			
			curveIndex += found->second.curveCount;

			if (found->second.constantType == BoundIndex::kIsNotConstant)
				totalUsedOptimizedCurveCount = curveIndex;
		}

		// The total used curve count is based on the properties we bind.
		
		// Muscle clip properties are not bound through the animation set they are pre-bound when building the clip
		// But we still have to allocate memory for them in the clipoutputs for sampling
		const mecanim::animation::ClipMuscleConstant& clipMuscleConstant = *clip->m_Clip;
		for (int m=0;m<mecanim::animation::s_ClipMuscleCurveCount;m++)
			totalUsedOptimizedCurveCount = std::max<mecanim::int32_t> (clipMuscleConstant.m_IndexArray[m] + 1, totalUsedOptimizedCurveCount);

		clip->m_TotalUsedOptimizedCurveCount = totalUsedOptimizedCurveCount;
	}
}

static void BindAdditionalCurves(mecanim::ValueArrayConstant const &valueConstant, dynamic_array<GenericBinding> const& genericBindings, size_t bindingOffset, mecanim::int32_t *additionalIndexArray)
{
	for(int genericIter = 0; genericIter < genericBindings.size(); genericIter++)
	{
		if (genericBindings[genericIter].classID ==  ClassID(Animator))
		{
			//@TODO: Watch out when switching hash to 64 bit
			mecanim::int32_t valueIndex = mecanim::FindValueIndex(&valueConstant, mecanim::uint32_t(genericBindings[genericIter].attribute));

			if (valueIndex != -1)
			{
				additionalIndexArray[valueIndex] = bindingOffset + genericIter;
			}
		}
	}
}

static void BindSkeletonMask(const mecanim::ValueArrayConstant& valueConstant, const mecanim::skeleton::SkeletonMask& skeletonMask, mecanim::ValueArrayMask& mask)
{
	bool emptyMask = skeletonMask.m_Count == 0;

	for(int maskIter = 0; maskIter < valueConstant.m_Count; maskIter++)
	{
		bool maskValue = false;
		
		if (emptyMask || (valueConstant.m_ValueArray[maskIter].m_Type == mecanim::kFloatType))
		{
			maskValue = true;
		}
		else
		{
			bool found = false;

			for(int skIter = 0; !found && skIter < skeletonMask.m_Count; skIter++)
			{
				if(skeletonMask.m_Data[skIter].m_Weight > 0)
				{
					found = valueConstant.m_ValueArray[maskIter].m_ID == skeletonMask.m_Data[skIter].m_PathHash;
				}
			}
			
			maskValue = found;
		}
		
		
		int index = valueConstant.m_ValueArray[maskIter].m_Index;
		switch (valueConstant.m_ValueArray[maskIter].m_Type)
		{
			case mecanim::kPositionType:
				mask.m_PositionValues[index] = maskValue;
				break;
			case mecanim::kQuaternionType:
				mask.m_QuaternionValues[index] = maskValue;
				break;
			case mecanim::kScaleType:
				mask.m_ScaleValues[index] = maskValue;
				break;
			case mecanim::kFloatType:
				mask.m_FloatValues[index] = maskValue;
				break;
			case mecanim::kInt32Type:
				mask.m_IntValues[index] = maskValue;
				break;
			default:
				Assert("Unsupported");
		}
	}
}
}
}
