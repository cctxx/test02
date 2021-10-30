#include "UnityPrefix.h"
#include "ParticleSystem.h"
#include "ParticleSystemParticle.h"
#include "ParticleSystemRenderer.h"
#include "ParticleSystemUtils.h"
#include "Modules/SubModule.h"
#include "Modules/UVModule.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/Renderqueue.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/DrawUtil.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Profiler/ExternalGraphicsProfiler.h"
#include "Runtime/Threads/JobScheduler.h"
#include "Runtime/Filters/Misc/LineBuilder.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/GfxDevice/ChannelAssigns.h"
#include "Runtime/Graphics/TriStripper.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/BaseClasses/GameObject.h"

IMPLEMENT_CLASS_INIT_ONLY (ParticleSystemRenderer)
IMPLEMENT_OBJECT_SERIALIZE (ParticleSystemRenderer)

PROFILER_INFORMATION(gParticlesSort, "ParticleSystem.Sort", kProfilerParticles)
PROFILER_INFORMATION(gParticlesSingleProfile, "ParticleSystem.RenderSingle", kProfilerParticles)
PROFILER_INFORMATION(gParticlesBatchProfile, "ParticleSystem.RenderBatch", kProfilerParticles)
PROFILER_INFORMATION(gSubmitVBOParticleProfile, "Mesh.SubmitVBO", kProfilerRender)

#define DEBUG_PARTICLE_SORTING (0)
#if UNITY_WII
#define kMaxNumParticlesPerBatch (65536/6)
#else
#define kMaxNumParticlesPerBatch (min<int>(kDynamicBatchingIndicesThreshold/6, VBO::kMaxQuads))
#endif

struct ParticleSystemVertex
{
	Vector3f vert;
	Vector3f normal;
	ColorRGBA32	color;
	Vector2f uv;
	Vector4f tangent; // Here, we put 2nd uv + blend factor
};

struct ParticleSystemGeomConstInputData
{
	Matrix4x4f m_ViewMatrix;
	Vector3f m_CameraVelocity;
	Object* m_Renderer;
	UInt16 const* m_MeshIndexBuffer[ParticleSystemRendererData::kMaxNumParticleMeshes];
	int m_MeshIndexCount[ParticleSystemRendererData::kMaxNumParticleMeshes];
	int m_NumTilesX;
	int m_NumTilesY;
	float maxPlaneScale;
	float maxOrthoSize;
	float numUVFrame;
	float animUScale;
	float animVScale;
	Vector3f xSpan;
	Vector3f ySpan;
	bool usesSheetIndex;
	float bentNormalFactor;
	Vector3f bentNormalVector;
};

inline void ScaleMatrix(Matrix4x4f& matrix, float scale)
{
	matrix.m_Data[0] *= scale;
	matrix.m_Data[1] *= scale;
	matrix.m_Data[2] *= scale;
	matrix.m_Data[4] *= scale;
	matrix.m_Data[5] *= scale;
	matrix.m_Data[6] *= scale;
	matrix.m_Data[8] *= scale;
	matrix.m_Data[9] *= scale;
	matrix.m_Data[10] *= scale;
}

struct ParticleSort
{
	inline static void SetValues(ParticleSort& sort, UInt32 inIndex, int inIntValue)
	{
		sort.index = inIndex;
		sort.intValue = inIntValue;
	}

	inline static bool CompareValue (const ParticleSort& left, const ParticleSort& right)
	{
		return (left.intValue < right.intValue);
	}

	inline static void Swap(ParticleSort* oneOfThem, ParticleSort* theOtherOne)
	{
		ParticleSort temp = *oneOfThem;
		*oneOfThem = *theOtherOne;
		*theOtherOne = temp;
	}

	UInt32 index;
	int intValue;
};

