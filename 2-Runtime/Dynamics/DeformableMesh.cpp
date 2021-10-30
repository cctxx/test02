#include "UnityPrefix.h"
#include "DeformableMesh.h"

#if ENABLE_CLOTH

#include "Runtime/Graphics/Transform.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "PhysicsManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "nxmemorystream.h"
#include "Runtime/Math/Random/Random.h"
#include "Runtime/Dynamics/ExtractDataFromMesh.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

Rand gClothRand (1);

namespace Unity {

Cloth::Cloth (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
	, m_FixedUpdateNode(this)	
	, m_Indices(kMemVertexData)
	, m_Vertices(kMemVertexData)
	, m_Normals(kMemVertexData)
	, m_Tangents(kMemVertexData)
	, m_UVs(kMemVertexData)
	, m_UV1s(kMemVertexData)
	, m_Colors(kMemVertexData)
	, m_TranslatedVertices(kMemVertexData)
	, m_TranslatedNormals(kMemVertexData)
	, m_TranslatedIndices(kMemVertexData)
{	
	// configuration
	m_BendingStiffness = 0.0f;
	m_StretchingStiffness = 1.0f;
	m_Damping = 0.0f;
	m_Thickness = 0.2f;
	m_UseGravity = true;
	m_SelfCollision = false;
	m_ExternalAcceleration = Vector3f::zero;
	m_RandomAcceleration = Vector3f::zero;

	// state
	m_Cloth = NULL;
	m_ClothScene = NULL;
	m_NumVertices = 0;
	m_NumIndices = 0;
	m_NumParentIndices = 0;
	m_NumVerticesFromPhysX = 0;
	m_NumIndicesFromPhysX = 0;
	m_SuspendCount = 0;
	m_IsSuspended = false;
	m_NeedToWakeUp = false;
	m_VerticesForRendering = &m_Vertices;
	m_NormalsForRendering = &m_Normals;
	m_IndicesForRendering = &m_Indices;
	m_NumVerticesForRendering = 0;
	m_NumIndicesForRendering = 0;
}

Cloth::~Cloth ()
{
	Cleanup();
}

void Cloth::Reset ()
{
	Super::Reset();

	// configuration
	m_BendingStiffness = 0.0f;
	m_StretchingStiffness = 1.0f;
	m_Damping = 0.0f;
	m_Thickness = 0.2f;
	m_UseGravity = true;
	m_SelfCollision = false;
	m_ExternalAcceleration = Vector3f::zero;
	m_RandomAcceleration = Vector3f::zero;
}

void Cloth::Cleanup ()
{
	if (m_Cloth)
	{
		NxClothMesh* mesh = m_Cloth->getClothMesh();
		m_ClothScene->releaseCloth(*m_Cloth);
		GetDynamicsSDK().releaseClothMesh(*mesh);
		m_Cloth = NULL;
	}
	m_NumVertices = 0;
	m_NumVerticesFromPhysX = 0;
	m_NumVerticesForRendering = 0;
	m_NumIndices = 0;
	m_NumIndicesFromPhysX = 0;
	m_NumIndicesForRendering = 0;
}

void Cloth::ProcessMeshForRenderer ()
{
	if (m_NumVerticesFromPhysX > m_NumVertices)
	{
		int newVertices = m_NumVerticesFromPhysX - m_NumVertices;
		int size = m_NumVertices;
		if (m_VertexTranslationTable.size())
		{
			size = m_VertexTranslationTable.size();
			m_VertexTranslationTable.resize_uninitialized(size + newVertices);
			for (int i=0; i<newVertices; i++)
				m_VertexTranslationTable[size + i] = m_NumVertices + i;
		}

		if (!m_UVs.empty())
		{
			for (int i=0; i<newVertices; i++)
				m_UVs[size+i] = m_UVs[m_ParentIndices[m_NumVertices+i]];
		}

		if (!m_UV1s.empty())
		{
			for (int i=0; i<newVertices; i++)
				m_UV1s[size+i] = m_UV1s[m_ParentIndices[m_NumVertices+i]];
		}
		
		if (!m_Colors.empty())
		{
			for (int i=0; i<newVertices; i++)
				m_Colors[size+i] = m_Colors[m_ParentIndices[m_NumVertices+i]];
		}

		if (!m_Tangents.empty())
		{
			for (int i=0; i<newVertices; i++)
				m_Tangents[size+i] = m_Tangents[m_ParentIndices[m_NumVertices+i]];
		}

		m_NumVertices = m_NumVerticesFromPhysX;
		m_NumVerticesForRendering += newVertices;
	}
	if (m_VertexTranslationTable.size())
	{
		for (int i=0; i<m_VertexTranslationTable.size(); i++)
		{
			int index = m_VertexTranslationTable[i];
			m_TranslatedVertices[i] = m_Vertices[index];
			m_TranslatedNormals[i] = m_Normals[index];
		}
		if (m_NumIndicesFromPhysX > m_NumIndices)
		{
			int size = m_NumIndicesForRendering;
			int newIndices = m_NumIndicesFromPhysX > m_NumIndices;
			for (int i=0; i<newIndices; i++)
			{
				int newIndex = m_Indices[m_NumIndices + i];
				if (newIndex < m_NumVertices)
					m_TranslatedIndices[size + i] = newIndex;
				else
					m_TranslatedIndices[size + i] = m_VertexTranslationTable[newIndex];
			}
			m_NumIndicesForRendering += newIndices; 
			m_NumIndices = m_NumIndicesFromPhysX;
		}
	}
	else if (m_NumIndicesFromPhysX > m_NumIndices)
	{
		m_NumIndices = m_NumIndicesFromPhysX;
		m_NumIndicesForRendering = m_NumIndicesFromPhysX;
	}
}

bool Cloth::SetupMeshData (bool transformToWorldSpace, bool externalVertexTranlation, int tearMemoryFactor)
{
	if (!ExtractDataFromMesh(*m_Mesh, m_Vertices, m_Indices, m_VertexTranslationTable))
		return false;

    m_NumVertices = m_Vertices.size();
    m_NumIndices = m_Indices.size();
    
	bool vertexTranlationIsNeeded = externalVertexTranlation;

	StrideIterator<Vector2f> uvs = m_Mesh->GetUvBegin(0);
	StrideIterator<Vector2f> uv1s = m_Mesh->GetUvBegin(1);
	StrideIterator<ColorRGBA32> colors = m_Mesh->GetColorBegin();
	if (!vertexTranlationIsNeeded)
	{
		// find out if vertex mapping really is needed. If there are UV or color seams, it is.
		// If there are just some duplicate vertices because the mesh is stripified, then we just use the physX indices instead.
		for (int i=m_NumVertices; i<m_VertexTranslationTable.size();i++)
		{
			if (!uvs.IsNull () && uvs[m_VertexTranslationTable[i]] != uvs[i])
			{
				vertexTranlationIsNeeded = true;
				break;
			}
			if (!uv1s.IsNull () && uv1s[m_VertexTranslationTable[i]] != uv1s[i])
			{
				vertexTranlationIsNeeded = true;
				break;
			}
			if (!colors.IsNull () && colors[m_VertexTranslationTable[i]] != colors[i])
			{
				vertexTranlationIsNeeded = true;				
				break;
			}
			// we don't check for normals and tangents.
			// normals are regenerated by the cloth code anyways, so split normals are no use here.
		}
	}

	if (!vertexTranlationIsNeeded)
		m_VertexTranslationTable.clear();

	UInt32 maxVertices = std::max (std::max (m_NumVertices, m_NumVerticesFromPhysX), m_NumVertices * tearMemoryFactor);
	UInt32 maxIndices = std::max (m_NumIndices, m_NumIndices * tearMemoryFactor);

	m_Vertices.resize_uninitialized(maxVertices);
	m_Normals.resize_uninitialized(maxVertices);
	m_Indices.resize_initialized(maxIndices, 0);
	if (tearMemoryFactor>1)
		m_ParentIndices.resize_initialized(maxVertices, 0);
	else
		m_ParentIndices.clear();
	
	int maxVerticesForRendering;
	if (vertexTranlationIsNeeded && !externalVertexTranlation)
	{
		Mesh::TemporaryIndexContainer triangles;
		m_Mesh->GetTriangles(triangles);
		m_NumIndicesForRendering = triangles.size();
		m_TranslatedIndices.resize_uninitialized(m_NumIndicesForRendering + m_NumIndices * (tearMemoryFactor-1));
		for (int i=0; i<m_NumIndicesForRendering; i++)	
			m_TranslatedIndices[i] = triangles[i];
		maxVerticesForRendering = m_Mesh->GetVertexCount() + m_NumVertices * (tearMemoryFactor-1);
		m_TranslatedVertices.resize_uninitialized(maxVerticesForRendering);
		m_TranslatedNormals.resize_uninitialized(maxVerticesForRendering);
		m_NumVerticesForRendering = m_Mesh->GetVertexCount();
		m_VerticesForRendering = &m_TranslatedVertices;
		m_NormalsForRendering = &m_TranslatedNormals;
		m_IndicesForRendering = &m_TranslatedIndices;
	}
	else 
	{
		maxVerticesForRendering = maxVertices;
		m_TranslatedIndices.clear();
		m_TranslatedVertices.clear();
		m_TranslatedNormals.clear();
		m_VerticesForRendering = &m_Vertices;
		m_NormalsForRendering = &m_Normals;
		m_IndicesForRendering = &m_Indices;
		m_NumVerticesForRendering = m_NumVertices;
		m_NumIndicesForRendering = m_NumIndices;
	}
	
	if (!m_Mesh->IsAvailable (kShaderChannelNormal))
	{
		WarningString("Cloth simulation requires normals!");
		std::fill (m_Normals.begin (), m_Normals.end (), Vector3f(0,0,0));
	}
	else if (vertexTranlationIsNeeded)
	{
		StrideIterator<Vector3f> n = m_Mesh->GetNormalBegin ();
		for (int i=0;i<m_VertexTranslationTable.size();++i, ++n)
		{
			int index = m_VertexTranslationTable[i];
			m_Normals[index] = *n;
		}
	}
	else
	{
		strided_copy (m_Mesh->GetNormalBegin (), m_Mesh->GetNormalBegin () + m_NumVertices, m_Normals.begin ());
	}
	
	if (!externalVertexTranlation)
	{
		if (m_Mesh->IsAvailable (kShaderChannelTexCoord0))
		{
			m_UVs.resize_uninitialized(maxVerticesForRendering);
			strided_copy (m_Mesh->GetUvBegin (0), m_Mesh->GetUvBegin (0) + m_NumVerticesForRendering, m_UVs.begin ());
		}
		else
			m_UVs.clear();

		if (m_Mesh->IsAvailable (kShaderChannelTexCoord1))
		{
			m_UV1s.resize_uninitialized(maxVerticesForRendering);
			strided_copy (m_Mesh->GetUvBegin (1), m_Mesh->GetUvBegin (1) + m_NumVerticesForRendering, m_UV1s.begin ());
		}
		else
			m_UV1s.clear();

		if (m_Mesh->IsAvailable(kShaderChannelColor))
		{
			m_Colors.resize_uninitialized(maxVerticesForRendering);
			strided_copy (m_Mesh->GetColorBegin (), m_Mesh->GetColorBegin () + m_NumVerticesForRendering, m_Colors.begin ());
		}
		else
			m_Colors.clear();

		if (m_Mesh->IsAvailable(kShaderChannelTangent))
		{
			m_Tangents.resize_uninitialized(maxVerticesForRendering);
			strided_copy (m_Mesh->GetTangentBegin (), m_Mesh->GetTangentBegin () + m_NumVerticesForRendering, m_Tangents.begin ());
		}
		else
			m_Tangents.clear();
	}
	
	if (transformToWorldSpace)
	{
		Transform& transform = GetComponent (Transform);
		TransformPoints3x4(transform.GetLocalToWorldMatrix(), &m_Vertices[0], &m_Vertices[0], m_NumVertices);
		if (m_Mesh->IsAvailable (kShaderChannelNormal))
			TransformPoints3x3(transform.GetLocalToWorldMatrixNoScale(), &m_Normals[0], &m_Normals[0], m_NumVertices);	
		if (m_Mesh->IsAvailable (kShaderChannelTangent))
		{
			Matrix4x4f m = transform.GetLocalToWorldMatrixNoScale();
			for ( int i=0; i<m_NumVertices; i++)
			{
				Vector3f tangent = Vector3f(m_Tangents[i].x, m_Tangents[i].y, m_Tangents[i].z);
				Vector3f normalized = NormalizeSafe (m.MultiplyVector3 (tangent));
				m_Tangents[i] = Vector4f(normalized.x, normalized.y ,normalized.z, m_Tangents[i].w);
			}
		}

		m_NumVerticesFromPhysX = m_NumVertices;
	}
	
	return true;
}

void Cloth::SetupMeshBuffers (NxMeshData &meshData)
{
	meshData.verticesPosBegin = m_Vertices.begin();
	meshData.verticesNormalBegin = m_Normals.empty() ? NULL : m_Normals.begin();
	meshData.verticesPosByteStride = sizeof (Vector3f);
	meshData.verticesNormalByteStride = sizeof (Vector3f);
	meshData.maxVertices = m_Vertices.size();
	meshData.numVerticesPtr = (NxU32*)&m_NumVerticesFromPhysX;

	meshData.indicesBegin = &m_Indices[0];
	meshData.indicesByteStride = sizeof (UInt16);
	meshData.maxIndices = m_Indices.size();
	meshData.numIndicesPtr = (NxU32*)&m_NumIndicesFromPhysX;
	meshData.flags = NX_MDF_16_BIT_INDICES;	

	if (m_ParentIndices.size())
	{
		meshData.parentIndicesBegin = &m_ParentIndices[0];
		meshData.parentIndicesByteStride = sizeof (UInt16);
		meshData.maxParentIndices = m_ParentIndices.size();
		meshData.numParentIndicesPtr = (NxU32*)&m_NumParentIndices;
	}
}

NxClothMesh *Cloth::CookClothMesh (bool tearable)
{
	NxClothMeshDesc clothMeshDesc;

	clothMeshDesc.numVertices = m_NumVertices;
	clothMeshDesc.numTriangles = m_NumIndices / 3;
	clothMeshDesc.pointStrideBytes = sizeof (Vector3f);
	clothMeshDesc.triangleStrideBytes = sizeof (m_Indices[0]) * 3;
	clothMeshDesc.vertexMassStrideBytes = 0;
	clothMeshDesc.vertexFlagStrideBytes = 0;
	clothMeshDesc.points = &m_Vertices[0];
	clothMeshDesc.triangles = &m_Indices[0];
	clothMeshDesc.vertexMasses = NULL;
	clothMeshDesc.vertexFlags = NULL;
	clothMeshDesc.flags = NX_MF_16_BIT_INDICES;

	if (tearable)
		clothMeshDesc.flags |= NX_CLOTH_MESH_TEARABLE;

	MemoryStream memoryStream(NULL, 0);	
	if (!NxCookClothMesh(clothMeshDesc, memoryStream))
	{
		ErrorString ("Failed cooking cloth");
		return NULL;
	}
	
	return GetDynamicsSDK ().createClothMesh(memoryStream);
}

void Cloth::SetupClothDesc (NxClothDesc &clothDesc, bool tearable)
{ 
	SetupMeshBuffers (clothDesc.meshData);
	clothDesc.collisionGroup = GetGameObject ().GetLayer ();
	clothDesc.clothMesh = CookClothMesh(tearable);
	clothDesc.bendingStiffness = m_BendingStiffness;
	clothDesc.stretchingStiffness = m_StretchingStiffness;
	clothDesc.dampingCoefficient = m_Damping;
	clothDesc.thickness = m_Thickness;
	clothDesc.selfCollisionThickness = m_Thickness;
	clothDesc.userData = this;
	if (m_BendingStiffness > 0)
		clothDesc.flags |= NX_CLF_BENDING;
	else
		clothDesc.flags &= ~NX_CLF_BENDING;
	if (m_Damping > 0)
		clothDesc.flags |= NX_CLF_DAMPING;
	else
		clothDesc.flags &= ~NX_CLF_DAMPING;
	if (m_UseGravity)
		clothDesc.flags |= NX_CLF_GRAVITY;
	else
		clothDesc.flags &= ~NX_CLF_GRAVITY;
	if (m_SelfCollision)
		clothDesc.flags |= NX_CLF_SELFCOLLISION;
	else
		clothDesc.flags &= ~NX_CLF_SELFCOLLISION;
}

void Cloth::Deactivate (DeactivateOperation operation)
{
	Super::Deactivate (operation);
	Cleanup ();
}

void Cloth::FixedUpdate() 
{
	if (m_Cloth)
	{
		Vector3f accel = m_ExternalAcceleration+RandomPointInsideCube (gClothRand, m_RandomAcceleration);
		m_Cloth->setExternalAcceleration((const NxVec3&)accel);
		if (m_NeedToWakeUp && !m_IsSuspended)
			m_Cloth->wakeUp();
	}
	m_NeedToWakeUp = false;
}

void Cloth::AddToManager ()
{
	GetFixedBehaviourManager().AddBehaviour (m_FixedUpdateNode, -1);
}

void Cloth::RemoveFromManager ()
{
	GetFixedBehaviourManager().RemoveBehaviour (m_FixedUpdateNode);
}

void Cloth::PauseSimulation ()
{
	if (!m_Cloth->isSleeping())
	{
		m_IsSuspended = true;
		m_Cloth->putToSleep();
	}
}

void Cloth::ResumeSimulation ()
{
	if (m_IsSuspended)
	{
		// only wake up the cloth when it becomes visible when it has been set to sleep by the renderer,
		// not when it has been set to sleep by PhysX.
		m_IsSuspended = false;
		m_Cloth->wakeUp();
	}
}

void Cloth::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	if (m_Cloth)
	{
		// Apply changed values
		SetThickness (m_Thickness);
		SetBendingStiffness (m_BendingStiffness);
		SetStretchingStiffness (m_StretchingStiffness);
		SetDamping (m_Damping);
		SetExternalAcceleration (m_ExternalAcceleration);
		SetRandomAcceleration (m_RandomAcceleration);
		SetUseGravity (m_UseGravity);		
		SetSelfCollision (m_SelfCollision);
	}

	Super::AwakeFromLoad (awakeMode);
	if (IsActive ())
	{
		if (!m_Cloth)
			Create ();
		else
			m_NeedToWakeUp = true;
	}
	else
		Cleanup ();
	
	m_SuspendCount = 0;
	
	if(!GetEnabled())
		SetSuspended(true);
}

void Cloth::SetSuspended (bool suspended)
{
	if (suspended)
		m_SuspendCount++;
	else
		m_SuspendCount--;

	if (m_Cloth)
	{
		if (m_SuspendCount)
			PauseSimulation();
		else
			ResumeSimulation();
	}
}

void Cloth::SetEnabled (bool enabled)
{
	if (enabled != GetEnabled())
	{
		Super::SetEnabled(enabled);
		SetSuspended(!enabled);
	}
}

#define ENFORCE_MINEQ(x) {if (value < x) { value = x; ErrorString("value must be greater than or equal to " #x);}}
#define ENFORCE_MIN(x) {if (value <= x) { value = x; ErrorString("value must be greater than " #x);}}
#define ENFORCE_MAXEQ(x) {if (value > x) { value = x; ErrorString("value must be smaller than or equal to " #x);}}
#define ENFORCE_MAX(x) {if (value >= x) { value = x; ErrorString("value must be smaller than " #x);}}

void Cloth::SetBendingStiffness (float value)
{
	ENFORCE_MINEQ(0);
	ENFORCE_MAXEQ(1);
	if (value != m_BendingStiffness)
	{
		m_NeedToWakeUp = true;
		m_BendingStiffness = value;
	}
	if (m_Cloth)
	{
		if (value > 0)
		{
			m_Cloth->setBendingStiffness(value);
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_BENDING);			
		}	
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_BENDING));			
	}
	SetDirty();
}

