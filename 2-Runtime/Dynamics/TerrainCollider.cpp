#include "UnityPrefix.h"

#if ENABLE_TERRAIN && ENABLE_PHYSICS
#include "TerrainCollider.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Dynamics/PhysicsManager.h"

///@TODO: THis should really only be ITerrain
#include "Runtime/Terrain/TerrainData.h"
#include "Runtime/Terrain/Heightmap.h"
#include "Runtime/Interfaces/ITerrainManager.h"

#include "Runtime/Dynamics/CapsuleCollider.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Dynamics/NxWrapperUtility.h"

inline UInt32 GetLowMaterialIndex (UInt32 index)
{
	return index & 0x7F;
}

inline UInt32 GetHighMaterialIndex (UInt32 index)
{
	return index >> 7;
}


IMPLEMENT_CLASS_HAS_INIT(TerrainCollider)
IMPLEMENT_OBJECT_SERIALIZE(TerrainCollider)

TerrainCollider::TerrainCollider (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_Node (this)
{
	m_CreateTreeColliders = true;
	m_CachedInvSize = Vector3f(1.0F, 1.0F, 1.0F);
}

TerrainCollider::~TerrainCollider ()
{
}

void TerrainCollider::SetTerrainData (PPtr<TerrainData> map)
{
	if (m_TerrainData != map)
	{
		m_TerrainData = map;
		Create(NULL);
		SetDirty();
	}
}

TerrainData* TerrainCollider::GetTerrainData ()
{
	return m_TerrainData;
}

void TerrainCollider::Cleanup ()
{
	Super::Cleanup();
	
	for (int i=0;i<m_TreeColliders.size();i++)
		GetDynamicsScene ().releaseActor (m_TreeColliders[i]->getActor());
	m_TreeColliders.clear();
		
	m_Node.RemoveFromList();
}

void TerrainCollider::Create (const Rigidbody* ignoreAttachRigidbody)
{
	Cleanup ();
	
	if (!GetTerrainData())
		return;
	
	ITerrainManager* terrainManager = GetITerrainManager();
	
	//////@TODO: Make this more directly just return the physx representation...
	
	Heightmap* map = &GetTerrainData()->GetHeightmap();
	if (map == NULL || terrainManager->Heightmap_GetNxHeightField(*map) == NULL)
		return;
	
	NxHeightFieldShapeDesc desc;
	desc.heightField = terrainManager->Heightmap_GetNxHeightField(*map);
	
	if (desc.heightField == NULL)
		return;
	
	m_CachedInvSize = Inverse(terrainManager->Heightmap_GetSize(*map));
	Vector3f mapScale = map->GetScale ();
	desc.heightScale = mapScale.y / (float)(Heightmap::kMaxHeight);
	desc.columnScale = mapScale.z;
	desc.rowScale = mapScale.x;
	desc.holeMaterial = 1;
	
	// Smooth sphere collisions on terrains are much better!
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion3_2_a1))
		desc.meshFlags = NX_MESH_SMOOTH_SPHERE_COLLISIONS;
		
	desc.materialIndexHighBits = GetHighMaterialIndex(terrainManager->Heightmap_GetMaterialIndex(*map));
	FinalizeCreate(desc, true, ignoreAttachRigidbody);
		
	if (m_Shape)
	{
		// Insert into front of terrain colliders. Create() may be called from inside
		// of RecreateColliders, and inserting to back will result in eternal loop.
		Heightmap::TerrainColliderList& colliders = map->GetTerrainColliders();
		colliders.insert(colliders.begin(), m_Node);
	}
	
	// Dont create trees in edit mode,its too slow to recreate all those trees when adding new trees
	if (m_CreateTreeColliders && IsWorldPlaying())
		CreateTrees();
}

