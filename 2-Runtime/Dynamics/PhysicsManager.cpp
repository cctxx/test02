#include "UnityPrefix.h"
#if ENABLE_PHYSICS
#include "PhysicsManager.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Input/TimeManager.h"
#include "RigidBody.h"
#include "Collider.h"
#include "Joint.h"
#include "PhysicMaterial.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxIntersectionBoxBox.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Utilities/Utility.h"
#include "NxWrapperUtility.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Cloth.h"
#include "SkinnedCloth.h"
#include "CharacterController.h"
#include "PhysXRaycast.h"
#include "Runtime/Profiler/ProfilerStats.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/ScriptingManager.h"
#include "Runtime/Scripting/ScriptingExportUtility.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Core/Callbacks/PlayerLoopCallbacks.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"
#include "PhysicsModule.h"

#if UNITY_PS3
#include "PS3/NxCellConfiguration.h"

extern uint8_t g_aPhysXPriorities[8];
extern CellSpurs2* g_pSpursInstance;

#endif

inline void VerifyObjectPtr(Object* obj)
{
	#if !UNITY_RELEASE
	if (obj == NULL)
		return;
	// OutputDebugString is windows/360 only!
	//if (Object::IDToPointer(obj->GetInstanceID()) != obj)
	//	OutputDebugString(Format("%x\n", obj).c_str());
	Assert(Object::IDToPointer(obj->GetInstanceID()) == obj);
	#endif
}

using namespace std;
using namespace Unity;

PROFILER_INFORMATION(gRaycastProfile, "Physics.Raycast", kProfilerPhysics)
PROFILER_INFORMATION(gRaycastAllProfile, "Physics.RaycastAll", kProfilerPhysics)
PROFILER_INFORMATION(gCapsuleTestProfile, "Physics.CheckCapsule", kProfilerPhysics)
PROFILER_INFORMATION(gSphereOverlapProfile, "Physics.OverlapSphere", kProfilerPhysics)
PROFILER_INFORMATION(gSphereTestProfile, "Physics.TestSphere", kProfilerPhysics)
PROFILER_INFORMATION(gCapsuleCastProfile, "Physics.CapsuleCast", kProfilerPhysics)
PROFILER_INFORMATION(gCapsuleCastAllProfile, "Physics.CapsuleCastAll", kProfilerPhysics)


#define RETURNIFNOPHYSX( v ) if(!gPhysicsSDK) return v;
#define RETURNIFNOPHYSX_VOID() if(!gPhysicsSDK) return;


static Vector3f CalculateRelativeVelocity (NxActor& a, NxActor& b);

#define DEBUG_VISUALIZE_PHYSX 0

enum
{
#if UNITY_WII
	kNovodexMemoryAlignment = 8
#else
	kNovodexMemoryAlignment = kDefaultMemoryAlignment
#endif
};

class NovodexAllocator : public NxUserAllocator
{
	public:

	void* malloc(size_t size, NxMemoryType type)
	{
		return UNITY_MALLOC_ALIGNED(kMemPhysics, size, kNovodexMemoryAlignment);
	}

	void* malloc(size_t size)
	{
		return UNITY_MALLOC_ALIGNED(kMemPhysics, size, kNovodexMemoryAlignment);
	}

	void* mallocDEBUG(size_t size, const char* fileName, int line, const char* className, NxMemoryType type)
		{
		return UNITY_MALLOC_ALIGNED(kMemPhysics, size, kNovodexMemoryAlignment);
		return 0;
		}
	void* mallocDEBUG(size_t size, const char* fileName, int line)
		{
		return UNITY_MALLOC_ALIGNED(kMemPhysics, size, kNovodexMemoryAlignment);
		return 0;
		}

	void* realloc(void* memory, size_t size)
	{
		return UNITY_REALLOC_ALIGNED(kMemPhysics, memory, size, kNovodexMemoryAlignment);
	}

	void free(void* memory)
	{
		UNITY_FREE(kMemPhysics, memory);
	}

/*  check is a bad name to choose. The MacOSX libraries already Carbon declare that function name.
	void check()
	{
	}
*/
};


struct ErrorStream : public NxUserOutputStream
{
	virtual void reportError(NxErrorCode code, const char * message, const char *file, int line)
	{
		if (code == NXE_DB_WARNING)
			return;

		DebugStringToFile (message, 0, file, line, kError);
	}
	/**
	Reports an assertion violation.  The user should return
	*/
	virtual NxAssertResponse reportAssertViolation(const char * message, const char *file, int line)
	{
		DebugStringToFile (message, 0, file, line, kAssert);
		return NX_AR_CONTINUE;
	}
	/**
	Simply prints some debug text
	*/
	virtual void print(const char * message)
	{
		LogString (message);
	}
};

static void CreateDynamicsScene ();

/*
struct CollisionData
{

};
*/

class RaycastCollector : public NxUserRaycastReport
{
	public:
	PhysicsManager::RaycastHits* hits;

	virtual bool onHit(const NxRaycastHit& hit)
	{
		RaycastHit outHit;
		NxToRaycastHit(hit, outHit);
		hits->push_back(outHit);
		return true;
	}
};


class TriggerMessage : public NxUserTriggerReport
{
	public:
	PhysicsManager::RecordedTriggers* record;

	virtual void onTrigger(NxShape& triggerShape, NxShape& otherShape, NxTriggerFlag status)
	{
		Collider* trigger = (Collider*)triggerShape.userData;
		Collider* collider = (Collider*)otherShape.userData;
		record->push_back (PhysicsManager::RecordedTrigger ());
		record->back ().trigger = trigger;
		record->back ().collider = collider;
		record->back ().status = status;
	}
};

static Vector3f CalculateRelativeVelocity (NxActor& a, NxActor& b)
{
	NxVec3 vel0, vel1;
	if (a.isDynamic ())
		vel0 = a.getLinearVelocity ();
	else
		vel0.zero ();

	if (b.isDynamic ())
		vel1 = b.getLinearVelocity ();
	else
		vel1.zero ();
	vel0 -= vel1;

	return (const Vector3f&)vel0;
}

class ContactMessage : public NxUserContactReport
{
	public:
	PhysicsManager::RecordedContacts* record;

