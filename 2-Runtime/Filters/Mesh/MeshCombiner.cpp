#include "UnityPrefix.h"
#include "MeshCombiner.h"
#include "Runtime/Graphics/TriStripper.h"
#include "Runtime/Shaders/GraphicsCaps.h"
#include "Runtime/Profiler/Profiler.h"
#include <limits>


#define sqr(x) ((x)*(x))

PROFILER_INFORMATION(gCombineMeshesProfile, "CombineMeshes", kProfilerRender)
PROFILER_INFORMATION(gCombineVerticesProfile, "CombineVertices", kProfilerRender)
PROFILER_INFORMATION(gCombineIndicesProfile, "CombineIndices", kProfilerRender)

static void CombineBoneSkinning (const CombineInstances &in, Mesh& outCombinedMesh);


size_t ExtractMeshIndices(Mesh::TemporaryIndexContainer& srcIndices, const CombineInstance& in, bool useVertexOffsets, size_t& inoutTotalVertexOffset, UInt16* dstIndices)
{
	srcIndices.clear();

	if (in.subMeshIndex < 0 || in.subMeshIndex >= in.mesh->GetSubMeshCount())
		return 0;
	
	const int subMeshIndex = in.subMeshIndex;
	const int vertexOffset = useVertexOffsets ? in.vertexOffset : inoutTotalVertexOffset;
	inoutTotalVertexOffset += in.mesh->GetVertexCount();

		in.mesh->GetTriangles( srcIndices, subMeshIndex );

	size_t numIndices = srcIndices.size();
	if (Dot (Cross(in.transform.GetAxisX(), in.transform.GetAxisY()), in.transform.GetAxisZ()) >= 0)
	{
		for ( size_t k=0; k!=numIndices; ++k )
			dstIndices[k] = srcIndices[k] + vertexOffset;
	} 
	else 
	{
		// if trilist, then
		// reverse Cull order by reversing indices
		for ( size_t k=0; k!=numIndices; ++k )
			dstIndices[k] = srcIndices[numIndices-k-1] + vertexOffset;
	}

	return numIndices;
}

static bool IsMeshBatchable (const Mesh* mesh, int subMeshIndex)
{
	return mesh && mesh->HasVertexData() && subMeshIndex >= 0 && subMeshIndex < mesh->GetSubMeshCount();
}


void CombineMeshIndicesForStaticBatching(const CombineInstances& in, Mesh& inoutMesh, bool mergeSubMeshes, bool useVertexOffsets)
{	
	PROFILER_AUTO(gCombineIndicesProfile, &inoutMesh);

	size_t size = in.size();

	UInt32 maxIndices = 0;
	for ( size_t i=0; i!=size; ++i )
	{
		if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
		{
				const UInt32 numTris = in[i].mesh->GetSubMeshFast( in[i].subMeshIndex ).indexCount;
				if (mergeSubMeshes)
					maxIndices += numTris;
				else
					maxIndices = std::max( maxIndices, numTris );
			}
		}
	
	UInt16* dstIndices = new UInt16[maxIndices+1];
	Mesh::TemporaryIndexContainer srcIndices;
	srcIndices.reserve( maxIndices+1 );
	
	size_t totalVertexOffset = 0;
	if (mergeSubMeshes)
	{
		inoutMesh.SetSubMeshCount( 1 );
		size_t totalNumIndices = 0;
		for ( size_t s=0; s!=size; ++s )
		{
			if (in[s].mesh)
			{
				size_t numIndices = ExtractMeshIndices (srcIndices, in[s], useVertexOffsets, totalVertexOffset, dstIndices+totalNumIndices);
				
				totalNumIndices += numIndices;
				Assert(totalNumIndices <= (maxIndices+1));				
			}
		}
		int mask = Mesh::k16BitIndices;
		inoutMesh.SetIndicesComplex (dstIndices, totalNumIndices, 0, kPrimitiveTriangles, mask);
	}
	else
	{
		inoutMesh.SetSubMeshCount( in.size() );
		for ( size_t s=0; s!=size; ++s )
		{
			if (in[s].mesh)
			{
				size_t numIndices = ExtractMeshIndices (srcIndices, in[s], useVertexOffsets, totalVertexOffset, dstIndices);
				Assert(numIndices <= (maxIndices+1));

				int mask = Mesh::k16BitIndices;
				inoutMesh.SetIndicesComplex (dstIndices, numIndices, s, kPrimitiveTriangles, mask);
			}
		}
	}
	
	delete []dstIndices;
}

