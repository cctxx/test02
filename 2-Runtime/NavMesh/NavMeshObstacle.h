#ifndef RUNTIME_NAVMESHOBSTACLE
#define RUNTIME_NAVMESHOBSTACLE

#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Vector3.h"
#include "NavMeshManager.h"
#include "DetourFeatures.h"
#include "DetourReference.h"

struct NavMeshCarveData;
class dtCrowd;
class dtNavMeshQuery;
class Matrix4x4f;

class NavMeshObstacle : public Behaviour
{
public:
	REGISTER_DERIVED_CLASS (NavMeshObstacle, Behaviour)
	DECLARE_OBJECT_SERIALIZE (NavMeshObstacle)

	NavMeshObstacle (MemLabelId& label, ObjectCreationMode mode);
	// ~NavMeshObstacle (); declared by a macro

	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	static void InitializeClass ();
	static void CleanupClass () { }

	inline bool InCrowdSystem () const;
	inline void SetManagerHandle (int handle);

	void OnNavMeshChanged ();
	void OnNavMeshCleanup ();

#if ENABLE_NAVMESH_CARVING
	inline void SetCarveHandle (int handle);

	void WillRebuildNavmesh (NavMeshCarveData& carveData);
	bool NeedsRebuild () const;

	inline bool GetCarving () const;
	void SetCarving (bool carve);

	inline float GetMoveThreshold () const;
	void SetMoveThreshold (float moveThreshold);
#endif

	Vector3f GetScaledDimensions () const;
	inline Vector3f GetPosition () const;

	inline Vector3f GetVelocity () const;
	void SetVelocity (const Vector3f& value);

	inline float GetRadius () const;
	void SetRadius (float value);

	inline float GetHeight () const;
	void SetHeight (float value);


protected:
	virtual void Reset ();
	virtual void SmartReset ();
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void CheckConsistency ();
	virtual UInt32 CalculateSupportedMessages ();

	void OnVelocityChanged (Vector3f* value);
	void OnTransformChanged (int mask);

private:
	void AddToCrowdSystem ();
	void RemoveFromCrowdSystem ();
	void CalculateTransformAndSize (Matrix4x4f& trans, Vector3f& size);

	static inline float EnsurePositive (float value);
	static dtCrowd* GetCrowdSystem ();

	int m_ManagerHandle;
	dtCrowdHandle m_ObstacleHandle;
	float m_Radius;
	float m_Height;
	Vector3f m_Velocity;

#if ENABLE_NAVMESH_CARVING
	void AddOrRemoveObstacle ();

	enum
	{
		kClean = 0,
		kHasMoved = 1 << 0,
		kForceRebuild = 1 << 1
	};
	float    m_MoveThreshold;
	Vector3f m_LastCarvedPosition;
	UInt32   m_Status;
	int      m_CarveHandle;
	bool     m_Carve;
#endif
};

inline bool NavMeshObstacle::InCrowdSystem () const
{
	return m_ObstacleHandle.IsValid ();
}

inline void NavMeshObstacle::SetManagerHandle (int handle)
{
	m_ManagerHandle = handle;
}

inline float NavMeshObstacle::EnsurePositive (float value)
{
	return std::max (0.00001F, value);
}

inline Vector3f NavMeshObstacle::GetPosition () const
{
	return GetComponent (Transform).GetPosition ();
}

inline Vector3f NavMeshObstacle::GetVelocity () const
{
	return m_Velocity;
}

inline float NavMeshObstacle::GetRadius () const
{
	return m_Radius;
}

inline float NavMeshObstacle::GetHeight () const
{
	return m_Height;
}

#if ENABLE_NAVMESH_CARVING
inline void NavMeshObstacle::SetCarveHandle (int handle)
{
	m_CarveHandle = handle;
}

inline bool NavMeshObstacle::GetCarving () const
{
	return m_Carve;
}

inline float NavMeshObstacle::GetMoveThreshold () const
{
	return m_MoveThreshold;
}
#endif // ENABLE_NAVMESH_CARVING

#endif
