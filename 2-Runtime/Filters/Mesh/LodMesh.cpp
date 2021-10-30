#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "LodMesh.h"
#include "Runtime/Utilities/vector_utility.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Graphics/TriStripper.h"
#include "MeshUtility.h"
#include "Runtime/Geometry/TangentSpaceCalculation.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Shaders/VBO.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/SwapEndianArray.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Camera/IntermediateRenderer.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Profiler/Profiler.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Utilities/UniqueIDGenerator.h"
#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/GfxDevice/GfxXenonVBO.h"
#endif
#include "Runtime/GfxDevice/GfxDeviceConfigure.h"

#if UNITY_FLASH
#include <limits.h>
#define FLT_MAX __FLT_MAX__
#define FLT_MIN __FLT_MIN__
#endif

#if UNITY_EDITOR
#	include "Editor/Src/BuildPipeline/PrepareMeshDataForBuildTarget.h"
#	include "Runtime/Camera/RenderLoops/RenderLoopPrivate.h"
#	include "Runtime/Misc/Player.h"
#endif


///* Checkbox in mesh importer that allows you have mesh access (Done)
///* Default for new importers is to have mesh access enabled (done)
///* Error Messages when acessing data although you shouldn't be allowed (--)
///* MeshColliders / SkinnedMeshes / non-uniform scale. Forces meshes to be non-readable. (Done)


///* MeshCollider with no-access allowed. Does it work / no errors
///* MeshCollider with no-access allowed, mesh is assigned from script. Does it give an error in editor & player
///* MeshCollider with no-access allowed, mesh is scaled at runtime does it give an error
///* MeshCollider with no-access allowed, mesh is scaled in scene. Does it work without errors.
///* Mesh data accessed from script, does it give an error.



static char const* kMeshAPIErrorMessage =
"Mesh.%s is out of bounds. The supplied array needs to be the same size as the Mesh.vertices array.";


static UniqueIDGenerator s_MeshIDGenerator;


// The Mesh class contains one of these for every Material that is bound to it.
struct DeprecatedMeshData
{
	std::vector<Face> faces;				// Indices for specific faces
	std::vector <unsigned short> strips;	// A list of triangle strips
	int triangleCount;
	DECLARE_SERIALIZE_NO_PPTR (MeshData)
};

template<class TransferFunc>
void DeprecatedMeshData::Transfer (TransferFunc& transfer)
{
	TRANSFER (faces);
	TRANSFER (strips);
	TRANSFER(triangleCount);
}

struct DeprecatedLOD
{
	vector<DeprecatedMeshData>	m_MeshData;

	DECLARE_SERIALIZE (LOD)
};

template<class TransferFunction>
void DeprecatedLOD::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_MeshData);
}

static void LoadDeprecatedMeshData (Mesh& mesh, vector<DeprecatedLOD> &lods)
{
	mesh.GetIndexBuffer().clear();
	mesh.GetSubMeshes().clear();

	if (lods.empty())
		return;

	DeprecatedLOD& lod = lods.front();

	mesh.SetSubMeshCount(lod.m_MeshData.size());
	for (int i=0;i<lod.m_MeshData.size();i++)
	{
		DeprecatedMeshData& oldMeshData = lod.m_MeshData[i];
		if (oldMeshData.faces.size())
			mesh.SetIndicesComplex (&oldMeshData.faces[0].v1, oldMeshData.faces.size()*3, i, kPrimitiveTriangles, Mesh::k16BitIndices);
		else
		{
			UNITY_TEMP_VECTOR(UInt16) triangles;
			Destripify(&oldMeshData.strips[0], oldMeshData.strips.size(), triangles);
			mesh.SetIndicesComplex (&triangles[0], triangles.size(), i, kPrimitiveTriangles, Mesh::k16BitIndices);
		}
	}
}


using namespace std;

