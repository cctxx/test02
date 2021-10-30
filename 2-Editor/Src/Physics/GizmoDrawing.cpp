#include "UnityPrefix.h"
#include "GizmoDrawing.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/Gizmos/GizmoDrawers.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"
#include "Runtime/Dynamics/HingeJoint.h"
#include "Collider.h"
#include "Runtime/Dynamics/BoxCollider.h"
#include "Runtime/Dynamics/SphereCollider.h"
#include "Runtime/Dynamics/CapsuleCollider.h"
#include "Runtime/Dynamics/RaycastCollider.h"
#include "Runtime/Dynamics/WheelCollider.h"
#include "Runtime/Dynamics/MeshCollider.h"
#include "Runtime/Dynamics/CharacterController.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Editor/Src/PrefKeys.h"


//todo: modularize settings like these. hacked for now.
extern ColorRGBAf GetPrefColor_kGizmoCollider();
extern ColorRGBAf GetPrefColor_kGizmoColliderDisabled();
extern ColorRGBAf GetPrefColor_kGizmoJointAxes();
extern ColorRGBAf GetPrefColor_kGizmoJointAxes2();

static inline void DrawJointArrow (const Vector3f& base, const Vector3f& dir, float size)
{
	Vector3f end = base + dir*size;
	DrawLine (base, end);
	DrawCone (base + dir * (size*0.8f), end, size * 0.07f);
}

static void DrawHingeGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<Joint*> (&o) == NULL);
	Joint& hinge = static_cast<HingeJoint&> (o);

	Vector3f anchor, axis, normal;
	hinge.CalculateGlobalHingeSpace (anchor, axis, normal);
	Vector3f connectedAnchor = hinge.CalculateGlobalConnectedAnchor (false);
	
	gizmos::BeginGizmo( anchor );
	

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	
	float size = CalcHandleSize (anchor) * 0.5f;
	DrawJointArrow (anchor, axis, size);

	gizmos::BeginGizmo( connectedAnchor );

	size = CalcHandleSize (connectedAnchor) * 0.5f;
	DrawJointArrow (connectedAnchor, axis, size);
}

static void DrawConfigurableJointGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<Joint*> (&o) == NULL);
	Joint& hinge = static_cast<Joint&> (o);

	Vector3f anchor, axis, normal;
	hinge.CalculateGlobalHingeSpace (anchor, axis, normal);
	Vector3f connectedAnchor = hinge.CalculateGlobalConnectedAnchor (false);

	gizmos::BeginGizmo( anchor );
	
	float size = CalcHandleSize (anchor) * 0.5;

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawJointArrow (anchor, axis, size);

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes2();
	DrawJointArrow (anchor, normal, size);

	gizmos::BeginGizmo( connectedAnchor );

	size = CalcHandleSize (connectedAnchor) * 0.5f;

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawJointArrow (connectedAnchor, axis, size);
	
	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes2();
	DrawJointArrow (connectedAnchor, normal, size);
}

static void DrawCharacterJointGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<Joint*> (&o) == NULL);
	Joint& hinge = static_cast<Joint&> (o);

	Vector3f anchor, axis, normal;
	hinge.CalculateGlobalHingeSpace (anchor, axis, normal);
	Vector3f connectedAnchor = hinge.CalculateGlobalConnectedAnchor (false);

	gizmos::BeginGizmo( anchor );
	
	float size = CalcHandleSize (anchor) * 0.5;

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawJointArrow (anchor, axis, size);

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes2();
	DrawJointArrow (anchor, normal, size);

	gizmos::BeginGizmo( connectedAnchor );

	size = CalcHandleSize (connectedAnchor) * 0.5f;

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawJointArrow (connectedAnchor, axis, size);
	
	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes2();
	DrawJointArrow (connectedAnchor, normal, size);
}

static void DrawSpringJointGizmo (Object& o, int options, void* userData)
{
	AssertIf (dynamic_pptr_cast<Joint*> (&o) == NULL);
	Joint& hinge = static_cast<Joint&> (o);

	Vector3f anchor, axis, normal;
	hinge.CalculateGlobalHingeSpace (anchor, axis, normal);
	Vector3f connectedAnchor = hinge.CalculateGlobalConnectedAnchor (false);

	gizmos::BeginGizmo( anchor );
	
	float size = CalcHandleSize (anchor);

	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawCap (kCapBox, anchor, axis, size * 0.2f);

	
	gizmos::BeginGizmo( connectedAnchor );
	
	size = CalcHandleSize (connectedAnchor);
	
	gizmos::g_GizmoColor = GetPrefColor_kGizmoJointAxes();
	DrawCap (kCapBox, connectedAnchor, axis, size * 0.2f);
}