void GenerateSortIndices(ParticleSort* indices, const Vector3f& distFactor, const ParticleSystemParticles& ps, ParticleSystemSortMode sortMode)
{
	const size_t particleCount = ps.array_size();
	if(IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
	{
		if(sortMode == kSSMByDistance)
			for(int i = 0; i < particleCount; i++)
				 ParticleSort::SetValues(indices[i], i, (int)(Dot (distFactor, ps.position[i]) * 40000.0f));
		else if(sortMode == kSSMOldestFirst)
			for(int i = 0; i < particleCount; i++)
				ParticleSort::SetValues(indices[i], i, (int)((ps.startLifetime[i]- ps.lifetime[i]) * -40000.0f));
		else if(sortMode == kSSMYoungestFirst)
			for(int i = 0; i < particleCount; i++)
				ParticleSort::SetValues(indices[i], i, (int)((ps.startLifetime[i]- ps.lifetime[i]) * 40000.0f));
	}
	else
	{
		// 3.5 used lifetime - this is pretty broken if you have random lifetimes, as you get random sorting
		if(sortMode == kSSMByDistance)
			for(int i = 0; i < particleCount; i++)
				ParticleSort::SetValues(indices[i], i, (int)(Dot (distFactor, ps.position[i]) * 40000.0f));
		else if(sortMode == kSSMOldestFirst)
			for(int i = 0; i < particleCount; i++)
			ParticleSort::SetValues(indices[i], i, (int)(ps.lifetime[i] * 40000.0f));
		else if(sortMode == kSSMYoungestFirst)
			for(int i = 0; i < particleCount; i++)
				ParticleSort::SetValues(indices[i], i, (int)(ps.lifetime[i] * -40000.0f));
	}
}

template<bool sortTempData>
void ApplySortRemap(ParticleSort* particleSortIndexBuffer, ParticleSystemParticlesTempData* tempData, ParticleSystemParticles& ps)
{
	const size_t count = ps.array_size();
	for(int i = 0; i < count; i++)
	{
		int dst = particleSortIndexBuffer[i].intValue;
		while(i != dst)
		{
			ParticleSort::Swap(&particleSortIndexBuffer[i], &particleSortIndexBuffer[dst]);
			ps.element_swap(i, dst);
			if(sortTempData)
				tempData->element_swap(i, dst);

			dst = particleSortIndexBuffer[i].intValue;
		}
	}
}

void Sort (const Matrix4x4f& matrix, ParticleSystemParticles& ps, ParticleSystemSortMode mode, ParticleSystemParticlesTempData* tempData, bool sortTempData)
{
	PROFILER_AUTO_GFX(gParticlesSort, 0);

	DebugAssert(mode != kSSMNone);

	const Vector3f distFactor = Vector3f (matrix.Get (2, 0), matrix.Get (2, 1), + matrix.Get (2, 2));
	const size_t count = ps.array_size();

	ParticleSort* particleSortIndexBuffer;
	ALLOC_TEMP(particleSortIndexBuffer, ParticleSort, count);
	GenerateSortIndices(&particleSortIndexBuffer[0], distFactor, ps, mode);

	// Sort
	std::sort(&particleSortIndexBuffer[0], &particleSortIndexBuffer[0] + count, ParticleSort::CompareValue);

	// Create inverse mapping
	for(int i = 0; i < count; i++)
		particleSortIndexBuffer[particleSortIndexBuffer[i].index].intValue = i;

	if(sortTempData)
		ApplySortRemap<true>(particleSortIndexBuffer, tempData, ps);
	else
		ApplySortRemap<false>(particleSortIndexBuffer, tempData, ps);
}

struct ParticleMeshData
{
	int vertexCount;
	StrideIterator<Vector3f> positions;
	StrideIterator<Vector3f> normals;
	StrideIterator<Vector4f> tangents;
	StrideIterator<ColorRGBA32> colors;
	StrideIterator<Vector2f> texCoords;
	int indexCount;
	const UInt16* indexBuffer;
};

template<bool hasNormals, bool hasTangents>
void TransformParticleMesh(const ParticleMeshData& src, ColorRGBA32 particleColor,
						   const Matrix4x4f& xform, const Matrix4x4f& xformNoScale, UInt8** dest)
{
	for(int vertex = 0; vertex < src.vertexCount; vertex++)
	{
		// Vertex format is position, color, uv, and optional normals and tangents
		xform.MultiplyPoint3(src.positions[vertex], *reinterpret_cast<Vector3f*>(*dest));
		*dest += sizeof(Vector3f);
		if (hasNormals)
		{
			xformNoScale.MultiplyVector3(src.normals[vertex], *reinterpret_cast<Vector3f*>(*dest));
			*dest += sizeof(Vector3f);
		}
		*reinterpret_cast<ColorRGBA32*>(*dest) = particleColor * src.colors[vertex];
		*dest += sizeof(ColorRGBA32);
		*reinterpret_cast<Vector2f*>(*dest) = src.texCoords[vertex];
		*dest += sizeof(Vector2f);
		// Tangent is last in vertex format
		if (hasTangents)
		{
			Vector3f newTangent = xformNoScale.MultiplyVector3((const Vector3f&)src.tangents[vertex]);
			*reinterpret_cast<Vector4f*>(*dest) = Vector4f(newTangent, src.tangents[vertex].w);
			*dest += sizeof(Vector4f);
		}
	}
}


ParticleSystemRenderer::ParticleSystemRenderer (MemLabelId label, ObjectCreationMode mode)
:	Super(kRendererParticleSystem, label, mode)
,	m_LocalSpaceAABB (Vector3f::zero, Vector3f::zero)
{
	SetVisible (false);

	for (int i = 0; i < ParticleSystemRendererData::kMaxNumParticleMeshes; ++i)
		m_Data.cachedMeshUserNode[i].SetData (this);

#if UNITY_EDITOR
	m_EditorEnabled = true;
#endif
}

ParticleSystemRenderer::~ParticleSystemRenderer ()
{
}

void ParticleSystemRenderer::InitializeClass ()
{
	REGISTER_MESSAGE_PTR (ParticleSystemRenderer, kDidDeleteMesh, OnDidDeleteMesh, Mesh);
}

void ParticleSystemRenderer::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	UpdateCachedMesh ();
}

void ParticleSystemRenderer::UpdateCachedMesh ()
{
	int dst = 0;
	for(int src = 0; src < ParticleSystemRendererData::kMaxNumParticleMeshes; src++)
	{
		m_Data.cachedMesh[src] = NULL;
		m_Data.cachedMeshUserNode[src].RemoveFromList ();

		Mesh* mesh = m_Mesh[src];
		if (mesh)
		{
			if (mesh->GetSubMeshCount() == 1)
			{
				m_Data.cachedMesh[dst] = mesh;
				const SubMesh& sm = mesh->GetSubMeshFast(0);
				const UInt16* buffer = mesh->GetSubMeshBuffer16(0);

				if (sm.topology == kPrimitiveTriangleStripDeprecated)
				{
					const int capacity = CountTrianglesInStrip(buffer, sm.indexCount) * 3;
					m_CachedIndexBuffer[dst].resize_uninitialized(capacity);
					Destripify(buffer, sm.indexCount, m_CachedIndexBuffer[dst].begin(), capacity);
				}
				else if (sm.topology == kPrimitiveTriangles)
				{
					const int capacity = sm.indexCount;
					m_CachedIndexBuffer[dst].resize_uninitialized(capacity);
					memcpy(m_CachedIndexBuffer[dst].begin(), buffer, capacity*kVBOIndexSize);
				}
				else
				{
					m_CachedIndexBuffer[dst].resize_uninitialized(0);
				}

				// Hook into mesh's user notifications.
				mesh->AddObjectUser (m_Data.cachedMeshUserNode[dst]);

				dst++;
			}
			else
			{
				m_Data.cachedMesh[src] = NULL;
				m_CachedIndexBuffer[src].resize_uninitialized(0);
				AssertString ("Particle system meshes will only work with exactly one (1) sub mesh");
			}
		}
	}
}

void ParticleSystemRenderer::OnDidDeleteMesh (Mesh* mesh)
{
	// Clear out cached pointer to mesh.
	for (int i = 0; i < ParticleSystemRendererData::kMaxNumParticleMeshes; ++i)
	{
		if (m_Data.cachedMesh[i] != mesh)
			continue;

		m_Data.cachedMesh[i] = NULL;
		m_Data.cachedMeshUserNode[i].RemoveFromList ();
	}
}

void ParticleSystemRenderer::GetLocalAABB (AABB& result)
{
	result = m_LocalSpaceAABB;
}

void ParticleSystemRenderer::GetWorldAABB (AABB& result)
{
	TransformAABB (m_LocalSpaceAABB, GetTransform ().GetPosition (), GetTransform ().GetRotation (), result);
}

float ParticleSystemRenderer::GetSortingFudge () const
{
	return m_Data.sortingFudge;
}

void ParticleSystemRenderer::CheckConsistency ()
{
	Super::CheckConsistency ();
	m_Data.maxParticleSize = std::max (0.0F, m_Data.maxParticleSize);
	m_Data.normalDirection = clamp<float>(m_Data.normalDirection, 0.0f, 1.0f);
}