Mesh::Mesh (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_ChannelsInVBO(0)
,	m_VerticesDirty(true)
,	m_IndicesDirty(true)
,	m_IsDynamic(false)
,	m_HideFromRuntimeStats(false)
,	m_VertexColorsSwizzled(false)
,	m_MeshUsageFlags(0)
,	m_LocalAABB(Vector3f::zero, Vector3f::zero)
,	m_VBO(NULL)
,   m_InternalMeshID (0)
,   m_Skin (label)
,	m_CachedSkin2 (label)
,	m_CachedSkin1 (label)
,	m_CachedBonesAABB(label)
,	m_Bindpose(label)
,	m_BonePathHashes(label)
,	m_RootBonePathHash(0)
{
	m_MaxBoneIndex = -1;
	SubMesh sub;
	m_SubMeshes.push_back(sub);

	m_MeshCompression = kMeshCompressionOff;
	m_StreamCompression = kStreamCompressionDefault;
	m_IsReadable = true;
	m_KeepVertices = false;
	m_KeepIndices = false;

#if UNITY_EDITOR
	m_MeshOptimized = false;
#endif

#if ENABLE_MULTITHREADED_CODE
	m_CurrentCPUFence = 0;
	m_WaitOnCPUFence = false;
#endif

	m_InternalMeshID = 0;
}

Mesh::~Mesh ()
{
	MainThreadCleanup ();
}

bool Mesh::MainThreadCleanup ()
{
	WaitOnRenderThreadUse();
	NotifyObjectUsers( kDidDeleteMesh );
	m_IntermediateUsers.Notify( kImNotifyAssetDeleted );

	m_CollisionMesh.Cleanup();

	if (m_VBO)
	{
		GetGfxDevice().DeleteVBO(m_VBO);
		m_VBO = NULL;
	}

	if (m_InternalMeshID != 0)
	{
		s_MeshIDGenerator.RemoveID (m_InternalMeshID);
		m_InternalMeshID = 0;
	}

	return true;
}

void Mesh::LoadDeprecatedTangentData (Mesh& mesh, DeprecatedTangentsArray &inTangents)
{
	int count = inTangents.size();
	unsigned needChannels = m_VertexData.GetChannelMask () | VERTEX_FORMAT2(Normal, Tangent);
	if (count != GetVertexCount () || m_VertexData.GetChannelMask () != needChannels)
		ResizeVertices (count, needChannels);

	Assert (GetVertexCount () == count);

	StrideIterator<Vector3f> normals = GetNormalBegin ();
	StrideIterator<Vector4f> tangents = GetTangentBegin ();

	for(int i=0;i<count; ++i, ++normals, ++tangents)
	{
		*normals = inTangents[i].normal;
		*tangents = Vector4f(inTangents[i].tangent.x,inTangents[i].tangent.y,inTangents[i].tangent.z,inTangents[i].handedness);
	}
}

void Mesh::SwizzleVertexColorsIfNeeded ()
{
	// Early out if color are already in the right format
	if (gGraphicsCaps.needsToSwizzleVertexColors == m_VertexColorsSwizzled)
		return;

	// Due to runtime GfxDevice switching we might need to unswizzle vertex colors (case 562695)
	if (m_VertexColorsSwizzled)
	{
		std::transform(GetColorBegin(), GetColorEnd(), GetColorBegin(), UnswizzleColorForPlatform);
		m_VertexColorsSwizzled = false;
	}
	else
	{
		std::transform(GetColorBegin(), GetColorEnd(), GetColorBegin(), SwizzleColorForPlatform);
		m_VertexColorsSwizzled = true;
	}
}

void Mesh::ExtractVertexArray (Vector3f* destination) const
{
	StrideIterator<Vector3f> v = GetVertexBegin ();
	for (Vector3f* end = destination + GetVertexCount(); destination != end; ++v, ++destination)
		*destination = *v;
}

void Mesh::ExtractNormalArray (Vector3f* destination) const
{
	StrideIterator<Vector3f> n = GetNormalBegin ();
	for (Vector3f* end = destination + GetVertexCount(); destination != end; ++n, ++destination)
		*destination = *n;
}

void Mesh::ExtractColorArray (ColorRGBA32* destination) const
{
	if (m_VertexColorsSwizzled)
		std::transform(GetColorBegin(), GetColorEnd(), destination, UnswizzleColorForPlatform);
	else
		std::copy(GetColorBegin(), GetColorEnd(), destination);
}

void Mesh::ExtractColorArrayConverting (ColorRGBAf* destination) const
{
	if (m_VertexColorsSwizzled)
		std::transform(GetColorBegin(), GetColorEnd(), destination, UnswizzleColorForPlatform);
	else
		std::copy(GetColorBegin(), GetColorEnd(), destination);
}

void Mesh::ExtractUvArray (int uvIndex, Vector2f* destination) const
{
	StrideIterator<Vector2f> uv = GetUvBegin (uvIndex);
	for (Vector2f* end = destination + GetVertexCount(); destination != end; ++uv, ++destination)
		*destination = *uv;
}

void Mesh::ExtractTangentArray (Vector4f* destination) const
{
	StrideIterator<Vector4f> t = GetTangentBegin ();
	for (Vector4f* end = destination + GetVertexCount(); destination != end; ++t, ++destination)
		*destination = *t;
}


UInt32 Mesh::ResizeVertices (size_t count, UInt32 shaderChannels, const VertexStreamsLayout& streams, const VertexChannelsLayout& channels)
{
	Assert (count <= std::numeric_limits<UInt16>::max());

	UInt32 prevChannels = m_VertexData.GetChannelMask();

	if (m_VertexData.GetVertexCount() != count ||
		m_VertexData.GetChannelMask() != shaderChannels ||
		!m_VertexData.ConformsToStreamsLayout(streams) ||
		!m_VertexData.ConformsToChannelsLayout(channels))
	{
		WaitOnRenderThreadUse();

		SET_ALLOC_OWNER(this);
		m_VertexData.Resize(count, shaderChannels, streams, channels);

		if (!m_Skin.empty ())
			m_Skin.resize_initialized (count, BoneInfluence());
	}

	return m_VertexData.GetChannelMask() & ~prevChannels;
}


UInt32 Mesh::FormatVertices (UInt32 shaderChannels)
{
	return ResizeVertices(GetVertexCount(), shaderChannels);
}

void Mesh::InitChannelsToDefault (unsigned begin, unsigned count, unsigned shaderChannels)
{
	if (shaderChannels & VERTEX_FORMAT1(Vertex))
		std::fill (GetVertexBegin () + begin, GetVertexBegin () + begin + count, Vector3f (0,0,0));
	if (shaderChannels & VERTEX_FORMAT1(Normal))
		std::fill (GetNormalBegin () + begin, GetNormalBegin () + begin + count, Vector3f (0,0,0));
	if (shaderChannels & VERTEX_FORMAT1(Color))
		std::fill (GetColorBegin () + begin, GetColorBegin () + begin + count, ColorRGBA32 (0xffffffff));
	if (shaderChannels & VERTEX_FORMAT1(TexCoord0))
		std::fill (GetUvBegin (0) + begin, GetUvBegin (0) + begin + count, Vector2f (0,0));
	if (shaderChannels & VERTEX_FORMAT1(Tangent))
		std::fill (GetTangentBegin () + begin, GetTangentBegin () + begin + count, Vector4f (0,0,0,0));

	if (shaderChannels & VERTEX_FORMAT1(TexCoord1))
	{
		if( GetAvailableChannels () & VERTEX_FORMAT1(TexCoord0) )
			std::copy (GetUvBegin (0) + begin, GetUvBegin (0) + begin + count, GetUvBegin (1) + begin);
		else
			std::fill (GetUvBegin (1) + begin, GetUvBegin (1) + begin + count, Vector2f (0,0));
	}
}

namespace
{
	bool IsStripValid(const Mesh::TemporaryIndexContainer& triangles, const Mesh::TemporaryIndexContainer& newStrip)
	{
		int invalidTriangleCount = 0;
		for (int j = 0; j < triangles.size(); j += 3)
		{
			int i0 = triangles[j + 0];
			int i1 = triangles[j + 1];
			int i2 = triangles[j + 2];

			bool found = false;
			for (int k = 0; k < newStrip.size() - 2; ++k)
			{
				int s0 = newStrip[k + 0];
				int s1 = newStrip[k + 1];
				int s2 = newStrip[k + 2];

				if (k&1)
					std::swap(s1, s2);

				if ((s0 == i0 && s1 == i1 && s2 == i2) ||
					(s0 == i1 && s1 == i2 && s2 == i0) ||
					(s0 == i2 && s1 == i0 && s2 == i1))
				{
					found = true;
					break;
				}
			}

			if (!found)
				++invalidTriangleCount;
		}

		AssertMsg(invalidTriangleCount == 0, "Mesh strip is missing %d triangles", invalidTriangleCount);
		return invalidTriangleCount == 0;
	}
}

void Mesh::RecalculateBoundsInternal ()
{
	MinMaxAABB minmax;
	minmax.Init ();
	for (StrideIterator<Vector3f> it = GetVertexBegin (), end = GetVertexEnd (); it != end; ++it)
		minmax.Encapsulate (*it);

	// Apply all blendshape targets to bounding volumes
	if (!m_Shapes.vertices.empty())
	{
		StrideIterator<Vector3f> verts = GetVertexBegin ();

		for (int i=0;i<m_Shapes.vertices.size();i++)
		{
			Vector3f pos = verts[m_Shapes.vertices[i].index] + m_Shapes.vertices[i].vertex;
			minmax.Encapsulate (pos);
		}
	}

	AABB aabb;
	if (GetVertexCount ())
		aabb = minmax;
	else
		aabb = AABB (Vector3f::zero, Vector3f::zero);

	m_LocalAABB = aabb;

	for (int submesh = 0; submesh < m_SubMeshes.size(); ++submesh)
		RecalculateSubmeshBoundsInternal (submesh);
}

void Mesh::RecalculateSubmeshBoundsInternal (unsigned submesh)
{
	MinMaxAABB minmax;
	minmax.Init ();

		const UInt16* indices = GetSubMeshBuffer16(submesh);
		StrideIterator<Vector3f> vertices = GetVertexBegin ();
		for (unsigned int i = 0; i < GetSubMeshFast(submesh).indexCount; i++)
			minmax.Encapsulate (vertices[indices[i]]);

	AABB aabb;
	if (GetSubMeshFast(submesh).indexCount > 0)
		aabb = minmax;
	else
		aabb = AABB (Vector3f::zero, Vector3f::zero);

	GetSubMeshFast(submesh).localAABB = aabb;
}


void Mesh::RecalculateBounds ()
{
	RecalculateBoundsInternal ();

	SetDirty();
	NotifyObjectUsers( kDidModifyBounds );
	m_IntermediateUsers.Notify( kImNotifyBoundsChanged );
}

void Mesh::RecalculateSubmeshBounds (unsigned submesh)
{
	RecalculateSubmeshBoundsInternal (submesh);

	SetDirty();
	NotifyObjectUsers( kDidModifyBounds );
	m_IntermediateUsers.Notify( kImNotifyBoundsChanged );
}


void Mesh::Clear (bool keepVertexLayout)
{
	WaitOnRenderThreadUse();

	m_SubMeshes.clear();
	SubMesh sub;
	m_SubMeshes.push_back(sub);

	ClearBlendShapes (m_Shapes);

	m_IndexBuffer.clear();
#if UNITY_EDITOR
	m_MeshOptimized = false;
#endif

#if UNITY_PS3 || UNITY_EDITOR
	m_PartitionInfos.clear();
	m_Partitions.clear();
#endif

	unsigned prevFormat = m_VertexData.GetChannelMask();

	if (m_VertexData.GetVertexCount() > 0)
	{
		// keepVertexLayout added in Unity 3.5.3; keep previous behaviour
		// for older content for safety.
		if (keepVertexLayout && IS_CONTENT_NEWER_OR_SAME (kUnityVersion3_5_3_a1))
		{
			ResizeVertices (0, prevFormat);
		}
		else
		{
			VertexData tempVD;
			swap (tempVD, m_VertexData);
		}
	}

	if (!m_Skin.empty())
	{
		m_Skin.clear();
	}

	m_VertexColorsSwizzled = false;
	ClearSkinCache();

	SetChannelsDirty( prevFormat, true );
}

IMPLEMENT_CLASS (Mesh)
IMPLEMENT_OBJECT_SERIALIZE (Mesh)

template <typename Index>
static void GetVertexBufferRange(const Index* indices, int indexCount, UInt32& fromVertex, UInt32& toVertex)
{
	Index a = Index(INT_MAX);
	Index b = 0;
	const Index* indicesEnd = indices + indexCount;
	for (const Index* index = indices; index < indicesEnd; ++index)
	{
		a = std::min(a, *index);
		b = std::max(b, *index);
	}
	fromVertex = a;
	toVertex = b;
}

void Mesh::ByteSwapIndices ()
{
	SwapEndianArray (&m_IndexBuffer[0], kVBOIndexSize, GetTotalndexCount());
}

template<class T>
bool ShouldSerializeForBigEndian (T& transfer)
{
	bool bigEndian = UNITY_BIG_ENDIAN;
	if (transfer.ConvertEndianess())
		bigEndian = !bigEndian;
	return bigEndian;
}

void Mesh::DestripifyIndices ()
{
	if (m_IndexBuffer.empty() || m_SubMeshes.empty())
		return;

	int submeshCount = m_SubMeshes.size();
	bool anyStripped = false;
	for (size_t i = 0; i < submeshCount; ++i)
	{
		if (m_SubMeshes[i].topology == kPrimitiveTriangleStripDeprecated)
		{
			anyStripped = true;
			break;
		}
	}
	if(!anyStripped)
		return;

	// destripify the stripped submeshes
	typedef UNITY_TEMP_VECTOR(UInt16) TemporaryIndexContainer;

	std::vector<TemporaryIndexContainer> submeshIndices;
	submeshIndices.resize(submeshCount);
	for(int i=0;i<submeshCount;i++)
	{
		SubMesh& sm = m_SubMeshes[i];
		if (sm.topology == kPrimitiveTriangleStripDeprecated)
			Destripify (GetSubMeshBuffer16(i), sm.indexCount, submeshIndices[i]);
		else
		{
			submeshIndices[i].resize(sm.indexCount);
			memcpy(&submeshIndices[i][0], GetSubMeshBuffer16(i), sm.indexCount << 1);
		}
	}

	SetSubMeshCount(0);
	SetSubMeshCount(submeshCount);

	for(int i=0;i<submeshCount;i++)
		SetIndices(&submeshIndices[i][0], submeshIndices[i].size(), i, kPrimitiveTriangles);
}

bool Mesh::CanAccessFromScript() const
{
#if UNITY_EDITOR
	// Allow editor scripts access even if not allowed in runtime
	if (!IsInsidePlayerLoop() && !IsInsideRenderLoop())
		return true;
#endif
	return m_IsReadable;
}


template<class TransferFunction>
void Mesh::Transfer (TransferFunction& transfer)
{
	#if SUPPORT_SERIALIZED_TYPETREES
	// See TransferWorkaround35SerializeFuckup below for comments.
	// Remove when we can break backwards-compatiblity.
	if (transfer.GetFlags() & kWorkaround35MeshSerializationFuckup)
	{
		TransferWorkaround35SerializeFuckup (transfer);
		return;
	}
	#endif

	Super::Transfer (transfer);
	transfer.SetVersion (8);

	#if UNITY_EDITOR
	const UInt32 supportedChannels = transfer.IsWritingGameReleaseData() ? transfer.GetBuildUsage().meshSupportedChannels : 0;
	const UInt32 meshUsageFlags = transfer.IsWritingGameReleaseData() ? transfer.GetBuildUsage().meshUsageFlags : 0;
	PrepareMeshDataForBuildTarget prepareMesh(*this, transfer.GetBuildingTarget().platform, supportedChannels, meshUsageFlags);
	#endif

	bool reswizzleColors = false;
	if (m_VertexColorsSwizzled)
	{
		// Unswizzle colors before serializing
		std::transform(GetColorBegin(), GetColorEnd(), GetColorBegin(), UnswizzleColorForPlatform);
		m_VertexColorsSwizzled = false;
		reswizzleColors = true;
	}

	transfer.Transfer (m_SubMeshes, "m_SubMeshes", kHideInEditorMask);
	transfer.Transfer (m_Shapes, "m_Shapes", kHideInEditorMask);
	transfer.Transfer (m_Bindpose, "m_BindPose", kHideInEditorMask);
	transfer.Transfer (m_BonePathHashes, "m_BoneNameHashes", kHideInEditorMask);
	transfer.Transfer (m_RootBonePathHash, "m_RootBoneNameHash", kHideInEditorMask);

	transfer.Transfer (m_MeshCompression, "m_MeshCompression", kHideInEditorMask);
	transfer.Transfer (m_StreamCompression, "m_StreamCompression", kHideInEditorMask);
	transfer.Transfer (m_IsReadable, "m_IsReadable", kHideInEditorMask);
	transfer.Transfer (m_KeepVertices, "m_KeepVertices", kHideInEditorMask);
	transfer.Transfer (m_KeepIndices, "m_KeepIndices", kHideInEditorMask);
	transfer.Align();

	// Notice the two codepaths for serialization here.
	// It is very important to keep both codepaths in sync, otherwise SafeBinaryRead serialization will crash.
	// Look at kSerializeForPrefabSystem to disable compression when using Transfer to instantiate a Mesh.
	// Changes to compression can break web content if we recompress at runtime. (case 546159)
	bool doCompression = m_MeshCompression && !(transfer.GetFlags() & kSerializeForPrefabSystem);
	if (!doCompression)
	{
		if (transfer.ConvertEndianess() && transfer.IsWriting ())
			ByteSwapIndices();

		transfer.Transfer (m_IndexBuffer, "m_IndexBuffer", kHideInEditorMask);

		if (transfer.ConvertEndianess() && (transfer.IsWriting () || transfer.IsReading ()))
			ByteSwapIndices();

		transfer.Transfer (m_Skin, "m_Skin", kHideInEditorMask);

		if (transfer.IsVersionSmallerOrEqual (5))
		{
			dynamic_array<Vector4f> tangents;
			dynamic_array<Vector3f> vertices, normals;
			dynamic_array<Vector2f> uvs, uvs1;
			dynamic_array<ColorRGBA32> colors;


			transfer.Transfer (vertices, "m_Vertices", kHideInEditorMask);
			transfer.Transfer (uvs, "m_UV", kHideInEditorMask);
			transfer.Transfer (uvs1, "m_UV1", kHideInEditorMask);
			transfer.Transfer (tangents, "m_Tangents", kHideInEditorMask);
			transfer.Transfer (normals, "m_Normals", kHideInEditorMask);
			transfer.Transfer (colors, "m_Colors", kHideInEditorMask);

			unsigned format = 0;
			if (!vertices.empty ()) format |= VERTEX_FORMAT1(Vertex);
			if (!tangents.empty ()) format |= VERTEX_FORMAT1(Tangent);
			if (!normals.empty ()) format |= VERTEX_FORMAT1(Normal);
			if (!uvs.empty ()) format |= VERTEX_FORMAT1(TexCoord0);
			if (!uvs1.empty ()) format |= VERTEX_FORMAT1(TexCoord1);
			if (!colors.empty ()) format |= VERTEX_FORMAT1(Color);

			size_t vertexCount = vertices.size ();
			if (GetVertexCount () != vertexCount || GetAvailableChannels () != format)
				ResizeVertices (vertexCount, format);

			strided_copy (vertices.begin (), vertices.begin () + std::min (vertices.size (), vertexCount), GetVertexBegin ());
			strided_copy (normals.begin (), normals.begin () + std::min (normals.size (), vertexCount), GetNormalBegin ());
			strided_copy (uvs.begin (), uvs.begin () + std::min (uvs.size (), vertexCount), GetUvBegin (0));
			strided_copy (uvs1.begin (), uvs1.begin () + std::min (uvs1.size (), vertexCount), GetUvBegin (1));
			strided_copy (tangents.begin (), tangents.begin () + std::min (tangents.size (), vertexCount), GetTangentBegin ());
			strided_copy (colors.begin (), colors.begin () + std::min (colors.size (), vertexCount), GetColorBegin ());
		}
		else
		{
			// version 6 introduces interleaved buffer
			if (transfer.ConvertEndianess() && transfer.IsWriting ())
				m_VertexData.SwapEndianess ();

			transfer.Transfer (m_VertexData, "m_VertexData", kHideInEditorMask);

			if (transfer.ConvertEndianess() && (transfer.IsWriting () || transfer.IsReading ()))
				m_VertexData.SwapEndianess ();
		}
	}
	// Notice the two codepaths for serialization here.
	// It is very important to keep both codepaths in sync, otherwise SafeBinaryRead serialization will crash.
	else
	{
		BoneInfluenceContainer dummySkin;
		VertexData dummyVertexData;
		IndexContainer dummyIndexContainer;

		transfer.Transfer (dummyIndexContainer, "m_IndexBuffer", kHideInEditorMask);
		transfer.Transfer (dummySkin, "m_Skin", kHideInEditorMask);
		transfer.Transfer (dummyVertexData, "m_VertexData", kHideInEditorMask);
	}

	{
		// only keep the compressed mesh in memory while needed
		CompressedMesh m_CompressedMesh;
		transfer.Align();
		// Check both IsWriting() and IsReading() since both are true when reading with SafeBinaryRead
		if (doCompression && transfer.IsWriting())
			m_CompressedMesh.Compress(*this, m_MeshCompression);

		transfer.Transfer (m_CompressedMesh, "m_CompressedMesh", kHideInEditorMask);

		if (doCompression && transfer.DidReadLastProperty ())
			m_CompressedMesh.Decompress(*this);
	}

	#if !GFX_SUPPORTS_TRISTRIPS
	if (transfer.IsReading())
		DestripifyIndices ();
	#endif

	// Reswizzle colors after serializing
	if (reswizzleColors)
	{
		std::transform(GetColorBegin(), GetColorEnd(), GetColorBegin(), SwizzleColorForPlatform);
		m_VertexColorsSwizzled = true;
	}

	transfer.Transfer (m_LocalAABB, "m_LocalAABB", kHideInEditorMask);

	#if UNITY_EDITOR
	// When building player we precalcuate mesh usage based on who uses the different MeshColliders in different scenes.
	if (transfer.IsWritingGameReleaseData())
	{
		int buildMeshUsageFlags = transfer.GetBuildUsage().meshUsageFlags;
		transfer.Transfer (buildMeshUsageFlags, "m_MeshUsageFlags", kHideInEditorMask);
	}
	else
		transfer.Transfer (m_MeshUsageFlags, "m_MeshUsageFlags", kHideInEditorMask);
	#else
	transfer.Transfer (m_MeshUsageFlags, "m_MeshUsageFlags", kHideInEditorMask);
	#endif

	m_CollisionMesh.Transfer(transfer, *this);

	if (transfer.IsOldVersion(1))
	{
		vector<DeprecatedLOD> lod;
		transfer.Transfer (lod, "m_LODData", kHideInEditorMask);
		LoadDeprecatedMeshData(*this, lod);
	}

	if (transfer.IsVersionSmallerOrEqual(4))
	{
		for (int sm = 0; sm < m_SubMeshes.size(); ++sm)
		{
			UpdateSubMeshVertexRange (sm);
			RecalculateSubmeshBoundsInternal (sm);
		}
	}

	if (transfer.IsOldVersion(2) || transfer.IsOldVersion(1))
	{
		DeprecatedTangentsArray m_TangentSpace;
		transfer.Transfer (m_TangentSpace, "m_TangentSpace", kHideInEditorMask);
		if(transfer.IsReading())
			LoadDeprecatedTangentData(*this,m_TangentSpace);
	}

	if (transfer.IsVersionSmallerOrEqual(7))
	{
		DestripifySubmeshOnTransferInternal();
	}
	TRANSFER_EDITOR_ONLY_HIDDEN(m_MeshOptimized);

#if UNITY_EDITOR || UNITY_PS3
	TransferPS3Data(transfer);
#endif
}

#if SUPPORT_SERIALIZED_TYPETREES
// Except for some dead-path removal and a change to the ResizeVertices call to account for an
// API change, this is an exact copy of the Mesh::Transfer function as it shipped in 3.5.0 final.
// This path exists solely to work around the issue with compressed mesh serialization in 3.5.0
// which produced different serializations for compressed and uncompressed meshes while using the
// same type tree for either case.  This makes it impossible for SafeBinaryRead to sort things out.
//
// By having the exact same transfer path, we end up with identical type trees compared to version
// 3.5.0 and thus automatically end up on the StreamedBinaryRead codepath.  Also, as long as this
// separate path here is preserved, we can read the faulty 3.5.0 streams without having to worry
// about it in the normal transfer path.
template<class TransferFunction>
void Mesh::TransferWorkaround35SerializeFuckup (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (6);

	if (m_VertexColorsSwizzled)
	{
		// Unswizzle colors before serializing
		std::transform(GetColorBegin(), GetColorEnd(), GetColorBegin(), UnswizzleColorForPlatform);
		m_VertexColorsSwizzled = false;
	}

	transfer.Transfer (m_SubMeshes, "m_SubMeshes", kHideInEditorMask);

	if (!transfer.IsVersionSmallerOrEqual(3))
		transfer.Transfer (m_MeshCompression, "m_MeshCompression", kHideInEditorMask);
	else
		m_MeshCompression = kMeshCompressionOff;

	transfer.Align();
	if (m_MeshCompression == kMeshCompressionOff)
	{
		if (transfer.ConvertEndianess() && transfer.IsWriting ())
			ByteSwapIndices();

		transfer.Transfer (m_IndexBuffer, "m_IndexBuffer", kHideInEditorMask);

		if (transfer.ConvertEndianess() && (transfer.IsWriting () || transfer.IsReading ()))
			ByteSwapIndices();

		transfer.Transfer (m_Skin, "m_Skin", kHideInEditorMask);
		transfer.Transfer (m_Bindpose, "m_BindPose", kHideInEditorMask);

		if (transfer.IsVersionSmallerOrEqual (5))
		{
			dynamic_array<Vector4f> tangents;
			dynamic_array<Vector3f> vertices, normals;
			dynamic_array<Vector2f> uvs, uvs1;
			dynamic_array<ColorRGBA32> colors;


			transfer.Transfer (vertices, "m_Vertices", kHideInEditorMask);
			transfer.Transfer (uvs, "m_UV", kHideInEditorMask);
			transfer.Transfer (uvs1, "m_UV1", kHideInEditorMask);
			transfer.Transfer (tangents, "m_Tangents", kHideInEditorMask);
			transfer.Transfer (normals, "m_Normals", kHideInEditorMask);
			transfer.Transfer (colors, "m_Colors", kHideInEditorMask);

			unsigned format = 0;
			if (!vertices.empty ()) format |= VERTEX_FORMAT1(Vertex);
			if (!tangents.empty ()) format |= VERTEX_FORMAT1(Tangent);
			if (!normals.empty ()) format |= VERTEX_FORMAT1(Normal);
			if (!uvs.empty ()) format |= VERTEX_FORMAT1(TexCoord0);
			if (!uvs1.empty ()) format |= VERTEX_FORMAT1(TexCoord1);
			if (!colors.empty ()) format |= VERTEX_FORMAT1(Color);

			size_t vertexCount = vertices.size ();
			if (GetVertexCount () != vertexCount || GetAvailableChannels () != format)
				ResizeVertices (vertexCount, format);

			strided_copy (vertices.begin (), vertices.begin () + std::min (vertices.size (), vertexCount), GetVertexBegin ());
			strided_copy (normals.begin (), normals.begin () + std::min (normals.size (), vertexCount), GetNormalBegin ());
			strided_copy (uvs.begin (), uvs.begin () + std::min (uvs.size (), vertexCount), GetUvBegin (0));
			strided_copy (uvs1.begin (), uvs1.begin () + std::min (uvs1.size (), vertexCount), GetUvBegin (1));
			strided_copy (tangents.begin (), tangents.begin () + std::min (tangents.size (), vertexCount), GetTangentBegin ());
			strided_copy (colors.begin (), colors.begin () + std::min (colors.size (), vertexCount), GetColorBegin ());
		}
		else
		{
			// version 6 introduces interleaved buffer
			if (transfer.ConvertEndianess() && transfer.IsWriting ())
				m_VertexData.SwapEndianess ();

			transfer.Transfer (m_VertexData, "m_VertexData", kHideInEditorMask);

			if (transfer.ConvertEndianess() && (transfer.IsWriting () || transfer.IsReading ()))
				m_VertexData.SwapEndianess ();
		}
	}
	else
	{
		vector<Vector4f> emptyVector4;
		vector<Vector3f> emptyVector3;
		vector<Vector2f> emptyVector2;
		vector<BoneInfluence> emptyBones;
		vector<UInt8> emptyIndices;
		vector<ColorRGBA32> emptyColors;

		transfer.Transfer (emptyIndices, "m_IndexBuffer", kHideInEditorMask);
		transfer.Transfer (emptyVector3, "m_Vertices", kHideInEditorMask);
		transfer.Transfer (emptyBones, "m_Skin", kHideInEditorMask);
		transfer.Transfer (m_Bindpose, "m_BindPose", kHideInEditorMask);
		transfer.Transfer (emptyVector2, "m_UV", kHideInEditorMask);
		transfer.Transfer (emptyVector2, "m_UV1", kHideInEditorMask);
		transfer.Transfer (emptyVector4, "m_Tangents", kHideInEditorMask);
		transfer.Transfer (emptyVector3, "m_Normals", kHideInEditorMask);
		transfer.Transfer (emptyColors, "m_Colors", kHideInEditorMask);
	}

	CompressedMesh m_CompressedMesh;
	transfer.Align();
	if (transfer.IsWriting() && m_MeshCompression)
		m_CompressedMesh.Compress(*this, m_MeshCompression);

	printf_console( "Reading compressed mesh...\n" );
	transfer.Transfer (m_CompressedMesh, "m_CompressedMesh", kHideInEditorMask);

	if (transfer.DidReadLastProperty () && m_MeshCompression)
		m_CompressedMesh.Decompress(*this);


#if !GFX_SUPPORTS_TRISTRIPS
	if (transfer.IsReading())
		DestripifyIndices ();
#endif

	transfer.Transfer (m_LocalAABB, "m_LocalAABB", kHideInEditorMask);
	transfer.Transfer (m_MeshUsageFlags, "m_MeshUsageFlags", kHideInEditorMask);

	m_CollisionMesh.Transfer(transfer, *this);

	if (transfer.IsOldVersion(1))
	{
		vector<DeprecatedLOD> lod;
		transfer.Transfer (lod, "m_LODData", kHideInEditorMask);
		LoadDeprecatedMeshData(*this, lod);
	}

	if (transfer.IsVersionSmallerOrEqual(4))
	{
		for (int sm = 0; sm < m_SubMeshes.size(); ++sm)
		{
			UpdateSubMeshVertexRange (sm);
			RecalculateSubmeshBoundsInternal (sm);
		}
	}

	if (transfer.IsOldVersion(2) || transfer.IsOldVersion(1))
	{
		DeprecatedTangentsArray m_TangentSpace;
		transfer.Transfer (m_TangentSpace, "m_TangentSpace", kHideInEditorMask);
		if(transfer.IsReading())
			LoadDeprecatedTangentData(*this,m_TangentSpace);
	}

	if (transfer.IsReading())
		DestripifySubmeshOnTransferInternal();
}
#endif

#if UNITY_EDITOR || UNITY_PS3
template<class TransferFunction>
void Mesh::TransferPS3Data (TransferFunction& transfer)
{
	if (UNITY_PS3 || (kBuildPS3 == transfer.GetBuildingTarget().platform))
	{
		transfer.Transfer(m_Partitions, "m_Partitions", kHideInEditorMask);
		transfer.Transfer(m_PartitionInfos, "m_PartitionInfos", kHideInEditorMask);
	}
}
#endif


void Mesh::UpdateSubMeshVertexRange (int index)
{
	SubMesh& submesh = m_SubMeshes[index];
	if (submesh.indexCount > 0)
	{
		UInt32 lastVertex = 0;
			GetVertexBufferRange(GetSubMeshBuffer16(index), submesh.indexCount, submesh.firstVertex, lastVertex);
		Assert(lastVertex < GetVertexCount ());
		Assert(submesh.firstVertex <= lastVertex);
		submesh.vertexCount = lastVertex - submesh.firstVertex + 1;
	}
	else
	{
		submesh.firstVertex = 0;
		submesh.vertexCount = 0;
	}
}

static bool CheckOutOfBounds (unsigned max, const UInt16* p, unsigned count)
{
	for (int i=0;i<count;i++)
	{
		if (p[i] >= max)
			return false;
	}
	return true;
}

static bool CheckOutOfBounds (unsigned max, const UInt32* p, unsigned count)
{
	for (int i=0;i<count;i++)
	{
		if (p[i] >= max)
			return false;
	}
	return true;
}

bool Mesh::ValidateVertexCount (unsigned newVertexCount, const void* newTriangles, unsigned indexCount)
{
	if (newTriangles)
	{
			return CheckOutOfBounds (newVertexCount, reinterpret_cast<const UInt16*>(newTriangles), indexCount);
	}
	else
	{
			return CheckOutOfBounds(newVertexCount, reinterpret_cast<const UInt16*>(&m_IndexBuffer[0]), GetTotalndexCount());
	}
}

int Mesh::GetTotalndexCount () const
{
	return m_IndexBuffer.size () / kVBOIndexSize;
}

void Mesh::SetVertices (Vector3f const* data, size_t count)
{
	if (m_StreamCompression)
		return;

	if (count > std::numeric_limits<UInt16>::max())
	{
		ErrorString("Mesh.vertices is too large. A mesh may not have more than 65000 vertices.");
		return;
	}

	size_t prevCount = GetVertexCount ();
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion3_5_3_a1) && count < prevCount && !ValidateVertexCount(count, NULL, 0))
	{
		ErrorString("Mesh.vertices is too small. The supplied vertex array has less vertices than are referenced by the triangles array.");
		return;
	}

	WaitOnRenderThreadUse();