static void DrawPrimitiveColliderGizmo (Object& o, int options, void* userData)
{
	const Transform& transform = static_cast<const Unity::Component&> (o).GetComponent (Transform);
	gizmos::BeginGizmo( transform.GetPosition() );
	
	gizmos::g_GizmoColor = GetPrefColor_kGizmoCollider();
	const Collider* collider = dynamic_pptr_cast<const Collider*> (&o);
	if (collider && !collider->GetEnabled ())
		gizmos::g_GizmoColor = GetPrefColor_kGizmoColliderDisabled();
	
	Matrix4x4f noScaleMatrix = transform.GetLocalToWorldMatrixNoScale ();
	
	const BoxCollider* box = dynamic_pptr_cast<const BoxCollider*> (&o);
	if (box)
	{
		SetGizmoMatrix (noScaleMatrix);
		DrawWireCube (noScaleMatrix.InverseMultiplyPoint3Affine (box->GetGlobalCenter ()), box->GetGlobalExtents () * 2.0F);
	}
	const SphereCollider* sphere = dynamic_pptr_cast<const SphereCollider*> (&o);
	if (sphere)
	{
		ClearGizmoMatrix();
		DrawWireSphereTwoShaded (sphere->GetGlobalCenter(), sphere->GetScaledRadius (), transform.GetRotation());
	}
	
	const CapsuleCollider* capsule = dynamic_pptr_cast<const CapsuleCollider*> (&o);
	if (capsule)
	{
		noScaleMatrix = capsule->CalculateTransform ();
		SetGizmoMatrix (noScaleMatrix);
		DrawWireCapsule (noScaleMatrix.InverseMultiplyPoint3Affine (capsule->GetGlobalCenter ()), capsule->GetGlobalExtents ().x, capsule->GetGlobalExtents ().y);
	}

	const RaycastCollider* ray = dynamic_pptr_cast<const RaycastCollider*> (&o);
	if (ray)
	{
		SetGizmoMatrix (noScaleMatrix);
		Vector3f origin = ray->GetGlobalCenter ();
		Vector3f direction = RotateVectorByQuat (ray->GetComponent (Transform).GetRotation (), Vector3f (0, -ray->GetGlobalLength (), 0));
		
		DrawLine (noScaleMatrix.InverseMultiplyPoint3Affine (origin), noScaleMatrix.InverseMultiplyPoint3Affine (origin + direction));
	}
	
	const WheelCollider* wheel = dynamic_pptr_cast<const WheelCollider*>( &o );
	if( wheel )
	{
		SetGizmoMatrix (noScaleMatrix);
		Vector3f origin = wheel->GetGlobalCenter();
		Quaternionf rotation = wheel->GetComponent(Transform).GetRotation();
		Vector3f direction = RotateVectorByQuat (rotation, Vector3f(0, -(wheel->GetGlobalRadius()+wheel->GetGlobalSuspensionDistance()), 0));
		
		Quaternionf steerRot = AxisAngleToQuaternion( Vector3f(0,1,0), Deg2Rad(wheel->GetSteerAngle()) );
		Vector3f sideDir = RotateVectorByQuat(rotation * steerRot, Vector3f(1.0f, 0, 0));
		
		Vector3f torigin = noScaleMatrix.InverseMultiplyPoint3Affine(origin);
		Vector3f tendpoint = noScaleMatrix.InverseMultiplyPoint3Affine(origin + direction);
		DrawLine( torigin, tendpoint );
		DrawWireDisk( torigin, Normalize(noScaleMatrix.InverseMultiplyVector3Affine(sideDir)), wheel->GetGlobalRadius() );
	}
	
	MeshCollider* mesh = dynamic_pptr_cast<MeshCollider*>( &o );
	if( mesh )
	{
		// gizmo for convex mesh collider
		if( mesh->GetConvex() )
		{
			const NxConvexMesh* convex = mesh->GetConvexMesh();
			if( convex )
			{
				NxConvexMeshDesc desc;
				if( convex->saveToDesc( desc ) && desc.triangleStrideBytes==12 && desc.pointStrideBytes==12 )
				{
					SetGizmoMatrix (noScaleMatrix);
					DrawRawMesh( (const Vector3f*)desc.points, (const UInt32*)desc.triangles, desc.numTriangles );
				}
			}
		}
		// draw mesh collider gizmo if it is different from visible mesh
		else
		{
			MeshRenderer* renderer = static_cast<const Unity::Component&> (o).QueryComponent (MeshRenderer);
			if( !renderer || !renderer->GetEnabled() || renderer->GetSharedMesh() != mesh->GetSharedMesh() )
			{
				const NxTriangleMesh* trimesh = mesh->GetTriangleMesh();
				if( trimesh )
				{
					NxTriangleMeshDesc desc;
					if( trimesh->saveToDesc( desc ) && desc.triangleStrideBytes==12 && desc.pointStrideBytes==12 )
					{
						SetGizmoMatrix (noScaleMatrix);
						DrawRawMesh( (const Vector3f*)desc.points, (const UInt32*)desc.triangles, desc.numTriangles );
					}
				}
			}
		}
	}
}

void DrawControllerGizmo (Object& o, int options, void* userData)
{
	const CharacterController* controller = dynamic_pptr_cast<const CharacterController*> (&o);
	if (controller)
	{
		Matrix4x4f noScaleMatrix;
		Vector3f center = controller->GetWorldCenterPosition();
		noScaleMatrix.SetTranslate(center);
		gizmos::BeginGizmo( center );
		gizmos::g_GizmoColor = GetPrefColor_kGizmoCollider();
		
		SetGizmoMatrix (noScaleMatrix);
		DrawWireCapsule (noScaleMatrix.InverseMultiplyPoint3Affine (controller->GetWorldCenterPosition()), controller->GetGlobalExtents ().x, controller->GetGlobalExtents ().y);
	}
}

void RegisterPhysicsGizmos()
{
	GizmoManager& gizmos = GizmoManager::Get ();

	gizmos.AddGizmoRenderer ("BoxCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("SphereCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("CapsuleCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("CharacterController", DrawControllerGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("RaycastCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("WheelCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("MeshCollider", DrawPrimitiveColliderGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("HingeJoint", DrawHingeGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("SpringJoint", DrawSpringJointGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("CharacterJoint", DrawCharacterJointGizmo, GizmoManager::kSelectedOrChild);
	gizmos.AddGizmoRenderer ("ConfigurableJoint", DrawConfigurableJointGizmo, GizmoManager::kSelectedOrChild);
}