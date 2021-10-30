#ifndef LODMESH_H
#define LODMESH_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector4.h"
#include "Mesh.h"
#include "Runtime/Math/Color.h"
#include <string>
#include <vector>
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/Shaders/VBO.h"
#include "CompressedMesh.h"
#include "VertexData.h"
#include "Runtime/Dynamics/CollisionMeshData.h"
#include "MeshBlendShape.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Camera/IntermediateUsers.h"

class IntermediateRenderer;

struct SubMesh
{
	UInt32 firstByte;
	UInt32 indexCount;
	GfxPrimitiveType topology;

	UInt32 firstVertex;
	UInt32 vertexCount;
	AABB   localAABB;

	SubMesh ()
	{
		firstByte = 0;
		indexCount = 0;
		topology = kPrimitiveTriangles;
		firstVertex = 0;
		vertexCount = 0;
		localAABB = AABB (Vector3f::zero, Vector3f::zero);
	}

	DECLARE_SERIALIZE_NO_PPTR (SubMesh)

#if SUPPORT_SERIALIZED_TYPETREES
	template<class TransferFunction>
	void TransferWorkaround35SerializationFuckup (TransferFunction& transfer);
#endif
};

/// typedef for tangent space lighting rotations
typedef std::vector<DeprecatedTangent, STL_ALLOCATOR(kMemGeometry, DeprecatedTangent) > DeprecatedTangentsArray;

template<class TransferFunc>
void SubMesh::Transfer (TransferFunc& transfer)
{
	#if SUPPORT_SERIALIZED_TYPETREES
	if (transfer.GetFlags() & kWorkaround35MeshSerializationFuckup)
	{
		TransferWorkaround35SerializationFuckup (transfer);
		return;
	}
	#endif

	transfer.SetVersion (2);
	TRANSFER(firstByte);
	TRANSFER(indexCount);
	TRANSFER_ENUM(topology);
	TRANSFER(firstVertex);
	TRANSFER(vertexCount);
	TRANSFER(localAABB);
	if (transfer.IsOldVersion(1))
	{
		UInt32 triStrip;
		transfer.Transfer (triStrip, "isTriStrip");
		topology = triStrip ? kPrimitiveTriangleStripDeprecated : kPrimitiveTriangles;
	}
}

#if SUPPORT_SERIALIZED_TYPETREES
template<class TransferFunc>
void SubMesh::TransferWorkaround35SerializationFuckup (TransferFunc& transfer)
{
	TRANSFER(firstByte);
	TRANSFER(indexCount);
	
	UInt32 triStrip;
	transfer.Transfer (triStrip, "isTriStrip");
	topology = triStrip ? kPrimitiveTriangleStripDeprecated : kPrimitiveTriangles;
	
	UInt32 triangleCount;
	transfer.Transfer (triangleCount, "triangleCount");

	TRANSFER(firstVertex);
	TRANSFER(vertexCount);
	TRANSFER(localAABB);
}
#endif

template<class TransferFunc>
void MeshPartition::Transfer (TransferFunc& transfer)
{
	TRANSFER(vertexCount);
	TRANSFER(vertexOffset);
	TRANSFER(indexCount);
	TRANSFER(indexByteOffset);
}

template<class TransferFunc>
void MeshPartitionInfo::Transfer (TransferFunc& transfer)
{
	TRANSFER(submeshStart);
	TRANSFER(partitionCount);
}

class EXPORT_COREMODULE Mesh : public NamedObject
{
public:
	enum
	{
	#if UNITY_IPHONE || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
		alignBoneContainer = 16,
	#else
		alignBoneContainer = kDefaultMemoryAlignment,
	#endif
	};

	//mircea@INFO PS3 doesn't render from VBOs hence m_VertexData and m_IndexBuffer *have* to be allocated with kMemVertexData.
	typedef UNITY_VECTOR(kMemVertexData, UInt8)				IndexContainer;
	typedef UNITY_VECTOR(kMemGeometry, SubMesh)				SubMeshContainer;
	typedef dynamic_array<Matrix4x4f>						MatrixContainer;
	typedef dynamic_array<int>								SkinContainer;
	typedef UNITY_VECTOR(kMemGeometry, UInt32)				CollisionTriangleContainer;
	typedef dynamic_array<MinMaxAABB>						AABBContainer;

	typedef dynamic_array<BoneInfluence, alignBoneContainer>  BoneInfluenceContainer;
	typedef dynamic_array<BoneInfluence2, alignBoneContainer> BoneInfluence2Container;

