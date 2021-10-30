#ifndef SKINGENERIC_H
#define SKINGENERIC_H

#include "Runtime/Filters/Mesh/VertexData.h"

#if UNITY_PS3
template<TransformInstruction transformInstruction, int bonesPerVertexCount,
bool skinNormal, bool skinTangent>
void SkinGenericStreamed (SkinMeshInfo& info)
{
	const int* influence1 = reinterpret_cast<const int*> (info.compactSkin);
	const BoneInfluence2* influence2 = reinterpret_cast<const BoneInfluence2*> (info.compactSkin);
	const BoneInfluence* influence4 = reinterpret_cast<const BoneInfluence*> (info.compactSkin);

	const Matrix4x4f* bones4x4 = info.cachedPose;

	int count = info.vertexCount;

	int vertexOffset = info.vertexData->GetStream(0).offset;
	const int vertexStride = info.vertexData->GetStream(0).stride;

	int normalOffset = info.vertexData->GetStream(1).offset;
	const int normalStride = info.vertexData->GetStream(1).stride;

	int tangentOffset = info.vertexData->GetStream(2).offset;
	const int tangentStride = info.vertexData->GetStream(2).stride;

	const int copyDataOffset = info.vertexData->GetStream(3).offset;
	const int copyDataSize = info.vertexData->GetStream(3).stride * info.vertexCount;

	const UInt8* inputVertex = (const UInt8*)info.inVertices;
	UInt8* outputVertex = (UInt8*)info.outVertices;

	Matrix4x4f poseBlended;
	const Matrix4x4f* poseToUse;

	for( int v = 0; v < count; v++ )
	{
		ALIGN_LOOP_OPTIMIZATION

			Prefetch(inputVertex + 256);

		// Blend the matrices first, then transform everything with this
		// blended matrix. Gives a small speed boost on XCode/Intel (11.3 to 12.00 FPS
		// in skin4 bench), and a good boost on MSVC/Windows (9.6 to 12.4 FPS).
		if (bonesPerVertexCount == 1)
		{
			poseToUse = &bones4x4[*influence1];
		}
		else if (bonesPerVertexCount == 2)
		{
			float weight0 = influence2->weight[0];
			float weight1 = influence2->weight[1];
			const float* b4x40 = bones4x4[influence2->boneIndex[0]].m_Data;
			const float* b4x41 = bones4x4[influence2->boneIndex[1]].m_Data;
			// we need only 12 components of the matrix
			poseBlended.m_Data[ 0] = b4x40[ 0] * weight0 + b4x41[ 0] * weight1;
			poseBlended.m_Data[ 1] = b4x40[ 1] * weight0 + b4x41[ 1] * weight1;
			poseBlended.m_Data[ 2] = b4x40[ 2] * weight0 + b4x41[ 2] * weight1;
			poseBlended.m_Data[ 4] = b4x40[ 4] * weight0 + b4x41[ 4] * weight1;
			poseBlended.m_Data[ 5] = b4x40[ 5] * weight0 + b4x41[ 5] * weight1;
			poseBlended.m_Data[ 6] = b4x40[ 6] * weight0 + b4x41[ 6] * weight1;
			poseBlended.m_Data[ 8] = b4x40[ 8] * weight0 + b4x41[ 8] * weight1;
			poseBlended.m_Data[ 9] = b4x40[ 9] * weight0 + b4x41[ 9] * weight1;
			poseBlended.m_Data[10] = b4x40[10] * weight0 + b4x41[10] * weight1;
			poseBlended.m_Data[12] = b4x40[12] * weight0 + b4x41[12] * weight1;
			poseBlended.m_Data[13] = b4x40[13] * weight0 + b4x41[13] * weight1;
			poseBlended.m_Data[14] = b4x40[14] * weight0 + b4x41[14] * weight1;
			poseToUse = &poseBlended;
		}
		else if (bonesPerVertexCount == 4)
		{
			float weight0 = influence4->weight[0];
			float weight1 = influence4->weight[1];
			float weight2 = influence4->weight[2];
			float weight3 = influence4->weight[3];

			const float* b4x40 = bones4x4[influence4->boneIndex[0]].m_Data;
			const float* b4x41 = bones4x4[influence4->boneIndex[1]].m_Data;
			const float* b4x42 = bones4x4[influence4->boneIndex[2]].m_Data;
			const float* b4x43 = bones4x4[influence4->boneIndex[3]].m_Data;
			// we need only 12 components of the matrix, so unroll 
			poseBlended.m_Data[ 0] = b4x40[ 0] * weight0 + b4x41[ 0] * weight1 + b4x42[ 0] * weight2 + b4x43[ 0] * weight3;
			poseBlended.m_Data[ 1] = b4x40[ 1] * weight0 + b4x41[ 1] * weight1 + b4x42[ 1] * weight2 + b4x43[ 1] * weight3;
			poseBlended.m_Data[ 2] = b4x40[ 2] * weight0 + b4x41[ 2] * weight1 + b4x42[ 2] * weight2 + b4x43[ 2] * weight3;
			poseBlended.m_Data[ 4] = b4x40[ 4] * weight0 + b4x41[ 4] * weight1 + b4x42[ 4] * weight2 + b4x43[ 4] * weight3;
			poseBlended.m_Data[ 5] = b4x40[ 5] * weight0 + b4x41[ 5] * weight1 + b4x42[ 5] * weight2 + b4x43[ 5] * weight3;
			poseBlended.m_Data[ 6] = b4x40[ 6] * weight0 + b4x41[ 6] * weight1 + b4x42[ 6] * weight2 + b4x43[ 6] * weight3;
			poseBlended.m_Data[ 8] = b4x40[ 8] * weight0 + b4x41[ 8] * weight1 + b4x42[ 8] * weight2 + b4x43[ 8] * weight3;
			poseBlended.m_Data[ 9] = b4x40[ 9] * weight0 + b4x41[ 9] * weight1 + b4x42[ 9] * weight2 + b4x43[ 9] * weight3;
			poseBlended.m_Data[10] = b4x40[10] * weight0 + b4x41[10] * weight1 + b4x42[10] * weight2 + b4x43[10] * weight3;
			poseBlended.m_Data[12] = b4x40[12] * weight0 + b4x41[12] * weight1 + b4x42[12] * weight2 + b4x43[12] * weight3;
			poseBlended.m_Data[13] = b4x40[13] * weight0 + b4x41[13] * weight1 + b4x42[13] * weight2 + b4x43[13] * weight3;
			poseBlended.m_Data[14] = b4x40[14] * weight0 + b4x41[14] * weight1 + b4x42[14] * weight2 + b4x43[14] * weight3;
			poseToUse = &poseBlended;
		}

		// skin components
		Vector3f outVertex, outNormal, outTangent;
		const Vector3f* vertex = reinterpret_cast<const Vector3f*>( inputVertex + vertexOffset);
		const Vector3f* normal = reinterpret_cast<const Vector3f*>( inputVertex + normalOffset );
		const Vector3f* tangent = reinterpret_cast<const Vector3f*>( inputVertex + tangentOffset );
		poseToUse->MultiplyPoint3( *vertex, outVertex );
		if( skinNormal )
		{
			poseToUse->MultiplyVector3( *normal, outNormal );
			if (transformInstruction == kNormalizeFastest)
			{
				float sqr1 = SqrMagnitude( outNormal );
				float invsqrt1 = FastestInvSqrt (sqr1);
				outNormal *= invsqrt1;
			}
			else if (transformInstruction == kNormalizeFast)
			{
				float sqr1 = SqrMagnitude( outNormal );
				float invsqrt1 = FastInvSqrt (sqr1);
				outNormal *= invsqrt1;
			}
		}
		if( skinTangent )
		{
			poseToUse->MultiplyVector3( *tangent, outTangent );
			if (transformInstruction == kNormalizeFastest)
			{
				float sqr1 = SqrMagnitude( outTangent );
				float invsqrt1 = FastestInvSqrt (sqr1);
				outTangent *= invsqrt1;
			}
			else if (transformInstruction == kNormalizeFast)
			{
				float sqr1 = SqrMagnitude( outTangent );
				float invsqrt1 = FastInvSqrt (sqr1);
				outTangent *= invsqrt1;
			}
		}

		// write data out
		*reinterpret_cast<Vector3f*> (outputVertex + vertexOffset) = outVertex;
		if( skinNormal )
		{
			*reinterpret_cast<Vector3f*>( outputVertex + normalOffset ) = outNormal;
		}
		if( skinTangent )
		{
			*reinterpret_cast<Vector3f*>( outputVertex + tangentOffset ) = outTangent;
			*reinterpret_cast<float*>( outputVertex + tangentOffset + sizeof(Vector3f) ) = *reinterpret_cast<const float*>( inputVertex + tangentOffset + sizeof(Vector3f) );
		}

		vertexOffset += vertexStride;
		normalOffset += normalStride;
		tangentOffset += tangentStride;

		if (bonesPerVertexCount == 1)
			influence1++;
		else if (bonesPerVertexCount == 2)
			influence2++;
		if (bonesPerVertexCount == 4)
			influence4++;
	}

	// copy 
	const UInt8* copyDataSrc = inputVertex + copyDataOffset;
	UInt8* copyDataDst = outputVertex + copyDataOffset;
	memcpy(copyDataDst, copyDataSrc, copyDataSize);
}
#endif