void CombineMeshVerticesForStaticBatching ( const CombineInstances& in, const string& combinedMeshName, Mesh& outCombinedMesh, bool useTransforms )
{
	PROFILER_AUTO(gCombineVerticesProfile, &outCombinedMesh);

	int vertexCount = 0;
	size_t size = in.size();
	for( size_t i=0; i!=size; ++i )
	{
		if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			vertexCount += in[i].mesh->GetVertexCount();
	}
	
	bool hasNormals = false;
	bool hasTangents = false;
	bool hasUV0 = false;
	bool hasUV1 = false;
	bool hasColors = false;
	bool hasSkin = false;
	int bindposeCount = 0;
	
	for( size_t i=0; i!=size; ++i )
	{
		if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
		{
			const Mesh* mesh = in[i].mesh;
			const UInt32 channels = mesh->GetAvailableChannels();
			hasNormals	|= (channels & (1<<kShaderChannelNormal)) != 0;
			hasTangents |= (channels & (1<<kShaderChannelTangent)) != 0;
			hasUV0		|= (channels & (1<<kShaderChannelTexCoord0)) != 0;
			hasUV1		|= (channels & (1<<kShaderChannelTexCoord1)) != 0 || (in[i].lightmapTilingOffset != Vector4f(1, 1, 0, 0));
			hasColors	|= (channels & (1<<kShaderChannelColor)) != 0;
			hasSkin		|= mesh->GetSkin().size() && mesh->GetBindpose().size();
			bindposeCount += mesh->GetBindpose().size();
		}
	}
	
	UInt32 channels = 1<<kShaderChannelVertex;
	if ( hasNormals )	channels |= 1<<kShaderChannelNormal;
	if ( hasTangents )	channels |= 1<<kShaderChannelTangent;
	if ( hasUV0 )		channels |= 1<<kShaderChannelTexCoord0;
	if ( hasUV1 )		channels |= 1<<kShaderChannelTexCoord1;
	if ( hasColors )	channels |= 1<<kShaderChannelColor;

	outCombinedMesh.Clear(true);
	outCombinedMesh.ResizeVertices( vertexCount, channels );
	outCombinedMesh.SetName( combinedMeshName.c_str() );
	// Input meshes are already swizzled correctly, so we can copy colors directly
	outCombinedMesh.SetVertexColorsSwizzled(gGraphicsCaps.needsToSwizzleVertexColors);
	
	if ( hasSkin )
	{
		outCombinedMesh.GetSkin().resize_initialized(vertexCount);
		outCombinedMesh.GetBindpose().resize_initialized(bindposeCount);
		outCombinedMesh.GetBonePathHashes().resize_uninitialized(bindposeCount);
	}

	// avoid doing twice (in worst case)
	Matrix4x4f* normalMatrices;
	bool* isNonUniformScaleTransform;
	ALLOC_TEMP (normalMatrices, Matrix4x4f, size);
	ALLOC_TEMP (isNonUniformScaleTransform, bool, size);
	if ( hasNormals || hasTangents )
	{
		for( size_t i=0; i!=size; ++i )
		{
			float uniformScale;
			TransformType type = ComputeTransformType(in[i].transform, uniformScale);
			Matrix4x4f m;
			isNonUniformScaleTransform[i] = IsNonUniformScaleTransform(type);
			if (isNonUniformScaleTransform[i])
			{
				Matrix4x4f::Invert_General3D( in[i].transform, normalMatrices[i] );
				normalMatrices[i].Transpose();
			}
			else
			{
				normalMatrices[i] = Matrix3x3f(in[i].transform);
				// Scale matrix to keep normals normalized
				normalMatrices[i].Scale(Vector3f::one * (1.0f/uniformScale));
			}
		}
	}
	
	int offset = 0;
	for( size_t i=0; i!=size; ++i )
	{
		if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
		{
			const Matrix4x4f& transform = in[i].transform;
			const Mesh* mesh = in[i].mesh;
			if (useTransforms)
				TransformPoints3x4 (transform, 
									(Vector3f const*)mesh->GetChannelPointer (kShaderChannelVertex), 
									mesh->GetStride (kShaderChannelVertex),
									(Vector3f*)outCombinedMesh.GetChannelPointer (kShaderChannelVertex, offset), 
									outCombinedMesh.GetStride (kShaderChannelVertex), 
									mesh->GetVertexCount());
			else
				strided_copy (mesh->GetVertexBegin (), mesh->GetVertexEnd (), outCombinedMesh.GetVertexBegin () + offset);
			offset += mesh->GetVertexCount();
		}
	}
	
	if ( hasNormals )
	{
		offset = 0;
		for( size_t i=0; i!=size; ++i )
		{
			if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			{
				const Mesh* mesh = in[i].mesh;
				int vertexCount = mesh->GetVertexCount ();
				if (!mesh->IsAvailable (kShaderChannelNormal))
					std::fill(outCombinedMesh.GetNormalBegin () + offset, outCombinedMesh.GetNormalBegin () + offset + vertexCount, Vector3f(0.0f,1.0f,0.0f));
				else
				{
					const Matrix4x4f& transform =  normalMatrices[i];

					StrideIterator<Vector3f> outNormal = outCombinedMesh.GetNormalBegin () + offset;
					if (useTransforms)
					{
						if (isNonUniformScaleTransform[i])
						{
							for (StrideIterator<Vector3f> it = mesh->GetNormalBegin (), end = mesh->GetNormalEnd (); it != end; ++it, ++outNormal)
								*outNormal = Normalize( transform.MultiplyVector3( *it) );
						}
						else
						{
							for (StrideIterator<Vector3f> it = mesh->GetNormalBegin (), end = mesh->GetNormalEnd (); it != end; ++it, ++outNormal)
								*outNormal = transform.MultiplyVector3( *it);
						}
					}
					else
						strided_copy (mesh->GetNormalBegin (), mesh->GetNormalEnd (), outCombinedMesh.GetNormalBegin () + offset);					
				}
				offset += vertexCount;
			}
		}
	}
	
	if ( hasTangents )
	{
		offset = 0;
		for ( size_t i=0; i!=size; ++i )
		{
			if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			{
				const Mesh* mesh = in[i].mesh;
				int vertexCount = mesh->GetVertexCount ();
				if (!mesh->IsAvailable (kShaderChannelTangent))
					std::fill(outCombinedMesh.GetTangentBegin () + offset, outCombinedMesh.GetTangentBegin () + offset + vertexCount, Vector4f(1.0f,0.0f,0.0f,1.0f));
				else
				{
					const Matrix4x4f& transform =  normalMatrices[i];
					
					StrideIterator<Vector4f> outTanget = outCombinedMesh.GetTangentBegin () + offset;
					if (useTransforms)
					{
						if (isNonUniformScaleTransform[i])
						{
							for (StrideIterator<Vector4f> it = mesh->GetTangentBegin (), end = mesh->GetTangentEnd (); it != end; ++it, ++outTanget)
							{
								Vector3f t3 = Normalize(transform.MultiplyVector3(Vector3f(it->x, it->y, it->z)));
								*outTanget = Vector4f(t3.x,t3.y,t3.z,it->w);
							}
						}
						else
						{
							for (StrideIterator<Vector4f> it = mesh->GetTangentBegin (), end = mesh->GetTangentEnd (); it != end; ++it, ++outTanget)
							{
								Vector3f t3 = transform.MultiplyVector3(Vector3f(it->x, it->y, it->z));
								*outTanget = Vector4f(t3.x,t3.y,t3.z,it->w);
							}
						}
					}
					else
						strided_copy (mesh->GetTangentBegin (), mesh->GetTangentEnd (), outCombinedMesh.GetTangentBegin () + offset);
				}
				offset += vertexCount;
			}
		}
	}
	
	if ( hasUV0 )
	{
		offset = 0;
		for ( size_t i=0; i!=size; ++i )
		{
			if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			{
				const Mesh* mesh = in[i].mesh;
				int vertexCount = mesh->GetVertexCount ();
				if (!mesh->IsAvailable (kShaderChannelTexCoord0))
					std::fill (outCombinedMesh.GetUvBegin (0) + offset, outCombinedMesh.GetUvBegin (0) + offset + vertexCount, Vector2f(0.0f,0.0f));
				else
					strided_copy (mesh->GetUvBegin (0), mesh->GetUvEnd (0), outCombinedMesh.GetUvBegin (0) + offset);
				offset += vertexCount;
			}
		}
	}
	
	if ( hasUV1 )
	{
		offset = 0;
		for ( size_t i=0; i!=size; ++i )
		{
			if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			{
				const Mesh* mesh = in[i].mesh;
				const int uvIndex = (mesh->GetAvailableChannels() & (1<<kShaderChannelTexCoord1))!=0? 1 : 0;
				StrideIterator<Vector2f> it = in[i].mesh->GetUvBegin( uvIndex );
				StrideIterator<Vector2f> end = in[i].mesh->GetUvEnd( uvIndex );
				
				int vertexCount = mesh->GetVertexCount ();
				if ( it == end)
					std::fill (outCombinedMesh.GetUvBegin (1) + offset, outCombinedMesh.GetUvBegin (1) + offset + vertexCount, Vector2f(0.0f,0.0f));
				else
				{
					// we have to apply lightmap UV scale and offset factors
					// callee is responsible to reset lightmapTilingOffset on the Renderer afterwards
					const Vector4f uvScaleOffset = in[i].lightmapTilingOffset;
					if ( uvScaleOffset != Vector4f(1, 1, 0, 0) )
					{
						StrideIterator<Vector2f> outUV = outCombinedMesh.GetUvBegin (1) + offset;
						for (; it != end; ++it, ++outUV)
						{
							outUV->x = it->x * uvScaleOffset.x + uvScaleOffset.z;
							outUV->y = it->y * uvScaleOffset.y + uvScaleOffset.w;
						}
					}
					else
						strided_copy (it, end, outCombinedMesh.GetUvBegin (1) + offset);
				}
				offset += vertexCount;
			}
		}
	}
	
	if ( hasColors )
	{
		offset = 0;
		for ( size_t i=0; i!=size; ++i )
		{
			if (IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			{
				const Mesh* mesh = in[i].mesh;
				int vertexCount = mesh->GetVertexCount ();
				if (!mesh->IsAvailable (kShaderChannelColor))
					std::fill (outCombinedMesh.GetColorBegin () + offset, outCombinedMesh.GetColorBegin () + offset + vertexCount, ColorRGBA32(255,255,255,255));
				else
				{
					DebugAssert(mesh->GetVertexColorsSwizzled() == outCombinedMesh.GetVertexColorsSwizzled());
					strided_copy (mesh->GetColorBegin (), mesh->GetColorEnd (), outCombinedMesh.GetColorBegin () + offset);
				}
				offset += vertexCount;
			}
		}
	}

	if ( hasSkin )
	{
		CombineBoneSkinning (in, outCombinedMesh);
	}
}

static void CalculateRootBonePathHash (const CombineInstances &in, Mesh& outCombinedMesh)
{
	// We always pick the root bone path hash of the first combine instance.
	// This is because anything else gives unpredictable behaviour and makes it impossible for the user
	// to setup the skinned mesh renderer T/R/S correctly.
	outCombinedMesh.SetRootBonePathHash(in[0].mesh->GetRootBonePathHash());

	// If we made it so that the skinnedmeshrenderer always used the default pose from the Avatar
	// Then it would be possible to pick the root bone from the mesh with the most bones instead.
#if 0
	size_t size = in.size();

	BindingHash rootBonePathHash = 0;
	int boneCount = 0;
	for (size_t i=0; i<size; ++i)
	{
		}
	}
	if (rootBonePathHash)
		outCombinedMesh.SetRootBonePathHash(rootBonePathHash);
#endif
}

