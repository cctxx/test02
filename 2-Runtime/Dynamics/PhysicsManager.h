#ifndef PHYSICSMANAGER_H
#define PHYSICSMANAGER_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Geometry/Ray.h"
#include "Runtime/Misc/MessageParameters.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Runtime/ClusterRenderer/ClusterRendererDefines.h"

class Collider;
class CharacterController;
class SkinnedMeshRenderer;
class PhysicMaterial;
namespace Unity { class Component; }
namespace Unity { class Joint; }
class NxActor;
class NxScene;
class NxShape;
class NxUserControllerHitReport;
struct NxRaycastHit;
struct NxSweepQueryHit;
class NxMaterialDesc;
struct PhysicsStats;
struct RaycastHit
{
	Vector3f point;
	Vector3f normal;
	UInt32 faceID;
	float distance;
	Vector2f  uv;
	Collider* collider;
};

struct RigidbodyInterpolationInfo : public ListElement
{
	Vector3f    position;
	Quaternionf rotation;
	Rigidbody*  body;
	int         disabled;
};

class PhysicsManager : public GlobalGameManager {
 public:

	PhysicsManager (MemLabelId label, ObjectCreationMode mode);
	// ~PhysicsManager (); declared-by-macro
	
	REGISTER_DERIVED_CLASS (PhysicsManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (PhysicsManager)
	DECLARE_CLUSTER_SERIALIZE (PhysicsManager)

	void FixedUpdate ();
	void Update ();
	void SkinnedClothUpdate ();
	void ResetInterpolatedTransformPosition ();
	void RefreshWhenPaused ();
	void RecreateScene ();

	void GetPerformanceStats(PhysicsStats& physicsStats);
	
	void UpdateSkinnedClothes();
	void FinishUpdatingSkinnedClothes ();

	static void InitializeClass ();
	static void CleanupClass ();
	
	Vector3f GetGravity ();
	void SetGravity (const Vector3f& value);

	void SetMinPenetrationForPenalty (float value);
	float GetMinPenetrationForPenalty ();

	void SetBounceThreshold (float value);
	float GetBounceThreshold ();
	
	void SetSleepVelocity (float value);
	float GetSleepVelocity ();

	void SetSleepAngularVelocity (float value);
	float GetSleepAngularVelocity ();

	void SetMaxAngularVelocity (float value);
	float GetMaxAngularVelocity ();

	void SetSolverIterationCount (int value);
	int GetSolverIterationCount () { return m_DefaultIterationCount; }

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	// Overlap sphere
	typedef std::vector<Collider*> ColliderCache;
	ColliderCache& OverlapSphere (const Vector3f& p, float radius, int mask);
	bool SphereTest (const Vector3f& p, float radius, int mask);

	bool CapsuleTest (Vector3f start, Vector3f end, float radius, int mask);
	
	typedef std::vector<RaycastHit> RaycastHits;

	bool RaycastTest (const Ray& ray, float distance, int mask);
	bool Raycast (const Ray& ray, float distance, RaycastHit& hit, int mask);
	bool CapsuleCast (const Vector3f &p0, const Vector3f &p1, float radius, const Vector3f &direction, float distance, RaycastHit& outHit, int mask);
	const RaycastHits& RaycastAll (const Ray& ray, float distance, int mask);
	const RaycastHits& CapsuleCastAll (const Vector3f &p0, const Vector3f &p1, float radius, const Vector3f &direction, float distance, int mask);

	NxActor* GetNULLActor ();
	
	bool GetRaycastsHitTriggers () { return m_RaycastsHitTriggers; }

	void IgnoreCollision (Collider& a, Collider& b, bool ignore);	
	void IgnoreCollision (int layer1, int layer2, bool ignore);
	bool GetIgnoreCollision(int layer1, int layer2);
	UInt32 GetLayerCollisionMask(int layer) {return m_LayerCollisionMatrix[layer];}

	bool IsRigidbodyTransformMessageEnabled() {return m_RigidbodyTransformMessageEnabled;}
	public:

	void AddBody(int depth, ListNode<Rigidbody>& node);
	NxScene* GetClothingScene ();
	
	// Only for internal use
	void SetTransformMessageEnabled(bool enable);

	List<RigidbodyInterpolationInfo>& GetInterpolatedBodies() { return m_InterpolatedBodies; }

	struct RecordedTrigger
	{
		int status;
		Collider* trigger;
		Collider* collider;
	};

	struct RecordedJointBreak
	{
		float impulse;
		PPtr<Unity::Joint> joint;
	};

	typedef std::vector<RecordedTrigger> RecordedTriggers;
	typedef std::vector<Collision> RecordedContacts;
	typedef std::vector<RecordedJointBreak> RecordedJointBreaks;

private:
	static void CleanupReports();
	void CreateReports();
	void ProcessRecordedReports();
	void SetupCollisionLayerMatrix();
	void WakeUpScene ();
	static void *SimulateClothingScene (void *voidIndex);

	static void ReleaseMaterials(std::vector<PhysicMaterial*>& materials, std::vector<NxMaterialDesc>& materialDescs);


	Vector3f m_Gravity;///< The gravity applied to all rigid bodies in the scene
	virtual void Reset ();

	#if DOXYGEN

	/// The minimum contact penetration value in order to apply a penalty force. (Default 0.01) range { 0 , infinity }
	float m_MinPenetrationForPenalty;
	/// The penalty force applied to the bodies in an interpenetrating contact is scaled by this value. (Default 0.6) range { 0, 2 }
	float m_PenetrationPenaltyForce;
	/// A contact with a relative velocity below this will not bounce. (Default 2) range { 0, infinity }
	float m_BounceThreshold;
	/// The default linear velocity, below which objects start going to sleep. This value can be overridden per rigidbodies with scripting. (Default 0.15) range { 0, infinity }
	float m_SleepVelocity;
	/// The default linear velocity, below which objects start going to sleep. This value can be overridden per rigidbodies with scripting. (Default 0.14) range { 0, infinity }
	float m_SleepAngularVelocity;
	/// The maximum angular velocity permitted for any rigid bodies. This can be overridden per rigidbodies with scripting. (Default 7) range { 0, infinity }
	float m_MaxAngularVelocity;
	
	#endif
	
	bool										m_RaycastsHitTriggers;
	
	bool										m_RigidbodyTransformMessageEnabled;
	void SetupDefaultMaterial ();

	
	PPtr<PhysicMaterial>                          m_DefaultMaterial;
	PhysicMaterial*                               m_CachedDefaultMaterial;
	
	ColliderCache                                 m_ColliderCache;
		
//	typedef std::set<PPtr<PhysicMaterial> >       Materials;
//	Materials                                     m_Materials;
	
	RecordedTriggers                              m_RecordedTriggers;
	RecordedContacts                              m_RecordedContacts;
	RecordedJointBreaks 							  m_RecordedJointBreaks;
	/// Solver accuracy. Higher value costs more performance. (Default 4) range { 1, 30 }
	int                                           m_DefaultIterationCount;
	
	std::vector<SInt32>                           m_DisableTransformMessage;
	typedef List<RigidbodyInterpolationInfo> InterpolatedBodiesList;
	typedef InterpolatedBodiesList::iterator      InterpolatedBodiesIterator;
	InterpolatedBodiesList                        m_InterpolatedBodies;
	float											m_SmoothedClothDeltaTime;
	
	enum { kMaxSortedActorsDepth = 64 };
	typedef List< ListNode<Rigidbody> > RigidbodyList;
	RigidbodyList									m_SortedActors[kMaxSortedActorsDepth];
#if ENABLE_MULTITHREADED_SKINNING
	JobScheduler::JobGroupID						m_ClothJobGroup;
#endif
	dynamic_array<SkinnedMeshRenderer*>				m_ActiveSkinnedMeshes;

	std::vector<UInt32>								m_LayerCollisionMatrix;
	friend class PhysicMaterial;

#if ENABLE_CLUSTER_SYNC
#ifdef DEBUG
	friend class ClusterTransfer;
#endif // DEBUG
#endif // ENABLE_CLUSTER_SYNC
};

enum { kContactNothingGroup = 0, kContactEnterExitGroup = 1, kContactTouchGroup = 2 };

PhysicsManager &GetPhysicsManager ();
PhysicsManager *GetPhysicsManagerPtr ();
class NxScene& GetDynamicsScene ();
class NxScene& GetInactiveDynamicsScene ();
class NxPhysicsSDK& GetDynamicsSDK ();
void NxToRaycastHit (const NxRaycastHit& hit, RaycastHit& outHit);
void NxToRaycastHit (const NxSweepQueryHit& hit, float sweepDistance, RaycastHit& outHit);

#if ENABLE_SCRIPTING
ScriptingArrayPtr ConvertNativeRaycastHitsToManaged(const PhysicsManager::RaycastHits& hits);
#endif

#endif
