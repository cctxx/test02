#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_EDITOR_HIERARCHY_ORDERING
#include "Runtime/Utilities/dynamic_array.h"

class Transform;

const SInt32 kDepthSeperation = 1000;

void	MoveTransformForward	(Transform* transform);
void	MoveTransformBackwards	(Transform* transform);
void	OrderTransformHierarchy	(dynamic_array<Transform*>  transform, Transform* target, bool dropUpon);
bool	ContainsTransform		(dynamic_array<Transform*> transforms, Transform* trans);
int		UnifyOrder				(Transform* target, dynamic_array<Transform*> ignoreTrans);
int		FindOrderAndSeperation	(Transform* target, dynamic_array<Transform*> ignoreTrans, int& seperation);
int		FindNextOrder	(bool highest, Transform* target, dynamic_array<Transform*> ignoreTrans);
#endif