#if UNITY_PS3
	if(m_Skin.empty() || (!(m_Skin.empty() || m_PartitionInfos.empty())))
	{
	// mircea@info: sadly for us GPU renders from pointers, so we need to create a new instance when something changes....(fixes nasty bug #434226)
		SET_ALLOC_OWNER(this);
		VertexData vertexData(m_VertexData, GetAvailableChannels(), GetStreamsLayout(), GetChannelsLayout());
	swap(vertexData, m_VertexData);
	}
#endif

	if (prevCount != count)
	{
		unsigned prevChannels = GetAvailableChannels ();
		ResizeVertices (count, prevChannels | VERTEX_FORMAT1(Vertex));

		// In case there were other channels present, initialize the newly created values of
		// the expanded buffer to something meaningful.
		if (prevCount != 0 && count > prevCount && (prevChannels & ~VERTEX_FORMAT1(Vertex)))
		{
			InitChannelsToDefault (prevCount, count - prevCount, prevChannels & ~VERTEX_FORMAT1(Vertex));
		}
	}

	// Make sure we'll not be overrunning the buffer
	if (GetVertexCount () < count)
		count = GetVertexCount ();

	strided_copy (data, data + count, GetVertexBegin ());
	SetChannelsDirty (VERTEX_FORMAT1(Vertex), false);

	// We do not recalc the bounds automatically when re-writing existing vertices
	if (prevCount != count)
		RecalculateBounds ();
}