	virtual void onContactNotify (NxContactPair& pair, NxU32 status)
	{

		Rigidbody* thisRigidbody = (Rigidbody*)pair.actors[0]->userData;
		Rigidbody* otherRigidbody = (Rigidbody*)pair.actors[1]->userData;

		Collider* thisCollider = NULL;
		if (pair.actors[0]->getNbShapes())
			thisCollider = (Collider*)pair.actors[0]->getShapes ()[0]->userData;

		Collider* otherCollider = NULL;
		if (pair.actors[1]->getNbShapes())
			otherCollider = (Collider*)pair.actors[1]->getShapes ()[0]->userData;

		record->push_back (Collision ());
		record->back ().thisRigidbody = thisRigidbody;
		record->back ().otherRigidbody = otherRigidbody;
		record->back ().thisCollider = thisCollider;
		record->back ().otherCollider = otherCollider;
		record->back ().status = status;
		record->back ().impactForceSum = (const Vector3f&)pair.sumNormalForce;
		record->back ().frictionForceSum = (const Vector3f&)pair.sumFrictionForce;
		record->back ().relativeVelocity = CalculateRelativeVelocity (*pair.actors[0], *pair.actors[1]);

		VerifyObjectPtr(record->back ().thisRigidbody);
		VerifyObjectPtr(record->back ().otherRigidbody);
		VerifyObjectPtr(record->back ().thisCollider);
		VerifyObjectPtr(record->back ().otherCollider);

		std::list<ContactPoint>& contacts = record->back ().contacts;
		NxContactStreamIterator i (pair.stream);

		//user can call getNumPairs() here
		while(i.goNextPair())
		{
			Collider* c0 = i.isDeletedShape(0) ? NULL : (Collider*)i.getShape (0)->userData;
			Collider* c1 = i.isDeletedShape(1) ? NULL : (Collider*)i.getShape (1)->userData;
			//user can also call getShape() and getNumPatches() here
			while(i.goNextPatch())
			{
				Vector3f normal = (const Vector3f&)i.getPatchNormal ();
				//user can also call getPatchNormal() and getNumPoints() here
				while(i.goNextPoint())
				{
					ContactPoint c;
					c.collider[0] = c0;
					c.collider[1] = c1;
					c.point = (const Vector3f&)i.getPoint ();
					c.normal = normal;
					contacts.push_back (c);
				}
			}
		}
	}
};

class BrokenJointMessage : public NxUserNotify
{
	public:
	PhysicsManager::RecordedJointBreaks* records;

	virtual bool onJointBreak (NxReal breakingForce, NxJoint & brokenJoint)
	{
		Joint* joint = (Joint*)brokenJoint.userData;
		joint->NullJoint();
		PhysicsManager::RecordedJointBreak jointbreak;
		jointbreak.impulse = breakingForce;
		jointbreak.joint = joint;
		records->push_back(jointbreak);
		return true;
	}

	virtual void onWake(NxActor** actors, NxU32 count)  {}
	virtual void onSleep(NxActor** actors, NxU32 count) { }
};

static ErrorStream	 gErrorStream;
static NovodexAllocator gAllocator;
static NxPhysicsSDK* gPhysicsSDK = NULL;
static NxScene*      gPhysicsScene = NULL;
static std::vector<NxScene*> gClothingScenes;
static NxScene*      gInactivePhysicsScene = NULL;
static NxActor*      gNULLActor = NULL;

void PhysicsManager::InitializeClass ()
{
	RegisterAllowNameConversion (PhysicsManager::GetClassStringStatic(), "m_BounceTreshold", "m_BounceThreshold");

	PhysXRaycast::InitializeClass();
	InitializePhysicsModule();

	gPhysicsSDK = NxCreatePhysicsSDK (NX_PHYSICS_SDK_VERSION, &gAllocator, &gErrorStream);
	if (!gPhysicsSDK)
		FatalErrorString ("Couldn't load physics");
	if (!NxInitCooking (0, &gErrorStream))
		FatalErrorString ("Couldn't load physics ");

#if UNITY_PS3
	NxCellSpursControl::initWithSpurs(g_pSpursInstance,5, g_aPhysXPriorities);
#endif

#if DEBUG_VISUALIZE_PHYSX
	gPhysicsSDK->setParameter (NX_VISUALIZATION_SCALE, 1F);
	gPhysicsSDK->setParameter (NX_VISUALIZE_COLLISION_SHAPES, 1F);
//	gPhysicsSDK->setParameter (NX_VISUALIZE_COLLISION_COMPOUNDS, 1F);
#endif

	gPhysicsSDK->setParameter(NX_IMPROVED_SPRING_SOLVER, 0);

	CreateDynamicsScene ();
	CharacterController::CreateControllerManager ();
	
	REGISTER_PLAYERLOOP_CALL (PhysicsFixedUpdate, GetPhysicsManager().FixedUpdate());
	REGISTER_PLAYERLOOP_CALL (PhysicsUpdate, GetPhysicsManager().Update ());
	REGISTER_PLAYERLOOP_CALL (PhysicsRefreshWhenPaused, GetPhysicsManager().RefreshWhenPaused ());
	REGISTER_PLAYERLOOP_CALL (PhysicsSkinnedClothUpdate, GetPhysicsManager().SkinnedClothUpdate ());
	REGISTER_PLAYERLOOP_CALL (PhysicsResetInterpolatedTransformPosition, GetPhysicsManager().ResetInterpolatedTransformPosition ());
	
	REGISTER_GLOBAL_CALLBACK (didUnloadScene, GetPhysicsManager().RecreateScene());
}


// If we use multiple scenes per CPU, we may get better parallelization, when scene load becomes
// unbalances because cloth objects get disabled or destroyed.
// Apparently, PhysX doesn't handle more then 64 scenes - and we use two for rigidbody physics.

#if (UNITY_XENON || UNITY_PS3)
#	define NUMBER_OF_CLOTHING_SCENES_PER_CPU 1
#	define MAX_CLOTHING_SCENES 6
#else
#	define NUMBER_OF_CLOTHING_SCENES_PER_CPU 4
#	define MAX_CLOTHING_SCENES 32
#endif

static void CreateDynamicsScene ()
{
	AssertIf (gPhysicsScene != NULL);
	NxSceneDesc sceneDesc;
	// Multi threading causes issues when simulating 1000 objects at the same location.
	// We are not taking advantage of it anyway. So...
	// Test is in exception unit test on rudolph
	sceneDesc.flags &= ~NX_SF_SIMULATE_SEPARATE_THREAD;

	if (gInactivePhysicsScene == NULL)
		gInactivePhysicsScene = gPhysicsSDK->createScene(sceneDesc);


	gPhysicsScene = gPhysicsSDK->createScene(sceneDesc);
	gPhysicsScene->setGravity (NxVec3 (0.0F, -9.81F, 0.0F));
	gPhysicsScene->setTiming (1.0F, 8, NX_TIMESTEP_VARIABLE);
//broken?
#if !UNITY_WINRT
	if (systeminfo::GetProcessorCount() > 1)
	{
		int numClothingScenes = std::min(NUMBER_OF_CLOTHING_SCENES_PER_CPU * systeminfo::GetProcessorCount(), MAX_CLOTHING_SCENES);
		gClothingScenes.resize (numClothingScenes);
	}
	else
#endif
		gClothingScenes.resize (1);
	for (std::vector<NxScene*>::iterator i = gClothingScenes.begin(); i!=gClothingScenes.end(); i++)
		*i = NULL;

	// Setup actor group masks!
	// We put every actor into a group depending on what contact notifications it wants.
	// This way we don't send too many contact notify messages!
	int masks[3] = {
		0,
		NX_NOTIFY_ON_START_TOUCH | NX_NOTIFY_ON_END_TOUCH,
		NX_NOTIFY_ON_TOUCH | NX_NOTIFY_ON_START_TOUCH | NX_NOTIFY_ON_END_TOUCH
	};

	for (int i=0;i<3;i++)
	{
		for (int j=0;j<3;j++)
			GetDynamicsScene ().setActorGroupPairFlags (i, j, masks[i] | masks[j]);
	}

	GetDynamicsScene ().setGroupCollisionFlag (kIgnoreCollisionLayer, kIgnoreCollisionLayer, 0);

	// Create hole material
	NxMaterialDesc holematerial;
	NxMaterial* material = GetDynamicsScene ().createMaterial (holematerial);
	AssertIf(material->getMaterialIndex() != 1);

	// Create wheel collider material - used by wheel collider
	NxMaterialDesc wheelMaterial;
	wheelMaterial.flags |= NX_MF_DISABLE_FRICTION;

	material = GetDynamicsScene ().createMaterial (wheelMaterial);
	AssertIf(material->getMaterialIndex() != 2);
}

