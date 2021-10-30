#include "UnityPrefix.h"
#include "MeshParticleEmitter.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/Mesh/MeshUtility.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

using namespace std;

static Rand gMeshEmitterRand (4);

MeshParticleEmitter::MeshParticleEmitter (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_VertexIndex = false;
}

MeshParticleEmitter::~MeshParticleEmitter ()
{
}

void MeshParticleEmitter::Reset ()
{
	Super::Reset();
	m_InterpolateTriangles = false;
	m_Systematic = false;
	m_MinNormalVelocity = 0.0F;
	m_MaxNormalVelocity = 0.0F;
}

void MeshParticleEmitter::SetupParticles (
	ParticleArray& particles,
	const Vector3f& velocityOffset,
	const Matrix3x3f& rotation,
	int firstIndex)
{
	Mesh* mesh = m_Mesh;
	
	MinMaxAABB& aabb = m_PrivateInfo.aabb;

	Matrix4x4f scale;
	GetComponent (Transform).CalculateTransformMatrixScaleDelta (scale);

	Matrix3x3f rotationAndScale = Matrix3x3f(scale);
	rotationAndScale = rotation * rotationAndScale;
	rotationAndScale.InvertTranspose ();

	float deltaTime = GetDeltaTime ();

	// If there's an invalid mesh, then just emit from the center
	if (mesh == NULL || mesh->GetSubMeshCount () == 0 || mesh->GetSubMeshFast (0).indexCount == 0 || !mesh->HasVertexData())
	{
		Vector3f v;
		StrideIterator<Vector3f> vertices(&v, 0);
		StrideIterator<Vector3f> normals;

		for (int i = firstIndex;i<particles.size ();i++)
		{
			SetupParticle (0, particles[i], velocityOffset, scale, rotation, rotationAndScale, deltaTime, vertices, normals);
			aabb.Encapsulate (particles[i].position);
		}
		return;
	}

	SubMesh& submesh = mesh->GetSubMeshFast (0);
	int vertexCount = mesh->GetVertexCount();
	
	StrideIterator<Vector3f> vertices = mesh->GetVertexBegin();
	StrideIterator<Vector3f> normals = mesh->GetNormalBegin();
	const UInt16* buffer = mesh->GetSubMeshBuffer16(0);

	if (!m_Systematic)
	{
		if (m_InterpolateTriangles)
		{
			if (submesh.topology == kPrimitiveTriangleStripDeprecated)
			{
				for (int i = firstIndex;i<particles.size ();i++)
				{
					SetupParticleStrip (particles[i], velocityOffset, scale, rotation, rotationAndScale, deltaTime, vertices, normals, buffer, submesh.indexCount);
					aabb.Encapsulate (particles[i].position);
				}
			}
			else if (submesh.topology == kPrimitiveTriangles)
			{
				for (int i = firstIndex;i<particles.size ();i++)
				{
					SetupParticleTri (particles[i], velocityOffset, scale, rotation, rotationAndScale, deltaTime, vertices, normals, buffer, submesh.indexCount/3);
					aabb.Encapsulate (particles[i].position);
				}
			}
		}
		else {
			if (vertexCount != 0)
			{
				for (int i = firstIndex;i<particles.size ();i++) {
					SetupParticle (RangedRandom (gMeshEmitterRand, 0, vertexCount), particles[i], velocityOffset, scale, rotation, rotationAndScale, deltaTime, vertices, normals);
					aabb.Encapsulate (particles[i].position);
				}
			}
		}
	}
	else {
		if (vertexCount != 0)
		{
			// Just in case the mesh changed while the particle emitter was running
			if (m_VertexIndex >= vertexCount) 
				m_VertexIndex = 0;
			
			for (int i = firstIndex;i<particles.size ();i++) {
				SetupParticle (m_VertexIndex, particles[i], velocityOffset, scale, rotation, rotationAndScale, deltaTime, vertices, normals);
				aabb.Encapsulate (particles[i].position);

				m_VertexIndex++;
				if (m_VertexIndex >= vertexCount) 
					m_VertexIndex = 0;
			}
		}
	}
}

