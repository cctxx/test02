#include "UnityPrefix.h"

#if ENABLE_EDITOR_HIERARCHY_ORDERING

#include "Editor/Src/Utility/TransformUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/SceneInspector.h"


typedef Transform::VisibleRootMap::const_iterator RootIterator;

void MoveTransformForward(Transform* transform)
{
	dynamic_array<Transform*> ignoreArray (kMemTempAlloc);
	ignoreArray.push_back(transform);
	transform->SetOrder(FindNextOrder(true, transform->GetParent(), ignoreArray) + kDepthSeperation);
}

void MoveTransformBackwards(Transform* transform)
{
	dynamic_array<Transform*> ignoreArray (kMemTempAlloc);
	ignoreArray.push_back(transform);
	transform->SetOrder(FindNextOrder(false, transform->GetParent(), ignoreArray) - kDepthSeperation);
}

void OrderTransformHierarchy (dynamic_array<Transform*>  transform, Transform* target, bool dropUpon)
{
	int seperation = kDepthSeperation;
	int orderIndex = dropUpon ? 
		FindNextOrder(true, target, transform) + kDepthSeperation:
	FindOrderAndSeperation(target, transform, seperation);

	// Set the order on all the children
	for (int i = 0; i < transform.size(); ++i)
	{
		transform[i]->SetOrder(orderIndex);
		orderIndex += seperation;
	}
}

bool ContainsTransform(dynamic_array<Transform*> transforms, Transform* trans)
{
	for(dynamic_array<Transform*>::iterator it = transforms.begin(); it != transforms.end(); ++it)
	{
		if (*it == trans) return true;
	}

	return false;
}

/// <summary>
/// We dont have enough space to insert all the new children at the desired spot. 
/// Resort the children to have original spacing.
/// </summary>

int UnifyOrder(Transform* target, dynamic_array<Transform*> ignoreTrans)
{
	dynamic_array<Transform*> sortedChildren (kMemTempAlloc);

	if (target->GetParent())
	{
		Transform* parent = target->GetParent();
		for (int i = 0; i < parent->GetChildrenCount(); ++i)
		{
			Transform& child = parent->GetChild(i);
			if (ContainsTransform(ignoreTrans, &child))
				continue;
			sortedChildren.push_back(&child);
		}
	}
	else
	{
		const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();

		for (RootIterator i=rootMap.begin();i!=rootMap.end();i++)
		{
			Transform* child = i->second;
			if(ContainsTransform(ignoreTrans, child))
				continue;

			sortedChildren.push_back(child);
		}

	}

	std::sort(sortedChildren.begin(), sortedChildren.end(), Transform::CompareDepths);

	int newOrder = 0;
	int returnOrder = 0;		
	for (int i = 0; i < sortedChildren.size(); ++i)
	{
		Transform* trans = sortedChildren[i];
		if (trans == target)
		{
			returnOrder = newOrder;
			newOrder += kDepthSeperation * ignoreTrans.size();
		}

		trans->SetOrder(newOrder);
		newOrder += kDepthSeperation;
	}

	return returnOrder;
}

static void FindOrderInternal (Transform* target, dynamic_array<Transform*> ignoreTrans, Transform* child, Transform*& previousTrans)
{
	// Needed as the ignoreTrans are already part of the targets children so we dont want to count their previous depth
	if (child == target || 
		child->TestHideFlag(Object::kHideInHierarchy) ||
		ContainsTransform(ignoreTrans, child)) 
		return;

	if (child->GetOrder() <= target->GetOrder())
	{
		if (!previousTrans || child->GetOrder() > previousTrans->GetOrder()) previousTrans = child;
	}
}

Transform* FindOrder (Transform* target, Transform* parent, dynamic_array<Transform*> ignoreTrans)
{
	Transform* previousTrans = NULL;

	if (parent)
	{
		for (int i = 0; i < parent->GetChildrenCount(); ++i)
		{
			Transform& child = parent->GetChild(i);
			FindOrderInternal(target, ignoreTrans, &child, previousTrans);
		}
	}
	else
	{
		const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();

		for (RootIterator i=rootMap.begin();i!=rootMap.end();i++)
		{
			Transform* child = i->second;
			FindOrderInternal(target, ignoreTrans, child, previousTrans);
		}
	}	

	return previousTrans;
}

int FindOrderAndSeperation (Transform* target, dynamic_array<Transform*> ignoreTrans, int& seperation)
{
	Transform* parent = target->GetParent();
	Transform* previousTrans = FindOrder(target, parent, ignoreTrans);

	if (previousTrans)
	{
		int diff = target->GetOrder() - previousTrans->GetOrder();

		if (diff <= ignoreTrans.size()) return UnifyOrder(target, ignoreTrans);

		seperation = diff / (ignoreTrans.size() + 1);
		return previousTrans->GetOrder() + seperation;
	}
	else
	{
		return target->GetOrder() - ignoreTrans.size() * seperation;
	}
}

int FindNextOrder (bool highest, Transform* target, dynamic_array<Transform*> ignoreTrans)
{
	int highestDepth = 0;

	if (target)
	{
		for (int i = 0; i < target->GetChildrenCount(); ++i)
		{
			Transform& child = target->GetChild(i);

			// Needed as the ignoreTrans are already part of the targets children so we dont want to count their previous depth
			if (ContainsTransform(ignoreTrans, &child))
				continue;
			if ((highest && child.GetOrder() >= highestDepth) || (!highest && child.GetOrder() <= highestDepth)) 
				highestDepth = child.GetOrder();
		}
	}
	else
	{
		const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();

		for (RootIterator i=rootMap.begin();i!=rootMap.end();i++)
		{
			Transform* child = i->second;

			// Needed as the ignoreTrans are already part of the targets children so we dont want to count their previous depth
			if (ContainsTransform(ignoreTrans, child)) 
				continue;

			if ((highest && child->GetOrder() >= highestDepth) || (!highest && child->GetOrder() <= highestDepth)) 
				highestDepth = child->GetOrder();
		}
	}

	return highestDepth;
}

#endif
