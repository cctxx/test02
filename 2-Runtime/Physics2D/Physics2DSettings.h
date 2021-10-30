#pragma once

#if ENABLE_2D_PHYSICS || DOXYGEN

#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Physics2D/Physics2DMaterial.h"
#include "Runtime/Utilities/dynamic_array.h"


// --------------------------------------------------------------------------


class Physics2DSettings : public GlobalGameManager
{
public:
	Physics2DSettings (MemLabelId label, ObjectCreationMode mode);
	// ~Physics2DSettings (); declared-by-macro
	
	REGISTER_DERIVED_CLASS (Physics2DSettings, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (Physics2DSettings)

	static void InitializeClass ();
	static void CleanupClass ();

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency ();
	virtual void Reset ();

	const Vector2f& GetGravity () const { return m_Gravity; }
	void SetGravity (const Vector2f& value);

	int GetVelocityIterations () const { return m_VelocityIterations; }
	void SetVelocityIterations (const int velocityIterations);

	int GetPositionIterations () const { return m_PositionIterations; }
	void SetPositionIterations (const int positionIterations);

	inline bool GetRaycastsHitTriggers () const { return m_RaycastsHitTriggers; }
	inline void SetRaycastsHitTriggers (const bool raycastsHitTriggers) { m_RaycastsHitTriggers = raycastsHitTriggers; }

	void IgnoreCollision (int layer1, int layer2, bool ignore);
	bool GetIgnoreCollision(int layer1, int layer2) const;
	UInt32 GetLayerCollisionMask(int layer) const { return m_LayerCollisionMatrix[layer]; }

	PhysicsMaterial2D* GetDefaultPhysicsMaterial () { return m_DefaultMaterial; }

private:
	Vector2f m_Gravity;			///< The gravity applied to all rigid bodies in the scene.
	PPtr<PhysicsMaterial2D> m_DefaultMaterial; ///< The default material to use on a collider if no material is specified on it.
	int m_VelocityIterations; 	///< The number of iterations used to solve simulation velocities.  More iterations yield a better simulation but is more expensive. (Default 8) range { 1 , infinity }
	int m_PositionIterations;	///< The number of iterations used to solve simulation positions.  More iterations yield a better simulation but is more expensive. (Default 3) range { 1 , infinity }
	bool m_RaycastsHitTriggers;	///< Whether ray/line casts hit triggers or not.

	dynamic_array<UInt32>	m_LayerCollisionMatrix;

};

Physics2DSettings& GetPhysics2DSettings ();
Physics2DSettings* GetPhysics2DSettingsPtr ();

#endif
