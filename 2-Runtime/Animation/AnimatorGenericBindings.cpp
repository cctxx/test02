#include "UnityPrefix.h"
#include "AnimatorGenericBindings.h"
#include "AnimationClipBindings.h"
#include "AnimationSetBinding.h"
#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/mecanim/skeleton/skeleton.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/Animation/AnimationUtility.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "GenericAnimationBindingCache.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/BaseClasses/EventIDs.h"

namespace UnityEngine { namespace Animation
{
	static void InitializeDefaultValues (const UnityEngine::Animation::AnimatorGenericBindingConstant& genericBinding, const mecanim::animation::AvatarConstant* avatar, bool hasTransformHierarchy, mecanim::animation::ControllerBindingConstant& controllerBindingConstant);
	
	struct BoundTransform
	{
		BindingHash  pathHash;
		
		Transform*  transform;
		int			bindIndexForSkeleton;
	};

	#define IS_DEPRECATED_NAME_BASED_BINDING (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_3_a1))

	mecanim::int32_t SkeletonFindNodeIndexByNameID(const mecanim::animation::AvatarConstant* avatar, mecanim::uint32_t aNameID)
	{
		mecanim::int32_t ret = -1;

		mecanim::int32_t i;
		for(i = 0; ret == -1 && i < avatar->m_SkeletonNameIDCount; i++)
		{
			if(avatar->m_SkeletonNameIDArray[i] == aNameID)
			{
				ret = i;
			}
		}

		return ret;
	}

	

	void GenerateTransformBindingMapRecursive (Transform& transform, const mecanim::crc32& nameHash, dynamic_array<BoundTransform>& bindings, const mecanim::animation::AvatarConstant* avatar, bool hasTransformHierarchy)
	{
		const mecanim::skeleton::Skeleton* skeleton = avatar->m_AvatarSkeleton.Get();

		BoundTransform& binding = bindings.push_back();
		binding.pathHash = nameHash.checksum();
		binding.transform = &transform;

		if (IS_DEPRECATED_NAME_BASED_BINDING)
		{
			binding.pathHash = mecanim::processCRC32(transform.GetName());
		}
		
		if (hasTransformHierarchy)
		{
			// binding.pathHash : full path
			binding.bindIndexForSkeleton = skeleton ? mecanim::skeleton::SkeletonFindNode(skeleton, binding.pathHash) : -1;
		}
		else
		{
			// binding.pathHash : flattened path
			binding.bindIndexForSkeleton = SkeletonFindNodeIndexByNameID(avatar, binding.pathHash);
		}
		
		Transform::iterator end=transform.end();
		for (Transform::iterator i=transform.begin();i != end;++i)
		{
			Transform* child = *i;
			const char* name = child->GetName();
						
			mecanim::crc32 childNameHash = AppendPathToHash(nameHash, name);			
			GenerateTransformBindingMapRecursive(*child, childNameHash, bindings, avatar, hasTransformHierarchy);
		}
	}
	
	int FindTransformBindingIndexByBindingHash (const dynamic_array<BoundTransform>& bindings, BindingHash pathHash)
	{
		for (int i=0;i<bindings.size();++i)
		{
			if (bindings[i].pathHash == pathHash)
				return i;
		}

		return -1;
	}

	int FindTransformBindingIndexBySkeletonIndex (const dynamic_array<BoundTransform>& bindings, int skeletonIndex)
	{
		for (int i=0;i<bindings.size();++i)
		{
			if (bindings[i].bindIndexForSkeleton == skeletonIndex)
				return i;
		}

		return -1;
	}

	static void BindControllerRootMotionMask(const mecanim::animation::AvatarConstant &avatar,const mecanim::animation::ControllerConstant &controller, bool *rootMotionLayerMask)
	{
		mecanim::uint32_t rootMotionNodePathHash = 0;

		if(avatar.m_RootMotionBoneIndex != -1)
		{
			rootMotionNodePathHash = avatar.m_AvatarSkeleton->m_ID[avatar.m_RootMotionBoneIndex];
		}

		for(int layerIter = 0; layerIter < controller.m_LayerCount; layerIter++)
		{
			bool mask = false;
			bool found = false;

			for(int maskIter = 0; !found && maskIter < controller.m_LayerArray[layerIter]->m_SkeletonMask->m_Count; maskIter++)
			{
				found = controller.m_LayerArray[layerIter]->m_SkeletonMask->m_Data[maskIter].m_PathHash == rootMotionNodePathHash;

				if(found)
				{
					mask = controller.m_LayerArray[layerIter]->m_SkeletonMask->m_Data[maskIter].m_Weight > 0;
				}
			}

			rootMotionLayerMask[layerIter] = mask;
		}
	}

	mecanim::animation::ControllerBindingConstant *CreateControllerBindingConstant(const mecanim::animation::ControllerConstant* controller, const mecanim::animation::AnimationSet* animationSet, mecanim::ValueArrayConstant* valueArrayConstant, mecanim::uint32_t valueArrayConstantSize, const mecanim::animation::AvatarConstant* avatar, mecanim::memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ControllerBindingConstant);

		mecanim::animation::ControllerBindingConstant* controllerBindingConstant = alloc.Construct<mecanim::animation::ControllerBindingConstant>();
		controllerBindingConstant->m_Avatar = avatar;
		controllerBindingConstant->m_Controller = controller;
		controllerBindingConstant->m_AnimationSet = animationSet;
		
		int skeletonCount = !avatar->m_AvatarSkeleton.IsNull() ? avatar->m_AvatarSkeleton->m_Count : 0;
		if (skeletonCount > 0)
			controllerBindingConstant->m_SkeletonTQSMap = alloc.ConstructArray<mecanim::animation::SkeletonTQSMap> (avatar->m_AvatarSkeleton->m_Count);
		
		controllerBindingConstant->m_DynamicValuesConstant = CreateValueArrayConstantCopy (valueArrayConstant, valueArrayConstantSize, alloc);
		controllerBindingConstant->m_DynamicValuesDefault = CreateValueArray(controllerBindingConstant->m_DynamicValuesConstant, alloc);
		