void PhysicsManager::CleanupReports()
{
	// is there any point doing these allocations?

	TriggerMessage* trigger = (TriggerMessage*)gPhysicsScene->getUserTriggerReport();
	gPhysicsScene->setUserTriggerReport(NULL);
	delete trigger;

	ContactMessage* contact = (ContactMessage*)gPhysicsScene->getUserContactReport();
	gPhysicsScene->setUserContactReport(NULL);
	delete contact;

	BrokenJointMessage* notify = (BrokenJointMessage*)gPhysicsScene->getUserNotify();
	gPhysicsScene->setUserNotify(NULL);
	delete notify;
}

void PhysicsManager::CreateReports()
{
	CleanupReports();

	TriggerMessage* triggerMessage = new TriggerMessage ();
	triggerMessage->record = &m_RecordedTriggers;
	gPhysicsScene->setUserTriggerReport (triggerMessage);

	ContactMessage* contactMessage = new ContactMessage ();
	contactMessage->record = &m_RecordedContacts;
	gPhysicsScene->setUserContactReport (contactMessage);

	BrokenJointMessage* brokenJoint = new BrokenJointMessage ();
	brokenJoint->records = &m_RecordedJointBreaks;
	gPhysicsScene->setUserNotify (brokenJoint);
}

void PhysicsManager::CleanupClass ()
{
	PhysXRaycast::CleanupClass();
	CleanupPhysicsModule();

	if( !gPhysicsSDK )
		return; // happens when user quits the screen selector

	// Needs to be cleaned up before the physics scene, or we get a crash, if there
	// are still controllers around (as is the case in the editor, which does not clean up all objects).
#if ENABLE_PHYSICS
	CharacterController::CleanupControllerManager ();
#endif

	vector<PhysicMaterial*> materialsTemp;
	vector<NxMaterialDesc> materialDescsTemp;
	ReleaseMaterials(materialsTemp, materialDescsTemp);

	CleanupReports();

	gPhysicsSDK->releaseScene (*gPhysicsScene);
	gPhysicsSDK->releaseScene (*gInactivePhysicsScene);

	for (std::vector<NxScene*>::iterator i = gClothingScenes.begin(); i!=gClothingScenes.end(); i++)
	{
		if (*i != NULL)
		{
			gPhysicsSDK->releaseScene (**i);
			*i = NULL;
		}
	}
	std::vector<NxScene*> emptyScenes;
	gClothingScenes.swap(emptyScenes);

	// We are not using NxReleasePhysicsSDK, because it doesn't release the sdk (a bug in our implementation of USE_STATIC_LIBS)
	// it's fine to call the release on gPhysicsSDK directly, except that it doesn't set g_nxPhysicsSDK in PhysX to NULL
	//	NxReleasePhysicsSDK(gPhysicsSDK);
	
	NxFoundationSDK* foundationSDK = NxGetFoundationSDK ();
	gPhysicsSDK->release ();
	gPhysicsSDK = NULL;

	foundationSDK->release();
	
#if UNITY_PS3
	if(NxCellSpursControl::isSpursInitialized())
		CELLCALL(NxCellSpursControl::terminate());
#endif
}


PhysicsManager::PhysicsManager (MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
	m_RaycastsHitTriggers = true;

	m_Gravity = Vector3f (0, -9.81F, 0.0F);
	m_LayerCollisionMatrix.resize (kNumLayers, 0xffffffff);
	m_DefaultIterationCount = 6;
	m_RigidbodyTransformMessageEnabled = true;
	m_SmoothedClothDeltaTime = 0.0f;

	RETURNIFNOPHYSX_VOID();

	GetDynamicsSDK ().setParameter (NX_SKIN_WIDTH, 0.01F);
	GetDynamicsSDK ().setParameter (NX_CONTINUOUS_CD, true);
	GetDynamicsSDK ().setParameter (NX_CCD_EPSILON, 0.01F);

	SetupDefaultMaterial ();
	CreateReports();
	Object::FindAllDerivedClasses (ClassID (Collider), &m_DisableTransformMessage);
}

void PhysicsManager::Reset ()
{
	RETURNIFNOPHYSX_VOID();

	Super::Reset();

	m_Gravity = Vector3f (0, -9.81F, 0.0F);
	m_LayerCollisionMatrix.resize (kNumLayers, 0xffffffff);
	m_DefaultIterationCount = 6;
	GetDynamicsSDK ().setParameter (NX_SKIN_WIDTH, 0.01F);


}

PhysicsManager::~PhysicsManager ()
{
	CleanupReports();
	// delete objects after setting them to null on novodex - just in case
}

inline Unity::Component* AttachedRigidbodyOrCollider (Collider& collider)
{
	Rigidbody* body = collider.GetRigidbody ();
	if (body)
		return body;
	else
		return &collider;
}


inline Unity::Component* GetRigidbodyOrCollider (Collision& col)
{
	return col.thisRigidbody != NULL ? (Unity::Component*)col.thisRigidbody : (Unity::Component*)col.thisCollider;
}

inline Unity::Component* GetOtherRigidbodyOrCollider (Collision& col)
{
	return col.otherRigidbody != NULL ? (Unity::Component*)col.otherRigidbody : (Unity::Component*)col.otherCollider;
}

void PhysicsManager::SetTransformMessageEnabled(bool enable)
{
	// Disable rigid body / collider transform changed message handler
	// This simply avoids setting the position/rotation in the rigidbody while we are fetching the novodex state.
	for (int i=0;i<m_DisableTransformMessage.size ();i++)
	{
		GameObject::GetMessageHandler ().SetMessageEnabled (m_DisableTransformMessage[i], kTransformChanged.messageID, enable);
	}

	m_RigidbodyTransformMessageEnabled = enable;
}

void PhysicsManager::ReleaseMaterials(vector<PhysicMaterial*>& materials, vector<NxMaterialDesc>& materialDescs)
{
	materials.clear();
		Object::FindObjectsOfType(&materials);

	materialDescs.resize(materials.size());

		for (int i=0;i<materials.size();i++)
		{
			materials[i]->m_Material->saveToDesc(materialDescs[i]);
			GetDynamicsScene ().releaseMaterial (*materials[i]->m_Material);
			materials[i]->m_Material = NULL;
		}
}