void Mesh::SetNormals (Vector3f const* data, size_t count)
{
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	if (count == 0 || !data)
	{
		FormatVertices (GetAvailableChannels () & ~VERTEX_FORMAT1(Normal));
		SetChannelsDirty (VERTEX_FORMAT1(Normal), false);
		return;
	}

	if (count != GetVertexCount ())
	{
		ErrorStringMsg(kMeshAPIErrorMessage, "normals");
		return;
	}

	if (!IsAvailable (kShaderChannelNormal))
		FormatVertices (GetAvailableChannels () | VERTEX_FORMAT1(Normal));

	strided_copy (data, data + count, GetNormalBegin ());

	SetChannelsDirty (VERTEX_FORMAT1(Normal), false);
}

void Mesh::SetTangents (Vector4f const* data, size_t count)
{
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	if (count == 0 || !data)
	{
		FormatVertices (GetAvailableChannels () & ~VERTEX_FORMAT1(Tangent));
		SetChannelsDirty (VERTEX_FORMAT1(Tangent), false);
		return;
	}

	if (count != GetVertexCount ())
	{
		ErrorStringMsg(kMeshAPIErrorMessage, "tangents");
		return;
	}

	if (!IsAvailable (kShaderChannelTangent))
		FormatVertices (GetAvailableChannels () | VERTEX_FORMAT1(Tangent));

	strided_copy (data, data + count, GetTangentBegin ());
	SetChannelsDirty( VERTEX_FORMAT1(Tangent), false );
}

void Mesh::SetUv (int uvIndex, Vector2f const* data, size_t count)
{
	Assert (uvIndex <= 1);
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	ShaderChannel texCoordChannel = static_cast<ShaderChannel>(kShaderChannelTexCoord0 + uvIndex);
	unsigned texCoordMask = 1 << texCoordChannel;
	if (count == 0 || !data)
	{
		FormatVertices (GetAvailableChannels () & ~texCoordMask);
		SetChannelsDirty (texCoordMask, false);
		return;
	}

	if (count != GetVertexCount ())
	{
		const char* uvName = uvIndex == 1 ? "uv2" : "uv";
		ErrorStringMsg(kMeshAPIErrorMessage, uvName);
		return;
	}

	if (!IsAvailable (texCoordChannel))
		FormatVertices (GetAvailableChannels () | texCoordMask);

	strided_copy (data, data + count, GetUvBegin (uvIndex));
	SetChannelsDirty (texCoordMask, false);
}

