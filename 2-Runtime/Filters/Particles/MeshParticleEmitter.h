#ifndef MESHPARTICLEEMITTER_H
#define MESHPARTICLEEMITTER_H

#include "ParticleEmitter.h"
#include "ParticleStruct.h"
#include "Runtime/Filters/Mesh/LodMesh.h"



class MeshParticleEmitter : public ParticleEmitter
{
public:
	REGISTER_DERIVED_CLASS (MeshParticleEmitter, ParticleEmitter)
	DECLARE_OBJECT_SERIALIZE (MeshParticleEmitter)
	MeshParticleEmitter (MemLabelId label, ObjectCreationMode mode);
	
	void SetMesh (PPtr<Mesh> mesh);
	PPtr<Mesh> GetMesh () { return m_Mesh; } 
	
	virtual void Reset ();
	
	static void InitializeClass ();
	static void CleanupClass ();
	
private:
	void SetupParticle (int vertexIndex, Particle& p, const Vector3f& velocityOffset, const Matrix4x4f& scale, const Matrix3x3f& rotation, const Matrix3x3f& normalTransform, float deltaTime, StrideIterator<Vector3f> vertices, StrideIterator<Vector3f> normals);
	void SetupParticleTri (Particle& p, const Vector3f& velocityOffset, const Matrix4x4f& scale, const Matrix3x3f& rotation, const Matrix3x3f& normalTransform, float deltaTime, StrideIterator<Vector3f> vertices, StrideIterator<Vector3f> normals, const UInt16* faces, int triCount);
	void SetupParticleStrip (Particle& p, const Vector3f& velocityOffset, const Matrix4x4f& scale, const Matrix3x3f& rotation, const Matrix3x3f& normalTransform, float deltaTime, StrideIterator<Vector3f> vertices, StrideIterator<Vector3f> normals, const UInt16* strip, int stripSize);

	virtual void SetupParticles (ParticleArray& particles, const Vector3f& velocityOffset,
			const Matrix3x3f& rotation,	int firstIndex);
	
private:
	bool				m_InterpolateTriangles;
	bool				m_Systematic;
	float				m_MinNormalVelocity;
	float				m_MaxNormalVelocity;
	int 				m_VertexIndex;
	PPtr<Mesh>		m_Mesh;
};

#endif