void Cloth::SetStretchingStiffness (float value)
{
	ENFORCE_MIN(0);
	ENFORCE_MAXEQ(1);
	if (value != m_StretchingStiffness)
	{
		m_NeedToWakeUp = true;
		m_StretchingStiffness = value;
	}
	if (m_Cloth)
		m_Cloth->setStretchingStiffness(value);
	SetDirty();
}

void Cloth::SetDamping (float value)
{
	ENFORCE_MINEQ(0);
	ENFORCE_MAXEQ(1);
	if (value != m_Damping)
	{
		m_NeedToWakeUp = true;
		m_Damping = value;
	}
	if (m_Cloth)
	{
		if (value > 0)
		{
			m_Cloth->setDampingCoefficient(value);
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_DAMPING);			
		}
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_DAMPING));			
	}
	SetDirty();
}

void Cloth::SetThickness (float value)
{
	ENFORCE_MIN(0);

	if (value != m_Thickness)
	{
		m_NeedToWakeUp = true;
		m_Thickness = value;
	}
	if (m_Cloth)
	{
		m_Cloth->setThickness(value);
		m_Cloth->setSelfCollisionThickness(value);
	}
	SetDirty();
}


void Cloth::SetUseGravity (bool value)
{
	if (value != m_UseGravity)
	{
		m_NeedToWakeUp = true;
		m_UseGravity = value;
	}
	if (m_Cloth)
	{
		if (value)
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_GRAVITY);			
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_GRAVITY));			
	}	
	SetDirty();
}