void Mesh::SetColors (ColorRGBA32 const* data, size_t count)
{
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	if (count == 0 || !data)
	{
		FormatVertices (GetAvailableChannels () & ~VERTEX_FORMAT1(Color));
		SetChannelsDirty( VERTEX_FORMAT1(Color), false );
		return;
	}

	if (count != GetVertexCount ())
	{
		ErrorStringMsg(kMeshAPIErrorMessage, "colors");
		return;
	}

	if (!IsAvailable (kShaderChannelColor))
	{
		FormatVertices (GetAvailableChannels () | VERTEX_FORMAT1(Color));
	}
	m_VertexColorsSwizzled = gGraphicsCaps.needsToSwizzleVertexColors;

	if (m_VertexColorsSwizzled)
		std::transform(data, data + count, GetColorBegin(), SwizzleColorForPlatform);
	else
		std::copy(data, data + count, GetColorBegin());

	SetChannelsDirty( VERTEX_FORMAT1(Color), false );
}

void Mesh::SetColorsConverting (ColorRGBAf const* data, size_t count)
{
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	if (count == 0 || !data)
	{
		FormatVertices (GetAvailableChannels () & ~VERTEX_FORMAT1(Color));
		SetChannelsDirty( VERTEX_FORMAT1(Color), false );
		return;
	}

	if (count != GetVertexCount ())
	{
		ErrorStringMsg(kMeshAPIErrorMessage, "colors");
		return;
	}

	if (!IsAvailable (kShaderChannelColor))
	{
		FormatVertices (GetAvailableChannels () | VERTEX_FORMAT1(Color));
	}
	m_VertexColorsSwizzled = gGraphicsCaps.needsToSwizzleVertexColors;

	if (m_VertexColorsSwizzled)
		std::transform(data, data + count, GetColorBegin(), SwizzleColorForPlatform);
	else
		strided_copy_convert(data, data + count, GetColorBegin());

	SetChannelsDirty( VERTEX_FORMAT1(Color), false );
}


void Mesh::GetTriangles (Mesh::TemporaryIndexContainer& triangles) const
{
	triangles.clear();
	for (unsigned m=0;m<GetSubMeshCount();m++)
		AppendTriangles(triangles, m);
}

void Mesh::GetTriangles (Mesh::TemporaryIndexContainer& triangles, unsigned submesh) const
{
	triangles.clear();
	AppendTriangles(triangles, submesh);
}

void QuadsToTriangles(const UInt16* quads, const int indexCount, Mesh::TemporaryIndexContainer& triangles)
{
	DebugAssert (indexCount%4 == 0);
	triangles.resize((indexCount/2)*3);
	for (int q = 0, t = 0; q < indexCount; q += 4, t +=6)
	{
		triangles[t] = quads[q];
		triangles[t + 1] = quads[q + 1];
		triangles[t + 2] = quads[q + 2];

		triangles[t + 3] = quads[q];
		triangles[t + 4] = quads[q + 2];
		triangles[t + 5] = quads[q + 3];
	}
}


void Mesh::AppendTriangles (Mesh::TemporaryIndexContainer& triangles, unsigned submesh) const
{
	if (submesh >= GetSubMeshCount())
	{
		ErrorString("Failed getting triangles. Submesh index is out of bounds.");
		return;
	}

	int topology = GetSubMeshFast(submesh).topology;
	if (topology == kPrimitiveTriangleStripDeprecated)
		Destripify(GetSubMeshBuffer16(submesh), GetSubMeshFast(submesh).indexCount, triangles);
	else if (topology == kPrimitiveQuads)
		QuadsToTriangles (GetSubMeshBuffer16 (submesh), GetSubMeshFast (submesh).indexCount, triangles);
	else if (topology == kPrimitiveTriangles)
		triangles.insert(triangles.end(), GetSubMeshBuffer16(submesh), GetSubMeshBuffer16(submesh) + GetSubMeshFast(submesh).indexCount);
	else
		ErrorString("Failed getting triangles. Submesh topology is lines or points.");
}

void Mesh::GetStrips (Mesh::TemporaryIndexContainer& triangles, unsigned submesh) const
{
	triangles.clear();
	if (submesh >= GetSubMeshCount())
	{
		ErrorString("Failed getting triangles. Submesh index is out of bounds.");
		return;
	}

	if (GetSubMeshFast(submesh).topology != kPrimitiveTriangleStripDeprecated)
		return;

	triangles.assign(GetSubMeshBuffer16(submesh), GetSubMeshBuffer16(submesh) + GetSubMeshFast(submesh).indexCount);
}

void Mesh::GetIndices (TemporaryIndexContainer& triangles, unsigned submesh) const
{
	triangles.clear();
	if (submesh >= GetSubMeshCount())
	{
		ErrorString("Failed getting indices. Submesh index is out of bounds.");
		return;
	}
	triangles.assign(GetSubMeshBuffer16(submesh), GetSubMeshBuffer16(submesh) + GetSubMeshFast(submesh).indexCount);
}


bool Mesh::SetIndices (const UInt32* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology)
{
	int mask = kRebuildCollisionTriangles;
	return SetIndicesComplex (indices, count, submesh, topology, mask);
}

bool Mesh::SetIndices (const UInt16* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology)
{
	int mask = kRebuildCollisionTriangles | k16BitIndices;
	return SetIndicesComplex (indices, count, submesh, topology, mask);
}


bool Mesh::SetIndicesComplex (const void* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology, int mode)
{
	WaitOnRenderThreadUse();

	if (indices == NULL && count != 0 && (mode & kDontAssignIndices) == 0)
	{
		ErrorString("failed setting triangles. triangles is NULL");
		return false;
	}

	if (submesh >= GetSubMeshCount())
	{
		ErrorString("Failed setting triangles. Submesh index is out of bounds.");
		return false;
	}

	if ((topology == kPrimitiveTriangles) && (count % 3 != 0))
	{
		ErrorString("Failed setting triangles. The number of supplied triangle indices must be a multiple of 3.");
		return false;
	}

	if ((mode & kDontAssignIndices) == 0)
	{
		bool valid;
		if (mode & k16BitIndices)
			valid = CheckOutOfBounds (GetVertexCount(), reinterpret_cast<const UInt16*>(indices), count);
		else
			valid = CheckOutOfBounds (GetVertexCount(), reinterpret_cast<const UInt32*>(indices), count);

		if (!valid)
		{
			ErrorString("Failed setting triangles. Some indices are referencing out of bounds vertices.");
			return false;
		}
	}

	SetIndexData(submesh, count, indices, topology, mode);

	if (mode & Mesh::kDontSupportSubMeshVertexRanges)
	{
		Assert(m_SubMeshes.size () == 1);
		m_SubMeshes[0].firstVertex = 0;
		m_SubMeshes[0].vertexCount = GetVertexCount();
		m_SubMeshes[0].localAABB = m_LocalAABB;
	}
	else
	{
		// Update vertex range
		UpdateSubMeshVertexRange (submesh);
		RecalculateSubmeshBounds(submesh);
	}

	if (mode & kRebuildCollisionTriangles)
		RebuildCollisionTriangles();

	SetChannelsDirty( 0, true );

	return true;
}

void Mesh::DestripifySubmeshOnTransferInternal()
{
	if (m_IndexBuffer.empty() || m_SubMeshes.empty())
		return;

	int submeshCount = m_SubMeshes.size();	
	typedef UNITY_TEMP_VECTOR(UInt16) TemporaryIndexContainer;
	
	std::vector<TemporaryIndexContainer> submeshIndices;
	submeshIndices.resize(submeshCount);
	
	// We have to do this in two batches, as SetIndexData seems to have a bug that causes
	// triangle windings to get screwed up if we attempt to modify the submeshes in-place.
	
	for (size_t i = 0; i < submeshCount; ++i)
	{
		SubMesh& sm = m_SubMeshes[i];
		if (sm.topology == kPrimitiveTriangleStripDeprecated)
		{
			Destripify (GetSubMeshBuffer16(i), sm.indexCount, submeshIndices[i]);
		}	
		else
		{
			submeshIndices[i].resize(sm.indexCount);
			memcpy(&submeshIndices[i][0], GetSubMeshBuffer16(i), sm.indexCount << 1);
		}
	}
	
	for(size_t i = 0; i < submeshCount; ++i)
	{
		SetIndexData(i, submeshIndices[i].size(), &submeshIndices[i][0], kPrimitiveTriangles, kRebuildCollisionTriangles | k16BitIndices);
	}
}

void Mesh::SetIndexData(int submeshIndex, int indexCount, const void* indices, GfxPrimitiveType topology, int mode)
{
	int newByteSize = indexCount * kVBOIndexSize;
	int oldSubmeshSize = GetSubMeshBufferByteSize (submeshIndex);
	int insertedBytes = newByteSize - GetSubMeshBufferByteSize (submeshIndex);
	int oldFirstByte = m_SubMeshes[submeshIndex].firstByte;
	// Growing the buffer
	if (insertedBytes > 0)
	{
		m_IndexBuffer.insert(m_IndexBuffer.begin() + oldFirstByte + oldSubmeshSize, insertedBytes, 0);
	}
	// Shrinking the buffer
	else
	{
		m_IndexBuffer.erase(m_IndexBuffer.begin() + oldFirstByte, m_IndexBuffer.begin() + oldFirstByte - insertedBytes);
	}

#if UNITY_PS3

	// mircea@info: sadly for us GPU renders from pointers, so we need to create a new instance when something changes....(fixes nasty bug #434226)
	IndexContainer newIndexContainer;
	newIndexContainer.resize(m_IndexBuffer.size());
	m_IndexBuffer.swap(newIndexContainer);

#endif

	// Update the sub mesh
	m_SubMeshes[submeshIndex].indexCount = indexCount;
	m_SubMeshes[submeshIndex].topology = topology;

	// Synchronize subsequent sub meshes
	for (int i=submeshIndex+1;i<m_SubMeshes.size();i++)
	{
		m_SubMeshes[i].firstByte = m_SubMeshes[i-1].firstByte + m_SubMeshes[i-1].indexCount * kVBOIndexSize;
	}

	// Write indices into the allocated data
	if ((mode & kDontAssignIndices) == 0)
	{
		if (mode & k16BitIndices)
		{
			const UInt16* src = reinterpret_cast<const UInt16*>(indices);
			UInt16* dst = GetSubMeshBuffer16(submeshIndex);
			for (int i=0;i<indexCount;i++)
				dst[i] = src[i];
		}
		else
		{
			const UInt32* src = reinterpret_cast<const UInt32*>(indices);
			UInt16* dst = GetSubMeshBuffer16(submeshIndex);
			for (int i=0;i<indexCount;i++)
				dst[i] = src[i];
		}
	}

	return;
}

