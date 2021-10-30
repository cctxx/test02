#ifndef SUPPORTEDMESSAGEOPTIMIZATION_H
#define SUPPORTEDMESSAGEOPTIMIZATION_H

enum
{
	kHasCollisionStay =   1 << 0,
	kHasCollisionEnterExit =  1 << 1,
	kWantsCollisionData = 1 << 2,
	kSupportsTransformChanged = 1 << 3,
	kHasOnWillRenderObject = 1 << 4,
	kSupportsVelocityChanged = 1 << 5,
	kHasOnAnimatorMove = 1 << 6,
	kHasOnAnimatorIK = 1 << 7,
	kHasCollision2D = 1 << 8
};

#endif
