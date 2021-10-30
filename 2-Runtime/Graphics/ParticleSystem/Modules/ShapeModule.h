#ifndef SHURIKENMODULESHAPE_H
#define SHURIKENMODULESHAPE_H

#include "Runtime/BaseClasses/BaseObject.h"
#include "ParticleSystemModule.h"
#include "Runtime/Math/Random/rand.h"
#include "Runtime/Utilities/LinkedList.h"

struct MeshTriangleData
{
	float area;
	UInt16 indices[3];
};

struct ParticleSystemEmitterMeshVertex
{
	Vector3f position;
	Vector3f normal;
	ColorRGBA32 color;
};

class Mesh;

class ShapeModule : public ParticleSystemModule
{
public:
	DECLARE_MODULE (ShapeModule)
	ShapeModule ();

	enum MeshPlacementMode { kVertex, kEdge, kTriangle, kModeMax };
	enum { kSphere, kSphereShell, kHemiSphere, kHemiSphereShell, kCone, kBox, kMesh, kConeShell, kConeVolume, kConeVolumeShell, kMax };

	void AwakeFromLoad (ParticleSystem* system, const ParticleSystemReadOnlyState& roState);
	void ResetSeed(const ParticleSystemReadOnlyState& roState);
	void DidModifyMeshData ();
	void DidDeleteMesh (ParticleSystem* system);
	
	PPtr<Mesh> GetMeshEmitterShape () { return m_Mesh; }
	
	void Start (const ParticleSystemReadOnlyState& roState, const ParticleSystemState& state, ParticleSystemParticles& ps, const Matrix4x4f& matrix, size_t fromIndex, float t);
	void CalculateProceduralBounds(MinMaxAABB& bounds, const Vector3f& emitterScale, Vector2f minMaxBounds) const;
	void CheckConsistency ();

	inline void SetShapeType(int type) { m_Type = type; };
	inline void SetRadius(float radius) { m_Radius = radius; };

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
	
private:
	Rand& GetRandom();
	
	int m_Type;

	// Primitive stuff
	float m_Radius;
	float m_Angle;
	float m_Length;
	float m_BoxX;
	float m_BoxY;
	float m_BoxZ;
	
	// Mesh stuff
	int m_PlacementMode;
	PPtr<Mesh>	m_Mesh;
	Mesh*	m_CachedMesh;
	dynamic_array<ParticleSystemEmitterMeshVertex> m_CachedVertexData;
	dynamic_array<MeshTriangleData> m_CachedTriangleData;
	float m_CachedTotalTriangleArea;	
	ListNode<Object> m_MeshNode;
	
	bool m_RandomDirection;
	Rand m_Random; 
#if UNITY_EDITOR
public:
	Rand m_EditorRandom;
#endif
};

#endif // SHURIKENMODULESHAPE_H