const UInt16* Mesh::GetSubMeshBuffer16 (int submesh) const
{
	return m_IndexBuffer.size() > 0 && m_SubMeshes[submesh].firstByte < m_IndexBuffer.size() ? reinterpret_cast<const UInt16*> (&m_IndexBuffer[m_SubMeshes[submesh].firstByte]) : NULL;
}
UInt16* Mesh::GetSubMeshBuffer16 (int submesh)
{
	return m_IndexBuffer.size() > 0 && m_SubMeshes[submesh].firstByte < m_IndexBuffer.size() ? reinterpret_cast<UInt16*> (&m_IndexBuffer[m_SubMeshes[submesh].firstByte]) : NULL;
}

void Mesh::SetBindposes (const Matrix4x4f* bindposes, int count)
{
	m_Bindpose.assign(bindposes, bindposes + count);
	SetDirty();
}

void Mesh::SetBounds (const AABB& aabb)
{
	m_LocalAABB = aabb;
	SetDirty();
	NotifyObjectUsers( kDidModifyBounds );
	m_IntermediateUsers.Notify( kImNotifyBoundsChanged );
}

void Mesh::SetBounds (unsigned submesh, const AABB& aabb)
{
	GetSubMeshFast(submesh).localAABB = aabb;
	SetDirty();
	NotifyObjectUsers( kDidModifyBounds );
	m_IntermediateUsers.Notify( kImNotifyBoundsChanged );
}

void Mesh::NotifyObjectUsers(const MessageIdentifier& msg)
{
	ASSERT_RUNNING_ON_MAIN_THREAD;

	MessageData data;
	data.SetData (this, ClassID (Mesh));

	ObjectList::iterator next;
	for( ObjectList::iterator i = m_ObjectUsers.begin(); i != m_ObjectUsers.end(); i=next )
	{
		next = i;
		++next;
		Object& target = **i;
		SendMessageDirect(target, msg, data);
	}
}

void Mesh::WaitOnRenderThreadUse()
{
#if ENABLE_MULTITHREADED_CODE
	if (m_WaitOnCPUFence)
	{
		GetGfxDevice().WaitOnCPUFence(m_CurrentCPUFence);
		m_WaitOnCPUFence = false;
	}
#endif
}

void Mesh::RebuildCollisionTriangles()
{
	m_CollisionMesh.VertexDataHasChanged ();
}

PROFILER_INFORMATION(gRecalculateNormals, "Mesh.RecalculateNormals", kProfilerOther)

void Mesh::RecalculateNormals()
{
	if (m_StreamCompression)
		return;
	WaitOnRenderThreadUse();

	PROFILER_AUTO(gRecalculateNormals, this);

	if (int vertexCount = GetVertexCount())
	{
		unsigned newChannels = m_VertexData.GetChannelMask () | VERTEX_FORMAT1(Normal);
		if (newChannels != m_VertexData.GetChannelMask ())
			FormatVertices (newChannels);

		TemporaryIndexContainer triangles;
		GetTriangles (triangles);

		CalculateNormals( GetVertexBegin (), &triangles[0], vertexCount, triangles.size()/3, GetNormalBegin () );
	}

	SetChannelsDirty( VERTEX_FORMAT1(Normal), false );
}


void Mesh::SetSubMeshCount (unsigned int count)
{
	WaitOnRenderThreadUse();

	if (count == 0)
	{
		m_IndexBuffer.clear();
		m_SubMeshes.clear();
		return;
	}

	// Remove elements
	if (count < m_SubMeshes.size ())
	{
		m_IndexBuffer.resize(m_SubMeshes[count].firstByte);
		m_SubMeshes.resize(count);
	}
	// Append elements
	else if (count > m_SubMeshes.size ())
	{
		SubMesh data;
		data.firstByte = m_IndexBuffer.size();
		data.indexCount = 0;
		data.topology = kPrimitiveTriangles;
		data.firstVertex = 0;
		data.vertexCount = 0;
		data.localAABB = AABB (Vector3f::zero, Vector3f::zero);
		m_SubMeshes.resize(count, data);
		RecalculateBounds();
	}
}

size_t Mesh::GetSubMeshCount () const
{
	return m_SubMeshes.size();
}

int Mesh::GetPrimitiveCount() const
{
	int submeshes = GetSubMeshCount();
	int count = 0;
	for( int m = 0; m < submeshes; ++m ) {
		const SubMesh& sub = m_SubMeshes[m];
		count += ::GetPrimitiveCount(sub.indexCount, sub.topology, false);
	}
	return count;
}

int Mesh::CalculateTriangleCount() const
{
	int submeshes = GetSubMeshCount();
	int count = 0;
	for( int m = 0; m < submeshes; ++m )
	{
		const SubMesh& sub = m_SubMeshes[m];
		if (sub.topology == kPrimitiveTriangleStripDeprecated)
		{
			const UInt16* indices = GetSubMeshBuffer16(m);
			int triCount = CountTrianglesInStrip (indices, sub.indexCount);
			count += triCount;
		}
		else if (sub.topology == kPrimitiveTriangles)
		{
			count += sub.indexCount / 3;
		}
	}
	return count;
}

Mesh& Mesh::GetInstantiatedMesh (Mesh* mesh, Object& owner)
{
	if (NULL != mesh && mesh->m_Owner == PPtr<Object> (&owner))
		return *mesh;

	if (!IsWorldPlaying())
		ErrorStringObject("Instantiating mesh due to calling MeshFilter.mesh during edit mode. This will leak meshes. Please use MeshFilter.sharedMesh instead.", &owner);

	if (mesh == NULL || !mesh->HasVertexData ())
	{
		if (!mesh)
			mesh = NEW_OBJECT (Mesh);
		mesh->Reset();

		mesh->SetName(owner.GetName());
		mesh->m_Owner = &owner;

		mesh->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
		return *mesh;
	}

	Mesh* instance = NEW_OBJECT (Mesh);
	CopySerialized(*mesh, *instance);
	instance->SetNameCpp (Append (mesh->GetName (), " Instance"));
	instance->m_Owner = &owner;
	return *instance;
}

const VertexStreamsLayout& Mesh::GetStreamsLayout() const
{
	if (!m_Skin.empty() || GetBlendShapeChannelCount() != 0)
		return VertexDataInfo::kVertexStreamsSkinnedHotColdSplit;
	else
		return VertexDataInfo::kVertexStreamsDefault;
}

const VertexChannelsLayout& Mesh::GetChannelsLayout() const
{
	UInt8 compressed = m_StreamCompression;
#if !UNITY_EDITOR
	// Editor only does build step for compression and never draws float16 vertices
	if (!gGraphicsCaps.has16BitFloatVertex)
	{
		compressed = kStreamCompressionDefault;
	}
#endif
	switch (compressed)
	{
		default: // fall through
		case kStreamCompressionDefault:
			return VertexDataInfo::kVertexChannelsDefault;
		case kStreamCompressionCompressed:
			return VertexDataInfo::kVertexChannelsCompressed;
		case kStreamCompressionCompressedAggressive:
			return VertexDataInfo::kVertexChannelsCompressedAggressive;
	}
}

void Mesh::InitVertexBufferData( UInt32 wantedChannels )
{
#if GFX_CAN_UNLOAD_MESH_DATA
	// If data was uploaded and freed we cannot update it.
	if (!HasVertexData())
		return;
#endif
	UInt32 presentChannels = GetAvailableChannels ();

	// Modify the vertex buffer before fetching any channel pointers, as modifying the format reallocates the buffer and pointers
	// are invalidated. Due to possible format changes, also fetch the stride sizes only after buffer reformatting.
	unsigned initChannels = 0;

	// Silently create an all-white color array if shader wants colors, but mesh does not have them.
	// On D3D, some runtime/driver combinations will crash if a vertex shader wants colors but does not
	// have them (e.g. Vista drivers for Intel 965). In other cases it will default to white for fixed function
	// pipe, and to undefined value for vertex shaders, which is not good either.
	if( (wantedChannels & VERTEX_FORMAT1(Color)) && !(presentChannels & VERTEX_FORMAT1(Color)) )
		initChannels |= VERTEX_FORMAT1(Color);

#if UNITY_PEPPER
	// Pepper OpenGL implementation fails to draw anything if any channel is missing.
	if( (wantedChannels & VERTEX_FORMAT1(Tangent)) && !(presentChannels & VERTEX_FORMAT1(Tangent)) )
		initChannels |= VERTEX_FORMAT1(Tangent);
#endif

	if ((initChannels & presentChannels) != initChannels)
	{
		FormatVertices (presentChannels | initChannels);
		InitChannelsToDefault (0, GetVertexCount (), initChannels);
	}
}

void Mesh::GetVertexBufferData( VertexBufferData& buffer, UInt32 wantedChannels )
{
	InitVertexBufferData(wantedChannels);

	for (int i = 0; i < kShaderChannelCount; i++)
		buffer.channels[i] = m_VertexData.GetChannel(i);

	for (int i = 0; i < kMaxVertexStreams; i++)
		buffer.streams[i] = m_VertexData.GetStream(i);

	int srcTexcoord = kShaderChannelNone;
	for (int i = kShaderChannelTexCoord0; i <= kShaderChannelTexCoord1; i++)
	{
		if (buffer.channels[i].IsValid())
		{
			// We have a valid texcoord
			srcTexcoord = i;
			continue;
		}
		UInt32 channelMask = 1 << i;
		if (srcTexcoord != kShaderChannelNone)
		{
			// Replicate last valid texture coord
			const ChannelInfo& srcChannel = buffer.channels[srcTexcoord];
			buffer.channels[i] = srcChannel;
			buffer.streams[srcChannel.stream].channelMask |= channelMask;
		}
	}

	// Data pointer can be NULL if we are only updating declaration of uploaded VBO
	buffer.buffer = m_VertexData.GetDataPtr();
	buffer.bufferSize = m_VertexData.GetDataSize();
	buffer.vertexCount = GetVertexCount();

#if UNITY_EDITOR
	#define LogStringObjectEditor(x) LogStringObject(Format(x, GetName()),this)

	if (Camera::ShouldShowChannelErrors(GetCurrentCameraPtr()))
	{
		const ChannelInfo* channels = buffer.channels;

		if ((wantedChannels & VERTEX_FORMAT1(Tangent)) && !channels[kShaderChannelTangent].IsValid())
			LogStringObjectEditor ("Shader wants tangents, but the mesh %s doesn't have them");

		if ((wantedChannels & VERTEX_FORMAT1(Normal)) && !channels[kShaderChannelNormal].IsValid())
			LogStringObjectEditor ("Shader wants normals, but the mesh %s doesn't have them");

		if ((wantedChannels & VERTEX_FORMAT1(TexCoord0)) && !channels[kShaderChannelTexCoord0].IsValid())
				LogStringObjectEditor ("Shader wants texture coordinates, but the mesh %s doesn't have them");

		if ((wantedChannels & VERTEX_FORMAT1(TexCoord1)) && !channels[kShaderChannelTexCoord1].IsValid())
			LogStringObjectEditor ("Shader wants secondary texture coordinates, but the mesh %s doesn't have any");

		if ((wantedChannels & VERTEX_FORMAT1(Color)) && !channels[kShaderChannelColor].IsValid())
			LogStringObjectEditor ("Shader wants vertex colors, and failed to create a vertex color array");
	}
	#undef LogStringObjectEditor
#endif

#if UNITY_PS3
	if(m_PartitionInfos.empty())
	{
		int submeshCount = m_SubMeshes.size();
		for (int submesh=0; submesh<submeshCount; submesh++)
		{
			SubMesh& sm = GetSubMeshFast(submesh);

			MeshPartitionInfo partInfo;
			partInfo.submeshStart = submesh;
			partInfo.partitionCount = 1;
			buffer.partInfo.push_back(partInfo);

			MeshPartition part;
			part.vertexCount = sm.vertexCount;
			part.vertexOffset = 0;
			part.indexCount = sm.indexCount;
			part.indexByteOffset = sm.firstByte;
			buffer.partitions.push_back(part);;
		}
	}
	else
	{
		buffer.partInfo = m_PartitionInfos;
		buffer.partitions = m_Partitions;
	}

#endif

	buffer.vertexCount = GetVertexCount ();
}

