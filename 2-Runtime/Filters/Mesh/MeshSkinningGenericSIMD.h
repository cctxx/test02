#if 0

/*
 mircea@INFO: this doesn't do normalization.
 */

#include "Runtime/Math/Simd/Matrix4x4Simd.h"

template<TransformInstruction transformInstruction, int bonesPerVertexCount,
bool skinNormal, bool skinTangent, bool copy8BytesAt24Offset>
void SkinGenericSimd (SkinMeshInfo& info)
{
	DebugAssertIf( copy8BytesAt24Offset && (!info.skinNormals || info.normalOffset != 12) );
	const int* influence1 = reinterpret_cast<const int*> (info.compactSkin);
	const BoneInfluence2* influence2 = reinterpret_cast<const BoneInfluence2*> (info.compactSkin);
	const BoneInfluence* influence4 = reinterpret_cast<const BoneInfluence*> (info.compactSkin);
	
	const Matrix4x4f* bones4x4 = info.cachedPose;
	
	const int inStride = info.inStride;
	int outStride = info.outStride;
	int count = info.vertexCount;
	
	const int normalOffset = (copy8BytesAt24Offset ? 12 : info.normalOffset) >> 2;
	const int tangentOffset = info.tangentOffset >> 2;
	
	const UInt8* inputVertex = (const UInt8*)info.inVertices;
	UInt8* outputVertex = (UInt8*)info.outVertices;
	
	Simd128 pose0, pose1, pose2, pose3;
	
	for( int v = 0; v < count; v++ )
	{
		ALIGN_LOOP_OPTIMIZATION
		
		// Blend the matrices first, then transform everything with this
		// blended matrix. Gives a small speed boost on XCode/Intel (11.3 to 12.00 FPS
		// in skin4 bench), and a good boost on MSVC/Windows (9.6 to 12.4 FPS).
		if (bonesPerVertexCount == 1)
		{
			const float* maddr = bones4x4[*influence1].m_Data;
			
			Prefetch(maddr);
			
			pose0 = V4LoadUnaligned( maddr, 0x0 );
			pose1 = V4LoadUnaligned( maddr, 0x4 );
			pose2 = V4LoadUnaligned( maddr, 0x8 );
			pose3 = V4LoadUnaligned( maddr, 0xC );
		}
		else if (bonesPerVertexCount == 2)
		{
			Prefetch(influence2);
			
			Simd128 weights = {influence2->weight[0], influence2->weight[1], 0, 0};
			
			const float* maddr0 = bones4x4[influence2->boneIndex[0]].m_Data;
			const float* maddr1 = bones4x4[influence2->boneIndex[1]].m_Data;
			
			Prefetch(maddr0);
			Prefetch(maddr1);
			
			Simd128 weight0 = V4Splat(weights, 0);
			Simd128 weight1 = V4Splat(weights, 1);
			
			Simd128 mat00 = V4LoadUnaligned( maddr0, 0x0 );
			Simd128 mat01 = V4LoadUnaligned( maddr0, 0x4 );
			Simd128 mat02 = V4LoadUnaligned( maddr0, 0x8 );
			Simd128 mat03 = V4LoadUnaligned( maddr0, 0xC );
			
			Simd128 mat10 = V4LoadUnaligned( maddr1, 0x0 );
			Simd128 mat11 = V4LoadUnaligned( maddr1, 0x4 );
			Simd128 mat12 = V4LoadUnaligned( maddr1, 0x8 );
			Simd128 mat13 = V4LoadUnaligned( maddr1, 0xC );
			
			pose0 = V4Mul(mat00, weight0);
			pose1 = V4Mul(mat01, weight0);
			pose2 = V4Mul(mat02, weight0);
			pose3 = V4Mul(mat03, weight0);
			
			pose0 = V4MulAdd(mat10, weight1, pose0);
			pose1 = V4MulAdd(mat11, weight1, pose1);
			pose2 = V4MulAdd(mat12, weight1, pose2);
			pose3 = V4MulAdd(mat13, weight1, pose3);
		}
		else if (bonesPerVertexCount == 4)
		{
			Prefetch(influence4);
			
			Simd128 weights = {influence4->weight[0], influence4->weight[1], influence4->weight[2], influence4->weight[3]};
			
			const float* maddr0 = bones4x4[influence4->boneIndex[0]].m_Data;
			const float* maddr1 = bones4x4[influence4->boneIndex[1]].m_Data;
			const float* maddr2 = bones4x4[influence4->boneIndex[2]].m_Data;
			const float* maddr3 = bones4x4[influence4->boneIndex[3]].m_Data;
			
			Prefetch(maddr0);
			Prefetch(maddr1);
			Prefetch(maddr2);
			Prefetch(maddr3);
			
			Simd128 weight0 = V4Splat(weights, 0);
			Simd128 weight1 = V4Splat(weights, 1);
			Simd128 weight2 = V4Splat(weights, 2);
			Simd128 weight3 = V4Splat(weights, 3);
			
			Simd128 mat00 = V4LoadUnaligned( maddr0, 0x0 );
			Simd128 mat01 = V4LoadUnaligned( maddr0, 0x4 );
			Simd128 mat02 = V4LoadUnaligned( maddr0, 0x8 );
			Simd128 mat03 = V4LoadUnaligned( maddr0, 0xC );
			
			Simd128 mat10 = V4LoadUnaligned( maddr1, 0x0 );
			Simd128 mat11 = V4LoadUnaligned( maddr1, 0x4 );
			Simd128 mat12 = V4LoadUnaligned( maddr1, 0x8 );
			Simd128 mat13 = V4LoadUnaligned( maddr1, 0xC );
			
			Simd128 mat20 = V4LoadUnaligned( maddr2, 0x0 );
			Simd128 mat21 = V4LoadUnaligned( maddr2, 0x4 );
			Simd128 mat22 = V4LoadUnaligned( maddr2, 0x8 );
			Simd128 mat23 = V4LoadUnaligned( maddr2, 0xC );
			
			Simd128 mat30 = V4LoadUnaligned( maddr3, 0x0 );
			Simd128 mat31 = V4LoadUnaligned( maddr3, 0x4 );
			Simd128 mat32 = V4LoadUnaligned( maddr3, 0x8 );
			Simd128 mat33 = V4LoadUnaligned( maddr3, 0xC );
			
			pose0 = V4Mul(mat00, weight0);
			pose1 = V4Mul(mat01, weight0);
			pose2 = V4Mul(mat02, weight0);
			pose3 = V4Mul(mat03, weight0);
			
			pose0 = V4MulAdd(mat10, weight1, pose0);
			pose1 = V4MulAdd(mat11, weight1, pose1);
			pose2 = V4MulAdd(mat12, weight1, pose2);
			pose3 = V4MulAdd(mat13, weight1, pose3);
			
			pose0 = V4MulAdd(mat20, weight2, pose0);
			pose1 = V4MulAdd(mat21, weight2, pose1);
			pose2 = V4MulAdd(mat22, weight2, pose2);
			pose3 = V4MulAdd(mat23, weight2, pose3);
			
			pose0 = V4MulAdd(mat30, weight3, pose0);
			pose1 = V4MulAdd(mat31, weight3, pose1);
			pose2 = V4MulAdd(mat32, weight3, pose2);
			pose3 = V4MulAdd(mat33, weight3, pose3);
		}
		
		Prefetch(inputVertex);
		
		Simd128 vpos = V4LoadUnaligned((const float*)inputVertex, 0);
		TransformPoint3NATIVE(pose0, pose1, pose2, pose3, vpos, vpos);
		
		Simd128 vnor, vtan, ndot, tdot;
		
		// remember... this is a template and skinNormal & skinTangent are consts 
		if(skinNormal || skinTangent) 
		{
			Simd128 vlen;
			if( skinNormal ) 
			{
				vnor = V4LoadUnaligned((const float*)inputVertex, normalOffset);
				TransformVector3NATIVE(pose0, pose1, pose2, pose3, vnor, vnor);
				ndot = V3Dot(vnor, vnor);
			} 
			else 
			{
				ndot = V4Zero();
			}
			
			if( skinTangent ) 
			{
				vtan = V4LoadUnaligned((const float*)inputVertex, tangentOffset);
				TransformVector3NATIVE(pose0, pose1, pose2, pose3, vtan, vtan);
				tdot = V3Dot(vtan, vtan);
			} 
			else 
			{
				tdot = V4Zero();
			}
			
			vlen = V4MergeH(ndot, tdot);
			vlen = V4Rsqrt(vlen);
			
			if(skinNormal) {
				vnor = V4Mul(vnor, V4Splat(vlen, 0));
				V3StoreUnaligned(vnor, (float*)outputVertex, normalOffset);
			}
			
			if(skinTangent) {
				vtan = V4Mul(vtan, V4Splat(vlen, 1));
				V3StoreUnaligned(vtan, (float*)outputVertex, tangentOffset);
			}
		}
		
		V3StoreUnaligned(vpos, (float*)outputVertex, 0);
		
		if( skinTangent )
		{
			*reinterpret_cast<float*>( outputVertex + (tangentOffset<<2) + sizeof(Vector3f) ) = *reinterpret_cast<const float*>( inputVertex + (tangentOffset<<2) + sizeof(Vector3f) );
		}
		
		outputVertex += outStride;
		inputVertex += inStride;
		
		if (bonesPerVertexCount == 1)
			influence1++;
		else if (bonesPerVertexCount == 2)
			influence2++;
		if (bonesPerVertexCount == 4)
			influence4++;
	}
}
#endif