void PhysicsManager::RecreateScene ()
{
	RETURNIFNOPHYSX_VOID();

	if (GetDynamicsScene().getNbActors() == 0 && GetDynamicsScene().getNbStaticShapes() == 0 && GetDynamicsScene().getNbJoints() == 0)
	{
		vector<PhysicMaterial*> materials;
		vector<NxMaterialDesc> materialDescs;
		ReleaseMaterials(materials, materialDescs);

		CleanupReports();
		for (std::vector<NxScene*>::iterator i = gClothingScenes.begin(); i!=gClothingScenes.end(); i++)
		{
			if (*i != NULL)
			{
				gPhysicsSDK->releaseScene (**i);
				*i = NULL;
			}
		}
		gPhysicsSDK->releaseScene (*gPhysicsScene);
		gPhysicsScene = NULL;
		CreateDynamicsScene ();
		CreateReports();

		for (int i=0;i<materials.size();i++)
		{
            NxMaterial* material = GetDynamicsScene ().createMaterial (materialDescs[i]);
            
            if (material)
            {
                materials[i]->m_MaterialIndex = material->getMaterialIndex ();
            }
            else
            {
                ErrorString(std::string("Invalid physics material ") + materials[i]->GetName());
                materials[i]->m_MaterialIndex = 0;
            }
            
			materials[i]->m_Material = material;

			materials[i]->AwakeFromLoad (kDefaultAwakeFromLoad);
		}

		GetPhysicsManager().AwakeFromLoad(kDefaultAwakeFromLoad);
	}
}

PROFILER_INFORMATION(gPhysicsProfile, "Physics.Simulate", kProfilerPhysics)
PROFILER_INFORMATION(gPhysicsClothProfile, "Physics.UpdateSkinnedCloth", kProfilerPhysics)
PROFILER_INFORMATION(gPhysicsInterpolationProfile, "Physics.Interpolation", kProfilerPhysics)

void PhysicsManager::UpdateSkinnedClothes ()
{
#if ENABLE_CLOTH
	if (m_SmoothedClothDeltaTime == 0.0)
		m_SmoothedClothDeltaTime = GetTimeManager().GetDeltaTime();
	else
		m_SmoothedClothDeltaTime = Lerp (m_SmoothedClothDeltaTime, GetTimeManager().GetDeltaTime(), 0.01F);

	if(!gClothingScenes.size())
		return;

	PROFILER_AUTO(gPhysicsClothProfile, NULL)

	Assert(m_ActiveSkinnedMeshes.empty());
	SkinnedMeshRenderer::UpdateAllSkinnedMeshes(SkinnedMeshRenderer::kUpdateCloth, &m_ActiveSkinnedMeshes);

#if ENABLE_MULTITHREADED_SKINNING
	JobScheduler& js = GetJobScheduler();
	m_ClothJobGroup = js.BeginGroup (gClothingScenes.size());
#endif

	for (std::vector<NxScene*>::iterator i = gClothingScenes.begin(); i!= gClothingScenes.end(); i++)
	{
		if (*i != NULL)
		{
			if ((*i)->getNbCloths() == 0)
			{
				gPhysicsSDK->releaseScene (**i);
				(*i) = NULL;
			}
			else
			{
			#if ENABLE_MULTITHREADED_SKINNING
				js.SubmitJob (m_ClothJobGroup, SimulateClothingScene, *i, NULL);
			#else
				SimulateClothingScene(*i);
			#endif
			}
		}
	}
#endif
}

void PhysicsManager::FinishUpdatingSkinnedClothes ()
{
#if ENABLE_CLOTH
#if ENABLE_MULTITHREADED_SKINNING
	PROFILER_AUTO(gPhysicsClothProfile, NULL)

	JobScheduler& js = GetJobScheduler();
	js.WaitForGroup (m_ClothJobGroup);
#endif

	SkinnedMeshRenderer::UploadSkinnedClothes(m_ActiveSkinnedMeshes);
	m_ActiveSkinnedMeshes.clear();
#endif
}

void PhysicsManager::FixedUpdate ()
{
	PROFILER_AUTO(gPhysicsProfile, NULL)

	RETURNIFNOPHYSX_VOID();

	AssertIf (!m_RecordedTriggers.empty ());

	// Store interpolated position
	for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin();i!=m_InterpolatedBodies.end();i++)
	{
		Rigidbody* body = i->body;
		i->disabled = 0;
		if (body->GetInterpolation() == kInterpolate)
		{
			i->position = body->GetPosition();
			i->rotation = body->GetRotation();
		}
	}

	// Force inactive scene to deallocate memory for all released actors
	// Requires PhysX code to be patched accordingly
	// NOTE: we don't need to call it on active physics scene since such deallocation will happen inside simulate() method
	gInactivePhysicsScene->flushCaches();

	gPhysicsScene->simulate(GetTimeManager ().GetFixedDeltaTime ());

   	gPhysicsScene->flushStream();
	gPhysicsScene->fetchResults(NX_RIGID_BODY_FINISHED, true);

	// Disable rigid body / collider / controller transform changed message handler
	// This simply avoids setting the position/rotation in the rigidbody while we are fetching the novodex state.
	SetTransformMessageEnabled (false);

	// Update position / rotation of all rigid bodies
	MessageData velocityMessageData;
	for (int level=0;level<kMaxSortedActorsDepth;level++)
	{
		RigidbodyList& bodies = m_SortedActors[level];
		for (RigidbodyList::iterator i=bodies.begin();i != bodies.end();i++)
		{
			Rigidbody& body = **i;
			NxActor* actor = body.m_Actor;

			if (actor->isSleeping ())
				continue;

			if (body.m_DisableReadUpdateTransform == 0)
			{
				GameObject& go = body.GetGameObject();
				Transform& transform = go.GetComponent (Transform);
				NxVec3 pos = actor->getGlobalPosition ();
				NxQuat rot = actor->getGlobalOrientationQuat ();

				/// @TODO: DONT NORMALIZE. NOVODEX VALUES ARE PRETTY CLOSE TO UNIT.
				/// INSTEAD MAKE OUR NORMALIZE ASSERT CHECKS MORE RELAXED! LOTS OF WORK!
				transform.SetPositionAndRotationSafe ((const Vector3f&)pos, (const Quaternionf&)rot);


				// Synchronize velocity with other components eg. NavMesh
				if (go.GetSupportedMessages() & kSupportsVelocityChanged)
				{
					Vector3f velocity = Vec3FromNx(actor->getLinearVelocity ());
					velocityMessageData.SetData(&velocity, ClassID(Vector3f));

					go.SendMessageAny(kDidVelocityChange, velocityMessageData);
				}
			}
		}
	}

	// Enable rigid body transform changed message handler
	SetTransformMessageEnabled (true);

	ProcessRecordedReports();
}

void PhysicsManager::ResetInterpolatedTransformPosition ()
{
	PROFILER_AUTO(gPhysicsInterpolationProfile, NULL)

	for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin();i!=m_InterpolatedBodies.end();i++)
	{
		Rigidbody* body = i->body;
		if (body->IsSleeping())
			continue;

		Transform& transform = body->GetComponent(Transform);

		Vector3f pos = body->GetPosition();
		Quaternionf rot = body->GetRotation();
		transform.SetPositionAndRotationSafeWithoutNotification (pos, rot);
	}
}