		controllerBindingConstant->m_RootMotionLayerMask = alloc.ConstructArray<bool>(controller->m_LayerCount);
		BindControllerRootMotionMask(*avatar,*controller,controllerBindingConstant->m_RootMotionLayerMask);

		return controllerBindingConstant;
	}
	
	void DestroyControllerBindingConstant(mecanim::animation::ControllerBindingConstant* controllerBindingConstant, mecanim::memory::Allocator& alloc)
	{
		if(controllerBindingConstant)
		{
			DestroyValueArray (controllerBindingConstant->m_DynamicValuesDefault, alloc);
			DestroyValueArrayConstant (controllerBindingConstant->m_DynamicValuesConstant, alloc);
			alloc.Deallocate (controllerBindingConstant->m_SkeletonTQSMap);
			alloc.Deallocate (controllerBindingConstant->m_RootMotionLayerMask);
			alloc.Deallocate (controllerBindingConstant);
		}
	}

	static void BindControllerTQSMap(const AnimationSetBindings& animationSetBindings,
									 const mecanim::skeleton::Skeleton& skeleton,
									 int nonConstantTransformBindingCount,
									 const int* genericTransformBindingToBindingCache,
									 const BoundTransform* bindingCache,
									 bool hasTransformHierarchy,
									 mecanim::animation::ControllerBindingConstant *binding,
									 mecanim::memory::Allocator& alloc)
	{
		if (binding->m_SkeletonTQSMap == NULL)
			return;

		int rotationCount = -1;
		int positionCount = -1;
		int scaleCount = -1;
		
		for (int transformIter = 0; transformIter < nonConstantTransformBindingCount; transformIter++)
		{
			const TransformBinding& transformBinding = animationSetBindings.transformBindings[transformIter];
			int targetType = transformBinding.bindType;
			
			if (targetType == kBindTransformScale)
				scaleCount++;
			else if (targetType == kBindTransformRotation)
				rotationCount++;
			else if (targetType == kBindTransformPosition)
				positionCount++;
			
			int skIndex = -1;
			if (hasTransformHierarchy)
			{
				int transformIndex = genericTransformBindingToBindingCache[transformIter];
				if (transformIndex == -1)
					continue;
				skIndex = bindingCache[transformIndex].bindIndexForSkeleton;
			}
			else
				skIndex = SkeletonFindNode(&skeleton, transformBinding.path);

			if (skIndex == -1)
				continue;

			if(targetType == kBindTransformScale)
				binding->m_SkeletonTQSMap[skIndex].m_SIndex = scaleCount;
			else if(targetType == kBindTransformRotation)
				binding->m_SkeletonTQSMap[skIndex].m_QIndex = rotationCount;
			else if(targetType == kBindTransformPosition)
				binding->m_SkeletonTQSMap[skIndex].m_TIndex = positionCount;
		}
	}
	
	static void GetDefaultTransformValues (Transform& targetTransform, int bindType, float* defaults)
	{
		if (bindType == kBindTransformPosition)
		{
			Vector3f pos = targetTransform.GetLocalPosition();
			memcpy(defaults, &pos, sizeof(pos));
		}
		else if (bindType == kBindTransformRotation)
		{
			Quaternionf rot = targetTransform.GetLocalRotation();
			memcpy(defaults, &rot, sizeof(rot));
		}
		else if (bindType == kBindTransformScale)
		{
			Vector3f scale = targetTransform.GetLocalScale();
			memcpy(defaults, &scale, sizeof(scale));
		}
		else
		{
			AssertString("Bad");
		}
	}
	
	static void GetDefaultSkeletonPoseValues (const math::xform& x, int bindType, float* defaults)
	{
		if (bindType == kBindTransformPosition)
			math::store(x.t, defaults);
		else if (bindType == kBindTransformRotation)
			math::store(x.q, defaults);
		else if (bindType == kBindTransformScale)
			math::store(x.s, defaults);
		else
		{
			AssertString("Bad");
		}
	}

	static int CalculateTransformBindingSizeBasedOnConstantOptimization (const AnimationSetBindings& animationSet, dynamic_array<BoundTransform> const& transformBindingCache, const int* genericTransformBindingToBindingCache, const mecanim::animation::AvatarConstant* avatar, bool hasTransformHierarchy)
	{
		// Generate Constant Default values
		ATTRIBUTE_ALIGN(ALIGN4F) float values[4];
		const mecanim::skeleton::Skeleton* skeleton = hasTransformHierarchy ? NULL : avatar->m_AvatarSkeleton.Get();
		const mecanim::skeleton::SkeletonPose* defaultPose = hasTransformHierarchy ? NULL : avatar->m_AvatarSkeletonPose.Get();

		int highestMismatch = animationSet.transformBindingsNonConstantSize;
		
		int constantDefaultValueIndex = 0;
		for (int i=animationSet.transformBindingsNonConstantSize;i<animationSet.transformBindingsSize;i++)
		{
			const TransformBinding& transformBinding = animationSet.transformBindings[i];
			int count = GetCurveCountForBindingType (transformBinding.bindType);

			if (hasTransformHierarchy)
			{
				// If we can't write the value, then we don't need it to be sampled either...
				// Thus we can simply assume it matches.
				int transformIndex = genericTransformBindingToBindingCache[i];
				if (transformIndex == -1)
				{
					constantDefaultValueIndex += count;
					continue;
				}
				Transform* targetTransform = transformBindingCache[transformIndex].transform;
				GetDefaultTransformValues(*targetTransform, transformBinding.bindType, values);
			}
			else
			{
				// get the default value from skeleton
				int skeletonIndex = SkeletonFindNode(skeleton, transformBinding.path);
				if (skeletonIndex == -1)
				{
					constantDefaultValueIndex += count;
					continue;
				}
				GetDefaultSkeletonPoseValues(defaultPose->m_X[skeletonIndex], transformBinding.bindType, values);
			}
			
			for (int k=0;k<count;k++)
			{
				float clipConstant = animationSet.constantCurveValues[constantDefaultValueIndex];
				
				if (!CompareApproximately(clipConstant, values[k], 0.00001F))
				{
					// printf_console("mismatch index: %d type: %d clipconstant/instance: %f vs %f.\n", i, bindType, animationSet.constantCurveValues[constantDefaultValueIndex], values[k]);
					highestMismatch = i + 1;
				}
					
				constantDefaultValueIndex++;
			}
		}
		
		Assert(constantDefaultValueIndex == animationSet.constantCurveValueCount);
		return highestMismatch;
	}