void Mesh::GetIndexBufferData (IndexBufferData& buffer)
{
	DebugAssert (!m_IndexBuffer.empty());
	buffer.indices = m_IndexBuffer.empty() ? NULL : (void*)&m_IndexBuffer[0];

	///@TODO: HACK for now to get index buffers working, without changing a lot of vbo code
	// We should be passing the byte size not the number of indices
	buffer.count = GetTotalndexCount();
	buffer.hasTopologies = 0;
	for (size_t i = 0, n = m_SubMeshes.size(); i < n; ++i)
	{
		buffer.hasTopologies |= (1<<m_SubMeshes[i].topology);
	}
}

PROFILER_INFORMATION(gCreateVBOProfile, "Mesh.CreateVBO", kProfilerRender);
PROFILER_INFORMATION(gAwakeFromLoadMesh, "Mesh.AwakeFromLoad", kProfilerLoading);
PROFILER_INFORMATION(gUploadMeshDataMesh, "Mesh.UploadMeshData", kProfilerLoading);

VBO* Mesh::GetSharedVBO( UInt32 wantedChannels )
{
	// Some badly written shaders have no Bind statements in the vertex shaders parts;
	// and only happened to work before by accident. If requiredChannels turns out to be
	// zero, let's pretend it did request at least position.
	if (wantedChannels == 0)
		wantedChannels = (1<<kShaderChannelVertex);

	UInt32 newChannels = wantedChannels | m_ChannelsInVBO;
	bool addedChannels = newChannels != m_ChannelsInVBO;

#if GFX_CAN_UNLOAD_MESH_DATA
	if (!m_IsReadable && !m_KeepVertices && m_VBO)
	{
		// Everything is already prepared, just return VBO
		return m_VBO;
	}
#endif

	if ((GFX_ALL_BUFFERS_CAN_BECOME_LOST || m_IsDynamic) && m_VBO && m_VBO->IsVertexBufferLost())
		m_VerticesDirty = true;
	if (GFX_ALL_BUFFERS_CAN_BECOME_LOST && m_VBO && m_VBO->IsIndexBufferLost())
		m_IndicesDirty = true;

	if (addedChannels || m_VerticesDirty || m_IndicesDirty)
		CreateSharedVBO(wantedChannels);

	return m_VBO;
}

void Mesh::CreateSharedVBO( UInt32 wantedChannels )
{
	if (m_IndexBuffer.empty())
	{
		if (m_VBO)
		{
			GetGfxDevice().DeleteVBO(m_VBO);
			m_VBO = NULL;
		}
		return;
	}

	PROFILER_BEGIN(gCreateVBOProfile, this)
	SET_ALLOC_OWNER(this);

	if (!m_VBO)
	{
		m_VBO = GetGfxDevice().CreateVBO();
		m_VBO->SetHideFromRuntimeStats(m_HideFromRuntimeStats);
	}

	UInt32 newChannels = wantedChannels | m_ChannelsInVBO;
	if (m_VerticesDirty || newChannels != m_ChannelsInVBO)
	{
		if (m_IsDynamic)
			m_VBO->SetVertexStreamMode(0, VBO::kStreamModeDynamic);

		VertexBufferData vertexBuffer;
		GetVertexBufferData (vertexBuffer, newChannels);
		m_VBO->UpdateVertexData (vertexBuffer);
	}

	if (m_IndicesDirty)
	{
		// TODO: probably add separate script access to set vertex/index dynamic
		if (m_IsDynamic)
			m_VBO->SetIndicesDynamic(true);

		IndexBufferData indexBuffer;
		GetIndexBufferData (indexBuffer);
		m_VBO->UpdateIndexData (indexBuffer);
	}

	m_VerticesDirty = false;
	m_IndicesDirty = false;
	m_ChannelsInVBO = newChannels;

	PROFILER_END
}

bool Mesh::CopyToVBO ( UInt32 wantedChannels, VBO& vbo )
{
	if( m_IndexBuffer.empty() )
		return false;

	PROFILER_BEGIN(gCreateVBOProfile, this)

	VertexBufferData vertexBuffer;
	GetVertexBufferData( vertexBuffer, wantedChannels );
	vbo.UpdateVertexData( vertexBuffer );

	IndexBufferData indexBuffer;
	GetIndexBufferData (indexBuffer);
	vbo.UpdateIndexData (indexBuffer);
#if UNITY_XENON
	if( m_VBO )
		vbo.CopyExtraUvChannels( m_VBO );
#endif
	PROFILER_END

	return true;
}


void Mesh::UnloadVBOFromGfxDevice()
{
	if (m_VBO)
	{
		WaitOnRenderThreadUse();
		GetGfxDevice().DeleteVBO (m_VBO);
	}
	m_VBO = NULL;
	m_ChannelsInVBO = 0;
	m_VerticesDirty = m_IndicesDirty = true;
#if ENABLE_MULTITHREADED_CODE
	m_CurrentCPUFence = 0;
	m_WaitOnCPUFence = false;
#endif
}

void Mesh::ReloadVBOToGfxDevice()
{
	const bool needReloadFromDisk = (!m_IsReadable && !HasVertexData());
	if (needReloadFromDisk)
	{
		GetPersistentManager().ReloadFromDisk(this);
	}
	else
	{
		m_ChannelsInVBO = 0;
		m_VerticesDirty = m_IndicesDirty = true;
	}
	SwizzleVertexColorsIfNeeded();
}


bool Mesh::ExtractTriangle (UInt32 face, UInt32* indices) const
{
	///@TODO: OPTIMIZE this away
	TemporaryIndexContainer triangles;
	GetTriangles(triangles);
	if (face * 3 > triangles.size ())
		return false;

	indices[0] = triangles[face * 3 + 0];
	indices[1] = triangles[face * 3 + 1];
	indices[2] = triangles[face * 3 + 2];
	return true;
}

static void TransformNormals (const Matrix3x3f& invTranspose, StrideIterator<Vector3f> inNormals, StrideIterator<Vector3f> inNormalsEnd, StrideIterator<Vector3f> outNormals)
{
	for (; inNormals != inNormalsEnd; ++inNormals, ++outNormals)
		*outNormals = NormalizeSafe (invTranspose.MultiplyVector3 (*inNormals));
}

static void TransformTangents (const Matrix3x3f& invTranspose, StrideIterator<Vector4f> inTangents, StrideIterator<Vector4f> inTangentsEnd, StrideIterator<Vector4f> outTangents)
{
	for ( ; inTangents != inTangentsEnd; ++inTangents, ++outTangents)
	{
		Vector3f tangent = Vector3f(inTangents->x,inTangents->y,inTangents->z);
		Vector3f normalized = NormalizeSafe (invTranspose.MultiplyVector3 (tangent));
		*outTangents = Vector4f(normalized.x, normalized.y ,normalized.z, inTangents->w);
	}
}

void Mesh::CopyTransformed (const Mesh& mesh, const Matrix4x4f& transform)
{
	int vertexCount = mesh.GetVertexCount();
	unsigned outVertexFormat = mesh.GetAvailableChannelsForRendering ();

	ResizeVertices(mesh.GetVertexCount (), outVertexFormat);

	if (outVertexFormat & VERTEX_FORMAT1(Vertex))
	TransformPoints3x4 (transform,
						(Vector3f*)mesh.GetChannelPointer (kShaderChannelVertex), mesh.GetStride (kShaderChannelVertex),
						(Vector3f*)GetChannelPointer (kShaderChannelVertex), GetStride (kShaderChannelVertex),
						vertexCount);

	Matrix3x3f invTranspose3x3 = Matrix3x3f(transform); invTranspose3x3.InvertTranspose ();

	if (outVertexFormat & VERTEX_FORMAT1(Normal))
	TransformNormals (invTranspose3x3, mesh.GetNormalBegin (), mesh.GetNormalEnd (), GetNormalBegin ());
	if (outVertexFormat & VERTEX_FORMAT1(Tangent))
	TransformTangents (invTranspose3x3, mesh.GetTangentBegin (), mesh.GetTangentEnd (), GetTangentBegin ());

	m_IndexBuffer = mesh.m_IndexBuffer;
	m_SubMeshes = mesh.m_SubMeshes;
	m_Skin = mesh.m_Skin;
	if (outVertexFormat & VERTEX_FORMAT1(TexCoord0))
	strided_copy (mesh.GetUvBegin (0), mesh.GetUvEnd (0), GetUvBegin (0));
	if (outVertexFormat & VERTEX_FORMAT1(TexCoord1))
	strided_copy (mesh.GetUvBegin (1), mesh.GetUvEnd (1), GetUvBegin (1));
	if (outVertexFormat & VERTEX_FORMAT1(Color))
	strided_copy (mesh.GetColorBegin (), mesh.GetColorEnd (), GetColorBegin ());
	m_VertexColorsSwizzled = mesh.m_VertexColorsSwizzled;
	m_LocalAABB = mesh.m_LocalAABB;

	SetChannelsDirty( outVertexFormat, true );
	ClearSkinCache();
}