#if ENABLE_PROFILER
void PhysicsManager::GetPerformanceStats(PhysicsStats& physicsStats)
{
	NxSceneStats stats;
	GetDynamicsScene().getStats(stats);

	physicsStats.activeRigidbodies = stats.numDynamicActorsInAwakeGroups;
	physicsStats.sleepingRigidbodies = stats.numDynamicActors - stats.numDynamicActorsInAwakeGroups;

	physicsStats.numberOfShapePairs = stats.numPairs;

	physicsStats.numberOfStaticColliders = stats.numStaticShapes;
	physicsStats.numberOfDynamicColliders = stats.numDynamicShapes;
}
#endif

void PhysicsManager::SkinnedClothUpdate ()
{
	UpdateSkinnedClothes();
	FinishUpdatingSkinnedClothes ();
}

void *PhysicsManager::SimulateClothingScene (void *voidScene)
{
#if ENABLE_CLOTH
	NxScene *scene = (NxScene*)voidScene;

	// For correct simulation, cloth should be updated in Fixed Update.
	// However, dynamic update tends to behave better for performance - as cloth is a purely visible effect, we care more about
	// performance, then about accuracy.
	// The problem is that physX seems to apply external forces stronger when using a longer delta time for the simulation,
	// resulting in cloth hanging down further at slower frame rates, because gravity is stronger. This can be fixed by adjusting
	// the gravity and external forces accordingly.

	// Also, we don't use the actual dynamic delta time, but a highly smoothed version of it.
	// Otherwise, the changes in frame rate cause the cloth to jitter.
	// This will make the cloth simulate to fast or too slow for a seconds or so, when the frame rate changes, resulting in
	// the appearance of more or less damping. While this is technically incorrect, it likely won't be notced, and seems to be better
	// then the alternative of running in fixed time.

	float timeScale = GetTimeManager().GetFixedDeltaTime() / GetPhysicsManager().m_SmoothedClothDeltaTime;
	Vector3f adjustedGravity = GetPhysicsManager().m_Gravity*timeScale;
	scene->setGravity ((NxVec3&)adjustedGravity);

	// same adjustment for external forces
	int numCloths = scene->getNbCloths();
	NxCloth **cloths = scene->getCloths();
	for (int j=0; j<numCloths; j++)
		cloths[j]->setExternalAcceleration(cloths[j]->getExternalAcceleration() * timeScale);

	scene->simulate (GetPhysicsManager().m_SmoothedClothDeltaTime);
   	scene->flushStream();
	scene->fetchResults(NX_RIGID_BODY_FINISHED, true);

	for (int j=0; j<numCloths; j++)
		((SkinnedCloth*)(cloths[j]->userData))->ReadBackSkinnedBuffers();
#endif
	return NULL;
}

NxScene* PhysicsManager::GetClothingScene ()
{
#if ENABLE_CLOTH
	int minVertices = std::numeric_limits<int>::max();
	std::vector<NxScene*>::iterator found = gClothingScenes.begin();
	for (std::vector<NxScene*>::iterator i = gClothingScenes.begin(); i!= gClothingScenes.end(); i++)
	{
		if ((*i) == NULL)
		{
			NxSceneDesc sceneDesc;
			// after simulating the cloth, we still need to transform the results, which can also be threaded.
			// so, we use our own threading wrapped around PhysX. No need to enable this, and upper comment suggests
			// it has issues.
			sceneDesc.flags &= ~NX_SF_SIMULATE_SEPARATE_THREAD;
			// Create the scene if it doesn't exist
			(*i) = gPhysicsSDK->createScene(sceneDesc);
			(*i)->setGravity (NxVec3 (0.0F, -9.81F, 0.0F));
			(*i)->setTiming (1.0F, 8, NX_TIMESTEP_VARIABLE);
#if UNITY_PS3
			NxCellConfig::setSceneParamInt((*i),NxCellConfig::NX_CELL_SCENE_PARAM_SPU_CLOTH, 0);
#endif
		}

		int numVertices = 0;
		int numCloths = (**i).getNbCloths();
		NxCloth **cloths = (**i).getCloths();
		for (int j=0; j<numCloths; j++)
			numVertices += cloths[j]->getNumberOfParticles();
		if (numVertices < minVertices)
		{
			found = i;
			minVertices = numVertices;
		}
		// No need to look further - this one is empty.
		if (numVertices == 0)
			return *found;
	}
	return *found;
#else
	return NULL;
#endif
}

void PhysicsManager::Update ()
{
	PROFILER_AUTO(gPhysicsInterpolationProfile, NULL)

	SetTransformMessageEnabled (false);

	// Also disable rigidbody transform changed message, otherwise interpolation will affect physics results.
	// This is not done in SetTransformMessageEnabled, as we need the rigidbody messages in the physics fixed update
	// so kinematic child rigidbodies are moved with their parents.
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(Rigidbody), kTransformChanged.messageID, false);

	// Interpolation time is [0...1] between the two steps
	// Extrapolation time the delta time since the last fixed step
	float dynamicTime = GetTimeManager().GetCurTime();
	float step = GetTimeManager().GetFixedDeltaTime();
	float fixedTime = GetTimeManager().GetFixedTime();
	float interpolationTime = clamp01 ((dynamicTime - fixedTime) / step);
	float extrapolationTime = dynamicTime - fixedTime;
//	AssertIf (t < 0.0F || t > 1.0F);

	// Update interpolated position
	for (InterpolatedBodiesIterator i=m_InterpolatedBodies.begin();i!=m_InterpolatedBodies.end();i++)
	{
		Rigidbody* body = i->body;
		if (i->disabled || body->IsSleeping())
			continue;

		Transform& transform = body->GetComponent(Transform);

		Quaternionf rot;
		Vector3f pos;
		RigidbodyInterpolation interpolation = body->GetInterpolation();
		// Interpolate between this physics and last physics frame
		if (interpolation == kInterpolate)
		{
			pos = Lerp(i->position, body->GetPosition(), interpolationTime);
			rot = Slerp(i->rotation, body->GetRotation(), interpolationTime);
			transform.SetPositionAndRotationSafe (pos, rot);
		}
		// Extrapolate current position using velocity
		else if (interpolation == kExtrapolate)
		{
			pos = body->GetPosition() + body->GetVelocity() * extrapolationTime;
			rot = AngularVelocityToQuaternion(body->GetAngularVelocity(), extrapolationTime) * body->GetRotation();
			transform.SetPositionAndRotationSafe (pos, rot);
		}
	}
	SetTransformMessageEnabled (true);
	if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		GameObject::GetMessageHandler ().SetMessageEnabled (ClassID(Rigidbody), kTransformChanged.messageID, true);
}