void ParticleSystemRenderer::Reset ()
{
	Super::Reset ();
	m_Data.renderMode = kSRMBillboard;
	m_Data.lengthScale = 2.0F;
	m_Data.velocityScale = 0.0F;
	m_Data.cameraVelocityScale = 0.0F;
	m_Data.maxParticleSize = 0.5F;
	m_Data.sortingFudge = 0.0F;
	m_Data.sortMode = kSSMNone;
	m_Data.normalDirection = 1.0f;

	for(int i = 0; i < ParticleSystemRendererData::kMaxNumParticleMeshes; i++)
		m_Mesh[i] = NULL;
	m_LocalSpaceAABB.SetCenterAndExtent (Vector3f::zero, Vector3f::zero);



#if UNITY_EDITOR
	m_EditorEnabled = true;
#endif
}

void ParticleSystemRenderer::UpdateRenderer ()
{
	ParticleSystem* system = QueryComponent(ParticleSystem);
	if (system)
	{
		SetVisible (true);
		BoundsChanged();
	}
	else
	{
		UpdateManagerState (false);
	}

	Super::UpdateRenderer ();
}

void ParticleSystemRenderer::Update (const AABB& aabb)
{
	m_LocalSpaceAABB = aabb;
	UpdateManagerState (true);
}

void ParticleSystemRenderer::RendererBecameVisible()
{
	Super::RendererBecameVisible();

	ParticleSystem* system = QueryComponent(ParticleSystem);
	if(system)
		system->RendererBecameVisible();
}

void ParticleSystemRenderer::RendererBecameInvisible()
{
	Super::RendererBecameInvisible();

	ParticleSystem* system = QueryComponent(ParticleSystem);
	if(system)
		system->RendererBecameInvisible();
}

void ParticleSystemRenderer::UpdateLocalAABB()
{
	AABB aabb;
	GetLocalAABB(aabb);
	m_TransformInfo.localAABB = aabb;
}

inline Rectf GetFrameUV (int index, int tilesX, float animUScale, float animVScale)
{
	int vIdx = index / tilesX;
	int uIdx = index - vIdx * tilesX;	// slightly faster than index % m_UVAnimation.xTile
	float uOffset = (float)uIdx * animUScale;
	float vOffset = 1.0f - animVScale - (float)vIdx * animVScale;

	return Rectf(uOffset, vOffset, animUScale, animVScale);
}

template<ParticleSystemRenderMode renderMode>
void GenerateParticleGeometry (ParticleSystemVertex* vbPtr,
							   const ParticleSystemGeomConstInputData& constData,
							   const ParticleSystemRendererData& rendererData,
							   const ParticleSystemParticles& ps,
							   const ParticleSystemParticlesTempData& psTemp,
							   size_t startIndex,
							   size_t endIndex,
							   const Matrix4x4f& worldViewMatrix,
							   const Matrix4x4f& viewToWorldMatrix)
{
	float maxPlaneScale = constData.maxPlaneScale;
	float maxOrthoSize = constData.maxOrthoSize;
	float numUVFrame = constData.numUVFrame;
	Vector3f xSpan = constData.xSpan;
	Vector3f ySpan = constData.ySpan;
	Vector3f cameraVelocity = constData.m_CameraVelocity * rendererData.cameraVelocityScale;
	int numTilesX = constData.m_NumTilesX;
	float animUScale = constData.animUScale;
	float animVScale = constData.animVScale;
	bool usesSheetIndex = constData.usesSheetIndex;
	float lengthScale = rendererData.lengthScale;
	float velocityScale = rendererData.velocityScale;

	float bentNormalFactor = constData.bentNormalFactor;
	Vector3f bentNormalVector = constData.bentNormalVector;

	Vector2f uv[4] = {	Vector2f(0.0f, 1.0f),
						Vector2f(1.0f, 1.0f),
						Vector2f(1.0f, 0.0f),
						Vector2f(0.0f, 0.0f)};
	Vector4f uv2[4] = {	Vector4f(0.0f, 1.0f, 0.0f, 0.0f),
						Vector4f(1.0f, 1.0f, 0.0f, 0.0f),
						Vector4f(1.0f, 0.0f, 0.0f, 0.0f),
						Vector4f(0.0f, 0.0f, 0.0f, 0.0f)};

	float invAnimVScale = 1.0f - animVScale;

	for( int i = startIndex; i < endIndex; ++i )
	{
		Vector3f vert[4];
		Vector3f n0, n1;

		Vector3f position;
		worldViewMatrix.MultiplyPoint3 (ps.position[i], position);

		// Constrain the size to be a fraction of the viewport size.
		// v[0].z *  / farPlaneZ * farPlaneWorldSpaceLength * maxLength[0...1]
		// Also all valid z's are negative so we just negate the whole equation
		float maxWorldSpaceLength = position.z * maxPlaneScale + maxOrthoSize;
		float hsize = std::min (psTemp.size[i], maxWorldSpaceLength) * 0.5f;
		if (renderMode == kSRMBillboard)
		{
			float s = Sin (ps.rotation[i]);
			float c = Cos (ps.rotation[i]);
			n0 = Vector3f(-c+s,  s+c, 0.0f);
			n1 = Vector3f( c+s, -s+c, 0.0f);
			vert[0] = position + n0 * hsize;
			vert[1] = position + n1 * hsize;
			vert[2] = position - n0 * hsize;
			vert[3] = position - n1 * hsize;
		}
		else if (renderMode == kSRMBillboardFixedHorizontal || renderMode == kSRMBillboardFixedVertical)
		{
			float s = Sin (ps.rotation[i]+0.78539816339744830961566084581988f);
			float c = Cos (ps.rotation[i]+0.78539816339744830961566084581988f);
			n0 = xSpan*c + ySpan*s;
			n1 = ySpan*c - xSpan*s;
			vert[0] = position + n0 * hsize;
			vert[1] = position + n1 * hsize;
			vert[2] = position - n0 * hsize;
			vert[3] = position - n1 * hsize;
		}
		else if (renderMode == kSRMStretch3D)
		{
			//RH BUG FOR LATER: Here we see the stretching bug as described by case no 434115...this is a Flash VM error, where a writeFloat (or readFloat) fails.
			Vector3f velocity;
			worldViewMatrix.MultiplyVector3(ps.velocity[i] + ps.animatedVelocity[i], velocity);
			velocity -= cameraVelocity;
			float sqrVelocity = SqrMagnitude (velocity);

			Vector2f delta;
			Vector3f endProj;
			bool nonZeroVelocity = sqrVelocity > Vector3f::epsilon;
			if (nonZeroVelocity)
			{
				endProj = position - velocity * (velocityScale + FastInvSqrt (sqrVelocity) * (lengthScale * psTemp.size[i]));
				delta.x = position.z*endProj.y - position.y*endProj.z;
				delta.y = position.x*endProj.z - position.z*endProj.x;
				delta = NormalizeFast(delta);
			}
			else
			{
				endProj = position;
				delta = Vector2f::xAxis;
			}
			n0 = n1 = Vector3f(delta.x, delta.y, 0.0f);
			vert[0] = position + n0 * hsize;
			vert[1] = endProj  + n1 * hsize;
			vert[2] = endProj  - n0 * hsize;
			vert[3] = position - n1 * hsize;
		}

		// UV animation
		float sheetIndex;
		if(usesSheetIndex)
		{
			// TODO: Pretty much the perfect candidate for SIMD

			sheetIndex = psTemp.sheetIndex[i] * numUVFrame;
			Assert (psTemp.sheetIndex[i] >= 0.0f && psTemp.sheetIndex[i] <= 1.0f);

			const int index0 = FloorfToIntPos (sheetIndex);
			const int index1 = index0 + 1;
			Vector2f offset0, offset1;
			const float blend = sheetIndex - (float)index0;

			int vIdx = index0 / numTilesX;
			int uIdx = index0 - vIdx * numTilesX;
			offset0.x = (float)uIdx * animUScale;
			offset0.y = invAnimVScale - (float)vIdx * animVScale;

			vIdx = index1 / numTilesX;
			uIdx = index1 - vIdx * numTilesX;
			offset1.x = (float)uIdx * animUScale;
			offset1.y = invAnimVScale - (float)vIdx * animVScale;

			uv[0].Set(offset0.x,				offset0.y + animVScale );
			uv[1].Set(offset0.x + animUScale,	offset0.y + animVScale );
			uv[2].Set(offset0.x + animUScale,	offset0.y );
			uv[3].Set(offset0.x,				offset0.y );

			uv2[0].Set(offset1.x,				offset1.y + animVScale, blend, 0.0f );
			uv2[1].Set(offset1.x + animUScale,	offset1.y + animVScale, blend, 0.0f );
			uv2[2].Set(offset1.x + animUScale,	offset1.y,				blend, 0.0f );
			uv2[3].Set(offset1.x,				offset1.y,				blend, 0.0f );
		}

		n0 = viewToWorldMatrix.MultiplyVector3(n0 * bentNormalFactor);
		n1 = viewToWorldMatrix.MultiplyVector3(n1 * bentNormalFactor);

		ColorRGBA32 color = psTemp.color[i];

		vbPtr[0].vert = vert[0];
		vbPtr[0].normal = bentNormalVector + n0;
		vbPtr[0].color = color;
		vbPtr[0].uv = uv[0];
		vbPtr[0].tangent = uv2[0];

		vbPtr[1].vert = vert[1];
		vbPtr[1].normal = bentNormalVector + n1;
		vbPtr[1].color = color;
		vbPtr[1].uv = uv[1];
		vbPtr[1].tangent = uv2[1];

		vbPtr[2].vert = vert[2];
		vbPtr[2].normal = bentNormalVector - n0;
		vbPtr[2].color = color;
		vbPtr[2].uv = uv[2];
		vbPtr[2].tangent = uv2[2];

		vbPtr[3].vert = vert[3];
		vbPtr[3].normal = bentNormalVector - n1;
		vbPtr[3].color = color;
		vbPtr[3].uv = uv[3];
		vbPtr[3].tangent = uv2[3];

		// Next four vertices
		vbPtr += 4;
	}
}