	typedef UNITY_TEMP_VECTOR(UInt32)		TemporaryIndexContainer;

#if UNITY_PS3 || UNITY_EDITOR
	typedef UNITY_VECTOR(kMemVertexData, MeshPartition)		MeshPartitionContainer;
	typedef UNITY_VECTOR(kMemVertexData, MeshPartitionInfo) MeshPartitionInfoContainer;
#endif

	REGISTER_DERIVED_CLASS (Mesh, NamedObject)
	DECLARE_OBJECT_SERIALIZE (Mesh)

	Mesh (MemLabelId label, ObjectCreationMode mode);
	// ~Mesh (); declared-by-macro

public:

	virtual int GetRuntimeMemorySize () const;

	VBO* GetSharedVBO( UInt32 wantedChannels );
	bool CopyToVBO ( UInt32 wantedChannels, VBO& vbo );
	void InitVertexBufferData ( UInt32 wantedChannels );
	void GetVertexBufferData ( VertexBufferData& buffer, UInt32 wantedChannels );
	void GetIndexBufferData (IndexBufferData& buffer);
	void UnloadVBOFromGfxDevice();
	void ReloadVBOToGfxDevice();


	void AwakeFromLoad(AwakeFromLoadMode mode);
	void AwakeFromLoadThreaded();
	void UploadMeshData(bool markNoLongerReadable);
	
	virtual bool MainThreadCleanup ();
	
	void MarkDynamic();
	void UpdateVertexFormat();

	void SetBounds (const AABB& aabb );
	const AABB& GetBounds () const { return m_LocalAABB; }

	void SetBounds (unsigned submesh, const AABB& aabb );
	const AABB& GetBounds (unsigned submesh) const
	{
		DebugAssertIf(submesh >= m_SubMeshes.size());
		return m_SubMeshes[submesh].localAABB;
	}

	void Clear (bool keepVertexLayout);

	/// Recalculate the bounding volume
	void RecalculateBounds ();
	void RecalculateSubmeshBounds (unsigned submesh);

	// Recalculate normals
	void RecalculateNormals();
	void RecalculateNormalsWithHardAngle( float hardAngle );

	// Validate that there are no out of bounds indices in the triangles
	bool ValidateVertexCount (unsigned newVertexCount, const void* newTriangles, unsigned indexCount);

	int GetVertexCount () const { return m_VertexData.GetVertexCount (); }

	// Gets count in all submeshes.
	int GetPrimitiveCount() const;
	int CalculateTriangleCount() const; // ignores degenerates in strips

	// NOTE: make sure to call SetChannelDirty and RecalculateBounds when changing the geometry!
	StrideIterator<Vector3f> GetVertexBegin () const { return m_VertexData.MakeStrideIterator<Vector3f> (kShaderChannelVertex); }
	StrideIterator<Vector3f> GetVertexEnd () const { return m_VertexData.MakeEndIterator<Vector3f> (kShaderChannelVertex); }

	StrideIterator<Vector3f> GetNormalBegin () const { return m_VertexData.MakeStrideIterator<Vector3f> (kShaderChannelNormal); }
	StrideIterator<Vector3f> GetNormalEnd () const { return m_VertexData.MakeEndIterator<Vector3f> (kShaderChannelNormal); }

	StrideIterator<ColorRGBA32> GetColorBegin () const { return m_VertexData.MakeStrideIterator<ColorRGBA32> (kShaderChannelColor); }
	StrideIterator<ColorRGBA32> GetColorEnd () const { return m_VertexData.MakeEndIterator<ColorRGBA32> (kShaderChannelColor); }

	StrideIterator<Vector2f> GetUvBegin (int uvIndex = 0) const { return m_VertexData.MakeStrideIterator<Vector2f> ((ShaderChannel)(kShaderChannelTexCoord0 + uvIndex)); }
	StrideIterator<Vector2f> GetUvEnd (int uvIndex = 0) const { return m_VertexData.MakeEndIterator<Vector2f> ((ShaderChannel)(kShaderChannelTexCoord0 + uvIndex)); }

	StrideIterator<Vector4f> GetTangentBegin () const { return m_VertexData.MakeStrideIterator<Vector4f> (kShaderChannelTangent); }
	StrideIterator<Vector4f> GetTangentEnd () const { return m_VertexData.MakeEndIterator<Vector4f> (kShaderChannelTangent); }