static void CombineBoneSkinning (const CombineInstances &in, Mesh& outCombinedMesh)
{
	size_t size = in.size();

	int boneOffset = 0;
	int offset = 0;
	for ( size_t i=0; i!=size; ++i )
	{
		if (!IsMeshBatchable(in[i].mesh, in[i].subMeshIndex))
			continue;
		
		const Mesh* mesh = in[i].mesh;
		Mesh::BoneInfluenceContainer& outSkin = outCombinedMesh.GetSkin();
		const Mesh::BoneInfluenceContainer& inSkin = mesh->GetSkin();
		int vertexCount = mesh->GetVertexCount ();
		if (inSkin.empty())
		{
			for(int i=0; i<vertexCount;i++)
			{
				outSkin[offset+i].weight[0] = 0;
				outSkin[offset+i].weight[1] = 0;
				outSkin[offset+i].weight[2] = 0;
				outSkin[offset+i].weight[3] = 0;
				outSkin[offset+i].boneIndex[0] = 0;
				outSkin[offset+i].boneIndex[1] = 0;
				outSkin[offset+i].boneIndex[2] = 0;
				outSkin[offset+i].boneIndex[3] = 0;
			}
		}
		else 
		{
			for(int i=0; i<vertexCount;i++)
			{
				outSkin[offset+i].weight[0] = inSkin[i].weight[0];
				outSkin[offset+i].weight[1] = inSkin[i].weight[1];
				outSkin[offset+i].weight[2] = inSkin[i].weight[2];
				outSkin[offset+i].weight[3] = inSkin[i].weight[3];
				outSkin[offset+i].boneIndex[0] = inSkin[i].boneIndex[0]+boneOffset;
				outSkin[offset+i].boneIndex[1] = inSkin[i].boneIndex[1]+boneOffset;
				outSkin[offset+i].boneIndex[2] = inSkin[i].boneIndex[2]+boneOffset;
				outSkin[offset+i].boneIndex[3] = inSkin[i].boneIndex[3]+boneOffset;
			}
		}
		
		offset += vertexCount;

		int poseCount = mesh->GetBindpose().size();
		int bindingHashCount = mesh->GetBonePathHashes().size();
		
		memcpy(outCombinedMesh.GetBindpose().begin() + boneOffset, mesh->GetBindpose().begin(), poseCount*sizeof(Matrix4x4f));

		// Old asset bundles might not have bindingHashCount in sync with bind poses.
		if (poseCount == bindingHashCount)
			memcpy(outCombinedMesh.GetBonePathHashes().begin () + boneOffset, mesh->GetBonePathHashes().begin(), poseCount*sizeof(BindingHash));
		else
			memset(outCombinedMesh.GetBonePathHashes().begin () + boneOffset, 0, poseCount*sizeof(BindingHash));
		
		boneOffset += poseCount;
	}

	CalculateRootBonePathHash (in, outCombinedMesh);
}