void Mesh::SetChannelsDirty (unsigned vertexChannelsChanged, bool indices)
{
	SetDirty();

	m_VerticesDirty |= vertexChannelsChanged != 0;
	m_IndicesDirty |= indices;

	// We should regenreate physics mesh only if verex data have changed
	if ((vertexChannelsChanged & VERTEX_FORMAT1(Vertex)) || indices)
	{
		m_CollisionMesh.VertexDataHasChanged();
		m_CachedBonesAABB.clear();
	}
	NotifyObjectUsers( kDidModifyMesh );
}

bool Mesh::SetBoneWeights (const BoneInfluence* v, int count)
{
	WaitOnRenderThreadUse();
	ClearSkinCache();
	if (count == 0)
	{
		m_Skin.clear();
		UpdateVertexFormat();
		return true;
	}

	if (count != GetVertexCount ())
	{
		ErrorString("Mesh.boneWeights is out of bounds. The supplied array needs to be the same size as the Mesh.vertices array.");
		return false;
	}
	m_Skin.assign(v, v + count);
	SetChannelsDirty (0, false);
	UpdateVertexFormat();

	return true;
}

static void ComputeBoneBindPoseAABB (const Matrix4x4f* bindPoses, size_t bindPoseCount, const StrideIterator<Vector3f> vertices, const BoneInfluence* influences, size_t vertexCount, const BlendShapeVertices& blendShapeVertices, MinMaxAABB* outputBounds)
{
	if (blendShapeVertices.empty())
	{
		for(int v=0;v<vertexCount;v++)
		{
			const Vector3f& vert = vertices[v];
			for (int i = 0; i < 4; i++)
			{
				if(influences[v].weight[i] > 0.0f)
				{
					const UInt32 boneIndex = influences[v].boneIndex[i];

					outputBounds[boneIndex].Encapsulate(bindPoses[boneIndex].MultiplyPoint3(vert));
				}
			}
		}
	}
	else
	{
		Vector3f* minVertices;
		ALLOC_TEMP(minVertices, Vector3f, vertexCount);
		Vector3f* maxVertices;
		ALLOC_TEMP(maxVertices, Vector3f, vertexCount);

		strided_copy(vertices, vertices + vertexCount, minVertices);
		strided_copy(vertices, vertices + vertexCount, maxVertices);

		for (int i=0;i<blendShapeVertices.size();i++)
	{
			int index = blendShapeVertices[i].index;
			Vector3f pos = blendShapeVertices[i].vertex + vertices[index];
			maxVertices[index]  = max (maxVertices[index], pos);
			minVertices[index]  = min (minVertices[index], pos);
		}

		for(int v=0;v<vertexCount;v++)
		{
        for (int i = 0; i < 4; i++)
        {
				if(influences[v].weight[i] > 0.0f)
            {
					const UInt32 boneIndex = influences[v].boneIndex[i];
					outputBounds[boneIndex].Encapsulate(bindPoses[boneIndex].MultiplyPoint3(minVertices[v]));
					outputBounds[boneIndex].Encapsulate(bindPoses[boneIndex].MultiplyPoint3(maxVertices[v]));
				}
			}
		}
	}
}

const Mesh::AABBContainer& Mesh::GetCachedBonesBounds()
{
	// Use cached result if it has the correct size (including empty)
	if (m_CachedBonesAABB.size() == m_Bindpose.size())
		return m_CachedBonesAABB;

	Assert(GetMaxBoneIndex() < m_Bindpose.size());

	m_CachedBonesAABB.resize_initialized(m_Bindpose.size(), MinMaxAABB());

	ComputeBoneBindPoseAABB (GetBindposes(), m_CachedBonesAABB.size(), GetVertexBegin(), m_Skin.begin(), GetVertexCount(), m_Shapes.vertices, &m_CachedBonesAABB[0]);

	return m_CachedBonesAABB;
}

void Mesh::ClearSkinCache ()
{
	m_CachedBonesAABB.clear();
	m_CachedSkin2.clear();
	m_CachedSkin1.clear();
	m_MaxBoneIndex = -1;
}

int Mesh::GetMaxBoneIndex ()
{
	if (m_MaxBoneIndex != -1)
		return m_MaxBoneIndex;

	m_MaxBoneIndex = 0;
	for (int i=0;i<m_Skin.size();i++)
	{
		m_MaxBoneIndex = max(m_MaxBoneIndex, m_Skin[i].boneIndex[0]);
		m_MaxBoneIndex = max(m_MaxBoneIndex, m_Skin[i].boneIndex[1]);
		m_MaxBoneIndex = max(m_MaxBoneIndex, m_Skin[i].boneIndex[2]);
		m_MaxBoneIndex = max(m_MaxBoneIndex, m_Skin[i].boneIndex[3]);
	}

	return m_MaxBoneIndex;
}

void* Mesh::GetSkinInfluence (int count)
{
	if (!m_Skin.empty())
	{
		BoneInfluence* bones4 = &m_Skin[0];
		if (count == 1)
		{
			if (!m_CachedSkin1.empty())
				return &m_CachedSkin1[0];

			// Cache 1 bone skin weights
			int size = m_Skin.size();
			m_CachedSkin1.resize_uninitialized(size);

			int* bones1 = &m_CachedSkin1[0];
			for (int i=0;i<size;i++)
				bones1[i] = bones4[i].boneIndex[0];
			return bones1;

		}
		else if (count == 2)
		{
			if (!m_CachedSkin2.empty ())
				return &m_CachedSkin2[0];

			// Cache 2 bone skin weights
			int size = m_Skin.size();
			m_CachedSkin2.resize_uninitialized(size);

			BoneInfluence2* bones2 = &m_CachedSkin2[0];
			for (int i=0;i<size;i++)
			{
				bones2[i].boneIndex[0] = bones4[i].boneIndex[0];
				bones2[i].boneIndex[1] = bones4[i].boneIndex[1];

				float invSum = 1.0F / (bones4[i].weight[0] + bones4[i].weight[1]);
				bones2[i].weight[0] = bones4[i].weight[0] * invSum;
				bones2[i].weight[1] = bones4[i].weight[1] * invSum;
			}
			return bones2;
		}
		else if (count == 4)
		{
			return bones4;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}


int Mesh::GetRuntimeMemorySize () const
{
	int size = Super::GetRuntimeMemorySize();

	#if ENABLE_PROFILER
	if (m_VBO)
		size += m_VBO->GetRuntimeMemorySize();
	#endif

	return size;
}


void* Mesh::GetSharedNxMesh ()
{
	return m_CollisionMesh.GetSharedNxMesh (*this);
}

void* Mesh::GetSharedNxConvexMesh ()
{
	return m_CollisionMesh.GetSharedNxConvexMesh (*this);
}

void Mesh::UploadMeshData(bool markNoLongerReadable)
{
	if(markNoLongerReadable)
		m_IsReadable = false;

	ClearSkinCache();
	UpdateVertexFormat();

	// prepare VBO
	UInt32 channelMask = GetAvailableChannelsForRendering();

	// Create color channel in case it's needed by shader (and we can't patch it)
#if GFX_CAN_UNLOAD_MESH_DATA
	bool unloadData = !m_IsReadable && m_Skin.empty();
	if (unloadData && !m_KeepVertices)
		channelMask |= VERTEX_FORMAT1(Color);
#endif

	// Shared VBO is not required for skinned meshes (unless used as non-skinned)
	if (m_Skin.empty())
		CreateSharedVBO(channelMask);

#if GFX_CAN_UNLOAD_MESH_DATA
	if (unloadData)
	{
		if (!m_KeepVertices && m_VBO && !m_VBO->IsUsingSourceVertices())
		{
			Assert(m_Skin.empty());
			m_VertexData.Deallocate();
			m_VBO->UnloadSourceVertices();
		}
		if (!m_KeepIndices && m_VBO && !m_VBO->IsUsingSourceIndices())
		{
#if UNITY_METRO
			m_IndexBuffer.clear();
			m_IndexBuffer.shrink_to_fit();
#else
			// On Metro this throws "Expression: vector containers incompatible for swap" when compiling in VS 2013, works okay if compiling in VS 2012
			// Case 568418
			IndexContainer emptyIndices;
			m_IndexBuffer.swap(emptyIndices);
#endif
		}
	}
#endif
}

void Mesh::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	PROFILER_AUTO(gAwakeFromLoadMesh, this)

	Super::AwakeFromLoad(awakeMode);
	m_CollisionMesh.AwakeFromLoad(awakeMode);

	UploadMeshData(!m_IsReadable);

	if (m_InternalMeshID == 0)
		m_InternalMeshID = s_MeshIDGenerator.AllocateID ();
}

void Mesh::AwakeFromLoadThreaded()
{
	Super::AwakeFromLoadThreaded();
	m_CollisionMesh.AwakeFromLoadThreaded(*this);
}

void Mesh::MarkDynamic()
{
	// Optimize for frequent updates
	m_IsDynamic = true;
}

void Mesh::UpdateVertexFormat()
{
	// Make sure vertex streams are in the format we want for rendering
	// This will also handle decompression of unsupported vertex formats
	FormatVertices(GetAvailableChannels());
	SwizzleVertexColorsIfNeeded();
}

bool Mesh::ShouldIgnoreInGarbageDependencyTracking ()
{
	return true;
}

UInt32 Mesh::GetAvailableChannels() const
{
	return m_VertexData.GetChannelMask ();
}

UInt32 Mesh::GetAvailableChannelsForRendering() const
{
	unsigned availChannels = m_VertexData.GetChannelMask ();
	return availChannels;
}

bool Mesh::IsSuitableSizeForDynamicBatching () const
{
	// If any submesh has too many vertices, don't keep mesh data for batching
	for (size_t i = 0; i < GetSubMeshCount(); i++)
	{
		if (m_SubMeshes[i].vertexCount > kDynamicBatchingVerticesThreshold)
			return false;
	}
	return true;
}

void Mesh::CheckConsistency()
{
	Super::CheckConsistency();

	for (int i = 0; i < m_SubMeshes.size(); ++i)
	{
		Assert(m_SubMeshes[i].topology != kPrimitiveTriangleStripDeprecated);
	}
}

void Mesh::SwapBlendShapeData (BlendShapeData& shapes)
{
	WaitOnRenderThreadUse();

//	swap (m_Shapes, shapes);
	m_Shapes = shapes;

	NotifyObjectUsers( kDidModifyMesh );
}