	void ExtractVertexArray (Vector3f* destination) const;
	void ExtractNormalArray (Vector3f* destination) const;
	void ExtractColorArray (ColorRGBA32* destination) const;
	void ExtractColorArrayConverting (ColorRGBAf* destination) const;
	void ExtractUvArray (int uvIndex, Vector2f* destination) const;
	void ExtractTangentArray (Vector4f* destination) const;

	void SetVertices (Vector3f const* data, size_t count);
	void SetNormals (Vector3f const* data, size_t count);
	void SetTangents (Vector4f const* data, size_t count);
	void SetUv (int uvIndex, Vector2f const* data, size_t count);
	void SetColors (ColorRGBA32 const* data, size_t count);
	void SetColorsConverting (ColorRGBAf const* data, size_t count);

	bool GetVertexColorsSwizzled() const { return m_VertexColorsSwizzled; }
	void SetVertexColorsSwizzled(bool flag) { m_VertexColorsSwizzled = flag; }
	bool HasVertexData () const { return m_VertexData.GetDataPtr () != NULL; }
	void* GetVertexDataPointer () const { return m_VertexData.GetDataPtr (); }
	size_t GetVertexDataSize () const { return m_VertexData.GetDataSize (); }
	size_t GetVertexSize () const { return m_VertexData.GetVertexSize(); }

	const void* GetChannelPointer (ShaderChannel channel) const { return m_VertexData.GetDataPtr () + m_VertexData.GetChannelOffset (channel); }
	void* GetChannelPointer (ShaderChannel channel) { return m_VertexData.GetDataPtr () + m_VertexData.GetChannelOffset (channel); }
	void* GetChannelPointer (ShaderChannel channel, size_t offsetInElements) { return m_VertexData.GetDataPtr () + m_VertexData.GetChannelOffset (channel) + offsetInElements * m_VertexData.GetChannelStride(channel); }
	size_t GetStride (ShaderChannel channel) const { return m_VertexData.GetChannelStride(channel); }

	bool IsAvailable (ShaderChannel channel) const { return m_VertexData.HasChannel (channel); }
	// returns a bitmask of a newly created channels
	UInt32 ResizeVertices (size_t count, UInt32 shaderChannels, const VertexStreamsLayout& streams, const VertexChannelsLayout& channels);
	UInt32 ResizeVertices (size_t count, UInt32 shaderChannels) { return ResizeVertices(count, shaderChannels, GetStreamsLayout(), GetChannelsLayout()); }

	// returns a bitmask of a newly created channels
	UInt32 FormatVertices (UInt32 shaderChannels);
	// initializes the specified channels to default values
	void InitChannelsToDefault (unsigned begin, unsigned count, unsigned shaderChannels);

	bool SetBoneWeights (const BoneInfluence* v, int count);
	const BoneInfluence* GetBoneWeights () const { return m_Skin.empty() ? NULL : &m_Skin[0]; }
	BoneInfluence* GetBoneWeights () { return m_Skin.empty() ? NULL : &m_Skin[0]; }
	void ClearSkinCache ();
	int GetMaxBoneIndex ();

	const Matrix4x4f* GetBindposes () const { return m_Bindpose.empty() ? NULL : &m_Bindpose[0]; }
	int GetBindposeCount () const { return m_Bindpose.size(); }
	void SetBindposes (const Matrix4x4f* bindposes, int count);

	bool SetIndices (const UInt32* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology);
	bool SetIndices (const UInt16* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology);

	void GetTriangles (TemporaryIndexContainer& triangles, unsigned submesh) const;
	void GetTriangles (TemporaryIndexContainer& triangles) const;
	void AppendTriangles (TemporaryIndexContainer& triangles, unsigned submesh) const;
	void GetStrips (TemporaryIndexContainer& triangles, unsigned submesh) const;
	void GetIndices (TemporaryIndexContainer& triangles, unsigned submesh) const;

	enum {
		k16BitIndices = 1 << 0,
		kRebuildCollisionTriangles = 1 << 2,
		kDontAssignIndices = 1 << 3,
		kDontSupportSubMeshVertexRanges = 1 << 4
	};
	bool SetIndicesComplex (const void* indices, unsigned count, unsigned submesh, GfxPrimitiveType topology, int mode);

	bool ExtractTriangle (UInt32 face, UInt32* indices) const;

	void SetSubMeshCount (unsigned int count);
	size_t GetSubMeshCount () const;

	void UpdateSubMeshVertexRange (int index);

	void AddObjectUser( ListNode<Object>& node ) { m_ObjectUsers.push_back(node); }
	void AddIntermediateUser( ListNode<IntermediateRenderer>& node ) { m_IntermediateUsers.AddUser(node); }
	