void CombineMeshes (const CombineInstances &in, Mesh& out, bool mergeSubMeshes, bool useTransforms)
{
	if (!out.CanAccessFromScript())
	{
		ErrorStringMsg("Cannot combine into mesh that does not allow access: %s", out.GetName());
		return;
	}
	for (size_t i = 0; i < in.size(); ++i)
	{
		Mesh* mesh = in[i].mesh;
		if (!mesh)
		{
			WarningStringMsg("Combine mesh instance %" PRINTF_SIZET_FORMAT " is null.", i);
		}
		if (mesh && (in[i].subMeshIndex < 0 || in[i].subMeshIndex >= mesh->GetSubMeshCount()))
		{
			WarningStringMsg("Submesh index %d is invalid for mesh %s.", in[i].subMeshIndex, mesh->GetName());
		}
		if (mesh && !mesh->CanAccessFromScript())
		{
			ErrorStringMsg("Cannot combine mesh that does not allow access: %s", mesh->GetName());
			return;
		}
		if (mesh == &out)
		{
			ErrorStringMsg("Cannot combine into a mesh that is also in the CombineInstances input: %s", mesh->GetName());
			return;
		}
	}

	CombineMeshVerticesForStaticBatching (in, out.GetName(), out, useTransforms);
	CombineMeshIndicesForStaticBatching (in, out, mergeSubMeshes, false);

	out.RecalculateBounds();
	out.UpdateVertexFormat();
}

