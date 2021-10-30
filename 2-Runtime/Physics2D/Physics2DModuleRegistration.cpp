#include "UnityPrefix.h"
#include "Runtime/BaseClasses/ClassRegistration.h"
#include "Runtime/Modules/ModuleRegistration.h"

#if ENABLE_2D_PHYSICS

static void RegisterPhysics2DClasses (ClassRegistrationContext& context)
{
	REGISTER_CLASS (Physics2DSettings)
	REGISTER_CLASS (Rigidbody2D)

	REGISTER_CLASS (Collider2D)
	REGISTER_CLASS (CircleCollider2D)
	REGISTER_CLASS (PolygonCollider2D)
	REGISTER_CLASS (PolygonColliderBase2D)
	#if ENABLE_SPRITECOLLIDER
	REGISTER_CLASS (SpriteCollider2D)
	#endif
	REGISTER_CLASS (BoxCollider2D)
	REGISTER_CLASS (EdgeCollider2D)

	REGISTER_CLASS (Joint2D)
	REGISTER_CLASS (SpringJoint2D)
	REGISTER_CLASS (DistanceJoint2D)
	REGISTER_CLASS (HingeJoint2D)
	REGISTER_CLASS (SliderJoint2D)

	REGISTER_CLASS (PhysicsMaterial2D)
}


#if ENABLE_MONO || UNITY_WINRT
void ExportPhysics2DBindings();

static void RegisterPhysics2DICallModule ()
{
	#if !INTERNAL_CALL_STRIPPING
	ExportPhysics2DBindings ();
	#endif
}
#endif

extern "C" EXPORT_MODULE void RegisterModule_Physics2D()
{
	ModuleRegistrationInfo info;
	info.registerClassesCallback = &RegisterPhysics2DClasses;
	#if ENABLE_MONO || UNITY_WINRT
	info.registerIcallsCallback = &RegisterPhysics2DICallModule;
	#endif
	RegisterModuleInfo(info);
}

#endif // #if ENABLE_2D_PHYSICS