template<class TransferFunction>
void PhysicsManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER_SIMPLE (m_Gravity);
	TRANSFER_SIMPLE (m_DefaultMaterial);
	TRANSFER_PROPERTY (float, m_BounceThreshold, GetBounceThreshold, SetBounceThreshold);
	TRANSFER_PROPERTY (float, m_SleepVelocity, GetSleepVelocity, SetSleepVelocity);
	TRANSFER_PROPERTY (float, m_SleepAngularVelocity, GetSleepAngularVelocity, SetSleepAngularVelocity);
	TRANSFER_PROPERTY (float, m_MaxAngularVelocity, GetMaxAngularVelocity, SetMaxAngularVelocity);
	TRANSFER_PROPERTY (float, m_MinPenetrationForPenalty, GetMinPenetrationForPenalty, SetMinPenetrationForPenalty);
	TRANSFER_PROPERTY (int, m_SolverIterationCount, GetSolverIterationCount, SetSolverIterationCount);
	TRANSFER (m_RaycastsHitTriggers);
	transfer.Align();
	transfer.Transfer (m_LayerCollisionMatrix, "m_LayerCollisionMatrix", kHideInEditorMask);
}

#if ENABLE_CLUSTER_SYNC
template<class TransferFunction>
void PhysicsManager::ClusterTransfer(TransferFunction& transfer)
{
	TRANSFER(m_SmoothedClothDeltaTime);
}
#endif

void PhysicsManager::SetupDefaultMaterial ()
{
	m_CachedDefaultMaterial = m_DefaultMaterial;
	if (m_CachedDefaultMaterial)
		m_CachedDefaultMaterial->CopyMaterialToDefault ();
	else
	{
		NxMaterialDesc material;
		material.dynamicFriction = 0.6F;
		material.staticFriction = 0.6F;
		GetDynamicsScene ().getMaterialFromIndex (0)->loadFromDesc (material);
	}
}

void PhysicsManager::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	RETURNIFNOPHYSX_VOID();
	GetDynamicsScene ().setGravity((const NxVec3&)m_Gravity);
	SetupCollisionLayerMatrix ();

	/// FIXME: We need to check this since when we load from disk we don't have a physicsmanager setup yet.
	if (GetManagerPtrFromContext (ManagerContext::kPhysicsManager))
	{
		SetupDefaultMaterial ();
	}
}

NxActor* PhysicsManager::GetNULLActor ()
{
	if (gNULLActor)
		return gNULLActor;

	NxBodyDesc bodyDesc;
	NxActorDesc actorDesc;
	bodyDesc.massSpaceInertia = NxVec3 (1.0F, 1.0F, 1.0F);
	bodyDesc.mass = 1.0F;
	actorDesc.body = &bodyDesc;

	gNULLActor = GetInactiveDynamicsScene ().createActor (actorDesc);
	return gNULLActor;
}


Vector3f PhysicsManager::GetGravity ()
{
	return m_Gravity;
}

void PhysicsManager::WakeUpScene ()
{
	NxU32 nbActors = GetDynamicsScene().getNbActors();
	NxActor** actors = GetDynamicsScene().getActors();
	for (int i =0; i<nbActors; i++)
	{
		if (actors[i]->isDynamic())
			actors[i]->wakeUp();
	}
}

void PhysicsManager::SetGravity (const Vector3f& value)
{
	if (m_Gravity != value)
	{
		m_Gravity = value;
		GetDynamicsScene ().setGravity ((const NxVec3&)value);
		if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_2_a1))
			WakeUpScene ();
		SetDirty ();
	}
}

void PhysicsManager::SetMinPenetrationForPenalty (float value)
{
	RETURNIFNOPHYSX_VOID();
	GetDynamicsSDK ().setParameter (NX_SKIN_WIDTH, value);
	SetDirty ();
}

float PhysicsManager::GetMinPenetrationForPenalty ()
{
	RETURNIFNOPHYSX(1.0f);
	return GetDynamicsSDK ().getParameter (NX_SKIN_WIDTH);
}

void PhysicsManager::SetBounceThreshold (float value)
{
	RETURNIFNOPHYSX_VOID();
	GetDynamicsSDK ().setParameter (NX_BOUNCE_THRESHOLD, -value);
	SetDirty ();
}

float PhysicsManager::GetBounceThreshold ()
{
	return -GetDynamicsSDK ().getParameter (NX_BOUNCE_THRESHOLD);
}

void PhysicsManager::SetSleepVelocity (float value)
{
	RETURNIFNOPHYSX_VOID();
	GetDynamicsSDK ().setParameter (NX_DEFAULT_SLEEP_LIN_VEL_SQUARED, value * value);
	SetDirty ();
}

float PhysicsManager::GetSleepVelocity ()
{
	return Sqrt (GetDynamicsSDK ().getParameter (NX_DEFAULT_SLEEP_LIN_VEL_SQUARED));
}

void PhysicsManager::SetSleepAngularVelocity (float value)
{
	RETURNIFNOPHYSX_VOID();
	GetDynamicsSDK ().setParameter (NX_DEFAULT_SLEEP_ANG_VEL_SQUARED, value * value);
	SetDirty ();
}

float PhysicsManager::GetSleepAngularVelocity ()
{
	return Sqrt (GetDynamicsSDK ().getParameter (NX_DEFAULT_SLEEP_ANG_VEL_SQUARED));
}

void PhysicsManager::SetMaxAngularVelocity (float value)
{
	RETURNIFNOPHYSX_VOID();
	GetDynamicsSDK ().setParameter (NX_MAX_ANGULAR_VELOCITY, value);
	SetDirty ();
}

float PhysicsManager::GetMaxAngularVelocity ()
{
	return GetDynamicsSDK ().getParameter (NX_MAX_ANGULAR_VELOCITY);
}

void PhysicsManager::SetSolverIterationCount (int value)
{
	RETURNIFNOPHYSX_VOID();
	m_DefaultIterationCount = clamp(value, 1, 100);
	SetDirty ();
}

class CollideShapesReport : public NxUserEntityReport<NxShape*>
{
public:

	NxSphere sphere;
	bool checkShapes;
	PhysicsManager::ColliderCache& cache;
	CollideShapesReport (PhysicsManager::ColliderCache& c) : cache (c) {}

	virtual bool onEvent(NxU32 nbEntities, NxShape** entities)
	{
		int offset = cache.size ();
		cache.reserve (offset + nbEntities);
		for (int i=0;i<nbEntities;i++)
		{
			if (entities[i]->checkOverlapSphere (sphere))
				cache.push_back ((Collider*)entities[i]->userData);
		}
		return true;
	}
};

PhysicsManager::ColliderCache& PhysicsManager::OverlapSphere (const Vector3f& p, float radius, int mask)
{
	PROFILER_AUTO(gSphereOverlapProfile, NULL)
	m_ColliderCache.clear ();
	CollideShapesReport report (m_ColliderCache);
	report.sphere = NxSphere ((NxVec3&)p, radius);
	report.checkShapes = true;

	GetDynamicsScene ().overlapSphereShapes (report.sphere, NX_ALL_SHAPES, 0, NULL, &report, mask);
	return m_ColliderCache;
}

bool PhysicsManager::SphereTest (const Vector3f& p, float radius, int mask)
{
	PROFILER_AUTO(gSphereTestProfile, NULL)
	NxSphere sphere((NxVec3&)p, radius);
	return GetDynamicsScene ().checkOverlapSphere (sphere, NX_ALL_SHAPES, mask);
}