void MeshParticleEmitter::SetupParticle (
	int vertexIndex,
	Particle& p,
	const Vector3f& velocityOffset,
	const Matrix4x4f& scale,
	const Matrix3x3f& rotation,
	const Matrix3x3f& normalTransform,
	float deltaTime,
	StrideIterator<Vector3f> vertices,
	StrideIterator<Vector3f> normals)
{
	InitParticleEnergy(gMeshEmitterRand, p, deltaTime);

	// position/normal of particle is vertex/vertex normal from mesh
	Vector3f positionOnMesh = vertices[vertexIndex];
	positionOnMesh = scale.MultiplyPoint3 (positionOnMesh);
	
	Vector3f normal;
	if (!normals.IsNull ())	
	{
		normal = normalTransform.MultiplyVector3 (normals[vertexIndex]);
		normal = NormalizeFast (normal);
	}
	else
		normal = Vector3f(0,0,0);
		
	
	// Set particle starting position
	p.position =	 m_PreviousEmitterPos;
	p.position += velocityOffset * RangedRandom (gMeshEmitterRand, 0.0F, deltaTime);
	p.position += (m_EmitterPos - m_PreviousEmitterPos) * Random01 (gMeshEmitterRand);
	p.position += rotation.MultiplyVector3 (positionOnMesh);
		
	// Set velocity
	p.velocity = velocityOffset + normal * RangedRandom (gMeshEmitterRand, m_MinNormalVelocity, m_MaxNormalVelocity);
	p.velocity += rotation.MultiplyVector3 (RandomPointInsideEllipsoid (gMeshEmitterRand, m_RndVelocity));
	
	p.rotation = m_RndInitialRotations ? RangedRandom (gMeshEmitterRand, 0.0f, 2*kPI):0.0F;
	float angularVelocity = m_AngularVelocity;
#if SUPPORT_REPRODUCE_LOG
	if (m_RndAngularVelocity > Vector3f::epsilon)
#endif
	angularVelocity += RangedRandom (gMeshEmitterRand, -m_RndAngularVelocity, m_RndAngularVelocity);
	p.angularVelocity = Deg2Rad(angularVelocity);

	p.color = ColorRGBA32 (255, 255, 255, 255);
	
	// Set size
	p.size = RangedRandom (gMeshEmitterRand, m_MinSize, m_MaxSize);
}

void MeshParticleEmitter::SetupParticleTri (
	Particle& p,
	const Vector3f& velocityOffset,
	const Matrix4x4f& scale,
	const Matrix3x3f& rotation,
	const Matrix3x3f& normalTransform,
	float deltaTime,
	StrideIterator<Vector3f> vertices,
	StrideIterator<Vector3f> normals,
	const UInt16* indices,
	int triCount)
{
	InitParticleEnergy(gMeshEmitterRand, p, deltaTime);

	int triIndex = RangedRandom (gMeshEmitterRand, 0, (int)triCount);
	const UInt16* face = indices + triIndex * 3;
	Vector3f barycenter = RandomBarycentricCoord (gMeshEmitterRand);

	// Interpolate vertex with barycentric coordinate
	Vector3f positionOnMesh = barycenter.x * vertices[face[0]] + barycenter.y * vertices[face[1]] + barycenter.z * vertices[face[2]];
	positionOnMesh = scale.MultiplyPoint3 (positionOnMesh);
	
	Vector3f normal;
	if (!normals.IsNull ())
	{
		// Interpolate normal with barycentric coordinate
		Vector3f const& normal1 = normals[face[0]];
		Vector3f const& normal2 = normals[face[1]];
		Vector3f const& normal3 = normals[face[2]];
		normal = barycenter.x * normal1 + barycenter.y * normal2 + barycenter.z * normal3;
		normal = normalTransform.MultiplyVector3 (normal);
		normal = NormalizeFast (normal);
	}
	else
		normal = Vector3f(0,0,0);

	// Set particle starting position
	p.position  = m_PreviousEmitterPos;
	p.position += velocityOffset * RangedRandom (gMeshEmitterRand, 0.0F, deltaTime);
	p.position += (m_EmitterPos - m_PreviousEmitterPos) * Random01 (gMeshEmitterRand);
	p.position += rotation.MultiplyVector3 (positionOnMesh);
		
	// Set velocity
	p.velocity = velocityOffset + normal * RangedRandom (gMeshEmitterRand, m_MinNormalVelocity, m_MaxNormalVelocity);
	p.velocity += rotation.MultiplyVector3 (RandomPointInsideEllipsoid (gMeshEmitterRand, m_RndVelocity));
	
	p.rotation = m_RndInitialRotations ? RangedRandom (gMeshEmitterRand, 0.0f, 2*kPI):0.0F;
	float angularVelocity = m_AngularVelocity;
#if SUPPORT_REPRODUCE_LOG
	if (m_RndAngularVelocity > Vector3f::epsilon)
#endif
	angularVelocity += RangedRandom (gMeshEmitterRand, -m_RndAngularVelocity, m_RndAngularVelocity);
	p.angularVelocity = Deg2Rad(angularVelocity);

	p.color = ColorRGBA32 (255, 255, 255, 255);
	
	// Set size
	p.size = RangedRandom (gMeshEmitterRand, m_MinSize, m_MaxSize);
}