	const BlendShapeData& GetBlendShapeData() const { return m_Shapes; }
	size_t GetBlendShapeChannelCount() const { return m_Shapes.channels.size(); }
	void SwapBlendShapeData (BlendShapeData& shapes);
	

	BlendShapeData& GetWriteBlendShapeDataInternal() { return m_Shapes; }

	
	void CheckConsistency();

#if ENABLE_MULTITHREADED_CODE
	void SetCurrentCPUFence( UInt32 fence ) { m_CurrentCPUFence = fence; m_WaitOnCPUFence = true; }
#endif

	void WaitOnRenderThreadUse();

	static Mesh& GetInstantiatedMesh (Mesh* mesh, Object& owner);

	void CopyTransformed (const Mesh& mesh, const Matrix4x4f& transform);

	void SetChannelsDirty (unsigned vertexChannelsChanged, bool indices);

	void* GetSharedNxMesh ();
	void* GetSharedNxConvexMesh ();

	void RebuildCollisionTriangles();

	const SubMesh& GetSubMeshFast (unsigned int submesh) const
	{
		DebugAssertIf(submesh >= m_SubMeshes.size());
		return m_SubMeshes[submesh];
	}
	SubMesh& GetSubMeshFast (unsigned int submesh)
	{
		DebugAssertIf(submesh >= m_SubMeshes.size());
		return m_SubMeshes[submesh];
	}

	const UInt16* GetSubMeshBuffer16 (int submesh) const;
	UInt16* GetSubMeshBuffer16 (int submesh);

	int GetSubMeshBufferByteSize (int submesh) const { return kVBOIndexSize * m_SubMeshes[submesh].indexCount; }

	// The number of indices contained in the index buffer (all submeshes)
	int GetTotalndexCount () const;

	void ByteSwapIndices ();

	/// 4, 2, 1 bone influence (BoneInfluence, BoneInfluence2, int)
	void* GetSkinInfluence (int count);

	int  GetMeshUsageFlags () const { return m_MeshUsageFlags; }

	virtual bool ShouldIgnoreInGarbageDependencyTracking ();

	UInt32 GetAvailableChannels() const;
	// May return only a subset of channels that are present in the mesh
	UInt32 GetAvailableChannelsForRendering() const;
	UInt32 GetChannelsInVBO() const { return m_ChannelsInVBO; }

	bool IsSuitableSizeForDynamicBatching () const;

	// Calculate cached bone bounds per bone by calculating the bounding volume in bind pose space.
	// This is used by the SkinnedMeshRenderer to compute an accurate world space bounding volume quickly.
	const AABBContainer& GetCachedBonesBounds();

	void DestripifyIndices ();
	void SetHideFromRuntimeStats(bool flag) { m_HideFromRuntimeStats = flag; }

	bool IsSharedPhysicsMeshDirty () { return m_CollisionMesh.IsSharedPhysicsMeshDirty(); }

	bool CanAccessFromScript() const;

	const VertexData&			GetVertexData() const			{ return m_VertexData; }
	VertexData&					GetVertexData()					{ return m_VertexData; }

	UInt8						GetMeshCompression() const		{ return m_MeshCompression; }
	void						SetMeshCompression(UInt8 mc)	{ m_MeshCompression = mc; }

	enum
	{
		kStreamCompressionDefault = 0,
		kStreamCompressionCompressed,
		kStreamCompressionCompressedAggressive
	};

	UInt8						GetStreamCompression() const	{ return m_StreamCompression; }
	void						SetStreamCompression(UInt8 cs)	{ m_StreamCompression = cs; }
	bool						GetIsReadable() const			{ return m_IsReadable; }
	void						SetIsReadable(bool readable)	{ m_IsReadable = readable; }


	bool						GetKeepVertices() const			{ return m_KeepVertices; }
	void						SetKeepVertices(bool keep)		{ m_KeepVertices = keep; }

	bool						GetKeepIndices() const			{ return m_KeepIndices; }
	void						SetKeepIndices(bool keep)		{ m_KeepIndices = keep; }

	const IndexContainer&		GetIndexBuffer() const			{ return m_IndexBuffer; }
	IndexContainer&				GetIndexBuffer()				{ return m_IndexBuffer; }

	const SubMeshContainer&		GetSubMeshes() const			{ return m_SubMeshes; }
	SubMeshContainer&			GetSubMeshes()					{ return m_SubMeshes; }