void TerrainCollider::CreateTrees ()
{
	const float kMinSize = 0.00001F;
	int prototypeCount = m_TerrainData->GetTreeDatabase().GetTreePrototypes().size();
	const TreePrototype* prototypes = prototypeCount > 0 ? &m_TerrainData->GetTreeDatabase().GetTreePrototypes()[0] : NULL;
	
	int instanceCount = m_TerrainData->GetTreeDatabase().GetInstances().size();
	const TreeInstance* instances = instanceCount > 0 ? &m_TerrainData->GetTreeDatabase().GetInstances()[0] : NULL;
	
	
	int supportedMessages = GetGameObject ().GetSupportedMessages ();

	NxActorDesc actorDesc;
	NxCapsuleShapeDesc shapeDesc;
	actorDesc.userData = NULL;
	actorDesc.shapes.push_back (&shapeDesc);
	shapeDesc.materialIndex = GetMaterialIndex ();
	
	if (supportedMessages & kHasCollisionStay)
		actorDesc.group = kContactTouchGroup;
	else if (supportedMessages & (kHasCollisionStay | kHasCollisionEnterExit))
		actorDesc.group = kContactEnterExitGroup;
	else
		actorDesc.group = kContactNothingGroup;
	shapeDesc.userData = this;
	shapeDesc.group = GetGameObject ().GetLayer ();

	Vector3f terrainPositionOffset = GetComponent(Transform).GetPosition();

	Vector3f scale = GetITerrainManager()->Heightmap_GetSize(m_TerrainData->GetHeightmap());
	NxMat33 id33;
	id33.id();
	for (int i=0;i<instanceCount;i++)
	{
		const TreeInstance& instance = instances[i];

		if (instance.index >= prototypeCount)
		{
			ErrorString("Prototype for tree missing.");
			continue;
		}

		const TreePrototype& proto = prototypes[instance.index];
		Vector3f pos = Scale(scale, instance.position) + terrainPositionOffset;;
		GameObject* prefab = proto.prefab;
		if (prefab == NULL)
			continue;
		CapsuleCollider* capsule = prefab->QueryComponent(CapsuleCollider);
		if (capsule == NULL)
			continue;

		float absoluteHeight = max (Abs (capsule->GetHeight() * instance.heightScale), kMinSize);
		float absoluteRadius = Abs (instance.widthScale) * capsule->GetRadius();
	
		float height = absoluteHeight - absoluteRadius * 2.0F;
	
		height = max (height, kMinSize);
		absoluteRadius = max (absoluteRadius, kMinSize);
		
		shapeDesc.height = height;
		shapeDesc.radius = absoluteRadius;
		shapeDesc.group = prefab->GetLayer();
		
		pos += Scale(capsule->GetCenter(), Vector3f(instance.widthScale, instance.heightScale, instance.widthScale));
		
		actorDesc.globalPose = NxMat34(id33, Vec3ToNx(pos ));

		NxActor* actor = GetDynamicsScene ().createActor (actorDesc);
		if (actor == NULL)
		{
			ErrorString ("Could not create tree colliders. Maybe there are more Trees then PhysX can handle?");
			for (int i=0;i<m_TreeColliders.size();i++)
				GetDynamicsScene ().releaseActor (m_TreeColliders[i]->getActor());
			m_TreeColliders.clear();
			return;
		}
		NxShape* shape = actor->getShapes ()[0];

		m_TreeColliders.push_back(shape);
	}
}

template<class TransferFunc>
void TerrainCollider::Transfer (TransferFunc& transfer)
{
	Super::Transfer (transfer);
	transfer.Align();
	TRANSFER(m_TerrainData);
	TRANSFER(m_CreateTreeColliders);
}

void TerrainCollider::FetchPoseFromTransform ()
{
	NxQuat quat; quat.id();
	Vector3f pos = GetComponent(Transform).GetPosition();
	NxMat34 shapeMatrix (quat, (const NxVec3&)pos);

	m_Shape->setGlobalPose(shapeMatrix);
}

void TerrainCollider::InitializeClass ()
{
	REGISTER_MESSAGE (TerrainCollider, kTerrainChanged, TerrainChanged, int);
}

void TerrainCollider::TransformChanged (int changeMask)
{
	Super::TransformChanged(changeMask);

	if (m_Shape)
		FetchPoseFromTransform ();
}

void TerrainCollider::TerrainChanged (int changeMask)
{
	if (changeMask == TerrainData::kWillBeDestroyed)
		Cleanup();
}
#endif
