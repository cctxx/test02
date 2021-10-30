#pragma once

#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Allocator/STLAllocator.h"

/// Forward declaration
class Avatar;
struct HumanDescription;

/// Optimize the transform hierarchy of the GameObject.
///
/// Briefly, it will:
/// 1. Flatten the Transform hierarchy.
/// 2. Remove all the Transforms which are not necessary.
/// 3. The SkinnedMeshRenderers will then use skeleton indices to query bone matrices instead of using Transforms.
void OptimizeTransformHierarchy(Unity::GameObject& root, const UnityStr* exposedTransforms = NULL, size_t exposedTransformCount = 0);

/// De-optimize the transform hierarchy of the GameObject.
void DeoptimizeTransformHierarchy(Unity::GameObject& root);

/// Remove unnecessary transforms.
///
/// 1. If 'human' isn't NULL, the human bones will be kept.
/// 2. All the 'exposedTransforms' will be kept.
/// 3. If 'doKeepSkeleton' is true, the Transforms that are used by SkinnedMeshRenderers will be kept.
/// 
/// If one Transform is kept, its ancestors will also be kept.
void RemoveUnnecessaryTransforms (Unity::GameObject& gameObject, const HumanDescription* human, const UnityStr* exposedTransforms, size_t exposedTransformCount, bool doKeepSkeleton);

/// Query the useful transform paths from the transform hierarchy.
///
/// 1. The output paths will be relative paths to 'root'
/// 2. If there are more than one Components attached to a GameObject, then the corresponding Transform will be regarded as *Useful*.
template <typename Alloc>
void GetUsefulTransformPaths (const Transform& root, const Transform& transform, std::vector<UnityStr, Alloc>& outPaths)
{
	for (int i=0; i<transform.GetChildrenCount(); i++)
	{
		const Transform& child = transform.GetChild(i);
		const GameObject& gameObject = child.GetGameObject();
		if (gameObject.GetComponentCount() > 1)
		{
			UnityStr path = CalculateTransformPath(child, &root);
			outPaths.push_back(path);
		}

		GetUsefulTransformPaths(root, child, outPaths);
	}
}