bool PhysicsManager::RaycastTest (const Ray& ray, float distance, int mask)
{
	PROFILER_AUTO(gRaycastProfile, NULL)

	AssertIf (!IsNormalized (ray.GetDirection ()));
	if (distance == std::numeric_limits<float>::infinity())
		distance = NX_MAX_F32;
	return GetDynamicsScene ().raycastAnyShape ((NxRay&)ray, NX_ALL_SHAPES, mask, distance);
}

bool PhysicsManager::Raycast (const Ray& ray, float distance, RaycastHit& outHit, int mask)
{
	AssertIf (!IsNormalized (ray.GetDirection ()));
	PROFILER_AUTO(gRaycastProfile, NULL)

	if (distance == std::numeric_limits<float>::infinity())
		distance = NX_MAX_F32;

	NxRaycastHit hit;
	NxShape* shape = GetDynamicsScene ().raycastClosestShape ((NxRay&)ray, NX_ALL_SHAPES, hit, mask, distance);
	if (shape)
	{
		NxToRaycastHit(hit, outHit);
		return true;
	}
	else
		return false;
}

bool PhysicsManager::CapsuleCast (const Vector3f &p0, const Vector3f &p1, float radius, const Vector3f &direction, float distance, RaycastHit& outHit, int mask)
{
	AssertIf (!IsNormalized (direction));
	PROFILER_AUTO(gCapsuleCastProfile, NULL)

	if (distance == std::numeric_limits<float>::infinity())
		// CapsuleCasts fail when using NX_MAX_F32 here.
		// So pick a lower "high" number instead.
		distance = 1000000.0f;

	NxCapsule testCapsule;
	testCapsule.radius = radius;
	testCapsule.p0 = (const NxVec3&)p0;
	testCapsule.p1 = (const NxVec3&)p1;

	NxSweepQueryHit hit;
	NxU32 nb = GetDynamicsScene ().linearCapsuleSweep (testCapsule, (const NxVec3&)direction * distance, NX_SF_DYNAMICS|NX_SF_STATICS, NULL, 1, &hit, NULL, mask);
	if (nb)
	{
		NxToRaycastHit(hit, distance, outHit);
		return true;
	}
	else
		return false;
}

void NxToRaycastHit (const NxRaycastHit& hit, RaycastHit& outHit)
{
		outHit.collider = (Collider*)hit.shape->userData;
		outHit.point = (const Vector3f&)hit.worldImpact;
		outHit.normal = (const Vector3f&)hit.worldNormal;
		outHit.faceID = hit.faceID;
		outHit.distance = hit.distance;
		outHit.uv.x = hit.u;
		outHit.uv.y = hit.v;
}

void NxToRaycastHit (const NxSweepQueryHit& hit, float sweepDistance, RaycastHit& outHit)
{
		outHit.collider = (Collider*)hit.hitShape->userData;
		outHit.point = (const Vector3f&)hit.point;
		outHit.normal = (const Vector3f&)hit.normal;
		outHit.faceID = hit.faceID;
		outHit.distance = hit.t * sweepDistance;
		outHit.uv.x = 0;
		outHit.uv.y = 0;
}

bool PhysicsManager::CapsuleTest (Vector3f start, Vector3f end, float radius, int mask)
{
	PROFILER_AUTO(gCapsuleTestProfile, NULL)
	NxCapsule capsule;
	capsule.p0 = Vec3ToNx(start);
	capsule.p1 = Vec3ToNx(end);
	capsule.radius = radius;

	return GetDynamicsScene ().checkOverlapCapsule (capsule, NX_ALL_SHAPES, mask);
}


const PhysicsManager::RaycastHits& PhysicsManager::RaycastAll (const Ray& ray, float distance, int mask)
{
	AssertIf (!IsNormalized (ray.GetDirection ()));
	PROFILER_AUTO(gRaycastAllProfile, NULL)

	if (distance == std::numeric_limits<float>::infinity())
		distance = NX_MAX_F32;

	static vector<RaycastHit> hits;
	hits.resize(0);
	RaycastCollector collector;
	collector.hits = &hits;

	GetDynamicsScene ().raycastAllShapes ((NxRay&)ray, collector, NX_ALL_SHAPES, mask, distance);

	return hits;
}

#define kCapsuleCastMaxHits 128
const PhysicsManager::RaycastHits& PhysicsManager::CapsuleCastAll (const Vector3f &p0, const Vector3f &p1, float radius, const Vector3f &direction, float distance, int mask)
{
	AssertIf (!IsNormalized (direction));
	PROFILER_AUTO(gCapsuleCastAllProfile, NULL)

	if (distance == std::numeric_limits<float>::infinity())
		// CapsuleCasts fail when using NX_MAX_F32 here.
		// So pick a lower "high" number instead.
		distance = 1000000.0f;

	static vector<RaycastHit> outHits;

	NxCapsule testCapsule;
	testCapsule.radius = radius;
	testCapsule.p0 = (const NxVec3&)p0;
	testCapsule.p1 = (const NxVec3&)p1;

	NxSweepQueryHit hits[kCapsuleCastMaxHits];
	NxU32 nb = GetDynamicsScene ().linearCapsuleSweep (testCapsule, (const NxVec3&)direction * distance, NX_SF_DYNAMICS|NX_SF_STATICS|NX_SF_ALL_HITS, NULL, kCapsuleCastMaxHits, hits, NULL, mask);

	outHits.resize(nb);
	for (int i=0; i<nb; i++)
		NxToRaycastHit(hits[i], distance, outHits[i]);

	return outHits;
}

void PhysicsManager::IgnoreCollision (Collider& lhs, Collider& rhs, bool ignore)
{
	if (lhs.m_Shape == NULL || rhs.m_Shape == NULL)
	{
		ErrorString ("Ignore collision failed. Both colliders need to be activated when calling this IgnoreCollision");
		return;
	}

	if (ignore)
		GetDynamicsScene().setShapePairFlags(*lhs.m_Shape, *rhs.m_Shape, NX_IGNORE_PAIR);
	else
		GetDynamicsScene().setShapePairFlags(*lhs.m_Shape, *rhs.m_Shape, 0);
}

void PhysicsManager::IgnoreCollision(int layer1, int layer2, bool ignore)
{
	if (layer1 >= kNumLayers || layer2 >= kNumLayers)
	{
		ErrorString(Format("layer numbers must be between 0 and %d", kNumLayers));
		return;
	}

	Assert (kNumLayers <= m_LayerCollisionMatrix.size());
	Assert (kNumLayers <= sizeof(m_LayerCollisionMatrix[0])*8);

	GetDynamicsScene ().setGroupCollisionFlag(layer1, layer2, !ignore);
	if (ignore)
	{
		m_LayerCollisionMatrix[layer1] &= ~(1<<layer2);
		m_LayerCollisionMatrix[layer2] &= ~(1<<layer1);
	}
	else
	{
		m_LayerCollisionMatrix[layer1] |= 1<<layer2;
		m_LayerCollisionMatrix[layer2] |= 1<<layer1;
	}
	SetDirty();
}

