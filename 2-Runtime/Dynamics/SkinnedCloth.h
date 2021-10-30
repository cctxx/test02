#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_CLOTH

#include "DeformableMesh.h"
#include "Runtime/Math/Matrix3x3.h"

namespace Unity
{


class SkinnedCloth : public Cloth
{
public:	
	REGISTER_DERIVED_CLASS (SkinnedCloth, Cloth)
	DECLARE_OBJECT_SERIALIZE (SkinnedCloth)

	SkinnedCloth (MemLabelId label, ObjectCreationMode mode);
		
	void SetUpSkinnedBuffers (void *vertices, void *normals, void *tangents, size_t bufferStride);
	void ReadBackSkinnedBuffers ();

	virtual void Reset ();
	virtual void SetEnabled (bool enabled);
	virtual void LateUpdate ();
	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	
	struct ClothConstrainCoefficients {
		float maxDistance;
		float maxDistanceBias;
		float collisionSphereRadius;
		float collisionSphereDistance;
		DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (ClothConstrainCoefficients)

		ClothConstrainCoefficients () : maxDistance(0.05f), maxDistanceBias(0), collisionSphereRadius(0.5f), collisionSphereDistance(0) {}
	};

	std::vector<ClothConstrainCoefficients> &GetCoefficients() {return m_Coefficients;}
	void SetCoefficients(ClothConstrainCoefficients *coefficients);

	float GetWorldVelocityScale () const { return m_WorldVelocityScale; }
	void SetWorldVelocityScale (float value);

	float GetWorldAccelerationScale () const { return m_WorldAccelerationScale; }
	void SetWorldAccelerationScale (float value);

	void SetEnabledFading (bool enabled, float interpolationTime);

protected:
	
	virtual void Create ();
	virtual void Cleanup ();

	void SetupCoefficients();
	
	float m_Fade;
	float m_TargetFade;
	float m_InterpolationTime;

	bool m_NeedsToReadVertices;
	void *m_VertexBuffer;
	void *m_NormalBuffer;
	void *m_TangentBuffer;
	Matrix3x3f m_WorldToLocalRotationMatrix;
	size_t m_VertexBufferStride;
	
	Vector3f m_LastFrameWorldPosition;
	Vector3f m_LastFrameVelocity;
	
	float m_WorldVelocityScale; ///<How much world-space movement of the character will affect cloth vertices.
	float m_WorldAccelerationScale; ///<How much world-space acceleration of the character will affect cloth vertices.
	
	BehaviourListNode m_UpdateNode;

	std::vector<ClothConstrainCoefficients> m_Coefficients;
};

}

#endif // ENABLE_CLOTH