	const MatrixContainer&		GetBindpose() const				{ return m_Bindpose; }
	MatrixContainer&			GetBindpose()					{ return m_Bindpose; }

	const dynamic_array<BindingHash>& GetBonePathHashes() const	{ return m_BonePathHashes; }
	dynamic_array<BindingHash>& GetBonePathHashes()				{ return m_BonePathHashes; }
	BindingHash					GetRootBonePathHash() const		{ return m_RootBonePathHash; }
	void						SetRootBonePathHash(BindingHash val) { m_RootBonePathHash = val; }

	const BoneInfluenceContainer& GetSkin() const				{ return m_Skin; }
	BoneInfluenceContainer&		GetSkin()						{ return m_Skin; }

	const AABB&					GetLocalAABB() const			{ return m_LocalAABB; }
	void						SetLocalAABB(const AABB& aabb)	{ m_LocalAABB = aabb; }

#if UNITY_PS3 || UNITY_EDITOR
	MeshPartitionContainer		m_Partitions;
	MeshPartitionInfoContainer	m_PartitionInfos;
#endif


#if UNITY_EDITOR
	void SetMeshOptimized(bool meshOptimized) { m_MeshOptimized = meshOptimized; }
	bool GetMeshOptimized() const { return m_MeshOptimized; }
#endif

	UInt32 GetInternalMeshID() const { Assert(m_InternalMeshID); return m_InternalMeshID; }
	
private:
	void CreateSharedVBO( UInt32 wantedChannels );
	void NotifyObjectUsers( const MessageIdentifier& msg );
	void RecalculateSubmeshBoundsInternal (unsigned submesh);
	void RecalculateBoundsInternal ();
	void LoadDeprecatedTangentData (Mesh& mesh, DeprecatedTangentsArray &tangents);
	void SwizzleVertexColorsIfNeeded ();

	const VertexStreamsLayout& GetStreamsLayout() const;
	const VertexChannelsLayout& GetChannelsLayout() const;

	void DestripifySubmeshOnTransferInternal();
	void SetIndexData(int submeshIndex, int indexCount, const void* indices, GfxPrimitiveType topology, int mode);

#if SUPPORT_SERIALIZED_TYPETREES
	template<class TransferFunction>
	void TransferWorkaround35SerializeFuckup (TransferFunction& transfer);
#endif

#if UNITY_EDITOR || UNITY_PS3
	template<class TransferFunction>
	void TransferPS3Data (TransferFunction& transfer);
#endif
#if UNITY_EDITOR
	bool						m_MeshOptimized;
#endif
	
	VertexData					m_VertexData;

	UInt8						m_MeshCompression;
	UInt8						m_StreamCompression;
	bool						m_IsReadable;
	bool						m_KeepVertices;
	bool						m_KeepIndices;
	UInt32						m_InternalMeshID;

	int							 m_MeshUsageFlags;

	IndexContainer	             m_IndexBuffer;
	SubMeshContainer             m_SubMeshes;
	MatrixContainer              m_Bindpose;
	BlendShapeData               m_Shapes;

	dynamic_array<BindingHash>   m_BonePathHashes;
	BindingHash                  m_RootBonePathHash;

	AABBContainer				 m_CachedBonesAABB;

	BoneInfluenceContainer       m_Skin;
	BoneInfluence2Container		 m_CachedSkin2;
	SkinContainer	     		 m_CachedSkin1;

	int                          m_MaxBoneIndex;

	AABB                         m_LocalAABB;

	CollisionMeshData            m_CollisionMesh;

	typedef List< ListNode<Object> > ObjectList;
	ObjectList					m_ObjectUsers; // Object-derived users of this mesh

	IntermediateUsers           m_IntermediateUsers; // IntermediateRenderer users of this mesh
								 
	#if ENABLE_MULTITHREADED_CODE
	UInt32			m_CurrentCPUFence;
	bool			m_WaitOnCPUFence;
	#endif

	PPtr<Object>	m_Owner;
	VBO*			m_VBO;


	UInt32			m_ChannelsInVBO;
	bool			m_VerticesDirty;
	bool			m_IndicesDirty;
	bool			m_IsDynamic;
	bool			m_HideFromRuntimeStats;
	bool			m_VertexColorsSwizzled;

	friend class MeshFilter;
	friend class ClothAnimator;
	friend class CompressedMesh;
	friend void PartitionSubmeshes (Mesh& m);
	friend void OptimizeReorderVertexBuffer (Mesh& mesh);
};

#endif