template<TransformInstruction transformInstruction, int bonesPerVertexCount,
	bool skinNormal, bool skinTangent>
void SkinGeneric (SkinMeshInfo& info);

template<TransformInstruction transformInstruction, int bonesPerVertexCount,
	bool skinNormal, bool skinTangent>
void SkinGeneric (SkinMeshInfo& info)
{
#if UNITY_PS3
	if(info.vertexData && (info.vertexData->GetActiveStreamCount() > 2))
		return SkinGenericStreamed<transformInstruction, bonesPerVertexCount, skinNormal, skinTangent>(info);
#endif
	const int* influence1 = reinterpret_cast<const int*> (info.compactSkin);
	const BoneInfluence2* influence2 = reinterpret_cast<const BoneInfluence2*> (info.compactSkin);
	const BoneInfluence* influence4 = reinterpret_cast<const BoneInfluence*> (info.compactSkin);
	
	const Matrix4x4f* bones4x4 = info.cachedPose;
	
	const int inStride = info.inStride;
	int outStride = info.outStride;
	int count = info.vertexCount;

	const int normalOffset = info.normalOffset;
	const int tangentOffset = info.tangentOffset;

	const UInt8* inputVertex = (const UInt8*)info.inVertices;
	UInt8* outputVertex = (UInt8*)info.outVertices;
	
	Matrix4x4f poseBlended;
	const Matrix4x4f* poseToUse;

	
#if !ENABLE_MULTITHREADED_SKINNING
	PROFILER_AUTO(gMeshSkinningSlowpath, NULL);
#endif

	//;;printf_console("bonesPerVertexCount: %d, skinNormal: %d, normalOffset: %d, inStride: %d, copyDataSizeInts: %d, count: %d, boneCount: %d, outputVertex: %d\n",
	//			   bonesPerVertexCount, (int)skinNormal, normalOffset, inStride, copyDataSizeInts, count, info.boneCount, (int)outputVertex);
	//;;uint64_t delta = mach_absolute_time();
	
	for( int v = 0; v < count; v++ )
	{
		ALIGN_LOOP_OPTIMIZATION
		
		Prefetch(inputVertex + 256);
		
		// Blend the matrices first, then transform everything with this
		// blended matrix. Gives a small speed boost on XCode/Intel (11.3 to 12.00 FPS
		// in skin4 bench), and a good boost on MSVC/Windows (9.6 to 12.4 FPS).
		if (bonesPerVertexCount == 1)
		{
			poseToUse = &bones4x4[*influence1];
		}
		else if (bonesPerVertexCount == 2)
		{
			float weight0 = influence2->weight[0];
			float weight1 = influence2->weight[1];
			const float* b4x40 = bones4x4[influence2->boneIndex[0]].m_Data;
			const float* b4x41 = bones4x4[influence2->boneIndex[1]].m_Data;
			// we need only 12 components of the matrix
			poseBlended.m_Data[ 0] = b4x40[ 0] * weight0 + b4x41[ 0] * weight1;
			poseBlended.m_Data[ 1] = b4x40[ 1] * weight0 + b4x41[ 1] * weight1;
			poseBlended.m_Data[ 2] = b4x40[ 2] * weight0 + b4x41[ 2] * weight1;
			poseBlended.m_Data[ 4] = b4x40[ 4] * weight0 + b4x41[ 4] * weight1;
			poseBlended.m_Data[ 5] = b4x40[ 5] * weight0 + b4x41[ 5] * weight1;
			poseBlended.m_Data[ 6] = b4x40[ 6] * weight0 + b4x41[ 6] * weight1;
			poseBlended.m_Data[ 8] = b4x40[ 8] * weight0 + b4x41[ 8] * weight1;
			poseBlended.m_Data[ 9] = b4x40[ 9] * weight0 + b4x41[ 9] * weight1;
			poseBlended.m_Data[10] = b4x40[10] * weight0 + b4x41[10] * weight1;
			poseBlended.m_Data[12] = b4x40[12] * weight0 + b4x41[12] * weight1;
			poseBlended.m_Data[13] = b4x40[13] * weight0 + b4x41[13] * weight1;
			poseBlended.m_Data[14] = b4x40[14] * weight0 + b4x41[14] * weight1;
			poseToUse = &poseBlended;
		}
		else if (bonesPerVertexCount == 4)
		{
			float weight0 = influence4->weight[0];
			float weight1 = influence4->weight[1];
			float weight2 = influence4->weight[2];
			float weight3 = influence4->weight[3];
			
			const float* b4x40 = bones4x4[influence4->boneIndex[0]].m_Data;
			const float* b4x41 = bones4x4[influence4->boneIndex[1]].m_Data;
			const float* b4x42 = bones4x4[influence4->boneIndex[2]].m_Data;
			const float* b4x43 = bones4x4[influence4->boneIndex[3]].m_Data;
			// we need only 12 components of the matrix, so unroll 
			poseBlended.m_Data[ 0] = b4x40[ 0] * weight0 + b4x41[ 0] * weight1 + b4x42[ 0] * weight2 + b4x43[ 0] * weight3;
			poseBlended.m_Data[ 1] = b4x40[ 1] * weight0 + b4x41[ 1] * weight1 + b4x42[ 1] * weight2 + b4x43[ 1] * weight3;
			poseBlended.m_Data[ 2] = b4x40[ 2] * weight0 + b4x41[ 2] * weight1 + b4x42[ 2] * weight2 + b4x43[ 2] * weight3;
			poseBlended.m_Data[ 4] = b4x40[ 4] * weight0 + b4x41[ 4] * weight1 + b4x42[ 4] * weight2 + b4x43[ 4] * weight3;
			poseBlended.m_Data[ 5] = b4x40[ 5] * weight0 + b4x41[ 5] * weight1 + b4x42[ 5] * weight2 + b4x43[ 5] * weight3;
			poseBlended.m_Data[ 6] = b4x40[ 6] * weight0 + b4x41[ 6] * weight1 + b4x42[ 6] * weight2 + b4x43[ 6] * weight3;
			poseBlended.m_Data[ 8] = b4x40[ 8] * weight0 + b4x41[ 8] * weight1 + b4x42[ 8] * weight2 + b4x43[ 8] * weight3;
			poseBlended.m_Data[ 9] = b4x40[ 9] * weight0 + b4x41[ 9] * weight1 + b4x42[ 9] * weight2 + b4x43[ 9] * weight3;
			poseBlended.m_Data[10] = b4x40[10] * weight0 + b4x41[10] * weight1 + b4x42[10] * weight2 + b4x43[10] * weight3;
			poseBlended.m_Data[12] = b4x40[12] * weight0 + b4x41[12] * weight1 + b4x42[12] * weight2 + b4x43[12] * weight3;
			poseBlended.m_Data[13] = b4x40[13] * weight0 + b4x41[13] * weight1 + b4x42[13] * weight2 + b4x43[13] * weight3;
			poseBlended.m_Data[14] = b4x40[14] * weight0 + b4x41[14] * weight1 + b4x42[14] * weight2 + b4x43[14] * weight3;
			poseToUse = &poseBlended;
		}

		// skin components
		Vector3f outVertex, outNormal, outTangent;
		const Vector3f* vertex = reinterpret_cast<const Vector3f*>( inputVertex );
		const Vector3f* normal = reinterpret_cast<const Vector3f*>( inputVertex + normalOffset );
		const Vector3f* tangent = reinterpret_cast<const Vector3f*>( inputVertex + tangentOffset );
		poseToUse->MultiplyPoint3( *vertex, outVertex );
		if( skinNormal )
		{
			poseToUse->MultiplyVector3( *normal, outNormal );
			if (transformInstruction == kNormalizeFastest)
			{
				float sqr1 = SqrMagnitude( outNormal );
				float invsqrt1 = FastestInvSqrt (sqr1);
				outNormal *= invsqrt1;
			}
			else if (transformInstruction == kNormalizeFast)
			{
				float sqr1 = SqrMagnitude( outNormal );
				float invsqrt1 = FastInvSqrt (sqr1);
				outNormal *= invsqrt1;
			}
		}
		if( skinTangent )
		{
			poseToUse->MultiplyVector3( *tangent, outTangent );
			if (transformInstruction == kNormalizeFastest)
			{
				float sqr1 = SqrMagnitude( outTangent );
				float invsqrt1 = FastestInvSqrt (sqr1);
				outTangent *= invsqrt1;
			}
			else if (transformInstruction == kNormalizeFast)
			{
				float sqr1 = SqrMagnitude( outTangent );
				float invsqrt1 = FastInvSqrt (sqr1);
				outTangent *= invsqrt1;
			}
		}
	
		// write data out
		*reinterpret_cast<Vector3f*> (outputVertex) = outVertex;
		if( skinNormal )
		{
			*reinterpret_cast<Vector3f*>( outputVertex + normalOffset ) = outNormal;
		}
		
		if( skinTangent )
		{
			*reinterpret_cast<Vector3f*>( outputVertex + tangentOffset ) = outTangent;
			*reinterpret_cast<float*>( outputVertex + tangentOffset + sizeof(Vector3f) ) = *reinterpret_cast<const float*>( inputVertex + tangentOffset + sizeof(Vector3f) );
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
		
	//;;static int frameCount = 0; frameCount++;
	//delta = mach_absolute_time() - delta;
	//;;static uint64_t deltaAccum = 0; deltaAccum += (int)(delta);
	//;;printf_console("skin-c: %d %d\n", (int)(deltaAccum / frameCount), (int)delta);
}

#endif