static void DrawMeshParticles (const ParticleSystemGeomConstInputData& constInput, const ParticleSystemRendererData& rendererData, const Matrix4x4f& worldMatrix, const ParticleSystemParticles& ps, const ParticleSystemParticlesTempData& psTemp, const ChannelAssigns& channels)
{
	int numMeshes = 0;
	ParticleMeshData particleMeshes[ParticleSystemRendererData::kMaxNumParticleMeshes];
	Vector3f defaultNormal(0, 0, 0);
	Vector4f defaultTangent(0, 0, 0, 0);
	ColorRGBA32 defaultColor(255, 255, 255, 255);
	Vector2f defaultTexCoords(0, 0);
	for(int i = 0; i < ParticleSystemRendererData::kMaxNumParticleMeshes; i++)
	{
		if(constInput.m_MeshIndexCount[i] == 0)
			break;
		const Mesh* mesh = rendererData.cachedMesh[i];
		if(mesh == NULL || !mesh->HasVertexData())
			break;
		ParticleMeshData& dest = particleMeshes[i];
		dest.vertexCount = mesh->GetVertexCount();
		dest.positions = mesh->GetVertexBegin();
		dest.normals = mesh->GetNormalBegin();
		if (dest.normals.IsNull())
			dest.normals = StrideIterator<Vector3f>(&defaultNormal, 0);
		dest.tangents = mesh->GetTangentBegin();
		if (dest.tangents.IsNull())
			dest.tangents = StrideIterator<Vector4f>(&defaultTangent, 0);
		dest.texCoords = mesh->GetUvBegin();
		if (dest.texCoords.IsNull())
			dest.texCoords = StrideIterator<Vector2f>(&defaultTexCoords, 0);
		dest.colors = mesh->GetColorBegin();
		if (dest.colors.IsNull())
			dest.colors = StrideIterator<ColorRGBA32>(&defaultColor, 0);
		dest.indexCount = constInput.m_MeshIndexCount[i];
		dest.indexBuffer = constInput.m_MeshIndexBuffer[i];
		numMeshes++;
	}

	if(0 == numMeshes)
		return;

	GfxDevice& device = GetGfxDevice();

	Matrix4x4f viewMatrix;
	CopyMatrix (device.GetViewMatrix (), viewMatrix.GetPtr ());

	const size_t particleCount = ps.array_size ();

	float probability = 1.0f / (float)numMeshes;

	// @TODO: We should move all these platform dependent numbers into Gfx specific code and get it from there.
	const int kMaxVertices = 65536;

#if UNITY_WII
	const int kMaxIndices = 65536;
#else
	const int kMaxIndices = kDynamicBatchingIndicesThreshold;
#endif

	int particleOffset = 0;
	while (particleOffset < particleCount)
	{
		int numVertices = 0;
		int numIndices = 0;
		int particleCountBatch = 0;

		// Figure out batch size
		for(int i = particleOffset; i < particleCount; i++)
		{
			const float randomValue = GenerateRandom(ps.randomSeed[i] + kParticleSystemMeshSelectionId);
			int lastNumVertices = 0;
			int lastNumIndices = 0;
			for(int j = 0; j < numMeshes; j++)
			{
				const float lower = probability * j;
				const float upper = probability * (j + 1);
				if((randomValue >= lower) && (randomValue <= upper))
				{
					lastNumVertices = particleMeshes[j].vertexCount;
					lastNumIndices = particleMeshes[j].indexCount;
					break;
				}
			}
			if((numVertices >= kMaxVertices) || (numIndices >= kMaxIndices))
			{
				break;
			}
			else
			{
				numVertices += lastNumVertices;
				numIndices += lastNumIndices;
				particleCountBatch++;
			}
		}

		const int vertexCount = numVertices;
		const int indexCount = numIndices;

		// Figure out if normals and tangents are needed by shader
		UInt32 normalTangentMask = channels.GetSourceMap() & VERTEX_FORMAT2(Normal, Tangent);

		// Tangents requires normals
		if( normalTangentMask & VERTEX_FORMAT1(Tangent) )
			normalTangentMask |= VERTEX_FORMAT1(Normal);

		// Get VBO chunk
		DynamicVBO& vbo = device.GetDynamicVBO();
		UInt8* vbPtr = NULL;
		UInt16* ibPtr = NULL;
		const UInt32 mandatoryChannels = VERTEX_FORMAT3(Vertex, Color, TexCoord0);
		if( !vbo.GetChunk( mandatoryChannels | normalTangentMask,
			vertexCount, indexCount,
			DynamicVBO::kDrawIndexedTriangles,
			(void**)&vbPtr, (void**)&ibPtr ) )
		{
			return;
		}

		int vertexOffset = 0;
		int indexOffset = 0;
		const int startIndex = particleOffset;
		const int endIndex = particleOffset + particleCountBatch;
		for( int i = startIndex; i < endIndex; ++i )
		{
			const Vector3f position = ps.position[i];
			const float rotation = ps.rotation[i];
			const float size = psTemp.size[i];
			const Vector3f axisOfRotation = NormalizeSafe (ps.axisOfRotation[i], Vector3f::yAxis);
			const ColorRGBA32 particleColor = psTemp.color[i];

			// Only shared part is actually rotation. xformNoScale doesn't need a translation, so no need to copy that data
			Matrix4x4f xformNoScale;
			xformNoScale.SetTR (position, AxisAngleToQuaternion (axisOfRotation, rotation));

			Matrix4x4f xform = xformNoScale;
			ScaleMatrix(xform, size);

			// Figure out which mesh to use
			const float randomValue = GenerateRandom(ps.randomSeed[i] + kParticleSystemMeshSelectionId);
			int meshIndex = 0;
			for(int j = 0; j < numMeshes; j++)
			{
				const float lower = probability * j;
				const float upper = probability * (j + 1);
				if((randomValue >= lower) && (randomValue <= upper))
				{
					meshIndex = j;
					break;
				}
			}

			const ParticleMeshData& mesh = particleMeshes[meshIndex];

			// Fill up vbo here
			if( normalTangentMask == VERTEX_FORMAT2(Normal, Tangent) )
				TransformParticleMesh<true, true>(mesh, particleColor, xform, xformNoScale, &vbPtr);
			else if( normalTangentMask == VERTEX_FORMAT1(Normal) )
				TransformParticleMesh<true, false>(mesh, particleColor, xform, xformNoScale, &vbPtr);
			else if( normalTangentMask == 0 )
				TransformParticleMesh<false, false>(mesh, particleColor, xform, xformNoScale, &vbPtr);
			else
				ErrorString("Invalid normalTangentMask");

			const int meshIndexMax = mesh.indexCount - 2;
			for(int index = 0; index < meshIndexMax; index+=3)
			{
				ibPtr[index+0] = mesh.indexBuffer[index+0] + vertexOffset;
				ibPtr[index+1] = mesh.indexBuffer[index+1] + vertexOffset;
				ibPtr[index+2] = mesh.indexBuffer[index+2] + vertexOffset;
			}
			ibPtr += mesh.indexCount;

			vertexOffset += mesh.vertexCount;
			indexOffset += mesh.indexCount;
		}

		vbo.ReleaseChunk (vertexCount, indexCount);
		device.SetViewMatrix(viewMatrix.GetPtr());
		device.SetWorldMatrix(worldMatrix.GetPtr());
		vbo.DrawChunk (channels);
		GPU_TIMESTAMP();

		particleOffset += particleCountBatch;
	}
}

