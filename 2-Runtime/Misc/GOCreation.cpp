#include "UnityPrefix.h"
#include "GOCreation.h"
#include "GameObjectUtility.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Dynamics/CapsuleCollider.h"
#include "Runtime/Dynamics/Collider.h"
#include "ResourceManager.h"
#include "Runtime/Filters/Mesh/LodMeshFilter.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Interfaces/IPhysics.h"

GameObject* CreatePrimitive (int type)
{
	GameObject* go = NULL;
	if (type == kPrimitiveSphere)
	{
	 	go = &CreateGameObject ("Sphere", "MeshFilter", "SphereCollider", "MeshRenderer", NULL);
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("New-Sphere.fbx"));

		Collider* collider = go->QueryComponent (Collider);
		if (collider)
		{
			if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
				SmartResetObject(*collider);
		}
		
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	else if (type == kPrimitiveCapsule)
	{
	 	go = &CreateGameObject ("Capsule", "MeshFilter", "CapsuleCollider", "MeshRenderer", NULL);
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("New-Capsule.fbx"));

		CapsuleCollider* collider = go->QueryComponent (CapsuleCollider);
		if (collider != NULL)
		{
			if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
				SmartResetObject(*collider);
			else
				GetIPhysics()->CapsuleColliderSetHeight(*collider, 2.0f);
		}
		
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	else if (type == kPrimitiveCylinder)
	{
	 	go = &CreateGameObject ("Cylinder", "MeshFilter", "CapsuleCollider", "MeshRenderer", NULL);
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("New-Cylinder.fbx"));

		CapsuleCollider* collider = go->QueryComponent (CapsuleCollider);
		if (collider != NULL)
		{
			if (!IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
				SmartResetObject(*collider);
			else
				GetIPhysics()->CapsuleColliderSetHeight(*collider, 2.0f);
		}
		
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	else if (type == kPrimitiveCube)
	{
	 	go = &CreateGameObject ("Cube", 
								"MeshFilter",
#if ENABLE_PHYSICS								
								"BoxCollider", 
#endif								
								"MeshRenderer", 
								NULL);
		
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("Cube.fbx"));
#if ENABLE_PHYSICS
		SmartResetObject(go->GetComponent (Collider));
#endif
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	else if (type == kPrimitivePlane)
	{
	 	go = &CreateGameObject ("Plane", "MeshFilter", "MeshCollider", "MeshRenderer", NULL);
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("New-Plane.fbx"));
		SmartResetObject(go->GetComponent (Collider));
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	else if (type == kPrimitiveQuad)
	{
	 	go = &CreateGameObject ("Quad", "MeshFilter", "MeshCollider", "MeshRenderer", NULL);
	 	go->GetComponent (MeshFilter).SetSharedMesh (GetBuiltinResource<Mesh> ("Quad.fbx"));
		SmartResetObject(go->GetComponent (Collider));
	 	go->GetComponent (Renderer).SetMaterial (Material::GetDefaultDiffuseMaterial(), 0);
	}
	
	return go;
}