	static void InvalidateBoundCurveArray(BoundCurve *boundCurveArray, int boundCurveCount, Object *object)
	{
		for(int iter = 0; iter < boundCurveCount; iter++)
		{
			if(boundCurveArray[iter].targetObject == object)
			{
				boundCurveArray[iter] = BoundCurve();
			}
		}
	}

	static void InvalidateTransformArray(Transform **transformArray, int transformCount, Object *object)
	{
		for(int iter = 0; iter < transformCount; iter++)
		{
			if(transformArray[iter] == object)
			{
				transformArray[iter] = 0;
			}
		}
	}

	void InvalidateAvatarBindingObject(AvatarBindingConstant* bindingConstant, Object *object)
	{
		InvalidateTransformArray(bindingConstant->skeletonBindings,bindingConstant->skeletonBindingsCount,object);
		for (int i = 0; i < bindingConstant->exposedTransformCount; i++)
		{
			if (bindingConstant->exposedTransforms[i].transform == object)
				bindingConstant->exposedTransforms[i].transform = NULL;
		}
	}


	static void InvalidateGenericBindingObject(AnimatorGenericBindingConstant* bindingConstant, Object *object)
	{
		InvalidateBoundCurveArray(bindingConstant->transformBindings,bindingConstant->transformBindingsCount,object);
		InvalidateBoundCurveArray(bindingConstant->genericBindings,bindingConstant->genericBindingsCount,object);
		InvalidateBoundCurveArray(bindingConstant->genericPPtrBindings,bindingConstant->genericPPtrBindingsCount,object);
	}

	static void GenericBindingCallback(void *userData, void *sender,int eventType)
	{
		if(eventType ==  kWillDestroyEvent)
		{
			InvalidateGenericBindingObject(reinterpret_cast<AnimatorGenericBindingConstant *>(userData),reinterpret_cast<Object *>(sender));
		}
	}

	void AvatarBindingCallback(void *userData, void *sender,int eventType)
	{
		if(eventType ==  kWillDestroyEvent)
		{
			InvalidateAvatarBindingObject(reinterpret_cast<AvatarBindingConstant *>(userData),reinterpret_cast<Object *>(sender));
		}
	}

	static void RegisterBoundCurveArray(BoundCurve *boundCurveArray, int boundCurveCount, AnimatorGenericBindingConstant* bindingConstant)
	{
		for(int iter = 0; iter < boundCurveCount; iter++)
		{
			if(boundCurveArray[iter].targetObject != 0)
			{
				if(!boundCurveArray[iter].targetObject->HasEvent(GenericBindingCallback,bindingConstant))
				{
					boundCurveArray[iter].targetObject->AddEvent(GenericBindingCallback,bindingConstant);
				}
			}
		}
	}

	template <typename TYPE> static void RegisterTransformArray(Transform **transformArray, int transformCount, TYPE* bindingConstant, Object::EventCallback* callback)
	{
		for(int iter = 0; iter < transformCount; iter++)
		{
			if(transformArray[iter] != 0)
            {
				if(!transformArray[iter]->HasEvent(callback, bindingConstant))
				{
				    transformArray[iter]->AddEvent(callback, bindingConstant);
				}
			}
		}
	}

	void RegisterAvatarBindingObjects(AvatarBindingConstant* bindingConstant)
	{
		RegisterTransformArray(bindingConstant->skeletonBindings,bindingConstant->skeletonBindingsCount,bindingConstant, AvatarBindingCallback);
		for (int i = 0; i < bindingConstant->exposedTransformCount; i++)
		{
			if (bindingConstant->exposedTransforms[i].transform &&
				!bindingConstant->exposedTransforms[i].transform->HasEvent(AvatarBindingCallback, bindingConstant))
				bindingConstant->exposedTransforms[i].transform->AddEvent(AvatarBindingCallback, bindingConstant);
		}
	}

	static void RegisterGenericBindingObjects(AnimatorGenericBindingConstant* bindingConstant)
	{
		RegisterBoundCurveArray(bindingConstant->transformBindings,bindingConstant->transformBindingsCount,bindingConstant);
		RegisterBoundCurveArray(bindingConstant->genericBindings,bindingConstant->genericBindingsCount,bindingConstant);
		RegisterBoundCurveArray(bindingConstant->genericPPtrBindings,bindingConstant->genericPPtrBindingsCount,bindingConstant);
	}

	static void UnregisterBoundCurveArray(BoundCurve *boundCurveArray, int boundCurveCount, AnimatorGenericBindingConstant* bindingConstant)
	{
		for(int iter = 0; iter < boundCurveCount; iter++)
		{
			if(boundCurveArray[iter].targetObject != 0)
			{
				boundCurveArray[iter].targetObject->RemoveEvent(GenericBindingCallback,bindingConstant);
			}
		}
	}

	template<typename TYPE> static void UnregisterTransformArray(Transform **transformArray, int transformCount, TYPE* bindingConstant, Object::EventCallback* callback)
	{
		for(int iter = 0; iter < transformCount; iter++)
		{
			if(transformArray[iter] != 0)
			{
				transformArray[iter]->RemoveEvent(callback,bindingConstant);
			}
		}
	}

