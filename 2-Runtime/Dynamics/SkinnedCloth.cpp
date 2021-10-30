#include "UnityPrefix.h"
#include "SkinnedCloth.h"

#if ENABLE_CLOTH

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Filters/Mesh/MeshSkinning.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "Runtime/Input/TimeManager.h"
#include "PhysicsManager.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Math/Random/Random.h"

extern Rand gClothRand;

namespace Unity
{

SkinnedCloth::SkinnedCloth (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode),
	m_UpdateNode(this)
{
	m_Fade = 0.0f;
	m_TargetFade = 1.0f;
	m_NeedsToReadVertices = true;
	m_InterpolationTime = 0.1f;
	m_WorldVelocityScale = 0.5f;
	m_WorldAccelerationScale = 1.0f;
}

SkinnedCloth::~SkinnedCloth ()
{
}

void SkinnedCloth::Reset ()
{
	Super::Reset();

	m_WorldVelocityScale = 0.5f;
	m_WorldAccelerationScale = 1.0f;
}

#if ENABLE_CLOTH

void SkinnedCloth::Create()
{
	Cleanup ();

	SkinnedMeshRenderer *smr = QueryComponent(SkinnedMeshRenderer);
	Assert (smr != NULL);
	if (smr->GetMesh() == NULL)
		return;

	m_LastFrameWorldPosition = smr->GetActualRootBone().GetPosition();
	m_LastFrameVelocity = Vector3f::zero;

	smr->SetCloth (this);
	m_Mesh = smr->GetMesh();

#if UNITY_PS3
	//-- we need to unify the streams for skinned cloth
	int streamCount = 0;
	VertexData& oldVertexData = m_Mesh->GetVertexData();
	if (oldVertexData.GetActiveStreamCount() > 2)
	{
		VertexStreamsLayout kHotColdSplit = {{ kShaderChannelsHot, kShaderChannelsCold, 0, 0 }};
		VertexData newVertexData(oldVertexData, m_Mesh->GetAvailableChannels(), kHotColdSplit);
		swap (newVertexData, oldVertexData);
	}
#endif

	SetupMeshData (false, true, 1);

	NxClothDesc clothDesc;
	SetupClothDesc (clothDesc, false);
	clothDesc.flags |= NX_CLF_DISABLE_COLLISION;

	m_ClothScene = GetPhysicsManager ().GetClothingScene();
	m_Cloth = m_ClothScene->createCloth(clothDesc);

	if (m_Cloth == NULL)
		GetDynamicsSDK().releaseClothMesh(*clothDesc.clothMesh);

	SetupCoefficients ();

	m_VertexBuffer = NULL;
	m_NormalBuffer = NULL;
	m_TangentBuffer = NULL;
	m_VertexBufferStride = 0;
}

void SkinnedCloth::Cleanup ()
{
	Super::Cleanup ();

	if (GetGameObjectPtr() != NULL)
	{
		SkinnedMeshRenderer *smr = QueryComponent(SkinnedMeshRenderer);
		if (smr != NULL)
			smr->SetCloth (NULL);
	}
}

void SkinnedCloth::SetEnabled (bool enabled) {

	if (enabled != GetEnabled())
	{
		Super::SetEnabled (enabled);
		if (enabled)
		{
			m_Fade = 0.0f;
			m_TargetFade = 1.0f;
			m_InterpolationTime = 0.1f;
			m_NeedsToReadVertices = true;
			SkinnedMeshRenderer *smr = QueryComponent(SkinnedMeshRenderer);
			smr->SetCloth (this);
			m_LastFrameWorldPosition = smr->GetActualRootBone().GetPosition();
			m_LastFrameVelocity = Vector3f::zero;
		}
		else
			GetComponent(SkinnedMeshRenderer).SetCloth (NULL);
	}
}

void SkinnedCloth::SetEnabledFading (bool enabled, float interpolationTime)
{
	if (enabled && !GetEnabled())
		SetEnabled(true);
	m_TargetFade = enabled ? 1.0f : 0.0f;
	m_InterpolationTime = interpolationTime;
}

void SkinnedCloth::LateUpdate ()
{
	if (m_Cloth)
	{
		// calculate forces from world space movement
		Vector3f externalAcceleration = m_ExternalAcceleration+RandomPointInsideCube (gClothRand, m_RandomAcceleration);
		SkinnedMeshRenderer *smr = QueryComponent(SkinnedMeshRenderer);
		Vector3f worldPosition = smr->GetActualRootBone().GetPosition();
		Vector3f velocity = (m_LastFrameWorldPosition - worldPosition) * GetInvDeltaTime();
		// Use squared velocity, as air friction is quadratic.
		externalAcceleration += velocity * Magnitude(velocity) * m_WorldVelocityScale;
		Vector3f acceleration = (m_LastFrameVelocity - velocity) * GetInvDeltaTime();
		externalAcceleration -= acceleration * m_WorldAccelerationScale;

		m_LastFrameWorldPosition = worldPosition;
		m_LastFrameVelocity = velocity;
		m_Cloth->setExternalAcceleration((const NxVec3&)externalAcceleration);
	}

	if (m_Fade != m_TargetFade)
	{
		if (m_InterpolationTime == 0.0f)
			m_Fade = m_TargetFade;
		else if (m_Fade > m_TargetFade)
		{
			m_Fade -= GetDeltaTime() / m_InterpolationTime;
			if (m_Fade < m_TargetFade)
				m_Fade = m_TargetFade;
		}
		else if (m_Fade < m_TargetFade)
		{
			m_Fade += GetDeltaTime() / m_InterpolationTime;
			if (m_Fade > m_TargetFade)
				m_Fade = m_TargetFade;
		}
		SetupCoefficients ();
		if (m_Fade == 0.0f && m_TargetFade == 0.0f)
			SetEnabled (false);
	}
}

void SkinnedCloth::AddToManager ()
{
	GetLateBehaviourManager().AddBehaviour (m_UpdateNode, -1);
}

void SkinnedCloth::RemoveFromManager ()
{
	GetLateBehaviourManager().RemoveBehaviour (m_UpdateNode);
}

void SkinnedCloth::SetupCoefficients ()
{
	m_Coefficients.resize (m_NumVertices);
	if (m_Cloth)
	{
		if (m_Fade == 1.0)
			m_Cloth->setConstrainCoefficients ((NxClothConstrainCoefficients*)&m_Coefficients[0]);
		else
		{
			std::vector<ClothConstrainCoefficients> coefficients = m_Coefficients;
			for (std::vector<ClothConstrainCoefficients>::iterator i = coefficients.begin(); i != coefficients.end(); i++)
				i->maxDistance *= m_Fade;
			m_Cloth->setConstrainCoefficients ((NxClothConstrainCoefficients*)&coefficients[0]);
		}
	}
}

PROFILER_INFORMATION(gSetUpProfile, "SkinnedCloth.SetUpSkinnedBuffers", kProfilerPhysics)

void SkinnedCloth::SetUpSkinnedBuffers (void *vertices, void *normals, void *tangents, size_t bufferStride)
{
	PROFILER_AUTO(gSetUpProfile, NULL)

	// If the mesh has changed, re-created skinning buffers to match the changed mesh
	SkinnedMeshRenderer *smr = QueryComponent(SkinnedMeshRenderer);
	if (smr->GetMesh() != m_Mesh || m_Mesh->GetVertexCount() != m_VertexTranslationTable.size())
		Create();

	QuaternionToMatrix(Inverse(smr->GetActualRootBone().GetRotation()), m_WorldToLocalRotationMatrix);

	int numTotalVertices = m_VertexTranslationTable.size();
	if (normals)
	{
		for (int i=0; i<numTotalVertices; i++)
		{
			int index = m_VertexTranslationTable[i];
			m_Vertices[index] = *(Vector3f*)((char*)vertices+i*bufferStride);
			m_Normals[index] = *(Vector3f*)((char*)normals+i*bufferStride);
		}
	}
	else
	{
		for (int i=0; i<numTotalVertices; i++)
		{
			int index = m_VertexTranslationTable[i];
			m_Vertices[index] = *(Vector3f*)((char*)vertices+i*bufferStride);
		}
	}

	// If the component has just been enabled, read vertices for the first frame,
	// so we don't suddenly jerk into place.
	if (m_NeedsToReadVertices)
	{
		m_NeedsToReadVertices = false;
		m_Cloth->setPositions (&m_Vertices[0], sizeof(Vector3f));
	}

	m_Cloth->setConstrainPositions (&m_Vertices[0], sizeof(Vector3f));
	if (normals)
		m_Cloth->setConstrainNormals (&m_Normals[0], sizeof(Vector3f));

	m_VertexBuffer = vertices;
	m_NormalBuffer = normals;
	m_TangentBuffer = tangents;
	m_VertexBufferStride = bufferStride;
}

void SkinnedCloth::ReadBackSkinnedBuffers ()
{
	if (m_VertexBuffer != NULL)
	{
		TransformPoints3x3 (m_WorldToLocalRotationMatrix, &m_Vertices[0], &m_Vertices[0], m_NumVertices);
		TransformPoints3x3 (m_WorldToLocalRotationMatrix, &m_Normals[0], &m_Normals[0], m_NumVertices);

		int numTotalVertices = m_VertexTranslationTable.size();
		if (m_NormalBuffer)
		{
			for (int i=0; i<numTotalVertices; i++)
			{
				int index = m_VertexTranslationTable[i];
				*(Vector3f*)((char*)m_VertexBuffer+i*m_VertexBufferStride) = m_Vertices[index];
				*(Vector3f*)((char*)m_NormalBuffer+i*m_VertexBufferStride) = m_Normals[index];
			}
		}
		else
		{
			for (int i=0; i<numTotalVertices; i++)
			{
				int index = m_VertexTranslationTable[i];
				*(Vector3f*)((char*)m_VertexBuffer+i*m_VertexBufferStride) = m_Vertices[index];
			}
		}

		if (m_TangentBuffer != NULL)
		{
			// PhysX cloth will not output tangents. But we should at least
			// transform them so things don't look completely wrong.
			for (int i=0; i<numTotalVertices; i++)
			{
				Vector3f& v = *(Vector3f*)((char*)m_TangentBuffer+i*m_VertexBufferStride);
				v = m_WorldToLocalRotationMatrix.MultiplyPoint3 (v);
			}
		}
	}
}

void SkinnedCloth::SetCoefficients(ClothConstrainCoefficients *coefficients)
{
	memcpy (&m_Coefficients[0], coefficients, sizeof(SkinnedCloth::ClothConstrainCoefficients) * m_Coefficients.size());
	SetupCoefficients();
	SetDirty();
}

#else //ENABLE_CLOTH

void SkinnedCloth::Cleanup () {}
void SkinnedCloth::Create() {}
void SkinnedCloth::LateUpdate() {}
void SkinnedCloth::SetEnabled (bool) {}
void SkinnedCloth::SetEnabledFading (bool enabled, float interpolationTime) {}
void SkinnedCloth::AddToManager () {}
void SkinnedCloth::RemoveFromManager () {}
void SkinnedCloth::SetCoefficients(ClothConstrainCoefficients *coefficients) {}
#endif //ENABLE_CLOTH

void SkinnedCloth::SetWorldVelocityScale (float value)
{
	m_WorldVelocityScale = value;
	SetDirty();
}

void SkinnedCloth::SetWorldAccelerationScale (float value)
{
	m_WorldAccelerationScale = value;
	SetDirty();
}

template<class TransferFunction>
void SkinnedCloth::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TRANSFER (m_WorldVelocityScale);
	TRANSFER (m_WorldAccelerationScale);
	transfer.Transfer (m_Coefficients, "m_Coefficients", kHideInEditorMask);
}

template<class TransferFunction>
void SkinnedCloth::ClothConstrainCoefficients::Transfer (TransferFunction& transfer)
{
	TRANSFER (maxDistance);
	TRANSFER (maxDistanceBias);
	TRANSFER (collisionSphereRadius);
	TRANSFER (collisionSphereDistance);
}

}

IMPLEMENT_CLASS (SkinnedCloth)
IMPLEMENT_OBJECT_SERIALIZE (SkinnedCloth)

#endif // ENABLE_CLOTH
