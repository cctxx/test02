#pragma once

#include "Runtime/Modules/ExportModules.h"

struct PlayerLookCallbacks
{
	typedef void UpdateFunc ();
	
	PlayerLookCallbacks ();
	
	// Animators
	UpdateFunc* AnimatorFixedUpdateRetargetIKWrite;
	UpdateFunc* AnimatorUpdateRetargetIKWrite;
	UpdateFunc* AnimatorUpdateFKMove;
	UpdateFunc* AnimatorFixedUpdateFKMove;
	
	// Physics
	UpdateFunc* PhysicsFixedUpdate;
	UpdateFunc* PhysicsUpdate;
	UpdateFunc* PhysicsRefreshWhenPaused;
	UpdateFunc* PhysicsSkinnedClothUpdate;
	UpdateFunc* PhysicsResetInterpolatedTransformPosition;
	
	// 2D Physics
	UpdateFunc* Physics2DUpdate;
	UpdateFunc* Physics2DFixedUpdate;
	UpdateFunc* Physics2DResetInterpolatedTransformPosition;
	
	// Navmesh
	UpdateFunc* NavMeshUpdate;
	
	// Legacy Animation
	UpdateFunc* LegacyFixedAnimationUpdate;
	UpdateFunc* LegacyAnimationUpdate;
};
EXPORT_COREMODULE extern PlayerLookCallbacks gPlayerLoopCallbacks;

#define CALL_UPDATE_MODULAR(x)  if (gPlayerLoopCallbacks.x) gPlayerLoopCallbacks.x ();
#define REGISTER_PLAYERLOOP_CALL(name,body) struct name { static void Forward () { body; } };  gPlayerLoopCallbacks.name = name::Forward;
