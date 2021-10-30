#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"

static void RegisterPhysicsClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (Rigidbody)
	REGISTER_CLASS (PhysicsManager)
	REGISTER_CLASS (Collider)
	REGISTER_CLASS (Joint)
	REGISTER_CLASS (HingeJoint)
	REGISTER_CLASS (MeshCollider)
	REGISTER_CLASS (BoxCollider)
	REGISTER_CLASS (ConstantForce)
	REGISTER_CLASS (PhysicMaterial)
	REGISTER_CLASS (SphereCollider)
	REGISTER_CLASS (CapsuleCollider)
	REGISTER_CLASS (FixedJoint)
	REGISTER_CLASS (RaycastCollider)
	REGISTER_CLASS (CharacterController)
	REGISTER_CLASS (CharacterJoint)
	REGISTER_CLASS (SpringJoint)
	REGISTER_CLASS (WheelCollider)
	REGISTER_CLASS (ConfigurableJoint)

#if ENABLE_CLOTH
	REGISTER_CLASS (Cloth)
	REGISTER_CLASS (InteractiveCloth)
	REGISTER_CLASS (ClothRenderer)
	REGISTER_CLASS (SkinnedCloth)
#endif

#if ENABLE_TERRAIN
	REGISTER_CLASS (TerrainCollider)
#endif
}

#if ENABLE_MONO || UNITY_WINRT
void ExportNewDynamics ();
#if UNITY_EDITOR
void ExportColliderUtil ();
#endif

static void RegisterPhysicsICallModule ()
{
	///@TODO: Maybe this ifdef should be moved to the cspreprocess generated code instead???? (For all modules)
	
#if !INTERNAL_CALL_STRIPPING
	ExportNewDynamics ();
#if UNITY_EDITOR
	ExportColliderUtil ();
#endif
#endif
}
#endif

extern "C" EXPORT_MODULE void RegisterModule_Physics ()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterPhysicsClasses;
#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterPhysicsICallModule;
#endif
	RegisterModuleInfo (info);
}
#endif