static void DrawParticlesInternal(const ParticleSystemGeomConstInputData& constData, const ParticleSystemRendererData& rendererData, const Matrix4x4f& worldViewMatrix, const Matrix4x4f& viewToWorldMatrix, const ParticleSystemParticles& ps, const ParticleSystemParticlesTempData& psTemp, ParticleSystemVertex* vbPtr, const size_t particleOffset, const size_t numParticles, int renderMode)
{
	const size_t endIndex = particleOffset + numParticles;

	if (renderMode == kSRMBillboard)
		GenerateParticleGeometry<kSRMBillboard> (vbPtr, constData, rendererData, ps, psTemp, particleOffset, endIndex, worldViewMatrix, viewToWorldMatrix);
	if (renderMode == kSRMStretch3D)
		GenerateParticleGeometry<kSRMStretch3D> (vbPtr, constData, rendererData, ps, psTemp, particleOffset, endIndex, worldViewMatrix, viewToWorldMatrix);
	if (renderMode == kSRMBillboardFixedHorizontal)
		GenerateParticleGeometry<kSRMBillboardFixedHorizontal> (vbPtr, constData, rendererData, ps, psTemp, particleOffset, endIndex, worldViewMatrix, viewToWorldMatrix);
	if (renderMode == kSRMBillboardFixedVertical)
		GenerateParticleGeometry<kSRMBillboardFixedVertical> (vbPtr, constData, rendererData, ps, psTemp, particleOffset, endIndex, worldViewMatrix, viewToWorldMatrix);
}

