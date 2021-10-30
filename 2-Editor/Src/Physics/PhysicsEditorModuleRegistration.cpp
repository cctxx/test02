#include "UnityPrefix.h"
#include "Editor/Src/Physics/GizmoDrawing.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

extern "C" EXPORT_MODULE void RegisterModule_PhysicsEditor ()
{
	REGISTER_GLOBAL_CALLBACK (registerGizmos, RegisterPhysicsGizmos());
}