	void UnregisterAvatarBindingObjects(AvatarBindingConstant* bindingConstant)
	{
		UnregisterTransformArray(bindingConstant->skeletonBindings,bindingConstant->skeletonBindingsCount,bindingConstant, AvatarBindingCallback);
		for (int i = 0; i < bindingConstant->exposedTransformCount; i++)
		{
			if (bindingConstant->exposedTransforms[i].transform != NULL)
				bindingConstant->exposedTransforms[i].transform->RemoveEvent(AvatarBindingCallback, bindingConstant);
		}
	}

	void UnregisterGenericBindingObjects(AnimatorGenericBindingConstant* bindingConstant)
	{
		UnregisterBoundCurveArray(bindingConstant->transformBindings,bindingConstant->transformBindingsCount,bindingConstant);
		UnregisterBoundCurveArray(bindingConstant->genericBindings,bindingConstant->genericBindingsCount,bindingConstant);
		UnregisterBoundCurveArray(bindingConstant->genericPPtrBindings,bindingConstant->genericPPtrBindingsCount,bindingConstant);
	}
	
	static Transform *humanMark = reinterpret_cast<Transform *>(std::numeric_limits<size_t>::max());

	static void humanMarkUp(mecanim::skeleton::Skeleton const &sk, int nodeIndex, Transform** bindings)
	{
		if(nodeIndex != -1)
		{
			bindings[nodeIndex] = humanMark;

			humanMarkUp(sk,sk.m_Node[nodeIndex].m_ParentId,bindings);
		}
	}

	AvatarBindingConstant* CreateAvatarBindingConstant (Transform& root, mecanim::animation::AvatarConstant const* avatar, mecanim::memory::Allocator& allocator)
	{
		SETPROFILERLABEL(AvatarBindingConstant);
		
		// Generate binding cache
		dynamic_array<BoundTransform> transformBindingCache (kMemTempAlloc);
		
		const mecanim::skeleton::Skeleton* skeleton = avatar->m_AvatarSkeleton.Get();	

		GenerateTransformBindingMapRecursive(root, mecanim::crc32(), transformBindingCache, avatar, true);

		AvatarBindingConstant* constant = allocator.Construct<AvatarBindingConstant> ();
		constant->exposedTransformCount = 0;
		constant->exposedTransforms = NULL;

		constant->skeletonBindingsCount = skeleton ? skeleton->m_Count : 0;
		constant->skeletonBindings = allocator.ConstructArray<Transform*> (constant->skeletonBindingsCount);

		int transformChangedMask = 0;

		// just bind what human will effectively affect

		if(IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_3_a1))
		{
			if (constant->skeletonBindingsCount != 0)
			{
				memset(constant->skeletonBindings, 0, sizeof(Transform*) * constant->skeletonBindingsCount);

				if(avatar->m_HumanSkeletonIndexCount > 0)
				{
					humanMarkUp(*skeleton,avatar->m_HumanSkeletonIndexArray[0],constant->skeletonBindings);

					for(int humanSkIndexIter = 0; humanSkIndexIter < avatar->m_HumanSkeletonIndexCount; humanSkIndexIter++)
					{
						int humanSkIndex = avatar->m_HumanSkeletonIndexArray[humanSkIndexIter];

						if(humanSkIndex != -1)
						{
							constant->skeletonBindings[humanSkIndex] = humanMark;
						}
					}
				}
			}
		}
		else
		{
			for(int i = 0 ;  i < constant->skeletonBindingsCount; ++i)
				constant->skeletonBindings[i] = humanMark;			
		}

		for (int i=0;i<transformBindingCache.size();i++)
		{
			int skeletonIndex = transformBindingCache[i].bindIndexForSkeleton;

			if (skeletonIndex != -1)
			{
				if(constant->skeletonBindings[skeletonIndex] == humanMark)
				{
					constant->skeletonBindings[skeletonIndex] = transformBindingCache[i].transform;
					transformChangedMask |= Transform::kPositionChanged | Transform::kRotationChanged;
				}				
			}
		}

		for(int i = 0 ;  i < constant->skeletonBindingsCount; ++i)
		{		
			if(constant->skeletonBindings[i] == humanMark)
				constant->skeletonBindings[i] = 0;
		}

		constant->transformChangedMask = transformChangedMask;

		RegisterAvatarBindingObjects(constant);