static void DrawParticles(const ParticleSystemGeomConstInputData& constData, const ParticleSystemRendererData& rendererData, const Matrix4x4f& worldViewMatrix, const Matrix4x4f& viewToWorldMatrix, const ParticleSystemParticles& ps, const ParticleSystemParticlesTempData& psTemp, const ChannelAssigns& channels, ParticleSystemVertex* vbPtr)
{
	GfxDevice& device = GetGfxDevice();
	const size_t particleCount = ps.array_size();

	if(vbPtr)
	{
		DrawParticlesInternal(constData, rendererData, worldViewMatrix, viewToWorldMatrix, ps, psTemp, vbPtr, 0, particleCount, rendererData.renderMode);
	}
	else
	{
		int particleOffset = 0;
		while (particleOffset < particleCount)
		{
			const int particleCountBatch = min(kMaxNumParticlesPerBatch, (int)particleCount - particleOffset);

			// Get VBO chunk
			DynamicVBO& vbo = device.GetDynamicVBO();
			if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor) | (1<<kShaderChannelTangent),
				particleCountBatch * 4, 0,
				DynamicVBO::kDrawQuads,
				(void**)&vbPtr, NULL ) )
			{
				continue;
			}

			DrawParticlesInternal(constData, rendererData, worldViewMatrix, viewToWorldMatrix, ps, psTemp, vbPtr, particleOffset, particleCountBatch, rendererData.renderMode);
			particleOffset += particleCountBatch;

			vbo.ReleaseChunk (particleCountBatch * 4, 0);

			// Draw
			device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity

			PROFILER_BEGIN(gSubmitVBOParticleProfile, constData.m_Renderer)
			vbo.DrawChunk (channels);
			GPU_TIMESTAMP();
			PROFILER_END

			device.SetViewMatrix(constData.m_ViewMatrix.GetPtr ());
		}
	}
}

void ParticleSystemRenderer::CalculateTotalParticleCount(UInt32& totalNumberOfParticles, ParticleSystem& system, bool first)
{
	ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
	if(!renderer || first)
	{
		totalNumberOfParticles += system.GetParticleCount();

		Transform* t = system.QueryComponent (Transform);
		if (t == NULL)
			return;
		for (Transform::iterator i=t->begin ();i != t->end ();i++)
		{
			ParticleSystem* child = (**i).QueryComponent(ParticleSystem);
			if (child)
				CalculateTotalParticleCount(totalNumberOfParticles, *child, false);
		}
	}
}

void ParticleSystemRenderer::CombineParticleBuffersRec(int& offset, ParticleSystemParticles& ps, ParticleSystemParticlesTempData& psTemp, ParticleSystem& system, bool first, bool needsAxisOfRotation)
{
	ParticleSystemRenderer* renderer = system.QueryComponent(ParticleSystemRenderer);
	if(!renderer || first)
	{
		int particleCount = system.GetParticleCount();
		ps.array_merge_preallocated(system.GetParticles(), offset, needsAxisOfRotation, false);

		if(system.m_ReadOnlyState->useLocalSpace)
		{
			Matrix4x4f localToWorld = system.GetComponent (Transform).GetLocalToWorldMatrixNoScale ();

			int endIndex = offset + particleCount;
			for(int i = offset; i < endIndex; i++)
				ps.position[i] = localToWorld.MultiplyPoint3(ps.position[i]);
			for(int i = offset; i < endIndex; i++)
				ps.velocity[i] = localToWorld.MultiplyVector3(ps.velocity[i]);
			for(int i = offset; i < endIndex; i++)
				ps.animatedVelocity[i] = localToWorld.MultiplyVector3(ps.animatedVelocity[i]);
			if(ps.usesAxisOfRotation)
				for(int i = offset; i < endIndex; i++)
					ps.axisOfRotation[i] = localToWorld.MultiplyVector3(ps.axisOfRotation[i]);
		}

		ParticleSystem::UpdateModulesNonIncremental(system, ps, psTemp, offset, offset + particleCount);
		offset += particleCount;

		Transform* t = system.QueryComponent (Transform);
		if (t == NULL)
			return;
		for (Transform::iterator i=t->begin ();i != t->end ();i++)
		{
			ParticleSystem* child = (**i).QueryComponent(ParticleSystem);
			if (child)
				CombineParticleBuffersRec(offset, ps, psTemp, *child, false, needsAxisOfRotation);
		}
	}
}

void ParticleSystemRenderer::SetUsesAxisOfRotationRec(ParticleSystem& shuriken, bool first)
{
	ParticleSystemRenderer* renderer = shuriken.QueryComponent(ParticleSystemRenderer);
	if(!renderer || first)
	{
		shuriken.SetUsesAxisOfRotation();

		Transform* t = shuriken.QueryComponent (Transform);
		if (t == NULL)
			return;
		for (Transform::iterator i=t->begin ();i != t->end ();i++)
		{
			ParticleSystem* shuriken = (**i).QueryComponent(ParticleSystem);
			if (shuriken)
				SetUsesAxisOfRotationRec(*shuriken, false);
		}
	}
}

void ParticleSystemRenderer::CombineBoundsRec(ParticleSystem& shuriken, MinMaxAABB& aabb, bool first)
{
	ParticleSystemRenderer* renderer = shuriken.QueryComponent(ParticleSystemRenderer);
	if(!renderer || first)
	{
		AABB result = shuriken.m_State->minMaxAABB;
		if(!shuriken.m_ReadOnlyState->useLocalSpace)
			InverseTransformAABB (result, renderer->GetTransform().GetPosition (), renderer->GetTransform().GetRotation (), result);

		if(first)
			aabb = result;
		else
			aabb.Encapsulate(result);

		Transform* t = shuriken.QueryComponent (Transform);
		if (t == NULL)
			return;
		for (Transform::iterator i=t->begin ();i != t->end ();i++)
		{
			ParticleSystem* shuriken = (**i).QueryComponent(ParticleSystem);
			if (shuriken)
				CombineBoundsRec(*shuriken, aabb, false);
		}
	}
}

void ParticleSystemRenderer::Render (int/* materialIndex*/, const ChannelAssigns& channels)
{
	ParticleSystem::SyncJobs();

	ParticleSystem* system = QueryComponent(ParticleSystem);
	if(!system)
		return;

	// Can't render without an active camera (case 568930)
	// Can remove check when we finally kill Renderer.Render()
	if (!GetCurrentCameraPtr())
		return;

	PROFILER_AUTO_GFX(gParticlesSingleProfile, this);

	UInt32 numParticles = 0;
	CalculateTotalParticleCount(numParticles, *system, true);
	if(numParticles)
		RenderInternal(*system, *this, channels, 0, numParticles);
}