void MeshParticleEmitter::SetupParticleStrip (
	Particle& p,
	const Vector3f& velocityOffset,
	const Matrix4x4f& scale,
	const Matrix3x3f& rotation,
	const Matrix3x3f& normalTransform,
	float deltaTime,
	StrideIterator<Vector3f> vertices,
	StrideIterator<Vector3f> normals,
	const UInt16* strip,
	int stripSize)
{
	InitParticleEnergy(gMeshEmitterRand, p, deltaTime);

	// Extract indices from tristrip
	int stripIndex = RangedRandom (gMeshEmitterRand, 2, stripSize);
	UInt16 a = strip[stripIndex-2];
	UInt16 b = strip[stripIndex-1];
	UInt16 c = strip[stripIndex];
	// Ignore degenerate triangles
	while (a == b || a == c || b == c)
	{
		stripIndex = RangedRandom (gMeshEmitterRand, 2, stripSize);
		a = strip[stripIndex-2];
		b = strip[stripIndex-1];
		c = strip[stripIndex];
		while (a == b || a == c || b == c)
		{
			stripIndex++;
			if (stripIndex >= stripSize)
				break;
			a = strip[stripIndex-2];
			b = strip[stripIndex-1];
			c = strip[stripIndex];
		}
	}
			
	Vector3f barycenter = RandomBarycentricCoord (gMeshEmitterRand);

	// Interpolate vertex with barycentric coordinate
	Vector3f positionOnMesh = barycenter.x * vertices[a] + barycenter.y * vertices[b] + barycenter.z * vertices[c];
	positionOnMesh = scale.MultiplyPoint3 (positionOnMesh);

	
	Vector3f normal;
	if (!normals.IsNull ())
	{
		// Interpolate normal with barycentric coordinate
		Vector3f normal1 = normals[a];
		Vector3f normal2 = normals[b];
		Vector3f normal3 = normals[c];
		normal =  barycenter.x * normal1 + barycenter.y * normal2 + barycenter.z * normal3;
		normal = normalTransform.MultiplyVector3 (normal);
		normal = NormalizeFast (normal);
	}
	else
	{
		normal = Vector3f(0,0,0);
	}
	
	// Set particle starting position
	p.position =	m_PreviousEmitterPos;
	p.position += velocityOffset * RangedRandom (gMeshEmitterRand, 0.0F, deltaTime);
	p.position += (m_EmitterPos - m_PreviousEmitterPos) * Random01 (gMeshEmitterRand);
	p.position += rotation.MultiplyVector3 (positionOnMesh);
	
	// Set velocity
	p.velocity = velocityOffset + normal * RangedRandom (gMeshEmitterRand, m_MinNormalVelocity, m_MaxNormalVelocity);
	p.velocity += rotation.MultiplyVector3 (RandomPointInsideEllipsoid (gMeshEmitterRand, m_RndVelocity));
	
	p.rotation = m_RndInitialRotations ? RangedRandom (gMeshEmitterRand, 0.0f, 2*kPI):0.0F;
	float angularVelocity = m_AngularVelocity;
#if SUPPORT_REPRODUCE_LOG
	if (m_RndAngularVelocity > Vector3f::epsilon)
#endif
	angularVelocity += RangedRandom (gMeshEmitterRand, -m_RndAngularVelocity, m_RndAngularVelocity);
	p.angularVelocity = Deg2Rad(angularVelocity);

	p.color = ColorRGBA32 (255, 255, 255, 255);
	
	// Set size
	p.size = RangedRandom (gMeshEmitterRand, m_MinSize, m_MaxSize);
}

static void ResetRandSeedForMeshParticleEmitter ()
{
	gMeshEmitterRand.SetSeed (4);
}

void MeshParticleEmitter::InitializeClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Register(ResetRandSeedForMeshParticleEmitter);
}

void MeshParticleEmitter::CleanupClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Unregister(ResetRandSeedForMeshParticleEmitter);
}

IMPLEMENT_CLASS_HAS_INIT (MeshParticleEmitter)
IMPLEMENT_OBJECT_SERIALIZE (MeshParticleEmitter)

template<class TransferFunction> inline
void MeshParticleEmitter::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_InterpolateTriangles);
	TRANSFER (m_Systematic);
	transfer.Align();
	TRANSFER (m_MinNormalVelocity);
	TRANSFER (m_MaxNormalVelocity);
	TRANSFER (m_Mesh);
}

void MeshParticleEmitter::SetMesh (PPtr<Mesh> mesh)
{
	m_Mesh = mesh;
	SetDirty();
} 