		return constant;
	}

	AvatarBindingConstant* CreateAvatarBindingConstantOpt (Transform& root, mecanim::animation::AvatarConstant const* avatar, mecanim::memory::Allocator& allocator)
	{
		SETPROFILERLABEL(AvatarBindingConstant);

		// Generate binding cache
		dynamic_array<BoundTransform> transformBindingCache (kMemTempAlloc);

		GenerateTransformBindingMapRecursive(root, mecanim::crc32(), transformBindingCache, avatar, false);

		mecanim::skeleton::Skeleton const* skeleton = avatar->m_AvatarSkeleton.Get();
		
		AvatarBindingConstant* constant = allocator.Construct<AvatarBindingConstant> ();
		constant->skeletonBindingsCount = 0;
		constant->skeletonBindings = NULL;
		constant->transformChangedMask = 0;

		// For the flattened transform, it's impossible to tell if the transform will be modified just by the curve which
		// is binded to it, because all its parent transforms will also affect it.
		// Since normally, there are only a few exposed transforms, the performance penalty will not be huge
		// if we don't care which exact properties of the transform are modified.
		int exposedCount = 0;
		int maxExposeCount = transformBindingCache.size();
		dynamic_array<ExposedTransform> exposedTransforms(maxExposeCount, kMemTempAlloc);
		for (int i = 0; i < maxExposeCount; ++i)
		{
			BoundTransform& boundTransform = transformBindingCache[i];
			bool isChildOfRoot = (boundTransform.transform->GetParent() == &root);
			if (!isChildOfRoot)
				continue;

			ExposedTransform& exposedTransform = exposedTransforms[exposedCount];
			exposedTransform.transform = boundTransform.transform;
			exposedTransform.skeletonIndex = -1;
			exposedTransform.skeletonIndexForUpdateTransform = -1;

			if (boundTransform.bindIndexForSkeleton != -1)
			{
				exposedTransform.skeletonIndex = boundTransform.bindIndexForSkeleton;
				exposedTransform.skeletonIndexForUpdateTransform = boundTransform.bindIndexForSkeleton;
			}

			// Handle special case: SkinnedMeshRenderer
			// We directly update the root bone to the Transform of the SkinnedMeshRenderer.
			SkinnedMeshRenderer* skin = boundTransform.transform->QueryComponent(SkinnedMeshRenderer);
			if (skin)
			{
				const Mesh* mesh = skin->GetMesh();
				if (mesh && mesh->GetRootBonePathHash() != 0)
				{
					int skinRootIndex = skeleton ? mecanim::skeleton::SkeletonFindNode(
						skeleton, mesh->GetRootBonePathHash()) : -1;
					if (skinRootIndex != -1)
						exposedTransform.skeletonIndexForUpdateTransform = skinRootIndex;
				}
			}
			if (exposedTransform.skeletonIndexForUpdateTransform != -1)
				exposedCount++;
		}

		constant->exposedTransformCount = exposedCount;
		constant->exposedTransforms = allocator.ConstructArray<ExposedTransform> (constant->exposedTransformCount);
		for (int i=0; i<exposedCount; i++)
			constant->exposedTransforms[i] = exposedTransforms[i];

		RegisterAvatarBindingObjects(constant);

		return constant;
	}
	
	void DestroyAvatarBindingConstant (AvatarBindingConstant* bindingConstant, mecanim::memory::Allocator& allocator)
	{
		if (bindingConstant != NULL)
		{
			UnregisterAvatarBindingObjects(bindingConstant);

			allocator.Deallocate(bindingConstant->skeletonBindings);
			allocator.Deallocate(bindingConstant->exposedTransforms);
			allocator.Deallocate(bindingConstant);
		}
	}

	AnimatorGenericBindingConstant* CreateAnimatorGenericBindings (const AnimationSetBindings& animationSet, Transform& root, const mecanim::animation::AvatarConstant* avatar, const mecanim::animation::ControllerConstant* controller, mecanim::memory::Allocator& allocator)
	{
		SETPROFILERLABEL(AnimatorGenericBindingConstant);

		GenericAnimationBindingCache& bindingCache = GetGenericAnimationBindingCache ();

		const mecanim::skeleton::Skeleton* skeleton = avatar->m_AvatarSkeleton.Get();
		
		// Generate binding cache
		dynamic_array<BoundTransform> transformBindingCache (kMemTempAlloc);
		dynamic_array<int> genericTransformBindingToBindingCache (kMemTempAlloc);
				
		GenerateTransformBindingMapRecursive(root, mecanim::crc32(), transformBindingCache, avatar, true);
		
		// Map from animation set to binding cache index
		genericTransformBindingToBindingCache.resize_uninitialized(animationSet.transformBindingsSize);
		for (int i=0;i<animationSet.transformBindingsSize;i++)
			genericTransformBindingToBindingCache[i] = FindTransformBindingIndexByBindingHash (transformBindingCache, animationSet.transformBindings[i].path);
		
		// Calculate Transform bindings that are actually animating (Constant curve values can be removed if they match the default values)
		// Generate new reduced ValueArrayCount from it.
		int nonConstantTransformBindingCount = CalculateTransformBindingSizeBasedOnConstantOptimization (animationSet, transformBindingCache, genericTransformBindingToBindingCache.begin(), avatar, true);
		int optimizedValueArrayConstantCount = animationSet.animationSet->m_DynamicFullValuesConstant->m_Count - (animationSet.transformBindingsSize - nonConstantTransformBindingCount);
		bool allowConstantClipSamplingOptimization = nonConstantTransformBindingCount == animationSet.transformBindingsNonConstantSize;
		
		AnimatorGenericBindingConstant* constant = allocator.Construct<AnimatorGenericBindingConstant> ();
		
		constant->transformBindingsCount = nonConstantTransformBindingCount;
		constant->transformBindings = allocator.ConstructArray<BoundCurve> (constant->transformBindingsCount);

		constant->genericBindingsCount = animationSet.genericBindingsSize;
		constant->genericBindings = allocator.ConstructArray<BoundCurve> (constant->genericBindingsCount);

		constant->genericPPtrBindingsCount = animationSet.genericPPtrBindingsSize;
		constant->genericPPtrBindings = allocator.ConstructArray<BoundCurve> (constant->genericPPtrBindingsCount);
		
		constant->allowConstantClipSamplingOptimization = allowConstantClipSamplingOptimization;
		
		int transformChangedMask = 0;
		
		// Bind Transforms
		for (int i=0;i<constant->transformBindingsCount;i++)
		{
			int transformIndex = genericTransformBindingToBindingCache[i];
			constant->transformBindings[i].targetType = animationSet.transformBindings[i].bindType;
			if (transformIndex != -1)
			{
				constant->transformBindings[i].targetObject = transformBindingCache[transformIndex].transform;				

				transformChangedMask |= Transform::kPositionChanged  | Transform::kRotationChanged;
				if (animationSet.transformBindings[i].bindType == kBindTransformScale)
					transformChangedMask |= Transform::kScaleChanged;
			}
			else
			{
				constant->transformBindings[i].targetObject = NULL;
			}			
		}
		
		constant->transformChangedMask = transformChangedMask;

		// Bind Generic properties
		for (int i=0;i<constant->genericBindingsCount;i++)
		{
			constant->genericBindings[i].targetObject = NULL;
			constant->genericBindings[i].targetType = kUnbound;

			int index = FindTransformBindingIndexByBindingHash (transformBindingCache, animationSet.genericBindings[i].path);
			if (index != -1)
				bindingCache.BindGeneric(animationSet.genericBindings[i], *transformBindingCache[index].transform, constant->genericBindings[i]);
		}

		// Bind Generic PPtr properties
		for (int i=0;i<constant->genericPPtrBindingsCount;i++)
		{
			constant->genericPPtrBindings[i].targetObject = NULL;
			constant->genericPPtrBindings[i].targetType = kUnbound;
			
			int index = FindTransformBindingIndexByBindingHash (transformBindingCache, animationSet.genericPPtrBindings[i].path);
			if (index != -1)
				bindingCache.BindPPtrGeneric(animationSet.genericPPtrBindings[i], *transformBindingCache[index].transform, constant->genericPPtrBindings[i]);
		}
		
		constant->controllerBindingConstant = CreateControllerBindingConstant (controller, animationSet.animationSet, animationSet.animationSet->m_DynamicFullValuesConstant, optimizedValueArrayConstantCount, avatar, allocator);

		// Gravity weight should must be in both optimized and non-optimized ValueArray
		Assert(animationSet.animationSet->m_GravityWeightIndex == -1 || animationSet.animationSet->m_GravityWeightIndex < constant->controllerBindingConstant->m_DynamicValuesDefault->m_FloatCount);
		
		// Bind Controller skeleton to dynamic value array
		BindControllerTQSMap(animationSet, *skeleton, nonConstantTransformBindingCount, genericTransformBindingToBindingCache.begin(), transformBindingCache.begin(), true, constant->controllerBindingConstant, allocator);

		RegisterGenericBindingObjects(constant);

		InitializeDefaultValues (*constant, avatar, true, *constant->controllerBindingConstant);

		return constant; 
	}

	AnimatorGenericBindingConstant* CreateAnimatorGenericBindingsOpt ( const AnimationSetBindings& animationSet, Transform& root, const mecanim::animation::AvatarConstant* avatar, const mecanim::animation::ControllerConstant* controller, mecanim::memory::Allocator& allocator)
	{
		GenericAnimationBindingCache& bindingCache = GetGenericAnimationBindingCache ();
		const mecanim::skeleton::Skeleton* skeleton = avatar->m_AvatarSkeleton.Get();

		dynamic_array<BoundTransform> transformBindingCache (kMemTempAlloc);
		// BoundTransform.pathHash:					hash of flattened path
		// BoundTransform.bindIndexForSkeleton:		corresponding skeleton index
		GenerateTransformBindingMapRecursive(root, mecanim::crc32(), transformBindingCache, avatar, false);

		// Calculate Transform bindings that are actually animating (Constant curve values can be removed if they match the default values)
		// Generate new reduced ValueArrayCount from it.
		int nonConstantTransformBindingCount = CalculateTransformBindingSizeBasedOnConstantOptimization (animationSet, transformBindingCache, NULL, avatar, false);
		int optimizedValueArrayConstantCount = animationSet.animationSet->m_DynamicFullValuesConstant->m_Count - (animationSet.transformBindingsSize - nonConstantTransformBindingCount);
		bool allowConstantClipSamplingOptimization = nonConstantTransformBindingCount == animationSet.transformBindingsNonConstantSize;

		AnimatorGenericBindingConstant* constant = allocator.Construct<AnimatorGenericBindingConstant> ();

		constant->transformBindingsCount = 0;
		constant->transformBindings = NULL;

		constant->genericBindingsCount = animationSet.genericBindingsSize;
		constant->genericBindings = allocator.ConstructArray<BoundCurve> (constant->genericBindingsCount);

		constant->genericPPtrBindingsCount = animationSet.genericPPtrBindingsSize;
		constant->genericPPtrBindings = allocator.ConstructArray<BoundCurve> (constant->genericPPtrBindingsCount);

		constant->allowConstantClipSamplingOptimization = allowConstantClipSamplingOptimization;

					
		// Bind Generic properties
		for (int i=0; i<constant->genericBindingsCount; i++)
		{
			constant->genericBindings[i].targetObject = NULL;
			constant->genericBindings[i].targetType = kUnbound;

			BindingHash fullPath = animationSet.genericBindings[i].path;
			int skeletonIndex = mecanim::skeleton::SkeletonFindNode(skeleton, fullPath);

			int transformIndex = FindTransformBindingIndexBySkeletonIndex (transformBindingCache, skeletonIndex);
			if (transformIndex != -1)
				bindingCache.BindGeneric(animationSet.genericBindings[i], *transformBindingCache[transformIndex].transform, constant->genericBindings[i]);
		}

		// Bind Generic PPtr properties
		for (int i=0; i<constant->genericPPtrBindingsCount; i++)
		{
			constant->genericPPtrBindings[i].targetObject = NULL;
			constant->genericPPtrBindings[i].targetType = kUnbound;

			BindingHash fullPath = animationSet.genericPPtrBindings[i].path;
			int skeletonIndex = mecanim::skeleton::SkeletonFindNode(skeleton, fullPath);

			int transformIndex = FindTransformBindingIndexBySkeletonIndex (transformBindingCache, skeletonIndex);
			if (transformIndex != -1)
				bindingCache.BindPPtrGeneric(animationSet.genericPPtrBindings[i], *transformBindingCache[transformIndex].transform, constant->genericPPtrBindings[i]);
		}

		constant->controllerBindingConstant = CreateControllerBindingConstant (controller, animationSet.animationSet, animationSet.animationSet->m_DynamicFullValuesConstant, optimizedValueArrayConstantCount, avatar, allocator);

		// Gravity weight should must be in both optimized and non-optimized ValueArray
		Assert(animationSet.animationSet->m_GravityWeightIndex == -1 || animationSet.animationSet->m_GravityWeightIndex < constant->controllerBindingConstant->m_DynamicValuesDefault->m_FloatCount);

		// Bind Controller skeleton to dynamic value array
		BindControllerTQSMap(animationSet, *skeleton, nonConstantTransformBindingCount, NULL, transformBindingCache.begin(), false, constant->controllerBindingConstant, allocator);

		RegisterGenericBindingObjects(constant);

		InitializeDefaultValues (*constant, avatar, false, *constant->controllerBindingConstant);

		return constant; 
	}
	
	void DestroyAnimatorGenericBindings (AnimatorGenericBindingConstant* bindingConstant, mecanim::memory::Allocator& allocator)
	{
		if (bindingConstant != NULL)
		{
			UnregisterGenericBindingObjects(bindingConstant);

			DestroyControllerBindingConstant(bindingConstant->controllerBindingConstant, allocator);
			allocator.Deallocate(bindingConstant->transformBindings);
			allocator.Deallocate(bindingConstant->genericBindings);
			allocator.Deallocate(bindingConstant);
		}
	}

	////// Get & Set Values
	void SetGenericPPtrPropertyValues (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray &values)
	{
		for (int bindIndex = 0;bindIndex != bindings.genericPPtrBindingsCount;bindIndex++)
		{
			const BoundCurve& binding = bindings.genericPPtrBindings[bindIndex];
			int targetType = binding.targetType;
			
			if (targetType == kUnbound)
				continue;
			
			Assert (targetType >= kMinSinglePropertyBinding);
			
			mecanim::int32_t value = 0;
			values.ReadData(value, bindIndex);
			SetBoundCurveIntValue (binding, value);
		}
	}
	
	void SetGenericFloatPropertyValues (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray &values)
	{
		Object* lastAwakeFromLoadObject = NULL;
		for (int bindIndex = 0;bindIndex != bindings.genericBindingsCount;bindIndex++)
		{
			const BoundCurve& binding = bindings.genericBindings[bindIndex];
			int targetType = binding.targetType;
			
			if (targetType == kUnbound)
				continue;
			
			// When applying multiple properties to the same object in a row.
			// Call AwakeFromLoad / SetDirty only once.
			if (ShouldAwakeGeneric(binding))
			{
				if (lastAwakeFromLoadObject != binding.targetObject)
				{
					if (lastAwakeFromLoadObject != NULL)
						BoundCurveValueAwakeGeneric(*lastAwakeFromLoadObject);
					lastAwakeFromLoadObject = binding.targetObject;
				}
			}
			
			Assert (targetType >= kMinSinglePropertyBinding);
			
			float value = 0.0F;
			values.ReadData(value, bindIndex);
			
			SetBoundCurveFloatValue (binding, value);
		}
		
		if (lastAwakeFromLoadObject != NULL)
			BoundCurveValueAwakeGeneric(*lastAwakeFromLoadObject);
	}


	void SetTransformPropertyApplyMainThread (Transform& root, const AvatarBindingConstant& avatarBindings, bool skipRoot, int mask)
	{
		// Send TransformChanged message
		int transformChangedMask = avatarBindings.transformChangedMask & mask;
		if (transformChangedMask != 0)
		{
			if(skipRoot)
			{
				for(int childIter = 0; childIter < root.m_Children.size(); childIter++)
				{
					root.m_Children[childIter]->SendTransformChanged(transformChangedMask);
				}
			}
			else
			{
				root.SendTransformChanged(transformChangedMask);
			}
		}

		// In The editor we set dirty so the inspector UI is always up to date
		#if UNITY_EDITOR
		for (int bindIndex = 0;bindIndex != avatarBindings.skeletonBindingsCount;bindIndex++)
		{
			Transform* targetTransform = avatarBindings.skeletonBindings[bindIndex];
			if (targetTransform)
				targetTransform->SetDirty();
		}
		#endif
	}

	void SetTransformPropertyApplyMainThread (Transform& root, const AnimatorGenericBindingConstant& bindings, const AvatarBindingConstant& avatarBindings, bool skipRoot)
	{
		// Send TransformChanged message
		if (bindings.transformChangedMask != 0)
		{
			if(skipRoot)
			{
				for(int childIter = 0; childIter < root.m_Children.size(); childIter++)
				{
					root.m_Children[childIter]->SendTransformChanged(bindings.transformChangedMask);
				}
			}
			else
			{
				root.SendTransformChanged(bindings.transformChangedMask);
			}
		}

		SetTransformPropertyApplyMainThread(root, avatarBindings, skipRoot, ~bindings.transformChangedMask);

		// In The editor we set dirty so the inspector UI is always up to date
		#if UNITY_EDITOR
		for (int bindIndex = 0;bindIndex != bindings.transformBindingsCount;bindIndex++)
		{
			const BoundCurve& binding = bindings.transformBindings[bindIndex];
			Transform* targetTransform = reinterpret_cast<Transform*> (binding.targetObject);
			if (targetTransform)
				targetTransform->SetDirty();
		}
		#endif
	}

	
	void SetHumanTransformPropertyValues (const AvatarBindingConstant& bindings, const mecanim::skeleton::SkeletonPose& pose)
	{
		int transformCount = bindings.skeletonBindingsCount;
		Assert(pose.m_Count == transformCount);
		
		// skip root node i = 1
		for(int i = 1; i < transformCount; i++)
		{
			Transform* transform = bindings.skeletonBindings[i];
			if (transform != NULL)
			{
				Vector3f t = float4ToVector3f(pose.m_X[i].t);
				transform->SetLocalPositionWithoutNotification(t);
				
				Quaternionf q = float4ToQuaternionf(pose.m_X[i].q);
				transform->SetLocalRotationWithoutNotification(q);
			}
		}
	}
	
	void SetFlattenedSkeletonTransformsMainThread (const AvatarBindingConstant& bindings, const mecanim::skeleton::SkeletonPose& globalSpacePose, const mecanim::animation::AvatarConstant& avatar)
	{
		for (size_t i=0;i < bindings.exposedTransformCount;i++)
		{
			ExposedTransform& exposedTransform = bindings.exposedTransforms[i];
			if (exposedTransform.transform)
			{
				const math::xform& globalXForm = globalSpacePose.m_X[exposedTransform.skeletonIndexForUpdateTransform];
				exposedTransform.transform->SetPositionAndRotation(float4ToVector3f(globalXForm.t), float4ToQuaternionf(globalXForm.q));
			}
		}
	}
	
	void SetGenericTransformPropertyValues (const AnimatorGenericBindingConstant& bindings, const mecanim::ValueArray &values, Transform *skipTransform)
	{
		int rotationIndex = 0;
		int positionIndex = 0;
		int scaleIndex = 0;
		
		for (int bindIndex = 0;bindIndex != bindings.transformBindingsCount;bindIndex++)
		{
			const BoundCurve& binding = bindings.transformBindings[bindIndex];
			int targetType = binding.targetType;
			
			Transform* targetTransform = reinterpret_cast<Transform*> (binding.targetObject);
				
			if (targetType == kBindTransformRotation)
			{
				if(targetTransform && (targetTransform  != skipTransform ))
				{
					math::float4 value = values.ReadQuaternion(rotationIndex);
					targetTransform->SetLocalRotationWithoutNotification(float4ToQuaternionf(value));
				}				
				rotationIndex++;
			}
			else if (targetType == kBindTransformPosition)
			{
				if(targetTransform && (targetTransform  != skipTransform ))
				{
					math::float4 value = values.ReadPosition(positionIndex);
					targetTransform->SetLocalPositionWithoutNotification(float4ToVector3f(value));
				}
				
				positionIndex++;
			}
			else if (targetType == kBindTransformScale)
			{
				if(targetTransform && (targetTransform  != skipTransform ))
				{
					math::float4 value = values.ReadScale(scaleIndex);
					targetTransform->SetLocalScaleWithoutNotification(float4ToVector3f(value));
				}
				
				scaleIndex++;
			}
		}		
	}
	
	
	static void GetDefaultTransformValues (const BoundCurve* transformBindings, size_t transformBindingsCount, mecanim::ValueArray &values)
	{
		int positionIndex = 0;
		int rotationIndex = 0;
		int scaleIndex = 0;
		
		for (int bindIndex = 0; bindIndex < transformBindingsCount; bindIndex++)
		{
			const BoundCurve& binding = transformBindings[bindIndex];
			
			Transform* targetTransform = reinterpret_cast<Transform*> (binding.targetObject);
			if (binding.targetType == kBindTransformPosition)
			{
				if(targetTransform)				
					values.WritePosition(Vector3fTofloat4(targetTransform->GetLocalPosition()), positionIndex);

				positionIndex++;
			}
			else if (binding.targetType == kBindTransformRotation)
			{
				if(targetTransform)
					values.WriteQuaternion(QuaternionfTofloat4(targetTransform->GetLocalRotation()), rotationIndex);

				rotationIndex++;
			}
			else if (binding.targetType == kBindTransformScale)
			{
				if(targetTransform)
					values.WriteScale(Vector3fTofloat4(targetTransform->GetLocalScale()), scaleIndex);

				scaleIndex++;
			}
		}
	}
	
	static void GetDefaultGenericFloatValues (const AnimatorGenericBindingConstant& bindings, mecanim::ValueArray &values)
	{
		for (int bindIndex = 0; bindIndex < bindings.genericBindingsCount; bindIndex++)
		{
			const BoundCurve& binding = bindings.genericBindings[bindIndex];
			int targetType = binding.targetType;
			if (targetType == kUnbound)
				continue;

			Assert (targetType >= kMinSinglePropertyBinding);
				
			float value = GetBoundCurveFloatValue (binding);

			values.WriteData(value, bindIndex);
		}
	}

	static void GetDefaultGenericPPtrValues (const AnimatorGenericBindingConstant& bindings, mecanim::ValueArray &values)
	{
		for (int bindIndex = 0; bindIndex < bindings.genericPPtrBindingsCount; bindIndex++)
		{
			const BoundCurve& binding = bindings.genericPPtrBindings[bindIndex];
			int targetType = binding.targetType;
			if (targetType == kUnbound)
				continue;
			
			Assert (targetType >= kMinSinglePropertyBinding);
			
			mecanim::int32_t value = GetBoundCurveIntValue (binding);
			
			values.WriteData(value, bindIndex);
		}
	}
	
	static void InitializeDefaultValues (const UnityEngine::Animation::AnimatorGenericBindingConstant& genericBinding, const mecanim::animation::AvatarConstant* avatar, bool hasTransformHierarchy, mecanim::animation::ControllerBindingConstant& controllerBindingConstant)
	{
		const mecanim::skeleton::Skeleton* skeleton = avatar->m_AvatarSkeleton.Get();
		const mecanim::skeleton::SkeletonPose* skeletonPose = avatar->m_AvatarSkeletonPose.Get();
		const mecanim::animation::AnimationSet& animSet = *controllerBindingConstant.m_AnimationSet;

		if (hasTransformHierarchy)
		{
			// Get default values from transform
			GetDefaultTransformValues (genericBinding.transformBindings, genericBinding.transformBindingsCount, *controllerBindingConstant.m_DynamicValuesDefault);
		}
		else
		{
			// When there is no transform & game object hierarchy, get it from the skeleton.
			if (skeleton != NULL && skeletonPose != NULL)
				ValueFromSkeletonPose (*skeleton, *skeletonPose, controllerBindingConstant.m_SkeletonTQSMap, *controllerBindingConstant.m_DynamicValuesDefault);
		}
		
		//  Get default values from generic bindings
		GetDefaultGenericFloatValues (genericBinding, *controllerBindingConstant.m_DynamicValuesDefault);
		GetDefaultGenericPPtrValues (genericBinding, *controllerBindingConstant.m_DynamicValuesDefault);
		
		// Copy default parameters from controller defaults to  m_DynamicValuesDefault
		const mecanim::animation::ControllerConstant* controller = controllerBindingConstant.m_Controller;
		mecanim::ValueArrayReverseCopy(controller->m_Values.Get(), controller->m_DefaultValues.Get(), controllerBindingConstant.m_DynamicValuesConstant, controllerBindingConstant.m_DynamicValuesDefault, animSet.m_AdditionalIndexArray);
	}
	
} }