void ParticleSystemRenderer::RenderMultiple (const BatchInstanceData* instances, size_t count, const ChannelAssigns& channels)
{
	ParticleSystem::SyncJobs();

	size_t numParticlesBatch = 0;

	BatchInstanceData const* instancesEnd = instances + count;
	BatchInstanceData const* iBatchBegin = instances;
	BatchInstanceData const* iBatchEnd = instances;
	while(iBatchEnd != instancesEnd)
	{
		Assert(iBatchEnd->renderer->GetRendererType() == kRendererParticleSystem);
		ParticleSystemRenderer* psRenderer = (ParticleSystemRenderer*)iBatchEnd->renderer;
		Assert(psRenderer->GetRenderMode() != kSRMMesh);
		ParticleSystem* system = psRenderer->QueryComponent(ParticleSystem);
		if (!system )
		{
			iBatchEnd++;
			continue;
		}
		UInt32 numParticles = 0;
		psRenderer->CalculateTotalParticleCount(numParticles, *system, true);

		if((numParticlesBatch + numParticles) <= kMaxNumParticlesPerBatch)
		{
			numParticlesBatch += numParticles;
			iBatchEnd++;
		}
		else
		{
			if(numParticlesBatch)
			{
				RenderBatch(iBatchBegin, iBatchEnd - iBatchBegin, numParticlesBatch, channels);
				numParticlesBatch = 0;
				iBatchBegin = iBatchEnd;
			}
			else // Can't fit in one draw call
			{
				RenderBatch(iBatchEnd, 1, numParticles, channels);
				iBatchEnd++;
				iBatchBegin = iBatchEnd;
			}
		}
	}

	if((iBatchBegin != iBatchEnd) && numParticlesBatch)
		RenderBatch(iBatchBegin, iBatchEnd - iBatchBegin, numParticlesBatch, channels);
}

void ParticleSystemRenderer::RenderBatch (const BatchInstanceData* instances, size_t count, size_t numParticles, const ChannelAssigns& channels)
{
	DebugAssert(numParticles);

	GfxDevice& device = GetGfxDevice();

	const MaterialPropertyBlock* customProps = count > 0 ? instances[0].renderer->GetCustomProperties() : NULL;
	if (customProps)
		device.SetMaterialProperties (*customProps);

	Matrix4x4f viewMatrix;
	CopyMatrix (device.GetViewMatrix (), viewMatrix.GetPtr ());

	ParticleSystemVertex* vbPtr = 0;
	DynamicVBO& vbo = device.GetDynamicVBO();
	if(numParticles <= kMaxNumParticlesPerBatch)
	{
		if( !vbo.GetChunk( (1<<kShaderChannelVertex) | (1<<kShaderChannelNormal) | (1<<kShaderChannelTexCoord0) | (1<<kShaderChannelColor) | (1<<kShaderChannelTangent),
			numParticles * 4, 0,
			DynamicVBO::kDrawQuads,
			(void**)&vbPtr, NULL ) )
		{
			return;
		}
	}

	PROFILER_AUTO_GFX(gParticlesBatchProfile, 0);

	// Allocate VBO if count is not above threshold. Else just pass null down
	BatchInstanceData const* iBatchBegin = instances;
	BatchInstanceData const* instancesEnd = instances + count;
	size_t particleOffset = 0;
	while (iBatchBegin != instancesEnd)
	{
		Assert(iBatchBegin->renderer->GetRendererType() == kRendererParticleSystem);
		ParticleSystemRenderer* psRenderer = (ParticleSystemRenderer*)iBatchBegin->renderer;
		Assert(psRenderer->GetRenderMode() != kSRMMesh);
		ParticleSystem* system = psRenderer->QueryComponent(ParticleSystem);
		UInt32 particleCountTotal = 0;
		if (system)
		{
			// It would be nice to filter out NULL particle systems earlier, but we don't (case 504744)
			CalculateTotalParticleCount(particleCountTotal, *system, true);
			if(particleCountTotal)
				RenderInternal(*system, *psRenderer, channels, vbPtr + particleOffset * 4, particleCountTotal);
		}
		iBatchBegin++;
		particleOffset += particleCountTotal;
	}

	if(vbPtr)
	{
		vbo.ReleaseChunk (numParticles * 4, 0);

		// Draw
		device.SetViewMatrix (Matrix4x4f::identity.GetPtr()); // implicitly sets world to identity

		PROFILER_BEGIN(gSubmitVBOParticleProfile, 0)
		vbo.DrawChunk (channels);
		GPU_TIMESTAMP();
		PROFILER_END

		if (count > 1)
			device.AddBatchingStats(numParticles * 2, numParticles * 4, count);

		device.SetViewMatrix(viewMatrix.GetPtr());
	}
}