void Cloth::SetSelfCollision (bool value)
{
	if (value != m_SelfCollision)
	{
		m_NeedToWakeUp = true;
		m_SelfCollision = value;
	}
	if (m_Cloth)
	{
		if (value)
			m_Cloth->setFlags(m_Cloth->getFlags() | NX_CLF_SELFCOLLISION);			
		else
			m_Cloth->setFlags(m_Cloth->getFlags() & ~(NX_CLF_SELFCOLLISION));			
	}	
	SetDirty();
}

void Cloth::SetExternalAcceleration (const Vector3f &value)
{
	if (value != m_ExternalAcceleration)
	{
		m_NeedToWakeUp = true;
		m_ExternalAcceleration = value;
	}
	SetDirty();
}

void Cloth::SetRandomAcceleration (const Vector3f &value)
{
	if (value != m_RandomAcceleration)
	{
		m_NeedToWakeUp = true;
		m_RandomAcceleration = value;
	}
	SetDirty();
}

template<class TransferFunction>
void Cloth::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER (m_BendingStiffness);
	TRANSFER (m_StretchingStiffness);
	TRANSFER (m_Damping);
	TRANSFER (m_Thickness);
	TRANSFER (m_UseGravity);
	TRANSFER (m_SelfCollision);
	transfer.Align();
	TRANSFER (m_ExternalAcceleration);
	TRANSFER (m_RandomAcceleration);
}