bool PhysicsManager::GetIgnoreCollision(int layer1, int layer2)
{
	if (layer1 >= kNumLayers || layer2 >= kNumLayers)
	{
		ErrorString(Format("layer numbers must be between 0 and %d", kNumLayers));
		return false;
	}

	return !GetDynamicsScene ().getGroupCollisionFlag(layer1, layer2);
}

void PhysicsManager::SetupCollisionLayerMatrix()
{
	for (int i=0;i<kNumLayers;i++)
	{
		for (int j=0;j<kNumLayers;j++)
		{
			if (i <= j)
			{
				bool enabled = m_LayerCollisionMatrix[i] & (1<<j);
				GetDynamicsScene().setGroupCollisionFlag(i, j, enabled);
			}
		}
	}
}


void PhysicsManager::AddBody(int depth, ListNode<Rigidbody>& node)
{
	depth = min(kMaxSortedActorsDepth-1, depth);
	if (depth >= kMaxSortedActorsDepth-1)
	{
		ErrorString("Too deep hierarchy to perform rigidbody ordering. Nested rigidbodies might look strange");
		depth = kMaxSortedActorsDepth-1;
	}
	m_SortedActors[depth].push_back(node);
}

NxScene& GetDynamicsScene ()
{
	AssertIf (gPhysicsScene == NULL);
	return *gPhysicsScene;
}

NxScene& GetInactiveDynamicsScene ()
{
	AssertIf (gInactivePhysicsScene == NULL);
	return *gInactivePhysicsScene;
}


NxPhysicsSDK& GetDynamicsSDK ()
{
	AssertIf (gPhysicsSDK == NULL);
	return *gPhysicsSDK;
}

void PhysicsManager::ProcessRecordedReports()
{
	SetDisableImmediateDestruction(true);
	
	/// Report recorded triggers.
	/// We can't call the recorded triggers immediately because novodex still keeps a write lock at that time.
	/// So we can't do much with physics at that time.
	for (int i=0;i<m_RecordedTriggers.size ();i++)
	{
		RecordedTrigger& trigger = m_RecordedTriggers[i];
		
		VerifyObjectPtr(trigger.collider);
		VerifyObjectPtr(trigger.trigger);
		
		Unity::Component* masterOfCollider = AttachedRigidbodyOrCollider (*trigger.collider);
		Unity::Component* masterOfTrigger = AttachedRigidbodyOrCollider (*trigger.trigger);
		MessageIdentifier messageID;
		if (trigger.status == NX_TRIGGER_ON_ENTER)
			messageID = kEnterTrigger;
		else if (trigger.status == NX_TRIGGER_ON_LEAVE)
			messageID = kExitTrigger;
		else
			messageID = kStayTrigger;
		
		trigger.trigger->SendMessage (messageID, trigger.collider, ClassID (Collider));
		masterOfCollider->SendMessage (messageID, trigger.trigger, ClassID (Collider));
		if (IS_CONTENT_NEWER_OR_SAME(kUnityVersion4_0_a1))
		{
			if (masterOfCollider->GetGameObjectPtr() != trigger.collider->GetGameObjectPtr() && trigger.collider->GetIsTrigger())
				trigger.collider->SendMessage (messageID, trigger.trigger, ClassID (Collider));
			if (masterOfTrigger->GetGameObjectPtr() != trigger.trigger->GetGameObjectPtr())
				masterOfTrigger->SendMessage (messageID, trigger.collider, ClassID (Collider));
		}
	}
	m_RecordedTriggers.clear ();
	
	/// Report recorded contact events.
	for (int i=0;i<m_RecordedContacts.size ();i++)
	{
		Collision& contact = m_RecordedContacts[i];
		VerifyObjectPtr(contact.thisRigidbody);
		VerifyObjectPtr(contact.otherRigidbody);
		VerifyObjectPtr(contact.thisCollider);
		VerifyObjectPtr(contact.otherCollider);
		
		if (contact.status & NX_NOTIFY_ON_START_TOUCH)
		{
			contact.flipped = false;
			GetRigidbodyOrCollider(contact)->SendMessage (kEnterContact, &contact, ClassID (Collision));
			contact.flipped = true;
			GetOtherRigidbodyOrCollider(contact)->SendMessage (kEnterContact, &contact, ClassID (Collision));
		}
		if (contact.status & NX_NOTIFY_ON_END_TOUCH)
		{
			contact.flipped = false;
			GetRigidbodyOrCollider(contact)->SendMessage (kExitContact, &contact, ClassID (Collision));
			contact.flipped = true;
			GetOtherRigidbodyOrCollider(contact)->SendMessage (kExitContact, &contact, ClassID (Collision));
		}
		if (contact.status & NX_NOTIFY_ON_TOUCH)
		{
			contact.flipped = false;
			GetRigidbodyOrCollider(contact)->SendMessage (kStayContact, &contact, ClassID (Collision));
			contact.flipped = true;
			GetOtherRigidbodyOrCollider(contact)->SendMessage (kStayContact, &contact, ClassID (Collision));
		}
	}
	m_RecordedContacts.clear ();
	
	/// Report recorded joint breaks
	for (int i=0;i<m_RecordedJointBreaks.size ();i++)
	{
		RecordedJointBreak& record = m_RecordedJointBreaks[i];
		Joint* joint = record.joint;
		if (joint && joint->IsActive())
		{
			joint->GetGameObject().SendMessage (kJointBreak, record.impulse, ClassID (float));
		}
		joint = record.joint;
		if (joint && joint->GetGameObjectPtr())
		{
			SetDisableImmediateDestruction(false);
			DestroyObjectHighLevel(joint, true);
			SetDisableImmediateDestruction(true);
		}
	}
	m_RecordedJointBreaks.clear();
	
	SetDisableImmediateDestruction(false);
}

// When using raycasts or other physics functionality in edit mode (such as is done when generating terrain lightmaps),
// this needs to be called to make sure all updates to colliders in the scene are taken into account.
void PhysicsManager::RefreshWhenPaused()
{
	gPhysicsScene->simulate(0);
	gPhysicsScene->flushCaches();
   	gPhysicsScene->flushStream();
	gPhysicsScene->fetchResults(NX_RIGID_BODY_FINISHED, true);
	ProcessRecordedReports();
}

#if ENABLE_SCRIPTING
ScriptingArrayPtr ConvertNativeRaycastHitsToManaged(const PhysicsManager::RaycastHits& hits)
{
	ScriptingArrayPtr arr = CreateScriptingArray(hits.size() > 0 ? &hits[0] : NULL, hits.size(), GetScriptingManager().GetCommonClasses().raycastHit);
	RaycastHit* firstElement = Scripting::GetScriptingArrayStart<RaycastHit>(arr);
	for (int i=0;i<hits.size();i++)
	{
		firstElement[i].collider = reinterpret_cast<Collider*>(ScriptingGetObjectReference (firstElement[i].collider));
	}
	return arr;
}
#endif

GET_MANAGER (PhysicsManager)
GET_MANAGER_PTR (PhysicsManager)
IMPLEMENT_CLASS_HAS_INIT (PhysicsManager)
IMPLEMENT_OBJECT_SERIALIZE (PhysicsManager)
IMPLEMENT_CLUSTER_SERIALIZE (PhysicsManager)
#endif //ENABLE_PHYSICS
