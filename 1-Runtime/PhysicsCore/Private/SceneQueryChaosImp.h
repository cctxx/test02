// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CHAOS

inline bool LowLevelRaycastImp(const FVector& Start, const FVector& Dir, float DeltaMag, const Chaos::FImplicitObject& Shape, const FTransform ActorTM, EHitFlags OutputFlags, FHitRaycast& Hit)
{
	//TODO_SQ_IMPLEMENTATION
	return false;			
}

inline bool LowLevelSweepImp(const FTransform& StartTM, const FVector& Dir, float DeltaMag, const FPhysicsGeometry& SweepGeom, const Chaos::FImplicitObject& Shape, const FTransform ActorTM, EHitFlags OutputFlags, FHitSweep& Hit)
{
	//TODO_SQ_IMPLEMENTATION
	return false;
}

inline bool LowLevelOverlapImp(const FTransform& GeomPose, const FPhysicsGeometry& OverlapGeom, const Chaos::FImplicitObject& Shape, const FTransform ActorTM, FHitOverlap& Overlap)
{
	//TODO_SQ_IMPLEMENTATION
	return false;
}
#endif // WITH_CHAOS