void ParticleSystemRenderer::RenderInternal (ParticleSystem& system, const ParticleSystemRenderer& renderer, const ChannelAssigns& channels, ParticleSystemVertex* vbPtr, UInt32 particleCountTotal)
{
	Assert(particleCountTotal);

#if UNITY_EDITOR
	if (!renderer.m_EditorEnabled)
		return;
#endif

	GfxDevice& device = GetGfxDevice();

	// Render matrix
	Matrix4x4f viewMatrix;
	CopyMatrix (device.GetViewMatrix (), viewMatrix.GetPtr ());

	ParticleSystemParticles* ps = &system.GetParticles ();
	size_t particleCount = ps->array_size ();
	AssertBreak(particleCountTotal >= particleCount);

	UInt8* combineBuffer = 0;
	ParticleSystemParticles combineParticles;
	if(particleCountTotal > particleCount)
	{
		particleCount = particleCountTotal;
		combineBuffer = ALLOC_TEMP_MANUAL(UInt8, particleCountTotal * ParticleSystemParticles::GetParticleSize());
		combineParticles.array_assign_external((void*)&combineBuffer[0], particleCountTotal);
		ps = &combineParticles;
	}

	if(!particleCount)
		return;

	const bool needsAxisOfRotation = !renderer.GetScreenSpaceRotation();
	if(needsAxisOfRotation)
	{
		if ( IS_CONTENT_NEWER_OR_SAME (GetNumericVersion ("4.0.0f7")) && !IS_CONTENT_NEWER_OR_SAME (GetNumericVersion ("4.1.1a1")) )
		{
			// this was introduced in 4.0 and is wrong, it will effectively force the rotation axis to be the y-axis for all particles, the function is meant only
			// to initialize the axis array (once) and should only be called from SetUsesAxisOfRotationRec
			ps->SetUsesAxisOfRotation ();
		}
		else
		{
			// this is the intended functionality, but was broken for 4.0 and 4.0.1 see above
			SetUsesAxisOfRotationRec (system, true);
		}
	}

	ParticleSystemSortMode sortMode = (ParticleSystemSortMode)renderer.m_Data.sortMode;
	bool isInLocalSpace = system.m_ReadOnlyState->useLocalSpace && !combineBuffer;
	Matrix4x4f worldMatrix = Matrix4x4f::identity;
	if(isInLocalSpace)
		worldMatrix = system.GetComponent (Transform).GetLocalToWorldMatrixNoScale ();

	ParticleSystemParticlesTempData psTemp;
	psTemp.color = ALLOC_TEMP_MANUAL(ColorRGBA32, particleCount);
	psTemp.size = ALLOC_TEMP_MANUAL(float, particleCount);
	psTemp.sheetIndex = 0;
	psTemp.particleCount = particleCount;
	if(combineBuffer)
	{
		int offset = 0;
		CombineParticleBuffersRec(offset, *ps, psTemp, system, true, needsAxisOfRotation);
		if (kSSMNone != sortMode)
			Sort(viewMatrix, *ps, sortMode, &psTemp, true);
	}
	else
	{
		if(system.m_UVModule->GetEnabled())
			psTemp.sheetIndex = ALLOC_TEMP_MANUAL(float, particleCount);

		if (kSSMNone != sortMode)
		{
			Matrix4x4f objectToViewMatrix;
			MultiplyMatrices3x4(viewMatrix, worldMatrix, objectToViewMatrix);
			Sort(objectToViewMatrix, *ps, sortMode, 0, false);
		}
		ParticleSystem::UpdateModulesNonIncremental(system, *ps, psTemp, 0, particleCount);
	}

	// Constrain the size to be a fraction of the viewport size.
	// In perspective case, max size is (z*factorA). In ortho case, max size is just factorB. To have both
	// without branches, we do (z*factorA+factorB) and set one of factors to zero.
	float maxPlaneScale = 0.0f;
	float maxOrthoSize = 0.0f;
	// Getting the camera isn't totally free, so do it once.
	const Camera& camera = GetCurrentCamera();
	if (!camera.GetOrthographic())
		maxPlaneScale = -camera.CalculateFarPlaneWorldSpaceLength() * renderer.m_Data.maxParticleSize / camera.GetFar();
	else
		maxOrthoSize = camera.CalculateFarPlaneWorldSpaceLength() * renderer.m_Data.maxParticleSize;

	int numMeshes = 0;
	for(int i = 0; i < ParticleSystemRendererData::kMaxNumParticleMeshes; i++)
		if(renderer.m_Data.cachedMesh[i])
			numMeshes++;

	ParticleSystemGeomConstInputData constData;
	constData.m_ViewMatrix = viewMatrix;
	constData.m_CameraVelocity = viewMatrix.MultiplyVector3(camera.GetVelocity ());
	constData.m_Renderer = (Object*)&renderer;
	for(int i = 0; i < numMeshes; i++)
	{
		constData.m_MeshIndexBuffer[i] = renderer.m_CachedIndexBuffer[i].begin();
		constData.m_MeshIndexCount[i] = renderer.m_CachedIndexBuffer[i].size();
		AssertBreak((constData.m_MeshIndexCount[i] % 3) == 0);
	}

	system.GetNumTiles(constData.m_NumTilesX, constData.m_NumTilesY);
	constData.maxPlaneScale = maxPlaneScale;
	constData.maxOrthoSize = maxOrthoSize;
	constData.numUVFrame = constData.m_NumTilesX * constData.m_NumTilesY;
	constData.animUScale = 1.0f / (float)constData.m_NumTilesX;
	constData.animVScale = 1.0f / (float)constData.m_NumTilesY;
	constData.xSpan = Vector3f(-1.0f,0.0f,0.0f);
	constData.ySpan = Vector3f(0.0f,0.0f,1.0f);
	if (renderer.m_Data.renderMode == kSRMBillboardFixedVertical)
	{
		constData.ySpan = Vector3f(0.0f,1.0f,0.0f);
		const Vector3f zSpan = viewMatrix.MultiplyVector3 (Vector3f::zAxis);// (RotateVectorByQuat (cameraRotation, Vector3f(0.0f,0.0f,1.0f));
		constData.xSpan = NormalizeSafe (Cross (constData.ySpan, zSpan));
	}
	constData.xSpan = viewMatrix.MultiplyVector3(constData.xSpan);
	constData.ySpan = viewMatrix.MultiplyVector3(constData.ySpan);
	constData.usesSheetIndex = psTemp.sheetIndex != NULL;

	const float bentNormalAngle = renderer.m_Data.normalDirection * 90.0f * kDeg2Rad;
	const float scale = (renderer.m_Data.renderMode == kSRMBillboard) ? 0.707106781f : 1.0f;

	Matrix4x4f viewToWorldMatrix;
	Matrix4x4f::Invert_General3D(viewMatrix, viewToWorldMatrix);

	Matrix4x4f worldViewMatrix;
	MultiplyMatrices4x4(&viewMatrix, &worldMatrix, &worldViewMatrix);

	Vector3f billboardNormal = Vector3f::zAxis;
	if((renderer.m_Data.renderMode == kSRMBillboardFixedHorizontal) || (renderer.m_Data.renderMode == kSRMBillboardFixedVertical))
		billboardNormal = viewMatrix.MultiplyVector3 (NormalizeSafe (Cross (constData.xSpan, constData.ySpan)));
	constData.bentNormalVector = viewToWorldMatrix.MultiplyVector3(Sin(bentNormalAngle) * billboardNormal);
	constData.bentNormalFactor = Cos(bentNormalAngle) * scale;

	if (renderer.m_Data.renderMode == kSRMMesh)
		DrawMeshParticles (constData, renderer.m_Data, worldMatrix, *ps, psTemp, channels);
	else
		DrawParticles(constData, renderer.m_Data, worldViewMatrix, viewToWorldMatrix, *ps, psTemp, channels, vbPtr);

	FREE_TEMP_MANUAL(psTemp.color);
	FREE_TEMP_MANUAL(psTemp.size);
	if(psTemp.sheetIndex)
		FREE_TEMP_MANUAL(psTemp.sheetIndex);
	if(combineBuffer)
		FREE_TEMP_MANUAL(combineBuffer);
}

template<class TransferFunction> inline
void ParticleSystemRenderer::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_Data.renderMode, "m_RenderMode");
	transfer.Transfer (m_Data.maxParticleSize, "m_MaxParticleSize");
	transfer.Transfer (m_Data.cameraVelocityScale, "m_CameraVelocityScale");
	transfer.Transfer (m_Data.velocityScale, "m_VelocityScale");
	transfer.Transfer (m_Data.lengthScale, "m_LengthScale");
	transfer.Transfer (m_Data.sortingFudge, "m_SortingFudge");
	transfer.Transfer (m_Data.normalDirection, "m_NormalDirection");
	transfer.Transfer (m_Data.sortMode, "m_SortMode");
	transfer.Transfer (m_Mesh[0], "m_Mesh");
	transfer.Transfer (m_Mesh[1], "m_Mesh1");
	transfer.Transfer (m_Mesh[2], "m_Mesh2");
	transfer.Transfer (m_Mesh[3], "m_Mesh3");
}