static void ResetRandSeed ()
{
	gClothRand.SetSeed (1);
}
	
void Cloth::InitializeClass ()
{
#if UNITY_EDITOR
	REGISTER_MESSAGE (Cloth, kTransformChanged, TransformChanged, int);
	REGISTER_MESSAGE_VOID (Cloth, kBecameVisible, BecameVisible);
	REGISTER_MESSAGE_VOID (Cloth, kBecameInvisible, BecameInvisible);
#endif	
	
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Register(ResetRandSeed);
}

void Cloth::CleanupClass ()
{
	GlobalCallbacks::Get().resetRandomAfterLevelLoad.Unregister(ResetRandSeed);
}

#if UNITY_EDITOR
// When the mesh is updated by Phyics, it acts independently of it's Transform, 
// simulation is in world coordinates. In the editor, we use this code to allow
// placement, though.
void Cloth::TransformChanged( int changeMask )
{
	if (!IsWorldPlaying() && IsActive())
	{
		Create();
	}
}

void Cloth::BecameVisible()
{
	if (GetSuspended())
		SetSuspended (false);
}

void Cloth::BecameInvisible()
{
	SetSuspended (true);
}

#endif
}

IMPLEMENT_CLASS_HAS_INIT (Cloth)
IMPLEMENT_OBJECT_SERIALIZE (Cloth)
INSTANTIATE_TEMPLATE_TRANSFER (Cloth)

#endif // ENABLE_CLOTH
